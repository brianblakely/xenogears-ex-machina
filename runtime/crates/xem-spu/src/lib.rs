//! The PS1 Sound Processing Unit as a deterministic host service.
//!
//! [`Spu`] holds the register file (0x1F801C00-0x1F801FFF), the 512 KB SPU
//! RAM, the 24 voices, the noise generator, the reverb unit and a CD audio
//! input queue, and advances them in whole 44.1 kHz samples with integer
//! arithmetic only. It is implemented from the psx-spx documentation (the
//! "Sound Processing Unit (SPU)" chapter, its later revision's envelope
//! counter and reverb resampling filter, and the ADPCM nibble arithmetic of
//! "CDROM XA Audio ADPCM Compression"); the module docs name the points the
//! documentation leaves open and the choice made for each.
//!
//! # Driving it
//!
//! - [`Spu::write16`] / [`Spu::read16`] access a register by its offset from
//!   0x1F801C00, with the hardware side effects (key on/off, ENDX, the
//!   transfer FIFO, SPUCNT, SPUSTAT). A write takes effect at once; the next
//!   sample [`Spu::run`] produces sees it, which is where the hardware's
//!   44.1 kHz register latch would apply it.
//! - [`Spu::sync_window`] applies the game's in-memory register mirror (see
//!   [Register window](#register-window)).
//! - [`Spu::dma_write`] / [`Spu::dma_read`] are DMA4 block transfers at the
//!   transfer address; the FIFO path is [`reg::TRANSFER_FIFO`] plus a
//!   SPUCNT write selecting manual write.
//! - [`Spu::push_cd_audio`] queues 44.1 kHz stereo CD/XA samples; each
//!   sample run consumes one frame (silence when the queue is empty).
//! - [`Spu::run`] advances exactly `samples` 44.1 kHz cycles, writing
//!   interleaved L/R `i16` output only when a buffer is given. All state
//!   (RAM written by reverb and capture, IRQs, envelopes) advances the same
//!   either way.
//! - [`Spu::save`] / [`Spu::load`] snapshot everything except the IRQ hook.
//!
//! # Register window
//!
//! The recovered sound driver writes the registers directly through
//! `sound_spu_registers`, which the game module maps to a 0x400-byte mirror
//! in game memory. [`Spu::sync_window`] reconciles that mirror with the SPU:
//!
//! 1. Every 16-bit register whose window value differs from the value the
//!    previous sync left there is written with [`Spu::write16`], in ascending
//!    address order, except the key registers.
//! 2. Then KOFF (low, high half), then KON (low, high half), each when
//!    nonzero. Key events latch at the next sample on hardware, after the
//!    voice registers written with them, and a voice keyed off and on in one
//!    interval ends keyed on.
//! 3. Every register is read back with [`Spu::read16`] into the window, so
//!    game reads see hardware values (ENDX, current ADSR volumes, SPUSTAT,
//!    current main volume, repeat addresses moved by loop-start flags, ...);
//!    KON and KOFF are cleared to 0, since they are write-triggered.
//!
//! The previous values are the SPU's own state (saved in snapshots), so the
//! caller passes only the window.
//!
//! Limits: the sync sees values, not writes. Two writes to one register
//! between syncs collapse to the last, and writing the value the window
//! already holds is not seen. That is harmless for plain registers, but the
//! registers whose writes act (the transfer FIFO and address, SPUCNT's
//! manual-write trigger, mBASE, the current ADSR volume) must be written
//! with [`Spu::write16`] when a repeated value matters; transfers belong to
//! the direct API. A register written both directly and through the window
//! between two syncs ends with the window's value.
//!
//! # Gaps against hardware
//!
//! - Register writes act immediately rather than at the next 44.1 kHz cycle,
//!   and key on starts decoding on the next sample (no extra key-on delay).
//! - Transfers complete within the call: SPUSTAT's busy bit (10) always reads
//!   0, and its mode bits follow SPUCNT without delay. FIFO writes past 32
//!   halfwords are dropped. DMA reads ignore the transfer type.
//! - IRQ hits are checked per 8-byte unit: a voice's whole 16-byte block at
//!   fetch, each transferred or captured halfword, and reverb buffer writes.
//! - External audio input (0x1F801DB4) has no source and contributes nothing.
//! - Mixing order (from the project's emulator observation of Wide mode in
//!   analysis/formats/sound-modes.md, not stated by psx-spx): voices and the
//!   reverb return are summed, muted by SPUCNT bit 14, CD input is added,
//!   the sum is saturated and then scaled by the current main volume.

pub mod adpcm;
mod envelope;
mod gauss;
mod reverb;
mod snapshot;

use std::collections::VecDeque;

pub use envelope::Phase;
pub use snapshot::SnapshotError;

/// SPU RAM size in bytes.
pub const RAM_SIZE: usize = 0x80000;
/// Size of the register block and of the game's register window.
pub const WINDOW_SIZE: usize = 0x400;
/// Output sample rate.
pub const SAMPLE_RATE: u32 = 44_100;
/// Number of voices.
pub const VOICES: usize = 24;

