"""Romhack Hispano's Spanish as a language of the game's text (es).

Romhack Hispano's translation (https://xenogears.romhackhispano.org/,
version 1) ships as two xdelta 1.1 patches, one for each North American
disc, applied to the whole raw image (the user's Redump dump, 718 738 272
and 688 700 880 bytes). The patched images keep every file of the disc
index, moved and resized, and every text table keeps the original's
entries one for one, so each entry's text there is Romhack Hispano's
reading of the original entry.

    xdelta patch parche_CD1.xdt "Xenogears (USA) (Disc 1).bin" .local/romhack-hispano/disc1.bin
    xdelta patch parche_CD2.xdt "Xenogears (USA) (Disc 2).bin" .local/romhack-hispano/disc2.bin
    python3 -m tools.romhack_hispano [--source DIR]   # .local/locale/es/000.txt
    python3 -m tools.translations merge es             # the shipped files
    python3 -m tools.translations check es

`--source` (default .local/romhack-hispano) holds the two patched images
(either patch, with or without its subtitled movies: the text is the same).
The tool extracts their files beside them (tools/extraction/disc_files.py)
and reads them beside the user's discs (.local/discs, .local/extract),
writing translators' lines for `tools.translations merge` as
tools/perfect_works.py does. A report of what it took and left goes to
<source>/report.txt. Nothing of either game's data is committed but the
translation lines `merge` writes, which hold Romhack Hispano's text alone.

The patch's font (file 0028) draws its accented letters and inverted marks
over glyphs the original uses for full-width digits and symbols, and moves
the symbols its texts still show (arrows, buttons, the ellipsis) to codes
the original leaves to kana. `characters` reads each code the patch
changes: a glyph it moved is the original's character where its image
now stands, a letter it draws anew is named in LETTERS, and a glyph it
blanks is a space, as its build shows it. Texts of the development data
written in kana keep the original's reading of every code.
"""

from __future__ import annotations

import argparse
import json
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

from tools import perfect_works as pw
from tools import translations as tr
from tools.analysis import events
from tools.analysis import text_control as tc
from tools.analysis.disc_index import Disc

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / ".local/romhack-hispano"
LANGUAGE = "es"
EVERYWHERE = pw.EVERYWHERE

# The one-byte glyphs Romhack Hispano's font draws anew, and what they are.
# 0x14, 0x5E, 0x5F and 0x60 are the original's full stop, parenthesis,
# colon and question mark redrawn; the others are its letters, drawn over
# the original's full-width digits and symbols.
LETTERS = {
    0x14: ".", 0x5E: ")", 0x5F: ":", 0x60: "?",
    0x62: "á", 0x64: "Á", 0x65: ";", 0x67: "é", 0x69: "É", 0x6B: "í", 0x6C: "¡", 0x6D: "Í",
    0x6F: "ó", 0x73: "¿", 0x75: "ú", 0x76: "ü", 0x77: "Ú", 0x79: "ñ",
    0xC2: "…", 0xD1: "×", 0xD5: "△",
}
# Codes the patch's font draws anew whose texts still mean the original's
# symbol: a few button names were left at the original's ○ and △, which
# the patch's own build shows as letters.
ORIGINAL = {0x6E, 0x70}


