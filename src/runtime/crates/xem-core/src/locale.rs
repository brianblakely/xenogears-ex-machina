//! The game's text in other languages (docs/runtime.md, Languages).
//!
//! The game draws its menus, dialogue and battle text from text tables in the
//! player's disc data (docs/scripts/text-control.md). A language ships as
//! translations of those texts (`locale/<code>/*.txt`, written and checked by
//! `tools/translations.py`): one line per text, `<where> <hash> <markup>`,
//! naming the original by the FNV-1a 64 hash of its bytes through its closing
//! 00 and holding no original text. The markup is characters, which become
//! the font's glyphs, and controls in braces, which become the codes
//! window_reveal_text interprets. A line `<where> <hash>@<entry> <markup>`
//! translates the text in one table entry alone, the one `<where>` names:
//! `<entry>` is the entry's name, the text's hash carried on over the
//! entry's index, its offset in its table and the table's count
//! ([`place_key`]), for a language that reads one original text differently
//! from place to place. A line without markup translates its text to
//! nothing.
//!
//! [`install`] places every language's translations in game memory at
//! [`BASE`], outside the PS1's addresses and outside snapshots (as the Kanji
//! font is, `snapshot::REGIONS`), and [`activate`] says which one the port
//! uses (src/port/locale.c): it passes a window the active language's
//! translation of each text it takes (of the table entry the game takes it
//! from, where the language translates that entry alone), sizes a field
//! message's window for its translation, and draws the letters a language
//! adds to the font (`locale/<code>/glyphs.txt`) from the game's own letters
//! and accents drawn for this project. Every language stays installed whatever is active, so a
//! window keeps revealing a translation begun before a change, and a state
//! saved under any language restores in a session of the same translations
//! (their digest is part of the state's identity, `Runtime::new`).

use std::collections::{BTreeMap, BTreeSet};
use std::sync::LazyLock;

use serde::{Deserialize, Serialize};

use crate::memory::{GameMemory, OutOfBounds};

/// Where the translations lie in game memory (src/port/locale.c XEM_LOCALE).
pub const BASE: u32 = 0x2000_0000;
const MAGIC: u32 = u32::from_le_bytes(*b"XLC2");
const HEADER_BYTES: usize = 32;
const LANGUAGE_BYTES: usize = 16;
const TEXT_BYTES: usize = 16;
const GLYPH_BYTES: usize = 28;
/// The first added glyph (src/port/locale.c XEM_LOCALE_FIRST_GLYPH): two-byte
/// glyphs 0xFF20 on, a second byte past the controls' 00-0F.
pub const FIRST_GLYPH: u16 = 0xFF20;
/// The added glyphs end before the original's special glyph 0xFFFF.
const MAX_GLYPHS: usize = 0xDF;

/// A language of the game's text: the original's English, or a translation.
/// Perfect Works is the English of the Perfect Works Build's retranslation
/// (`locale/en-x-pw/README.md`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub enum Language {
    #[default]
    English,
    PerfectWorks,
    BrazilianPortuguese,
    French,
}

impl Language {
    pub const ALL: [Language; 4] = [Language::English, Language::PerfectWorks, Language::BrazilianPortuguese, Language::French];

    /// Its BCP 47 tag, as commands and `locale/` name it.
    pub fn code(self) -> &'static str {
        match self {
            Language::English => "en",
            Language::PerfectWorks => "en-x-pw",
            Language::BrazilianPortuguese => "pt-BR",
            Language::French => "fr",
        }
    }

    pub fn from_code(code: &str) -> Option<Language> {
        Language::ALL.into_iter().find(|language| language.code() == code)
    }

    /// Its name in itself, as the settings list it.
    pub fn name(self) -> &'static str {
        match self {
            Language::English => "English",
            Language::PerfectWorks => "English (Perfect Works)",
            Language::BrazilianPortuguese => "Português (Brasil)",
            Language::French => "Français",
        }
    }

    /// The translations' files (none for the original's English).
    fn sources(self) -> &'static [(&'static str, &'static str)] {
        match self {
            Language::English => &[],
            Language::PerfectWorks => &EN_X_PW,
            Language::BrazilianPortuguese => &PT_BR,
            Language::French => &FR,
        }
    }

    fn glyphs(self) -> &'static str {
        match self {
            Language::English => "",
            Language::PerfectWorks => include_str!("../locale/en-x-pw/glyphs.txt"),
            Language::BrazilianPortuguese => include_str!("../locale/pt-BR/glyphs.txt"),
            Language::French => include_str!("../locale/fr/glyphs.txt"),
        }
    }

    /// The value the port reads at BASE + 4: 0 the original's, else the
    /// translation's number among [`Language::ALL`]'s.
    fn number(self) -> u32 {
        Language::ALL.iter().position(|l| *l == self).expect("listed") as u32
    }
}

