import asyncio
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import AsyncMock, Mock, patch

from bridge.cycle_activator import CycleActivator, PROMPT, WINDOW_SECONDS
from bridge.quota_normalizer import Window

SESSION = "0199a213-81c0-7800-8aa1-bbab2a035a53"
NOW = 1800000000


def windows(reset=NOW + WINDOW_SECONDS, used=0, source="api"):
    return {"h": Window(used, reset, source), "w": None}


def process(events=None, code=0):
    proc = AsyncMock()
    proc.kill = Mock()
    proc.returncode = code
    if events is None:
        events = [{"type": "thread.started", "thread_id": SESSION},
                  {"type": "item.completed", "item": {"type": "agent_message", "text": "OK"}},
                  {"type": "turn.completed"}]
    proc.stdout = asyncio.StreamReader()
    for event in events:
        proc.stdout.feed_data((json.dumps(event) + "\n").encode())
    proc.stdout.feed_eof()
    return proc


class ActivationTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / "state.activation.json"
        self.activator = CycleActivator("codex", self.path)

    async def test_only_fresh_expired_or_full_second_window_is_eligible(self):
        for value in ({"h": None}, windows(used=None), windows(reset=None),
                      windows(source="stale"), windows(source="estimated"),
                      windows(reset=NOW + WINDOW_SECONDS - 30),  # UI still shows 05h 00m.
                      windows(reset=NOW + WINDOW_SECONDS, used=1),
                      windows(reset=NOW + 30, used=100)):
            self.assertFalse(self.activator.due(value, NOW), value)
        self.assertTrue(self.activator.due(windows(), NOW))
        self.assertTrue(self.activator.due(windows(reset=NOW - 1, used=99), NOW))

    async def test_create_persist_then_resume_same_session_after_restart(self):
        with patch("bridge.cycle_activator.time.time", return_value=NOW), \
                patch("bridge.cycle_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=process())) as spawn:
            self.assertTrue(await self.activator.activate_if_due(windows()))
            command = spawn.call_args.args
            self.assertNotIn("resume", command)
            self.assertEqual(command[-1], PROMPT)
            self.assertIn("--json", command)
            self.assertIn("read-only", command)
            self.assertEqual(json.loads(self.path.read_text())["session_id"], SESSION)
            self.assertEqual(self.path.stat().st_mode & 0o777, 0o600)
        restarted = CycleActivator("codex", self.path)
        with patch("bridge.cycle_activator.time.time", return_value=NOW + WINDOW_SECONDS), \
                patch("bridge.cycle_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=process())) as spawn:
            self.assertTrue(await restarted.activate_if_due(windows(reset=NOW + WINDOW_SECONDS)))
            self.assertEqual(spawn.call_args.args[-3:], ("resume", SESSION, PROMPT))

    async def test_moving_full_reset_and_restart_do_not_repeat_request(self):
        with patch("bridge.cycle_activator.time.time", return_value=NOW), \
                patch("bridge.cycle_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=process())) as spawn:
            await self.activator.activate_if_due(windows())
            with patch("bridge.cycle_activator.time.time", return_value=NOW + 300):
                restarted = CycleActivator("codex", self.path)
                self.assertFalse(await restarted.activate_if_due(windows(reset=NOW + 300 + WINDOW_SECONDS)))
            self.assertEqual(spawn.await_count, 1)

    async def test_identical_expired_snapshot_never_retriggers(self):
        with patch("bridge.cycle_activator.time.time", return_value=NOW), \
                patch("bridge.cycle_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=process())) as spawn:
            old = windows(reset=NOW - 100)
            await self.activator.activate_if_due(old)
            with patch("bridge.cycle_activator.time.time", return_value=NOW + WINDOW_SECONDS * 2):
                restarted = CycleActivator("codex", self.path)
                self.assertFalse(await restarted.activate_if_due(old))
                self.assertTrue(await restarted.activate_if_due(windows(reset=NOW + WINDOW_SECONDS)))
            self.assertEqual(spawn.await_count, 2)

    async def test_attempt_saved_before_spawn_and_spawn_failure_is_not_retried(self):
        async def fail(*args, **kwargs):
            self.assertEqual(json.loads(self.path.read_text())["attempted_at"], NOW)
            raise OSError("private subprocess details")
        with patch("bridge.cycle_activator.time.time", return_value=NOW), \
                patch("bridge.cycle_activator.asyncio.create_subprocess_exec", side_effect=fail) as spawn:
            await self.activator.activate_if_due(windows())
            await CycleActivator("codex", self.path).activate_if_due(windows())
            self.assertEqual(spawn.await_count, 1)
        self.assertEqual(json.loads(self.path.read_text())["last_result"], "failed")

    async def test_failed_turn_still_saves_session_and_never_creates_replacement(self):
        events = [{"type": "thread.started", "thread_id": SESSION}, {"type": "turn.failed"}]
        with patch("bridge.cycle_activator.time.time", return_value=NOW), \
                patch("bridge.cycle_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=process(events, code=1))):
            await self.activator.activate_if_due(windows())
        self.assertEqual(json.loads(self.path.read_text())["session_id"], SESSION)
        self.assertEqual(self.activator.data["last_result"], "failed")
        with patch("bridge.cycle_activator.time.time", return_value=NOW + WINDOW_SECONDS), \
                patch("bridge.cycle_activator.asyncio.create_subprocess_exec",
                      AsyncMock(return_value=process([{"type": "turn.failed"}], code=1))) as spawn:
            await CycleActivator("codex", self.path).activate_if_due(windows())
            self.assertEqual(spawn.call_args.args[-3:], ("resume", SESSION, PROMPT))
            self.assertEqual(spawn.await_count, 1)

    async def test_non_ok_or_incomplete_output_is_not_success(self):
        for events in ([], [{"type": "turn.completed"}],
                       [{"type": "thread.started", "thread_id": SESSION},
                        {"type": "item.completed", "item": {"type": "agent_message", "text": "No"}},
                        {"type": "turn.completed"}]):
            with patch("bridge.cycle_activator.asyncio.create_subprocess_exec",
                       AsyncMock(return_value=process(events))):
                self.assertFalse(await self.activator.execute())

    async def test_timeout_and_cancellation_kill_child_preserving_session(self):
        for cancel in (False, True):
            proc = process()
            proc.returncode = None
            proc.stdout = asyncio.StreamReader()  # Never completes.
            self.activator.timeout = 60 if cancel else 0.01
            with patch("bridge.cycle_activator.asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
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

    async def test_invalid_saved_state_is_not_silently_replaced(self):
        for value in ("not json", "[]", '{"session_id": "invalid"}', '{"attempted_at": true}'):
            self.path.write_text(value)
            with self.assertRaises(ValueError):
                CycleActivator("codex", self.path)
            self.assertEqual(self.path.read_text(), value)
