//! Presentation records: what the port captures at the original's
//! pre-projection seams (src/port/present.c) during a frame, kept read-only for
//! renderers, spectators and agents. They never feed back into the game.

/// Record kinds (src/port/present.c).
pub const MODEL: u32 = 1;
pub const SPRITE_MATRIX: u32 = 2;
pub const SPRITE_PARTS: u32 = 3;
pub const SPRITE_FACING: u32 = 4;

/// Copy the port's temporary facing assembly before its scratch area is reused.
/// The pixels are owned by presentation and never uploaded to the game GPU.
pub(crate) fn sprite_facing(memory: &dyn crate::memory::GameMemory, address: u32)
    -> Result<(u32, u32, xem_gpu::SpriteFacing), String>
{
    use xem_gpu::{Projection, SpriteFacing, SpriteFacingPart, VramRect};
    let read = |address, len| {
        let mut bytes = vec![0; len];
        memory.read(address, &mut bytes).map_err(|e| e.to_string())?;
        Ok::<_, String>(bytes)
    };
    let word = |bytes: &[u8], at: usize| u32::from_le_bytes(bytes[at..at + 4].try_into().unwrap());
    let header = read(address, 28)?;
    let count = word(&header, 12) as usize;
    let uploads = word(&header, 20) as usize;
    if count > 63 || uploads > 65 { return Err("invalid sprite facing record".into()); }
    let mut facing = SpriteFacing::default();
    for p in read(word(&header,16), count * 196)?.chunks_exact(196) {
        let words = std::array::from_fn(|i| word(p,i * 4));
        let points = std::array::from_fn(|i| {
            let p = &p[36 + i * 40..];
            let offset = word(p,16);
            Projection {
                sxy: word(p,0), sz: word(p,4) as u16, class: (word(p,4) >> 16) as u8,
                x: word(p,8) as i32, y: word(p,12) as i32,
                offset: (offset as i16,(offset >> 16) as i16), h: word(p,20) as u16,
                owner: word(p,24), view: [word(p,28) as i32,word(p,32) as i32,word(p,36) as i32],
            }
        });
        facing.parts.push(SpriteFacingPart { words, points });
    }
    // The original queues uploads at the list head (including its final clear).
    for u in read(word(&header,24), uploads * 12)?.chunks_exact(12).rev() {
        let half = |at| u16::from_le_bytes(u[at..at + 2].try_into().unwrap());
        let rect = VramRect { x: half(4), y: half(6), width: half(8), height: half(10) };
        if usize::from(rect.x) + usize::from(rect.width) > xem_gpu::VRAM_WIDTH ||
            usize::from(rect.y) + usize::from(rect.height) > xem_gpu::VRAM_HEIGHT {
            return Err("sprite facing texture outside VRAM".into());
        }
        let len = usize::from(rect.width) * usize::from(rect.height);
        let pixels = if word(u,0) == 0 { vec![0; len] } else {
            read(word(u,0), len * 2)?.chunks_exact(2).map(|p| u16::from_le_bytes([p[0],p[1]])).collect()
        };
        facing.textures.push((rect, pixels.into()));
    }
    Ok((word(&header,4), word(&header,8), facing))
}

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
