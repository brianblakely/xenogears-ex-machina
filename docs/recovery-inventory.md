# Recovery inventory diagnostics

[The recovery inventory](../analysis/recovery.json) records incomplete symbols,
formats, behavior and original dispatch windows. Repository validation now checks
its IDs, next experiments, status labels, evidence links, authored source paths,
opcode order and table fingerprints. A missing row, changed original table value,
unsupported status or unlisted source fails the public check with its location.

Run these commands inside the [pinned Nix environment](development.md):

```sh
python3 tools/repository/recovery.py
python3 tools/repository/recovery.py \
  --coverage P01-SLICE-ENCOUNTER-RETURN \
  --dependency symbol:field-return-sprite-ownership
```

The first command checks inventory integrity and reports incomplete-entry counts.
The second returns exit code 1 and JSON identifying the dependency, its next
experiment, available evidence and the requested coverage that it blocks.
Malformed inventory or command input returns exit code 2.

Dependency names use `symbol:<id>`, `format:<id>`, `behavior:<id>` or
`instruction:<namespace>:0xNN`. Instruction namespaces are `primary`, `extended`
and `sprite`. A syntactically valid but unlisted dependency remains blocked.
Observed instruction subsets and authored C++ libraries retain those statuses;
neither establishes full instruction behavior. Equal dispatch values retain
separate opcode entries, including extended-table values not established as code.

The coverage name is an explicit caller declaration. This diagnostic does not
infer that an unused instruction blocks the selected route, update validation
results, or grant a coverage pass. The [slice gate](phase1-slice-contract.md)
continues to require independent original evidence for each scoped proof.
Complete symbol/format discovery and P01-T15 remain open.

[The synthetic tests](../tests/test_recovery.py) inject unresolved symbols,
formats and instructions, then verify precise blockers and a failing CLI result.
They also reject lost entries, aliased status promotion, unsupported completion,
changed table values and broken evidence/source links. Their invented dispatch
values describe no original game behavior.
