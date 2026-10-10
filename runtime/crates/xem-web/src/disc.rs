//! The user's disc image, read in place from the `File` the user picked.
//!
//! The page reads a `File` only asynchronously; the sector sources read
//! synchronously over a [`PrefetchedFile`]. Each [`DiscImport::poll`] tries to
//! make progress (detect the image kind, open it, identify the disc); when the
//! bytes it needs have not arrived it reports the missing ranges, which the
//! page fetches with `Blob.slice(start, end).arrayBuffer()` and hands back with
//! [`DiscImport::fill`]. Memory is the chunk budget plus the decoded hunk
//! budget, whatever the image size. Nothing leaves the machine.

use std::cell::RefCell;
use std::ops::Range;
use std::rc::Rc;

use serde::Serialize;
use xem_disc::{BinSource, ChdSource, Error, Identification, PrefetchedFile, ReadAt, SECTOR_SIZE, SectorSource};

/// Fetch granularity.
pub const CHUNK_BYTES: usize = 256 << 10;
/// Bytes of fetched chunks kept.
pub const CHUNK_BUDGET: usize = 8 << 20;
/// Bytes of decoded CHD hunks kept.
pub const HUNK_BUDGET: usize = 4 << 20;
/// The largest sector read (its chunks must fit the budget at once).
pub const MAX_READ_SECTORS: u32 = 64;

const CHD_MAGIC: &[u8; 8] = b"MComprHD";

type SharedFile = Rc<RefCell<PrefetchedFile>>;

enum Stage {
    Detect,
    Open { chd: bool },
    Identify(Box<dyn SectorSource>),
    Ready(Box<dyn SectorSource>),
    Failed(String),
}

/// The disc identity shown to the user and to automation.
#[derive(Clone, Debug, Serialize)]
pub struct Identity {
    /// 1 or 2 for a known Xenogears disc.
    pub disc: Option<u8>,
    pub serial: Option<&'static str>,
    pub boot_path: String,
    pub boot_lba: u32,
    pub sha256: String,
    pub executable_bytes: usize,
}

#[derive(Clone, Debug, Serialize)]
pub struct DiscStatus {
    pub name: String,
    pub bytes: u64,
    /// "detecting", "opening", "identifying", "identified" or "failed".
    pub state: &'static str,
    pub kind: Option<&'static str>,
    pub error: Option<String>,
    pub identity: Option<Identity>,
    pub chunk_bytes: usize,
    pub chunk_budget: usize,
    pub hunk_budget: usize,
    pub resident_bytes: usize,
    pub peak_resident_bytes: usize,
    pub fetched_bytes: u64,
    pub fetches: u64,
    pub polls: u64,
}

/// One disc image being imported, then served.
pub struct DiscImport {
    name: String,
    file: SharedFile,
    stage: Stage,
    /// Whether the image is a CHD, once detected.
    chd: Option<bool>,
    identification: Option<Identification>,
    peak: usize,
    fetched: u64,
    fetches: u64,
    polls: u64,
}

impl DiscImport {
    pub fn new(name: String, bytes: u64) -> Self {
        DiscImport {
            name,
            file: Rc::new(RefCell::new(PrefetchedFile::new(bytes, CHUNK_BYTES, CHUNK_BUDGET))),
            stage: Stage::Detect,
            chd: None,
            identification: None,
            peak: 0,
            fetched: 0,
            fetches: 0,
            polls: 0,
        }
    }

    /// Store fetched bytes (a range `poll` asked for).
    pub fn fill(&mut self, offset: u64, data: &[u8]) -> Result<(), String> {
        let mut file = self.file.borrow_mut();
        file.fill(offset, data).map_err(|e| e.to_string())?;
        self.fetched += data.len() as u64;
        self.fetches += 1;
        self.peak = self.peak.max(file.resident_bytes());
        Ok(())
    }

