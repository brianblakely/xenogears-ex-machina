//! Positioned byte access to the user's disc image files.

use std::cell::RefCell;
use std::collections::BTreeSet;
use std::fs::File;
use std::io::{self, Read, Seek, SeekFrom};
use std::ops::Range;
use std::rc::Rc;

use crate::cache::ByteLru;

/// Random access to the bytes of one image file (a CHD, a raw track).
///
/// `read_at` fills the whole buffer or fails; a read past the end fails with
/// `UnexpectedEof`. A source whose bytes have not arrived yet fails with
/// `WouldBlock` (see [`PrefetchedFile`]); the sector sources report that as
/// [`Error::NotReady`](crate::Error::NotReady).
pub trait ReadAt {
    fn read_at(&mut self, offset: u64, buf: &mut [u8]) -> io::Result<()>;
    fn len(&self) -> u64;
    fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

impl<T: ReadAt + ?Sized> ReadAt for &mut T {
    fn read_at(&mut self, offset: u64, buf: &mut [u8]) -> io::Result<()> {
        (**self).read_at(offset, buf)
    }
    fn len(&self) -> u64 {
        (**self).len()
    }
}

impl<T: ReadAt + ?Sized> ReadAt for Box<T> {
    fn read_at(&mut self, offset: u64, buf: &mut [u8]) -> io::Result<()> {
        (**self).read_at(offset, buf)
    }
    fn len(&self) -> u64 {
        (**self).len()
    }
}

/// A shared handle, so a single-threaded host can keep feeding a
/// [`PrefetchedFile`] that a sector source reads.
impl<T: ReadAt> ReadAt for Rc<RefCell<T>> {
    fn read_at(&mut self, offset: u64, buf: &mut [u8]) -> io::Result<()> {
        self.borrow_mut().read_at(offset, buf)
    }
    fn len(&self) -> u64 {
        self.borrow().len()
    }
}

fn check_range(offset: u64, size: usize, len: u64) -> io::Result<()> {
    match offset.checked_add(size as u64) {
        Some(end) if end <= len => Ok(()),
        _ => Err(io::Error::new(
            io::ErrorKind::UnexpectedEof,
            format!("read of {size} bytes at {offset} past the end ({len})"),
        )),
    }
}

/// Bytes held in memory (tests, small images).
impl ReadAt for Vec<u8> {
    fn read_at(&mut self, offset: u64, buf: &mut [u8]) -> io::Result<()> {
        check_range(offset, buf.len(), Vec::len(self) as u64)?;
        buf.copy_from_slice(&self[offset as usize..offset as usize + buf.len()]);
        Ok(())
    }
    fn len(&self) -> u64 {
        Vec::len(self) as u64
    }
}

/// A native file. Nothing is buffered here: the operating system caches pages.
pub struct FileReadAt {
    file: File,
    len: u64,
}

impl FileReadAt {
    pub fn new(file: File) -> io::Result<Self> {
        let len = file.metadata()?.len();
        Ok(Self { file, len })
    }

    pub fn open(path: impl AsRef<std::path::Path>) -> io::Result<Self> {
        Self::new(File::open(path)?)
    }
}

impl ReadAt for FileReadAt {
    fn read_at(&mut self, offset: u64, buf: &mut [u8]) -> io::Result<()> {
        check_range(offset, buf.len(), self.len)?;
        self.file.seek(SeekFrom::Start(offset))?;
        self.file.read_exact(buf)
    }
    fn len(&self) -> u64 {
        self.len
    }
}

/// A file whose bytes a host fetches asynchronously, in fixed-size chunks, into
/// a bounded cache: the browser's `File`/`Blob`, which a single-threaded page
/// can only read with promises.
///
/// Reads are synchronous. A read touching a chunk that is not resident fails
/// with `WouldBlock` and records the chunk; the host takes the recorded ranges
/// with [`take_missing`](Self::take_missing), fetches them (`Blob.slice(start,
/// end).arrayBuffer()`), hands them over with [`fill`](Self::fill) and retries
/// the read. Because one attempt reports every chunk that read touched and a
/// CHD hunk's compressed block is read with one call, a hunk normally needs one
/// round trip; [`request`](Self::request) queues chunks ahead of use (read-ahead
/// for streams). The budget must hold the chunks of the largest single read.
pub struct PrefetchedFile {
    len: u64,
    chunk: u64,
    chunks: ByteLru<u64>,
    missing: BTreeSet<u64>,
}

impl PrefetchedFile {
    /// `len` is the file size, `chunk_size` the fetch granularity in bytes and
    /// `budget` the bytes of chunks kept.
    pub fn new(len: u64, chunk_size: usize, budget: usize) -> Self {
        assert!(chunk_size > 0, "chunk size must be positive");
        Self {
            len,
            chunk: chunk_size as u64,
            chunks: ByteLru::new(budget),
            missing: BTreeSet::new(),
        }
    }

