# Rendering behavior and visual references

This note records what the original program asks the PS1 GPU and GTE to do on
every required route path of the forest slice, the PS1 hardware behavior those
requests rely on, and the source-qualified reference frames of each path. It is
the evidence base for the optional PS1-fidelity settings; native modern
rendering stays the default.

Three kinds of statement are kept apart:

- **Game intent**: what the original code and its emitted packets request.
  Derived from the qualified executable
  (`dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119`), its
  decoded overlays and captured RAM.
- **Hardware**: how a PS1 GPU/GTE executes such requests. Taken only from
  general PS1 documentation (psx-spx) and Sony library conventions; marked
  *(psx-spx)*. Not measured on hardware in this project.
- **Reference emulator**: what the captured frames show. The frames come from
  PCSX-ReARMed r26 (libretro, HLE BIOS, software GPU) with dithering enabled,
  16-bit frontend output (`rgb32_output` disabled), no enhancement or overscan.
  Emulator output is a reference, not proof of hardware rasterization.

Evidence levels: **observed** (read from captured original RAM, packets or
frames), **static** (read from original instructions), **documented**
(psx-spx), **inferred**, **untested**.

## Method

`tools/analysis/gpu_packets.py` walks an ordering table in a RAM image, decodes
every GP0 command, and summarizes primitive classes, texture pages, CLUTs,
semi-transparency modes, dithering state at draw time, texture windows, state
commands, vertex ranges and triangles beyond the GPU size limits. It also
decodes the libgpu `DRAWENV`/`DISPENV` structures (including the GP0 words of
`dr_env`) and the GTE control registers recorded with each snapshot. The list
heads and environments are the ones the original code passes to libgpu:

| Mode | Original call | List head | DRAWENV / DISPENV |
| --- | --- | --- | --- |
| Field overlay | `800758c8` DrawOTag | `[800c426c] + 80f0` | `[800c426c]` / `+b8` (PutDispEnv `80075788`) |
| Battle overlay | `800bea40` DrawOTag | `[800ccb00] + 406c` | `[800ccb00]` / `+5c` |
| Menu overlay | `801c7d4c` DrawOTag | `[[800625a0] + 1d4] + ac` | `[[800625a0] + 1d4]` / `+5c` |
| Mode 6 (movie) | `80073aa4` PutDrawEnv | none while a movie plays | `[80077120]` / `+5c` |

Snapshots are taken where the list is complete: field frame exit (`80075908`,
after DrawOTag), battle frame entry/exit (`800716d8`/`80071714`; the buffer
swap happens inside the frame at `800be7f8`), battle action frames
(`800797cc`), menu frame entry (`801c7bf4`, before the swap at `801c7c60`).
The field frame-exit method gives exactly the same census as DrawOTag
snapshots for the same frames (frontend runs 5001/5003). The first menu frame
after menu entry has no built list yet and is reported as unreadable, not
used. The title logo and field 22 near the save point and after loading come
from frame-boundary RAM dumps, which may hold a list under construction; only
complete-looking lists (full model table plus closing packets) are used, and
those observations are marked *boundary dump*.

Captures reused (disc content
`14aec5ccd2589fb07f86334b1b2cab57d63ef6c34dbd163d8ee569028c6f7f86`; no new
capture was needed):

| Capture | Scenario (sha256) | Used for |
| --- | --- | --- |
| `p1shared-route-field-frames` | frozen route `phase1-field23-encounter-victory-return` (`5a92857b…c24683`) | field 23 dialogue and exploration packets; route reference frames |
| `p1render-gpu-{dialogue,walk,battle,menu}-20260924` | same frozen route | DrawOTag snapshots (cross-check) |
| `p1shared-transition-chain` | `p1transition-field23-to-22-long` (`ec20d81b…5c8e`) | field 23 → 22 transition packets and frames |
| `p1shared-battle-frames-p1battleov`, `p1shared-victory-results-20260924-v1` | frozen route | battle entry, victory and results packets |
| `p1battle-turns-20260924-v3` | `phase1-field23-encounter-enemy-turns` (`bc5bf5a7…f3bc`) | battle actions and enemy turns |
| `p1shared-menu-actions-menu` | `phase1-field23-menu-actions` (`93348a58…4b96`) | menu screens |
| `p1shared-save-menu` | `phase1-field22-save-point-card2` (`6c08ec7c…9437`) | save screens, field 22 save point |
| `p1shared-load-menu` | `phase1-field22-load-card1` (`d8d230b1…8de6`) | title, load screens, field 22 after load |
| `p1shared-movie-transition-menu`, `p1shared-movie-transition-end` | `p1media-new-game-movie` (observation `9900c943…1f69`) | title menu, opening movie, post-movie field |
| `p1title-mode6-chain` | `p1media-new-game-movie-skip` (observation `4f27d5b2…07c8`) | mode 6 boot logo movie |

