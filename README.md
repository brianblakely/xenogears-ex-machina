# Xenogears: Ex Machina

An independent Xenogears decompilation, native PC runtime and graphical modding
toolkit, with Arch Linux leading development. **Phase 1 is in progress.** The
repository has a Phase 0 build baseline and a connected reconstruction analysis
driver; neither runs the complete game yet.

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix
python3 tools/repository/check.py
```

The pinned Nix environment supplies C++20/Clang, CMake, Ninja, Python and formatting
tools. Debug, Release and ASan/UBSan configurations and public tests need no game
data. See [development setup](docs/development.md).

## Phase 1: run, inspect the blocker, extend the recovered program

The [connected decompilation workflow](docs/connected-decomp.md) is the active
Phase 1 execution strategy. `xem-reconstruct` links the same recovered C++ library
used by its tests and intended for the later native runtime. Its first connected
boundary composes event parsing, owned actor/variable/control state, scheduling,
core event handlers and battle request/continuation. It preserves state between
calls and stops explicitly at missing inputs, unconnected instructions or the
remaining field-update tail. This is a prepared analysis boundary, not a cold boot,
a complete game tick or a playable native slice.

```sh
cmake --preset debug
cmake --build --preset debug --target check-connected
```

The Python front end runs qualified private cases and compares the resulting state;
it does not implement gameplay or feed expected results into C++. Existing Python
models and evidence remain independent references. The first original connected
case still needs qualification; the new public composition fixtures are synthetic.
See [the case contract and next integration work](docs/connected-decomp.md).

The [plan](plan.md) defines the full project. The
[requirement-to-test matrix](docs/requirements.md) covers every requested feature,
default, platform and editor capability. A defined test is not an executed pass.

The [native agent contract](docs/agent/README.md) specifies direct engine input,
full state access, debugging, typed scenario setup, exact/unlocked time and optional
spectator images. Its schemas and acceptance gates are established before native
gameplay; `xem-baseline` and `xem-reconstruct` do not implement this interface.
The versioned [emulator parity inventory](docs/agent/emulator-parity.json) records
verified reference paths, unsupported adapters and required native extensions separately.

The [agent authoring contract](docs/authoring/README.md) specifies source-oriented
TypeScript geometry, isolated build workers, native content loading and protected
build/play/repair gates. Its [separate dependency review](docs/authoring/dependencies.json)
pins the build-only toolchain. The dependency qualification fixture works; the
authoring SDK, untrusted-build isolation and playable native bridge remain unimplemented.

[Reference profiles](analysis/reference-profiles.json) identify the exact measured
original inputs. [Coverage](analysis/coverage/README.md) grounds all ten content
categories on both discs in original evidence and preserves unresolved content,
formats and behavior. The baseline inventory is ready; the exhaustive game catalog
remains open. [Checkpoint definitions](analysis/coverage/checkpoints.json) prepare
both discs for original/native validation before gameplay implementation.

The [Phase 1 handoff](docs/phase1-handoff.md) records exact starting evidence and
the remaining analysis work. [Current Phase 1 progress](docs/phase1-progress.md)
records bounded original format, event, movement and animation comparisons,
including complete observed party motion updates composed from recovered source,
together with their unresolved scope. Connected-driver tests do not enlarge those
historical original-execution claims. The native runtime is still a build baseline.

The [original-game scenario system](analysis/scenarios/README.md) cold-boots
selected fields through the original loaders, records guarded setup and ordered
inputs, and captures reference output locally. Use it for emulator-based research;
its independently recovered adapters and current limits are documented alongside
the presets.

Read the [evidence workflow](docs/evidence-workflow.md) and
[contribution rules](CONTRIBUTING.md) before analysis or implementation. We do not
use another Xenogears project's source or reverse-engineering results as a
foundation. General-purpose tools are reviewed in
[the dependency record](docs/dependencies.json).

User-supplied images stay in ignored `discs/`; extracted assets, execution captures
and private saves stay under ignored `.local/`. Nothing copyrighted is needed by
public CI or included in the explicit [source allowlist](packaging/source-files.txt).
The Nix path input is only `nix/`, so private data is not copied into its store.
