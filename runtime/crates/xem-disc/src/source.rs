//! Raw sectors by LBA from the user's images.

use std::fmt;
use std::io;
use std::path::Path;

use crate::chd::ChdSource;
use crate::cue::{self, CueTrack};
use crate::read_at::{FileReadAt, ReadAt};
use crate::sector::SECTOR_SIZE;

#[derive(Debug)]
pub enum Error {
    /// The image bytes a read needs have not been fetched yet; the host fetches
    /// them (see [`PrefetchedFile`](crate::PrefetchedFile)) and retries.
    NotReady,
    /// Sectors past the end of the track.
    OutOfRange {
        lba: u32,
        count: u32,
        sector_count: u32,
    },
    Io(io::Error),
    /// The image or the data on it is not in the expected form.
    Format(String),
}

pub type Result<T> = std::result::Result<T, Error>;

impl From<io::Error> for Error {
    fn from(err: io::Error) -> Self {
        if err.kind() == io::ErrorKind::WouldBlock {
            Error::NotReady
        } else {
            Error::Io(err)
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::NotReady => write!(f, "image bytes not fetched yet"),
            Error::OutOfRange {
                lba,
                count,
                sector_count,
            } => {
                write!(
                    f,
                    "sectors {lba}+{count} outside the track's {sector_count}"
                )
            }
            Error::Io(err) => write!(f, "{err}"),
            Error::Format(message) => write!(f, "{message}"),
        }
    }
}

impl std::error::Error for Error {}

pub(crate) fn format_error(message: impl Into<String>) -> Error {
    Error::Format(message.into())
}

/// The data track of one disc as raw 2352-byte sectors (sync, header,
/// subheader, payload, EDC/ECC), addressed by LBA from 0.
pub trait SectorSource {
    fn sector_count(&self) -> u32;

    fn read_sector(&mut self, lba: u32, out: &mut [u8; SECTOR_SIZE]) -> Result<()>;

    /// Consecutive sectors into `out`, whose length is a multiple of 2352.
    fn read_sectors(&mut self, lba: u32, out: &mut [u8]) -> Result<()> {
        let count = check_span(lba, out.len(), self.sector_count())?;
        for (n, chunk) in out.chunks_exact_mut(SECTOR_SIZE).enumerate() {
            self.read_sector(lba + n as u32, chunk.try_into().expect("sector chunk"))?;
        }
        debug_assert_eq!(count as usize * SECTOR_SIZE, out.len());
        Ok(())
    }
}

impl<S: SectorSource + ?Sized> SectorSource for Box<S> {
    fn sector_count(&self) -> u32 {
        (**self).sector_count()
    }
    fn read_sector(&mut self, lba: u32, out: &mut [u8; SECTOR_SIZE]) -> Result<()> {
        (**self).read_sector(lba, out)
    }
    fn read_sectors(&mut self, lba: u32, out: &mut [u8]) -> Result<()> {
        (**self).read_sectors(lba, out)
    }
}

impl<S: SectorSource + ?Sized> SectorSource for &mut S {
    fn sector_count(&self) -> u32 {
        (**self).sector_count()
    }
    fn read_sector(&mut self, lba: u32, out: &mut [u8; SECTOR_SIZE]) -> Result<()> {
        (**self).read_sector(lba, out)
    }
    fn read_sectors(&mut self, lba: u32, out: &mut [u8]) -> Result<()> {
        (**self).read_sectors(lba, out)
    }
}

/// Validate a read of `bytes` at `lba`; returns the sector count.
pub(crate) fn check_span(lba: u32, bytes: usize, sector_count: u32) -> Result<u32> {
    if !bytes.is_multiple_of(SECTOR_SIZE) {
        return Err(format_error(format!(
            "buffer of {bytes} bytes is not whole sectors"
        )));
    }
    let count = (bytes / SECTOR_SIZE) as u32;
    match lba.checked_add(count) {
        Some(end) if end <= sector_count => Ok(count),
        _ => Err(Error::OutOfRange {
            lba,
            count,
            sector_count,
        }),
    }
}

/// A raw MODE2/2352 track: `chdman extractcd` output or a `.bin` a `.cue`
/// describes. With `Vec<u8>` it is the in-memory source.
pub struct BinSource<R> {
    reader: R,
    first_byte: u64,
    sector_count: u32,
}

impl<R: ReadAt> BinSource<R> {
    /// The track starts `first_sector` sectors into the file and runs for
    /// `sector_count` sectors, or to the end of the file.
    pub fn new(reader: R, first_sector: u32, sector_count: Option<u32>) -> Result<Self> {
        let first_byte = u64::from(first_sector) * SECTOR_SIZE as u64;
        let available = reader
            .len()
            .checked_sub(first_byte)
            .ok_or_else(|| format_error("track starts past the end of its file"))?;
        let whole = available / SECTOR_SIZE as u64;
        let sector_count = match sector_count {
            Some(count) if u64::from(count) <= whole => count,
            Some(count) => {
                return Err(format_error(format!(
                    "track of {count} sectors exceeds its file ({whole})"
                )));
            }
            None => {
                if !available.is_multiple_of(SECTOR_SIZE as u64) {
                    return Err(format_error("file is not whole 2352-byte sectors"));
                }
                u32::try_from(whole).map_err(|_| format_error("track too long"))?
            }
        };
        Ok(Self {
            reader,
            first_byte,
            sector_count,
        })
    }

    /// The whole file as the track.
    pub fn whole(reader: R) -> Result<Self> {
        Self::new(reader, 0, None)
    }