## Display and draw environments (observed)

| Route path | Draw area / display (VRAM) | Display flags | DRAWENV | GTE H, OFX, OFY |
| --- | --- | --- | --- | --- |
| Field 23, field 22, post-movie field | 320x224 at y 0 and y 256, swapped each frame | 15-bit, non-interlaced, `screen=(0,10,256,216)` | clip = buffer, offset = buffer origin, `dtd=1`, `dfe=1`, `isbg=0`, tpage `000a` | H 640 (field 23), 512 (field 22 and the black-screen dialogue), 768 (post-movie map); OF (160, 112) |
| Title logo (field map 490) | as field | as field | as field | H 512 |
| Battle, victory, results | 320x224 at y 0 and y 224 | 15-bit, non-interlaced, same screen | `isbg=1`, background (12,12,4) → fill `040c0c` each frame | H 512, OF (160, 164) |
| Menus, title menu, save/load | 320x224 at y 0 and y 224 | 15-bit, non-interlaced, same screen | `isbg=0` | no projection |
| Field-player movie (opening) | 320x224 at y 0 and y 256 | **24-bit** (`isrgb24=1` set by `800a7120` on the block showing the frame) | field DRAWENV; the list holds only E1/E2/NOP packets | – |
| Mode 6 movie (boot logo) | draw 320x240 at y 0, display 320x240 at y 240 | **24-bit** (`800763bc` sets `isrgb24`) | `isbg=0` during playback | libgte defaults (H 1000, OF 0) |

Every `dr_env` packet is E3/E4 clip, E5 offset, E1 (tpage `000a`, dither on,
draw-to-display on), E2 window 0, **E6 set=0 check=0**, plus a fill in battle.
No list contains an E6 packet: the mask bit is never set or tested on any
route path (observed). Drawing to the displayed buffer never happens (the
draw and display areas always differ); the E1 draw-to-display bit only
matters for interlaced output *(psx-spx)*, which no path uses.

The display shows 216 of the 224 drawn lines (`screen.h = 216`, starting 10
lines below the standard top). In the reference emulator's 320x240 output the
lit rows of full-screen frames are exactly rows 10..225 on every path
(observed, 31 reference frames).

## Primitives per route path (observed)

Counts are summed over the sampled lists; "per list" is the mean number of
drawing primitives.

| Route path | Lists | Per list | Primitive classes | Semi-transparency modes | Texture depths |
| --- | --- | --- | --- | --- | --- |
| Field 23 exploration | 43 (runs 2401..5845) | 239 | flat textured tris/quads, raw and modulated (colour `808080`) | 1 (raw quads), 2 (modulated quads) | 8-bit, 4-bit CLUT |
| Field 23 dialogue | 27 + 13 (black screen) | 174 / 45 | as above plus variable textured rectangles (text, window) and flat semi rectangles | 1, 2; window rectangles 2 | 8-bit, 4-bit |
| Field 23 → 22 transition, field 22 | 5 + 30 | 142 / 246 | as field 23 | 2; near the save point (boundary dump) also **3** and 1 | 8-bit, 4-bit |
| Field screen fade: post-movie field; field 22 entry | 11 lists (frame exit); boundary dump | 8 | modulated semi quads textured from **15-bit pages at (704..960, 256)**, i.e. the other frame buffer | 1 | 15-bit direct, 8-bit, 4-bit |
| Title logo (map 490) | 4 boundary dumps | 4 | two raw textured quads from 15-bit pages (512,0) and (768,0), a flat quad, a Gouraud quad | – | 15-bit |
| Battle entry, commands, actions | 61 + 4 + 24 | 375..465 | flat textured tris/quads, Gouraud quads, Gouraud textured quads, flat tris, flat lines, flat semi quads | 0, 1, 2 | 8-bit, 4-bit |
| Victory and results | 53 + 64 | 339 / 375 | as battle; results windows are Gouraud (semi) quads | 2 | 8-bit, 4-bit |
| Menus (status, items, equipment) | 63 + 4 | 123..133 | textured quads raw/modulated/semi, Gouraud and Gouraud semi quads, flat semi quad | **0** (windows), 1, 2 | mostly 4-bit |
| Save and load screens, title menu | 64 + 64 + 56 + 63 | 7..135 | as menus plus flat **polylines** (card slot frames) | 0, 1, 2 | 4-bit, 8-bit |
| Movies | 37 + 33 | 0 | none (MDEC frames go to VRAM by LoadImage) | – | – |

