//! Local import of the user's Xenogears discs and a sector service over them.
//!
//! The discs are never bundled or uploaded: hosts open the user's own images
//! and read them in place, with bounded memory.
//!
//! - [`SectorSource`]: the data track as raw MODE2/2352 sectors by LBA, header
//!   and subheader kept, Form 1 and Form 2 alike, over a CHD
//!   ([`ChdSource`]), a raw track or `.bin`/`.cue` ([`BinSource`],
//!   [`parse_cue`]) or memory ([`MemorySource`]). [`open_path`] opens a native
//!   file by its kind, [`open_reader`] an already open one (CHD or raw track).
//! - [`ReadAt`]: the byte access under every source. Natively a [`FileReadAt`];
//!   in a browser a [`PrefetchedFile`].
//! - [`ByteLru`]: the byte-budgeted cache of decoded CHD hunks and fetched chunks.
//! - [`sector`]: header, subheader, form and user data of one sector.
//! - [`identify`]: which disc it is, from SYSTEM.CNF's boot program and its SHA-256.
//! - [`game_index`]: the game's own file index, keyed as the game keys files.
//!
//! # Reading in a browser
//!
//! A page reads a user's `File` only asynchronously, while the game's disc
//! service reads synchronously from the host loop, and the baseline host has
//! no worker to block. The sources therefore stay synchronous over a
//! [`PrefetchedFile`]: a bounded cache of fixed-size chunks of the file. A read
//! that needs an absent chunk fails with [`Error::NotReady`] and records the
//! chunk; the host takes the ranges ([`PrefetchedFile::take_missing`]), fetches
//! them with `Blob.slice(start, end).arrayBuffer()`, stores them
//! ([`PrefetchedFile::fill`]) and retries on a later turn of its loop. The
//! disc service reports such a read as still pending, as a real drive is
//! while it seeks, so the game waits without the host blocking. Opening a CHD
//! is retried the same way (its header, metadata and map arrive first), with
//! the file shared as `Rc<RefCell<PrefetchedFile>>`. Read-ahead is a matter of
//! requesting the ranges of upcoming sectors ([`PrefetchedFile::request`]) or
//! probing reads of them. Memory is the chunk budget plus the decoded hunk
//! budget, independent of disc size.

pub mod cache;
pub mod chd;
pub mod cue;
pub mod identify;
pub mod index;
pub mod read_at;
pub mod sector;
pub mod source;

pub use cache::ByteLru;
pub use chd::ChdSource;
pub use cue::{CueTrack, parse_cue};
pub use identify::{Identification, KnownDisc, identify};
pub use index::{Index, Record, game_index};
pub use read_at::{FileReadAt, PrefetchedFile, ReadAt};
pub use sector::{Form, SECTOR_SIZE, Sector};
pub use source::{
    BinSource, DEFAULT_CACHE_BYTES, Error, MemorySource, Result, SectorSource, open_path,
    open_reader,
};

use sha2::{Digest, Sha256};

/// `size` bytes of Form 1 data starting at `lba`: the 2048-byte window of each
/// sector, as the game's file reads and the extractor take them.
pub fn read_data(source: &mut dyn SectorSource, lba: u32, size: usize) -> Result<Vec<u8>> {
    let count = size.div_ceil(sector::FORM1_DATA_SIZE);
    let mut raw = vec![0u8; count * SECTOR_SIZE];
    source.read_sectors(lba, &mut raw)?;
    let mut out = Vec::with_capacity(count * sector::FORM1_DATA_SIZE);
    for chunk in raw.chunks_exact(SECTOR_SIZE) {
        out.extend_from_slice(Sector::new(chunk).expect("whole sector").form1_window());
    }
    out.truncate(size);
    Ok(out)
}

