"""Build and test public code without generated requirements paperwork."""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.reference.host_slots import slot  # noqa: E402


def run_all(commands: list[list[str]]) -> list[dict]:
    results = []
    for command in commands:
        print('Running: ' + ' '.join(command), flush=True)
        result = subprocess.run(command, cwd=ROOT, check=False)
        results.append({'command': command, 'exit_code': result.returncode})
        if result.returncode:
            raise SystemExit(result.returncode)
    return results


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preset', choices=['debug', 'sanitize', 'release', 'all'], default='debug')
    parser.add_argument('--format', action='store_true', help='also check repository formatting')
    args = parser.parse_args()
    commands = [[sys.executable, 'tools/repository/validate.py']]
    if args.format:
        commands.append([sys.executable, 'tools/repository/format.py', '--check'])
    presets = ['debug', 'sanitize', 'release'] if args.preset == 'all' else [args.preset]
    for preset in presets:
        commands += [['cmake', '--preset', preset], ['cmake', '--build', '--preset', preset],
                     ['ctest', '--preset', preset]]
    with slot('build'):
        run_all(commands)
    print('Public build and tests passed; no original-game match is claimed.')


if __name__ == '__main__':
    main()
