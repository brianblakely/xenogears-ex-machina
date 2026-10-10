//! CD-XA ADPCM audio as the drive plays it into the SPU's CD input,
//! following psx-spx "CDROM XA Audio ADPCM Compression" and "Data/ADPCM
//! Sector Filtering/Delivery": the drive's choice of sectors (Setmode bits 6
//! and 3, Setfilter), the decoder with per-channel history kept across
//! sectors, and the zig-zag resampler to 44100 Hz stereo.

use std::collections::VecDeque;

use crate::snapshot::{Reader, SnapshotError, Writer};

/// Bytes of ADPCM data in a Form 2 sector: 18 sound groups of 128 bytes.
pub const SECTOR_AUDIO_BYTES: usize = 0x900;
/// Sound groups per sector.
pub const SOUND_GROUPS: usize = 18;
/// Samples each sound unit holds.
pub const UNIT_SAMPLES: usize = 28;

/// Submode bits (third subheader byte).
pub mod submode {
    pub const END_OF_RECORD: u8 = 0x01;
    pub const VIDEO: u8 = 0x02;
    pub const AUDIO: u8 = 0x04;
    pub const DATA: u8 = 0x08;
    pub const TRIGGER: u8 = 0x10;
    pub const FORM2: u8 = 0x20;
    pub const REAL_TIME: u8 = 0x40;
    pub const END_OF_FILE: u8 = 0x80;
}

/// Setmode bits the ADPCM path reads.
pub mod mode {
    /// Send XA-ADPCM sectors to the SPU's audio input.
    pub const XA_ADPCM: u8 = 0x40;
    /// Process only ADPCM sectors matching Setfilter.
    pub const XA_FILTER: u8 = 0x08;
}

/// The first four subheader bytes.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Subheader {
    pub file: u8,
    pub channel: u8,
    pub submode: u8,
    pub coding: u8,
}

impl Subheader {
    pub fn from_bytes(bytes: [u8; 4]) -> Self {
        Self {
            file: bytes[0],
            channel: bytes[1],
            submode: bytes[2],
            coding: bytes[3],
        }
    }

    /// Audio and real-time, the submode the drive plays.
    pub fn is_realtime_audio(self) -> bool {
        let both = submode::AUDIO | submode::REAL_TIME;
        self.submode & both == both
    }
}

/// The coding information byte of an audio sector.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Coding {
    pub stereo: bool,
    /// 18900 Hz rather than 37800 Hz.
    pub half_rate: bool,
    /// 8-bit rather than 4-bit samples.
    pub eight_bit: bool,
    pub emphasis: bool,
}

impl Coding {
    /// Decode the byte; the reserved values of each field read as their
    /// lowest bit (stereo bit 0, rate bit 2, depth bit 4).
    pub fn from_byte(byte: u8) -> Self {
        Self {
            stereo: byte & 0x01 != 0,
            half_rate: byte & 0x04 != 0,
            eight_bit: byte & 0x10 != 0,
            emphasis: byte & 0x40 != 0,
        }
    }

    /// Sample rate of the decoded audio.
    pub fn sample_rate(self) -> u32 {
        if self.half_rate { 18900 } else { 37800 }
    }

    /// Samples per channel one sector decodes to.
    pub fn samples_per_channel(self) -> usize {
        let units = if self.eight_bit { 4 } else { 8 };
        let per_sector = SOUND_GROUPS * units * UNIT_SAMPLES;
        if self.stereo {
            per_sector / 2
        } else {
            per_sector
        }
    }

    /// 44100 Hz stereo frames one sector yields.
    pub fn output_frames(self) -> usize {
        let at_37800 = self.samples_per_channel() * if self.half_rate { 2 } else { 1 };
        at_37800 / 6 * 7
    }
}

const POS: [i32; 4] = [0, 60, 115, 98];
const NEG: [i32; 4] = [0, 0, -52, -55];

/// The previous two samples of one channel.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct History {
    pub old: i16,
    pub older: i16,
}

