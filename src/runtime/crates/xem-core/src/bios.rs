//! What the port takes from the BIOS ROM: its Kanji font.
//!
//! Krom2RawAdd (src/port/bios.c) returns glyph addresses in the ROM's font at
//! offset 0x66000: 3489 glyphs of 30 bytes (16x15 pixels, one bit each), one
//! per assigned JIS X 0208 character of the non-kanji rows 1-8 and the
//! first-level kanji rows 16-47 in code order, ending at offset 0x7F8DE. The
//! ROM's address 0xBFC00000 lies outside game memory, so the port uses its
//! KUSEG address 0x1FC00000, where the host places the font. The font is the
//! user's: without a BIOS image the glyphs are left blank (all zero), so save
//! titles and staff-roll text drawn from the ROM font appear empty.

use crate::memory::{GameMemory, OutOfBounds};

/// The font's address in game memory and in the ROM.
pub const KANJI_FONT_ADDRESS: u32 = 0x1FC6_6000;
pub const KANJI_FONT_ROM_OFFSET: usize = 0x6_6000;
pub const KANJI_GLYPHS: usize = 3489;
pub const KANJI_GLYPH_BYTES: usize = 30;
pub const KANJI_FONT_BYTES: usize = KANJI_GLYPHS * KANJI_GLYPH_BYTES;
/// A PS1 BIOS image's size.
pub const BIOS_SIZE: usize = 0x8_0000;

/// Where the font came from.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KanjiFont {
    /// The user's BIOS image.
    Bios,
    /// No BIOS image: blank glyphs.
    Blank,
}

/// Place the Kanji font in game memory from `bios` (a 512 KB BIOS image), or
/// blank glyphs without one.
pub fn install_kanji_font(memory: &mut dyn GameMemory, bios: Option<&[u8]>) -> Result<KanjiFont, OutOfBounds> {
    match bios.filter(|rom| rom.len() == BIOS_SIZE) {
        Some(rom) => {
            let font = &rom[KANJI_FONT_ROM_OFFSET..KANJI_FONT_ROM_OFFSET + KANJI_FONT_BYTES];
            memory.write(KANJI_FONT_ADDRESS, font)?;
            Ok(KanjiFont::Bios)
        }
        None => {
            memory.write(KANJI_FONT_ADDRESS, &vec![0; KANJI_FONT_BYTES])?;
            Ok(KanjiFont::Blank)
        }
    }
}
