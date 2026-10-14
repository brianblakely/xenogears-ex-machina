//! Original saves in the files players bring and take: whole memory cards
//! and single saves in the containers emulators and save tools write, and
//! what a card holds as the BIOS and the game see it. Pure functions over
//! bytes; the card device ([`super::card::Card`]) applies them to the
//! inserted cards between frames.
//!
//! # Containers
//!
//! The layouts are psx-spx's ("Memory Card Images") and, for the PSP's and
//! the PS3's, those of the tools that write them. [`detect`] tells them
//! apart by size and magic, and names a raw single save by its file name
//! (its directory name):
//!
//! | Container | Extensions | Layout | Here |
//! |---|---|---|---|
//! | Raw card | .mcd .mcr .mc .srm .mem .bin .vm1 .ddf .ps | 131 072 bytes beginning "MC" | import, export |
//! | DexDrive | .gme | a 0xF40-byte header beginning "123-456-STD", then the card | import, export |
//! | Connectix VGS | .vgs .mem | a 64-byte header beginning "VgsM", then the card | import |
//! | PSP, PS Vita | .vmp | a 0x80-byte header beginning 00 "PMV", then the card | import |
//! | Single save | .mcs .ps1 | the 0x80-byte directory frame of its first block, then its blocks | import, export |
//! | PS3 | .psv | a 0x84-byte header beginning 00 "VSP" (type 1, a PS1 save, at 0x3C, its name at 0x64), then its blocks | import |
//! | Action Replay, GameShark, Xploder, Caetla | .psx .mcb .mcx .pda | a 54-byte header (its name at 0, a title at 0x15), then its blocks | import |
//! | Raw single save | its directory name | its blocks | import |
//!
//! A single save's blocks begin with its title frame ("SC"). A PSP's or a
//! PS3's file carries a signature over its contents: it is not checked on
//! import, and those files are not written (signing is not implemented).
//!
//! # What a card holds
//!
//! [`list`] reads a card's directory as the BIOS does and checks it: the
//! header frame, each directory frame's checksum, each file's chain of
//! blocks and its size, and its title frame (psx-spx "Memory Card Data
//! Format": "SC", the icon's frame count 0x11-0x13, the block count and the
//! Shift-JIS title). For this game's saves it adds what the game itself
//! checks (`src/decomp/src/slot39/menu_framework.c`):
//! - its own files are those whose names begin with the twelve characters
//!   "BASLUS-00664" (`menu_card_mark_own_saves`, on both discs), each the
//!   file its digit names (first block offset 0x123; the game has 15 files,
//!   digits 0-14, `menu_save_choose_digit`);
//! - its load sums the payload (block offsets 0x100-0x1FFE) to a byte and
//!   takes the save only when that is the byte at 0x1FFF (`READ_SAVE`;
//!   otherwise "Load failed", message 3e);
//! - after a load from the title it asks for disc vars\[82\] + 1
//!   (`menu_main`; vars\[82\] at block offset 0x12D8,
//!   `menu_save_build_payload`).
//!
//! Japanese Xenogears saves are told by the name prefix "BISLPS-01160",
//! which is inferred: the North American program keeps "BASLUS-01160" as
//! a second prefix that no code reads (`menu_load_resources`), the Japanese
//! first disc's serial under its own region code. No Japanese disc or save
//! was at hand to confirm it.

use super::card::{self, BLOCK_SIZE, BLOCKS, CARD_SIZE, FRAME_SIZE, NAME_LENGTH, NAME_OFFSET};
use serde::{Deserialize, Serialize};

const GME_HEADER: usize = 0xF40;
const VGS_HEADER: usize = 0x40;
const VMP_HEADER: usize = 0x80;
const PSV_HEADER: usize = 0x84;
/// A PS3 save's type (1: a PS1 save) and name.
const PSV_TYPE: usize = 0x3C;
const PSV_NAME: usize = 0x64;
const AR_HEADER: usize = 54;
/// The largest file a supported container holds (a DexDrive card).
pub const MAX_FILE: usize = GME_HEADER + CARD_SIZE;

/// This game's save names begin so, then its file digit's character.
pub const XENOGEARS_PREFIX: &[u8] = b"BASLUS-00664";
/// The Japanese release's prefix (inferred; see the module's notes).
pub const JAPANESE_XENOGEARS_PREFIX: &[u8] = b"BISLPS-01160";
/// In this game's save's first block: its file digit (the payload's +0x23),
/// its payload and the checksum byte after it, and vars[82] (the disc less
/// one).
const DIGIT: usize = 0x123;
const PAYLOAD: usize = 0x100;
const SUM: usize = PAYLOAD + 0x1EFF;
const DISC_VARIABLE: usize = 0x12D8;
/// The game's files: digits 0-14.
const GAME_FILES: u8 = 15;
/// A title frame: "SC", the icon flag, the block count, the title.
const TITLE: std::ops::Range<usize> = 4..0x44;

/// A file's container.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Container {
    Raw,
    Gme,
    Vgs,
    Vmp,
    Mcs,
    Psv,
    ActionReplay,
    RawSave,
}

impl Container {
    pub fn describe(self) -> &'static str {
        match self {
            Container::Raw => "a raw memory card image",
            Container::Gme => "a DexDrive card (.gme)",
            Container::Vgs => "a Connectix VGS card (.vgs, .mem)",
            Container::Vmp => "a PSP or PS Vita card (.vmp)",
            Container::Mcs => "a single save (.mcs, .ps1)",
            Container::Psv => "a PS3 single save (.psv)",
            Container::ActionReplay => "an Action Replay, GameShark, Xploder or Caetla save (.psx, .mcb, .mcx, .pda)",
            Container::RawSave => "a raw single save",
        }
    }
}

/// The containers a whole card is written in.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum CardFormat {
    /// A raw image (.mcd).
    #[default]
    Raw,
    /// A DexDrive file (.gme).
    Gme,
}

impl CardFormat {
    pub fn extension(self) -> &'static str {
        match self {
            CardFormat::Raw => "mcd",
            CardFormat::Gme => "gme",
        }
    }
}

/// A save as a file holds it: its directory name and its blocks (whole
/// 8 KiB blocks, the title frame first).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Save {
    pub name: Vec<u8>,
    pub data: Vec<u8>,
}

/// What a file holds.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Contents {
    /// A raw 128 KiB card.
    Card(Vec<u8>),
    Save(Save),
}

