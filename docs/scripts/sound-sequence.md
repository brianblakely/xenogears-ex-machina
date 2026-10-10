# Sound sequence VM

- Interpreter: `sound_seq_interpret_channels` (decomp/src/resident/sound.c), run per tick by
  `sound_run_tick` for every playing sequence and the effect channel set.
- Dispatch: a byte below 0x80 is a note (volume byte, encoded key k: semitone
  `sound_note_semitones[k]` = k / 19, ticks `sound_note_durations[k]` = one of
  19 lengths, 0 meaning an explicit third byte; 228 keys). 0x80-0xFF call
  `sound_seq_opcode_handlers[op - 0x80]` (128 slots: 97 handlers, 31 unused
  slots on `sound_seq_unused_opcode`). The release look-ahead steps by
  `sound_seq_opcode_lengths` instead of executing.
- Opcodes: 97 defined, plus notes. Each handler's comment in sound.c states its
  opcode, operands and effect; `tools/analysis/sound_sequence.py` holds the same
  table and checks it against both boot programs' four tables.
- Scripts: music sequences `smds` (`SoundSeqHeader`, channel offsets at +0x22;
  the driver never validates them) and effect banks `seds` (`SoundBank`, two
  channel offsets per effect at +0x20; `sound_check_file` requires a zero word
  sum and version 0x101). The install sites (`sound_add_effect_bank` for banks;
  `sound_create_seq`, `sound_create_and_play_seq` and
  `sound_create_seq_in_place` for sequences) take whole files (for example field
  music 0x14 + 2n and movie banks 0x115 + n of directory 0x1C, the field bank
  0xA8 of directory 4, the battle bank file 2 of directory 12, the menu's
  files), banks inside loaded files (battle object scripts, battle command
  files) or the boot program's error bank (0x80050910). Only the movie overlay's
  debug loader reads a sequence from elsewhere:
  `c:\work\cdrom\sound\music\battle2.smd` on the host PC.

```sh
python3 -m tools.analysis.sound_sequence --sweep          # aggregate, both discs
python3 -m tools.analysis.sound_sequence FILE [--offset N]  # list one local script
```

The sweep scans the boot programs, every file, every file that unpacks and
every offset-archive entry (of a file or of its unpacked form) that unpacks:
8961 containers on both discs. No packed archive entry holds a script; 213
sequences (63 distinct) and 1203 banks (259 distinct) are located, every
magic found is a valid script. 7053 channels, 201317 instructions; 72 opcodes
plus notes used, the same 72 on each disc; unknown or undecodable: 0. Every
channel ends in 0x90.
Unused by the data: 8a 8d 8e 8f 9c 9d 9e a1 a4 a5 a6 a7 aa b6 bc bd be c1 c3 c6
c8 ee f5 fd ff. 736 nonzero bytes (2 sequences, 36 banks) are reached by no
channel. The sweep decodes them too, outside the counts: they form 58
channels ending in 90 (unreferenced effect channels and stray ends in the
banks; in the two sequences, instructions just before a channel's start
offset), and a rest that runs off the end of one bank (disc 1 slot 3903
+0x2478) is the only leftover.

The sweep also counts the waves the modulator opcodes install from
`sound_modulator_waves[mode & 0xF]` (16 slots; 8-15 switch the modulator off; D9/E5/ED only
with a nonzero rate and depth). The distinct scripts install shapes 0-10, 12 and
15 (D9 all of these, E5 0-9, ED 0-3 and 5-7, F0 2-4, 6 and 7). F0 selects
modulators 0-3 of the channel's `modulator[4]`, none past the array.

Notes from the handlers:

- The look-ahead lengths differ from execution for 9D (4 against 3) and F5
  (2 against 1); the unused slots have length 0, so the look-ahead would never
  leave one. None occurs in the data.
- 9E continues three bytes into the target effect channel, and with no
  matching bank returns its operand pointer, so the operands run as opcodes.
- EA stores the pan difference as its target: the slide's last frame sets the
  pan to `(target - start) << 8`. A1 zeroes the per-frame tick step until a
  rate or tempo opcode or slide recomputes it.
- Mode bit 4 of D9/E5/ED/F0 clears modulator flag 2, which `sound_seq_interpret_channels`
  tests to restart a modulator on each new note. The period opcodes (D7, E3,
  EB, F2) fade a modulator in over (period + 1) * 4 frames; period 0xFF
  wraps to 0 and changes nothing.
- FC/FE fall back to the first loaded wave bank (the list head) when no wave
  bank has the key.
