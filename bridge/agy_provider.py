"""Read-only Antigravity CLI (/usage) quota client reusing existing agy login state."""
import asyncio
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import urllib.request
from .quota_normalizer import Window, number
from .quota_provider import QuotaError, classify

AGY_KEYS = ("gh", "gw", "ch", "cw")
PROXY_ENV_KEYS = ("http_proxy", "https_proxy", "all_proxy", "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY")


def subprocess_env_with_system_proxy(base_env=None):
    """Build subprocess env inheriting explicit proxy vars or macOS live system proxy."""
    env = dict(os.environ if base_env is None else base_env)
    if any(env.get(k) for k in PROXY_ENV_KEYS):
        return env
    proxies = urllib.request.getproxies()
    if proxies.get("http"):
        env["http_proxy"] = env["HTTP_PROXY"] = proxies["http"]
    if proxies.get("https"):
        env["https_proxy"] = env["HTTPS_PROXY"] = proxies["https"]
    if not proxies.get("http") and not proxies.get("https") and proxies.get("socks"):
        socks = proxies["socks"]
        if socks.startswith("http://"):
            socks = "socks5h://" + socks[len("http://"):]
        env["all_proxy"] = env["ALL_PROXY"] = socks
    return env


def default_agy_binary():
    found = shutil.which("agy")
    if found:
        return found
    local_bin = Path.home() / ".local" / "bin" / "agy"
    if local_bin.is_file() and os.access(local_bin, os.X_OK):
        return str(local_bin)
    return None


def _parse_iso_reset(value):
    if not isinstance(value, str) or not value.strip():
        return None
    try:
        dt = datetime.fromisoformat(value.strip().replace("Z", "+00:00"))
    except ValueError:
        return None
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    epoch = int(dt.timestamp())
    return epoch if 0 < epoch < 10**11 else None


def _bucket_key(group_name, bucket):
    bid = str(bucket.get("id") or "").lower()
    if bid == "gemini-5h":
        return "gh"
    if bid == "gemini-weekly":
        return "gw"
    if bid == "3p-5h":
        return "ch"
    if bid == "3p-weekly":
        return "cw"
    window = str(bucket.get("window") or "").lower()
    gname = str(group_name or "").lower()
    if "gemini" in gname:
        if window == "5h":
            return "gh"
        if window == "weekly":
            return "gw"
    if "claude" in gname or "gpt" in gname or "3p" in gname:
        if window == "5h":
            return "ch"
        if window == "weekly":
            return "cw"
    return None


def normalize_agy(payload):
    if not isinstance(payload, dict):
        raise ValueError("invalid agy quota response")
    status = payload.get("status")
    if status is not None and status != "SUCCESS":
        raise QuotaError(classify(str(payload.get("error") or status)))
    command = payload.get("command")
    data = command.get("data") if isinstance(command, dict) else None
    groups = data.get("groups") if isinstance(data, dict) else None
    if not isinstance(groups, list):
        raise ValueError("missing agy quota groups")
    result = {k: None for k in AGY_KEYS}
    for group in groups:
        if not isinstance(group, dict):
            continue
        group_name = group.get("name", "")
        buckets = group.get("buckets")
        if not isinstance(buckets, list):
            continue
        for bucket in buckets:
            if not isinstance(bucket, dict):
                continue
            key = _bucket_key(group_name, bucket)
            if key is None:
                continue
            frac = bucket.get("remaining_fraction")
            if number(frac):
                clamped = max(0.0, min(1.0, float(frac)))
                used = round((1.0 - clamped) * 100.0, 1)
            else:
                used = None
            # Unused buckets (100% remaining without an active usage description)
            # carry a rolling sliding reset timestamp rather than an active timer.
            desc = bucket.get("description")
            idle_full = used == 0.0 and not (isinstance(desc, str) and desc.strip())
            reset = None if idle_full else _parse_iso_reset(bucket.get("reset_time"))
            result[key] = Window(used, reset, "api" if used is not None else "unavailable")
    return result


class AgyQuotaProvider:
    def __init__(self, agy=None, timeout=60):
        self.agy = agy or default_agy_binary() or "agy"
        self.timeout = timeout
        self.last_source = None

    async def fetch(self):
        proc = await asyncio.create_subprocess_exec(
            self.agy, "--print", "/usage", "--output-format", "json",
            "--print-timeout", f"{max(10, int(self.timeout - 5))}s",
            stdin=asyncio.subprocess.DEVNULL,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            env=subprocess_env_with_system_proxy(),
            limit=2**20,
        )
        try:
            async with asyncio.timeout(self.timeout):
                stdout, stderr = await proc.communicate()
        except TimeoutError:
            if proc.returncode is None:
                try:
                    proc.terminate()
                except ProcessLookupError:
                    pass
                try:
                    await asyncio.wait_for(proc.wait(), 3)
                except TimeoutError:
                    proc.kill()
                    await proc.wait()
            raise QuotaError("network_error") from None
        if proc.returncode != 0:
            err_text = (stderr or stdout or b"").decode(errors="ignore")
            raise QuotaError(classify(err_text))
        text = stdout.decode(errors="replace").strip()
        if not text:
            raise QuotaError("network_error")
        # Tolerate any stray log lines before the final JSON line.
        parsed = None
        for line in reversed(text.splitlines()):
            line = line.strip()
            if line.startswith("{"):
                try:
                    parsed = json.loads(line)
                    break
                except ValueError:
                    continue
        if parsed is None:
            raise ValueError("invalid agy json output")
        result = normalize_agy(parsed)
        self.last_source = "agy-cli"
        return result
