//! Presentation records: what the port captures at the original's
//! pre-projection seams (port/present.c) during a frame, kept read-only for
//! renderers, spectators and agents. They never feed back into the game.

/// Record kinds (port/present.c).
pub const MODEL: u32 = 1;
pub const SPRITE_MATRIX: u32 = 2;
pub const SPRITE_PARTS: u32 = 3;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Record {
    pub kind: u32,
    pub bytes: Vec<u8>,
}

impl Record {
    pub fn word(&self, index: usize) -> u32 {
        self.bytes.get(index * 4..index * 4 + 4).map(|b| u32::from_le_bytes(b.try_into().unwrap())).unwrap_or(0)
    }
}

/// The records of the frame being drawn and of the last complete frame.
#[derive(Debug, Clone, Default)]
pub struct Presentation {
    pub current: Vec<Record>,
    pub last_frame: Vec<Record>,
    /// Records are kept only when someone presents or inspects them.
    pub enabled: bool,
}

impl Presentation {
    pub fn record(&mut self, kind: u32, bytes: Vec<u8>) {
        if self.enabled {
            self.current.push(Record { kind, bytes });
        }
    }

    /// A vertical blank ends the frame.
    pub fn end_frame(&mut self) {
        self.last_frame = std::mem::take(&mut self.current);
    }
}
