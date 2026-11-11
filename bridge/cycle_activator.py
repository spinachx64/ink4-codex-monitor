"""Restart expired 5H windows with a persisted, dedicated Codex session."""
import asyncio
import json
import logging
import os
from pathlib import Path
import time
from uuid import UUID

log = logging.getLogger(__name__)
PROMPT = "Reply with OK only."
WINDOW_SECONDS = 5 * 3600


class CycleActivator:
    def __init__(self, codex, path, timeout=120):
        self.codex, self.path, self.timeout = codex, Path(path), timeout
        self.path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        try:
            self.data = json.loads(self.path.read_text())
        except FileNotFoundError:
            self.data = {}
        # Do not silently discard a saved session and start another one.
        if not isinstance(self.data, dict):
            raise ValueError("invalid activation state")
        if self.data.get("session_id") is not None:
            UUID(self.data["session_id"])
        for key in ("attempted_at", "attempted_reset", "expired_reset"):
            if key in self.data and (type(self.data[key]) is not int or self.data[key] < 0):
                raise ValueError("invalid activation timestamp")

    def save(self):
        tmp = self.path.with_suffix(".tmp")
        fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(fd, "w") as output:
            os.fchmod(output.fileno(), 0o600)
            json.dump(self.data, output, allow_nan=False)
        os.replace(tmp, self.path)

    def due(self, windows, now):
        window = windows.get("h")
        if window is None or window.source != "api" or window.used is None or window.reset is None:
            return False
        # Use seconds, never the UI's rounded-up minute display. A full window
        # is only a heuristic: allow one second of API timestamp quantization.
        full = window.used == 0 and abs(window.reset - now - WINDOW_SECONDS) <= 1
        expired = window.reset <= now
        if not (full or expired):
            return False
        attempted = self.data.get("attempted_at")
        if attempted is not None and now - attempted < WINDOW_SECONDS:
            return False
        # A repeated expired API snapshot is not evidence of another cycle.
        return not (expired and self.data.get("expired_reset") == window.reset)

    async def activate_if_due(self, windows):
        now = int(time.time())
        if not self.due(windows, now):
            return False
        self.data.update(attempted_at=now, attempted_reset=windows["h"].reset,
                         last_result="pending")
        if windows["h"].reset <= now:
            self.data["expired_reset"] = windows["h"].reset
        self.save()  # Survives crashes/restarts; no repeated inference this cycle.
        log.info("5H cycle ended; automatic activation (%s)",
                 "resume" if self.data.get("session_id") else "new session")
        try:
            success = await self.execute()
            self.data["last_result"] = "ok" if success else "failed"
            self.save()
            log.info("5H automatic activation %s", "completed" if success else "failed; no immediate retry")
        except (OSError, ValueError, TimeoutError):
            self.data["last_result"] = "failed"
            self.save()
            log.warning("5H automatic activation failed; no immediate retry")
        return True  # Re-read quotas even if an accepted turn failed locally.

    async def execute(self):
        session = self.data.get("session_id")
        command = [self.codex, "exec", "--sandbox", "read-only", "--json", "--skip-git-repo-check"]
        if session:
            command += ["resume", session]
        command.append(PROMPT)
        # Keep the probe outside the repository and its project instructions.
        workdir = self.path.parent / "activation-workspace"
        workdir.mkdir(mode=0o700, exist_ok=True)
        proc = await asyncio.create_subprocess_exec(
            *command, cwd=workdir, stdin=asyncio.subprocess.DEVNULL,
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL,
            limit=2**20)
        completed, failed, reply = False, False, None
        try:
            async with asyncio.timeout(self.timeout):
                while line := await proc.stdout.readline():
                    event = json.loads(line)
                    if not isinstance(event, dict):
                        raise ValueError("invalid Codex event")
                    if event.get("type") == "thread.started":
                        if not isinstance(event.get("thread_id"), str):
                            raise ValueError("missing activation session")
                        ident = str(UUID(event["thread_id"]))
                        if session and ident != session:
                            raise ValueError("unexpected activation session")
                        session = ident
                        self.data["session_id"] = ident
                        self.save()  # Save immediately, even if this turn fails.
                    elif event.get("type") == "turn.completed":
                        completed = True
                    elif event.get("type") in ("turn.failed", "error"):
                        failed = True
                    elif event.get("type") == "item.completed":
                        item = event.get("item") or {}
                        if not isinstance(item, dict):
                            raise ValueError("invalid Codex item")
                        if item.get("type") == "agent_message":
                            reply = item.get("text")
                await proc.wait()
            return (proc.returncode == 0 and completed and not failed and bool(session)
                    and isinstance(reply, str) and reply.strip() == "OK")
        finally:
            if proc.returncode is None:
                try:
                    proc.kill()
                except ProcessLookupError:
                    pass
                await proc.wait()