Only battle uses Gouraud-shaded textured quads; all field geometry is
flat-shaded. No list contains fixed-size (1x1, 8x8, 16x16) rectangles; field
text uses variable-size textured rectangles.

Texture windows (E2) are active only for field dialogue windows and text
(masks 240/248 with offsets tiling 8-16 pixel cells); every other E2 has mask
0 and so no effect *(psx-spx)*.

## Dithering (observed intent)

Each DRAWENV enables dithering. Lists then contain E1 packets with dither 0:

- Field: two E1 packets (page (896,256), dither 0) in the closing small table.
  Model and sprite quads drawn before them are dithered; dialogue windows and
  text after them are not.
- Battle: E1 packets (page (960,0), dither 0) split the list; stage and model
  quads before them are dithered (80% of modulated quads at battle entry, 46%
  on the results screens), effects, windows and HUD after them are not.
- Menus: an E1 packet ahead of all menu primitives turns dithering off.

On the PS1, dithering affects Gouraud-shaded and texture-blended (modulated)
primitives; raw-textured primitives and rectangles are not dithered
*(psx-spx)*. The field's modulated quads use the neutral colour `808080`, so
their only visible difference from raw texturing is the dither pattern
*(psx-spx; untested)*.

## Ordering tables and primitive order (static + observed)

- **Field** (buffers `800b249c + 80f4*n`): a 4096-entry model table at
  `+cc` (ClearOTagR at `80073ffc`), an optional second 4096-entry table at
  `+40d0`, and an 8-entry small table at `+80d4` whose last entry `+80f0` is
  the DrawOTag head. `AddPrims` (`80043b84` via `80075458`) links model
  entries `0..ot_depth` (`800b21d4`, observed 1824 on maps 23, 22 and 4) into
  the small table; entries above `ot_depth` are never drawn, a far limit at
  slot 1824. Each list has 1825 + 8 empty nodes (observed).
- **Field model sort key** (`draw_primitives`, table `8004fe50`): OTZ from
  AVSZ3/AVSZ4 (ZSF3 `0x155`, ZSF4 `0x100`: a quarter of the mean SZ) shifted
  right by `80050100` (observed 2), so a slot covers 16 units of mean depth;
  the "farthest" routines use max(SZ) with shift + 2. Primitives whose OTZ is
  0 are skipped. Entries are reverse-linked (ClearOTagR), so higher slots
  (farther) draw first; within a slot the last linked primitive draws first
  (AddPrim inserts at the head).
- **Field culling**: NCLIP ≤ 0 is culled (back faces); a primitive is dropped
  unless some vertex has 0 ≤ x < `800500f8` (320) and some vertex word is
  below `800500fc` (`00ef0000`, i.e. 0 ≤ y < 239). The GTE error test reads
  data register 31 (LZCR) instead of FLAG and never rejects, so projections
  near or behind the camera are submitted: field lists contain triangles more
  than 1023 pixels wide or 511 tall (both triangles of two quads in every
  sampled field 22 list; 14 triangles over 70 field 23 lists). The GPU does
  not draw such triangles *(psx-spx)*.
- **Battle**: one 4096-entry table per buffer (`800c3eb0 + b70` / `+4be0`,
  table at `+70`, head `+406c`); projection by RTPS/RTPT and AVSZ3 in the
  battle overlay. The slot mapping is not yet recovered (inferred: average Z).
- **Menu**: a 16-entry table per buffer (`+70`, head `+ac`), used as layers;
  no GTE projection.

## Projection precision (static + observed)

