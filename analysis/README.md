# Independent analysis

- `reference-profiles.json`: exact measured inputs, region/identity evidence,
  explicitly scoped revision support, and unresolved identity questions.
- `findings/`: original factual research records with source coordinates and
  reproducible validation. Templates define required fields.
- `coverage/`: observed source inventory, game-content inventory gaps, and
  representative progression/checkpoint capture definitions.
- `subsystems.json`: evidence-gated maturity and explicit unknown/change status.
- `decisions/`: intentional differences from original behavior, each kept separate
  from unknown behavior and correctness findings.
- `formats/`: bounded, source-qualified reconstructions and local original-data
  comparisons, including the packed decoder's actual source-memory boundary.
  [Field control](formats/field-control.md) documents the recovered direction and
  jump-request subset and its remaining physics/eligibility work.
  [Jump physics](formats/jump-physics.md) extends it with original party-resource
  loading, gravity/impulse arithmetic and a bounded vertical stage.
  [Planar motion](formats/planar-motion.md) adds mode/speed/vector reconstruction,
  field quantization and observed sprite instruction-pointer stores.
  [Movement sweeps](formats/movement-sweep.md) adds original adjacency/terrain
  queries and complete nested slope and edge calculations on two bounded routes.
  [Sprite animation](formats/sprite-animation.md) adds observed header selection,
  facing replay, frame scheduling/metadata and matrix updates, with reviewed
  Ghidra types and exact original-call comparisons.
  [Party motion](formats/active-motion.md) composes those models into complete
  observed party updates, with exact function-entry, intermediate and return
  comparisons and a selective compiled m2c check of the idle predicate.
  [Field lifecycle](formats/field-lifecycle.md) connects loader ownership, event
  initialization, reusable C++ execution, position/history models and field return
  to the provisional battle/menu surveys and remaining Phase 2 dependencies.

Ghidra with lab313ru/ghidra_psx_ldr is the primary reverse-engineering environment.
The pinned environment is `nix/ghidra`; m2c is an optional aid for functions that
benefit from matching-oriented reconstruction. Keep projects and generated
decompiler output private, and review every inferred type and control-flow path
against the qualified original source and execution.

All original images/executables/assets, emulator saves, private memory/trace dumps,
decoded media, and screenshots remain under ignored `discs/` or `.local/`.
Only independently authored explanations, non-content metadata, hashes and source
coordinates are publishable here. See `docs/evidence-workflow.md` and
`CONTRIBUTING.md`. Nothing in Phase 0 is decompiled gameplay source.
