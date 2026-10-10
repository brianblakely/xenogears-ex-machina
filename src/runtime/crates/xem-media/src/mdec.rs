//! The MDEC (macroblock decoder) as a host device: the command/parameter
//! port (1F801820h), the status and control port (1F801824h), DMA0 input and
//! DMA1 output, following psx-spx "Macroblock Decoder (MDEC)".
//!
//! The device decodes as soon as a block's last halfword arrives, so its
//! output is complete and deterministic whenever the host looks at it; it has
//! no notion of time. Output words are in the order DMA1 stores them in RAM:
//! a colour macroblock as 16x16 pixels row by row (DMA1 re-orders the
//! hardware's four 8x8 luminance blocks), a monochrome block as 8x8 pixels.

use std::collections::VecDeque;

use crate::snapshot::{Reader, SnapshotError, Writer};

/// The zig-zag scan position of each coefficient in raster order
/// (psx-spx `zigzag[0..63]`).
pub const ZIGZAG: [u8; 64] = [
    0, 1, 5, 6, 14, 15, 27, 28, //
    2, 4, 7, 13, 16, 26, 29, 42, //
    3, 8, 12, 17, 25, 30, 41, 43, //
    9, 11, 18, 24, 31, 40, 44, 53, //
    10, 19, 23, 32, 39, 45, 52, 54, //
    20, 22, 33, 38, 46, 51, 55, 60, //
    21, 34, 37, 47, 50, 56, 59, 61, //
    35, 36, 48, 49, 57, 58, 62, 63,
];

/// The raster position of each zig-zag scan position (psx-spx `zagzig`).
pub const ZAGZIG: [u8; 64] = {
    let mut table = [0u8; 64];
    let mut i = 0;
    while i < 64 {
        table[ZIGZAG[i] as usize] = i as u8;
        i += 1;
    }
    table
};

/// The quantization table libpress's `DecDCTReset` loads for both luminance
/// and colour, in zig-zag order as the MDEC(2) command takes it (the movie
/// library carries the same 64 bytes twice after its 0x40000001 command word).
pub const DEFAULT_QUANT_TABLE: [u8; 64] = [
    2, 16, 16, 19, 16, 19, 22, 22, 22, 22, 22, 22, 26, 24, 26, 27, //
    27, 27, 26, 26, 26, 26, 27, 27, 27, 29, 29, 29, 34, 34, 34, 29, //
    29, 29, 27, 27, 29, 29, 32, 32, 34, 34, 37, 38, 37, 35, 35, 34, //
    35, 38, 38, 40, 40, 40, 48, 48, 46, 46, 56, 56, 58, 69, 69, 83,
];

/// The standard IDCT scale matrix of MDEC(3) (psx-spx `set_scale_table`):
/// row `f` holds the basis for frequency `f`, 14 fractional bits.
pub const DEFAULT_SCALE_TABLE: [i16; 64] = {
    const ROWS: [u16; 64] = [
        0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, //
        0x7D8A, 0x6A6D, 0x471C, 0x18F8, 0xE707, 0xB8E3, 0x9592, 0x8275, //
        0x7641, 0x30FB, 0xCF04, 0x89BE, 0x89BE, 0xCF04, 0x30FB, 0x7641, //
        0x6A6D, 0xE707, 0x8275, 0xB8E3, 0x471C, 0x7D8A, 0x18F8, 0x9592, //
        0x5A82, 0xA57D, 0xA57D, 0x5A82, 0x5A82, 0xA57D, 0xA57D, 0x5A82, //
        0x471C, 0x8275, 0x18F8, 0x6A6D, 0x9592, 0xE707, 0x7D8A, 0xB8E3, //
        0x30FB, 0x89BE, 0x7641, 0xCF04, 0xCF04, 0x7641, 0x89BE, 0x30FB, //
        0x18F8, 0xB8E3, 0x6A6D, 0x8275, 0x7D8A, 0x9592, 0x471C, 0xE707,
    ];
    let mut table = [0i16; 64];
    let mut i = 0;
    while i < 64 {
        table[i] = ROWS[i] as i16;
        i += 1;
    }
    table
};