# The menus' pictures of words (their command labels and stat panels'
# words) and the letters they place one by one, which the patch redraws
# in its textures and so has no text for: written for this project in the
# patch's terms (Estado, Objetos, Habilidades, Técnicas, Piezas, Ether ...).
# They are this project's (MIT), not Romhack Hispano's.
LABELS = {
    "mc222-0000": "Ataque", "mc223-0000": "Pre%", "mc224-0000": "Defensa", "mc225-0000": "Esq%",
    "mc226-0000": "Ether", "mc228-0000": "DefEth", "mc230-0000": "Peso", "mc235-0000": "Agilid.",
    "mc243-0000": "Defensa", "mc244-0000": "DefEth", "mc245-0000": "comb", "mc246-0000": "Resp.",
    "mc247-0000": "AmpEth", "mc248-0000": "CalMax",
    "mc291-0000": "Nueva partida", "mc292-0000": "Continuar", "mc293-0000": "Sonido", "mc294-0000": "Salir",
    "mc295-0000": "Guardar", "mc296-0000": "Cargar", "mc297-0000": "Copiar", "mc298-0000": "Borrar",
    "mc299-0000": "Estado", "mc300-0000": "Equipar", "mc301-0000": "Objetos", "mc302-0000": "Habilidades",
    "mc303-0000": "Gear", "mc304-0000": "Archivo", "mc305-0000": "Salir", "mc306-0000": "Técnicas",
    "mc307-0000": "Habilidades", "mc308-0000": "Equipar", "mc309-0000": "Comprar", "mc310-0000": "Vender",
    "mc312-0000": "Objetos", "mc313-0000": "Armas", "mc314-0000": "Accesorios", "mc315-0000": "Equipo",
    "mc316-0000": "Opciones de Gear", "mc318-0000": "Equipar", "mc319-0000": "Subir/Bajar",
    "mc320-0000": "Estéreo", "mc321-0000": "Estéreo amplio", "mc322-0000": "Mono", "mc323-0000": "Ajustes",
    "mc324-0000": "Armas", "mc325-0000": "Piezas", "mc326-0000": "Combustible", "mc327-0000": "Motor",
    "mc328-0000": "Chasis", "mc329-0000": "Armadura",
    "mr000-0000": "Faltan", "mr003-0000": "COMB", "mr004-0000": "PESO",
}


class Patched(Disc):
    """A patched raw image, extracted beside it."""

    def __init__(self, number: int, source: Path):
        self.number = number
        self.track = source / f"disc{number}.bin"
        if not self.track.is_file():
            raise SystemExit(f"{self.track} is missing: apply Romhack Hispano's patch for disc {number} to the Redump image there")
        extract = source / f"extract/disc{number}"
        if not (extract / "manifest.json").is_file():
            subprocess.run([sys.executable, str(ROOT / "tools/extraction/disc_files.py"), str(self.track), str(extract)], check=True)
        self.manifest = json.loads((extract / "manifest.json").read_text())
        self.boot_path = extract / self.manifest["boot"]["name"]
        self.entries = {entry["slot"]: entry for entry in self.manifest["files"]}


def font_glyphs(disc: Disc) -> dict[int, bytes]:
    """text_install_font: halfword 1 the glyphs' offset, 6 the first one-byte glyph's code; 22 bytes a glyph."""
    font = tc.unpack(disc.sectors(disc.slot(0, 1, 6)))
    offset, first = struct.unpack_from("<H", font, 2)[0], struct.unpack_from("<H", font, 12)[0]
    return {code: font[offset + 22 * (code - first) :][:22] for code in range(first, 0x100)}


def characters(disc: Disc, patched: Patched, report: list[str]) -> tuple[dict[int, str], set[int]]:
    """Each one-byte code's character as the patch's font draws it, and the
    codes it changes that no rule names (their texts are refused)."""
    old, new = font_glyphs(disc), font_glyphs(patched)
    blank = old[tr.FIRST_GLYPH]
    where = {}
    for code, image in sorted(old.items()):
        if code in tr.CHARS and image != blank:
            where.setdefault(image, tr.CHARS[code])
    chars = dict(tr.CHARS)
    unnamed = []
    for code, image in new.items():
        if image == old.get(code) or code in ORIGINAL:
            continue
        chars.pop(code, None)
        if code in LETTERS:
            chars[code] = LETTERS[code]
        elif image == blank:
            chars[code] = " "
        elif image in where:
            chars[code] = where[image]
        else:
            unnamed.append(code)
    if unnamed:
        report.append(f"disc {patched.number}: the patch's font draws {len(unnamed)} code(s) no rule names: "
                      + " ".join(f"{code:02X}" for code in unnamed))
    return chars, set(unnamed)


