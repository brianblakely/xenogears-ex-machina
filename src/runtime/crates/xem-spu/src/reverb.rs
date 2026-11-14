//! The reverb unit (psx-spx "SPU Reverb Registers", "SPU Reverb Formula",
//! "Reverb Buffer Resampling").
//!
//! The unit runs at 22.05 kHz: on alternate 44.1 kHz cycles it computes the
//! left half of the formula, then the right half, then advances the buffer
//! address. Its input is downsampled with the documented 39-tap FIR filter
//! (gain 0x7FFE/0x8000) and its output upsampled with the same filter over
//! the zero-stuffed 44.1 kHz sequence (gain doubled, `>> 14`).
//!
//! Choices the documentation leaves open:
//! - every product is shifted right by 15 on its own, and each sum is
//!   saturated to 16 bits before its next multiplication;
//! - with reverb master enable clear (SPUCNT bit 7) the unit still reads and
//!   outputs but writes nothing (psx-spx "Reverb Disable"; skipping the
//!   writes reproduces its description of the APF/COMB reads);
//! - only buffer writes are IRQ accesses (psx-spx does not know which reverb
//!   accesses trigger the IRQ);
//! - vIIR = -0x8000 negates the value written by the SAME/DIFF stages (the
//!   documented "Bug"); no other volume -0x8000 effect is modelled.

use crate::IrqProbe;

/// The 39 resampling coefficients, symmetric around 0x4000.
pub const FIR: [i32; 39] = [
    -0x0001, 0x0000, 0x0002, 0x0000, -0x000A, 0x0000, 0x0023, 0x0000, //
    -0x0067, 0x0000, 0x010A, 0x0000, -0x0268, 0x0000, 0x0534, 0x0000, //
    -0x0B90, 0x0000, 0x2806, 0x4000, 0x2806, 0x0000, -0x0B90, 0x0000, //
    0x0534, 0x0000, -0x0268, 0x0000, 0x010A, 0x0000, -0x0067, 0x0000, //
    0x0023, 0x0000, -0x000A, 0x0000, 0x0002, 0x0000, -0x0001,
];

const RING: usize = 64;

#[inline]
fn sat(v: i32) -> i32 {
    v.clamp(i16::MIN as i32, i16::MAX as i32)
}

#[inline]
fn mul(a: i32, b: i32) -> i32 {
    (a * b) >> 15
}

/// Reverb unit state.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Reverb {
    /// Current buffer address in bytes (mBASE * 8 ..= 0x7FFFE).
    pub cur: u32,
    /// Ring position of the newest entry.
    pub pos: u8,
    /// The next 22.05 kHz half computed is the right one.
    pub right_next: bool,
    /// 44.1 kHz input history per side.
    pub input: [[i16; RING]; 2],
    /// Zero-stuffed 22.05 kHz output history per side.
    pub output: [[i16; RING]; 2],
}

impl Default for Reverb {
    fn default() -> Self {
        Reverb {
            cur: 0,
            pos: 0,
            right_next: false,
            input: [[0; RING]; 2],
            output: [[0; RING]; 2],
        }
    }
}

/// The registers the unit reads in one cycle.
pub struct Regs<'a> {
    /// 0x1C0-0x1FF: dAPF1 ... vRIN.
    pub rev: &'a [u16; 32],
    /// vLOUT, vROUT.
    pub out_volume: [i16; 2],
    /// mBASE, in 8-byte units.
    pub base: u16,
    /// SPUCNT bit 7.
    pub write_enable: bool,
}

/// The filter's coefficients that are not zero: every second one (the
/// first of `EVEN` is the newest sample's) and the centre's.
const EVEN: [i32; 20] = {
    let mut taps = [0; 20];
    let mut j = 0;
    while j < 20 {
        taps[j] = FIR[2 * j];
        j += 1;
    }
    taps
};
const CENTRE: usize = 19;

/// The filter over the ring's newest 39 entries (`pos` the newest). The sum
/// is far from a 32-bit word's ends whatever the samples.
fn fir(ring: &[i16; RING], pos: usize) -> i32 {
    let at = |k: usize| ring[(pos + RING - k) % RING] as i32;
    let mut acc = FIR[CENTRE] * at(CENTRE);
    for (j, tap) in EVEN.iter().enumerate() {
        acc += tap * at(2 * j);
    }
    acc
}

impl Reverb {
    /// Sets the buffer address, as writing mBASE does.
    pub fn set_base(&mut self, base: u16) {
        self.cur = base as u32 * 8;
    }

    /// One 44.1 kHz cycle: takes the summed reverb input, returns the
    /// reverb output (after vLOUT/vROUT).
    pub fn tick(
        &mut self,
        ram: &mut [u8],
        regs: &Regs,
        input: [i16; 2],
        probe: &mut IrqProbe,
    ) -> [i16; 2] {
        let pos = (self.pos as usize + 1) % RING;
        self.pos = pos as u8;
        self.input[0][pos] = input[0];
        self.input[1][pos] = input[1];

        let side = self.right_next as usize;
        let x = sat(fir(&self.input[side], pos) >> 15);
        let y = self.compute(ram, regs, side, x, probe);
        self.output[side][pos] = y as i16;
        self.output[side ^ 1][pos] = 0;
        if self.right_next {
            let base = regs.base as u32 * 8;
            self.cur = ((self.cur + 2) & 0x7FFFE).max(base);
        }
        self.right_next.toggle();

        [
            sat(fir(&self.output[0], pos) >> 14) as i16,
            sat(fir(&self.output[1], pos) >> 14) as i16,
        ]
    }

