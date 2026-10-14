//! The native memory card commands (`card_import`, `card_export`) and the
//! shared `card_list`, over card files in a temporary directory: an import
//! reads any supported card or single-save file by its contents (a single
//! save becomes a fresh card holding only it), refuses damaged and unknown
//! files, changing nothing, and replies with what it found; an export is the
//! raw card. A stand-in module (no game, no local inputs):
//!
//!     cargo test -p xem-game --test cards

use serde_json::{Value, json};
use std::path::{Path, PathBuf};
use xem_core::control::Session;
use xem_core::devices::card::{BLOCK_SIZE, formatted};
use xem_core::memory::OutOfBounds;
use xem_core::module::{AsyncState, ImportHandler};
use xem_core::{GameMemory, GameModule, Runtime, Trap};
use xem_game::storage::{FileStorage, command};

/// No memory: the card commands run no game code.
struct Nothing;
impl GameMemory for Nothing {
    fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> { Err(OutOfBounds { address, len: out.len() }) }
    fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> { Err(OutOfBounds { address, len: data.len() }) }
}

/// A module that is never run.
struct Idle(Nothing);
impl GameModule for Idle {
    unsafe fn set_import_handler(&mut self, _: *mut dyn ImportHandler) {}
    fn set_resume_value(&mut self, _: u32) {}
    fn run(&mut self, _: u32, _: u32) -> Result<(), Trap> { unreachable!("no frames") }
    fn run_task(&mut self) -> Result<(), Trap> { unreachable!("no frames") }
    fn call(&mut self, _: u32) -> Result<(), Trap> { unreachable!("no calls") }
    fn interrupt(&mut self, _: u32, _: u32) -> Result<(), Trap> { unreachable!("no interrupts") }
    fn async_state(&mut self) -> AsyncState { AsyncState::Normal }
    fn stop_unwind(&mut self) {}
    fn unwind_area(&mut self) -> u32 { 0 }
    fn start_rewind(&mut self, _: u32) {}
    fn stack_pointer(&mut self) -> u32 { 0x1010_0000 }
    fn set_stack_pointer(&mut self, _: u32) {}
    fn memory(&mut self) -> &mut dyn GameMemory { &mut self.0 }
    fn globals(&mut self) -> Vec<u32> { Vec::new() }
    fn set_globals(&mut self, _: &[u32]) {}
}

/// This game's save for file `digit` (one block: its title frame, its
/// digit and its payload's checksum as the game writes them): its name and
/// its block.
fn game_save(digit: u8) -> (String, Vec<u8>) {
    let mut data = vec![0; BLOCK_SIZE];
    data[..4].copy_from_slice(&[b'S', b'C', 0x11, 1]);
    data[4..30].copy_from_slice(b"\x82\x77\x82\x64\x82\x6d\x82\x6e\x82\x66\x82\x64\x82\x60\x82\x71\x82\x72\x81\x5e\x82\x6d\x82\x8f\x81\x44");
    data[30..34].copy_from_slice(&[0x82, (digit + 1) / 10 + 0x4F, 0x82, (digit + 1) % 10 + 0x4F]);
    data[0x123] = digit;
    data[0x1FFF] = data[0x100..0x1FFF].iter().fold(0u8, |a, &b| a.wrapping_add(b));
    (format!("BASLUS-00664{}", char::from(b'0' + digit)), data)
}

/// The save as a single save (.mcs): the directory frame of its first
/// block, then its blocks.
fn mcs((name, data): &(String, Vec<u8>)) -> Vec<u8> {
    let mut frame = vec![0; 0x80];
    frame[0] = 0x51;
    frame[4..8].copy_from_slice(&(data.len() as u32).to_le_bytes());
    frame[8..10].copy_from_slice(&0xFFFFu16.to_le_bytes());
    frame[0x0A..0x0A + name.len()].copy_from_slice(name.as_bytes());
    frame[0x7F] = frame[..0x7F].iter().fold(0, |a, b| a ^ b);
    [frame, data.clone()].concat()
}

/// The save as a PS3 save (.psv): its header (type 1, its name), then its blocks.
fn psv((name, data): &(String, Vec<u8>)) -> Vec<u8> {
    let mut header = vec![0; 0x84];
    header[..4].copy_from_slice(b"\0VSP");
    header[0x3C] = 1;
    header[0x64..0x64 + name.len()].copy_from_slice(name.as_bytes());
    [header, data.clone()].concat()
}

