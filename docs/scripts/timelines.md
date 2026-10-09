# Cue timelines

Readers that step through (time, value) entries embedded in an overlay's data
and act on each entry in turn, without dispatching on it. The tables are
user-supplied: the units link them from the user's image (`INCLUDE_ASSET`;
`asset` lines in the targets' classification files), and
`python3 -m tools.analysis.overlay_scripts` decodes them from each disc's own
packed overlay.

## Field movie sound timelines

- **Reader:** `func_80085678` in the `field` overlay (Disc 1 file 36, Disc 2 file
  31). The field movie player `func_800A7C58` seeks the timeline with
  `func_80085788` before the movie starts and runs `func_80085678` after each
  movie step (`func_800A732C`); at the end `func_80085738` releases the bank.
- **Table:** `D_800AE060`, 96 u16 (frame, sound) pairs: a leading end, then one run
  per movie sound-effect bank, each ended by an entry whose frame is 0xFFFF (sound
  0). The asset line is in `decomp/targets/overlays/field.classification.txt`.
- **Bank:** event `fe a0` (`func_8008EA58`) requests a movie with its sound bank
  in operand 9; 0xFF requests none, as events 60 and 67 always do, and then
  `func_80085678` plays nothing. `func_80085788` loads file 0x115 + bank of
  directory (0x1C, 0), adds it to the open effect banks (`func_80038428`) and
  leaves the position `D_800C3A64` past bank + 1 ends.
- **Timing:** the movie's frame callback (`func_800A7120`) stores the frame in
  `D_800B06A0`. `func_80085678` plays, in table order, every entry whose frame
  plus the movie's sound start (`FIELD_MOVIE.sound_start`, event `fe a0`'s operand
  5) the movie frame has reached, several in one call when they are due together:
  the loaded bank's effect in the low byte of the sound, on the voice pair in bits
  8-10 (`func_80039EC4` gets pair * 2). Neither routine tests the end itself:
  frame 0xFFFF lies past every movie.
- **Coverage:** both discs hold the same table: 10 runs, 85 entries, each run's
  frames in order and no sound with bits 11-15 set. Banks 0-9 are files
  0x115-0x11E (Disc 1 slots 392-401, Disc 2 slots 387-396), each a "seds" bank
  that `func_8003F614` accepts, and each run plays every effect of its bank
  except effect 0 exactly once. The field event scripts request each of banks
  0-9 with `fe a0` (0-3 once, 4-9 three times), no bank 11 times on Disc 1 and 10
  on Disc 2, and once a bank held in a variable.
- **Tool:** `--sweep` prints these aggregates; `--list movie-sounds [--disc N]`
  prints each bank's run. `tests/test_overlay_scripts.py` checks the decoder
  against `func_80085788`, `func_80085678` and `func_8008EA58`, and that the table
  stays an asset.