/// What `bytes`, a file named `file_name` (its directories are ignored),
/// holds: by its size and magic, or for a raw single save by its name.
pub fn detect(bytes: &[u8], file_name: &str) -> Result<(Container, Contents), String> {
    let file_name = base_name(file_name);
    let size = bytes.len();
    let card = |container: Container, header: usize| {
        if size == header + CARD_SIZE && bytes[header..].starts_with(b"MC") {
            Ok((container, Contents::Card(bytes[header..].to_vec())))
        } else {
            Err(format!(
                "{file_name} begins as {} but is not one: that is {} bytes with a card beginning \"MC\" after its header, this file {size} bytes",
                container.describe(),
                header + CARD_SIZE
            ))
        }
    };
    if size == CARD_SIZE && bytes.starts_with(b"MC") {
        return Ok((Container::Raw, Contents::Card(bytes.to_vec())));
    }
    if bytes.starts_with(b"123-456-STD") {
        return card(Container::Gme, GME_HEADER);
    }
    if bytes.starts_with(b"VgsM") {
        return card(Container::Vgs, VGS_HEADER);
    }
    if bytes.starts_with(b"\0PMV") {
        return card(Container::Vmp, VMP_HEADER);
    }
    let save = |container: Container, header: usize, name: Result<Vec<u8>, String>| -> Result<(Container, Contents), String> {
        let data = blocks_after(bytes, header).ok_or_else(|| {
            format!(
                "{file_name} begins as {} but is not one: that is a header of {header} bytes, then 1-15 blocks of 8 KiB beginning \"SC\", this file {size} bytes",
                container.describe()
            )
        })?;
        let name = name.map_err(|error| format!("{file_name} ({}): {error}", container.describe()))?;
        Ok((container, Contents::Save(Save { name, data })))
    };
    if bytes.starts_with(b"\0VSP") {
        let kind = bytes.get(PSV_TYPE).copied().unwrap_or(0);
        if kind != 1 {
            return Err(format!("{file_name} is a PS3 save of type {kind}, not of a PS1 game (type 1)"));
        }
        return save(Container::Psv, PSV_HEADER, field_name(bytes.get(PSV_NAME..PSV_NAME + NAME_LENGTH)));
    }
    if bytes.first() == Some(&(card::STATE_FIRST as u8)) && blocks_after(bytes, FRAME_SIZE).is_some() {
        return save(Container::Mcs, FRAME_SIZE, field_name(bytes.get(NAME_OFFSET..NAME_OFFSET + NAME_LENGTH + 1)));
    }
    if blocks_after(bytes, AR_HEADER).is_some() {
        return save(Container::ActionReplay, AR_HEADER, field_name(bytes.get(..NAME_LENGTH + 1)));
    }
    if blocks_after(bytes, 0).is_some() {
        return save(Container::RawSave, 0, raw_save_name(file_name));
    }
    Err(format!(
        "{file_name} ({size} bytes) is not a memory card or a save this host reads: a card (raw, DexDrive, VGS, PSP), \
         a single save (.mcs, .ps1, .psv, .psx, .mcb, .mcx, .pda) or a raw save named by its directory name"
    ))
}

/// A path's last part (hosts give paths, Android document names).
fn base_name(path: &str) -> &str {
    path.rsplit(['/', '\\']).next().unwrap_or(path)
}

/// The blocks after a container's header: 1-15 whole blocks, the first a
/// title frame.
fn blocks_after(bytes: &[u8], header: usize) -> Option<Vec<u8>> {
    let data = bytes.get(header..)?;
    let blocks = data.len() / BLOCK_SIZE;
    (data.len().is_multiple_of(BLOCK_SIZE) && (1..BLOCKS).contains(&blocks) && data.starts_with(b"SC")).then(|| data.to_vec())
}

/// A name field (its bytes up to a NUL) as a card file's name: 1-20
/// printable ASCII characters.
fn field_name(field: Option<&[u8]>) -> Result<Vec<u8>, String> {
    let field = field.ok_or("its name field is cut short")?;
    let name = &field[..field.iter().position(|&b| b == 0).unwrap_or(field.len())];
    if name.is_empty() || name.len() > NAME_LENGTH || !name.iter().all(|b| (0x20..0x7F).contains(b)) {
        return Err(format!("its name {:?} is not a card file's (1-20 printable ASCII characters)", name_text(name)));
    }
    Ok(name.to_vec())
}

/// A raw save's name: its file's, which must be a directory name as the
/// PlayStation's games give them (psx-spx "Filename Notes"): a region (BA,
/// BE, BI), a game code (four letters, "-" or "P", five digits) and up to
/// eight more characters.
fn raw_save_name(file_name: &str) -> Result<Vec<u8>, String> {
    let name = file_name.as_bytes();
    let code = name.len() >= 12
        && name[0] == b'B'
        && matches!(name[1], b'A' | b'E' | b'I')
        && name[2..6].iter().all(u8::is_ascii_uppercase)
        && matches!(name[6], b'-' | b'P')
        && name[7..12].iter().all(u8::is_ascii_digit);
    if !code || name.len() > NAME_LENGTH || !name.iter().all(|b| (0x21..0x7F).contains(b)) {
        return Err(format!(
            "a raw single save is named by its file name, its directory name (a region and a game code, as \
             BASLUS-006640 for this game's first file); {file_name} is not one"
        ));
    }
    Ok(name.to_vec())
}

/// A name's bytes as text, each byte the character of that code (Latin-1,
/// so every name reads back as itself, [`name_bytes`]).
pub fn name_text(name: &[u8]) -> String {
    name.iter().map(|&b| char::from(b)).collect()
}

/// A name given as text ([`name_text`]) as its bytes.
pub fn name_bytes(name: &str) -> Result<Vec<u8>, String> {
    name.chars().map(|c| u8::try_from(c).map_err(|_| format!("{name:?} is not a card file's name (its characters are single bytes)"))).collect()
}

/// A card, as [`list`] reads it.
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Listing {
    /// It has a file system ("MC").
    pub formatted: bool,
    /// Blocks free or holding deleted files, for new saves.
    pub free_blocks: usize,
    /// Damage to the directory a save's blocks depend on (an in-use
    /// directory frame's checksum, a broken chain, blocks two files claim):
    /// a card import refuses such a card.
    pub problems: Vec<String>,
    /// What else is odd in the directory (the header frame's checksum, a
    /// free frame's, a frame neither in use nor free, a block in use by no
    /// file); harmless to the files.
    pub notes: Vec<String>,
    pub saves: Vec<Entry>,
}