/// Decode 0x900 bytes of ADPCM (18 sound groups) into per-channel samples
/// (`left` alone for mono), carrying `history` across calls. psx-spx
/// `decode_sector`: units in each group decode in order; in stereo even
/// units are left and odd units right.
pub fn decode_adpcm(
    data: &[u8],
    coding: Coding,
    history: &mut [History; 2],
    left: &mut Vec<i16>,
    right: &mut Vec<i16>,
) {
    let units = if coding.eight_bit { 4 } else { 8 };
    for group in data.chunks_exact(128).take(SOUND_GROUPS) {
        for unit in 0..units {
            let channel = if coding.stereo { unit & 1 } else { 0 };
            let out = if channel == 0 {
                &mut *left
            } else {
                &mut *right
            };
            let header = group[4 + unit];
            let mut shift = u32::from(header & 0x0F);
            if shift > 12 {
                shift = 9;
            }
            let filter = usize::from(header >> 4 & 3);
            let h = &mut history[channel];
            for j in 0..UNIT_SAMPLES {
                let word = u32::from_le_bytes([
                    group[16 + j * 4],
                    group[17 + j * 4],
                    group[18 + j * 4],
                    group[19 + j * 4],
                ]);
                // Expand to 16 bits, then shift right by the header's amount.
                let raw = if coding.eight_bit {
                    i32::from((word >> (unit * 8)) as u8 as i8) << 8
                } else {
                    (((word >> (unit * 4)) as i32) << 28 >> 28) << 12
                };
                let predicted =
                    (i32::from(h.old) * POS[filter] + i32::from(h.older) * NEG[filter] + 32) >> 6;
                let sample = ((raw >> shift) + predicted).clamp(-0x8000, 0x7FFF) as i16;
                out.push(sample);
                h.older = h.old;
                h.old = sample;
            }
        }
    }
}

/// psx-spx's 25-point zig-zag interpolation tables 1..7, indexed by the
/// distance back from the newest sample (1..29).
const ZIGZAG_TABLES: [[i32; 29]; 7] = {
    const ROWS: [[i32; 7]; 29] = [
        [0, 0, 0, 0, -0x0001, 0x0002, -0x0005],
        [0, 0, 0, -0x0001, 0x0003, -0x0008, 0x0011],
        [0, 0, -0x0001, 0x0003, -0x0008, 0x0010, -0x0023],
        [0, -0x0002, 0x0003, -0x0008, 0x0011, -0x0023, 0x0046],
        [0, 0, -0x0002, 0x0006, -0x0010, 0x002B, -0x0017],
        [-0x0002, 0x0003, -0x0005, 0x0005, 0x000A, 0x001A, -0x0044],
        [0x000A, -0x0013, 0x001F, -0x001B, 0x006B, -0x00EB, 0x015B],
        [-0x0022, 0x003C, -0x004A, 0x00A6, -0x016D, 0x027B, -0x0347],
        [0x0041, -0x004B, 0x00B3, -0x01A8, 0x0350, -0x0548, 0x080E],
        [-0x0054, 0x00A2, -0x0192, 0x0372, -0x0623, 0x0AFA, -0x1249],
        [0x0034, -0x00E3, 0x02B1, -0x05BF, 0x0BCD, -0x16FA, 0x3C07],
        [0x0009, 0x0132, -0x039E, 0x09B8, -0x1780, 0x53E0, 0x53E0],
        [-0x010A, -0x0043, 0x04F8, -0x11B4, 0x6794, 0x3C07, -0x16FA],
        [0x0400, -0x0267, -0x05A6, 0x74BB, 0x234C, -0x1249, 0x0AFA],
        [-0x0A78, 0x0C9D, 0x7939, 0x0C9D, -0x0A78, 0x080E, -0x0548],
        [0x234C, 0x74BB, -0x05A6, -0x0267, 0x0400, -0x0347, 0x027B],
        [0x6794, -0x11B4, 0x04F8, -0x0043, -0x010A, 0x015B, -0x00EB],
        [-0x1780, 0x09B8, -0x039E, 0x0132, 0x0009, -0x0044, 0x001A],
        [0x0BCD, -0x05BF, 0x02B1, -0x00E3, 0x0034, -0x0017, 0x002B],
        [-0x0623, 0x0372, -0x0192, 0x00A2, -0x0054, 0x0046, -0x0023],
        [0x0350, -0x01A8, 0x00B3, -0x004B, 0x0041, -0x0023, 0x0010],
        [-0x016D, 0x00A6, -0x004A, 0x003C, -0x0022, 0x0011, -0x0008],
        [0x006B, -0x001B, 0x001F, -0x0013, 0x000A, -0x0005, 0x0002],
        [0x000A, 0x0005, -0x0005, 0x0003, -0x0001, 0, 0],
        [-0x0010, 0x0006, -0x0002, 0, 0, 0, 0],
        [0x0011, -0x0008, 0x0003, -0x0002, 0x0001, 0, 0],
        [-0x0008, 0x0003, -0x0001, 0, 0, 0, 0],
        [0x0003, -0x0001, 0, 0, 0, 0, 0],
        [-0x0001, 0, 0, 0, 0, 0, 0],
    ];
    let mut tables = [[0i32; 29]; 7];
    let mut row = 0;
    while row < 29 {
        let mut table = 0;
        while table < 7 {
            tables[table][row] = ROWS[row][table];
            table += 1;
        }
        row += 1;
    }
    tables
};