    /// The track a cue sheet's first track describes, read from its file.
    pub fn from_cue(reader: R, track: &CueTrack) -> Result<Self> {
        Self::new(reader, track.start, track.end.map(|end| end - track.start))
    }

    pub fn into_inner(self) -> R {
        self.reader
    }
}

impl BinSource<Vec<u8>> {
    /// An in-memory track from raw sectors, LBA 0 first.
    pub fn from_sectors<I: IntoIterator<Item = [u8; SECTOR_SIZE]>>(sectors: I) -> Self {
        let bytes: Vec<u8> = sectors.into_iter().flatten().collect();
        Self::whole(bytes).expect("whole sectors")
    }
}

/// The in-memory source.
pub type MemorySource = BinSource<Vec<u8>>;

impl<R: ReadAt> SectorSource for BinSource<R> {
    fn sector_count(&self) -> u32 {
        self.sector_count
    }

    fn read_sector(&mut self, lba: u32, out: &mut [u8; SECTOR_SIZE]) -> Result<()> {
        self.read_sectors(lba, out)
    }

    fn read_sectors(&mut self, lba: u32, out: &mut [u8]) -> Result<()> {
        check_span(lba, out.len(), self.sector_count)?;
        let offset = self.first_byte + u64::from(lba) * SECTOR_SIZE as u64;
        Ok(self.reader.read_at(offset, out)?)
    }
}

/// Default bytes of decompressed CHD hunks kept by [`open_path`]: 4 MiB, about
/// 1700 sectors.
pub const DEFAULT_CACHE_BYTES: usize = 4 << 20;

/// Open a native image file: a CHD (by its signature), a `.cue` (its first
/// track's `.bin`, relative to the sheet) or a raw MODE2/2352 track.
pub fn open_path(path: &Path) -> Result<Box<dyn SectorSource>> {
    let mut file = FileReadAt::open(path)?;
    let mut magic = [0u8; 8];
    if file.len() >= 8 {
        file.read_at(0, &mut magic)?;
    }
    if &magic == crate::chd::CHD_MAGIC {
        return Ok(Box::new(ChdSource::open(file, DEFAULT_CACHE_BYTES)?));
    }
    let is_cue = path
        .extension()
        .is_some_and(|ext| ext.eq_ignore_ascii_case("cue"));
    if is_cue {
        let text = std::fs::read_to_string(path)?;
        let track = cue::parse_cue(&text)?;
        let bin = path.parent().unwrap_or(Path::new(".")).join(&track.file);
        return Ok(Box::new(BinSource::from_cue(
            FileReadAt::open(bin)?,
            &track,
        )?));
    }
    Ok(Box::new(BinSource::whole(file)?))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::sector::{Sector, Subheader, build_mode2_sector, submode};

    fn data_sector(lba: u32) -> [u8; SECTOR_SIZE] {
        let sub = Subheader {
            file: 0,
            channel: 0,
            submode: submode::DATA,
            coding: 0,
        };
        build_mode2_sector(lba, sub, &[lba as u8; 2048])
    }

    #[test]
    fn memory_source_reads_by_lba() {
        let mut src = MemorySource::from_sectors((0..10).map(data_sector));
        assert_eq!(src.sector_count(), 10);
        let mut one = [0u8; SECTOR_SIZE];
        src.read_sector(7, &mut one).unwrap();
        assert_eq!(Sector(&one).header().lba(), Some(7));
        let mut two = vec![0u8; 2 * SECTOR_SIZE];
        src.read_sectors(8, &mut two).unwrap();
        assert_eq!(two[SECTOR_SIZE + 24], 9);
        assert!(matches!(
            src.read_sectors(9, &mut two),
            Err(Error::OutOfRange { .. })
        ));
        assert!(matches!(
            src.read_sector(10, &mut one),
            Err(Error::OutOfRange { .. })
        ));
    }

    #[test]
    fn bin_source_honours_track_window() {
        let bytes: Vec<u8> = (0..6).flat_map(data_sector).collect();
        let mut src = BinSource::new(bytes.clone(), 2, Some(3)).unwrap();
        assert_eq!(src.sector_count(), 3);
        let mut one = [0u8; SECTOR_SIZE];
        src.read_sector(0, &mut one).unwrap();
        assert_eq!(Sector(&one).header().lba(), Some(2));
        assert!(BinSource::new(bytes.clone(), 2, Some(5)).is_err());
        let mut ragged = bytes;
        ragged.push(0);
        assert!(BinSource::whole(ragged).is_err());
    }

    #[test]
    fn not_ready_from_prefetched_file() {
        use crate::read_at::PrefetchedFile;
        use std::cell::RefCell;
        use std::rc::Rc;

        let bytes: Vec<u8> = (0..4).flat_map(data_sector).collect();
        let file = Rc::new(RefCell::new(PrefetchedFile::new(
            bytes.len() as u64,
            4096,
            1 << 20,
        )));
        let mut src = BinSource::whole(file.clone()).unwrap();
        let mut one = [0u8; SECTOR_SIZE];
        assert!(matches!(src.read_sector(3, &mut one), Err(Error::NotReady)));
        let missing = file.borrow_mut().take_missing();
        for range in missing {
            file.borrow_mut()
                .fill(
                    range.start,
                    &bytes[range.start as usize..range.end as usize],
                )
                .unwrap();
        }
        src.read_sector(3, &mut one).unwrap();
        assert_eq!(Sector(&one).header().lba(), Some(3));
    }
}
