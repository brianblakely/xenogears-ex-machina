//! GPU: the registers and DMA channel 2 that libgpu (port/libgpu.c) writes,
//! over xem-gpu (port/include/xem/gpu.h).
//!
//! A DMA2 transfer moves its words when the game starts it, so VRAM and game
//! memory are up to date at once, but the channel stays busy (CHCR bit 24)
//! until the game next waits: the transfer's completion is a device event at
//! the cycle it started, which the run loop handles at the wait and which
//! raises the DMA interrupt for channel 2. libgpu queues work and runs its
//! DrawSync callback from that interrupt, as on the console.

use super::{Context, Device, unknown};
use crate::clock::VBLANK_CYCLES;
use crate::memory::{GameMemory, RAM_BASE, RAM_SIZE};
use crate::module::Action;
use crate::runtime::IRQ_DMA;

/// The DMA channel of the GPU.
pub const DMA_CHANNEL: u32 = 2;
/// CHCR bit 24: start, and busy while the transfer runs.
pub const CHCR_BUSY: u32 = 0x0100_0000;
/// The most GP0 words one linked list may send (a list longer than RAM
/// revisits nodes; xem-gpu also stops at a loop).
const LIST_WORD_LIMIT: u32 = RAM_SIZE / 4;

const SNAPSHOT_MAGIC: &[u8; 8] = b"XEMGPUD1";

/// DMA addresses are 24-bit physical: RAM mirrors over the low 8 MB.
fn ram_address(address: u32) -> u32 {
    RAM_BASE | (address & (RAM_SIZE - 1))
}

/// Game memory as DMA sees it. A read outside game memory gives 0 and a
/// write is dropped (both only reachable through RAM mirrors, which map
/// inside).
struct DmaMemory<'a, M: GameMemory + ?Sized>(&'a mut M);

impl<M: GameMemory + ?Sized> xem_gpu::Memory for DmaMemory<'_, M> {
    fn read_u32(&self, address: u32) -> u32 {
        self.0.read_u32(ram_address(address)).unwrap_or(0)
    }
}

impl<M: GameMemory + ?Sized> xem_gpu::MemoryMut for DmaMemory<'_, M> {
    fn write_u32(&mut self, address: u32, value: u32) {
        let _ = self.0.write_u32(ram_address(address), value);
    }
}

#[derive(Default)]
pub struct Gpu {
    gpu: xem_gpu::Gpu,
    /// DMA2 MADR, BCR and CHCR as last written (CHCR bit 24 while busy).
    madr: u32,
    bcr: u32,
    chcr: u32,
    /// The cycle at which the running transfer ends.
    completion: Option<u64>,
    /// Vertical blanks seen (the interlaced field toggles at each).
    vblanks: u64,
}

impl Gpu {
    /// The xem-gpu device: VRAM, registers, display readout.
    pub fn device(&self) -> &xem_gpu::Gpu {
        &self.gpu
    }

    pub fn device_mut(&mut self) -> &mut xem_gpu::Gpu {
        &mut self.gpu
    }

    /// The displayed picture: (width, height, RGBA8 rows).
    pub fn display_rgba8(&self) -> (u32, u32, Vec<u8>) {
        self.gpu.display_rgba8()
    }

    /// DMA2's CHCR (bit 24 while a transfer runs).
    pub fn dma_control(&self) -> u32 {
        self.chcr
    }

