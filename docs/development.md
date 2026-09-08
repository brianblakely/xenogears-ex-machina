# Development on Arch Linux

The leading development host is Arch Linux. Phase 0 was exercised on x86_64
Omarchy (Arch-derived); the verification record identifies the actual host. The
project uses C++20, CMake and Ninja. No game-specific dependency or disc data is
needed for the empty build or public tests. Nix supplies all development tooling.

From the repository root:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix
python3 tools/repository/check.py
```

The tracked `nix/flake.lock` pins every tool through a specific nixpkgs revision
and content hash. Network/store access is needed to realize that lock initially.
No user Nix configuration, `HOME`, `CODEX_HOME`, or system package set is modified.
The flake is deliberately inside `nix/`: `path:.` would copy ignored private disc
data into the Nix store. Keep that directory free of project/data symlinks or
imports outside its tooling boundary. Review all lock-file updates.

Individual workflows inside the shell:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize
cmake --preset release
cmake --build --preset release
ctest --preset release
python3 tools/repository/format.py
python3 tools/repository/matrix.py
python3 tools/repository/validate.py
python3 tools/repository/source_archive.py
```

`debug` retains debug information. `sanitize` enables AddressSanitizer and
UndefinedBehaviorSanitizer, frame pointers and fatal diagnostics; its CTest preset
enables leak checks. These presets use strict warnings as errors. Sanitizers fail
configuration on unsupported platforms rather than silently disabling themselves.
`release` is optimized and is separately exercised for reproducibility. Builds,
generated headers, compile databases and install experiments belong under ignored
`build/` or `.local/`. Optional local presets belong in ignored CMakeUserPresets.json.

The public repository gate also checks the [native agent specification](agent/README.md):
wire/query/scenario schemas, examples, architecture acceptance definitions, full
plan traceability and emulator parity source hashes. These are specification and
tooling checks; native gameplay/agent acceptance remains unexecuted. After a plan
edit, review task-to-facet coverage and update `docs/requirement-review.json` before
regenerating the matrix. After changing a reference runner dependency, inspect the
parity rows and bump their version and reviewed hashes. Do not refresh these
records merely to silence drift errors.

For private original-source measurement, enter the separate analysis shell:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix#analysis
python3 tools/reference/capture.py 'discs/Xenogears disc 1.chd' .local/references/new-source-1
python3 tools/reference/capture.py 'discs/Xenogears disc 2.chd' .local/references/new-source-2
```

The output directory must be new; the tool never overwrites previous evidence.
It runs pinned chdman integrity verification and extraction, then bounded standard
metadata inspection. It makes no guesses about game archives, disc sequence or
retail revision names. Consult the reference profile and evidence workflow before
promoting a measurement. Source CUE paths and raw bytes remain private.

Ghidra with the pinned PlayStation loader is the primary reverse-engineering
environment. Follow [the qualified import and review workflow](reverse-engineering.md)
for both original discs and optional m2c reconstruction:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra
```

The `disassembly` shell supplies secondary Capstone Python bindings for
original-MIPS byte inspection. It replaces the plain Python interpreter with the
Nix Python environment containing those bindings:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix#disassembly
```

The `observation` shell supplies the pinned external libretro core through
`XEM_REFERENCE_CORE`. Follow `tools/reference/README.md` for its evidence limits
and the required go-anywhere testing workflow. These optional tools are excluded
from the default build and installed baseline.

Windows/macOS source portability is designed into CMake, but native compile checks
begin in Phase 2 and runtime/backend qualification occurs later. The Nix shell's
Darwin entries are prospective tooling configurations, not tested platform support.
No SDL, renderer, emulator or event runtime is linked into `xem-baseline`.
