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
| libgpu | DrawSync (159), AddPrim (135), GetTPage (114), LoadImage (109), SetSemiTrans (76), GetClut (69), SetShadeTex (59), MoveImage (40), PutDispEnv/PutDrawEnv (38/37), SetDrawMode (30), DrawOTag (27), ClearOTagR (25), StoreImage (23), ClearImage (17), OpenTIM/ReadTIM (17), SetDefDrawEnv/SetDefDispEnv (15), SetDispMask (13), SetDrawTPage (13), ResetGraph (8), DrawSyncCallback (5), AddPrims (3), TermPrim (3: kernel menu func_8001A4B4, the debug console's flush func_80037324, arena func_80088E90), GetDrawEnv (2: the resident panorama func_8002709C, ovl2615's stage backdrop func_801E7914), MargePrim (arena HUD func_80085EC8), LoadClut2 (arena palette func_8007B270), SetGraphDebug(0) (boot func_80019578), SetDrawMove, SetDrawArea, SetDrawOffset, SetDrawEnv, SetTexWindow, DrawOTagEnv, DrawPrim, and the primitive setters SetPolyFT4 (37), SetPolyG4 (21), SetPolyF4 (13), SetLineF3, SetLineF2, SetSprt, SetPolyFT3, SetPolyGT3, SetPolyGT4, SetPolyF3, SetTile, SetLineG2, SetPolyG3 |
| libgte | SetRotMatrix (94), SetTransMatrix (92), SquareRoot0 (61), ratan2 (56), RotTransPers4 (51), CompMatrix (47), VectorNormal (36), ScaleMatrix/TransMatrix (29 each), SetGeomScreen (24), ApplyMatrix (22), MulMatrix0 (20), RotTransPers (19), SetGeomOffset (19), Push/PopMatrix (15), ReadGeomScreen (12), MulMatrix2, ReadGeomOffset, RotAverage4, RotTransPers3, ApplyMatrixLV/ApplyMatrixSV, SetBackColor, SetColorMatrix, SetLightMatrix, OuterProduct0, VectorNormalS/VectorNormalSS, InitGeom, RotTransSV, RotMatrixZ, ScaleMatrixL, SetMulMatrix, lighting (NormalColor, NormalColor3, NormalColorCol, NormalColorCol3), SetFogNearFar, and 16 entries without a signature name (below) |
| libcd | CdSyncCallback (17), CdIntToPos (16), CdControlF (14), CdReadyCallback (14), CdDataCallback (10), CdControlB (8), CdPosToInt, CdGetSector, CdInit, CdControl and CdSetDebug (func_80028230 only: Standby and debug level 0 at start-up), CdSync, CdDataSync, CdFlush, CdReadCallback, CdMix (dormant, see Sound); mdec: CdRead2 and the St* streaming ring (Disc and files) |
| libapi | events (Open/Close/Enable/Disable/Test/UnDeliverEvent; Enable/DisableEvent 18/17, the sound driver's tick guard, Interrupt-context work), root counters (Set/Get/Start/StopRCnt), critical sections (11 each), SwEnterCriticalSection/SwExitCriticalSection (the soft reset func_80019CD0), FlushCache, InitPAD/StartPAD/StopPAD/ChangeClearPAD, BIOS file calls open B(32h), read B(34h), write B(35h), close B(36h) and B(41h)-B(45h) (Memory card and saves), Krom2RawAdd B(51h), A(ABh) card check, GetGp (reported as ChangeClearPAD+0x10, which holds it) |
| libetc | VSync (75), VSyncCallback (3), ResetCallback, SetVideoMode |
| libspu | SpuInit/SpuQuit, SpuInitMalloc, SpuSetCommonAttr, SpuSetReverb and the reverb mode setters, SpuGetReverbModeType, SpuSetIRQ/SpuSetIRQCallback, SpuSetTransferMode/StartAddr/Callback, SpuReadDecodedData, SpuGetVoiceEnvelopeAttr, SpuSetNoiseClock; SpuRead/SpuWrite are func_8004D818/func_8004D878 by inspection (unattributed, called by sound.c func_8003BE68) |
| libcard | InitCARD, StartCARD, `_bu_init` (A(70h)) |
| libpress, libds | DecDCTReset/DecDCTin/DecDCTout/DecDCTvlc/DecDCTvlcSize and DecDCToutCallback, all from mdec |
| libc, libc2 | bzero (126), rand (96), memcpy (33), memmove (16), sprintf (11), strcat, strcpy, strlen, memset, memchr, strcmp |
| libsn | PCopen, PCcreat, PClseek, PCclose and func_8004C38C/func_8004C398/func_8004C470 (PCinit/PCread/PCwrite by inspection): development-host paths only (D_8004FE48 set, or `D_80010000 != -1`) |

The rows other than libcd name every entry the scan reports at this revision;
its `--detail` output lists each entry's caller functions.

BIOS stubs are named by table and number; splat merged consecutive stubs into
one symbol (`close` also holds B(41h)-B(45h), SpuGetReverbModeType ends with
A(ABh)). The slot39 headers name them by use: format, firstfile, nextfile,
rename, erase (slot39/menu.h). func_80040C3C (unattributed) is the pad send
registration: it calls `_SendPAD` with its four arguments (Controllers).

The libgte entries without a signature name, by their code:

| Entry | What it computes |
| --- | --- |
| func_80048E94 | an orthonormal matrix from a matrix's first two rows: two outer products (OP), each row normalised by 80048DD8 (MatrixNormal form) |
| func_8004901C | weighted sum of two SVECTORs, GPF then GPL (LoadAverageShort12, menu.h) |
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
| func_8004A92C, func_8004ABBC | rotation matrix from three angles through the sin/cos table D_800523F0 (RotMatrixYXZ and RotMatrix in battle/psyq.h) |
| func_8004AE4C, func_8004AFEC | single-axis rotation matrix (RotMatrixX, RotMatrixY in worldmap.h) |

Outside the libraries the game itself uses:

- **SPU registers** through the pointer `D_800508E4` (0x1F801C00, sound.h),
  loaded by 15 functions of sound.c and main2_800366E0.c (the voice-register
  writers). No compiled game function addresses the I/O window directly.
- **GTE** inline in 78 compiled functions (the PsyQ GTE macros) and in the
  handwritten renderers (resident model renderers, menu and world-map batches).
- **Scratchpad** (0x1F800000-0x1F8003FF) as work memory in 98 functions, 80 of
  them in the world map, and as a temporary stack. The scan above does not
  count the stack use, because it does not follow the `move` in the stack
  switch, `move $8, top; sw $29, 0($8); addiu $8, $8, -4; move $29, $8`
  (undone by `addiu $29, $29, 4; lw $29, 0($29)`). Matching that sequence in
  the linked images finds 23 switches:
  - Eight go to the scratchpad top 0x1F8003FC, in seven functions. These are
    field func_800739C0 (twice: the camera update func_80073230 and the view
    composition func_800722F4) and func_8007554C (the frame); battle
    func_800BE790 (the frame), func_800A4654 (the stage update) and
    func_800B7870 (the intro swirl); and ovl2615's shatter and burst load
    modes func_801E8588 and func_801E91E8. In the C they are the
    `SPAD_STACK_ENTER()` and `"r"(0x1F8003FC)` sites.
  - The other 15 go to the top of a heap block: the 11
    `STACK_ENTER(stack + size)` sites of the resident sprite code (3) and
    battle (8), and the private stacks of debug2611 func_80281FD8, ovl2615
    func_801E6DC8/func_801E6FEC and ovl3387 func_801FC898.

  The arena coroutine switches stacks its own way (Control flow).
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

### Disc and files

The resident disc unit (main_8002709C.c, 80028230-8002c3e8, API in
`resident/cd.h`) reads every file. Outside it, three shipped parts issue drive
commands themselves: the world-map terrain reader, the movie library (mdec) and
the disc check of the swap (and the movie overlay's development tools). Each of
them, like the unit, branches on func_8002C3D8 (D_8004FE48) to a host-file path,
so the original development build replaced the drive at these four places (PC
file server, below). A port can take the same cut: serve the unit's reads,
rings and image streams, the world map's request lists, the movie ring and the
swap's disc check from the imported discs. Raw sector reads are part of it,
which the host path does not serve (func_8002954C returns -1 there): the
arena's portraits and the swap's label and index. The other libcd calls are the
busy test's CdDataSync(1) (func_800286CC), CdDataSync(0) before a field movie
(field func_800A7394), a per-frame CdSync(1) whose status no code reads (world
map func_800712D0), CdFlush at soft reset, the card screens' callback save and
restore (Memory card and saves) and the dormant CdMix.

- **Index.** Each resident image carries its disc's index ahead of its code,
  the only bytes in which SLUS_006.64 and SLUS_006.69 differ (three rodatabins,
  [matching.md](matching.md#recovering-data)): the boot word D_80010000 (-1 in
  both), 0x8000 bytes of 7-byte file records (D_8004FDF0: a 24-bit first sector,
  then a signed 32-bit size in bytes; a negative size marks a directory record
  whose magnitude counts its files, func_80028928) and the u16 directory table
  (D_8004FDF4: each directory's first record, 1-based; word 0x3C is the disc
  number). Boot passes the three to func_80028230 (main.c func_80019578): -1
  keeps the embedded copies, 0 re-reads them from sectors 24 (0x8000 bytes) and
  40 (0x7A bytes), any other value selects the PC file server.
- **Addressing.** func_80028470(g, i) selects the base D_8004FE14 =
  table[g + i] - 1 (g a multiple of four, func_800284B4; a zero entry returns -1
  and selects 0, but the tables' unused entries hold 0xFFFF, which it does not
  catch). File f of the selection is record f + base - 1 (func_800289D0 its
  first sector, func_80028738 its size, func_800288EC the size rounded up to
  words), that is slot f + table[g + i] - 2, the rule
  [matching.md](matching.md#script-instructions) gives and the slot numbering of
  `tools/extraction/disc_files.py`. It reproduces the field pairs of
  [EVID-REF-007](../analysis/findings/EVID-REF-007.json) (files 0xB8/0xB9 + 2m
  of entry 4, which holds 424 on Disc 1 and 419 on Disc 2: slots 606/607 + 2m
  and 601/602 + 2m) and the mode overlays' slots (files 0xD-0x12 of (0, 1),
  whose entry holds 24 on Disc 1 and 19 on Disc 2: slots 35-40 and 30-35, the
  same files).
  [EVID-REF-005](../analysis/findings/EVID-REF-005.json) measured the record
  format at LBA 24 and the label at LBA 23 on both discs and left their loader
  use open; these functions are that use. A read resolves its later files in
  the directory selected when it started (D_8004FE18: func_80028808,
  func_80028A18), so callers reselect right after starting one (field
  func_80085B20); func_800284B4 returns the selection's (g, i), which battle,
  mdec, main.c func_800199CC and func_800379D8 restore after reading elsewhere.
- **Reads.** Each read entry spins until the drive is idle (func_80028A60(0)),
  sets the busy count D_8004FDFC, issues Setloc and returns; the CD callbacks
  (Interrupt-context work) finish it. func_800286CC returns that count (for a
  list, the files not yet finished, which func_8002BA40 copies from D_8004FE00;
  otherwise 1), or 1 while a command (D_8004FE1C) or a sector transfer
  (CdDataSync(1)) is pending. The world map's entry (func_80072238) and scene
  setups go on once a list has fewer than three files left
  (`while (func_800286CC() >= 3)` at eleven sites; `>= 2` in func_800758C0),
  so a port must report the count, not only busy. A finished or stopped read
  seeks to the file given as its `after` argument (D_8004FE38, func_8002A394)
  or, for 0, pauses; every shipped caller passes 0 (only the movie overlay's
  development tools pass 1).

  | Entry | Reads | Sector callbacks |
  | --- | --- | --- |
  | func_800295D8(file, dest, after, flags), func_80029690 | one file of the selection; flags 0x100: into a stream ring (0x200: the host movie stream; on the disc it issues nothing) | func_8002B084 copies; for a ring, func_8002B2F0 puts each sector in a free slot numbered in arrival order and the data callback func_8002BA58 completes the slots in order |
  | func_8002954C(sector, dest, size) | raw sectors: the swap's label and index, the arena's portraits (two sectors each inside file 6, menu4.c func_80080644), boot word 0's index, the movie tools | func_8002B084 |
  | func_80029AFC(list, after) | a zero-terminated (file, destination) list, sorted by file and read in one pass: a next file at most D_8004FDE0 = 16 sectors ahead is read through (the sectors between are not copied), a farther one gets Pause and Setloc | func_8002AC24 |
  | func_80029EB0(file, ring, after, 0, placement) | an image stream into VRAM (Streams) | func_8002B5D0 (a copy of func_8002B2F0); the data callback func_8002BB50 loads the strips |
  | func_8002A2D0, func_8002A394(file) | a seek (Setloc, SeekL) to a file, or Pause for file <= 0; func_8002A2D0 only when idle, once before a field movie (field func_800A7C58) | — |
  | func_8002A428(mode), func_8002A498(after) | Setmode, then Pause; a stop request that the next sector callback carries out | — |

- **Commands**, numbered as libcd's own name table (D_800564D0) names them.
  CdControlF returns at once and completion reaches the sync callback;
  CdControlB waits for completion (it calls CD_sync), and its callers mostly
  repeat it until it succeeds.

  | Command | Issued by |
  | --- | --- |
  | Setloc (2) | every resident read and seek (func_80029690, func_80029AFC, func_80029EB0, func_8002A2D0, func_8002A394), retries and list gaps (func_8002A68C, func_8002AC24); the world-map reader (func_8009699C, func_80096A6C, func_80096C0C); movie_restart; the swap (sector 0) |
  | ReadN (6) | func_8002A68C after Setloc: every resident read |
  | ReadS (0x1B) | the world-map reader (func_80096A6C); CdRead2 for movies (movie_restart) |
  | SeekL (0x15) | func_8002A68C after a seek's Setloc; the swap |
  | Pause (9) | the end of every shipped resident read, func_8002A428 after Setmode, retries; soft reset (func_800283D4); the world-map reader at a list's end and in recovery; movie_stop |
  | Stop (8) | retry reason 4; the swap's preparation (func_801E92CC) |
  | Standby (7) | func_80028230 through CdControl, after CdInit (repeated until it succeeds) and CdSetDebug(0) |
  | Setmode (0x0E) | func_8002A428: 0xA0 at start-up, soft reset, movie_stop and before the swap's label read, 0 in the swap's preparation; retry reason 6; CdRead2 with the movie mode |
  | Setfilter (0x0D) | movie_start: file 1 and the movie's channel, when its select bit 0 is set |
  | Nop (1, Getstat) | retries (func_8002A68C, func_80096A6C, func_80096C0C); the swap's lid and motor polling |
  | GetTN (0x13) | retries, once the drive status shows the lid closed; the swap |

- **Modes and sectors** (Setmode bits after psx-spx: 0x80 double speed, 0x40
  XA-ADPCM to the SPU, 0x20 whole 0x924-byte sectors, 0x08 XA filter).
  Resident and world-map reads run in 0xA0: each sector callback copies the
  4-byte header and 8-byte subheader (CdGetSector(..., 3)) and the 2048 data
  bytes (the rest of a short last sector goes to D_800596F8, the world map's to
  D_8009D7D4) and checks the header's position against the expected sector
  (CdPosToInt): on a mismatch, counted in D_8004FDE4/D_8004FDE8/D_8004FDEC (the
  world map's in D_8009CCA0), the sector is not taken and the read restarts
  there (Retries). Movies read 2048-byte sectors in the low byte of
  movie_cd_mode | 0x80: 0xC8 with XA audio, 0x88 when select bit 1 clears the
  ADPCM bit again (field movies with their own sound bank, field
  func_800A7218), 0x80 without. CdRead2 issues that Setmode, installs the St
  ring's callbacks (StCdInterrupt2, data_ready_callback) for bit 0x100 and
  sends ReadS (mdec CdRead2). Setmode 0 is "NORMAL SPEED" in the development
  test (movie.c func_80072480).
- **Retries.** func_8002A68C (state D_8004FE1C, reason D_8004FE20): a command
  that does not complete (status other than Complete, 2), or a sector that
  fails, arrives out of place or finds no free ring slot, makes it poll Getstat
  until a status completes with the lid closed (bit 0x10 clear) and send GetTN,
  then by reason: 1 Setloc and SeekL again, 2 Pause again, 3 Pause, then Setloc
  and ReadN from the failed sector, 4 Stop and then as 3, 6 Setmode and Pause
  again. Every third failed sector of a read (D_8005A4DC) takes reason 4 after
  an empty 10,000 x 2,000 loop inside the callback, which an optimising native
  compiler may delete. Nothing limits the retries or reports an error to the
  caller: an unreadable disc leaves func_80028A60(0) spinning, without a message
  (func_8002804C's bars run only on the PC server's paths). States 12 and 8
  (Setmode, then Setfilter with file 1 and the low byte of D_8004FE38) are
  entered only through reason 5, which only they set, so the resident never
  selects an XA channel. The read statistics D_8005A488-D_8005A4B4 feed only
  the movie overlay's development monitor (func_800704E8). The world-map reader
  recovers the same way under its own state D_8009CD44 (Getstat while the lid
  is open, GetTN, Pause, Setloc and ReadS), while the world-map loop spins
  VSync(0) (worldmap.c func_800712D0).
- **Streams.**
  - Music: the field streams a music's wave file (0x13 + 2 * wave of directory
    (0x1C, 0), field func_80085B20) through an eight-slot ring
    (func_80085560, flags 0x100). The field's post-frame step (func_80078B5C,
    func_80085C90, func_80085C3C) takes up to five sectors a frame in order
    (func_80028B14); func_800859DC gathers the first four into a 0x2000-byte
    wave bank (func_800380D0) and hands later ones to the SPU (func_8003827C)
    after the previous transfer (func_8003BDFC(0x10)). A full ring makes the
    drive read the sector again (Retries). Sequences (0x14 + 2 * music) and the
    battle music (func_800379D8, a two-file list of directory (12, 3)) are
    plain reads.
  - Image streams (func_80029EB0): the field map's (file 0xB9 + 2 * map of
    entry 4, four slots, field.c func_80070488, waited for by func_80070508)
    and a battle action's (file 0x23 + 2 * index of (0xC, 2), eight slots,
    battle_800B7134.c func_800B7C34). Each image's first sector holds type
    0x1200 or 0x1201, an origin and an offset (placed by the caller's mode and
    base for each type, D_80059F24-D_80059F38; both shipped callers pass 0,
    origin plus offset), the strip width, the image count, the strip count and
    the strip heights. Each following sector is one strip, loaded with
    LoadImage by the data callback func_8002BB50 at interrupt time.
  - World-map terrain: the world map queues (sector, bytes, destination)
    requests per frame (func_8009623C: at most 0x58 a list, 16 lists;
    func_80096328 sorts a list by sector). Its reader (func_8009699C,
    func_80096A6C, func_80096C0C) reads a list with one Setloc and ReadS,
    reading through gaps under 0x13 sectors, seeking past larger ones and
    pausing at the end. Terrain blocks are 0x710 bytes, one per sector: rows
    from file D_8009BCD8 (the area's file + 9) in block order, columns from
    file D_8009BD08 (+ 10) in column-major order (func_80097DC0,
    func_80098CC0). func_80096130 and func_80096694 wait with VSync(0) for list
    space and for the queue to drain.
  - Movies (mdec, Services): movie_open makes a ring of 2048-byte sectors
    (StSetRing); movie_start sets the XA filter and StSetStream; movie_restart
    issues Setloc (the file's first sector plus the start sector, or
    StGetBackloc's position after 0x871 polls without a frame, movie_poll) and
    CdRead2; frames come through StGetNext and StFreeRing
    (movie_next_bitstream, movie_decode); movie_stop unsets the ring, pauses
    and sets 0xA0 again. The field plays file + 2 of directory (0x18, 1)
    (func_800A7218), the movie mode a request's entry + 2 of (0x18, 1) or + 3
    of (0x18, 0) (movie.c func_80076488).
  - XA: only movie_start selects a channel: file 1 and the channel, 1 at every
    shipped caller (field func_800A7218; movie.c func_800737EC for a request).
    With bit 0x40 the drive plays the matching sectors into the SPU's CD input
    (Sound output modes).

  The disc path overlaps reads with frames: battle runs battle frames until the
  disc is idle (func_800B8354), the field runs field frames before a movie
  (func_800A7394), the world map steps its reader on VSync(0) and each frame,
  and the field's frame feeds the music ring. How many frames a load spans is
  the read latency, which nothing bounds. The host path finishes plain and list
  reads and image streams inside the call.
- **Disc identity and the swap.** func_80028530 returns directory word 0x3C, 1
  in Disc 1's table and 2 in Disc 2's. Its readers: boot's opening-movie
  request (main.c func_80019578: D_8004FE45 = 0x10 on Disc 1, else 7, played as
  file 0x12 or 9 of directory (0x18, 1), movie.c func_80076488), the title file
  screen's exit after 600 idle frames, on Disc 1 only (slot39.c func_801C58EC),
  the swap (func_801C8694), the save (func_801CBA4C: D_8006F008 = disc - 1, or
  1 for the save offered at the change), field event `cd` (store_disc_number,
  func_800A0DFC) and the movie overlay's development screens.

  The swap runs in the in-game menu (mode 5). Menu kind 6 (field event `da`,
  slot39.c func_801C57A4) offers a save and then asks for Disc 2
  (func_801C8694(1)); after the title file screen (kind 2, func_801C62A8) the
  menu asks for Disc 1 or, after a load, for the file's disc D_8006F008
  ([EVID-REF-003](../analysis/findings/EVID-REF-003.json) saw Disc 2's New Game
  ask for Disc 1). func_801C8694(d) repeats while the reported disc is not
  d + 1. func_801E92CC stops the read, sets Setmode 0 and retries Stop every
  VSync(3); the change notice shows; func_801E93A0(d + 1) polls Getstat every
  VSync(3) until the lid opens (bit 0x10), until it closes and until the motor
  runs (bit 0x02) with the command completing, then sends GetTN, Setloc to
  sector 0 and SeekL. A SeekL failing with the error bit 0x01 and error-code bit
  0x40 returns 2; another failure retries from GetTN. The development test
  (movie.c func_80072480 over func_80072A08) labels these steps "CD STOPED",
  "CD OPENED", "CD CLOSED", "SPINDLE OK", "TOC OK" and "SET LOCATION OK", and
  that failure "IT IS NOT PLAY STATION DISC". Then come Setmode 0xA0 and 16
  bytes of sector 0x17: bytes 4-7 must read "_XEN" (else 2) and byte 3 the
  digit of the wanted disc (else 3); a nonzero result shows message 0x89 for 29
  frames and asks again. On a match the index (sector 0x18, 0x8000 bytes) and
  the directory table (sector 0x28, 0x7A bytes) are re-read over the copies
  boot passed (0x80010004, 0x80018004), so func_80028530 reports the new disc;
  they lie in the loaded image, which a soft reset keeps (Control flow).
  EVID-REF-005 found "DS01_XENOGEARS" and "DS02_XENOGEARS" at the start of LBA
  23. Both discs run the same code and address files only through these tables,
  so for a swap a port's drive presents the lid opening and closing, a running
  motor, successful GetTN, Setloc and SeekL, the label at sector 23 and the
  other disc's sectors 24 and 40.
- **PC file server, development only.** A boot word other than 0 and -1 selects
  it: func_80028230 calls func_8004C38C (PCinit by inspection) and keeps the
  word as D_8004FE48, a table of 64-byte host file names, one per index record
  (func_80028998), which func_8002C3D8 returns. Both retail images hold -1, so
  none of this runs on retail. Plain and list reads then open, read and close
  host files with four tries per call, drawing func_8002804C's coloured bars
  and, after the fourth failure, its endless text screen; sizes come from
  PClseek; ring streams read a sector per step (func_80028B14; func_80028F30
  reads 0x920-byte records with their subheaders when the mode byte has 0x08,
  skipping file-1 sectors); func_80029EB0 pumps a whole image stream before it
  returns; the world map reads (path, offset) lists (func_800962B0,
  func_800966CC); the movie library uses the resident ring
  (movie_host_stream); and the swap loads `c:\work\cdrom.mdg` (the index,
  0x8000 bytes), `cdrom.fid` (the directory table, 0x7A) and `cdrom.fnd` (the
  names, 0x40000), or `cdrom2.*` for Disc 2 (slot39 func_801E93A0, movie
  func_80072A08). libsn's entries are in the Services table.

## Timing

### VSync and the clocks the game reads

libetc VSync (8004b54c): VSync(0) waits for the next vertical blank; VSync(n),
n > 1, returns once n blanks have passed since the previous wait returned (at
least one new blank); VSync(1) returns the horizontal blanks since the last
wait; VSync(-1) returns libetc's blank counter (D_80058960). The game calls it
from 75 functions: VSync(0) 89 sites, VSync(2) 15, VSync(3) 13, VSync(1) 13,
VSync(-1) 4, and VSync(8), VSync(D_80092898), VSync(D_80059198 + 1).

- VSync(-1) is read at exactly four sites: the start and end of the field
  frame (field.c func_8007554C), which only pace it, and of the battle frame
  (battle_800BD3AC.c func_800BE790), whose measurement battle logic consumes
  (Pacing per mode).
- VSync(1) feeds only profiling values (field D_800ADB9C/D_800ADBA0/D_800ADBA4
  read by debug595, battle D_800D309C.drawn/synced, arena D_800927F0 and the
  movie monitor's D_800773B8).
- GetRCnt: the sound tick times itself on root counter 2 (D_800595C4,
  D_80059540; profiling); the arena compacts its ordering table while root
  counter 1 (horizontal blanks) stays within a budget (menu7.c func_8008AC8C,
  func_8008ACB8), which changes which empty tags are skipped, not the image.
- D_80059488, the game's own blank count (vblank handler), is the play time
  stored in saves; it also drives input repeat, arena colour cycles and the
  arena's vblank hook. Every pause and missing-pad loop (Pacing per mode)
  saves and restores it, so paused time is not counted. These are field
  func_80077E88, world map func_8007634C and func_80076594, the battle input
  readers func_80089CCC, func_8008A3EC and func_8008A684 (battle.c), and the
  menu input readers slot39 func_801C7D78, ovl2598 func_801C92AC, ovl2600
  func_801C98E8, ovl2601 func_801CACC8 and ovl2602 func_801CB4E4.
- The vblank handler's h:m:s clock (main2.c func_80035E44) counts blanks in
  D_80059370 (60 to the second), then seconds in D_80059418, minutes in
  D_80059420 and hours in D_80059484; D_800501F8 stops it at 100 h. It lives in
  BSS, so it starts at zero at boot and after a soft reset. It counts through
  pauses, because only D_80059488 is restored, and saves do not store it (the
  payload keeps D_80059488). Scripts can read it: in each field frame
  (func_80077DAC) func_800A31E8 copies it into event variables 0xC (seconds |
  minutes << 8) and 0xE (hours) (field_800854D0.c:11067-11068). The kernel
  menu also prints it (main.c func_8001A344). Decoding all 935 maps
  of both discs with `tools/analysis/events.py` (walking from each entry) finds
  no instruction that names variables 0xC-0xF as a variable or bit operand.
  Shipped scripts are therefore not known to read the clock, though an indirect
  read is not ruled out.
- Video is NTSC only: SetVideoMode(0) at boot (main.c func_80019578).

### Pacing per mode

| Mode (entry) | Frame wait | Blanks per frame | Notes |
| --- | --- | --- | --- |
| 0 kernel menu (resident func_8001A4B4) | DrawSync, VSync(0) | 1 | development start screen |
| boot logo (func_80019D48) | VSync(0) per step | 1 | 16 fade-in steps, 110 held, 17 fade-out |
| 1 field (func_80077E88, frame func_8007554C) | start = VSync(-1); DrawSync and VSync(0) mid-frame; at the end busy-waits until VSync(-1) >= start + D_800B2078.unk217C + 2 | 2, 3 or 4 | unk217C comes from field event d6 (func_800925A0, operand 0/1/2, which also sets text speed 8/6/4); scripts use all three values on both discs. Sprite scripts tick D_80059198 + 1 = 2 times per update (field.c func_80071FB0) |
| 1 field pause (func_80077E88) | DrawSync, VSync(2) | 2 | Two loops, each also running the input drain func_80074700 and the soft-reset check. One runs while the first pad is missing (field.c:2997-3008). The other is a toggle on Start (field.c:3010-3021): it starts when the field's repeat word D_800C3900 (the queued D_800594A4, masked) holds Start 0x800, unless held bit 0x40 or D_800B2078.unk2358 is set, and ends when D_800C3900 holds Start again. That is the next press, or the auto-repeat once Start has been held for 32 blanks (Controllers). Both silence the voices and suspend the sound tick (func_80037EE4, until func_80037E8C) and restore D_80059488 |
| 1 field movie (func_800A7C58) | mode D_800ADB74 0: VSync(0) before each decode batch (func_800A732C); 1: field overlays and VSync(0) (func_80075910); 2: the full field frame | 1, or the field's | movie progress D_800B06A0 comes from the MDEC callback |
| 2 battle (resident func_8001B6C4, overlay func_80070F40, frame func_800BE790) | start = VSync(-1); after DrawSync D_80059494 = VSync(-1) - start - D_80059198, clamped 0-4; then VSync(D_80059198 + 1), or VSync(0) when D_80059198 is 0 | 1, plus catch-up | the next frame consumes the measured blanks (below). func_800B88C4 leaves D_80059198 = 0. Turn logic calls frames from inside rules (func_800716D8, nesting counted by D_800C37D0). Development: holding Select gives VSync(8) and drops the catch-up (func_800BEBC4, D_80010000 != -1) |
| 2 battle pause and missing pad (battle.c func_80089CCC, func_8008A3EC, func_8008A684) | none: spins | — | The battle step's input readers spin, with no VSync or DrawSync, while the first pad is missing. A Start press (the pressed word; in func_80089CCC and func_8008A3EC only while D_800CCC58 is set) toggles D_800C3444. While it is set the reader keeps draining the input queue, which only the vblank handler fills, until the next press. No frame is presented meanwhile. Voices and tick are suspended and D_80059488 is restored, as in the field |
| 3 world map (func_80070CFC, loop func_800712D0) | DrawSync, VSync(2) | 2 | first spins VSync(0) while the terrain stream reports 3 (func_800967E4); sprite scripts tick twice per update (D_80059198 = 1, func_80072238) |
| 3 world-map pause (worldmap_80072238.c func_8007634C, func_80076594) | DrawSync, VSync(0) | 1 | A Start press (the pressed word D_8009BD10 & 0x800) enters func_8007634C until the next press, and a missing first pad enters func_80076594 until one answers (worldmap.c func_800712D0). Both draw on the other buffer, suspend voices and tick, and restore D_80059488 |
| 4 Battling arena (`menu` target, func_80088E90) | VSync(D_80092898) | 1 or 2 | the arena task sets D_80092898 to 0 or 2 (menu5.c func_800852C4); the arena coroutine runs once per frame (Control flow) |
| 5 in-game menu (resident func_8001C634; frames func_8001C074, slot39 func_801C7BF4) | DrawSync, VSync(0) | 1 | also the ovl2598/2600/2601/2602 screens; card and disc-change waits inside |
| 5 menu missing pad (slot39 func_801C7D78; ovl2598 func_801C92AC, ovl2600 func_801C98E8, ovl2601 func_801CACC8, ovl2602 func_801CB4E4) | none: spins | — | each screen's input reader spins on func_80035734(0), with no VSync, until the first pad answers; voices and tick suspended, D_80059488 restored |
| 6 movie (func_800737EC, player func_80076488) | VSync(0) per pass, three decode steps per frame (func_801D3F7C) | 1 | mdec's DecDCTout callback delivers frames |
| battle screen effects (ovl2615 load/burst modes, ovl3387) | DrawSync, VSync(2) | 2 | |

Battle alone measures its frames and scales logic by the result. The field's
VSync(-1) values only pace its frame (field.c:2084-2160), and the work driven by
blanks rather than frames follows D_80059488 (above). After DrawSync the battle
frame func_800BE790 stores D_80059494, the blanks since the frame's start less
D_80059198, clamped to 0-4. It also stores frameTicks D_800CCC5C
(BattleArea.frameTicks) = D_80059494 + D_80059198 (battle_800BD3AC.c:675-682).
The next frame consumes them:

- Catch-up in the frame (battle_800BD3AC.c:661-671). The camera step
  func_800BBAB8 and the main task list func_8001C964 run D_80059494 more times,
  and so does the battle step func_8008A9C0(1). func_8008A9C0 dispatches on
  D_800C3E4C, and in state 1 its func_8008A274 runs the ATB tick func_8007171C
  (while D_800CCC58 is set). The ATB therefore advances once per blank of the
  previous frame, up to five times a frame. func_8001C964 zeroes D_80059494
  when its pause count D_80059428 runs out.
- Stage steps (battle_8009E53C.c func_800A9A50, 4057-4065). D_800C3E88 += 1 +
  frameTicks, capped at 6, yields up to three steps of two blanks, and the
  remainder carries over. The steps advance:
  - the wave phase (D_800C3CF0 += 56 each);
  - each object's pose and animation (func_800AAA20, which runs func_800A0838
    and func_800AE2A4 once per step);
  - the camera channels (func_800B026C, while D_800C3DF8 is set);
  - the sprite pool's fades (func_800A2FD8);
  - the timers and wait-frames counts of the objects' effect scripts
    (func_800AAD54, run from func_800AAA20). These scripts run only in an
    update with at least one step.
- frameTicks itself. Those effect scripts integrate the object's motion
  frameTicks + 1 times (func_800AAD54). Image animations advance by speed x
  (frameTicks + 1) (func_800A3E98), both for every drawn object (func_8009F844)
  and for the stage object (func_800A4654, from func_800BB9D4 each frame).
- Not scaled. The highlight pulse (D_800C3B7C += 0x80), the push-apart of the
  acting object (func_800B10EC) and the placement of child objects
  (func_800AAB34) run once per stage update.
- The first frame's frameTicks is max(0, n / 2 - 1), where n counts the gear
  enemies present: slots 3-10 that hold a gear and whose field2 is below 0x11
  (battle_800B8098.c func_800B88C4).

The sound driver runs beside this on its own timer: 240 ticks per second, about
4.01 per NTSC vertical blank, so the ticks per frame are not a whole number.

### Interrupt-context work

| Callback | Installed by | Runs | Writes | Waits that depend on it |
| --- | --- | --- | --- | --- |
| vblank handler func_8003634C (main2.c) | VSyncCallback at boot (func_80019578) and on world-map entry (func_80070CFC); cleared by soft reset | every vertical blank | D_80059488++; pad words D_80059570/D_80059574, D_8005948C/D_80059490, D_800594A4/D_800594A8 and the sticks; the input ring D_8005A0FC-D_8005A19C; the h:m:s clock D_80059370/D_80059418/D_80059420/D_80059484 (stops at 100 h, D_800501F8); actuators D_8005A1BC; then the hook D_800501FC. Last comes a development-only `pollhost()` (`break 1024`, a host-debugger trap). It needs D_80010000 != -1 and D_80059390 set. Only func_800363E0 can set D_80059390 (func_80036288 clears it), and nothing calls or references func_800363E0 | input readers, including the battle pause, which spins on the queue |
| BIOS pad driver | InitPAD on the two 0x22-byte receive buffers, StartPAD and ChangeClearPAD(0) at boot (main2.c func_80036288); StopPAD in the soft reset | in the BIOS's interrupt handling (psx-spx: at vertical blank; not in the recovered code) | D_800625FC[0]/[1] (pad.h PadBuffer: status 0, or 0xFF without a controller; type; buttons; stick or mouse bytes) | the vblank handler's read (func_800358BC). Main-thread readers: the pad check func_80035734, which the battle and menu missing-pad loops spin on without VSync; the button read func_8003569C in the movie player (movie.c func_800737EC, func_800747AC, func_800769A4) and in battle's development pad read func_800BEC18; the field's mouse pointer (field_8007A44C.c func_8007AE78/func_8007AF74, type 0x12 on port 1); the unreferenced dump func_800365FC |
| vblank hook D_800501FC | the arena (menu5.c func_800852C4: func_80084FD0); cleared by the dispatcher | every vertical blank | on odd blanks while D_80092784 is set, func_8008E120 steps the arena's glow field, calling rand(): interrupt-time draws from the gameplay RNG | — |
| sound tick func_8003C020 (sound.c) | func_80037B88: OpenEvent(0xF2000002), SetRCnt(0xF2000002, 0x44E8, 0x1000) (system clock / 8: 4233600 / 17640 = 240 Hz) | 240 Hz; returns at once while flag 0x40 is set (every pause loop sets it with the voices silenced, func_80037EE4, and func_80037E8C clears it) | D_80059504 (tick count, stamps effect voices); every other tick the master and CD fades (D_8005A3C0), every tick the sequence slides and beats (D_80059564, which links the effect channels too), staged voice registers written through D_800508E4 (func_8003E900, func_8003EB5C), SpuSetCommonAttr, SPU IRQ re-enable (D_8005955C) | the effect-end waits (below); the main loop reads the same driver state between ticks |
| SPU transfer done func_8003BB64 | func_80037B88, each transfer (func_8003BE68) | DMA completion | runs the transfer's callback with flag 4, clears flag 0x10, starts the next of the eight queued transfers (D_80059510/D_800594F4), which sets 0x10 again | the transfer waits (below) |
| SPU IRQ func_8003BFA0 | func_80037B88 | SPU IRQ | D_80059514++, hook D_8005950C | — |
| CD sync/ready/data | resident func_80029690 (one file), func_80029AFC (file list), func_80029EB0 (image stream), func_8002A2D0, then the callbacks themselves (main_8002709C.c); world map func_8009699C, func_80096A6C (installs func_80096C0C) | CD interrupts | the read state machine D_8004FE1C, retry reason D_8004FE20, counters D_8005A48C/D_8005A490, sector copies (CdGetSector), stream-ring slots D_8004FE2C; image streams load VRAM strips from the callback (func_8002BB50) | func_80028A60(0) and `while (func_800286CC() ...)` busy-wait without VSync; world-map func_800967E4 |
| MDEC output movie_slice_decoded (mdec.c) | DecDCToutCallback in movie_start (mdec.c:183), removed by movie_stop (mdec.c:443) | MDEC DMA completion | LoadImage of each slice, StCdInterrupt, the decoder state, then the frame callback: field func_800A7120 (D_800B06A0 frame number, display block D_800ADB78/D_800C426C, isrgb24) or movie func_800768D8 | movie loops; field scripts and the staff roll key off D_800B06A0 |
| DrawSync callback | field func_8007781C (development kits only, D_800C268C == 0), arena func_80088C00 | GPU idle | VSync(1) stamps (profiling) | — |
| card events 0xF4000001 | slot39 func_801D9B08 | BIOS card driver | event state | func_801C881C (TestEvent spin) |

How the main program excludes these handlers:

- **Critical sections.** EnterCriticalSection/ExitCriticalSection appear in 11
  functions. They bracket:
  - the instruction-cache flush, FlushCache, in the dispatcher func_80019ACC,
    field func_8007999C, battle func_800BEB04 and world map func_800762FC,
    each after a DrawSync and a VSync;
  - the sound driver's start and stop (func_80037B88, func_80037DC0) and its
    transfer queue (func_8003BCA0);
  - slot39's card events and its CD-callback swap (func_801C8960,
    func_801D9B08, func_801D9C84, func_801D9E3C).

  The soft reset func_80019CD0 uses SwExitCriticalSection and
  SwEnterCriticalSection instead. These set and clear status-register bits
  0x401 directly (80040514, 800404f4).
- **The sound tick.** The sound-driver API excludes only the tick: 16 functions
  wrap their updates in DisableEvent/EnableEvent(D_800595BC), the tick's
  root-counter event. They guard:
  - the wave-bank list: func_80037FD8, func_800380D0, func_80038310;
  - the effect-bank list: func_80038428, func_8003852C;
  - the driver memory pool: func_80038F18, func_80039024, func_80039144;
  - sequence start, resume, snapshot, restart and stop: func_80039A80,
    func_8003AA30, func_8003ADCC, func_8003AE84, func_8003AF24;
  - the effect channels: func_80039FF8, func_8003B644;
  - the playing list: func_8003B9E4.

  The functions up to func_80039A80 are in main2_800366E0.c, the rest in
  sound.c. A port that runs the tick on an audio thread must keep each of these
  sections exclusive with it. Two paths return with the event still disabled:
  a failed allocation in func_80038F18 or func_80039024, and error 0xB in
  func_8003852C. The tick then stays off until the next guarded call reaches
  its EnableEvent.

Waits on the sound driver. A host that does not run the tick and the transfer
callback never leaves these:

- **Effect ends.** The tick ends an effect channel when its data reaches opcode
  90 end or FF stop_when_silent (sound.c func_8003CD8C, func_8003E54C).
  Otherwise a channel ends only through a stop call or a new effect that takes
  it. The stop calls are func_80039FF8 (all), func_8003A094 (a bank),
  func_8003A14C (an effect) and func_8003A20C (a channel pair); the channel
  choice for a new effect is func_8003A65C.
  - Field event fe 64 (func_8008F5E4) yields while any effect channel in the
    mask operand 1 << 8 is active (func_8003A5D0(-1)), so its script stalls.
  - Battle func_800B838C, reached through ovl3087 opcode 48 (func_801E8750),
    runs battle frames until func_8003A5D0(sound) returns 0. The event VM
    stalls while the battle keeps running.
- **SPU transfers.** Flag 0x10 stays set until the transfer queue is empty.
  - func_8003BDFC(0x10) re-reads the flag until it clears (the loop at
    8003be08). Every such call is a busy-wait
    (`grep -rn 'func_8003BDFC(0x10)' decomp/src`). They are boot
    func_80019578 (main.c:114), the driver's error beep func_8003F6B0
    (sound.c:3995), field func_80085788, func_80085890, func_800859DC,
    func_80085C90, func_80085F30 and func_8008AACC (field_800854D0.c:170, 204,
    233, 294, 367, 1951), and ovl3087 func_801E5160 (ovl3087.c:125).
  - `while (func_8003BDFC(0) != 0)` runs battle frames in battle
    func_800B838C, func_800B853C and func_800B89FC (battle_800B8098.c:193, 257,
    390) and func_800BFBA0 (battle_800BD3AC.c:1166). It spins in battle
    func_800C0FAC (battle_800BFE48.c:534), trapping to the debugger on each
    pass on a development kit, and in movie.c's unreferenced host-PC loaders
    func_800753B8 and func_8007548C.
  - ovl2615 func_801E6D6C polls the flag once per task step.
  - Outside a transfer callback, func_8003BCA0 spins while at least six of the
    eight ring entries are queued (func_8003BDBC).
  - Not a wait: the world map's `while (D_8005957C & 0x10) {}` in
    func_80072238 (worldmap_80072238.c:300), and its
    `if (debug) while (debug)` form in func_8007D918, func_80080D00 and
    func_80082324. Each runs after a wave-bank load (func_80037FD8) and the
    terrain-stream wait. All four compile to a single test followed by a
    branch to itself (`bnez v0` at 0x80072584, 0x8007DB54, 0x80080EFC and
    0x80082538), so the world map hangs if a transfer is still running when it
    gets there. No other code in the images branches to itself this way. The
    other self-branches are counted delay loops, which change their counter in
    the delay slot, and the fatal error's endless `j` (func_80019EF8). A modern
    compiler may treat the loop differently: it can re-read the flag, keep the
    hang, or drop the loop, since C11 lets it assume such a loop terminates
    (6.8.5p6). A port should code the intended behaviour explicitly.

Blocking waits deep in call stacks:

- VSync and DrawSync (75 and 159 functions).
- Disc waits: func_80028A60(0) and the world map's
  `while (func_800286CC() >= 3)` spin without VSync.
- The CdInit/CdControlB retry loops (func_80028230, func_800283D4) and the
  disc-change checks in slot39 (func_801E92CC, func_801E93A0, with VSync(3)
  between commands).
- The transfer waits above.
- The missing-pad spins of battle and the menus, and the battle pause (Pacing
  per mode). These run without VSync and depend on the pad driver and the
  vblank handler.
- The card TestEvent loop.
- The endless error screens (func_80019EF8, func_8002804C).

Development only:

- The ovl2606 battle-scene selector func_801E0238 loops DrawSync and VSync(0)
  until Start. Battle func_80070F40 runs it only while D_800594F8 is set
  (battle_80070E2C.c:345), and the field's and world map's battle requests
  clear that flag (field.c:3468, field_800854D0.c:5040 and 5067,
  worldmap.c:118).
- The vblank handler's pollhost() trap (above).

## Control flow and state

| Routine | Where and who | Keeps | Discards | Native replacement |
| --- | --- | --- | --- | --- |
| entry func_80019524 (handwritten) | PS-X EXE entry (header.c); soft reset calls it | .data and .sdata as they are | resident BSS 0x800592BC-0x8006FAEC (GameData included) | start-up that clears only BSS-backed state; restarting the process is not equivalent |
| stack reset func_80019548 (handwritten) | entry fallthrough; the dispatcher (call at 80019bd0) | resident globals, kept heap blocks, VRAM, SPU RAM | every stack frame: sp = fp = 0x80200000, gp = _gp | transfer to a host-owned dispatch loop (longjmp or fiber reset) that unwinds no host frames |
| dispatcher func_80019ACC(error) (main.c) | error: GET_RA, then the error screen. Otherwise ResetGraph, clear DrawSync and vblank hooks, VSync(2), restart the heap, clear the mode's BSS (func_80019560), decode its cached overlay to 0x8006FAF0 (func_80032EB4), FlushCache, reset the stack, clear the pad queue, run `entry()`, then call itself | the mode row (in a register across the reset), resident data | the previous mode's stack, BSS, heap blocks not marked keep and its overlay .data (re-decoded on each entry) | top-level loop; overlay .data reset from a pristine copy and BSS cleared on each entry |
| never-returning dispatch sites | boot func_80019578, kernel menu func_8001A4B4, field func_8007954C, battle func_8001B6C4, world map func_80070CFC, arena func_800851D4 (from inside the arena coroutine), menu func_8001C1A8 (debug start), movie func_800737EC (2 sites), heap errors 0x82 (func_80031BDC) and 0x83 (func_800320E8, func_80032C18) | — | the caller's whole stack | a non-local mode exit (12 sites); the heap errors and the disc error indicator (func_8002804C, endless after four retries) are fatal |
| fatal error func_80019EF8 | dispatcher with a nonzero error (the heap's 0x82 and 0x83) | — | — | on retail (D_80010000 = -1) it clears VRAM red and loops forever; the 384x240 report only runs on the PC host |
| soft reset func_80019CA0 -> func_80019CD0 | held 0x90C, checked at 18 call sites in 14 files; field extended event e2 (func_80086D4C), unused by shipped scripts | resident .data/.sdata | with interrupts enabled (SwExitCriticalSection) stops the graphics (ResetGraph), the CD (func_800283D4, CdFlush), sound (func_80037DC0), SPU, the vblank hook, the DrawSync and VSync callbacks and the pads, disables interrupts (SwEnterCriticalSection), then calls the entry, which clears BSS | reset that keeps modified initialised data; the sound mode returns to Stereo |
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
  builds mostly turn it off for what follows. The counts below take the third
  (dither) argument of every call that
  `grep -rn 'SetDrawMode(\|SetDrawTPage(' decomp/src --include=*.c` lists. It
  lists 63 and 21 lines, all of them calls, each with its first three
  arguments on the line:
  - SetDrawMode passes 0 at 58 of 63 call sites.
  - SetDrawTPage passes 0 at 16 of 21.
  - The linked images hold 62 and 20 such calls, because the two branches of
    arena func_8008E2B8 and of world map func_800925A0 each share one call.
  - Dither 1 goes to the arena's screen fades (menu7.c func_8008E2B8 at 2118
    and 2120, func_8008E3CC at 2167), its HUD packets (menu5.c func_80085EC8
    at 1565 and 1635, func_800868E0 at 1792 and 1794) and the world map's
    dithered saved-screen fade (worldmap_80072238.c:515 in func_80072DB4, a
    DR_TPAGE drawn before its translucent black quad).
  - ovl2615's stage backdrops copy the draw environment's dfe and dtd
    (stage.c func_801E7914, two sites, after GetDrawEnv).

  Observed: rendering-behavior.md, Dithering.
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
  bit 15 (STP) on captured pixels (battle_800B7134.c func_800B7870, field
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