/// A save on a card.
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Entry {
    pub name: String,
    pub first_block: usize,
    pub blocks: usize,
    /// Its title, decoded from Shift-JIS (kanji show as U+FFFD).
    pub title: String,
    pub kind: Kind,
    /// What is wrong with it: its title frame, its size, or a check of this
    /// game's that it fails.
    pub problems: Vec<String>,
}

/// Whose save it is.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(tag = "game", rename_all = "snake_case")]
pub enum Kind {
    /// This game's (North American): its file number (its digit + 1, as
    /// the game shows it) and the disc it asks for after a load (none when
    /// vars[82] names neither).
    Xenogears { file: u8, disc: Option<u8> },
    JapaneseXenogears,
    Other,
}

impl Kind {
    pub fn describe(self) -> String {
        match self {
            Kind::Xenogears { file, disc: Some(disc) } => format!("Xenogears disc {disc}, file {file:02}"),
            Kind::Xenogears { file, disc: None } => format!("Xenogears, file {file:02}"),
            Kind::JapaneseXenogears => "Japanese Xenogears".into(),
            Kind::Other => "another game".into(),
        }
    }
}

/// What a card (a raw 128 KiB image) holds, and what is wrong with it.
pub fn list(image: &[u8]) -> Listing {
    let mut listing = Listing { formatted: card::is_formatted(image), free_blocks: 0, problems: Vec::new(), notes: Vec::new(), saves: Vec::new() };
    if !listing.formatted {
        return listing;
    }
    let header = card::frame(image, 0);
    if header[0x7F] != card::checksum(header) {
        listing.notes.push(format!("the header frame's checksum is {:#04x}, its bytes give {:#04x}", header[0x7F], card::checksum(header)));
    }
    for block in 1..BLOCKS {
        let frame = card::frame(image, block);
        let state = card::state(image, block);
        let sealed = frame[0x7F] == card::checksum(frame);
        match state {
            card::STATE_FIRST..=card::STATE_LAST if !sealed => listing.problems.push(format!(
                "directory frame {block} (a file's block) has the checksum {:#04x}, its bytes give {:#04x}",
                frame[0x7F],
                card::checksum(frame)
            )),
            0xA0..=0xA3 if !sealed => listing.notes.push(format!(
                "directory frame {block} (a free block) has the checksum {:#04x}, its bytes give {:#04x}",
                frame[0x7F],
                card::checksum(frame)
            )),
            card::STATE_FIRST..=card::STATE_LAST | 0xA0..=0xA3 => {}
            other => listing.notes.push(format!("directory frame {block} has the state {other:#x}, neither in use nor free: its block is not used")),
        }
    }
    listing.free_blocks = (1..BLOCKS).filter(|&b| card::state(image, b) & 0xF0 == card::STATE_FREE).count();
    let mut owners: [Option<usize>; BLOCKS] = [None; BLOCKS];
    for first in (1..BLOCKS).filter(|&b| card::state(image, b) == card::STATE_FIRST) {
        let name = name_text(card::entry_name(image, first));
        let blocks = follow(image, first, &name, &mut listing.problems);
        for &block in &blocks {
            match owners[block] {
                Some(other) => listing.problems.push(format!(
                    "{name} and {} both claim block {block}",
                    name_text(card::entry_name(image, other))
                )),
                None => owners[block] = Some(first),
            }
        }
        listing.saves.push(entry(image, name, &blocks));
    }
    for block in 1..BLOCKS {
        let state = card::state(image, block);
        if matches!(state, card::STATE_MIDDLE | card::STATE_LAST) && owners[block].is_none() {
            listing.notes.push(format!("block {block} is marked as a file's ({state:#04x}) but no file's chain reaches it"));
        }
    }
    listing
}

/// A file's blocks from its first, its chain checked as it goes.
fn follow(image: &[u8], first: usize, name: &str, problems: &mut Vec<String>) -> Vec<usize> {
    let mut blocks = vec![first];
    loop {
        let at = *blocks.last().unwrap();
        let next = card::get_u16(card::frame(image, at), 8);
        if next == 0xFFFF {
            if blocks.len() > 1 && card::state(image, at) != card::STATE_LAST {
                problems.push(format!("{name} ends at block {at}, which is not marked as its last block"));
            }
            return blocks;
        }
        let block = next as usize + 1;
        if !(1..BLOCKS).contains(&block) {
            problems.push(format!("{name}'s block {at} links to block {block}, past the card"));
            return blocks;
        }
        if blocks.contains(&block) {
            problems.push(format!("{name}'s blocks loop back to block {block}"));
            return blocks;
        }
        let state = card::state(image, block);
        if !matches!(state, card::STATE_MIDDLE | card::STATE_LAST) {
            problems.push(format!("{name}'s block {at} links to block {block}, which is not a following block ({state:#04x})"));
            return blocks;
        }
        if card::state(image, at) == card::STATE_LAST {
            problems.push(format!("{name}'s last block {at} links on to block {block}"));
            return blocks;
        }
        blocks.push(block);
    }
}

/// A save as the listing shows it.
fn entry(image: &[u8], name: String, blocks: &[usize]) -> Entry {
    let first = &image[blocks[0] * BLOCK_SIZE..][..BLOCK_SIZE];
    let mut problems = Vec::new();
    let size = card::get_u32(card::frame(image, blocks[0]), 4) as usize;
    if size != blocks.len() * BLOCK_SIZE {
        problems.push(format!("its directory gives {size} bytes, its blocks hold {}", blocks.len() * BLOCK_SIZE));
    }
    let mut title = String::new();
    if !first.starts_with(b"SC") {
        problems.push("its first block has no title frame (it does not begin \"SC\")".into());
    } else {
        if !(0x11..=0x13).contains(&first[2]) {
            problems.push(format!("its icon flag is {:#04x} (0x11-0x13 show an icon of one to three frames)", first[2]));
        }
        if usize::from(first[3]) != blocks.len() {
            problems.push(format!("its title frame gives {} block(s), its directory {}", first[3], blocks.len()));
        }
        let (text, bad) = decode_title(&first[TITLE]);
        title = text;
        if let Some(at) = bad {
            problems.push(format!("its title is not Shift-JIS from byte {at}"));
        }
    }
    let kind = if name.as_bytes().starts_with(XENOGEARS_PREFIX) {
        xenogears(first, &mut problems)
    } else if name.as_bytes().starts_with(JAPANESE_XENOGEARS_PREFIX) {
        Kind::JapaneseXenogears
    } else {
        Kind::Other
    };
    Entry { name, first_block: blocks[0], blocks: blocks.len(), title, kind, problems }
}