const RAM_MASK: u32 = RAM_SIZE as u32 - 1;
const SNAPSHOT_MAGIC: &[u8; 4] = b"XSPU";
const SNAPSHOT_VERSION: u32 = 1;

/// Register offsets from 0x1F801C00.
pub mod reg {
    /// The first register of voice `v` (eight per voice, 0x10 bytes apart).
    pub const fn voice(v: usize) -> usize {
        v * 0x10
    }
    /// Voice register: volume left (fixed or sweep).
    pub const VOICE_VOLUME_LEFT: usize = 0x0;
    /// Voice register: volume right (fixed or sweep).
    pub const VOICE_VOLUME_RIGHT: usize = 0x2;
    /// Voice register: ADPCM sample rate (0x1000 = 44.1 kHz).
    pub const VOICE_PITCH: usize = 0x4;
    /// Voice register: start address in 8-byte units.
    pub const VOICE_START: usize = 0x6;
    /// Voice register: ADSR bits 0-15.
    pub const VOICE_ADSR1: usize = 0x8;
    /// Voice register: ADSR bits 16-31.
    pub const VOICE_ADSR2: usize = 0xA;
    /// Voice register: current ADSR volume.
    pub const VOICE_ENVELOPE: usize = 0xC;
    /// Voice register: repeat address in 8-byte units.
    pub const VOICE_REPEAT: usize = 0xE;
    pub const MAIN_VOLUME_LEFT: usize = 0x180;
    pub const MAIN_VOLUME_RIGHT: usize = 0x182;
    /// vLOUT (reverb output volume left).
    pub const REVERB_VOLUME_LEFT: usize = 0x184;
    /// vROUT (reverb output volume right).
    pub const REVERB_VOLUME_RIGHT: usize = 0x186;
    /// KON, voices 0-15 (high half at +2).
    pub const KEY_ON: usize = 0x188;
    /// KOFF, voices 0-15 (high half at +2).
    pub const KEY_OFF: usize = 0x18C;
    /// PMON (high half at +2).
    pub const PITCH_MODULATION: usize = 0x190;
    /// NON (high half at +2).
    pub const NOISE: usize = 0x194;
    /// EON (high half at +2).
    pub const REVERB_ON: usize = 0x198;
    /// ENDX (high half at +2), read-only.
    pub const ENDX: usize = 0x19C;
    /// mBASE, reverb work area start in 8-byte units.
    pub const REVERB_BASE: usize = 0x1A2;
    pub const IRQ_ADDRESS: usize = 0x1A4;
    pub const TRANSFER_ADDRESS: usize = 0x1A6;
    pub const TRANSFER_FIFO: usize = 0x1A8;
    /// SPUCNT.
    pub const CONTROL: usize = 0x1AA;
    pub const TRANSFER_CONTROL: usize = 0x1AC;
    /// SPUSTAT, read-only.
    pub const STATUS: usize = 0x1AE;
    pub const CD_VOLUME_LEFT: usize = 0x1B0;
    pub const CD_VOLUME_RIGHT: usize = 0x1B2;
    pub const EXTERN_VOLUME_LEFT: usize = 0x1B4;
    pub const EXTERN_VOLUME_RIGHT: usize = 0x1B6;
    /// Current main volume left, read-only.
    pub const CURRENT_MAIN_VOLUME_LEFT: usize = 0x1B8;
    /// Current main volume right, read-only.
    pub const CURRENT_MAIN_VOLUME_RIGHT: usize = 0x1BA;
    /// dAPF1 ... vRIN, 32 registers.
    pub const REVERB_CONFIG: usize = 0x1C0;
    /// Current voice volume left/right, 4 bytes per voice, read-only.
    pub const VOICE_CURRENT_VOLUME: usize = 0x200;
}

/// SPUCNT bits.
pub mod control {
    pub const CD_AUDIO: u16 = 1 << 0;
    pub const EXTERN_AUDIO: u16 = 1 << 1;
    pub const CD_REVERB: u16 = 1 << 2;
    pub const EXTERN_REVERB: u16 = 1 << 3;
    /// Bits 4-5: 0 stop, 1 manual write, 2 DMA write, 3 DMA read.
    pub const TRANSFER_MODE: u16 = 3 << 4;
    pub const TRANSFER_MANUAL_WRITE: u16 = 1 << 4;
    pub const TRANSFER_DMA_WRITE: u16 = 2 << 4;
    pub const TRANSFER_DMA_READ: u16 = 3 << 4;
    pub const IRQ_ENABLE: u16 = 1 << 6;
    pub const REVERB_ENABLE: u16 = 1 << 7;
    /// 0 mute, 1 unmute (CD audio is not muted).
    pub const UNMUTE: u16 = 1 << 14;
    /// 0 reset/off, 1 on.
    pub const ENABLE: u16 = 1 << 15;
}

/// SPUSTAT bits beyond the mode bits 0-5.
pub mod status {
    pub const IRQ: u16 = 1 << 6;
    pub const DMA_REQUEST: u16 = 1 << 7;
    pub const DMA_WRITE_REQUEST: u16 = 1 << 8;
    pub const DMA_READ_REQUEST: u16 = 1 << 9;
    pub const BUSY: u16 = 1 << 10;
    pub const CAPTURE_SECOND_HALF: u16 = 1 << 11;
}