/// psx-spx's 37800 to 44100 Hz resampler: a 32-sample ring per channel and
/// a six-step counter; every sixth input sample emits seven outputs.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Resampler {
    ring: [[i16; 32]; 2],
    position: u8,
    six_step: u8,
}

impl Default for Resampler {
    fn default() -> Self {
        Self::new()
    }
}

impl Resampler {
    /// The hardware's counter starts at an undocumented random value; this
    /// one starts at 6 so the first seven outputs follow the sixth input.
    pub fn new() -> Self {
        Self {
            ring: [[0; 32]; 2],
            position: 0,
            six_step: 6,
        }
    }

    /// Push one 37800 Hz stereo sample, appending any 44100 Hz output.
    pub fn push(&mut self, sample: [i16; 2], out: &mut VecDeque<[i16; 2]>) {
        let p = usize::from(self.position);
        self.ring[0][p & 31] = sample[0];
        self.ring[1][p & 31] = sample[1];
        self.position = ((p + 1) & 31) as u8;
        self.six_step -= 1;
        if self.six_step == 0 {
            self.six_step = 6;
            for table in &ZIGZAG_TABLES {
                out.push_back([self.interpolate(0, table), self.interpolate(1, table)]);
            }
        }
    }

    /// psx-spx `ZigZagInterpolate`: each product divided by 8000h (an
    /// arithmetic shift), summed and saturated.
    fn interpolate(&self, channel: usize, table: &[i32; 29]) -> i16 {
        let p = usize::from(self.position);
        let mut sum = 0i32;
        for (i, &factor) in table.iter().enumerate() {
            let sample = i32::from(self.ring[channel][(p + 32 - (i + 1)) & 31]);
            sum += (sample * factor) >> 15;
        }
        sum.clamp(-0x8000, 0x7FFF) as i16
    }
}

/// How the drive routed a sector.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Delivery {
    /// Decoded into the audio output.
    Played,
    /// Not taken by the ADPCM path (it goes to the CPU as data if the drive's
    /// data rules allow).
    NotAudio,
}

/// The drive's XA-ADPCM path: Setmode/Setfilter state, decoder history,
/// resampler and the 44100 Hz stereo output queue for the SPU's CD input.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct XaAudio {
    mode: u8,
    file: u8,
    channel: u8,
    history: [History; 2],
    resampler: Resampler,
    output: VecDeque<[i16; 2]>,
}

impl Default for XaAudio {
    fn default() -> Self {
        Self::new()
    }
}

impl XaAudio {
    pub fn new() -> Self {
        Self {
            mode: 0,
            file: 0,
            channel: 0,
            history: [History::default(); 2],
            resampler: Resampler::new(),
            output: VecDeque::new(),
        }
    }

    /// Setmode (only bits 6 and 3 matter here).
    pub fn set_mode(&mut self, mode: u8) {
        self.mode = mode;
    }

    /// Setfilter.
    pub fn set_filter(&mut self, file: u8, channel: u8) {
        self.file = file;
        self.channel = channel;
    }

    /// Clear decoder history and the resampler (the hardware's behaviour at
    /// seeks or new files is undocumented; the host decides when to call it).
    pub fn reset_decoder(&mut self) {
        self.history = [History::default(); 2];
        self.resampler = Resampler::new();
    }