    pub fn chunk_size(&self) -> usize {
        self.chunk as usize
    }

    fn chunk_range(&self, index: u64) -> Range<u64> {
        let start = index * self.chunk;
        start..(start + self.chunk).min(self.len)
    }

    /// Queue the chunks covering `range` that are not resident.
    pub fn request(&mut self, range: Range<u64>) {
        let end = range.end.min(self.len);
        if range.start >= end {
            return;
        }
        for index in range.start / self.chunk..=(end - 1) / self.chunk {
            if !self.chunks.contains(&index) {
                self.missing.insert(index);
            }
        }
    }

    /// The queued byte ranges, merged and chunk-aligned, emptying the queue.
    pub fn take_missing(&mut self) -> Vec<Range<u64>> {
        let mut out: Vec<Range<u64>> = Vec::new();
        for index in std::mem::take(&mut self.missing) {
            let range = self.chunk_range(index);
            match out.last_mut() {
                Some(last) if last.end == range.start => last.end = range.end,
                _ => out.push(range),
            }
        }
        out
    }

    /// Store fetched bytes starting at a chunk boundary; they may span several
    /// chunks (as `take_missing` merges them), the last one ending at the file end.
    pub fn fill(&mut self, offset: u64, data: &[u8]) -> io::Result<()> {
        if !offset.is_multiple_of(self.chunk) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "fill must start at a chunk boundary",
            ));
        }
        let mut index = offset / self.chunk;
        let mut rest = data;
        while !rest.is_empty() {
            let range = self.chunk_range(index);
            let size = (range.end - range.start) as usize;
            if range.start >= self.len || rest.len() < size {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "fill must cover whole chunks",
                ));
            }
            self.chunks.insert(index, rest[..size].to_vec());
            self.missing.remove(&index);
            rest = &rest[size..];
            index += 1;
        }
        Ok(())
    }

    /// Bytes of chunks resident.
    pub fn resident_bytes(&self) -> usize {
        self.chunks.used()
    }
}

impl ReadAt for PrefetchedFile {
    fn read_at(&mut self, offset: u64, buf: &mut [u8]) -> io::Result<()> {
        check_range(offset, buf.len(), self.len)?;
        if buf.is_empty() {
            return Ok(());
        }
        let end = offset + buf.len() as u64;
        let (first, last) = (offset / self.chunk, (end - 1) / self.chunk);
        let absent: Vec<u64> = (first..=last)
            .filter(|i| !self.chunks.contains(i))
            .collect();
        if !absent.is_empty() {
            self.missing.extend(absent);
            return Err(io::Error::new(
                io::ErrorKind::WouldBlock,
                "image bytes not fetched yet",
            ));
        }
        for index in first..=last {
            let range = self.chunk_range(index);
            let data = self.chunks.get(&index).expect("resident chunk");
            let from = offset.max(range.start);
            let to = end.min(range.end);
            buf[(from - offset) as usize..(to - offset) as usize]
                .copy_from_slice(&data[(from - range.start) as usize..(to - range.start) as usize]);
        }
        Ok(())
    }
    fn len(&self) -> u64 {
        self.len
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn prefetched_file_reports_missing_chunks_then_reads() {
        let bytes: Vec<u8> = (0..1000u32).map(|n| n as u8).collect();
        let mut file = PrefetchedFile::new(bytes.len() as u64, 128, 1024);
        let mut buf = [0u8; 300];
        let err = file.read_at(100, &mut buf).unwrap_err();
        assert_eq!(err.kind(), io::ErrorKind::WouldBlock);
        let missing = file.take_missing();
        assert_eq!(missing, vec![0..512]);
        for range in missing {
            file.fill(
                range.start,
                &bytes[range.start as usize..range.end as usize],
            )
            .unwrap();
        }
        file.read_at(100, &mut buf).unwrap();
        assert_eq!(&buf[..], &bytes[100..400]);
        // The last chunk is short.
        file.request(990..1000);
        assert_eq!(file.take_missing(), vec![896..1000]);
        file.fill(896, &bytes[896..]).unwrap();
        let mut tail = [0u8; 10];
        file.read_at(990, &mut tail).unwrap();
        assert_eq!(&tail[..], &bytes[990..]);
        assert!(file.read_at(995, &mut tail).is_err());
    }

    #[test]
    fn prefetched_file_evicts_within_budget() {
        let bytes = vec![7u8; 4096];
        let mut file = PrefetchedFile::new(4096, 256, 512);
        file.fill(0, &bytes).unwrap();
        assert_eq!(file.resident_bytes(), 512);
        let mut buf = [0u8; 16];
        assert!(file.read_at(0, &mut buf).is_err());
        file.read_at(4080, &mut buf).unwrap();
    }
}
