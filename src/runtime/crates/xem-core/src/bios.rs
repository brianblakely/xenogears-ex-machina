//! What the port takes from the BIOS ROM: its Kanji font.
//!
//! Krom2RawAdd (src/port/bios.c) returns glyph addresses in the ROM's font at
//! offset 0x66000: 3489 glyphs of 30 bytes (16x15 pixels, one bit each, a row
//! two bytes with the leftmost pixel the high bit of the first), one per
//! assigned JIS X 0208 character of the non-kanji rows 1-8 and the
//! first-level kanji rows 16-47 in code order, ending at offset 0x7F8DE. The
//! ROM's address 0xBFC00000 lies outside game memory, so the port uses its
//! KUSEG address 0x1FC00000, where the host places the font. The game draws
//! its file screens' save titles from it (slot39 menu_save_title_render), and
//! the staff roll's text (field field_staff_roll_write_line), which no
//! shipped script enables.
//!
//! The font is the player's: a BIOS image they import gives it ([`check`]
//! says whether one can). Without one the host places substitute glyphs
//! drawn for this project (`font/substitute.txt`, `font/README.md`) for what
//! North American save titles hold, the full-width space, digits, Latin
//! letters and five punctuation marks, and leaves every other glyph blank
//! (the kanji among them). The font region lies outside
//! snapshots (`snapshot::REGIONS`): like the disc and the cards, the font is
//! the session's attachment, which a restored state keeps.

use crate::memory::GameMemory;
use serde::Serialize;

/// The font's address in game memory and in the ROM.
pub const KANJI_FONT_ADDRESS: u32 = 0x1FC6_6000;
pub const KANJI_FONT_ROM_OFFSET: usize = 0x6_6000;
pub const KANJI_GLYPHS: usize = 3489;
pub const KANJI_GLYPH_BYTES: usize = 30;
pub const KANJI_FONT_BYTES: usize = KANJI_GLYPHS * KANJI_GLYPH_BYTES;
/// A PS1 BIOS image's size.
pub const BIOS_SIZE: usize = 0x8_0000;
/// The most hosts read of a file the player names as a BIOS image: enough
/// for [`check`] to say what another console's BIOS or a ROM dump is.
pub const MAX_FILE: usize = 16 * 1024 * 1024;

/// The assigned cells of the non-kanji rows 1-8, {row, first cell, last
/// cell} in code order: the font's first glyphs (src/port/bios.c
/// `xem_kanji_rows`, which Krom2RawAdd reads).
const NON_KANJI_ROWS: [(u32, u32, u32); 17] = [
    (1, 1, 94), (2, 1, 14), (2, 26, 33), (2, 42, 48), (2, 60, 74), (2, 82, 89), (2, 94, 94),
    (3, 16, 25), (3, 33, 58), (3, 65, 90), (4, 1, 83), (5, 1, 86), (6, 1, 24), (6, 33, 56),
    (7, 1, 33), (7, 49, 81), (8, 1, 32),
];
/// The glyphs of rows 1-8, before the kanji.
const NON_KANJI: usize = 524;

/// The glyph a Shift-JIS code has in the font, or none for a code it lacks
/// (src/port/bios.c `xem_kanji_index`, as Krom2RawAdd finds it).
pub fn glyph_index(code: u16) -> Option<usize> {
    let (lead, trail) = (u32::from(code >> 8), u32::from(code & 0xFF));
    if !(0x81..=0x9F).contains(&lead) || !(0x40..=0xFC).contains(&trail) || trail == 0x7F {
        return None;
    }
    let (row, cell) = if trail >= 0x9F {
        ((lead - 0x81) * 2 + 2, trail - 0x9E)
    } else {
        ((lead - 0x81) * 2 + 1, trail - 0x40 + 1 - u32::from(trail > 0x7F))
    };
    if row <= 8 {
        let mut index = 0;
        for (r, first, last) in NON_KANJI_ROWS {
            if r == row && (first..=last).contains(&cell) {
                return Some(index + (cell - first) as usize);
            }
            index += (last - first + 1) as usize;
        }
        return None;
    }
    (row >= 16 && (row < 47 || (row == 47 && cell <= 51))).then(|| NON_KANJI + ((row - 16) * 94 + cell - 1) as usize)
}

/// Where the font in game memory came from (`status` reports it).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum KanjiFont {
    /// The player's BIOS image.
    Bios,
    /// No BIOS image: the substitute glyphs, every other glyph blank.
    Substitute,
}

