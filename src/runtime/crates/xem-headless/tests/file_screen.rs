//! The original game's file screens over the host's Kanji font and cards
//! (local only: it needs the user's disc 1 and a game module build). The
//! title screen's Load lists a card holding a save made here (this game's
//! name, title, digit and checksum as the game writes them; no original
//! content), and its title is drawn, pixel for pixel as slot39
//! menu_save_title_render composes it, from the substitute glyphs without a
//! BIOS image and from a BIOS image's font with one (an image made here,
//! whose glyphs are the substitute's drawn bold). With both cards taken out
//! (`card_present`) the Load shows the game's "No Memory Card" notice and
//! returns to the title; a card put back is listed, and at the file screen
//! a card taken out and put back is listed anew, as after a physical swap.
//! The inputs to the title's Load are the load slice's
//! (tests/reference-inputs/slice-load.json).
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test file_screen

#![cfg(all(has_game_module, not(debug_assertions)))]

use std::path::{Path, PathBuf};

use serde_json::{Value, json};
use xem_core::bios::{self, KANJI_FONT_ROM_OFFSET, KANJI_GLYPH_BYTES};
use xem_core::control::Session;
use xem_core::devices::card::{BLOCK_SIZE, formatted};
use xem_core::devices::card_file;
use xem_game::{BootOptions, NativeModule, boot};

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn scratch(name: &str) -> PathBuf {
    let dir = std::env::temp_dir().join(format!("xem-file-screen-{name}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();
    dir
}

/// Text in full-width Shift-JIS: Latin letters, digits, the space and the
/// marks titles use.
fn full_width(text: &str) -> Vec<u8> {
    text.bytes()
        .flat_map(|c| match c {
            b'A'..=b'Z' => [0x82, 0x60 + c - b'A'],
            b'a'..=b'z' => [0x82, 0x81 + c - b'a'],
            b'0'..=b'9' => [0x82, 0x4F + c - b'0'],
            b' ' => [0x81, 0x40],
            b'.' => [0x81, 0x44],
            b'/' => [0x81, 0x5E],
            other => panic!("{other} has no full-width form here"),
        })
        .collect()
}

/// This game's save for file `digit` + 1 whose title line (30 bytes, as the
/// game takes it from its text file) is `line`: its name and its block, the
/// title as slot39 menu_save_build_header_title builds it, Fei alone in the
/// party, the digit and the payload's checksum as the game writes them.
fn game_save(digit: u8, line: &str) -> (String, Vec<u8>) {
    let mut block = vec![0; BLOCK_SIZE];
    block[..4].copy_from_slice(&[b'S', b'C', 0x11, 1]);
    let mut line = full_width(line);
    line.resize(30, 0);
    for pair in line.chunks_mut(2).filter(|pair| pair[0] == 0) {
        pair.copy_from_slice(&[0x81, 0x40]);
    }
    let mut title = full_width("XENOGEARS/No.");
    title.extend(full_width(&format!("{:02} ", digit + 1)));
    title.extend(line);
    block[4..4 + title.len()].copy_from_slice(&title);
    block[0x100 + 0x1C..0x100 + 0x1F].copy_from_slice(&[0, 0xFF, 0xFF]);
    block[0x123] = digit;
    block[0x1FFF] = block[0x100..0x1FFF].iter().fold(0u8, |a, &b| a.wrapping_add(b));
    (format!("BASLUS-00664{}", char::from(b'0' + digit)), block)
}

/// A freshly formatted card holding `save` (as an imported `.mcs` becomes).
fn card_holding((name, block): &(String, Vec<u8>)) -> Vec<u8> {
    let mut frame = vec![0; 0x80];
    frame[0] = 0x51;
    frame[4..8].copy_from_slice(&(block.len() as u32).to_le_bytes());
    frame[8..10].copy_from_slice(&0xFFFFu16.to_le_bytes());
    frame[0x0A..0x0A + name.len()].copy_from_slice(name.as_bytes());
    frame[0x7F] = frame[..0x7F].iter().fold(0, |a, b| a ^ b);
    card_file::detect(&[frame, block.clone()].concat(), "save.mcs").expect("a single save").1
}

/// The load slice's inputs before frame `until`: the title, then its Load.
fn title_inputs(until: u64) -> Vec<Value> {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../tests/reference-inputs/slice-load.json");
    let log: Value = serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();
    log["commands"].as_array().unwrap().iter().filter(|c| c[0].as_u64().unwrap() < until).cloned().collect()
}

fn session(disc: &Path, cards: &Path, bios: Option<PathBuf>) -> Session<NativeModule> {
    boot(disc, &BootOptions { cards: Some(cards.to_path_buf()), bios, ..Default::default() }).expect("boot").0
}

fn ok(s: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = s.execute(&command);
    assert!(reply.get("error").is_none(), "{command}: {reply}");
    reply
}

fn run_to(s: &mut Session<NativeModule>, frame: u64) {
    while s.runtime.frames() < frame {
        assert!(s.frame().expect("frame"));
    }
}

/// The title image in VRAM (x 0x140-0x17F, y 0xE0-0xFF, halfwords of four
/// 4-bit pixels), where slot39 menu_save_title_render loads it.
fn drawn_title(s: &mut Session<NativeModule>) -> Vec<u16> {
    let vram = s.runtime.services().gpu.vram();
    (0xE0..0x100).flat_map(|y| (0x140..0x180).map(move |x| (y, x))).map(|(y, x)| vram[y * 1024 + x]).collect()
}

/// The image slot39 menu_save_title_render composes for `title` (two-byte
/// codes) from `font` (the font region and the bytes after it): each
/// character's 16 rows read from its glyph on (the last from the next
/// glyph's first), the leftmost pixel the first byte's high bit, narrowed
/// to 12 columns (1|2, 5|6, 9|10 and 13|14 merged) at a 16-pixel pitch, 16
/// characters a line, at most 32 characters or 64 bytes.
fn title_image(title: &[u8], font: &[u8]) -> Vec<u16> {
    let mut image = vec![0u16; 0x40 * 0x20];
    let (mut at, mut chars) = (0, 0);
    while at < title.len() && title[at] != 0 && at < 0x40 && chars < 0x20 {
        assert!(title[at] >= 0x80, "two-byte codes only");
        if let Some(index) = bios::glyph_index(u16::from_be_bytes([title[at], title[at + 1]])) {
            let rows = &font[index * KANJI_GLYPH_BYTES..index * KANJI_GLYPH_BYTES + 32];
            for row in 0..16 {
                let bits = u16::from_le_bytes([rows[row * 2], rows[row * 2 + 1]]);
                let pixel: Vec<u16> = (0..8).rev().chain((8..16).rev()).map(|col| bits >> col & 1).collect();
                let narrow = [pixel[0], pixel[1] | pixel[2], pixel[3], pixel[4], pixel[5] | pixel[6], pixel[7], pixel[8],
                    pixel[9] | pixel[10], pixel[11], pixel[12], pixel[13] | pixel[14], pixel[15]];
                for (col, bit) in narrow.into_iter().enumerate() {
                    image[col / 4 + row * 64 + chars % 16 * 4 + chars / 16 * 1024] |= bit << (col % 4 * 4);
                }
            }
        }
        at += 2;
        chars += 1;
    }
    image
}

/// A BIOS image (made here) whose font holds the substitute glyphs drawn
/// bold: each row with its copy one column to the right.
fn bold_bios() -> Vec<u8> {
    let mut rom = vec![0; bios::BIOS_SIZE];
    for (code, glyph) in bios::substitute_glyphs() {
        let at = KANJI_FONT_ROM_OFFSET + bios::glyph_index(code).unwrap() * KANJI_GLYPH_BYTES;
        for row in 0..15 {
            let bits = u16::from_be_bytes([glyph[row * 2], glyph[row * 2 + 1]]);
            rom[at + row * 2..at + row * 2 + 2].copy_from_slice(&(bits | bits >> 1).to_be_bytes());
        }
    }
    rom
}

/// What the title screen's Load draws for card 1's first save: its title
/// image once the Load's cursor is on it, and which font the boot placed.
fn title_on_the_file_screen(disc: &Path, cards: &Path, bios: Option<PathBuf>) -> (Vec<u16>, Value) {
    let mut s = session(disc, cards, bios);
    ok(&mut s, json!({ "cmd": "schedule", "commands": title_inputs(3004) }));
    run_to(&mut s, 2900);
    assert!(drawn_title(&mut s).iter().all(|&p| p == 0), "no title before the Load's cursor reaches a save");
    run_to(&mut s, 3040);
    let font = s.execute(&json!({ "cmd": "status" }))["kanji_font"].clone();
    (drawn_title(&mut s), font)
}

#[test]
fn the_file_screen_draws_save_titles_from_the_substitute_glyphs_or_a_bios_image() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let save = game_save(0, "Lahan Village");
    let title = save.1[4..4 + 0x5C].to_vec();
    let cards = scratch("titles");
    std::fs::write(cards.join("card1.mcd"), card_holding(&save)).unwrap();

    // Without a BIOS image: the substitute glyphs.
    let (drawn, font) = title_on_the_file_screen(&disc, &cards, None);
    assert_eq!(font, json!("substitute"));
    assert!(drawn.iter().any(|&p| p != 0), "the title is drawn");
    assert!(drawn == title_image(&title, &bios::substitute_font()), "from the substitute glyphs");

    // With one: its font.
    let rom = bold_bios();
    let path = cards.join("bios.bin");
    std::fs::write(&path, &rom).unwrap();
    let (bold, font) = title_on_the_file_screen(&disc, &cards, Some(path.clone()));
    assert_eq!(font, json!("bios"));
    assert!(bold == title_image(&title, &rom[KANJI_FONT_ROM_OFFSET..]), "from the BIOS image's font");
    assert_ne!(bold, drawn);

    // A file that is not a BIOS image fails the boot, naming it.
    std::fs::write(&path, &rom[..0x40000]).unwrap();
    let refused = boot(&disc, &BootOptions { cards: Some(cards.clone()), bios: Some(path.clone()), ..Default::default() }).err().unwrap();
    assert!(refused.to_string().ends_with("a PS1 BIOS image is 512 KiB (524288 bytes), not 262144 bytes"), "{refused}");
    let _ = std::fs::remove_dir_all(cards);
}

/// The file screen's card state (slot39 MenuState and MenuCard, while a
/// menu runs): whether each card is present, its file count, whether its
/// first file is this game's, and whether the "No Memory Card" notice shows
/// (the notice panel open while no card is present and the card message is
/// not, as menu_card_refresh opens it).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct Cards {
    present: [u8; 2],
    files: [u8; 2],
    ours: u8,
    no_card_notice: bool,
}

