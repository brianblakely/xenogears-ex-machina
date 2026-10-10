# Independent analysis

New Phase 1 work follows the [matching-first workflow](../docs/matching.md):
recover original-target source, compile, compare, and fix the first difference.
Do not read this entire directory before implementing a change.

Consult these resources only for the source or subsystem being changed:

- `reference-profiles.json` binds exact original images and supported revisions.
- `findings/` and `formats/` preserve original-source knowledge, recovered types,
  algorithms, formats, uncertainty and the scope of measured comparisons.
- `coverage/` retains source/content observations and unresolved coverage. A
  baseline inventory or a passing route is not a complete both-disc decomp.
- `scenarios/` supports targeted original execution when it resolves a question.
- `decisions/` distinguishes intentional changes from unknown original behavior.

The host C++ reconstruction and its memory-comparison workflow were retired after
commit b969b11: the paths that findings, formats, coverage records and
`recovery.json` cite under `src/reconstruction`, `src/analysis`, `include/xem`,
`tests/*.cpp` and the removed runner tools (`tools/analysis/execution.py`,
`memory_case.py`, `movie_chain.py`, `menu_save_file.py`) name files at that commit
(`git show b969b11:src/reconstruction/program.cpp`). Historical
phase/facet labels in findings describe their original scope; `plan.md` owns the
current roadmap. Source coverage and byte matching must be reported separately.

Keep original bytes, generated disassembly, Ghidra projects, captures and personal
saves under ignored local paths. Publish reviewed authored source and concise
factual findings, not original payloads. See [evidence boundaries](../docs/evidence-workflow.md)
and [contributing](../CONTRIBUTING.md).
