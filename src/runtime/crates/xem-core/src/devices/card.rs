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
//! image back. Snapshots hold the images, so a restore brings back the cards
//! as they were; they reach storage at the next change.

use std::path::PathBuf;

use super::{Context, Device, unknown};
use crate::clock::CPU_HZ;
use crate::memory::GameMemory;
use crate::module::Action;
use crate::snapshot::{Reader, SnapshotError, Writer};

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
const SNAPSHOT_VERSION: u32 = 1;

/// Where the card images persist.
pub trait CardStorage {
    /// The image in `slot` (0, 1), or None when no card is inserted there.
    fn load(&mut self, slot: usize) -> std::io::Result<Option<Vec<u8>>>;
    fn store(&mut self, slot: usize, image: &[u8]) -> std::io::Result<()>;
}

/// Images as files `card1.mcd` and `card2.mcd` in a directory; a missing
/// file is a fresh formatted card.
pub struct FileStorage {
    pub dir: PathBuf,
}

impl FileStorage {
    pub fn path(&self, slot: usize) -> PathBuf {
        self.dir.join(format!("card{}.mcd", slot + 1))
    }
}

impl CardStorage for FileStorage {
    fn load(&mut self, slot: usize) -> std::io::Result<Option<Vec<u8>>> {
        match std::fs::read(self.path(slot)) {
            Ok(image) => Ok(Some(image)),
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(Some(formatted())),
            Err(error) => Err(error),
        }
    }

    fn store(&mut self, slot: usize, image: &[u8]) -> std::io::Result<()> {
        std::fs::create_dir_all(&self.dir)?;
        let path = self.path(slot);
        let partial = path.with_extension("mcd.partial");
        std::fs::write(&partial, image)?;
        std::fs::rename(partial, path)
    }
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
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct Search {
    slot: usize,
    pattern: Vec<u8>,
    next: usize,
}

#[derive(Default)]
pub struct Card {
    storage: Option<Box<dyn CardStorage>>,
    images: [Option<Vec<u8>>; 2],
    started: bool,
    files: [Option<OpenFile>; FILES],
    search: Option<Search>,
    busy_until: u64,
    /// A pending `_card_info`: when it ends and its event.
    check: Option<(u64, u32)>,
    /// Storage errors, for the host to report.
    pub errors: Vec<String>,
}

impl Card {
    /// Use `storage` and insert the cards it holds.
    pub fn attach_storage(&mut self, mut storage: Box<dyn CardStorage>) -> std::io::Result<()> {
        for slot in 0..2 {
            self.images[slot] = storage.load(slot)?.map(|mut image| {
                image.resize(CARD_SIZE, 0);
                image
            });
        }
        self.storage = Some(storage);
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
    }

    pub fn image(&self, slot: usize) -> Option<&[u8]> {
        self.images[slot].as_deref()
    }

