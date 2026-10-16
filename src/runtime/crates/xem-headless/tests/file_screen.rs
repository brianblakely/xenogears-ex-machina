//! The original game's file screens over the host's Kanji font and cards
//! (local only: it needs the user's disc 1 and a game module build). The
//! title screen's Load lists a card holding a save made here (this game's
//! name, title, digit and checksum as the game writes them; no original
//! content), and its title is drawn, pixel for pixel as slot39
//! menu_save_title_render composes it, from the substitute glyphs without a
//! BIOS image and from a BIOS image's font with one (an image made here,
//! whose glyphs are the substitute's drawn bold). An imported image the
//! check refuses leaves a boot the substitute glyphs and says why, and the
//! native `bios_import` gives a running game an image's font, from which
//! the next title is drawn. With both cards taken out
//! (`card_present`) the Load shows the game's "No Memory Card" notice and
//! returns to the title; a card put back is listed, and at the file screen
//! a card taken out and put back is listed anew, as after a physical swap.
//! A card imported while the Load's file screen shows (`card_import`, as
//! Import original card 1/2) is refused into either slot, naming why, and
//! changes nothing; left, the title takes it, and the Load opened again
//! lists the imported card's saves. An import at each frame of the Load's
//! opening, from before the screen reads the cards until it has listed
//! them, is either refused or listed by the screen as the card it put in
//! (each save's name and title, and their count): taken until the screen
//! begins to read the cards, refused from then on. The inputs to the
//! title's Load are the load slice's (tests/reference-inputs/slice-load.json).
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

/// A one-block save's directory frame: its first block, its size, no next
/// block, its name and the frame's checksum.
fn directory_frame(name: &str, size: usize) -> Vec<u8> {
    let mut frame = vec![0; 0x80];
    frame[0] = 0x51;
    frame[4..8].copy_from_slice(&(size as u32).to_le_bytes());
    frame[8..10].copy_from_slice(&0xFFFFu16.to_le_bytes());
    frame[0x0A..0x0A + name.len()].copy_from_slice(name.as_bytes());
    frame[0x7F] = frame[..0x7F].iter().fold(0, |a, b| a ^ b);
    frame
}

/// A freshly formatted card holding `save` (as an imported `.mcs` becomes).
fn card_holding((name, block): &(String, Vec<u8>)) -> Vec<u8> {
    card_file::detect(&[directory_frame(name, block.len()), block.clone()].concat(), "save.mcs").expect("a single save").1
}