/// A callback run when the SPU raises its IRQ, with the sample clock.
pub type IrqHook = Box<dyn FnMut(u64) + Send>;

/// Watches the RAM accesses of one operation for the IRQ address.
pub(crate) struct IrqProbe {
    target: u32,
    armed: bool,
    hit: bool,
}

impl IrqProbe {
    /// Records an access of `len` bytes from `addr`; a hit is any byte in the
    /// IRQ address's 8-byte unit.
    pub(crate) fn access(&mut self, addr: u32, len: u32) {
        if !self.armed || self.hit {
            return;
        }
        let mut off = 0;
        while off < len {
            if (addr + off) & RAM_MASK & !7 == self.target {
                self.hit = true;
                return;
            }
            off += 2;
        }
    }
}

/// Read-only view of one voice's internal state.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct VoiceInfo {
    pub phase: Phase,
    /// Current ADSR level.
    pub envelope: i16,
    /// Byte address of the block being played.
    pub address: u32,
    /// Pitch counter: bits 12+ index the block's samples, 4-11 interpolate.
    pub pitch_counter: u32,
    /// The latest sample after the envelope (VxOUTX).
    pub output: i16,
    /// Current left/right volume.
    pub volume: [i16; 2],
}

#[derive(Clone, Debug, PartialEq, Eq)]
struct Voice {
    phase: Phase,
    level: i16,
    env_counter: u16,
    volume: [i16; 2],
    volume_counter: [u16; 2],
    /// Byte address of the current block.
    address: u32,
    counter: u32,
    /// The current block has been fetched and decoded.
    fetched: bool,
    /// Loop flags of the current block.
    flags: u8,
    history: adpcm::History,
    /// Three samples of the previous block, then the current block's 28.
    samples: [i16; 31],
    output: i16,
}

impl Default for Voice {
    fn default() -> Self {
        Voice {
            phase: Phase::Off,
            level: 0,
            env_counter: 0,
            volume: [0; 2],
            volume_counter: [0; 2],
            address: 0,
            counter: 0,
            fetched: false,
            flags: 0,
            history: [0; 2],
            samples: [0; 31],
            output: 0,
        }
    }
}

const BLOCK_END: u32 = (adpcm::BLOCK_SAMPLES as u32) << 12;

impl Voice {
    fn key_on(&mut self, start: u16) {
        self.phase = Phase::Attack;
        self.level = 0;
        self.env_counter = 0;
        self.address = (start as u32) << 3;
        self.counter = 0;
        self.fetched = false;
        self.flags = 0;
        self.history = [0; 2];
        self.samples = [0; 31];
    }

    /// Fetches and decodes the block at `address`.
    fn fetch(&mut self, ram: &[u8], repeat: &mut u16, probe: &mut IrqProbe) {
        let mut block = [0u8; 16];
        for (i, b) in block.iter_mut().enumerate() {
            *b = ram[((self.address + i as u32) & RAM_MASK) as usize];
        }
        probe.access(self.address, 16);
        self.flags = block[1];
        if self.flags & adpcm::FLAG_LOOP_START != 0 {
            *repeat = (self.address >> 3) as u16;
        }
        self.samples.copy_within(28..31, 0);
        let mut decoded = [0i16; adpcm::BLOCK_SAMPLES];
        adpcm::decode_block(&block, &mut self.history, &mut decoded);
        self.samples[3..].copy_from_slice(&decoded);
        self.fetched = true;
    }

    /// Leaves the current block by its loop flags and fetches the next.
    fn next_block(
        &mut self,
        bit: u32,
        ram: &[u8],
        repeat: &mut u16,
        endx: &mut u32,
        probe: &mut IrqProbe,
    ) {
        if self.flags & adpcm::FLAG_LOOP_END != 0 {
            *endx |= bit;
            self.address = (*repeat as u32) << 3;
            if self.flags & adpcm::FLAG_LOOP_REPEAT == 0 {
                self.phase = Phase::Release;
                self.level = 0;
                self.env_counter = 0;
            }
        } else {
            self.address = (self.address + 16) & RAM_MASK;
        }
        self.fetch(ram, repeat, probe);
    }

    fn interpolate(&self) -> i32 {
        let s = (self.counter >> 12) as usize;
        let i = ((self.counter >> 4) & 0xFF) as usize;
        gauss::interpolate(
            self.samples[s],
            self.samples[s + 1],
            self.samples[s + 2],
            self.samples[s + 3],
            i,
        )
    }
}

#[inline]
fn sat(v: i32) -> i32 {
    v.clamp(i16::MIN as i32, i16::MAX as i32)
}

