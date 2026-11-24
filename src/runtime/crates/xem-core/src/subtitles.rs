//! The anime movies' subtitles (docs/runtime.md, Subtitles): Perfect Works
//! Build's lines and the frames they show on (`subtitles/README.md`), in
//! each language of the game's text, drawn in the game's own font for the
//! hosts to show over the movie when the player asks for them.
//!
//! The movie player keeps the file it streams (`movie_file`) and the
//! directory it started it in (`movie_saved_directory`, `movie_saved_index`:
//! `cd_get_selected_directory`'s); the anime movies are files of directory
//! (0x18, 1), on either disc, which field movies and the movie mode's second
//! kind play. A line shows from its first frame until its end, by the frame
//! the movie shows ([`crate::movie`]), as Perfect Works' own player counts
//! the frames it loads. Read from the game's memory between steps; nothing
//! is written.
//!
//! A line is drawn as the game draws text: each glyph is 11 rows of 12
//! columns from the font the game installed (`text_font`), a narrow glyph
//! 8 pixels on and a wide one 12, and a language's added letters are made
//! from it as src/port/locale.c makes them. Japanese comes from the Japanese
//! release's font, installed beside the translations with the Japanese text
//! (`locale::install_japanese`), by the codes `subtitles/ja-font.txt` gives
//! its characters. Each line of a subtitle is centred, white with a dark
//! outline, in the display's own pixels.

use std::collections::BTreeMap;
use std::sync::LazyLock;

use crate::locale::{self, Added, Language};
use crate::memory::GameMemory;

/// The movie player's file (a halfword) and the directory it started it in
/// (its group and index), while its library is loaded.
const PLAYER_FILE: u32 = 0x801E_8974;
const PLAYER_DIRECTORY: u32 = 0x801E_899C;
const PLAYER_DIRECTORY_INDEX: u32 = 0x801E_89A0;
/// The anime movies' directory.
const MOVIES: (u32, u32) = (0x18, 1);

/// A glyph's rows, the bytes a font gives each and a two-byte page's bytes
/// (text_install_font's block).
const GLYPH_ROWS: usize = 11;
const GLYPH_BYTES: u32 = 22;
const PAGE_BYTES: u32 = 0x1600;
/// The pixels a font's column is.
const COLUMN_PIXELS: u32 = 4;
/// From one line of a subtitle to the next, in pixels.
pub const LINE_PITCH: u32 = 14;
/// The text and its outline (sRGB-encoded RGBA).
const TEXT: [u8; 4] = [0xEC, 0xEC, 0xEC, 0xFF];
const OUTLINE: [u8; 4] = [0x14, 0x14, 0x1C, 0xFF];

const TIMINGS: &str = include_str!("../subtitles/timings.txt");
const JAPANESE_FONT: &str = include_str!("../subtitles/ja-font.txt");

/// Each language's lines.
fn source(language: Language) -> &'static str {
    match language {
        Language::English => include_str!("../subtitles/en.txt"),
        Language::PerfectWorks => include_str!("../subtitles/en-x-pw.txt"),
        Language::BrazilianPortuguese => include_str!("../subtitles/pt-BR.txt"),
        Language::French => include_str!("../subtitles/fr.txt"),
        Language::Italian => include_str!("../subtitles/it.txt"),
        Language::Spanish => include_str!("../subtitles/es.txt"),
        Language::Japanese => include_str!("../subtitles/ja.txt"),
    }
}

/// A line of a movie's subtitles: the movie (the disc and its file in
/// directory (0x18, 1)), the line's number in it, and the movie frames it
/// shows on: from `first` until `end`.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Line {
    pub disc: u8,
    pub file: u16,
    pub number: u16,
    pub first: u32,
    pub end: u32,
}

impl Line {
    /// Its name in the files: `<disc>.<file>.<number>`.
    pub fn name(&self) -> String {
        format!("{}.{}.{}", self.disc, self.file, self.number)
    }
}