/// This game's checks of its save's first block (see the module's notes).
fn xenogears(block: &[u8], problems: &mut Vec<String>) -> Kind {
    let digit = block[DIGIT];
    if digit >= GAME_FILES {
        problems.push(format!("its file digit is {digit}, past the game's {GAME_FILES} files (0-14)"));
    }
    let sum = block[PAYLOAD..SUM].iter().fold(0u8, |a, &b| a.wrapping_add(b));
    if sum != block[SUM] {
        problems.push(format!("its checksum byte is {:#04x} and its data sum to {sum:#04x}: the game's load refuses it", block[SUM]));
    }
    let variable = u16::from_le_bytes([block[DISC_VARIABLE], block[DISC_VARIABLE + 1]]);
    let disc = match variable {
        0 | 1 => Some(variable as u8 + 1),
        _ => {
            problems.push(format!("its disc (vars[82]) is {variable}: after a load the game asks for a disc {} that does not exist", u32::from(variable) + 1));
            None
        }
    };
    Kind::Xenogears { file: digit.wrapping_add(1), disc }
}

/// The save named `name` on a card, its blocks as the BIOS reads them.
pub fn extract(image: &[u8], name: &[u8]) -> Result<Save, String> {
    let first = card::find(image, name).ok_or_else(|| format!("no save named {} is on the card", name_text(name)))?;
    let data = card::chain(image, first).iter().flat_map(|&b| image[b * BLOCK_SIZE..(b + 1) * BLOCK_SIZE].iter().copied()).collect();
    Ok(Save { name: name.to_vec(), data })
}

/// The card (a formatted raw image) with `save` added as the BIOS creates
/// a file ([`super::card`]'s create: the first free or deleted blocks),
/// or why not: as the game's Copy refuses them (`menu_copy_command_run`),
/// a save of that name on the card, or too few free blocks.
pub fn insert(image: &[u8], save: &Save) -> Result<Vec<u8>, String> {
    let name = name_text(&save.name);
    if !card::is_formatted(image) {
        return Err("the card has no file system: the game formats it from its save screen".into());
    }
    let count = save.data.len() / BLOCK_SIZE;
    if count == 0 || !save.data.len().is_multiple_of(BLOCK_SIZE) || count >= BLOCKS {
        return Err(format!("{name} is not 1-15 whole blocks"));
    }
    let mut out = image.to_vec();
    let blocks = card::create(&mut out, &save.name, count).map_err(|refused| match refused {
        card::Refused::Name => format!("{name:?} is not a card file's name (1-20 characters)"),
        card::Refused::Exists => format!("the card already has a save named {name} (the game's Copy refuses it too): delete that one first"),
        card::Refused::Full { free } => {
            format!("{name} needs {count} block(s) and the card has {free} free (the game's Copy refuses a full card too)")
        }
    })?;
    for (i, &block) in blocks.iter().enumerate() {
        out[block * BLOCK_SIZE..(block + 1) * BLOCK_SIZE].copy_from_slice(&save.data[i * BLOCK_SIZE..(i + 1) * BLOCK_SIZE]);
    }
    Ok(out)
}

/// The card with the save named `name` deleted as the BIOS deletes it
/// (its blocks marked deleted, free for the next save).
pub fn delete(image: &[u8], name: &[u8]) -> Result<Vec<u8>, String> {
    let mut out = image.to_vec();
    card::erase(&mut out, name).ok_or_else(|| format!("no save named {} is on the card", name_text(name)))?;
    Ok(out)
}

/// A card as a file of `format`. A DexDrive header (psx-spx) copies each
/// frame's first and ninth bytes of block 0 and leaves its 15 comments
/// empty.
pub fn export_card(image: &[u8], format: CardFormat) -> Vec<u8> {
    match format {
        CardFormat::Raw => image.to_vec(),
        CardFormat::Gme => {
            let mut out = vec![0; GME_HEADER];
            out[..11].copy_from_slice(b"123-456-STD");
            out[0x12] = 1;
            out[0x14] = 1;
            for n in 0..BLOCKS {
                out[0x15 + n] = card::frame(image, n)[0];
                out[0x25 + n] = card::frame(image, n)[8];
            }
            out.extend_from_slice(image);
            out
        }
    }
}

/// The save named `name` as a single-save file (.mcs): its first block's
/// directory frame as the card holds it, then its blocks.
pub fn export_save(image: &[u8], name: &[u8]) -> Result<Vec<u8>, String> {
    let save = extract(image, name)?;
    let first = card::find(image, name).expect("extracted");
    let mut out = card::frame(image, first).to_vec();
    out.extend_from_slice(&save.data);
    Ok(out)
}

/// A title (bytes up to a NUL) decoded from Shift-JIS: ASCII, half-width
/// katakana and the JIS X 0208 rows of symbols, digits, Latin letters,
/// kana, Greek, Cyrillic and box drawing (lead bytes 0x81-0x84, the cp932
/// mapping); kanji, which need a table not kept here, become U+FFFD.
/// Returns the text and the offset of the first byte that is not
/// Shift-JIS, where decoding stops.
pub fn decode_title(raw: &[u8]) -> (String, Option<usize>) {
    let raw = &raw[..raw.iter().position(|&b| b == 0).unwrap_or(raw.len())];
    let mut text = String::new();
    let mut at = 0;
    while at < raw.len() {
        let lead = raw[at];
        match lead {
            0x20..=0x7E => {
                text.push(char::from(lead));
                at += 1;
            }
            0xA1..=0xDF => {
                text.push(char::from_u32(0xFF61 + u32::from(lead - 0xA1)).expect("half-width katakana"));
                at += 1;
            }
            0x81..=0x9F | 0xE0..=0xFC => {
                let Some(&trail) = raw.get(at + 1).filter(|&&t| matches!(t, 0x40..=0x7E | 0x80..=0xFC)) else {
                    return (text, Some(at));
                };
                let cell = usize::from(trail) - if trail < 0x7F { 0x40 } else { 0x41 };
                let code = match lead {
                    0x81..=0x84 => u32::from(SJIS_ROWS[usize::from(lead - 0x81) * 188 + cell]),
                    _ => 0xFFFD,
                };
                let Some(c) = char::from_u32(code).filter(|_| code != 0) else {
                    return (text, Some(at));
                };
                text.push(c);
                at += 2;
            }
            _ => return (text, Some(at)),
        }
    }
    (text, None)
}

