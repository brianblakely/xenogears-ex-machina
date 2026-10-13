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

/// The field as the assisted controls read it: the walkmesh under the
/// player, the camera that projects the player onto the screen and picks
/// the floor back, and the D-pad's directions by the camera's angle.
#[test]
fn the_field_reads_its_walkmesh_camera_and_dpad_directions() {
    use xem_core::field::{Field, heading, screen_direction};
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    let field = Field::read(session.runtime.memory()).expect("a field");
    let player = *field.player().expect("the player");
    let mesh = &field.layers[player.layer];
    eprintln!("player {player:?} layers {} triangles {} camera {:?}", field.layers.len(), mesh.triangles.len(), field.camera);
    assert!(mesh.locate(player.position.x, player.position.z).is_some(), "the player stands on its layer");
    let screen = field.camera.project(player.position).expect("in front of the camera");
    eprintln!("player on screen {screen:?}");
    assert!((0.0..320.0).contains(&screen.0) && (0.0..224.0).contains(&screen.1), "{screen:?}");
    let hit = field.pick(screen).expect("the floor under the player");
    eprintln!("pick {hit:?}");
    assert!(hit.point.planar(player.position) < 40.0, "{hit:?}");
    // Each D-pad direction moves the player the way the camera's angle says.
    for (buttons, direction) in [(["right"], 0), (["down"], 0x400), (["left"], 0x800), (["up"], 0xC00)] {
        let before = Field::read(session.runtime.memory()).unwrap();
        let from = before.player().unwrap().position;
        run(&mut session, json!({"cmd": "press", "buttons": buttons, "frames": 12}));
        run(&mut session, json!({"cmd": "frames", "count": 14}));
        let after = Field::read(session.runtime.memory()).unwrap();
        let to = after.player().unwrap().position;
        let moved = heading(to.x - from.x, to.z - from.z);
        let dir = screen_direction(moved, before.camera.angle, before.alt_dpad);
        let error = ((dir - direction + 0x800).rem_euclid(0x1000) - 0x800).abs();
        eprintln!("{buttons:?}: moved {:.1} heading {moved:#x} dir {dir:#x} (angle {:#x})", from.planar(to), before.camera.angle);
        assert!(from.planar(to) > 8.0, "it moved");
        assert!(error < 0x100, "{buttons:?} {dir:#x}");
    }
}

/// Assisted movement walks the player to a floor point by the pad alone,
/// running for distance; the player's own direction replaces it.
#[test]
fn navigate_walks_to_a_floor_point_and_the_players_direction_replaces_it() {
    use xem_core::field::{Field, Reach};
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    let field = Field::read(session.runtime.memory()).unwrap();
    let player = *field.player().unwrap();
    let mesh = &field.layers[player.layer];
    // The farthest walkable triangle's centre.
    let goal = (0..mesh.triangles.len())
        .map(|t| mesh.centroid(t))
        .filter(|c| field.reach(&player, mesh.locate(c.x, c.z).unwrap(), *c) == Reach::Walk)
        .max_by(|a, b| a.planar(player.position).total_cmp(&b.planar(player.position)))
        .unwrap();
    eprintln!("from {:?} to {goal:?} ({:.0} away)", player.position, goal.planar(player.position));
    let reply = session.execute(&json!({"cmd": "navigate", "to": [goal.x, goal.z]}));
    assert!(reply.get("error").is_none(), "{reply}");
    let mut ran = 0;
    let mut ran_with_run = false;
    while session.assist.is_some() && ran < 1200 {
        run(&mut session, json!({"cmd": "frames", "count": 1}));
        ran_with_run |= session.runtime.services().pads.0[0].buttons & xem_core::pad::button::CIRCLE != 0;
        ran += 1;
    }
    let at = Field::read(session.runtime.memory()).unwrap().player().unwrap().position;
    eprintln!("outcome {:?} after {ran} frames at {at:?} ({:.1} off)", session.assist_outcome, at.planar(goal));
    assert_eq!(session.assist_outcome, Some(xem_core::assist::Outcome::Arrived));
    assert!(at.planar(goal) < 12.0);
    assert!(ran_with_run, "it ran for the distance");
    assert_eq!(session.runtime.services().pads.0[0].buttons, 0, "released");
    // Back again, interrupted by the player's own Left.
    run(&mut session, json!({"cmd": "navigate", "to": [player.position.x, player.position.z]}));
    run(&mut session, json!({"cmd": "frames", "count": 10}));
    run(&mut session, json!({"cmd": "hold", "actions": ["Left"]}));
    run(&mut session, json!({"cmd": "frames", "count": 1}));
    assert!(session.assist.is_none());
    assert_eq!(session.assist_outcome, Some(xem_core::assist::Outcome::Replaced));
}

