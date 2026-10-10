//! Versioned, deterministic snapshots of the whole GPU.
//!
//! Layout (little-endian): the magic `XEMGPU\0\0`, the format version (u32),
//! VRAM (1024x512 u16), the drawing registers E1-E6, the display registers,
//! the flags, GPUREAD, then the receiver: the partial command words, the
//! polyline or CPU-to-VRAM transfer in progress and the VRAM-to-CPU transfer
//! in progress. The per-frame statistics are not state and are not saved.

use crate::{Gpu, Image, Receive, VRAM_HEIGHT, VRAM_WIDTH};

const MAGIC: &[u8; 8] = b"XEMGPU\0\0";
const VERSION: u32 = 1;
/// Longest partial command: a Gouraud textured quad is 12 words.
const MAX_FIFO: usize = 12;

/// Why [`Gpu::load`] refused a snapshot; the GPU is left unchanged.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SnapshotError {
    /// The data does not start with the GPU snapshot magic.
    Magic,
    /// The snapshot was written by an unsupported format version.
    Version(u32),
    /// The data ends early or has bytes after the snapshot.
    Length,
    /// A field holds a value no GPU state produces.
    Invalid(&'static str),
}

impl std::fmt::Display for SnapshotError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            SnapshotError::Magic => write!(f, "not a GPU snapshot"),
            SnapshotError::Version(version) => {
                write!(f, "unsupported GPU snapshot version {version}")
            }
            SnapshotError::Length => write!(f, "GPU snapshot has the wrong length"),
            SnapshotError::Invalid(field) => write!(f, "GPU snapshot has an invalid {field}"),
        }
    }
}

impl std::error::Error for SnapshotError {}

struct Writer(Vec<u8>);

impl Writer {
    fn u8(&mut self, value: u8) {
        self.0.push(value);
    }
    fn bool(&mut self, value: bool) {
        self.u8(u8::from(value));
    }
    fn u16(&mut self, value: u16) {
        self.0.extend_from_slice(&value.to_le_bytes());
    }
    fn u32(&mut self, value: u32) {
        self.0.extend_from_slice(&value.to_le_bytes());
    }
    fn image(&mut self, image: &Image) {
        self.u16(image.x);
        self.u16(image.y);
        self.u16(image.width);
        self.u16(image.height);
        self.u32(image.index);
    }
}

struct Reader<'a>(&'a [u8]);

impl Reader<'_> {
    fn take<const N: usize>(&mut self) -> Result<[u8; N], SnapshotError> {
        let (head, rest) = self
            .0
            .split_first_chunk::<N>()
            .ok_or(SnapshotError::Length)?;
        self.0 = rest;
        Ok(*head)
    }
    fn u8(&mut self) -> Result<u8, SnapshotError> {
        Ok(self.take::<1>()?[0])
    }
    fn bool(&mut self) -> Result<bool, SnapshotError> {
        match self.u8()? {
            0 => Ok(false),
            1 => Ok(true),
            _ => Err(SnapshotError::Invalid("flag")),
        }
    }
    fn u16(&mut self) -> Result<u16, SnapshotError> {
        Ok(u16::from_le_bytes(self.take()?))
    }
    fn u32(&mut self) -> Result<u32, SnapshotError> {
        Ok(u32::from_le_bytes(self.take()?))
    }
    /// A register value that must fit in `mask`.
    fn bits(&mut self, mask: u32, field: &'static str) -> Result<u32, SnapshotError> {
        let value = self.u32()?;
        if value & !mask != 0 {
            return Err(SnapshotError::Invalid(field));
        }
        Ok(value)
    }
    fn image(&mut self) -> Result<Image, SnapshotError> {
        let image = Image {
            x: self.u16()?,
            y: self.u16()?,
            width: self.u16()?,
            height: self.u16()?,
            index: self.u32()?,
        };
        let valid = usize::from(image.x) < VRAM_WIDTH
            && usize::from(image.y) < VRAM_HEIGHT
            && (1..=VRAM_WIDTH).contains(&usize::from(image.width))
            && (1..=VRAM_HEIGHT).contains(&usize::from(image.height))
            && image.index < image.pixels();
        if !valid {
            return Err(SnapshotError::Invalid("transfer"));
        }
        Ok(image)
    }
}

