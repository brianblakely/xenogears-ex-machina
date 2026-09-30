# Contributing

Work from plan.md and the relevant implementation. Prefer a small source change
that compiles and reduces a real diff over a new framework, report or checklist.

For Phase 1 use docs/matching.md. Keep game-specific recovery independent of other
Xenogears projects. Reuse qualified source identities, types, algorithms and
existing observations. General-purpose tools and infrastructure libraries are
permitted; record their actual versions/licenses in the owning Nix recipe or a
short dependency note, not a new approval registry for every change.

Preserve original arithmetic, layout, call/overlay context and hardware intent.
Binary matching and readable source coverage are distinct. Keep unknowns visible;
never patch expected original bytes, hide mismatches or count retained assembly
as recovered compiled source. Do not demand a separate capture pipeline for an
already qualified exact match. Behavioral tests remain essential when adapting
source to a new platform or resolving uncertainty.

Use focused builds/tests during iteration; run the public build and relevant
matching tests before review. Report what ran and what was blocked by missing
private inputs. Source comments should explain actual semantics and source
coordinates. Findings are for new knowledge, not ceremonial per-function forms.

Original binaries, extracted assets, generated disassembly and private captures
remain local. Add only reviewed authored files to packaging/source-files.txt;
run tools/repository/source_archive.py --check. Preserve historical evidence at
its original scope. Do not change prompt.md as part of repository maintenance.