/// A subtitle drawn: `width` x `height` RGBA8 pixels (sRGB-encoded, straight
/// alpha), each a pixel of the game's display.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Picture {
    pub width: u32,
    pub height: u32,
    pub rgba: Vec<u8>,
}

struct Subtitles {
    lines: Vec<Line>,
    /// Each language's text of each line (markup), in `lines`' order.
    texts: BTreeMap<Language, Vec<&'static str>>,
    /// The Japanese font's code of each character the Japanese lines write.
    japanese: BTreeMap<char, u16>,
    /// Every language's added letters and their codes (`locale::glyphs`).
    glyphs: Vec<locale::Glyph>,
    added: Added,
}

static SUBTITLES: LazyLock<Result<Subtitles, String>> = LazyLock::new(read);

fn lines_of(source: &str) -> impl Iterator<Item = (usize, &str)> {
    source.lines().enumerate().filter(|(_, line)| !line.trim().is_empty() && !line.starts_with('#'))
}

fn read() -> Result<Subtitles, String> {
    let mut lines: Vec<Line> = Vec::new();
    for (number, text) in lines_of(TIMINGS) {
        let fail = |what: &str| format!("timings.txt:{}: {what}", number + 1);
        let fields: Vec<&str> = text.split(' ').collect();
        let [name, first, end] = fields[..] else { return Err(fail("not `<name> <first> <end>`")) };
        let parts: Vec<&str> = name.split('.').collect();
        let [disc, file, n] = parts[..] else { return Err(fail("a name is <disc>.<file>.<n>")) };
        let line = Line {
            disc: disc.parse().map_err(|_| fail("disc"))?,
            file: file.parse().map_err(|_| fail("file"))?,
            number: n.parse().map_err(|_| fail("line number"))?,
            first: first.parse().map_err(|_| fail("first frame"))?,
            end: end.parse().map_err(|_| fail("end frame"))?,
        };
        if line.first >= line.end {
            return Err(fail("a line ends after its first frame"));
        }
        let follows = match lines.last() {
            Some(last) if (last.disc, last.file) == (line.disc, line.file) => line.number == last.number + 1 && line.first >= last.end,
            _ => line.number == 1,
        };
        if !follows {
            return Err(fail("a movie's lines are numbered from 1 and follow one another"));
        }
        lines.push(line);
    }
    let mut texts = BTreeMap::new();
    for language in Language::ALL {
        let file = language.code();
        let mut found = Vec::with_capacity(lines.len());
        for (number, text) in lines_of(source(language)) {
            let (name, markup) = text.split_once(' ').ok_or_else(|| format!("{file}.txt:{}: not `<name> <text>`", number + 1))?;
            let expected = lines.get(found.len()).map(Line::name);
            if expected.as_deref() != Some(name) || markup.trim().is_empty() {
                return Err(format!("{file}.txt:{}: {name} is not the next line, {expected:?}, or has no text", number + 1));
            }
            found.push(markup);
        }
        if found.len() != lines.len() {
            return Err(format!("{file}.txt has {} lines of {}", found.len(), lines.len()));
        }
        texts.insert(language, found);
    }
    let mut japanese = BTreeMap::new();
    for (number, text) in lines_of(JAPANESE_FONT) {
        let fail = || format!("ja-font.txt:{}: not `<character> <code>`", number + 1);
        let mut chars = text.chars();
        let (Some(character), Some(' ')) = (chars.next(), chars.next()) else { return Err(fail()) };
        let code: String = chars.collect();
        let value = u16::from_str_radix(&code, 16).ok().filter(|_| matches!(code.len(), 2 | 4)).ok_or_else(fail)?;
        if japanese.insert(character, value).is_some() {
            return Err(format!("ja-font.txt:{}: {character:?} again", number + 1));
        }
    }
    let glyphs = locale::glyphs()?;
    let added = locale::added_codes(&glyphs);
    Ok(Subtitles { lines, texts, japanese, glyphs, added })
}

/// Every line, in the order the files give them (by disc, movie and frame).
pub fn lines() -> &'static [Line] {
    SUBTITLES.as_ref().map_or(&[], |s| &s.lines)
}

