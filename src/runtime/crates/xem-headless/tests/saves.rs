//! Ordinary saves through the original game's own menus, on both discs
//! (local only: the user's two discs and a game module build). Every save
//! here is the game's: its file screens (slot39 `menu_save_command_run`,
//! `menu_copy_command_run`, `menu_delete_command_run`) over the card device,
//! driven by pad presses along the cursor paths their screens show, and
//! checked by `card_list` (each file's name, digit, disc and blocks, the
//! game's own payload checksum and the directory's checks) and the cards'
//! bytes:
//!
//! - after the slice's save at field 22 (tests/reference-inputs/slice-save.json)
//!   the same save point saves a second file (No.02: the first free digit, in
//!   the first free block), overwrites No.01 in its block, deletes No.02,
//!   copies No.01 to card 1 and, asked again, refuses that copy (card 1 has a
//!   file of that name) without writing;
//! - on the world map (tests/reference-inputs/world-map-lahan-exit.json: out
//!   of Lahan by its south exit) the menu's file screen finds no card while
//!   both are out (`card_present`), and with them in, once the party has
//!   walked on south, it saves to card 1; the title's Load takes that save
//!   back onto the world map where it was made, not where the party came out
//!   of Lahan;
//! - the story's disc change (its menu, kind 6, requested as the last event of
//!   disc 1 requests it, discs.rs) offers a save, whose payload carries the
//!   disc to load it from (vars\[82\] = 1: disc 2); loaded from disc 1's
//!   title, the changer puts disc 2 in at once and the game reads disc 2's
//!   index;
//! - a save point on disc 2 (map 687's, where tools/memcard_routes.json's
//!   "first-and-last" route saves; the map entered as the development kernel
//!   menu does, `enter_field`) saves to card 1, and disc 2's title loads it
//!   there.
//!
//! The title's Load with no card is file_screen.rs's. The title's inputs here
//! are the load slice's (tests/reference-inputs/slice-load.json), the same on
//! both discs.
//!
//!     XEM_DISC1=<disc 1> XEM_DISC2=<disc 2> cargo test --release -p xem-headless --test saves

#![cfg(all(has_game_module, not(debug_assertions)))]

use std::path::{Path, PathBuf};

use serde_json::{Value, json};
use xem_core::context::Context;
use xem_core::control::Session;
use xem_core::devices::card::{BLOCK_SIZE, CARD_SIZE, FRAME_SIZE};
use xem_game::{BootOptions, NativeModule, boot};

fn disc(number: u8) -> Option<PathBuf> {
    let path = std::env::var_os(format!("XEM_DISC{number}")).map(PathBuf::from).unwrap_or_else(|| {
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join(format!("../../../../discs/Xenogears disc {number}.chd"))
    });
    path.exists().then_some(path)
}

/// Both discs, side by side (each session has the other beside the one it boots).
fn discs() -> Option<(PathBuf, PathBuf)> {
    Some((disc(1)?, disc(2)?))
}