/// The SPU.
pub struct Spu {
    ram: Box<[u8]>,
    /// Stored register values by halfword index; computed registers are
    /// produced by `read16`.
    regs: [u16; WINDOW_SIZE / 2],
    voices: [Voice; VOICES],
    endx: u32,
    noise_timer: i32,
    noise_level: u16,
    main_volume: [i16; 2],
    main_counter: [u16; 2],
    reverb: reverb::Reverb,
    /// Internal transfer address in bytes.
    transfer_address: u32,
    fifo: [u16; 32],
    fifo_len: u8,
    irq_flag: bool,
    irq_raised_at: Option<u64>,
    capture_pos: u16,
    cd_queue: VecDeque<[i16; 2]>,
    clock: u64,
    window_shadow: [u16; WINDOW_SIZE / 2],
    irq_hook: Option<IrqHook>,
}

impl Default for Spu {
    fn default() -> Self {
        Self::new()
    }
}

impl Spu {
    /// A powered-on SPU: RAM and registers zero (SPUCNT 0: off and muted).
    pub fn new() -> Self {
        Spu {
            ram: vec![0u8; RAM_SIZE].into_boxed_slice(),
            regs: [0; WINDOW_SIZE / 2],
            voices: std::array::from_fn(|_| Voice::default()),
            endx: 0,
            noise_timer: 0,
            noise_level: 0,
            main_volume: [0; 2],
            main_counter: [0; 2],
            reverb: reverb::Reverb::default(),
            transfer_address: 0,
            fifo: [0; 32],
            fifo_len: 0,
            irq_flag: false,
            irq_raised_at: None,
            capture_pos: 0,
            cd_queue: VecDeque::new(),
            clock: 0,
            window_shadow: [0; WINDOW_SIZE / 2],
            irq_hook: None,
        }
    }

    /// Returns to the power-on state, keeping the IRQ hook.
    pub fn reset(&mut self) {
        let hook = self.irq_hook.take();
        *self = Spu::new();
        self.irq_hook = hook;
    }

    /// Installs (or removes) the callback run when the IRQ is raised. It runs
    /// inside the call that raised it (`run`, a transfer), with the sample
    /// clock; [`Spu::irq_raised_at`] holds the same value until acknowledged.
    pub fn set_irq_hook(&mut self, hook: Option<IrqHook>) {
        self.irq_hook = hook;
    }

    /// SPU RAM.
    pub fn ram(&self) -> &[u8] {
        &self.ram
    }

    /// SPU RAM, for hosts that import or inspect it outside the transfer path.
    pub fn ram_mut(&mut self) -> &mut [u8] {
        &mut self.ram
    }

    /// Samples run since power-on.
    pub fn clock(&self) -> u64 {
        self.clock
    }

    /// SPUSTAT's IRQ flag.
    pub fn irq_flag(&self) -> bool {
        self.irq_flag
    }

    /// The sample clock at which the pending IRQ was raised.
    pub fn irq_raised_at(&self) -> Option<u64> {
        self.irq_raised_at
    }

    /// Internal state of voice `v` (0-23).
    pub fn voice(&self, v: usize) -> VoiceInfo {
        let voice = &self.voices[v];
        VoiceInfo {
            phase: voice.phase,
            envelope: voice.level,
            address: voice.address,
            pitch_counter: voice.counter,
            output: voice.output,
            volume: voice.volume,
        }
    }

    /// Queues interleaved L/R 44.1 kHz CD audio (CD-DA or decoded XA).
    pub fn push_cd_audio(&mut self, interleaved: &[i16]) {
        self.cd_queue
            .extend(interleaved.chunks_exact(2).map(|p| [p[0], p[1]]));
    }

    /// Stereo frames waiting in the CD audio queue.
    pub fn cd_audio_queued(&self) -> usize {
        self.cd_queue.len()
    }

    /// Drops the queued CD audio.
    pub fn clear_cd_audio(&mut self) {
        self.cd_queue.clear();
    }

    fn reg(&self, offset: usize) -> u16 {
        self.regs[offset >> 1]
    }

    fn mask32(&self, offset: usize) -> u32 {
        self.reg(offset) as u32 | (self.reg(offset + 2) as u32) << 16
    }

    fn probe(&self) -> IrqProbe {
        let ctrl = self.reg(reg::CONTROL);
        IrqProbe {
            target: (self.reg(reg::IRQ_ADDRESS) as u32) << 3,
            armed: ctrl & (control::ENABLE | control::IRQ_ENABLE)
                == control::ENABLE | control::IRQ_ENABLE
                && !self.irq_flag,
            hit: false,
        }
    }

    fn finish_probe(&mut self, probe: IrqProbe) {
        if probe.hit && !self.irq_flag {
            self.irq_flag = true;
            self.irq_raised_at = Some(self.clock);
            if let Some(hook) = self.irq_hook.as_mut() {
                hook(self.clock);
            }
        }
    }

