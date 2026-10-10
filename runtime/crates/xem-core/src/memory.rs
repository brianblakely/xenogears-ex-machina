//! Game memory: the PS1 address space inside the game module's linear memory.

/// Main RAM keeps its KSEG0 addresses.
pub const RAM_BASE: u32 = 0x8000_0000;
pub const RAM_SIZE: u32 = 0x20_0000;
/// The scratchpad keeps its address too.
pub const SCRATCHPAD_BASE: u32 = 0x1F80_0000;
pub const SCRATCHPAD_SIZE: u32 = 0x400;

/// Bounds-checked access to game memory. Natively it is the wasm2c memory;
/// in browsers the module's `WebAssembly.Memory`, reached through copies.
pub trait GameMemory {
    fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds>;
    fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds>;

    fn read_u32(&self, address: u32) -> Result<u32, OutOfBounds> {
        let mut word = [0; 4];
        self.read(address, &mut word)?;
        Ok(u32::from_le_bytes(word))
    }

    fn write_u32(&mut self, address: u32, value: u32) -> Result<(), OutOfBounds> {
        self.write(address, &value.to_le_bytes())
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutOfBounds {
    pub address: u32,
    pub len: usize,
}

impl std::fmt::Display for OutOfBounds {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "game memory access {:#010x}+{:#x} is out of bounds", self.address, self.len)
    }
}

impl std::error::Error for OutOfBounds {}

/// Game memory over a byte slice that starts at address 0.
pub struct SliceMemory<'a>(pub &'a mut [u8]);

impl SliceMemory<'_> {
    fn range(&self, address: u32, len: usize) -> Result<std::ops::Range<usize>, OutOfBounds> {
        let start = address as usize;
        match start.checked_add(len) {
            Some(end) if end <= self.0.len() => Ok(start..end),
            _ => Err(OutOfBounds { address, len }),
        }
    }
}

impl GameMemory for SliceMemory<'_> {
    fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
        let range = self.range(address, out.len())?;
        out.copy_from_slice(&self.0[range]);
        Ok(())
    }

    fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
        let range = self.range(address, data.len())?;
        self.0[range].copy_from_slice(data);
        Ok(())
    }
}
