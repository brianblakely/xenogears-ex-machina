# Xenogears: Ex Machina

An independent Xenogears decompilation, native PC runtime and graphical modding
toolkit, with Arch Linux leading development. This repository is currently a
**Phase 0 research and build baseline**. It does not run the game yet.

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix
python3 tools/repository/check.py
```

The pinned Nix environment supplies C++20/Clang, CMake, Ninja, Python and formatting
tools. Debug, Release and ASan/UBSan configurations and public tests need no game
data. See [development setup](docs/development.md).

The [plan](plan.md) defines the full project. The
[requirement-to-test matrix](docs/requirements.md) covers every requested feature,
default, platform and editor capability. A defined test is not an executed pass.

[Reference profiles](analysis/reference-profiles.json) identify the exact measured
original inputs. [Coverage](analysis/coverage/README.md) grounds all ten content
categories on both discs in original evidence and preserves unresolved content,
formats and behavior. The baseline inventory is ready; the exhaustive game catalog
remains open. [Checkpoint definitions](analysis/coverage/checkpoints.json) prepare
both discs for original/native validation before gameplay implementation.

The [Phase 1 handoff](docs/phase1-handoff.md) records exact starting evidence and
the remaining analysis work. No later-phase implementation is claimed by this
research baseline.

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
