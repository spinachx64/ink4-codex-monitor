#!/usr/bin/env python3
"""Update the generated app, managed configuration and checked low-power integration."""
import argparse
from pathlib import Path
import shutil
try:
    from .integrate_power import integrate_power
except ImportError:
    from integrate_power import integrate_power

ROOT = Path(__file__).resolve().parents[1]
CONFIG_MARKER = '# Codex monitor managed configuration'
MANAGED_CONFIG = {
    'CONFIG_BT_ENABLED': 'y',
    'CONFIG_BT_NIMBLE_ENABLED': 'y',
    'CONFIG_BT_BLUEDROID_ENABLED': 'n',
    'CONFIG_BT_NIMBLE_ROLE_PERIPHERAL': 'y',
    'CONFIG_BT_NIMBLE_ROLE_BROADCASTER': 'y',
    'CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL': 'y',
    'CONFIG_BT_NIMBLE_MAX_CONNECTIONS': '1',
    'CONFIG_BT_NIMBLE_ROLE_CENTRAL': 'n',
    'CONFIG_BT_NIMBLE_ROLE_OBSERVER': 'n',
    'CONFIG_BT_CTRL_MODEM_SLEEP': 'y',
    'CONFIG_BT_CTRL_MODEM_SLEEP_MODE_1': 'y',
    'CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL': 'y',
    'CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP': 'y',
    'CONFIG_PM_ENABLE': 'y',
    'CONFIG_FREERTOS_USE_TICKLESS_IDLE': 'y',
    'CONFIG_PM_PROFILING': 'y',
    # SDK PM profiling prints 64-bit counters; nano printf cannot parse them.
    'CONFIG_NEWLIB_NANO_FORMAT': 'n',
    'CONFIG_LIBC_NEWLIB_NANO_FORMAT': 'n',  # IDF 5.5 canonical name (legacy option is an alias).
    'CONFIG_BT_CTRL_LPCLK_SEL_EXT_32K_XTAL': 'n',
    'CONFIG_BT_CTRL_LPCLK_SEL_RTC_SLOW': 'n',
    'CONFIG_LV_FONT_MONTSERRAT_28': 'y',
}


def configure(config):
    original = config.read_text()
    kept = []
    seen = set()
    for line in original.splitlines():
        key = line[2:-11] if line.startswith('# ') and line.endswith(' is not set') else line.split('=', 1)[0]
        if key not in MANAGED_CONFIG:
            kept.append(line)
        elif key not in seen:
            seen.add(key)
            value = MANAGED_CONFIG[key]
            kept.append(f'# {key} is not set' if value == 'n' else f'{key}={value}')
    # Kconfig omits disabled choices whose dependencies are not enabled. Do not
    # reinsert them on every build, causing needless configuration regeneration.
    missing = [f'{key}={value}' for key, value in MANAGED_CONFIG.items()
               if key not in seen and value != 'n']
    if missing:
        if kept and kept[-1]:
            kept.append('')
        if CONFIG_MARKER not in kept:
            kept.append(CONFIG_MARKER)
        kept.extend(missing)
    updated = '\n'.join(kept) + '\n'
    if updated != original:
        config.write_text(updated)


def update(output):
    output = output.resolve()
    if not (output / 'CODEX_MONITOR_BASELINE.txt').is_file():
        raise ValueError('not a prepared Codex firmware copy; run prepare_firmware.py first')
    destination = output / 'main/codex_monitor'
    if destination.is_symlink() or not destination.is_dir():
        raise ValueError('expected a generated main/codex_monitor directory')
    print(f'Updating {destination} and applying checked Codex power/touch integration.')
    shutil.copytree(ROOT / 'firmware/codex_monitor', destination, dirs_exist_ok=True)
    configure(output / 'sdkconfig')
    integrate_power(output)
    print('Applied managed sdkconfig options (including Bluetooth modem sleep).')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build-firmware')
    update(parser.parse_args().output)
