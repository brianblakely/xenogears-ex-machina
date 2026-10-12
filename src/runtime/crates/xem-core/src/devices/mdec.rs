//! MDEC: xem-media's macroblock decoder behind libpress's register code
//! (src/port/libpress.c, src/port/include/xem/mdec.h), with its DMA channels.
//!
//! | Import | libpress | Effect |
//! | --- | --- | --- |
//! | `mdec_control(value)` | writes of 1F801824h | reset and DMA enables |
//! | `mdec_status()` | reads of 1F801824h | the status register |
//! | `mdec_in(addr, size)` | `MDEC_in` | the command word at `addr`, then (size / 32) blocks of 32 words by DMA0 |
//! | `mdec_out(addr, size)` | `MDEC_out` | (size / 32) blocks of 32 words by DMA1, ending with the DMA1 interrupt |
//! | `mdec_out_busy()` | DMA1's busy bit | whether the output transfer is still running |
//! | `mdec_stop_dma()` | clearing both DMA control registers | stop the transfers |
//!
//! The decoder works as soon as its input arrives (DMA0 ends at once), so
//! the output is ready when DMA1 asks for it; DMA1 takes the decoder's time:
//! [`MACROBLOCK_CYCLES`] per colour macroblock (a 16x16 one: about 9000 a
//! second, a 320x240 frame in a thirtieth of a second, an estimate), in
//! proportion for monochrome blocks. Its words land in game memory and the
//! DMA1 interrupt (`XEM_IRQ_DMA`, channel 1) is raised when it ends.

use xem_media::mdec::{Depth, Mdec as Decoder};

use super::{Context, Device, unknown};
use crate::clock::CPU_HZ;
use crate::module::Action;
use crate::snapshot::{Reader, Writer};

/// The DMA interrupt (src/port/include/xem/port.h) and the MDEC's channels.
pub const IRQ_DMA: u32 = 3;
pub const DMA_MDEC_IN: u32 = 0;
pub const DMA_MDEC_OUT: u32 = 1;

/// CPU cycles the MDEC takes per colour macroblock.
pub const MACROBLOCK_CYCLES: u64 = CPU_HZ / 9000;

/// A running DMA1 transfer.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Out {
    at: u64,
    address: u32,
    words: u32,
}

#[derive(Default)]
pub struct Mdec {
    decoder: Decoder,
    out: Option<Out>,
}

impl Mdec {
    pub fn decoder(&self) -> &Decoder {
        &self.decoder
    }

    /// Whether the decoder keeps the colour macroblocks it decodes, and
    /// those it kept since the last take (presentation only).
    pub fn keep_decoded(&mut self, keep: bool) {
        self.decoder.keep_decoded(keep);
    }

    pub fn take_decoded(&mut self) -> Vec<xem_media::mdec::Macroblock> {
        self.decoder.take_decoded()
    }

    /// Cycles the decoder takes for `words` output words at its depth.
    fn cycles(&self, words: u32) -> u64 {
        let depth = Depth::from_bits(self.decoder.status() >> 25);
        let per_block = depth.macroblock_words() as u64;
        let blocks = u64::from(words).div_ceil(per_block);
        let cost = if depth.is_color() { MACROBLOCK_CYCLES } else { MACROBLOCK_CYCLES / 6 };
        (blocks * cost).max(1)
    }

    fn run_out(&mut self, context: &mut Context<'_>) {
        let Some(out) = self.out.filter(|out| out.at <= context.now) else {
            return;
        };
        let mut words = vec![0u32; out.words as usize];
        let n = self.decoder.pop_output(&mut words);
        let bytes: Vec<u8> = words[..n].iter().flat_map(|w| w.to_le_bytes()).collect();
        if let Err(error) = context.memory.write(out.address, &bytes) {
            *context.trap_reason = Some(format!("mdec DMA1: {error}"));
        }
        if (n as u32) < out.words {
            // DMA1 waits for the rest of the decoder's output.
            let rest = out.words - n as u32;
            self.out = Some(Out { at: context.now + self.cycles(rest), address: out.address + 4 * n as u32, words: rest });
        } else {
            self.out = None;
            context.raised.push((IRQ_DMA, DMA_MDEC_OUT));
        }
    }
}

