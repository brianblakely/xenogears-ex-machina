"""Perfect Works' English as a language of the game's text (en-x-pw).

Xenogears: Perfect Works Build (https://github.com/PWBuild-Team/Perfect_Works_Build,
GPL-3.0) retranslates the game from the Japanese. It ships as replacement disc
files, numbered as the disc index numbers them (0606 the first field map, 0029
the system data, ...; Disc 2's index starts five entries earlier): its release
archive's gamefiles/script1 and gamefiles/script2 are the script and names
patch for each disc. Every text table in them has the original's entries, one
for one, so each entry's text there is Perfect Works' reading of the original
entry.

    python3 -m tools.perfect_works [--source DIR]   # .local/locale/en-x-pw/000.txt
    python3 -m tools.translations merge en-x-pw     # the shipped files
    python3 -m tools.translations check en-x-pw

`--source` (default .local/perfect-works) holds script1/ and script2/ as the
release archive (Xenogears_Perfect_Works_Edition_<version>.7z) has them under
gamefiles/. The tool reads them beside the user's discs (.local/discs,
.local/extract) and writes translators' lines for `tools.translations merge`:
one for each text Perfect Works reads the same wherever the text stands, and
one flagged [here] for each table entry it reads differently from the text's
other entries (tools/translations.py). A report of what it took and left goes
to <source>/report.txt. Nothing of either game's data is committed but the
translation lines `merge` writes, which hold Perfect Works' text alone.

What it takes: the text of every table entry and of the party's and Gears'
names, as Perfect Works' build shows it on the console, with the marks its
font adds (`FONT`) as the language's added glyphs. What it leaves: Perfect
Works' changes to the game itself (event scripts, battle and menu code, the
rebalance, pictures, the memory card titles), which a language of the text
does not carry. Where Perfect Works changed how a field script shows a
message (`displays`), its text is held to the window the original's script
gives it and is broken anew where it is too wide; a text that still does not
fit, or needs a glyph the game's font lacks, keeps the original's reading and
is listed in the report.
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

from tools import translations as tr
from tools.analysis import events
from tools.analysis import text_control as tc
from tools.analysis.disc_index import Disc

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / ".local/perfect-works"
LANGUAGE = "en-x-pw"
EVERYWHERE = "text"  # a line's flag: the text wherever it stands (tr.HERE: in that entry alone)
# Disc 2's index holds each file five entries before Disc 1's, and Perfect
# Works numbers both discs' files as Disc 1's.
SHIFT = {1: 0, 2: -5}

# The one-byte glyphs Perfect Works' font (file 0028) draws anew, and what
# they are: marks in place of the wide digits ０-３ (narrow but the dash),
# a blank in place of the wide underscore, and two halves of the Hebrew El in
# place of the brackets, which only the Omnigears' names use. Its full stop
# and arrows are the original's, moved or trimmed. Every other glyph a text
# uses must be the original's own image (`font_changes`).
FONT = {0x61: "*", 0x62: ";", 0x63: "ö", 0x64: "—", 0x8C: "␣"}
EL = (0x3A, 0x3B)  # together "ﭏ"
REDRAWN = {0x14, 0x72, 0x73, 0x74, 0x75}  # the same characters


class Patched(Disc):
    """A disc with Perfect Works' files in place of its own."""

    def __init__(self, number: int, source: Path):
        super().__init__(number)
        directory = source / f"script{number}"
        if not directory.is_dir():
            raise SystemExit(f"{directory} is missing: extract gamefiles/script{number} of Perfect Works' release there")
        self.files = {int(path.name.split(".")[0]) + SHIFT[number]: path for path in directory.iterdir()}
        self.entries = dict(self.entries)
        for slot, path in self.files.items():
            if slot in self.entries:
                self.entries[slot] = {**self.entries[slot], "size": path.stat().st_size}

    def sectors(self, slot: int) -> bytes:
        if slot in self.files:
            data = self.files[slot].read_bytes()
            return data + bytes(-len(data) % 2048 + 2048)  # a packed stream may read past its file
        return super().sectors(slot)

    def data(self, slot: int) -> bytes:
        return self.files[slot].read_bytes() if slot in self.files else super().data(slot)