/// The full-width digits and Latin letters: every font draws them, each
/// differently.
fn alphanumerics() -> impl Iterator<Item = u16> {
    (0x824F..=0x8258).chain(0x8260..=0x8279).chain(0x8281..=0x829A)
}

/// A glyph of `font` (a whole font region).
fn glyph(font: &[u8], code: u16) -> &[u8] {
    let at = glyph_index(code).expect("a code the font has") * KANJI_GLYPH_BYTES;
    &font[at..at + KANJI_GLYPH_BYTES]
}

/// Whether `rom` is a PS1 BIOS image whose Kanji font the game can use: 512
/// KiB, with a font where the BIOS keeps it (offset 0x66000), which this
/// checks for plausibility: its full-width space is blank, and its
/// full-width digits and Latin letters are drawn, each differently. Why not
/// otherwise.
pub fn check(rom: &[u8]) -> Result<(), String> {
    if rom.len() != BIOS_SIZE {
        return Err(format!("a PS1 BIOS image is 512 KiB (524288 bytes), not {} bytes", rom.len()));
    }
    let font = &rom[KANJI_FONT_ROM_OFFSET..KANJI_FONT_ROM_OFFSET + KANJI_FONT_BYTES];
    let drawn: Vec<&[u8]> = alphanumerics().map(|code| glyph(font, code)).collect();
    let problem = if glyph(font, 0x8140).iter().any(|&byte| byte != 0) {
        "its full-width space is not blank"
    } else if drawn.iter().any(|glyph| glyph.iter().all(|&byte| byte == 0)) {
        "a full-width letter or digit is blank"
    } else if (1..drawn.len()).any(|i| drawn[..i].contains(&drawn[i])) {
        "two full-width letters or digits are the same"
    } else {
        return Ok(());
    };
    Err(format!("this file has no Kanji font where a PS1 BIOS keeps it (offset 0x66000): {problem}"))
}

/// `font/substitute.txt`.
const SUBSTITUTE: &str = include_str!("../font/substitute.txt");

/// The lines of `font/substitute.txt` that hold its glyphs: each glyph's
/// code and character, then its rows (a comment is "#" alone or begins
/// "# ", which no row does: rows have no spaces).
fn substitute_lines() -> impl Iterator<Item = &'static str> {
    SUBSTITUTE.lines().filter(|line| !line.is_empty() && *line != "#" && !line.starts_with("# "))
}

/// The substitute glyphs (`font/substitute.txt`), each with its Shift-JIS
/// code.
pub fn substitute_glyphs() -> Vec<(u16, [u8; KANJI_GLYPH_BYTES])> {
    let mut lines = substitute_lines();
    let mut glyphs = Vec::new();
    while let Some(header) = lines.next() {
        let code = u16::from_str_radix(&header[..4], 16).expect("a glyph's code");
        let mut glyph = [0; KANJI_GLYPH_BYTES];
        for row in 0..KANJI_GLYPH_BYTES / 2 {
            let line = lines.next().expect("15 rows a glyph");
            for (column, pixel) in line.bytes().enumerate() {
                if pixel == b'#' {
                    glyph[row * 2 + column / 8] |= 0x80 >> (column % 8);
                }
            }
        }
        glyphs.push((code, glyph));
    }
    glyphs
}

/// The substitute font: its glyphs where Krom2RawAdd finds them, every
/// other glyph blank.
pub fn substitute_font() -> Vec<u8> {
    let mut font = vec![0; KANJI_FONT_BYTES];
    for (code, glyph) in substitute_glyphs() {
        let at = glyph_index(code).expect("a code the font has") * KANJI_GLYPH_BYTES;
        font[at..at + KANJI_GLYPH_BYTES].copy_from_slice(&glyph);
    }
    font
}

