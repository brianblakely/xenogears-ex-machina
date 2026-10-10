//! The game's own file index: what `cd_select_directory`, `cd_get_file_sector`
//! and `cd_get_file_size` (cd_reads_and_streams.c) address files by.
//!
//! The index is 0x8000 bytes of 7-byte records, a 24-bit first sector then a
//! signed 32-bit size in bytes (a negative size heads a directory and counts
//! its files). The directory table holds u16 1-based first records; entry 0x3C
//! is the disc number. Each boot program embeds both (resident 0x80010004 and
//! 0x80018004, the only bytes in which SLUS_006.64 and SLUS_006.69 differ) and
//! the disc carries them at sectors 24 and 40. Selecting directory `d` (the
//! game's group + index) and reading file `f` uses record `f + table[d] - 2`,
//! the rule of tools/analysis/disc_index.py.

use crate::read_data;
use crate::sector::FORM1_DATA_SIZE;
use crate::source::{Result, SectorSource, format_error};

/// Sector of the index on disc.
pub const INDEX_LBA: u32 = 24;
/// Bytes of index the game keeps (and reads from sector 24 with boot word 0).
pub const INDEX_BYTES: usize = 0x8000;
/// Sector of the directory table on disc.
pub const DIRECTORY_LBA: u32 = 40;
/// Bytes of directory table the game reads from sector 40.
pub const DIRECTORY_BYTES: usize = 0x7A;
pub const RECORD_SIZE: usize = 7;
/// Directory table entry holding the disc number.
pub const DISC_NUMBER_ENTRY: usize = 0x3C;

/// Boot program layout: a 0x800-byte PS-X EXE header, then text loaded at
/// 0x80010000 starting with the boot word, the index and the directory table.
const EXE_HEADER: usize = 0x800;
const EXE_TEXT_ADDRESS: u32 = 0x8001_0000;
const EMBEDDED_INDEX: usize = EXE_HEADER + 4;
const EMBEDDED_DIRECTORIES: usize = EMBEDDED_INDEX + INDEX_BYTES;
/// The embedded table runs to 0x80018080 (0x7C bytes, one more entry than the
/// game reads from disc).
const EMBEDDED_DIRECTORY_BYTES: usize = 0x7C;

/// One 7-byte index record.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Record {
    /// First sector (24 bits).
    pub lba: u32,
    /// Size in bytes, or minus the file count of a directory.
    pub size: i32,
}

impl Record {
    fn parse(bytes: &[u8]) -> Self {
        let lba = u32::from(bytes[0]) | u32::from(bytes[1]) << 8 | u32::from(bytes[2]) << 16;
        Self {
            lba,
            size: i32::from_le_bytes(bytes[3..7].try_into().expect("4 bytes")),
        }
    }

    /// An unused record (0xFFFFFF start, or zero start and size), as the
    /// extractor skips them.
    pub fn is_unused(&self) -> bool {
        self.lba == 0xFF_FFFF || (self.lba == 0 && self.size == 0)
    }

    /// The file count of a directory record.
    pub fn directory_len(&self) -> Option<u32> {
        (self.size < 0).then(|| self.size.unsigned_abs())
    }

    /// Sectors a file read covers (whole 2048-byte sectors; movie sizes count
    /// 2336 bytes per sector, so use their own unit for them).
    pub fn sectors(&self) -> u32 {
        (self.size.max(0) as u32).div_ceil(FORM1_DATA_SIZE as u32)
    }
}

/// The parsed index and directory table.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Index {
    records: Vec<Record>,
    directories: Vec<u16>,
}

impl Index {
    /// Parse index bytes (whole records are used) and directory table bytes.
    pub fn parse(index: &[u8], directories: &[u8]) -> Self {
        Self {
            records: index.chunks_exact(RECORD_SIZE).map(Record::parse).collect(),
            directories: directories
                .chunks_exact(2)
                .map(|b| u16::from_le_bytes([b[0], b[1]]))
                .collect(),
        }
    }

    /// The copy on disc, which `cd_init_disc_access` reads with boot word 0
    /// and the disc swap reads from the new disc.
    pub fn from_disc(source: &mut dyn SectorSource) -> Result<Self> {
        let index = read_data(source, INDEX_LBA, INDEX_BYTES)?;
        let directories = read_data(source, DIRECTORY_LBA, DIRECTORY_BYTES)?;
        Ok(Self::parse(&index, &directories))
    }

    /// Records by slot (0-based), unused ones included.
    pub fn records(&self) -> &[Record] {
        &self.records
    }

    pub fn record(&self, slot: usize) -> Option<Record> {
        self.records.get(slot).copied()
    }

