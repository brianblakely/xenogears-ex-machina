//! Memory cards: two cards as raw 128 KB images (the `.mcd` layout) and the
//! BIOS's card file system over them, as src/port/bios.c and src/port/libcard.c call
//! it (src/port/include/xem/card.h).
//!
//! # The card format (psx-spx, "Memory Card Data Format")
//!
//! 16 blocks of 8 KB, each 64 frames of 128 bytes. Block 0 holds the header
//! frame ("MC"), one directory frame per data block (frames 1-15), the broken
//! sector list (frames 16-35) and a copy of the header (frame 63). A
//! directory frame holds the block's state (0x51 a file's first block, 0x52
//! a middle one, 0x53 its last, 0xA0 free, 0xA1-0xA3 deleted), the file size
//! (first block), the next block (0-14, 0xFFFF none), the name (0x0A, 21
//! bytes with the NUL) and an XOR checksum of bytes 0-0x7E at 0x7F.
//!
//! # BIOS semantics
//!
//! A device name is "buXY:" (port X, multitap slot Y; only slot 0 exists
//! here). `open` takes 1 read, 2 write and 0x200 create (blocks << 16 the
//! size, 1 block when 0), failing when a created file exists or an opened one
//! does not; reads and writes move whole 128-byte sectors at the file
//! position and fail past the file's end. `firstfile`/`nextfile` list the
//! files whose names match a pattern with `?` (one character) and `*` (the
//! rest); the device alone lists every file. `rename` keeps the blocks;
//! `delete` marks them deleted; `format` writes an empty file system.
//!
//! # Time
//!
//! Each access costs [`FRAME_CYCLES`] per frame it reads or writes (a 140-byte
//! card command at the controller port's 250 kHz): a file call keeps the card
//! busy that long and the port waits it out, as the BIOS blocks; `_card_info`
//! reads one frame and then raises the card interrupt with its event: done,
//! timeout when no card is inserted, or "new card" when the card holds no
//! file system (the game's file screen then offers to format it). Directory
//! listings come from the BIOS's directory copy and cost nothing.
//!
//! # Storage
//!
//! Images live in a [`CardStorage`]: files under a directory natively, memory
//! in tests, IndexedDB in browsers. Every call that changes a card writes its
//! image back. Card images are persistent saves, independent of a game-state
//! snapshot. Restoring game progress keeps the currently inserted cards.
//!
//! # Restoring over the inserted cards
//!
//! A state keeps the calls under way: the open files, a directory search and
//! a pending `_card_info`. Each open file carries what its calls depend on as
//! this device last left it (recorded at `open`, kept in step as its own
//! calls change the card): the directory frame of its first block and a
//! digest of its bytes, all of them for a file open only to read, those
//! before its position for one open to write (what it has written; it writes
//! the rest). The search carries the digest of the directory it lists.
//! Restored over the inserted cards, a file whose frame or bytes read
//! otherwise there is closed: its calls fail as on a removed card, and the
//! game takes its own retry or failure path instead of writing the rest of an
//! old save over whatever its block holds now (a newer save that reused it, a
//! later save's temporary file, another card) or reading the rest of a load
//! from a save written over it (the same save made again, whose frame reads
//! the same). A search over a directory that differs ends; a pending check
//! reports the card inserted now. Over the same card the state goes on as it
//! would have, also after a rewind inside a save: the bytes it wrote are
//! still there, and it writes the rest again.

use super::{Context, Device, unknown};
use crate::clock::CPU_HZ;
use crate::memory::GameMemory;
use crate::module::Action;
use crate::snapshot::{Reader, SnapshotError, Writer};
use sha2::{Digest, Sha256};

pub const CARD_SIZE: usize = 0x2_0000;
pub const BLOCK_SIZE: usize = 0x2000;
pub const FRAME_SIZE: usize = 0x80;
pub const BLOCKS: usize = 16;
/// CPU cycles to read or write one frame: a 140-byte command at 250 kHz.
pub const FRAME_CYCLES: u64 = CPU_HZ * 140 * 8 / 250_000;
/// The controller and memory card interrupt (XEM_IRQ_PAD); the detail is the
/// event spec.
pub const IRQ_CARD: u32 = 7;
pub const EVENT_DONE: u32 = 0x0004;
pub const EVENT_TIMEOUT: u32 = 0x0100;
pub const EVENT_NEW: u32 = 0x2000;

const STATE_FIRST: u32 = 0x51;
const STATE_MIDDLE: u32 = 0x52;
const STATE_LAST: u32 = 0x53;
const STATE_FREE: u32 = 0xA0;
const NAME_OFFSET: usize = 0x0A;
const NAME_LENGTH: usize = 20;
const FILES: usize = 16;
/// The BIOS's first file descriptor for a file (0 and 1 are the console).
const FIRST_FD: u32 = 2;
const SNAPSHOT_VERSION: u32 = 3;
/// The directory a search lists: block 0's header and directory frames.
const DIRECTORY_SIZE: usize = BLOCKS * FRAME_SIZE;

/// Where the card images persist.
pub trait CardStorage {
    /// The image in `slot` (0, 1), or None when no card is inserted there.
    fn load(&mut self, slot: usize) -> std::io::Result<Option<Vec<u8>>>;
    fn store(&mut self, slot: usize, image: &[u8]) -> std::io::Result<()>;
}

/// Images in memory (tests, hosts without storage).
#[derive(Default, Clone)]
pub struct MemoryStorage {
    pub images: [Option<Vec<u8>>; 2],
}

impl CardStorage for MemoryStorage {
    fn load(&mut self, slot: usize) -> std::io::Result<Option<Vec<u8>>> {
        Ok(self.images[slot].clone())
    }

    fn store(&mut self, slot: usize, image: &[u8]) -> std::io::Result<()> {
        self.images[slot] = Some(image.to_vec());
        Ok(())
    }
}

/// An empty formatted card.
pub fn formatted() -> Vec<u8> {
    let mut image = vec![0u8; CARD_SIZE];
    format_image(&mut image);
    image
}

fn format_image(image: &mut [u8]) {
    image[..BLOCK_SIZE].fill(0);
    image[0] = b'M';
    image[1] = b'C';
    for block in 1..BLOCKS {
        let frame = frame_mut(image, block);
        put_u32(frame, 0, STATE_FREE);
        put_u16(frame, 8, 0xFFFF);
    }
    for n in 16..36 {
        let frame = frame_mut(image, n);
        put_u32(frame, 0, 0xFFFF_FFFF);
        put_u16(frame, 8, 0xFFFF);
    }
    for n in (0..16).chain(16..36) {
        seal(frame_mut(image, n));
    }
    let header: Vec<u8> = image[..FRAME_SIZE].to_vec();
    frame_mut(image, 63).copy_from_slice(&header);
}