    /// Whether the ADPCM path takes a Mode 2 sector with this subheader
    /// (psx-spx `try_deliver_as_adpcm_sector`, after the CD-DA and Mode 2 checks).
    pub fn accepts(&self, subheader: Subheader) -> bool {
        if self.mode & mode::XA_ADPCM == 0 {
            return false;
        }
        if self.mode & mode::XA_FILTER != 0
            && (subheader.file != self.file || subheader.channel != self.channel)
        {
            return false;
        }
        subheader.is_realtime_audio()
    }

    /// Offer a Mode 2 sector: its subheader and the data that follows it
    /// (at least 0x900 bytes). An accepted sector is decoded and resampled.
    pub fn deliver(&mut self, subheader: Subheader, data: &[u8]) -> Delivery {
        if !self.accepts(subheader) || data.len() < SECTOR_AUDIO_BYTES {
            return Delivery::NotAudio;
        }
        self.decode_sector(subheader.coding, data);
        Delivery::Played
    }

    /// Offer a raw 2352-byte sector; anything but Mode 2 is not audio.
    pub fn deliver_raw(&mut self, sector: &[u8]) -> Delivery {
        if sector.len() < 24 + SECTOR_AUDIO_BYTES || sector[15] != 2 {
            return Delivery::NotAudio;
        }
        let subheader = Subheader::from_bytes([sector[16], sector[17], sector[18], sector[19]]);
        self.deliver(subheader, &sector[24..])
    }

    /// Decode and resample one sector's audio regardless of mode and filter.
    /// 18900 Hz samples enter the resampler twice each and mono samples feed
    /// both channels.
    pub fn decode_sector(&mut self, coding: u8, data: &[u8]) {
        let coding = Coding::from_byte(coding);
        let mut left = Vec::with_capacity(4032);
        let mut right = Vec::with_capacity(2016);
        decode_adpcm(
            &data[..SECTOR_AUDIO_BYTES],
            coding,
            &mut self.history,
            &mut left,
            &mut right,
        );
        let repeat = if coding.half_rate { 2 } else { 1 };
        for (i, &l) in left.iter().enumerate() {
            let r = if coding.stereo { right[i] } else { l };
            for _ in 0..repeat {
                self.resampler.push([l, r], &mut self.output);
            }
        }
    }

    /// 44100 Hz stereo frames waiting for the SPU.
    pub fn available(&self) -> usize {
        self.output.len()
    }

    /// Move waiting frames into `buf`; returns how many.
    pub fn pop_samples(&mut self, buf: &mut [[i16; 2]]) -> usize {
        let n = buf.len().min(self.output.len());
        for (slot, sample) in buf.iter_mut().zip(self.output.drain(..n)) {
            *slot = sample;
        }
        n
    }

    const TAG: &'static [u8; 4] = b"XAAD";
    const VERSION: u8 = 1;

    /// Serialize the whole state.
    pub fn save(&self) -> Vec<u8> {
        let mut w = Writer::new(Self::TAG, Self::VERSION);
        w.u8(self.mode);
        w.u8(self.file);
        w.u8(self.channel);
        for h in &self.history {
            w.i16(h.old);
            w.i16(h.older);
        }
        for &v in self.resampler.ring.iter().flatten() {
            w.i16(v);
        }
        w.u8(self.resampler.position);
        w.u8(self.resampler.six_step);
        w.u32(self.output.len() as u32);
        for &[l, r] in &self.output {
            w.i16(l);
            w.i16(r);
        }
        w.finish()
    }

    /// Restore a state written by [`XaAudio::save`]. On error the state is unchanged.
    pub fn load(&mut self, data: &[u8]) -> Result<(), SnapshotError> {
        let mut r = Reader::new(data, Self::TAG, Self::VERSION)?;
        let mut next = Self::new();
        next.mode = r.u8()?;
        next.file = r.u8()?;
        next.channel = r.u8()?;
        for h in &mut next.history {
            h.old = r.i16()?;
            h.older = r.i16()?;
        }
        for v in next.resampler.ring.iter_mut().flatten() {
            *v = r.i16()?;
        }
        next.resampler.position = r.u8()?;
        next.resampler.six_step = r.u8()?;
        if next.resampler.position > 31 || !(1..=6).contains(&next.resampler.six_step) {
            return Err(SnapshotError::InvalidValue("xa resampler"));
        }
        let count = r.u32()?;
        for _ in 0..count {
            next.output.push_back([r.i16()?, r.i16()?]);
        }
        r.finish()?;
        *self = next;
        Ok(())
    }
}
