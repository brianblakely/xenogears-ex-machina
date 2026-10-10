//! Raw CD-ROM XA (MODE2/2352) sectors: sync, header, subheader and payload.
//!
//! Layout: 12 sync bytes, a 4-byte header (BCD minute, second, frame, mode),
//! an 8-byte subheader (file, channel, submode, coding, then the same four
//! again), then 2048 user bytes (Form 1, followed by EDC/ECC) or 2324 (Form 2,
//! followed by an optional EDC). EDC and ECC are not checked.

/// Bytes in a raw sector.
pub const SECTOR_SIZE: usize = 2352;
/// Offset of the header (after the sync pattern).
pub const HEADER_OFFSET: usize = 12;
/// Offset of the subheader.
pub const SUBHEADER_OFFSET: usize = 16;
/// Offset of the user data of either form.
pub const USER_DATA_OFFSET: usize = 24;
/// User data bytes of a Form 1 sector.
pub const FORM1_DATA_SIZE: usize = 2048;
/// User data bytes of a Form 2 sector.
pub const FORM2_DATA_SIZE: usize = 2324;
/// Bytes from the subheader to the end of a sector: the unit movie sizes count in.
pub const MODE2_PAYLOAD_SIZE: usize = 2336;
/// Sectors before LBA 0 in the CD's absolute addressing (two seconds).
pub const LEAD_IN_SECTORS: u32 = 150;

const SYNC: [u8; 12] = [
    0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0,
];

/// The 4-byte header: the sector's absolute position and its mode.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Header {
    pub minute: u8,
    pub second: u8,
    pub frame: u8,
    pub mode: u8,
}

fn from_bcd(value: u8) -> Option<u32> {
    let (high, low) = (value >> 4, value & 0xF);
    (high < 10 && low < 10).then_some(u32::from(high) * 10 + u32::from(low))
}

fn to_bcd(value: u32) -> u8 {
    (((value / 10) << 4) | (value % 10)) as u8
}

impl Header {
    /// The header the drive writes for `lba` with the given mode.
    pub fn for_lba(lba: u32, mode: u8) -> Self {
        let absolute = lba + LEAD_IN_SECTORS;
        Self {
            minute: to_bcd(absolute / (60 * 75)),
            second: to_bcd(absolute / 75 % 60),
            frame: to_bcd(absolute % 75),
            mode,
        }
    }

    /// The LBA the position encodes (as `CdPosToInt`), or None for bad BCD.
    pub fn lba(&self) -> Option<u32> {
        let absolute =
            (from_bcd(self.minute)? * 60 + from_bcd(self.second)?) * 75 + from_bcd(self.frame)?;
        absolute.checked_sub(LEAD_IN_SECTORS)
    }

    pub fn to_bytes(self) -> [u8; 4] {
        [self.minute, self.second, self.frame, self.mode]
    }
}

/// Submode bits of the subheader.
pub mod submode {
    pub const END_OF_RECORD: u8 = 0x01;
    pub const VIDEO: u8 = 0x02;
    pub const AUDIO: u8 = 0x04;
    pub const DATA: u8 = 0x08;
    pub const TRIGGER: u8 = 0x10;
    pub const FORM2: u8 = 0x20;
    pub const REAL_TIME: u8 = 0x40;
    pub const END_OF_FILE: u8 = 0x80;
}

/// The first copy of the 8-byte subheader.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Subheader {
    pub file: u8,
    pub channel: u8,
    pub submode: u8,
    pub coding: u8,
}

impl Subheader {
    pub fn form(&self) -> Form {
        if self.submode & submode::FORM2 != 0 {
            Form::Form2
        } else {
            Form::Form1
        }
    }

    /// Both copies, as written on disc.
    pub fn to_bytes(self) -> [u8; 8] {
        let one = [self.file, self.channel, self.submode, self.coding];
        [
            one[0], one[1], one[2], one[3], one[0], one[1], one[2], one[3],
        ]
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Form {
    Form1,
    Form2,
}

impl Form {
    pub fn data_size(self) -> usize {
        match self {
            Form::Form1 => FORM1_DATA_SIZE,
            Form::Form2 => FORM2_DATA_SIZE,
        }
    }
}

/// A view of one raw sector.
#[derive(Clone, Copy)]
pub struct Sector<'a>(pub &'a [u8; SECTOR_SIZE]);

impl<'a> Sector<'a> {
    /// A view of the first `SECTOR_SIZE` bytes, if there are that many.
    pub fn new(bytes: &'a [u8]) -> Option<Self> {
        bytes
            .get(..SECTOR_SIZE)
            .map(|raw| Sector(raw.try_into().expect("sector length")))
    }

