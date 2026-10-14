//! The native memory card commands (`card_import`, `card_export`,
//! `card_restore_previous`, `save_import`, `save_export`, `save_delete`)
//! and the shared `card_list`, over card files in a temporary directory:
//! each change keeps the card it replaces as `cardN.previous.mcd`, the
//! files come back as they went in, and refusals name why. A stand-in
//! module (no game, no local inputs):
//!
//!     cargo test -p xem-game --test cards

use serde_json::{Value, json};
use std::path::{Path, PathBuf};
use xem_core::control::Session;
use xem_core::devices::card::{BLOCK_SIZE, formatted};
use xem_core::devices::card_file::{self, Contents, Save};
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

/// This game's save for file `digit` (its title frame, its checksum right).
fn game_save(digit: u8) -> Save {
    let mut data = vec![0; BLOCK_SIZE];
    data[..4].copy_from_slice(&[b'S', b'C', 0x11, 1]);
    data[4..30].copy_from_slice(b"\x82\x77\x82\x64\x82\x6d\x82\x6e\x82\x66\x82\x64\x82\x60\x82\x71\x82\x72\x81\x5e\x82\x6d\x82\x8f\x81\x44");
    data[30..34].copy_from_slice(&[0x82, (digit + 1) / 10 + 0x4F, 0x82, (digit + 1) % 10 + 0x4F]);
    data[0x123] = digit;
    data[0x1FFF] = data[0x100..0x1FFF].iter().fold(0u8, |a, &b| a.wrapping_add(b));
    Save { name: format!("BASLUS-00664{}", char::from(b'0' + digit)).into_bytes(), data }
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

/// The save names of a card status (`card_list`'s reply, a command reply's `card`).
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
fn saves_and_cards_go_in_and_out_as_files_and_each_change_keeps_the_card_it_replaced() {
    let dir = scratch("files");
    let cards = dir.join("cards");
    let mut s = session(&cards);
    let listed = command(&mut s, &json!({"cmd":"card_list","slot":0}));
    assert_eq!((listed["card"]["formatted"].as_bool(), listed["previous"].as_bool()), (Some(true), Some(false)), "{listed}");
    assert!(names(&listed).is_empty());

    // A single save (.mcs, as another card exports it) joins card 1.
    let first = game_save(0);
    let source = card_file::insert(&formatted(), &first).unwrap();
    let mcs = card_file::export_save(&source, &first.name).unwrap();
    std::fs::write(dir.join("file1.mcs"), &mcs).unwrap();
    let reply = ok(&mut s, json!({"cmd":"save_import","slot":0,"path":dir.join("file1.mcs")}));
    assert_eq!(reply["done"]["imported"], json!({"imported":"save","container":"mcs","name":"BASLUS-006640"}));
    assert_eq!(names(&reply["card"]), ["BASLUS-006640"]);
    let save = &reply["card"]["card"]["saves"][0];
    assert_eq!(save["kind"], json!({"game":"xenogears","file":1,"disc":1}), "{save}");
    assert_eq!(save["title"], "ＸＥＮＯＧＥＡＲＳ／Ｎｏ．０１");
    assert_eq!(save["problems"], json!([]));
    assert_eq!(reply["card"]["previous"], true);
    assert_eq!(std::fs::read(cards.join("card1.previous.mcd")).unwrap(), formatted(), "the fresh card it replaced");
    assert_eq!(card_file::extract(&std::fs::read(cards.join("card1.mcd")).unwrap(), &first.name).unwrap(), first);

    // The game's Copy refusals; a save file is not a card, and a card's save needs its name.
    assert!(refused(&mut s, json!({"cmd":"save_import","slot":0,"path":dir.join("file1.mcs")})).contains("already has a save named BASLUS-006640"));
    assert!(refused(&mut s, json!({"cmd":"card_import","slot":0,"path":dir.join("file1.mcs")})).contains("is a single save (.mcs, .ps1), not a whole card"));

    // The save goes out as it came in; the card as a DexDrive file, into card 2.
    ok(&mut s, json!({"cmd":"save_export","slot":0,"name":"BASLUS-006640","path":dir.join("out.mcs")}));
    assert_eq!(std::fs::read(dir.join("out.mcs")).unwrap(), mcs);
    ok(&mut s, json!({"cmd":"card_export","slot":0,"format":"gme","path":dir.join("card.gme")}));
    let gme = std::fs::read(dir.join("card.gme")).unwrap();
    let card1 = std::fs::read(cards.join("card1.mcd")).unwrap();
    assert_eq!(card_file::detect(&gme, "card.gme").unwrap().1, Contents::Card(card1.clone()));
    ok(&mut s, json!({"cmd":"card_export","slot":0,"path":dir.join("card.mcd")}));
    assert_eq!(std::fs::read(dir.join("card.mcd")).unwrap(), card1, "raw by default");
    let reply = ok(&mut s, json!({"cmd":"card_import","slot":1,"path":dir.join("card.gme")}));
    assert_eq!(reply["done"]["imported"], json!({"imported":"card","container":"gme"}));
    assert_eq!(std::fs::read(cards.join("card2.mcd")).unwrap(), card1);
    assert!(refused(&mut s, json!({"cmd":"save_import","slot":1,"path":dir.join("card.gme")})).contains("name the save of it to import"));
    // A card's save by name: the second game file of another card.
    let second = game_save(1);
    std::fs::write(dir.join("other.mcd"), card_file::insert(&formatted(), &second).unwrap()).unwrap();
    let reply = ok(&mut s, json!({"cmd":"save_import","slot":1,"path":dir.join("other.mcd"),"name":"BASLUS-006641"}));
    assert_eq!(names(&reply["card"]), ["BASLUS-006640", "BASLUS-006641"]);
    assert!(refused(&mut s, json!({"cmd":"save_import","slot":1,"path":dir.join("other.mcd"),"name":"BASLUS-006649"})).contains("no save named BASLUS-006649"));

    // A delete keeps the card as it was; restoring the previous card undoes it.
    let reply = ok(&mut s, json!({"cmd":"save_delete","slot":1,"name":"BASLUS-006640"}));
    assert_eq!(names(&reply["card"]), ["BASLUS-006641"]);
    assert!(card_file::list(&std::fs::read(cards.join("card2.previous.mcd")).unwrap()).saves.len() == 2);
    let reply = ok(&mut s, json!({"cmd":"card_restore_previous","slot":1}));
    assert_eq!(names(&reply["card"]), ["BASLUS-006640", "BASLUS-006641"]);

    // What the commands need.
    assert!(refused(&mut s, json!({"cmd":"save_delete","name":"X"})).contains("needs slot"));
    assert!(refused(&mut s, json!({"cmd":"save_delete","slot":2,"name":"X"})).contains("memory cards are 0 and 1"));
    assert!(refused(&mut s, json!({"cmd":"save_export","slot":0,"path":dir.join("x")})).contains("needs the save's name"));
    assert!(refused(&mut s, json!({"cmd":"card_export","slot":0,"format":"vmp","path":dir.join("x")})).contains("\"raw\" or \"gme\""));
    assert!(refused(&mut s, json!({"cmd":"card_import","slot":0})).contains("card_import needs path"));
    assert!(refused(&mut s, json!({"cmd":"save_import","slot":0,"path":dir.join("missing.mcs")})).contains("missing.mcs"));
    std::fs::remove_dir_all(dir).unwrap();
}

#[test]
fn a_change_waits_while_the_game_checks_the_card() {
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
    let error = refused(&mut s, json!({"cmd":"card_restore_previous","slot":0}));
    assert!(error.contains("has no previous card"), "{error}");
    std::fs::write(dir.join("card.mcd"), formatted()).unwrap();
    let error = refused(&mut s, json!({"cmd":"card_import","slot":0,"path":dir.join("card.mcd")}));
    assert!(error.contains("memory card 1 not changed: the game is checking memory card 1"), "{error}");
    let listed = command(&mut s, &json!({"cmd":"card_list","slot":0}));
    assert!(listed["busy"].as_str().unwrap().contains("checking memory card 1"));
    ok(&mut s, json!({"cmd":"card_import","slot":1,"path":dir.join("card.mcd")}));
    assert!(!dir.join("cards/card1.previous.mcd").exists(), "nothing of card 1 changed");
    std::fs::remove_dir_all(dir).unwrap();
}