def font_changes(disc: Disc, patched: Patched) -> set[int]:
    """The one-byte glyph codes whose images Perfect Works' font changes."""
    def glyphs(font: bytes) -> dict[int, bytes]:
        # text_install_font: halfword 1 the glyphs' offset, 6 the first one-byte glyph's code; 22 bytes a glyph.
        offset, first = struct.unpack_from("<H", font, 2)[0], struct.unpack_from("<H", font, 12)[0]
        return {code: font[offset + 22 * (code - first) :][:22] for code in range(first, 0x100)}

    slot = disc.slot(0, 1, 6)
    old, new = glyphs(tc.unpack(disc.sectors(slot))), glyphs(tc.unpack(patched.sectors(slot)))
    return {code for code in old if old[code] != new.get(code)}


def markup(raw: bytes, changed: set[int]) -> str:
    """Perfect Works' text as the language's markup. A glyph its font draws
    anew and FONT does not name stays a hex code no added glyph answers, so
    the text is refused."""
    chars = {code: char for code, char in tr.CHARS.items() if code not in changed or code in REDRAWN}
    chars.update(FONT)
    chars[EL[0]], chars[EL[1]] = "{3A}", "{3B}"
    text = tr.render(raw, chars).replace("{3A}{3B}", "ﭏ")
    for code in changed - REDRAWN - set(FONT):
        text = text.replace(f"{{{code:02X}}}", f"{{PW {code:02X}}}")
    return text


MESSAGE_FIRST = (0x03, 0xD2, 0xD3, 0xF5)  # the message is operand 0
MESSAGE_SECOND = (0xD4, 0xFC)  # operand 1


def displays(package) -> dict[int, tuple]:
    """How a map's scripts show each message, to tell where Perfect Works'
    differ from the original's: every instruction that shows it, with the
    layout instruction before it and the choice after it in the bytecode's
    order (a comparison of two scripts, not a reading of their windows:
    tools.translations.sized_windows reads those)."""
    starts, _ = events.script_entries(package)
    found = events.walk(package.bytecode, [pc for _, _, pc in starts])
    shown: dict[int, list] = defaultdict(list)
    last, layout = None, None
    for pc in sorted(found.instructions):
        instruction = found.instructions[pc]
        if instruction.extended is not None:
            continue
        opcode, operands = instruction.opcode, tuple(instruction.operands)
        if opcode in MESSAGE_FIRST or opcode in MESSAGE_SECOND:
            last = operands[0 if opcode in MESSAGE_FIRST else 1]
            shown[last].append((opcode, operands, layout))
        elif opcode == 0xA9 and last is not None:
            shown[last].append((opcode, operands, None))
        elif opcode in (0xCF, 0xD0):
            layout = (opcode, operands)
        elif opcode == 0xF4 and operands[0] != 0:
            layout = None
    return {message: tuple(sorted(map(repr, uses))) for message, uses in shown.items()}


def field_events(disc: Disc, slot: int):
    try:
        return events.map_events(disc.sectors(slot))
    except Exception:  # noqa: BLE001 - a placeholder or unreadable map has no scripts
        return None


