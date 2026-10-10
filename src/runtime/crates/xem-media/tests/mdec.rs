//! MDEC device tests on synthetic run-level blocks.

use xem_media::mdec::{
    self, DEFAULT_QUANT_TABLE, DEFAULT_SCALE_TABLE, Depth, END_OF_BLOCK, Mdec, ZAGZIG, ZIGZAG,
    control, decode_command, idct, set_quant_command, status,
};

fn dc(q: u16, level: i16) -> u16 {
    q << 10 | (level as u16 & 0x3FF)
}

fn ac(run: u16, level: i16) -> u16 {
    run << 10 | (level as u16 & 0x3FF)
}

fn words(halfwords: &[u16]) -> Vec<u32> {
    halfwords
        .chunks(2)
        .map(|p| u32::from(p[0]) | u32::from(*p.get(1).unwrap_or(&END_OF_BLOCK)) << 16)
        .collect()
}

fn decode(depth: Depth, signed: bool, bit15: bool, halfwords: &[u16]) -> (Mdec, Vec<u32>) {
    let mut mdec = Mdec::new();
    let input = words(halfwords);
    mdec.command(decode_command(depth, signed, bit15, input.len() as u16));
    assert_eq!(mdec.push_input(&input), input.len());
    let mut out = vec![0; mdec.output_len()];
    mdec.pop_output(&mut out);
    (mdec, out)
}

fn bytes(words: &[u32]) -> Vec<u8> {
    words.iter().flat_map(|w| w.to_le_bytes()).collect()
}

/// The 2-D inverse DCT in floating point (JPEG's definition).
fn reference_idct(coefficients: &[f64; 64]) -> [f64; 64] {
    let c = |u: usize| {
        if u == 0 {
            std::f64::consts::FRAC_1_SQRT_2
        } else {
            1.0
        }
    };
    let mut out = [0.0; 64];
    for y in 0..8 {
        for x in 0..8 {
            let mut sum = 0.0;
            for v in 0..8 {
                for u in 0..8 {
                    let cx = ((2 * x + 1) as f64 * u as f64 * std::f64::consts::PI / 16.0).cos();
                    let cy = ((2 * y + 1) as f64 * v as f64 * std::f64::consts::PI / 16.0).cos();
                    sum += c(u) * c(v) * coefficients[u + v * 8] * cx * cy;
                }
            }
            out[x + y * 8] = sum / 4.0;
        }
    }
    out
}

#[test]
fn zigzag_tables_are_inverse() {
    for i in 0..64 {
        assert_eq!(ZAGZIG[ZIGZAG[i] as usize] as usize, i);
    }
    // Scan position 2 is the first coefficient of the second row.
    assert_eq!(ZAGZIG[2], 8);
}

#[test]
fn reset_status() {
    let mut mdec = Mdec::new();
    mdec.control(control::RESET);
    assert_eq!(mdec.status(), 0x8004_0000);
}

#[test]
fn dc_only_block_is_flat() {
    // DC 64 times quant[0] = 2: 128, an eighth of it through both IDCT passes.
    let (_, out) = decode(Depth::Bits8, false, false, &[dc(1, 64), END_OF_BLOCK]);
    assert_eq!(out.len(), 16);
    assert!(
        bytes(&out).iter().all(|&p| p == 128 + 16),
        "{:x?}",
        bytes(&out)
    );

    let (_, out) = decode(Depth::Bits8, true, false, &[dc(1, -64), END_OF_BLOCK]);
    let flat = bytes(&out)[0] as i8;
    assert!(bytes(&out).iter().all(|&p| p as i8 == flat));
    assert_eq!(flat, -16);
}

