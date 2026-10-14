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
        paused: session.runtime.game_paused(),
    };
    let buttons = player.buttons(bindings, 0.0, context);
    session.set_pad(0, buttons);
    session.set_attack_page(player.attack_pointed());
    run(session, json!({"cmd": "frames", "count": 1}));
    player.frames_ran(1);
}

fn tap(session: &mut Session<NativeModule>, player: &mut xem_settings::player::PlayerInput, bindings: &xem_settings::GameBindings, key: &str, then: usize) {
    player.key(bindings, key, true);
    for _ in 0..2 { host_frame(session, player, bindings); }
    player.key(bindings, key, false);
    for _ in 0..then { host_frame(session, player, bindings); }
}

/// Whether the field's menu read a Cancel press in the last frame (the
/// frame's decoded input, `menu_state_current` +0x325, is 5): it plays its
/// sound for each.
fn menu_read_cancel(session: &mut Session<NativeModule>) -> bool {
    let mut bytes = [0u8; 4];
    session.runtime.memory().read(0x8005_9178, &mut bytes[..1]).unwrap();
    if bytes[0] != 0 {
        return false;
    }
    session.runtime.memory().read(0x8006_25A0, &mut bytes).unwrap();
    let mut input = [0u8];
    session.runtime.memory().read(u32::from_le_bytes(bytes) + 0x325, &mut input).is_ok_and(|()| input[0] == 5)
}

/// Tab for two frames, then host frames until the field menu has closed;
/// the Cancel presses the menu read meanwhile.
fn tab_until_closed(session: &mut Session<NativeModule>, player: &mut xem_settings::player::PlayerInput, bindings: &xem_settings::GameBindings) -> u32 {
    let (mut frames, mut cancels) = (0, 0);
    player.key(bindings, "Tab", true);
    while context(session)["kind"] != "walk" {
        if frames == 2 {
            player.key(bindings, "Tab", false);
        }
        host_frame(session, player, bindings);
        cancels += u32::from(menu_read_cancel(session));
        frames += 1;
        assert!(frames < 600, "the menu closes: {}", context(session));
    }
    cancels
}

/// Tab opens the field menu and, pressed again, closes it with one Cancel
/// at its commands (the menu sounds each Cancel it reads, closing too),
/// or from a screen inside it, a Cancel out of that screen first; the wheel
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
    assert_eq!(context(&mut session)["commands"], true, "{}", context(&mut session));
    assert_eq!(tab_until_closed(&mut session, &mut player, &bindings), 1, "one Cancel at the commands");
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
    assert_eq!(context(&mut session)["open"], true, "{}", context(&mut session));
    assert_eq!(tab_until_closed(&mut session, &mut player, &bindings), 2, "out of the item screen, then at the commands");
}

/// A click on the game while it shows its pause screen (or a tap) resumes
/// it with one Start; playing, a click is the mouse's binding again.
#[test]
fn a_click_resumes_the_original_pause() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let bindings = xem_settings::GameBindings::default();
    let mut player = xem_settings::player::PlayerInput::default();
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    tap(&mut session, &mut player, &bindings, "Enter", 30);
    assert!(session.runtime.game_paused(), "Start pauses the field");
    assert!(player.click(None), "the click is Start's");
    for _ in 0..60 {
        host_frame(&mut session, &mut player, &bindings);
    }
    assert!(!session.runtime.game_paused(), "one Start resumed play");
    assert!(!player.click(None));
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

