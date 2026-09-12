# Field event control flow — recovered subset

This is a partial instruction reference derived from the original field overlay
whose decoded SHA256 is
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`.
Addresses below always mean that overlay, loaded at `0x8006faf0`.
[EVID-REF-015](../findings/EVID-REF-015.json) records conditional validation;
[EVID-REF-016](../findings/EVID-REF-016.json) adds slot selection and state effects.

The primary dispatch table begins at `0x800ae2a0`. Python analysis source in
`tools/analysis/events.py` and `tools/analysis/event_state.py` expresses this
subset and rejects instructions outside it. Integer operands are little-endian.

| Opcode | Handler | Encoding | Recovered effect | Original validation |
| --- | --- | --- | --- | --- |
| `00` | `0x800a1b70` | One byte | End selected slot; set budget mode and break request to one; PC stays unchanged | 27,620 original calls |
| `01` | `0x800a1e74` | Opcode, absolute u16 target | Assign the actor's working u16 PC; preserve slot resume PC | 1,833 original calls |
| `02` | `0x800a1bd0` | Opcode, u16 left, u16 right, u8 mode/comparison, u16 false target | True advances PC by eight; false assigns the target | 16,509 original calls in the field 23 route; scoped below |
| `04` | `0x800a1a8c` | One byte | Reset priority-seven slots to event one, end selected slot, request break; preserve budget mode and working PC | 713 original calls, all in budget mode one |
| `26` | `0x8009dd34` | Opcode, u16 tagged value/reference | Load/decrement selected slot's byte countdown; advance three bytes only at zero; request break | 84 original calls, immediate waits of 1, 20 and 60 |
| `35` | `0x8009d9a4` | Opcode, u16 destination, u16 source, u8 selector | Store source halfword; advance six bytes | 1,454 original calls, selector `00`/`40` |
| `36` | `0x8009d960` | Opcode, u16 destination | Store one; advance three bytes | 23 original calls |
| `37` | `0x8009d91c` | Opcode, u16 destination | Store zero; advance three bytes | 1,875 original calls |
| `38` | `0x8009d890` | Same six-byte encoding as `35` | Add source to destination modulo 65,536 | 21 original calls, selector `00` |
| `39` | `0x8009d804` | Same six-byte encoding as `35` | Subtract source from destination modulo 65,536 | 1,427 original calls, selector `40` |
| `71` | `0x80093568` | Opcode, u16 tagged selector/reference | Retry while gated; otherwise latch mode, publish request and advance three bytes; request break | [Six original calls](battle-request.md): five music retries and one immediate-zero acceptance |
| `fe/a2` | Prefix `0x800869b8`, extended handler `0x8008825c` | Two bytes | Retry the prefix while music load result is exactly `ffffffff`; otherwise advance two bytes; request break | 993 pending calls; ready branch has source/synthetic evidence only |

The disassembler follows both known branch successors, terminates cycles and
rejects overlapping instruction interpretations, truncated operands and targets
outside the package. Unknown instructions raise `UnknownInstruction` with the
opcode, namespace and exact bytecode PC. It cannot disassemble the whole forest script yet.

## Slot state and selection

The original actor state contains eight slots. Each has a u16 resume PC, u8
countdown, u8 event tag and u32 control word. Bits 18–21 of the control word
contain priority. The separate working PC is u16 and the selected slot is u8.
For external correlation, slot zero starts at actor `+0x8c`, slots have stride
eight, working PC is at `+0xcc`, and selected slot at `+0xce`. These coordinates
are not a native agent API. The reconstruction uses named semantic fields and
keeps its RAM correlation codec separate.

Selection at `0x800a2194..0x800a2214` takes the smallest priority, with the last
slot winning ties. If every priority is 15, it selects slot zero, assigns that
slot the actor's event-one entry and sets its priority to seven. Countdown,
event tag, other control bits and working PC stay unchanged at this stage.
Original comparisons cover 29,968 selections: one at priority zero, 1,649 at
priority seven and 28,318 all-ended fallbacks. No active-slot priority tie occurs
in this route; that tie rule has source and synthetic evidence only.

Both end instructions set selected priority to 15 and tag to `ff`. Instruction
`04` first replaces the resume PC of every priority-seven slot with the actor's
event-one entry. Instruction `00` sets both interpreter controls to one; `04`
sets only the break request. Original globals `0x800affec` and `0x800b00c0` are
called budget mode and break request here because of their observed dispatch
tests. Their names do not claim player-control readiness.

The caller copies the chosen resume PC into the working PC before dispatch and
copies the working PC back after the batch. That surrounding caller and actor
eligibility still need a complete original comparison. Slot selection alone
does not establish the entire scheduler or a simulation tick rate.

## Interpreter batches

[EVID-REF-017](../findings/EVID-REF-017.json) validates the dispatch policy at
`0x800a1ec8..0x800a202c`, reconstructed in `tools/analysis/event_schedule.py`.
Entry clears the break request, stores its u32 argument as the budget and starts
counter zero. A nonpositive **signed** budget returns immediately. Before each
dispatch, a counter of 1,025 stops the batch; counter 1,024 still dispatches.

After a handler returns, budget mode zero replaces the budget with 65,535.
An enabled gate at `0x800adb1c` then stops the batch if any of the three words at
`0x800adbe0`, `0x800adbe4` and `0x800adbec` is zero. Otherwise a break is honored
only when both the break request and budget mode equal one. These exits preserve
the counter. If neither exit applies, the counter increases and another loop
check occurs only while it is less than the signed budget. Handler changes to
the budget remain inputs to this policy.

Two original windows compare 29,993 complete batches and 73,257 post-handler
decisions. The original has 25 initialization batches with budget 65,535 and
mode zero, followed by 29,968 normal batches with budget eight and mode one.
There are 29,256 break exits, 736 budget exits and one safeguard exit. In 993
initialization returns the break request is one and execution continues. The
first actor-zero batch reaches the 1,025-dispatch limit across frontend runs
510–511. This
observed startup behavior is relevant to initialization/readiness recovery.

The verifier checks original stage ordering, counters and all 40 captured global
bytes. It takes handler effects from original execution; it does not simulate
or validate unknown handlers. Nested batches, blocking gate cases, nonpositive
entry budgets and flag values beyond zero/one remain unobserved. Six synthetic
policy tests cover those numeric boundaries; nesting still needs its own case.
Eight corrupted original-input trials are rejected, and both traced runs match
the control's full RAM, external state, audio and all 18 PNGs exactly. Neither
this evidence nor the frontend frame numbers establish a native tick rate or
hardware timing.

## Waits and variable writes

For `26`, a nonzero countdown decreases by one. At zero, an operand with bit
`8000` set loads its low 15 bits; otherwise the operand is a variable byte
reference. The result is stored as a byte. Only a resulting zero advances PC.
Thus an immediate wait of N in the observed range takes N+1 handler calls,
including its initial load. This describes dispatches, not frontend frames.
Variable waits, byte truncation at 256 and waits in initialization mode remain
source/synthetic findings awaiting original execution cases.

For `35`, `38` and `39`, source selector bit `40` chooses a signed 16-bit immediate;
otherwise the source is read from the typed variable bank. The original helper
at `0x8009cfbc` ignores other selector bits. Writes use the halfword store helper
at `0x800a3074`, so odd destinations alias the preceding even reference, results
wrap modulo 65,536, and the unsigned bitmap is preserved. These operations
preserve both interpreter controls. Additional modes and arithmetic boundaries
are covered by synthetic tests without claiming unobserved original cases.

EVID-REF-016 compares the full 256-byte actor prefix and both controls across
65,018 original transitions. The wait/variable group also compares all 2,048
variable bytes and 128 type bytes before and after. Unrelated captured bytes
must match exactly. Eleven corrupted-input trials are rejected. All three
instrumented runs match the control's full RAM, external state, audio and 18 PNGs.
The evidence remains supported, with independent review outstanding.

## Extended music wait

[EVID-REF-018](../findings/EVID-REF-018.json) compares the `fe` prefix and extended
`a2` music wait. The prefix first increments the working PC as a u16, then reads
the extended byte and dispatches through `0x800ae6a0`. All 2,921 observed prefix
increments preserve the other captured actor bytes and globals. This does not
validate the effects of the other extended handlers.

Handler `a2` reads the music load result at `0x8004f308`. Exactly `ffffffff`
decrements the working PC back to the prefix; any other u32 value increments it
past `a2`. It always sets the break request to one and preserves budget mode,
slots and unrelated captured state. Both PC changes wrap as u16. The analysis
decoder recognizes only this extended instruction and reports the extended
namespace and byte PC for every unknown extended opcode.

The original candidate route executes 993 pending calls in actor-zero
initialization at bytecode `+0x00f4`, during frontend runs 510–511. These are the
same continued break requests explained by EVID-REF-017. The ready branch and
u16 wrapping are source-derived and covered by synthetic tests; they have no
original execution case yet. The original caller later commits music-poll
completion for selectors 12 and 6 during frontend runs 591 and 1601. The route
does **not** show `a2` resuming after completion. See the
[music-loading reference](music-loading.md) for ownership and service limits.
This load result is insufficient to determine player-control readiness.

## Operand types and comparisons

Original helpers at `0x800a2fe0` and `0x800a3018` interpret a variable operand as
a **byte reference**, discarding its low bit to obtain a halfword index. Odd
references therefore alias the preceding even reference. A 128-byte bitmap
selects unsigned versus signed reads for 1024 field variables. Native semantic
IDs should identify the variables; these original memory coordinates are
analysis correlations, not a proposed public agent API.

The mode byte's high nibble selects operand sources. Its low nibble indexes an
11-entry comparison dispatch table at `0x8006fd58`.

| High nibble | Sources | Conversion before comparison |
| --- | --- | --- |
| `00` | Variable, variable | Convert the right value to the left variable's signedness |
| `40` | Variable, immediate | Convert the right immediate to the left variable's signedness |
| `80` | Immediate, variable | Convert the left immediate to the right variable's signedness |
| `c0` | Immediate, immediate | Both immediate values are signed 16-bit |

Comparison indices 0–5 are equality, inequality, greater-than, less-than,
greater-or-equal and less-or-equal. Index 6 tests nonzero bitwise AND; 7 duplicates
inequality; 8 tests nonzero bitwise OR; 9 duplicates nonzero bitwise AND; 10 tests
nonzero `(~left) & right`. The duplicate table entries are retained, rather than
assigning them invented distinct semantics. Other mode nibbles/comparison indices
are explicitly unsupported by this reconstruction. Their original default paths
have not been validated as a usable instruction contract.

The two original observation windows cover frontend runs `[0,2000)` and
`[2000,5000)` of the same 4,317-run cold boot. They compare 4,350 and 12,159 calls,
respectively, including original variable-bank snapshots, type bits, resolved
`s1`/`s0` operands and the PC after the handler. Only mode `40` and comparison
indices 0, 1, 2 and 5 execute in this route. Synthetic boundary tests additionally
exercise signedness conversions and all eleven comparisons; those tests do not
promote unobserved original paths.

## Reproduce and extend

Enter the pinned `#observation-trace` shell. Use
`tools/analysis/verify_events.py prepare` with `--raw`, `--profile`, `--map`,
`--start`, `--end`, and a new private `--output` to generate a guarded trace.
Run the authored [entry-dialogue input route](../../tests/reference-inputs/field23-entry-dialogue.json)
through `tools/reference/scenario.py --trace-instructions <spec>`. Then use
`verify_events.py compare` with the same arguments and `--capture <run>/capture`.
The comparison also checks the original field structures against final RAM.

For the state effects, use `tools/analysis/verify_event_state.py prepare` and
`compare` with `--raw`, `--profile`, `--map` and `--group`. The three groups are
`slot-control`, `wait-variables` and `slot-selection`; each needs a separate new
capture. Defaults cover frontend runs `[0,5000)`. Use the same authored route and
pinned shell. Reports and source-qualified trace specifications remain private.

For batch policy use `tools/analysis/verify_event_schedule.py prepare` and
`compare` with `--raw`, `--profile`, `--map`, `--start`, `--end` and private
`--output`. Capture windows `[0,2200)` and `[2200,5000)` separately using the same
authored route. Complete batches must be present inside each observation window;
the verifier rejects a window that starts or ends inside an unmatched batch.

This route advances dialogue with repeated Cross presses. It does not demonstrate
an encounter, victory, rewards, a save round trip or the Phase 1 slice exit.
The [recovery inventory](../recovery.json) retains the unknown instruction set,
symbols and formats. Next, validate the remaining operand/slot modes against
original execution and recover actor eligibility, initialization ownership,
dialogue and encounter entry. The music wait and observed loading policy now have
comparisons; their unobserved branches and the rest of semantic readiness remain
explicit work in that inventory.