#[test]
fn quant_scale_zero_stores_without_zigzag() {
    // With q_scale 0 values are level*2 at the scan index itself.
    let mut block = [0i16; 64];
    block[0] = 100;
    block[2] = 40; // raster 2: horizontal frequency 2
    let (_, out) = decode(
        Depth::Bits8,
        true,
        false,
        &[dc(0, 50), ac(1, 20), END_OF_BLOCK],
    );
    let expected = idct(&block, &DEFAULT_SCALE_TABLE);
    for (i, &p) in bytes(&out).iter().enumerate() {
        assert_eq!(p as i8 as i16, expected[i].clamp(-128, 127), "pixel {i}");
    }
}

#[test]
fn single_ac_coefficients_match_reference_idct() {
    // Each scan position alone, at q_scale 8 so the value is level * quant[k].
    for k in 1..64usize {
        let quant = DEFAULT_QUANT_TABLE[k] as i32;
        let level = (200 / quant).max(1) as i16;
        let (_, out) = decode(
            Depth::Bits8,
            true,
            false,
            &[dc(8, 0), ac(k as u16 - 1, level), END_OF_BLOCK],
        );
        let mut coefficients = [0.0; 64];
        coefficients[ZAGZIG[k] as usize] = ((level as i32 * quant * 8 + 4) >> 3) as f64;
        let expected = reference_idct(&coefficients);
        for (i, &p) in bytes(&out).iter().enumerate() {
            let diff = (p as i8 as f64 - expected[i]).abs();
            assert!(
                diff <= 1.0,
                "scan {k} pixel {i}: {} vs {:.2}",
                p as i8,
                expected[i]
            );
        }
    }
}

#[test]
fn mixed_block_matches_reference_idct() {
    let halfwords = [
        dc(4, 30),
        ac(0, -25),
        ac(2, 17),
        ac(0, 9),
        ac(5, -12),
        ac(20, 6),
        END_OF_BLOCK,
    ];
    let (_, out) = decode(Depth::Bits8, true, false, &halfwords);
    let mut coefficients = [0.0; 64];
    coefficients[0] = 60.0;
    let mut k = 0usize;
    for &h in &halfwords[1..halfwords.len() - 1] {
        k += (h >> 10) as usize + 1;
        let level = ((h as i32) << 22) >> 22;
        coefficients[ZAGZIG[k] as usize] =
            ((level * DEFAULT_QUANT_TABLE[k] as i32 * 4 + 4) >> 3) as f64;
    }
    let expected = reference_idct(&coefficients);
    for (i, &p) in bytes(&out).iter().enumerate() {
        assert!(
            (p as i8 as f64 - expected[i].clamp(-128.0, 127.0)).abs() <= 1.0,
            "pixel {i}"
        );
    }
}

#[test]
fn coefficients_saturate_to_11_bits() {
    // 500 * 83 * 63 / 8 saturates to 3FFh like 1023 itself.
    let a = decode(
        Depth::Bits8,
        true,
        false,
        &[dc(63, 0), ac(62, 500), END_OF_BLOCK],
    )
    .1;
    let mut block = [0i16; 64];
    block[63] = 0x3FF;
    let expected = idct(&block, &DEFAULT_SCALE_TABLE);
    for (i, &p) in bytes(&a).iter().enumerate() {
        assert_eq!(p as i8 as i16, expected[i].clamp(-128, 127));
    }
}

#[test]
fn padding_between_blocks_is_ignored_and_full_blocks_need_no_end_code() {
    let plain = decode(
        Depth::Bits8,
        false,
        false,
        &[dc(1, 64), END_OF_BLOCK, dc(1, -64), END_OF_BLOCK],
    )
    .1;
    let padded = decode(
        Depth::Bits8,
        false,
        false,
        &[
            END_OF_BLOCK,
            END_OF_BLOCK,
            dc(1, 64),
            END_OF_BLOCK,
            END_OF_BLOCK,
            dc(1, -64),
            END_OF_BLOCK,
        ],
    )
    .1;
    assert_eq!(plain, padded);
    // A block whose last AC lands on coefficient 63 ends there.
    let full = decode(
        Depth::Bits8,
        false,
        false,
        &[dc(1, 64), ac(62, 0), dc(1, -64), END_OF_BLOCK],
    )
    .1;
    assert_eq!(full, plain);
}

