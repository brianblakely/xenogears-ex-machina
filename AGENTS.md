- Do not preserve backward compatibility. Remove obsolete paths instead of adding compatibility layers, fallbacks, or migrations.

- Choose the simplest implementation that fully meets the current requirements. Avoid speculative abstractions, configuration, and indirection.

- Prefer established, well-maintained libraries when they reduce overall complexity or improve reliability. Do not reimplement common functionality without a clear reason.

- Lean on the dependencies already in the project before writing your own implementation or adding packages. Do not assume a library lacks a capability without checking its documentation and types.

## Phase 1 execution

Read `docs/connected-decomp.md` before choosing work. The primary deliverable is
connected, reusable recovered C++, not an expanding collection of isolated models
or proof frameworks. Use `xem-reconstruct` and its Python comparison front end to
run a qualified prepared boundary, inspect the first actual stop or divergence,
and integrate or recover the connected dependency that prevents further execution.
The current prepared event pass is not fresh initialization, a game tick or a
complete field runtime. Do not run it repeatedly to bypass the unconnected tail.

Inspect `analysis/recovery.json` and existing C++ before fresh recovery. An
unconnected handler may already exist. Prefer direct C++ for new stable game
behavior; Python is for extraction, experimentation and comparison, not a mandatory
second implementation. Use Ghidra and selective assembly/m2c review to resolve
concrete uncertainty. Correct shared types and re-decompile related functions.

Never feed expected outputs back into execution, skip unknown code, reset state
between internal calls, invent readiness, or replace a missing game service with
a successful stub. Prepared input, missing integration, unrecovered behavior,
malformed data, a divergence and a host budget are different problems. Preserve
partial writes for diagnosis and restart from the immutable prepared boundary.

Run focused connected and affected subsystem tests during development. Batch
repository/provenance/distribution audits at integration milestones. Preserve
Phase 0 artifacts, existing models useful as independent references, historical
findings, source identities and the versionless trace interface. Do not add new
status schemas, migrations, evidence ledgers or compatibility paths for this loop.
`plan.md` and the slice contract retain their scope and completion gates; a longer
execution prefix or passing synthetic test does not complete Phase 1.