def rescripted(disc: Disc, patched: Patched) -> dict[int, set[int] | None]:
    """Per field map whose event scripts Perfect Works changes: the messages
    its scripts show otherwise than the original's do (None: every message,
    where a script does not read)."""
    out: dict[int, set[int] | None] = {}
    first = disc.slot(4, 0, 0xB8)
    for slot in patched.files:
        if not first <= slot < first + 1460 or (slot - first) % 2 or disc.entries.get(slot, {"size": 0})["size"] < events.HEADER:
            continue
        old, new = field_events(disc, slot), field_events(patched, slot)
        if old is None or new is None:
            out[(slot - first) // 2] = None
            continue
        if old.declared == new.declared and old.entries == new.entries:
            continue
        try:
            a, b = displays(old), displays(new)
        except Exception:  # noqa: BLE001 - a script the walk does not follow
            out[(slot - first) // 2] = None
            continue
        out[(slot - first) // 2] = {message for message in set(a) | set(b) if a.get(message) != b.get(message)}
    return out


def readings(source: Path, report: list[str]) -> tuple[dict[str, str], dict[int, int], dict[int, set[int] | None]]:
    """Perfect Works' reading of every table entry it changes: `where` and
    its markup; the count of entries read on each disc; and per field map
    the messages its scripts show otherwise than the original's (None:
    every message)."""
    found: dict[str, str] = {}
    counts: Counter = Counter()
    otherwise: dict[int, set[int] | None] = {}
    for number in (1, 2):
        disc, patched = Disc(number), Patched(number, source)
        changed = font_changes(disc, patched)
        unknown = changed - REDRAWN - set(FONT) - set(EL)
        report.append(f"disc {number}: Perfect Works' font redraws {len(changed)} one-byte glyphs, "
                      f"{len([c for c in unknown if c < 0x93])} of them unnamed below 0x93")
        for map_id, messages in sorted(rescripted(disc, patched).items()):
            if messages is None:
                report.append(f"disc {number} map {map_id}: scripts not compared; every message held to the original's windows")
                otherwise[map_id] = None
            elif otherwise.get(map_id, set()) is not None:
                otherwise[map_id] = otherwise.get(map_id, set()) | messages
        originals = {(tr.PREFIX[group], item): data for group, item, data in tc.text_tables(disc)}
        for group, item, data in tc.text_tables(patched):
            old = originals.get((tr.PREFIX[group], item))
            if not isinstance(data, bytes) or not isinstance(old, bytes):
                continue
            prefix = tr.PREFIX[group]
            ours, _ = tc.text_table(old)
            theirs, _ = tc.text_table(data)
            if len(ours) != len(theirs):
                report.append(f"disc {number} {group} {item}: {len(theirs)} entries for the original's {len(ours)}; left")
                continue
            for index, (a, b) in enumerate(zip(ours, theirs)):
                counts[number] += 1
                raw, new = tr.text_bytes(old, a), tr.text_bytes(data, b)
                if raw == new:
                    continue
                where = f"{prefix}{item:03d}-{index:04d}"
                text = markup(new, changed)
                if number == 2 and found.get(where, text) != text:
                    report.append(f"{where}: the discs' files differ; Disc 1's taken")
                    continue
                found[where] = text
        ours, theirs = tr.initial_names(disc), tr.initial_names(patched)
        for slot, (raw, new) in enumerate(zip(ours, theirs)):
            if raw != new:
                found.setdefault(f"nm000-{slot:04d}", markup(new, changed))
    return found, dict(counts), otherwise


FIELD_RE = re.compile(r"f(\d{3})-(\d{4})")


def shown_otherwise(where: str, otherwise: dict[int, set[int] | None]) -> bool:
    """Whether Perfect Works' scripts show this entry otherwise than the original's."""
    match = FIELD_RE.fullmatch(where)
    if not match or int(match[1]) not in otherwise:
        return False
    messages = otherwise[int(match[1])]
    return messages is None or int(match[2]) in messages


def fit(original: tr.Text, text: str, extra, strict: bool) -> tuple[str | None, str]:
    """The reading as it fits the entry's window: itself, broken anew, or
    None with why. `strict` holds it to the window the original's script
    gives (Perfect Works' own script shows it otherwise)."""
    if strict:
        original = tr.Text(**{**vars(original), "own": (0, 0)})
    try:
        new = tr.tokens(text, extra)
    except tr.MarkupError as error:
        return None, str(error)
    if not tr.problems(original, text, extra, True):
        return text, ""
    # A choice with fewer lines than the original's, which the cursor still
    # walks (Perfect Works' window leaves them blank): they stay, blank.
    short = len(tr.pages(tr.tokens(original.markup))[-1]) - len(tr.pages(new)[-1])
    if original.choice and short > 0 and not tr.problems(original, text + "{n}" * short, extra, True):
        return text + "{n}" * short, f"{short} blank line(s) kept for the cursor"
    wrapped = tr.wrap(original, text, extra, True)
    found = tr.problems(original, wrapped, extra, True)
    return (None, "; ".join(found)) if found else (wrapped, "broken anew")


def lines(texts, found: dict[str, str], otherwise: dict[int, set[int] | None], report: list[str]) -> tuple[list[str], Counter]:
    """The translators' lines of the readings of `texts` (the originals,
    tools.translations.originals), and what became of each."""
    extra = tr.language_characters(LANGUAGE)
    entries = tr.entries_by_where(texts)
    tally: Counter = Counter()
    out = []
    for text in sorted(texts, key=lambda t: t.where):
        # Two tables alike (the world map's copy of the menu's labels) give
        # an entry of each one name: the first stands for both.
        named: dict[str, str] = {}
        for place in sorted((place for place in text.places if place.key), key=lambda place: place.where):
            named.setdefault(place.key, place.where)
        wheres = sorted(named.values())
        if not wheres:  # a menu's picture or letter row: Perfect Works' text has none
            if tr.translatable(text):
                out.append(f"{text.where} [{EVERYWHERE}] {text.markup}")
            continue
        read: dict[str, str | None] = {}
        for where in wheres:
            others = {found[place.where] for place in text.places if named.get(place.key) == where and place.where in found}
            if len(others) > 1:
                report.append(f"{where}: one entry of tables alike, read {len(others)} ways; the first table's taken")
            reading = found.get(where, min(others) if others else None)
            if reading is None:
                read[where] = None
                continue
            strict = shown_otherwise(where, otherwise)
            fitted, note = fit(tr.at_entry(*entries[where]), reading, extra, strict)
            if fitted is not None and tr.encode(fitted, extra) == tr.encode(text.markup):
                fitted, note = None, ""
            elif fitted is None:
                tally["left: " + note.split(" (")[0].split(" of ")[0][:40]] += 1
                report.append(f"{where}: left as the original ({note}): {reading}")
            elif note:
                tally[note + (" (rescripted)" if strict else "")] += 1
                report.append(f"{where}: {note}: {fitted}")
            read[where] = fitted
        kinds = Counter(read.values())
        if set(kinds) == {None}:
            tally["entries the same as the original"] += len(wheres)
            if tr.translatable(text):
                out.append(f"{text.where} [{EVERYWHERE}] {text.markup}")
            continue
        # The reading most entries share is the text's wherever it stands
        # (the first of them where two share as many), unless an entry
        # keeps the original's: then the original is, and each entry
        # Perfect Works reads anew is translated alone. A name no table
        # holds is respelled in its slot alone: the same letters a player
        # types into another name are the player's.
        everywhere = None
        if None not in kinds and text.group != tr.GROUPS["nm"]:
            everywhere = min(kinds, key=lambda kind: (-kinds[kind], min(w for w in wheres if read[w] == kind)))
            if tr.problems(text, everywhere, extra, True):
                report.append(f"{text.where}: read alike in {kinds[everywhere]} entries, but not in the tightest window of all; each entry alone")
                everywhere = None
        if everywhere is not None:
            out.append(f"{text.where} [{EVERYWHERE}] {everywhere}".rstrip(" "))
            tally["texts read anew wherever they stand"] += 1
        elif tr.translatable(text):
            out.append(f"{text.where} [{EVERYWHERE}] {text.markup}")
        for where in wheres:
            if read[where] is None:
                tally["entries the same as the original"] += 1
            elif read[where] != everywhere:
                out.append(f"{where} [{tr.HERE}] {read[where]}".rstrip(" "))
                tally["entries read anew alone"] += 1
            else:
                tally["entries read as their text is everywhere"] += 1
    return out, tally


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", type=Path, default=SOURCE, help="the directory holding script1/ and script2/")
    args = parser.parse_args(argv)
    report: list[str] = []
    found, counts, otherwise = readings(args.source, report)
    work, tally = lines(tr.originals().values(), found, otherwise, report)
    out = tr.LOCAL / LANGUAGE
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("[0-9]*.txt"):
        old.unlink()
    (out / "000.txt").write_text("".join(line + "\n" for line in work))
    summary = [f"{sum(counts.values())} table entries read ({counts.get(1, 0)} on Disc 1, {counts.get(2, 0)} on Disc 2), "
               f"{len(found)} of them Perfect Works' own", *(f"  {count:6d}  {name}" for name, count in sorted(tally.items()))]
    (args.source / "report.txt").write_text("\n".join([*summary, "", *report]) + "\n")
    print("\n".join(summary))
    print(f"{len(work)} lines written to {out / '000.txt'}; the report to {args.source / 'report.txt'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
