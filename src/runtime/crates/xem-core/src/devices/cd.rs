//! CD-ROM drive: the controller commands src/port/libcd.c issues, their responses
//! and interrupts on the virtual clock, the sector buffer CdGetSector reads,
//! and XA-ADPCM audio for the SPU's CD input, over the user's disc
//! (xem-disc) and xem-media's XA decoder.
//!
//! The port talks to the controller the way libcd's register code does
//! (bios.c `CD_cw`, `getintr`, `CD_getsector`, `CD_vol`, `CD_flush`), one
//! import each:
//!
//! | Import | libcd | Effect |
//! | --- | --- | --- |
//! | `cd_command(com, param, count)` | `CD_cw` | start command `com` with `count` parameter bytes at `param` |
//! | `cd_interrupt(result)` | `getintr` | take the pending interrupt: its type (1-5, 0 for none) \| byte count << 8; the 8 response bytes go to `result` |
//! | `cd_get_sector(dest, words)` | `CD_getsector` | copy `words` words of the sector buffer to `dest` |
//! | `cd_mix(atv)` | `CD_vol` | the CD audio attenuation (CdlATV at `atv`) |
//! | `cd_flush()` | `CD_flush` | acknowledge every pending interrupt |
//! | `cd_poll_now()` | the CheckCallback branch of `CD_sync`/`CD_ready`/`CD_cw` | inside an interrupt handler the game cannot wait: run the drive's next event at once |
//!
//! # Timing
//!
//! Every response is scheduled on the virtual clock from the command's cycle
//! and raises `XEM_IRQ_CDROM` when it is due; the drive keeps no wall clock.
//! The figures follow psx-spx ("CDROM - Response Timings", averages) and
//! Mednafen's seek model without its random term, so equal command sequences
//! give equal interrupt cycles:
//!
//! - first response (INT3): [`ACK_CYCLES`] (Init [`INIT_ACK_CYCLES`]);
//! - Pause/Stop completion (INT2): psx-spx's figures by speed, short when
//!   already paused or stopped; Init, Standby and GetID completions likewise;
//! - seeks ([`seek_cycles`]): one second per 72 minutes of distance, at least
//!   20000 cycles, plus 300 ms for a jump of 2250 sectors or more, plus
//!   [`SPIN_UP_CYCLES`] when the motor is off;
//! - reads: the first sector one sector period after the seek, then one per
//!   period, CPU_HZ / 75 at single speed and / 150 at double (Setmode 0x80).
//!
//! # Sectors
//!
//! A data-ready interrupt (INT1) carries its sector; taking it loads the
//! sector buffer (`getintr` resets the data FIFO, `CD_getsector` reads it in
//! order): 0x924 bytes from the header with Setmode 0x20, else the 0x800
//! data bytes. With Setmode 0x40 a real-time audio sector goes to the XA
//! decoder instead of the CPU, or nowhere when the filter (Setmode 0x08,
//! Setfilter) does not select it (psx-spx "CDROM XA Audio ADPCM
//! Compression", sector filtering and delivery). [`Cd::take_audio`] gives the
//! SPU device the 44100 Hz stereo PCM with the CdMix attenuation applied; at
//! most [`MAX_AUDIO_FRAMES`] wait for it, older audio is lost.
//!
//! # Disc and lid
//!
//! The host inserts the user's disc ([`Cd::insert_disc`]). The lid can open
//! and close ([`Cd::open_lid`], [`Cd::close_lid`]) and the disc be changed
//! while it is open ([`Cd::swap_disc`]): opening stops the motor and the read,
//! the status keeps its shell-open bit (0x10) until a Getstat after the lid
//! closed, and commands other than Getstat and the mode/filter settings fail
//! (INT5, error 0x80) without a closed lid and a disc.

use std::collections::VecDeque;

use xem_disc::sector::{FORM1_DATA_SIZE, SECTOR_SIZE};
use xem_disc::{Error as DiscError, SectorSource};
use xem_media::xa::{Delivery, Subheader as XaSubheader, XaAudio};

use super::{Context, Device, unknown};
use crate::clock::CPU_HZ;
use crate::module::Action;
use crate::snapshot::{Reader, Writer};

/// The CD-ROM interrupt source (src/port/include/xem/port.h).
pub const IRQ_CDROM: u32 = 2;

/// First response of a command (psx-spx average for Getstat with the motor on).
pub const ACK_CYCLES: u64 = 0xC4E1;
/// First response of Init.
pub const INIT_ACK_CYCLES: u64 = 0x13CCE;
/// Init's completion after its first response.
pub const INIT_DONE_CYCLES: u64 = 0xF210;
/// GetID's completion after its first response.
pub const GETID_DONE_CYCLES: u64 = 0x4A00;
/// Pause/Stop completion when the drive is already paused/stopped.
pub const IDLE_DONE_CYCLES: u64 = 0x1DF2;
/// Pause completion while reading at single and double speed.
pub const PAUSE_CYCLES: [u64; 2] = [0x21_181C, 0x10_BD93];
/// Stop completion with the motor running at single and double speed.
pub const STOP_CYCLES: [u64; 2] = [0xD3_8ACA, 0x18A_6076];
/// The motor's spin-up (Standby, or a seek or read with the motor off): one second.
pub const SPIN_UP_CYCLES: u64 = CPU_HZ;
/// XA audio kept for the SPU, at most (two seconds at 44100 Hz).
pub const MAX_AUDIO_FRAMES: usize = 2 * 44_100;