macro_rules! sources {
    ($code:literal: $($file:literal),*) => {
        [$(($file, include_str!(concat!("../locale/", $code, "/", $file)))),*]
    };
}

/// The translation files of each language, as `tools/translations.py` writes them.
static EN_X_PW: [(&str, &str); 12] = sources!("en-x-pw": "system.txt", "menus.txt", "battle.txt", "world.txt",
    "field-0.txt", "field-1.txt", "field-2.txt", "field-3.txt", "field-4.txt", "field-5.txt", "field-6.txt", "field-7.txt");
static PT_BR: [(&str, &str); 12] = sources!("pt-BR": "system.txt", "menus.txt", "battle.txt", "world.txt",
    "field-0.txt", "field-1.txt", "field-2.txt", "field-3.txt", "field-4.txt", "field-5.txt", "field-6.txt", "field-7.txt");
static FR: [(&str, &str); 12] = sources!("fr": "system.txt", "menus.txt", "battle.txt", "world.txt",
    "field-0.txt", "field-1.txt", "field-2.txt", "field-3.txt", "field-4.txt", "field-5.txt", "field-6.txt", "field-7.txt");

// The font's one-byte glyphs (directory (0, 1) file 6, read by
// text_install_font): the narrow ones 0x10-0x60 are ASCII, the wide ones
// from 0x61 full-width forms and symbols (0x82 and 0x83 repeat 0x6B and
// 0x6C; tools/translations.py names them alike).
const FIRST_ONE_BYTE: u8 = 0x10;
const NARROW_GLYPHS: u8 = 0x51;
const NARROW: &str = " +,-./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ[]=abcdefghijklmnopqrstuvwxyz!\"#%&'():?";
const WIDE: &str = "０１２３４５６７８９％＆☆○□△※→←↑↓．：－／‥“」＋−×＜＞\0\0『』＃、。・？！＿～…”（）「";

/// Controls by markup name: their bytes, and the operands that follow.
const CONTROLS: [(&str, &[u8], usize); 19] = [
    ("n", &[0x01], 0),
    ("p", &[0x02], 0),
    ("pause", &[0x03], 0),
    ("wait", &[0x0F, 0x00], 1),
    ("speed", &[0x0F, 0x01], 1),
    ("done", &[0x0F, 0x02], 1),
    ("res", &[0x0F, 0x03], 2),
    ("item", &[0x0F, 0x04], 0),
    ("name", &[0x0F, 0x05], 1),
    ("res23", &[0x0F, 0x06], 1),
    ("res24", &[0x0F, 0x07], 1),
    ("res25", &[0x0F, 0x08], 1),
    ("num", &[0x0F, 0x09], 1),
    ("num1", &[0x0F, 0x0A], 1),
    ("set6d", &[0x0F, 0x0B], 0),
    ("signed", &[0x0F, 0x0C], 1),
    ("skip", &[0x0F, 0x0D], 1),
    ("slow", &[0x0F, 0x0E], 1),
    ("button", &[0x0F, 0x0F], 1),
];

/// The columns (4 pixels) an insertion is taken to fill when a line is
/// measured, as tools/translations.py takes them.
fn insert_columns(name: &str) -> u32 {
    match name {
        "name" | "button" => 16,
        "item" | "res" | "res23" | "res24" | "res25" => 36,
        "num" | "num1" => 12,
        "signed" => 14,
        _ => 0,
    }
}

static ONE_BYTE: LazyLock<BTreeMap<char, u8>> = LazyLock::new(|| {
    let mut map = BTreeMap::new();
    for (i, c) in NARROW.chars().enumerate() {
        map.insert(c, FIRST_ONE_BYTE + i as u8);
    }
    for (i, c) in WIDE.chars().enumerate() {
        if c != '\0' {
            map.insert(c, 0x61 + i as u8);
        }
    }
    map
});

/// The letters the languages add to the font: each character's code, and
/// the codes the game draws 3 columns wide.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Added {
    pub codes: BTreeMap<char, u16>,
    pub wide: BTreeSet<u16>,
}

