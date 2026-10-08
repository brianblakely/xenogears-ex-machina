# Text control codes

- Interpreter: `func_80033DF0` (decomp/src/resident/main2.c), the reveal step
  of message windows (`func_80034888`) and one-line layouts (`func_80034EAC`).
- Dispatch: bytes 00, 01, 02, 03 and 0F are controls; 0F takes a sub-code
  through the 16-entry jump table at 0x80018A7C (cases 0-15). Other bytes are
  glyphs, two bytes long from the font's threshold `D_8005934C` (font halfword
  2, 0xFE on both discs) up.
- Controls: 20 defined (4 bytes, 16 sub-codes), each commented in
  `func_80033DF0` and listed in `tools/analysis/text_control.py`. A sub-code of
  16 or more matches no case and leaves the pointer on the 0F: the window
  stalls, so the parser reports it as undecodable.
- Sources: windows get text only through `func_80034714` (queue),
  `func_80034EAC` (one-line layout) and the insertions (`func_80033DD4`). The
  122 calls in decomp/src pass entries of the tables below or text built at
  run time; no function still linked as assembly calls them.
- Tables (a count, 0, count + 1 offsets and count (columns, rows) pairs, read
  by `func_80033728`): field messages (map bundle component 7, D_800ADBF0),
  system data (`MES SYSDATA`, except the character pairs of resource 27), menu
  labels and menu data (directory 0x10 files 1 and 2; the world map loads the
  menu resources from its own file 0x26, byte-identical to file 1 on both
  discs), menu mode text, the battle archive, enemy data, battle menu files,
  battle event scripts (ovl3087) and world map areas.
- Run-time text (numbers, names, name entry, saved names) comes from
  character codes through the byte pairs of system resource 27
  (`func_80033ABC`/`func_80033B34`). Of its 0x144 codes, 95 give a one-byte
  glyph and 229 an empty pair that writes 00 and ends the text, so such text
  holds only glyphs. The initial names (directory 0x10 file 3, decoded by
  `func_8001B970`) use codes inside the table.

```sh
python3 -m tools.analysis.text_control --sweep    # aggregate, both discs
```

Sweep of both discs: 1375 tables (805 distinct; 525 placeholder map files
skipped), 42367 texts, 1961794 tokens; 13 controls used (12 on disc 2: 0F 0D
occurs only on disc 1); unknown or undecodable: 0. Unused by the data: 0F 03, 0F 06, 0F 07, 0F 08, 0F 09, 0F 0B,
0F 0F. 1886 nonzero bytes in 32 tables are reached by no entry. The sweep
decodes them too, outside the counts: they form 37 whole texts ending in 00
(messages no entry shows). The 62 initial names decode as 392 glyphs.

Notes from the handler:

- Inserting controls set the resume pointer to their last operand; the
  inserted text's 00 resumes after it. 0F 0B instead stops on its operand,
  which is then read again as text; 0F 04 with an unknown selection kind reads
  its 04 as a glyph.
- 00 (outside an insertion), 0F 02 and 0F 0D end the window's current text:
  the next step only clears window flag 4, so the window moves on to its next
  queued text; 800345e0 drops the wait of 0F 0D.
- Disc 1 map 145's packed message stream reads two bytes past its file; two
  different continuations give the same table, the bytes they change lying
  past its logical size.