/// Status bits (the first response byte).
pub mod stat {
    pub const ERROR: u8 = 0x01;
    pub const MOTOR: u8 = 0x02;
    pub const SHELL_OPEN: u8 = 0x10;
    pub const READING: u8 = 0x20;
    pub const SEEKING: u8 = 0x40;
}

/// Setmode bits.
pub mod mode {
    pub const DOUBLE_SPEED: u8 = 0x80;
    pub const XA_ADPCM: u8 = 0x40;
    pub const WHOLE_SECTOR: u8 = 0x20;
}

/// Interrupt types (the controller's INT1-INT5).
pub mod intr {
    pub const DATA_READY: u8 = 1;
    pub const COMPLETE: u8 = 2;
    pub const ACKNOWLEDGE: u8 = 3;
    pub const DATA_END: u8 = 4;
    pub const DISK_ERROR: u8 = 5;
}

/// Commands (libcd's numbering).
pub mod com {
    pub const GETSTAT: u8 = 0x01;
    pub const SETLOC: u8 = 0x02;
    pub const READN: u8 = 0x06;
    pub const STANDBY: u8 = 0x07;
    pub const STOP: u8 = 0x08;
    pub const PAUSE: u8 = 0x09;
    pub const INIT: u8 = 0x0A;
    pub const MUTE: u8 = 0x0B;
    pub const DEMUTE: u8 = 0x0C;
    pub const SETFILTER: u8 = 0x0D;
    pub const SETMODE: u8 = 0x0E;
    pub const GETPARAM: u8 = 0x0F;
    pub const GETLOCL: u8 = 0x10;
    pub const GETLOCP: u8 = 0x11;
    pub const GETTN: u8 = 0x13;
    pub const GETTD: u8 = 0x14;
    pub const SEEKL: u8 = 0x15;
    pub const SEEKP: u8 = 0x16;
    pub const GETID: u8 = 0x1A;
    pub const READS: u8 = 0x1B;
    pub const READTOC: u8 = 0x1E;
}

/// Error codes (the second byte of an INT5 response).
pub mod error {
    pub const INVALID_COMMAND: u8 = 0x40;
    pub const NOT_READY: u8 = 0x80;
    pub const WRONG_PARAMETERS: u8 = 0x20;
}

/// A seek of `from` to `to` (LBAs) by the deterministic part of Mednafen's
/// model: the head crosses a 72-minute disc in a second, a seek takes at
/// least 20000 cycles, and a jump of 2250 sectors or more 300 ms longer.
pub fn seek_cycles(from: u32, to: u32) -> u64 {
    let distance = u64::from(from.abs_diff(to));
    let mut cycles = (distance * CPU_HZ / (72 * 60 * 75)).max(20_000);
    if distance >= 2250 {
        cycles += CPU_HZ * 3 / 10;
    }
    cycles
}

fn bcd(value: u32) -> u8 {
    (((value / 10) % 10) << 4 | (value % 10)) as u8
}

fn from_bcd(value: u8) -> u32 {
    u32::from(value >> 4) * 10 + u32::from(value & 0xF)
}

/// An interrupt with its response bytes; a data-ready one carries its sector.
#[derive(Clone)]
struct Response {
    intr: u8,
    bytes: Vec<u8>,
    sector: Option<Box<[u8; SECTOR_SIZE]>>,
}

#[derive(Clone)]
enum Event {
    Respond(Response),
    /// The next sector of the read reaches the head.
    Sector,
    /// A SeekL/SeekP arrives (its completion).
    SeekDone,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Drive {
    Idle,
    /// Reading (ReadN/ReadS), the head on `position` from cycle `from`.
    Reading { from: u64 },
    /// SeekL/SeekP under way.
    Seeking,
}

/// One data sector the drive delivered (for verification).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DeliveredSector {
    pub cycle: u64,
    pub lba: u32,
}

/// The drive.
pub struct Cd {
    disc: Option<Box<dyn SectorSource>>,
    /// A sector the disc source has not fetched yet stopped the drive (browser
    /// hosts); the runtime waits for the host to fetch it.
    pub blocked: bool,
    lid_open: bool,
    /// Status bit 0x10 stays set until a Getstat after the lid closed.
    shell_latch: bool,
    motor: bool,
    muted: bool,
    mode: u8,
    filter: (u8, u8),
    /// Setloc's target, until a read or seek takes it.
    target: Option<u32>,
    /// The head: the next sector a read delivers.
    position: u32,
    drive: Drive,
    /// Header and subheader of the last sector read (GetlocL).
    last_header: [u8; 8],
    events: Vec<(u64, u64, Event)>,
    sequence: u64,
    /// Interrupts the port has not taken.
    pending: VecDeque<Response>,
    /// The sector buffer as CD_getsector reads it.
    buffer: Vec<u8>,
    buffer_read: usize,
    xa: XaAudio,
    /// CdMix: left to left, left to right, right to right, right to left (0x80 = 100%).
    atv: [u8; 4],
    /// Data sectors delivered to the CPU, when the host asks for them.
    pub sector_log: Option<Vec<DeliveredSector>>,
    /// XA sectors played (for verification).
    pub xa_sectors: u64,
}

impl Default for Cd {
    fn default() -> Self {
        Cd {
            disc: None,
            blocked: false,
            lid_open: false,
            shell_latch: false,
            motor: false,
            muted: false,
            mode: 0,
            filter: (0, 0),
            target: None,
            position: 0,
            drive: Drive::Idle,
            last_header: [0; 8],
            events: Vec::new(),
            sequence: 0,
            pending: VecDeque::new(),
            buffer: Vec::new(),
            buffer_read: 0,
            xa: XaAudio::new(),
            atv: [0x80, 0, 0x80, 0],
            sector_log: None,
            xa_sectors: 0,
        }
    }
}

