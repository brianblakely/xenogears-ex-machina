//! The PS1 GPU as a deterministic host service.
//!
//! The portable libgpu of the runtime hands this crate the words the original
//! SDK wrote to the GPU: GP0 command and data words, GP1 control words, DMA2
//! linked lists (ordering tables) and block transfers. The crate keeps the
//! 1024x512 16-bit VRAM and every register, draws in software, answers GPUSTAT
//! and GPUREAD, and reads the displayed picture back for presentation.
//!
//! Command encodings and rendering rules follow the psx-spx (problemkaputt)
//! GPU documentation. Where it does not specify the hardware's arithmetic (the
//! interpolation precision, the line stepping) the crate uses exact, documented
//! integer rules, listed with the other gaps in [`Gpu`]. Everything is integer arithmetic over owned state, so the same
//! input words give the same VRAM on every host, and [`Gpu::save`] captures the
//! whole device, including a command or transfer in progress.

mod display;
mod dma;
mod raster;
mod snapshot;

pub use display::DisplayInfo;
pub use dma::{DmaError, DmaSummary, Memory, MemoryMut};
pub use snapshot::SnapshotError;

/// VRAM width in 16-bit pixels.
pub const VRAM_WIDTH: usize = 1024;
/// VRAM height in lines.
pub const VRAM_HEIGHT: usize = 512;

/// Classes of GP0 commands counted by [`PrimitiveStats`].
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PrimitiveKind {
    /// GP0(02h) fill rectangle.
    Fill,
    /// Flat untextured polygon (a quad counts once).
    PolygonFlat,
    /// Gouraud untextured polygon.
    PolygonGouraud,
    /// Flat textured polygon.
    PolygonTextured,
    /// Gouraud textured polygon.
    PolygonGouraudTextured,
    /// Single line.
    Line,
    /// Polyline (counted once, whatever its length).
    Polyline,
    /// Untextured rectangle (any size).
    Rectangle,
    /// Textured rectangle (sprite).
    RectangleTextured,
    /// GP0(80h) VRAM-to-VRAM copy.
    VramCopy,
    /// GP0(A0h) CPU-to-VRAM transfer.
    CpuToVram,
    /// GP0(C0h) VRAM-to-CPU transfer.
    VramToCpu,
    /// GP0(E1h)-GP0(E6h) environment settings.
    Environment,
    /// NOP, clear cache, interrupt request and unused command numbers.
    Other,
}

impl PrimitiveKind {
    /// Every kind, in [`PrimitiveStats`] order.
    pub const ALL: [PrimitiveKind; 14] = [
        PrimitiveKind::Fill,
        PrimitiveKind::PolygonFlat,
        PrimitiveKind::PolygonGouraud,
        PrimitiveKind::PolygonTextured,
        PrimitiveKind::PolygonGouraudTextured,
        PrimitiveKind::Line,
        PrimitiveKind::Polyline,
        PrimitiveKind::Rectangle,
        PrimitiveKind::RectangleTextured,
        PrimitiveKind::VramCopy,
        PrimitiveKind::CpuToVram,
        PrimitiveKind::VramToCpu,
        PrimitiveKind::Environment,
        PrimitiveKind::Other,
    ];
}

/// Per-frame counters of the commands the GPU executed. They observe the
/// device and are not part of its state: [`Gpu::save`] leaves them out and
/// [`Gpu::take_stats`] starts a new count.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct PrimitiveStats {
    counts: [u32; PrimitiveKind::ALL.len()],
    rejected: u32,
}

impl PrimitiveStats {
    /// Commands of one kind executed since the count started.
    pub fn count(&self, kind: PrimitiveKind) -> u32 {
        self.counts[kind as usize]
    }

    /// Triangles and lines dropped by the size limits (a quad is two triangles).
    pub fn rejected(&self) -> u32 {
        self.rejected
    }

    /// Commands executed, all kinds together.
    pub fn total(&self) -> u32 {
        self.counts.iter().sum()
    }

    fn add(&mut self, kind: PrimitiveKind) {
        self.counts[kind as usize] = self.counts[kind as usize].wrapping_add(1);
    }
}

/// A VRAM rectangle being written by GP0(A0h) or read by GP0(C0h); `index`
/// is the next pixel, row-major.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Image {
    x: u16,
    y: u16,
    width: u16,
    height: u16,
    index: u32,
}

