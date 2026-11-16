//! The Japanese release beside the North American one (docs/runtime.md,
//! Japanese): its text, movies and voices read from the player's own
//! Japanese discs (SLPS-01160 and SLPS-01161) as the game's English is read
//! from the North American ones. Nothing of either is shipped.
//!
//! **Text.** Both releases draw their text through one renderer from text
//! tables of the same files (`tools/analysis/text_control.py`
//! `text_tables`, read here from each disc as the decompiled loaders read
//! them). [`read`] pairs every North American table entry with the
//! Japanese table entry the game shows in its place: the same entry of the
//! same table, and for a field map's messages the entry a sequence
//! alignment of the two tables puts beside it (the North American text
//! names a party member speaking by spelling the name on its first line,
//! the Japanese text by inserting it, `0F 05 n`; a localised map may add or
//! drop messages, and 13 maps of disc 1 do). A table whose entries differ
//! in number outside the field keeps the original's English, as does an
//! entry the alignment leaves alone. The pairs become a language of
//! [`crate::locale`]: a North American text's translation wherever a
//! window takes it (the reading most of its places give) and the table
//! entries read otherwise translated alone, as Perfect Works' are, and the
//! party's and Gears' names by their slots.
//!
//! The Japanese text keeps its own font's codes: the font (directory (0, 1)
//! file 6 of the Japanese disc, 232 one-byte glyphs from 0x10 and two-byte
//! pages 0xF8-0xFE) is installed beside the translations and its codes are
//! carried in two-byte codes of the port's own (src/port/locale.c): a
//! one-byte glyph `c` as `F0 c`, a two-byte glyph `L s` as `(F1 + L - F8)
//! s`. The game's own font keeps its codes, so the numbers and names it
//! builds from its character table stay as they are. While Japanese is the
//! active language the font's two-byte threshold is 0xF0 (`locale::activate`),
//! which the original texts' one-byte glyphs never reach but in the
//! developers' Japanese placeholders (0xF0-0xFD), whose untranslated texts
//! are given as `F8 c`, the game's own glyph `c`.
//!
//! **Movies and voices.** The anime movies (directory (0x18, 1)) are the
//! same frames on both releases with the Japanese or the English voices,
//! and directory (0x2C, 1) holds the battle voices' sound banks. A North
//! American movie is matched with the Japanese movie of the same frames on
//! either disc; a battle voice file with the Japanese disc's file of its
//! number where they differ. [`Substitution`]s say where the drive finds
//! each (the North American file's own sectors when the Japanese one fits
//! in them, else sectors after the disc's end) and the index records the
//! game finds them by (src/runtime/crates/xem-core/src/devices/cd.rs,
//! `Session::set_voices`).

use std::collections::BTreeMap;

use xem_disc::{Index, Record, SECTOR_SIZE, SectorSource};

use crate::locale::{Encoded, Translations, fnv1a64, place_key};

/// The North American font's two-byte threshold, and the Japanese one's.
const US_THRESHOLD: u8 = 0xFE;
const JP_THRESHOLD: u8 = 0xF8;
/// The port's codes for the Japanese glyphs (src/port/locale.c): one-byte
/// glyphs after 0xF0, the two-byte pages after 0xF1 .. 0xF7.
pub const ONE_BYTE_LEAD: u8 = 0xF0;
const FIRST_PAGE_LEAD: u8 = 0xF1;
/// The game's own one-byte glyph `c` (0xF0-0xFD) while the threshold is lowered.
pub const OWN_GLYPH_LEAD: u8 = 0xF8;
/// The threshold while Japanese is active: the first lead byte of the port's codes.
pub const THRESHOLD: u8 = ONE_BYTE_LEAD;
/// The Japanese two-byte pages: leads 0xF8-0xFE.
const JP_PAGES: u8 = 7;

