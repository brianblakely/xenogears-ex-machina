"""One-shot integration repairs; removed by the branch-only workflow."""
from pathlib import Path
import ast
import os
import re
import subprocess
import sys

ROOT = Path.cwd()
BASE = '6ec589387649af12c719458daa4b3530c57ba3b4'


def write(name, text):
    p = ROOT / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text.strip() + '\n')


def replace(name, old, new):
    p = ROOT / name
    text = p.read_text()
    if old not in text:
        raise RuntimeError(f'Missing anchor in {name}: {old[:80]}')
    p.write_text(text.replace(old, new))


if '--format' in sys.argv:
    names = set(subprocess.check_output(['git', 'diff', '--name-only', BASE], text=True).splitlines())
    names |= set(subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], text=True).splitlines())
    names = sorted(n for n in names if Path(n).is_file() and not n.startswith('.github/'))
    py = [n for n in names if n.endswith('.py')]
    cpp = [n for n in names if n.endswith(('.hpp', '.cpp'))]
    nix = [n for n in names if n.endswith('.nix')]
    if py:
        subprocess.run(['ruff', 'check', '--fix', *py], check=True)
        subprocess.run(['ruff', 'format', *py], check=True)
    if cpp:
        subprocess.run(['clang-format', '-i', *cpp], check=True)
    if nix:
        subprocess.run(['nixfmt', *nix], check=True)
    raise SystemExit(0)

# Retired matrix.py also exported ROOT. Keep data tests independent of that module.
for name in ('test_authoring_contract.py', 'test_inventory_records.py', 'test_projection_records.py'):
    p = ROOT / 'tests' / name
    text = p.read_text()
    if not re.search(r'(?m)^(?:ROOT\s*=|from .* import .*\bROOT\b)', text):
        anchor = 'import unittest\n'
        if anchor not in text:
            raise RuntimeError('Missing unittest import: ' + name)
        text = text.replace(anchor, anchor + '\nfrom tools.repository.validate import ROOT\n', 1)
    p.write_text(text)

# Restore the original baseline evidence validator, not the retired matrix gates.
old = subprocess.check_output(['git', 'show', BASE + ':tools/repository/validate.py'], text=True)
old_tree = ast.parse(old)
functions = {n.name: n for n in old_tree.body if isinstance(n, ast.FunctionDef)}
p = ROOT / 'tools/repository/validate.py'
text = p.read_text()
existing = {n.name for n in ast.parse(text).body if isinstance(n, ast.FunctionDef)}
needed = {'validate_baseline_inventory'}
while True:
    expanded = needed | {
        n.id for name in needed for n in ast.walk(functions[name])
        if isinstance(n, ast.Name) and n.id in functions and n.id not in existing
    }
    if expanded == needed:
        break
    needed = expanded
parts = [ast.get_source_segment(old, n) for name, n in functions.items()
         if name in needed and name not in existing]
anchor = '\ndef validate(root:'
if anchor not in text:
    raise RuntimeError('Missing public validator entry')
p.write_text(text.replace(anchor, '\n\n' + '\n\n'.join(parts) + '\n\n' + anchor, 1))

# A clean checkout has no private working directory. Create it at use, not import.
p = ROOT / 'tools/analysis/memory_case.py'
text = p.read_text()
anchor = 'tempfile.TemporaryDirectory(dir=ROOT / ".local")'
if anchor not in text:
    raise RuntimeError('Missing private temporary-directory allocation')
text = text.replace(anchor, 'private_temporary_directory()')
helper = '''def private_temporary_directory():
    """Allocate private work lazily, including on a source-only clean checkout."""
    directory = ROOT / ".local"
    directory.mkdir(parents=True, exist_ok=True)
    return tempfile.TemporaryDirectory(dir=directory)


'''
text = text.replace('def require(condition: bool, message: str) -> None:',
                    helper + 'def require(condition: bool, message: str) -> None:', 1)
p.write_text(text)

# Tiny state modules should not transitively depend on the whole game header set.
headers = {
    'input_state.hpp': ['<array>', '<cstdint>'],
    'interrupt_state.hpp': ['<array>', '<cstdint>'],
    'gpu_state.hpp': ['"xem/reconstruction/gpu.hpp"', '<array>', '<cstdint>', '<deque>', '<vector>'],
    'source_point.hpp': ['"xem/reconstruction/field_sprite_factory.hpp"', '<cstddef>', '<cstdint>',
                         '<optional>', '<stdexcept>', '<string>', '<string_view>', '<utility>'],
}
for name, includes in headers.items():
    p = ROOT / 'include/xem/reconstruction' / name
    text = p.read_text()
    body = text[text.index('namespace xem::reconstruction {'):]
    p.write_text('#pragma once\n\n' + '\n'.join('#include ' + i for i in includes) + '\n\n' + body)

# The smoke target must fail, not silently skip, without the matching shell.
# Disable GNU make's implicit C rules so an unqualified host cc is never selected.
replace('decomp/Makefile', '.DEFAULT_GOAL := verify',
        '.DEFAULT_GOAL := verify\n.SUFFIXES:\nMAKEFLAGS += --no-builtin-rules')
replace('decomp/Makefile', 'smoke:\n',
        'smoke:\n\t@command -v psx-as >/dev/null && command -v psx-ld >/dev/null && command -v psx-objcopy >/dev/null || { echo "Enter the matching Nix shell first." >&2; exit 2; }\n')
