//! MAME CD CHD images (`chdman createcd`), decoded by the `chd` crate.
//!
//! A CD CHD stores frames of 2448 bytes, the 2352-byte sector followed by 96
//! bytes of subcode, several to a hunk; the CD codecs (cdlz, cdzl, cdfl, cdzs)
//! drop and regenerate the sync and ECC of data sectors, so the decoded frames
//! hold the whole raw sector. Only the first track is served, and it must be a
//! MODE2_RAW track with no pregap stored in the image.

use std::io::{self, Read, Seek, SeekFrom};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};

use chd::Chd;
use chd::metadata::{KnownMetadata, Metadata, MetadataTag};

use crate::cache::ByteLru;
use crate::read_at::ReadAt;
use crate::sector::SECTOR_SIZE;
use crate::source::{Error, Result, SectorSource, check_span, format_error};

pub(crate) const CHD_MAGIC: &[u8; 8] = b"MComprHD";
/// Bytes per CD frame in a CHD: the sector and its subcode.
pub const CHD_FRAME_SIZE: usize = 2448;

/// `Read + Seek` over a [`ReadAt`] for the `chd` crate, which turns every I/O
/// error into its own codes: a `WouldBlock` is remembered in `blocked` so it
/// can be reported as [`Error::NotReady`].
struct Cursor<R> {
    inner: R,
    pos: u64,
    blocked: Arc<AtomicBool>,
}

impl<R: ReadAt> Read for Cursor<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let size = (self.inner.len().saturating_sub(self.pos)).min(buf.len() as u64) as usize;
        if let Err(err) = self.inner.read_at(self.pos, &mut buf[..size]) {
            if err.kind() == io::ErrorKind::WouldBlock {
                self.blocked.store(true, Ordering::Relaxed);
            }
            return Err(err);
        }
        self.pos += size as u64;
        Ok(size)
    }
}

impl<R: ReadAt> Seek for Cursor<R> {
    fn seek(&mut self, to: SeekFrom) -> io::Result<u64> {
        let base = match to {
            SeekFrom::Start(offset) => {
                self.pos = offset;
                return Ok(offset);
            }
            SeekFrom::End(delta) => (self.inner.len(), delta),
            SeekFrom::Current(delta) => (self.pos, delta),
        };
        self.pos = base
            .0
            .checked_add_signed(base.1)
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "seek before the start"))?;
        Ok(self.pos)
    }
}

fn chd_error(blocked: &AtomicBool, err: chd::Error) -> Error {
    if blocked.swap(false, Ordering::Relaxed) {
        Error::NotReady
    } else {
        format_error(format!("chd: {err}"))
    }
}

/// The first track's layout from its `CHT2`/`CHTR` metadata.
fn first_track(metadata: &[Metadata]) -> Result<u32> {
    let tracks = [
        KnownMetadata::CdRomTrack2.metatag(),
        KnownMetadata::CdRomTrack.metatag(),
    ];
    let mut found = None;
    for entry in metadata.iter().filter(|m| tracks.contains(&m.metatag)) {
        let text = String::from_utf8_lossy(&entry.value);
        let text = text.trim_end_matches('\0');
        let field = |name: &str| {
            text.split_whitespace()
                .find_map(|word| word.strip_prefix(name).and_then(|v| v.strip_prefix(':')))
        };
        if field("TRACK") != Some("1") {
            continue;
        }
        let kind = field("TYPE").unwrap_or("");
        if kind != "MODE2_RAW" {
            return Err(format_error(format!(
                "chd: track 1 is {kind}, not MODE2_RAW"
            )));
        }
        let pregap: u32 = field("PREGAP").and_then(|v| v.parse().ok()).unwrap_or(0);
        if pregap != 0 && field("PGTYPE").is_some_and(|t| t.starts_with('V')) {
            return Err(format_error(
                "chd: track 1 has its pregap stored in the image (unsupported)",
            ));
        }
        let frames = field("FRAMES")
            .and_then(|v| v.parse().ok())
            .ok_or_else(|| format_error("chd: track 1 without FRAMES"))?;
        found = Some(frames);
    }
    found.ok_or_else(|| format_error("chd: no CD track metadata (not a CD image)"))
}