All projection uses GTE RTPS/RTPT with sf=1 (resident 14 RTPS/23 RTPT, battle
overlay 11/7, field overlay 1 RTPS; static count). Screen coordinates are the
SXY FIFO values written unchanged into packets: integer pixels, saturated to
-1024..1023 *(psx-spx)*; GP0 vertices are signed 11-bit integers. No path
carries sub-pixel positions (observed: all packet vertices are integers by
format). This integer snapping is the source of PS1 vertex "wobble".

## Fog and depth cueing

Field frames load fog parameters (`800b2190` colour 128,128,128;
`800b2194` far colour 255,255,255; `800b2198` range 5600..12300) but on every
sampled field, battle and menu list the GTE far colour is 0 and DQA/DQB are
the libgte defaults (−4194, `0x1400000`), and every field modulated colour is
the neutral `808080`. Depth cueing is therefore not visible on the route
(observed); the fog path (`frame_models` SetFarColor/SetFogNearFar, sprite
DPCS) is reached only when the field's sprite gate is set (static).

## Parameters for the PS1-fidelity settings

| Behavior | Game intent (slice parameters) | Hardware | Reference emulator | Evidence |
| --- | --- | --- | --- | --- |
| Vertex snapping | Integer screen XY from RTPS/RTPT (sf=1), OF (160,112) field / (160,164) battle, H per map (512/640/768) | SX/SY integer, saturated ±1024; GP0 11-bit signed | integer rasterization | observed + static; hardware documented |
| Affine texture mapping | All textured polygons are flat or Gouraud tris/quads with per-vertex UV; quads are two triangles (0-1-2, 1-2-3) | No perspective correction; quads split into two triangles; nearest texel, no filtering | same | observed intent; hardware documented |
| Texture interpolation | None requested (4/8-bit CLUT and 15-bit direct pages) | Point sampling only; texel `0000` transparent | same | observed; documented |
| 15-bit colour | Draw and display 15-bit on all rendered paths; 24-bit display only for MDEC movies | 5:5:5 framebuffer | 16-bit frontend output (FMV colours in references are frontend-quantized) | observed |
| Dithering | DRAWENV `dtd=1`; E1 dither 0 before field UI, parts of battle, all menus | Applies to Gouraud and texture-blended primitives | enabled | observed intent; hardware documented; emulator pattern untested |
| Rasterization resolution | 320x224 drawing, 216 lines displayed from line 10 (`screen=(0,10,256,216)`); movie mode 320x240 | 320-wide dot clock mode, non-interlaced | 320x240 output, image rows 10..225 | observed |
| OT ordering artifacts | Field: mean-depth slots of 16 units, reverse OT, LIFO within slot, far limit slot 1824, OTZ 0 dropped; battle 4096 slots; menu 16 layers | Draws in list order, no depth buffer | same | observed + static; battle slot mapping inferred |
| Oversized primitives | Field submits near-camera triangles beyond 1023x511 | Not drawn | not measured | observed intent; hardware documented; emulator untested |
| Semi-transparency | Modes per route above: field 1, 2 (3 near the field 22 save point), battle 0, 1, 2, menus 0, 1, 2; field fades blend the previous frame (15-bit page at y 256) additively | Mode 0 B/2+F/2, 1 B+F, 2 B−F, 3 B+F/4; textured: only texels with bit 15 blend | same | observed; hardware documented |
| Mask bit | Never used (E6 0/0, no in-list E6) | – | – | observed |
| Texture windows | Field dialogue only (E2 masks 240/248) | Window formula | same | observed; documented |
| Depth cueing | Not visible on route (defaults, far colour 0) | DPCS/NCDS interpolation | – | observed |

## Visual references

Reference frames are private images under `.local/scenarios`; only their
identities are recorded here. All are 320x240 frames of the frontend run
named in the capture (frame file `frame-NNNNNN.png`).