    /// Write `slot`'s image back to storage; false when that failed.
    fn persist(&mut self, slot: usize) -> bool {
        let (Some(storage), Some(image)) = (self.storage.as_mut(), self.images[slot].as_ref()) else {
            return true;
        };
        match storage.store(slot, image) {
            Ok(()) => true,
            Err(error) => {
                self.errors.push(format!("memory card {}: {error}", slot + 1));
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
        let slot = (channel >> 4 & 1) as usize;
        let event = match (&self.images[slot], channel & 0xF) {
            (Some(image), 0) if is_formatted(image) => EVENT_DONE,
            (Some(_), 0) => EVENT_NEW,
            _ => EVENT_TIMEOUT,
        };
        self.check = Some((self.busy_until.max(now) + FRAME_CYCLES, event));
        1
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
            self.busy(now, count as u64);
            if !self.persist(slot) {
                return u32::MAX;
            }
        } else {
            let Some(found) = find(image, &name) else { return u32::MAX };
            first = found;
        }
        let image = self.images[slot].as_ref().unwrap();
        let size = get_u32(frame(image, first), 4);
        self.files[fd] = Some(OpenFile { slot, first, size, position: 0, flags });
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
        self.file(fd).unwrap().position += length;
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
                    self.search = parse(&path).map(|(slot, pattern)| Search { slot, pattern: pattern.to_vec(), next: 1 });
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
        self.check.map(|(at, _)| at)
    }

    fn run_events(&mut self, context: &mut Context<'_>) {
        if let Some((at, event)) = self.check
            && at <= context.now
        {
            self.check = None;
            context.raised.push((IRQ_CARD, event));
        }
    }

    fn save(&self) -> Vec<u8> {
        let mut w = Writer::default();
        w.u32(SNAPSHOT_VERSION);
        for image in &self.images {
            match image {
                Some(image) => {
                    w.u32(1);
                    w.memory(image);
                }
                None => w.u32(0),
            }
        }
        w.u32(self.started as u32);
        for file in &self.files {
            match file {
                Some(f) => {
                    w.u32(1);
                    for v in [f.slot as u32, f.first as u32, f.size, f.position, f.flags] {
                        w.u32(v);
                    }
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
            }
            None => w.u32(0),
        }
        w.u64(self.busy_until);
        match self.check {
            Some((at, event)) => {
                w.u32(1);
                w.u64(at);
                w.u32(event);
            }
            None => w.u32(0),
        }
        w.0
    }

    fn load(&mut self, state: &[u8]) -> Result<(), String> {
        type Loaded = ([Option<Vec<u8>>; 2], bool, [Option<OpenFile>; FILES], Option<Search>, u64, Option<(u64, u32)>);
        let read = |state: &[u8]| -> Result<Loaded, SnapshotError> {
            let mut r = Reader::new(state);
            if r.u32()? != SNAPSHOT_VERSION {
                return Err(SnapshotError::Format("card device version"));
            }
            let mut images: [Option<Vec<u8>>; 2] = Default::default();
            for image in &mut images {
                if r.u32()? != 0 {
                    let data = r.memory()?;
                    if data.len() != CARD_SIZE {
                        return Err(SnapshotError::Format("card image size"));
                    }
                    *image = Some(data);
                }
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
                    *file = Some(OpenFile { slot, first, size: r.u32()?, position: r.u32()?, flags: r.u32()? });
                }
            }
            let search = if r.u32()? != 0 {
                let slot = r.u32()? as usize;
                let pattern = r.bytes()?.to_vec();
                let next = r.u32()? as usize;
                if slot > 1 {
                    return Err(SnapshotError::Format("card search"));
                }
                Some(Search { slot, pattern, next })
            } else {
                None
            };
            let busy_until = r.u64()?;
            let check = if r.u32()? != 0 { Some((r.u64()?, r.u32()?)) } else { None };
            if !r.done() {
                return Err(SnapshotError::Format("trailing card device bytes"));
            }
            Ok((images, started, files, search, busy_until, check))
        };
        let (images, started, files, search, busy_until, check) = read(state).map_err(|e| e.to_string())?;
        self.images = images;
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
    fn file_storage_round_trips_and_starts_with_a_formatted_card() {
        let dir = std::env::temp_dir().join(format!("xem-card-test-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        let mut card = Card::default();
        card.attach_storage(Box::new(FileStorage { dir: dir.clone() })).unwrap();
        assert_eq!(card.image(0).unwrap(), &formatted()[..]);
        assert!(card.persist(0));
        assert_eq!(std::fs::read(dir.join("card1.mcd")).unwrap(), formatted());
        std::fs::remove_dir_all(dir).unwrap();
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
        other.load(&state).unwrap();
        assert_eq!(other.save(), state);
        assert_eq!(other.next_event(), b.card.next_event());
        assert_eq!(other.image(0), b.card.image(0));
        assert!(other.files[(fd - FIRST_FD) as usize].is_some());
        assert!(other.load(&state[..state.len() - 2]).is_err());
    }
}
