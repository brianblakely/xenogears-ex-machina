# Hardware-facing services on the frozen route

This census identifies the original code that talks to PlayStation hardware on
the frozen field 23 route, and states the service boundary a native
implementation replaces. It is evidence of hardware-call intent, not a timing
proof: the observation core is an HLE-BIOS interpreter.

## Method

The qualified resident Ghidra export (index
`.local/ghidra/subsystem-survey-disc1-v4/index.json`, 960 resident functions)
lists every data reference. 86 resident export entries reference hardware
registers (`1f801000..1f802fff`) directly. Some entries are fragments or loop
heads of larger routines, so counts are hits on entry addresses, not calls, and
group sums can count one routine more than once. The entries were hooked in
groups of up to 16 (guarded entry PC, no snapshots) in six 300-400 frame windows of the route: field with a music load
(1500-1800), field walking (5000-5300), combat (6700-7000), post-battle
(8500-8800), field return (10250-10550) and the closing menu entry
(10800-11200). Every capture's final state matches the uninstrumented route. Two
captures (hook group 2 in the field windows) reached their callback budget on a
VSync wait, so cells marked `+` are lower bounds. The private census
`.local/execution/connected/hw-census.json` lists every function, register,
count and call site. See [EVID-REF-044](../findings/EVID-REF-044.json).

Device groups follow the registers each entry references. Code is named by
address only; no library identity is claimed.

| Device group | Export entries | Entries hit on route | Entry hits per window (field-music, field-walk, combat, post-battle, return, menu) |
| --- | --- | --- | --- |
| CD controller | 13 | 8: `800415b4`, `80041618`, `8004196c`, `80041c68`, `80041c98`, `80042088`, `80042ca8`, `80042cdc` | 670 / 0 / 1263 / 561 / 1540 / 902 |
| CD controller + DMA + memory control | 2 | 1: `80042aa8` | 173 / 0 / 323 / 148 / 417 / 231 |
| CD controller + SPU | 1 | 0:  | 0 / 0 / 0 / 0 / 0 / 0 |
| CD controller + memory control | 2 | 0:  | 0 / 0 / 0 / 0 / 0 / 0 |
| DMA | 7 | 5: `80042a58`, `80045d5c`, `8004c098`, `8004c21c`, `8004ce9c` | 698 / 450 / 334 / 313 / 779 / 15692 |
| DMA + GPU | 8 | 5: `800465ec`, `800466d8`, `8004696c`, `80046dec`, `80046f30` | 407+ / 1812+ / 5345 / 1500 / 3167 / 13670 |
| DMA + SPU | 2 | 0:  | 0 / 0 / 0 / 0 / 0 / 0 |
| DMA + interrupt control | 3 | 0:  | 0 / 0 / 0 / 0 / 0 / 0 |
| GPU | 6 | 3: `80045ed0`, `80046560`, `80046638` | 252+ / 732+ / 1104 / 600 / 455 / 676 |
| GPU + root counters | 2 | 2: `8004b54c`, `8004b5d0` | 299083+ / 295298+ / 8655 / 3032 / 3191 / 43469 |
| SPU | 24 | 4: `8004cb3c`, `8004cca8`, `8004d070`, `8004e574` | 402 / 0 / 10 / 0 / 402 / 0 |
| interrupt control | 8 | 3: `80040a4c`, `8004b8bc`, `8004b9b4` | 2556+ / 4191+ / 7320 / 3158 / 3780 / 4852 |
| memory control | 2 | 1: `8004d1b0` | 80 / 0 / 2 / 0 / 80 / 0 |
| root counters | 5 | 1: `80040690` | 2400 / 2398 / 2400 / 2400 / 2400 / 3200 |
| serial/pad | 1 | 1: `800409e4` | 300 / 301 / 296 / 300 / 284 / 399 |

## Service boundaries

| Service | Original intent on the route | Native boundary | Reconstructed so far |
| --- | --- | --- | --- |
| Display | Ordering-table clear by DMA (`80045d5c`), GPU DMA (`800465ec`, `800466d8`) and completion waits every frame; the field's VSync wait loop (overlay `800758d0`) polls a resident entry about 1,100-3,000 times per frame on average | Submit a frame's primitive list; frame completion | None: drawing is not reconstructed |
| Controller | One serial/pad transaction per frame | Per-frame controller state | Input queue reset (80035db0) and command-code consumers |
| Disc | CD commands during music, battle and return loads: the command writer `80042088` is always called in its no-wait mode on the route, writing the command and parameters to the controller registers and returning. The acknowledgement arrives through the interrupt callback path (`8004ba94` to `80042ca8`/`80042cdc` to `800415b4`), which updates the RAM flag `80056788` that the sync wait `80041b3c` checks | Asynchronous command and sector reads with completion status | Disc status `800286cc` and the data-sync wait `8004293c` (which reads DMA3 status from the observed I/O page); the resident disc stream ring |
| Audio | SPU voice and control registers written while music loads | Voice key on/off and parameter updates from driver state | Sound-driver state the field reaches: effect voices, sequence and wave-bank release, SPU memory table (EVID-REF-042) |
| Interrupts and time | Interrupt acknowledge and dispatch several times per frame; root counter reads 8 times per frame | VSync tick and timed callbacks | Interrupt handlers are bracketed in comparisons, not reconstructed; the VSync counter `80058960` is owned state |

The music load of opcode 75 stops before this boundary in the reconstruction. Its
read setup calls the control entry `8004111c`, which runs the sync wait `80041b3c`
(the completion flag was already set in every observed call, so it returned at
once) and the no-wait command writer. That call changes only `800564c5..c6`,
`800564c9`, `80056788` and `8005a228..8005a231`, and the DMA callback
registration changes only `80058978` plus the DMA interrupt register
`1f8010f4`. On the observed path these are computable RAM effects plus register
writes; a native disc service receives the writes as commands. The
reconstruction does not synthesize controller responses.

## Limits

This is a sampled census of directly referencing functions. Functions that reach
hardware only through pointers without a resolved data reference are not listed.
Timing and cadence here are HLE interpreter observations, not hardware timing.