    /// Advance as far as the fetched bytes allow. Returns the byte ranges to
    /// fetch before the next poll; empty once identified or failed.
    pub fn poll(&mut self) -> Vec<Range<u64>> {
        self.polls += 1;
        loop {
            let next = match &mut self.stage {
                Stage::Detect => {
                    let mut magic = [0u8; 8];
                    let len = self.file.borrow().len();
                    if len < 8 {
                        Ok(Stage::Open { chd: false })
                    } else {
                        self.file.read_at(0, &mut magic).map(|()| Stage::Open { chd: &magic == CHD_MAGIC }).map_err(Error::from)
                    }
                }
                Stage::Open { chd: true } => ChdSource::open(self.file.clone(), HUNK_BUDGET)
                    .map(|source| Stage::Identify(Box::new(source) as Box<dyn SectorSource>)),
                Stage::Open { chd: false } => BinSource::whole(self.file.clone())
                    .map(|source| Stage::Identify(Box::new(source) as Box<dyn SectorSource>)),
                Stage::Identify(source) => match xem_disc::identify(source.as_mut()) {
                    Ok(identification) => {
                        self.identification = Some(identification);
                        let Stage::Identify(source) = std::mem::replace(&mut self.stage, Stage::Detect) else {
                            unreachable!()
                        };
                        Ok(Stage::Ready(source))
                    }
                    Err(error) => Err(error),
                },
                Stage::Ready(_) | Stage::Failed(_) => return Vec::new(),
            };
            match next {
                Ok(stage) => {
                    if let Stage::Open { chd } = stage {
                        self.chd = Some(chd);
                    }
                    self.stage = stage;
                }
                Err(Error::NotReady) => return self.file.borrow_mut().take_missing(),
                Err(error) => self.stage = Stage::Failed(error.to_string()),
            }
        }
    }

    /// Raw 2352-byte sectors of the identified disc, or `None` while their
    /// bytes are being fetched (the ranges are in [`take_missing`](Self::take_missing)).
    pub fn read_sectors(&mut self, lba: u32, count: u32) -> Result<Option<Vec<u8>>, String> {
        if count > MAX_READ_SECTORS {
            return Err(format!("read at most {MAX_READ_SECTORS} sectors at once"));
        }
        let Stage::Ready(source) = &mut self.stage else {
            return Err("the disc is not identified".into());
        };
        let mut out = vec![0; count as usize * SECTOR_SIZE];
        match source.read_sectors(lba, &mut out) {
            Ok(()) => Ok(Some(out)),
            Err(Error::NotReady) => Ok(None),
            Err(error) => Err(error.to_string()),
        }
    }

    /// The byte ranges reads are waiting for.
    pub fn take_missing(&mut self) -> Vec<Range<u64>> {
        self.file.borrow_mut().take_missing()
    }

    pub fn identification(&self) -> Option<&Identification> {
        self.identification.as_ref()
    }

    pub fn status(&self) -> DiscStatus {
        let (state, error) = match &self.stage {
            Stage::Detect => ("detecting", None),
            Stage::Open { .. } => ("opening", None),
            Stage::Identify(_) => ("identifying", None),
            Stage::Ready(_) => ("identified", None),
            Stage::Failed(error) => ("failed", Some(error.clone())),
        };
        let kind = self.chd.map(|chd| if chd { "chd" } else { "bin" });
        DiscStatus {
            name: self.name.clone(),
            bytes: self.file.borrow().len(),
            state,
            kind,
            error,
            identity: self.identification.as_ref().map(|id| Identity {
                disc: id.disc.map(|d| d.number()),
                serial: id.disc.map(|d| d.serial()),
                boot_path: id.boot_path.clone(),
                boot_lba: id.boot_lba,
                sha256: xem_disc::identify::hex(&id.sha256),
                executable_bytes: id.executable.len(),
            }),
            chunk_bytes: CHUNK_BYTES,
            chunk_budget: CHUNK_BUDGET,
            hunk_budget: HUNK_BUDGET,
            resident_bytes: self.file.borrow().resident_bytes(),
            peak_resident_bytes: self.peak,
            fetched_bytes: self.fetched,
            fetches: self.fetches,
            polls: self.polls,
        }
    }
}