impl Added {
    /// text_get_glyph_width: a glyph's columns.
    fn columns(&self, code: u16) -> u32 {
        if code < 0x100 {
            if (code as u8).wrapping_sub(FIRST_ONE_BYTE) < NARROW_GLYPHS { 2 } else { 3 }
        } else if (FIRST_GLYPH..0xFFFF).contains(&code) && !self.wide.contains(&code) {
            2
        } else {
            3
        }
    }
}

/// A letter a language adds to the font (src/port/locale.c XemLocaleGlyph).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Glyph {
    pub character: char,
    /// The one-byte glyph it is drawn from.
    pub base: u8,
    /// Rows taken out of the base (bit n row n), those above moving down.
    pub drop: u16,
    /// Rows blanked.
    pub clear: u16,
    /// Rows drawn over it, in the font's format: a row's first eight columns
    /// are its low byte's bits 7-0, the next four its high byte's bits 7-4.
    pub accent: [u16; 11],
    /// It advances the wide glyphs' 3 columns, not a letter's 2.
    pub wide: bool,
}

fn rows(list: &str) -> Result<u16, String> {
    list.split(',').try_fold(0u16, |mask, row| match row.parse::<u16>() {
        Ok(row) if row < 11 => Ok(mask | 1 << row),
        _ => Err(format!("row {row:?} is not 0-10")),
    })
}

/// A language's added letters (`glyphs.txt`).
pub fn parse_glyphs(source: &str) -> Result<Vec<Glyph>, String> {
    let mut accents: BTreeMap<&str, [u16; 11]> = BTreeMap::new();
    let mut glyphs = Vec::new();
    for (number, line) in source.lines().enumerate() {
        let fail = |what: String| format!("glyphs.txt:{}: {what}", number + 1);
        let fields: Vec<&str> = line.split_whitespace().collect();
        if fields.is_empty() || line.starts_with('#') {
            continue;
        }
        if fields[0] == "accent" {
            let name = fields.get(1).ok_or_else(|| fail("an accent needs a name".into()))?;
            let mut accent = [0u16; 11];
            for row in &fields[2..] {
                let (index, pattern) = row.split_once('=').ok_or_else(|| fail(format!("{row:?} is not <row>=<columns>")))?;
                let index: usize = index.parse().ok().filter(|i| *i < 11).ok_or_else(|| fail(format!("row {index:?} is not 0-10")))?;
                if pattern.chars().count() > 12 || pattern.chars().any(|c| c != '#' && c != '.') {
                    return Err(fail(format!("{pattern:?} is not up to 12 columns of # and .")));
                }
                for (column, c) in pattern.chars().enumerate() {
                    if c == '#' {
                        accent[index] |= if column < 8 { 0x80 >> column } else { 0x8000 >> (column - 8) };
                    }
                }
            }
            accents.insert(name, accent);
            continue;
        }
        let mut chars = fields[0].chars();
        let (Some(character), None) = (chars.next(), chars.next()) else {
            return Err(fail(format!("{:?} is not one character", fields[0])));
        };
        // `blank` is the font's space, which a field of the line cannot hold.
        let base = fields.get(1).and_then(|b| {
            let mut chars = b.chars();
            match (chars.next(), chars.next()) {
                _ if *b == "blank" => ONE_BYTE.get(&' ').copied(),
                (Some(c), None) => ONE_BYTE.get(&c).copied(),
                _ => None,
            }
        });
        let base = base.filter(|b| b.wrapping_sub(FIRST_ONE_BYTE) < NARROW_GLYPHS).ok_or_else(|| fail("the base is not a narrow glyph of the font".into()))?;
        let accent = fields.get(2).and_then(|a| accents.get(a)).ok_or_else(|| fail("no such accent".into()))?;
        let (mut drop, mut clear, mut wide) = (0, 0, false);
        for option in &fields[3..] {
            match option.split_once('=') {
                Some(("drop", list)) => drop = rows(list).map_err(fail)?,
                Some(("clear", list)) => clear = rows(list).map_err(fail)?,
                None if *option == "wide" => wide = true,
                _ => return Err(fail(format!("{option:?} is not drop=, clear= or wide"))),
            }
        }
        glyphs.push(Glyph { character, base, drop, clear, accent: *accent, wide });
    }
    Ok(glyphs)
}

/// A translation's bytes as the game reads them, through its closing 00,
/// and its window's needs: its widest line in columns and its longest page
/// in lines.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Encoded {
    pub bytes: Vec<u8>,
    pub columns: u32,
    pub rows: u32,
}