/// The halfword that ends a block, and pads between blocks.
pub const END_OF_BLOCK: u16 = 0xFE00;

/// MDEC(1) output depth (command bits 27-28, status bits 25-26).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Depth {
    /// Monochrome, 4 bits per pixel.
    Bits4 = 0,
    /// Monochrome, 8 bits per pixel.
    Bits8 = 1,
    /// Colour, 24 bits per pixel (R, G, B bytes).
    Bits24 = 2,
    /// Colour, 15 bits per pixel (BGR555 halfwords).
    Bits15 = 3,
}

impl Depth {
    /// The depth encoded in two bits.
    pub const fn from_bits(bits: u32) -> Self {
        match bits & 3 {
            0 => Self::Bits4,
            1 => Self::Bits8,
            2 => Self::Bits24,
            _ => Self::Bits15,
        }
    }

    /// Colour depths decode six-block 16x16 macroblocks; monochrome depths
    /// decode single 8x8 blocks.
    pub const fn is_color(self) -> bool {
        matches!(self, Self::Bits24 | Self::Bits15)
    }

    /// Side of the square a macroblock covers, in pixels.
    pub const fn macroblock_size(self) -> usize {
        if self.is_color() { 16 } else { 8 }
    }

    /// Output words per macroblock.
    pub const fn macroblock_words(self) -> usize {
        match self {
            Self::Bits4 => 8,
            Self::Bits8 => 16,
            Self::Bits24 => 192,
            Self::Bits15 => 128,
        }
    }

    /// Bits per output pixel.
    pub const fn bits_per_pixel(self) -> usize {
        match self {
            Self::Bits4 => 4,
            Self::Bits8 => 8,
            Self::Bits24 => 24,
            Self::Bits15 => 16,
        }
    }
}

/// The MDEC(1) decode command word for `words` parameter words.
pub const fn decode_command(depth: Depth, signed: bool, bit15: bool, words: u16) -> u32 {
    (1 << 29)
        | ((depth as u32) << 27)
        | ((signed as u32) << 26)
        | ((bit15 as u32) << 25)
        | words as u32
}

/// The MDEC(2) command word: luminance table, plus the colour table if `color`.
pub const fn set_quant_command(color: bool) -> u32 {
    (2 << 29) | color as u32
}

/// The MDEC(3) command word.
pub const SET_SCALE_COMMAND: u32 = 3 << 29;

/// Control register (1F801824h write) bits.
pub mod control {
    /// Abort any command and reset the status to 80040000h.
    pub const RESET: u32 = 1 << 31;
    /// Enable DMA0 (data in) requests.
    pub const ENABLE_DATA_IN: u32 = 1 << 30;
    /// Enable DMA1 (data out) requests.
    pub const ENABLE_DATA_OUT: u32 = 1 << 29;
}

/// Status register (1F801824h read) bits.
pub mod status {
    /// Data-out FIFO empty.
    pub const OUT_EMPTY: u32 = 1 << 31;
    /// Data-in FIFO full, or the command's last parameter word received.
    pub const IN_FULL: u32 = 1 << 30;
    /// Busy receiving or processing parameters.
    pub const BUSY: u32 = 1 << 29;
    /// Data-in request (DMA0).
    pub const DATA_IN_REQUEST: u32 = 1 << 28;
    /// Data-out request (DMA1).
    pub const DATA_OUT_REQUEST: u32 = 1 << 27;
}

/// YCbCr to RGB coefficients of psx-spx `yuv_to_rgb` (1.402, -0.3437,
/// -0.7143, 1.772) with 12 fractional bits.
const R_CR: i32 = 5743;
const G_CB: i32 = -1408;
const G_CR: i32 = -2926;
const B_CB: i32 = 7258;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Phase {
    Idle = 0,
    Decode = 1,
    Quant = 2,
    Scale = 3,
}

