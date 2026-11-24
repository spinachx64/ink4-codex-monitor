import asyncio
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import AsyncMock, patch

from bridge.agy_provider import AgyQuotaProvider, normalize_agy, subprocess_env_with_system_proxy
from bridge.ble_bridge import BridgeLink, send_updates
from bridge.protocol import encode_agy
from bridge.quota_normalizer import Window
from bridge.quota_provider import QuotaError
from bridge.state import State


SAMPLE_AGY_PAYLOAD = {
    "conversation_id": "",
    "status": "SUCCESS",
    "duration_seconds": 0,
    "num_turns": 0,
    "usage": {"total_tokens": 0},
    "command": {
        "name": "usage",
        "data": {
            "groups": [
                {
                    "name": "Gemini Models",
                    "buckets": [
                        {
                            "id": "gemini-weekly",
                            "window": "weekly",
                            "description": "You have used some of your weekly limit.",
                            "remaining_fraction": 0.9779,
                            "reset_time": "2026-12-01T11:58:46Z",
                        },
                        {
                            "id": "gemini-5h",
                            "window": "5h",
                            "description": "You have used some of your 5-hour limit.",
                            "remaining_fraction": 0.9116,
                            "reset_time": "2026-11-24T16:58:46Z",
                        },
                    ],
                },
                {
                    "name": "Claude and GPT models",
                    "buckets": [
                        {
                            "id": "3p-weekly",
                            "window": "weekly",
                            "remaining_fraction": 1,
                            "reset_time": "2026-12-01T13:00:49Z",
                        },
                        {
                            "id": "3p-5h",
                            "window": "5h",
                            "remaining_fraction": 1,
                            "reset_time": "2026-11-24T18:00:49Z",
                        },
                    ],
                },
            ]
        },
    },
}


class AgyBridgeTests(unittest.TestCase):
    def test_normalize_agy_maps_all_four_buckets_and_idle_full(self):
        windows = normalize_agy(SAMPLE_AGY_PAYLOAD)
        self.assertAlmostEqual(windows["gh"].remaining, 91.2, places=1)
        self.assertIsNotNone(windows["gh"].reset)
        self.assertAlmostEqual(windows["gw"].remaining, 97.8, places=1)
        self.assertIsNotNone(windows["gw"].reset)
        self.assertEqual(windows["ch"].remaining, 100.0)
        self.assertIsNone(windows["ch"].reset)  # 100% idle bucket shows AVAILABLE
        self.assertEqual(windows["cw"].remaining, 100.0)
        self.assertIsNone(windows["cw"].reset)

    def test_encode_agy_wire_format_and_size(self):
        windows = normalize_agy(SAMPLE_AGY_PAYLOAD)
        packet = encode_agy(1793327720130002, windows, 1793327720130)
        self.assertTrue(packet.endswith(b"\n"))
        self.assertLess(len(packet), 512)
        parsed = json.loads(packet)
        self.assertEqual(parsed["v"], 2)
        self.assertEqual(parsed["k"], "a")
        self.assertEqual(parsed["g"]["h"]["s"], "api")
        self.assertEqual(parsed["c"]["w"]["u"], 0.0)
        self.assertIsNone(parsed["c"]["w"]["r"])

    def test_state_persists_agy_independently_and_marks_stale(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "state.json"
            state = State(path)
            state.update({"h": Window(25, 1800000000), "w": None})
            state.update_agy(normalize_agy(SAMPLE_AGY_PAYLOAD))
            reloaded = State(path)
            self.assertEqual(reloaded.windows()["h"].remaining, 75)
            self.assertAlmostEqual(reloaded.agy_windows()["gh"].remaining, 91.2, places=1)
            self.assertTrue(reloaded.mark_agy_failed())
            self.assertFalse(reloaded.mark_agy_failed())
            self.assertEqual(reloaded.agy_windows()["gh"].source, "stale")
            # Codex data stays fresh when only AGY fails.
            self.assertEqual(reloaded.windows()["h"].source, "api")

    def test_subprocess_env_reads_system_proxy_dynamically(self):
        with patch("bridge.agy_provider.urllib.request.getproxies",
                   return_value={"http": "http://127.0.0.1:7897", "https": "http://127.0.0.1:7897"}):
            env = subprocess_env_with_system_proxy({"PATH": "/usr/bin"})
        self.assertEqual(env["HTTPS_PROXY"], "http://127.0.0.1:7897")
        self.assertEqual(env["http_proxy"], "http://127.0.0.1:7897")
        # Direct mode when system proxy is off.
        with patch("bridge.agy_provider.urllib.request.getproxies", return_value={}):
            env = subprocess_env_with_system_proxy({"PATH": "/usr/bin"})
        self.assertNotIn("HTTPS_PROXY", env)
        # Explicit shell proxy takes precedence over system proxy.
        with patch("bridge.agy_provider.urllib.request.getproxies",
                   return_value={"https": "http://127.0.0.1:9999"}):
            env = subprocess_env_with_system_proxy({"https_proxy": "http://127.0.0.1:1234"})
        self.assertEqual(env["https_proxy"], "http://127.0.0.1:1234")
        self.assertNotIn("HTTPS_PROXY", env)


class AgyAsyncTests(unittest.IsolatedAsyncioTestCase):
    async def test_provider_parses_json_with_prefix_logs(self):
        proc = AsyncMock()
        proc.returncode = 0
        proc.communicate = AsyncMock(
            return_value=(b"some warning line\n" + json.dumps(SAMPLE_AGY_PAYLOAD).encode() + b"\n", b"")
        )
        provider = AgyQuotaProvider(agy="agy")
        with patch("bridge.agy_provider.asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
            result = await provider.fetch()
        self.assertEqual(provider.last_source, "agy-cli")
        self.assertAlmostEqual(result["gh"].remaining, 91.2, places=1)

    async def test_provider_maps_auth_error(self):
        proc = AsyncMock()
        proc.returncode = 1
        proc.communicate = AsyncMock(return_value=(b"", b"401 please login first"))
        provider = AgyQuotaProvider(agy="agy")
        with patch("bridge.agy_provider.asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
            with self.assertRaises(QuotaError) as ctx:
                await provider.fetch()
        self.assertEqual(ctx.exception.status, "no_auth")

    async def test_ble_send_updates_pushes_both_codex_and_agy(self):
        with tempfile.TemporaryDirectory() as tmp:
            state = State(Path(tmp) / "state.json")
            state.update({"h": Window(20, 1800000000), "w": Window(50, 1800500000)})
            state.update_agy(normalize_agy(SAMPLE_AGY_PAYLOAD))
            link = BridgeLink()
            session = link.connect()
            link.publish(success_session=session)
            link.publish_agy(success_session=session)
            disconnected = asyncio.Event()
            buf = bytearray()

            class FakeClient:
                is_connected = True

                async def write_gatt_char(self, _uuid, chunk, response=True):
                    buf.extend(chunk)
                    if buf.count(b"\n") >= 2:
                        disconnected.set()

            await send_updates(FakeClient(), state, link, session, disconnected)
            lines = [json.loads(line) for line in buf.decode().splitlines() if line]
            self.assertEqual(len(lines), 2)
            self.assertEqual(lines[0]["v"], 1)
            self.assertEqual(lines[1]["v"], 2)
            self.assertEqual(lines[1]["k"], "a")