    /// Writes the 16-bit register at `offset` from 0x1F801C00 (bit 0 and
    /// bits above 0x3FF are ignored).
    pub fn write16(&mut self, offset: usize, value: u16) {
        let off = offset & 0x3FE;
        let i = off >> 1;
        match off {
            0x000..=0x17E => {
                self.regs[i] = value;
                if off & 0xF == reg::VOICE_ENVELOPE {
                    self.voices[off >> 4].level = value as i16;
                }
            }
            0x188 | 0x18A => {
                self.regs[i] = value;
                if self.reg(reg::CONTROL) & control::ENABLE != 0 {
                    let first = if off == reg::KEY_ON { 0 } else { 16 };
                    for bit in 0..16 {
                        let v = first + bit;
                        if v < VOICES && value & (1 << bit) != 0 {
                            let start = self.reg(reg::voice(v) + reg::VOICE_START);
                            self.voices[v].key_on(start);
                            self.endx &= !(1 << v);
                        }
                    }
                }
            }
            0x18C | 0x18E => {
                self.regs[i] = value;
                let first = if off == reg::KEY_OFF { 0 } else { 16 };
                for bit in 0..16 {
                    let v = first + bit;
                    if v < VOICES && value & (1 << bit) != 0 {
                        let voice = &mut self.voices[v];
                        if voice.phase != Phase::Off {
                            voice.phase = Phase::Release;
                            voice.env_counter = 0;
                        }
                    }
                }
            }
            // Read-only: ENDX, SPUSTAT, current main volume, current voice
            // volumes; 0x280-0x3FF is unused.
            0x19C | 0x19E | 0x1AE | 0x1B8 | 0x1BA | 0x200..=0x25E | 0x280..=0x3FE => {}
            reg::REVERB_BASE => {
                self.regs[i] = value;
                self.reverb.set_base(value);
            }
            reg::TRANSFER_ADDRESS => {
                self.regs[i] = value;
                self.transfer_address = (value as u32) << 3;
            }
            reg::TRANSFER_FIFO => {
                self.regs[i] = value;
                if (self.fifo_len as usize) < self.fifo.len() {
                    self.fifo[self.fifo_len as usize] = value;
                    self.fifo_len += 1;
                }
            }
            reg::CONTROL => {
                self.regs[i] = value;
                if value & control::IRQ_ENABLE == 0 {
                    self.irq_flag = false;
                    self.irq_raised_at = None;
                }
                if value & control::ENABLE == 0 {
                    for voice in &mut self.voices {
                        voice.phase = Phase::Off;
                        voice.level = 0;
                    }
                }
                if value & control::TRANSFER_MODE == control::TRANSFER_MANUAL_WRITE {
                    let fifo = self.fifo;
                    let len = self.fifo_len as usize;
                    self.fifo_len = 0;
                    self.write_halfwords(&fifo[..len]);
                }
            }
            _ => self.regs[i] = value,
        }
    }

    /// Reads the 16-bit register at `offset` from 0x1F801C00.
    pub fn read16(&self, offset: usize) -> u16 {
        let off = offset & 0x3FE;
        match off {
            0x000..=0x17E if off & 0xF == reg::VOICE_ENVELOPE => self.voices[off >> 4].level as u16,
            0x19C => self.endx as u16,
            0x19E => (self.endx >> 16) as u16,
            reg::STATUS => self.status(),
            reg::CURRENT_MAIN_VOLUME_LEFT => self.main_volume[0] as u16,
            reg::CURRENT_MAIN_VOLUME_RIGHT => self.main_volume[1] as u16,
            0x200..=0x25E => self.voices[(off - 0x200) >> 2].volume[(off >> 1) & 1] as u16,
            0x280..=0x3FE => 0xFFFF,
            _ => self.regs[off >> 1],
        }
    }

    fn status(&self) -> u16 {
        let ctrl = self.reg(reg::CONTROL);
        let mut s = ctrl & 0x3F;
        if self.irq_flag {
            s |= status::IRQ;
        }
        match ctrl & control::TRANSFER_MODE {
            control::TRANSFER_DMA_WRITE => s |= status::DMA_REQUEST | status::DMA_WRITE_REQUEST,
            control::TRANSFER_DMA_READ => s |= status::DMA_REQUEST | status::DMA_READ_REQUEST,
            _ => {}
        }
        if self.capture_pos & 0x100 != 0 {
            s |= status::CAPTURE_SECOND_HALF;
        }
        s
    }

    /// Writes halfwords at the transfer address through the transfer type of
    /// 0x1F801DAC bits 1-3, in FIFO batches of 32.
    fn write_halfwords(&mut self, data: &[u16]) {
        let kind = (self.reg(reg::TRANSFER_CONTROL) >> 1) & 7;
        let mut probe = self.probe();
        for batch in data.chunks(32) {
            let n = batch.len();
            for i in 0..n {
                let v = match kind {
                    2 => batch[i],
                    3 => batch[i & !1],
                    4 => batch[i & !3],
                    5 => batch[(i | 7).min(n - 1)],
                    _ => batch[n - 1],
                };
                let a = self.transfer_address as usize;
                self.ram[a..a + 2].copy_from_slice(&v.to_le_bytes());
                probe.access(self.transfer_address, 2);
                self.transfer_address = (self.transfer_address + 2) & RAM_MASK;
            }
        }
        self.finish_probe(probe);
    }

