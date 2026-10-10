# Sprite animation scripts

These byte scripts drive every sprite and sprite-driven model: their frames and
timing, motion, placement, colour and child sprites, and in battle also effects and
camera cues. Each case in the recovered handlers carries a comment with its
semantics. [sprite_vm.py](../../tools/analysis/sprite_vm.py) decodes the scripts
with opcode tables read from those cases, and its tests check the tables against
the `decomp/src` switch labels and the C length table.

- **Interpreters:** resident `sprite_vm_run` (`sprite_vm_draw.c`) runs commands
  until one takes time. Resident `sprite_vm_tick` calls it once the halfword countdown
  (`+9e`) reaches 0, ticking skip + 1 times per frame. While `sprite_in_battle` is set
  (battle `800B8840` to `800B8774`), it hands every sprite to the battle overlay's
  copy `func_800C11CC` (`battle_800C11CC.c`). Resident `sprite_vm_replay_frames`
  (`sprite_construction.c`) replays frame commands untimed when the facing group changes.
- **Dispatch:** 800248d4's switch table is at `800186e0` (80-fa) and 800c11cc's at
  `80070c14` (80-fb, then the 10 `f8` conditions). Both fall back to the generic
  commands `sprite_vm_run_generic_command` (`sprite.c`, 8a-fc), which `c8` also runs and which holds
  the 39 `bc` selectors. `func_800B3F04` (`battle_800B3F04.c`, table `80070850`)
  implements battle commands 01-6b (all but 39) for `c3`, `ec`, `f9` and `e8`.
- **Format:** a command is one opcode byte followed by little-endian operands.
  - `00`-`7f` take one byte and show a frame: `00`-`0f` the next frame, `10`-`1f`
    the next frame-table entry, `20`-`2f` the previous frame, and `30`-`3f` keep
    the frame. Each waits `(op & f) + 1` frames scaled by the sprite's divisor.
    `40`-`7f` set no duration; the original reads a stale register.
  - A handler at `80`-`ff` that keeps the script pointer then advances it by the
    resident length table `sprite_vm_command_lengths[op - 0x80]` (`sprite_construction.c`; the battle
    copy reads the same bytes as `sprite_vm_command_lengths_by_opcode[op]`): `80`-`9f` 1, `a0`-`c7` 2,
    `c8`-`f0` 3, `f1`-`ff` 4.
  - Jumps (`d4`, `e1`, `e2`, `e4`, `fa`, battle `f8` and `fb`), `cc` and `e3` count
    from the command's first byte. `e0`, `ca`, `cb` and the 24-bit offsets (`f3`,
    `f5`-`f7`, `fc`) count from the operand bytes; battle command data count from
    the command's argument bytes.
  - A variable byte (`8001fba4`) names a byte of the sprite's table (`+88`) when
    bit 7 is set, else a byte of its stack (`+8e`, at the stack index `+8c` plus the
    signed byte). `e2`/`85` call and return through 3-byte stack entries, and
    `b4`/`e4` loop on a pushed count.
- **Data:** a sprite resource block is an offset table: the section count n, n
  section offsets, then the size, so section 1 starts at `8 + 4n`.
  - `80022224` reads only sections 1-3: the animations, the frame directory and the
    palette. Most blocks have three sections. The battle command files have 4-6:
    directory 14 file `0x22 + 2n` (`800b7c34`) and directory 45 files 18-30
    (`800bf2b8`), as does one enemy set file. `800c0fac` loads the extra sections
    as sound banks (`seds`, `wds `) or as further blocks.
  - Section 1 starts with a directory halfword. Its bits 0-5 are the animation
    count, which `8002435c` uses to find the `be` frame map (`+60`) after the
    offsets. Bits 6-11 go to `800591b3` in battle, and bits 12-13 of a command file
    say which gears restart (`800b7c34`). Next come the header offsets, the frame
    map, the headers and the commands.
  - A header (`80023538`) holds flags, then the command offset and 1, 3 or 5
    frame-table offsets, each counted from its own halfword. The flag bits are 0-1
    the facing groups (one, four or eight, `800223b0`), 2-7 a signed weight, and
    11-13 to keep the speeds, angles and scale.