fn scratch(name: &str) -> PathBuf {
    let dir = std::env::temp_dir().join(format!("xem-saves-{name}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();
    dir
}

fn session(disc: &Path, cards: &Path) -> Session<NativeModule> {
    boot(disc, &BootOptions { cards: Some(cards.to_path_buf()), ..Default::default() }).expect("boot").0
}

fn ok(s: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = s.execute(&command);
    assert!(reply.get("error").is_none(), "{command}: {reply}");
    reply
}

/// A session log from tests/reference-inputs.
fn log(name: &str) -> Value {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../tests/reference-inputs").join(name);
    serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap()
}

fn step(s: &mut Session<NativeModule>) {
    assert!(s.frame().expect("frame"), "a native disc never blocks");
}

/// Run to frame `to`, counted from boot.
fn run_to(s: &mut Session<NativeModule>, to: u64) {
    while s.runtime.frames() < to {
        step(s);
    }
}

fn frames(s: &mut Session<NativeModule>, count: u64) {
    for _ in 0..count {
        step(s);
    }
}

fn hash(s: &mut Session<NativeModule>) -> String {
    format!("{:016x}", s.memory_hash().unwrap())
}

/// Hold `button` for six frames, then run `after` frames more.
fn press(s: &mut Session<NativeModule>, button: &str, after: u64) {
    ok(s, json!({ "cmd": "press", "buttons": [button], "frames": 6 }));
    frames(s, 6 + after);
}

/// Run until `done` (at most `max` frames), saying what was awaited if not.
fn until(s: &mut Session<NativeModule>, max: u64, what: &str, mut done: impl FnMut(&mut Session<NativeModule>) -> bool) {
    for _ in 0..max {
        if done(s) {
            return;
        }
        step(s);
    }
    let context = s.runtime.context();
    panic!("{what}: not within {max} frames (frame {}, {context:?})", s.runtime.frames());
}

fn word(s: &mut Session<NativeModule>, at: u32) -> u32 {
    s.runtime.memory().read_u32(at).unwrap()
}

fn byte(s: &mut Session<NativeModule>, at: u32) -> u8 {
    let mut out = [0];
    s.runtime.memory().read(at, &mut out).unwrap();
    out[0]
}

/// The menu's state (slot39 `menu_state_current`, resident/menu.h MenuState).
const MENU_STATE_CURRENT: u32 = 0x8006_25A0;

fn ram(pointer: u32) -> Option<u32> {
    (0x8000_0000..0x8020_0000).contains(&pointer).then_some(pointer)
}

/// The menu's flags (MenuState +0x33C, menu/screen.h MenuFlags) and card
/// state (MenuState +0x32C, menu/card.h MenuCard) while a menu runs.
fn menu_blocks(s: &mut Session<NativeModule>) -> Option<(u32, u32)> {
    let state = ram(word(s, MENU_STATE_CURRENT))?;
    Some((ram(word(s, state + 0x33C))?, ram(word(s, state + 0x32C))?))
}

/// Whether a menu's commands take the pad (a field's, the world map's or
/// the title's).
fn commands(s: &mut Session<NativeModule>) -> bool {
    matches!(s.runtime.context(), Context::Menu { commands: true, .. })
}

fn until_commands(s: &mut Session<NativeModule>, what: &str) {
    until(s, 900, what, commands);
    frames(s, 4);
}

/// The file screen's cursor over both cards (MenuCard.cursor +0x4F7C):
/// column * 3 + row, card 2's boxes from 15. Each card shows its 15 boxes in
/// five columns of three, its files in the directory's order down each
/// column; Right moves a column, Down a row.
fn file_cursor(s: &mut Session<NativeModule>) -> u32 {
    let (_, card) = menu_blocks(s).expect("a file screen");
    word(s, card + 0x4F7C)
}

fn cursor_to(s: &mut Session<NativeModule>, to: u32) {
    for _ in 0..20 {
        let at = file_cursor(s);
        if at == to {
            return;
        }
        let button = match (at % 15 % 3).cmp(&(to % 15 % 3)) {
            std::cmp::Ordering::Less => "down",
            std::cmp::Ordering::Greater => "up",
            std::cmp::Ordering::Equal if at < to => "right",
            std::cmp::Ordering::Equal => "left",
        };
        press(s, button, 24);
    }
    panic!("the file cursor stays at {} short of {to}", file_cursor(s));
}

/// The menu's File (its cursor on it, as the save point and the world map
/// open the menu): the file screen, once it has checked the cards.
fn file_screen(s: &mut Session<NativeModule>) {
    press(s, "cross", 250);
}

/// One of the file screen's commands, Save, Copy or Delete (downward):
/// the cursor then stands on the boxes.
fn command(s: &mut Session<NativeModule>, downs: usize) {
    for _ in 0..downs {
        press(s, "down", 24);
    }
    press(s, "cross", 60);
}

/// Yes to a notice's question, whose cursor starts on No.
fn yes(s: &mut Session<NativeModule>, after: u64) {
    press(s, "left", 24);
    press(s, "cross", after);
}

const SAVE: usize = 0;
const COPY: usize = 1;
const DELETE: usize = 2;

/// Whether the menu's notice (panel 2) shows (MenuFlags.panels_shown[2],
/// +0x22), and its text as menu_notice_open loads it: the label pixels of its
/// lines at VRAM x 0x140, rows 0x4E-0x67 (0x3A halfwords wide).
fn notice(s: &mut Session<NativeModule>) -> (bool, Vec<u16>) {
    let shown = menu_blocks(s).is_some_and(|(flags, _)| byte(s, flags + 0x22) != 0);
    let vram = s.runtime.services().gpu.vram();
    (shown, (0x4E..0x68).flat_map(|y| (0x140..0x17A).map(move |x| vram[y * 1024 + x])).collect())
}

fn card_image(s: &mut Session<NativeModule>, slot: usize) -> Vec<u8> {
    s.runtime.services().card.image(slot).expect("a card").to_vec()
}

fn block(image: &[u8], block: usize) -> &[u8] {
    &image[block * BLOCK_SIZE..(block + 1) * BLOCK_SIZE]
}

/// A save as `card_list` lists it and the card's bytes confirm it.
#[derive(Debug, PartialEq, Eq)]
struct Listed {
    name: String,
    first_block: u64,
    /// The file the game shows (its digit + 1).
    file: u64,
    /// The disc the game asks for after loading it (vars\[82\] + 1).
    disc: u64,
}

fn listed(name: &str, first_block: u64, file: u64, disc: u64) -> Listed {
    Listed { name: name.into(), first_block, file, disc }
}

/// Card `slot`'s saves, every one this game's single-block save: none of
/// `card_list`'s problems (the directory frame's checksum, the chain and
/// size, the title frame, the digit, the payload's checksum, the disc), and
/// the directory frame and the payload's checksum read again from the bytes.
fn saves(s: &mut Session<NativeModule>, slot: usize) -> Vec<Listed> {
    let reply = ok(s, json!({ "cmd": "card_list", "slot": slot }));
    let card = &reply["card"];
    assert_eq!((&card["formatted"], &card["problems"]), (&json!(true), &json!([])), "card {}: {reply}", slot + 1);
    let image = card_image(s, slot);
    assert_eq!(image.len(), CARD_SIZE);
    card["saves"]
        .as_array()
        .unwrap()
        .iter()
        .map(|save| {
            assert_eq!(save["problems"], json!([]), "{save}");
            assert_eq!(save["blocks"], 1, "{save}");
            assert_eq!(save["kind"]["game"], "xenogears", "{save}");
            let name = save["name"].as_str().unwrap().to_string();
            let first = save["first_block"].as_u64().unwrap();
            let frame = &image[first as usize * FRAME_SIZE..][..FRAME_SIZE];
            assert_eq!(frame[0], 0x51, "{name}: a first block");
            assert_eq!(u32::from_le_bytes(frame[4..8].try_into().unwrap()), BLOCK_SIZE as u32, "{name}: one block");
            assert_eq!(u16::from_le_bytes(frame[8..10].try_into().unwrap()), 0xFFFF, "{name}: no next block");
            assert_eq!(&frame[0x0A..0x0A + name.len() + 1], [name.as_bytes(), &[0]].concat(), "{name}");
            assert_eq!(frame[0x7F], frame[..0x7F].iter().fold(0, |a, b| a ^ b), "{name}: the frame's checksum");
            let data = block(&image, first as usize);
            assert_eq!(&data[..2], b"SC", "{name}: a title frame");
            assert_eq!(data[0x1FFF], data[0x100..0x1FFF].iter().fold(0u8, |a, &b| a.wrapping_add(b)), "{name}: the payload's sum");
            Listed { name, first_block: first, file: save["kind"]["file"].as_u64().unwrap(), disc: save["kind"]["disc"].as_u64().unwrap() }
        })
        .collect()
}

/// A save's play time (its summary, payload +0) in vertical blanks.
fn play_time(image: &[u8], at: usize) -> u32 {
    u32::from_le_bytes(block(image, at)[0x100..0x104].try_into().unwrap())
}

/// The title's Load of card 1's file 1 (the load slice's inputs), from a
/// fresh boot of `disc` with `card` as card 1, until `arrived`.
fn title_load(disc: &Path, card: &[u8], name: &str, arrived: impl FnMut(&mut Session<NativeModule>) -> bool) -> Session<NativeModule> {
    let cards = scratch(name);
    std::fs::write(cards.join("card1.mcd"), card).unwrap();
    let mut s = session(disc, &cards);
    ok(&mut s, json!({ "cmd": "schedule", "commands": log("slice-load.json")["commands"] }));
    run_to(&mut s, 3330);
    until(&mut s, 3000, "the title's load", arrived);
    s
}

fn inspect(s: &mut Session<NativeModule>, path: &str, depth: u32) -> Value {
    let reply = s.execute(&json!({ "cmd": "inspect", "path": path, "depth": depth }));
    assert!(reply.get("error").is_none(), "{path}: {reply}");
    reply
}

#[test]
fn the_save_point_saves_overwrites_deletes_and_copies_and_refuses_a_copy_the_other_card_has() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // The slice's save: card 2 unformatted, formatted by the game, its No.01
    // in block 1; the player at field 22's save point.
    let save = log("slice-save.json");
    let cards = scratch("field-22");
    std::fs::write(cards.join("card2.mcd"), vec![0u8; CARD_SIZE]).unwrap();
    let mut s = session(&disc, &cards);
    ok(&mut s, json!({ "cmd": "schedule", "commands": save["commands"] }));
    run_to(&mut s, save["frames"].as_u64().unwrap());
    assert_eq!(hash(&mut s), save["memory_hash"].as_str().unwrap(), "the slice's save");
    assert_eq!(saves(&mut s, 1), [listed("BASLUS-006640", 1, 1, 1)]);
    let slice = card_image(&mut s, 1);

    // A second save: the save point's menu (its cursor on File), Save, and a
    // free box (card 2's fourth: column 1, row 0): "This block is free.
    // Save?". The game takes the first free digit and the first free block.
    press(&mut s, "cross", 0);
    until_commands(&mut s, "the save point's menu");
    file_screen(&mut s);
    command(&mut s, SAVE);
    assert_eq!(file_cursor(&mut s), 0, "Save starts on card 1's first box");
    cursor_to(&mut s, 18);
    press(&mut s, "cross", 90);
    yes(&mut s, 0);
    until_commands(&mut s, "the field menu after the second save");
    assert_eq!(saves(&mut s, 1), [listed("BASLUS-006640", 1, 1, 1), listed("BASLUS-006641", 2, 2, 1)]);
    let second = card_image(&mut s, 1);
    assert_eq!(block(&second, 1), block(&slice, 1), "No.01 untouched");
    assert_eq!(saves(&mut s, 0), [], "card 1 untouched");

    // Over No.01 (card 2's first box): "Overwrite previous file?". The game
    // deletes it and saves again, into the block it freed, as No.01.
    file_screen(&mut s);
    command(&mut s, SAVE);
    cursor_to(&mut s, 15);
    press(&mut s, "cross", 90);
    yes(&mut s, 0);
    until_commands(&mut s, "the field menu after the overwrite");
    assert_eq!(saves(&mut s, 1), [listed("BASLUS-006640", 1, 1, 1), listed("BASLUS-006641", 2, 2, 1)]);
    let overwritten = card_image(&mut s, 1);
    assert!(play_time(&overwritten, 1) > play_time(&second, 1), "No.01 is the later save now");
    assert_eq!(block(&overwritten, 2), block(&second, 2), "No.02 untouched");

    // Delete No.02 (card 2's second box): "Delete this file?", then "Are
    // you sure you want to delete?" (Delete or Exit).
    file_screen(&mut s);
    command(&mut s, DELETE);
    cursor_to(&mut s, 16);
    press(&mut s, "cross", 90);
    yes(&mut s, 90);
    yes(&mut s, 0);
    until_commands(&mut s, "the field menu after the delete");
    assert_eq!(saves(&mut s, 1), [listed("BASLUS-006640", 1, 1, 1)]);
    let deleted = card_image(&mut s, 1);
    assert_eq!(deleted[2 * FRAME_SIZE], 0xA1, "block 2 holds a deleted file's first block");
    assert_eq!(block(&deleted, 1), block(&overwritten, 1), "No.01 untouched");

    // Copy No.01 to card 1: "Copy this file to the Memory Card in Slot 1?".
    // The copy is the same file: its directory frame and its block.
    file_screen(&mut s);
    command(&mut s, COPY);
    assert_eq!(file_cursor(&mut s), 15, "Copy starts on the first file");
    press(&mut s, "cross", 90);
    let question = notice(&mut s);
    assert!(question.0, "the copy's question");
    yes(&mut s, 0);
    let mut copying = None;
    until(&mut s, 900, "the field menu after the copy", |s| {
        let shown = notice(s);
        if shown.0 && shown.1 != question.1 && copying.is_none() {
            copying = Some(shown.1);
        }
        commands(s)
    });
    let copying = copying.expect("a notice while the game copies");
    frames(&mut s, 4);
    assert_eq!(saves(&mut s, 0), [listed("BASLUS-006640", 1, 1, 1)]);
    let (card1, card2) = (card_image(&mut s, 0), card_image(&mut s, 1));
    assert_eq!(&card1[FRAME_SIZE..2 * FRAME_SIZE], &card2[FRAME_SIZE..2 * FRAME_SIZE], "the same directory frame");
    assert_eq!(block(&card1, 1), block(&card2, 1), "the same save");
    assert_eq!(card2, deleted, "card 2 untouched");

    // Copy it again: the game answers with a third notice, neither the
    // question nor its copying ("The same file exists on the Memory Card in
    // Slot 1. Cannot copy.", message 0x4d, as screenshots show), goes back to
    // the menu by itself and writes nothing.
    file_screen(&mut s);
    command(&mut s, COPY);
    assert_eq!(file_cursor(&mut s), 0, "Copy starts on the first file, card 1's now");
    cursor_to(&mut s, 15);
    press(&mut s, "cross", 90);
    assert_eq!(notice(&mut s), question, "the same question");
    yes(&mut s, 120);
    let refusal = notice(&mut s);
    assert!(refusal.0, "a notice");
    assert!(refusal.1 != question.1 && refusal.1 != copying, "the refusal's own text");
    until_commands(&mut s, "the field menu after the refusal");
    assert_eq!((card_image(&mut s, 0), card_image(&mut s, 1)), (card1, card2), "nothing written");
    // The cards on disk are the device's.
    assert_eq!(std::fs::read(cards.join("card1.mcd")).unwrap(), card_image(&mut s, 0));
    assert_eq!(std::fs::read(cards.join("card2.mcd")).unwrap(), card_image(&mut s, 1));
    let _ = std::fs::remove_dir_all(cards);
}

/// On the world map out of Lahan's south exit, with `cards`.
fn world_map(disc: &Path, cards: &Path) -> Session<NativeModule> {
    let route = log("world-map-lahan-exit.json");
    let mut s = session(disc, cards);
    ok(&mut s, json!({ "cmd": "schedule", "commands": route["commands"] }));
    run_to(&mut s, route["frames"].as_u64().unwrap());
    assert_eq!(hash(&mut s), route["memory_hash"].as_str().unwrap(), "the route's end");
    assert!(matches!(s.runtime.context(), Context::WorldMap { movement: 1, .. }), "on foot on the world map");
    s
}

/// The world map's menu: its button 0x10 (`worldmap_steer_on_foot`), Square
/// with the default buttons, away from a place's name.
fn world_map_menu(s: &mut Session<NativeModule>) {
    press(s, "square", 60);
    until_commands(s, "the world map's menu");
}

#[test]
fn the_world_maps_file_screen_finds_no_card_while_both_are_out() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let cards = scratch("no-card");
    let mut s = world_map(&disc, &cards);
    let images = (card_image(&mut s, 0), card_image(&mut s, 1));
    for slot in 0..2 {
        ok(&mut s, json!({ "cmd": "card_present", "slot": slot, "present": false }));
    }
    world_map_menu(&mut s);
    // File: the game checks both cards, finds none and says so (notice
    // 0x23, `menu_card_refresh`: the notice up, both cards absent, no card
    // message), then the menu takes the pad again.
    press(&mut s, "cross", 0);
    let mut seen = false;
    until(&mut s, 900, "the menu after the no-card notice", |s| {
        if let Some((flags, card)) = menu_blocks(s) {
            let absent = byte(s, card + 0x4FE4) == 0 && byte(s, card + 0x4FE5) == 0;
            seen |= byte(s, flags + 0x22) != 0 && byte(s, flags + 0x33) == 0 && absent;
        }
        seen && commands(s)
    });
    assert!(seen, "the No Memory Card notice");
    assert_eq!((card_image(&mut s, 0), card_image(&mut s, 1)), images, "nothing written");
    let _ = std::fs::remove_dir_all(cards);
}

