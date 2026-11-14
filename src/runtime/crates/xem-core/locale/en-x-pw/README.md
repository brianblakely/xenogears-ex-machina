# English (Perfect Works)

The game's text as [Xenogears: Perfect Works Build](https://github.com/PWBuild-Team/Perfect_Works_Build)
reads it: the Perfect Works Build team's retranslation of Xenogears, offered
here as the game text language **English (Perfect Works)**
(`xem_core::locale::Language::PerfectWorks`, tag `en-x-pw`).

## Licence

`battle.txt`, `field-0.txt` … `field-7.txt`, `menus.txt`, `system.txt` and
`world.txt` hold Perfect Works Build's text. They are the work of the
Perfect Works Build team and its contributors (the Credits of its README),
licensed under the GNU General Public License, version 3: see
[`LICENSE`](LICENSE) in this directory. They are distributed without any
warranty, as that licence says.

These files alone are under that licence. They are a separate work that
Xenogears: Ex Machina carries and shows; the rest of this repository,
`glyphs.txt` (the marks drawn for this project) and `same.txt` (names and
hashes of texts, no text) among it, stays under the MIT licence
(`LICENSE` at the repository root).

## Source and changes

The text is from Perfect Works Build 0.11.4 (4 October 2026): the script and
names patch of its release archive
(`Xenogears_Perfect_Works_Edition_0.11.4.7z`, `gamefiles/script1` and
`gamefiles/script2`), which ships as replacement disc files.

It was changed from that form on 13 November 2026, by
`tools/perfect_works.py` and `tools/translations.py merge` of this
repository:

- Each text table entry of those files was read beside the same entry of
  the original discs. An entry Perfect Works Build leaves as the original
  has no line here (`same.txt` names such texts by place and hash only).
- Its text is written in this project's markup, one line a text
  (`<where> <hash>[@<entry>] <text>`, `tools/translations.py`): the game's
  control codes as words in braces, and the marks Perfect Works Build's font
  adds (`* ; ö —`, a wide blank and the Hebrew El before an Omnigear's name)
  as the characters of `glyphs.txt`.
- Seventeen entries keep the original's text because their windows, which
  scripts and battle code size, are too small for Perfect Works Build's
  (nine battle messages and eight field messages; its own build lays some
  of them out anew), and two Japanese development texts were left; eight
  entries' lines were broken anew to fit the original's windows, and one
  list of choices keeps a blank line the cursor still walks.
  `tools/perfect_works.py` lists each in its report.

Perfect Works Build's other changes (event scripts, battle and menu code,
the rebalance, pictures, undubbed voices, memory card titles) are not part
of a language of the game's text and are not here.

To make the files again from a later release, see `tools/perfect_works.py`.
