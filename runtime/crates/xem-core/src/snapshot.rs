//! Snapshots: the whole runtime at a wait, restorable bit for bit.
//!
//! A snapshot holds the game module's memory regions (its own data, stack and
//! asyncify save area; the scratchpad and register pages; the PS1's 2 MB of
//! RAM), the module's stack pointer and suspension, the clock, the pads and
//! each device's state. Native pointers and device handles are never part of
//! it: game memory holds only PS1 addresses.

use crate::clock::{Clock, RootCounter};
use crate::memory::{GameMemory, OutOfBounds};
use crate::pad::{Pad, Pads};

const MAGIC: &[u8; 8] = b"XEMSNAP\0";
const VERSION: u32 = 1;

/// The linear-memory regions a snapshot keeps: the module's low memory (port
/// data, shadow stack, asyncify area), the scratchpad and I/O pages, main RAM.
pub const REGIONS: [(u32, u32); 3] = [(0, 0x0100_0000), (0x1F80_0000, 0x3000), (0x8000_0000, 0x20_0000)];

#[derive(Debug)]
pub enum SnapshotError {
    Format(&'static str),
    Memory(OutOfBounds),
    Device(String),
}

impl std::fmt::Display for SnapshotError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            SnapshotError::Format(what) => write!(f, "bad snapshot: {what}"),
            SnapshotError::Memory(e) => write!(f, "snapshot memory: {e}"),
            SnapshotError::Device(e) => write!(f, "snapshot device state: {e}"),
        }
    }
}

impl std::error::Error for SnapshotError {}

impl From<OutOfBounds> for SnapshotError {
    fn from(e: OutOfBounds) -> Self {
        SnapshotError::Memory(e)
    }
}

/// Little-endian writer with zero-run compression for memory.
#[derive(Default)]
pub struct Writer(pub Vec<u8>);

impl Writer {
    pub fn u32(&mut self, v: u32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    pub fn u64(&mut self, v: u64) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    pub fn bytes(&mut self, data: &[u8]) {
        self.u32(data.len() as u32);
        self.0.extend_from_slice(data);
    }
    /// Runs of literal bytes and of zeros (length | 0x8000_0000); zero runs
    /// shorter than 16 bytes stay literal.
    pub fn memory(&mut self, data: &[u8]) {
        self.u32(data.len() as u32);
        let mut literal = 0;
        let mut i = 0;
        while i < data.len() {
            if data[i] != 0 {
                i += 1;
                continue;
            }
            let zeros = i;
            while i < data.len() && data[i] == 0 {
                i += 1;
            }
            if i - zeros >= 16 {
                self.literal(&data[literal..zeros]);
                self.u32((i - zeros) as u32 | 0x8000_0000);
                literal = i;
            }
        }
        self.literal(&data[literal..]);
    }

    fn literal(&mut self, data: &[u8]) {
        if !data.is_empty() {
            self.u32(data.len() as u32);
            self.0.extend_from_slice(data);
        }
    }
}

pub struct Reader<'a> {
    data: &'a [u8],
    at: usize,
}

impl<'a> Reader<'a> {
    pub fn new(data: &'a [u8]) -> Self {
        Reader { data, at: 0 }
    }
    fn take(&mut self, n: usize) -> Result<&'a [u8], SnapshotError> {
        let end = self.at.checked_add(n).filter(|&e| e <= self.data.len()).ok_or(SnapshotError::Format("truncated"))?;
        let out = &self.data[self.at..end];
        self.at = end;
        Ok(out)
    }
    pub fn u32(&mut self) -> Result<u32, SnapshotError> {
        Ok(u32::from_le_bytes(self.take(4)?.try_into().unwrap()))
    }
    pub fn u64(&mut self) -> Result<u64, SnapshotError> {
        Ok(u64::from_le_bytes(self.take(8)?.try_into().unwrap()))
    }
    pub fn bytes(&mut self) -> Result<&'a [u8], SnapshotError> {
        let n = self.u32()? as usize;
        self.take(n)
    }
    pub fn memory(&mut self) -> Result<Vec<u8>, SnapshotError> {
        let len = self.u32()? as usize;
        let mut out = Vec::with_capacity(len);
        while out.len() < len {
            let run = self.u32()?;
            if run & 0x8000_0000 != 0 {
                out.resize(out.len() + (run & 0x7FFF_FFFF) as usize, 0);
            } else {
                out.extend_from_slice(self.take(run as usize)?);
            }
        }
        if out.len() != len {
            return Err(SnapshotError::Format("memory run overflows its region"));
        }
        Ok(out)
    }
    pub fn done(&self) -> bool {
        self.at == self.data.len()
    }
}

pub(crate) fn write_header(w: &mut Writer) {
    w.0.extend_from_slice(MAGIC);
    w.u32(VERSION);
}

pub(crate) fn read_header(r: &mut Reader<'_>) -> Result<(), SnapshotError> {
    if r.take(8)? != MAGIC {
        return Err(SnapshotError::Format("magic"));
    }
    if r.u32()? != VERSION {
        return Err(SnapshotError::Format("version"));
    }
    Ok(())
}

pub(crate) fn write_memory(w: &mut Writer, memory: &dyn GameMemory) -> Result<(), SnapshotError> {
    for (start, len) in REGIONS {
        let mut data = vec![0; len as usize];
        memory.read(start, &mut data)?;
        w.memory(&data);
    }
    Ok(())
}

pub(crate) fn read_memory(r: &mut Reader<'_>, memory: &mut dyn GameMemory) -> Result<(), SnapshotError> {
    for (start, len) in REGIONS {
        let data = r.memory()?;
        if data.len() != len as usize {
            return Err(SnapshotError::Format("region size"));
        }
        memory.write(start, &data)?;
    }
    Ok(())
}

pub(crate) fn write_clock(w: &mut Writer, clock: &Clock) {
    w.u64(clock.now);
    w.u64(clock.vblanks);
    for c in &clock.counters {
        w.u32(c.target);
        w.u32(c.mode);
        w.u32(c.running as u32);
        w.u64(c.origin);
        w.u64(c.due);
    }
}

pub(crate) fn read_clock(r: &mut Reader<'_>) -> Result<Clock, SnapshotError> {
    let mut clock = Clock { now: r.u64()?, vblanks: r.u64()?, ..Clock::default() };
    for c in clock.counters.iter_mut() {
        *c = RootCounter { target: r.u32()?, mode: r.u32()?, running: r.u32()? != 0, origin: r.u64()?, due: r.u64()? };
    }
    Ok(clock)
}

pub(crate) fn write_pads(w: &mut Writer, pads: &Pads) {
    for pad in &pads.0 {
        w.u32(pad.connected as u32);
        w.u32(pad.buttons as u32);
    }
}

pub(crate) fn read_pads(r: &mut Reader<'_>) -> Result<Pads, SnapshotError> {
    let mut pads = Pads::default();
    for pad in pads.0.iter_mut() {
        *pad = Pad { connected: r.u32()? != 0, buttons: r.u32()? as u16 };
    }
    Ok(pads)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn memory_runs_round_trip() {
        let mut data = vec![0u8; 5000];
        data[3] = 7;
        data[100..110].copy_from_slice(&[1; 10]);
        data[200] = 0;
        data[4999] = 9;
        let mut w = Writer::default();
        w.memory(&data);
        assert!(w.0.len() < 200);
        let mut r = Reader::new(&w.0);
        assert_eq!(r.memory().unwrap(), data);
        assert!(r.done());
    }
}