fn frame_mut(image: &mut [u8], n: usize) -> &mut [u8] {
    &mut image[n * FRAME_SIZE..(n + 1) * FRAME_SIZE]
}

fn frame(image: &[u8], n: usize) -> &[u8] {
    &image[n * FRAME_SIZE..(n + 1) * FRAME_SIZE]
}

fn put_u32(frame: &mut [u8], at: usize, v: u32) {
    frame[at..at + 4].copy_from_slice(&v.to_le_bytes());
}

fn put_u16(frame: &mut [u8], at: usize, v: u16) {
    frame[at..at + 2].copy_from_slice(&v.to_le_bytes());
}

fn get_u32(frame: &[u8], at: usize) -> u32 {
    u32::from_le_bytes(frame[at..at + 4].try_into().unwrap())
}

fn get_u16(frame: &[u8], at: usize) -> u16 {
    u16::from_le_bytes(frame[at..at + 2].try_into().unwrap())
}

/// Write the frame's checksum.
fn seal(frame: &mut [u8]) {
    frame[0x7F] = frame[..0x7F].iter().fold(0, |a, b| a ^ b);
}

fn is_formatted(image: &[u8]) -> bool {
    image[0] == b'M' && image[1] == b'C'
}

/// A directory entry's name.
fn entry_name(image: &[u8], block: usize) -> &[u8] {
    let raw = &frame(image, block)[NAME_OFFSET..NAME_OFFSET + 21];
    let end = raw.iter().position(|&b| b == 0).unwrap_or(raw.len());
    &raw[..end]
}

fn state(image: &[u8], block: usize) -> u32 {
    get_u32(frame(image, block), 0)
}

/// Block `block`'s directory frame on a card, or None without one.
fn directory_frame(image: Option<&[u8]>, block: usize) -> Option<[u8; FRAME_SIZE]> {
    image.map(|image| frame(image, block).try_into().expect("a frame"))
}

/// The digest of the directory a search lists (none without a card).
fn directory_digest(image: Option<&[u8]>) -> [u8; 32] {
    crate::snapshot::digest(image.map_or(&[][..], |image| &image[..DIRECTORY_SIZE]))
}

/// The digest of the bytes of `file` its calls depend on, as `image` holds
/// them: all of them for a file open only to read, those before its
/// position for one open to write (none without a card).
fn file_digest(image: Option<&[u8]>, file: &OpenFile) -> [u8; 32] {
    let mut hash = Sha256::new();
    if let Some(image) = image {
        let mut left = if file.flags & 2 == 0 { file.size } else { file.position } as usize;
        for block in chain(image, file.first) {
            let take = left.min(BLOCK_SIZE);
            hash.update(&image[block * BLOCK_SIZE..block * BLOCK_SIZE + take]);
            left -= take;
            if left == 0 {
                break;
            }
        }
    }
    hash.finalize().into()
}

/// The first block of the file named `name`.
fn find(image: &[u8], name: &[u8]) -> Option<usize> {
    (1..BLOCKS).find(|&b| state(image, b) == STATE_FIRST && entry_name(image, b) == name)
}

/// A file's blocks in order.
fn chain(image: &[u8], first: usize) -> Vec<usize> {
    let mut blocks = vec![first];
    let mut block = first;
    loop {
        let next = get_u16(frame(image, block), 8);
        if next == 0xFFFF || blocks.len() >= BLOCKS - 1 {
            return blocks;
        }
        block = next as usize + 1;
        if !(1..BLOCKS).contains(&block) || blocks.contains(&block) {
            return blocks;
        }
        blocks.push(block);
    }
}

/// The BIOS's name match: `?` any one character, `*` the rest; an empty
/// pattern matches every name.
fn matches(pattern: &[u8], name: &[u8]) -> bool {
    if pattern.is_empty() {
        return true;
    }
    let mut i = 0;
    for &p in pattern {
        if p == b'*' {
            return true;
        }
        match name.get(i) {
            Some(&c) if p == b'?' || p == c => i += 1,
            _ => return false,
        }
    }
    i == name.len()
}

