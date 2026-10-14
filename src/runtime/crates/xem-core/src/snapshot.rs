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
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};

const MAGIC: &[u8; 8] = b"XEMSNAP\0";
/// The state format. A state of another version is refused, never converted.
pub const VERSION: u32 = 9;
/// The SHA-256 of the three stand-in states xem-core's tests/persistence.rs
/// builds (`format_fingerprint_matches_version`), as this format lays them
/// out. Between them they hold each part of the format and each variant it
/// writes (every fiber, drive, event, receiver and queue kind), so a change
/// to what a state holds or to how any of these is written changes it, and
/// the test fails until [`VERSION`] and this change together. A variant
/// none of them holds (one added later) is outside it.
pub const FORMAT_FINGERPRINT: &str = "9ddf2d36321229279f8aea05ce3bda2acbcbea0d4e14b15d59c0ce7da4a9880e";
/// Hard bound on any decoded snapshot region/device buffer.
pub const MAX_BUFFER: usize = 32 * 1024 * 1024;
pub const MAX_STATE: usize = 64 * 1024 * 1024;

/// Content identities, not machine architecture or presentation settings.
/// Packages are ordered in their actual load order and use content digests.
#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct StateIdentity {
    pub module: String,
    pub disc: String,
    pub packages: Vec<String>,
}

impl StateIdentity {
    /// What of `state` differs from this identity, each part with the
    /// state's value and this one's; None when they match.
    pub fn difference(&self, state: &StateIdentity) -> Option<String> {
        let mut parts = Vec::new();
        if state.module != self.module {
            parts.push(format!("the state's game module is {:?}, this session's is {:?}", state.module, self.module));
        }
        if state.disc != self.disc {
            parts.push(format!("the state's disc is {:?}, this session's is {:?}", state.disc, self.disc));
        }
        if state.packages != self.packages {
            parts.push(format!("the state's packages are {:?}, this session's are {:?}", state.packages, self.packages));
        }
        (!parts.is_empty()).then(|| parts.join("; "))
    }
}

pub fn digest(bytes: &[u8]) -> [u8; 32] {
    Sha256::digest(bytes).into()
}

pub fn seal(mut bytes: Vec<u8>) -> Vec<u8> {
    let hash = digest(&bytes);
    bytes.reserve_exact(hash.len());
    bytes.extend_from_slice(&hash);
    bytes.shrink_to_fit();
    bytes
}

pub fn checked(bytes: &[u8]) -> Result<&[u8], SnapshotError> {
    if bytes.len() < 32 || bytes.len() > MAX_STATE {
        return Err(SnapshotError::Size(bytes.len()));
    }
    let (data, hash) = bytes.split_at(bytes.len() - 32);
    if digest(data) != hash {
        return Err(SnapshotError::Format("checksum"));
    }
    Ok(data)
}

/// The linear-memory regions a snapshot keeps: KUSEG below 8 MB (where stray
/// game stores land), the port's data, shadow stack and asyncify areas
/// (tools/game_module.py PORT_BASE), the scratchpad and I/O pages, main RAM.
pub const REGIONS: [(u32, u32); 4] =
    [(0, 0x80_0000), (0x1000_0000, 0x0100_0000), (0x1F80_0000, 0x3000), (0x8000_0000, 0x20_0000)];