#[test]
fn a_world_map_save_loads_from_the_title_onto_the_world_map() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // Card 1 a fresh formatted card (no card file yet). The party walks
    // south first, away from where it came out of Lahan.
    let cards = scratch("world-map");
    let mut s = world_map(&disc, &cards);
    let arrived = inspect(&mut s, "game_data.worldmap", 1);
    ok(&mut s, json!({ "cmd": "press", "buttons": ["down"], "frames": 300 }));
    frames(&mut s, 360);
    world_map_menu(&mut s);
    file_screen(&mut s);
    command(&mut s, SAVE);
    assert_eq!(file_cursor(&mut s), 0);
    press(&mut s, "cross", 90);
    yes(&mut s, 0);
    until_commands(&mut s, "the world map's menu after the save");
    assert_eq!(saves(&mut s, 0), [listed("BASLUS-006640", 1, 1, 1)]);
    // Where the save puts the party (game_data.worldmap: its place, its
    // heading, the vehicle's; the title's before a load are zero): where it
    // walked to, not where it arrived.
    let place = inspect(&mut s, "game_data.worldmap", 1);
    assert_ne!(place, arrived, "the party walked on from where it came out of Lahan");
    // Out of the menu, the party walks on there.
    press(&mut s, "circle", 200);
    assert_eq!(inspect(&mut s, "worldmap_menu_requested", 0), json!(0));
    assert!(matches!(s.runtime.context(), Context::WorldMap { movement: 1, .. }));
    let card = card_image(&mut s, 0);
    drop(s);

    let mut loaded = title_load(&disc, &card, "world-map-load", |s| {
        s.runtime.services().mode == Some(3) && matches!(s.runtime.context(), Context::WorldMap { movement: 1, .. })
    });
    assert_eq!(inspect(&mut loaded, "game_data.worldmap", 1), place, "the world map where it was saved");
    let _ = std::fs::remove_dir_all(cards);
}