/// The settings Reset action uses the same command available to agents,
/// re-entering the original boot code with the disc and cards still attached.
#[test]
fn console_reset_reboots_the_original_game_and_keeps_memory_cards() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    assert_eq!(context(&mut session)["kind"], "walk");
    let cards: Vec<_> = (0..2).map(|slot| {
        let mut image = xem_core::devices::card::formatted();
        image[0x2000..0x2004].copy_from_slice(&[b'X', b'E', b'M', slot as u8]);
        let now = session.runtime.services().clock.now;
        session.runtime.services().card.change(slot, image.clone(), now).unwrap();
        image
    }).collect();
    session.configure_rewind(xem_core::rewind::MAX_REWIND_BYTES, 1, 30).unwrap();
    let revision = session.timeline_revision;
    run(&mut session, json!({"cmd": "hold", "actions": ["Up", "Run"]}));
    run(&mut session, json!({"cmd": "reset"}));
    assert_eq!(session.runtime.services().pads.0[0].buttons, 0);
    assert_eq!(session.timeline_revision, revision + 1);
    assert_eq!(session.rewind.status()["states"], 0);
    assert_eq!(session.runtime.services().mode, None);
    run(&mut session, json!({"cmd": "frames", "count": 300}));
    assert_eq!(session.runtime.services().mode, Some(6), "boot enters the opening movie");
    for (slot, card) in cards.iter().enumerate() {
        assert_eq!(session.runtime.services().card.image(slot), Some(card.as_slice()));
    }
}

/// Leave Lahan through its south exit, then walk until the original world
/// map rolls an encounter. Query input exactly as the desktop does through
/// every dispatcher wait, overlay load and battle initialization frame.
#[test]
fn control_queries_survive_a_world_map_encounter() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    for command in [
        json!({"cmd":"enter_field", "map":1}),
        json!({"cmd":"frames", "count":400}),
        json!({"cmd":"navigate", "to":[-300,-1800]}),
        json!({"cmd":"frames", "count":600}),
        json!({"cmd":"press", "buttons":["right","down","cross"], "frames":100}),
        json!({"cmd":"frames", "count":100}),
        json!({"cmd":"navigate", "to":[-300,-1800]}),
        json!({"cmd":"frames", "count":500}),
        json!({"cmd":"press", "buttons":["down","cross"], "frames":250}),
        json!({"cmd":"frames", "count":550}),
    ] {
        run(&mut session, command);
    }
    assert!(matches!(session.runtime.context(), xem_core::context::Context::WorldMap { movement: 1, .. }));
    let bindings = xem_settings::GameBindings::default();
    let mut player = xem_settings::player::PlayerInput::default();
    player.key(&bindings, "KeyS", true);
    let mut loading = 0;
    let mut battle = false;
    for _ in 0..600 {
        if session.runtime.services().mode == Some(2) {
            player.clear();
            if session.execute(&json!({"cmd":"inspect", "path":"mode_next_mode"})) == 2 {
                loading += 1;
                assert!(!session.runtime.combo_input(), "no attack input before the overlay's entry");
                assert_eq!(session.runtime.field_controls(), Default::default());
            }
            battle |= matches!(session.runtime.context(), xem_core::context::Context::Battle { .. });
        }
        host_frame(&mut session, &mut player, &bindings);
    }
    assert!(loading > 0, "the host observed the dispatcher before overlay entry");
    assert!(battle, "the naturally rolled encounter reached the battle interface");
    assert_eq!(session.runtime.services().mode, Some(2));
    assert!(session.stopped.is_none());
}

/// The debug battle selector's battle on foot (ovl2606), at the player's
/// first command ring.
fn debug_battle() -> Option<Session<NativeModule>> {
    let disc = disc()?;
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "frames", "count": 60}));
    run(&mut session, json!({"cmd": "write", "path": "mode_battle_standalone", "value": 1}));
    run(&mut session, json!({"cmd": "boot_mode", "mode": 2}));
    run(&mut session, json!({"cmd": "frames", "count": 900}));
    run(&mut session, json!({"cmd": "press", "buttons": ["start"], "frames": 1}));
    for _ in 0..80 {
        run(&mut session, json!({"cmd": "frames", "count": 50}));
        let context = context(&mut session);
        if context["kind"] == "battle" && context["page"] == 1 {
            run(&mut session, json!({"cmd": "frames", "count": 4}));
            return Some(session);
        }
    }
    panic!("no command ring: {}", context(&mut session));
}