/// Encode markup; `added` gives the languages' added letters their codes.
pub fn encode(markup: &str, added: &Added) -> Result<Encoded, String> {
    let mut bytes = Vec::new();
    let (mut line, mut columns, mut lines, mut rows) = (0u32, 0u32, 1u32, 0u32);
    let glyph = |code: u16, bytes: &mut Vec<u8>, line: &mut u32| {
        if code < 0x100 {
            bytes.push(code as u8);
        } else {
            bytes.extend(code.to_be_bytes());
        }
        *line += added.columns(code);
    };
    let mut chars = markup.chars();
    while let Some(c) = chars.next() {
        if c == '}' {
            return Err("a } without its {".into());
        }
        if c != '{' {
            let code = match (ONE_BYTE.get(&c), added.codes.get(&c)) {
                (Some(code), _) => u16::from(*code),
                (None, Some(code)) => *code,
                (None, None) => return Err(format!("no glyph for {c:?}")),
            };
            glyph(code, &mut bytes, &mut line);
            continue;
        }
        let tag: String = chars.by_ref().take_while(|c| *c != '}').collect();
        let parts: Vec<&str> = tag.split_whitespace().collect();
        let name = *parts.first().ok_or("empty braces")?;
        if parts.len() == 1 && matches!(name.len(), 2 | 4) && name.chars().all(|c| c.is_ascii_hexdigit() && !c.is_ascii_lowercase()) {
            glyph(u16::from_str_radix(name, 16).expect("hex"), &mut bytes, &mut line);
            continue;
        }
        let (_, code, operands) = CONTROLS.iter().find(|(control, _, _)| *control == name).ok_or_else(|| format!("unknown control {{{tag}}}"))?;
        if parts.len() != 1 + operands {
            return Err(format!("{{{tag}}} takes {operands} operand(s)"));
        }
        bytes.extend_from_slice(code);
        for operand in &parts[1..] {
            bytes.push(operand.parse::<u8>().map_err(|_| format!("{{{tag}}}: {operand:?} is not 0-255"))?);
        }
        line += insert_columns(name);
        match name {
            "n" => {
                columns = columns.max(line);
                line = 0;
                lines += 1;
            }
            "p" => {
                columns = columns.max(line);
                rows = rows.max(lines);
                line = 0;
                lines = 1;
            }
            _ => {}
        }
    }
    bytes.push(0);
    Ok(Encoded { bytes, columns: columns.max(line), rows: rows.max(lines) })
}

/// FNV-1a 64, the port's name for a text (src/port/locale.c).
pub fn fnv1a64(bytes: &[u8]) -> u64 {
    bytes.iter().fold(0xCBF2_9CE4_8422_2325, |hash, byte| (hash ^ u64::from(*byte)).wrapping_mul(0x100_0000_01B3))
}

/// A table entry's name (src/port/locale.c xem_locale_place): the hash of
/// its text's bytes through their 00, carried on over the entry's index, its
/// offset in its table and the table's count of entries, two bytes each.
pub fn place_key(text: &[u8], index: u16, offset: u16, count: u16) -> u64 {
    let place = [index.to_le_bytes(), offset.to_le_bytes(), count.to_le_bytes()].concat();
    place.iter().fold(fnv1a64(text), |hash, byte| (hash ^ u64::from(*byte)).wrapping_mul(0x100_0000_01B3))
}

/// One language's translations: of a text wherever a window takes it, by
/// the hash of the original, and of a text in one table entry alone, by
/// that entry's name ([`place_key`]).
#[derive(Clone, Debug, Default)]
pub struct Translations {
    pub texts: BTreeMap<u64, Encoded>,
    pub entries: BTreeMap<u64, Encoded>,
}

/// Parse a language's files with the added letters' codes; a line that does
/// not read or encode fails with its file and line.
pub fn parse(files: &[(&str, &str)], added: &Added) -> Result<Translations, String> {
    let mut translations = Translations::default();
    for (file, source) in files {
        for (number, line) in source.lines().enumerate() {
            if line.trim().is_empty() {
                continue;
            }
            let fail = |what: String| format!("{file}:{}: {what}", number + 1);
            let mut fields = line.splitn(3, ' ');
            let (Some(_where), Some(name)) = (fields.next(), fields.next()) else {
                return Err(fail("not `<where> <hash> <translation>`".into()));
            };
            let hex = |digits: &str| u64::from_str_radix(digits, 16).ok().filter(|_| digits.len() == 16).ok_or_else(|| fail(format!("{digits:?} is not 16 hex digits")));
            let encoded = encode(fields.next().unwrap_or(""), added).map_err(fail)?;
            let (kind, known) = match name.split_once('@') {
                Some((hash, entry)) => {
                    hex(hash)?;
                    ("entry", translations.entries.insert(hex(entry)?, encoded).is_some())
                }
                None => ("hash", translations.texts.insert(hex(name)?, encoded).is_some()),
            };
            if known {
                return Err(fail(format!("{kind} {name} again")));
            }
        }
    }
    Ok(translations)
}