#[test]
fn four_bit_output_packs_low_nibble_first() {
    let (_, out) = decode(Depth::Bits4, false, false, &[dc(1, 64), END_OF_BLOCK]);
    assert_eq!(out.len(), 8);
    // 144 = 90h: each pixel keeps its upper nibble, 9.
    assert!(out.iter().all(|&w| w == 0x9999_9999));
}

/// A colour macroblock with flat blocks: Cr, Cb, then Y1..Y4 DC levels.
fn flat_macroblock(levels: [i16; 6]) -> Vec<u16> {
    levels
        .iter()
        .flat_map(|&l| [dc(1, l), END_OF_BLOCK])
        .collect()
}

fn flat(level: i16) -> i32 {
    let mut block = [0i16; 64];
    block[0] = level * 2;
    idct(&block, &DEFAULT_SCALE_TABLE)[0] as i32
}

#[test]
fn color_conversion_samples() {
    for (cr, cb, y) in [
        (0, 0, 0),
        (100, 0, 40),
        (0, 100, -40),
        (-120, 60, 10),
        (200, -200, 0),
        (-200, 200, 100),
    ] {
        let (_, out) = decode(
            Depth::Bits24,
            false,
            false,
            &flat_macroblock([cr, cb, y, y, y, y]),
        );
        let (cr, cb, y) = (flat(cr) as f64, flat(cb) as f64, flat(y) as f64);
        let expected = [
            y + 1.402 * cr,
            y - 0.3437 * cb - 0.7143 * cr,
            y + 1.772 * cb,
        ]
        .map(|v| v.round().clamp(-128.0, 127.0) + 128.0);
        let pixels = bytes(&out);
        assert_eq!(pixels.len(), 768);
        for channel in 0..3 {
            let got = pixels[channel] as f64;
            assert!(
                (got - expected[channel]).abs() <= 1.0,
                "channel {channel}: {got} vs {}",
                expected[channel]
            );
        }
        assert!(pixels.chunks(3).all(|p| p == &pixels[..3]));
    }
}

#[test]
fn macroblock_packing_order_24_and_15_bit() {
    // Y1..Y4 differ; Cr pushes red above green so byte order shows.
    let levels = [80, 0, -200, -60, 60, 200];
    let (_, out24) = decode(Depth::Bits24, false, false, &flat_macroblock(levels));
    assert_eq!(out24.len(), 192);
    let px = bytes(&out24);
    let pixel = |x: usize, y: usize| {
        [
            px[(x + y * 16) * 3],
            px[(x + y * 16) * 3 + 1],
            px[(x + y * 16) * 3 + 2],
        ]
    };
    let quadrant = [pixel(0, 0), pixel(15, 0), pixel(0, 15), pixel(15, 15)];
    for (q, (x0, y0)) in [(0, 0), (8, 0), (0, 8), (8, 8)].into_iter().enumerate() {
        for y in y0..y0 + 8 {
            for x in x0..x0 + 8 {
                assert_eq!(pixel(x, y), quadrant[q], "pixel {x},{y}");
            }
        }
    }
    // Luminance increases Y1 < Y2 < Y3 < Y4; red (byte 0) exceeds green (byte 1).
    assert!(quadrant.windows(2).all(|w| w[0][1] < w[1][1]));
    assert!(quadrant.iter().all(|p| p[0] > p[1] && p[2] > p[1]));

    let (_, out15) = decode(Depth::Bits15, false, true, &flat_macroblock(levels));
    assert_eq!(out15.len(), 128);
    for (i, &word) in out15.iter().enumerate() {
        for half in 0..2 {
            let n = i * 2 + half;
            let [r, g, b] = pixel(n % 16, n / 16);
            let expected =
                (r >> 3) as u32 | ((g >> 3) as u32) << 5 | ((b >> 3) as u32) << 10 | 0x8000;
            assert_eq!(word >> (half * 16) & 0xFFFF, expected, "pixel {n}");
        }
    }
}

