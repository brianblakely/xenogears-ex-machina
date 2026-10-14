//! The drive's changer with the user's two discs (local only: both discs and
//! a game module build). Booted from disc 2, a new game asks for disc 1: the
//! changer puts it in at once when it is beside disc 2, and without it the
//! game waits at its change-disc notice until the host adds it. A state saved
//! before the change restores in a session booted from disc 1 and goes on
//! identically; one saved while the game waits takes disc 1 there at once.
//! At the story's disc change the changer waits five seconds.
//!
//!     XEM_DISC1=<disc 1> XEM_DISC2=<disc 2> cargo test --release -p xem-headless --test discs

#![cfg(has_game_module)]

use std::path::PathBuf;

use xem_core::control::{Session, button_bit};
use xem_core::devices::cd::STORY_CHANGE_CYCLES;
use xem_game::{BootOptions, NativeModule, boot};

/// tests/reference-inputs/new-game-movie-skip.json's first steps (route.rs):
/// skip the opening movie, open the title's file screen and choose its new game.
const ROUTE: [(u64, u64, &str); 5] =
    [(901, 910, "start"), (1300, 1310, "start"), (1500, 1510, "cross"), (1750, 1760, "up"), (1810, 1820, "cross")];
/// The title's file screen is open, before its new game is chosen.
const FILE_SCREEN: u64 = 1700;
const END: u64 = 2400;
/// The title's map, and the first map of a new game.
const TITLE_MAP: i64 = 490;
const FIRST_MAP: i64 = 4;

fn disc(number: u8) -> Option<PathBuf> {
    let path = std::env::var_os(format!("XEM_DISC{number}")).map(PathBuf::from).unwrap_or_else(|| {
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join(format!("../../../../discs/Xenogears disc {number}.chd"))
    });
    path.exists().then_some(path)
}

fn session(disc: &std::path::Path) -> Session<NativeModule> {
    boot(disc, &BootOptions::default()).expect("boot").0
}

/// Run frames `from..to` of the route.
fn run(session: &mut Session<NativeModule>, from: u64, to: u64) {
    for frame in from..to {
        let held = ROUTE
            .iter()
            .filter(|(start, end, _)| (*start..*end).contains(&frame))
            .fold(0, |bits, (_, _, name)| bits | button_bit(name).unwrap());
        session.runtime.services().pads.0[0].buttons = held;
        assert!(session.frame().expect("frame"), "a native disc never blocks");
    }
}

/// A game global, through the inspection schema.
fn value(session: &mut Session<NativeModule>, path: &str) -> i64 {
    let reply = session.execute(&serde_json::json!({ "cmd": "inspect", "path": path, "depth": 0 }));
    reply.as_i64().unwrap_or_else(|| panic!("{path}: {reply}"))
}

/// The map, the disc the game's file index belongs to, and the disc in the drive.
fn place(session: &mut Session<NativeModule>) -> (i64, i64, u8) {
    let map = value(session, "mode_field_map_id");
    let index = value(session, "cd_directory_table[60]");
    (map, index, session.runtime.services().cd.inserted())
}

#[test]
fn a_new_game_on_disc_2_gets_disc_1_from_beside_it() {
    let (Some(disc1), Some(disc2)) = (disc(1), disc(2)) else {
        eprintln!("skipping: needs both discs (XEM_DISC1, XEM_DISC2), side by side");
        return;
    };
    let mut first = session(&disc2);
    run(&mut first, 0, FILE_SCREEN);
    assert_eq!(place(&mut first), (TITLE_MAP, 2, 2));
    let before = first.runtime.snapshot().unwrap();
    run(&mut first, FILE_SCREEN, END);
    assert_eq!(place(&mut first), (FIRST_MAP, 1, 1), "disc 1 went in, its index loaded, the new game began");
    assert_eq!(first.runtime.services().cd.wanted(), None);
    let reference = first.memory_hash().unwrap();

    // In a session booted from disc 1 (disc 2 beside it) the state restores
    // with disc 2 in the drive and goes on identically.
    let mut other = session(&disc1);
    other.runtime.restore(&before).expect("a state restores across the boot discs");
    assert_eq!(place(&mut other), (TITLE_MAP, 2, 2));
    run(&mut other, FILE_SCREEN, END);
    assert_eq!(other.memory_hash().unwrap(), reference);
}

/// Disc 2 alone in a directory (no disc 1 beside it).
fn disc_2_alone(disc2: &std::path::Path) -> PathBuf {
    let alone = PathBuf::from(env!("CARGO_TARGET_TMPDIR")).join("disc-2-alone");
    std::fs::create_dir_all(&alone).unwrap();
    let link = alone.join(disc2.file_name().unwrap());
    if !link.exists() {
        std::os::unix::fs::symlink(std::fs::canonicalize(disc2).unwrap(), &link).unwrap();
    }
    link
}

