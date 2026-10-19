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
- Use the pinned Nix shells. While developing, test only the platform whose host you
  are changing: `src/runtime/scripts/suite.sh desktop` (native and shared code, about
  fifteen minutes), `web`, `android` or `xr`, or narrower focused tests. Run
  `suite.sh full` (every platform and what CI runs, about an hour) once at the
  end before review or a merge only when runtime inputs changed, never after each
  change. Documentation, release copy and standalone tooling changes need their
  focused checks, not the runtime suite.
  The suite skips before starting Nix if runtime sources, tests and build inputs
  have no changes against the merge base with `origin/master`; staged, unstaged,
  committed branch changes and non-ignored untracked files count. It does not
  fetch. Use `--base REF` for another comparison (e.g. `--base HEAD^` to check a
  committed update on master), or deliberately use `--force` to run regardless,
  including after changing private/ignored inputs or the test environment. Do not
  use `--force` solely because a review or merge is coming.
  A skip is not a passing test run; it leaves previous logs intact.
  Record exact commands/results and missing private inputs honestly.
- Keep original assets and generated disassembly local. Do not modify prompt.md.