/// The MDEC.
#[derive(Clone, Debug)]
pub struct Mdec {
    iq_y: [u8; 64],
    iq_uv: [u8; 64],
    scale: [i16; 64],
    /// The last command word (status bits 23-26 reflect its bits 25-28).
    command: u32,
    phase: Phase,
    /// Parameter words the current command still expects.
    remaining: u32,
    /// Parameter words the current table command received.
    received: u32,
    /// Status bits 0-15 while no command takes parameters.
    idle_count: u16,
    last_word: bool,
    enable_in: bool,
    enable_out: bool,
    /// Block within the macroblock being received (0..5 = Cr, Cb, Y1..Y4).
    block: u8,
    in_block: bool,
    k: u8,
    q_scale: u8,
    coefficients: [i16; 64],
    /// IDCT results: Cr, Cb, Y1, Y2, Y3, Y4 (monochrome uses the first).
    blocks: [[i16; 64]; 6],
    output: VecDeque<u32>,
    /// Words of the head macroblock already read, for the current-block field.
    output_read: u32,
}

impl Default for Mdec {
    fn default() -> Self {
        Self::new()
    }
}

impl Mdec {
    /// A reset MDEC holding the tables libpress's `DecDCTReset` loads (the
    /// hardware's power-on tables are undocumented; games load their own).
    pub fn new() -> Self {
        Self {
            iq_y: DEFAULT_QUANT_TABLE,
            iq_uv: DEFAULT_QUANT_TABLE,
            scale: DEFAULT_SCALE_TABLE,
            command: 0,
            phase: Phase::Idle,
            remaining: 0,
            received: 0,
            idle_count: 0,
            last_word: false,
            enable_in: false,
            enable_out: false,
            block: 0,
            in_block: false,
            k: 0,
            q_scale: 0,
            coefficients: [0; 64],
            blocks: [[0; 64]; 6],
            output: VecDeque::new(),
            output_read: 0,
        }
    }

    /// The luminance quantization table (zig-zag order).
    pub fn luma_quant_table(&self) -> &[u8; 64] {
        &self.iq_y
    }

    /// The colour quantization table (zig-zag order).
    pub fn chroma_quant_table(&self) -> &[u8; 64] {
        &self.iq_uv
    }

    /// The IDCT scale table.
    pub fn scale_table(&self) -> &[i16; 64] {
        &self.scale
    }

    fn depth(&self) -> Depth {
        Depth::from_bits(self.command >> 27)
    }

    /// Write the control register (1F801824h).
    pub fn control(&mut self, value: u32) {
        if value & control::RESET != 0 {
            self.command = 0;
            self.phase = Phase::Idle;
            self.remaining = 0;
            self.received = 0;
            self.idle_count = 0;
            self.last_word = false;
            self.reset_parser();
            self.output.clear();
            self.output_read = 0;
        }
        self.enable_in = value & control::ENABLE_DATA_IN != 0;
        self.enable_out = value & control::ENABLE_DATA_OUT != 0;
    }

    /// Write the command/parameter register (1F801820h): a parameter word
    /// while a command expects one, otherwise a new command.
    pub fn command(&mut self, word: u32) {
        if self.phase == Phase::Idle {
            self.start(word);
        } else {
            self.parameter(word);
        }
    }

    /// DMA0: hand parameter words to the current command. Returns how many
    /// were taken; it stops when the command has all its parameters.
    pub fn push_input(&mut self, words: &[u32]) -> usize {
        let mut taken = 0;
        for &word in words {
            if self.phase == Phase::Idle {
                break;
            }
            self.parameter(word);
            taken += 1;
        }
        taken
    }

