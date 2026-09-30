- Implement first. Read plan.md, docs/matching.md and only the relevant source/build
  files. Start from a real target/function or failing test; compile and inspect the
  first difference. Do not read all findings or regenerate status paperwork.
- Phase 1 is the complete both-disc PS1 matching decomp. New source belongs under
  decomp/, not in an expanded host Program model. Preserve original semantics and
  small subsystem headers; use original assembly privately as temporary scaffolding.
- A byte match is not source coverage. Never count placeholders, guess compiler
  identity, normalize away mismatches, replace expected bytes or invent passes.
- Keep the existing host reconstruction, captures and useful tests as reference/
  portability assets. Add original captures only for a concrete unresolved question.
- Do not preserve obsolete compatibility paths, fallbacks, migrations or duplicated
  requirement registries. Prefer the simplest implementation and existing tools.
- Use the pinned Nix shells. Run focused tests while editing and the public build
  before review. Record exact commands/results and missing private inputs honestly.
- Keep original assets and generated disassembly local. Do not modify prompt.md.