/// A packed block (8003342c's archives, the field files' components): a
/// u32 length, then groups of a flag byte and eight tokens, a literal byte
/// or (flag bit set) a 12-bit distance and 4-bit length - 3 copy from the
/// output (tools/analysis/packed.py). The game reads whole sectors, so a
/// stream may run past its file into the sector's padding.
pub fn unpack(source: &[u8]) -> Option<Vec<u8>> {
    let size = u32::from_le_bytes(source.get(..4)?.try_into().ok()?) as usize;
    if size > 0x20_0000 {
        return None;
    }
    let mut output = Vec::with_capacity(size);
    let mut position = 4;
    while output.len() < size {
        let flags = *source.get(position)?;
        position += 1;
        for token in 0..8 {
            if output.len() >= size {
                break;
            }
            let low = *source.get(position)?;
            position += 1;
            if flags & (1 << token) != 0 {
                let high = *source.get(position)?;
                position += 1;
                let distance = usize::from(low) | usize::from(high & 0x0F) << 8;
                let length = usize::from(high >> 4) + 3;
                if distance == 0 || distance > output.len() {
                    return None;
                }
                for _ in 0..length.min(size - output.len()) {
                    output.push(output[output.len() - distance]);
                }
            } else {
                output.push(low);
            }
        }
    }
    Some(output)
}

/// One disc's files as the game addresses them (`xem_disc::Index`).
struct Disc<'a> {
    source: &'a mut dyn SectorSource,
    index: Index,
}

impl<'a> Disc<'a> {
    fn new(source: &'a mut dyn SectorSource) -> Result<Self, String> {
        let index = Index::from_disc(source).map_err(|e| e.to_string())?;
        Ok(Disc { source, index })
    }

    fn record(&self, directory: usize, file: u32) -> Option<Record> {
        self.index.file(directory, file).filter(|r| !r.is_unused())
    }

    /// The file's whole sectors, as the drive delivers them.
    fn sectors(&mut self, directory: usize, file: u32) -> Option<Vec<u8>> {
        let record = self.record(directory, file).filter(|r| r.size > 0)?;
        xem_disc::read_data(self.source, record.lba, record.sectors() as usize * 2048).ok()
    }

    fn data(&mut self, directory: usize, file: u32) -> Option<Vec<u8>> {
        let size = self.record(directory, file)?.size as usize;
        let mut data = self.sectors(directory, file)?;
        data.truncate(size);
        Some(data)
    }
}

fn u16_at(data: &[u8], at: usize) -> Option<u16> {
    Some(u16::from_le_bytes(data.get(at..at + 2)?.try_into().ok()?))
}

fn u32_at(data: &[u8], at: usize) -> Option<u32> {
    Some(u32::from_le_bytes(data.get(at..at + 4)?.try_into().ok()?))
}

/// Entry `index` of an offset archive (a u32 count, then u32 offsets), to
/// the next entry's offset or the data's end; unpacked from its offset when `packed`.
fn archive_entry(data: &[u8], index: u32, packed: bool) -> Option<Vec<u8>> {
    let count = u32_at(data, 0)?;
    if index >= count {
        return None;
    }
    let start = u32_at(data, 4 + 4 * index as usize)? as usize;
    if packed {
        return unpack(data.get(start..)?);
    }
    let end = if index + 1 < count { u32_at(data, 8 + 4 * index as usize)? as usize } else { data.len() };
    data.get(start..end.max(start)).map(<[u8]>::to_vec)
}

/// The world map's area file sets (tools/analysis/text_control.py WORLD_AREA_FILES).
const WORLD_AREA_FILES: [u32; 14] = [43, 54, 65, 76, 87, 98, 109, 120, 131, 142, 153, 164, 175, 186];
const WORLD_AREA_FILES_MORE: [u32; 3] = [197, 208, 219];
/// The menu data archive's text tables (tools/analysis/text_control.py).
const MENU_DATA: [u32; 19] = [14, 15, 20, 21, 52, 53, 54, 55, 0x27, 0x28, 0x29, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33];