impl Gpu {
    /// Serializes the whole device state: VRAM, every register and any
    /// command, polyline or transfer in progress.
    pub fn save(&self) -> Vec<u8> {
        let mut w = Writer(Vec::with_capacity(VRAM_WIDTH * VRAM_HEIGHT * 2 + 256));
        w.0.extend_from_slice(MAGIC);
        w.u32(VERSION);
        for &pixel in &self.vram {
            w.u16(pixel);
        }
        for value in [
            self.draw_mode,
            self.texture_window,
            self.area_top_left,
            self.area_bottom_right,
            self.draw_offset,
            self.mask,
            self.dma_direction,
            self.display_start,
            self.horizontal_range,
            self.vertical_range,
            self.display_mode,
            self.gpuread,
        ] {
            w.u32(value);
        }
        for flag in [
            self.display_disabled,
            self.texture_disable_allowed,
            self.irq,
            self.odd_field,
        ] {
            w.bool(flag);
        }
        w.u8(self.fifo.len() as u8);
        for &word in &self.fifo {
            w.u32(word);
        }
        match self.receive {
            Receive::Command => w.u8(0),
            Receive::Polyline {
                command,
                color,
                vertex,
                next_color,
            } => {
                w.u8(1);
                w.u32(command);
                w.u32(color);
                w.u32(vertex);
                w.bool(next_color.is_some());
                w.u32(next_color.unwrap_or(0));
            }
            Receive::ImageLoad(image) => {
                w.u8(2);
                w.image(&image);
            }
        }
        w.bool(self.read.is_some());
        if let Some(image) = &self.read {
            w.image(image);
        }
        w.0
    }

    /// Restores a [`Gpu::save`] snapshot. On error the GPU is unchanged. The
    /// per-frame statistics are kept.
    pub fn load(&mut self, data: &[u8]) -> Result<(), SnapshotError> {
        let mut r = Reader(data);
        if &r.take::<8>()? != MAGIC {
            return Err(SnapshotError::Magic);
        }
        let version = r.u32()?;
        if version != VERSION {
            return Err(SnapshotError::Version(version));
        }
        let mut vram = vec![0u16; VRAM_WIDTH * VRAM_HEIGHT];
        for pixel in &mut vram {
            *pixel = r.u16()?;
        }
        let mut gpu = Gpu {
            vram,
            draw_mode: r.bits(0x3FFF, "draw mode")?,
            texture_window: r.bits(0xF_FFFF, "texture window")?,
            area_top_left: r.bits(0x7_FFFF, "drawing area")?,
            area_bottom_right: r.bits(0x7_FFFF, "drawing area")?,
            draw_offset: r.bits(0x3F_FFFF, "drawing offset")?,
            mask: r.bits(3, "mask setting")?,
            dma_direction: r.bits(3, "DMA direction")?,
            display_start: r.bits(0x7_FFFF, "display start")?,
            horizontal_range: r.bits(0xFF_FFFF, "horizontal range")?,
            vertical_range: r.bits(0xF_FFFF, "vertical range")?,
            display_mode: r.bits(0xFF, "display mode")?,
            gpuread: r.u32()?,
            display_disabled: r.bool()?,
            texture_disable_allowed: r.bool()?,
            irq: r.bool()?,
            odd_field: r.bool()?,
            fifo: Vec::with_capacity(16),
            receive: Receive::Command,
            read: None,
            stats: crate::PrimitiveStats::default(),
        };
        let pending = usize::from(r.u8()?);
        for _ in 0..pending {
            gpu.fifo.push(r.u32()?);
        }
        if pending > MAX_FIFO
            || gpu
                .fifo
                .first()
                .is_some_and(|&command| pending >= crate::command_words(command))
        {
            return Err(SnapshotError::Invalid("partial command"));
        }
        gpu.receive = match r.u8()? {
            0 => Receive::Command,
            1 => {
                let command = r.u32()?;
                let color = r.u32()?;
                let vertex = r.u32()?;
                let has_color = r.bool()?;
                let next_color = r.u32()?;
                if !(0x48..=0x5F).contains(&(command >> 24)) || command & 0x0800_0000 == 0 {
                    return Err(SnapshotError::Invalid("polyline"));
                }
                Receive::Polyline {
                    command,
                    color,
                    vertex,
                    next_color: has_color.then_some(next_color),
                }
            }
            2 => Receive::ImageLoad(r.image()?),
            _ => return Err(SnapshotError::Invalid("receiver")),
        };
        if gpu.receive != Receive::Command && !gpu.fifo.is_empty() {
            return Err(SnapshotError::Invalid("receiver"));
        }
        gpu.read = if r.bool()? { Some(r.image()?) } else { None };
        if !r.0.is_empty() {
            return Err(SnapshotError::Length);
        }
        gpu.stats = std::mem::take(&mut self.stats);
        *self = gpu;
        Ok(())
    }
}