/// A title for a display without full-width glyphs: full-width ASCII
/// forms as ASCII, the ideographic space as a space.
pub fn narrow(title: &str) -> String {
    title
        .chars()
        .map(|c| match c {
            '\u{FF01}'..='\u{FF5E}' => char::from_u32(c as u32 - 0xFEE0).expect("ASCII"),
            '\u{3000}' => ' ',
            other => other,
        })
        .collect()
}

/// Shift-JIS lead bytes 0x81-0x84 by trail byte (0x40-0x7E, 0x80-0xFC; 188
/// each), as Unicode code points, 0 where none is assigned. Generated with
/// Python's cp932 codec:
/// `[ord(bytes([l, t]).decode('cp932')) for l in range(0x81, 0x85) for t in [*range(0x40, 0x7F), *range(0x80, 0xFD)]]`,
/// with 0 for each pair it refuses.
const SJIS_ROWS: [u16; 4 * 188] = [
    0x3000, 0x3001, 0x3002, 0xFF0C, 0xFF0E, 0x30FB, 0xFF1A, 0xFF1B, 0xFF1F, 0xFF01, 0x309B, 0x309C,
    0x00B4, 0xFF40, 0x00A8, 0xFF3E, 0xFFE3, 0xFF3F, 0x30FD, 0x30FE, 0x309D, 0x309E, 0x3003, 0x4EDD,
    0x3005, 0x3006, 0x3007, 0x30FC, 0x2015, 0x2010, 0xFF0F, 0xFF3C, 0xFF5E, 0x2225, 0xFF5C, 0x2026,
    0x2025, 0x2018, 0x2019, 0x201C, 0x201D, 0xFF08, 0xFF09, 0x3014, 0x3015, 0xFF3B, 0xFF3D, 0xFF5B,
    0xFF5D, 0x3008, 0x3009, 0x300A, 0x300B, 0x300C, 0x300D, 0x300E, 0x300F, 0x3010, 0x3011, 0xFF0B,
    0xFF0D, 0x00B1, 0x00D7, 0x00F7, 0xFF1D, 0x2260, 0xFF1C, 0xFF1E, 0x2266, 0x2267, 0x221E, 0x2234,
    0x2642, 0x2640, 0x00B0, 0x2032, 0x2033, 0x2103, 0xFFE5, 0xFF04, 0xFFE0, 0xFFE1, 0xFF05, 0xFF03,
    0xFF06, 0xFF0A, 0xFF20, 0x00A7, 0x2606, 0x2605, 0x25CB, 0x25CF, 0x25CE, 0x25C7, 0x25C6, 0x25A1,
    0x25A0, 0x25B3, 0x25B2, 0x25BD, 0x25BC, 0x203B, 0x3012, 0x2192, 0x2190, 0x2191, 0x2193, 0x3013,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2208,
    0x220B, 0x2286, 0x2287, 0x2282, 0x2283, 0x222A, 0x2229, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x2227, 0x2228, 0xFFE2, 0x21D2, 0x21D4, 0x2200, 0x2203, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2220, 0x22A5, 0x2312,
    0x2202, 0x2207, 0x2261, 0x2252, 0x226A, 0x226B, 0x221A, 0x223D, 0x221D, 0x2235, 0x222B, 0x222C,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x212B, 0x2030, 0x266F, 0x266D, 0x266A,
    0x2020, 0x2021, 0x00B6, 0x0000, 0x0000, 0x0000, 0x0000, 0x25EF,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0xFF10, 0xFF11, 0xFF12, 0xFF13, 0xFF14, 0xFF15, 0xFF16, 0xFF17, 0xFF18,
    0xFF19, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFF21, 0xFF22, 0xFF23, 0xFF24,
    0xFF25, 0xFF26, 0xFF27, 0xFF28, 0xFF29, 0xFF2A, 0xFF2B, 0xFF2C, 0xFF2D, 0xFF2E, 0xFF2F, 0xFF30,
    0xFF31, 0xFF32, 0xFF33, 0xFF34, 0xFF35, 0xFF36, 0xFF37, 0xFF38, 0xFF39, 0xFF3A, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0xFF41, 0xFF42, 0xFF43, 0xFF44, 0xFF45, 0xFF46, 0xFF47, 0xFF48,
    0xFF49, 0xFF4A, 0xFF4B, 0xFF4C, 0xFF4D, 0xFF4E, 0xFF4F, 0xFF50, 0xFF51, 0xFF52, 0xFF53, 0xFF54,
    0xFF55, 0xFF56, 0xFF57, 0xFF58, 0xFF59, 0xFF5A, 0x0000, 0x0000, 0x0000, 0x0000, 0x3041, 0x3042,
    0x3043, 0x3044, 0x3045, 0x3046, 0x3047, 0x3048, 0x3049, 0x304A, 0x304B, 0x304C, 0x304D, 0x304E,
    0x304F, 0x3050, 0x3051, 0x3052, 0x3053, 0x3054, 0x3055, 0x3056, 0x3057, 0x3058, 0x3059, 0x305A,
    0x305B, 0x305C, 0x305D, 0x305E, 0x305F, 0x3060, 0x3061, 0x3062, 0x3063, 0x3064, 0x3065, 0x3066,
    0x3067, 0x3068, 0x3069, 0x306A, 0x306B, 0x306C, 0x306D, 0x306E, 0x306F, 0x3070, 0x3071, 0x3072,
    0x3073, 0x3074, 0x3075, 0x3076, 0x3077, 0x3078, 0x3079, 0x307A, 0x307B, 0x307C, 0x307D, 0x307E,
    0x307F, 0x3080, 0x3081, 0x3082, 0x3083, 0x3084, 0x3085, 0x3086, 0x3087, 0x3088, 0x3089, 0x308A,
    0x308B, 0x308C, 0x308D, 0x308E, 0x308F, 0x3090, 0x3091, 0x3092, 0x3093, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x30A1, 0x30A2, 0x30A3, 0x30A4, 0x30A5, 0x30A6, 0x30A7, 0x30A8, 0x30A9, 0x30AA, 0x30AB, 0x30AC,
    0x30AD, 0x30AE, 0x30AF, 0x30B0, 0x30B1, 0x30B2, 0x30B3, 0x30B4, 0x30B5, 0x30B6, 0x30B7, 0x30B8,
    0x30B9, 0x30BA, 0x30BB, 0x30BC, 0x30BD, 0x30BE, 0x30BF, 0x30C0, 0x30C1, 0x30C2, 0x30C3, 0x30C4,
    0x30C5, 0x30C6, 0x30C7, 0x30C8, 0x30C9, 0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE, 0x30CF, 0x30D0,
    0x30D1, 0x30D2, 0x30D3, 0x30D4, 0x30D5, 0x30D6, 0x30D7, 0x30D8, 0x30D9, 0x30DA, 0x30DB, 0x30DC,
    0x30DD, 0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2, 0x30E3, 0x30E4, 0x30E5, 0x30E6, 0x30E7, 0x30E8,
    0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, 0x30EE, 0x30EF, 0x30F0, 0x30F1, 0x30F2, 0x30F3, 0x30F4,
    0x30F5, 0x30F6, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0391, 0x0392,
    0x0393, 0x0394, 0x0395, 0x0396, 0x0397, 0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E,
    0x039F, 0x03A0, 0x03A1, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7, 0x03A8, 0x03A9, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6,
    0x03B7, 0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF, 0x03C0, 0x03C1, 0x03C3,
    0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0401, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A,
    0x041B, 0x041C, 0x041D, 0x041E, 0x041F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426,
    0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0451, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A,
    0x043B, 0x043C, 0x043D, 0x043E, 0x043F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446,
    0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2500, 0x2502,
    0x250C, 0x2510, 0x2518, 0x2514, 0x251C, 0x252C, 0x2524, 0x2534, 0x253C, 0x2501, 0x2503, 0x250F,
    0x2513, 0x251B, 0x2517, 0x2523, 0x2533, 0x252B, 0x253B, 0x254B, 0x2520, 0x252F, 0x2528, 0x2537,
    0x253F, 0x251D, 0x2530, 0x2525, 0x2538, 0x2542, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
];

