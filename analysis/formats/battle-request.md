# Field battle request — primary 71

[EVID-REF-031](../findings/EVID-REF-031.json) reconstructs the field handler at
`80093568` and its tagged operand reader. This queues a battle request; battle
entry, combat, rewards, return ownership and player readiness remain separate.
All field addresses here belong to decoded overlay
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`,
Disc 1 source slot 36, LBA 108933, loaded at `8006faf0`.

The [authored model](../../tools/analysis/battle_request.py) preserves the six
ordered gate tests below. The original reads each gate as a 32-bit word and
stops at the first blocked condition. Names with source suffixes retain unknown
roles instead of assuming a complete readiness condition.

| Order | Original source | Retry condition |
| --- | --- | --- |
| 1 | `800adbdc` | Zero |
| 2 | `800adbe4` | Zero |
| 3 | `800adbec` | Zero |
| 4 | `800adb2c` | Nonzero |
| 5 | Music result `8004f308` | Exactly `ffffffff` |
| 6 | `800adb90` | Nonzero |

A retry stores one to interpreter break request `800b00c0`, preserving the
working PC and request fields. It does not read the selector operand. These are
handler effects; surrounding scheduler policy determines whether a break yields.

When all gates allow a request, the original first copies the byte at
`800b2356` to `8005954c`. The two-byte selector follows the opcode. Helper
`800acdec` treats bit `8000` as a 15-bit immediate tag; otherwise it calls
`800a3018` to read the typed variable bank. Odd references alias the preceding
even reference. The variable's type bit chooses signed or unsigned extension;
the later selector store retains only eight bits.

The `800acdb8` helper loads operand bytes separately, high byte before low byte,
using bytecode base plus the zero-extended working PC plus the operand offset.
That address arithmetic does not wrap to 16 bits. The model uses a bounded byte
buffer and rejects missing data; it does not emulate invalid original accesses.
The nearby unsigned-mask helper `800a2fe0` was reviewed as context but is not a
callee of this handler. The handler and actual operand dependency bodies contain
113 reviewed words; the adjacent helper adds 14.

After operand resolution the handler publishes these stores in source order:

1. Store the selector byte at `80059508`, then zero byte `800594f8`.
2. Clear words `800adbdc` and `800adbe0`.
3. Set words `800adb88` and `800b00c0` to one.
4. Add three to the actor's working PC and store its low 16 bits at actor `+cc`.

The model exposes the intermediate state after the mode byte is latched and
before selector publication. Its immutable host inputs and malformed-input
rejection do not establish transactional behavior for the original game.
Original addresses remain correlation metadata, not the future native API.

The public [disassembler](../../tools/analysis/events.py) recognizes the
three-byte encoding, preserves the raw tagged operand, and follows both retry
and continuation successors. Unknown following instructions still fail with
their opcode and PC. The C++ core event library does not yet implement this
handler; this Python reconstruction is not a complete native interpreter.

## Evidence and limits

The preserved ordinary encounter route from [EVID-REF-020](../findings/EVID-REF-020.json)
contains six calls by actor 14. Five retry at PC `8ab` while music is pending;
the sixth accepts immediate selector zero, latches mode five, and advances to
`8ae`. The comparison matches all 13 captured boundaries, each containing 13
regions totaling 4,516 range bytes (4,512 distinct RAM bytes), including
unchanged actor, bytecode and control data. Selected SP/RA boundaries and the
resolved selector register also match. Stack memory and all other registers are
outside this capture's comparison scope.

All 124 complete-run control artifacts match, and all 146 historical EVID020
artifact fingerprints reproduce. Twenty-seven new corrupted state/lineage cases
and the earlier 15 corruption cases reject. Independent instruction review,
original replay and address-based projection checks pass. Twelve public test
groups cover source-derived gates, byte truncation, signed variables, PC bounds,
lazy operands and disassembly. The independent source-formula checks remain
synthetic evidence; they do not expand the six observed original calls.

Other gate paths, variable selectors, invalid references and PC boundary cases
remain source/synthetic coverage. This one route does not establish full script
execution, arbitrary formation validity, combat rules, tick cadence, readiness,
or the Phase 1 slice exit.
