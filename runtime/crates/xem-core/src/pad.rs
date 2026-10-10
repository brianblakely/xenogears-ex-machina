//! Controllers as the BIOS pad driver reports them in its receive buffers.

use crate::memory::{GameMemory, OutOfBounds};

/// Button bits as the receive buffer's data halfword has them (active low there).
pub mod button {
    pub const SELECT: u16 = 1 << 0;
    pub const L3: u16 = 1 << 1;
    pub const R3: u16 = 1 << 2;
    pub const START: u16 = 1 << 3;
    pub const UP: u16 = 1 << 4;
    pub const RIGHT: u16 = 1 << 5;
    pub const DOWN: u16 = 1 << 6;
    pub const LEFT: u16 = 1 << 7;
    pub const L2: u16 = 1 << 8;
    pub const R2: u16 = 1 << 9;
    pub const L1: u16 = 1 << 10;
    pub const R1: u16 = 1 << 11;
    pub const TRIANGLE: u16 = 1 << 12;
    pub const CIRCLE: u16 = 1 << 13;
    pub const CROSS: u16 = 1 << 14;
    pub const SQUARE: u16 = 1 << 15;
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Pad {
    pub connected: bool,
    /// Pressed buttons (`button::*`).
    pub buttons: u16,
}

/// Port 1 holds a digital pad; port 2 is empty unless the host connects one.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Pads(pub [Pad; 2]);

impl Default for Pads {
    fn default() -> Self {
        Pads([Pad { connected: true, buttons: 0 }, Pad::default()])
    }
}

impl Pads {
    /// Fill a receive buffer as the BIOS does: status 0x00 and the digital
    /// pad id 0x41 then the buttons active low, or status 0xFF without a pad.
    pub fn write_receive_buffer(
        &self,
        port: u32,
        address: u32,
        length: u32,
        memory: &mut dyn GameMemory,
    ) -> Result<(), OutOfBounds> {
        let pad = self.0.get(port as usize).copied().unwrap_or_default();
        let mut data = vec![0u8; length as usize];
        if pad.connected && data.len() >= 4 {
            data[0] = 0x00;
            data[1] = 0x41;
            data[2..4].copy_from_slice(&(!pad.buttons).to_le_bytes());
        } else if !data.is_empty() {
            data[0] = 0xFF;
        }
        memory.write(address, &data)
    }
}