/// What [`install`] writes: every language's translations and the added
/// glyphs, and their digest.
pub struct Image {
    pub bytes: Vec<u8>,
    pub digest: String,
    /// Each translated language, its number of texts and of single entries.
    pub languages: Vec<(Language, usize, usize)>,
}

/// The added glyphs of every language, each character once, in
/// [`Language::ALL`]'s order: a character's code is the same in every
/// language (src/port/locale.c draws them whichever is active).
pub fn glyphs() -> Result<Vec<Glyph>, String> {
    let mut all: Vec<Glyph> = Vec::new();
    for language in Language::ALL {
        for glyph in parse_glyphs(language.glyphs()).map_err(|e| format!("{}: {e}", language.code()))? {
            match all.iter().find(|g| g.character == glyph.character) {
                Some(known) if *known != glyph => {
                    return Err(format!("{}: {:?} differs from another language's", language.code(), glyph.character));
                }
                Some(_) => {}
                None => all.push(glyph),
            }
        }
    }
    if all.len() > MAX_GLYPHS {
        return Err(format!("{} added glyphs, at most {MAX_GLYPHS}", all.len()));
    }
    Ok(all)
}

pub fn added_codes(glyphs: &[Glyph]) -> Added {
    let code = |i: usize| FIRST_GLYPH + i as u16;
    Added {
        codes: glyphs.iter().enumerate().map(|(i, g)| (g.character, code(i))).collect(),
        wide: glyphs.iter().enumerate().filter(|(_, g)| g.wide).map(|(i, _)| code(i)).collect(),
    }
}

/// Build the image the port reads (src/port/locale.c): a header (magic, the
/// active language, the number of languages, the glyphs' address and
/// count), each language's texts' and single entries' addresses and counts,
/// the glyphs' recipes, each language's text records ascending by hash and
/// its entry records ascending by name, then the texts.
pub fn build() -> Result<Image, String> {
    let glyphs = glyphs()?;
    let added = added_codes(&glyphs);
    let languages: Vec<(Language, Translations)> = Language::ALL[1..]
        .iter()
        .map(|language| parse(language.sources(), &added).map(|t| (*language, t)).map_err(|e| format!("{}: {e}", language.code())))
        .collect::<Result<_, _>>()?;
    let address = |offset: usize| BASE + offset as u32;
    let glyphs_at = HEADER_BYTES + LANGUAGE_BYTES * languages.len();
    let mut records = glyphs_at + GLYPH_BYTES * glyphs.len();
    let mut text = records + TEXT_BYTES * languages.iter().map(|(_, t)| t.texts.len() + t.entries.len()).sum::<usize>();
    let mut bytes = vec![0u8; text];
    let put = |bytes: &mut Vec<u8>, at: usize, value: u32| bytes[at..at + 4].copy_from_slice(&value.to_le_bytes());
    put(&mut bytes, 0, MAGIC);
    put(&mut bytes, 8, languages.len() as u32);
    put(&mut bytes, 12, address(glyphs_at));
    put(&mut bytes, 16, glyphs.len() as u32);
    for (i, glyph) in glyphs.iter().enumerate() {
        let at = glyphs_at + GLYPH_BYTES * i;
        bytes[at] = glyph.base;
        bytes[at + 1] = u8::from(glyph.wide);
        bytes[at + 2..at + 4].copy_from_slice(&glyph.drop.to_le_bytes());
        bytes[at + 4..at + 6].copy_from_slice(&glyph.clear.to_le_bytes());
        for (row, value) in glyph.accent.iter().enumerate() {
            bytes[at + 6 + 2 * row..at + 8 + 2 * row].copy_from_slice(&value.to_le_bytes());
        }
    }
    for (n, (_, translations)) in languages.iter().enumerate() {
        for (field, named) in [(0, &translations.texts), (8, &translations.entries)] {
            put(&mut bytes, HEADER_BYTES + LANGUAGE_BYTES * n + field, address(records));
            put(&mut bytes, HEADER_BYTES + LANGUAGE_BYTES * n + field + 4, named.len() as u32);
            for (name, encoded) in named {
                put(&mut bytes, records, *name as u32);
                put(&mut bytes, records + 4, (*name >> 32) as u32);
                put(&mut bytes, records + 8, address(text));
                bytes[records + 12] = encoded.columns.min(255) as u8;
                bytes[records + 13] = encoded.rows.min(255) as u8;
                records += TEXT_BYTES;
                bytes.extend_from_slice(&encoded.bytes);
                text += encoded.bytes.len();
            }
        }
    }
    let digest = crate::snapshot::digest(&bytes).iter().map(|b| format!("{b:02x}")).collect();
    let languages = languages.iter().map(|(language, t)| (*language, t.texts.len(), t.entries.len())).collect();
    Ok(Image { bytes, digest, languages })
}