impl Device for Mdec {
    fn import(&mut self, op: &str, args: &[u32], context: &mut Context<'_>) -> Action {
        let arg = |i: usize| args.get(i).copied().unwrap_or(0);
        match op {
            "control" => {
                self.decoder.control(arg(0));
                Action::Return(0)
            }
            "status" => Action::Return(self.decoder.status()),
            "in" => {
                let words = (arg(1) >> 5) * 32;
                let mut bytes = vec![0u8; 4 + 4 * words as usize];
                if let Err(error) = context.memory.read(arg(0), &mut bytes) {
                    *context.trap_reason = Some(format!("mdec_in: {error}"));
                    return Action::Trap;
                }
                let mut data = bytes.chunks_exact(4).map(|c| u32::from_le_bytes(c.try_into().expect("word")));
                self.decoder.command(data.next().expect("command word"));
                let rest: Vec<u32> = data.collect();
                self.decoder.push_input(&rest);
                context.raised.push((IRQ_DMA, DMA_MDEC_IN));
                Action::Return(0)
            }
            "out" => {
                let words = (arg(1) >> 5) * 32;
                self.out = Some(Out { at: context.now + self.cycles(words), address: arg(0), words });
                Action::Return(0)
            }
            "out_busy" => Action::Return(u32::from(self.out.is_some())),
            "stop_dma" => {
                self.out = None;
                Action::Return(0)
            }
            _ => unknown("mdec", op, context),
        }
    }

    fn next_event(&self) -> Option<u64> {
        self.out.map(|out| out.at)
    }

    fn run_events(&mut self, context: &mut Context<'_>) {
        self.run_out(context);
    }

    fn save(&self) -> Vec<u8> {
        let mut w = Writer::default();
        w.bytes(&self.decoder.save());
        match self.out {
            Some(out) => {
                w.u32(1);
                w.u64(out.at);
                w.u32(out.address);
                w.u32(out.words);
            }
            None => w.u32(0),
        }
        w.0
    }

    fn load(&mut self, state: &[u8]) -> Result<(), String> {
        let e = |error: crate::snapshot::SnapshotError| error.to_string();
        let mut r = Reader::new(state);
        self.decoder.load(r.bytes().map_err(e)?).map_err(|error| error.to_string())?;
        self.out = match r.u32().map_err(e)? {
            0 => None,
            1 => Some(Out { at: r.u64().map_err(e)?, address: r.u32().map_err(e)?, words: r.u32().map_err(e)? }),
            _ => return Err("bad mdec output".into()),
        };
        if !r.done() { return Err("trailing mdec bytes".into()); }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory::SliceMemory;
    use xem_media::mdec::{Depth, decode_command};

    fn context<'a>(memory: &'a mut SliceMemory<'_>, now: u64, raised: &'a mut Vec<(u32, u32)>, reason: &'a mut Option<String>) -> Context<'a> {
        Context { memory, now, raised, trap_reason: reason }
    }

    /// One flat grey 15-bit macroblock in, its 128 words out after the decoder's time, then the DMA1 interrupt.
    #[test]
    fn a_macroblock_goes_in_and_comes_out_with_the_dma1_interrupt() {
        let mut ram = vec![0u8; 0x4000];
        // Six blocks: a DC of 0 with quantization 1, then end of block; padded to 32 words.
        let mut halfwords = Vec::new();
        for _ in 0..6 {
            halfwords.extend_from_slice(&[0x0400u16, 0xFE00]);
        }
        halfwords.resize(64, 0xFE00);
        let command = decode_command(Depth::Bits15, false, false, 32);
        let mut bytes = command.to_le_bytes().to_vec();
        bytes.extend(halfwords.iter().flat_map(|h| h.to_le_bytes()));
        ram[0x100..0x100 + bytes.len()].copy_from_slice(&bytes);
        let mut memory = SliceMemory(&mut ram);
        let (mut raised, mut reason) = (Vec::new(), None);
        let mut mdec = Mdec::default();
        let mut ctx = context(&mut memory, 1000, &mut raised, &mut reason);
        mdec.import("control", &[0x6000_0000], &mut ctx);
        mdec.import("in", &[0x100, 32], &mut ctx);
        mdec.import("out", &[0x1000, 128], &mut ctx);
        assert_eq!(mdec.import("out_busy", &[], &mut ctx), Action::Return(1));
        assert_eq!(mdec.next_event(), Some(1000 + MACROBLOCK_CYCLES));
        drop(ctx);
        assert_eq!(raised, vec![(IRQ_DMA, DMA_MDEC_IN)]);
        raised.clear();
        let mut ctx = context(&mut memory, 1000 + MACROBLOCK_CYCLES, &mut raised, &mut reason);
        mdec.run_events(&mut ctx);
        assert_eq!(mdec.import("out_busy", &[], &mut ctx), Action::Return(0));
        drop(ctx);
        assert_eq!(raised, vec![(IRQ_DMA, DMA_MDEC_OUT)]);
        // A zero DC decodes to mid grey (0x10 per channel in 15 bits).
        let pixel = u16::from_le_bytes([ram[0x1000], ram[0x1001]]);
        assert_eq!(pixel & 0x7FFF, 0x10 | 0x10 << 5 | 0x10 << 10);
        let state = mdec.save();
        let mut copy = Mdec::default();
        copy.load(&state).unwrap();
        assert_eq!(copy.save(), state);
    }
}