/// A freshly formatted card holding `saves`, the first in block 1, the
/// next in block 2 and so on.
fn card_with(saves: &[(String, Vec<u8>)]) -> Vec<u8> {
    let mut image = card_holding(&saves[0]);
    for (block, (name, data)) in saves.iter().enumerate().skip(1).map(|(i, save)| (i + 1, save)) {
        image[block * 0x80..(block + 1) * 0x80].copy_from_slice(&directory_frame(name, data.len()));
        image[block * BLOCK_SIZE..(block + 1) * BLOCK_SIZE].copy_from_slice(data);
    }
    image
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

fn font_status(s: &mut Session<NativeModule>) -> (Value, Option<String>) {
    let font = s.execute(&json!({ "cmd": "status" }))["kanji_font"].clone();
    (font, s.runtime.services().kanji_font_notice.clone())
}

#[test]
fn an_imported_bios_the_check_refuses_is_reported_and_bios_import_gives_the_running_game_a_font() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let save = game_save(0, "Lahan Village");
    let title = save.1[4..4 + 0x5C].to_vec();
    let cards = scratch("imported");
    std::fs::write(cards.join("card1.mcd"), card_holding(&save)).unwrap();
    let rom = bold_bios();
    let stored = cards.join("bios.bin");
    let imported = |stored: &Path| BootOptions { cards: Some(cards.clone()), stored_bios: Some(stored.to_path_buf()), ..Default::default() };

    // The image Import BIOS kept gives a boot its font.
    std::fs::write(&stored, &rom).unwrap();
    {
        let mut s = boot(&disc, &imported(&stored)).expect("boot").0;
        assert_eq!(font_status(&mut s), (json!("bios"), None));
    }

    // Damaged since: the boot goes on with the substitute glyphs, says why
    // (the storage line), and leaves the file as it is.
    std::fs::write(&stored, &rom[..0x40000]).unwrap();
    let mut s = boot(&disc, &imported(&stored)).expect("a boot despite the image").0;
    let notice = format!(
        "Imported BIOS image {} not used: a PS1 BIOS image is 512 KiB (524288 bytes), not 262144 bytes; save titles use the substitute glyphs.",
        stored.display()
    );
    assert_eq!(font_status(&mut s), (json!("substitute"), Some(notice.clone())));
    assert!(std::fs::read(&stored).unwrap() == rom[..0x40000], "the file left as it is");

    // bios_import before the Load's cursor reaches the save: a file that is
    // not an image is refused by name, changing nothing; an image's font is
    // the running game's at once (the notice ends), and the next title is
    // drawn from it.
    ok(&mut s, json!({ "cmd": "schedule", "commands": title_inputs(3004) }));
    run_to(&mut s, 2900);
    assert!(drawn_title(&mut s).iter().all(|&p| p == 0), "no title yet");
    let refused = xem_game::storage::command(&mut s, &json!({ "cmd": "bios_import", "path": stored }));
    assert_eq!(refused, json!({ "error": "BIOS not imported: a PS1 BIOS image is 512 KiB (524288 bytes), not 262144 bytes." }));
    assert_eq!(font_status(&mut s), (json!("substitute"), Some(notice)));
    let image = cards.join("image.bin");
    std::fs::write(&image, &rom).unwrap();
    let reply = xem_game::storage::command(&mut s, &json!({ "cmd": "bios_import", "path": image }));
    assert_eq!(reply, json!({ "ok": true, "message": "BIOS imported: save titles use its Kanji font.", "kanji_font": "bios" }));
    assert_eq!(font_status(&mut s), (json!("bios"), None));
    run_to(&mut s, 3040);
    assert!(drawn_title(&mut s) == title_image(&title, &rom[KANJI_FONT_ROM_OFFSET..]), "the next title from the imported font");
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

#[test]
fn a_card_import_waits_while_the_file_screen_shows_and_the_screen_lists_the_imported_card_after() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let cards = scratch("import");
    let card1 = card_holding(&game_save(0, "Lahan Village"));
    std::fs::write(cards.join("card1.mcd"), &card1).unwrap();
    std::fs::write(cards.join("card2.mcd"), formatted()).unwrap();
    let file = cards.join("imported.mcd");
    std::fs::write(&file, card_with(&[game_save(0, "Lahan Village"), game_save(1, "Mountain Path")])).unwrap();
    let mut s = session(&disc, &cards, None);
    ok(&mut s, json!({ "cmd": "schedule", "commands": title_inputs(3004) }));
    run_to(&mut s, 3040);
    let listed = cards_shown(&mut s).expect("the title's Load");
    assert_eq!((listed.present, listed.files[0], listed.ours), ([1, 1], 1, 1), "card 1's one save listed");

    // An import into either card while the file screen shows is refused,
    // naming why (`card_list` says it too), and changes nothing.
    for slot in [0, 1] {
        let refused = xem_game::storage::command(&mut s, &json!({ "cmd": "card_import", "slot": slot, "path": file }));
        let reason = format!("memory card {} not changed: the game's file screen shows the cards' saves and would act on the ones it listed; leave it, then try again", slot + 1);
        assert_eq!(refused, json!({ "error": reason }));
        let busy = ok(&mut s, json!({ "cmd": "card_list", "slot": slot }))["busy"].clone();
        assert!(busy.as_str().is_some_and(|b| b.contains("file screen")), "{busy}");
    }
    assert!(s.runtime.services().card.image(0).unwrap() == card1 && std::fs::read(cards.join("card1.mcd")).unwrap() == card1, "card 1 as it was");
    assert!(std::fs::read(cards.join("card2.mcd")).unwrap() == formatted(), "card 2 as it was");

    // Circle takes the cursor off the saves to the screen's commands, where
    // the import still waits; Circle again leaves the screen, the title's
    // commands take the input, and the import goes through. The Load
    // opened again lists the imported card's saves.
    let press_circle = |s: &mut Session<NativeModule>| {
        let at = s.runtime.frames();
        ok(s, json!({ "cmd": "schedule", "commands": [[at + 5, { "cmd": "pad", "port": 0, "buttons": ["circle"] }], [at + 13, { "cmd": "pad", "port": 0, "buttons": [] }]] }));
        run_to(s, at + 60);
    };
    press_circle(&mut s);
    assert!(cards_shown(&mut s).is_some() && !title_menu(&mut s), "the file screen's commands");
    let refused = xem_game::storage::command(&mut s, &json!({ "cmd": "card_import", "slot": 0, "path": file }));
    assert!(refused["error"].as_str().is_some_and(|e| e.contains("file screen")), "{refused}");
    press_circle(&mut s);
    let mut left = title_menu(&mut s);
    for _ in 0..300 {
        if left {
            break;
        }
        assert!(s.frame().expect("frame"));
        left = title_menu(&mut s);
    }
    assert!(left, "back at the title's commands");
    let reply = xem_game::storage::command(&mut s, &json!({ "cmd": "card_import", "slot": 0, "path": file }));
    assert_eq!(reply["message"], json!("Card 1: raw card, 2 saves"), "{reply}");
    press_cross(&mut s);
    let seen = watch(&mut s, 260);
    let again = seen.last().unwrap().expect("the file screen");
    assert_eq!((again.present, again.files[0], again.ours), ([1, 1], 2, 1), "the imported card's two saves listed: {seen:?}");
    let _ = std::fs::remove_dir_all(cards);
}