#[cfg(test)]
mod tests {
    use super::*;
    use crate::devices::card::formatted;

    /// This game's title prefix and file number, as `menu_save_build_header_title` writes them.
    const GAME_TITLE: &[u8] = b"\x82\x77\x82\x64\x82\x6d\x82\x6e\x82\x66\x82\x64\x82\x60\x82\x71\x82\x72\x81\x5e\x82\x6d\x82\x8f\x81\x44";

    /// A save's blocks: its title frame (`title`, `blocks` blocks), the
    /// rest `fill`.
    fn blocks(blocks: usize, title: &[u8], fill: u8) -> Vec<u8> {
        let mut data = vec![fill; blocks * BLOCK_SIZE];
        data[..0x80].fill(0);
        data[..4].copy_from_slice(&[b'S', b'C', 0x11, blocks as u8]);
        data[4..4 + title.len()].copy_from_slice(title);
        data
    }

    /// This game's save for file `digit` on `disc`, its checksum right.
    fn xenogears_save(digit: u8, disc: u16) -> Save {
        let mut title = GAME_TITLE.to_vec();
        title.extend_from_slice(&[0x82, (digit + 1) / 10 + 0x4F, 0x82, (digit + 1) % 10 + 0x4F, 0x81, 0x40]);
        title.extend_from_slice(b"\x82\x6b\x82\x81\x82\x88\x82\x81\x82\x8e"); // Ｌａｈａｎ
        let mut data = blocks(1, &title, 0);
        for (i, byte) in data[PAYLOAD..SUM].iter_mut().enumerate() {
            *byte = (i * 7) as u8;
        }
        data[DIGIT] = digit;
        data[DISC_VARIABLE..DISC_VARIABLE + 2].copy_from_slice(&disc.to_le_bytes());
        data[SUM] = data[PAYLOAD..SUM].iter().fold(0u8, |a, &b| a.wrapping_add(b));
        Save { name: [XENOGEARS_PREFIX, &[b'0' + digit]].concat(), data }
    }

    /// Another game's two-block save.
    fn foreign_save() -> Save {
        Save { name: b"BESLES-01234GAME".to_vec(), data: blocks(2, b"ABC", 0x5A) }
    }

    /// A card holding `saves`.
    fn card_with(saves: &[&Save]) -> Vec<u8> {
        saves.iter().fold(formatted(), |image, save| insert(&image, save).unwrap())
    }

    fn detected(bytes: &[u8], name: &str) -> (Container, Contents) {
        detect(bytes, name).unwrap_or_else(|error| panic!("{name}: {error}"))
    }

    #[test]
    fn every_card_container_reads_back_the_card_and_raw_and_dexdrive_write_it() {
        let image = card_with(&[&xenogears_save(0, 0), &foreign_save()]);
        assert_eq!(detected(&image, "card1.mcd"), (Container::Raw, Contents::Card(image.clone())));
        // DexDrive, as written here: its header (psx-spx) and the card.
        let gme = export_card(&image, CardFormat::Gme);
        assert_eq!(gme.len(), 0xF40 + CARD_SIZE);
        assert_eq!(&gme[..12], b"123-456-STD\0");
        assert_eq!(&gme[0x10..0x15], &[0, 0, 1, 0, 1]);
        assert_eq!(&gme[0x15..0x18], &[b'M', 0x51, 0x51], "frame 0's first byte, then the files' states");
        assert_eq!(gme[0x26..0x28], [0xFF, 0x02], "then their links: none, and to block 3 (2)");
        assert!(gme[0x40..0xF40].iter().all(|&b| b == 0), "empty comments");
        assert_eq!(detected(&gme, "card1.gme"), (Container::Gme, Contents::Card(image.clone())));
        // Connectix VGS and PSP headers as their tools write them (the PSP's
        // signature is not checked: these are junk).
        let mut vgs = vec![0; 0x40];
        vgs[..4].copy_from_slice(b"VgsM");
        vgs[4] = 1;
        vgs[8] = 1;
        vgs[12] = 1;
        vgs[17] = 2;
        vgs.extend_from_slice(&image);
        assert_eq!(detected(&vgs, "card.vgs"), (Container::Vgs, Contents::Card(image.clone())));
        let mut vmp = vec![0x5A; 0x80];
        vmp[..4].copy_from_slice(b"\0PMV");
        vmp[4..8].copy_from_slice(&0x80u32.to_le_bytes());
        vmp.extend_from_slice(&image);
        assert_eq!(detected(&vmp, "SCEVMC0.VMP"), (Container::Vmp, Contents::Card(image.clone())));
        assert_eq!(export_card(&image, CardFormat::Raw), image);
    }

