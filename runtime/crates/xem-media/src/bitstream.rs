//! A reference decoder for Sony's MDEC bitstream ("BS") frames, versions 2
//! and 3 (version 1 decodes as version 2), into the run-level halfwords the
//! MDEC takes, following psx-spx "CDROM File Video BS Compression". The
//! original game does this step in software (libpress `DecDCTvlc`, game-side
//! C); this decoder is for tests and for decoding movie frames as a reference.
//!
//! Frame layout: a header of four halfwords (run-level size in words, the
//! 3800h magic, the quantization scale, the version), then a bitstream read
//! as little-endian halfwords from bit 15 down. Each macroblock holds six
//! blocks (Cr, Cb, Y1..Y4); each block is a DC value, AC codes and an
//! end-of-block code.

use std::fmt;

use crate::mdec::END_OF_BLOCK;

/// The header's second halfword.
pub const BS_MAGIC: u16 = 0x3800;

/// Why a frame could not be decoded.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum BitstreamError {
    /// Shorter than the 8-byte header.
    MissingHeader,
    /// The header's second halfword is not 3800h.
    BadMagic(u16),
    /// A version this decoder does not handle (only 1, 2 and 3).
    UnsupportedVersion(u16),
    /// An unused code at the given bit offset.
    InvalidCode(usize),
    /// AC codes ran past the 64th coefficient at the given bit offset.
    BlockOverrun(usize),
}

impl fmt::Display for BitstreamError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::MissingHeader => write!(f, "frame is shorter than its header"),
            Self::BadMagic(m) => write!(f, "frame magic {m:#06x} is not 0x3800"),
            Self::UnsupportedVersion(v) => write!(f, "unsupported bitstream version {v}"),
            Self::InvalidCode(at) => write!(f, "unused code at bit {at}"),
            Self::BlockOverrun(at) => write!(f, "block runs past 64 coefficients at bit {at}"),
        }
    }
}

impl std::error::Error for BitstreamError {}

/// The four header halfwords.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct BsHeader {
    /// Run-level data size in 32-bit words (rounded up to 20h words).
    pub rle_words: u16,
    /// Quantization scale for every block (MDEC DC halfword bits 10-15).
    pub quant: u16,
    /// Bitstream version.
    pub version: u16,
}

impl BsHeader {
    /// Parse the 8-byte header.
    pub fn parse(frame: &[u8]) -> Result<Self, BitstreamError> {
        if frame.len() < 8 {
            return Err(BitstreamError::MissingHeader);
        }
        let half = |i: usize| u16::from_le_bytes([frame[i], frame[i + 1]]);
        if half(2) != BS_MAGIC {
            return Err(BitstreamError::BadMagic(half(2)));
        }
        Ok(Self {
            rle_words: half(0),
            quant: half(4),
            version: half(6),
        })
    }
}

/// A decoded frame: the run-level halfwords, one DC halfword, AC halfwords
/// and an end-of-block (FE00h) per block, six blocks per macroblock.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct RleFrame {
    pub header: BsHeader,
    pub halfwords: Vec<u16>,
    pub macroblocks: usize,
}

impl RleFrame {
    /// The frame in libpress's output layout: a word holding the data size
    /// in words and 3800h (the MDEC(1) command for 15-bit output), then the
    /// halfwords padded with FE00h to a multiple of 20h words, DMA0's block.
    pub fn mdec_words(&self) -> Vec<u32> {
        let mut halfwords = self.halfwords.clone();
        halfwords.resize(halfwords.len().next_multiple_of(64), END_OF_BLOCK);
        let mut words = Vec::with_capacity(1 + halfwords.len() / 2);
        words.push(u32::from(BS_MAGIC) << 16 | (halfwords.len() / 2) as u32);
        words.extend(
            halfwords
                .chunks_exact(2)
                .map(|p| u32::from(p[0]) | u32::from(p[1]) << 16),
        );
        words
    }
}

struct Bits<'a> {
    data: &'a [u8],
    /// Bit position in the halfword stream.
    pos: usize,
}

impl Bits<'_> {
    fn total(&self) -> usize {
        self.data.len() / 2 * 16
    }

    fn bit(&self, at: usize) -> u32 {
        if at >= self.total() {
            return 0;
        }
        let half = u16::from_le_bytes([self.data[at / 16 * 2], self.data[at / 16 * 2 + 1]]);
        u32::from(half >> (15 - at % 16) & 1)
    }

    /// The next `n` (at most 32) bits, zeros past the end.
    fn peek(&self, n: usize) -> u32 {
        (0..n).fold(0, |v, i| v << 1 | self.bit(self.pos + i))
    }

    fn read(&mut self, n: usize) -> u32 {
        let v = self.peek(n);
        self.pos += n;
        v
    }

    fn exhausted(&self) -> bool {
        self.pos >= self.total()
    }

    fn leading_zeros(&self, limit: usize) -> usize {
        (0..limit)
            .take_while(|&i| self.bit(self.pos + i) == 0)
            .count()
    }
}