impl Cd {
    /// Put the user's disc in the drive (the lid closed on it).
    pub fn insert_disc(&mut self, disc: Box<dyn SectorSource>) {
        self.disc = Some(disc);
    }

    /// Open the lid: the motor and any read stop.
    pub fn open_lid(&mut self) {
        self.lid_open = true;
        self.shell_latch = true;
        self.motor = false;
        self.drive = Drive::Idle;
        self.events.retain(|(_, _, e)| matches!(e, Event::Respond(_)));
    }

    /// Close the lid; the next command finds the disc (if any) stopped.
    pub fn close_lid(&mut self) {
        self.lid_open = false;
    }

    /// Change the disc: the lid opens (if it was closed) and the new disc
    /// replaces the old one, which is returned; close the lid afterwards.
    pub fn swap_disc(&mut self, disc: Option<Box<dyn SectorSource>>) -> Option<Box<dyn SectorSource>> {
        self.open_lid();
        std::mem::replace(&mut self.disc, disc)
    }

    pub fn lid_open(&self) -> bool {
        self.lid_open
    }

    /// Move the XA audio decoded so far into `out` as interleaved 44100 Hz
    /// stereo, with the CdMix attenuation applied (silence while muted).
    pub fn take_audio(&mut self, out: &mut Vec<i16>) {
        let mut frames = [[0i16; 2]; 256];
        let [ll, lr, rr, rl] = self.atv.map(i32::from);
        loop {
            let n = self.xa.pop_samples(&mut frames);
            if n == 0 {
                break;
            }
            for &[l, r] in &frames[..n] {
                let (l, r) = (i32::from(l), i32::from(r));
                let left = ((l * ll + r * rl) >> 7).clamp(-0x8000, 0x7FFF) as i16;
                let right = ((r * rr + l * lr) >> 7).clamp(-0x8000, 0x7FFF) as i16;
                if self.muted {
                    out.extend_from_slice(&[0, 0]);
                } else {
                    out.extend_from_slice(&[left, right]);
                }
            }
        }
    }

    fn speed(&self) -> usize {
        usize::from(self.mode & mode::DOUBLE_SPEED != 0)
    }

    /// CPU cycles per sector at the current speed.
    pub fn sector_cycles(&self) -> u64 {
        CPU_HZ / (75 << self.speed())
    }

    fn status(&self, now: u64) -> u8 {
        let mut s = 0;
        if self.motor {
            s |= stat::MOTOR;
        }
        if self.shell_latch {
            s |= stat::SHELL_OPEN;
        }
        match self.drive {
            Drive::Reading { from } if now < from => s |= stat::SEEKING,
            Drive::Reading { .. } => s |= stat::READING,
            Drive::Seeking => s |= stat::SEEKING,
            Drive::Idle => {}
        }
        s
    }

    fn schedule(&mut self, at: u64, event: Event) {
        self.sequence += 1;
        self.events.push((at, self.sequence, event));
    }

    fn respond_at(&mut self, at: u64, intr: u8, bytes: Vec<u8>) {
        self.schedule(at, Event::Respond(Response { intr, bytes, sector: None }));
    }

    fn cancel_read(&mut self) {
        self.events.retain(|(_, _, e)| !matches!(e, Event::Sector | Event::SeekDone));
        self.drive = Drive::Idle;
    }

    fn ready(&self) -> bool {
        !self.lid_open && self.disc.is_some()
    }

    /// The cycles before the head is on `to`, spinning up a stopped motor.
    fn seek_time(&self, to: u32) -> u64 {
        seek_cycles(self.position, to) + if self.motor { 0 } else { SPIN_UP_CYCLES }
    }

