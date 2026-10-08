# Original service, timing and state boundaries

What the recovered PS1 program needs from its platform, for the later native port.
The facts come from the matching source and from `tools/service_calls.py`, a
static scan of the linked images (calls, base-register tracking, cop2 use); it is
not an execution trace. Regenerate the inventory instead of copying it here:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching -c \
  python3 tools/service_calls.py <psx_psyq_signatures data dir> \
  decomp/targets/resident/slus_006.64.mk decomp/targets/overlays/*.mk [--detail]
```

(the signatures are the pinned `psx_psyq_signatures` input of nix/ghidra, found at
`lib/ghidra/Ghidra/Extensions/ghidra_psx_ldr/data/psyq` in the loader package).

## Services

Game code reaches hardware through the PsyQ libraries linked into the resident
(classified `sdk`, 8003f8e8-8004ea88) and the movie overlay (801d444c-801d68b4).
Of 4435 compiled game functions, the scan finds calls into these libraries:

| Library | Use |
| --- | --- |
| libgpu | display/draw environments, ordering tables, primitive setup, VRAM loads/stores/moves, TIM parsing, DrawSync |
| libgte | matrices, perspective transforms, lighting/normal colour, square roots, ratan2 (plus 17 unnamed members) |
| libcd | CD control/read, sector positions, streaming ring (`St*`), ready/sync/data callbacks, CdMix |
| libapi | events, root counters, critical sections, pad start/stop, BIOS file I/O, FlushCache |
| libetc | VSync, VSyncCallback, ResetCallback, SetVideoMode |
| libspu | SPU init/memory, transfers (SpuRead/SpuWrite at 8004d818/8004d878 by inspection), reverb, IRQ and transfer callbacks |
| libcard | InitCARD, StartCARD, `_bu_init` |
| libpress, libds | MDEC decoding (DecDCT*), DecDCToutCallback; CdIntToPos/CdPosToInt |
| libc, libc2 | memory/string helpers, rand, sprintf |
| libsn | PC host file I/O (PCopen/PCread/PCwrite/PClseek/PCclose; PCinit/PCread/PCwrite identified by inspection at 8004c38c/8004c398/8004c470) used by development-only paths |

Outside the libraries the game itself uses:

- **SPU registers** through the pointer `D_800508E4` (0x1F801C00, sound.h): the
  sound driver's voice-register writers in sound.c and main2_800366E0.c (15
  functions). No compiled game function addresses the I/O window directly.
- **GTE** inline in 77 compiled functions (GTE macros) and in the classified
  hand-written routines (resident model renderers, menu and world-map batches).
- **Scratchpad** (0x1F800000-0x1F8003FF) as fast work memory in 97 functions,
  79 of them in the world map (terrain batches and their tables).
- 88 indirect calls (`jalr`): dispatch tables, task callbacks and hooks.

## Timing

- Video: `SetVideoMode(0)` (NTSC) at boot (main.c `func_80019578`). Modes pace
  their frames with `VSync` (75 callers); draw completion is waited with
  `DrawSync` or handled by DrawSyncCallback (field 80077e88, menu 80088d1c).
- Vertical blank: `func_8003634C` (main2.c), installed at boot and by the world
  map, counts frames, polls controllers, the input queue and the play clock,
  runs an installable hook (`D_800501FC`) and, on a development host, polls it.
- Sound: the driver tick `func_8003C020` (sound.c) runs from root counter 2
  (event 0xF2000002, `SetRCnt(..., 0x44E8, RCntMdINTR)`: libapi selects
  system clock / 8, so 4233600 / 17640 = 240 ticks per second), installed by
  `func_80037B88` with the SPU transfer (`func_8003BB64`) and IRQ
  (`func_8003BFA0`) callbacks. Fades step on every other tick.
- CD: the resident read/stream code (main_8002709C.c, main_8002A260.c) and the
  slot39/world-map readers install CdReady/CdSync/CdData callbacks; reads
  complete asynchronously into the callers' buffers.
- Movies: MDEC output completes through `DecDCToutCallback` (mdec/movie).
- Shared state touched from interrupts is guarded with Enter/ExitCriticalSection.

## State

- The resident (both discs, `SLUS_006.64/69`) owns persistent state: its .data/.bss
  (0x8004ea88-0x8006faec), the heap (heap.c; the battle modules' slot at
  0x801fc000 bounds it) and the sound driver pools.
- Mode overlays share 0x8006faf0 and open with their number: field 4, world map 5,
  battle 6, menu 7, movie 8. The dispatcher `func_80019ACC` (main.c) reports a
  fatal error if requested, resets graphics and the heap, clears the next mode's
  BSS, decodes its overlay and runs it; state that survives a mode change lives
  in resident globals.
- Secondary overlays load at fixed addresses: slot39/ovl2598-2602 at 0x801c5000,
  mdec at 0x801d3000, ovl2143 at 0x801dc000, ovl2596 at 0x801de000, ovl2606 at
  0x801e0000, ovl2615 at 0x801e4000, ovl3087 at 0x801e5000, the battle modules
  ovl3381-3387 at 0x801fc000 (loaded by battle `800beb04`), and the debug
  overlays at 0x80280000 (development-kit RAM only).

## Presentation and sound modes

- Display: 320x224 double buffers for the boot logo and the kernel menu
  (main.c `func_80019D48`, `func_8001A250`) and for battle (shown through a
  256x216 screen area, battle_800B8098.c `func_800B8284`), 640x224 for the field's
  high-resolution scenes (field_800A4748.c), 384x240 for the fatal error screen
  (`func_80019EF8`); menus take their size from the caller (menu7.c).
- Output modes are driver flags in `D_8005957C` (`func_800386C4`, main2_800366E0.c):
  mode 0 Mono (every voice at the centre gain 0x5A00), mode 1 Stereo (0x100, the
  pan law of `func_8003EBF0` in sound.c), mode 2 Wide (0x300: the stereo pan
  law plus the right master and reverb volumes inverted by `func_80038E6C`),
  mode 3 (0x500, the other side inverted) is never selected. The sound option
  (slot39.c `func_801D9808`) maps its choices 0/1/2 to modes 0/2/1. CD
  audio: Mono halves the CD volume into both channels (`func_8003885C`).
- Original evidence (local): `.local/scenarios/p1-original-audio-modes-20261002.json`
  compares Mono/Stereo/Wide captures; in both measured windows Wide's right
  channel is Stereo's inverted within 3 LSB.