/// One host frame with the mouse: the pointer steers to the interface's
/// entries, then the frame runs.
fn pointer_frame(session: &mut Session<NativeModule>, player: &mut xem_settings::player::PlayerInput, bindings: &xem_settings::GameBindings) {
    let targets = session.runtime.targets();
    player.browse(targets.as_ref());
    host_frame(session, player, bindings);
}

/// A point on entry `index` that no other entry takes (`Targets::at`).
fn point_on(targets: &xem_core::menu::Targets, index: usize) -> (f32, f32) {
    let rect = targets.entries[index].rect;
    (0..20).flat_map(|i| (0..20).map(move |j| (rect.x + rect.width * (i as f32 + 0.5) / 20.0, rect.y + rect.height * (j as f32 + 0.5) / 20.0)))
        .find(|point| targets.at(*point) == Some(index))
        .unwrap_or_else(|| panic!("entry {index} is covered: {targets:?}"))
}

/// Clicks entry `index` of the interface's entries and plays `frames` host
/// frames.
fn click(session: &mut Session<NativeModule>, player: &mut xem_settings::player::PlayerInput, bindings: &xem_settings::GameBindings, index: usize, frames: usize) {
    let targets = session.runtime.targets().expect("entries to click");
    player.hover(Some(point_on(&targets, index)));
    assert!(player.click(Some(&targets)), "entry {index} of {targets:?}");
    for _ in 0..frames {
        pointer_frame(session, player, bindings);
    }
}

/// The shade of the attack page's first attack glyph as the panel draws it
/// (battle graphics +0x641C, list 0 in the panel's buffer): 0x80 shown,
/// down to 0 as the panel fades.
fn attack_page_shade(session: &mut Session<NativeModule>) -> u8 {
    let word = |session: &mut Session<NativeModule>, address: u32| {
        let mut bytes = [0; 4];
        session.runtime.memory().read(address, &mut bytes).unwrap();
        u32::from_le_bytes(bytes)
    };
    let graphics = word(session, 0x800C_3EA4);
    let ui = word(session, 0x800D_2D28);
    let mut byte = [0; 1];
    session.runtime.memory().read(ui + 0xA3, &mut byte).unwrap();
    session.runtime.memory().read(graphics + 0x641C + u32::from(byte[0]) * 0x28 + 4, &mut byte).unwrap();
    byte[0]
}

/// The entry of the interface's entries that `matches`.
fn entry(session: &mut Session<NativeModule>, matches: fn(&xem_core::menu::Entry) -> bool) -> usize {
    let targets = session.runtime.targets().expect("entries");
    targets.entries.iter().position(matches).unwrap_or_else(|| panic!("no such entry: {targets:?}"))
}