    #[test]
    fn every_single_save_container_reads_back_the_save_and_mcs_writes_it() {
        let save = foreign_save();
        let image = card_with(&[&xenogears_save(3, 1), &save]);
        let mcs = export_save(&image, &save.name).unwrap();
        assert_eq!(mcs.len(), 0x80 + 2 * BLOCK_SIZE);
        assert_eq!(&mcs[..0x80], card::frame(&image, 2), "its first block's directory frame");
        assert_eq!(detected(&mcs, "game.mcs"), (Container::Mcs, Contents::Save(save.clone())));
        assert_eq!(detected(&mcs, "game.ps1").0, Container::Mcs);
        // PS3: header with type 1 and the name, then the blocks (the
        // signature is not checked).
        let mut psv = vec![0; 0x84];
        psv[..4].copy_from_slice(b"\0VSP");
        psv[0x08..0x30].fill(0xA5);
        psv[0x38] = 0x14;
        psv[0x3C] = 1;
        psv[0x40..0x44].copy_from_slice(&(save.data.len() as u32).to_le_bytes());
        psv[0x44] = 0x84;
        psv[0x64..0x64 + save.name.len()].copy_from_slice(&save.name);
        psv.extend_from_slice(&save.data);
        assert_eq!(detected(&psv, "BESLES-01234.PSV"), (Container::Psv, Contents::Save(save.clone())));
        // Action Replay's 54-byte header: the name, then a title.
        let mut ar = vec![0; 54];
        ar[..save.name.len()].copy_from_slice(&save.name);
        ar[0x15..0x15 + 4].copy_from_slice(b"Game");
        ar.extend_from_slice(&save.data);
        for name in ["save.psx", "save.mcb", "save.mcx", "save.pda"] {
            assert_eq!(detected(&ar, name), (Container::ActionReplay, Contents::Save(save.clone())));
        }
        // A raw save, named by its file (its directories ignored).
        let digit_ten = xenogears_save(10, 0);
        assert_eq!(&digit_ten.name, b"BASLUS-00664:", "digit 10 is ':'");
        assert_eq!(
            detected(&digit_ten.data, "/saves/BASLUS-00664:"),
            (Container::RawSave, Contents::Save(digit_ten.clone()))
        );
        // Each goes onto another card and comes back as it was.
        for bytes in [&mcs, &psv, &ar] {
            let Contents::Save(read) = detected(bytes, "save.mcs").1 else { panic!() };
            let other = insert(&formatted(), &read).unwrap();
            assert_eq!(extract(&other, &read.name).unwrap(), save);
        }
    }

    #[test]
    fn files_that_are_not_cards_or_saves_are_refused_by_what_they_are_not() {
        let refused = |bytes: &[u8], name: &str| detect(bytes, name).err().unwrap_or_else(|| panic!("{name} accepted"));
        assert!(refused(&[0; 1000], "notes.txt").contains("not a memory card or a save"));
        let mut short_gme = export_card(&formatted(), CardFormat::Gme);
        short_gme.truncate(0x10000);
        assert!(refused(&short_gme, "card.gme").contains("begins as a DexDrive card (.gme) but is not one"));
        let mut ps2 = vec![0; 0x84 + BLOCK_SIZE];
        ps2[..4].copy_from_slice(b"\0VSP");
        ps2[0x3C] = 2;
        assert!(refused(&ps2, "game.psv").contains("type 2, not of a PS1 game"));
        let raw = foreign_save().data;
        assert!(refused(&raw, "my save.bin").contains("named by its file name"), "a raw save needs its directory name");
        let mut nameless = export_save(&card_with(&[&foreign_save()]), b"BESLES-01234GAME").unwrap();
        nameless[NAME_OFFSET..NAME_OFFSET + 21].fill(0);
        assert!(refused(&nameless, "game.mcs").contains("is not a card file's"));
        let mut unformatted = vec![0xFF; CARD_SIZE];
        unformatted[0] = 0;
        assert!(refused(&unformatted, "blank.mcd").contains("not a memory card or a save"));
    }

    #[test]
    fn the_listing_reads_titles_kinds_and_the_games_own_checks() {
        let mut damaged = xenogears_save(1, 0);
        damaged.data[0x500] ^= 1;
        let mut lost = xenogears_save(4, 7);
        lost.data[DIGIT] = 15;
        lost.data[SUM] = lost.data[PAYLOAD..SUM].iter().fold(0u8, |a, &b| a.wrapping_add(b));
        let japanese = Save { name: b"BISLPS-011600".to_vec(), data: blocks(1, b"\x83\x5b\x83\x6d", 0) };
        let image = card_with(&[&xenogears_save(0, 0), &xenogears_save(2, 1), &damaged, &lost, &japanese, &foreign_save()]);
        let listing = list(&image);
        assert!(listing.formatted && listing.problems.is_empty() && listing.notes.is_empty(), "{listing:?}");
        assert_eq!(listing.free_blocks, 15 - 7);
        let saves = &listing.saves;
        assert_eq!(saves[0].title, "ＸＥＮＯＧＥＡＲＳ／Ｎｏ．０１　Ｌａｈａｎ");
        assert_eq!(narrow(&saves[0].title), "XENOGEARS/No.01 Lahan");
        assert_eq!((saves[0].kind, saves[0].problems.len()), (Kind::Xenogears { file: 1, disc: Some(1) }, 0));
        assert_eq!(saves[1].kind, Kind::Xenogears { file: 3, disc: Some(2) });
        assert_eq!(saves[1].kind.describe(), "Xenogears disc 2, file 03");
        assert!(saves[2].problems[0].contains("the game's load refuses it"), "{:?}", saves[2].problems);
        assert_eq!(saves[3].kind, Kind::Xenogears { file: 16, disc: None });
        assert!(saves[3].problems[0].contains("past the game's 15 files"));
        assert!(saves[3].problems[1].contains("vars[82]) is 7"));
        assert_eq!((saves[4].kind, saves[4].title.as_str()), (Kind::JapaneseXenogears, "ゼノ"));
        assert_eq!((saves[5].kind, saves[5].blocks, saves[5].first_block), (Kind::Other, 2, 6));
        assert!(saves[5].problems.is_empty());
    }

