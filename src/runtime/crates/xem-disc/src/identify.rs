//! Which disc this is: SYSTEM.CNF's boot program, found through ISO9660, and
//! its SHA-256.

use sha2::{Digest, Sha256};

use crate::source::{Result, SectorSource, format_error};
use crate::{read_data, sector::FORM1_DATA_SIZE};

/// A Xenogears disc this runtime knows.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum KnownDisc {
    /// NTSC-U disc 1, SLUS-00664.
    Disc1,
    /// NTSC-U disc 2, SLUS-00669.
    Disc2,
}

impl KnownDisc {
    pub fn number(self) -> u8 {
        match self {
            KnownDisc::Disc1 => 1,
            KnownDisc::Disc2 => 2,
        }
    }

    pub fn serial(self) -> &'static str {
        match self {
            KnownDisc::Disc1 => "SLUS-00664",
            KnownDisc::Disc2 => "SLUS-00669",
        }
    }

    /// SHA-256 of the disc's boot program.
    pub fn executable_sha256(self) -> [u8; 32] {
        match self {
            KnownDisc::Disc1 => {
                hex32("dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119")
            }
            KnownDisc::Disc2 => {
                hex32("3246e15f4040305b280adae06bc7bb908ee882794183bec9fc23e71d85c19c35")
            }
        }
    }

    fn from_sha256(sha256: &[u8; 32]) -> Option<Self> {
        [KnownDisc::Disc1, KnownDisc::Disc2]
            .into_iter()
            .find(|disc| disc.executable_sha256() == *sha256)
    }
}

fn hex32(text: &str) -> [u8; 32] {
    let mut out = [0u8; 32];
    for (n, byte) in out.iter_mut().enumerate() {
        *byte = u8::from_str_radix(&text[2 * n..2 * n + 2], 16).expect("hex digest");
    }
    out
}

/// Lower-case hex of a digest.
pub fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

/// A file found through the ISO9660 file system.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct IsoFile {
    pub lba: u32,
    pub size: u32,
    pub is_directory: bool,
}

/// Look up a path (components separated by `\` or `/`, version suffix `;1`
/// optional, case ignored) from the root of the primary volume.
pub fn iso_find(source: &mut dyn SectorSource, path: &str) -> Result<Option<IsoFile>> {
    let pvd = read_data(source, 16, FORM1_DATA_SIZE)?;
    if &pvd[..6] != b"\x01CD001" {
        return Err(format_error(
            "no ISO9660 primary volume descriptor at sector 16",
        ));
    }
    let mut current = directory_record(&pvd[156..190])?;
    for name in path.split(['\\', '/']).filter(|c| !c.is_empty()) {
        if !current.is_directory {
            return Ok(None);
        }
        let want = name.split(';').next().unwrap_or(name);
        let listing = read_data(source, current.lba, current.size as usize)?;
        let mut found = None;
        let mut pos = 0;
        while pos < listing.len() {
            let length = listing[pos] as usize;
            if length == 0 {
                // Records do not cross sectors; the rest of this one is padding.
                pos = (pos / FORM1_DATA_SIZE + 1) * FORM1_DATA_SIZE;
                continue;
            }
            let record = listing
                .get(pos..pos + length)
                .ok_or_else(|| format_error("ISO9660 record past its directory"))?;
            let entry_name = record_name(record)?;
            if entry_name
                .split(';')
                .next()
                .unwrap_or("")
                .eq_ignore_ascii_case(want)
            {
                found = Some(directory_record(record)?);
                break;
            }
            pos += length;
        }
        match found {
            Some(entry) => current = entry,
            None => return Ok(None),
        }
    }
    Ok(Some(current))
}

fn directory_record(record: &[u8]) -> Result<IsoFile> {
    if record.len() < 34 {
        return Err(format_error("short ISO9660 directory record"));
    }
    let le32 = |at: usize| u32::from_le_bytes(record[at..at + 4].try_into().expect("4 bytes"));
    Ok(IsoFile {
        lba: le32(2),
        size: le32(10),
        is_directory: record[25] & 2 != 0,
    })
}

fn record_name(record: &[u8]) -> Result<String> {
    let length = *record
        .get(32)
        .ok_or_else(|| format_error("short ISO9660 directory record"))? as usize;
    let name = record
        .get(33..33 + length)
        .ok_or_else(|| format_error("ISO9660 name past its record"))?;
    Ok(String::from_utf8_lossy(name).into_owned())
}

/// The boot path of a PlayStation `SYSTEM.CNF` (`BOOT = cdrom:\SLUS_006.64;1`),
/// without the device prefix.
pub fn boot_path(system_cnf: &str) -> Option<String> {
    system_cnf.lines().find_map(|line| {
        let (key, value) = line.split_once('=')?;
        if !key.trim().eq_ignore_ascii_case("BOOT") {
            return None;
        }
        let value = value.trim();
        let path = value
            .get(..6)
            .filter(|p| p.eq_ignore_ascii_case("cdrom:"))
            .map_or(value, |_| &value[6..]);
        Some(path.trim_start_matches(['\\', '/']).to_string())
    })
}

/// What a disc is.
#[derive(Clone, Debug)]
pub struct Identification {
    /// The boot program's path from SYSTEM.CNF, as written.
    pub boot_path: String,
    pub boot_lba: u32,
    /// The boot program's bytes.
    pub executable: Vec<u8>,
    /// SHA-256 of `executable`: the disc's content identity.
    pub sha256: [u8; 32],
    /// The known disc it is, None for any other disc.
    pub disc: Option<KnownDisc>,
}

/// Identify a PlayStation disc by its boot program.
pub fn identify(source: &mut dyn SectorSource) -> Result<Identification> {
    let cnf = iso_find(source, "SYSTEM.CNF")?
        .ok_or_else(|| format_error("no SYSTEM.CNF in the ISO9660 root"))?;
    let text = read_data(source, cnf.lba, cnf.size as usize)?;
    let boot_path = boot_path(&String::from_utf8_lossy(&text))
        .ok_or_else(|| format_error("SYSTEM.CNF names no BOOT program"))?;
    let exe = iso_find(source, &boot_path)?
        .filter(|f| !f.is_directory)
        .ok_or_else(|| format_error(format!("boot program {boot_path} not on the disc")))?;
    let executable = read_data(source, exe.lba, exe.size as usize)?;
    let sha256: [u8; 32] = Sha256::digest(&executable).into();
    Ok(Identification {
        boot_path,
        boot_lba: exe.lba,
        disc: KnownDisc::from_sha256(&sha256),
        sha256,
        executable,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn boot_line_forms() {
        assert_eq!(
            boot_path("BOOT = cdrom:\\SLUS_006.64;1\r\nTCB = 4\r\n").as_deref(),
            Some("SLUS_006.64;1")
        );
        assert_eq!(
            boot_path("boot=cdrom:\\GAME\\MAIN.EXE;1").as_deref(),
            Some("GAME\\MAIN.EXE;1")
        );
        assert_eq!(boot_path("TCB = 4"), None);
    }

    #[test]
    fn known_digests() {
        assert_eq!(
            KnownDisc::from_sha256(&KnownDisc::Disc2.executable_sha256()),
            Some(KnownDisc::Disc2)
        );
        assert_eq!(hex(&KnownDisc::Disc1.executable_sha256())[..8], *"dc0b2dd7");
        assert_eq!(KnownDisc::from_sha256(&[0; 32]), None);
    }
}