/// `line`'s text in `language`: markup, `{n}` ending a line.
pub fn text(line: &Line, language: Language) -> Option<&'static str> {
    let subtitles = SUBTITLES.as_ref().ok()?;
    let index = subtitles.lines.iter().position(|l| l == line)?;
    subtitles.texts.get(&language)?.get(index).copied()
}

/// The line the movie playing in `mode` (`Services::mode`) on disc `disc`
/// shows now, as `memory` holds it.
pub fn line(memory: &dyn GameMemory, mode: Option<u32>, disc: u8) -> Option<&'static Line> {
    let movie = crate::movie::movie(memory, mode)?;
    let mut file = [0; 2];
    memory.read(PLAYER_FILE, &mut file).ok()?;
    let directory = (memory.read_u32(PLAYER_DIRECTORY).ok()?, memory.read_u32(PLAYER_DIRECTORY_INDEX).ok()?);
    let frame = u32::try_from(movie.frame).ok()?;
    if directory != MOVIES {
        return None;
    }
    let file = u16::from_le_bytes(file);
    lines().iter().find(|l| (l.disc, l.file) == (disc, file) && (l.first..l.end).contains(&frame))
}

/// A glyph to draw: its rows (a row's first eight columns its low byte's
/// bits 7-0, the next four its high byte's bits 7-4) and its advance.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Glyph {
    rows: [u16; GLYPH_ROWS],
    advance: u32,
}

/// A font block in game memory: its address, its header's halfwords
/// (text_install_font: 1 the glyphs' offset, 2 the two-byte threshold, 3 the
/// two-byte pages' offset, 4 and 5 the narrow glyphs' ends in the Japanese
/// font, 6 the first one-byte code) and its length where known.
#[derive(Clone, Copy, Debug)]
struct Font {
    address: u32,
    header: [u16; 8],
    bytes: Option<u32>,
}

impl Font {
    fn at(memory: &dyn GameMemory, address: u32, bytes: Option<u32>) -> Option<Font> {
        let mut raw = [0; 16];
        (address != 0).then_some(())?;
        memory.read(address, &mut raw).ok()?;
        Some(Font { address, header: std::array::from_fn(|i| u16::from_le_bytes([raw[2 * i], raw[2 * i + 1]])), bytes })
    }

    /// The font the game installed.
    fn game(memory: &dyn GameMemory) -> Option<Font> {
        Font::at(memory, memory.read_u32(locale::TEXT_FONT).ok()?, None)
    }

    /// The Japanese release's, installed with the Japanese text.
    fn japanese(memory: &dyn GameMemory) -> Option<Font> {
        let record = memory.read_u32(locale::JAPANESE_RECORD).ok().filter(|&r| r != 0)?;
        Font::at(memory, memory.read_u32(record).ok()?, Some(memory.read_u32(record + 4).ok()?))
    }

    /// The rows of one-byte glyph `second` (`first` 0) or two-byte glyph
    /// (`first`, `second`) (src/port/locale.c xem_font_rows).
    fn rows(&self, memory: &dyn GameMemory, first: u8, second: u8) -> Option<[u16; GLYPH_ROWS]> {
        let [_, glyphs, threshold, pages, _, _, one_byte, _] = self.header.map(u32::from);
        let (first, second) = (u32::from(first), u32::from(second));
        let at = if first == 0 {
            glyphs + second.checked_sub(one_byte)? * GLYPH_BYTES
        } else {
            glyphs + pages + second * GLYPH_BYTES + first.checked_sub(threshold)? * PAGE_BYTES
        };
        if self.bytes.is_some_and(|bytes| at + GLYPH_BYTES > bytes) {
            return None;
        }
        let mut raw = [0; GLYPH_BYTES as usize];
        memory.read(self.address + at, &mut raw).ok()?;
        Some(std::array::from_fn(|i| u16::from_le_bytes([raw[2 * i], raw[2 * i + 1]])))
    }
}