    pub fn has_sync(&self) -> bool {
        self.0[..HEADER_OFFSET] == SYNC
    }

    pub fn header(&self) -> Header {
        let h = &self.0[HEADER_OFFSET..SUBHEADER_OFFSET];
        Header {
            minute: h[0],
            second: h[1],
            frame: h[2],
            mode: h[3],
        }
    }

    /// The subheader of a mode 2 sector, None otherwise.
    pub fn subheader(&self) -> Option<Subheader> {
        if self.header().mode != 2 {
            return None;
        }
        let s = &self.0[SUBHEADER_OFFSET..USER_DATA_OFFSET];
        Some(Subheader {
            file: s[0],
            channel: s[1],
            submode: s[2],
            coding: s[3],
        })
    }

    /// The form of a mode 2 sector.
    pub fn form(&self) -> Option<Form> {
        self.subheader().map(|s| s.form())
    }

    /// The user data of a mode 2 sector: 2048 bytes in Form 1, 2324 in Form 2.
    pub fn user_data(&self) -> Option<&'a [u8]> {
        let size = self.form()?.data_size();
        Some(&self.0[USER_DATA_OFFSET..USER_DATA_OFFSET + size])
    }

    /// The 2048 bytes a Form 1 read delivers, whatever the subheader says (the
    /// game's file reads take this window of every sector).
    pub fn form1_window(&self) -> &'a [u8] {
        &self.0[USER_DATA_OFFSET..USER_DATA_OFFSET + FORM1_DATA_SIZE]
    }

    /// Subheader and the rest of the sector (2336 bytes): a movie record.
    pub fn mode2_payload(&self) -> &'a [u8] {
        &self.0[SUBHEADER_OFFSET..]
    }
}

/// Build a MODE2 sector with the given position, subheader and user data (EDC
/// and ECC left zero). For tests and synthetic images.
pub fn build_mode2_sector(lba: u32, subheader: Subheader, data: &[u8]) -> [u8; SECTOR_SIZE] {
    let size = subheader.form().data_size();
    assert!(data.len() <= size, "user data longer than the form allows");
    let mut out = [0u8; SECTOR_SIZE];
    out[..HEADER_OFFSET].copy_from_slice(&SYNC);
    out[HEADER_OFFSET..SUBHEADER_OFFSET].copy_from_slice(&Header::for_lba(lba, 2).to_bytes());
    out[SUBHEADER_OFFSET..USER_DATA_OFFSET].copy_from_slice(&subheader.to_bytes());
    out[USER_DATA_OFFSET..USER_DATA_OFFSET + data.len()].copy_from_slice(data);
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn header_round_trips_lba() {
        for lba in [0, 1, 16, 74, 75, 4499, 4500, 305_000] {
            let header = Header::for_lba(lba, 2);
            assert_eq!(header.lba(), Some(lba));
        }
        // LBA 16 is 00:02:16 absolute.
        assert_eq!(Header::for_lba(16, 2).to_bytes(), [0x00, 0x02, 0x16, 0x02]);
        assert_eq!(
            Header {
                minute: 0x0A,
                second: 0,
                frame: 0,
                mode: 2
            }
            .lba(),
            None
        );
    }

    #[test]
    fn forms_and_user_data() {
        let data1 = [0xAB; FORM1_DATA_SIZE];
        let raw1 = build_mode2_sector(
            20,
            Subheader {
                file: 0,
                channel: 0,
                submode: submode::DATA,
                coding: 0,
            },
            &data1,
        );
        let s1 = Sector(&raw1);
        assert!(s1.has_sync());
        assert_eq!(s1.header().lba(), Some(20));
        assert_eq!(s1.form(), Some(Form::Form1));
        assert_eq!(s1.user_data().unwrap(), &data1[..]);

        let data2 = [0xCD; FORM2_DATA_SIZE];
        let sub2 = Subheader {
            file: 1,
            channel: 3,
            submode: submode::FORM2 | submode::AUDIO | submode::REAL_TIME,
            coding: 1,
        };
        let raw2 = build_mode2_sector(21, sub2, &data2);
        let s2 = Sector(&raw2);
        assert_eq!(s2.subheader(), Some(sub2));
        assert_eq!(s2.form(), Some(Form::Form2));
        assert_eq!(s2.user_data().unwrap().len(), FORM2_DATA_SIZE);
        assert_eq!(s2.mode2_payload().len(), MODE2_PAYLOAD_SIZE);
        assert_eq!(&s2.mode2_payload()[..8], &sub2.to_bytes());

        let mut mode1 = raw1;
        mode1[15] = 1;
        assert_eq!(Sector(&mode1).subheader(), None);
        assert_eq!(Sector(&mode1).user_data(), None);
    }
}
