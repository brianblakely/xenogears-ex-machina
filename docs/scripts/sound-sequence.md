# Sound sequence VM

- Interpreter: `func_8003C6E8` (decomp/src/resident/sound.c), run per tick by
  `func_8003C020` for every playing sequence and the effect channel set.
- Dispatch: a byte below 0x80 is a note (volume byte, encoded key k: semitone
  `D_80050A94[k]` = k / 19, ticks `D_800509B0[k]` = one of 19 lengths, 0 meaning
  an explicit third byte; 228 keys). 0x80-0xFF call `D_80050624[op - 0x80]`
  (128 slots: 97 handlers, 31 unused slots on `func_8003CD00`). The release
  look-ahead steps by `D_80050824` instead of executing.
- Opcodes: 97 defined, plus notes. Each handler's comment in sound.c states its
  opcode, operands and effect; `tools/analysis/sound_sequence.py` holds the same
  table and checks it against both boot programs' four tables.
- Scripts: music sequences `smds` (`SoundSeqHeader`, channel offsets at +0x22;
  the driver never validates them) and effect banks `seds` (`SoundBank`, two
  channel offsets per effect at +0x20; `func_8003F614` requires a zero word sum
  and version 0x101), as whole files (music directory 0x1C), inside other
  files and unpacked files, and the boot program's error bank (0x80050910).

```sh
python3 -m tools.analysis.sound_sequence --sweep          # aggregate, both discs
python3 -m tools.analysis.sound_sequence FILE [--offset N]  # list one local script
```

Sweep of both discs (7797 containers): 213 sequences (63 distinct) and 1203
banks (259 distinct) located; 7053 channels, 201317 instructions; 72 opcodes
plus notes used; unknown or undecodable: 0. Every channel ends in 0x90.
Unused by the data: 8a 8d 8e 8f 9c 9d 9e a1 a4 a5 a6 a7 aa b6 bc bd be c1 c3 c6
c8 ee f5 fd ff. 736 nonzero bytes (2 sequences, 36 banks) are reached by no
channel. The sweep decodes them too, outside the counts: they form 58
channels ending in 90 (unreferenced effect channels and stray ends in the
banks; in the two sequences, instructions just before a channel's start
offset), and a rest that runs off the end of one bank (disc 1 slot 3903
+0x2478) is the only leftover.

Notes from the handlers:

- The look-ahead lengths differ from execution for 9D (4 against 3) and F5
  (2 against 1); the unused slots have length 0, so the look-ahead would never
  leave one. None occurs in the data.
- 9E continues three bytes into the target effect channel, and with no
  matching bank returns its operand pointer, so the operands run as opcodes.
- EA stores the pan difference as its target: the slide's last frame sets the
  pan to `(target - start) << 8`. A1 zeroes the per-frame tick step until the
  next rate or tempo change.
- Mode bit 4 of D9/E5/ED/F0 clears modulator flag 2, which `func_8003C6E8`
  tests to restart a modulator on each new note.