/// SHA-256 of a file's data (its record's size of Form 1 data): the content
/// identity of a (directory, file).
pub fn file_sha256(source: &mut dyn SectorSource, record: Record) -> Result<[u8; 32]> {
    let size = usize::try_from(record.size)
        .map_err(|_| source::format_error("a directory record has no data"))?;
    Ok(Sha256::digest(read_data(source, record.lba, size)?).into())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::sector::{FORM1_DATA_SIZE, Subheader, build_mode2_sector, submode};

    fn dir_record(name: &[u8], lba: u32, size: u32, directory: bool) -> Vec<u8> {
        let length = 33 + name.len() + (name.len() + 1) % 2;
        let mut r = vec![0u8; length];
        r[0] = length as u8;
        r[2..6].copy_from_slice(&lba.to_le_bytes());
        r[6..10].copy_from_slice(&lba.to_be_bytes());
        r[10..14].copy_from_slice(&size.to_le_bytes());
        r[14..18].copy_from_slice(&size.to_be_bytes());
        r[25] = if directory { 2 } else { 0 };
        r[32] = name.len() as u8;
        r[33..33 + name.len()].copy_from_slice(name);
        r
    }

    /// A small disc: PVD at 16, root at 18, SYSTEM.CNF at 19, the boot program
    /// from 50, and the index/directory table copies at 24 and 40.
    fn synthetic_disc(exe: &[u8]) -> MemorySource {
        let mut user = vec![[0u8; FORM1_DATA_SIZE]; 50 + exe.len().div_ceil(FORM1_DATA_SIZE)];
        let cnf = b"BOOT = cdrom:\\SLUS_999.99;1\r\nTCB = 4\r\n";
        let mut root = Vec::new();
        root.extend(dir_record(&[0], 18, 2048, true));
        root.extend(dir_record(&[1], 18, 2048, true));
        root.extend(dir_record(b"SLUS_999.99;1", 50, exe.len() as u32, false));
        root.extend(dir_record(b"SYSTEM.CNF;1", 19, cnf.len() as u32, false));
        user[18][..root.len()].copy_from_slice(&root);
        user[16][..6].copy_from_slice(b"\x01CD001");
        user[16][156..190].copy_from_slice(&dir_record(&[0], 18, 2048, true));
        user[19][..cnf.len()].copy_from_slice(cnf);
        for (n, chunk) in exe.chunks(FORM1_DATA_SIZE).enumerate() {
            user[50 + n][..chunk.len()].copy_from_slice(chunk);
        }
        // The copies on disc: index from exe 0x804, table from 0x8804.
        for n in 0..16 {
            user[24 + n].copy_from_slice(&exe[0x804 + n * 2048..0x804 + (n + 1) * 2048]);
        }
        user[40][..0x7A].copy_from_slice(&exe[0x8804..0x8804 + 0x7A]);
        let sub = Subheader {
            file: 0,
            channel: 0,
            submode: submode::DATA,
            coding: 0,
        };
        MemorySource::from_sectors(
            user.iter()
                .enumerate()
                .map(|(lba, data)| build_mode2_sector(lba as u32, sub, data)),
        )
    }

    #[test]
    fn identifies_and_indexes_a_synthetic_disc() {
        let exe = index::tests_support::synthetic_exe();
        let mut disc = synthetic_disc(&exe);
        let id = identify(&mut disc).unwrap();
        assert_eq!(id.boot_path, "SLUS_999.99;1");
        assert_eq!(id.boot_lba, 50);
        assert_eq!(id.executable, exe);
        assert_eq!(id.disc, None);
        let embedded = game_index(&id.executable).unwrap();
        let on_disc = Index::from_disc(&mut disc).unwrap();
        assert_eq!(embedded.records(), on_disc.records());
        assert_eq!(
            &embedded.directory_table()[..0x7A / 2],
            on_disc.directory_table()
        );
        assert_eq!(on_disc.disc_number(), Some(1));
        let record = embedded.file(0, 1).unwrap();
        assert_eq!(
            file_sha256(&mut disc, record).unwrap(),
            <[u8; 32]>::from(Sha256::digest(read_data(&mut disc, 41, 4096).unwrap()))
        );
        assert!(identify::iso_find(&mut disc, "NOPE.BIN").unwrap().is_none());
    }
}