    /// Start DMA2 as a CHCR write does: move the words now, end at `now`'s
    /// next wait. Bits 9-10 select block (1) or linked-list (2) mode, bit 0
    /// the direction (1: memory to GPU).
    fn start_dma(&mut self, memory: &mut dyn GameMemory, now: u64) -> Result<(), String> {
        if self.chcr & CHCR_BUSY == 0 {
            self.completion = None;
            return Ok(());
        }
        let mut memory = DmaMemory(memory);
        let from_memory = self.chcr & 1 != 0;
        match (self.chcr >> 9 & 3, from_memory) {
            (2, true) => {
                self.gpu.dma_linked_list(&memory, self.madr, LIST_WORD_LIMIT).map_err(|error| error.to_string())?;
            }
            (1, direction) | (0, direction) => {
                let size = match self.bcr & 0xFFFF {
                    0 => 0x1_0000,
                    size => size,
                };
                let blocks = if self.chcr >> 9 & 3 == 1 { (self.bcr >> 16).max(1) } else { 1 };
                let words = size * blocks;
                if direction {
                    self.gpu.dma_block_write(&memory, self.madr, words);
                } else {
                    self.gpu.dma_block_read(&mut memory, self.madr, words);
                }
            }
            (mode, _) => {
                return Err(format!("GPU DMA mode {mode} (CHCR {:#010x}) is not supported", self.chcr));
            }
        }
        self.completion = Some(now);
        Ok(())
    }
}

impl Device for Gpu {
    fn import(&mut self, op: &str, args: &[u32], context: &mut Context<'_>) -> Action {
        let arg = |i: usize| args.get(i).copied().unwrap_or(0);
        match op {
            "gp0" => self.gpu.write_gp0(arg(0)),
            "gp1" => self.gpu.write_gp1(arg(0)),
            "read" => return Action::Return(self.gpu.read_gpuread()),
            "status" => return Action::Return(self.gpu.gpustat()),
            "dma_control" => return Action::Return(self.chcr),
            "dma" => {
                self.madr = arg(0) & 0xFF_FFFF;
                self.bcr = arg(1);
                self.chcr = arg(2);
                if let Err(reason) = self.start_dma(context.memory, context.now) {
                    *context.trap_reason = Some(reason);
                    return Action::Trap;
                }
            }
            _ => return unknown("gpu", op, context),
        }
        Action::Return(0)
    }

    fn next_event(&self) -> Option<u64> {
        self.completion
    }

    /// The transfer ended: the channel is free and raises its interrupt.
    fn run_events(&mut self, context: &mut Context<'_>) {
        if self.completion.is_some_and(|at| at <= context.now) {
            self.completion = None;
            self.chcr &= !CHCR_BUSY;
            context.raised.push((IRQ_DMA, DMA_CHANNEL));
        }
    }

    /// The device state for a snapshot: DMA2 and xem-gpu's own snapshot.
    fn save(&self) -> Vec<u8> {
        let mut out = SNAPSHOT_MAGIC.to_vec();
        for word in [self.madr, self.bcr, self.chcr] {
            out.extend_from_slice(&word.to_le_bytes());
        }
        out.push(self.completion.is_some() as u8);
        out.extend_from_slice(&self.completion.unwrap_or(0).to_le_bytes());
        out.extend_from_slice(&self.vblanks.to_le_bytes());
        out.extend_from_slice(&self.gpu.save());
        out
    }

    /// Restore a [`Gpu::save`] snapshot; on error the device is unchanged.
    fn load(&mut self, data: &[u8]) -> Result<(), String> {
        let header = 8 + 12 + 1 + 8 + 8;
        if data.len() < header || &data[..8] != SNAPSHOT_MAGIC {
            return Err("not a GPU device snapshot".into());
        }
        let word = |at: usize| u32::from_le_bytes(data[at..at + 4].try_into().unwrap());
        let wide = |at: usize| u64::from_le_bytes(data[at..at + 8].try_into().unwrap());
        let mut gpu = xem_gpu::Gpu::new();
        gpu.load(&data[header..]).map_err(|error| error.to_string())?;
        self.gpu = gpu;
        self.madr = word(8);
        self.bcr = word(12);
        self.chcr = word(16);
        self.completion = (data[20] != 0).then(|| wide(21));
        self.vblanks = wide(29);
        Ok(())
    }