    #[test]
    fn a_damaged_directory_is_named_and_harmless_oddities_are_noted() {
        // The game's save in block 1, the other game's in blocks 2 and 3.
        let image = card_with(&[&xenogears_save(0, 0), &foreign_save()]);
        let damage = |change: &dyn Fn(&mut Vec<u8>)| {
            let mut card = image.clone();
            change(&mut card);
            list(&card)
        };
        let reseal = |card: &mut Vec<u8>, n: usize| {
            let at = n * FRAME_SIZE;
            card[at + 0x7F] = card::checksum(&card[at..at + FRAME_SIZE]);
        };
        let link = |card: &mut Vec<u8>, n: usize, next: u16| {
            card[n * FRAME_SIZE + 8..n * FRAME_SIZE + 10].copy_from_slice(&next.to_le_bytes());
            reseal(card, n);
        };
        let listing = damage(&|card: &mut Vec<u8>| card[FRAME_SIZE + 0x20] ^= 1);
        assert!(listing.problems[0].contains("directory frame 1 (a file's block) has the checksum"), "{:?}", listing.problems);
        let listing = damage(&|card: &mut Vec<u8>| link(card, 3, 1));
        assert!(listing.problems[0].contains("loop back to block 2"), "{:?}", listing.problems);
        let listing = damage(&|card: &mut Vec<u8>| link(card, 3, 40));
        assert!(listing.problems[0].contains("links to block 41, past the card"), "{:?}", listing.problems);
        let listing = damage(&|card: &mut Vec<u8>| link(card, 1, 2));
        assert!(listing.problems.iter().any(|p| p.contains("both claim block 3")), "{:?}", listing.problems);
        let listing = damage(&|card: &mut Vec<u8>| {
            card[0x7F] ^= 1;
            card[10 * FRAME_SIZE] = 0x00;
            card[FRAME_SIZE + 5] = 0x40;
            reseal(card, 1);
        });
        assert!(listing.problems.is_empty(), "{:?}", listing.problems);
        assert_eq!(listing.notes.len(), 2, "{:?}", listing.notes);
        assert!(listing.notes[0].contains("header frame's checksum"));
        assert!(listing.notes[1].contains("neither in use nor free"));
        assert!(listing.saves[0].problems[0].contains("its directory gives 16384 bytes"), "{:?}", listing.saves[0].problems);
        let orphan = damage(&|card: &mut Vec<u8>| {
            card[4 * FRAME_SIZE..4 * FRAME_SIZE + 4].copy_from_slice(&0x52u32.to_le_bytes());
            reseal(card, 4);
        });
        assert!(orphan.problems.is_empty() && orphan.notes[0].contains("block 4 is marked as a file's (0x52)"), "{:?}", orphan.notes);
        let mut title = image.clone();
        title[BLOCK_SIZE..BLOCK_SIZE + 2].copy_from_slice(b"XX");
        assert!(list(&title).saves[0].problems.iter().any(|p| p.contains("no title frame")));
    }

    #[test]
    fn a_save_takes_the_first_free_or_deleted_blocks_and_the_games_copy_refusals() {
        let one = xenogears_save(0, 0);
        let image = card_with(&[&one, &foreign_save()]);
        let freed = delete(&image, &one.name).unwrap();
        assert_eq!(card::state(&freed, 1), 0xA1, "deleted as the BIOS deletes");
        assert!(list(&freed).saves.iter().all(|s| s.name.as_bytes() != one.name));
        // A three-block save takes deleted block 1, then 4 and 5.
        let three = Save { name: b"BASLUS-00999LONG".to_vec(), data: blocks(3, b"L", 0x33) };
        let card = insert(&freed, &three).unwrap();
        assert_eq!(card::chain(&card, 1), vec![1, 4, 5]);
        assert_eq!([card::state(&card, 1), card::state(&card, 4), card::state(&card, 5)], [0x51, 0x52, 0x53]);
        assert_eq!(card::get_u32(card::frame(&card, 1), 4), 3 * BLOCK_SIZE as u32);
        assert_eq!(extract(&card, &three.name).unwrap(), three);
        assert!(list(&card).problems.is_empty());
        let again = insert(&card, &three).unwrap_err();
        assert!(again.contains("already has a save named BASLUS-00999LONG"), "{again}");
        let mut full = card.clone();
        for n in 0..10 {
            full = insert(&full, &Save { name: format!("BASLUS-00999F{n}").into_bytes(), data: blocks(1, b"F", 0) }).unwrap();
        }
        let refused = insert(&full, &xenogears_save(5, 0)).unwrap_err();
        assert!(refused.contains("needs 1 block(s) and the card has 0 free"), "{refused}");
        assert!(insert(&vec![0; CARD_SIZE], &one).unwrap_err().contains("no file system"));
        assert!(delete(&card, b"BASLUS-006640").unwrap_err().contains("no save named BASLUS-006640"));
    }

    #[test]
    fn titles_decode_shift_jis_kana_and_say_where_they_stop() {
        assert_eq!(decode_title(b"Plain ASCII\0junk"), ("Plain ASCII".into(), None));
        assert_eq!(decode_title(&[0xB1, 0xB2, 0x83, 0x41, 0x82, 0xA0]), ("ｱｲアあ".into(), None));
        assert_eq!(decode_title(&[0x88, 0x9F, 0x41]), ("\u{FFFD}A".into(), None), "a kanji is a valid character not decoded");
        assert_eq!(decode_title(&[0x41, 0x82]), ("A".into(), Some(1)), "a lead byte without its trail");
        assert_eq!(decode_title(&[0x41, 0x81, 0xAD]), ("A".into(), Some(1)), "a pair cp932 does not assign");
        assert_eq!(decode_title(&[0x80, 0x41]), (String::new(), Some(0)));
        assert_eq!(narrow("ＸＥＮＯＧＥＡＲＳ／Ｎｏ．０１　"), "XENOGEARS/No.01 ");
        assert_eq!(name_bytes(&name_text(&[0x42, 0xE9, 0x7F])).unwrap(), [0x42, 0xE9, 0x7F]);
        assert!(name_bytes("名").is_err());
    }
}