- **Scripts:** `800245d8` starts directory animation n of the bound block (a
  negative n: ~n of the alternate block `+4c`). `80023b84` starts a child at a
  header: `e0` does, as does `bd` with the shared block (in battle, directory 44
  file 2, bound by ovl2615 `801e6e48`), and so do battle runners. `e3` starts a header on the
  partner, and `fb` resumes at its target. The tool follows all of these from every
  directory entry.
  - The sweep reads the field archives' sprite bundle (component 3), every numbered
    file raw and unpacked, and the packed entries of offset tables.
  - 800248d4 runs the blocks in directories 4 and 36 (field and world map).
    800c11cc runs those the battle overlay reads (13, 14, 32, 42, 44 and 45,
    through `cd_select_directory` groups `0c`, `10`, `20`, `28` and `2c`). The boot
    executables hold none.
- **Coverage** (both discs, 2026-10-08):

  | | Field (`800248d4`) | Battle (`800c11cc`) |
  | --- | --- | --- |
  | Blocks (distinct) | 4713 (550), all 3 sections | 1196 (372): 784, 268, 120, 24 with 3-6 sections |
  | Animations | 23084 | 11606 |
  | Scripts, all blocks (distinct blocks) | 17852 (1626) | 35334 (9976) |
  | Commands decoded (distinct blocks) | 133032 (9699) | 576388 (165618) |
  | Opcodes with a handler: defined / used | 222 / 75 | 246 / 133 |
  | Used without a handler | `84` (42 uses in 4 distinct blocks) | none |
  | Undecodable | 0 | 0 |

  Battle data use 96 of the 106 battle commands, 51 `bc` selector forms and 22 `c8`
  commands. Field data use three `bc` selectors and no `c8`, and no data use
  `40`-`7f`. `84` sits in walking loops; with no handler, it advances one byte and
  does nothing.
- **Not started:** in 189 distinct battle blocks, 903 headers lie in section 1 that
  no directory lists and no command spawns, and no starter the sweep follows (above)
  reaches them. Their scripts add 9527 commands, none undecodable, though some show
  `40`-`7f`, `b1` and `ff`. Data can take a header's shape, so the sweep reports
  these apart.
- **Tool:** `python3 -m tools.analysis.sprite_vm --sweep [--listing .local/sprite-vm]`
  reads `.local/extract` and `.local/discs` and prints aggregates only. It checks
  the length table against each disc's resident. `--listing` writes one listing per
  distinct block and only under `.local/`.

## Pitfalls

- `be` is three bytes. Both interpreters advance by 3, but its length entry is 2 and
  the replay `80022660` advances by 2.
- The replay runs only frames, `b3`, `be` and `e2`. It steps over every other
  command by its length, including `e1`, `e4` and `85`, so after a call it reads on
  linearly.
- `a7` waits `n + 2` frames in the interpreters but `n + 1` through `c8`.
- Battle commands `50` and `67` retry by moving back two bytes, so only their `c3`
  form, the one the data use, repeats itself. `c3` has no argument bytes of its own:
  a command taking arguments would read the next command's bytes (none does).
- The same opcode can mean different things per interpreter. Battle `a4` replaces
  the generic one. `88`, `89`, `8b`, `8f`, `95`, `97`, `99`-`9f`, `c2`, `c3`, `ca`,
  `cb`, `e3`, `e8`, `ec`, `f3`, `f8`, `f9` and `fb` have no handler outside battle;
  there `8f` does not even stop the script.
- Analysis references: [sprite animation](../../analysis/formats/sprite-animation.md)
  (replay, facing) and [construction](../../analysis/formats/sprite-construction.md).
