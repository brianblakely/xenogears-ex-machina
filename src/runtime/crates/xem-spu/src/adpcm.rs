//! SPU-ADPCM block decoding (psx-spx "SPU ADPCM Samples", with the nibble
//! arithmetic of "CDROM XA Audio ADPCM Compression").
//!
//! A block is 16 bytes: byte 0 holds the shift (bits 0-3) and the filter
//! (bits 4-6), byte 1 the loop flags, bytes 2-15 the 28 nibbles, low nibble
//! first.

/// Loop flag: the block ends the sample (set ENDX, continue at the repeat
/// address).
pub const FLAG_LOOP_END: u8 = 1;
/// Loop flag: with `FLAG_LOOP_END`, keep playing; without it, the voice is
/// released with its envelope at zero.
pub const FLAG_LOOP_REPEAT: u8 = 2;
/// Loop flag: the block's address becomes the repeat address when fetched.
pub const FLAG_LOOP_START: u8 = 4;

/// Samples per block.
pub const BLOCK_SAMPLES: usize = 28;

/// Positive (previous sample) filter weights in 1/64, filters 0-4.
const FILTER_POS: [i32; 5] = [0, 60, 115, 98, 122];
/// Negative (sample before that) filter weights in 1/64, filters 0-4.
const FILTER_NEG: [i32; 5] = [0, 0, -52, -55, -60];

/// The decoder history: the last two decoded samples, `[old, older]`.
pub type History = [i32; 2];

/// Decodes one 16-byte block into 28 samples, updating `history`.
///
/// Per sample: `s = (nibble << 12 >> shift) + ((old * f0 + older * f1 + 32) >> 6)`,
/// saturated to 16 bits. Choices the documentation leaves open:
/// - the filter term is an arithmetic shift (the "/64" of psx-spx rounds the
///   same way for non-negative sums only);
/// - shift values 13-15 decode as shift 9, and filter values 5-7 as filter 0
///   (both reserved; no shipped data is known to use them).
pub fn decode_block(block: &[u8; 16], history: &mut History, out: &mut [i16; BLOCK_SAMPLES]) {
    let mut shift = (block[0] & 0x0F) as u32;
    if shift > 12 {
        shift = 9;
    }
    let filter = ((block[0] >> 4) & 0x07) as usize;
    let (f0, f1) = if filter < 5 {
        (FILTER_POS[filter], FILTER_NEG[filter])
    } else {
        (0, 0)
    };
    let [mut old, mut older] = *history;
    for (j, sample) in out.iter_mut().enumerate() {
        let byte = block[2 + j / 2];
        let nibble = if j & 1 == 0 { byte & 0x0F } else { byte >> 4 };
        // Sign-extend the nibble into the top of a 16-bit value, then shift.
        let t = ((((nibble as u16) << 12) as i16) as i32) >> shift;
        let s = (t + ((old * f0 + older * f1 + 32) >> 6)).clamp(i16::MIN as i32, i16::MAX as i32);
        *sample = s as i16;
        older = old;
        old = s;
    }
    *history = [old, older];
}

#[cfg(test)]
mod tests {
    use super::*;

    fn block(shift: u8, filter: u8, nibbles: &[u8; 28]) -> [u8; 16] {
        let mut b = [0u8; 16];
        b[0] = (filter << 4) | shift;
        for (j, n) in nibbles.iter().enumerate() {
            b[2 + j / 2] |= (n & 0xF) << ((j & 1) * 4);
        }
        b
    }

    #[test]
    fn filter0_scales_nibbles_by_shift() {
        let mut nibbles = [0u8; 28];
        nibbles[0] = 7;
        nibbles[1] = 8; // -8
        nibbles[2] = 0xF; // -1
        nibbles[3] = 1;
        let mut h = [0, 0];
        let mut out = [0; 28];
        decode_block(&block(0, 0, &nibbles), &mut h, &mut out);
        assert_eq!(&out[..4], &[0x7000, -0x8000, -0x1000, 0x1000]);
        decode_block(&block(12, 0, &nibbles), &mut h, &mut out);
        assert_eq!(&out[..4], &[7, -8, -1, 1]);
        decode_block(&block(4, 0, &nibbles), &mut h, &mut out);
        assert_eq!(&out[..4], &[0x700, -0x800, -0x100, 0x100]);
    }

    #[test]
    fn each_filter_uses_its_weights() {
        // Zero nibbles leave only the prediction from the history.
        let zero = [0u8; 28];
        let expect = |f0: i32, f1: i32| {
            let (mut old, mut older) = (1000i32, -500i32);
            let mut v = Vec::new();
            for _ in 0..28 {
                let s = ((old * f0 + older * f1 + 32) >> 6).clamp(-32768, 32767);
                v.push(s as i16);
                older = old;
                old = s;
            }
            v
        };
        for (filter, (f0, f1)) in [
            (0, (0, 0)),
            (1, (60, 0)),
            (2, (115, -52)),
            (3, (98, -55)),
            (4, (122, -60)),
        ] {
            let mut h = [1000, -500];
            let mut out = [0; 28];
            decode_block(&block(12, filter, &zero), &mut h, &mut out);
            assert_eq!(out.to_vec(), expect(f0, f1), "filter {filter}");
            assert_eq!(h, [out[27] as i32, out[26] as i32]);
        }
        // Hand-computed first samples: filter 1 is 60/64 of the previous sample.
        let mut h = [1000, -500];
        let mut out = [0; 28];
        decode_block(&block(12, 1, &zero), &mut h, &mut out);
        assert_eq!(&out[..3], &[938, 879, 824]);
        // Filter 2: (1000 * 115 - 500 * -52 + 32) >> 6 = 2203.
        let mut h = [1000, -500];
        decode_block(&block(12, 2, &zero), &mut h, &mut out);
        assert_eq!(out[0], 2203);
    }

    #[test]
    fn prediction_saturates() {
        let mut nibbles = [0u8; 28];
        nibbles.fill(7);
        let mut h = [0, 0];
        let mut out = [0; 28];
        decode_block(&block(0, 4, &nibbles), &mut h, &mut out);
        assert!(out[1..].iter().all(|&s| s == i16::MAX));
    }

    #[test]
    fn reserved_shift_and_filter() {
        let mut nibbles = [0u8; 28];
        nibbles[0] = 1;
        let mut h = [0, 0];
        let mut out = [0; 28];
        decode_block(&block(13, 0, &nibbles), &mut h, &mut out);
        assert_eq!(out[0], 1 << 3);
        let mut h = [1000, 1000];
        decode_block(&block(12, 5, &[0; 28]), &mut h, &mut out);
        assert!(out.iter().all(|&s| s == 0));
    }
}