fn word(s: &mut Session<NativeModule>, at: u32) -> u32 {
    s.runtime.memory().read_u32(at).unwrap()
}

fn bytes<const N: usize>(s: &mut Session<NativeModule>, at: u32) -> [u8; N] {
    let mut out = [0; N];
    s.runtime.memory().read(at, &mut out).unwrap();
    out
}

fn cards_shown(s: &mut Session<NativeModule>) -> Option<Cards> {
    let ram = 0x8000_0000..0x8020_0000;
    // menu_state_current (resident/menu.h MenuState: card +0x32C, flags
    // +0x33C; menu/card.h MenuCard: present +0x4FE4, the file counts
    // +0x4F8A, ours +0x4F8E; menu/screen.h MenuFlags: panels_shown +0x20,
    // card_message_shown +0x33).
    let state = Some(word(s, 0x8006_25A0)).filter(|p| ram.contains(p))?;
    let card = Some(word(s, state + 0x32C)).filter(|p| ram.contains(p))?;
    let flags = Some(word(s, state + 0x33C)).filter(|p| ram.contains(p))?;
    let present = bytes::<2>(s, card + 0x4FE4);
    let [panel] = bytes::<1>(s, flags + 0x22);
    let [message] = bytes::<1>(s, flags + 0x33);
    Some(Cards {
        present,
        files: bytes::<2>(s, card + 0x4F8A),
        ours: bytes::<1>(s, card + 0x4F8E)[0],
        no_card_notice: panel != 0 && message == 0 && present == [0, 0],
    })
}