impl Image {
    fn from_words(position: u32, size: u32) -> Image {
        Image {
            x: (position & 0x3FF) as u16,
            y: ((position >> 16) & 0x1FF) as u16,
            width: (((size & 0xFFFF).wrapping_sub(1) & 0x3FF) + 1) as u16,
            height: (((size >> 16).wrapping_sub(1) & 0x1FF) + 1) as u16,
            index: 0,
        }
    }

    fn pixels(&self) -> u32 {
        u32::from(self.width) * u32::from(self.height)
    }

    /// VRAM index of pixel `n` of the rectangle, wrapping at the VRAM edges.
    fn vram_index(&self, n: u32) -> usize {
        let x = (u32::from(self.x) + n % u32::from(self.width)) as usize & (VRAM_WIDTH - 1);
        let y = (u32::from(self.y) + n / u32::from(self.width)) as usize & (VRAM_HEIGHT - 1);
        y * VRAM_WIDTH + x
    }
}

/// What the next GP0 word is.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Receive {
    /// A command word, or the next parameter of the command in `fifo`.
    Command,
    /// A further colour or vertex of a polyline whose first segment is drawn.
    Polyline {
        command: u32,
        color: u32,
        vertex: u32,
        next_color: Option<u32>,
    },
    /// Pixel data of a CPU-to-VRAM transfer.
    ImageLoad(Image),
}

/// The GPU: VRAM, drawing and display registers and the command receiver.
///
/// Known differences from the hardware (none are documented by psx-spx
/// precisely enough to reproduce, and none add state):
/// - Commands execute as soon as their last word arrives; GPUSTAT never
///   reports busy and there is no FIFO depth or timing.
/// - The texture cache is not modelled: texels are always read from current
///   VRAM, so a game that relies on stale cached texels after overwriting a
///   texture page differs. GP0(01h) is accepted and does nothing.
/// - Gouraud colours and texture coordinates are interpolated exactly
///   (rational barycentric interpolation at the pixel's integer position,
///   rounded down), not with the hardware's fixed-point gradients; results
///   agree at the vertices and may differ by one step inside.
/// - Lines are stepped with a symmetric DDA that draws both end points; the
///   hardware's exact stepping and end-point rule are not documented.
///   Polyline joints are drawn by both segments, and the end marker is
///   recognised from the third vertex (or its colour) on.
/// - GP1(10h) answers indexes 2-5 and 7 (GPU version 2) by bits 0-2 of its
///   parameter; GP1(20h) and the 2 MB-VRAM bits are not modelled.
/// - Texture page colour depth 3 (reserved) reads as 15-bit.
/// - Interlaced drawing ("drawing to display area" off, 480i) does not skip
///   the displayed field's lines; GPUSTAT.13/31 toggle on [`Gpu::vblank`].
/// - The display range is read back as the area it selects (its width and
///   height derived from GP1(06h)/GP1(07h)); borders, overscan and video
///   timing are not produced.
/// - VRAM-to-VRAM copies read each row before writing it, so overlapping
///   copies behave like a row-buffered memmove.
#[derive(Clone)]
pub struct Gpu {
    vram: Vec<u16>,
    /// GP0(E1h) bits 0-13 (bits 0-8 and 11 also set by textured polygons).
    draw_mode: u32,
    /// GP0(E2h) bits 0-19.
    texture_window: u32,
    /// GP0(E3h) bits 0-18.
    area_top_left: u32,
    /// GP0(E4h) bits 0-18.
    area_bottom_right: u32,
    /// GP0(E5h) bits 0-21.
    draw_offset: u32,
    /// GP0(E6h) bits 0-1: set mask bit, check mask bit.
    mask: u32,
    display_disabled: bool,
    /// GP1(04h) bits 0-1.
    dma_direction: u32,
    /// GP1(05h) bits 0-18.
    display_start: u32,
    /// GP1(06h) bits 0-23.
    horizontal_range: u32,
    /// GP1(07h) bits 0-19.
    vertical_range: u32,
    /// GP1(08h) bits 0-7.
    display_mode: u32,
    /// GP1(09h) bit 0.
    texture_disable_allowed: bool,
    irq: bool,
    odd_field: bool,
    gpuread: u32,
    fifo: Vec<u32>,
    receive: Receive,
    read: Option<Image>,
    stats: PrimitiveStats,
}