/// A text table's place: its group (the prefix `tools/translations.py`
/// names its entries by) and its item.
type Table = (&'static str, u32);

/// Every text table the decompiled loaders read on the disc
/// (tools/analysis/text_control.py `text_tables`), and the system data.
fn text_tables(disc: &mut Disc) -> (BTreeMap<Table, Vec<u8>>, Vec<u8>) {
    let mut tables = BTreeMap::new();
    let system = disc.sectors(1, 7).and_then(|s| unpack(&s)).unwrap_or_default();
    for index in 0..u32_at(&system, 0).unwrap_or(0) {
        // Resource 27 is the character table, not text.
        if index != 27 {
            if let Some(table) = archive_entry(&system, index, false) {
                tables.insert(("s", index), table);
            }
        }
    }
    let maps = disc.record(4, 0xB7).and_then(|r| r.directory_len()).unwrap_or(0);
    for file in (0xB8..0xB8 + maps).step_by(2) {
        let field = (file - 0xB8) / 2;
        if disc.record(4, file).is_none_or(|r| r.size < 0x154) {
            continue;
        }
        let Some(sectors) = disc.sectors(4, file) else { continue };
        let (Some(size), Some(offset)) = (u32_at(&sectors, 0x10C + 28), u32_at(&sectors, 0x130 + 28)) else { continue };
        if let Some(mut messages) = sectors.get(offset as usize..).and_then(|s| {
            let mut padded = s.to_vec();
            padded.extend_from_slice(&[0; 64]);
            unpack(&padded)
        }) {
            messages.truncate(size as usize);
            tables.insert(("f", field), messages);
        }
    }
    if let Some(menu) = disc.sectors(0x10, 1).and_then(|m| archive_entry(&m, 3, true)) {
        tables.insert(("ml", 3), menu);
    }
    if let Some(world) = disc.sectors(0x24, 0x26).and_then(|m| archive_entry(&m, 3, true)) {
        tables.insert(("mw", 3), world);
    }
    if let Some(data) = disc.sectors(0x10, 2) {
        for index in MENU_DATA {
            if let Some(table) = archive_entry(&data, index, true) {
                tables.insert(("md", index), table);
            }
        }
    }
    if let Some(mode) = disc.sectors(0x30, 3).and_then(|m| unpack(&m)).and_then(|m| archive_entry(&m, 0, false)) {
        tables.insert(("mm", 0), mode);
    }
    if let Some(battle) = disc.sectors(12, 3) {
        for index in [0x0F, 0x25] {
            if let Some(table) = archive_entry(&battle, index, true) {
                tables.insert(("ba", index), table);
            }
        }
    }
    let enemies = disc.record(13, 1).and_then(|r| r.directory_len()).unwrap_or(0);
    for file in (2..2 + enemies).step_by(2) {
        if let Some(enemy) = disc.data(13, file) {
            if let Some(table) = u16_at(&enemy, 0x30).and_then(|at| enemy.get(at as usize..)) {
                tables.insert(("en", file), table.to_vec());
            }
        }
    }
    for file in [1, 2, 3] {
        if let Some(table) = disc.data(0x23, file) {
            tables.insert(("bm", file), table);
        }
    }
    if let Some(scripts) = disc.sectors(0x20, 2) {
        for index in (1..u32_at(&scripts, 0).unwrap_or(0)).step_by(2) {
            if let Some(table) = archive_entry(&scripts, index, true) {
                tables.insert(("be", index / 2), table);
            }
        }
    }
    for file in WORLD_AREA_FILES.into_iter().chain(WORLD_AREA_FILES_MORE) {
        if let Some(area) = disc.sectors(0x24, file + 1).and_then(|a| unpack(&a)) {
            if let Some(table) = u32_at(&area, 0x1C).and_then(|at| area.get(at as usize..)) {
                tables.insert(("wa", file), table.to_vec());
            }
        }
    }
    (tables, system)
}

/// A text table's entries' offsets (a u16 count, 0, then count + 1 offsets;
/// a count of 0xFFFF a field without messages).
fn entries(table: &[u8]) -> Vec<u16> {
    let (Some(count), Some(0)) = (u16_at(table, 0), u16_at(table, 2)) else { return Vec::new() };
    if count == 0xFFFF {
        return Vec::new();
    }
    let offsets: Option<Vec<u16>> = (0..=usize::from(count)).map(|i| u16_at(table, 4 + 2 * i)).collect();
    match offsets {
        Some(mut offsets) if offsets.iter().all(|o| usize::from(*o) <= table.len()) => {
            offsets.pop();
            offsets
        }
        _ => Vec::new(),
    }
}

/// The text at `offset` through its 00, walked as window_reveal_text walks
/// it under `threshold` (src/port/locale.c xem_text_length); None without its 00.
fn text_at(table: &[u8], offset: usize, threshold: u8) -> Option<&[u8]> {
    let mut at = offset;
    loop {
        let byte = *table.get(at)?;
        at += match byte {
            0 => return table.get(offset..=at),
            0x0F => match *table.get(at + 1)? {
                0x04 | 0x0B => 2,
                0x03 => 4,
                _ => 3,
            },
            byte if byte >= threshold => 2,
            _ => 1,
        };
    }
}

/// The columns (4 pixels) an insertion fills when a line is measured
/// (`locale::insert_columns`), by its 0F sub-code.
fn insert_columns(sub: u8) -> u32 {
    match sub {
        0x05 | 0x0F => 16,
        0x03 | 0x04 | 0x06 | 0x07 | 0x08 => 36,
        0x09 | 0x0A => 12,
        0x0C => 14,
        _ => 0,
    }
}

/// The Japanese font's narrow glyphs (font halfwords 4 and 5).
#[derive(Clone, Copy, Debug)]
struct Narrow {
    one_byte: u8,
    two_byte_limit: u8,
}

/// A Japanese text in the port's codes, with its window's needs.
fn transcode(text: &[u8], narrow: Narrow) -> Encoded {
    let mut bytes = Vec::with_capacity(text.len() * 2);
    let (mut line, mut columns, mut lines, mut rows) = (0u32, 0u32, 1u32, 0u32);
    let mut at = 0;
    while let Some(&byte) = text.get(at) {
        match byte {
            0 => break,
            0x0F => {
                let sub = text.get(at + 1).copied().unwrap_or(0);
                let length = match sub {
                    0x04 | 0x0B => 2,
                    0x03 => 4,
                    _ => 3,
                };
                bytes.extend_from_slice(&text[at..(at + length).min(text.len())]);
                line += insert_columns(sub);
                at += length;
            }
            0x01 | 0x02 => {
                bytes.push(byte);
                columns = columns.max(line);
                line = 0;
                if byte == 0x01 {
                    lines += 1;
                } else {
                    rows = rows.max(lines);
                    lines = 1;
                }
                at += 1;
            }
            0x03..=0x0E => {
                bytes.push(byte);
                at += 1;
            }
            byte if byte < JP_THRESHOLD => {
                bytes.extend([ONE_BYTE_LEAD, byte]);
                line += if byte - 0x10 < narrow.one_byte { 2 } else { 3 };
                at += 1;
            }
            lead => {
                let second = text.get(at + 1).copied().unwrap_or(0);
                if lead == 0xFF {
                    // The special glyph; nothing else of the 0xFF page is drawn.
                    bytes.extend([0xFF, 0xFF]);
                } else {
                    bytes.extend([FIRST_PAGE_LEAD + (lead - JP_THRESHOLD), second]);
                }
                line += if lead == JP_THRESHOLD && second < narrow.two_byte_limit { 2 } else { 3 };
                at += 2;
            }
        }
    }
    bytes.push(0);
    Encoded { bytes, columns: columns.max(line), rows: rows.max(lines) }
}

/// A North American text that holds one-byte glyphs from the port's first
/// lead byte on (the developers' placeholders), with them as the game's own
/// glyphs (`F8 c`), or None when it holds none.
fn own_glyphs(text: &[u8]) -> Option<Vec<u8>> {
    let mut bytes = Vec::with_capacity(text.len() + 8);
    let mut changed = false;
    let mut at = 0;
    while let Some(&byte) = text.get(at) {
        let length = match byte {
            0x0F => match text.get(at + 1) {
                Some(0x04 | 0x0B) => 2,
                Some(0x03) => 4,
                _ => 3,
            },
            byte if byte >= US_THRESHOLD => 2,
            _ => 1,
        };
        if (ONE_BYTE_LEAD..US_THRESHOLD).contains(&byte) {
            bytes.extend([OWN_GLYPH_LEAD, byte]);
            changed = true;
        } else {
            bytes.extend_from_slice(&text[at..(at + length).min(text.len())]);
        }
        at += length;
    }
    changed.then_some(bytes)
}

/// The party's and the Gears' names the game data file gives
/// (`tools/analysis/text_control.py` initial_names): up to ten character
/// codes a 20-byte slot, through the character table's byte pairs.
fn initial_names(disc: &mut Disc, system: &[u8]) -> Vec<Vec<u8>> {
    let pairs = archive_entry(system, 27, false).unwrap_or_default();
    let game = disc.data(0x10, 3).unwrap_or_default();
    (0..NAME_SLOTS)
        .map(|slot| {
            let mut text = Vec::new();
            for i in 0..10 {
                let Some(code) = u16_at(&game, slot * NAME_BYTES + 2 * i) else { break };
                if code == 0x000F {
                    break;
                }
                let (Some(&first), Some(&second)) = (pairs.get(2 * usize::from(code)), pairs.get(2 * usize::from(code) + 1)) else { break };
                if first != 0 {
                    text.push(first);
                }
                text.push(second);
            }
            text.push(0);
            text
        })
        .collect()
}

const NAME_SLOTS: usize = 31;
const NAME_BYTES: usize = 20;

/// The party member a field message names on its first line: the North
/// American text spells the name, the Japanese one inserts it.
fn us_speaker(text: &[u8], names: &[Vec<u8>]) -> Option<usize> {
    let line = &text[..text.iter().position(|b| *b == 0x01)?];
    names.iter().position(|name| name.len() == line.len() + 1 && name.starts_with(line))
}

fn jp_speaker(text: &[u8]) -> Option<usize> {
    match text {
        [0x0F, 0x05, n, 0x01, ..] => Some(usize::from(*n)),
        _ => None,
    }
}

/// The North American entries' Japanese entries: a global alignment
/// (Needleman-Wunsch) in which entries pair as they come, gaining where
/// both name the same speaker and losing where they name two.
fn align(us: &[Option<usize>], jp: &[Option<usize>]) -> Vec<(usize, usize)> {
    const GAP: i32 = -2;
    let score = |a: Option<usize>, b: Option<usize>| match (a, b) {
        (Some(a), Some(b)) if a == b => 4,
        (Some(_), Some(_)) => -5,
        _ => 1,
    };
    let (n, m) = (us.len(), jp.len());
    let mut best = vec![vec![0i32; m + 1]; n + 1];
    let mut step = vec![vec![0u8; m + 1]; n + 1];
    for i in 1..=n {
        best[i][0] = GAP * i as i32;
        step[i][0] = 1;
    }
    for j in 1..=m {
        best[0][j] = GAP * j as i32;
        step[0][j] = 2;
    }
    for i in 1..=n {
        for j in 1..=m {
            let pair = best[i - 1][j - 1] + score(us[i - 1], jp[j - 1]);
            let skip_us = best[i - 1][j] + GAP;
            let skip_jp = best[i][j - 1] + GAP;
            (best[i][j], step[i][j]) = if pair >= skip_us && pair >= skip_jp {
                (pair, 0)
            } else if skip_us >= skip_jp {
                (skip_us, 1)
            } else {
                (skip_jp, 2)
            };
        }
    }
    let (mut i, mut j, mut pairs) = (n, m, Vec::new());
    while i > 0 || j > 0 {
        match step[i][j] {
            0 => {
                pairs.push((i - 1, j - 1));
                i -= 1;
                j -= 1;
            }
            1 => i -= 1,
            _ => j -= 1,
        }
    }
    pairs.reverse();
    pairs
}

/// A file the drive gives from a Japanese disc: the North American file
/// (its index slot and record) and where its Japanese counterpart is read
/// (`disc`'s `lba`, `sectors` of it) and placed (`japanese`'s record:
/// the North American file's own first sector when it fits there, else a
/// sector after the disc's end).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Substitution {
    pub slot: usize,
    pub english: Record,
    pub japanese: Record,
    pub disc: u8,
    pub lba: u32,
    pub sectors: u32,
}

