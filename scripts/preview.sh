#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S tests/preview -B .state/preview -G Ninja
cmake --build .state/preview -j 6
.state/preview/preview
.state/build-venv/bin/python - <<'PY'
from PIL import Image
from pathlib import Path
for p in Path('docs/ui').glob('*.png.ppm'):
    Image.open(p).save(p.with_suffix(''))
    p.unlink()
PY