    /// The buffer address `rel` bytes from the current one, wrapped within
    /// the buffer (from mBASE to the end of RAM).
    #[inline]
    fn address(&self, base: u16, rel: i32) -> usize {
        let base = base as i64 * 8;
        let size = 0x80000 - base;
        let offset = self.cur as i64 - base + rel as i64;
        // Most offsets are within a buffer's length of it: the remainder
        // without its division.
        let wrapped = if (0..size).contains(&offset) {
            offset
        } else if (-size..0).contains(&offset) {
            offset + size
        } else if (size..2 * size).contains(&offset) {
            offset - size
        } else {
            offset.rem_euclid(size)
        };
        (base + wrapped) as usize
    }

    fn compute(
        &self,
        ram: &mut [u8],
        regs: &Regs,
        side: usize,
        input: i32,
        probe: &mut IrqProbe,
    ) -> i32 {
        let rev = regs.rev;
        let reg = |i: usize| rev[i] as i32 * 8;
        let vol = |i: usize| rev[i] as i16 as i32;
        let read = |ram: &[u8], rel: i32| {
            let a = self.address(regs.base, rel);
            i16::from_le_bytes([ram[a], ram[a + 1]]) as i32
        };
        let write = |ram: &mut [u8], rel: i32, v: i32, probe: &mut IrqProbe| {
            if regs.write_enable {
                let a = self.address(regs.base, rel);
                ram[a..a + 2].copy_from_slice(&(v as i16).to_le_bytes());
                probe.access(a as u32, 2);
            }
        };

        let (d_apf1, d_apf2) = (reg(0x00), reg(0x01));
        let (v_iir, v_wall, v_apf1, v_apf2) = (vol(0x02), vol(0x07), vol(0x08), vol(0x09));
        let v_comb = [vol(0x03), vol(0x04), vol(0x05), vol(0x06)];
        let s = side;
        let m_same = reg(0x0A + s);
        let m_comb = [reg(0x0C + s), reg(0x0E + s), reg(0x14 + s), reg(0x16 + s)];
        let d_same = reg(0x10 + s);
        let m_diff = reg(0x12 + s);
        // L-to-L reads dLSAME, R-to-L reads dRDIFF (and the mirror image).
        let d_diff = reg(0x18 + (s ^ 1));
        let m_apf1 = reg(0x1A + s);
        let m_apf2 = reg(0x1C + s);
        let v_in = vol(0x1E + s);

        let lin = mul(v_in, input);
        if regs.write_enable {
            for (m, d) in [(m_same, d_same), (m_diff, d_diff)] {
                let prev = read(ram, m - 2);
                let t = sat(lin + mul(read(ram, d), v_wall) - prev);
                let mut v = sat(mul(t, v_iir) + prev);
                if v_iir == -0x8000 {
                    v = sat(-v);
                }
                write(ram, m, v, probe);
            }
        }

        let mut out = 0;
        for (m, v) in m_comb.iter().zip(v_comb) {
            out += mul(v, read(ram, *m));
        }
        let mut out = sat(out);
        for (m, d, v) in [(m_apf1, d_apf1, v_apf1), (m_apf2, d_apf2, v_apf2)] {
            let delayed = read(ram, m - d);
            let t = sat(out - mul(v, delayed));
            write(ram, m, t, probe);
            out = sat(mul(t, v) + delayed);
        }
        sat(mul(out, regs.out_volume[side] as i32))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fir_gains() {
        let sum: i32 = FIR.iter().sum();
        assert_eq!(sum, 0x7FFE);
        let odd: i32 = FIR.iter().skip(1).step_by(2).sum();
        assert_eq!(odd, 0x4000, "the zero-stuffed phase with the centre tap");
        assert_eq!(sum - odd, 0x3FFE);
        for i in 0..39 {
            assert_eq!(FIR[i], FIR[38 - i]);
        }
        // What `fir` sums is every coefficient that is not zero.
        for (k, &c) in FIR.iter().enumerate() {
            assert_eq!(c, if k % 2 == 0 { EVEN[k / 2] } else if k == CENTRE { 0x4000 } else { 0 }, "coefficient {k}");
        }
    }

    #[test]
    fn a_buffer_address_wraps_as_the_remainder_does() {
        let mut reverb = Reverb::default();
        for base in [0u16, 0x8000, 0xE000, 0xFFF0, 0xFFFE, 0xFFFF] {
            let (start, size) = (base as i64 * 8, 0x80000 - base as i64 * 8);
            for cur in [0u32, 8, base as u32 * 8, 0x7FFFE, (base as u32 * 8 + 0x1234) & 0x7FFFE] {
                reverb.cur = cur;
                for rel in [0, 2, -2, 8, -8, 0x1234, -0x1234, 0x7FFF8, -0x7FFF8, 0x7FFFF * 8, -(0x7FFFF * 8), size as i32, -(size as i32), 2 * size as i32 - 2, -2 * size as i32] {
                    let expected = start + (cur as i64 - start + rel as i64).rem_euclid(size);
                    assert_eq!(reverb.address(base, rel) as i64, expected, "mBASE {base:#x}, at {cur:#x}, {rel:+#x}");
                }
            }
        }
    }
}