    /// DMA1 (or reads of 1F801820h): move decoded words into `buf`. Returns
    /// how many were written.
    pub fn pop_output(&mut self, buf: &mut [u32]) -> usize {
        let n = buf.len().min(self.output.len());
        for slot in &mut buf[..n] {
            *slot = self.output.pop_front().unwrap_or_default();
        }
        if self.output.is_empty() {
            self.output_read = 0;
        } else {
            self.output_read =
                (self.output_read + n as u32) % self.depth().macroblock_words() as u32;
        }
        n
    }

    /// Decoded words waiting for DMA1.
    pub fn output_len(&self) -> usize {
        self.output.len()
    }

    /// Read the status register (1F801824h).
    pub fn status(&self) -> u32 {
        let mut value = 0;
        if self.output.is_empty() {
            value |= status::OUT_EMPTY;
        }
        if self.last_word {
            value |= status::IN_FULL;
        }
        if self.phase != Phase::Idle || !self.output.is_empty() {
            value |= status::BUSY;
        }
        if self.enable_in && self.phase != Phase::Idle {
            value |= status::DATA_IN_REQUEST;
        }
        if self.enable_out && !self.output.is_empty() {
            value |= status::DATA_OUT_REQUEST;
        }
        value |= ((self.command >> 25) & 0xF) << 23;
        value |= self.current_block() << 16;
        value |= match self.phase {
            Phase::Idle => u32::from(self.idle_count),
            _ => self.remaining.wrapping_sub(1) & 0xFFFF,
        };
        value
    }

    /// Status bits 16-18: the block being output (Y1..Y4 = 0..3, mono 4)
    /// while output waits, else the block being received (Cr 4, Cb 5,
    /// Y1..Y4 0..3; mono 4).
    fn current_block(&self) -> u32 {
        let depth = self.depth();
        if !depth.is_color() {
            return 4;
        }
        if !self.output.is_empty() {
            return self.output_read / (depth.macroblock_words() as u32 / 4);
        }
        [4, 5, 0, 1, 2, 3][usize::from(self.block)]
    }

    fn start(&mut self, word: u32) {
        self.command = word;
        self.last_word = false;
        self.received = 0;
        match word >> 29 {
            1 => {
                self.reset_parser();
                self.begin(Phase::Decode, word & 0xFFFF);
            }
            2 => self.begin(Phase::Quant, if word & 1 != 0 { 32 } else { 16 }),
            3 => self.begin(Phase::Scale, 32),
            _ => self.idle_count = word as u16,
        }
    }

    fn begin(&mut self, phase: Phase, words: u32) {
        if words == 0 {
            self.finish_command();
        } else {
            self.phase = phase;
            self.remaining = words;
        }
    }

    fn finish_command(&mut self) {
        self.phase = Phase::Idle;
        self.remaining = 0;
        self.idle_count = 0xFFFF;
        self.last_word = true;
    }

    fn parameter(&mut self, word: u32) {
        match self.phase {
            Phase::Idle => return,
            Phase::Decode => {
                self.feed(word as u16);
                self.feed((word >> 16) as u16);
            }
            Phase::Quant => {
                let index = self.received as usize;
                let table = if index < 16 {
                    &mut self.iq_y
                } else {
                    &mut self.iq_uv
                };
                let at = (index % 16) * 4;
                table[at..at + 4].copy_from_slice(&word.to_le_bytes());
            }
            Phase::Scale => {
                let at = self.received as usize * 2;
                self.scale[at] = word as i16;
                self.scale[at + 1] = (word >> 16) as i16;
            }
        }
        self.received += 1;
        self.remaining -= 1;
        if self.remaining == 0 {
            self.finish_command();
        }
    }

    fn reset_parser(&mut self) {
        self.block = 0;
        self.in_block = false;
        self.k = 0;
        self.q_scale = 0;
    }

    fn quant_table(&self) -> &[u8; 64] {
        if self.depth().is_color() && self.block < 2 {
            &self.iq_uv
        } else {
            &self.iq_y
        }
    }

