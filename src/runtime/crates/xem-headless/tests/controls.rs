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

/// An agent's typed actions and a player's bound keys reach the game as the
/// same pad: the menu opens from either, and the two runs end equal.
#[test]
fn agents_actions_and_players_keys_are_one_command_path() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let bindings = xem_settings::GameBindings::default();
    let mut hashes = Vec::new();
    for human in [false, true] {
        let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
        run(&mut session, json!({"cmd": "enter_field", "map": 30}));
        run(&mut session, json!({"cmd": "frames", "count": 400}));
        assert_eq!(session.runtime.button_assignment(), xem_core::action::Assignment::default(), "pad_init's assignment");
        if human {
            // The player's Tab, through the bindings, held for two frames.
            let mut player = xem_settings::player::PlayerInput::default();
            player.key(&bindings, "Tab", true);
            let context = xem_settings::player::Context { assignment: session.runtime.button_assignment(), ..Default::default() };
            let buttons = player.buttons(&bindings, 0.0, context);
            assert_eq!(buttons, xem_core::pad::button::SQUARE);
            run(&mut session, json!({"cmd": "press", "buttons": ["square"], "frames": 2}));
        } else {
            run(&mut session, json!({"cmd": "act", "actions": ["Menu"], "frames": 2}));
        }
        run(&mut session, json!({"cmd": "frames", "count": 60}));
        assert!(session.runtime.field_controls().menu, "the menu runs");
        hashes.push(session.execute(&json!({"cmd": "memory_hash"})));
    }
    assert_eq!(hashes[0], hashes[1]);
}

fn context(session: &mut Session<NativeModule>) -> Value {
    session.execute(&json!({"cmd": "context"}))
}

/// The interface context follows the game: walking, the field menu and its
/// cursor, back to walking; a dialogue on the forest map.
#[test]
fn the_context_follows_walking_the_menu_and_dialogue() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    assert_eq!(context(&mut session)["kind"], "walk");
    run(&mut session, json!({"cmd": "act", "actions": ["Menu"], "frames": 2}));
    // The menu's files load first.
    run(&mut session, json!({"cmd": "frames", "count": 200}));
    let menu = context(&mut session);
    assert_eq!(menu["kind"], "menu", "{menu}");
    assert_eq!(menu["screen"], 0, "{menu}");
    let cursor = menu["cursor"].as_i64().unwrap();
    run(&mut session, json!({"cmd": "act", "actions": ["Up"], "frames": 2}));
    run(&mut session, json!({"cmd": "frames", "count": 20}));
    assert_eq!(context(&mut session)["cursor"].as_i64().unwrap(), (cursor + 1) % 7, "Up steps the arc's cursor");
    run(&mut session, json!({"cmd": "act", "actions": ["Cancel"], "frames": 2}));
    run(&mut session, json!({"cmd": "frames", "count": 300}));
    assert_eq!(context(&mut session)["kind"], "walk");
    // The forest's opening dialogue.
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 23}));
    let reply = session.execute(&json!({"cmd": "run_until", "max_frames": 1500, "until": {"path": "field_work.open_windows", "not_equals": 0}}));
    assert_eq!(reply["met"], true, "{reply}");
    assert_eq!(context(&mut session)["kind"], "dialogue");
}

/// One host frame: the player's input resolved in the game's context, one
/// game frame run with it.
fn host_frame(session: &mut Session<NativeModule>, player: &mut xem_settings::player::PlayerInput, bindings: &xem_settings::GameBindings) {
    let context = xem_settings::player::Context {
        attack: session.runtime.combo_input(),
        field: session.runtime.field_controls(),
        assignment: session.runtime.button_assignment(),
        ui: session.runtime.context(),
    };
    let buttons = player.buttons(bindings, 0.0, context);
    session.set_pad(0, buttons);
    run(session, json!({"cmd": "frames", "count": 1}));
    player.frames_ran(1);
}

fn tap(session: &mut Session<NativeModule>, player: &mut xem_settings::player::PlayerInput, bindings: &xem_settings::GameBindings, key: &str, then: usize) {
    player.key(bindings, key, true);
    for _ in 0..2 { host_frame(session, player, bindings); }
    player.key(bindings, key, false);
    for _ in 0..then { host_frame(session, player, bindings); }
}

/// Tab opens the field menu and, pressed again from a screen inside it,
/// backs out with Cancel presses until the menu has closed; the wheel
/// steps the menu's cursor.
#[test]
fn tab_toggles_the_field_menu_from_any_of_its_screens() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let bindings = xem_settings::GameBindings::default();
    let mut player = xem_settings::player::PlayerInput::default();
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    tap(&mut session, &mut player, &bindings, "Tab", 200);
    let menu = context(&mut session);
    assert_eq!(menu["kind"], "menu", "{menu}");
    // The wheel (a notch toward the player) steps the arc's cursor down.
    let cursor = menu["cursor"].as_u64().unwrap();
    assert!(player.wheel(1.0, session.runtime.context()));
    for _ in 0..10 { host_frame(&mut session, &mut player, &bindings); }
    assert_eq!(context(&mut session)["cursor"].as_u64().unwrap(), (cursor + 6) % 7, "{}", context(&mut session));
    // Into the items screen (cursor 4), then Tab.
    while context(&mut session)["cursor"].as_u64().unwrap() != 4 {
        tap(&mut session, &mut player, &bindings, "KeyW", 6);
    }
    tap(&mut session, &mut player, &bindings, "KeyE", 60);
    tap(&mut session, &mut player, &bindings, "Tab", 0);
    let mut frames = 0;
    while context(&mut session)["kind"] != "walk" {
        host_frame(&mut session, &mut player, &bindings);
        frames += 1;
        assert!(frames < 600, "the menu closes: {}", context(&mut session));
    }
    assert!(!player.pulsing() || frames < 600);
}