    /// Start command `com` at `now`; its responses follow on the clock.
    pub fn command(&mut self, com: u8, params: &[u8], now: u64) {
        let ack = now + if com == com::INIT { INIT_ACK_CYCLES } else { ACK_CYCLES };
        let needs_disc = !matches!(
            com,
            com::GETSTAT | com::SETMODE | com::SETFILTER | com::GETPARAM | com::MUTE | com::DEMUTE | com::INIT
        );
        if needs_disc && !self.ready() {
            let s = self.status(now) | stat::ERROR;
            self.respond_at(ack, intr::DISK_ERROR, vec![s, error::NOT_READY]);
            return;
        }
        let param = |i: usize| params.get(i).copied().unwrap_or(0);
        match com {
            com::GETSTAT => {
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
                if !self.lid_open {
                    self.shell_latch = false;
                }
            }
            com::SETLOC => {
                let lba = (from_bcd(param(0)) * 60 + from_bcd(param(1))) * 75 + from_bcd(param(2));
                self.target = Some(lba.saturating_sub(150));
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
            }
            com::READN | com::READS => {
                let to = self.target.take().unwrap_or(self.position);
                let seek = self.seek_time(to);
                self.cancel_read();
                self.motor = true;
                self.position = to;
                let first = now + seek;
                self.drive = Drive::Reading { from: first };
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
                let period = self.sector_cycles();
                self.schedule(first + period, Event::Sector);
            }
            com::SEEKL | com::SEEKP => {
                let to = self.target.take().unwrap_or(self.position);
                let seek = self.seek_time(to);
                self.cancel_read();
                self.motor = true;
                self.position = to;
                self.drive = Drive::Seeking;
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
                self.schedule(now + seek.max(ACK_CYCLES + 1), Event::SeekDone);
            }
            com::PAUSE => {
                let busy = self.drive != Drive::Idle;
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
                self.cancel_read();
                let done = if busy { PAUSE_CYCLES[self.speed()] } else { IDLE_DONE_CYCLES };
                let s = self.status(now);
                self.respond_at(ack + done, intr::COMPLETE, vec![s]);
            }
            com::STOP => {
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
                let done = if self.motor { STOP_CYCLES[self.speed()] } else { IDLE_DONE_CYCLES };
                self.cancel_read();
                self.motor = false;
                let s = self.status(now);
                self.respond_at(ack + done, intr::COMPLETE, vec![s]);
            }
            com::STANDBY => {
                if self.motor {
                    let s = self.status(now) | stat::ERROR;
                    self.respond_at(ack, intr::DISK_ERROR, vec![s, error::WRONG_PARAMETERS]);
                } else {
                    let s = self.status(now);
                    self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
                    self.motor = true;
                    let s = self.status(now);
                    self.respond_at(ack + SPIN_UP_CYCLES, intr::COMPLETE, vec![s]);
                }
            }
            com::INIT => {
                self.cancel_read();
                self.mode = mode::WHOLE_SECTOR;
                self.xa.set_mode(self.mode);
                self.muted = false;
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
                if self.ready() {
                    self.motor = true;
                }
                let s = self.status(now);
                self.respond_at(ack + INIT_DONE_CYCLES, intr::COMPLETE, vec![s]);
            }
            com::MUTE | com::DEMUTE => {
                self.muted = com == com::MUTE;
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
            }
            com::SETFILTER => {
                self.filter = (param(0), param(1));
                self.xa.set_filter(param(0), param(1));
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
            }
            com::SETMODE => {
                self.mode = param(0);
                self.xa.set_mode(self.mode);
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
            }
            com::GETPARAM => {
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s, self.mode, 0, self.filter.0, self.filter.1]);
            }
            com::GETLOCL => {
                let bytes = self.last_header.to_vec();
                self.respond_at(ack, intr::ACKNOWLEDGE, bytes);
            }
            com::GETLOCP => {
                let lba = self.position.saturating_sub(1);
                let absolute = lba + 150;
                let bytes = vec![
                    1,
                    1,
                    bcd(lba / 4500),
                    bcd(lba / 75 % 60),
                    bcd(lba % 75),
                    bcd(absolute / 4500),
                    bcd(absolute / 75 % 60),
                    bcd(absolute % 75),
                ];
                self.respond_at(ack, intr::ACKNOWLEDGE, bytes);
            }
            com::GETTN => {
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s, 1, 1]);
            }
            com::GETTD => {
                let s = self.status(now);
                // Track 0 is the end of the disc; the data track starts at 00:02.
                let at = match from_bcd(param(0)) {
                    0 => self.disc.as_ref().map_or(0, |d| d.sector_count()) + 150,
                    1 => 150,
                    _ => {
                        self.respond_at(ack, intr::DISK_ERROR, vec![s | stat::ERROR, 0x10]);
                        return;
                    }
                };
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s, bcd(at / 4500), bcd(at / 75 % 60)]);
            }
            com::GETID => {
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
                self.respond_at(ack + GETID_DONE_CYCLES, intr::COMPLETE, vec![s, 0, 0x20, 0, b'S', b'C', b'E', b'A']);
            }
            com::READTOC => {
                let s = self.status(now);
                self.respond_at(ack, intr::ACKNOWLEDGE, vec![s]);
                self.respond_at(ack + SPIN_UP_CYCLES, intr::COMPLETE, vec![s]);
            }
            _ => {
                let s = self.status(now) | stat::ERROR;
                self.respond_at(ack, intr::DISK_ERROR, vec![s, error::INVALID_COMMAND]);
            }
        }
    }

    fn push_response(&mut self, response: Response, raised: &mut Vec<(u32, u32)>) {
        if self.pending.is_empty() {
            raised.push((IRQ_CDROM, 0));
        }
        self.pending.push_back(response);
    }

    /// The next sector reaches the head: deliver it, play it or drop it.
    fn sector(&mut self, now: u64, raised: &mut Vec<(u32, u32)>) {
        let period = self.sector_cycles();
        let Some(disc) = self.disc.as_mut() else {
            self.drive = Drive::Idle;
            return;
        };
        if self.position >= disc.sector_count() {
            self.drive = Drive::Idle;
            let s = self.status(now);
            self.push_response(Response { intr: intr::DATA_END, bytes: vec![s], sector: None }, raised);
            return;
        }
        let mut raw = Box::new([0u8; SECTOR_SIZE]);
        match disc.read_sector(self.position, &mut raw) {
            Ok(()) => {}
            Err(DiscError::NotReady) => {
                // A browser still fetching the image: the sector stays due now
                // and the runtime stops until the host has fetched it, so I/O
                // latency never moves the virtual clock.
                self.blocked = true;
                self.schedule(now, Event::Sector);
                return;
            }
            Err(_) => {
                self.drive = Drive::Idle;
                let s = self.status(now) | stat::ERROR;
                self.push_response(Response { intr: intr::DISK_ERROR, bytes: vec![s, 0x04], sector: None }, raised);
                return;
            }
        }
        self.last_header.copy_from_slice(&raw[12..20]);
        let lba = self.position;
        self.position += 1;
        self.schedule(now + period, Event::Sector);
        if raw[15] == 2 && self.mode & mode::XA_ADPCM != 0 {
            let subheader = XaSubheader::from_bytes([raw[16], raw[17], raw[18], raw[19]]);
            if subheader.is_realtime_audio() {
                // Played into the SPU when the filter selects it, else dropped.
                if self.xa.deliver(subheader, &raw[24..]) == Delivery::Played {
                    self.xa_sectors += 1;
                }
                // Audio nobody takes is lost, as the drive's output is.
                let excess = self.xa.available().saturating_sub(MAX_AUDIO_FRAMES);
                if excess > 0 {
                    let mut drop = vec![[0i16; 2]; excess];
                    self.xa.pop_samples(&mut drop);
                }
                return;
            }
        }
        if let Some(log) = self.sector_log.as_mut() {
            log.push(DeliveredSector { cycle: now, lba });
        }
        let s = self.status(now);
        self.push_response(Response { intr: intr::DATA_READY, bytes: vec![s], sector: Some(raw) }, raised);
    }

    fn run_event(&mut self, at: u64, event: Event, raised: &mut Vec<(u32, u32)>) {
        match event {
            Event::Respond(response) => self.push_response(response, raised),
            Event::Sector => self.sector(at, raised),
            Event::SeekDone => {
                self.drive = Drive::Idle;
                let s = self.status(at);
                self.push_response(Response { intr: intr::COMPLETE, bytes: vec![s], sector: None }, raised);
            }
        }
    }

    fn take_next_event(&mut self) -> Option<(u64, Event)> {
        let index = (0..self.events.len()).min_by_key(|&i| (self.events[i].0, self.events[i].1))?;
        let (at, _, event) = self.events.remove(index);
        Some((at, event))
    }

    /// Run every event due at `now`.
    pub fn run_due(&mut self, now: u64, raised: &mut Vec<(u32, u32)>) {
        while !self.blocked && self.events.iter().any(|(at, _, _)| *at <= now) {
            let (at, event) = self.take_next_event().expect("a due event");
            self.run_event(at, event, raised);
        }
    }

    /// Run the next event now, ahead of its cycle (an interrupt handler
    /// spinning on the drive). Returns whether there was one.
    pub fn run_next_now(&mut self, now: u64, raised: &mut Vec<(u32, u32)>) -> bool {
        match self.take_next_event() {
            Some((_, event)) => {
                self.run_event(now, event, raised);
                true
            }
            None => false,
        }
    }

    /// Take the pending interrupt: (type, response bytes). A data-ready one
    /// loads its sector into the buffer.
    pub fn take_interrupt(&mut self) -> Option<(u8, Vec<u8>)> {
        let response = self.pending.pop_front()?;
        if let Some(sector) = response.sector {
            self.buffer.clear();
            if self.mode & mode::WHOLE_SECTOR != 0 {
                self.buffer.extend_from_slice(&sector[12..12 + 0x924]);
            } else {
                self.buffer.extend_from_slice(&sector[24..24 + FORM1_DATA_SIZE]);
            }
            self.buffer_read = 0;
        }
        Some((response.intr, response.bytes))
    }

    /// The next `len` bytes of the sector buffer (zeros past its end).
    pub fn read_buffer(&mut self, len: usize) -> Vec<u8> {
        let mut out = vec![0u8; len];
        let start = self.buffer_read.min(self.buffer.len());
        let end = (start + len).min(self.buffer.len());
        out[..end - start].copy_from_slice(&self.buffer[start..end]);
        self.buffer_read += len;
        out
    }
}