    /// psx-spx `rl_decode_block`, one halfword at a time.
    fn feed(&mut self, halfword: u16) {
        let level = signed10(halfword);
        if !self.in_block {
            if halfword == END_OF_BLOCK {
                return;
            }
            self.coefficients = [0; 64];
            self.q_scale = (halfword >> 10) as u8;
            self.k = 0;
            self.in_block = true;
            let value = if self.q_scale == 0 {
                level * 2
            } else {
                level * i32::from(self.quant_table()[0])
            };
            self.store(value);
            return;
        }
        let k = u32::from(self.k) + u32::from(halfword >> 10) + 1;
        if k > 63 {
            self.finish_block();
            return;
        }
        self.k = k as u8;
        let value = if self.q_scale == 0 {
            level * 2
        } else {
            (level * i32::from(self.quant_table()[k as usize]) * i32::from(self.q_scale) + 4) >> 3
        };
        self.store(value);
    }

    fn store(&mut self, value: i32) {
        let value = value.clamp(-0x400, 0x3FF) as i16;
        let k = usize::from(self.k);
        let at = if self.q_scale == 0 {
            k
        } else {
            usize::from(ZAGZIG[k])
        };
        self.coefficients[at] = value;
        if k == 63 {
            // A fully defined block needs no end code.
            self.finish_block();
        }
    }

    fn finish_block(&mut self) {
        self.in_block = false;
        let block = idct(&self.coefficients, &self.scale);
        let depth = self.depth();
        if depth.is_color() {
            self.blocks[usize::from(self.block)] = block;
            self.block += 1;
            if self.block == 6 {
                self.block = 0;
                self.emit_color(depth);
            }
        } else {
            self.blocks[0] = block;
            self.emit_mono(depth);
        }
    }

    fn output_byte(&self, value: i32) -> u8 {
        let value = value.clamp(-128, 127);
        if self.command & (1 << 26) != 0 {
            value as i8 as u8
        } else {
            (value + 128) as u8
        }
    }

    /// psx-spx `yuv_to_rgb` for the four luminance blocks, packed at `depth`.
    fn emit_color(&mut self, depth: Depth) {
        let mut rgb = [[0u8; 3]; 256];
        for (quarter, (xx, yy)) in [(0, 0), (8, 0), (0, 8), (8, 8)].into_iter().enumerate() {
            let luma = &self.blocks[2 + quarter];
            for y in 0..8 {
                for x in 0..8 {
                    let c = (x + xx) / 2 + (y + yy) / 2 * 8;
                    let cr = i32::from(self.blocks[0][c]);
                    let cb = i32::from(self.blocks[1][c]);
                    let r = (R_CR * cr + 2048) >> 12;
                    let g = (G_CB * cb + G_CR * cr + 2048) >> 12;
                    let b = (B_CB * cb + 2048) >> 12;
                    let luma = i32::from(luma[x + y * 8]);
                    rgb[(x + xx) + (y + yy) * 16] = [
                        self.output_byte(luma + r),
                        self.output_byte(luma + g),
                        self.output_byte(luma + b),
                    ];
                }
            }
        }
        if depth == Depth::Bits24 {
            let bytes: Vec<u8> = rgb.iter().flatten().copied().collect();
            for word in bytes.chunks_exact(4) {
                self.output
                    .push_back(u32::from_le_bytes([word[0], word[1], word[2], word[3]]));
            }
        } else {
            let stp = u32::from(self.command & (1 << 25) != 0) << 15;
            for pair in rgb.chunks_exact(2) {
                let pixel = |[r, g, b]: [u8; 3]| {
                    u32::from(r >> 3) | u32::from(g >> 3) << 5 | u32::from(b >> 3) << 10 | stp
                };
                self.output.push_back(pixel(pair[0]) | pixel(pair[1]) << 16);
            }
        }
    }