/// A glyph row's column (0-11).
fn column_bit(column: u32) -> u16 {
    if column < 8 { 0x80 >> column } else { 0x8000 >> (column - 8) }
}

/// `rows` turned a half turn within the box their ink fills
/// (src/port/locale.c xem_turn_rows).
fn turned(rows: [u16; GLYPH_ROWS]) -> [u16; GLYPH_ROWS] {
    let ink = rows.iter().fold(0, |ink, row| ink | row);
    let (Some(top), Some(bottom)) = (rows.iter().position(|&r| r != 0), rows.iter().rposition(|&r| r != 0)) else { return rows };
    let columns: Vec<u32> = (0..12).filter(|&c| ink & column_bit(c) != 0).collect();
    let (left, right) = (columns[0], columns[columns.len() - 1]);
    let mut out = [0; GLYPH_ROWS];
    for row in top..=bottom {
        for column in left..=right {
            if rows[row] & column_bit(column) != 0 {
                out[top + bottom - row] |= column_bit(left + right - column);
            }
        }
    }
    out
}

/// An added letter's rows from its base's (src/port/locale.c
/// text_draw_glyph): rows dropped and those above moved down, turned,
/// rows cleared, the accent drawn over.
fn compose(base: [u16; GLYPH_ROWS], recipe: &locale::Glyph) -> [u16; GLYPH_ROWS] {
    let mut rows = [0; GLYPH_ROWS];
    let mut to = GLYPH_ROWS;
    for from in (0..GLYPH_ROWS).rev() {
        if recipe.drop >> from & 1 == 0 {
            to -= 1;
            rows[to] = base[from];
        }
    }
    if recipe.turn {
        rows = turned(rows);
    }
    for (row, value) in rows.iter_mut().enumerate() {
        if recipe.clear >> row & 1 != 0 {
            *value = 0;
        }
        *value |= recipe.accent[row];
    }
    rows
}

/// A subtitle's glyphs, line by line, in the game's font: its markup
/// encoded as the game's text is (`locale::encode`).
fn game_glyphs(memory: &dyn GameMemory, subtitles: &Subtitles, markup: &str) -> Option<Vec<Vec<Glyph>>> {
    let font = Font::game(memory)?;
    let encoded = locale::encode(markup, &subtitles.added).ok()?;
    let mut lines = vec![Vec::new()];
    let mut bytes = encoded.bytes.iter().copied();
    while let Some(byte) = bytes.next() {
        let (code, rows) = match byte {
            0x00 => break,
            0x01 => {
                lines.push(Vec::new());
                continue;
            }
            0xFF => {
                let second = bytes.next()?;
                let recipe = subtitles.glyphs.get(usize::from(second.checked_sub(locale::FIRST_GLYPH as u8)?))?;
                (u16::from_be_bytes([0xFF, second]), compose(font.rows(memory, 0, recipe.base)?, recipe))
            }
            byte if byte >= 0x10 && u16::from(byte) < font.header[2] => (u16::from(byte), font.rows(memory, 0, byte)?),
            _ => return None,
        };
        lines.last_mut()?.push(Glyph { rows, advance: subtitles.added.columns(code) * COLUMN_PIXELS });
    }
    Some(lines)
}

/// A Japanese subtitle's glyphs, line by line, in the Japanese release's
/// font: each character by its code there (`ja-font.txt`), `{n}` ending a
/// line. A glyph is narrow among the font's first one-byte glyphs (header
/// halfword 4 of them) and its first page's first (halfword 5), as
/// src/port/locale.c measures them.
fn japanese_glyphs(memory: &dyn GameMemory, subtitles: &Subtitles, markup: &str) -> Option<Vec<Vec<Glyph>>> {
    let font = Font::japanese(memory)?;
    let [_, _, threshold, _, narrow, narrow_page, one_byte, _] = font.header;
    let mut lines = vec![Vec::new()];
    let mut rest = markup;
    while let Some(c) = rest.chars().next() {
        if let Some(after) = rest.strip_prefix("{n}") {
            lines.push(Vec::new());
            rest = after;
            continue;
        }
        rest = &rest[c.len_utf8()..];
        let code = *subtitles.japanese.get(&c)?;
        let [lead, second] = code.to_be_bytes();
        let (rows, wide) = if lead == 0 {
            (font.rows(memory, 0, second)?, u16::from(second).checked_sub(one_byte)? >= narrow)
        } else {
            (font.rows(memory, lead, second)?, u16::from(lead) != threshold || u16::from(second) >= narrow_page)
        };
        lines.last_mut()?.push(Glyph { rows, advance: if wide { 12 } else { 8 } });
    }
    Some(lines)
}

