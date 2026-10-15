# Substitute Kanji glyphs

`substitute.txt` holds the glyphs the runtime places in the BIOS's Kanji
font when the player has no BIOS image (`src/bios.rs`): 68 cells of 16x15
pixels, one bit each, the font's own layout, keyed by Shift-JIS code. The
game's file screens draw each save's title from that font
(`src/decomp/src/slot39/menu_member_screens.c`, `menu_save_title_render`
through `Krom2RawAdd`), so without a BIOS the titles would be blank. These
glyphs are a substitute, not the BIOS's font: they are drawn differently, and
every character they do not cover stays blank.

- **Licence.** Drawn by hand for Xenogears Ex Machina and licensed with it
  under the MIT licence (`LICENSE` at the repository root). No glyph is taken
  from a BIOS image or from another font.
- **What they cover.** A North American save's title is
  "ＸＥＮＯＧＥＡＲＳ／Ｎｏ．", the file number as two full-width digits, a
  full-width space and the first 30 bytes of the line its story value picks
  from directory (0x10, 1) file 1 (`menu_save_build_header_title`,
  `menu_card_init_and_read_title`). Every line of that file is full-width
  text on both discs (355 lines each). `python3 -m tools.analysis.save_titles`
  reads both discs and lists the 61 codes the titles of every line and file
  number ask the font for, and checks that each has a glyph here: the
  full-width space (8140), the digits, 45 of the 52 Latin letters, and five
  punctuation marks: ． (8144), ！ (8149), ／ (815E), ’ (8166) and − (817C).
  The seven letters no line uses (Ｊ Ｑ Ｕ Ｚ ｊ ｑ ｘ) are drawn too, so the
  set is the whole full-width alphabet, the digits, the space and those five
  marks.
- **What they do not cover.** Other games' saves on the same card may use
  other characters, kanji and kana among them; those stay blank without a
  BIOS. So would the staff roll (directory 4 file 0xAB, drawn by
  `field_staff_roll_write_line`), which asks the font for 575 characters,
  most of them kanji in the staff's names, though no shipped script enables
  it (`docs/original-boundaries.md`, Services: BIOS Kanji ROM).
- **Layout.** Each glyph is a line with its code in hexadecimal and the
  character, then 15 rows of 16 characters, `#` a set pixel. In the font a
  row is two bytes, the left eight pixels first, the leftmost pixel the
  high bit (as `menu_save_title_render` reads them). The file screen merges
  columns 1-2, 5-6, 9-10 and 13-14 (`menu_save_title_narrow_glyph`), so
  strokes are placed to survive that, and it reads a sixteenth row from the
  next glyph's first, which every glyph here leaves clear. Capitals and
  digits stand in rows 2-12, small letters' bodies in rows 5-12 with
  descenders to row 14.