    /// psx-spx `y_to_mono`, packed at `depth`.
    fn emit_mono(&mut self, depth: Depth) {
        let mut pixels = [0u8; 64];
        for (pixel, &value) in pixels.iter_mut().zip(&self.blocks[0]) {
            // Clip to signed 9 bits, then saturate to 8.
            let value = (i32::from(value) << 23) >> 23;
            *pixel = self.output_byte(value);
        }
        if depth == Depth::Bits8 {
            for word in pixels.chunks_exact(4) {
                self.output
                    .push_back(u32::from_le_bytes([word[0], word[1], word[2], word[3]]));
            }
        } else {
            for eight in pixels.chunks_exact(8) {
                let word = eight
                    .iter()
                    .enumerate()
                    .fold(0u32, |w, (i, &p)| w | u32::from(p >> 4) << (i * 4));
                self.output.push_back(word);
            }
        }
    }

    /// Decode a whole picture of `width` x `height` pixels from run-level
    /// halfwords (a frame's data after the MDEC(1) command word), as the
    /// movie code lays it out: macroblocks fill columns top to bottom, columns
    /// left to right. Returns the pixels row by row: R, G, B bytes at 24 bits,
    /// little-endian BGR555 halfwords at 15 bits, one byte per pixel at 8 bits
    /// and two pixels per byte (first in the low nibble) at 4 bits. Output is
    /// unsigned with bit 15 clear. The device is reset first (its tables are
    /// kept) and is left idle.
    ///
    /// # Panics
    /// If the halfwords exceed the 0xFFFF words one MDEC(1) command can take.
    pub fn decode_frame(
        &mut self,
        rle: &[u16],
        width: usize,
        height: usize,
        depth: Depth,
    ) -> Vec<u8> {
        let words: Vec<u32> = rle
            .chunks(2)
            .map(|pair| u32::from(pair[0]) | u32::from(*pair.get(1).unwrap_or(&END_OF_BLOCK)) << 16)
            .collect();
        let count =
            u16::try_from(words.len()).expect("one MDEC(1) command takes at most 0xFFFF words");
        self.control(control::RESET);
        self.command(decode_command(depth, false, false, count));
        self.push_input(&words);
        let mut decoded = vec![0u32; self.output_len()];
        self.pop_output(&mut decoded);

        let size = depth.macroblock_size();
        let rows = height.div_ceil(size);
        let mb_bytes = depth.macroblock_words() * 4;
        let bytes: Vec<u8> = decoded.iter().flat_map(|w| w.to_le_bytes()).collect();
        let row_bits = width * depth.bits_per_pixel();
        let mut image = vec![0u8; row_bits.div_ceil(8) * height];
        let row_bytes = row_bits.div_ceil(8);
        let mb_row_bits = size * depth.bits_per_pixel();
        for (index, macroblock) in bytes.chunks_exact(mb_bytes).enumerate() {
            let (column, row) = (index / rows, index % rows);
            for y in 0..size {
                let py = row * size + y;
                if py >= height {
                    break;
                }
                for x in 0..size {
                    let px = column * size + x;
                    if px >= width {
                        break;
                    }
                    let bits = depth.bits_per_pixel();
                    let src_bit = y * mb_row_bits + x * bits;
                    let dst_bit = py * row_bytes * 8 + px * bits;
                    if bits == 4 {
                        let nibble = macroblock[src_bit / 8] >> (src_bit % 8) & 0xF;
                        image[dst_bit / 8] |= nibble << (dst_bit % 8);
                    } else {
                        let n = bits / 8;
                        image[dst_bit / 8..dst_bit / 8 + n]
                            .copy_from_slice(&macroblock[src_bit / 8..src_bit / 8 + n]);
                    }
                }
            }
        }
        image
    }

    const TAG: &'static [u8; 4] = b"MDEC";
    const VERSION: u8 = 1;