    /// Used records with their slots, in slot order (the extractor's manifest).
    pub fn entries(&self) -> impl Iterator<Item = (usize, Record)> + '_ {
        self.records
            .iter()
            .copied()
            .enumerate()
            .filter(|(_, r)| !r.is_unused())
    }

    /// The directory table entries.
    pub fn directory_table(&self) -> &[u16] {
        &self.directories
    }

    /// The disc number the table carries.
    pub fn disc_number(&self) -> Option<u16> {
        self.directories.get(DISC_NUMBER_ENTRY).copied()
    }

    /// The first record (1-based) of directory `directory` (the game's group +
    /// index), None for an empty (0) or unused (0xFFFF) entry or the disc number.
    pub fn directory_start(&self, directory: usize) -> Option<u16> {
        if directory >= DISC_NUMBER_ENTRY {
            return None;
        }
        self.directories
            .get(directory)
            .copied()
            .filter(|&start| start != 0 && start != 0xFFFF)
    }

    /// The slot of file `file` of directory `directory`: `file + table[directory] - 2`.
    pub fn slot(&self, directory: usize, file: u32) -> Option<usize> {
        let start = self.directory_start(directory)?;
        let slot = (u64::from(file) + u64::from(start)).checked_sub(2)? as usize;
        (slot < self.records.len()).then_some(slot)
    }

    /// The record of file `file` of directory `directory`.
    pub fn file(&self, directory: usize, file: u32) -> Option<Record> {
        self.record(self.slot(directory, file)?)
    }
}

/// The index embedded in a boot program (SLUS_006.64 or SLUS_006.69).
pub fn game_index(exe: &[u8]) -> Result<Index> {
    if exe.len() < EMBEDDED_DIRECTORIES + EMBEDDED_DIRECTORY_BYTES || &exe[..8] != b"PS-X EXE" {
        return Err(format_error(
            "not a PS-X EXE large enough to hold the file index",
        ));
    }
    let text = u32::from_le_bytes(exe[0x18..0x1C].try_into().expect("4 bytes"));
    if text != EXE_TEXT_ADDRESS {
        return Err(format_error(format!(
            "boot program text at {text:#x}, not {EXE_TEXT_ADDRESS:#x}"
        )));
    }
    Ok(Index::parse(
        &exe[EMBEDDED_INDEX..EMBEDDED_DIRECTORIES],
        &exe[EMBEDDED_DIRECTORIES..EMBEDDED_DIRECTORIES + EMBEDDED_DIRECTORY_BYTES],
    ))
}

#[cfg(test)]
pub(crate) mod tests_support {
    use super::*;

    fn record(lba: u32, size: i32) -> [u8; 7] {
        let l = lba.to_le_bytes();
        let s = size.to_le_bytes();
        [l[0], l[1], l[2], s[0], s[1], s[2], s[3]]
    }

    pub(crate) fn synthetic_exe() -> Vec<u8> {
        let mut exe = vec![0u8; EMBEDDED_DIRECTORIES + EMBEDDED_DIRECTORY_BYTES + 0x100];
        exe[..8].copy_from_slice(b"PS-X EXE");
        exe[0x18..0x1C].copy_from_slice(&EXE_TEXT_ADDRESS.to_le_bytes());
        exe[EXE_HEADER..EXE_HEADER + 4].copy_from_slice(&(-1i32).to_le_bytes());
        let records = [
            record(41, -3),
            record(41, 4096),
            record(43, 100),
            record(44, 2049),
            record(0xFF_FFFF, 0),
        ];
        for (n, r) in records.iter().enumerate() {
            exe[EMBEDDED_INDEX + n * 7..EMBEDDED_INDEX + n * 7 + 7].copy_from_slice(r);
        }
        let mut table = [0xFFFFu16; EMBEDDED_DIRECTORY_BYTES / 2];
        table[0] = 2; // file 1 is slot 1
        table[1] = 0;
        table[DISC_NUMBER_ENTRY] = 1;
        table[DISC_NUMBER_ENTRY + 1] = 0;
        for (n, entry) in table.iter().enumerate() {
            exe[EMBEDDED_DIRECTORIES + 2 * n..EMBEDDED_DIRECTORIES + 2 * n + 2]
                .copy_from_slice(&entry.to_le_bytes());
        }
        exe
    }
}

#[cfg(test)]
mod tests {
    use super::tests_support::synthetic_exe;
    use super::*;

    #[test]
    fn embedded_index_addressing() {
        let index = game_index(&synthetic_exe()).unwrap();
        assert_eq!(index.records().len(), INDEX_BYTES / RECORD_SIZE);
        assert_eq!(index.disc_number(), Some(1));
        assert_eq!(index.record(0).unwrap().directory_len(), Some(3));
        assert_eq!(index.slot(0, 1), Some(1));
        assert_eq!(
            index.file(0, 3),
            Some(Record {
                lba: 44,
                size: 2049
            })
        );
        assert_eq!(index.file(0, 3).unwrap().sectors(), 2);
        assert_eq!(index.file(0, 0).unwrap().directory_len(), Some(3));
        assert_eq!(index.slot(1, 1), None); // empty entry
        assert_eq!(index.slot(2, 1), None); // unused entry
        assert_eq!(index.slot(DISC_NUMBER_ENTRY, 1), None);
        let used: Vec<usize> = index.entries().map(|(slot, _)| slot).collect();
        assert_eq!(used, vec![0, 1, 2, 3]);
    }

    #[test]
    fn rejects_other_programs() {
        let mut exe = synthetic_exe();
        exe[0x18] = 0x10;
        assert!(game_index(&exe).is_err());
        assert!(game_index(&exe[..100]).is_err());
    }
}
