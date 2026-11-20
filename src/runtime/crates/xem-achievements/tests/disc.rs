//! RetroAchievements' hash of a disc ([`xem_achievements::disc_hash`]):
//! rcheevos' own PlayStation hash read through the hosts' sector sources.
//!
//!     cargo test -p xem-achievements --test disc
//!
//! The synthetic disc's expected hashes were computed apart from rcheevos
//! (Python's hashlib over the boot program's name and its first
//! `t_size + 2048` bytes, as RetroAchievements documents the hash). The
//! user's discs (local: discs/ beside the repository, or XEM_DISC1 and
//! XEM_DISC2) hash to the values RetroAchievements' server knows as its game
//! 11248, Xenogears (`r=gameid`, checked on 2026-11-19).

use xem_disc::sector::{FORM1_DATA_SIZE, Subheader, build_mode2_sector, submode};
use xem_disc::{MemorySource, SectorSource};

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

/// xem-disc's synthetic disc (examples/synthetic-disc.rs): SYSTEM.CNF boots
/// `SLUS_999.99`, a three-sector PS-X EXE at LBA 24 whose header gives its
/// text's size `t_size`.
fn synthetic_disc(t_size: u32, cnf: &[u8]) -> MemorySource {
    let mut exe = vec![0u8; 3 * FORM1_DATA_SIZE];
    exe[..8].copy_from_slice(b"PS-X EXE");
    exe[28..32].copy_from_slice(&t_size.to_le_bytes());
    for (n, byte) in exe[0x800..].iter_mut().enumerate() {
        *byte = (n * 7 % 251) as u8;
    }
    let exe_lba = 24usize;
    let mut user = vec![[0u8; FORM1_DATA_SIZE]; exe_lba + exe.len() / FORM1_DATA_SIZE];
    let mut root = Vec::new();
    root.extend(dir_record(&[0], 18, 2048, true));
    root.extend(dir_record(&[1], 18, 2048, true));
    root.extend(dir_record(b"SLUS_999.99;1", exe_lba as u32, exe.len() as u32, false));
    root.extend(dir_record(b"SYSTEM.CNF;1", 19, cnf.len() as u32, false));
    user[16][..6].copy_from_slice(b"\x01CD001");
    user[16][156..190].copy_from_slice(&dir_record(&[0], 18, 2048, true));
    user[18][..root.len()].copy_from_slice(&root);
    user[19][..cnf.len()].copy_from_slice(cnf);
    for (n, chunk) in exe.chunks(FORM1_DATA_SIZE).enumerate() {
        user[exe_lba + n].copy_from_slice(chunk);
    }
    let sub = Subheader { file: 0, channel: 0, submode: submode::DATA, coding: 0 };
    MemorySource::from_sectors(user.iter().enumerate().map(|(lba, data)| build_mode2_sector(lba as u32, sub, data)))
}

const CNF: &[u8] = b"BOOT = cdrom:\\SLUS_999.99;1\r\nTCB = 4\r\n";

#[test]
fn a_disc_hashes_its_boot_programs_name_and_the_size_its_header_gives() {
    // The header's size and the header's own sector: the whole program.
    assert_eq!(xem_achievements::disc_hash(&mut synthetic_disc(4096, CNF)).unwrap(), "c3f019c1b0e4557aa6369e58ad96561f");
    // A smaller size in the header hashes that much, not the whole file.
    assert_eq!(xem_achievements::disc_hash(&mut synthetic_disc(2048, CNF)).unwrap(), "0c4577969829ece7e415f53d847871fa");
}

#[test]
fn a_disc_without_its_boot_program_is_refused_with_why() {
    let error = xem_achievements::disc_hash(&mut synthetic_disc(4096, b"BOOT = cdrom:\\SLUS_000.00;1\r\n")).unwrap_err();
    assert_eq!(error, "Could not locate primary executable");
    let blank: &mut dyn SectorSource = &mut MemorySource::from_sectors(std::iter::repeat_n([0u8; 2352], 32));
    assert!(xem_achievements::disc_hash(blank).is_err());
}

/// The user's disc `number`, if it is here.
fn disc(number: u8) -> Option<std::path::PathBuf> {
    let path = std::env::var_os(format!("XEM_DISC{number}")).map(std::path::PathBuf::from)
        .unwrap_or_else(|| std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join(format!("../../../../discs/Xenogears disc {number}.chd")));
    path.exists().then_some(path)
}

#[test]
fn the_users_discs_hash_as_retroachievements_knows_xenogears() {
    for (number, expected) in [(1, "1bde22c83a069c43cb32b8521bfdf7ae"), (2, "02ba8ee8c226923babe876bdf3a8f401")] {
        let Some(path) = disc(number) else {
            eprintln!("skipping disc {number}: not here (set XEM_DISC{number})");
            continue;
        };
        let mut source = xem_disc::open_path(&path).unwrap();
        assert_eq!(xem_achievements::disc_hash(source.as_mut()).unwrap(), expected, "{}", path.display());
    }
}