/// The save with Action Replay's 54-byte header (its name, a title).
fn action_replay((name, data): &(String, Vec<u8>)) -> Vec<u8> {
    let mut header = vec![0; 54];
    header[..name.len()].copy_from_slice(name.as_bytes());
    header[0x15..0x15 + 9].copy_from_slice(b"Xenogears");
    [header, data.clone()].concat()
}

/// A raw card as a DexDrive card (.gme): its 0xF40-byte header, the card.
fn gme(card: &[u8]) -> Vec<u8> {
    let mut header = vec![0; 0xF40];
    header[..11].copy_from_slice(b"123-456-STD");
    [header, card.to_vec()].concat()
}

fn session(cards: &Path) -> Session<Idle> {
    let mut runtime = Runtime::new(Idle(Nothing));
    runtime.services().card.attach_storage(Box::new(FileStorage { dir: cards.to_path_buf() })).unwrap();
    Session::new(runtime, None, std::time::Duration::default)
}

fn ok(session: &mut Session<Idle>, cmd: Value) -> Value {
    let reply = command(session, &cmd);
    assert!(reply.get("error").is_none(), "{cmd}: {reply}");
    reply
}

fn refused(session: &mut Session<Idle>, cmd: Value) -> String {
    let reply = command(session, &cmd);
    reply["error"].as_str().unwrap_or_else(|| panic!("{cmd} was not refused: {reply}")).to_string()
}

/// The save names of a card status (`card_list`'s reply, a reply's `card`).
fn names(status: &Value) -> Vec<String> {
    status["card"]["saves"].as_array().unwrap().iter().map(|s| s["name"].as_str().unwrap().to_string()).collect()
}

