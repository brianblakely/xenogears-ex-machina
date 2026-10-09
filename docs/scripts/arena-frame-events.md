# Arena move frame events

- **Interpreter:** `func_80074678` (`decomp/src/menu/menu3.c`) in the `menu` overlay
  (Disc 1 file 35, Disc 2 file 30). It is called with a frame and a count for an
  arena actor's current animation and covers count frames from that frame, once per
  frame. It is the arena counterpart of the battle animation events.
- **Data:** the gear model files, directory (0x30, 1) file `id + 2` for model ids
  0-48 (Disc 1 slots 52-100, Disc 2 slots 47-95). `func_8008509C` loads one with
  `func_800891C0`, which reads the file's size rounded up to words. `func_800852C4`
  unpacks it (80032E88), and `func_80084C88` relocates it (`func_8008AF6C`, against
  the build address at +0x1C). At +0x34 the header (+0x10) holds one s16
  header-relative list offset (`unk900`, 0 for none) per animation of the table at
  +0x08.
- **Format:** a list of 4-byte `FrameEvent {u8 first, u8 last, s16 spec}` records
  ending at first 0xFF. Each record whose range holds the frame runs the `HitSpec`
  at header + spec (`actor.h`: kind, type, part_a, part_b, s16 vertex_a, s16
  vertex_b). Kinds 1, 3, 4 and 5 read only the first 4, 1, 2 and 2 bytes, and the
  files pack them that way. Hit records take 10 bytes, of which the code reads 8.
  There are no jumps.
- **Dispatch:** a jump-table switch on the kind, six cases, each commented; kinds 0
  and 2 dispatch again on the type (`func_800740E4`, `func_80073F34`). Any other kind
  re-tests the same record forever (`continue`).

  | Kind | Handler | Operands | Effect |
  | --- | --- | --- | --- |
  | 0 hit | `func_800740E4` | type, part/vertex a and b | every frame: a sparkle trail at a, or a line trail from a to b, unless type bit 0x40 is set or the actor's `unk84[2]` is 0; while live (flag 0x4000000, from the first frame until the last or until a trail connects): 0x20 a charged shot, 4 a kind-1 shot at the opponent, 0x21-0x26 shots of kind 1-6 (away from b when the points differ), any other type a trail segment from a to b, the volume `func_80075B50` tests for hits |
  | 1 sounds | `func_8008EB88` | part_a, part_b: sound ids | both at the actor, for the first kind-1 event of a call only |
  | 2 effect | `func_80073F34` | type, part/vertex a and b | once per HitSpec in a call, up to 20: 0x10 a line trail, 0x11-0x13 bolts 0-2, 0x20-0x22 sparkle trails, 0-4 sparkles and 8-12 jittered ones. 5-7, 13-15 and 0x14-0x1F do nothing; 0x23 and up read past `D_80091228` |
  | 3 return_home | `func_80078154` | none | the actor back at its home position, idle |
  | 4 hide_part | case 4 | type: model node | set the node model's hidden flag |
  | 5 show_part | case 5 | type: model node | clear it |

- **Coverage:** 6 kinds defined, all 6 used. Each disc has 49 files, 2352 animations,
  1357 of them with a list (754 distinct lists) and 3851 events (kinds 0-5: 886, 506,
  886, 1, 627, 945). None is undecodable, no range has first > last, and no form has
  no effect or reads past a table. Kind 0 uses types 0x00-0x04, 0x10, 0x20-0x26 and
  0x40-0x43; kind 2 uses 0x00-0x02, 0x04, 0x08-0x0C, 0x10-0x12 and 0x20-0x22. The two
  discs give the same counts.
- **Reads past the file:** in 38 of the 49 files the stream reads past the bytes
  `func_800891C0` loads before its output is complete, so its last 1-7 output bytes
  come from memory after the file. The decoder keeps only the output before that
  read; no event reaches the missing bytes.
- **Unread bytes:** before the first list, the HitSpec area holds 1174 bytes in the
  two-byte tails of 587 hit records and 1077 other bytes that no event reads.
- **Tool:** `python3 -m tools.analysis.overlay_scripts --sweep` prints aggregates;
  `--list arena-events [--disc N]` prints every file's lists.
  `tests/test_overlay_scripts.py` checks the kinds against the switch cases and their
  callees, and the layouts against `actor.h`.

Open: the code never reads the tails of the hit records. The other unread bytes are
not split into records.