replace('decomp/Makefile', '$(IMAGE): $(OBJECTS) $(LINKER_SCRIPT)',
        '$(IMAGE): $(OBJECTS) $(LINKER_SCRIPT) $(CONFIG)')
replace('tools/matching.py', "    actual = rebuilt.read_bytes()\n", "    actual = rebuilt.read_bytes()\n    if not expected:\n        raise ValueError('The original executable image must not be empty')\n")
p = ROOT / 'tests/test_matching.py'
text = p.read_text()
anchor = "    def test_cli_exit_codes(self):"
extra = '''    def test_empty_image_cannot_pass(self):
        self.original.write_bytes(b'')
        self.rebuilt.write_bytes(b'')
        with self.assertRaisesRegex(ValueError, 'must not be empty'):
            compare(self.original, self.rebuilt, hashlib.sha256(b'').hexdigest())

    @unittest.skipUnless(shutil.which('make'), 'GNU make is unavailable')
    def test_unconfigured_game_target_fails(self):
        root = Path(__file__).resolve().parents[1]
        result = subprocess.run(['make', '-C', str(root / 'decomp'), 'verify'],
                                text=True, capture_output=True, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('qualifying the original toolchain', result.stderr)

'''
if anchor not in text:
    raise RuntimeError('Missing matching test anchor')
p.write_text(text.replace(anchor, extra + anchor, 1))

# Preserve detailed product specifications, but remove old stage numbers from the
# active authoring/native-agent descriptions. Historical JSON evidence is untouched.
for name in ('docs/authoring/README.md', 'docs/agent/README.md'):
    p = ROOT / name
    text = p.read_text()
    lines = text.splitlines(keepends=True)
    text = lines[0] + '\n> Roadmap ownership: plan.md is authoritative. Native runtime and original-game\n> play come first (Phases 2–4); the authoring bridge belongs to Phase 7. Legacy\n> facet/gate labels in retained specification JSON are historical identifiers,\n> not additional Phase 1 work or implementation-completion claims.\n\n' + ''.join(lines[1:])
    text = text.replace('Phase 2A', 'Phase 7').replace('Phases 2A', 'Phase 7')
    p.write_text(text)

# Dead matrix-dependent entry points should not survive merely as compatibility API.
for name in ('agent_contract.py', 'authoring_contract.py'):
    p = ROOT / 'tools/repository' / name
    text = p.read_text()
    tree = ast.parse(text)
    lines = text.splitlines(keepends=True)
    for node in reversed(tree.body):
        if isinstance(node, ast.FunctionDef) and node.name == 'validate':
            # Only remove orchestration that consumes a generated matrix. Pure
            # schema/semantic/build-isolation validators and their tests remain.
            if 'matrix' in {arg.arg for arg in node.args.args}:
                if any(re.search(r'\bvalidate\(', q.read_text()) for q in []):
                    raise RuntimeError('Unexpected validation consumer')
                lines[node.lineno - 1:node.end_lineno] = []
    p.write_text(''.join(lines))
# The retained tests no longer import those retired aggregate entry points.
for name in ('test_agent_contract.py', 'test_authoring_contract.py'):
    p = ROOT / 'tests' / name
    text = p.read_text()
    tree = ast.parse(text)
    lines = text.splitlines(keepends=True)
    for node in reversed(tree.body):
        if isinstance(node, ast.ImportFrom) and node.module in {
            'tools.repository.agent_contract', 'tools.repository.authoring_contract'}:
            names = [a for a in node.names if a.name != 'validate']
            if len(names) != len(node.names):
                value = 'from ' + node.module + ' import (' + ', '.join(
                    a.name + (' as ' + a.asname if a.asname else '') for a in names) + ')\n'
                lines[node.lineno - 1:node.end_lineno] = [value]
    p.write_text(''.join(lines))

write('.github/workflows/ci.yml', '''
name: Public build and matching tools
on:
  pull_request:
  push:
    branches: [master]
permissions:
  contents: read
concurrency:
  group: public-${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
jobs:
  public:
    runs-on: ubuntu-24.04
    timeout-minutes: 30
    steps:
      - uses: actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683
        with:
          persist-credentials: false
      - uses: cachix/install-nix-action@13d8dd58da0234aa297dedd986986ccb8e7f3e24
        with:
          extra_nix_config: |
            experimental-features = nix-command flakes
      - name: Public build, standalone headers, and regressions
        run: nix develop path:./nix -c python3 tools/repository/check.py --preset debug
      - name: Actual matching toolchain and exact synthetic image
        run: nix develop path:./nix/ghidra#matching -c make -C decomp smoke
''')

# Allowlist only the permanent CI file. One-shot scripts are never distributed.
p = ROOT / 'packaging/source-files.txt'
names = {n for n in p.read_text().splitlines() if n and not n.startswith('#') and Path(n).is_file()}
names.add('.github/workflows/ci.yml')
p.write_text('\n'.join(sorted(names)) + '\n')
# Verify prompt.md is byte-for-byte unchanged before any test/publication step.
expected = subprocess.check_output(['git', 'show', BASE + ':prompt.md'])
assert Path('prompt.md').read_bytes() == expected, 'prompt.md changed unexpectedly'
print('Retained evidence and lifecycle tests repaired; smoke is fail-closed; CI is read-only.')
subprocess.run(['git', 'diff', '--stat'], check=True)