#[test]
fn without_disc_1_the_game_waits_until_the_host_adds_it() {
    let (Some(disc1), Some(disc2)) = (disc(1), disc(2)) else {
        eprintln!("skipping: needs both discs (XEM_DISC1, XEM_DISC2)");
        return;
    };
    let mut session = session(&disc_2_alone(&disc2));
    run(&mut session, 0, END + 600);
    assert_eq!(session.runtime.services().cd.wanted(), Some(1), "the new game asks for disc 1");
    assert_eq!(place(&mut session), (TITLE_MAP, 2, 2), "and waits at its change-disc notice");
    session.runtime.services().add_disc(1, xem_disc::open_path(&disc1).unwrap());
    assert_eq!(session.runtime.services().cd.wanted(), None);
    run(&mut session, END + 600, END + 1200);
    assert_eq!(place(&mut session), (FIRST_MAP, 1, 1), "with disc 1 the new game begins");
}

#[test]
fn a_state_waiting_for_disc_1_takes_it_in_a_session_that_has_it() {
    let (Some(disc1), Some(disc2)) = (disc(1), disc(2)) else {
        eprintln!("skipping: needs both discs (XEM_DISC1, XEM_DISC2), side by side");
        return;
    };
    let mut alone = session(&disc_2_alone(&disc2));
    run(&mut alone, 0, END + 600);
    assert_eq!(alone.runtime.services().cd.wanted(), Some(1), "the new game waits for disc 1");
    let waiting = alone.snapshot_bytes().unwrap();
    // Restored in a session booted from disc 1 with disc 2 beside it: the
    // state's disc 2 goes back in the drive, and disc 1 in at once.
    let mut both = session(&disc1);
    both.restore_bytes(&waiting).expect("a state restores across the boot discs");
    assert_eq!(both.runtime.services().cd.wanted(), None, "the wait takes the disc the session holds");
    run(&mut both, END + 600, END + 1200);
    assert_eq!(place(&mut both), (FIRST_MAP, 1, 1), "disc 1 went in, its index loaded, the new game began");
}

#[test]
fn the_story_disc_change_puts_disc_2_in_five_seconds_after_it_asks() {
    let (Some(disc1), Some(_)) = (disc(1), disc(2)) else {
        eprintln!("skipping: needs both discs (XEM_DISC1, XEM_DISC2), side by side");
        return;
    };
    let mut session = session(&disc1);
    let reply = session.execute(&serde_json::json!({ "cmd": "enter_field", "map": 23 }));
    assert_eq!(reply["map"], 23, "{reply}");
    run(&mut session, END, END + 600);
    // The menu kind the story's last event of disc 1 requests (field event
    // fe 56): field_menu_request (field_mode.h, 0x800ADB64 in the field
    // overlay; not in the inspection schema).
    session.runtime.memory().write(0x800A_DB64, &6i32.to_le_bytes()).unwrap();
    // Through its save questions (Cross, then Circle: there is no card to save to).
    let mut due = None;
    for frame in 0..3000u64 {
        let button = match frame % 60 {
            0..6 => button_bit("cross"),
            30..36 => button_bit("circle"),
            _ => None,
        };
        session.runtime.services().pads.0[0].buttons = button.unwrap_or(0);
        assert!(session.frame().expect("frame"));
        due = session.runtime.services().cd.change_due();
        if due.is_some() {
            break;
        }
    }
    let Some((wanted, at)) = due else { panic!("the disc change menu asked for no disc") };
    assert_eq!(wanted, 2);
    let asked = session.runtime.services().clock.now;
    assert!(at - asked <= STORY_CHANGE_CYCLES && at - asked > STORY_CHANGE_CYCLES - 2 * xem_core::clock::VBLANK_CYCLES);
    session.runtime.services().pads.0[0].buttons = 0;
    while session.runtime.services().cd.change_due().is_some() {
        assert_eq!(session.runtime.services().cd.inserted(), 1, "disc 1 stays in for the five seconds");
        assert!(session.frame().expect("frame"));
    }
    assert!(session.runtime.services().clock.now >= at);
    assert_eq!(session.runtime.services().cd.inserted(), 2);
    for _ in 0..600 {
        assert!(session.frame().expect("frame"));
    }
    assert_eq!(value(&mut session, "cd_directory_table[60]"), 2, "the game loaded disc 2's index");
}