/// AC values by the code's count of leading zeros: after the zeros and the
/// 1 come log2(len) index bits, then the sign bit. Empty groups are decoded
/// apart in `read_ac`.
const AC_GROUPS: [&[u16]; 12] = [
    &[],                               // "1x": EOB or 0001h, handled apart
    &[],                               // "01x": handled apart
    &[],                               // "001x": handled apart
    &[0x1C01, 0x1801, 0x0402, 0x1401], // 0001xxs
    &[0x0802, 0x2401, 0x0004, 0x2001], // 00001xxs
    &[],                               // 000001: escape
    &[
        0x4001, 0x1402, 0x0007, 0x0803, 0x0404, 0x3C01, 0x3801, 0x1002,
    ], // 0000001xxxs
    &[
        0x000B, 0x2002, 0x1003, 0x000A, 0x0804, 0x1C02, 0x5401, 0x5001, //
        0x0009, 0x4C01, 0x4801, 0x0405, 0x0C03, 0x0008, 0x1802, 0x4401,
    ], // 00000001xxxxs
    &[
        0x2802, 0x2402, 0x1403, 0x0C04, 0x0805, 0x0407, 0x0406, 0x000F, //
        0x000E, 0x000D, 0x000C, 0x6801, 0x6401, 0x6001, 0x5C01, 0x5801,
    ], // 000000001xxxxs
    &[
        0x001F, 0x001E, 0x001D, 0x001C, 0x001B, 0x001A, 0x0019, 0x0018, //
        0x0017, 0x0016, 0x0015, 0x0014, 0x0013, 0x0012, 0x0011, 0x0010,
    ], // 0000000001xxxxs
    &[
        0x0028, 0x0027, 0x0026, 0x0025, 0x0024, 0x0023, 0x0022, 0x0021, //
        0x0020, 0x040E, 0x040D, 0x040C, 0x040B, 0x040A, 0x0409, 0x0408,
    ], // 00000000001xxxxs
    &[
        0x0412, 0x0411, 0x0410, 0x040F, 0x1803, 0x4002, 0x3C02, 0x3802, //
        0x3402, 0x3002, 0x2C02, 0x7C01, 0x7801, 0x7401, 0x7001, 0x6C01,
    ], // 000000000001xxxxs
];

const AC_00100: [u16; 8] = [
    0x3401, 0x0006, 0x3001, 0x2C01, 0x0C02, 0x0403, 0x0005, 0x2801,
];

/// One AC code: `None` for end of block, else the MDEC halfword.
fn read_ac(bits: &mut Bits) -> Result<Option<u16>, BitstreamError> {
    let at = bits.pos;
    let zeros = bits.leading_zeros(12);
    // (value, index bits after the prefix) for prefixes with a sign bit.
    let (value, sign) = match zeros {
        0 => {
            if bits.read(2) == 0b10 {
                return Ok(None);
            }
            (0x0001, bits.read(1))
        }
        1 => {
            bits.read(2);
            if bits.read(1) == 1 {
                (0x0401, bits.read(1)) // 011s
            } else {
                ([0x0002, 0x0801][bits.read(1) as usize], bits.read(1)) // 010xs
            }
        }
        2 => {
            bits.read(3);
            if bits.read(1) == 1 {
                ([0x1001, 0x0C01][bits.read(1) as usize], bits.read(1)) // 0011xs
            } else if bits.read(1) == 1 {
                (0x0003, bits.read(1)) // 00101s
            } else {
                (AC_00100[bits.read(3) as usize], bits.read(1)) // 00100xxxs
            }
        }
        5 => {
            bits.read(6);
            return Ok(Some(bits.read(16) as u16)); // escape: raw halfword
        }
        12 => return Err(BitstreamError::InvalidCode(at)),
        _ => {
            bits.read(zeros + 1);
            let group = AC_GROUPS[zeros];
            let index_bits = group.len().trailing_zeros() as usize;
            (group[bits.read(index_bits) as usize], bits.read(1))
        }
    };
    Ok(Some(if sign == 1 {
        value & 0xFC00 | (value & 0x3FF).wrapping_neg() & 0x3FF
    } else {
        value
    }))
}