#[test]
fn status_through_a_decode() {
    let mut mdec = Mdec::new();
    mdec.control(control::RESET | control::ENABLE_DATA_IN | control::ENABLE_DATA_OUT);
    let input = words(&flat_macroblock([0, 0, 10, 20, 30, 40]));
    mdec.command(decode_command(
        Depth::Bits15,
        false,
        false,
        input.len() as u16,
    ));
    let s = mdec.status();
    assert_eq!(s & 0xFFFF, input.len() as u32 - 1);
    assert_eq!(s >> 16 & 7, 4, "Cr comes first");
    assert_eq!(
        s >> 23 & 0xF,
        0b1100,
        "15-bit depth, unsigned, bit 15 clear"
    );
    assert_ne!(s & status::BUSY, 0);
    assert_ne!(s & status::DATA_IN_REQUEST, 0);
    assert_ne!(s & status::OUT_EMPTY, 0);

    // Cr and Cb arrive: the next block is Y1.
    mdec.push_input(&input[..2]);
    assert_eq!(mdec.status() >> 16 & 7, 0);
    assert_eq!(mdec.status() & 0xFFFF, input.len() as u32 - 3);

    mdec.push_input(&input[2..]);
    let s = mdec.status();
    assert_eq!(s & 0xFFFF, 0xFFFF);
    assert_ne!(s & status::IN_FULL, 0);
    assert_eq!(s & status::DATA_IN_REQUEST, 0);
    assert_ne!(s & status::DATA_OUT_REQUEST, 0);
    assert_eq!(s & status::OUT_EMPTY, 0);
    assert_eq!(s >> 16 & 7, 0, "output starts with Y1");

    let mut buf = [0u32; 64];
    assert_eq!(mdec.pop_output(&mut buf), 64);
    assert_eq!(mdec.status() >> 16 & 7, 2, "half the macroblock read: Y3");
    assert_eq!(mdec.pop_output(&mut buf), 64);
    assert_eq!(mdec.pop_output(&mut buf), 0);
    let s = mdec.status();
    assert_eq!(s & (status::BUSY | status::DATA_OUT_REQUEST), 0);
    assert_ne!(s & status::OUT_EMPTY, 0);
}

#[test]
fn table_commands_load_tables() {
    let mut mdec = Mdec::new();
    let luma: Vec<u8> = (1..=64).collect();
    let chroma: Vec<u8> = (101..=164).collect();
    mdec.command(set_quant_command(true));
    assert_eq!(mdec.status() & 0xFFFF, 31);
    let table_words: Vec<u32> = luma
        .chunks(4)
        .chain(chroma.chunks(4))
        .map(|c| u32::from_le_bytes([c[0], c[1], c[2], c[3]]))
        .collect();
    assert_eq!(mdec.push_input(&table_words), 32);
    assert_eq!(&mdec.luma_quant_table()[..], &luma[..]);
    assert_eq!(&mdec.chroma_quant_table()[..], &chroma[..]);

    // Luminance only leaves the colour table.
    mdec.command(set_quant_command(false));
    mdec.push_input(&[0x0202_0202; 16]);
    assert_eq!(mdec.luma_quant_table(), &[2; 64]);
    assert_eq!(&mdec.chroma_quant_table()[..], &chroma[..]);

    let scale: Vec<i16> = (0..64).map(|i| i * 3 - 90).collect();
    mdec.command(mdec::SET_SCALE_COMMAND);
    for pair in scale.chunks(2) {
        // Parameters through the command port as well as DMA.
        mdec.command(pair[0] as u16 as u32 | (pair[1] as u16 as u32) << 16);
    }
    assert_eq!(&mdec.scale_table()[..], &scale[..]);
    assert_eq!(mdec.status() & 0xFFFF, 0xFFFF);

    // No-function commands reflect bits 0-15 and 25-28.
    mdec.command(0x1E00_1234);
    assert_eq!(mdec.status() & 0xFFFF, 0x1234);
    assert_eq!(mdec.status() >> 23 & 0xF, 0xF);
}