    /// DMA4 write: copies little-endian halfwords from `data` to SPU RAM at
    /// the transfer address, which advances and wraps at 512 KB. A trailing
    /// odd byte is ignored. The transfer mode in SPUCNT is not checked; the
    /// caller (the libspu port) sequences it.
    pub fn dma_write(&mut self, data: &[u8]) {
        let halfwords: Vec<u16> = data
            .chunks_exact(2)
            .map(|p| u16::from_le_bytes([p[0], p[1]]))
            .collect();
        self.write_halfwords(&halfwords);
    }

    /// DMA4 read: copies SPU RAM at the transfer address into `out`,
    /// advancing it (a trailing odd byte takes the next halfword's low byte).
    pub fn dma_read(&mut self, out: &mut [u8]) {
        let mut probe = self.probe();
        for chunk in out.chunks_mut(2) {
            let a = self.transfer_address as usize;
            let n = chunk.len();
            chunk.copy_from_slice(&self.ram[a..a + n]);
            probe.access(self.transfer_address, 2);
            self.transfer_address = (self.transfer_address + 2) & RAM_MASK;
        }
        self.finish_probe(probe);
    }

    /// Applies the game's register window and writes the hardware values
    /// back into it (see [Register window](crate#register-window)).
    pub fn sync_window(&mut self, window: &mut [u8; WINDOW_SIZE]) {
        const KEYS: [usize; 4] = [reg::KEY_OFF, reg::KEY_OFF + 2, reg::KEY_ON, reg::KEY_ON + 2];
        let value = |w: &[u8; WINDOW_SIZE], off: usize| u16::from_le_bytes([w[off], w[off + 1]]);
        for i in 0..WINDOW_SIZE / 2 {
            let off = i * 2;
            if KEYS.contains(&off) {
                continue;
            }
            let v = value(window, off);
            if v != self.window_shadow[i] {
                self.write16(off, v);
            }
        }
        for off in KEYS {
            let v = value(window, off);
            if v != 0 {
                self.write16(off, v);
            }
        }
        for i in 0..WINDOW_SIZE / 2 {
            let off = i * 2;
            let v = if KEYS.contains(&off) {
                0
            } else {
                self.read16(off)
            };
            window[off..off + 2].copy_from_slice(&v.to_le_bytes());
            self.window_shadow[i] = v;
        }
    }

    /// Advances exactly `samples` 44.1 kHz cycles. With `out`, writes
    /// interleaved L/R samples to its first `2 * samples` entries.
    ///
    /// # Panics
    /// If `out` is shorter than `2 * samples`.
    pub fn run(&mut self, samples: u32, mut out: Option<&mut [i16]>) {
        if let Some(buf) = out.as_deref() {
            assert!(
                buf.len() >= samples as usize * 2,
                "output buffer shorter than 2 * samples"
            );
        }
        for n in 0..samples as usize {
            let frame = self.tick();
            if let Some(buf) = out.as_deref_mut() {
                buf[2 * n] = frame[0];
                buf[2 * n + 1] = frame[1];
            }
        }
    }