/// Version 3 DC size categories: (prefix, prefix length) for sizes 0..8.
const DC_LUMA: [(u32, usize); 9] = [
    (0b100, 3),
    (0b00, 2),
    (0b01, 2),
    (0b101, 3),
    (0b110, 3),
    (0b1110, 4),
    (0b11110, 5),
    (0b111110, 6),
    (0b1111110, 7),
];
const DC_CHROMA: [(u32, usize); 9] = [
    (0b00, 2),
    (0b01, 2),
    (0b10, 2),
    (0b110, 3),
    (0b1110, 4),
    (0b11110, 5),
    (0b111110, 6),
    (0b1111110, 7),
    (0b11111110, 8),
];

/// A version 3 DC difference (in units of 4).
fn read_dc_v3(bits: &mut Bits, chroma: bool) -> Result<i32, BitstreamError> {
    let table = if chroma { &DC_CHROMA } else { &DC_LUMA };
    for (size, &(prefix, length)) in table.iter().enumerate() {
        if bits.peek(length) == prefix {
            bits.read(length);
            if size == 0 {
                return Ok(0);
            }
            let v = bits.read(size) as i32;
            return Ok(if v >> (size - 1) == 1 {
                v
            } else {
                v - (1 << size) + 1
            });
        }
    }
    Err(BitstreamError::InvalidCode(bits.pos))
}

/// Decode one BS frame (header included). Decoding stops at the end-of-frame
/// code, at the end of the data, or after `max_macroblocks` macroblocks
/// (pass the picture's macroblock count when the frame may lack an end code).
pub fn decode(frame: &[u8], max_macroblocks: Option<usize>) -> Result<RleFrame, BitstreamError> {
    let header = BsHeader::parse(frame)?;
    let v3 = match header.version {
        1 | 2 => false,
        3 => true,
        v => return Err(BitstreamError::UnsupportedVersion(v)),
    };
    let quant = (header.quant & 0x3F) << 10;
    let mut bits = Bits {
        data: &frame[8..],
        pos: 0,
    };
    let mut halfwords = Vec::new();
    let mut macroblocks = 0;
    // Version 3 predictors: Cr, Cb, Y.
    let mut previous = [0i32; 3];
    while max_macroblocks.is_none_or(|max| macroblocks < max) && !bits.exhausted() {
        let end = if v3 {
            bits.peek(10) == 0x3FF
        } else {
            bits.peek(10) == 0x1FF
        };
        if end {
            break;
        }
        for block in 0..6 {
            let dc = if v3 {
                let predictor = block.min(2);
                previous[predictor] += read_dc_v3(&mut bits, block < 2)? * 4;
                previous[predictor]
            } else {
                bits.read(10) as i32
            };
            halfwords.push(quant | (dc & 0x3FF) as u16);
            let mut k = 0;
            loop {
                let at = bits.pos;
                match read_ac(&mut bits)? {
                    None => break,
                    Some(code) => {
                        k += usize::from(code >> 10) + 1;
                        if k > 63 {
                            return Err(BitstreamError::BlockOverrun(at));
                        }
                        halfwords.push(code);
                    }
                }
            }
            halfwords.push(END_OF_BLOCK);
        }
        macroblocks += 1;
    }
    Ok(RleFrame {
        header,
        halfwords,
        macroblocks,
    })
}

/// The 20h-byte header of a movie (STR) sector's user data (psx-spx
/// "CDROM File Video Streaming STR").
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct StrSectorHeader {
    pub status: u16,
    pub kind: u16,
    /// Sector number within the frame.
    pub sector: u16,
    /// Sectors in the frame.
    pub sectors: u16,
    pub frame: u32,
    /// Frame bitstream size in bytes.
    pub frame_size: u32,
    pub width: u16,
    pub height: u16,
}

impl StrSectorHeader {
    /// Bytes of frame data that follow the header in a 2048-byte sector.
    pub const DATA_BYTES: usize = 0x7E0;

    /// Parse a sector's user data; `None` unless it is an MDEC video sector
    /// (status 0160h, type 8001h).
    pub fn parse(user_data: &[u8]) -> Option<Self> {
        if user_data.len() < 0x20 {
            return None;
        }
        let half = |i: usize| u16::from_le_bytes([user_data[i], user_data[i + 1]]);
        let word = |i: usize| {
            u32::from_le_bytes([
                user_data[i],
                user_data[i + 1],
                user_data[i + 2],
                user_data[i + 3],
            ])
        };
        let header = Self {
            status: half(0),
            kind: half(2),
            sector: half(4),
            sectors: half(6),
            frame: word(8),
            frame_size: word(12),
            width: half(16),
            height: half(18),
        };
        (header.status == 0x0160 && header.kind == 0x8001).then_some(header)
    }
}
