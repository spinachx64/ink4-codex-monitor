#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LABEL=com.spinach.ink4-codex-monitor
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
PYTHON="${BRIDGE_PYTHON:-$ROOT/.venv/bin/python}"
CODEX="${CODEX_BIN:-$(command -v codex || true)}"
AGY="${AGY_BIN:-$(command -v agy || ([ -x "$HOME/.local/bin/agy" ] && echo "$HOME/.local/bin/agy") || true)}"
if [ ! -x "$PYTHON" ] || [ -z "$CODEX" ]; then
    echo 'Create .venv, install requirements.txt and install Codex CLI first.' >&2; exit 1
fi
mkdir -p "$HOME/Library/LaunchAgents" "$HOME/Library/Logs/CodexInkMonitor"
python3 - "$ROOT" "$PYTHON" "$CODEX" "$AGY" "$PLIST" <<'PY'
import os, plistlib, sys
from pathlib import Path
root,python,codex,agy,output=sys.argv[1:]
logs=Path.home()/'Library/Logs/CodexInkMonitor'
program_args=[python,'-m','bridge.main','--codex',codex,'--log-file',str(logs/'bridge.log')]
if agy:
    program_args+=['--agy',agy]
path_entries=os.environ.get('PATH','').split(':')
local_bin=str(Path.home()/'.local/bin')
if local_bin not in path_entries:
    path_entries.append(local_bin)
config={
    'Label':'com.spinach.ink4-codex-monitor',
    'ProgramArguments':program_args,
    'WorkingDirectory':root,
    'RunAtLoad':True,'KeepAlive':True,'ThrottleInterval':30,
    'ProcessType':'Background',
    'EnvironmentVariables':{'PATH':':'.join(p for p in path_entries if p),'PYTHONUNBUFFERED':'1'},
    # App logs rotate themselves. Do not duplicate them into an unbounded launchd log.
    'StandardOutPath':'/dev/null','StandardErrorPath':'/dev/null',
}
if os.environ.get('CODEX_HOME'):
    config['EnvironmentVariables']['CODEX_HOME']=os.environ['CODEX_HOME']
with open(output,'wb') as f: plistlib.dump(config,f)
PY
launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
sleep 1
launchctl bootstrap "gui/$(id -u)" "$PLIST"
printf 'Started %s; log: %s/Library/Logs/CodexInkMonitor/bridge.log\n' "$LABEL" "$HOME"