#[test]
fn the_story_disc_changes_save_asks_for_disc_2_and_loading_it_on_disc_1_puts_disc_2_in() {
    let Some((disc1, _)) = discs() else {
        eprintln!("skipping: needs both discs (XEM_DISC1, XEM_DISC2), side by side");
        return;
    };
    let cards = scratch("disc-change");
    let mut s = session(&disc1, &cards);
    ok(&mut s, json!({ "cmd": "enter_field", "map": 23 }));
    frames(&mut s, 600);
    // The menu kind the story's last event of disc 1 requests (field event
    // fe 56): field_menu_request (field_mode.h, 0x800ADB64 in the field
    // overlay; not in the inspection schema).
    s.runtime.memory().write(0x800A_DB64, &6i32.to_le_bytes()).unwrap();
    // "Save?" (Yes first), the file screen, Save to card 1's first box.
    frames(&mut s, 180);
    press(&mut s, "cross", 250);
    command(&mut s, SAVE);
    assert_eq!(file_cursor(&mut s), 0);
    press(&mut s, "cross", 90);
    yes(&mut s, 350);
    assert_eq!(saves(&mut s, 0), [listed("BASLUS-006640", 1, 1, 2)], "the save asks for disc 2");
    let card = card_image(&mut s, 0);
    assert_eq!(u16::from_le_bytes(block(&card, 1)[0x12D8..0x12DA].try_into().unwrap()), 1, "vars[82]");
    // Out of the file screen: the change asks for disc 2, which goes in
    // after the story's five seconds.
    press(&mut s, "circle", 0);
    until(&mut s, 600, "the story's change", |s| s.runtime.services().cd.change_due().is_some());
    until(&mut s, 400, "disc 2 in", |s| s.runtime.services().cd.inserted() == 2);
    drop(s);

    // Loaded from disc 1's title (disc 2 beside it): the game asks for
    // disc 2 and the changer puts it in at once, no wait.
    let mut loaded = title_load(&disc1, &card, "disc-change-load", |s| {
        assert_eq!(s.runtime.services().cd.change_due(), None, "no story wait");
        s.runtime.services().cd.inserted() == 2
    });
    until(&mut loaded, 900, "disc 2's index and the save's field", |s| {
        s.runtime.services().mode == Some(1)
            && s.execute(&json!({"cmd": "inspect", "path": "cd_directory_table[60]", "depth": 0})) == json!(2)
            && s.execute(&json!({"cmd": "inspect", "path": "mode_field_map_id", "depth": 0})).as_u64().is_some_and(|m| m & 0xFFF == 23)
    });
    assert_eq!(loaded.runtime.services().cd.wanted(), None);
    let _ = std::fs::remove_dir_all(cards);
}

