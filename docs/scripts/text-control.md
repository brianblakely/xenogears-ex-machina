# Text control codes

- Interpreter: `window_reveal_text` (decomp/src/resident/main2.c), the reveal step
  of message windows (`window_draw_frame`) and one-line layouts (`window_render_text_line`).
- Dispatch: bytes 00, 01, 02, 03 and 0F are controls; 0F takes a sub-code
  through the 16-entry jump table at 0x80018A7C (cases 0-15). Other bytes are
  glyphs, two bytes long from the font's threshold `text_font_two_byte_threshold` (font halfword
  2, 0xFE on both discs) up.
- Controls: 20 defined (4 bytes, 16 sub-codes), each commented in
  `window_reveal_text` and listed in `tools/analysis/text_control.py`. A sub-code of
  16 or more matches no case and leaves the pointer on the 0F: the window
  stalls, so the parser reports it as undecodable.
- Sources: windows get text only through `window_queue_message` (queue),
  `window_render_text_line` (one-line layout) and the insertions (`window_insert_text`). The
  122 calls in decomp/src pass entries of the tables below or text built at
  run time; no function still linked as assembly calls them.
- Tables (a count, 0, count + 1 offsets and count (columns, rows) pairs, read
  by `text_get_resource_entry`): field messages (map bundle component 7, D_800ADBF0),
  system data (`MES SYSDATA`, except the character pairs of resource 27), menu
  labels and menu data (directory 0x10 files 1 and 2; the world map loads the
  menu resources from its own file 0x26, byte-identical to file 1 on both
  discs), menu mode text, the battle archive, enemy data, battle menu files,
  battle event scripts (ovl3087) and world map areas.
- Run-time text (numbers, names, name entry, saved names) comes from
  character codes through the byte pairs of system resource 27
  (`text_decode_codes_to_buffer`/`text_decode_codes`). Of its 0x144 codes, 95 give a one-byte
  glyph and 229 an empty pair that writes 00 and ends the text, so such text
  holds only glyphs. The initial names (directory 0x10 file 3, decoded by
  `mode_load_initial_game_data`) use codes inside the table.

```sh
python3 -m tools.analysis.text_control --sweep    # aggregate, both discs
python3 -m tools.analysis.text_control --list field --item 2 --disc 1 > .local/text-field-2.txt
python3 -m tools.analysis.text_control --list field --item 2 --disc 1 --chars > .local/text-field-2-chars.txt
```

`--list GROUP` (system, field, menu-labels, worldmap-labels, menu-data, menu-mode,
battle-archive, enemy, battle-menu, battle-events, world-areas) prints every table of
the group, or with `--item` one table (a field map's number, an archive entry or a
file, as the sweep names them): each entry's offset, (columns, rows) and tokens, with
glyphs as hex codes and controls by mnemonic, then the texts no entry reaches.
Listings stay under `.local/`.

`--chars` prints the glyphs whose characters the disc's own data gives as those
characters and every other glyph as its hex code in braces (`{58}Whoa{57}`). A line
at the top names the characters. The tool holds no font map; `character_map` reads
it from three sources:

- The number code. `text_format_number` writes a number's digits as the codes
  palette × 16 + digit and its sign as palette × 16 + 10 (negative) or + 11. The
  window controls pass palettes 0 and 1. The menus write the blank of a number's
  leading zeros as code 0xC3 (slot39 `func_801DC3D8`, ovl2601 `func_801CDD14`,
  ovl2602 `func_801D1304`), which the name entry also enters for an empty cell
  (ovl2600 `func_801CB33C`). Resource 27 gives these codes 25 one-byte glyphs on
  both discs: 0x61-0x6A and 0x16-0x1F the digits, 0x7E and 0x13 the minus sign,
  0x7D and 0x11 the plus sign, 0x10 the blank.
- The memory card titles, directory (0x10, 1) file 1. `func_801C6400` copies line
  `D_8006EF64` (30 bytes of two-byte Shift-JIS) into the save header. The menu
  turns ASCII into Shift-JIS through its table `D_801EA610` (`func_801E65E4`), so
  the inverse of that table reads the titles as text: 68 distinct ones. A title
  with known and unknown characters names the glyphs of the one whole text that
  fits it, if exactly one does. Such a text has the same length, the known glyphs
  at their characters, and the other characters at glyphs not yet known, equal
  characters at equal glyphs and different ones apart. Each round keeps the
  titles found in every largest set of findings that agree (no glyph read as two
  characters, no character with two glyphs) and repeats with the glyphs they add.
  Agreement matters: in the first round "Night Purge" fits exactly one whole text
  (entry 10 of enemy data file 110), and the largest agreeing set is the round's
  three other titles, so it adds nothing. Five titles align, all as world area
  names (Lahan Village, Mountain Path, Road to Nisan, Babel Tower, Dazil), and
  give 25 letters: B D L M N P R T V and a b d e g h i l n o r s t u w z. Night
  Purge is the only other title that ever fits exactly one whole text.
