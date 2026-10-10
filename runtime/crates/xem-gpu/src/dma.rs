//! DMA channel 2 (GPU): linked-list and block transfers over host memory.
//!
//! psx-spx (DMA channels): in linked-list mode each node is a header word
//! whose bits 0-23 address the next node and bits 24-31 count the GP0 words
//! that follow the header; the walk ends at a next address with bit 23 set
//! (usually FFFFFFh). Addresses are 24-bit and word aligned. The memory
//! accessor receives these physical addresses (`address & 0xFF_FFFC`) and maps
//! them to RAM itself (the PS1 mirrors 2 MB of RAM over the low 8 MB).
//!
//! A list that loops would hang the hardware; here the walk detects the
//! cycle (Brent's algorithm over the node addresses, which do not depend on
//! the GPU) and stops with an error, and a word budget bounds long lists.

use crate::Gpu;

/// Read access to the memory DMA transfers come from.
pub trait Memory {
    /// The word at a 24-bit, word-aligned physical address.
    fn read_u32(&self, address: u32) -> u32;
}

/// Write access for GPU-to-memory block transfers.
pub trait MemoryMut: Memory {
    /// Stores a word at a 24-bit, word-aligned physical address.
    fn write_u32(&mut self, address: u32, value: u32);
}

/// What a completed linked-list walk did.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct DmaSummary {
    /// Nodes visited, including those with no words.
    pub nodes: u32,
    /// GP0 words sent.
    pub words: u32,
}

/// Why a linked-list walk stopped before its terminator. The nodes before
/// the failure have been sent to GP0, as the hardware would have.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DmaError {
    /// The list returned to `address`, a node it had already sent.
    Loop { address: u32, summary: DmaSummary },
    /// The node at `address` would exceed the word budget; it was not sent.
    WordLimit { address: u32, summary: DmaSummary },
}

impl std::fmt::Display for DmaError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            DmaError::Loop { address, summary } => write!(
                f,
                "GPU DMA list loops back to {address:06x} after {} nodes and {} words",
                summary.nodes, summary.words
            ),
            DmaError::WordLimit { address, summary } => write!(
                f,
                "GPU DMA list node {address:06x} exceeds the word budget after {} nodes and {} words",
                summary.nodes, summary.words
            ),
        }
    }
}

impl std::error::Error for DmaError {}

const ADDRESS_MASK: u32 = 0xFF_FFFC;

impl Gpu {
    /// Walks a DMA2 linked list from `head`, sending each node's words to
    /// GP0, until a next address with bit 23 set. At most `max_words` GP0
    /// words are sent.
    pub fn dma_linked_list<M: Memory + ?Sized>(
        &mut self,
        memory: &M,
        head: u32,
        max_words: u32,
    ) -> Result<DmaSummary, DmaError> {
        let mut summary = DmaSummary::default();
        let mut address = head & 0xFF_FFFF;
        // Brent: `saved` is the address `distance` nodes back, re-saved at
        // every power of two; meeting it again proves a cycle.
        let mut saved = address;
        let (mut power, mut distance) = (1u32, 0u32);
        while address & 0x80_0000 == 0 {
            let node = address & ADDRESS_MASK;
            let header = memory.read_u32(node);
            let count = header >> 24;
            if summary.words.saturating_add(count) > max_words {
                return Err(DmaError::WordLimit {
                    address: node,
                    summary,
                });
            }
            for k in 1..=count {
                self.write_gp0(memory.read_u32(node.wrapping_add(4 * k) & ADDRESS_MASK));
            }
            summary.nodes += 1;
            summary.words += count;
            address = header & 0xFF_FFFF;
            distance += 1;
            if address == saved {
                return Err(DmaError::Loop { address, summary });
            }
            if distance == power {
                saved = address;
                power = power.saturating_mul(2);
                distance = 0;
            }
        }
        Ok(summary)
    }

    /// Block mode, memory to GPU: sends `words` words from `address` to GP0.
    pub fn dma_block_write<M: Memory + ?Sized>(&mut self, memory: &M, address: u32, words: u32) {
        for k in 0..words {
            self.write_gp0(memory.read_u32(address.wrapping_add(4 * k) & ADDRESS_MASK));
        }
    }

    /// Block mode, GPU to memory: stores `words` GPUREAD words at `address`.
    pub fn dma_block_read<M: MemoryMut + ?Sized>(
        &mut self,
        memory: &mut M,
        address: u32,
        words: u32,
    ) {
        for k in 0..words {
            let word = self.read_gpuread();
            memory.write_u32(address.wrapping_add(4 * k) & ADDRESS_MASK, word);
        }
    }
}