/// The mouse plays a battle: pointing at the ring's Item highlights it, a
/// page arrow flips the ring to the other page's slot its way, a click on
/// Attack opens the attack page, where a click on the enemy keeps it as the
/// target without attacking and clicks on the attacks spend their AP until
/// the end ends the sequence; begun by a click, the sequence keeps its
/// attack page shown.
#[test]
fn the_mouse_plays_the_command_ring_and_the_attacks() {
    use xem_core::action::GameAction;
    let Some(mut session) = debug_battle() else { return };
    let bindings = xem_settings::GameBindings::default();
    let mut player = xem_settings::player::PlayerInput::default();
    let ring = session.runtime.targets().expect("the ring");
    assert_eq!((ring.what, ring.entries.len()), ("battle_ring", 7), "{ring:?}");
    assert_eq!(ring.selected.map(|s| ring.entries[s].select), Some(Some(GameAction::Right)), "Attack: {ring:?}");
    let item = entry(&mut session, |e| e.select == Some(GameAction::Down));
    player.hover(Some(point_on(&ring, item)));
    for _ in 0..20 {
        pointer_frame(&mut session, &mut player, &bindings);
    }
    assert_eq!(context(&mut session)["page"], 2, "pointing at Item highlights it");
    let up = entry(&mut session, |e| e.press == Some(GameAction::Up));
    click(&mut session, &mut player, &bindings, up, 20);
    assert_eq!(context(&mut session)["page"], 0xA, "the up arrow flips the ring to its other page's Up slot");
    let left = entry(&mut session, |e| e.press == Some(GameAction::Left));
    click(&mut session, &mut player, &bindings, left, 20);
    assert_eq!(context(&mut session)["page"], 3, "the left arrow flips it back, to Defense");
    let attack = entry(&mut session, |e| e.select == Some(GameAction::Right));
    click(&mut session, &mut player, &bindings, attack, 40);
    let attack = context(&mut session);
    assert_eq!((attack["page"].as_u64(), attack["attack"].as_bool()), (Some(5), Some(true)), "Attack opened the attack page: {attack}");
    let ap = attack["ap"].as_u64().unwrap();
    let page = session.runtime.targets().expect("the attack page");
    assert_eq!(page.what, "battle_attack");
    let enemy = page.entries.iter().position(|e| e.ground.is_some_and(|g| g.slot == 3)).expect("the enemy");
    click(&mut session, &mut player, &bindings, enemy, 60);
    let still = context(&mut session);
    assert_eq!((still["page"].as_u64(), still["ap"].as_u64()), (Some(5), Some(ap)), "choosing the target attacks nothing: {still}");
    let weak = entry(&mut session, |e| e.press == Some(GameAction::AttackWeak));
    click(&mut session, &mut player, &bindings, weak, 60);
    assert_eq!(context(&mut session)["ap"].as_u64(), Some(ap - 1), "Weak costs one");
    assert_eq!(attack_page_shade(&mut session), 0x80, "the attack page stays shown");
    let strong = entry(&mut session, |e| e.press == Some(GameAction::AttackStrong));
    click(&mut session, &mut player, &bindings, strong, 60);
    assert_eq!(context(&mut session)["ap"].as_u64(), Some(ap - 3), "Strong costs two");
    assert_eq!(attack_page_shade(&mut session), 0x80);
    let end = entry(&mut session, |e| e.press == Some(GameAction::AttackEnd));
    click(&mut session, &mut player, &bindings, end, 30);
    assert_eq!(context(&mut session)["attack"], false, "the sequence ended: {}", context(&mut session));
    assert_eq!(session.execute(&json!({"cmd": "status"}))["attack_page"], false, "and the page is the game's again");
}

/// A sequence the player's buttons begin fades its attack page as the
/// original does, the attacks still pointed at where it showed them.
#[test]
fn a_sequence_the_buttons_begin_fades_its_attack_page() {
    let Some(mut session) = debug_battle() else { return };
    let bindings = xem_settings::GameBindings::default();
    let mut player = xem_settings::player::PlayerInput::default();
    tap(&mut session, &mut player, &bindings, "KeyE", 40);
    assert_eq!(context(&mut session)["attack"], true, "{}", context(&mut session));
    let ap = context(&mut session)["ap"].as_u64().unwrap();
    tap(&mut session, &mut player, &bindings, "KeyW", 60);
    assert_eq!(context(&mut session)["ap"].as_u64(), Some(ap - 1), "Weak, from its key");
    assert_eq!(attack_page_shade(&mut session), 0, "the page faded");
    assert_eq!(session.execute(&json!({"cmd": "status"}))["attack_page"], false);
    let strong = entry(&mut session, |e| e.press == Some(xem_core::action::GameAction::AttackStrong));
    click(&mut session, &mut player, &bindings, strong, 60);
    assert_eq!(context(&mut session)["ap"].as_u64(), Some(ap - 3), "Strong, clicked where the faded page showed it");
    assert_eq!(attack_page_shade(&mut session), 0, "the page stays faded");
}

