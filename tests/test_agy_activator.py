import asyncio
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, Mock, patch

from bridge.agy_activator import AgyCycleActivator, DEFAULT_AGY_MODEL
from bridge.ble_bridge import BridgeLink
from bridge.cycle_activator import PROMPT, WINDOW_SECONDS
from bridge.main import poll_agy
from bridge.quota_normalizer import Window
from bridge.quota_provider import QuotaError
from bridge.state import State

SESSION = "055a398f-db14-4c5f-abbb-1bf03f8120a7"
NOW = 1800000000


def agy_windows(reset=None, used=0, source="api"):
    return {
        "gh": Window(used, reset, source),
        "gw": Window(1.0, NOW + 7 * 86400, "api"),
        "ch": Window(0.0, None, "api"),
        "cw": Window(0.0, None, "api"),
    }


def agy_process(events=None, code=0):
    proc = AsyncMock()
    proc.kill = Mock()
    proc.returncode = code
    if events is None:
        events = [
            {"event": "init", "conversation_id": SESSION, "init": {}},
            {"event": "result", "result": {"status": "SUCCESS", "conversation_id": SESSION, "response": "OK"}},
        ]
    proc.stdout = asyncio.StreamReader()
    for event in events:
        if isinstance(event, bytes):
            proc.stdout.feed_data(event)
        else:
            proc.stdout.feed_data((json.dumps(event) + "\n").encode())
    proc.stdout.feed_eof()
    return proc


class AgyActivationTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / "state.agy-activation.json"
        self.activator = AgyCycleActivator("agy", self.path)

    async def test_only_fresh_idle_full_or_expired_gemini_5h_is_eligible(self):
        for value in (
            {"gh": None, "gw": None, "ch": None, "cw": None},
            agy_windows(used=None, reset=NOW + WINDOW_SECONDS),
            agy_windows(used=0, reset=None, source="stale"),
            agy_windows(used=0, reset=None, source="estimated"),
            agy_windows(used=0, reset=NOW + WINDOW_SECONDS - 30),
            agy_windows(used=1, reset=NOW + WINDOW_SECONDS),
            agy_windows(used=100, reset=NOW + 30),
        ):
            self.assertFalse(self.activator.due(value, NOW), value)
        self.assertTrue(self.activator.due(agy_windows(used=0, reset=None), NOW))
        self.assertTrue(self.activator.due(agy_windows(used=0, reset=NOW + WINDOW_SECONDS), NOW))
        self.assertTrue(self.activator.due(agy_windows(used=99, reset=NOW - 1), NOW))

    async def test_create_persist_then_resume_same_conversation_after_restart(self):
        with patch("bridge.agy_activator.time.time", return_value=NOW), \
                patch("bridge.agy_activator.subprocess_env_with_system_proxy", return_value={"HTTPS_PROXY": "http://127.0.0.1:7897"}), \
                patch("bridge.agy_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=agy_process())) as spawn:
            self.assertTrue(await self.activator.activate_if_due(agy_windows()))
            command = spawn.call_args.args
            self.assertNotIn("--conversation", command)
            self.assertEqual(command[-1], f"--print={PROMPT}")
            self.assertIn(DEFAULT_AGY_MODEL, command)
            self.assertIn("--sandbox", command)
            self.assertIn("--disable-slash-commands", command)
            self.assertIn("stream-json", command)
            self.assertEqual(spawn.call_args.kwargs["env"]["HTTPS_PROXY"], "http://127.0.0.1:7897")
            self.assertEqual(json.loads(self.path.read_text())["session_id"], SESSION)
            self.assertEqual(self.path.stat().st_mode & 0o777, 0o600)

        restarted = AgyCycleActivator("agy", self.path)
        with patch("bridge.agy_activator.time.time", return_value=NOW + WINDOW_SECONDS), \
                patch("bridge.agy_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=agy_process())) as spawn:
            self.assertTrue(await restarted.activate_if_due(agy_windows()))
            self.assertEqual(spawn.call_args.args[-3:], ("--conversation", SESSION, f"--print={PROMPT}"))

    async def test_cooldown_and_restart_do_not_repeat_request(self):
        with patch("bridge.agy_activator.time.time", return_value=NOW), \
                patch("bridge.agy_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=agy_process())) as spawn:
            await self.activator.activate_if_due(agy_windows())
            with patch("bridge.agy_activator.time.time", return_value=NOW + 300):
                restarted = AgyCycleActivator("agy", self.path)
                self.assertFalse(await restarted.activate_if_due(agy_windows()))
            self.assertEqual(spawn.await_count, 1)

    async def test_identical_expired_snapshot_never_retriggers(self):
        with patch("bridge.agy_activator.time.time", return_value=NOW), \
                patch("bridge.agy_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=agy_process())) as spawn:
            old = agy_windows(used=50, reset=NOW - 100)
            await self.activator.activate_if_due(old)
            with patch("bridge.agy_activator.time.time", return_value=NOW + WINDOW_SECONDS * 2):
                restarted = AgyCycleActivator("agy", self.path)
                self.assertFalse(await restarted.activate_if_due(old))
                self.assertTrue(await restarted.activate_if_due(agy_windows(used=0, reset=None)))
            self.assertEqual(spawn.await_count, 2)

    async def test_attempt_saved_before_spawn_and_spawn_failure_not_retried(self):
        async def fail(*args, **kwargs):
            self.assertEqual(json.loads(self.path.read_text())["attempted_at"], NOW)
            raise OSError("spawn error")

        with patch("bridge.agy_activator.time.time", return_value=NOW), \
                patch("bridge.agy_activator.asyncio.create_subprocess_exec", side_effect=fail) as spawn:
            await self.activator.activate_if_due(agy_windows())
            await AgyCycleActivator("agy", self.path).activate_if_due(agy_windows())
            self.assertEqual(spawn.await_count, 1)
        self.assertEqual(json.loads(self.path.read_text())["last_result"], "failed")

    async def test_failed_turn_still_saves_conversation_and_reuses_it(self):
        events = [
            b"non-json log line\n",
            {"event": "init", "conversation_id": SESSION},
            {"event": "error", "error": "boom"},
        ]
        with patch("bridge.agy_activator.time.time", return_value=NOW), \
                patch("bridge.agy_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=agy_process(events, code=1))):
            await self.activator.activate_if_due(agy_windows())
        self.assertEqual(json.loads(self.path.read_text())["session_id"], SESSION)
        self.assertEqual(self.activator.data["last_result"], "failed")
        with patch("bridge.agy_activator.time.time", return_value=NOW + WINDOW_SECONDS), \
                patch("bridge.agy_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=agy_process([{"event": "error"}], code=1))) as spawn:
            await AgyCycleActivator("agy", self.path).activate_if_due(agy_windows())
            self.assertEqual(spawn.call_args.args[-3:], ("--conversation", SESSION, f"--print={PROMPT}"))

    async def test_non_ok_or_incomplete_output_is_not_success(self):
        for events in (
            [],
            [{"event": "init", "conversation_id": SESSION}],
            [{"event": "result", "result": {"status": "SUCCESS", "conversation_id": SESSION, "response": "No"}}],
        ):
            with patch("bridge.agy_activator.asyncio.create_subprocess_exec",
                       AsyncMock(return_value=agy_process(events))):
                self.assertFalse(await self.activator.execute())

    async def test_timeout_and_cancellation_kill_child(self):
        for cancel in (False, True):
            proc = agy_process()
            proc.returncode = None
            proc.stdout = asyncio.StreamReader()
            self.activator.timeout = 60 if cancel else 0.01
            with patch("bridge.agy_activator.asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
                if cancel:
                    task = asyncio.create_task(self.activator.execute())
                    await asyncio.sleep(0)
                    task.cancel()
                    with self.assertRaises(asyncio.CancelledError):
                        await task
                else:
                    with self.assertRaises(TimeoutError):
                        await self.activator.execute()
            proc.kill.assert_called_once()
            proc.wait.assert_awaited_once()

    async def test_invalid_saved_state_not_overwritten(self):
        for value in ("not json", "[]", '{"session_id": "invalid"}', '{"attempted_at": true}'):
            self.path.write_text(value)
            with self.assertRaises(ValueError):
                AgyCycleActivator("agy", self.path)
            self.assertEqual(self.path.read_text(), value)

    async def test_poll_agy_requeries_after_activation_and_isolates_failure(self):
        state = State(Path(self.tmp.name) / "state.json")
        link = BridgeLink()
        link.connect()
        args = SimpleNamespace(interval=300)

        idle = agy_windows(used=0, reset=None)
        active = agy_windows(used=0.5, reset=NOW + 17990)

        provider = Mock()
        provider.last_source = "agy-cli"
        provider.fetch = AsyncMock(side_effect=[idle, active])
        activator = Mock()
        activator.activate_if_due = AsyncMock(return_value=True)

        task = asyncio.create_task(poll_agy(args, state, provider, link, activator))
        while link.agy_revision < 2:
            await asyncio.sleep(0.01)
        task.cancel()
        with self.assertRaises(asyncio.CancelledError):
            await task
        self.assertEqual(provider.fetch.await_count, 2)
        self.assertAlmostEqual(state.agy_windows()["gh"].remaining, 99.5, places=1)
        self.assertEqual(state.agy_windows()["gh"].reset, NOW + 17990)
        activator.record_cycle_reset.assert_called_once_with(NOW + 17990)

        # Mid-cycle activation with an earlier quantized reset can re-activate as soon as that reset passes.
        mid_cycle = AgyCycleActivator("agy", Path(self.tmp.name) / "mid.json")
        mid_cycle.data = {
            "session_id": SESSION,
            "attempted_at": NOW,
            "active_reset": NOW + 15000,  # Quantized reset 4h10m after attempted_at
            "last_result": "ok",
        }
        self.assertFalse(mid_cycle.due(agy_windows(used=0, reset=None), NOW + 14999))
        self.assertTrue(mid_cycle.due(agy_windows(used=0, reset=None), NOW + 15000))

        # Failed activation without reset still enforces full 5h cooldown.
        failed_act = AgyCycleActivator("agy", Path(self.tmp.name) / "failed.json")
        failed_act.data = {"attempted_at": NOW, "last_result": "failed"}
        self.assertFalse(failed_act.due(agy_windows(used=0, reset=None), NOW + 15000))
        self.assertTrue(failed_act.due(agy_windows(used=0, reset=None), NOW + WINDOW_SECONDS))

        # Activation/re-fetch failure retains the successful initial fetch without marking stale.
        provider2 = Mock()
        provider2.last_source = "agy-cli"
        provider2.fetch = AsyncMock(side_effect=[idle, QuotaError("network_error")])
        activator2 = Mock()
        activator2.activate_if_due = AsyncMock(return_value=True)
        link2 = BridgeLink()
        link2.connect()
        task2 = asyncio.create_task(poll_agy(args, state, provider2, link2, activator2))
        while provider2.fetch.await_count < 2:
            await asyncio.sleep(0.01)
        await asyncio.sleep(0.02)
        task2.cancel()
        with self.assertRaises(asyncio.CancelledError):
            await task2
        self.assertEqual(state.agy_windows()["gh"].source, "api")