    /// Serialize the whole device state.
    pub fn save(&self) -> Vec<u8> {
        let mut w = Writer::new(Self::TAG, Self::VERSION);
        w.bytes(&self.iq_y);
        w.bytes(&self.iq_uv);
        for &v in &self.scale {
            w.i16(v);
        }
        w.u32(self.command);
        w.u8(self.phase as u8);
        w.u32(self.remaining);
        w.u32(self.received);
        w.u16(self.idle_count);
        w.bool(self.last_word);
        w.bool(self.enable_in);
        w.bool(self.enable_out);
        w.u8(self.block);
        w.bool(self.in_block);
        w.u8(self.k);
        w.u8(self.q_scale);
        for &v in self.coefficients.iter().chain(self.blocks.iter().flatten()) {
            w.i16(v);
        }
        w.u32(self.output.len() as u32);
        for &v in &self.output {
            w.u32(v);
        }
        w.u32(self.output_read);
        w.finish()
    }

    /// Restore a state written by [`Mdec::save`]. On error the device is unchanged.
    pub fn load(&mut self, data: &[u8]) -> Result<(), SnapshotError> {
        let mut r = Reader::new(data, Self::TAG, Self::VERSION)?;
        let mut next = Self::new();
        r.bytes(&mut next.iq_y)?;
        r.bytes(&mut next.iq_uv)?;
        for v in &mut next.scale {
            *v = r.i16()?;
        }
        next.command = r.u32()?;
        next.phase = match r.u8()? {
            0 => Phase::Idle,
            1 => Phase::Decode,
            2 => Phase::Quant,
            3 => Phase::Scale,
            _ => return Err(SnapshotError::InvalidValue("mdec phase")),
        };
        next.remaining = r.u32()?;
        next.received = r.u32()?;
        next.idle_count = r.u16()?;
        next.last_word = r.bool()?;
        next.enable_in = r.bool()?;
        next.enable_out = r.bool()?;
        next.block = r.u8()?;
        next.in_block = r.bool()?;
        next.k = r.u8()?;
        next.q_scale = r.u8()?;
        for v in next
            .coefficients
            .iter_mut()
            .chain(next.blocks.iter_mut().flatten())
        {
            *v = r.i16()?;
        }
        let count = r.u32()?;
        for _ in 0..count {
            next.output.push_back(r.u32()?);
        }
        next.output_read = r.u32()?;
        r.finish()?;
        let table_words = match next.phase {
            Phase::Quant => 32,
            Phase::Scale => 32,
            _ => u32::MAX,
        };
        if next.block > 5
            || next.k > 63
            || next.q_scale > 63
            || (next.phase == Phase::Idle) != (next.remaining == 0)
            || next.received.saturating_add(next.remaining) > table_words
        {
            return Err(SnapshotError::InvalidValue("mdec command state"));
        }
        *self = next;
        Ok(())
    }
}

fn signed10(halfword: u16) -> i32 {
    (i32::from(halfword) << 22) >> 22
}

/// psx-spx `real_idct_core`: two passes of the scale matrix (its upper 13
/// bits) over the block, each rounding off 13 fractional bits.
pub fn idct(block: &[i16; 64], scale: &[i16; 64]) -> [i16; 64] {
    let mut src = [0i32; 64];
    for (s, &b) in src.iter_mut().zip(block) {
        *s = i32::from(b);
    }
    let mut dst = [0i32; 64];
    for _pass in 0..2 {
        for x in 0..8 {
            for y in 0..8 {
                let mut sum = 0i64;
                for z in 0..8 {
                    sum += i64::from(src[y + z * 8]) * i64::from(scale[x + z * 8] >> 3);
                }
                dst[x + y * 8] = ((sum + 0xFFF) >> 13) as i32;
            }
        }
        std::mem::swap(&mut src, &mut dst);
    }
    let mut out = [0i16; 64];
    for (o, &s) in out.iter_mut().zip(&src) {
        *o = s.clamp(i32::from(i16::MIN), i32::from(i16::MAX)) as i16;
    }
    out
}
