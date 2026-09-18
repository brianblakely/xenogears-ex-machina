# Battle request continuation source

The existing `field_battle` C++ module now connects primary `71` to extended
`7F` and primary `86` through the same `EventContext`, variable bank and batch
scheduler. These are recovered field event operations, not combat result or
field-return producers. The original field overlay identity is
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`.

Extended `7F` at `8008a244` runs after primary `FE` has incremented the working
halfword PC. A zero request word (`800adb88`, shared `BattleRequestState.pending`)
advances past `7F`; every nonzero value backs up to `FE`. Both paths set the
break request to one and preserve budget mode. This differs from the music
wait's exact `ffffffff` sentinel. The wait does not clear the pending word.

Primary `86` at `80096724` reads its first operand with
`800acdec`: a tagged unsigned 15-bit immediate or a typed variable reference.
It compares this against typed variable reference zero. Each variable retains
its own signedness. Equality advances the PC by five without reading the
destination bytes; inequality reads the raw halfword destination at PC plus
three. Operand addresses do not wrap with the final PC store. The handler
does not write the variable bank or request a break. Variable zero's broader
gameplay meaning remains unresolved.

The preserved qualified Ghidra group includes both handlers, primary `FE`,
the halfword/tagged operand readers and the variable reader. A new bounded
capture extends the earlier entry-only discovery with complete original exits.
At frontend run 10304 actor 14 enters `7F` at PC `08af`, observes pending zero
and exits at `08b0`; the enclosing `FE` starts at `08ae`. At run 10306 `86`
compares the captured variable value 27 with immediate 32 and branches to
`08bb`. These are frontend observations, not recovered simulation tick units.

The C++ replay compares complete 6,496-byte actor, field-control, event-control,
request, variable, type-map, budget and bytecode payloads at the three paired
boundaries. All 124 images, audio, RAM and external-state artifacts match a
separately built uninstrumented control. The capture and replay are private at
`.local/analysis/phase1-battle-continuation-20260918`; no original bytes enter
the source archive.

Public `test-field-battle` exercises request, pending retry, ready wait,
equal/unequal branch and real assignment/end continuation. Its caller-supplied
return gates and result values are synthetic inputs, not evidence for their
producers. Pending original waits, equal branches, alternate variable operands,
malformed storage and wrap boundaries have source/synthetic coverage only.
Battle setup, attacks, rewards, failure handling and complete return ownership
remain required. Independent review and promotion are recorded with the
integration finding; earlier Python references remain unchanged.
