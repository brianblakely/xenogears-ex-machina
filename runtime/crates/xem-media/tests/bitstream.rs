//! BS v2/v3 bitstream decoding of hand-built frames.

use xem_media::bitstream::{self, BitstreamError, StrSectorHeader};
use xem_media::mdec::{Depth, END_OF_BLOCK, Mdec};

/// Builds a frame: the header, then bits packed into little-endian halfwords
/// from bit 15 down.
struct Frame {
    header: [u16; 4],
    bits: Vec<bool>,
}

impl Frame {
    fn new(quant: u16, version: u16) -> Self {
        Self {
            header: [0, 0x3800, quant, version],
            bits: Vec::new(),
        }
    }

    fn put(&mut self, code: &str) -> &mut Self {
        self.bits.extend(
            code.chars()
                .filter(|c| !c.is_whitespace())
                .map(|c| c == '1'),
        );
        self
    }

    fn put_value(&mut self, value: u32, n: usize) -> &mut Self {
        self.bits.extend((0..n).rev().map(|i| value >> i & 1 == 1));
        self
    }

    fn bytes(&self) -> Vec<u8> {
        let mut out: Vec<u8> = self.header.iter().flat_map(|h| h.to_le_bytes()).collect();
        for chunk in self.bits.chunks(16) {
            let half = chunk
                .iter()
                .enumerate()
                .fold(0u16, |h, (i, &b)| h | u16::from(b) << (15 - i));
            out.extend(half.to_le_bytes());
        }
        out
    }
}

#[test]
fn v2_frame_with_every_kind_of_code() {
    let mut f = Frame::new(2, 2);
    // Cr: DC 123h, then AC codes and an end of block.
    f.put_value(0x123, 10)
        .put("110") // 0001h
        .put("111") // -1
        .put("0111") // 0401h negated
        .put("01010") // 010xs, x=1: 0801h
        .put("000001")
        .put_value(0x1C05, 16) // escape, raw halfword
        .put("00100 101 0") // 00100xxxs, x=5: 0403h
        .put("0000000001 0011 1") // 001Ch negated
        .put("0001 10 0") // 0001xxs, x=2: 0402h
        .put("10");
    // Cb, Y1..Y4: negative and positive DCs, no AC.
    for dc in [0x3FFu32, 0x200, 0x001, 0x1FE, 0x0] {
        f.put_value(dc, 10).put("10");
    }
    f.put_value(0x1FF, 10); // end of frame where Cr would be
    let frame = bitstream::decode(&f.bytes(), None).unwrap();
    assert_eq!(frame.macroblocks, 1);
    assert_eq!(frame.header.version, 2);
    let q = 2 << 10;
    let mut expected = vec![
        q | 0x123,
        0x0001,
        0x03FF,
        0x07FF,
        0x0801,
        0x1C05,
        0x0403,
        0x03E4,
        0x0402,
        END_OF_BLOCK,
    ];
    for dc in [0x3FF, 0x200, 0x001, 0x1FE, 0x0] {
        expected.extend([q | dc, END_OF_BLOCK]);
    }
    assert_eq!(frame.halfwords, expected);

    let words = frame.mdec_words();
    assert_eq!(words.len(), 1 + 32);
    assert_eq!(words[0], 0x3800_0020);
    assert_eq!(
        words[1],
        u32::from(expected[0]) | u32::from(expected[1]) << 16
    );
    assert_eq!(*words.last().unwrap(), 0xFE00_FE00);
}

#[test]
fn v2_without_end_code_stops_at_the_macroblock_limit() {
    let mut f = Frame::new(1, 2);
    for _ in 0..3 {
        for _ in 0..6 {
            f.put_value(0x10, 10).put("10");
        }
    }
    let bytes = f.bytes();
    assert_eq!(bitstream::decode(&bytes, Some(2)).unwrap().macroblocks, 2);
    // The data runs out after three; zero padding to the halfword would
    // read as a DC of zero followed by an unused code.
    let all = bitstream::decode(&bytes, Some(3)).unwrap();
    assert_eq!(all.halfwords.len(), 3 * 6 * 2);
}