/// The mouse chooses an item's target: a click on Item opens the list, a
/// click on its fourth item asks for a target, and a click on Elly gives
/// it to her.
#[test]
fn the_mouse_chooses_a_battle_target() {
    let Some(mut session) = debug_battle() else { return };
    let bindings = xem_settings::GameBindings::default();
    let mut player = xem_settings::player::PlayerInput::default();
    let item = entry(&mut session, |e| e.select == Some(xem_core::action::GameAction::Down));
    click(&mut session, &mut player, &bindings, item, 60);
    assert_eq!(context(&mut session)["list"], true, "the item list: {}", context(&mut session));
    click(&mut session, &mut player, &bindings, 3, 60);
    assert_eq!(context(&mut session)["targeting"], true, "{}", context(&mut session));
    let targets = session.runtime.targets().expect("the targets");
    let elly = targets.entries.iter().position(|e| e.ground.is_some_and(|g| g.slot == 1)).expect("Elly");
    player.hover(Some(point_on(&targets, elly)));
    assert!(player.click(Some(&targets)));
    for _ in 0..120 {
        pointer_frame(&mut session, &mut player, &bindings);
        if context(&mut session)["targeting"] != true { break; }
    }
    assert_eq!(context(&mut session)["targeting"], false, "confirmed: {}", context(&mut session));
    assert_eq!(session.execute(&json!({"cmd": "inspect", "path": "battle_target_cursor_slot"})), json!(1), "Elly");
}

/// A reset leaves nothing of the abandoned game's sound: the field's music
/// stops at once, the boot's first jingle sounds as it does after power-on,
/// a second at a time (the reset game's sound plays a frame later; its loud
/// seconds within half a decibel), and the sound driver ticks as often as
/// after power-on (no event of the abandoned game's left to tick it too).
#[test]
fn console_reset_leaves_none_of_the_abandoned_games_sound() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let options = xem_game::BootOptions { audio_output: true, ..Default::default() };
    let levels = |session: &mut Session<NativeModule>| -> Vec<[Option<f64>; 2]> {
        (0..24).map(|_| {
            run(session, json!({"cmd": "audio_capture", "action": "start"}));
            run(session, json!({"cmd": "frames", "count": 60}));
            let capture = session.execute(&json!({"cmd": "audio_capture", "action": "stop"}));
            [capture["rms_db"][0].as_f64(), capture["rms_db"][1].as_f64()]
        }).collect()
    };
    let sounding = |session: &mut Session<NativeModule>| {
        (0..xem_spu::VOICES).filter(|&v| session.runtime.services().spu.chip().voice(v).phase != xem_spu::Phase::Off).count()
    };
    let (mut clean, _) = boot(&disc, &options).expect("boot");
    let power_on = levels(&mut clean);
    let (mut session, _) = boot(&disc, &options).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 600}));
    assert!(sounding(&mut session) > 0, "the field's music plays");
    run(&mut session, json!({"cmd": "reset"}));
    assert_eq!(sounding(&mut session), 0, "the reset silenced it");
    assert_eq!(session.runtime.services().spu.audio_queued(), 0, "and dropped what it had not played");
    let reset = levels(&mut session);
    let loud: Vec<_> = power_on.iter().zip(&reset).enumerate()
        .flat_map(|(second, (a, b))| (0..2).filter_map(move |side| Some((second, a[side]?, b[side]?))))
        .filter(|(_, a, _)| *a > -30.0)
        .collect();
    assert!(loud.len() >= 20, "the jingle plays after power-on: {power_on:?}");
    for (second, a, b) in loud {
        assert!((a - b).abs() < 0.5, "second {second}: {a:.1} dB after power-on, {b:.1} dB after the reset");
    }
    let ticks = |session: &mut Session<NativeModule>| {
        let count = |session: &mut Session<NativeModule>| session.execute(&json!({"cmd": "inspect", "path": "sound_tick_count", "depth": 0})).as_u64().unwrap();
        let before = count(session);
        run(session, json!({"cmd": "frames", "count": 600}));
        count(session) - before
    };
    assert_eq!(ticks(&mut session), ticks(&mut clean), "the sound driver's ticks in ten seconds");
}