/// The subtitle's lines drawn: each centred, its glyphs' ink in the text's
/// colour and the pixels around it in the outline's, with a pixel's border.
fn picture(lines: &[Vec<Glyph>]) -> Picture {
    let widths: Vec<u32> = lines.iter().map(|glyphs| glyphs.iter().map(|g| g.advance).sum()).collect();
    let width = widths.iter().copied().max().unwrap_or(0) + 2;
    let height = GLYPH_ROWS as u32 + (lines.len() as u32).saturating_sub(1) * LINE_PITCH + 2;
    let mut ink = vec![false; (width * height) as usize];
    for (n, (glyphs, line_width)) in lines.iter().zip(&widths).enumerate() {
        let mut x = 1 + (width - 2 - line_width) / 2;
        let y = 1 + n as u32 * LINE_PITCH;
        for glyph in glyphs {
            for (row, bits) in glyph.rows.iter().enumerate() {
                for column in (0..12).filter(|&c| bits & column_bit(c) != 0 && c < glyph.advance) {
                    ink[((y + row as u32) * width + x + column) as usize] = true;
                }
            }
            x += glyph.advance;
        }
    }
    let at = |x: i64, y: i64| x >= 0 && y >= 0 && x < i64::from(width) && y < i64::from(height) && ink[(y * i64::from(width) + x) as usize];
    let mut rgba = vec![0; (width * height * 4) as usize];
    for y in 0..i64::from(height) {
        for x in 0..i64::from(width) {
            let colour = if at(x, y) {
                TEXT
            } else if (-1..=1).any(|dy| (-1..=1).any(|dx| at(x + dx, y + dy))) {
                OUTLINE
            } else {
                continue;
            };
            let i = ((y * i64::from(width) + x) * 4) as usize;
            rgba[i..i + 4].copy_from_slice(&colour);
        }
    }
    Picture { width, height, rgba }
}

