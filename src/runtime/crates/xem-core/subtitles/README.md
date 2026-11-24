# Movie subtitles

The subtitles the movies show when the player turns on **Movie subtitles**
(`xem_core::subtitles`, docs/runtime.md, Subtitles): which lines show over
which frames of which movies (`timings.txt`), and their text in each
language of the game's text (`<language>.txt`).

## Licence

`timings.txt`, `en-x-pw.txt` and `en.txt`, and `fr.txt`, `it.txt`, `es.txt`,
`pt-BR.txt` and `ja.txt`, translated from them, hold the movie subtitles of
[Xenogears: Perfect Works Build](https://github.com/PWBuild-Team/Perfect_Works_Build):
the work of the Perfect Works Build team and its contributors (the Credits
of its README), licensed under the GNU General Public License, version 3:
see [`LICENSE`](LICENSE) in this directory. They are distributed without any
warranty, as that licence says.

These files alone are under that licence. The rest of this repository,
`ja-font.txt` (the Japanese font's codes of characters) and this README
among it, stays under the MIT licence (`LICENSE` at the repository root).

## Source and changes

The subtitles are Perfect Works Build 0.11.4's (4 October 2026, tag
`0.11.4-stable`), the subtitles of its movies with the Japanese voices (its
undub option): their timings from the tables its movie player keeps in its
executables (`in_game/sub_executable/disc1/SLUS_006.64` and
`disc2/SLUS_006.69`), their text from the subtitle pictures it shows
(`in_game/fmv1/*.str`, `in_game/fmv2/*.str`).

They were changed from that form for this project on 24 November 2026:

- Each line is named by its movie, the movie's file in directory (0x18, 1)
  (Perfect Works numbers the file's index slot, one less), with the frames
  it shows on; its text is written in the markup of the game's text, its
  line breaks as Perfect Works' pictures break it.
- `en.txt` is Perfect Works' text with the names the original's English
  gives (Krelian for Karellen); `en-x-pw.txt` keeps Perfect Works' own.
- `fr.txt`, `it.txt`, `es.txt`, `pt-BR.txt` and `ja.txt` are translations of
  `en.txt` made for this project. `ja.txt`'s line 1.15.5 writes its numbers
  in digits, as the Japanese release's font has no 億 or 塩.
