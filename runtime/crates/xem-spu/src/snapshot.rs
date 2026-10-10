//! Little-endian field encoding for SPU snapshots.

use std::fmt;

/// Why a snapshot could not be loaded.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum SnapshotError {
    /// The data does not start with the SPU snapshot magic.
    BadMagic,
    /// The snapshot was written by another format version.
    UnsupportedVersion(u32),
    /// The data ended before the snapshot did.
    Truncated,
    /// Bytes follow the end of the snapshot.
    TrailingData,
    /// A field holds a value no snapshot writes.
    InvalidValue(&'static str),
}

impl fmt::Display for SnapshotError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            SnapshotError::BadMagic => write!(f, "not an SPU snapshot"),
            SnapshotError::UnsupportedVersion(v) => {
                write!(f, "unsupported SPU snapshot version {v}")
            }
            SnapshotError::Truncated => write!(f, "truncated SPU snapshot"),
            SnapshotError::TrailingData => write!(f, "trailing data after SPU snapshot"),
            SnapshotError::InvalidValue(what) => write!(f, "invalid SPU snapshot field: {what}"),
        }
    }
}

impl std::error::Error for SnapshotError {}

pub(crate) struct Writer(pub Vec<u8>);

impl Writer {
    pub fn bytes(&mut self, b: &[u8]) {
        self.0.extend_from_slice(b);
    }
    pub fn u8(&mut self, v: u8) {
        self.0.push(v);
    }
    pub fn bool(&mut self, v: bool) {
        self.0.push(v as u8);
    }
    pub fn u16(&mut self, v: u16) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn i16(&mut self, v: i16) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn u32(&mut self, v: u32) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn i32(&mut self, v: i32) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn u64(&mut self, v: u64) {
        self.bytes(&v.to_le_bytes());
    }
}

pub(crate) struct Reader<'a> {
    data: &'a [u8],
}

impl<'a> Reader<'a> {
    pub fn new(data: &'a [u8]) -> Self {
        Reader { data }
    }
    pub fn bytes(&mut self, n: usize) -> Result<&'a [u8], SnapshotError> {
        if self.data.len() < n {
            return Err(SnapshotError::Truncated);
        }
        let (head, tail) = self.data.split_at(n);
        self.data = tail;
        Ok(head)
    }
    fn array<const N: usize>(&mut self) -> Result<[u8; N], SnapshotError> {
        Ok(self.bytes(N)?.try_into().expect("length checked"))
    }
    pub fn u8(&mut self) -> Result<u8, SnapshotError> {
        Ok(self.array::<1>()?[0])
    }
    pub fn bool(&mut self) -> Result<bool, SnapshotError> {
        match self.u8()? {
            0 => Ok(false),
            1 => Ok(true),
            _ => Err(SnapshotError::InvalidValue("bool")),
        }
    }
    pub fn u16(&mut self) -> Result<u16, SnapshotError> {
        Ok(u16::from_le_bytes(self.array()?))
    }
    pub fn i16(&mut self) -> Result<i16, SnapshotError> {
        Ok(i16::from_le_bytes(self.array()?))
    }
    pub fn u32(&mut self) -> Result<u32, SnapshotError> {
        Ok(u32::from_le_bytes(self.array()?))
    }
    pub fn i32(&mut self) -> Result<i32, SnapshotError> {
        Ok(i32::from_le_bytes(self.array()?))
    }
    pub fn u64(&mut self) -> Result<u64, SnapshotError> {
        Ok(u64::from_le_bytes(self.array()?))
    }
    pub fn finish(self) -> Result<(), SnapshotError> {
        if self.data.is_empty() {
            Ok(())
        } else {
            Err(SnapshotError::TrailingData)
        }
    }
}