impl Default for Gpu {
    fn default() -> Self {
        Gpu::new()
    }
}

/// Number of words of the command whose first word is `command`; for a
/// polyline, the words of its first segment.
fn command_words(command: u32) -> usize {
    let op = command >> 24;
    let gouraud = op & 0x10 != 0;
    let textured = usize::from(op & 0x04 != 0);
    match op {
        0x02 => 3,
        0x20..=0x3F => {
            let vertices = if op & 0x08 != 0 { 4 } else { 3 };
            1 + vertices * (1 + textured) + if gouraud { vertices - 1 } else { 0 }
        }
        0x40..=0x5F => {
            if gouraud {
                4
            } else {
                3
            }
        }
        0x60..=0x7F => 2 + textured + usize::from(op & 0x18 == 0),
        0x80..=0x9F => 4,
        0xA0..=0xDF => 3,
        _ => 1,
    }
}

/// psx-spx: a polyline ends at a word whose bits 12-15 and 28-31 are 5
/// (usually 55555555h, some games use 50005000h).
fn is_polyline_end(word: u32) -> bool {
    word & 0xF000_F000 == 0x5000_5000
}

impl Gpu {
    /// The GPU after power-on: zeroed VRAM and the GP1(00h) reset state.
    pub fn new() -> Gpu {
        let mut gpu = Gpu {
            vram: vec![0; VRAM_WIDTH * VRAM_HEIGHT],
            draw_mode: 0,
            texture_window: 0,
            area_top_left: 0,
            area_bottom_right: 0,
            draw_offset: 0,
            mask: 0,
            display_disabled: true,
            dma_direction: 0,
            display_start: 0,
            horizontal_range: 0,
            vertical_range: 0,
            display_mode: 0,
            texture_disable_allowed: false,
            irq: false,
            odd_field: false,
            gpuread: 0,
            fifo: Vec::with_capacity(16),
            receive: Receive::Command,
            read: None,
            stats: PrimitiveStats::default(),
        };
        gpu.reset();
        gpu
    }

    /// GP1(00h): clears the command buffer and the interrupt, turns the
    /// display and DMA requests off, restores the default display area,
    /// ranges and mode, and zeroes GP0(E1h)-GP0(E6h). VRAM is kept.
    fn reset(&mut self) {
        self.reset_command_buffer();
        self.irq = false;
        self.display_disabled = true;
        self.dma_direction = 0;
        self.display_start = 0;
        self.horizontal_range = 0x200 | (0x200 + 256 * 10) << 12;
        self.vertical_range = 0x10 | (0x10 + 240) << 10;
        self.display_mode = 0;
        self.draw_mode = 0;
        self.texture_window = 0;
        self.area_top_left = 0;
        self.area_bottom_right = 0;
        self.draw_offset = 0;
        self.mask = 0;
        self.odd_field = false;
    }

    /// GP1(01h): drops a partly received command, polyline or transfer.
    fn reset_command_buffer(&mut self) {
        self.fifo.clear();
        self.receive = Receive::Command;
        self.read = None;
    }

    /// Writes one word to GP0.
    pub fn write_gp0(&mut self, word: u32) {
        match self.receive {
            Receive::ImageLoad(image) => return self.load_image_word(image, word),
            Receive::Polyline { .. } => return self.polyline_word(word),
            Receive::Command => {}
        }
        self.fifo.push(word);
        if self.fifo.len() < command_words(self.fifo[0]) {
            return;
        }
        let words = std::mem::take(&mut self.fifo);
        self.execute(&words);
        self.fifo = words;
        self.fifo.clear();
    }

    /// Writes GP0 words in order.
    pub fn write_gp0_words(&mut self, words: &[u32]) {
        for &word in words {
            self.write_gp0(word);
        }
    }

