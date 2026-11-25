- Implement first. Read plan.md, docs/matching.md and only the relevant source/build
  files. Start from a real target/function or failing test; compile and inspect the
  first difference. Do not read all findings or regenerate status paperwork.
- Phase 1 is the complete both-disc PS1 matching decomp. New source belongs under
  src/decomp/. Preserve original semantics and
  small subsystem headers; use original assembly privately as temporary scaffolding.
- A byte match is not source coverage. Never count placeholders, guess compiler
  identity, normalize away mismatches, replace expected bytes or invent passes.
- The decomp stays a pure, byte-identical implementation of the original game and
  remains the runtime's game-logic provider. Never edit src/decomp/ for native/browser
  integration (not even behind #ifdef); put integration in src/port/ and the build
  tooling. Run `make -C src/decomp all-verify` after anything touches src/decomp/.
- Keep the existing captures and useful tests as reference/portability assets (the
  host C++ reconstruction was retired; read it at commit b969b11). Add original
  captures only for a concrete unresolved question.
- Do not preserve obsolete compatibility paths, fallbacks, migrations or duplicated
  requirement registries. Prefer the simplest implementation and existing tools.
- Work must not create runtime performance regressions. When a change touches
  per-frame or load paths (the game module, src/port/, rendering, audio, input, the
  hosts' loops), measure it against master on the same scenes and machine, and fix
  any slower frame, presentation, load or larger memory use before review.
- Use the pinned Nix shells. While developing, run focused tests: the changed
  crate's or file's tests and a repro on the affected platform. Never run
  `src/runtime/scripts/suite.sh` locally unless the user explicitly asks for it.
  GitHub CI (`.github/workflows/ci.yml`) runs the full suite for every pull request
  from this repository and every push to master whose runtime inputs changed: each
  platform tier on its own runner, with the discs from the private fixtures release
  (the `tests` environment's secrets). Read the suite's results there; a skipped
  suite (no runtime inputs changed, or a fork's pull request, which gets no
  secrets) is not a pass. Documentation, release copy and standalone tooling
  changes need their focused checks, not the runtime suite.
  When the user asks for a local run, `suite.sh` skips before starting Nix if no
  runtime inputs changed against the merge base with `origin/master` (`--base REF`
  for another comparison, `--force` to run regardless, e.g. after changing
  private/ignored inputs); a skip is not a passing run.
  Record exact commands/results and missing private inputs honestly.
- Keep original assets and generated disassembly local. Do not modify prompt.md.
