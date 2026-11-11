#!/usr/bin/env python3
"""Prepare an isolated official-firmware copy; never modify the source tree."""
import argparse
from pathlib import Path
import shutil
import subprocess
from integrate_home import integrate_home
from patch_display import patch_display
from update_firmware import configure
from integrate_power import integrate_power

ROOT = Path(__file__).resolve().parents[1]

def prepare(source, output):
    source, output = source.resolve(), output.resolve()
    if source == output or source in output.parents or output in source.parents:
        raise ValueError("source and output must be separate, non-nested directories")
    if output.exists():
        raise ValueError(f"output already exists: {output}; choose a new --output directory")
    if not (source / 'main/display/screen/home_screen/home_screen.cc').is_file():
        raise ValueError("not a supported Metalio firmware checkout")
    shutil.copytree(source, output, ignore=shutil.ignore_patterns(
        '.git', 'build', '.pio', 'managed_components', 'node_modules', '.venv', 'daily-builds', '__pycache__'))
    shutil.copytree(ROOT / 'firmware/codex_monitor', output / 'main/codex_monitor')
    lock = ROOT / 'firmware/dependencies.lock'
    if lock.exists():
        shutil.copyfile(lock, output / 'dependencies.lock')
    cmake = output / 'main/CMakeLists.txt'
    text = cmake.read_text()
    marker = 'idf_component_register(SRCS ${SOURCES}'
    if marker not in text:
        raise ValueError('unsupported CMake structure')
    text = text.replace(marker, '''list(APPEND SOURCES "codex_monitor/codex_screen.cc"
                    "codex_monitor/ble/ble_nus.cpp"
                    "codex_monitor/model/quota_state.cpp")
file(GLOB CODEX_FONTS "codex_monitor/fonts/codex_inter_*.c")
list(APPEND SOURCES "codex_monitor/view/quota_view.cpp" "codex_monitor/view/codex_icon.c" ${CODEX_FONTS})
list(APPEND INCLUDE_DIRS "codex_monitor")
idf_component_register(SRCS ${SOURCES}''', 1)
    cmake.write_text(text)
    integrate_home(source, output)
    patch_display(source, output)
    integrate_power(output)
    configure(output / 'sdkconfig')
    revision = subprocess.run(['git', '-C', str(source), 'rev-parse', 'HEAD'],
                              capture_output=True, text=True, check=True).stdout.strip()
    (output / 'CODEX_MONITOR_BASELINE.txt').write_text(revision + '\n')
    print(f'Prepared {output}\nOfficial revision: {revision}')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT.parent / 'Metalio-E-INK4')
    parser.add_argument('--output', type=Path, default=ROOT / 'build-firmware')
    args = parser.parse_args()
    prepare(args.source, args.output)