    fn execute(&mut self, words: &[u32]) {
        let op = words[0] >> 24;
        match op {
            0x02 => {
                self.stats.add(PrimitiveKind::Fill);
                self.fill_rectangle(words);
            }
            0x1F => {
                self.stats.add(PrimitiveKind::Other);
                self.irq = true;
            }
            0x20..=0x3F => self.draw_polygon(words),
            0x40..=0x5F => {
                let polyline = op & 0x08 != 0;
                let gouraud = op & 0x10 != 0;
                self.stats.add(if polyline {
                    PrimitiveKind::Polyline
                } else {
                    PrimitiveKind::Line
                });
                let (color1, vertex1) = if gouraud {
                    (words[2], words[3])
                } else {
                    (words[0], words[2])
                };
                self.draw_line(words[0], words[0], words[1], color1, vertex1);
                if polyline {
                    self.receive = Receive::Polyline {
                        command: words[0],
                        color: color1,
                        vertex: vertex1,
                        next_color: None,
                    };
                }
            }
            0x60..=0x7F => self.draw_rectangle(words),
            0x80..=0x9F => {
                self.stats.add(PrimitiveKind::VramCopy);
                self.copy_rectangle(words);
            }
            0xA0..=0xBF => {
                self.stats.add(PrimitiveKind::CpuToVram);
                self.receive = Receive::ImageLoad(Image::from_words(words[1], words[2]));
            }
            0xC0..=0xDF => {
                self.stats.add(PrimitiveKind::VramToCpu);
                self.read = Some(Image::from_words(words[1], words[2]));
            }
            0xE1..=0xE6 => {
                self.stats.add(PrimitiveKind::Environment);
                let word = words[0];
                match op {
                    0xE1 => self.draw_mode = word & 0x3FFF,
                    0xE2 => self.texture_window = word & 0xF_FFFF,
                    0xE3 => self.area_top_left = word & 0x7_FFFF,
                    0xE4 => self.area_bottom_right = word & 0x7_FFFF,
                    0xE5 => self.draw_offset = word & 0x3F_FFFF,
                    _ => self.mask = word & 3,
                }
            }
            // NOP, GP0(01h) clear cache, GP0(03h) and the unused numbers.
            _ => self.stats.add(PrimitiveKind::Other),
        }
    }

    fn polyline_word(&mut self, word: u32) {
        let Receive::Polyline {
            command,
            color,
            vertex,
            next_color,
        } = self.receive
        else {
            unreachable!("polyline_word outside a polyline");
        };
        let gouraud = command & 0x1000_0000 != 0;
        if gouraud && next_color.is_none() {
            self.receive = if is_polyline_end(word) {
                Receive::Command
            } else {
                Receive::Polyline {
                    command,
                    color,
                    vertex,
                    next_color: Some(word),
                }
            };
            return;
        }
        if !gouraud && is_polyline_end(word) {
            self.receive = Receive::Command;
            return;
        }
        let new_color = next_color.unwrap_or(color);
        self.draw_line(command, color, vertex, new_color, word);
        self.receive = Receive::Polyline {
            command,
            color: new_color,
            vertex: word,
            next_color: None,
        };
    }

    fn load_image_word(&mut self, mut image: Image, word: u32) {
        let total = image.pixels();
        for pixel in [word as u16, (word >> 16) as u16] {
            if image.index < total {
                let index = image.vram_index(image.index);
                self.write_masked(index, pixel);
                image.index += 1;
            }
        }
        self.receive = if image.index >= total {
            Receive::Command
        } else {
            Receive::ImageLoad(image)
        };
    }

    /// Writes a pixel the way transfers and copies do: GP0(E6h) can protect
    /// masked pixels and force bit 15.
    fn write_masked(&mut self, index: usize, pixel: u16) {
        if self.mask & 2 != 0 && self.vram[index] & 0x8000 != 0 {
            return;
        }
        self.vram[index] = pixel | if self.mask & 1 != 0 { 0x8000 } else { 0 };
    }

    /// Writes one word to GP1.
    pub fn write_gp1(&mut self, word: u32) {
        let parameter = word & 0xFF_FFFF;
        match (word >> 24) & 0x3F {
            0x00 => self.reset(),
            0x01 => self.reset_command_buffer(),
            0x02 => self.irq = false,
            0x03 => self.display_disabled = parameter & 1 != 0,
            0x04 => self.dma_direction = parameter & 3,
            0x05 => self.display_start = parameter & 0x7_FFFF,
            0x06 => self.horizontal_range = parameter,
            0x07 => self.vertical_range = parameter & 0xF_FFFF,
            0x08 => self.display_mode = parameter & 0xFF,
            0x09 => self.texture_disable_allowed = parameter & 1 != 0,
            0x10..=0x1F => {
                // GPU info: 2-5 return the drawing settings, 7 the GPU
                // version (2); the others leave GPUREAD unchanged.
                match parameter & 7 {
                    2 => self.gpuread = self.texture_window,
                    3 => self.gpuread = self.area_top_left,
                    4 => self.gpuread = self.area_bottom_right,
                    5 => self.gpuread = self.draw_offset,
                    7 => self.gpuread = 2,
                    _ => {}
                }
            }
            _ => {}
        }
    }

