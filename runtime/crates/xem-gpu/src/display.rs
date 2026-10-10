//! Display readout for presentation and the VRAM debug view.
//!
//! psx-spx (GPU Display Control): GP1(05h) gives the VRAM position of the
//! displayed area's top-left pixel; GP1(06h) the horizontal range X1-X2 in
//! video clock cycles, of which `((X2-X1)/dotclock + 2) & !3` pixels are shown
//! (dotclock 10, 8, 5, 4 or 7 for 256, 320, 512, 640 and 368 wide modes);
//! GP1(07h) the vertical range Y1-Y2 in scanlines, doubled in 480-line
//! interlaced mode. In 24-bit mode each displayed pixel takes three bytes of
//! VRAM, read as R, G, B in byte order.

use crate::{Gpu, VRAM_HEIGHT, VRAM_WIDTH};

/// The displayed picture's geometry and format.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DisplayInfo {
    /// GP1(03h): the display is on.
    pub enabled: bool,
    /// VRAM X of the first displayed pixel, in 16-bit units.
    pub x: u16,
    /// VRAM Y of the first displayed line.
    pub y: u16,
    /// Displayed pixels per line.
    pub width: u32,
    /// Displayed lines (both fields in 480-line interlaced mode).
    pub height: u32,
    /// The nominal horizontal resolution of the display mode (256, 320, 368, 512 or 640).
    pub mode_width: u32,
    /// 24-bit colour (MDEC frames) instead of 15-bit.
    pub color24: bool,
    /// Interlaced output.
    pub interlaced: bool,
    /// PAL timing instead of NTSC.
    pub pal: bool,
}

fn expand5(value: u16) -> u8 {
    let value = (value & 31) as u8;
    value << 3 | value >> 2
}

fn rgb555_to_rgba8(pixel: u16, out: &mut [u8]) {
    out[0] = expand5(pixel);
    out[1] = expand5(pixel >> 5);
    out[2] = expand5(pixel >> 10);
    out[3] = 0xFF;
}

impl Gpu {
    /// The display settings as GP1(03h) and GP1(05h)-GP1(08h) left them.
    pub fn display(&self) -> DisplayInfo {
        let mode = self.display_mode;
        let (dotclock, mode_width) = if mode & 0x40 != 0 {
            (7, 368)
        } else {
            [(10, 256), (8, 320), (5, 512), (4, 640)][(mode & 3) as usize]
        };
        let x1 = self.horizontal_range & 0xFFF;
        let x2 = self.horizontal_range >> 12 & 0xFFF;
        let width = if x2 > x1 {
            ((x2 - x1) / dotclock + 2) & !3
        } else {
            0
        };
        let y1 = self.vertical_range & 0x3FF;
        let y2 = self.vertical_range >> 10 & 0x3FF;
        let interlaced = mode & 0x20 != 0;
        let lines = y2.saturating_sub(y1);
        let height = if interlaced && mode & 0x04 != 0 {
            lines * 2
        } else {
            lines
        };
        DisplayInfo {
            enabled: !self.display_disabled,
            x: (self.display_start & 0x3FF) as u16,
            y: (self.display_start >> 10 & 0x1FF) as u16,
            width,
            height,
            mode_width,
            color24: mode & 0x10 != 0,
            interlaced,
            pal: mode & 0x08 != 0,
        }
    }

    /// The displayed picture as RGBA8 rows: (width, height, pixels). A
    /// disabled display reads as opaque black of the same size. The area
    /// wraps at the VRAM edges.
    pub fn display_rgba8(&self) -> (u32, u32, Vec<u8>) {
        let info = self.display();
        let (width, height) = (info.width as usize, info.height as usize);
        let mut out = vec![0u8; width * height * 4];
        if !info.enabled {
            out.chunks_exact_mut(4).for_each(|pixel| pixel[3] = 0xFF);
            return (info.width, info.height, out);
        }
        for line in 0..height {
            let row = (usize::from(info.y) + line) & (VRAM_HEIGHT - 1);
            let vram_row = &self.vram[row * VRAM_WIDTH..(row + 1) * VRAM_WIDTH];
            let out_row = &mut out[line * width * 4..(line + 1) * width * 4];
            if info.color24 {
                let byte = |offset: usize| {
                    let offset = offset & (VRAM_WIDTH * 2 - 1);
                    (vram_row[offset / 2] >> (8 * (offset & 1))) as u8
                };
                let start = usize::from(info.x) * 2;
                for (column, pixel) in out_row.chunks_exact_mut(4).enumerate() {
                    let offset = start + column * 3;
                    pixel.copy_from_slice(&[
                        byte(offset),
                        byte(offset + 1),
                        byte(offset + 2),
                        0xFF,
                    ]);
                }
            } else {
                for (column, pixel) in out_row.chunks_exact_mut(4).enumerate() {
                    let x = (usize::from(info.x) + column) & (VRAM_WIDTH - 1);
                    rgb555_to_rgba8(vram_row[x], pixel);
                }
            }
        }
        (info.width, info.height, out)
    }

    /// The whole VRAM as 1024x512 RGBA8, read as 15-bit colour (debug view).
    pub fn vram_rgba8(&self) -> Vec<u8> {
        let mut out = vec![0u8; VRAM_WIDTH * VRAM_HEIGHT * 4];
        for (&pixel, rgba) in self.vram.iter().zip(out.chunks_exact_mut(4)) {
            rgb555_to_rgba8(pixel, rgba);
        }
        out
    }
}
