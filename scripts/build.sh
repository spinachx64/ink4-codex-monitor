#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v idf.py >/dev/null && [[ -f .state/idf-env.sh ]]; then
  source .state/idf-env.sh
fi
command -v idf.py >/dev/null || { echo 'Activate ESP-IDF v5.5.4 export.sh first.' >&2; exit 1; }
if [[ ! -d build-firmware ]]; then
  python3 scripts/prepare_firmware.py
fi
cd build-firmware
idf.py build "$@"