    /// Reads GPUREAD: the next two pixels of a VRAM-to-CPU transfer, or the
    /// latched value (the last pixels read or the last GP1(10h) answer).
    pub fn read_gpuread(&mut self) -> u32 {
        if let Some(mut image) = self.read {
            let total = image.pixels();
            let mut word = 0;
            for half in 0..2 {
                if image.index < total {
                    let pixel = self.vram[image.vram_index(image.index)];
                    word |= u32::from(pixel) << (16 * half);
                    image.index += 1;
                }
            }
            self.read = if image.index >= total {
                None
            } else {
                Some(image)
            };
            self.gpuread = word;
        }
        self.gpuread
    }

    /// GPUSTAT. The GPU never stalls, so the ready bits (26, 28) are set.
    pub fn gpustat(&self) -> u32 {
        let mode = self.display_mode;
        let interlaced = mode & 0x20 != 0;
        let read_ready = self.read.is_some();
        let mut status = self.draw_mode & 0x7FF;
        status |= (self.draw_mode >> 11 & 1) << 15;
        status |= (self.mask & 1) << 11 | (self.mask >> 1 & 1) << 12;
        status |= u32::from(!interlaced || self.odd_field) << 13;
        status |= (mode >> 7 & 1) << 14;
        status |= (mode >> 6 & 1) << 16;
        status |= (mode & 3) << 17;
        status |= (mode >> 2 & 1) << 19;
        status |= (mode >> 3 & 1) << 20;
        status |= (mode >> 4 & 1) << 21;
        status |= (mode >> 5 & 1) << 22;
        status |= u32::from(self.display_disabled) << 23;
        status |= u32::from(self.irq) << 24;
        let request = match self.dma_direction {
            0 => false,
            1 | 2 => true,
            _ => read_ready,
        };
        status |= u32::from(request) << 25;
        status |= 1 << 26;
        status |= u32::from(read_ready) << 27;
        status |= 1 << 28;
        status |= self.dma_direction << 29;
        status |= u32::from(interlaced && self.odd_field) << 31;
        status
    }

    /// Marks a vertical blank: in interlaced modes the displayed field
    /// changes (GPUSTAT.13 and .31).
    pub fn vblank(&mut self) {
        if self.display_mode & 0x20 != 0 {
            self.odd_field = !self.odd_field;
        }
    }

    /// Whether GP0(1Fh) has requested an interrupt not yet acknowledged.
    pub fn irq_pending(&self) -> bool {
        self.irq
    }

    /// The whole VRAM, 1024x512 row-major 16-bit pixels (bit 15 is the mask).
    pub fn vram(&self) -> &[u16] {
        &self.vram
    }

    /// Mutable VRAM, for hosts that restore or inspect it directly.
    pub fn vram_mut(&mut self) -> &mut [u16] {
        &mut self.vram
    }

    /// Counters since the last [`Gpu::take_stats`].
    pub fn stats(&self) -> &PrimitiveStats {
        &self.stats
    }

    /// Returns the counters and starts a new count (call once per frame).
    pub fn take_stats(&mut self) -> PrimitiveStats {
        std::mem::take(&mut self.stats)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn command_lengths() {
        assert_eq!(command_words(0x2C00_0000), 9);
        assert_eq!(command_words(0x3C00_0000), 12);
        assert_eq!(command_words(0x3000_0000), 6);
        assert_eq!(command_words(0x2800_0000), 5);
        assert_eq!(command_words(0x6400_0000), 4);
        assert_eq!(command_words(0x7800_0000), 2);
        assert_eq!(command_words(0x7C00_0000), 3);
        assert_eq!(command_words(0x4000_0000), 3);
        assert_eq!(command_words(0x5000_0000), 4);
        assert_eq!(command_words(0x0200_0000), 3);
        assert_eq!(command_words(0x8000_0000), 4);
        assert_eq!(command_words(0xE100_0000), 1);
    }

    #[test]
    fn polyline_end_marker() {
        assert!(is_polyline_end(0x5555_5555));
        assert!(is_polyline_end(0x5000_5000));
        assert!(!is_polyline_end(0x0050_0050));
    }
}