| Route path | Checkpoint | Capture | Run | PNG sha256 | Lit rows |
| --- | --- | --- | --- | --- | --- |
| title-logo | title screen (field map 490 draws the logo) | `p1shared-load-menu` | 1200 | `72261feb872e294b1ccb7c1876f247a066b869ede98bc52374e442d957f8b438` | 13..225 |
| title-menu | New Game/Continue (menu overlay) | `p1shared-load-menu` | 2761 | `2651667b23763e23df0cf59b407a94022b541635e2bdfe3421221de10847f336` | 20..225 |
| title-menu-newgame | title menu, New Game | `p1shared-movie-transition-end` | 1800 | `9932112211812a6471937ff665c7243b293cff9a851a941e09e49d36887fcab3` | 20..225 |
| fmv-mode6-logo | mode 6 boot logo movie | `p1title-mode6-chain` | 600 | `bdc34a14f27b3f1d37dbc0b13346719b8550973ab697e03880aa44411bfaf561` | 106..129 |
| fmv-opening-a | opening movie (field movie player) | `p1shared-movie-transition-end` | 4200 | `38543c594d33ed7c53366582cee9e262d08918e656168ac75d5ce46e36e6db19` | 10..225 |
| fmv-opening-b | opening movie | `p1shared-movie-transition-end` | 8400 | `571cf8e58850925e183d31d982bf326555ae51024be29c7a7cf8f48b8062b114` | 10..225 |
| fmv-opening-c | opening movie | `p1shared-movie-transition-end` | 15000 | `0133ff4183d9f0840354acd950e8231d7a97556d1713bfd2aef758d420e94bcf` | 10..225 |
| post-fmv-text | prologue text after the movie | `p1shared-movie-transition-end` | 21600 | `c9d1fffe4041cd21578d417177d0a1640e6e7b6a939c6e13c053b8877f4c27d5` | 37..202 |
| field23-dialogue-black | entry dialogue on black | `p1shared-route-field-frames` | 1237 | `d7e43563523813921953cb1836cf7653e50ff9ede1a9b2e7470242982a4d5212` | 23..50 |
| field23-dialogue | entry dialogue over the scene | `p1shared-route-field-frames` | 1545 | `a16a892ed515d02d363b4bbba1ff30edf6a7b3e1a2afe7f69398e5b3ab1c2fb2` | 10..225 |
| field23-exploration-a | exploration, start | `p1shared-route-field-frames` | 2400 | `d496e5d02cbc84d173ff9a178acefb08eb2ed59fd03f79417c56bc557446e57b` | 10..225 |
| field23-exploration-b | exploration, eastern ramp | `p1shared-route-field-frames` | 4977 | `53b387fd95e7b07c9f81ce1b3dc322dc4d21721169b0eace6bd4586a4c2c51e4` | 10..225 |
| field23-exploration-c | enemies approaching | `p1shared-route-field-frames` | 5830 | `cd4a0d2ac593f3bb88fc548988d5614b1bce5bd051f1e399366ed3fa80dc0673` | 10..225 |
| field23-to-22-transition | zone 1 transition | `p1shared-transition-chain` | 5406 | `5701427261614f7c1936e312b94e4eda15fef373d774956778ffbb015f230d04` | 10..225 |
| field22-arrival | field 22 after the transition | `p1shared-transition-chain` | 6106 | `996e6b6194fcc1182db14009e41f2773955073117a4931056fad4b7bb62a397e` | 10..225 |
| field22-save-point | field 22 save point | `p1shared-save-menu` | 6992 | `14e9658525a37ad614ba85c8ea6f1e2abf42f02c510490b42c74ece422306c66` | 10..225 |
| field22-after-load | field 22 after loading | `p1shared-load-menu` | 4364 | `ea732851002d9c677ed0aec50d13264148f5dfac5036ec46ee6fdf68cde3513b` | 10..225 |
| battle-command | command ring and HUD | `p1shared-route-field-frames` | 6600 | `35e6d73c99c401e0d86f69434ff5cc611399a9ceadf8e663bc756d715df1db1a` | 10..225 |
| battle-attack | attack | `p1shared-route-field-frames` | 7222 | `f8b5c9d1d44e4c1798f05c88b93beb869c1a27211d0e848ef5d7ff35d96c909e` | 10..225 |
| battle-hit-effect | hit effect | `p1shared-route-field-frames` | 7800 | `5f7dce860bad5864f3484c8e15a8981539a18df5a73b6606a08a1e7b8b41d5d2` | 10..225 |
| battle-victory | victory pose | `p1shared-route-field-frames` | 8400 | `c433cbe9cdd3738f7219e15ec367f2fb14ac9842954ff3ca4764fd73be653ccf` | 10..225 |
| battle-results-exp | results, experience | `p1shared-route-field-frames` | 9000 | `c021164cbd7ceb069cd4f5baa2909227b1c0d01d80f183edc988bd64714bff5b` | 10..194 |
| battle-results-levelup | results, level up | `p1shared-route-field-frames` | 9600 | `f1d232835452258b7a1de2d3029e92bd5ea6fdb6f05b76d745653b92d981fdf5` | 10..217 |
| battle-results-items | results, items and gold | `p1shared-route-field-frames` | 10154 | `5e09744fa2fc876bf76c0199139738914778c33000f95c3399c70360948de754` | 10..193 |
| field23-return | field 23 after battle | `p1shared-route-field-frames` | 10762 | `0b55b25468f68eb46b3ef9ad375221d01bd9382aa9155c2af98bc09cfb3e2520` | 10..225 |
| menu-main | game menu, status | `p1shared-route-field-frames` | 11400 | `50462df3bef00062c1872b893f88f0760c02bba230e0239643cd30c2203c7dce` | 16..225 |
| menu-save-slots | save, card slots | `p1shared-save-menu` | 7559 | `214c00225cef25360f0cf1d7ef4c3ca0758f87b583a2f2b9afbc3b38132da90b` | 19..221 |
| menu-save-format | save, format prompt | `p1shared-save-menu` | 8119 | `819378177ac5bc2134487f760390fd43f56c5c125c71f6d34ca740c309837d82` | 19..225 |
| menu-save-progress | save, writing | `p1shared-save-menu` | 8716 | `6d983277480dc9d316e11cf9519c831a9ee892939bb4a2f3e712003c13dab1f2` | 24..225 |
| menu-load-file | load, file detail | `p1shared-load-menu` | 3199 | `ec23e37b89416c499b1780611fe5f7e32a28b094e3fe2a79872952317e1670ba` | 19..225 |
| menu-load-confirm | load, confirmation | `p1shared-load-menu` | 3333 | `33188db4c841a5a1387edd6d4f5428be7d89f42938ec93296166e2af411abe40` | 19..225 |