    /// One 44.1 kHz cycle.
    fn tick(&mut self) -> [i16; 2] {
        let mut probe = self.probe();
        let ctrl = self.reg(reg::CONTROL);
        let enabled = ctrl & control::ENABLE != 0;
        let pmon = self.mask32(reg::PITCH_MODULATION);
        let non = self.mask32(reg::NOISE);
        let eon = self.mask32(reg::REVERB_ON);
        let noise = self.noise_level as i16 as i32;

        let Spu {
            ram,
            regs,
            voices,
            endx,
            ..
        } = self;
        let mut dry = [0i32; 2];
        let mut wet = [0i32; 2];
        let mut previous = 0i16;
        for (v, voice) in voices.iter_mut().enumerate() {
            let bit = 1u32 << v;
            let base = reg::voice(v) >> 1;
            let [vol_l, vol_r, pitch, _start, adsr1, adsr2, _env, _repeat] =
                <[u16; 8]>::try_from(&regs[base..base + 8]).expect("eight voice registers");
            let repeat = &mut regs[base + (reg::VOICE_REPEAT >> 1)];
            if !enabled {
                voice.phase = Phase::Off;
                voice.level = 0;
            }
            if !voice.fetched {
                voice.fetch(ram, repeat, &mut probe);
            }

            let raw = if non & bit != 0 {
                noise
            } else {
                voice.interpolate()
            };
            let out = sat((raw * voice.level as i32) >> 15) as i16;
            voice.output = out;
            let l = (out as i32 * voice.volume[0] as i32) >> 15;
            let r = (out as i32 * voice.volume[1] as i32) >> 15;
            dry[0] += l;
            dry[1] += r;
            if eon & bit != 0 {
                wet[0] += l;
                wet[1] += r;
            }

            let mut step = pitch as u32;
            if v > 0 && pmon & bit != 0 {
                let factor = previous as i32 + 0x8000;
                step = (((pitch as i16 as i32) * factor) >> 15) as u32 & 0xFFFF;
            }
            voice.counter += step.min(0x4000);
            while voice.counter >= BLOCK_END {
                voice.counter -= BLOCK_END;
                voice.next_block(bit, ram, repeat, endx, &mut probe);
            }

            envelope::adsr_tick(
                &mut voice.phase,
                &mut voice.level,
                &mut voice.env_counter,
                adsr1,
                adsr2,
            );
            envelope::volume_tick(vol_l, &mut voice.volume[0], &mut voice.volume_counter[0]);
            envelope::volume_tick(vol_r, &mut voice.volume[1], &mut voice.volume_counter[1]);
            previous = out;
        }

        // CD input, raw into the capture buffers, scaled into the mix.
        let cd = self.cd_queue.pop_front().unwrap_or([0, 0]);
        let cd_volume = [
            self.reg(reg::CD_VOLUME_LEFT) as i16 as i32,
            self.reg(reg::CD_VOLUME_RIGHT) as i16 as i32,
        ];
        let cd_scaled = [
            (cd[0] as i32 * cd_volume[0]) >> 15,
            (cd[1] as i32 * cd_volume[1]) >> 15,
        ];
        let cd_dry = if ctrl & control::CD_AUDIO != 0 {
            cd_scaled
        } else {
            [0, 0]
        };
        if ctrl & control::CD_REVERB != 0 {
            wet[0] += cd_scaled[0];
            wet[1] += cd_scaled[1];
        }

        let capture_irq = self.reg(reg::TRANSFER_CONTROL) & 0xC != 0;
        let pos = self.capture_pos as usize * 2;
        for (area, value) in [
            (0x000, cd[0]),
            (0x400, cd[1]),
            (0x800, self.voices[1].output),
            (0xC00, self.voices[3].output),
        ] {
            let a = area + pos;
            self.ram[a..a + 2].copy_from_slice(&value.to_le_bytes());
            if capture_irq {
                probe.access(a as u32, 2);
            }
        }
        self.capture_pos = (self.capture_pos + 1) & 0x1FF;

        let rev: [u16; 32] = self.regs[reg::REVERB_CONFIG >> 1..(reg::REVERB_CONFIG >> 1) + 32]
            .try_into()
            .expect("32 reverb registers");
        let rev_regs = reverb::Regs {
            rev: &rev,
            out_volume: [
                self.reg(reg::REVERB_VOLUME_LEFT) as i16,
                self.reg(reg::REVERB_VOLUME_RIGHT) as i16,
            ],
            base: self.reg(reg::REVERB_BASE),
            write_enable: ctrl & control::REVERB_ENABLE != 0,
        };
        let returned = self.reverb.tick(
            &mut self.ram,
            &rev_regs,
            [sat(wet[0]) as i16, sat(wet[1]) as i16],
            &mut probe,
        );

        let muted = ctrl & control::UNMUTE == 0;
        let mut frame = [0i16; 2];
        for s in 0..2 {
            let spu = if muted {
                0
            } else {
                dry[s] + returned[s] as i32
            };
            let sum = sat(spu + cd_dry[s]);
            frame[s] = sat((sum * self.main_volume[s] as i32) >> 15) as i16;
        }
        for (s, offset) in [reg::MAIN_VOLUME_LEFT, reg::MAIN_VOLUME_RIGHT]
            .into_iter()
            .enumerate()
        {
            let main = self.reg(offset);
            envelope::volume_tick(main, &mut self.main_volume[s], &mut self.main_counter[s]);
        }

        self.noise_tick(ctrl);
        self.finish_probe(probe);
        self.clock += 1;
        frame
    }

    /// psx-spx "SPU Noise Generator".
    fn noise_tick(&mut self, ctrl: u16) {
        let shift = (ctrl >> 10) & 0xF;
        let step = 4 + ((ctrl >> 8) & 3) as i32;
        let l = self.noise_level;
        let parity = ((l >> 15) ^ (l >> 12) ^ (l >> 11) ^ (l >> 10) ^ 1) & 1;
        self.noise_timer -= step;
        if self.noise_timer < 0 {
            self.noise_level = (l << 1) | parity;
            let reload = 0x20000 >> shift;
            self.noise_timer += reload;
            if self.noise_timer < 0 {
                self.noise_timer += reload;
            }
        }
    }