/// What [`read`] found.
#[derive(Default)]
pub struct Japanese {
    /// The Japanese font block (text_install_font's format).
    pub font: Vec<u8>,
    pub translations: Translations,
    /// The North American table entries given a Japanese entry, of all.
    pub paired: usize,
    pub entries: usize,
    /// By North American disc (1 and 2): its files from the Japanese discs.
    pub media: [Vec<Substitution>; 2],
}

/// Movies' sectors are counted in 2336 bytes, other files' in 2048.
const MOVIES: usize = 0x19;
const VOICES: usize = 0x2D;
const MOVIE_SECTOR: u32 = 2336;
/// The video sectors a movie is known by.
const MOVIE_PROBE: usize = 48;

/// The files of a directory: when its file 1 is a directory record, the
/// files it counts; else those up to the next directory's first record.
fn files(disc: &Disc, directory: usize) -> std::ops::RangeInclusive<u32> {
    if let Some(count) = disc.record(directory, 1).and_then(|r| r.directory_len()) {
        return 2..=1 + count;
    }
    let Some(start) = disc.index.directory_start(directory) else { return 1..=0 };
    let next = disc.index.directory_table().iter().take(xem_disc::index::DISC_NUMBER_ENTRY)
        .copied().filter(|&s| s != 0xFFFF && s > start).min()
        .map_or(disc.index.records().len() as u32 + 1, u32::from);
    1..=next - u32::from(start)
}

