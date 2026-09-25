# Field frame, drawing and PS1 rendering behavior

`Program::field_frame` (field overlay `8007554c`) and `Program::field_loop_step`
(main loop `80077e88`, between-frame code from `800782e4`) reconstruct one
complete field frame of the frozen field 23 route: movement and events, drawing,
dialogue text, frame submission and the frame-rate wait. The source is
project-authored from the qualified field overlay (sha256
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`, loaded at
`8006faf0`) and the resident executable
(`dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119`).

## Frame structure

| Original | C++ | Work |
| --- | --- | --- |
| `800739c0` | `field_move` | Events, field update `8008110c`, camera, facing |
| `80086590`, `80071cb4` | `field_frame` steps | Effect emitters, screen fade |
| `80074108` (+`8007ab6c`, `8007ac58`) | compass | Direction compass sprite |
| `800748e8` (+`8002c700`, `8002e010`..`8002e688`) | `field_models.cpp` | Background and object models: GTE transform, primitive routines per model part |
| `800752c8` (+`800250e0`, `8001d468`, `8001dae8`, `8001d53c`, `800251c8`, `80022038`) | `field_characters.cpp` | Actor sprites: frame build, cell frames, texture upload, sprite matrices |
| `80075b44`, `800764b4` (+`8001e298`, `8001e148`, `8001e3d8`, `80021b98`) | `field_characters.cpp` | Billboards and shadows |
| `800805f4`, `8008004c` (+`80033df0`, `80034888`, `80034f98`, `80034ffc`, `80034714`, `800348d8`, `8007e1c0`, `8007f6f8`) | `field_dialogue_draw.cpp`, `field_text.cpp` | Dialogue windows, glyph rendering, pages, open and close |
| libgpu `80044bd0` DrawOTag, `80044c44` PutDrawEnv, `80044e9c` PutDispEnv, `80044894` LoadImage, `80044764` ClearImage, `800445d0` DrawSync | `gpu_queue.cpp` | Ordered `GpuCommand` records (the display service boundary) |
| `8004b54c` VSync | frame services | Frame-rate wait |
| `800782e4`..`800782dc`, `80074700` | `field_loop.cpp` | Between frames: loop checks, map-change step, ordering-table clear, pad drain |

Primitives and ordering tables are Program-owned RAM, exactly as the original
builds them. Hardware-facing libgpu operations are ordered commands for a native
display service. They are never guessed pixel output.

## Cadence

Each frame reads `VSync(-1)` at its start. After `DrawOTag` it waits until the
vertical-blank count reaches `start + *800b217c + 2`. With `800b217c = 0` on the
route, a field frame lasts at least two vertical blanks, about 30 frames per
second on NTSC. This is a gameplay-visible rule of the original. The emulator's
vertical-blank timing is an HLE observation, not a hardware timing proof.

## Rendering behavior observed on the route

A census of every primitive submitted with `DrawOTag`, plus each `PutDrawEnv`,
`PutDispEnv`, `LoadImage`, `MoveImage` and `ClearImage` call, sampled over dialogue,
walking, battle, post-battle and menu frames of the frozen route (captures
`p1render-gpu-{dialogue,walk,battle,postbattle,menu}-20260924`), records these
original settings. Command encodings are general PS1 GPU knowledge.

| Behavior | Original setting |
| --- | --- |
| Display resolution | 320x224, non-interlaced, 15-bit color (`isinter=0`, `isrgb24=0`) |
| Double buffering | Field buffers at VRAM y 0 and 256. Battle and menu buffers at y 0 and 224 |
| Screen area | `screen=(0,10,256,216)`, the TV display window |
| Dithering | Each `DrawEnv` enables dithering (`dtd=1`). Draw-mode (`E1`) packets inside the ordering tables turn it off (`dither=0`) for the primitive groups that follow them |
| Texture depth | 4-bit and 8-bit CLUT texture pages. No 15-bit direct textures in the sampled frames |
| Texture modulation | Both raw (unmodulated) and color-modulated textured polygons |
| Semi-transparency | Modes 1 and 2 (additive and subtractive) on field polygons. Mode 2 also through `E1` in menus and battle |
| Shading | No Gouraud polygons in field frames. Battle adds Gouraud polygons and lines, the menu Gouraud semi-transparent quads |
| Primitive ordering | Ordering-table linked lists. Depth order comes from the recovered model and sprite code, as the original computes it |
| Texture windows | `E2` texture-window packets in every sampled mode |
| Drawing to the displayed buffer | Not used (`drawdisp=0`) |
| Texture upload | `LoadImage` of CLUT rows (for example `(0,251,128,1)`) and sprite cells near VRAM x 640, per frame |

These settings define the PS1-style rendering options of the native renderer:
dithering, color depth, semi-transparency modes, primitive ordering and
240-line-class resolution. Native modern rendering remains the default. Visual
references are the captured frame images of the route (private, under
`.local/scenarios`).

## Validation

Every compared frame matches its original entry and exit memory images exactly.
Under the capture policy in
[executable reconstruction](../../docs/executable-reconstruction.md), the
dialogue, walking and post-battle windows match:

- one frame at a time;
- in chains of 10 frames;
- as main-loop chains from one import with no game state supplied. The
  dialogue window runs 99 frames through a music load, walking 99 and
  post-battle 65. Only declared platform inputs are supplied: interrupt
  arrivals with their recorded hardware reads, the controller bytes the BIOS
  writes, and recorded vertical-blank and draw-sync results.

The dialogue font is disc file 2608 and every glyph drawn on the route matches
it. One glyph that the route never draws, code `0x16`, differs in RAM.

## Limits

- Paths the route does not reach stop with `MissingDependency`:
  - dialogue choices and control code `0x0f`;
  - particles and distortion effects;
  - some sprite and billboard modes;
  - the party model path `801e8670`.
- Rasterization is not modeled. A GPU pixel comparison is outside Phase 1; the
  command stream is the reconstructed output.
- The field-entry loop after battle (`80078d44`) and the map-change reload
  `800a5c40` are tracked separately in the slice manifest.
