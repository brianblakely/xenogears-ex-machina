# Español

The game's text as [Romhack Hispano](https://xenogears.romhackhispano.org/)'s
Spanish translation of Xenogears reads it, offered here as the game text
language **Español** (`xem_core::locale::Language::Spanish`, tag `es`).

## Credit and permission

`battle.txt`, `field-0.txt` … `field-7.txt`, `menus.txt` (but its menu
pictures' words, below), `system.txt` and `world.txt` hold Romhack
Hispano's text: the work of Romhack Hispano's Xenogears translation team
(the credits of its site and its patches' documentation). **Its use here
awaits their permission**, requested from the translation's contact on 17
November 2026: until they grant it, and say under which terms and with
which credit, these files are not to be merged or released.

These files alone are their work. The rest of this directory —
`glyphs.txt` (the marks drawn for this project), `same.txt` (names and
hashes of texts, no text) and the menu pictures' words of `menus.txt`
(`mc` and `mr` lines, written for this project in the translation's terms,
`tools/romhack_hispano.py` LABELS) — stays under the MIT licence of the
repository (`LICENSE` at its root).

## Source and changes

The text is from Romhack Hispano's translation, version 1: its two xdelta
patches (`parche_CD1.xdt` and `parche_CD2.xdt`, with or without subtitled
movies) applied to the North American discs' Redump images. It was
changed from that form by `tools/romhack_hispano.py` and
`tools/translations.py merge` of this repository:

- Each text table entry of the patched discs was read beside the same
  entry of the original discs; every table keeps the original's entries
  one for one. An entry the translation leaves as the original has no
  line here (`same.txt` names such texts by place and hash only).
- Its text is written in this project's markup, one line a text
  (`<where> <hash>[@<entry>] <text>`): the game's control codes as words in
  braces, and the letters the patch's font draws over the original's
  full-width digits and symbols (á é í ó ú Á É Í Ú ñ ü ¡ ¿ ;) as those
  characters, which `glyphs.txt` composes from the player's own font. The
  symbols the patch moves (arrows, buttons, the ellipsis) are the
  original's, and a glyph its font blanks is a space, as its build shows.
- Where one text reads differently on the two discs (935 entries: mostly
  capitals and small rewordings), Disc 1's reading is taken.
- Thirteen entries keep the original's text because the windows their
  scripts size are too small for the translation, and five because they
  use pictures the patch draws in its font for its menus; the few button
  names the patch left at the original's ○ and △ codes, which its own build shows
  as letters, show the buttons here.

The patch's other changes (its menu textures, subtitled movies and
pictures) are not part of a language of the game's text and are not here.

To make the files again: see `tools/romhack_hispano.py`.
