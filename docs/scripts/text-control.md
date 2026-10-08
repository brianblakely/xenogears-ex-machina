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
- Tables: every text the loaders pass to `func_80033728` is a table of a
  count, 0, count + 1 offsets and count (columns, rows) pairs: field messages
  (map bundle component 7, D_800ADBF0), system data (`MES SYSDATA`, except the
  character pairs of resource 27), menu labels and menu data (directory 0x10
  files 1 and 2), menu mode text, the battle archive, enemy data, battle menu
  files, battle event scripts (ovl3087) and world map areas.

```sh
python3 -m tools.analysis.text_control --sweep    # aggregate, both discs
```

Sweep of both discs: 1373 tables (525 placeholder map files skipped), 41969
texts, 1957022 tokens; 13 controls used; unknown or undecodable: 0. Unused by
the data: 0F 03, 0F 06, 0F 07, 0F 08, 0F 09, 0F 0B, 0F 0F. 1886 nonzero bytes
in 32 tables are reached by no entry (text left after a terminator).

Notes from the handler:

- Inserting controls set the resume pointer to their last operand; the
  inserted text's 00 resumes after it. 0F 0B instead stops on its operand,
  which is then read again as text; 0F 04 with an unknown selection kind reads
  its 04 as a glyph.
- 0F 02 and 0F 0D end the window's current text after their wait (the next
  step clears window flag 4); 800345e0 drops the wait of 0F 0D.
- Disc 1 map 145's packed message stream reads two bytes past its file; two
  different continuations give the same table, the bytes they change lying
  past its logical size.
