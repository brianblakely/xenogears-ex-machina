# Original service, timing and state boundaries

What the recovered PS1 program needs from its platform, and where its timing,
control flow, presentation and sound modes cross that boundary, for the native
port (plan.md Phases 2-5). Every statement names the matching source
(`decomp/src`, functions by address) that shows it; observations of the
original are marked with their capture and its limits.

Related records: [sound-modes](../analysis/formats/sound-modes.md) (Mono/Stereo/Wide
arithmetic and recordings), [rendering-behavior](../analysis/formats/rendering-behavior.md)
(packets, environments, dithering and OT order on the Disc 1 forest slice),
[service-boundaries](../analysis/formats/service-boundaries.md) (hardware-entry
census of the forest route, EVID-REF-044), the script machines in
[docs/scripts](scripts/) and the build and coverage rules in
[matching.md](matching.md).

## Evidence and its limits

- Static facts come from the matching source and from `tools/service_calls.py`,
  a linear scan of the linked images (calls, `lui` base tracking, cop2 use). It
  skips SDK and handwritten code, counts but does not resolve the 88 indirect
  calls (`jalr`) and misses accesses through passed-in pointers. Caller counts
  below come from this command at this revision; regenerate them rather than
  copying:

  ```sh
  nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching -c \
    python3 tools/service_calls.py <psx_psyq_signatures data dir> \
    decomp/targets/resident/slus_006.64.mk decomp/targets/overlays/*.mk [--detail]
  ```

  The signatures are the pinned `psx_psyq_signatures` input of nix/ghidra, at
  `lib/ghidra/Ghidra/Extensions/ghidra_psx_ldr/data/psyq` in the loader package
  (`nix build --print-out-paths path:./nix/ghidra#psx-loader`).