#[test]
fn a_save_at_a_disc_2_save_point_loads_from_disc_2s_title() {
    let Some((_, disc2)) = discs() else {
        eprintln!("skipping: needs both discs (XEM_DISC1, XEM_DISC2), side by side");
        return;
    };
    let cards = scratch("disc-2");
    let mut s = session(&disc2, &cards);
    ok(&mut s, json!({ "cmd": "enter_field", "map": 687, "entry": 1 }));
    frames(&mut s, 600);
    // Map 687's save point (actor 18 at x 130, z 22: its talk event opens
    // menu kind 0), approached from the south round the room's corner.
    for to in [[200, 220], [130, 40]] {
        ok(&mut s, json!({ "cmd": "navigate", "to": to }));
        until(&mut s, 900, "the walk to the save point", |s| s.assist.is_none());
        assert_eq!(s.assist_outcome, Some(xem_core::assist::Outcome::Arrived), "at {to:?}");
    }
    press(&mut s, "cross", 0);
    until_commands(&mut s, "the save point's menu");
    file_screen(&mut s);
    command(&mut s, SAVE);
    assert_eq!(file_cursor(&mut s), 0);
    press(&mut s, "cross", 90);
    yes(&mut s, 0);
    until_commands(&mut s, "the field menu after the save");
    assert_eq!(saves(&mut s, 0), [listed("BASLUS-006640", 1, 1, 2)], "a disc 2 save");
    press(&mut s, "circle", 0);
    until(&mut s, 600, "back in the field", |s| s.runtime.context() == Context::Walk);
    let card = card_image(&mut s, 0);
    drop(s);

    let mut loaded = title_load(&disc2, &card, "disc-2-load", |s| s.runtime.context() == Context::Walk);
    assert_eq!(inspect(&mut loaded, "mode_field_map_id", 0).as_u64().unwrap() & 0xFFF, 687, "the save's field");
    assert_eq!(inspect(&mut loaded, "cd_directory_table[60]", 0), json!(2), "disc 2's index");
    assert_eq!(loaded.runtime.services().cd.inserted(), 2);
    let _ = std::fs::remove_dir_all(cards);
}