/// The image every runtime installs, built once.
pub static IMAGE: LazyLock<Result<Image, String>> = LazyLock::new(build);

/// Place every language's translations at [`BASE`], none active; the digest
/// names them in states' identities. A memory too small for them (a test's
/// module) gets none, and the game keeps its original text.
pub fn install(memory: &mut dyn GameMemory) -> Result<Option<String>, String> {
    let image = IMAGE.as_ref().map_err(Clone::clone)?;
    match memory.write(BASE, &image.bytes) {
        Ok(()) => Ok(Some(image.digest.clone())),
        Err(OutOfBounds { .. }) => Ok(None),
    }
}

/// Make `language` the one the port uses from the next text a window takes.
pub fn activate(memory: &mut dyn GameMemory, language: Language) -> Result<(), OutOfBounds> {
    memory.write_u32(BASE + 4, language.number())
}

/// The state identity's name for the installed translations.
pub fn package(digest: &str) -> String {
    format!("translations {digest}")
}

#[cfg(test)]
mod tests {
    use super::*;

    fn added() -> Added {
        added_codes(&glyphs().unwrap())
    }

    #[test]
    fn markup_encodes_as_the_game_reads_text() {
        let encoded = encode("Fei{n}\"Ação!{wait 10}\"{p}{name 128} {item}", &added()).unwrap();
        let codes = added().codes;
        let c = codes[&'ç'];
        let atil = codes[&'ã'];
        let mut expected = vec![0x25, 0x41, 0x45, 0x01, 0x58, 0x20];
        expected.extend(c.to_be_bytes());
        expected.extend(atil.to_be_bytes());
        expected.extend([0x4B, 0x57, 0x0F, 0x00, 10, 0x58, 0x02, 0x0F, 0x05, 128, 0x10, 0x0F, 0x04, 0x00]);
        assert_eq!(encoded.bytes, expected);
        // Lines: "Fei" 6, "\"Ação!\"" 7 narrow glyphs = 14; page 2: name 16 + space 2 + item 36.
        assert_eq!((encoded.columns, encoded.rows), (54, 2));
        // A glyph its recipe makes wide takes the wide glyphs' 3 columns: "Wha—!?" is five letters and a dash.
        assert!(added().wide.contains(&codes[&'—']) && !added().wide.contains(&codes[&';']));
        assert_eq!(encode("Wha—!?", &added()).unwrap().columns, 5 * 2 + 3);
        // No markup is a text of nothing: its 00 alone.
        let nothing = encode("", &added()).unwrap();
        assert_eq!((nothing.bytes, nothing.columns, nothing.rows), (vec![0], 0, 1));
    }

    #[test]
    fn markup_refuses_what_the_font_and_the_window_do_not_have() {
        for (markup, why) in [("a^b", "no glyph"), ("{wait}", "takes 1"), ("{wait 300}", "0-255"), ("{bold}", "unknown control"),
                              ("a}", "without its"), ("{}", "empty")] {
            let error = encode(markup, &added()).unwrap_err();
            assert!(error.contains(why), "{markup}: {error}");
        }
        // Glyph codes by hex, wide symbols by their characters.
        assert_eq!(encode("{FE3C}※{82}", &added()).unwrap().bytes, [0xFE, 0x3C, 0x71, 0x82, 0]);
    }

