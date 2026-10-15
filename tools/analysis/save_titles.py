"""The characters North American Xenogears save titles hold (plan.md, Phase 6).

    python3 -m tools.analysis.save_titles

A save's title is "ＸＥＮＯＧＥＡＲＳ／Ｎｏ．", its file number (1-15) as two full-width
digits, a full-width space and 30 bytes of line vars[0] of directory (0x10, 1) file 1
(slot39 menu_save_build_header_title and menu_card_init_and_read_title;
tools/memcard_saves.py title_lines). The file screen draws each character from the BIOS's
Kanji font (menu_save_title_render, menu_save_title_find_glyph): a byte 0x20-0x7F through
menu_ascii_to_sjis_table, a byte below 0x20 as the full-width space, a two-byte code as it
is, at most 32 characters or 64 bytes. This lists every code the titles of both discs' lines
ask the font for, and checks that the runtime's substitute glyphs
(src/runtime/crates/xem-core/font/substitute.txt) have each. Inputs: the user's discs
(.local/discs, .local/extract).
"""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

from tools.analysis.disc_index import discs
from tools.memcard_saves import title_lines

ROOT = Path(__file__).resolve().parents[2]
MENU = ROOT / "src/decomp/src/slot39/menu_framework.c"
SUBSTITUTE = ROOT / "src/runtime/crates/xem-core/font/substitute.txt"
HEAD = "ＸＥＮＯＧＥＡＲＳ／Ｎｏ．".encode("shift_jis")


def ascii_table() -> list[int]:
    """menu_ascii_to_sjis_table as the recovered source holds it."""
    source = MENU.read_text()
    body = re.search(r"menu_ascii_to_sjis_table\[96\] = \{[^\n]*\n(.*?)\};", source, re.S)
    codes = [int(code, 16) for code in re.findall(r"0x[0-9A-F]{4}", body.group(1))]
    assert len(codes) == 96, "menu_ascii_to_sjis_table has 96 codes"
    return codes


def codes(title: bytes, table: list[int]) -> list[int]:
    """The glyph codes the file screen asks for, in the order it draws them."""
    out, at = [], 0
    while at < len(title) and title[at] != 0 and at < 0x40 and len(out) < 0x20:
        byte = title[at]
        if byte >= 0x80:
            out.append(byte << 8 | (title[at + 1] if at + 1 < len(title) else 0))
            at += 2
        else:
            out.append(table[byte - 0x20] if byte >= 0x20 else 0x8140)
            at += 1
    return out


def substitute_codes() -> set[int]:
    lines = SUBSTITUTE.read_text().splitlines()
    return {int(line[:4], 16) for line in lines if re.match(r"[0-9A-F]{4} ", line)}


def main() -> int:
    table = ascii_table()
    used: Counter[int] = Counter()
    for disc in discs():
        lines = title_lines(disc.number)
        for file in range(1, 16):
            number = bytes([0x82, 0x4F + file // 10, 0x82, 0x4F + file % 10, 0x81, 0x40])
            for line in lines:
                used.update(set(codes(HEAD + number + line, table)))
        print(f"disc {disc.number}: {len(lines)} title lines")
    glyphs = substitute_codes()
    for code in sorted(used):
        char = bytes([code >> 8, code & 0xFF]).decode("shift_jis", errors="replace")
        print(f"{code:04X} {char} {'' if code in glyphs else 'NO GLYPH '}in {used[code]} titles")
    missing = sorted(set(used) - glyphs)
    print(f"{len(used)} codes; the substitute font has {len(used) - len(missing)} of them")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
