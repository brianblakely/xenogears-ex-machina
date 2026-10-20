//! Settings' debug requests through the real game and its original selectors.
#![cfg(has_game_module)]

use serde_json::{Value, json};
use xem_core::{control::Session, debug::Request};
use xem_game::{BootOptions, NativeModule, boot};

fn session() -> Option<Session<NativeModule>> {
    let disc = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../../../discs/Xenogears disc 1.chd");
    if !disc.exists() { eprintln!("skipping: no disc 1"); return None; }
    Some(boot(&disc, &BootOptions::default()).unwrap().0)
}

fn read(s: &mut Session<NativeModule>, path: &str) -> Value {
    let reply = s.execute(&json!({"cmd":"inspect", "path":path}));
    assert!(reply.get("error").is_none(), "{path}: {reply}");
    reply
}

fn launch(s: &mut Session<NativeModule>, request: Request) {
    s.request_debug(request).unwrap();
    for _ in 0..1501 {
        if !s.debug.busy() { return; }
        s.tick_debug().unwrap();
    }
    panic!("debug operation did not finish: {}", s.debug_status());
}

#[test]
fn debug_field_uses_the_requested_map_and_entrance() {
    let Some(mut s) = session() else { return; };
    launch(&mut s, Request::Field { map: 22, entry: 0 });
    for _ in 0..300 { s.frame().unwrap(); }
    assert_eq!(read(&mut s, "mode_field_map_id"), 22);
    assert_eq!(read(&mut s, "game_data.entry[2]"), 0);
    let before = s.memory_hash().unwrap();
    assert!(s.request_debug(Request::Field { map: 1024, entry: 0 }).is_err());
    assert_eq!(s.memory_hash().unwrap(), before, "invalid requests must not reset the game");
}

#[test]
fn debug_battle_uses_the_original_selector_setup() {
    let Some(mut s) = session() else { return; };
    launch(&mut s, Request::Battle { formation: 15, file: 253, party: [0, 2, 1], gears: [1; 3] });
    for _ in 0..300 { s.frame().unwrap(); }
    assert_eq!(read(&mut s, "game_data.party[0]"), 0);
    assert_eq!(read(&mut s, "game_data.party[1]"), 2);
    assert_eq!(read(&mut s, "game_data.party[2]"), 1);
    for i in 0..3 { assert_eq!(read(&mut s, &format!("game_data.inGear[{i}]")), 1); }
    assert_eq!(read(&mut s, "formation_selected_index"), 15);
    assert_eq!(read(&mut s, "game_data.characters[0].maxHp"), 999);
    assert!(!s.debug.busy());
    // Repeated launches must wait for the new selector, not the old
    // selector's values left in overlay memory.
    for _ in 0..2 {
        launch(&mut s, Request::Battle { formation: 3, file: 0, party: [0, 1, 2], gears: [0; 3] });
        for _ in 0..300 { s.frame().unwrap(); }
        assert_eq!(read(&mut s, "formation_selected_index"), 3);
        assert_eq!(read(&mut s, "game_data.party[1]"), 1);
        assert_eq!(read(&mut s, "game_data.inGear[0]"), 0);
    }
}

#[test]
fn debug_menu_uses_the_requested_screen_and_parameter() {
    let Some(mut s) = session() else { return; };
    for (screen, parameter) in [(3, 2), (0, 0), (1, 0), (2, 0), (4, 0), (5, 0), (6, 1)] {
        launch(&mut s, Request::Menu { screen, parameter });
        for _ in 0..90 { s.frame().unwrap_or_else(|e| panic!("menu {screen}: {e}")); }
        assert_eq!(read(&mut s, "menu_state_screen"), screen);
        assert_eq!(read(&mut s, "menu_state_screen_parameter"), parameter);
        assert_eq!(read(&mut s, "game_data.gold"), 999999999);
    }
}

#[test]
fn debug_launches_worldmap_battling_movie_and_developer_room() {
    let Some(mut s) = session() else { return; };
    for (request, mode) in [
        (Request::Worldmap { map: 1024 }, 3),
        (Request::Battling, 4),
        (Request::Movie { movie: 0, kind: 1 }, 6),
        (Request::Field { map: 0, entry: 0 }, 1),
    ] {
        launch(&mut s, request);
        assert_eq!(s.runtime.services().mode, Some(mode), "{request:?}");
        for _ in 0..300 { s.frame().unwrap_or_else(|e| panic!("{request:?}: {e}")); }
        match request {
            Request::Worldmap { map } => assert_eq!(read(&mut s, "game_data.map"), map),
            Request::Battling => assert_eq!(read(&mut s, "arena_mode_state"), 0),
            Request::Movie { movie, kind } => {
                assert_eq!(read(&mut s, "movie_mode_movie_index"), movie);
                assert_eq!(read(&mut s, "movie_mode_movie_kind"), kind);
            }
            Request::Field { map, .. } => assert_eq!(read(&mut s, "mode_field_map_id"), map),
            _ => unreachable!(),
        }
    }
}

#[test]
fn pending_debug_rejects_overlap_and_snapshot_and_reset_cancels_it() {
    let Some(mut s) = session() else { return; };
    let request = json!({"cmd":"debug", "request":{"tool":"field", "map":0, "entry":0}});
    assert_eq!(s.execute(&request)["busy"], true);
    let before = s.memory_hash().unwrap();
    assert!(s.execute(&request).get("error").is_some());
    assert_eq!(s.memory_hash().unwrap(), before);
    assert!(s.snapshot_bytes().unwrap_err().contains("debug operation"));
    assert!(s.execute(&json!({"cmd":"reset"})).get("error").is_none());
    assert!(!s.debug.busy());
    let saved = s.snapshot_bytes().unwrap();
    assert_eq!(s.execute(&request)["busy"], true);
    s.restore_bytes(&saved).unwrap();
    assert!(!s.debug.busy());
    assert!(s.execute(&json!({"cmd":"debug", "request":{"tool":"movie", "movie":0, "kind":3}})).get("error").is_some());
}

#[test]
fn debug_request_records_and_replays_the_same_setup() {
    let Some(mut s) = session() else { return; };
    let initial = s.snapshot_bytes().unwrap();
    s.execute(&json!({"cmd":"record"}));
    launch(&mut s, Request::Menu { screen: 3, parameter: 2 });
    for _ in 0..90 { s.frame().unwrap(); }
    let frames = s.runtime.frames();
    let expected = s.memory_hash().unwrap();
    let mut log = s.execute(&json!({"cmd":"stop_recording"}));
    assert!(log["controls"].as_array().unwrap().iter().any(|entry| entry[1]["cmd"] == "debug"));
    s.restore_bytes(&initial).unwrap();
    log["cmd"] = json!("replay");
    let reply = s.execute(&log);
    assert!(reply.get("error").is_none(), "{reply}");
    while s.runtime.frames() < frames { s.frame().unwrap(); }
    assert_eq!(s.memory_hash().unwrap(), expected);
}