    #[test]
    fn the_added_letters_are_drawn_from_the_fonts_letters_and_the_accents() {
        let glyphs = glyphs().unwrap();
        // Perfect Works' 6 marks, Brazilian Portuguese's 24 letters, then the 14 French adds (è ë î ï ù û ü and capitals).
        assert_eq!(glyphs.len(), 44);
        let codes = added_codes(&glyphs).codes;
        let a_acute = &glyphs[6];
        assert_eq!((a_acute.character, a_acute.base, a_acute.drop, a_acute.clear, a_acute.wide), ('á', 0x3D, 0, 0, false));
        assert_eq!(&a_acute.accent[..3], &[0x10, 0x20, 0]);
        let i_acute = glyphs.iter().find(|g| g.character == 'í').unwrap();
        assert_eq!((i_acute.base, i_acute.clear), (0x45, 0b111));
        let capital = glyphs.iter().find(|g| g.character == 'Á').unwrap();
        assert_eq!((capital.base, capital.drop), (0x20, 1 << 2 | 1 << 4));
        let cedilla = glyphs.iter().find(|g| g.character == 'ç').unwrap();
        assert_eq!((cedilla.base, cedilla.accent[10]), (0x3F, 0x30));
        // French's letters: a letter both languages add keeps one code; the
        // diaeresis is two dots over the letter, the i's own dot blanked.
        assert_eq!((codes[&'é'], codes[&'è'], codes[&'Ü']), (FIRST_GLYPH + 10, FIRST_GLYPH + 30, FIRST_GLYPH + 43));
        let e_diaeresis = glyphs.iter().find(|g| g.character == 'ë').unwrap();
        assert_eq!((e_diaeresis.base, e_diaeresis.accent[0], e_diaeresis.accent[1]), (0x41, 0, 0x50));
        let i_diaeresis = glyphs.iter().find(|g| g.character == 'ï').unwrap();
        assert_eq!((i_diaeresis.base, i_diaeresis.clear, i_diaeresis.accent[1]), (0x45, 0b111, 0x50));
        // Perfect Works' marks: the semicolon is the colon with a tail, the
        // dash the hyphen's row drawn on across a wide glyph, and the blank
        // and El are drawn on the font's space.
        assert_eq!(glyphs[..6].iter().map(|g| g.character).collect::<String>(), "ö;*—␣ﭏ");
        let semicolon = &glyphs[1];
        assert_eq!((semicolon.base, semicolon.accent[9], semicolon.accent[10], semicolon.wide), (0x5F, 0x20, 0x40, false));
        let dash = &glyphs[3];
        assert_eq!((dash.base, dash.accent[5], dash.wide), (0x13, 0x07 | 0xC000, true));
        let blank = &glyphs[4];
        assert_eq!((blank.base, blank.accent, blank.wide), (0x10, [0; 11], true));
        let el = &glyphs[5];
        assert_eq!((el.base, el.accent[3], el.wide), (0x10, 0xF2 | 0x2000, true));
        assert!(parse_glyphs("accent dot 0=..#..\nx ※ dot").unwrap_err().contains("narrow glyph"));
        assert!(parse_glyphs("ä a umlaut").unwrap_err().contains("no such accent"));
        assert!(parse_glyphs("accent dot 0=..#..\nx a dot tall").unwrap_err().contains("wide"));
    }

