# Actor defaults and initial floor queries

[EVID-REF-030](../findings/EVID-REF-030.json) reconstructs field actor defaults
`80080a74..80080f44` and the resident RNG step `8003fa38..8003fa68` (exclusive
ends). The [authored model](../../tools/analysis/actor_defaults.py) compares all
25 calls during the observed encounter return. It provides original-format state
correlation; it does not allocate native actors or establish valid scenario entry.

The field source is overlay
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`,
Disc 1 slot 36, LBA 108933, base `8006faf0`. Resident source is executable
`dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119`,
LBA 108606. All execution evidence uses profile `na-slus-00664-39c547a9afc6`.

## Recovered effects

Defaults preserve incoming actor bytes except for the original stores. They
reset movement, event selection and eight slots; initialize animation, floor and
direction fields; and clear selected packed flags while retaining other bits.
Slot priority becomes 15, with resume PC and the low control halfword `ffff`.
The complete 312-byte actor is compared, including untouched allocation pointers.
The model accepts the existing actor allocation as input.

Each call advances resident seed `8005a1fc` once:
`seed = (seed * 41c64e6d + 3039) mod 2^32` (hexadecimal constants). The returned
value is `(seed >> 16) & 7fff`; the last mask occurs in the return delay slot.
Actor `+102` receives that halfword only after return. The seed's producers,
other consumers and full-game RNG lifetimes remain separate requirements.

The routine queries layers `0 .. signed16(800afb54)-2`, using the descriptor's
signed low X/Z halfwords. The observed layer count is three, so each actor gets
two queries. Each query writes a three-word normal and three-halfword point,
preserving padding in its 16-byte normal and eight-byte point records. The
original triangle counts are separate mutable values; parsed resource bounds
still constrain host reads.

The source locator returns zero with zero outputs when no triangle matches.
This differs from a successful triangle-zero query. The model retains the
original stored zero and terrain lookup behavior, while its diagnostic stages
expose whether a match occurred. After a query, a non-`-1` signed-halfword result
that is not below the unsigned original count clears that count and those output
components. These branches have source and synthetic coverage; all 50 captured
queries match and fall within their counts.

Terrain is read through the existing triangle attribute lookup. The layer-zero
normal becomes actor `+50/+54/+58`. If descriptor `+58 & 80` is clear, the point's
signed height replaces descriptor Y. Then XYZ shift left 16 with 32-bit wrapping,
and the low Y halfword becomes actor `+72`. Every captured call has the Y-preserve
bit set; Y replacement remains source/synthetic coverage.

The 96-byte local scratch region must be supplied explicitly. With no queries,
the original can consume old layer-zero values; the model does not invent zero
scratch or declare an empty field ready. Invalid storage and out-of-layer geometry
raise explicit errors. These bounded host failures do not emulate arbitrary
invalid PS1 memory accesses.

## Evidence and limits

The original capture contains 446 records: 300 within 25 complete defaults calls,
142 factory context records, and four later terrain entries without matching
returns. The model compares the complete actor, all 25 captured descriptors,
local scratch and preserved stack bytes, RNG, mesh controls and stable globals.
It checks each captured nested call's arguments, saved return and stack identity.
The final returned API state is also bound to the final original state. All
686,400 compared payload bytes agree; this count includes repeated observations.
The capture matches all 124 uninstrumented control artifacts exactly.

The factory calls indexes 0 through 70. Only 0 through 24 initialize actors;
46 later calls return without allocation. Actor and shadow allocation addresses
are observed inputs to the correlation, not a reconstructed allocator result.
No optional actor extension occurs in this recording. The later source-qualified
RAM anchor retains the complete collision component and reciprocal table, and
all live pointer/count tables agree. There is no per-call hash of every mesh byte;
the exact query outputs are the execution comparison.

Independent review checks the default/RNG source, original replay, 14 authored
synthetic tests and 76 corrupted source, state, lineage and API-result cases.
A separate reviewer rejects 19 wrong-model outputs, including six altered final
API fields. Thirty connected Ghidra exports are byte-qualified, including six
12-byte PSX macro records; this does not establish 30 complete semantic models.

One allocation fact follows directly from resident constructor `80024524`:
it requests `164` hexadecimal bytes (356) and stores that size at sprite `+86`.
The earlier 512-byte sprite captures are observation windows that include 156
neighboring bytes. Their exact comparisons remain valid; they did not establish
512-byte object ownership. Constructor behavior and resource ownership are the
next connected reconstruction, followed by initialization VM effects and readiness.

Field loading, event initialization, sprite creation/rebinding and destruction,
shadow/geometry ownership, optional return variants, fresh scenario setup and
player-control readiness remain incomplete. This finding closes no broad Phase 1
todo and no complete slice proof domain.