    /// Vertical blanks change the interlaced field.
    fn advance(&mut self, context: &mut Context<'_>) {
        let frames = context.now / VBLANK_CYCLES;
        while self.vblanks < frames {
            self.gpu.vblank();
            self.vblanks += 1;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory::OutOfBounds;

    /// RAM at its KSEG0 addresses.
    struct Ram(Vec<u8>);

    impl Ram {
        fn new() -> Ram {
            Ram(vec![0; RAM_SIZE as usize])
        }

        fn words(&mut self, address: u32, words: &[u32]) {
            for (i, word) in words.iter().enumerate() {
                self.write_u32(address + 4 * i as u32, *word).unwrap();
            }
        }
    }

    impl GameMemory for Ram {
        fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
            let start = address.wrapping_sub(RAM_BASE) as usize;
            let bytes = self.0.get(start..start + out.len()).ok_or(OutOfBounds { address, len: out.len() })?;
            out.copy_from_slice(bytes);
            Ok(())
        }

        fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
            let start = address.wrapping_sub(RAM_BASE) as usize;
            let len = data.len();
            self.0.get_mut(start..start + len).ok_or(OutOfBounds { address, len })?.copy_from_slice(data);
            Ok(())
        }
    }

    struct Harness {
        gpu: Gpu,
        ram: Ram,
        raised: Vec<(u32, u32)>,
        trap: Option<String>,
        now: u64,
    }

    impl Harness {
        fn new() -> Harness {
            Harness { gpu: Gpu::default(), ram: Ram::new(), raised: Vec::new(), trap: None, now: 1000 }
        }

        fn call(&mut self, op: &str, args: &[u32]) -> Action {
            let mut context =
                Context { memory: &mut self.ram, now: self.now, raised: &mut self.raised, trap_reason: &mut self.trap };
            self.gpu.import(op, args, &mut context)
        }

        /// The game waits: due events run.
        fn wait(&mut self) {
            if self.gpu.next_event().is_some_and(|at| at <= self.now) {
                let mut context = Context {
                    memory: &mut self.ram,
                    now: self.now,
                    raised: &mut self.raised,
                    trap_reason: &mut self.trap,
                };
                self.gpu.run_events(&mut context);
            }
        }

        fn pixel(&self, x: usize, y: usize) -> u16 {
            self.gpu.device().vram()[y * xem_gpu::VRAM_WIDTH + x]
        }
    }

    /// A reverse ordering table of two entries with one GP0(02h) fill in it,
    /// as ClearOTagR and AddPrim leave it.
    fn ordering_table(ram: &mut Ram) -> u32 {
        // ot[0] (0x80010000) ends the list at a terminator node with four NOPs.
        ram.words(0x8002_0000, &[0x04FF_FFFF, 0, 0, 0, 0]);
        ram.words(0x8001_0000, &[0x02_0000, 0x0001_0000]);
        // The fill packet links back to ot[0].
        ram.words(0x8003_0000, &[0x0301_0000, 0x0200_00FF, 0x0010_0020, 0x0008_0040]);
        ram.write_u32(0x8001_0004, 0x03_0000).unwrap();
        0x8001_0004
    }

    #[test]
    fn linked_list_draws_at_once_and_ends_at_the_wait() {
        let mut h = Harness::new();
        let ot = ordering_table(&mut h.ram);
        assert_eq!(h.call("gp1", &[0x0400_0002]), Action::Return(0));
        assert_eq!(h.call("dma", &[ot, 0, 0x0100_0401]), Action::Return(0));
        // Drawn at once, busy until the wait.
        assert_eq!(h.pixel(0x20, 0x10), 0x001F);
        assert_eq!(h.pixel(0x5F, 0x17), 0x001F);
        assert_eq!(h.pixel(0x60, 0x10), 0);
        assert_eq!(h.call("dma_control", &[]), Action::Return(0x0100_0401));
        assert!(h.raised.is_empty());
        h.wait();
        assert_eq!(h.call("dma_control", &[]), Action::Return(0x0000_0401));
        assert_eq!(h.raised, vec![(IRQ_DMA, DMA_CHANNEL)]);
        assert_eq!(h.gpu.next_event(), None);
    }