    #[test]
    fn the_image_holds_each_languages_texts_by_hash_its_single_entries_by_name_and_the_glyphs() {
        let glyphs = glyphs().unwrap();
        let files = [("a.txt", "f001-0000 00000000000000ff Olá{n}mundo\nf001-0001 0000000000000001 Sim\n\
                               f001-0002 0000000000000001@00000000000000aa Não\nf001-0003 0000000000000002\n\
                               f001-0004 0000000000000003@00000000000000ab\n")];
        let translations = parse(&files, &added_codes(&glyphs)).unwrap();
        assert_eq!((translations.texts.len(), translations.entries.len()), (3, 2));
        assert_eq!(translations.texts[&1].bytes, [0x32, 0x45, 0x49, 0]);
        assert_eq!((translations.texts[&0xFF].columns, translations.texts[&0xFF].rows), (10, 2));
        // A text in one entry alone is named by the entry, and a line without markup is a text of nothing.
        assert_eq!(translations.entries[&0xAA].bytes[0], 0x2D);
        assert_eq!((&translations.texts[&2].bytes, &translations.entries[&0xAB].bytes), (&vec![0], &vec![0]));
        assert!(parse(&[("b.txt", "f001-0000 xyz Sim\n")], &Added::default()).unwrap_err().starts_with("b.txt:1:"));
        assert!(parse(&[("b.txt", "f001-0000 0000000000000001@12 Sim\n")], &Added::default()).unwrap_err().contains("16 hex digits"));
        assert!(parse(&[("b.txt", "f001-0000 0000000000000001 A\nf001-0001 0000000000000001 B\n")], &Added::default())
            .unwrap_err().contains("again"));
        assert!(parse(&[("b.txt", "f001-0000 0000000000000001@0000000000000002 A\nf001-0001 0000000000000003@0000000000000002 B\n")], &Added::default())
            .unwrap_err().contains("again"));
        // An entry's name is its text's hash carried on over its index, offset and its table's count.
        let text = [0x25, 0x41, 0x45, 0];
        let mut named = text.to_vec();
        named.extend([3, 0, 0x34, 0x12, 9, 0]);
        assert_eq!(place_key(&text, 3, 0x1234, 9), fnv1a64(&named));
        let image = IMAGE.as_ref().unwrap();
        let word = |at: usize| u32::from_le_bytes(image.bytes[at..at + 4].try_into().unwrap());
        assert_eq!((word(0), word(4), word(8), word(16)), (MAGIC, 0, 3, glyphs.len() as u32));
        assert_eq!(word(12), BASE + (HEADER_BYTES + 3 * LANGUAGE_BYTES) as u32);
        let wide: Vec<usize> = (0..glyphs.len()).filter(|i| image.bytes[(word(12) - BASE) as usize + GLYPH_BYTES * i + 1] == 1).collect();
        assert_eq!(wide, [3, 4, 5], "the glyphs drawn wide");
        let mut next = (word(12) - BASE) as usize + GLYPH_BYTES * glyphs.len();
        for (n, (_, texts, entries)) in image.languages.iter().enumerate() {
            for (field, expected) in [(0, texts), (8, entries)] {
                let (records, count) = (word(HEADER_BYTES + LANGUAGE_BYTES * n + field) - BASE, word(HEADER_BYTES + LANGUAGE_BYTES * n + field + 4));
                assert_eq!((records as usize, count as usize), (next, *expected), "each table follows the one before");
                next += TEXT_BYTES * count as usize;
                let mut last = None;
                for record in 0..count as usize {
                    let at = records as usize + TEXT_BYTES * record;
                    let hash = u64::from(word(at)) | u64::from(word(at + 4)) << 32;
                    assert!(last < Some(hash), "ascending by name");
                    last = Some(hash);
                    let text = (word(at + 8) - BASE) as usize;
                    assert!(image.bytes[text..].contains(&0), "each text ends at its 00");
                }
            }
        }
    }

    #[test]
    fn every_shipped_translation_reads_and_encodes() {
        // A line that did not would leave every translation uninstalled.
        let image = IMAGE.as_ref().expect("the shipped translations build");
        let lines = |files: &[(&str, &str)]| files.iter().map(|(_, source)| source.lines().filter(|l| !l.trim().is_empty()).count()).sum::<usize>();
        let (perfect_works, portuguese, french) = (lines(&EN_X_PW), lines(&PT_BR), lines(&FR));
        assert_eq!(image.languages.iter().map(|(language, texts, entries)| (*language, texts + entries)).collect::<Vec<_>>(),
            [(Language::PerfectWorks, perfect_works), (Language::BrazilianPortuguese, portuguese), (Language::French, french)]);
        assert!(portuguese > 16_000, "Brazilian Portuguese ships its translations ({portuguese})");
        assert!(french > 16_000, "French ships its translations ({french})");
        // Perfect Works reads anew the texts it changes, some of them in one entry alone.
        assert!(perfect_works > 10_000, "Perfect Works ships its translations ({perfect_works})");
        assert!(image.languages[0].2 > 100 && image.languages[1].2 == 0 && image.languages[2].2 == 0, "only Perfect Works translates single entries");
    }

    #[test]
    fn languages_are_named_by_their_tags_and_in_themselves() {
        assert_eq!(Language::from_code("pt-BR"), Some(Language::BrazilianPortuguese));
        assert_eq!(Language::from_code("pt"), None);
        assert_eq!(Language::default(), Language::English);
        assert_eq!(Language::BrazilianPortuguese.name(), "Português (Brasil)");
        assert_eq!(Language::BrazilianPortuguese.number(), 2);
        assert_eq!(Language::from_code("fr"), Some(Language::French));
        assert_eq!((Language::French.name(), Language::French.number()), ("Français", 3));
        // Perfect Works' English stands beside the original's.
        assert_eq!(Language::from_code("en-x-pw"), Some(Language::PerfectWorks));
        assert_eq!((Language::PerfectWorks.name(), Language::PerfectWorks.number()), ("English (Perfect Works)", 1));
        assert_eq!(Language::ALL[..2], [Language::English, Language::PerfectWorks]);
    }
}
