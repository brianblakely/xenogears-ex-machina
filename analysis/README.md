# Independent analysis

New Phase 1 work follows the [matching-first workflow](../docs/matching.md):
recover original-target source, compile, compare, and fix the first difference.
Do not first translate every function into the host reconstruction or read this
entire directory before implementing a change.

Consult these resources only for the source or subsystem being changed:

- `reference-profiles.json` binds exact original images and supported revisions.
- `findings/` and `formats/` preserve original-source knowledge, recovered types,
  algorithms, formats, uncertainty and the scope of measured comparisons.
- `coverage/` retains source/content observations and unresolved coverage. A
  baseline inventory or a passing route is not a complete both-disc decomp.
- `scenarios/` supports targeted original execution when it resolves a question.
- `decisions/` distinguishes intentional changes from unknown original behavior.

The existing host C++ and its memory-comparison workflow remain reference and
portability assets, not the mandatory path for new PS1-target source. Historical
phase/facet labels in findings describe their original scope; `plan.md` owns the
current roadmap. Source coverage and byte matching must be reported separately.

Keep original bytes, generated disassembly, Ghidra projects, captures and personal
saves under ignored local paths. Publish reviewed authored source and concise
factual findings, not original payloads. See [evidence boundaries](../docs/evidence-workflow.md)
and [contributing](../CONTRIBUTING.md).
