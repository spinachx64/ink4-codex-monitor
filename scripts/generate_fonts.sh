#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .state
curl -fL --retry 3 'https://raw.githubusercontent.com/google/fonts/main/ofl/inter/Inter%5Bopsz,wght%5D.ttf' -o .state/Inter-variable.ttf
.state/build-venv/bin/python - <<'PY'
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
font=TTFont('.state/Inter-variable.ttf')
instantiateVariableFont(font,{'wght':900,'opsz':32},inplace=True).save('.state/Inter-Black.ttf')
PY
for size in 12 14 21 34 42 60 90 108; do
  symbols='ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789% /:.-!dhm'
  [[ "$size" != 90 ]] || symbols='0123456789'
  npx --yes lv_font_conv@1.5.3 --font .state/Inter-Black.ttf --size "$size" --bpp 2 \
    --format lvgl --symbols "$symbols" --no-compress -o "firmware/codex_monitor/fonts/codex_inter_$size.c"
done
