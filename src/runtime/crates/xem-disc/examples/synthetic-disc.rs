//! Write a small synthetic MODE2/2352 disc image for host tests (no game
//! data): an ISO9660 root with SYSTEM.CNF and a stand-in boot program
//! `SLUS_999.99`, which `identify` reads as an unknown disc. Prints the boot
//! program's SHA-256.
//!
//! cargo run -p xem-disc --example synthetic-disc -- out.bin

use sha2::{Digest, Sha256};
use xem_disc::identify::hex;
use xem_disc::sector::{FORM1_DATA_SIZE, Subheader, build_mode2_sector, submode};

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

fn main() -> std::io::Result<()> {
    let out = std::env::args()
        .nth(1)
        .expect("usage: synthetic-disc <out.bin>");
    // The stand-in boot program: a PS-X EXE header and a recognisable pattern.
    let mut exe = vec![0u8; 3 * FORM1_DATA_SIZE];
    exe[..8].copy_from_slice(b"PS-X EXE");
    for (n, byte) in exe[0x800..].iter_mut().enumerate() {
        *byte = (n * 7 % 251) as u8;
    }
    let exe_lba = 24u32;
    let cnf = b"BOOT = cdrom:\\SLUS_999.99;1\r\nTCB = 4\r\n";
    let mut user = vec![[0u8; FORM1_DATA_SIZE]; exe_lba as usize + exe.len() / FORM1_DATA_SIZE];
    let mut root = Vec::new();
    root.extend(dir_record(&[0], 18, 2048, true));
    root.extend(dir_record(&[1], 18, 2048, true));
    root.extend(dir_record(
        b"SLUS_999.99;1",
        exe_lba,
        exe.len() as u32,
        false,
    ));
    root.extend(dir_record(b"SYSTEM.CNF;1", 19, cnf.len() as u32, false));
    user[16][..6].copy_from_slice(b"\x01CD001");
    user[16][156..190].copy_from_slice(&dir_record(&[0], 18, 2048, true));
    user[18][..root.len()].copy_from_slice(&root);
    user[19][..cnf.len()].copy_from_slice(cnf);
    for (n, chunk) in exe.chunks(FORM1_DATA_SIZE).enumerate() {
        user[exe_lba as usize + n].copy_from_slice(chunk);
    }
    let sub = Subheader {
        file: 0,
        channel: 0,
        submode: submode::DATA,
        coding: 0,
    };
    let image: Vec<u8> = user
        .iter()
        .enumerate()
        .flat_map(|(lba, data)| build_mode2_sector(lba as u32, sub, data))
        .collect();
    std::fs::write(&out, image)?;
    println!("{}", hex(&Sha256::digest(&exe)));
    Ok(())
}