/// A movie's first video sectors' data and its length: two releases' movie
/// of the same frames give the same (their audio sectors are not read).
fn movie_signature(disc: &mut Disc, record: Record) -> Option<(u64, u32)> {
    let sectors = (record.size.max(0) as u32).div_ceil(MOVIE_SECTOR);
    let mut raw = [0u8; SECTOR_SIZE];
    let mut video = Vec::new();
    for lba in record.lba..record.lba + sectors.min(400) {
        disc.source.read_sector(lba, &mut raw).ok()?;
        // A video sector: data (not audio) whose STR header has magic 0x160.
        if raw[18] & 0x04 == 0 && raw[24..26] == [0x60, 0x01] {
            video.extend_from_slice(&raw[24 + 32..24 + 2048]);
            if video.len() >= MOVIE_PROBE * 2016 {
                break;
            }
        }
    }
    (!video.is_empty()).then(|| (fnv1a64(&video), sectors))
}

/// Read the Japanese text, font and media beside the North American discs
/// (`us[n - 1]`, `jp[n - 1]` disc n; either may be absent). Fails when
/// no Japanese disc gives a font of the Japanese release's form.
pub fn read(us: [Option<&mut dyn SectorSource>; 2], jp: [Option<&mut dyn SectorSource>; 2]) -> Result<Japanese, String> {
    let mut us: Vec<Option<Disc>> = us.into_iter().map(|s| s.map(Disc::new).transpose()).collect::<Result<_, _>>()?;
    let mut jp: Vec<Option<Disc>> = jp.into_iter().map(|s| s.map(Disc::new).transpose()).collect::<Result<_, _>>()?;
    let mut result = Japanese::default();
    for disc in jp.iter_mut().flatten() {
        if let Some(font) = disc.sectors(1, 6).and_then(|f| unpack(&f)) {
            result.font = font;
            break;
        }
    }
    let font = &result.font;
    let header = |n: usize| u16_at(font, 2 * n).unwrap_or(0);
    let pages = font.len().saturating_sub(usize::from(header(1)) + usize::from(header(3))).div_ceil(0x1600);
    if font.is_empty() || header(2) != u16::from(JP_THRESHOLD) || header(6) != 0x10 || pages == 0 || pages > usize::from(JP_PAGES) {
        return Err("no Japanese disc gives the Japanese release's font".into());
    }
    let narrow = Narrow { one_byte: header(4) as u8, two_byte_limit: header(5) as u8 };

    // Each North American text's readings, by its places.
    let mut readings: BTreeMap<u64, Vec<(u64, Encoded)>> = BTreeMap::new();
    let mut own: BTreeMap<u64, Encoded> = BTreeMap::new();
    for number in 0..2 {
        let (Some(us), Some(jp)) = (us[number].as_mut(), jp[number].as_mut()) else { continue };
        let (us_tables, us_system) = text_tables(us);
        let (jp_tables, jp_system) = text_tables(jp);
        let us_names = initial_names(us, &us_system);
        let jp_names = initial_names(jp, &jp_system);
        for (slot, (name, reading)) in us_names.iter().zip(&jp_names).enumerate() {
            if name.len() > 1 && reading.len() > 1 {
                let place = place_key(name, slot as u16, (NAME_BYTES * slot) as u16, NAME_SLOTS as u16);
                readings.entry(fnv1a64(name)).or_default().push((place, transcode(reading, narrow)));
            }
        }
        for (table, us_table) in &us_tables {
            let us_offsets = entries(us_table);
            let us_texts: Vec<Option<&[u8]>> = us_offsets.iter().map(|o| text_at(us_table, usize::from(*o), US_THRESHOLD)).collect();
            result.entries += us_texts.iter().flatten().count();
            for text in us_texts.iter().flatten() {
                if let Some(bytes) = own_glyphs(text) {
                    own.insert(fnv1a64(text), Encoded { bytes, columns: 0, rows: 0 });
                }
            }
            let Some(jp_table) = jp_tables.get(table) else { continue };
            let jp_offsets = entries(jp_table);
            let jp_texts: Vec<Option<&[u8]>> = jp_offsets.iter().map(|o| text_at(jp_table, usize::from(*o), JP_THRESHOLD)).collect();
            let pairs: Vec<(usize, usize)> = if table.0 == "f" {
                let us_speakers: Vec<_> = us_texts.iter().map(|t| t.and_then(|t| us_speaker(t, &us_names))).collect();
                let jp_speakers: Vec<_> = jp_texts.iter().map(|t| t.and_then(jp_speaker)).collect();
                align(&us_speakers, &jp_speakers)
            } else if us_texts.len() == jp_texts.len() {
                (0..us_texts.len()).map(|i| (i, i)).collect()
            } else {
                Vec::new()
            };
            for (i, j) in pairs {
                let (Some(text), Some(reading)) = (us_texts[i], jp_texts[j]) else { continue };
                if reading.len() <= 1 && text.len() > 1 {
                    continue;
                }
                let place = place_key(text, i as u16, us_offsets[i], us_offsets.len() as u16);
                let mut encoded = transcode(reading, narrow);
                if table.0 == "f" {
                    // The Japanese table's own window for it.
                    let pair = 4 + 2 * (jp_offsets.len() + 1) + 2 * j;
                    encoded.columns = encoded.columns.max(u32::from(jp_table.get(pair).copied().unwrap_or(0)));
                    encoded.rows = encoded.rows.max(u32::from(jp_table.get(pair + 1).copied().unwrap_or(0)));
                }
                readings.entry(fnv1a64(text)).or_default().push((place, encoded));
                result.paired += 1;
            }
        }
    }
    // A text's translation is the reading most of its places give; a place
    // read otherwise is translated alone.
    for (hash, places) in readings {
        let mut counts: BTreeMap<&[u8], usize> = BTreeMap::new();
        for (_, reading) in &places {
            *counts.entry(&reading.bytes).or_default() += 1;
        }
        let common = counts.iter().max_by_key(|(_, n)| **n).map(|(b, _)| b.to_vec()).expect("a place");
        for (place, reading) in &places {
            if reading.bytes != common {
                result.translations.entries.insert(*place, reading.clone());
            }
        }
        let common = places.into_iter().find(|(_, r)| r.bytes == common).expect("a place").1;
        result.translations.texts.insert(hash, common);
        own.remove(&hash);
    }
    result.translations.texts.extend(own);

    // The movies and the battle voices.
    for number in 0..2 {
        let Some(us_disc) = us[number].as_mut() else { continue };
        let mut end = us_disc.source.sector_count().next_multiple_of(75);
        let mut media = Vec::new();
        for file in files(us_disc, MOVIES) {
            let Some(slot) = us_disc.index.slot(MOVIES, file) else { continue };
            let Some(record) = us_disc.record(MOVIES, file).filter(|r| r.size > 0) else { continue };
            let Some(signature) = movie_signature(us_disc, record) else { continue };
            'found: for (other, jp_disc) in jp.iter_mut().enumerate() {
                let Some(jp_disc) = jp_disc.as_mut() else { continue };
                for jp_file in files(jp_disc, MOVIES) {
                    let Some(jp_record) = jp_disc.record(MOVIES, jp_file).filter(|r| r.size > 0) else { continue };
                    let Some(jp_signature) = movie_signature(jp_disc, jp_record) else { continue };
                    if jp_signature.0 == signature.0 && jp_signature.1.abs_diff(signature.1) * 50 <= signature.1 {
                        media.push(place(slot, record, jp_record, other as u8 + 1, jp_signature.1, signature.1, &mut end));
                        break 'found;
                    }
                }
            }
        }
        let Some(jp_disc) = jp[number].as_mut() else {
            result.media[number] = media;
            continue;
        };
        for file in files(us_disc, VOICES) {
            let (Some(slot), Some(record), Some(jp_record)) = (us_disc.index.slot(VOICES, file), us_disc.record(VOICES, file), jp_disc.record(VOICES, file)) else { continue };
            if record.size <= 0 || jp_record.size <= 0 {
                continue;
            }
            let (Some(english), Some(japanese)) = (us_disc.data(VOICES, file), jp_disc.data(VOICES, file)) else { continue };
            let sound = |data: &[u8]| data.windows(4).any(|w| w == b"wds " || w == b"seds");
            if english != japanese && sound(&english) && sound(&japanese) {
                media.push(place(slot, record, jp_record, number as u8 + 1, jp_record.sectors(), record.sectors(), &mut end));
            }
        }
        result.media[number] = media;
    }
    Ok(result)
}