/// Place the Kanji font in game memory: `bios`'s, a BIOS image [`check`]
/// accepts (refused, nothing written, otherwise), or the substitute font
/// without one.
pub fn install_kanji_font(memory: &mut dyn GameMemory, bios: Option<&[u8]>) -> Result<KanjiFont, String> {
    let (font, kind) = match bios {
        Some(rom) => {
            check(rom)?;
            (rom[KANJI_FONT_ROM_OFFSET..KANJI_FONT_ROM_OFFSET + KANJI_FONT_BYTES].to_vec(), KanjiFont::Bios)
        }
        None => (substitute_font(), KanjiFont::Substitute),
    };
    memory.write(KANJI_FONT_ADDRESS, &font).map_err(|error| error.to_string())?;
    Ok(kind)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory::OutOfBounds;

    /// Game memory holding only the font's region.
    struct FontMemory(Vec<u8>);

    impl GameMemory for FontMemory {
        fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
            let at = address.wrapping_sub(KANJI_FONT_ADDRESS) as usize;
            out.copy_from_slice(self.0.get(at..at + out.len()).ok_or(OutOfBounds { address, len: out.len() })?);
            Ok(())
        }
        fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
            let at = address.wrapping_sub(KANJI_FONT_ADDRESS) as usize;
            self.0.get_mut(at..at + data.len()).ok_or(OutOfBounds { address, len: data.len() })?.copy_from_slice(data);
            Ok(())
        }
    }

    /// A BIOS image whose font holds `glyphs` (code, glyph), blank elsewhere.
    fn bios_with(glyphs: impl IntoIterator<Item = (u16, [u8; KANJI_GLYPH_BYTES])>) -> Vec<u8> {
        let mut rom = vec![0; BIOS_SIZE];
        for (code, glyph) in glyphs {
            let at = KANJI_FONT_ROM_OFFSET + glyph_index(code).unwrap() * KANJI_GLYPH_BYTES;
            rom[at..at + KANJI_GLYPH_BYTES].copy_from_slice(&glyph);
        }
        rom
    }

    #[test]
    fn glyphs_follow_the_fonts_code_order() {
        for (code, index) in [
            (0x8140, 0), (0x819E, 93), (0x819F, 94), (0x824F, 147), (0x8258, 156), (0x8260, 157),
            (0x8281, 183), (0x829A, 208), (0x84BE, 523), (0x889F, 524), (0x88FC, 617), (0x9872, 3488),
        ] {
            assert_eq!(glyph_index(code), Some(index), "{code:04X}");
        }
        for code in [0x0041, 0x8040, 0x817F, 0x81AD, 0x8240, 0x82F2, 0x84BF, 0x8540, 0x887E, 0x9873, 0xE040] {
            assert_eq!(glyph_index(code), None, "{code:04X} is not in the font");
        }
        // Every glyph is one code's.
        let mut seen = vec![false; KANJI_GLYPHS];
        for code in 0..=u16::MAX {
            if let Some(index) = glyph_index(code) {
                assert!(!std::mem::replace(&mut seen[index], true), "glyph {index} twice");
            }
        }
        assert!(seen.iter().all(|&s| s), "every glyph has a code");
        assert_eq!(NON_KANJI_ROWS.iter().map(|(_, first, last)| (last - first + 1) as usize).sum::<usize>(), NON_KANJI);
    }

    #[test]
    fn the_port_finds_glyphs_where_the_host_puts_them() {
        // src/port/bios.c's table and count, which Krom2RawAdd uses.
        let source = std::fs::read_to_string(concat!(env!("CARGO_MANIFEST_DIR"), "/../../../port/bios.c")).unwrap();
        let start = source.find("xem_kanji_rows[][3] = {").expect("the port's table");
        let table = &source[start + "xem_kanji_rows[][3] = {".len()..];
        let table = &table[..table.find("};").unwrap()];
        let numbers: Vec<u32> = table.split(|c: char| !c.is_ascii_digit()).filter(|s| !s.is_empty()).map(|s| s.parse().unwrap()).collect();
        let rows: Vec<(u32, u32, u32)> = numbers.chunks(3).map(|c| (c[0], c[1], c[2])).collect();
        assert_eq!(rows, NON_KANJI_ROWS);
        assert!(source.contains(&format!("#define XEM_KANJI_NON_KANJI {NON_KANJI}\n")));
        assert!(source.contains(&format!("#define XEM_KANJI_FONT {KANJI_FONT_ADDRESS:#X}\n").replace("0X", "0x")));
        assert!(source.contains(&format!("#define XEM_KANJI_GLYPH_BYTES {KANJI_GLYPH_BYTES}\n")));
    }

    #[test]
    fn the_substitute_covers_what_north_american_save_titles_hold() {
        // `python3 -m tools.analysis.save_titles` lists the codes both
        // discs' titles ask for: the space, five marks, the digits and 45
        // letters; the substitute has the whole alphabet.
        let title_marks = [0x8140, 0x8144, 0x8149, 0x815E, 0x8166, 0x817C];
        let expected: Vec<u16> = title_marks.into_iter().chain(alphanumerics()).collect();
        let glyphs = substitute_glyphs();
        assert_eq!(glyphs.iter().map(|(code, _)| *code).collect::<Vec<_>>(), expected, "in code order, each once");
        for (code, glyph) in &glyphs {
            assert_eq!(glyph[..2], [0, 0], "{code:04X}: the file screen reads the next glyph's top row");
            assert_eq!(glyph.iter().all(|&b| b == 0), *code == 0x8140, "{code:04X}: drawn, the space blank");
        }
        // The data's rows: 16 pixels each, '#' or '.'.
        let rows: Vec<&str> = substitute_lines().filter(|line| !line.contains(' ')).collect();
        assert_eq!(rows.len(), 15 * glyphs.len());
        assert!(rows.iter().all(|row| row.len() == 16 && row.bytes().all(|b| b == b'#' || b == b'.')));
        // A font made of them is a plausible font.
        assert_eq!(check(&bios_with(glyphs)), Ok(()));
    }

    #[test]
    fn a_bios_image_is_checked_for_its_size_and_its_font() {
        let font = substitute_glyphs();
        assert_eq!(check(&bios_with(font.clone())), Ok(()));
        for size in [0, BIOS_SIZE - 1, BIOS_SIZE + 1, 4 * 1024 * 1024] {
            assert_eq!(check(&vec![0; size]), Err(format!("a PS1 BIOS image is 512 KiB (524288 bytes), not {size} bytes")));
        }
        let refused = |rom: &[u8]| check(rom).unwrap_err();
        assert!(refused(&vec![0; BIOS_SIZE]).ends_with("a full-width letter or digit is blank"));
        assert!(refused(&vec![0xFF; BIOS_SIZE]).ends_with("its full-width space is not blank"));
        // Noise: a space with pixels.
        let noise: Vec<u8> = (0..BIOS_SIZE as u32).map(|i| (i.wrapping_mul(2_654_435_761) >> 13) as u8).collect();
        assert!(refused(&noise).starts_with("this file has no Kanji font where a PS1 BIOS keeps it (offset 0x66000)"));
        // One glyph for every letter and digit.
        let same = font.iter().map(|&(code, _)| (code, if code == 0x8140 { [0; 30] } else { font[1].1 }));
        assert!(refused(&bios_with(same)).ends_with("two full-width letters or digits are the same"));
    }

    #[test]
    fn the_font_comes_from_a_bios_or_the_substitute() {
        let mut memory = FontMemory(vec![0xAA; KANJI_FONT_BYTES]);
        assert_eq!(install_kanji_font(&mut memory, None), Ok(KanjiFont::Substitute));
        assert_eq!(memory.0, substitute_font());
        let a = memory.0[157 * 30..158 * 30].to_vec();
        assert!(a.iter().any(|&b| b != 0), "Ａ is drawn");
        assert!(memory.0[524 * 30..].iter().all(|&b| b == 0), "the kanji stay blank");

        // A BIOS's glyphs, here the substitute's one column to the right.
        let shifted: Vec<_> = substitute_glyphs().into_iter().map(|(code, glyph)| {
            let mut moved = [0; 30];
            for row in 0..15 {
                let bits = u16::from_be_bytes([glyph[row * 2], glyph[row * 2 + 1]]) >> 1;
                moved[row * 2..row * 2 + 2].copy_from_slice(&bits.to_be_bytes());
            }
            (code, moved)
        }).collect();
        let rom = bios_with(shifted);
        assert_eq!(install_kanji_font(&mut memory, Some(&rom)), Ok(KanjiFont::Bios));
        assert_eq!(memory.0, rom[KANJI_FONT_ROM_OFFSET..KANJI_FONT_ROM_OFFSET + KANJI_FONT_BYTES]);
        assert_ne!(memory.0, substitute_font());

        // A file that is not one changes nothing.
        let before = memory.0.clone();
        assert!(install_kanji_font(&mut memory, Some(&rom[1..])).is_err());
        assert!(install_kanji_font(&mut memory, Some(&vec![0; BIOS_SIZE])).is_err());
        assert_eq!(memory.0, before);
    }
}