/// Walks a map's opening event through (Confirm on each dialogue page)
/// until the player walks.
fn until_walking(session: &mut Session<NativeModule>) {
    for _ in 0..120 {
        match context(session)["kind"].as_str() {
            Some("walk") => return,
            Some("dialogue") => run(session, json!({"cmd": "act", "actions": ["Confirm"], "frames": 2})),
            _ => {}
        }
        run(session, json!({"cmd": "frames", "count": 30}));
    }
    panic!("the player never walked: {}", context(session));
}

/// Approach-and-interact walks to a villager and presses Confirm once in
/// reach: the villager's dialogue opens. A recorded session with assisted
/// moves replays to the same state from the recorded intents.
#[test]
fn interact_talks_to_a_villager_and_assisted_moves_replay() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 5}));
    run(&mut session, json!({"cmd": "frames", "count": 300}));
    until_walking(&mut session);
    run(&mut session, json!({"cmd": "snapshot", "name": "village"}));
    run(&mut session, json!({"cmd": "record"}));
    run(&mut session, json!({"cmd": "interact", "actor": 11}));
    let mut confirms = 0;
    let mut held = false;
    for _ in 0..600 {
        if session.assist.is_none() { break; }
        run(&mut session, json!({"cmd": "frames", "count": 1}));
        let confirm = session.runtime.services().pads.0[0].buttons & xem_core::pad::button::CROSS != 0;
        if confirm && !held { confirms += 1; }
        held = confirm;
    }
    assert_eq!(session.assist_outcome, Some(xem_core::assist::Outcome::Confirmed));
    assert_eq!(confirms, 1, "Confirm exactly once");
    run(&mut session, json!({"cmd": "frames", "count": 30}));
    assert_eq!(context(&mut session)["kind"], "dialogue");
    let log = session.execute(&json!({"cmd": "stop_recording"}));
    let recorded = session.execute(&json!({"cmd": "memory_hash"}));
    // The same from the snapshot, by the recorded intent alone.
    run(&mut session, json!({"cmd": "restore", "name": "village"}));
    let mut replay = log.clone();
    replay["cmd"] = json!("replay");
    run(&mut session, replay);
    let frames = log["end_vblank"].as_u64().unwrap() - log["start_vblank"].as_u64().unwrap();
    run(&mut session, json!({"cmd": "frames", "count": frames}));
    assert_eq!(session.execute(&json!({"cmd": "memory_hash"})), recorded);
}

/// Pointing at the field menu's commands selects them by the D-pad, and a
/// click confirms: clicking the first command (leave) closes the menu.
#[test]
fn pointing_browses_the_field_menu() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let bindings = xem_settings::GameBindings::default();
    let mut player = xem_settings::player::PlayerInput::default();
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    run(&mut session, json!({"cmd": "act", "actions": ["Menu"], "frames": 2}));
    run(&mut session, json!({"cmd": "frames", "count": 200}));
    let targets = session.runtime.targets().expect("the commands");
    eprintln!("{targets:?}");
    assert_eq!(targets.what, "commands");
    assert_eq!(targets.entries.len(), 7);
    let start = targets.selected.unwrap();
    let goal = (start + 3) % 7;
    player.hover(Some(targets.entries[goal].rect.centre()));
    for _ in 0..80 {
        let targets = session.runtime.targets();
        player.browse(targets.as_ref());
        host_frame(&mut session, &mut player, &bindings);
    }
    assert_eq!(context(&mut session)["cursor"].as_u64().unwrap() as usize, goal, "pointing moved the cursor");
    // Click the first command.
    let targets = session.runtime.targets().unwrap();
    player.hover(Some(targets.entries[0].rect.centre()));
    assert!(player.click(Some(&targets)));
    for _ in 0..400 {
        let targets = session.runtime.targets();
        player.browse(targets.as_ref());
        host_frame(&mut session, &mut player, &bindings);
        if context(&mut session)["kind"] == "walk" { break; }
    }
    assert_eq!(context(&mut session)["kind"], "walk", "leave closed the menu");
}

