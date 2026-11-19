//! RetroAchievements in a host's game (`Game::follow_achievements`, as the
//! window and a headset follow it) over the original game (local only: the
//! user's disc 1 and a game module build), against a stand-in server
//! (xem-achievements' tests/stand_in): the disc in the drive loads its
//! achievements by its hash, an achievement on the pad driver's vertical
//! blank counter (0x80059488, RetroAchievements' 0x059488) unlocks from the
//! game's own RAM as the session's frames run, hardcore mode turned on
//! restarts the console and restricts the session until it unlocks again
//! as hardcore, and signing out lifts the restrictions.
//!
//!     XEM_DISC1=<disc 1> cargo test --release -p xem-host --test achievements

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::json;
use std::cell::RefCell;
use std::rc::Rc;
use std::time::{Duration, Instant};
use xem_achievements::Achievements;
use xem_host::Game;

#[path = "../../xem-achievements/tests/stand_in/mod.rs"]
mod stand_in;
use stand_in::requests;

/// RetroAchievements' hash of disc 1 (xem-achievements' tests/disc.rs).
const DISC1: &str = "1bde22c83a069c43cb32b8521bfdf7ae";

fn disc() -> Option<std::path::PathBuf> {
    let path = std::env::var_os("XEM_DISC1").map(std::path::PathBuf::from)
        .unwrap_or_else(|| std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

/// The pad driver's vertical blank counter, which the console's reset
/// starts again.
fn pad_vblanks(game: &mut Game) -> u32 {
    let reply = game.command(&json!({"cmd": "read_memory", "address": 0x8005_9488u32, "length": 4})).unwrap();
    let hex = reply.as_str().unwrap();
    u32::from_le_bytes(std::array::from_fn(|i| u8::from_str_radix(&hex[i * 2..i * 2 + 2], 16).unwrap()))
}

/// Follows and plays `frames` frames at a time until `done` holds.
fn play(game: &mut Game, client: &Rc<RefCell<Achievements>>, hardcore: bool, mut done: impl FnMut(&mut Game, &Rc<RefCell<Achievements>>) -> bool) {
    let deadline = Instant::now() + Duration::from_secs(60);
    while !done(game, client) {
        assert!(Instant::now() < deadline, "timed out: {:?}", client.borrow().status());
        game.follow_achievements(client, hardcore, true);
        let reply = game.command(&json!({"cmd": "frames", "count": 10})).unwrap();
        assert!(reply.get("error").is_none(), "{reply}");
        std::thread::sleep(Duration::from_millis(2));
    }
}

#[test]
fn the_original_game_unlocks_from_its_ram_and_hardcore_mode_restarts_and_restricts_it() {
    let Some(disc) = disc() else { eprintln!("skipping: no disc 1 (set XEM_DISC1)"); return };
    let (host, log) = stand_in::server(vec![(DISC1, stand_in::game(11248, "Xenogears", &[
        (201, "Under way", "The pad driver has counted 120 vertical blanks", 5, "0xX059488>120"),
    ]))]);
    let directory = std::env::temp_dir().join(format!("xem-host-achievements-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&directory);
    let client = Rc::new(RefCell::new(Achievements::new(Some(&directory), "XemTest/1.0")));
    client.borrow_mut().set_host(&host);
    let (session, _) = xem_game::boot(&disc, &Default::default()).unwrap();
    let mut game = Game::new(session);
    client.borrow_mut().sign_in("Player", "secret").unwrap();

    // Softcore: the disc in the drive's achievements, unlocked from the RAM
    // the game's frames leave.
    let unlocked = |client: &Rc<RefCell<Achievements>>| client.borrow_mut().take_notices().into_iter().filter(|n| n.title == "Under way").count();
    let mut count = 0;
    play(&mut game, &client, false, |_, client| { count += unlocked(client); count > 0 });
    assert_eq!(requests(&log, "achievementsets")[0]["m"], DISC1, "loaded by the hash of the disc in the drive");
    play(&mut game, &client, false, |_, _| !requests(&log, "awardachievement").is_empty());
    assert_eq!(requests(&log, "awardachievement")[0]["h"], "0");
    assert!(!game.hardcore());
    let before = pad_vblanks(&mut game);
    assert!(before > 120, "{before} vertical blanks");

    // Hardcore turned on during play: the console resets (rcheevos asks),
    // the session refuses states, rewind and fast-forward, and the
    // achievement unlocks again, as hardcore, from the reset.
    game.follow_achievements(&client, true, true);
    assert!(game.hardcore());
    let after = pad_vblanks(&mut game);
    assert!(after < before, "the console reset: the pad driver counts {after} vertical blanks, {before} before");
    let state = directory.join("state.xemstate");
    assert_eq!(game.command(&json!({"cmd": "state_save", "path": state})), Err(xem_core::control::HARDCORE_STATES.into()));
    assert_eq!(game.command(&json!({"cmd": "fast_forward", "enabled": true})), Err(xem_core::control::HARDCORE_FAST_FORWARD.into()));
    assert_eq!(game.set_rewind_held(true, false), Err(xem_core::control::HARDCORE_REWIND.into()));
    let mut count = 0;
    play(&mut game, &client, true, |_, client| { count += unlocked(client); count > 0 });
    play(&mut game, &client, true, |_, _| requests(&log, "awardachievement").len() > 1);
    assert_eq!(requests(&log, "awardachievement")[1]["h"], "1", "a hardcore unlock");
    assert_eq!(game.command(&json!({"cmd": "status"})).unwrap()["hardcore"], true);

    // Signed out: hardcore no longer applies.
    client.borrow_mut().sign_out();
    game.follow_achievements(&client, true, true);
    assert!(!game.hardcore());
    assert!(game.command(&json!({"cmd": "state_save", "path": state})).is_ok());
    std::fs::remove_dir_all(directory).unwrap();
}