/// Whether the title screen's commands take the input (no file screen up).
fn title_menu(s: &mut Session<NativeModule>) -> bool {
    let context = s.execute(&json!({ "cmd": "context" }));
    context["kind"] == "menu" && context["commands"] == true && context["open"] == false
}

/// Frames up to `frames` from now: each state the cards showed, in order.
fn watch(s: &mut Session<NativeModule>, frames: u64) -> Vec<Option<Cards>> {
    let mut seen: Vec<Option<Cards>> = Vec::new();
    let end = s.runtime.frames() + frames;
    while s.runtime.frames() < end {
        assert!(s.frame().expect("frame"));
        let now = cards_shown(s);
        if seen.last() != Some(&now) {
            seen.push(now);
        }
    }
    seen
}

fn press_cross(s: &mut Session<NativeModule>) {
    let at = s.runtime.frames();
    ok(s, json!({ "cmd": "schedule", "commands": [[at + 5, { "cmd": "pad", "port": 0, "buttons": ["cross"] }], [at + 13, { "cmd": "pad", "port": 0, "buttons": [] }]] }));
}

fn card_present(s: &mut Session<NativeModule>, slot: usize, present: bool) {
    let status = ok(s, json!({ "cmd": "card_present", "slot": slot, "present": present }));
    assert_eq!(status["card_present"][slot], present);
}