#[test]
fn snapshot_mid_decode_resumes_identically() {
    let mut halfwords = Vec::new();
    for i in 0..5i16 {
        halfwords.extend(flat_macroblock([i * 7, -i * 5, i * 20, -i * 20, 30, -30]));
    }
    let input = words(&halfwords);
    let mut a = Mdec::new();
    a.control(control::ENABLE_DATA_IN | control::ENABLE_DATA_OUT);
    a.command(decode_command(
        Depth::Bits24,
        false,
        false,
        input.len() as u16,
    ));
    a.push_input(&input[..13]);
    let saved = a.save();
    let mut b = Mdec::new();
    b.load(&saved).unwrap();
    assert_eq!(b.save(), saved);
    assert_eq!(a.status(), b.status());
    a.push_input(&input[13..]);
    b.push_input(&input[13..]);
    let mut out_a = vec![0; a.output_len()];
    let mut out_b = vec![0; b.output_len()];
    a.pop_output(&mut out_a);
    b.pop_output(&mut out_b);
    assert_eq!(out_a.len(), 5 * 192);
    assert_eq!(out_a, out_b);
    assert!(b.load(&saved[..saved.len() - 1]).is_err());
    assert!(b.load(b"XAAD\x01").is_err());
}

#[test]
fn decode_frame_lays_macroblocks_in_columns() {
    // 32x32: four macroblocks, the first column top to bottom, then the next.
    let mut halfwords = Vec::new();
    for y in [-150i16, -50, 50, 150] {
        halfwords.extend(flat_macroblock([0, 0, y, y, y, y]));
    }
    let mut mdec = Mdec::new();
    let image = mdec.decode_frame(&halfwords, 32, 32, Depth::Bits24);
    assert_eq!(image.len(), 32 * 32 * 3);
    let at = |x: usize, y: usize| image[(x + y * 32) * 3];
    let expected = [-150, -50, 50, 150].map(|l| (flat(l) + 128) as u8);
    assert_eq!([at(0, 0), at(0, 16), at(16, 0), at(16, 16)], expected);

    // Cropped to 24x24 (macroblocks still fill 32x32 positions).
    let cropped = mdec.decode_frame(&halfwords, 24, 24, Depth::Bits15);
    assert_eq!(cropped.len(), 24 * 24 * 2);
    let pixel = |x: usize, y: usize| {
        u16::from_le_bytes([cropped[(x + y * 24) * 2], cropped[(x + y * 24) * 2 + 1]])
    };
    assert_eq!(pixel(20, 20) & 0x1F, u16::from(expected[3] >> 3));
    assert_eq!(pixel(0, 20) & 0x1F, u16::from(expected[1] >> 3));

    // Monochrome frames use 8x8 macroblocks.
    let mono: Vec<u16> = [-100i16, 0, 100, 50]
        .iter()
        .flat_map(|&l| [dc(1, l), END_OF_BLOCK])
        .collect();
    let image = mdec.decode_frame(&mono, 16, 16, Depth::Bits8);
    assert_eq!(
        [image[0], image[8 * 16], image[8], image[8 * 16 + 8]],
        [-100, 0, 100, 50].map(|l| (flat(l) + 128) as u8)
    );
    let image = mdec.decode_frame(&mono, 16, 16, Depth::Bits4);
    assert_eq!(image.len(), 8 * 16);
    assert_eq!(image[8 * 8 + 4] >> 4, (flat(50) + 128) as u8 >> 4);
}