fn write_response(w: &mut Writer, response: &Response) {
    w.u32(u32::from(response.intr));
    w.bytes(&response.bytes);
    w.bytes(response.sector.as_deref().map_or(&[][..], |s| &s[..]));
}

fn read_response(r: &mut Reader<'_>) -> Result<Response, String> {
    let e = |error: crate::snapshot::SnapshotError| error.to_string();
    let intr = r.u32().map_err(e)? as u8;
    let bytes = r.bytes().map_err(e)?.to_vec();
    let sector = r.bytes().map_err(e)?;
    let sector = match sector.len() {
        0 => None,
        SECTOR_SIZE => Some(Box::new(<[u8; SECTOR_SIZE]>::try_from(sector).expect("sector length"))),
        _ => return Err("bad cd sector".into()),
    };
    Ok(Response { intr, bytes, sector })
}

impl Device for Cd {
    fn import(&mut self, op: &str, args: &[u32], context: &mut Context<'_>) -> Action {
        let arg = |i: usize| args.get(i).copied().unwrap_or(0);
        let fault = |context: &mut Context<'_>, error: crate::memory::OutOfBounds| {
            *context.trap_reason = Some(format!("cd_{op}: {error}"));
            Action::Trap
        };
        match op {
            "command" => {
                let mut params = vec![0u8; arg(2).min(16) as usize];
                if !params.is_empty() {
                    if let Err(error) = context.memory.read(arg(1), &mut params) {
                        return fault(context, error);
                    }
                }
                self.command(arg(0) as u8, &params, context.now);
                Action::Return(0)
            }
            "interrupt" => match self.take_interrupt() {
                Some((kind, bytes)) => {
                    let mut result = [0u8; 8];
                    let n = bytes.len().min(8);
                    result[..n].copy_from_slice(&bytes[..n]);
                    if let Err(error) = context.memory.write(arg(0), &result) {
                        return fault(context, error);
                    }
                    Action::Return(u32::from(kind) | (n as u32) << 8)
                }
                None => Action::Return(0),
            },
            "get_sector" => {
                let data = self.read_buffer(arg(1) as usize * 4);
                match context.memory.write(arg(0), &data) {
                    Ok(()) => Action::Return(0),
                    Err(error) => fault(context, error),
                }
            }
            "mix" => {
                let mut atv = [0u8; 4];
                if let Err(error) = context.memory.read(arg(0), &mut atv) {
                    return fault(context, error);
                }
                self.atv = atv;
                Action::Return(0)
            }
            "flush" => {
                self.pending.clear();
                Action::Return(0)
            }
            "poll_now" => Action::Return(u32::from(self.run_next_now(context.now, context.raised))),
            _ => unknown("cd", op, context),
        }
    }

    fn next_event(&self) -> Option<u64> {
        self.events.iter().map(|(at, _, _)| *at).min()
    }

    /// The drive's state without the disc (the host inserts the same disc
    /// again before restoring) and without the sector log.
    fn save(&self) -> Vec<u8> {
        let mut w = Writer::default();
        w.u32(u32::from(self.lid_open) | u32::from(self.shell_latch) << 1 | u32::from(self.motor) << 2 | u32::from(self.muted) << 3);
        w.u32(u32::from(self.mode) | u32::from(self.filter.0) << 8 | u32::from(self.filter.1) << 16);
        w.u64(self.target.map_or(u64::MAX, u64::from));
        w.u32(self.position);
        match self.drive {
            Drive::Idle => w.u64(0),
            Drive::Reading { from } => {
                w.u64(1);
                w.u64(from);
            }
            Drive::Seeking => w.u64(2),
        }
        w.bytes(&self.last_header);
        w.u64(self.sequence);
        w.u32(self.events.len() as u32);
        for (at, sequence, event) in &self.events {
            w.u64(*at);
            w.u64(*sequence);
            match event {
                Event::Respond(response) => {
                    w.u32(0);
                    write_response(&mut w, response);
                }
                Event::Sector => w.u32(1),
                Event::SeekDone => w.u32(2),
            }
        }
        w.u32(self.pending.len() as u32);
        for response in &self.pending {
            write_response(&mut w, response);
        }
        w.bytes(&self.buffer);
        w.u64(self.buffer_read as u64);
        w.bytes(&self.xa.save());
        w.bytes(&self.atv);
        w.0
    }

    fn load(&mut self, state: &[u8]) -> Result<(), String> {
        let disc = self.disc.take();
        let log = self.sector_log.take();
        let xa_sectors = self.xa_sectors;
        *self = Cd::default();
        self.disc = disc;
        self.sector_log = log;
        self.xa_sectors = xa_sectors;
        let mut r = Reader::new(state);
        let e = |error: crate::snapshot::SnapshotError| error.to_string();
        let flags = r.u32().map_err(e)?;
        self.lid_open = flags & 1 != 0;
        self.shell_latch = flags & 2 != 0;
        self.motor = flags & 4 != 0;
        self.muted = flags & 8 != 0;
        let modes = r.u32().map_err(e)?;
        self.mode = modes as u8;
        self.filter = ((modes >> 8) as u8, (modes >> 16) as u8);
        self.target = u32::try_from(r.u64().map_err(e)?).ok();
        self.position = r.u32().map_err(e)?;
        self.drive = match r.u64().map_err(e)? {
            0 => Drive::Idle,
            1 => Drive::Reading { from: r.u64().map_err(e)? },
            2 => Drive::Seeking,
            _ => return Err("bad cd drive state".into()),
        };
        self.last_header.copy_from_slice(r.bytes().map_err(e)?.get(..8).ok_or("bad cd header")?);
        self.sequence = r.u64().map_err(e)?;
        for _ in 0..r.u32().map_err(e)? {
            let at = r.u64().map_err(e)?;
            let sequence = r.u64().map_err(e)?;
            let event = match r.u32().map_err(e)? {
                0 => Event::Respond(read_response(&mut r)?),
                1 => Event::Sector,
                2 => Event::SeekDone,
                _ => return Err("bad cd event".into()),
            };
            self.events.push((at, sequence, event));
        }
        for _ in 0..r.u32().map_err(e)? {
            let response = read_response(&mut r)?;
            self.pending.push_back(response);
        }
        self.buffer = r.bytes().map_err(e)?.to_vec();
        self.buffer_read = r.u64().map_err(e)? as usize;
        self.xa.load(r.bytes().map_err(e)?).map_err(|error| error.to_string())?;
        self.atv.copy_from_slice(r.bytes().map_err(e)?.get(..4).ok_or("bad cd mix")?);
        Ok(())
    }

    fn run_events(&mut self, context: &mut Context<'_>) {
        self.run_due(context.now, context.raised);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use xem_disc::MemorySource;
    use xem_disc::sector::{Subheader, build_mode2_sector, submode};

    const DATA: Subheader = Subheader { file: 0, channel: 0, submode: submode::DATA, coding: 0 };

    fn disc(sectors: u32) -> Box<dyn SectorSource> {
        Box::new(MemorySource::from_sectors((0..sectors).map(|lba| {
            let data: Vec<u8> = (0..FORM1_DATA_SIZE).map(|i| (lba as usize * 7 + i) as u8).collect();
            build_mode2_sector(lba, DATA, &data)
        })))
    }

    /// Run the drive to `until`, collecting interrupts in order as the port takes them.
    fn run(cd: &mut Cd, now: &mut u64, until: u64) -> Vec<(u64, u8, Vec<u8>)> {
        let mut out = Vec::new();
        loop {
            let Some(at) = cd.next_event().filter(|&at| at <= until) else { break };
            *now = at;
            let mut raised = Vec::new();
            cd.run_due(at, &mut raised);
            while let Some((kind, bytes)) = cd.take_interrupt() {
                out.push((at, kind, bytes));
            }
        }
        *now = until;
        out
    }

    fn run_for(cd: &mut Cd, now: &mut u64, span: u64) -> Vec<(u64, u8, Vec<u8>)> {
        let until = *now + span;
        run(cd, now, until)
    }

    fn loc(lba: u32) -> [u8; 3] {
        let a = lba + 150;
        [bcd(a / 4500), bcd(a / 75 % 60), bcd(a % 75)]
    }

    #[test]
    fn setloc_readn_delivers_sectors_at_the_read_speed() {
        let mut cd = Cd::default();
        cd.insert_disc(disc(64));
        let mut now = 1000;
        cd.command(com::SETMODE, &[0xA0], now);
        let acks = run_for(&mut cd, &mut now, 100_000);
        assert_eq!(acks, vec![(1000 + ACK_CYCLES, intr::ACKNOWLEDGE, vec![0])]);
        cd.command(com::SETLOC, &loc(24), now);
        run_for(&mut cd, &mut now, 100_000);
        let start = now;
        cd.command(com::READN, &[], start);
        let events = run(&mut cd, &mut now, start + 2 * CPU_HZ / 10);
        assert_eq!(events[0], (start + ACK_CYCLES, intr::ACKNOWLEDGE, vec![stat::MOTOR | stat::SEEKING]));
        // A stopped motor spins up, then seeks from 0 to 24.
        let first = start + SPIN_UP_CYCLES + seek_cycles(0, 24) + CPU_HZ / 150;
        let reads: Vec<_> = events[1..].iter().collect();
        assert!(reads.is_empty(), "the spin-up outlasts 200 ms");
        let events = run(&mut cd, &mut now, first + 2 * CPU_HZ / 150);
        assert_eq!(events.len(), 3);
        for (n, (at, kind, bytes)) in events.iter().enumerate() {
            assert_eq!(*at, first + n as u64 * (CPU_HZ / 150));
            assert_eq!(*kind, intr::DATA_READY);
            assert_eq!(bytes, &vec![stat::MOTOR | stat::READING]);
        }
        // The last sector taken (LBA 26) fills the buffer: header, subheader, data.
        let header = cd.read_buffer(12);
        assert_eq!(&header[..4], &[0x00, 0x02, 0x26, 0x02]);
        let data = cd.read_buffer(4);
        assert_eq!(data, vec![(26 * 7) as u8, (26 * 7 + 1) as u8, (26 * 7 + 2) as u8, (26 * 7 + 3) as u8]);
    }

    #[test]
    fn pause_acknowledges_then_completes_and_stops_the_read() {
        let mut cd = Cd::default();
        cd.insert_disc(disc(1000));
        let mut now = 0;
        cd.command(com::SETMODE, &[0x80], now);
        cd.command(com::READN, &[], now);
        run(&mut cd, &mut now, CPU_HZ * 2);
        cd.command(com::PAUSE, &[], now);
        let start = now;
        let events = run_for(&mut cd, &mut now, CPU_HZ);
        assert_eq!(events.len(), 2);
        assert_eq!(events[0], (start + ACK_CYCLES, intr::ACKNOWLEDGE, vec![stat::MOTOR | stat::READING]));
        assert_eq!(events[1], (start + ACK_CYCLES + PAUSE_CYCLES[1], intr::COMPLETE, vec![stat::MOTOR]));
        assert!(cd.next_event().is_none());
    }

    #[test]
    fn whole_sector_mode_and_data_mode_fill_the_buffer_differently() {
        let mut cd = Cd::default();
        cd.insert_disc(disc(8));
        let mut now = 0;
        cd.command(com::SETMODE, &[0x80], now);
        cd.command(com::SETLOC, &loc(3), now);
        cd.command(com::READN, &[], now);
        let events = run(&mut cd, &mut now, CPU_HZ * 2);
        assert!(events.iter().any(|e| e.1 == intr::DATA_READY));
        // Mode 0x80: the buffer is the 0x800 data bytes of the last sector.
        let last = events.iter().filter(|e| e.1 == intr::DATA_READY).count() as u32 + 2;
        assert_eq!(cd.read_buffer(1)[0], (last as usize * 7) as u8);
        assert_eq!(cd.buffer.len(), 0x800);
    }

    #[test]
    fn a_saved_drive_continues_identically() {
        let mut cd = Cd::default();
        cd.insert_disc(disc(200));
        let mut now = 0;
        cd.command(com::SETMODE, &[0xA0], now);
        cd.command(com::SETLOC, &loc(10), now);
        cd.command(com::READN, &[], now);
        run(&mut cd, &mut now, CPU_HZ + CPU_HZ / 10);
        let mut raised = Vec::new();
        let at = cd.next_event().unwrap();
        cd.run_due(at, &mut raised);
        now = at;
        let state = cd.save();
        let mut copy = Cd::default();
        copy.insert_disc(disc(200));
        copy.load(&state).unwrap();
        assert_eq!(copy.save(), state);
        let (mut a, mut b) = (now, now);
        assert_eq!(run(&mut cd, &mut a, now + CPU_HZ / 5), run(&mut copy, &mut b, now + CPU_HZ / 5));
        assert_eq!(cd.read_buffer(16), copy.read_buffer(16));
    }

    #[test]
    fn reading_past_the_end_reports_data_end() {
        let mut cd = Cd::default();
        cd.insert_disc(disc(4));
        let mut now = 0;
        cd.command(com::SETLOC, &loc(2), now);
        cd.command(com::READN, &[], now);
        let events = run(&mut cd, &mut now, CPU_HZ * 3);
        let kinds: Vec<u8> = events.iter().map(|e| e.1).collect();
        assert_eq!(kinds, vec![intr::ACKNOWLEDGE, intr::ACKNOWLEDGE, intr::DATA_READY, intr::DATA_READY, intr::DATA_END]);
    }

    #[test]
    fn seekl_completes_after_the_seek_time() {
        let mut cd = Cd::default();
        cd.insert_disc(disc(10_000));
        let mut now = 0;
        cd.command(com::INIT, &[], now);
        run(&mut cd, &mut now, CPU_HZ);
        cd.command(com::SETLOC, &loc(9000), now);
        run_for(&mut cd, &mut now, CPU_HZ);
        let start = now;
        cd.command(com::SEEKL, &[], start);
        let events = run(&mut cd, &mut now, start + 2 * CPU_HZ);
        assert_eq!(events[0].1, intr::ACKNOWLEDGE);
        assert_eq!(events[0].2, vec![stat::MOTOR | stat::SEEKING]);
        assert_eq!(events[1], (start + seek_cycles(0, 9000), intr::COMPLETE, vec![stat::MOTOR]));
        assert_eq!(seek_cycles(0, 9000), 9000 * CPU_HZ / 324_000 + CPU_HZ * 3 / 10);
    }

    #[test]
    fn open_lid_fails_commands_and_latches_the_shell_bit() {
        let mut cd = Cd::default();
        cd.insert_disc(disc(4));
        let mut now = 0;
        cd.command(com::INIT, &[], now);
        run(&mut cd, &mut now, CPU_HZ);
        let old = cd.swap_disc(Some(disc(8)));
        assert!(old.is_some() && cd.lid_open());
        cd.command(com::SEEKL, &[], now);
        cd.command(com::GETSTAT, &[], now);
        let events = run_for(&mut cd, &mut now, CPU_HZ);
        assert_eq!(events[0].1, intr::DISK_ERROR);
        assert_eq!(events[0].2, vec![stat::SHELL_OPEN | stat::ERROR, error::NOT_READY]);
        assert_eq!(events[1].2, vec![stat::SHELL_OPEN]);
        cd.close_lid();
        cd.command(com::GETSTAT, &[], now);
        cd.command(com::GETSTAT, &[], now);
        let events = run_for(&mut cd, &mut now, CPU_HZ);
        // The first Getstat after closing still reports the opening, the next does not.
        assert_eq!(events[0].2, vec![stat::SHELL_OPEN]);
        assert_eq!(events[1].2, vec![0]);
        cd.command(com::GETTD, &[0], now);
        let events = run_for(&mut cd, &mut now, CPU_HZ);
        assert_eq!(events[0].2, vec![0, 0x00, 0x02]);
    }

    #[test]
    fn xa_audio_sectors_go_to_the_decoder_not_the_cpu() {
        let audio = Subheader {
            file: 1,
            channel: 1,
            submode: submode::AUDIO | submode::REAL_TIME | submode::FORM2,
            coding: 0x01, // stereo, 37800 Hz, 4-bit
        };
        let other = Subheader { channel: 2, ..audio };
        let video = Subheader { file: 1, channel: 0, submode: submode::VIDEO | submode::DATA, coding: 0 };
        let mut group = [0u8; 128];
        group[..4].copy_from_slice(&[0x0C, 0x0C, 0x0C, 0x0C]);
        group[16..].fill(0x77);
        let adpcm: Vec<u8> = group.iter().copied().cycle().take(0x900).collect();
        let sectors = (0..6u32).map(|lba| match lba % 3 {
            0 => build_mode2_sector(lba, video, &[0x5A; 2048]),
            1 => build_mode2_sector(lba, audio, &adpcm),
            _ => build_mode2_sector(lba, other, &adpcm),
        });
        let mut cd = Cd::default();
        cd.insert_disc(Box::new(MemorySource::from_sectors(sectors)));
        let mut now = 0;
        cd.command(com::SETFILTER, &[1, 1], now);
        cd.command(com::SETMODE, &[0xC8], now);
        cd.command(com::READS, &[], now);
        let events = run(&mut cd, &mut now, CPU_HZ * 3);
        let data: Vec<_> = events.iter().filter(|e| e.1 == intr::DATA_READY).collect();
        // Only the two video sectors reach the CPU; channel 1 plays, channel 2 is dropped.
        assert_eq!(data.len(), 2);
        let mut pcm = Vec::new();
        cd.take_audio(&mut pcm);
        // Two stereo 37800 Hz sectors of 2016 frames each, resampled to 44100 Hz.
        let frames = pcm.len() / 2;
        assert!((4600..=4710).contains(&frames), "{frames} frames");
        assert!(pcm.iter().any(|&s| s != 0));
        // CdMix routes left to the right output only.
        let mut mixed = Cd::default();
        mixed.atv = [0, 0x80, 0, 0];
        mixed.xa.set_mode(0x40);
        mixed.xa.deliver(XaSubheader::from_bytes([1, 1, audio.submode, 0x01]), &adpcm);
        let mut out = Vec::new();
        mixed.take_audio(&mut out);
        assert!(out.chunks(2).all(|f| f[0] == 0));
        assert!(out.chunks(2).any(|f| f[1] != 0));
    }
}