#[test]
fn the_titles_load_finds_no_card_while_both_are_out_and_lists_a_card_put_back() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let cards = scratch("presence");
    std::fs::write(cards.join("card1.mcd"), card_holding(&game_save(0, "Lahan Village"))).unwrap();
    std::fs::write(cards.join("card2.mcd"), formatted()).unwrap();
    let mut s = session(&disc, &cards, None);
    card_present(&mut s, 0, false);
    card_present(&mut s, 1, false);
    ok(&mut s, json!({ "cmd": "schedule", "commands": title_inputs(2755) }));
    run_to(&mut s, 2740);
    assert!(title_menu(&mut s), "the title's commands, Load chosen next");

    // Load: the game checks both cards, finds none, shows its notice and
    // goes back to the title.
    let seen = watch(&mut s, 200);
    let notice = seen.iter().position(|c| c.is_some_and(|c| c.no_card_notice)).expect("the No Memory Card notice");
    assert!(seen[notice..].iter().all(|c| c.is_some_and(|c| c.present == [0, 0] && c.files == [0, 0])), "{seen:?}");
    assert!(!seen.last().unwrap().unwrap().no_card_notice, "the notice closed");
    assert!(title_menu(&mut s), "back at the title's commands");
    assert!(s.runtime.services().card.image(0).unwrap() == card_holding(&game_save(0, "Lahan Village")), "card 1 untouched");

    // Card 1 put back: the Load lists it and its save, and no notice shows.
    card_present(&mut s, 0, true);
    press_cross(&mut s);
    let seen = watch(&mut s, 260);
    assert!(seen.iter().all(|c| c.is_none_or(|c| !c.no_card_notice)), "{seen:?}");
    let listed = seen.last().unwrap().expect("the file screen");
    assert_eq!((listed.present, listed.files[0], listed.ours), ([1, 0], 1, 1), "card 1 listed with this game's save");
    assert!(!title_menu(&mut s), "the file screen stays");

    // Card 2 put in, then card 1 taken out and put back while the file
    // screen shows: the game drops card 1's listing and lists it again.
    card_present(&mut s, 1, true);
    let seen = watch(&mut s, 60);
    assert_eq!(seen.last().unwrap().unwrap().present, [1, 1], "{seen:?}");
    card_present(&mut s, 0, false);
    let seen = watch(&mut s, 60);
    let out = seen.last().unwrap().expect("the file screen");
    assert_eq!((out.present, out.files[0], out.ours), ([0, 1], 0, 0), "card 1's listing dropped: {seen:?}");
    card_present(&mut s, 0, true);
    let seen = watch(&mut s, 60);
    let back = seen.last().unwrap().expect("the file screen");
    assert_eq!((back.present, back.files[0], back.ours), ([1, 1], 1, 1), "card 1 listed anew: {seen:?}");
    assert!(seen.iter().all(|c| c.is_some_and(|c| !c.no_card_notice)), "{seen:?}");
    let _ = std::fs::remove_dir_all(cards);
}
