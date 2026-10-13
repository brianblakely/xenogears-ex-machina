//! The hosts' control queries answer from the original game's state (local
//! only: it needs the user's disc 1 and a game module build). Select shows
//! a field map's picture only on a map with one and with its item held;
//! elsewhere in a field the hosts toggle first-person inspection with it,
//! and they hold back its walking buttons only while the player walks.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test controls

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::path::PathBuf;
use xem_core::control::Session;
use xem_game::{NativeModule, boot};

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn run(session: &mut Session<NativeModule>, command: Value) {
    let reply = session.execute(&command);
    assert!(reply.get("error").is_none(), "{command} -> {reply}");
}

#[test]
fn select_shows_a_field_maps_picture_only_where_it_has_one_and_its_item_is_held() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    // A map with a picture (field_picture_table: map 0xEF, item 0x61).
    run(&mut session, json!({"cmd": "enter_field", "map": 0xEF}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    let controls = session.runtime.field_controls();
    assert!(controls.field && controls.walking && !controls.menu && !controls.picture, "its item is not held: {controls:?}");
    run(&mut session, json!({"cmd": "write", "path": "game_data.itemIds[149]", "value": 0x61}));
    run(&mut session, json!({"cmd": "write", "path": "game_data.itemCounts[149]", "value": 1}));
    assert!(session.runtime.field_controls().picture, "held, Select shows the picture");
    // A map without one leaves Select to the hosts, item or not; its menu
    // takes the directions back.
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    let controls = session.runtime.field_controls();
    assert!(controls.walking && !controls.picture, "{controls:?}");
    run(&mut session, json!({"cmd": "press", "buttons": ["square"], "frames": 2}));
    run(&mut session, json!({"cmd": "frames", "count": 60}));
    let controls = session.runtime.field_controls();
    assert!(controls.field && controls.menu && !controls.walking, "the menu runs: {controls:?}");
}