/// `line` in `language`, drawn with the fonts `memory` holds; none without
/// them (Japanese needs the Japanese text installed) or for a character
/// they lack.
pub fn draw(memory: &dyn GameMemory, line: &Line, language: Language) -> Option<Picture> {
    let subtitles = SUBTITLES.as_ref().ok()?;
    let markup = text(line, language)?;
    let lines = match language {
        Language::Japanese => japanese_glyphs(memory, subtitles, markup)?,
        _ => game_glyphs(memory, subtitles, markup)?,
    };
    Some(picture(&lines))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory::OutOfBounds;

    /// PS1 RAM at its KSEG0 addresses and the translations' space at
    /// `locale::BASE`.
    struct Ram {
        main: Vec<u8>,
        high: Vec<u8>,
    }

    impl Ram {
        fn new() -> Self {
            Ram { main: vec![0; 0x20_0000], high: vec![0; 0x1_0000] }
        }
        fn slice(&self, address: u32, len: usize) -> Option<&[u8]> {
            let (memory, at) = if address < 0x8000_0000 { (&self.high, address.wrapping_sub(locale::BASE)) } else { (&self.main, address - 0x8000_0000) };
            memory.get(at as usize..at as usize + len)
        }
        fn set(&mut self, address: u32, bytes: &[u8]) -> &mut Self {
            self.write(address, bytes).unwrap();
            self
        }
        fn word(&mut self, address: u32, value: u32) -> &mut Self {
            self.set(address, &value.to_le_bytes())
        }
    }

    impl GameMemory for Ram {
        fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
            out.copy_from_slice(self.slice(address, out.len()).ok_or(OutOfBounds { address, len: out.len() })?);
            Ok(())
        }
        fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
            let (memory, at) = if address < 0x8000_0000 { (&mut self.high, address.wrapping_sub(locale::BASE)) } else { (&mut self.main, address - 0x8000_0000) };
            memory.get_mut(at as usize..at as usize + data.len()).ok_or(OutOfBounds { address, len: data.len() })?.copy_from_slice(data);
            Ok(())
        }
    }

    /// The field's movie of `file` in directory `directory` showing `frame`
    /// past its first picture, its player streaming it (invented memory, as
    /// `crate::movie`'s tests set it).
    fn field_movie(file: u16, directory: (u32, u32), frame: i32) -> Ram {
        let mut memory = Ram::new();
        memory.word(0x800A_DB70, 1).word(0x800A_DB6C, 0).word(0x800B_06A0, frame as u32);
        memory.set(0x801E_8964, &[1]).word(0x801E_8990, 0x800A_7120);
        memory.set(PLAYER_FILE, &file.to_le_bytes()).word(PLAYER_DIRECTORY, directory.0).word(PLAYER_DIRECTORY_INDEX, directory.1);
        memory
    }

    #[test]
    fn every_language_has_every_line_the_timings_give() {
        let subtitles = SUBTITLES.as_ref().unwrap();
        assert_eq!(subtitles.lines.len(), 128);
        let movies: std::collections::BTreeSet<(u8, u16)> = subtitles.lines.iter().map(|l| (l.disc, l.file)).collect();
        assert_eq!(movies.into_iter().collect::<Vec<_>>(), [(1, 3), (1, 4), (1, 7), (1, 8), (1, 9), (1, 10), (1, 11), (1, 15), (2, 2), (2, 6), (2, 10)]);
        for language in Language::ALL {
            assert_eq!(subtitles.texts[&language].len(), 128, "{}", language.code());
        }
        let opening = Line { disc: 1, file: 15, number: 1, first: 934, end: 975 };
        assert_eq!(lines().iter().find(|l| l.name() == "1.15.1"), Some(&opening));
        assert_eq!(text(&opening, Language::PerfectWorks), Some("This is an emergency. Level 1 alert."));
        // The original's English names Perfect Works' Karellen Krelian.
        let named = lines().iter().find(|l| l.name() == "2.6.11").unwrap();
        assert_eq!((text(named, Language::PerfectWorks), text(named, Language::English)), (Some("Karellen..."), Some("Krelian...")));
    }

    /// The pixels a line of `glyphs` takes: narrow letters 8, wide ones 12.
    fn widest(language: Language, markup: &str) -> u32 {
        let subtitles = SUBTITLES.as_ref().unwrap();
        markup.split("{n}").map(|line| match language {
            Language::Japanese => line.chars().map(|c| {
                let code = subtitles.japanese[&c];
                // The Japanese font's narrow glyphs (header halfwords 4 and 5 of
                // the Japanese discs' font: 0x3F one-byte glyphs from 0x10, and
                // page 0xF8's first 0x15).
                let narrow = if code < 0x100 { code - 0x10 < 0x3F } else { code >> 8 == 0xF8 && code & 0xFF < 0x15 };
                if narrow { 8 } else { 12 }
            }).sum(),
            _ => locale::encode(line, &subtitles.added).unwrap().columns * COLUMN_PIXELS,
        }).max().unwrap()
    }

    #[test]
    fn every_line_has_its_glyphs_and_fits_the_movie() {
        let subtitles = SUBTITLES.as_ref().unwrap();
        for language in Language::ALL {
            for (line, markup) in subtitles.lines.iter().zip(&subtitles.texts[&language]) {
                let where_ = format!("{} {}", language.code(), line.name());
                match language {
                    Language::Japanese => {
                        let missing: String = markup.replace("{n}", "").chars().filter(|c| !subtitles.japanese.contains_key(c)).collect();
                        assert!(missing.is_empty(), "{where_}: no code for {missing:?}");
                    }
                    _ => assert!(locale::encode(markup, &subtitles.added).is_ok_and(|e| e.bytes[..e.bytes.len() - 1].iter().all(|&b| b == 0x01 || b >= 0x10)),
                        "{where_}: {markup:?} has a control or a character the font lacks"),
                }
                // 8 pixels clear each side of the 320 the movies are (Perfect
                // Works' widest line spans them all), and three lines at most.
                assert!(widest(language, markup) <= 304, "{where_}: {markup:?} is {} pixels wide", widest(language, markup));
                assert!(markup.matches("{n}").count() <= 2, "{where_}: more than three lines");
            }
        }
    }

    #[test]
    fn a_line_shows_from_its_first_frame_until_its_end_while_its_movie_plays() {
        let shown = |memory: &Ram, disc| line(memory, Some(1), disc).map(Line::name);
        assert_eq!(shown(&field_movie(15, MOVIES, 934), 1).as_deref(), Some("1.15.1"));
        assert_eq!(shown(&field_movie(15, MOVIES, 974), 1).as_deref(), Some("1.15.1"));
        assert_eq!(shown(&field_movie(15, MOVIES, 975), 1), None, "gone by its end");
        assert_eq!(shown(&field_movie(15, MOVIES, 933), 1), None, "not yet");
        assert_eq!(shown(&field_movie(15, MOVIES, 1211), 1).as_deref(), Some("1.15.7"), "the next line from the frame the last ends");
        assert_eq!(shown(&field_movie(15, MOVIES, 934), 2), None, "disc 2's file 15 is another movie");
        assert_eq!(shown(&field_movie(15, (0x18, 0), 934), 1), None, "another directory's file 15");
        assert_eq!(shown(&field_movie(6, MOVIES, 286), 2).as_deref(), Some("2.6.1"));
        assert_eq!(line(&field_movie(15, MOVIES, 934), Some(2), 1), None, "no movie plays in a battle");
        let mut stopped = field_movie(15, MOVIES, 934);
        stopped.set(0x801E_8964, &[0xFF]);
        assert_eq!(shown(&stopped, 1), None, "the player stopped");
    }

    /// A font block at `address` (text_install_font's format) whose one-byte
    /// glyph `c` from 0x10 is a column of ink `c - 0x10` pixels tall in
    /// column 1, and whose first page's glyph s (s < 12) fills rows 0-10 of
    /// column s; its length covers six pages.
    fn font(memory: &mut Ram, address: u32, threshold: u16, narrow: u16, narrow_page: u16) -> u32 {
        let one_byte = 0xE8u32;
        let glyphs = 16u32;
        let pages = one_byte * GLYPH_BYTES;
        let header = [0u16, glyphs as u16, threshold, pages as u16, narrow, narrow_page, 0x10, 0];
        memory.set(address, &header.iter().flat_map(|h| h.to_le_bytes()).collect::<Vec<_>>());
        for c in 0..one_byte {
            let rows: Vec<u8> = (0..GLYPH_ROWS as u32).flat_map(|r| if r < c.min(11) { column_bit(1) } else { 0 }.to_le_bytes()).collect();
            memory.set(address + glyphs + c * GLYPH_BYTES, &rows);
        }
        for s in 0..12u32 {
            memory.set(address + glyphs + pages + s * GLYPH_BYTES, &column_bit(s).to_le_bytes().repeat(GLYPH_ROWS));
        }
        glyphs + pages + 6 * PAGE_BYTES
    }

    fn ink(picture: &Picture) -> Vec<String> {
        picture.rgba.chunks(4 * picture.width as usize).map(|row| row.chunks(4).map(|p| match p {
            [0xEC, ..] => '#',
            [0x14, ..] => 'o',
            _ => '.',
        }).collect()).collect()
    }

    #[test]
    fn a_line_is_drawn_in_the_games_font_centred_and_outlined() {
        let mut memory = field_movie(15, MOVIES, 934);
        memory.word(locale::TEXT_FONT, 0x8010_0000);
        font(&mut memory, 0x8010_0000, 0xFE, 0, 0);
        let line = lines().iter().find(|l| l.name() == "1.11.1").unwrap(); // "Who are you?"
        let picture = draw(&memory, line, Language::PerfectWorks).unwrap();
        // 12 narrow glyphs of 8 pixels and a pixel each side; 11 rows and a pixel above and below.
        assert_eq!((picture.width, picture.height), (12 * 8 + 2, 13));
        // "W" (0x37) is a column 11 rows tall at x 1 + 1, the space (0x10) none.
        let rows = ink(&picture);
        assert_eq!(&rows[0][..4], ".ooo");
        assert_eq!(&rows[1][..4], ".o#o");
        assert_eq!(&rows[11][..4], ".o#o");
        assert_eq!(&rows[12][..4], ".ooo");
        assert_eq!(rows[1].chars().nth(1 + 3 * 8 + 1), Some('.'), "the space after \"Who\"");
        // An added letter is its base composed with its accent: Perfect Works'
        // dash is the hyphen's rows with the dash's, 12 pixels on.
        let cut = lines().iter().find(|l| l.name() == "1.11.2").unwrap(); // "Who—"
        let picture = draw(&memory, cut, Language::PerfectWorks).unwrap();
        assert_eq!(picture.width, 3 * 8 + 12 + 2);
        assert_eq!(&ink(&picture)[6][1 + 24..1 + 36], "....o#####o.");
        // Lines are centred under one another, LINE_PITCH apart.
        let two = lines().iter().find(|l| l.name() == "1.7.2").unwrap(); // "- Sis!{n}- Stop!"
        let picture = draw(&memory, two, Language::PerfectWorks).unwrap();
        assert_eq!((picture.width, picture.height), (7 * 8 + 2, 11 + LINE_PITCH + 2));
        // No font installed, no picture.
        memory.word(locale::TEXT_FONT, 0);
        assert_eq!(draw(&memory, line, Language::PerfectWorks), None);
    }

    #[test]
    fn japanese_is_drawn_in_the_japanese_font_by_its_codes() {
        let subtitles = SUBTITLES.as_ref().unwrap();
        let line = &subtitles.lines[0];
        let mut memory = field_movie(3, MOVIES, 271);
        assert_eq!(draw(&memory, line, Language::Japanese), None, "without the Japanese text installed");
        let address = locale::BASE + 0x100;
        let bytes = font(&mut memory, address, 0xF8, 0x3F, 0x15);
        memory.word(locale::JAPANESE_RECORD, locale::BASE + 0x40).word(locale::BASE + 0x40, address).word(locale::BASE + 0x44, bytes);
        let picture = draw(&memory, line, Language::Japanese).unwrap();
        let markup = text(line, Language::Japanese).unwrap();
        assert_eq!(picture.width, widest(Language::Japanese, markup) + 2);
        assert_eq!(picture.height, 11 + markup.matches("{n}").count() as u32 * LINE_PITCH + 2);
    }

    #[test]
    fn an_added_letter_is_made_as_the_port_makes_it() {
        let recipe = |c| locale::glyphs().unwrap().into_iter().find(|g| g.character == c).unwrap();
        let base: [u16; GLYPH_ROWS] = std::array::from_fn(|r| 1 << r);
        // ¡ is ! turned within its ink.
        let bang = [0, 0x20, 0x20, 0x20, 0x20, 0x20, 0, 0x20, 0, 0, 0];
        assert_eq!(compose(bang, &recipe('¡'))[..10], [0, 0x20, 0, 0x20, 0x20, 0x20, 0x20, 0x20, 0, 0]);
        // A capital's dropped rows move those above down; the accent goes over.
        let capital = recipe('Á');
        let composed = compose(base, &capital);
        assert_eq!(composed[10], 1 << 10);
        assert_eq!(composed[4] & !capital.accent[4], 1 << 3, "row 4 dropped: row 3 moved down to it");
    }
}