/// "buXY:name" as (card slot, name); None for another device or slot.
fn parse(path: &[u8]) -> Option<(usize, &[u8])> {
    let colon = path.iter().position(|&b| b == b':')?;
    let (device, name) = (&path[..colon], &path[colon + 1..]);
    match device {
        b"bu00" => Some((0, name)),
        b"bu10" => Some((1, name)),
        _ => None,
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct OpenFile {
    slot: usize,
    first: usize,
    size: u32,
    position: u32,
    flags: u32,
    /// The directory frame of the file's first block and the digest of its
    /// bytes its calls depend on ([`file_digest`]) as this device last left
    /// them: a restored file stays open only where the card has them.
    frame: [u8; FRAME_SIZE],
    data: [u8; 32],
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct Search {
    slot: usize,
    pattern: Vec<u8>,
    next: usize,
    /// The digest of the directory it lists ([`directory_digest`]).
    directory: [u8; 32],
}

/// A pending `_card_info`: when it ends, the channel it checks and its event.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct Check {
    at: u64,
    channel: u32,
    event: u32,
}

#[derive(Default)]
pub struct Card {
    storage: Option<Box<dyn CardStorage>>,
    images: [Option<Vec<u8>>; 2],
    started: bool,
    files: [Option<OpenFile>; FILES],
    search: Option<Search>,
    busy_until: u64,
    check: Option<Check>,
    /// Storage errors, for the host to report.
    pub errors: Vec<String>,
    pending: [bool; 2],
}

impl Card {
    /// The device as a snapshot left it (`state`, loaded) over the inserted
    /// cards (see "Restoring over the inserted cards" above).
    pub(crate) fn restore_state(&mut self, mut state: Self) {
        state.storage = self.storage.take();
        state.images = std::mem::take(&mut self.images);
        state.pending = self.pending;
        state.errors = std::mem::take(&mut self.errors);
        let images = &state.images;
        for file in &mut state.files {
            let elsewhere = |f: &OpenFile| {
                let image = images[f.slot].as_deref();
                directory_frame(image, f.first) != Some(f.frame) || file_digest(image, f) != f.data
            };
            if file.as_ref().is_some_and(elsewhere) {
                *file = None;
            }
        }
        if state.search.as_ref().is_some_and(|s| directory_digest(state.images[s.slot].as_deref()) != s.directory) {
            state.search = None;
        }
        if let Some(check) = state.check {
            state.check = Some(Check { event: state.info_event(check.channel), ..check });
        }
        *self = state;
    }

    /// Keep what the open files and the search expect of `slot`'s card in
    /// step with a change this device made there (a file created, written,
    /// renamed or deleted, a writer's position moved, the card formatted or
    /// another inserted).
    fn card_changed(&mut self, slot: usize) {
        let image = self.images[slot].as_deref();
        for file in self.files.iter_mut().flatten().filter(|file| file.slot == slot) {
            file.frame = directory_frame(image, file.first).unwrap_or([0; FRAME_SIZE]);
            file.data = file_digest(image, file);
        }
        if let Some(search) = self.search.as_mut().filter(|search| search.slot == slot) {
            search.directory = directory_digest(image);
        }
    }

    /// Use `storage` and insert the cards it holds.
    pub fn attach_storage(&mut self, mut storage: Box<dyn CardStorage>) -> std::io::Result<()> {
        let mut images = [None, None];
        for slot in 0..2 {
            match storage.load(slot) {
                Ok(Some(image)) if image.len() == CARD_SIZE => images[slot] = Some(image),
                Ok(None) => {},
                Ok(Some(_)) => self.errors.push(format!("memory card {}: expected a raw 128 KiB image; slot left empty, file unchanged", slot + 1)),
                Err(error) => self.errors.push(format!("memory card {}: {error}; slot left empty, file unchanged", slot + 1)),
            }
        }
        self.images = images;
        self.storage = Some(storage);
        self.card_changed(0);
        self.card_changed(1);
        Ok(())
    }

    /// Insert a card image into `slot` (0, 1), or remove it with None.
    pub fn insert(&mut self, slot: usize, image: Option<Vec<u8>>) {
        self.images[slot] = image.map(|mut image| {
            image.resize(CARD_SIZE, 0);
            image
        });
        for file in &mut self.files {
            if file.as_ref().is_some_and(|f| f.slot == slot) {
                *file = None;
            }
        }
        self.card_changed(slot);
    }

    pub fn image(&self, slot: usize) -> Option<&[u8]> {
        self.images[slot].as_deref()
    }

    /// Import an exact original raw .mcd image without resizing or conversions.
    /// Do this between frames; replacing a card closes its file descriptors.
    pub fn import_image(&mut self, slot: usize, image: Vec<u8>) -> Result<(), String> {
        if slot > 1 || image.len() != CARD_SIZE { return Err("card slot is 0 or 1; image must be exactly 128 KiB (.mcd)".into()); }
        if let Some(storage) = &mut self.storage {
            storage.store(slot, &image).map_err(|e| e.to_string())?;
        }
        self.insert(slot, Some(image));
        self.pending[slot] = false;
        Ok(())
    }

    pub fn retry_writes(&mut self) -> Result<(), String> {
        self.errors.clear();
        for slot in 0..2 {
            if self.pending[slot] { self.persist(slot); }
        }
        if self.pending.iter().any(|p| *p) { Err(self.errors.join("; ")) } else { Ok(()) }
    }

    pub fn pending_writes(&self) -> usize { self.pending.iter().filter(|p| **p).count() }

    /// Write `slot`'s image back to storage; false when that failed.
    fn persist(&mut self, slot: usize) -> bool {
        let (Some(storage), Some(image)) = (self.storage.as_mut(), self.images[slot].as_ref()) else {
            self.pending[slot] = false;
            return true;
        };
        match storage.store(slot, image) {
            Ok(()) => { self.pending[slot] = false; true },
            Err(error) => {
                self.pending[slot] = true;
                let message = format!("memory card {}: {error}", slot + 1);
                if !self.errors.contains(&message) { self.errors.push(message); }
                false
            }
        }
    }

    fn busy(&mut self, now: u64, frames: u64) {
        self.busy_until = self.busy_until.max(now) + frames * FRAME_CYCLES;
    }

    fn formatted_image(&mut self, slot: usize) -> Option<&mut Vec<u8>> {
        self.images[slot].as_mut().filter(|image| is_formatted(image))
    }

    fn card_info(&mut self, channel: u32, now: u64) -> u32 {
        if !self.started || self.check.is_some() {
            return 0;
        }
        let event = self.info_event(channel);
        self.check = Some(Check { at: self.busy_until.max(now) + FRAME_CYCLES, channel, event });
        1
    }

    /// What `_card_info` on `channel` finds: a file system, a card without
    /// one, or no card.
    fn info_event(&self, channel: u32) -> u32 {
        let slot = (channel >> 4 & 1) as usize;
        match (&self.images[slot], channel & 0xF) {
            (Some(image), 0) if is_formatted(image) => EVENT_DONE,
            (Some(_), 0) => EVENT_NEW,
            _ => EVENT_TIMEOUT,
        }
    }

    fn open(&mut self, path: &[u8], flags: u32, now: u64) -> u32 {
        let Some((slot, name)) = parse(path) else { return u32::MAX };
        let name = name.to_vec();
        let Some(fd) = self.files.iter().position(Option::is_none) else { return u32::MAX };
        let Some(image) = self.formatted_image(slot) else { return u32::MAX };
        let first;
        if flags & 0x200 != 0 {
            if name.is_empty() || name.len() > NAME_LENGTH || find(image, &name).is_some() {
                return u32::MAX;
            }
            let count = ((flags >> 16) as usize).max(1);
            let free: Vec<usize> = (1..BLOCKS).filter(|&b| state(image, b) & 0xF0 == STATE_FREE).take(count).collect();
            if free.len() < count {
                return u32::MAX;
            }
            for (i, &block) in free.iter().enumerate() {
                let f = frame_mut(image, block);
                f.fill(0);
                let s = if i == 0 {
                    STATE_FIRST
                } else if i + 1 == count {
                    STATE_LAST
                } else {
                    STATE_MIDDLE
                };
                put_u32(f, 0, s);
                put_u32(f, 4, if i == 0 { (count * BLOCK_SIZE) as u32 } else { 0 });
                put_u16(f, 8, free.get(i + 1).map_or(0xFFFF, |&n| (n - 1) as u16));
                if i == 0 {
                    f[NAME_OFFSET..NAME_OFFSET + name.len()].copy_from_slice(&name);
                }
                seal(f);
            }
            first = free[0];
            self.card_changed(slot);
            self.busy(now, count as u64);
            if !self.persist(slot) {
                return u32::MAX;
            }
        } else {
            let Some(found) = find(image, &name) else { return u32::MAX };
            first = found;
        }
        let image = self.images[slot].as_deref();
        let frame = directory_frame(image, first).expect("a formatted card");
        let mut file = OpenFile { slot, first, size: get_u32(&frame, 4), position: 0, flags, frame, data: [0; 32] };
        file.data = file_digest(image, &file);
        self.files[fd] = Some(file);
        fd as u32 + FIRST_FD
    }

    fn file(&mut self, fd: u32) -> Option<&mut OpenFile> {
        let index = fd.checked_sub(FIRST_FD)? as usize;
        self.files.get_mut(index)?.as_mut()
    }

    /// The byte offsets in the card image of a file's `length` bytes from its
    /// position, sector by sector; None when they do not fit.
    fn sectors(&mut self, fd: u32, length: u32, mode: u32) -> Option<(usize, Vec<usize>)> {
        let file = self.file(fd)?.clone();
        if file.flags & mode == 0 || length == 0 || !length.is_multiple_of(FRAME_SIZE as u32) || file.position + length > file.size {
            return None;
        }
        let image = self.images[file.slot].as_ref()?;
        let blocks = chain(image, file.first);
        let mut offsets = Vec::new();
        for at in (file.position..file.position + length).step_by(FRAME_SIZE) {
            let block = *blocks.get(at as usize / BLOCK_SIZE)?;
            offsets.push(block * BLOCK_SIZE + at as usize % BLOCK_SIZE);
        }
        Some((file.slot, offsets))
    }

    fn read(&mut self, fd: u32, buffer: u32, length: u32, context: &mut Context<'_>) -> Result<u32, String> {
        let Some((slot, offsets)) = self.sectors(fd, length, 1) else { return Ok(u32::MAX) };
        let image = self.images[slot].as_ref().unwrap();
        let mut data = Vec::with_capacity(length as usize);
        for &at in &offsets {
            data.extend_from_slice(&image[at..at + FRAME_SIZE]);
        }
        context.memory.write(buffer, &data).map_err(|e| e.to_string())?;
        let file = self.file(fd).unwrap();
        file.position += length;
        if file.flags & 2 != 0 {
            // A file open to write depends on the bytes before its position.
            self.card_changed(slot);
        }
        self.busy(context.now, offsets.len() as u64);
        Ok(length)
    }

    fn write(&mut self, fd: u32, buffer: u32, length: u32, context: &mut Context<'_>) -> Result<u32, String> {
        let Some((slot, offsets)) = self.sectors(fd, length, 2) else { return Ok(u32::MAX) };
        let mut data = vec![0u8; length as usize];
        context.memory.read(buffer, &mut data).map_err(|e| e.to_string())?;
        let image = self.images[slot].as_mut().unwrap();
        for (chunk, &at) in data.chunks(FRAME_SIZE).zip(&offsets) {
            image[at..at + FRAME_SIZE].copy_from_slice(chunk);
        }
        self.file(fd).unwrap().position += length;
        self.card_changed(slot);
        self.busy(context.now, offsets.len() as u64);
        Ok(if self.persist(slot) { length } else { u32::MAX })
    }

    fn close(&mut self, fd: u32) -> u32 {
        match self.file(fd) {
            Some(_) => {
                self.files[(fd - FIRST_FD) as usize] = None;
                fd
            }
            None => u32::MAX,
        }
    }

    /// The next file of the search, written as a DIRENTRY at `entry`.
    fn next_match(&mut self, entry: u32, context: &mut Context<'_>) -> Result<u32, String> {
        let Some(search) = self.search.as_mut() else { return Ok(0) };
        let Some(image) = self.images[search.slot].as_ref().filter(|image| is_formatted(image)) else {
            return Ok(0);
        };
        while search.next < BLOCKS {
            let block = search.next;
            search.next += 1;
            if state(image, block) != STATE_FIRST || !matches(&search.pattern, entry_name(image, block)) {
                continue;
            }
            // struct DIRENTRY: name[20], attr, size, next, head, system[4].
            let mut d = [0u8; 40];
            let name = entry_name(image, block);
            let n = name.len().min(NAME_LENGTH - 1);
            d[..n].copy_from_slice(&name[..n]);
            d[20..24].copy_from_slice(&STATE_FIRST.to_le_bytes());
            d[24..28].copy_from_slice(&get_u32(frame(image, block), 4).to_le_bytes());
            d[32..36].copy_from_slice(&(block as u32).to_le_bytes());
            context.memory.write(entry, &d).map_err(|e| e.to_string())?;
            return Ok(1);
        }
        Ok(0)
    }

    fn rename(&mut self, from: &[u8], to: &[u8], now: u64) -> u32 {
        let (Some((slot, old)), Some((to_slot, new))) = (parse(from), parse(to)) else { return 0 };
        let (old, new) = (old.to_vec(), new.to_vec());
        let Some(image) = self.formatted_image(slot) else { return 0 };
        if slot != to_slot || new.is_empty() || new.len() > NAME_LENGTH || find(image, &new).is_some() {
            return 0;
        }
        let Some(block) = find(image, &old) else { return 0 };
        let f = frame_mut(image, block);
        f[NAME_OFFSET..NAME_OFFSET + 21].fill(0);
        f[NAME_OFFSET..NAME_OFFSET + new.len()].copy_from_slice(&new);
        seal(f);
        self.card_changed(slot);
        self.busy(now, 1);
        self.persist(slot) as u32
    }

    fn delete(&mut self, path: &[u8], now: u64) -> u32 {
        let Some((slot, name)) = parse(path) else { return 0 };
        let name = name.to_vec();
        let Some(image) = self.formatted_image(slot) else { return 0 };
        let Some(first) = find(image, &name) else { return 0 };
        let blocks = chain(image, first);
        for &block in &blocks {
            let f = frame_mut(image, block);
            let s = get_u32(f, 0);
            put_u32(f, 0, (s & 0x0F) | 0xA0);
            seal(f);
        }
        self.card_changed(slot);
        self.busy(now, blocks.len() as u64);
        self.persist(slot) as u32
    }

    fn format(&mut self, device: &[u8], now: u64) -> u32 {
        let Some((slot, _)) = parse(device) else { return 0 };
        let Some(image) = self.images[slot].as_mut() else { return 0 };
        format_image(image);
        for file in &mut self.files {
            if file.as_ref().is_some_and(|f| f.slot == slot) {
                *file = None;
            }
        }
        self.card_changed(slot);
        self.busy(now, 64);
        self.persist(slot) as u32
    }
}

/// A NUL-terminated string from game memory (at most 128 bytes).
fn read_string(memory: &dyn GameMemory, address: u32) -> Result<Vec<u8>, String> {
    let mut out = Vec::new();
    for i in 0..128 {
        let mut b = [0u8];
        memory.read(address + i, &mut b).map_err(|e| e.to_string())?;
        if b[0] == 0 {
            return Ok(out);
        }
        out.push(b[0]);
    }
    Err(format!("card file name at {address:#010x} is not terminated"))
}

impl Device for Card {
    fn import(&mut self, op: &str, args: &[u32], context: &mut Context<'_>) -> Action {
        let arg = |i: usize| args.get(i).copied().unwrap_or(0);
        let now = context.now;
        let result: Result<u32, String> = (|| {
            Ok(match op {
                "init" => {
                    self.started = false;
                    0
                }
                "start" => {
                    self.started = true;
                    0
                }
                "stop" => {
                    self.started = false;
                    0
                }
                "bu_init" => {
                    self.search = None;
                    0
                }
                "info" => self.card_info(arg(0), now),
                "busy" => (now < self.busy_until) as u32,
                "open" => {
                    let path = read_string(context.memory, arg(0))?;
                    self.open(&path, arg(1), now)
                }
                "read" => self.read(arg(0), arg(1), arg(2), context)?,
                "write" => self.write(arg(0), arg(1), arg(2), context)?,
                "close" => self.close(arg(0)),
                "firstfile" => {
                    let path = read_string(context.memory, arg(0))?;
                    self.search = parse(&path).map(|(slot, pattern)| Search {
                        slot,
                        pattern: pattern.to_vec(),
                        next: 1,
                        directory: directory_digest(self.images[slot].as_deref()),
                    });
                    self.next_match(arg(1), context)?
                }
                "nextfile" => self.next_match(arg(0), context)?,
                "rename" => {
                    let from = read_string(context.memory, arg(0))?;
                    let to = read_string(context.memory, arg(1))?;
                    self.rename(&from, &to, now)
                }
                "delete" => {
                    let path = read_string(context.memory, arg(0))?;
                    self.delete(&path, now)
                }
                "format" => {
                    let path = read_string(context.memory, arg(0))?;
                    self.format(&path, now)
                }
                _ => return Err(String::new()),
            })
        })();
        match result {
            Ok(value) => Action::Return(value),
            Err(reason) if reason.is_empty() => unknown("card", op, context),
            Err(reason) => {
                *context.trap_reason = Some(format!("card_{op}: {reason}"));
                Action::Trap
            }
        }
    }

    fn next_event(&self) -> Option<u64> {
        self.check.map(|check| check.at)
    }

    fn run_events(&mut self, context: &mut Context<'_>) {
        if let Some(check) = self.check
            && check.at <= context.now
        {
            self.check = None;
            context.raised.push((IRQ_CARD, check.event));
        }
    }

    fn save(&self) -> Vec<u8> {
        let mut w = Writer::default();
        w.u32(SNAPSHOT_VERSION);
        w.u32(self.started as u32);
        for file in &self.files {
            match file {
                Some(f) => {
                    w.u32(1);
                    for v in [f.slot as u32, f.first as u32, f.size, f.position, f.flags] {
                        w.u32(v);
                    }
                    w.bytes(&f.frame);
                    w.bytes(&f.data);
                }
                None => w.u32(0),
            }
        }
        match &self.search {
            Some(s) => {
                w.u32(1);
                w.u32(s.slot as u32);
                w.bytes(&s.pattern);
                w.u32(s.next as u32);
                w.bytes(&s.directory);
            }
            None => w.u32(0),
        }
        w.u64(self.busy_until);
        match self.check {
            Some(check) => {
                w.u32(1);
                w.u64(check.at);
                w.u32(check.channel);
                w.u32(check.event);
            }
            None => w.u32(0),
        }
        w.0
    }

    fn load(&mut self, state: &[u8]) -> Result<(), String> {
        type Loaded = (bool, [Option<OpenFile>; FILES], Option<Search>, u64, Option<Check>);
        let read = |state: &[u8]| -> Result<Loaded, SnapshotError> {
            let mut r = Reader::new(state);
            let found = r.u32()?;
            if found != SNAPSHOT_VERSION {
                return Err(SnapshotError::Version { what: "card device", found, expected: SNAPSHOT_VERSION });
            }
            let started = r.u32()? != 0;
            let mut files: [Option<OpenFile>; FILES] = Default::default();
            for file in &mut files {
                if r.u32()? != 0 {
                    let slot = r.u32()? as usize;
                    let first = r.u32()? as usize;
                    if slot > 1 || !(1..BLOCKS).contains(&first) {
                        return Err(SnapshotError::Format("card file"));
                    }
                    let (size, position, flags) = (r.u32()?, r.u32()?, r.u32()?);
                    let frame = r.bytes()?.try_into().map_err(|_| SnapshotError::Format("card file frame"))?;
                    let data = r.bytes()?.try_into().map_err(|_| SnapshotError::Format("card file data"))?;
                    *file = Some(OpenFile { slot, first, size, position, flags, frame, data });
                }
            }
            let search = if r.u32()? != 0 {
                let slot = r.u32()? as usize;
                let pattern = r.bytes()?.to_vec();
                let next = r.u32()? as usize;
                let directory = r.bytes()?.try_into().map_err(|_| SnapshotError::Format("card search directory"))?;
                if slot > 1 {
                    return Err(SnapshotError::Format("card search"));
                }
                Some(Search { slot, pattern, next, directory })
            } else {
                None
            };
            let busy_until = r.u64()?;
            let check = if r.u32()? != 0 { Some(Check { at: r.u64()?, channel: r.u32()?, event: r.u32()? }) } else { None };
            if !r.done() {
                return Err(SnapshotError::Format("trailing card device bytes"));
            }
            Ok((started, files, search, busy_until, check))
        };
        let (started, files, search, busy_until, check) = read(state).map_err(|e| e.to_string())?;
        self.started = started;
        self.files = files;
        self.search = search;
        self.busy_until = busy_until;
        self.check = check;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory::OutOfBounds;

    /// A page of game memory at 0x80010000.
    struct Memory(Vec<u8>);

    const BASE: u32 = 0x8001_0000;
    const NAME: u32 = BASE;
    const NAME2: u32 = BASE + 0x40;
    const ENTRY: u32 = BASE + 0x80;
    const BUFFER: u32 = BASE + 0x100;

    impl GameMemory for Memory {
        fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
            let start = address.checked_sub(BASE).ok_or(OutOfBounds { address, len: out.len() })? as usize;
            out.copy_from_slice(self.0.get(start..start + out.len()).ok_or(OutOfBounds { address, len: out.len() })?);
            Ok(())
        }
        fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
            let start = address.checked_sub(BASE).ok_or(OutOfBounds { address, len: data.len() })? as usize;
            self.0
                .get_mut(start..start + data.len())
                .ok_or(OutOfBounds { address, len: data.len() })?
                .copy_from_slice(data);
            Ok(())
        }
    }

    struct Bench {
        card: Card,
        memory: Memory,
        now: u64,
        raised: Vec<(u32, u32)>,
    }

    impl Bench {
        fn new(storage: MemoryStorage) -> Self {
            let mut card = Card::default();
            card.attach_storage(Box::new(storage)).unwrap();
            Bench { card, memory: Memory(vec![0; 0x4000]), now: 0, raised: Vec::new() }
        }

        fn call(&mut self, op: &str, args: &[u32]) -> u32 {
            let mut reason = None;
            let mut context =
                Context { memory: &mut self.memory, now: self.now, raised: &mut self.raised, trap_reason: &mut reason };
            let action = self.card.import(op, args, &mut context);
            assert_eq!(reason, None);
            match action {
                Action::Return(v) => v,
                other => panic!("{op}: {other:?}"),
            }
        }

        fn string(&mut self, at: u32, s: &str) -> u32 {
            let mut bytes = s.as_bytes().to_vec();
            bytes.push(0);
            self.memory.write(at, &bytes).unwrap();
            at
        }

        fn wait(&mut self) {
            self.now = self.now.max(self.card.busy_until);
            if let Some(at) = self.card.next_event() {
                self.now = self.now.max(at);
                let mut reason = None;
                let mut context = Context {
                    memory: &mut self.memory,
                    now: self.now,
                    raised: &mut self.raised,
                    trap_reason: &mut reason,
                };
                self.card.run_events(&mut context);
            }
        }

        fn list(&mut self, device: &str) -> Vec<(String, u32)> {
            let name = self.string(NAME, device);
            let mut out = Vec::new();
            let mut found = self.call("firstfile", &[name, ENTRY]);
            while found == 1 {
                let mut d = [0u8; 40];
                self.memory.read(ENTRY, &mut d).unwrap();
                let end = d[..20].iter().position(|&b| b == 0).unwrap();
                out.push((String::from_utf8(d[..end].to_vec()).unwrap(), get_u32(&d, 24)));
                found = self.call("nextfile", &[ENTRY]);
            }
            out
        }
    }

    fn storage_with_formatted_card() -> MemoryStorage {
        MemoryStorage { images: [Some(formatted()), None] }
    }

    /// The game's save (menu_framework.c's save command): its temporary
    /// file created and opened for writing; the caller writes and commits.
    fn begin_save(b: &mut Bench) -> u32 {
        let tmp = b.string(NAME, "bu00:__tmp_file");
        b.call("delete", &[tmp]);
        let created = b.call("open", &[tmp, 1 << 16 | 0x200]);
        b.call("close", &[created]);
        b.call("open", &[tmp, 2])
    }

    /// The save's 0x100-byte writes `sectors`, of `fill` mixed with each
    /// sector's number; what each returned.
    fn write_sectors(b: &mut Bench, fd: u32, sectors: std::ops::Range<u8>, fill: u8) -> Vec<u32> {
        sectors.map(|n| {
            b.memory.write(BUFFER, &[fill ^ n; 0x100]).unwrap();
            b.call("write", &[fd, BUFFER, 0x100])
        }).collect()
    }

    /// The load's 0x100-byte reads of `sectors`: what each returned; those
    /// that read find `fill` mixed with the sector's number.
    fn read_sectors(b: &mut Bench, fd: u32, sectors: std::ops::Range<u8>, fill: u8) -> Vec<u32> {
        sectors.map(|n| {
            b.memory.write(BUFFER, &[0; 0x100]).unwrap();
            let read = b.call("read", &[fd, BUFFER, 0x100]);
            if read == 0x100 {
                let mut back = [0u8; 0x100];
                b.memory.read(BUFFER, &mut back).unwrap();
                assert!(back.iter().all(|&x| x == fill ^ n), "sector {n} holds another save's bytes");
            }
            read
        }).collect()
    }

    /// The save's commit: close, then rename to its name.
    fn commit_save(b: &mut Bench, fd: u32) -> u32 {
        b.call("close", &[fd]);
        let tmp = b.string(NAME, "bu00:__tmp_file");
        let save = b.string(NAME2, "bu00:BASLUS-00664");
        b.call("rename", &[tmp, save])
    }

    /// Restore a saved device over the bench's inserted cards.
    fn restore(b: &mut Bench, state: &[u8]) {
        let mut loaded = Card::default();
        loaded.load(state).unwrap();
        b.card.restore_state(loaded);
    }

    #[test]
    fn a_restored_save_writes_on_over_the_same_card_and_never_over_another_file() {
        let mut b = Bench::new(storage_with_formatted_card());
        b.call("start", &[]);
        let fd = begin_save(&mut b);
        assert!(write_sectors(&mut b, fd, 0..4, 0xA0).iter().all(|&n| n == 0x100));
        let state = b.card.save();
        let card_then = b.card.image(0).unwrap().to_vec();
        assert!(write_sectors(&mut b, fd, 4..0x20, 0xA0).iter().all(|&n| n == 0x100));
        assert_eq!(commit_save(&mut b, fd), 1);
        let first_save = b.card.image(0).unwrap().to_vec();
        // A newer save to the same file takes the same block.
        let old = b.string(NAME2, "bu00:BASLUS-00664");
        assert_eq!(b.call("delete", &[old]), 1);
        let newer = begin_save(&mut b);
        assert!(write_sectors(&mut b, newer, 0..0x20, 0x5B).iter().all(|&n| n == 0x100));
        assert_eq!(commit_save(&mut b, newer), 1);
        let card_now = b.card.image(0).unwrap().to_vec();
        assert_eq!(card_now[BLOCK_SIZE + 0x1F00], 0x5B ^ 0x1F, "the newer save holds block 1");

        // Rewound into the first save's writes over the card that holds the
        // newer save: the old descriptor is closed and writes nothing.
        restore(&mut b, &state);
        assert_eq!(write_sectors(&mut b, fd, 4..0x20, 0xA0), vec![u32::MAX; 0x1C], "the rest of the old save is refused");
        assert_eq!(b.call("close", &[fd]), u32::MAX, "as on a removed card");
        assert_eq!(b.card.image(0).unwrap(), card_now, "the newer save is intact");
        assert_eq!(b.list("bu00:"), vec![("BASLUS-00664".to_string(), 0x2000)]);

        // Over the card as the state left it, the save goes on identically.
        b.card.import_image(0, card_then).unwrap();
        restore(&mut b, &state);
        assert_eq!(b.card.save(), state, "nothing of the state is dropped");
        assert!(write_sectors(&mut b, fd, 4..0x20, 0xA0).iter().all(|&n| n == 0x100));
        assert_eq!(commit_save(&mut b, fd), 1);
        assert_eq!(b.card.image(0).unwrap(), first_save);
    }

    #[test]
    fn a_file_the_device_changed_while_open_restores_over_the_same_card() {
        // The game's failure path after a refused first write closes fd 0
        // instead of its file and deletes the file it still has open.
        let mut b = Bench::new(storage_with_formatted_card());
        let fd = begin_save(&mut b);
        let tmp = b.string(NAME, "bu00:__tmp_file");
        assert_eq!(b.call("close", &[0]), u32::MAX);
        assert_eq!(b.call("delete", &[tmp]), 1);
        let state = b.card.save();
        restore(&mut b, &state);
        assert_eq!(b.card.save(), state, "the leaked descriptor stays open, as it would have");
        assert_eq!(b.call("close", &[fd]), fd);
    }

    #[test]
    fn a_restored_save_writes_on_where_its_bytes_are_and_never_over_a_later_save_in_its_block() {
        let mut b = Bench::new(storage_with_formatted_card());
        let fd = begin_save(&mut b);
        assert!(write_sectors(&mut b, fd, 0..4, 0xA0).iter().all(|&n| n == 0x100));
        let state = b.card.save();
        let temporary = frame(b.card.image(0).unwrap(), 1).to_vec();
        // Rewound after more of its writes, the save finds the bytes it had
        // written and writes on: the save it would have made.
        assert!(write_sectors(&mut b, fd, 4..10, 0xA0).iter().all(|&n| n == 0x100));
        restore(&mut b, &state);
        assert_eq!(b.card.save(), state, "nothing of the state is dropped");
        assert!(write_sectors(&mut b, fd, 4..0x20, 0xA0).iter().all(|&n| n == 0x100));
        assert_eq!(commit_save(&mut b, fd), 1);
        let mut straight = Bench::new(storage_with_formatted_card());
        let once = begin_save(&mut straight);
        write_sectors(&mut straight, once, 0..0x20, 0xA0);
        assert_eq!(commit_save(&mut straight, once), 1);
        assert_eq!(b.card.image(0), straight.card.image(0));

        // A later save deletes it, and its temporary file takes the block
        // with the same directory frame. Restored inside that save's writes,
        // the first save's writes are refused: the later save's bytes stay.
        let old = b.string(NAME2, "bu00:BASLUS-00664");
        assert_eq!(b.call("delete", &[old]), 1);
        let later = begin_save(&mut b);
        assert!(write_sectors(&mut b, later, 0..6, 0x5B).iter().all(|&n| n == 0x100));
        let card_now = b.card.image(0).unwrap().to_vec();
        assert_eq!(frame(&card_now, 1), &temporary[..], "the same directory frame");
        restore(&mut b, &state);
        assert_eq!(write_sectors(&mut b, fd, 4..0x20, 0xA0), vec![u32::MAX; 0x1C]);
        assert_eq!(b.card.image(0).unwrap(), card_now);
    }

    #[test]
    fn a_restored_load_reads_on_over_the_same_save_and_never_from_one_made_over_it() {
        let mut b = Bench::new(storage_with_formatted_card());
        let fd = begin_save(&mut b);
        write_sectors(&mut b, fd, 0..0x20, 0xA0);
        assert_eq!(commit_save(&mut b, fd), 1);
        let card_then = b.card.image(0).unwrap().to_vec();
        // The game's load: the save opened to read, a sector a call.
        let save = b.string(NAME2, "bu00:BASLUS-00664");
        let fd = b.call("open", &[save, 1]);
        assert!(read_sectors(&mut b, fd, 0..9, 0xA0).iter().all(|&n| n == 0x100));
        let state = b.card.save();
        // The same save made again: deleted, its temporary file in the freed
        // block, renamed back. The block's directory frame reads the same,
        // its bytes are the newer save's.
        assert_eq!(b.call("close", &[fd]), fd);
        assert_eq!(b.call("delete", &[save]), 1);
        let newer = begin_save(&mut b);
        write_sectors(&mut b, newer, 0..0x20, 0x5B);
        assert_eq!(commit_save(&mut b, newer), 1);
        let card_now = b.card.image(0).unwrap().to_vec();
        assert_eq!(frame(&card_now, 1), frame(&card_then, 1));
        assert_ne!(&card_now[BLOCK_SIZE..2 * BLOCK_SIZE], &card_then[BLOCK_SIZE..2 * BLOCK_SIZE]);

        // Rewound into the load over the newer save, its reads are refused
        // as on a removed card (the game shows its failed load, 3e) rather
        // than reading the rest from the newer save.
        restore(&mut b, &state);
        assert_eq!(read_sectors(&mut b, fd, 9..0x20, 0xA0), vec![u32::MAX; 0x17]);
        assert_eq!(b.card.image(0).unwrap(), card_now);

        // Over the card as the state left it, the load reads on.
        b.card.import_image(0, card_then).unwrap();
        restore(&mut b, &state);
        assert_eq!(b.card.save(), state, "nothing of the state is dropped");
        assert!(read_sectors(&mut b, fd, 9..0x20, 0xA0).iter().all(|&n| n == 0x100));
    }

    #[test]
    fn a_restored_listing_and_check_follow_the_inserted_card() {
        let mut b = Bench::new(storage_with_formatted_card());
        b.call("start", &[]);
        for name in ["bu00:ONE", "bu00:TWO"] {
            let path = b.string(NAME, name);
            let fd = b.call("open", &[path, 1 << 16 | 0x200]);
            b.call("close", &[fd]);
        }
        let device = b.string(NAME, "bu00:");
        assert_eq!(b.call("firstfile", &[device, ENTRY]), 1);
        assert_eq!(b.call("info", &[0]), 1);
        let state = b.card.save();
        // Over the same card the listing goes on and the check finds it.
        restore(&mut b, &state);
        assert_eq!(b.card.save(), state);
        assert_eq!(b.call("nextfile", &[ENTRY]), 1);
        assert_eq!(&b.memory.0[(ENTRY - BASE) as usize..][..4], b"TWO\0");
        b.wait();
        assert_eq!(b.raised.last(), Some(&(IRQ_CARD, EVENT_DONE)));
        // Over a card with other files the listing ends; without a card the
        // check reports none.
        b.card.insert(0, Some(formatted()));
        restore(&mut b, &state);
        assert_eq!(b.call("nextfile", &[ENTRY]), 0);
        b.card.insert(0, None);
        restore(&mut b, &state);
        b.wait();
        assert_eq!(b.raised.last(), Some(&(IRQ_CARD, EVENT_TIMEOUT)));
    }

    #[test]
    fn a_formatted_card_has_valid_checksums_and_no_files() {
        let image = formatted();
        assert_eq!(&image[..2], b"MC");
        for n in 0..64 {
            let f = frame(&image, n);
            if n < 36 || n == 63 {
                assert_eq!(f[0x7F], f[..0x7F].iter().fold(0, |a, b| a ^ b), "frame {n}");
            }
        }
        assert_eq!(state(&image, 1), STATE_FREE);
        assert_eq!(frame(&image, 0)[0x7F], b'M' ^ b'C');
    }

    #[test]
    fn card_info_reports_done_timeout_and_new_after_a_frame() {
        let mut storage = storage_with_formatted_card();
        storage.images[1] = Some(vec![0; CARD_SIZE]);
        let mut b = Bench::new(storage);
        assert_eq!(b.call("info", &[0]), 0, "not started");
        b.call("start", &[]);
        assert_eq!(b.call("info", &[0]), 1);
        assert_eq!(b.call("info", &[0]), 0, "one check at a time");
        assert_eq!(b.card.next_event(), Some(FRAME_CYCLES));
        b.wait();
        assert_eq!(b.raised, vec![(IRQ_CARD, EVENT_DONE)]);
        b.call("info", &[0x10]);
        b.wait();
        assert_eq!(b.raised[1], (IRQ_CARD, EVENT_NEW));
        b.card.insert(1, None);
        b.call("info", &[0x10]);
        b.wait();
        assert_eq!(b.raised[2], (IRQ_CARD, EVENT_TIMEOUT));
    }

    #[test]
    fn the_games_save_sequence_commits_by_rename() {
        let mut b = Bench::new(storage_with_formatted_card());
        b.call("start", &[]);
        let tmp = b.string(NAME, "bu00:__tmp_file");
        assert_eq!(b.call("delete", &[tmp]), 0, "nothing to erase yet");
        let fd = b.call("open", &[tmp, 1 << 16 | 0x200]);
        assert_eq!(fd, 2);
        assert_eq!(b.card.busy_until, FRAME_CYCLES);
        assert_eq!(b.call("busy", &[]), 1);
        b.wait();
        assert_eq!(b.call("busy", &[]), 0);
        assert_eq!(b.call("open", &[tmp, 1 << 16 | 0x200]), u32::MAX, "exists");
        assert_eq!(b.call("close", &[fd]), fd);
        let fd = b.call("open", &[tmp, 2]);
        for i in 0..0x20u8 {
            b.memory.write(BUFFER, &[i; 0x100]).unwrap();
            assert_eq!(b.call("write", &[fd, BUFFER, 0x100]), 0x100);
        }
        assert_eq!(b.call("write", &[fd, BUFFER, 0x80]), u32::MAX, "past the end");
        assert_eq!(b.call("read", &[fd, BUFFER, 0x80]), u32::MAX, "write-only");
        b.call("close", &[fd]);
        let save = b.string(NAME2, "bu00:BASLUS-00664:");
        assert_eq!(b.call("rename", &[tmp, save]), 1);
        assert_eq!(b.list("bu00:"), vec![("BASLUS-00664:".to_string(), 0x2000)]);
        assert_eq!(b.list("bu00:BASLUS-00664?"), vec![("BASLUS-00664:".to_string(), 0x2000)]);
        assert_eq!(b.list("bu00:BAS*"), vec![("BASLUS-00664:".to_string(), 0x2000)]);
        assert!(b.list("bu00:__tmp_file").is_empty());

        // Read it back as the load does.
        let fd = b.call("open", &[save, 1]);
        assert_eq!(b.call("read", &[fd, BUFFER, 0x200]), 0x200);
        let mut back = [0u8; 0x200];
        b.memory.read(BUFFER, &mut back).unwrap();
        assert!(back[..0x100].iter().all(|&x| x == 0) && back[0x100..].iter().all(|&x| x == 1));
        b.call("close", &[fd]);

        // The image reached storage, with a well-formed directory frame.
        let image = b.card.image(0).unwrap().to_vec();
        let f = frame(&image, 1);
        assert_eq!(get_u32(f, 0), STATE_FIRST);
        assert_eq!(get_u16(f, 8), 0xFFFF);
        assert_eq!(f[0x7F], f[..0x7F].iter().fold(0, |a, b| a ^ b));
        assert_eq!(image[BLOCK_SIZE + 0x1F00], 0x1F);

        assert_eq!(b.call("delete", &[save]), 1);
        assert!(b.list("bu00:").is_empty());
        assert_eq!(state(b.card.image(0).unwrap(), 1), 0xA1);
    }

    #[test]
    fn multi_block_files_chain_their_blocks() {
        let mut b = Bench::new(storage_with_formatted_card());
        let one = b.string(NAME, "bu00:ONE");
        let fd = b.call("open", &[one, 1 << 16 | 0x200]);
        b.call("close", &[fd]);
        let three = b.string(NAME2, "bu00:THREE");
        let fd = b.call("open", &[three, 3 << 16 | 0x200]);
        b.call("close", &[fd]);
        let image = b.card.image(0).unwrap();
        assert_eq!(chain(image, 2), vec![2, 3, 4]);
        assert_eq!([state(image, 2), state(image, 3), state(image, 4)], [STATE_FIRST, STATE_MIDDLE, STATE_LAST]);
        let fd = b.call("open", &[three, 3]);
        b.memory.write(BUFFER, &[9; 0x100]).unwrap();
        // Seek across a block boundary by writing whole sectors.
        for _ in 0..0x20 {
            b.call("write", &[fd, BUFFER, 0x100]);
        }
        b.memory.write(BUFFER, &[5; 0x100]).unwrap();
        b.call("write", &[fd, BUFFER, 0x100]);
        assert_eq!(b.card.image(0).unwrap()[3 * BLOCK_SIZE], 5);
        assert_eq!(b.list("bu00:"), vec![("ONE".into(), 0x2000), ("THREE".into(), 0x6000)]);
    }

    #[test]
    fn an_unformatted_card_formats() {
        let mut b = Bench::new(MemoryStorage { images: [Some(vec![0xFF; CARD_SIZE]), None] });
        let device = b.string(NAME, "bu00:");
        assert!(b.list("bu00:").is_empty());
        assert_eq!(b.call("format", &[device]), 1);
        // Formatting writes block 0, the file system; the data blocks keep their bytes.
        assert_eq!(&b.card.image(0).unwrap()[..BLOCK_SIZE], &formatted()[..BLOCK_SIZE]);
        assert_eq!(b.card.image(0).unwrap()[BLOCK_SIZE], 0xFF);
        let other = b.string(NAME2, "bu10:");
        assert_eq!(b.call("format", &[other]), 0, "no card in port 2");
    }

    #[test]
    fn save_and_load_restore_the_device() {
        let mut b = Bench::new(storage_with_formatted_card());
        b.call("start", &[]);
        let tmp = b.string(NAME, "bu00:FILE");
        let fd = b.call("open", &[tmp, 1 << 16 | 0x200 | 2]);
        b.call("info", &[0]);
        let first = b.string(NAME2, "bu00:");
        b.call("firstfile", &[first, ENTRY]);
        let state = b.card.save();
        let mut other = Card::default();
        other.insert(0, b.card.image(0).map(<[u8]>::to_vec));
        other.load(&state).unwrap();
        assert_eq!(other.save(), state);
        assert_eq!(other.next_event(), b.card.next_event());
        assert_eq!(other.image(0), b.card.image(0));
        assert!(other.files[(fd - FIRST_FD) as usize].is_some());
        assert!(other.load(&state[..state.len() - 2]).is_err());
    }
}
