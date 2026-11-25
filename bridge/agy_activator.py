"""Restart expired/idle AGY Gemini 5H windows with a persisted, dedicated agy conversation."""
import asyncio
import json
import logging
import os
from pathlib import Path
import time
from uuid import UUID

from .agy_provider import subprocess_env_with_system_proxy
from .cycle_activator import PROMPT, WINDOW_SECONDS

log = logging.getLogger(__name__)
DEFAULT_AGY_MODEL = "gemini-4-hydrogen-low"


def _extract_conversation_id(event):
    for container in (event, event.get("result"), event.get("step_update"), event.get("init")):
        if isinstance(container, dict):
            cid = container.get("conversation_id")
            if isinstance(cid, str) and cid.strip():
                return cid.strip()
    return None


class AgyCycleActivator:
    def __init__(self, agy, path, model=DEFAULT_AGY_MODEL, timeout=120):
        self.agy = agy
        self.path = Path(path)
        self.model = model
        self.timeout = timeout
        self.path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        try:
            self.data = json.loads(self.path.read_text())
        except FileNotFoundError:
            self.data = {}
        # Do not silently discard a saved conversation and create a new one.
        if not isinstance(self.data, dict):
            raise ValueError("invalid AGY activation state")
        if self.data.get("session_id") is not None:
            UUID(self.data["session_id"])
        for key in ("attempted_at", "attempted_reset", "expired_reset"):
            val = self.data.get(key)
            if val is not None and (type(val) is not int or val < 0):
                raise ValueError("invalid AGY activation timestamp")

    def save(self):
        tmp = self.path.with_suffix(".tmp")
        fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(fd, "w") as output:
            os.fchmod(output.fileno(), 0o600)
            json.dump(self.data, output, allow_nan=False)
        os.replace(tmp, self.path)

    def due(self, windows, now):
        window = windows.get("gh")
        if window is None or window.source != "api" or window.used is None:
            return False
        # AGY reports unused 100% windows without an active timer (reset=None)
        # or with a sliding 5H timestamp; expired windows have reset <= now.
        idle_full = window.used == 0 and window.reset is None
        full_window = (window.used == 0 and window.reset is not None
                       and abs(window.reset - now - WINDOW_SECONDS) <= 1)
        expired = window.reset is not None and window.reset <= now
        if not (idle_full or full_window or expired):
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
        window = windows["gh"]
        self.data["attempted_at"] = now
        self.data["last_result"] = "pending"
        if window.reset is not None:
            self.data["attempted_reset"] = window.reset
            if window.reset <= now:
                self.data["expired_reset"] = window.reset
        else:
            self.data.pop("attempted_reset", None)
        self.save()  # Survives crashes/restarts; no repeated inference this cycle.
        log.info("AGY Gemini 5H cycle ended; automatic activation (%s, model=%s)",
                 "resume" if self.data.get("session_id") else "new conversation", self.model)
        try:
            success = await self.execute()
            self.data["last_result"] = "ok" if success else "failed"
            self.save()
            log.info("AGY Gemini 5H automatic activation %s",
                     "completed" if success else "failed; no immediate retry")
        except (OSError, ValueError, TimeoutError):
            self.data["last_result"] = "failed"
            self.save()
            log.warning("AGY Gemini 5H automatic activation failed; no immediate retry")
        return True  # Re-read quotas even if an accepted turn failed locally.

    async def execute(self):
        session = self.data.get("session_id")
        command = [
            self.agy,
            "--model", self.model,
            "--effort", "low",
            "--sandbox",
            "--disable-slash-commands",
            "--output-format", "stream-json",
            "--print-timeout", f"{max(15, int(self.timeout - 5))}s",
        ]
        if session:
            command += ["--conversation", session]
        command.append(f"--print={PROMPT}")
        # Keep the probe outside the repository and its project instructions.
        workdir = self.path.parent / "agy-activation-workspace"
        workdir.mkdir(mode=0o700, exist_ok=True)
        proc = await asyncio.create_subprocess_exec(
            *command,
            cwd=workdir,
            stdin=asyncio.subprocess.DEVNULL,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.DEVNULL,
            env=subprocess_env_with_system_proxy(),
            limit=2**20,
        )
        completed, failed, reply = False, False, None
        try:
            async with asyncio.timeout(self.timeout):
                while raw_line := await proc.stdout.readline():
                    stripped = raw_line.strip()
                    if not stripped.startswith(b"{"):
                        continue
                    event = json.loads(stripped)
                    if not isinstance(event, dict):
                        raise ValueError("invalid AGY event")
                    cid = _extract_conversation_id(event)
                    if cid is not None:
                        ident = str(UUID(cid))
                        if session and ident != session:
                            raise ValueError("unexpected AGY activation conversation")
                        if not session:
                            session = ident
                            self.data["session_id"] = ident
                            self.save()  # Save immediately, even if this turn fails.
                    ev_name = event.get("event") or event.get("type")
                    if ev_name == "result" and isinstance(event.get("result"), dict):
                        res = event["result"]
                        status = res.get("status")
                        if status == "SUCCESS":
                            completed = True
                        elif status is not None:
                            failed = True
                        if isinstance(res.get("response"), str):
                            reply = res["response"]
                    elif ev_name in ("error", "turn.failed"):
                        failed = True
                    elif "status" in event:
                        status = event.get("status")
                        if status == "SUCCESS":
                            completed = True
                        elif status is not None:
                            failed = True
                        if isinstance(event.get("response"), str):
                            reply = event["response"]
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