/// The first track of a CD CHD, with a bounded cache of decoded hunks.
pub struct ChdSource<R: ReadAt> {
    chd: Chd<Cursor<R>>,
    blocked: Arc<AtomicBool>,
    hunks: ByteLru<u32>,
    frames_per_hunk: u32,
    sector_count: u32,
    compressed: Vec<u8>,
    hunk: Vec<u8>,
}

impl<R: ReadAt> ChdSource<R> {
    /// Open a CHD without a parent. `cache_bytes` bounds the decoded hunks kept
    /// (each is `frames_per_hunk * 2448` bytes, 19584 from `chdman createcd`).
    pub fn open(reader: R, cache_bytes: usize) -> Result<Self> {
        let blocked = Arc::new(AtomicBool::new(false));
        let cursor = Cursor {
            inner: reader,
            pos: 0,
            blocked: blocked.clone(),
        };
        let mut chd = Chd::open(cursor, None).map_err(|err| chd_error(&blocked, err))?;
        let header = chd.header();
        if header.has_parent() {
            return Err(format_error("chd: images with a parent are not supported"));
        }
        let (unit, hunk_size) = (header.unit_bytes() as usize, header.hunk_size() as usize);
        if unit != CHD_FRAME_SIZE || hunk_size % CHD_FRAME_SIZE != 0 || hunk_size == 0 {
            return Err(format_error(format!(
                "chd: not a CD image (unit {unit}, hunk {hunk_size})"
            )));
        }
        let units = header.unit_count();
        let metadata: Vec<Metadata> = chd
            .metadata_refs()
            .try_into()
            .map_err(|err| chd_error(&blocked, err))?;
        let sector_count = first_track(&metadata)?;
        if u64::from(sector_count) > units {
            return Err(format_error("chd: track 1 longer than the image"));
        }
        let hunk = chd.get_hunksized_buffer();
        Ok(Self {
            chd,
            blocked,
            hunks: ByteLru::new(cache_bytes),
            frames_per_hunk: (hunk_size / CHD_FRAME_SIZE) as u32,
            sector_count,
            compressed: Vec::new(),
            hunk,
        })
    }

    pub fn frames_per_hunk(&self) -> u32 {
        self.frames_per_hunk
    }

    /// Bytes of decoded hunks held.
    pub fn cached_bytes(&self) -> usize {
        self.hunks.used()
    }

    /// Decode `number` into `self.hunk` unless cached; returns it.
    fn hunk(&mut self, number: u32) -> Result<&[u8]> {
        if !self.hunks.contains(&number) {
            self.blocked.store(false, Ordering::Relaxed);
            let mut hunk = self
                .chd
                .hunk(number)
                .map_err(|err| chd_error(&self.blocked, err))?;
            hunk.read_hunk_in(&mut self.compressed, &mut self.hunk)
                .map_err(|err| chd_error(&self.blocked, err))?;
            if self.hunks.insert(number, self.hunk.clone()) {
                return Ok(self.hunks.get(&number).expect("inserted hunk"));
            }
            return Ok(&self.hunk);
        }
        Ok(self.hunks.get(&number).expect("cached hunk"))
    }
}

impl<R: ReadAt> SectorSource for ChdSource<R> {
    fn sector_count(&self) -> u32 {
        self.sector_count
    }

    fn read_sector(&mut self, lba: u32, out: &mut [u8; SECTOR_SIZE]) -> Result<()> {
        check_span(lba, SECTOR_SIZE, self.sector_count)?;
        let fph = self.frames_per_hunk;
        let at = (lba % fph) as usize * CHD_FRAME_SIZE;
        let hunk = self.hunk(lba / fph)?;
        out.copy_from_slice(&hunk[at..at + SECTOR_SIZE]);
        Ok(())
    }
}