#[derive(Debug)]
pub enum SnapshotError {
    Format(&'static str),
    /// A file of `n` bytes: under the seal's 32 or over [`MAX_STATE`].
    Size(usize),
    /// What carries the version (the format, a device), the version the
    /// state has and the one this runtime reads.
    Version { what: &'static str, found: u32, expected: u32 },
    /// The identity record does not decode (an unknown field, say).
    Identity(String),
    /// The state names another game module, disc or package list: each
    /// part that differs, with the state's value and this session's.
    Incompatible(String),
    Memory(OutOfBounds),
    Device(String),
}

impl std::fmt::Display for SnapshotError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            SnapshotError::Format(what) => write!(f, "bad snapshot: {what}"),
            SnapshotError::Size(n) => write!(f, "bad snapshot: state size {n} bytes, outside 32 bytes to 64 MiB"),
            SnapshotError::Version { what, found, expected } => {
                write!(f, "bad snapshot: {what} version {found}, expected {expected}")
            }
            SnapshotError::Identity(e) => write!(f, "bad snapshot: identity: {e}"),
            SnapshotError::Incompatible(what) => write!(f, "incompatible state: {what}"),
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
        while let Some(word) = word_at(data, i) {
            // Every compressible run (>=16 zeros) contains a whole 8-byte
            // word, regardless of alignment. Skip mixed words, including
            // short zeros in assets, and refine only all-zero words.
            if word != 0 {
                i += 8;
                continue;
            }
            let mut zeros = i;
            while zeros > literal && data[zeros - 1] == 0 {
                zeros -= 1;
            }
            while word_at(data, i) == Some(0) {
                i += 8;
            }
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

#[inline]
fn word_at(data: &[u8], at: usize) -> Option<u64> {
    data.get(at..at + 8).map(|word| u64::from_le_bytes(word.try_into().unwrap()))
}

#[cfg(test)]
mod word_scan_tests {
    use super::*;

    #[test]
    fn word_scan_preserves_byte_runs_at_every_alignment_and_length() {
        fn byte_scan(data: &[u8]) -> Vec<u8> {
            let mut w = Writer::default();
            w.u32(data.len() as u32);
            let (mut literal, mut i) = (0, 0);
            while i < data.len() {
                if data[i] != 0 { i += 1; continue; }
                let zeros = i;
                while i < data.len() && data[i] == 0 { i += 1; }
                if i - zeros >= 16 {
                    w.literal(&data[literal..zeros]);
                    w.u32((i - zeros) as u32 | 0x8000_0000);
                    literal = i;
                }
            }
            w.literal(&data[literal..]);
            w.0
        }
        for offset in 0..16 {
            for zeros in 0..80 {
                for tail in 0..16 {
                    let mut data = vec![0xff; offset];
                    data.extend(std::iter::repeat_n(0, zeros));
                    data.extend(std::iter::repeat_n(1, tail));
                    data.extend_from_slice(&[2, 1, 0, 0xff, 0, 0, 3]);
                    let mut w = Writer::default();
                    w.memory(&data);
                    assert_eq!(w.0, byte_scan(&data),"offset={offset}, zeros={zeros}, tail={tail}");
                    assert_eq!(Reader::new(&w.0).memory().unwrap(),data);
                }
            }
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
        self.memory_limit(MAX_BUFFER)
    }
    pub fn memory_limit(&mut self, limit: usize) -> Result<Vec<u8>, SnapshotError> {
        let len = self.u32()? as usize;
        if len > limit {
            return Err(SnapshotError::Format("memory size exceeds limit"));
        }
        let mut out = Vec::with_capacity(len);
        while out.len() < len {
            let run = self.u32()?;
            let count = (run & 0x7FFF_FFFF) as usize;
            if count == 0 || count > len - out.len() {
                return Err(SnapshotError::Format("invalid memory run"));
            }
            if run & 0x8000_0000 != 0 {
                out.resize(out.len() + count, 0);
            } else {
                out.extend_from_slice(self.take(count)?);
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
    /// A length for fixed-width records, checked before allocating.
    pub fn count(&mut self, width: usize, limit: usize) -> Result<usize, SnapshotError> {
        let count = self.u32()? as usize;
        if count > limit || count > (self.data.len() - self.at) / width {
            return Err(SnapshotError::Format("record count exceeds limit"));
        }
        Ok(count)
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
    let found = r.u32()?;
    if found != VERSION {
        return Err(SnapshotError::Version { what: "format", found, expected: VERSION });
    }
    Ok(())
}

pub(crate) fn write_memory(w: &mut Writer, memory: &dyn GameMemory) -> Result<(), SnapshotError> {
    for (start, len) in REGIONS {
        memory.with_bytes(start, len as usize, &mut |data| w.memory(data))?;
    }
    Ok(())
}

pub(crate) fn read_memory(r: &mut Reader<'_>) -> Result<Vec<Vec<u8>>, SnapshotError> {
    let mut regions = Vec::new();
    for (_, len) in REGIONS {
        let data = r.memory_limit(len as usize)?;
        if data.len() != len as usize {
            return Err(SnapshotError::Format("region size"));
        }
        regions.push(data);
    }
    Ok(regions)
}

pub(crate) fn write_clock(w: &mut Writer, clock: &Clock) {
    w.u64(clock.now);
    w.u64(clock.vblanks);
    w.u64(clock.audio_now);
    w.u64(clock.audio_due);
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
    clock.audio_now = r.u64()?;
    clock.audio_due = r.u64()?;
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

    #[test]
    fn malformed_runs_and_unbounded_allocations_are_rejected() {
        for words in [[1, 0], [1, 0x8000_0000], [1, 0x8000_0002], [u32::MAX, 1]] {
            let bytes: Vec<_> = words.into_iter().flat_map(u32::to_le_bytes).collect();
            assert!(Reader::new(&bytes).memory().is_err());
        }
    }
}