fn scratch(name: &str) -> PathBuf {
    let dir = std::env::temp_dir().join(format!("xem-game-cards-{name}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(dir.join("cards")).unwrap();
    dir
}

#[test]
fn an_import_reads_any_card_or_save_file_and_says_what_it_found() {
    let dir = scratch("files");
    let cards = dir.join("cards");
    let mut s = session(&cards);
    let listed = command(&mut s, &json!({"cmd":"card_list","slot":0}));
    assert_eq!((listed["card"]["formatted"].as_bool(), listed["busy"].is_null()), (Some(true), true), "{listed}");
    assert!(names(&listed).is_empty());
    let import = |s: &mut Session<Idle>, slot: u8, name: &str, bytes: &[u8]| {
        std::fs::write(dir.join(name), bytes).unwrap();
        ok(s, json!({"cmd":"card_import","slot":slot,"path":dir.join(name)}))
    };

    // A single save (.mcs) becomes a fresh card holding it.
    let reply = import(&mut s, 0, "file1.mcs", &mcs(&game_save(0)));
    assert_eq!(reply["message"], "Card 1: single save (.mcs, .ps1) on a fresh card, 1 save");
    assert_eq!(names(&reply["card"]), ["BASLUS-006640"]);
    let save = &reply["card"]["card"]["saves"][0];
    assert_eq!(save["kind"], json!({"game":"xenogears","file":1,"disc":1}), "{save}");
    assert_eq!(save["title"], "ＸＥＮＯＧＥＡＲＳ／Ｎｏ．０１");
    assert_eq!(save["problems"], json!([]));
    let card1 = std::fs::read(cards.join("card1.mcd")).unwrap();
    assert_eq!(&card1[0x2000..0x4000], &game_save(0).1[..], "its block on the card");

    // The card goes out raw and comes back in as a DexDrive card.
    ok(&mut s, json!({"cmd":"card_export","slot":0,"path":dir.join("card.mcd")}));
    assert_eq!(std::fs::read(dir.join("card.mcd")).unwrap(), card1);
    let reply = import(&mut s, 1, "card.gme", &gme(&card1));
    assert_eq!(reply["message"], "Card 2: DexDrive card, 1 save");
    assert_eq!(std::fs::read(cards.join("card2.mcd")).unwrap(), card1);

    // Each other single save replaces the card with a fresh one holding it.
    let reply = import(&mut s, 1, "BASLUS-006641.PSV", &psv(&game_save(1)));
    assert_eq!(reply["message"], "Card 2: PS3 save (.psv) on a fresh card, 1 save");
    assert_eq!(names(&reply["card"]), ["BASLUS-006641"]);
    let reply = import(&mut s, 0, "file3.psx", &action_replay(&game_save(2)));
    assert_eq!(reply["message"], "Card 1: Action Replay save (.psx, .mcb, .mcx, .pda) on a fresh card, 1 save");
    assert_eq!(names(&reply["card"]), ["BASLUS-006642"]);
    let reply = import(&mut s, 0, "BASLUS-006643", &game_save(3).1);
    assert_eq!(reply["message"], "Card 1: raw save on a fresh card, 1 save");
    assert_eq!(names(&reply["card"]), ["BASLUS-006643"]);
    // A save the game's load would refuse goes in, and the reply says so.
    let mut wrong = game_save(4);
    wrong.1[0x500] ^= 1;
    let reply = import(&mut s, 0, "file5.mcs", &mcs(&wrong));
    let message = reply["message"].as_str().unwrap();
    assert!(message.starts_with("Card 1: single save (.mcs, .ps1) on a fresh card, 1 save; BASLUS-006644: its checksum byte"), "{message}");
    assert!(message.ends_with("the game's load refuses it"), "{message}");

    // Refusals name why and change nothing.
    let before = std::fs::read(cards.join("card1.mcd")).unwrap();
    let mut damaged = card1.clone();
    damaged[0x80 + 0x20] ^= 1;
    std::fs::write(dir.join("damaged.mcd"), &damaged).unwrap();
    let error = refused(&mut s, json!({"cmd":"card_import","slot":0,"path":dir.join("damaged.mcd")}));
    assert!(error.starts_with("memory card 1 not changed: the picked card's directory is damaged: directory frame 1"), "{error}");
    std::fs::write(dir.join("notes.txt"), [7; 1000]).unwrap();
    let error = refused(&mut s, json!({"cmd":"card_import","slot":0,"path":dir.join("notes.txt")}));
    assert!(error.contains("notes.txt (1000 bytes) is not a memory card or a save"), "{error}");
    std::fs::write(dir.join("my save.bin"), &game_save(5).1).unwrap();
    let error = refused(&mut s, json!({"cmd":"card_import","slot":0,"path":dir.join("my save.bin")}));
    assert!(error.contains("named by its file name"), "{error}");
    std::fs::write(dir.join("big.gme"), vec![0; 0xF40 + 0x20000 + 1]).unwrap();
    assert!(refused(&mut s, json!({"cmd":"card_import","slot":0,"path":dir.join("big.gme")})).contains("exceeds size limit"));
    assert!(refused(&mut s, json!({"cmd":"card_import","slot":0,"path":dir.join("missing.mcs")})).contains("missing.mcs"));
    assert!(refused(&mut s, json!({"cmd":"card_import","slot":0})).contains("needs path"));
    assert!(refused(&mut s, json!({"cmd":"card_import","slot":2,"path":dir.join("file1.mcs")})).contains("0 or 1"));
    assert!(std::fs::read(cards.join("card1.mcd")).unwrap() == before, "card 1 as it was");
    std::fs::remove_dir_all(dir).unwrap();
}

#[test]
fn an_import_waits_while_the_game_checks_the_card() {
    use xem_core::devices::{Context, Device};
    let dir = scratch("busy");
    let mut s = session(&dir.join("cards"));
    let mut memory = Nothing;
    let (mut raised, mut trap) = (Vec::new(), None);
    let now = s.runtime.services().clock.now;
    let card = &mut s.runtime.services().card;
    for (op, args) in [("start", &[][..]), ("info", &[0][..])] {
        let mut context = Context { memory: &mut memory, now, raised: &mut raised, trap_reason: &mut trap };
        card.import(op, args, &mut context);
    }
    let mut other = formatted();
    other[0x2000] = 0x42;
    std::fs::write(dir.join("card.mcd"), &other).unwrap();
    let error = refused(&mut s, json!({"cmd":"card_import","slot":0,"path":dir.join("card.mcd")}));
    assert!(error.contains("memory card 1 not changed: the game is checking memory card 1"), "{error}");
    let listed = command(&mut s, &json!({"cmd":"card_list","slot":0}));
    assert!(listed["busy"].as_str().unwrap().contains("checking memory card 1"), "{listed}");
    assert_eq!(ok(&mut s, json!({"cmd":"card_import","slot":1,"path":dir.join("card.mcd")}))["message"], "Card 2: raw card, no saves");
    assert_eq!(std::fs::read(dir.join("cards/card2.mcd")).unwrap(), other, "the other card is not held up");
    assert!(!dir.join("cards/card1.mcd").exists(), "nothing of card 1 changed");
    std::fs::remove_dir_all(dir).unwrap();
}
