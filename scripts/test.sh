#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
"${BRIDGE_PYTHON:-python3}" -m unittest discover -s tests -p "test_*.py" -v
: "${CJSON_DIR:?Set CJSON_DIR to a directory containing cJSON.c and cJSON.h}"
mkdir -p .state
cc -c "$CJSON_DIR/cJSON.c" -I"$CJSON_DIR" -o .state/cJSON.o
c++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/codex_monitor -I"$CJSON_DIR" \
  tests/test_quota.cpp firmware/codex_monitor/model/quota_state.cpp .state/cJSON.o -o .state/test_quota
.state/test_quota
c++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/codex_monitor tests/test_idle_profile.cpp -o .state/test_idle_profile
.state/test_idle_profile
python3 -m py_compile scripts/prepare_firmware.py scripts/update_firmware.py scripts/integrate_power.py
echo 'Quota model checks passed.'