/// Where the drive gives a Japanese file of `sectors` in place of a North
/// American one of `own` sectors: in its sectors when it fits, else from
/// `end`, which moves on past it.
fn place(slot: usize, english: Record, japanese: Record, disc: u8, sectors: u32, own: u32, end: &mut u32) -> Substitution {
    let lba = if sectors <= own {
        english.lba
    } else {
        let lba = *end;
        *end = (lba + sectors).next_multiple_of(75);
        lba
    };
    Substitution { slot, english, japanese: Record { lba, size: japanese.size }, disc, lba: japanese.lba, sectors: sectors.max(own) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_packed_block_unpacks() {
        // "abcabcab": three literals, then a copy of 5 from 3 back.
        let source = [8, 0, 0, 0, 0b1000, b'a', b'b', b'c', 0x03, 0x20];
        assert_eq!(unpack(&source).unwrap(), b"abcabcab");
        assert_eq!(unpack(&[4, 0, 0, 0, 0b1, 0x01, 0x00]), None, "a copy from before the output");
    }

    #[test]
    fn japanese_glyphs_take_the_ports_codes() {
        let narrow = Narrow { one_byte: 63, two_byte_limit: 21 };
        // {name 1}, a line, a narrow one-byte glyph, a wide one, a page, a narrow and a wide two-byte glyph.
        let text = [0x0F, 0x05, 0x01, 0x01, 0x20, 0x60, 0x02, 0xF8, 0x05, 0xFA, 0x40, 0xFF, 0xFF, 0x00];
        let encoded = transcode(&text, narrow);
        assert_eq!(encoded.bytes, [0x0F, 0x05, 0x01, 0x01, 0xF0, 0x20, 0xF0, 0x60, 0x02, 0xF1, 0x05, 0xF3, 0x40, 0xFF, 0xFF, 0x00]);
        // Lines: the name 16; 2 + 3 = 5; page 2: 2 + 3 + 3 = 8.
        assert_eq!((encoded.columns, encoded.rows), (16, 2));
    }

    #[test]
    fn a_placeholders_glyphs_stay_the_games() {
        assert_eq!(own_glyphs(&[0x20, 0xF4, 0xFE, 0x31, 0xFC, 0x00]).unwrap(), [0x20, 0xF8, 0xF4, 0xFE, 0x31, 0xF8, 0xFC, 0x00]);
        assert_eq!(own_glyphs(&[0x20, 0x0F, 0x05, 0xF4, 0x00]), None, "an insertion's operand is no glyph");
    }

    #[test]
    fn messages_pair_by_their_speakers() {
        // The North American table adds a message after the second.
        let us = [Some(2), Some(3), None, Some(3), Some(0), None];
        let jp = [Some(2), Some(3), Some(3), Some(0), None];
        assert_eq!(align(&us, &jp), [(0, 0), (1, 1), (3, 2), (4, 3), (5, 4)]);
        let same = [None, Some(1), None];
        assert_eq!(align(&same, &same), [(0, 0), (1, 1), (2, 2)]);
    }

    #[test]
    fn a_japanese_file_takes_the_english_files_sectors_where_it_fits() {
        let english = Record { lba: 1000, size: 10 * 2048 };
        let mut end = 300_000;
        let fits = place(5, english, Record { lba: 7, size: 9 * 2048 }, 1, 9, 10, &mut end);
        assert_eq!((fits.japanese, fits.sectors, end), (Record { lba: 1000, size: 9 * 2048 }, 10, 300_000));
        let longer = place(6, english, Record { lba: 7, size: 12 * 2048 }, 2, 12, 10, &mut end);
        assert_eq!((longer.japanese.lba, longer.sectors, longer.lba, longer.disc, end), (300_000, 12, 7, 2, 300_075));
    }
}