    #[test]
    fn block_transfers_load_and_store_images() {
        let mut h = Harness::new();
        // LoadImage of a 16x2 rectangle: 16 words, one block of 16 by DMA.
        let pixels: Vec<u32> = (0..16u32).map(|i| ((2 * i + 1) << 16) | (2 * i)).collect();
        h.ram.words(0x8004_0000, &pixels);
        for word in [0x0100_0000, 0xA000_0000, 0x0020_0100, 0x0002_0010] {
            h.call("gp0", &[word]);
        }
        h.call("dma", &[0x0004_0000, 1 << 16 | 0x10, 0x0100_0201]);
        assert_eq!(h.pixel(0x100, 0x20), 0);
        assert_eq!(h.pixel(0x10F, 0x21), 31);
        h.wait();
        // StoreImage of the same rectangle into other memory.
        for word in [0x0100_0000, 0xC000_0000, 0x0020_0100, 0x0002_0010] {
            h.call("gp0", &[word]);
        }
        let Action::Return(status) = h.call("status", &[]) else { unreachable!() };
        assert_ne!(status & 0x0800_0000, 0, "VRAM-to-CPU data is ready");
        h.call("dma", &[0x8005_0000, 1 << 16 | 0x10, 0x0100_0200]);
        for (i, word) in pixels.iter().enumerate() {
            assert_eq!(h.ram.read_u32(0x8005_0000 + 4 * i as u32).unwrap(), *word);
        }
    }

    #[test]
    fn stopping_the_channel_cancels_its_completion() {
        let mut h = Harness::new();
        let ot = ordering_table(&mut h.ram);
        h.call("dma", &[ot, 0, 0x0100_0401]);
        h.call("dma", &[0, 0, 0x401]);
        h.wait();
        assert!(h.raised.is_empty());
        assert_eq!(h.call("dma_control", &[]), Action::Return(0x401));
    }

    #[test]
    fn a_looping_list_traps() {
        let mut h = Harness::new();
        h.ram.words(0x8001_0000, &[0x0001_0004, 0x0001_0000]);
        assert_eq!(h.call("dma", &[0x8001_0000, 0, 0x0100_0401]), Action::Trap);
        assert!(h.trap.as_deref().unwrap().contains("loops"));
    }

    #[test]
    fn display_follows_gp1_and_snapshots_restore_the_device() {
        let mut h = Harness::new();
        // A red 320x240 NTSC display at (0, 0), as PutDispEnv sets it.
        for word in [0x0300_0000, 0x0500_0000, 0x06C6_0260, 0x0704_0010, 0x0800_0001] {
            h.call("gp1", &[word]);
        }
        for word in [0x0200_00FF, 0x0000_0000, 0x00F0_0140] {
            h.call("gp0", &[word]);
        }
        let (width, height, rgba) = h.gpu.display_rgba8();
        assert_eq!((width, height), (320, 240));
        assert_eq!(&rgba[..4], &[0xFF, 0, 0, 0xFF]);
        let ot = ordering_table(&mut h.ram);
        h.call("dma", &[ot, 0, 0x0100_0401]);
        let snapshot = h.gpu.save();
        let mut restored = Gpu::default();
        restored.load(&snapshot).unwrap();
        assert_eq!(restored.dma_control(), 0x0100_0401);
        assert_eq!(restored.next_event(), Some(h.now));
        assert_eq!(restored.display_rgba8(), h.gpu.display_rgba8());
        assert!(restored.load(b"nonsense").is_err());
    }

    #[test]
    fn unknown_operations_trap() {
        let mut h = Harness::new();
        assert_eq!(h.call("fly", &[]), Action::Trap);
        assert_eq!(h.trap.as_deref(), Some("unknown import xem.gpu_fly"));
    }
}