- The name entry grid (ovl2600 `D_801CBEC0`): `func_801CA558` shows its 36
  entries of five codes in four columns of nine, so screen row r shows entries
  r, r + 9, r + 18 and r + 27. In that order it holds two runs of 26 ascending
  codes, 0x20-0x39 across the first two rows and 0x3D-0x56 across the next two,
  and resource 27 gives those codes the one-byte glyphs of the same numbers.
  Every letter the titles give sits at its own place in the alphabet within one
  of them, the nine capitals in the first (B at 0x21, V at 0x35) and the sixteen
  small letters in the second (a at 0x3D, z at 0x56), so `grid_letters` reads
  each run as an alphabet and names the 27 glyphs left (17 capitals, 10 small
  letters). That is an inference
  from the grid's order, which the 25 placed letters confirm; a run adds nothing
  where a known letter is out of place, the known letters are fewer than two or
  of both cases, or a letter it would add has a glyph already.

On both discs this gives 77 glyphs: the number code's 25, the titles' 25 letters
and the grid's other 27. The listings spell words (`Fei[newline]{58}Whoa{57}`),
while every punctuation glyph and every two-byte glyph stays hex.

Open: punctuation and the two-byte glyphs. The grid's other codes, besides its
digits 0x10-0x19, have no order that names their characters, and no data maps a
two-byte glyph to one.

Sweep of both discs: 1375 tables (805 distinct; 525 placeholder map files
skipped), 42367 texts, 1961794 tokens; 13 controls used (12 on disc 2: 0F 0D
occurs only on disc 1); unknown or undecodable: 0. Unused by the data: 0F 03, 0F 06, 0F 07, 0F 08, 0F 09, 0F 0B,
0F 0F. 1886 nonzero bytes in 32 tables are reached by no entry. The sweep
decodes them too, outside the counts: they form 37 whole texts ending in 00
(messages no entry shows). The 62 initial names decode as 392 glyphs. The
sweep also reports each disc's `--chars` glyphs (above: 77 on each disc).

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

## Staff-roll text (field file 0xAB)

- Reader: field `func_800AC0F0` (decomp/src/field/field_800A9274.c) draws one
  line into VRAM row n & 15 at x 0x300. `func_800ACCF4` calls it on every
  16th pass of the movie loop in `func_800A7948` (movie frames 0x687-0x18E1),
  and `func_800AC99C` scrolls the sixteen rows up a pixel each pass.
- Format: lines of up to 28 big-endian two-byte glyph codes, each ending at a
  CR (0x0D) that the line consumes. The CR test comes before each code, so a
  line of 28 codes leaves its CR to the next line, an empty one. Codes
  8540-887F are cells of the font image (file 0xAC at (380, 100), seven 9x16
  cells a row; `func_800ABFDC`); any other code goes to the BIOS kanji ROM
  (`Krom2RawAdd`, whose PsyQ prototype takes a Shift-JIS code). CR is the only
  control. Once the file's bytes are used up (`D_800AF780`), every further
  line is blank.
- Source: directory (4, 0) files 0xAB and 0xAC, loaded by `func_800AC308`
  from `func_800ACC58` when `mode_staff_roll_enabled` is set. Only field ext `be`
  (`enable_movie_overlay`, `func_80087C0C`) sets it, and no reachable field
  script on either disc uses that event (`python3 -m tools.analysis.events
  --sweep`).

```sh
python3 -m tools.analysis.staff_roll --sweep    # aggregate, both discs
```

Sweep of both discs: one text each, byte-identical (9522 bytes). Each decodes
as 571 lines: 570 end at a CR and one stops at 28 codes. 155 lines are empty.
The 4476 glyph codes are 90 font cells (85 distinct, highest 94, inside the
112 cells of file 0xAC's 64x256 image) and 4386 kanji ROM codes (575
distinct). Unknown or undecodable: 0. `tests/test_staff_roll.py` checks the
reader against the C.