#[test]
fn v3_dc_differences() {
    let mut f = Frame::new(1, 3);
    // Macroblock 1: Cr +3, Cb 0, Y1 +2, Y2 -1, Y3 0, Y4 -4 (all times 4).
    f.put("10 11").put("10");
    f.put("00").put("10");
    f.put("01 10").put("10");
    f.put("00 0").put("10");
    f.put("100").put("10");
    f.put("101 011").put("10");
    // Macroblock 2: Cr +1, Cb -7, Y1 -255 (wraps below -200h), Y2..Y4 0.
    f.put("01 1").put("10");
    f.put("110 000").put("10");
    f.put("1111110 00000000").put("10");
    for _ in 0..3 {
        f.put("100").put("10");
    }
    f.put("1111111111");
    let frame = bitstream::decode(&f.bytes(), None).unwrap();
    assert_eq!(frame.macroblocks, 2);
    let dcs: Vec<i32> = frame
        .halfwords
        .iter()
        .step_by(2)
        .map(|&h| ((h as i32) << 22) >> 22)
        .collect();
    assert!(
        frame
            .halfwords
            .iter()
            .skip(1)
            .step_by(2)
            .all(|&h| h == END_OF_BLOCK)
    );
    assert!(frame.halfwords.iter().step_by(2).all(|&h| h >> 10 == 1));
    let y = [8, 4, 4, -12];
    let y2 = -12 - 1020 + 2048; // 10-bit wrap of -1032: 1016, read back as -8
    assert_eq!(
        dcs,
        vec![
            12,
            0,
            y[0],
            y[1],
            y[2],
            y[3],
            16,
            -28,
            ((y2 << 22) >> 22),
            ((y2 << 22) >> 22),
            ((y2 << 22) >> 22),
            ((y2 << 22) >> 22)
        ]
    );
}

#[test]
fn errors() {
    assert_eq!(
        bitstream::decode(&[0; 6], None),
        Err(BitstreamError::MissingHeader)
    );
    let mut f = Frame::new(1, 2);
    f.header[1] = 0x3000;
    assert_eq!(
        bitstream::decode(&f.bytes(), None),
        Err(BitstreamError::BadMagic(0x3000))
    );
    let f = Frame::new(1, 4);
    assert_eq!(
        bitstream::decode(&f.bytes(), None),
        Err(BitstreamError::UnsupportedVersion(4))
    );
    let mut f = Frame::new(1, 2);
    f.put_value(0, 10).put("000000000000 1111");
    assert_eq!(
        bitstream::decode(&f.bytes(), None),
        Err(BitstreamError::InvalidCode(10))
    );
    let mut f = Frame::new(1, 2);
    f.put_value(0, 10)
        .put("000001")
        .put_value(0xF800, 16)
        .put("000001")
        .put_value(0x0801, 16);
    assert_eq!(
        bitstream::decode(&f.bytes(), None),
        Err(BitstreamError::BlockOverrun(32))
    );
}

#[test]
fn decoded_frame_drives_the_mdec() {
    // A grey 16x16 frame: chroma 0, every luminance block DC 40h.
    let mut f = Frame::new(1, 2);
    for dc in [0u32, 0, 0x40, 0x40, 0x40, 0x40] {
        f.put_value(dc, 10).put("10");
    }
    f.put_value(0x1FF, 10);
    let frame = bitstream::decode(&f.bytes(), None).unwrap();
    let words = frame.mdec_words();
    let halfwords: Vec<u16> = words[1..]
        .iter()
        .flat_map(|w| [*w as u16, (w >> 16) as u16])
        .collect();
    let mut mdec = Mdec::new();
    let image = mdec.decode_frame(&halfwords, 16, 16, Depth::Bits24);
    assert!(image.iter().all(|&b| b == 128 + 16), "{:?}", &image[..6]);
}

#[test]
fn str_sector_header() {
    let mut data = [0u8; 0x20];
    data[..4].copy_from_slice(&[0x60, 0x01, 0x01, 0x80]);
    data[4..8].copy_from_slice(&[2, 0, 9, 0]);
    data[8..12].copy_from_slice(&7u32.to_le_bytes());
    data[12..16].copy_from_slice(&0x3520u32.to_le_bytes());
    data[16..20].copy_from_slice(&[0x40, 0x01, 0xE0, 0x00]);
    let header = StrSectorHeader::parse(&data).unwrap();
    assert_eq!(
        (
            header.sector,
            header.sectors,
            header.frame,
            header.frame_size
        ),
        (2, 9, 7, 0x3520)
    );
    assert_eq!((header.width, header.height), (320, 224));
    data[2] = 0;
    assert_eq!(StrSectorHeader::parse(&data), None);
}