## Unreconstructed packet-emitting code on the route paths

Executed functions that build primitives, link ordering-table entries or
project vertices (static scan of the executed, not reconstructed functions of
the p1cov census; routes are census route names):

| Owner | Functions | What they emit | Routes |
| --- | --- | --- | --- |
| battle-a | `800728b8`, `80073a58`, `80073b64`, `80073fb8`, `800743a4`, `800745ec`, `80074d4c`, `80074f70`, `8007500c` | AddPrim callers (HUD, command and effect primitives) | encounter, enemy-turns, defeat, escape |
| battle-b | `8009f844`, `800a2fd8`, `800a4db8` | RTPS/RTPT projection with packet writes | battle routes (8009f844 on the sound routes' battle) |
| battle-c | `800b1f6c`, `800b7160` | RTPT/NCLIP/AVSZ3 model primitives; AddPrim | battle routes |
| post-battle module | `801de1c4`, `801de408` | AddPrim | victory routes |
| module slot 2143 | `801dcec8`, `801e0398` | RTPS/RTPT projection | sound-mode routes |
| resident-game | `80019d48` (PutDrawEnv/PutDispEnv/DrawPrim/LoadImage), `8001e9bc` (billboard sprite, quad code 2c; the field stops at it with `MissingDependency`), `80025544` (AddPrim + E1), `80026ba4` (quad code 2d), `800273c4` (RTPS; field `80075484` stops at it), `80030ee8` (RTPT) | field sprites and effects, movie/title screens | field, battle, movie routes |
| resident-lib | libgpu `80043c60`, `80043c9c` SetPolyF4, `80043cc4` SetPolyG4, `80043cd8` SetPolyGT4, `80043d78` SetLineF2, `80043da0` SetLineF3; `80045ae8`/`80045bb4`/`80045c00` (E3/E4/E5 words); `80045ed0` (DRAWENV `dr_env` packet: E1..E6); `8004709c` (E1 word) | primitive headers and environment packets | all routes |

The libgpu setters are already open-coded in several reconstructions without
their address comments; `80045ed0` is the only producer of the `dr_env`
words recorded above.

## Limits

- Hardware rasterization (dither matrix, blending precision, triangle edge
  rules) is documented, not measured; no pixel comparison is made here.
- Emulator reference frames use 16-bit frontend output; 24-bit movie colours in
  the references are quantized by the frontend, not by the game.
- Battle slot mapping, battle effect primitives and the fog path are not
  recovered.
- Field 22 near the save point, after load, the title logo and the field
  transition fade come from frame-boundary dumps (complete-looking lists only).