/// The field menu's lists are cells around their cursor: a click on Items
/// opens the item screen, pointing at a cell steps the cursor there by
/// rows and columns.
#[test]
fn pointing_browses_the_item_list() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let bindings = xem_settings::GameBindings::default();
    let mut player = xem_settings::player::PlayerInput::default();
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    run(&mut session, json!({"cmd": "act", "actions": ["Menu"], "frames": 2}));
    run(&mut session, json!({"cmd": "frames", "count": 200}));
    let commands = session.runtime.targets().expect("the commands");
    // The commands from Exit (0) up to Status (6); Items is the fourth.
    player.hover(Some(commands.entries[4].rect.centre()));
    assert!(player.click(Some(&commands)));
    let mut list = None;
    for _ in 0..300 {
        let targets = session.runtime.targets();
        player.browse(targets.as_ref());
        host_frame(&mut session, &mut player, &bindings);
        if let Some(targets) = targets.filter(|t| t.what == "list") {
            list = Some(targets);
            break;
        }
    }
    assert!(list.is_some(), "the item list");
    // The screen settles (its cursor is drawn where it belongs).
    for _ in 0..60 {
        host_frame(&mut session, &mut player, &bindings);
    }
    let list = session.runtime.targets().expect("the item list");
    assert_eq!((list.entries.len(), list.selected), (16, Some(0)), "two columns of eight rows, the first selected");
    player.hover(Some(list.entries[5].rect.centre()));
    for _ in 0..60 {
        let targets = session.runtime.targets();
        player.browse(targets.as_ref());
        host_frame(&mut session, &mut player, &bindings);
    }
    assert_eq!(session.runtime.targets().unwrap().selected, Some(5), "row three, the second column");
}

/// A shop's quantity buttons edit the selected entry's proposal by the
/// list's own presses: ALL reaches the most the shop allows (here the 99
/// of one purchase), NONE clears it, plus and minus step it.
#[test]
fn shop_quantities_all_none_plus_and_minus() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 5}));
    run(&mut session, json!({"cmd": "frames", "count": 300}));
    until_walking(&mut session);
    // The village's item shop (1), as its script would open it; gold enough.
    run(&mut session, json!({"cmd": "write", "path": "game_data.gold", "value": 5000}));
    run(&mut session, json!({"cmd": "write_memory", "address": 0x80059171u64, "hex": "01"}));
    run(&mut session, json!({"cmd": "write_memory", "address": 0x800ADB64u64, "hex": "84"}));
    run(&mut session, json!({"cmd": "frames", "count": 200}));
    run(&mut session, json!({"cmd": "act", "actions": ["Confirm"], "frames": 2}));
    run(&mut session, json!({"cmd": "frames", "count": 60}));
    assert_eq!(context(&mut session)["quantities"], true, "{}", context(&mut session));
    let amounts = |session: &mut Session<NativeModule>| xem_core::menu::shop_amounts(session.runtime.memory()).unwrap();
    for (op, frames, expected) in [("all", 400, 99), ("minus", 10, 98), ("none", 400, 0), ("plus", 10, 1)] {
        run(&mut session, json!({"cmd": "shop_quantity", "op": op}));
        run(&mut session, json!({"cmd": "frames", "count": frames}));
        assert_eq!(amounts(&mut session), expected, "{op}");
    }
}
