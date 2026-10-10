//! A small, explicit byte format for device snapshots: little-endian fields
//! behind a four-byte tag and a version byte, so a snapshot never depends on
//! the host's layout, pointer width or endianness.

use std::fmt;

/// Why a snapshot could not be loaded.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SnapshotError {
    /// The data does not start with this device's tag.
    WrongTag,
    /// The data was written by an unknown format version.
    UnsupportedVersion(u8),
    /// The data ends before the state does.
    Truncated,
    /// The data continues after the state.
    TrailingBytes,
    /// A field holds a value the device cannot be in.
    InvalidValue(&'static str),
}

impl fmt::Display for SnapshotError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::WrongTag => write!(f, "snapshot belongs to another device"),
            Self::UnsupportedVersion(v) => write!(f, "unsupported snapshot version {v}"),
            Self::Truncated => write!(f, "snapshot is truncated"),
            Self::TrailingBytes => write!(f, "snapshot has trailing bytes"),
            Self::InvalidValue(field) => write!(f, "snapshot field {field} is out of range"),
        }
    }
}

impl std::error::Error for SnapshotError {}

pub(crate) struct Writer(Vec<u8>);

impl Writer {
    pub(crate) fn new(tag: &[u8; 4], version: u8) -> Self {
        let mut bytes = Vec::with_capacity(256);
        bytes.extend_from_slice(tag);
        bytes.push(version);
        Self(bytes)
    }

    pub(crate) fn u8(&mut self, v: u8) {
        self.0.push(v);
    }

    pub(crate) fn bool(&mut self, v: bool) {
        self.0.push(u8::from(v));
    }

    pub(crate) fn u16(&mut self, v: u16) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }

    pub(crate) fn i16(&mut self, v: i16) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }

    pub(crate) fn u32(&mut self, v: u32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }

    pub(crate) fn bytes(&mut self, v: &[u8]) {
        self.0.extend_from_slice(v);
    }

    pub(crate) fn finish(self) -> Vec<u8> {
        self.0
    }
}

pub(crate) struct Reader<'a>(&'a [u8]);

impl<'a> Reader<'a> {
    pub(crate) fn new(data: &'a [u8], tag: &[u8; 4], version: u8) -> Result<Self, SnapshotError> {
        let mut reader = Self(data);
        if reader.take(4).map_err(|_| SnapshotError::WrongTag)? != tag {
            return Err(SnapshotError::WrongTag);
        }
        let found = reader.u8()?;
        if found != version {
            return Err(SnapshotError::UnsupportedVersion(found));
        }
        Ok(reader)
    }

    fn take(&mut self, n: usize) -> Result<&'a [u8], SnapshotError> {
        if self.0.len() < n {
            return Err(SnapshotError::Truncated);
        }
        let (head, rest) = self.0.split_at(n);
        self.0 = rest;
        Ok(head)
    }

    pub(crate) fn u8(&mut self) -> Result<u8, SnapshotError> {
        Ok(self.take(1)?[0])
    }

    pub(crate) fn bool(&mut self) -> Result<bool, SnapshotError> {
        match self.u8()? {
            0 => Ok(false),
            1 => Ok(true),
            _ => Err(SnapshotError::InvalidValue("bool")),
        }
    }

    pub(crate) fn u16(&mut self) -> Result<u16, SnapshotError> {
        let b = self.take(2)?;
        Ok(u16::from_le_bytes([b[0], b[1]]))
    }

    pub(crate) fn i16(&mut self) -> Result<i16, SnapshotError> {
        let b = self.take(2)?;
        Ok(i16::from_le_bytes([b[0], b[1]]))
    }

    pub(crate) fn u32(&mut self) -> Result<u32, SnapshotError> {
        let b = self.take(4)?;
        Ok(u32::from_le_bytes([b[0], b[1], b[2], b[3]]))
    }

    pub(crate) fn bytes(&mut self, out: &mut [u8]) -> Result<(), SnapshotError> {
        out.copy_from_slice(self.take(out.len())?);
        Ok(())
    }

    pub(crate) fn finish(self) -> Result<(), SnapshotError> {
        if self.0.is_empty() {
            Ok(())
        } else {
            Err(SnapshotError::TrailingBytes)
        }
    }
}