- Original observations ran on the PCSX-ReARMed libretro core (core sha256
  e61a8d4ac1f5…) with its HLE BIOS
  (`.local/scenarios/p1-original-route-review-20261002.json`: "timing and
  compatibility require corroboration"), never on hardware. Sound and movie
  schedules were recorded on Disc 1 only; the forest-slice rendering census is
  Disc 1 field 22/23, battle and menus. Both discs run the same code (the
  resident images differ only in the embedded disc index, the overlays are
  byte-identical), so the Disc 1 observations cover the code paths, not Disc 2
  data or timing. No packets of the world map, Gear battles or the arena have
  been observed; their display facts below are static. Captures stay local
  under `.local/scenarios`.

## Services

Game code reaches hardware through the PsyQ libraries linked into the resident
(classified `sdk`, 8003f8e8-8004eaa0) and into the mdec movie library image
loaded at 0x801d3000 (libpress, libds and libcd streaming, 801d444c-801d68b4).
The scan finds 4436 compiled game functions and these library entries
(caller functions in parentheses):

| Library | Entries the game calls |
| --- | --- |
| libgpu | DrawSync (159), AddPrim (135), GetTPage (114), LoadImage (109), SetSemiTrans (76), GetClut (69), SetShadeTex (59), MoveImage (40), PutDispEnv/PutDrawEnv (38/37), SetDrawMode (30), DrawOTag (27), ClearOTagR (25), StoreImage (23), ClearImage (17), OpenTIM/ReadTIM (17), SetDefDrawEnv/SetDefDispEnv (15), SetDispMask (13), SetDrawTPage (13), ResetGraph (8), DrawSyncCallback (5), SetDrawMove, SetDrawArea, SetDrawOffset, SetDrawEnv, SetTexWindow, DrawOTagEnv, DrawPrim, primitive setters |
| libgte | SetRotMatrix (94), SetTransMatrix (92), SquareRoot0 (61), ratan2 (56), RotTransPers4 (51), CompMatrix (47), VectorNormal (36), ScaleMatrix, TransMatrix, lighting (NormalColor*), Push/PopMatrix, SetGeomOffset/SetGeomScreen, SetFogNearFar, and 16 entries without a signature name (below) |
| libcd | CdSyncCallback (17), CdIntToPos (16), CdControlF (14), CdReadyCallback (14), CdDataCallback (10), CdControlB (8), CdPosToInt, CdGetSector, CdInit, CdSync, CdDataSync, CdFlush, CdReadCallback, CdMix (dormant, see Sound); mdec: CdRead2 and the St* streaming ring |
| libapi | events (Open/Close/Enable/Disable/Test/UnDeliverEvent), root counters (Set/Get/Start/StopRCnt), critical sections (11 each), FlushCache, InitPAD/StartPAD/StopPAD/ChangeClearPAD, BIOS file calls open B(32h), read B(34h), write B(35h), close B(36h) and B(41h)-B(45h) (Memory card and saves), Krom2RawAdd B(51h), A(ABh) card check, GetGp (reported as ChangeClearPAD+0x10, which holds it) |
| libetc | VSync (75), VSyncCallback (3), ResetCallback, SetVideoMode |
| libspu | SpuInit/SpuQuit, SpuInitMalloc, SpuSetCommonAttr, SpuSetReverb and the reverb mode setters, SpuGetReverbModeType, SpuSetIRQ/SpuSetIRQCallback, SpuSetTransferMode/StartAddr/Callback, SpuReadDecodedData, SpuGetVoiceEnvelopeAttr, SpuSetNoiseClock; SpuRead/SpuWrite are func_8004D818/func_8004D878 by inspection (unattributed, called by sound.c func_8003BE68) |
| libcard | InitCARD, StartCARD, `_bu_init` (A(70h)) |
| libpress, libds | DecDCTReset/DecDCTin/DecDCTout/DecDCTvlc/DecDCTvlcSize and DecDCToutCallback, all from mdec |
| libc, libc2 | bzero (126), rand (96), memcpy (33), memmove (16), sprintf (11), strcat, strcpy, strlen, memset, memchr, strcmp |
| libsn | PCopen, PCcreat, PClseek, PCclose and func_8004C38C/func_8004C398/func_8004C470 (PCinit/PCread/PCwrite by inspection): development-host paths only (D_8004FE48 set, or `D_80010000 != -1`) |

BIOS stubs are named by table and number; splat merged consecutive stubs into
one symbol (`close` also holds B(41h)-B(45h), SpuGetReverbModeType ends with
A(ABh)). The slot39 headers name them by use: format, firstfile, nextfile,
rename, erase (slot39/menu.h). func_80040C3C (unattributed) is the pad send
registration: it calls `_SendPAD` with its four arguments (Controllers).

The libgte entries without a signature name, by their code:

| Entry | What it computes |
| --- | --- |
| func_80048E94 | an orthonormal matrix from a matrix's first two rows: two outer products (OP), each row normalised by 80048DD8 (MatrixNormal form) |
| func_8004901C | weighted sum of two SVECTORs, GPF then GPL (LoadAverageShort12, psyq/libgte.h) |
| func_800495DC | SVECTOR times the rotation matrix, MVMVA sf=1 without translation, to a VECTOR |
| func_8004998C | a 32-bit VECTOR times the rotation matrix, split into 15-bit halves (two MVMVAs) |
| func_80049ACC | m0 = m0 x m1 through the rotation registers (MulMatrix form) |
| func_8004A10C | far colour = rgb << 4 (SetFarColor) |
| func_8004A414 | SQR sf=0 of a VECTOR (Square0) |
| func_8004A480 | outer product sf=1, keeping the rotation diagonal (OuterProduct12) |
| func_8004A6DC | RT x v + TR with FLAG out (RotTrans) |
| func_8004A70C | NCLIP of three screen points (NormalClip) |
| func_8004A83C | RTPT, NCLIP, then RTPS, AVSZ4, depth cue and FLAG out (RotAverageNclip4's arguments) |
| func_8004A8EC | copy a matrix's rotation transposed (TransposeMatrix form) |
| func_8004A92C, func_8004ABBC | rotation matrix from three angles through the sin/cos table D_800523F0 (RotMatrixYXZ and RotMatrix in psyq/libgte.h) |
| func_8004AE4C, func_8004AFEC | single-axis rotation matrix (RotMatrixX, RotMatrixY in psyq/libgte.h) |

Outside the libraries the game itself uses:

- **SPU registers** through the pointer `D_800508E4` (0x1F801C00, sound.h),
  loaded by 15 functions of sound.c and main2_800366E0.c (the voice-register
  writers). No compiled game function addresses the I/O window directly.
- **GTE** inline in 78 compiled functions (the PsyQ GTE macros) and in the
  handwritten renderers (resident model renderers, menu and world-map batches).
- **Scratchpad** (0x1F800000-0x1F8003FF) as work memory in 98 functions, 80 of
  them in the world map, and as a temporary stack (field func_8007554C, battle
  `SPAD_STACK_ENTER`).
- **BIOS Kanji ROM**: Krom2RawAdd returns a ROM glyph address for a Shift-JIS
  code. slot39 func_801E65E4 draws every listed save title from it; field
  func_800ABFDC uses it for staff-roll codes outside the game font (8540-887F),
  reachable only through field event BE, which no shipped script uses
  (tools/analysis/staff_roll.py). A hosted build needs the user's BIOS font or
  a labelled substitute.

### Controllers

- InitPAD fills `D_800625FC[2]` (0x22-byte receive buffers); StartPAD and
  ChangeClearPAD(0) follow (main2.c func_80036288). func_8003569C accepts types
  0x40 (digital), 0x50 (analog joystick) and 0x70 (analog pad) with status 0;
  func_80035734 reports 0 none (status 0xFF), 1 digital, 2 mouse, 3 joystick,
  4 analog pad, -1 other. Field, world-map, battle and menu loops pause while
  the first pad is missing.
- func_800358BC remaps the low button byte through the assignment D_80050238
  (func_800357C0; default {1,0,3,2,4,5,6,7}) and swaps the shoulder and face
  bits of a 0x50 pad (func_8003582C). Analog pads store the stick bytes
  data[0..3] in D_80059444/D_8005944C/D_80059430/D_80059438 (port 0) and
  D_80059448/D_80059450/D_80059434/D_8005943C (port 1); a digital pad gets
  D_80059430/D_80059438 from the d-pad (tables D_8005020C/D_8005021C). Only the
  arena reads them (menu3.c func_80076884, menu4.c func_8008162C).
- Vibration: `D_8005A1BC[2]` hold each port's four transmit bytes, registered
  once with func_80040C3C(act0, 4, act1, 4) (func_8003611C). The vblank handler
  steps them (func_80036220/func_80036188): {1, 0x40, 1, 0} while the timer
  runs, {1, 0x40, 0, 0} once, then off. func_80036258(port, frames) starts one;
  only the arena calls it (menu3.c func_800776A8, per side, gated by the
  settings' port vibration options D_80099D98.option4/.option5).
- Input is sampled once per vertical blank, not per game frame: the vblank
  handler queues held, pressed and repeat words for both ports in a 16-entry
  ring (func_80035C0C; overflow sets D_80050208); loops dequeue
  (func_80035CDC), or OR all queued states (func_80036420, which resets after
  an overflow). Repeat starts after 32 unchanged vblanks, then every fourth
  (`D_80059488 & 3`).
- Soft reset is the remapped held word 0x90C (L1+R1+Start+Select) in
  D_80059570 (Control flow).

### Memory card and saves

- Start-up: InitCARD(1), StartCARD, `_bu_init` at boot (main.c func_80019578)
  and again on the file screens (slot39.c func_801D9B08), which also open and
  enable the four card events on 0xF4000001: 4 (done), 0x8000 (error), 0x100
  (timeout), 0x2000 (new card). func_801C881C busy-polls them with TestEvent
  (no VSync); func_801C891C starts a check with A(ABh) and waits there. The
  file screens save and clear the CD callbacks around card access
  (func_801D9C84, restored by func_801D9E3C).
- Files: devices "bu00:" and "bu10:" (D_801C50A8/D_801C50B0); a save is
  "BASLUS-00664" followed by the character '0' + digit, digit 0-14
  (func_801CB9E8), so digits 10-14 give ':' ';' '<' '=' '>', which a host
  filesystem may reject. func_801C8D78 lists a card with B(42h)/B(43h);
  func_801C9038 reads each file's first 0x200 bytes. func_801C65F4 also stores
  "BASLUS-01160" in MenuCard.otherPrefix, which no recovered code reads.
- Save (func_801CBD90): erase "bu0X:__tmp_file" (B(45h)), create it with
  `open(name, blocks << 16 | 0x200)`, reopen it for writing (mode 2), write the
  0x100-byte header (magic "SC", icon flag 0x11, one block, a 0x5C-byte
  Shift-JIS title from func_801CA8C0, a 0x20-byte palette and the 0x80-byte
  16x16 icon), then the 0x1F00-byte payload in 0x100-byte writes, one menu frame
  (func_801C7BF4) before each; payload byte 0x1EFF is the 8-bit sum of bytes
  0-0x1EFE. Rename (B(44h)) to the final name commits it. An unformatted card
  is formatted with B(41h). Writes retry three times.
- Load (func_801CB304, READ_SAVE): read the 0x2000-byte file in 0x100-byte
  reads (five retries) into a 0x2100-byte heap block, compare the sum, then
  func_801CB28C: func_801E4D10 copies the saved blocks and recomputes the gear
  fields that are not saved, D_80059488 = the saved play time and
  D_8005A3A0[16] (battle-AI globals) come back from GameData +0x2324. The field
  finishes a load through event variables 0x46 and 4 (field.c func_800799D4).
- Payload layout (slot39/menu.h): the play time in vblanks at +0 and the party
  summary with the file digit at +0x23 (MenuSavePayload), then SaveData's
  blocks: names +0x24, GameData +0xDC at +0x100, characters +0x290, gear
  subsets +0x99C, more GameData blocks at +0xE4C, +0xEC4, +0x1024 and +0x1124,
  ending at +0x1B5C; the rest is zero. tools/analysis/menu_save_file.py checks
  the serialisation against a card image a capture wrote.

## Timing

### VSync and the clocks the game reads

libetc VSync (8004b54c): VSync(0) waits for the next vertical blank; VSync(n),
n > 1, returns once n blanks have passed since the previous wait returned (at
least one new blank); VSync(1) returns the horizontal blanks since the last
wait; VSync(-1) returns libetc's blank counter (D_80058960). The game calls it
from 75 functions: VSync(0) 89 sites, VSync(2) 15, VSync(3) 13, VSync(1) 13,
VSync(-1) 4, and VSync(8), VSync(D_80092898), VSync(D_80059198 + 1).

- VSync(-1) feeds game logic at exactly four sites: the start and end of the
  field frame (field.c func_8007554C) and of the battle frame
  (battle_800BD3AC.c func_800BE790).
- VSync(1) feeds only profiling values (field D_800ADB9C/D_800ADBA0/D_800ADBA4
  read by debug595, battle D_800D309C.cpu/gpu read by debug2611, arena
  D_800927F0 and the movie monitor's D_800773B8).
- GetRCnt: the sound tick times itself on root counter 2 (D_800595C4,
  D_80059540; profiling); the arena compacts its ordering table while root
  counter 1 (horizontal blanks) stays within a budget (menu7.c func_8008AC8C,
  func_8008ACB8), which changes which empty tags are skipped, not the image.
- D_80059488, the game's own blank count (vblank handler), is the play time
  stored in saves; it also drives input repeat, arena colour cycles and the
  arena's vblank hook. Pause loops (field func_80077E88, world map
  worldmap_80072238.c, battle battle.c, menus ovl2598/ovl2600/ovl2601/ovl2602)
  save and restore it, so paused time is not counted.
- Video is NTSC only: SetVideoMode(0) at boot (main.c func_80019578).

### Pacing per mode

| Mode (entry) | Frame wait | Blanks per frame | Notes |
| --- | --- | --- | --- |
| 0 kernel menu (resident func_8001A4B4) | DrawSync, VSync(0) | 1 | development start screen |
| boot logo (func_80019D48) | VSync(0) per step | 1 | 16 fade-in steps, 110 held, 17 fade-out |
| 1 field (func_80077E88, frame func_8007554C) | start = VSync(-1); DrawSync and VSync(0) mid-frame; at the end busy-waits until VSync(-1) >= start + D_800B2078.unk217C + 2 | 2, 3 or 4 | unk217C comes from field event d6 (func_800925A0, operand 0/1/2, which also sets text speed 8/6/4); scripts use all three values on both discs. Sprite scripts tick D_80059198 + 1 = 2 times per update (field.c func_80071FB0) |
| 1 field pause | DrawSync, VSync(2) | 2 | while the pad is missing or Start is held (func_80077E88); D_80059488 restored |
| 1 field movie (func_800A7C58) | mode D_800ADB74 0: VSync(0) before each decode batch (func_800A732C); 1: field overlays and VSync(0) (func_80075910); 2: the full field frame | 1, or the field's | movie progress D_800B06A0 comes from the MDEC callback |
| 2 battle (resident func_8001B6C4, overlay func_80070F40, frame func_800BE790) | start = VSync(-1); after DrawSync D_80059494 = VSync(-1) - start - D_80059198, clamped 0-4; then VSync(D_80059198 + 1), or VSync(0) when D_80059198 is 0 | 1, plus catch-up | the next frame re-runs camera and sprite updates (func_800BBAB8, func_8001C964) and effects (func_8008A9C0(1)) D_80059494 more times. func_800B88C4 leaves D_80059198 = 0. Turn logic calls frames from inside rules (func_800716D8, nesting counted by D_800C37D0). Development: Select gives VSync(8) |
| 3 world map (func_80070CFC, loop func_800712D0) | DrawSync, VSync(2) | 2 | first spins VSync(0) while the terrain stream reports 3 (func_800967E4); sprite scripts tick twice per update (D_80059198 = 1, func_80072238) |
| 4 Battling arena (`menu` target, func_80088E90) | VSync(D_80092898) | 1 or 2 | the arena task sets D_80092898 to 0 or 2 (menu5.c func_800852C4); the arena coroutine runs once per frame (Control flow) |
| 5 in-game menu (resident func_8001C634; frames func_8001C074, slot39 func_801C7BF4) | DrawSync, VSync(0) | 1 | also the ovl2598/2600/2601/2602 screens; card and disc-change waits inside |
| 6 movie (func_800737EC, player func_80076488) | VSync(0) per pass, three decode steps per frame (func_801D3F7C) | 1 | mdec's DecDCTout callback delivers frames |
| battle screen effects (ovl2615 load/burst modes, ovl3387) | DrawSync, VSync(2) | 2 | |

The sound driver runs beside this on its own timer: 240 ticks per second, about
4.01 per NTSC vertical blank, so the ticks per frame are not a whole number.

### Interrupt-context work

| Callback | Installed by | Runs | Writes | Waits that depend on it |
| --- | --- | --- | --- | --- |
| vblank handler func_8003634C (main2.c) | VSyncCallback at boot (func_80019578) and on world-map entry (func_80070CFC); cleared by soft reset | every vertical blank | D_80059488++; pad words D_80059570/D_80059574, D_8005948C/D_80059490, D_800594A4/D_800594A8 and the sticks; the input ring D_8005A0FC-D_8005A19C; the h:m:s clock D_80059370/D_80059418/D_80059420/D_80059484 (stops at 100 h, D_800501F8); actuators D_8005A1BC; then the hook D_800501FC | input readers |
| vblank hook D_800501FC | the arena (menu5.c func_800852C4: func_80084FD0); cleared by the dispatcher | every vertical blank | on odd blanks while D_80092784 is set, func_8008E120 steps the arena's glow field, calling rand(): interrupt-time draws from the gameplay RNG | — |
| sound tick func_8003C020 (sound.c) | func_80037B88: OpenEvent(0xF2000002), SetRCnt(0xF2000002, 0x44E8, 0x1000) (system clock / 8: 4233600 / 17640 = 240 Hz) | 240 Hz unless flag 0x40 (suspended) | D_80059504 (tick count, stamps effect voices); every other tick the master and CD fades (D_8005A3C0), every tick the sequence slides and beats (D_80059564), staged voice registers written through D_800508E4 (func_8003E900, func_8003EB5C), SpuSetCommonAttr, SPU IRQ re-enable (D_8005955C) | the main loop reads the same driver state between ticks |
| SPU transfer done func_8003BB64 | func_80037B88, each transfer (func_8003BE68) | DMA completion | runs the transfer's callback with flag 4, clears flag 0x10, starts the next of the eight queued transfers (D_80059510/D_800594F4) | `while (func_8003BDFC(0))` spins (battle, movie), `while (D_8005957C & 0x10)` (world map func_80072238), ring space in func_8003BCA0 |
| SPU IRQ func_8003BFA0 | func_80037B88 | SPU IRQ | D_80059514++, hook D_8005950C | — |
| CD sync/ready/data | resident func_80029690 (one file), func_80029AFC (file list), func_80029EB0 (image stream), func_8002A2D0, then the callbacks themselves (main_8002709C.c); world map func_8009699C, func_80096A6C (installs func_80096C0C) | CD interrupts | the read state machine D_8004FE1C, retry reason D_8004FE20, counters D_8005A48C/D_8005A490, sector copies (CdGetSector), stream-ring slots D_8004FE2C; image streams load VRAM strips from the callback (func_8002BB50) | func_80028A60(0) and `while (func_800286CC() ...)` busy-wait without VSync; world-map func_800967E4 |
| MDEC output movie_slice_decoded (mdec.c) | DecDCToutCallback in movie_open, removed by movie_stop | MDEC DMA completion | LoadImage of each slice, StCdInterrupt, the decoder state, then the frame callback: field func_800A7120 (D_800B06A0 frame number, display block D_800ADB78/D_800C426C, isrgb24) or movie func_800768D8 | movie loops; field scripts and the staff roll key off D_800B06A0 |
| DrawSync callback | field func_8007781C (development kits only, D_800C268C == 0), arena func_80088C00 | GPU idle | VSync(1) stamps (profiling) | — |
| card events 0xF4000001 | slot39 func_801D9B08 | BIOS card driver | event state | func_801C881C (TestEvent spin) |

Shared state touched from interrupts is guarded with Enter/ExitCriticalSection
(11 functions). Blocking waits deep in call stacks: VSync and DrawSync (75 and
159 functions); disc waits func_80028A60(0) and the world map's
`while (func_800286CC() >= 3)` spin without VSync; CdInit/CdControlB retry
loops (func_80028230, func_800283D4) and the disc-change checks in slot39
(func_801E92CC, func_801E93A0, VSync(3) between commands); the SPU transfer
spins above; the card TestEvent loop; and the endless error screens
(func_80019EF8, func_8002804C).

## Control flow and state

| Routine | Where and who | Keeps | Discards | Native replacement |
| --- | --- | --- | --- | --- |
| entry func_80019524 (handwritten) | PS-X EXE entry (header.c); soft reset calls it | .data and .sdata as they are | resident BSS 0x800592BC-0x8006FAEC (GameData included) | start-up that clears only BSS-backed state; restarting the process is not equivalent |
| stack reset func_80019548 (handwritten) | entry fallthrough; the dispatcher (call at 80019bd0) | resident globals, kept heap blocks, VRAM, SPU RAM | every stack frame: sp = fp = 0x80200000, gp = _gp | transfer to a host-owned dispatch loop (longjmp or fiber reset) that unwinds no host frames |
| dispatcher func_80019ACC(error) (main.c) | error: GET_RA, then the error screen. Otherwise ResetGraph, clear DrawSync and vblank hooks, VSync(2), restart the heap, clear the mode's BSS (func_80019560), decode its cached overlay to 0x8006FAF0 (func_80032EB4), FlushCache, reset the stack, clear the pad queue, run `entry()`, then call itself | the mode row (in a register across the reset), resident data | the previous mode's stack, BSS, heap blocks not marked keep and its overlay .data (re-decoded on each entry) | top-level loop; overlay .data reset from a pristine copy and BSS cleared on each entry |
| never-returning dispatch sites | boot func_80019578, kernel menu func_8001A4B4, field func_8007954C, battle func_8001B6C4, world map func_80070CFC, arena func_800851D4 (from inside the arena coroutine), menu func_8001C1A8 (debug start), movie func_800737EC (2 sites), heap errors 0x82 (func_80031BDC) and 0x83 (func_800320E8, func_80032C18) | — | the caller's whole stack | a non-local mode exit (12 sites); the heap errors and the disc error indicator (func_8002804C, endless after four retries) are fatal |
| fatal error func_80019EF8 | dispatcher with a nonzero error (the heap's 0x82 and 0x83) | — | — | on retail (D_80010000 = -1) it clears VRAM red and loops forever; the 384x240 report only runs on the PC host |
| soft reset func_80019CA0 -> func_80019CD0 | held 0x90C, checked at 18 call sites in 14 files; field extended event e2 (func_80086D4C), unused by shipped scripts | resident .data/.sdata | stops the CD (func_800283D4), sound (func_80037DC0), SPU, callbacks and pads, then the entry clears BSS | reset that keeps modified initialised data; the sound mode returns to Stereo |
| arena coroutine func_8008BB3C (resume) / func_8008BC04 (yield) (handwritten, menu) | task made by func_8008BA2C on a 0x1000-byte stack at 0x801FE000 with gp from GetGp, resumed once per frame (menu6.c func_80088E90); yields at 7 sites (menu2.c 1, menu5.c 6); D_80096D88/D_80096D8C hold the suspended caller; func_8008BB00/func_8008BB1C (nested schedulers) are unreferenced | the task's registers and stack | — | a fiber nested in the game fiber (Asyncify, JSPI or stack switching in WebAssembly); snapshots only outside the task or at its yields |
| environment-map patcher func_80030988 (handwritten) | rewrites the six `srl` shift fields and six `addiu` offsets of func_80030750 at fixed offsets from D_800308D0; image default (6, 6, 0x40, 0x40); callers arena menu2.c func_800725B0 (5, 4), func_800726B4 and menu5.c func_800852C4 (1, 1), battle func_800A8B0C and ovl2143 func_801E738C (2, 2); no I-cache flush | persists across modes in resident code | — | four resident globals read by the C renderer, kept in snapshots |

Mode table D_8001808C (rows {entry, BSS start, BSS end, loaded}; overlay files
D_8004EAA0 in directory (0, 1)):

| Mode | Entry | Image, first word at 0x8006FAF0 | BSS | File |
| --- | --- | --- | --- | --- |
| 0 kernel menu | 8001a4b4 (resident) | none | — | — |
| 1 field | 80077e88 | field, 4 | 800af5e4-800c426c | 0xE |
| 2 battle | 8001b6c4 (resident), overlay 80070f40 | battle, 6 | 800c3a6c-800d39f0 | 0x10 |
| 3 world map | 80070cfc | worldmap, 5 | 8009bbb0-8009d80c | 0xF |
| 4 Battling arena | 80088e90 | menu, 7 | 800925d0-8009b554 | 0xD |
| 5 in-game menu | 8001c634 (resident) | none (loaded 0) | — | 0x11, not read |
| 6 movie | 800737ec | movie, 8 | 80076f38-80077454 | 0x12 |

The heap runs from the mode's BSS end to 0x801FC000 (main.c func_80019578).
Secondary overlays sit at fixed addresses inside it, each placed by a top
allocation sized to end at the slot ("top - slot - 8"), so placement is heap
state and an image at one address can be any of its tenants:

| Address | Images | Loaded by |
| --- | --- | --- |
| 0x801C5000 | slot39, ovl2598, ovl2600, ovl2601, ovl2602 (directory 0x10, file kind + 5) | field func_800799D4 (reads the file), world map func_800758C0 (decodes its packed copy D_8009D528), resident func_8001C1A8 on the debug start; func_8001C1A8 then calls the tenant's entry: func_801C62A8, func_801CB0A8, func_801CBDBC, func_801CCD28 or func_801CE024 |
| 0x801D3000 | mdec movie library | movie func_800737EC (directory 0x18 file 1); field func_800A7C58 copies directory 4 file 0xA9 there |
| 0x801DC000 | ovl2143 | field func_80077884 (directory 4 file 0x6B9); for the gear shop func_800799D4 reads directory 0x10 file 0xC to 0x1DC000, the slot's KUSEG mirror; resident func_8001C1A8 on the debug start |
| 0x801DE000 | ovl2596 (battle results) | battle func_80070F40 |
| 0x801E0000 | ovl2606 (debug battle selector) | battle func_80070F40 |
| 0x801E4000 | ovl2615 (battle setup) | resident func_8001BBAC |
| 0x801E5000 | ovl3087 (battle event VM) | battle func_80070E2C |
| 0x801FC000 | ovl3381, ovl3383-ovl3387 (battle modules) | battle func_800BEB04, mid-frame when D_800591B3 changes (DrawSync, VSync(0), FlushCache); above the heap |
| 0x80280000 | debug595, debug2611 | field func_80077E88, battle func_80070F40; development kits only (8 MB) |

Resident code calls these entries by address whichever tenant is loaded, so a
native link needs a per-slot registry that rejects a call into an absent image.
State that survives a mode change lives in resident .data/.bss (GameData
0x8006D634-0x8006F98C included), kept heap blocks, VRAM and SPU RAM.

Minigames and optional content: the Battling arena is mode 4's overlay, the
target named `menu` (Disc 1 file 35, docs/scripts/arena-scene.md), while the
in-game menu and saves are mode 5's slot39 with its party, name and shop
screens (ovl2598, ovl2600, ovl2601, ovl2602); the debug content is
debug595 (field monitor), debug2611 (battle state pages and tools), ovl2606
(battle-scene selector) and the resident kernel menu with its Game of Life
screen. No other disc file holds code (`tools/extraction/code_census.py`): the
other minigames run as field event scripts on the field overlay.

## Presentation

### Display and drawing per mode

`SetDefDrawEnv` in this libgpu (80043928) sets tpage 0x0A, dither (dtd) 1,
drawing to the display area (dfe) for heights up to 256, no background clear
and texture window 0; `SetDefDispEnv` leaves the screen window 0 and isinter,
isrgb24 0. The game changes only what the table lists.

| Mode | Draw / display buffers (VRAM) | Screen window | isrgb24 | isinter | Draw environment | Frame | Source |
| --- | --- | --- | --- | --- | --- | --- | --- |
| boot logo | one 320x224 at (0, 0) | default | 0 | 0 | defaults; DrawPrim without an OT | 1 | main.c func_80019D48 |
| kernel menu | 320x224 at y 0 and 240 | default | 0 | 0 | isbg (0, 0, 0x20) | 1 | main.c func_8001A250 |
| fatal error (host only) | 384x240 at y 0 and 240 | default | 0 | 0 | isbg black | 1 | main.c func_80019EF8 |
| disc error | draw 320x256, display 320x240 at (0, 0) | default | 0 | 0 | dtd 0, dfe 1 | — | main_8002709C.c func_8002804C |
| field | 320x224 at y 0 and 256 | (0, 10, 256, 216) | 0, 1 on the shown block while a 24-bit movie plays (func_800A7120) | 0 | no isbg: ClearImage each frame; dtd 0 after map setup when D_800B2078.jump_mode is set (func_80078C5C) | 2-4 | field.c func_80071FB0, field_800854D0.c func_80086D8C |
| field, 640 wide | 640x224 at y 0 and 256 | (0, 10, 256, 216) | 0 | 0 | as field | 2-4 | event fe df operand 0 (func_80086E1C), used once, field map 41 on both discs; the staff roll func_800A7948 also switches to it but needs event BE |
| world map | 320x216 at y 0 and 216 | (0, 10, 256, 216) | 0 | 0 | isbg (0, 0, 0x70), black in mode 2 | 2 | worldmap_80072238.c func_80072BB0 |
| battle (resident preparation) | 320x224 at y 0 and 224 | default | 0 | 0 | isbg (0x3C, 0x78, 0x78) | — | main_8001B6C4.c func_8001B844 |
| battle | 320x224 at y 0 and 224 | (0, 10, 256, 216) | 0 | 0 | isbg from the stage (func_801E7210) | 1 + catch-up | battle_800B8098.c func_800B8284 |
| Battling arena | 640x218 for scenes (menu2.c func_800725B0), 320x218 for its menus (func_800726B4, menu5.c func_800852C4), at y 0 and 256 | (0, 10, 256, 218) | 0 | 0; the interlaced branch for heights above 256 (both buffers at y 0, screen (0, 16, 256, 212)) has no caller | dtd 1, isbg 0, tpage GetTPage(0, 2, 0x280, 0); DR_AREA/DR_OFFSET per layer | 1 or 2 | menu7.c func_80089330, func_80089534 |
| in-game menu | 320x224 at y 0 and 224 | (0, 10, 256, 216) | 0 | 0 | dtd 1, isbg 0; the saved screen is copied into the back buffer every frame | 1 | main_8001B6C4.c func_8001BE14, func_8001BDDC; slot39.c func_801C7BF4 |
| movie | 320x240 at y 0 and 240 | (0, 10, 256, 216) | 1 while a movie plays | 0 | isbg 1 (0 during playback) | 1 | movie.c func_800737EC, func_800763BC; its CD-ROM and sector monitors (debug) use 640x240 |

The screen window values are PsyQ display units; rendering-behavior.md
observed the field, battle and menus showing lines 10-225 of a 320x240 frame.
GTE projection is set per draw, not per mode: field offset (160, 112) with H
from the map (FieldView.projection), the compass H 0x80 at (0x10A, 0xA6); world
map H 0x100 at (160, D_8009BE0C); battle (160, 164) with H 0x200 (the screen
shatter 512, the sky its own); menu (160, 112) H 0x200; arena (width / 2,
height / 2) with H set per screen (0xC0-0x800); 2D sprite sheets through the
GTE with H 0x1000 (sprite_80025C04.c).

### GPU features the game uses

- Ordering tables, reverse-cleared (ClearOTagR): field ot and ot2 of 0x1000
  entries plus an 8-entry overlay table; ot is linked into it only from entry
  D_800B2078.unk21D4 (0x720 by default, set by an event) downwards, a far
  cutoff (field.c func_8007554C, func_80075458). Battle, ovl2606, ovl2615 and
  ovl3387 frames 0x1000; world map and ovl2602's model table 0x400; menus 16;
  the arena per layer (OtPair); movie 32; the disc error screen 8.
- 24-bit links: AddPrim (135 functions), AddPrims, the handwritten OT helpers
  (800315a0-80031894) and model renderers store packet addresses masked with
  0x00FFFFFF; the arena rebuilds pointers from links as
  `(tag & 0xFFFFFF) - 0x80000000` (menu7.c func_8008ACB8).
- Dithering is on by default (above). The DR_MODE/DR_TPAGE packets the game
  builds mostly turn it off for what follows (SetDrawMode at 58 call sites and
  SetDrawTPage at 12 pass dither 0): the arena's screen fades (menu7.c
  func_8008E2B8, func_8008E3CC) and HUD pages (menu5.c) pass 1, ovl2615's
  stage backdrops copy the draw environment's flags. Observed:
  rendering-behavior.md, Dithering.
- Semi-transparency: SetSemiTrans (76 functions) with the rate from GetTPage's
  abr argument (0 at 127 sites, 1 at 29, 2 at 31, 3 once in
  worldmap_80077E68.c, plus variables such as the field fade channels'
  D_800B2078.fades[i].abr) or from data in raw E1 words (battle_800B3F04.c
  effects, sprite render bits 5-6 in sprite_800248D4.c).
- Texture windows: SetDrawMode with a window for field dialogue and text
  (field_8007A44C.c), field overlay sprites (field_800A9274.c) and ovl2615's
  stage backdrops (stage.c); DR_TWIN through SetTexWindow for the world-map
  horizon (worldmap_80072238.c func_800739B8).
- Framebuffer feedback: DR_MOVE packets copy screen regions inside the list
  (SetDrawMove: the field distortion effect func_800A484C, the arena's
  func_80080D20, the world-map heat haze func_80081D80); StoreImage (23
  functions), MoveImage (40), LoadImage (109) and ClearImage (17) read, copy and
  rewrite VRAM, including the saved-screen slot at (0x2C0, 0x100). The CPU sets
  bit 15 (STP) on captured pixels (battle_800B7870.c func_800B7870, field
  func_800A5774), so textures rely on per-texel semi-transparency.
- Mask bit: no game code sends an E6 packet; the observed environments write
  E6 set=0 check=0 (rendering-behavior.md).
- 24-bit display only for MDEC frames (field and movie, above).
- Culling: the resident model renderers (8002e010-8003014c) drop faces outside
  the screen bounds D_800500F8/D_800500FC (func_8002DFF0), back faces (NCLIP) and
  OTZ 0. Their error test (`mfc2 $t0, $31`, model_draw.s) reads GTE data
  register 31, LZCR, not the FLAG control register, so it never rejects:
  near-camera triangles larger than the GPU's 1023x511 limit are submitted
  (observed on fields 22/23), which the GPU does not draw (psx-spx). The
  world-map terrain renderers test FLAG (`cfc2`). A port's GTE must return
  LZCR there, or it culls faces the original submits.

## Sound output modes

- **State**: bits 0x700 of the driver flags D_8005957C, set by
  `func_800386C4(mode)` (main2_800366E0.c): 0 Mono (no bit), 1 Stereo (0x100),
  2 Wide (0x300), 3 (0x500) has no caller. Boot runs func_80037B88(0): flags
  0xB801, then Stereo. The sound option (slot39.c func_801D9808) maps its
  choices 0/1/2 to modes 0/2/1. The mode is not saved, and boot (also after a
  soft reset) restores Stereo.
- **Voices**: the pan law func_8003EBF0 (sound.c) gives every voice the centre
  gain 0x5A00 on both sides in Mono; Stereo and Wide share the two-ramp law
  (0x7F00/0 at the edges, 0x5A00 at the centre). Wide changes no voice value.
- **Signed pairs** (func_80038E6C): both sides take the volume, then
  - 0x300 (Wide): the master pair's **right** side (D_8005A3C0.attr.mvol, SPU
    0x1F801D80/2) and the reverb depth pair's **left** side (D_8005940C, SPU
    0x1F801D84/6) are negated (`0 - volume`);
  - 0x500: the master's left and the reverb's right, the opposite pair;
  - Mono and Stereo leave both pairs positive.

  The master volume is 0x3FFF from boot, so Wide writes (0x3FFF, 0xC001); the
  reverb depth comes from each sequence (func_80038934), 0x2800 at the end of
  the recorded routes, so Wide writes (0xD800, 0x2800). Master fades (func_80038C68,
  stepped on every other tick) and reverb changes re-apply the sign through
  func_80038E6C. The CD input pair (D_8005A3C0.attr.cd.volume, SPU
  0x1F801DB0/2, func_80038D18) is never negated.
- **CD, XA and movie audio stay stereo in every mode.** Boot enables CD input
  without reverb (func_80038DB4(0, 1)) at volume 0x7FFF. func_8003885C would
  program the CD-to-SPU attenuation with libcd CdMix (Mono: all four at half
  volume; stereo modes: same side full, cross 0), but both its callers
  (func_80037B88, func_800386C4) require flag 0x4000, which boot's 0xB801 does
  not set and no store ever ORs in: CdMix is never called. Mono therefore
  changes only the voice pan law; the master and CD pairs and the stereo
  reverb return are as in Stereo. The capture agrees: in the opening movie
  Mono is sample-identical to Stereo on both channels.
- **Mixer order the port must reproduce**: a negative volume inverts phase
  (psx-spx), and in the recordings the main volume applies after CD input and
  the reverb return are summed (Wide right is exactly Stereo right inverted,
  movie audio and reverb included). Wide output is therefore
  L = dry_L + cd_L - rev_L, R = -(dry_R + cd_R) - rev_R. Observed in the
  emulator only, untested on hardware.
- **Evidence** (local, Disc 1, PCSX-ReARMed with HLE BIOS): the routes
  `p1-original-sound-{stereo,wide,mono}-20261002` (tests/reference-inputs/
  new-game-sound-mode-*.json, 36,000 frames each) are identical up to sample
  1,471,335 (the first Sound-screen press) and end with flags b901, bb01 and
  b801, Wide master 3fff/c001 and reverb d800/2800. The comparison
  `.local/scenarios/p1-original-audio-modes-20261002.json` came from

  ```sh
  nix --extra-experimental-features 'nix-command flakes' develop path:./nix#observation -c \
    python3 tools/analysis/audio_modes.py \
    --stereo .local/scenarios/p1-original-sound-stereo-20261002/audio.wav \
    --wide .local/scenarios/p1-original-sound-wide-20261002/audio.wav \
    --mono .local/scenarios/p1-original-sound-mono-20261002/audio.wav \
    --window opening-movie:4200:4800 --window post-movie:22200:22800 \
    --delay-seconds 0.01 --max-lag-ms 0 \
    --output .local/scenarios/p1-original-audio-modes-20261002.json
  ```

  Opening movie (frames 4200-4800): Wide left equals Stereo left exactly, Wide
  right is Stereo's inverted within 3 LSB, Mono equals Stereo exactly (max
  residual 0). Post-movie field (22200-22800): Wide right inverted within 2
  LSB, Wide left differs by a flipped, uncorrelated component (the reverb
  return; RMS 1490 against 1994 kept), Mono L/R correlation 0.68 (centred
  voices, stereo reverb). sound-modes.md records the earlier p1cov recordings
  and the per-mode arithmetic in full.
