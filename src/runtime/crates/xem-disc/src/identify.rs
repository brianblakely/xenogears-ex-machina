//! Which disc this is: SYSTEM.CNF's boot program, found through ISO9660, and
//! its SHA-256; whether the image is Redump's dump of it ([`verify`]).

use sha1::Sha1;
use sha2::{Digest, Sha256};

use crate::sector::SECTOR_SIZE;
use crate::source::{Error, Result, SectorSource, format_error};
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

    /// The data track's bytes, its one MODE2/2352 track as Redump records it
    /// (redump.org discs 177 and 178, the original and Greatest Hits editions).
    pub fn track_bytes(self) -> u64 {
        match self {
            KnownDisc::Disc1 => 718_738_272,
            KnownDisc::Disc2 => 688_700_880,
        }
    }

    /// SHA-1 of the whole track, as Redump records it.
    pub fn track_sha1(self) -> [u8; 20] {
        hex20(match self {
            KnownDisc::Disc1 => "12db8ccb93516c391630f046a143762337cc21f4",
            KnownDisc::Disc2 => "0a4c56e6c64db11f8eada43545c2af08e948665c",
        })
    }

    /// The SHA-1 the header of `chdman createcd`'s CHD of Redump's dump records
    /// of its raw data (frames with zeroed subcode, the track padded to four
    /// frames); its decoded track hashes to [`Self::track_sha1`].
    pub fn chd_raw_sha1(self) -> [u8; 20] {
        hex20(match self {
            KnownDisc::Disc1 => "4c3d8621e4aaf431d5cac0f2da91e6cf45e68ef5",
            KnownDisc::Disc2 => "de365ba58f10376a85d91f4e72c411023c2053ec",
        })
    }

    fn from_sha256(sha256: &[u8; 32]) -> Option<Self> {
        [KnownDisc::Disc1, KnownDisc::Disc2]
            .into_iter()
            .find(|disc| disc.executable_sha256() == *sha256)
    }
}

/// What save states name the game by when it boots from a known disc: both
/// discs hold one program (their boot programs differ only in the file index
/// each embeds, which the game loads again from the disc it changes to), so
/// a state restores in a session booted from either, the drive holding the
/// disc it had. Any other disc is named by its boot program's SHA-256.
pub fn state_identity(identification: &Identification) -> String {
    match identification.disc {
        Some(_) => "SLUS-00664+SLUS-00669".into(),
        None => hex(&identification.sha256),
    }
}

/// Whether `source` is Redump's dump of `disc`: a CHD by the SHA-1 its header
/// records of its raw data (that of chdman's CHD of Redump's dump), any other
/// image by the size and SHA-1 of its whole track. A step at a time where
/// the image's bytes arrive asynchronously ([`Verification::advance`]).
pub struct Verification {
    disc: KnownDisc,
    /// The next sector to hash.
    next: u32,
    sha1: Sha1,
}

/// Sectors a [`Verification`] reads at once.
const VERIFY_SECTORS: u32 = 64;

impl Verification {
    pub fn new(disc: KnownDisc) -> Self {
        Verification { disc, next: 0, sha1: Sha1::new() }
    }

    /// Reads on as far as the image's bytes allow: true once the image is the
    /// dump, false when a read waits for bytes ([`Error::NotReady`]); an error
    /// when the image is not the dump.
    pub fn advance(&mut self, source: &mut dyn SectorSource) -> Result<bool> {
        let disc = self.disc;
        let refused = |what: &str| {
            format_error(format!("not Redump's dump of Xenogears (USA) disc {}: {what}", disc.number()))
        };
        if let Some(raw) = source.chd_raw_sha1() {
            return if raw == disc.chd_raw_sha1() { Ok(true) } else { Err(refused("this CHD holds other data")) };
        }
        let sectors = self.sectors();
        if source.sector_count() != sectors {
            return Err(refused(&format!("its track has {} sectors, Redump's {sectors}", source.sector_count())));
        }
        let mut chunk = vec![0u8; VERIFY_SECTORS as usize * SECTOR_SIZE];
        while self.next < sectors {
            let bytes = &mut chunk[..(sectors - self.next).min(VERIFY_SECTORS) as usize * SECTOR_SIZE];
            match source.read_sectors(self.next, bytes) {
                Ok(()) => {}
                Err(Error::NotReady) => return Ok(false),
                Err(error) => return Err(error),
            }
            self.sha1.update(&bytes[..]);
            self.next += (bytes.len() / SECTOR_SIZE) as u32;
        }
        if <[u8; 20]>::from(self.sha1.clone().finalize()) != disc.track_sha1() {
            return Err(refused("its SHA-1 is not Redump's"));
        }
        Ok(true)
    }

    /// The byte range of the image the next reads take (a host fetching it
    /// asynchronously reads ahead with it); empty for a CHD.
    pub fn next_bytes(&self, source: &dyn SectorSource, budget: u64) -> std::ops::Range<u64> {
        if source.chd_raw_sha1().is_some() {
            return 0..0;
        }
        let start = u64::from(self.next) * SECTOR_SIZE as u64;
        start..(start + budget).min(self.disc.track_bytes())
    }

    /// The share of the image hashed so far (0 to 1).
    pub fn progress(&self) -> f32 {
        self.next as f32 / self.sectors() as f32
    }

    fn sectors(&self) -> u32 {
        (self.disc.track_bytes() / SECTOR_SIZE as u64) as u32
    }
}

/// [`Verification`] at once, for images whose reads never wait (native files).
pub fn verify(source: &mut dyn SectorSource, disc: KnownDisc) -> Result<()> {
    match Verification::new(disc).advance(source)? {
        true => Ok(()),
        false => Err(Error::NotReady),
    }
}

fn hex20(text: &str) -> [u8; 20] {
    let mut out = [0u8; 20];
    for (n, byte) in out.iter_mut().enumerate() {
        *byte = u8::from_str_radix(&text[2 * n..2 * n + 2], 16).expect("hex digest");
    }
    out
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
    fn an_image_must_be_redumps_dump() {
        use crate::{BinSource, PrefetchedFile};
        let refused = |source: &mut dyn SectorSource| verify(source, KnownDisc::Disc2).unwrap_err().to_string();
        let mut short = BinSource::whole(vec![0u8; 64 * SECTOR_SIZE]).unwrap();
        assert!(refused(&mut short).contains("its track has 64 sectors, Redump's 292815"), "{}", refused(&mut short));
        // A track of disc 2's size that waits for its bytes, a step at a time.
        let bytes = KnownDisc::Disc2.track_bytes();
        let file = std::rc::Rc::new(std::cell::RefCell::new(PrefetchedFile::new(bytes, 1 << 20, 4 << 20)));
        let mut waiting = BinSource::whole(file.clone()).unwrap();
        let mut verification = Verification::new(KnownDisc::Disc2);
        assert!(!verification.advance(&mut waiting).unwrap(), "no bytes yet");
        assert_eq!(verification.next_bytes(&waiting, 4 << 20), 0..4 << 20);
        let missing = file.borrow_mut().take_missing();
        for range in missing {
            let length = (range.end - range.start) as usize;
            file.borrow_mut().fill(range.start, &vec![0u8; length]).unwrap();
        }
        assert!(!verification.advance(&mut waiting).unwrap());
        assert!(verification.progress() > 0.0, "it went on from where it waited");
        assert_eq!(hex(&KnownDisc::Disc1.track_sha1())[..8], *"12db8ccb");
        assert_eq!(hex(&KnownDisc::Disc2.chd_raw_sha1())[..8], *"de365ba5");
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