    /// Serialises the whole SPU state (not the IRQ hook).
    pub fn save(&self) -> Vec<u8> {
        let mut w = snapshot::Writer(Vec::with_capacity(RAM_SIZE + 0x2000));
        w.bytes(SNAPSHOT_MAGIC);
        w.u32(SNAPSHOT_VERSION);
        w.bytes(&self.ram);
        for &r in &self.regs {
            w.u16(r);
        }
        for &r in &self.window_shadow {
            w.u16(r);
        }
        for v in &self.voices {
            w.u8(v.phase as u8);
            w.i16(v.level);
            w.u16(v.env_counter);
            for s in 0..2 {
                w.i16(v.volume[s]);
                w.u16(v.volume_counter[s]);
            }
            w.u32(v.address);
            w.u32(v.counter);
            w.bool(v.fetched);
            w.u8(v.flags);
            w.i32(v.history[0]);
            w.i32(v.history[1]);
            for &s in &v.samples {
                w.i16(s);
            }
            w.i16(v.output);
        }
        w.u32(self.endx);
        w.i32(self.noise_timer);
        w.u16(self.noise_level);
        for s in 0..2 {
            w.i16(self.main_volume[s]);
            w.u16(self.main_counter[s]);
        }
        w.u32(self.reverb.cur);
        w.u8(self.reverb.pos);
        w.bool(self.reverb.right_next);
        for side in 0..2 {
            for &s in &self.reverb.input[side] {
                w.i16(s);
            }
            for &s in &self.reverb.output[side] {
                w.i16(s);
            }
        }
        w.u32(self.transfer_address);
        w.u8(self.fifo_len);
        for &h in &self.fifo {
            w.u16(h);
        }
        w.bool(self.irq_flag);
        w.bool(self.irq_raised_at.is_some());
        w.u64(self.irq_raised_at.unwrap_or(0));
        w.u16(self.capture_pos);
        w.u64(self.clock);
        w.u32(self.cd_queue.len() as u32);
        for frame in &self.cd_queue {
            w.i16(frame[0]);
            w.i16(frame[1]);
        }
        w.0
    }

    /// Restores a state written by [`Spu::save`]. On error the SPU is
    /// unchanged. The IRQ hook stays installed.
    pub fn load(&mut self, data: &[u8]) -> Result<(), SnapshotError> {
        let mut r = snapshot::Reader::new(data);
        if r.bytes(4)? != SNAPSHOT_MAGIC {
            return Err(SnapshotError::BadMagic);
        }
        let version = r.u32()?;
        if version != SNAPSHOT_VERSION {
            return Err(SnapshotError::UnsupportedVersion(version));
        }
        let mut s = Spu::new();
        s.ram.copy_from_slice(r.bytes(RAM_SIZE)?);
        for x in s.regs.iter_mut() {
            *x = r.u16()?;
        }
        for x in s.window_shadow.iter_mut() {
            *x = r.u16()?;
        }
        for v in s.voices.iter_mut() {
            v.phase = Phase::from_u8(r.u8()?).ok_or(SnapshotError::InvalidValue("voice phase"))?;
            v.level = r.i16()?;
            v.env_counter = r.u16()?;
            for side in 0..2 {
                v.volume[side] = r.i16()?;
                v.volume_counter[side] = r.u16()?;
            }
            v.address = r.u32()?;
            if v.address > RAM_MASK {
                return Err(SnapshotError::InvalidValue("voice address"));
            }
            v.counter = r.u32()?;
            if v.counter >= BLOCK_END {
                return Err(SnapshotError::InvalidValue("voice pitch counter"));
            }
            v.fetched = r.bool()?;
            v.flags = r.u8()?;
            v.history = [r.i32()?, r.i32()?];
            for x in v.samples.iter_mut() {
                *x = r.i16()?;
            }
            v.output = r.i16()?;
        }
        s.endx = r.u32()?;
        s.noise_timer = r.i32()?;
        s.noise_level = r.u16()?;
        for side in 0..2 {
            s.main_volume[side] = r.i16()?;
            s.main_counter[side] = r.u16()?;
        }
        s.reverb.cur = r.u32()?;
        if s.reverb.cur > RAM_MASK {
            return Err(SnapshotError::InvalidValue("reverb address"));
        }
        s.reverb.pos = r.u8()?;
        if s.reverb.pos as usize >= s.reverb.input[0].len() {
            return Err(SnapshotError::InvalidValue("reverb ring position"));
        }
        s.reverb.right_next = r.bool()?;
        for side in 0..2 {
            for x in s.reverb.input[side].iter_mut() {
                *x = r.i16()?;
            }
            for x in s.reverb.output[side].iter_mut() {
                *x = r.i16()?;
            }
        }
        s.transfer_address = r.u32()?;
        if s.transfer_address > RAM_MASK {
            return Err(SnapshotError::InvalidValue("transfer address"));
        }
        s.fifo_len = r.u8()?;
        if s.fifo_len as usize > s.fifo.len() {
            return Err(SnapshotError::InvalidValue("FIFO length"));
        }
        for x in s.fifo.iter_mut() {
            *x = r.u16()?;
        }
        s.irq_flag = r.bool()?;
        let raised = r.bool()?;
        let at = r.u64()?;
        s.irq_raised_at = raised.then_some(at);
        s.capture_pos = r.u16()?;
        if s.capture_pos >= 0x200 {
            return Err(SnapshotError::InvalidValue("capture position"));
        }
        s.clock = r.u64()?;
        let frames = r.u32()? as usize;
        let bytes = r.bytes(frames.checked_mul(4).ok_or(SnapshotError::Truncated)?)?;
        s.cd_queue = bytes
            .chunks_exact(4)
            .map(|c| {
                [
                    i16::from_le_bytes([c[0], c[1]]),
                    i16::from_le_bytes([c[2], c[3]]),
                ]
            })
            .collect();
        r.finish()?;
        s.irq_hook = self.irq_hook.take();
        *self = s;
        Ok(())
    }
}