def markup(raw: bytes, original: bytes, chars: dict[int, str], unnamed: set[int]) -> str:
    """The patch's text as the language's markup. A text of the development
    data in kana keeps the original's reading of every code; a code the
    patch changes and no rule names stays a code no glyph answers, so the
    text is refused."""
    if tr.kana(tr.render(original)):
        return tr.render(raw)
    text = tr.render(raw, chars)
    for code in unnamed & set(raw):
        text = text.replace(f"{{{code:02X}}}", f"{{ES {code:02X}}}")
    return text


def field_slots(disc: Disc) -> range:
    first = disc.slot(4, 0, 0xB8)
    return range(first, first + 1460, 2)


def rescripted(disc: Disc, patched: Patched) -> dict[int, set[int] | None]:
    """Per field map whose event scripts the patch changes: the messages its
    scripts show otherwise than the original's do (None: every message,
    where a script does not read)."""
    out: dict[int, set[int] | None] = {}
    for slot in field_slots(disc):
        a, b = disc.entries.get(slot), patched.entries.get(slot)
        if not a or not b or a["size"] < events.HEADER or a.get("sha256") == b.get("sha256"):
            continue
        map_id = (slot - field_slots(disc).start) // 2
        old, new = pw.field_events(disc, slot), pw.field_events(patched, slot)
        if old is None or new is None:
            out[map_id] = None
            continue
        if old.declared == new.declared and old.entries == new.entries:
            continue
        try:
            shown_old, shown_new = pw.displays(old), pw.displays(new)
        except Exception:  # noqa: BLE001 - a script the walk does not follow
            out[map_id] = None
            continue
        out[map_id] = {m for m in set(shown_old) | set(shown_new) if shown_old.get(m) != shown_new.get(m)}
    return out


def readings(source: Path, report: list[str]) -> tuple[dict[str, str], dict[int, int], dict[int, set[int] | None]]:
    """Romhack Hispano's reading of every table entry it changes: `where`
    and its markup; the count of entries read on each disc; and per field
    map the messages its scripts show otherwise than the original's."""
    found: dict[str, str] = {}
    counts: Counter = Counter()
    otherwise: dict[int, set[int] | None] = {}
    for number in (1, 2):
        disc, patched = Disc(number), Patched(number, source)
        chars, unnamed = characters(disc, patched, report)
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
                text = markup(new, raw, chars, unnamed)
                if number == 2 and found.get(where, text) != text:
                    report.append(f"{where}: the discs' files differ; Disc 1's taken")
                    continue
                found[where] = text
        for slot, (raw, new) in enumerate(zip(tr.initial_names(disc), tr.initial_names(patched))):
            if raw != new:
                found.setdefault(f"nm000-{slot:04d}", markup(new, raw, chars, unnamed))
    return found, dict(counts), otherwise


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", type=Path, default=SOURCE, help="the directory holding the patched disc1.bin and disc2.bin")
    args = parser.parse_args(argv)
    report: list[str] = []
    found, counts, otherwise = readings(args.source, report)
    work, tally = pw.lines(tr.originals().values(), found, otherwise, report, LANGUAGE)
    work = [line for line in work if line.split(" ", 1)[0] not in LABELS]
    work += [f"{where} [{EVERYWHERE}] {label}" for where, label in sorted(LABELS.items())]
    tally["menu pictures' words written for this project"] = len(LABELS)
    out = tr.LOCAL / LANGUAGE
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("[0-9]*.txt"):
        old.unlink()
    (out / "000.txt").write_text("".join(line + "\n" for line in work))
    summary = [f"{sum(counts.values())} table entries read ({counts.get(1, 0)} on Disc 1, {counts.get(2, 0)} on Disc 2), "
               f"{len(found)} of them Romhack Hispano's own", *(f"  {count:6d}  {name}" for name, count in sorted(tally.items()))]
    (args.source / "report.txt").write_text("\n".join([*summary, "", *report]) + "\n")
    print("\n".join(summary))
    print(f"{len(work)} lines written to {out / '000.txt'}; the report to {args.source / 'report.txt'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