/// Card 1 as the file screen lists it (slot39 `MenuCard` while a menu runs:
/// the files its directory listing found, +0x4F8A, each file's name, +0x18
/// of its 0x5C-byte entry, and its title, bytes 4..0x60 of the first block
/// read into its 0x200-byte head from +0xB94), and whether the screen polls
/// the cards (`MenuCard.mode` +0x4FE6) and its card mode holds
/// (`MenuFlags.card_mode` +0x68).
fn card1_listed(s: &mut Session<NativeModule>) -> Option<(Vec<(String, Vec<u8>)>, bool, bool)> {
    let ram = 0x8000_0000..0x8020_0000;
    let state = Some(word(s, 0x8006_25A0)).filter(|p| ram.contains(p))?;
    let card = Some(word(s, state + 0x32C)).filter(|p| ram.contains(p))?;
    let flags = Some(word(s, state + 0x33C)).filter(|p| ram.contains(p))?;
    let [count] = bytes::<1>(s, card + 0x4F8A);
    let files = (0..u32::from(count))
        .map(|i| {
            let name = bytes::<21>(s, card + i * 0x5C + 0x18);
            let name = String::from_utf8_lossy(&name[..name.iter().position(|&b| b == 0).unwrap_or(21)]).into_owned();
            (name, bytes::<0x5C>(s, card + 0xB94 + i * 0x200 + 4).to_vec())
        })
        .collect();
    Some((files, bytes::<1>(s, card + 0x4FE6)[0] != 0, bytes::<1>(s, flags + 0x68)[0] != 0))
}

#[test]
fn an_import_at_each_frame_of_the_loads_opening_is_refused_or_listed_as_the_card_it_put_in() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let cards = scratch("opening");
    let lahan = game_save(0, "Lahan Village");
    let card1 = card_holding(&lahan);
    std::fs::write(cards.join("card1.mcd"), &card1).unwrap();
    std::fs::write(cards.join("card2.mcd"), formatted()).unwrap();
    // Another card: a save of the same name with another title, and a second.
    let other = [game_save(0, "Mountain Path"), game_save(1, "Shevat")];
    let file = cards.join("other.mcd");
    std::fs::write(&file, card_with(&other)).unwrap();
    let listing = |saves: &[(String, Vec<u8>)]| saves.iter().map(|(name, block)| (name.clone(), block[4..4 + 0x5C].to_vec())).collect::<Vec<_>>();
    let mut s = session(&disc, &cards, None);
    // The title's Load chosen (its press let go at 2754): the screen opens.
    ok(&mut s, json!({ "cmd": "schedule", "commands": title_inputs(2755) }));
    run_to(&mut s, 2775);
    let opening = s.snapshot_bytes().unwrap();
    assert!(card1_listed(&mut s).is_some_and(|(_, polls, card_mode)| !polls && !card_mode), "the screen has not read the cards yet");

    // From that moment, an import at each frame from before the screen
    // reads the cards to after it has listed them: once the screen has
    // settled, it lists the card the slot holds, whichever was taken.
    let mut seen = Vec::new();
    for frame in 2775..2810 {
        s.restore_bytes(&opening).unwrap();
        let now = s.runtime.services().clock.now;
        s.runtime.services().card.change(0, card1.clone(), now).expect("card 1 back before the screen reads it");
        run_to(&mut s, frame);
        let (_, polls, card_mode) = card1_listed(&mut s).expect("the title's menu");
        let reply = xem_game::storage::command(&mut s, &json!({ "cmd": "card_import", "slot": 0, "path": file }));
        let took = reply.get("error").is_none();
        assert!(took || reply["error"].as_str().is_some_and(|e| e.starts_with("memory card 1 not changed: ")), "{reply}");
        run_to(&mut s, 2860);
        let (listed, ..) = card1_listed(&mut s).expect("the Load's listing");
        let expected = if took { listing(&other) } else { listing(std::slice::from_ref(&lahan)) };
        assert!(listed == expected, "an import at frame {frame} ({reply}): the screen lists {listed:?}");
        seen.push((frame, took, polls, card_mode));
    }
    // The frames spanned the opening: imports taken until the screen began
    // to read the cards, and refused from then on, before its card mode too.
    eprintln!("(frame, taken, polling, card mode): {seen:?}");
    assert!(seen.iter().any(|&(_, took, polls, _)| took && !polls), "{seen:?}");
    assert!(seen.iter().any(|&(_, took, polls, card_mode)| !took && polls && !card_mode), "{seen:?}");
    assert!(seen.iter().all(|&(_, took, polls, card_mode)| took == !(polls || card_mode)), "{seen:?}");
    let _ = std::fs::remove_dir_all(cards);
}
