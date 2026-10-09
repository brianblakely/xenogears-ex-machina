# Host reconstruction inventory

[The inventory](../analysis/recovery.json) records the status of the host
reference reconstruction ([executable-reconstruction.md](executable-reconstruction.md)):
which original dispatch entries its C++ library implements, the bounded original
comparisons behind them and the connections it still lacks. It does not record the
recovery of the original program. Every function of both discs is matching C under
decomp/, and the formats, dispatch tables and script instructions are documented in
[docs/scripts](scripts/interpreters.md) with their decoders under tools/analysis.
An `unresolved` or `unimplemented` entry means only that the host library lacks it.

- Dispatch rows: the field event primary and extended tables (`D_800AE2A0`,
  `D_800AE6A0`) and the resident sprite command table (`0x800183d8`, opcodes
  8a-fc), with every original table value, also the extended values that are not
  code; the table fingerprints keep them unchanged. `native_status` says whether the
  C++ library implements an entry (`cpp_reconstruction` names its source).
  `analysis_status` says whether that implementation was compared with original
  execution (`observed_subset`, with its evidence) or only written from the source
  (`source_reconstructed_unobserved`).
- Symbols and behaviour: connections the host runner stops at (it names them, such
  as `symbol:field-return-sprite-ownership`) and original branches its comparisons
  have not exercised. The format list is empty: the original formats are documented
  with the decomp.

Run these commands inside the [pinned Nix environment](development.md):

```sh
python3 tools/repository/recovery.py
python3 tools/repository/recovery.py \
  --coverage P01-SLICE-ENCOUNTER-RETURN \
  --dependency symbol:field-return-sprite-ownership
```

The first command checks the inventory and counts the entries the host library has
not completed (649: 627 dispatch rows, 6 symbols, 16 behaviours). The second returns
exit code 1 and JSON identifying the dependency, its next step, its evidence and the
requested coverage that it blocks. Malformed inventory or command input returns
exit code 2. `tools/analysis/execution.py` adds the matching row to a runner report
that stops at a dependency.

Dependency names use `symbol:<id>`, `format:<id>`, `behavior:<id>` or
`instruction:<namespace>:0xNN`. Instruction namespaces are `primary`, `extended`
and `sprite`. A syntactically valid but unlisted dependency remains blocked. Equal
dispatch values keep separate entries. The coverage name is an explicit caller
declaration: the diagnostic does not infer that an instruction blocks a route or
grant a pass.

[The synthetic tests](../tests/test_recovery.py) inject unresolved symbols,
formats and instructions, then verify precise blockers and a failing CLI result.
They also reject lost entries, aliased status promotion, unsupported completion,
changed table values and broken evidence/source links. Their invented dispatch
values describe no original game behavior.
