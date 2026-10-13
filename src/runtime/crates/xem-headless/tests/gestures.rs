//! The headset's hand and gaze controls on the original game (local only:
//! it needs the user's disc 1 and a game module build). A harness plays the
//! host's part: synthetic hands (`xem_assist::synthetic`) in front of a
//! head whose eye gaze aims at a game screen pixel of a virtual screen,
//! recognised and resolved by the shared headset core, carried out through
//! the player's input and the command layer, one game frame per headset
//! frame, and no game frame while the player's Pause or Settings hold it.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test gestures

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::path::PathBuf;
use xem_assist::headset::{Carry, Headset, Input, PointerSource, Pose, Quad, Scaling, Screen};
use xem_assist::synthetic;
use xem_core::action::GameAction;
use xem_core::control::Session;
use xem_game::{NativeModule, boot};
use xem_render::glam::{Quat, Vec3};
use xem_settings::player::{Context, PlayerInput, Pulse};

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn run(session: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = session.execute(&command);
    assert!(reply.get("error").is_none(), "{command} -> {reply}");
    reply
}

struct Carried<'a> {
    session: &'a mut Session<NativeModule>,
    input: &'a mut PlayerInput,
}

impl Carry for Carried<'_> {
    fn command(&mut self, command: Value) {
        let reply = self.session.execute(&command);
        assert!(reply.get("error").is_none(), "{command} -> {reply}");
    }
    fn press(&mut self, actions: Vec<GameAction>) {
        self.input.pulse(Pulse::new(actions));
    }
    fn select(&mut self, index: usize, confirm: bool) {
        self.input.select(index, confirm);
    }
    fn inspect(&mut self) {}
    fn release(&mut self) {
        self.input.clear();
    }
}

/// The host's part, headless.
struct Player {
    session: Session<NativeModule>,
    input: PlayerInput,
    headset: Headset,
    scaling: Scaling,
    bindings: xem_settings::GameBindings,
    screen: Screen,
    time: f64,
    /// Game frames run (none while paused).
    ran: u64,
}

impl Player {
    fn new(session: Session<NativeModule>) -> Player {
        let screen = Screen {
            quad: Quad { pose: Pose { position: Vec3::new(0.0, 1.6, -2.5), orientation: Quat::IDENTITY }, size: (3.2, 2.24) },
            display: (320.0, 224.0),
            side: 0.0,
        };
        let mut headset = Headset::default();
        headset.source = PointerSource::Eye;
        Player { session, input: PlayerInput::default(), headset, scaling: Scaling::default(), bindings: Default::default(), screen, time: 0.0, ran: 0 }
    }

    fn context(&mut self) -> Value {
        self.session.execute(&json!({"cmd": "context"}))
    }

    /// One headset frame with these hands and the gaze on `aim`.
    fn frame(&mut self, hands: [Option<[Vec3; 25]>; 2], aim: Option<(f32, f32)>) {
        let head = Pose { position: Vec3::new(0.0, 1.6, 0.0), orientation: Quat::IDENTITY };
        let gaze = aim.map(|aim| Pose {
            position: head.position,
            orientation: Quat::from_rotation_arc(Vec3::NEG_Z, (self.screen.point(aim) - head.position).normalize()),
        });
        let input = Input {
            time: self.time,
            head: Some(head),
            gaze,
            hands: synthetic::in_front(hands, head.position, head.orientation),
            focused: true,
            ..Input::default()
        };
        self.time += 1.0 / 60.0;
        let ui = self.session.runtime.context();
        let field = (self.session.runtime.services().mode == Some(1)).then(|| xem_core::field::Field::read(self.session.runtime.memory())).flatten();
        let targets = self.session.runtime.targets();
        let effects = self.headset.update(&input, &self.screen, ui, field.as_ref(), targets.as_ref());
        self.input.touch = effects.held.clone();
        xem_assist::headset::carry(effects.outs, &mut Carried { session: &mut self.session, input: &mut self.input }, &mut self.scaling);
        if self.headset.pauses.paused() {
            return;
        }
        let context = Context {
            attack: self.session.runtime.combo_input(),
            field: self.session.runtime.field_controls(),
            assignment: self.session.runtime.button_assignment(),
            ui,
        };
        self.input.browse(targets.as_ref());
        let buttons = self.input.buttons(&self.bindings, 0.0, context);
        self.session.set_pad(0, buttons);
        run(&mut self.session, json!({"cmd": "frames", "count": 1}));
        self.input.frames_ran(1);
        self.ran += 1;
    }

    fn rest(&mut self, frames: usize) {
        let open = synthetic::gesture("pause").unwrap()[0];
        for _ in 0..frames {
            self.frame(open, None);
        }
    }

    /// A gesture's frames with the gaze on `aim`, then a few at rest.
    fn gesture(&mut self, name: &str, aim: Option<(f32, f32)>) {
        for hands in synthetic::gesture(name).unwrap() {
            self.frame(hands, aim);
        }
        let open = synthetic::gesture("pause").unwrap()[0];
        for _ in 0..4 {
            self.frame(open, aim);
        }
    }

    fn field(&mut self) -> xem_core::field::Field {
        xem_core::field::Field::read(self.session.runtime.memory()).expect("a field")
    }
}

fn walking(map: u64) -> Option<Player> {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return None;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": map}));
    run(&mut session, json!({"cmd": "frames", "count": 300}));
    let mut player = Player::new(session);
    for _ in 0..120 {
        match player.context()["kind"].as_str() {
            Some("walk") => break,
            Some("dialogue") => { run(&mut player.session, json!({"cmd": "act", "actions": ["Confirm"], "frames": 2})); }
            _ => {}
        }
        run(&mut player.session, json!({"cmd": "frames", "count": 30}));
    }
    assert_eq!(player.context()["kind"], "walk");
    player.rest(4);
    Some(player)
}

/// A pinch with the gaze on the floor walks there; the Pause grab holds
/// the game mid-route and the next grab lets it finish; a thumb tap on
/// the ground stops a route.
#[test]
fn pinch_walks_where_the_gaze_is_and_pause_holds_the_route() {
    let Some(mut player) = walking(30) else { return };
    let field = player.field();
    let goal = xem_core::field::V3::new(-80.0, 0.0, -90.0);
    let aim = field.camera.project(goal).unwrap();
    player.gesture("pinch", Some(aim));
    assert!(player.session.assist.is_some(), "walking: {:?}", player.session.assist_outcome);
    player.rest(6);
    // Pause mid-route: no game frame runs.
    player.gesture("pause", None);
    assert!(player.headset.pauses.player);
    let ran = player.ran;
    player.rest(30);
    assert_eq!(player.ran, ran, "paused");
    player.gesture("pause", None);
    assert!(!player.headset.pauses.player);
    for _ in 0..200 {
        if player.session.assist.is_none() { break; }
        player.rest(1);
    }
    assert_eq!(player.session.assist_outcome, Some(xem_core::assist::Outcome::Arrived));
    let at = player.field().player().unwrap().position;
    assert!(at.planar(goal) < 16.0, "{at:?}");
    // A new route, stopped by a tap on the ground (away from any actor).
    let field = player.field();
    let mesh = &field.layers[0];
    let ground = (0..mesh.triangles.len())
        .filter_map(|t| field.camera.project(mesh.centroid(t)).filter(|_| field.reach(field.player().unwrap(), t, mesh.centroid(t)) == xem_core::field::Reach::Walk))
        .filter(|p| field.actor_at(*p, 30.0).is_none() && field.pick(*p).is_some())
        .max_by(|a, b| a.1.total_cmp(&b.1))
        .expect("open ground");
    player.gesture("pinch", Some(ground));
    player.rest(4);
    assert!(player.session.assist.is_some());
    player.gesture("tap", Some(ground));
    assert_eq!(player.session.assist_outcome, Some(xem_core::assist::Outcome::Stopped));
}

/// A thumb tap on a villager approaches and talks to them; taps advance
/// the dialogue; a swipe down opens the game menu and a tap on Exit, gazed
/// at, closes it.
#[test]
fn tap_talks_to_a_villager_and_menus_take_gaze_and_taps() {
    let Some(mut player) = walking(5) else { return };
    let field = player.field();
    let villager = field.actors[11].unwrap().position;
    let aim = field.camera.project(villager).unwrap();
    player.gesture("tap", Some(aim));
    for _ in 0..600 {
        if player.session.assist.is_none() { break; }
        player.rest(1);
    }
    assert_eq!(player.session.assist_outcome, Some(xem_core::assist::Outcome::Confirmed));
    player.rest(30);
    assert_eq!(player.context()["kind"], "dialogue");
    let mut taps = 0;
    while player.context()["kind"] != "walk" {
        // A choice answers only under the gaze: its first option.
        let aim = player.context()["choice"].is_object()
            .then(|| player.session.runtime.targets().expect("the choice's lines").entries[0].rect.centre());
        if aim.is_none() && player.context()["choice"].is_object() { panic!("no choice lines"); }
        player.gesture("tap", aim);
        player.rest(20);
        taps += 1;
        assert!(taps < 120, "the dialogue ends: {}", player.context());
    }
    eprintln!("{taps} taps through the dialogue");
    // The game menu, then Exit (its first command) under the gaze.
    player.gesture("swipe-down", None);
    player.rest(200);
    assert_eq!(player.context()["kind"], "menu");
    let exit = player.session.runtime.targets().expect("the commands").entries[0].rect.centre();
    player.gesture("tap", Some(exit));
    for _ in 0..300 {
        if player.context()["kind"] == "walk" { break; }
        player.rest(1);
    }
    assert_eq!(player.context()["kind"], "walk");
}

/// In a shop, a tap on Buy under the gaze opens the list; ALL beside the
/// screen proposes the most allowed, NONE clears it.
#[test]
fn shop_commands_and_quantity_keys_take_gaze_and_taps() {
    let Some(mut player) = walking(5) else { return };
    run(&mut player.session, json!({"cmd": "write", "path": "game_data.gold", "value": 5000}));
    run(&mut player.session, json!({"cmd": "write_memory", "address": 0x80059171u64, "hex": "01"}));
    run(&mut player.session, json!({"cmd": "write_memory", "address": 0x800ADB64u64, "hex": "84"}));
    player.rest(200);
    let targets = player.session.runtime.targets().expect("the shop's commands");
    assert_eq!(targets.entries.len(), 3);
    // Sell (1), then back to Buy (2): the gaze moves the selection.
    player.gesture("tap", Some(targets.entries[2].rect.centre()));
    player.rest(60);
    assert_eq!(player.context()["quantities"], true, "{}", player.context());
    // The keys stand beside the picture: the gaze reaches them through the
    // screen's plane, past its edge.
    let keys = xem_assist::draw::side_keys(&player.screen, xem_assist::overlay::QuantityKey::ALL.len());
    let gaze_at = |player: &mut Player, i: usize, name: &str| {
        let local = keys[i].1.pose.position - player.screen.quad.pose.position;
        let x = (local.x / player.screen.quad.size.0 + 0.5) * 320.0;
        let y = (0.5 - local.y / player.screen.quad.size.1) * 224.0;
        assert!(x > 320.0, "beside the picture, not on it");
        player.gesture(name, Some((x, y)));
    };
    gaze_at(&mut player, 2, "tap");
    player.rest(400);
    assert_eq!(xem_core::menu::shop_amounts(player.session.runtime.memory()), Some(99));
    gaze_at(&mut player, 3, "tap");
    player.rest(400);
    assert_eq!(xem_core::menu::shop_amounts(player.session.runtime.memory()), Some(0));
}

/// Losing the hands mid-route stops the route and pauses the game until
/// the player's Pause.
#[test]
fn losing_tracking_stops_the_route_and_pauses() {
    let Some(mut player) = walking(30) else { return };
    let aim = player.field().camera.project(xem_core::field::V3::new(-80.0, 0.0, -90.0)).unwrap();
    player.gesture("pinch", Some(aim));
    player.rest(4);
    assert!(player.session.assist.is_some());
    player.gesture("lose", None);
    assert!(player.session.assist.is_none(), "the route stopped");
    assert!(player.headset.pauses.player, "paused");
    let ran = player.ran;
    player.rest(20);
    assert_eq!(player.ran, ran);
    assert_eq!(player.input.touch.len(), 0);
    player.gesture("pause", None);
    assert!(!player.headset.pauses.player, "the player resumed");
}

/// The debug battle selector's battle (ovl2606), on foot or in Gears, at
/// the player's first command ring.
fn battle(gears: bool) -> Option<Player> {
    let disc = disc()?;
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "frames", "count": 60}));
    run(&mut session, json!({"cmd": "write", "path": "mode_battle_standalone", "value": 1}));
    run(&mut session, json!({"cmd": "boot_mode", "mode": 2}));
    run(&mut session, json!({"cmd": "frames", "count": 900}));
    if gears {
        for button in ["down", "down", "cross", "right", "cross", "right", "cross"] {
            run(&mut session, json!({"cmd": "press", "buttons": [button], "frames": 1}));
            run(&mut session, json!({"cmd": "frames", "count": 20}));
        }
    }
    run(&mut session, json!({"cmd": "press", "buttons": ["start"], "frames": 1}));
    let mut player = Player::new(session);
    let first = if gears { 0x10 } else { 1 };
    for _ in 0..80 {
        run(&mut player.session, json!({"cmd": "frames", "count": 50}));
        let context = player.context();
        if context["kind"] == "battle" && context["page"] == first {
            player.rest(4);
            return Some(player);
        }
    }
    panic!("no command ring: {}", player.context());
}

/// On foot: a tap pages the command ring, a swipe activates the slot in
/// its direction; on the attack ring the four swipes are Weak, Strong,
/// Fierce and the end of the sequence, each spending its AP.
#[test]
fn battle_rings_on_foot_take_swipes_and_taps() {
    let Some(mut player) = battle(false) else { return };
    let page = |player: &mut Player| player.context()["page"].as_u64().unwrap();
    player.gesture("tap", None);
    player.rest(20);
    assert_eq!(page(&mut player), 7, "the ring's other page");
    player.gesture("tap", None);
    player.rest(20);
    assert_eq!(page(&mut player), 1, "and back");
    // Attack (Right), highlighted: the swipe activates it.
    player.gesture("swipe-right", None);
    player.rest(30);
    let attack = player.context();
    assert_eq!(attack["attack"], true, "{attack}");
    let ap = attack["ap"].as_u64().unwrap();
    player.gesture("swipe-up", None);
    player.rest(60);
    assert_eq!(player.context()["ap"].as_u64().unwrap(), ap - 1, "Weak costs one: {}", player.context());
    player.gesture("swipe-left", None);
    player.rest(60);
    assert_eq!(player.context()["ap"].as_u64().unwrap(), ap - 3, "Strong costs two: {}", player.context());
    player.gesture("swipe-right", None);
    player.rest(30);
    assert_eq!(player.context()["attack"], false, "the sequence ended: {}", player.context());
}

/// In Gears the ring is the Gear's (pages 0x10..), and a swipe up on its
/// attack ring strikes.
#[test]
fn battle_rings_in_gears_take_swipes() {
    let Some(mut player) = battle(true) else { return };
    player.gesture("swipe-right", None);
    player.rest(30);
    let attack = player.context();
    assert_eq!(attack["page"], 0x19, "the Gear's attack ring: {attack}");
    player.gesture("swipe-up", None);
    player.rest(60);
    let after = player.context();
    assert_ne!(after["page"], 0x19, "it struck: {after}");
}

/// A battle's list and targets: a tap with the gaze on the item list's
/// fourth item (the second column's second row) chooses it, a swipe left
/// moves to the nearest target that way (the enemy), and a tap with the
/// gaze on a party member steers the cursor there the game's own way and
/// confirms it.
#[test]
fn battle_targets_take_swipes_and_gaze() {
    let Some(mut player) = battle(false) else { return };
    player.gesture("swipe-down", None);
    player.rest(60);
    assert_eq!(player.context()["list"], true, "{}", player.context());
    let list = player.session.runtime.targets().expect("the item list");
    assert_eq!((list.what, list.entries.len(), list.selected), ("battle_list", 16, Some(0)));
    player.gesture("tap", Some(list.entries[3].rect.centre()));
    let mut last = None;
    for _ in 0..60 {
        match player.session.runtime.targets() {
            Some(t) if t.what == "battle_list" => last = t.selected,
            _ => {}
        }
        if player.context()["targeting"] == true { break; }
        player.rest(1);
    }
    assert_eq!(last, Some(3), "the fourth item was selected, then confirmed");
    assert_eq!(player.context()["targeting"], true, "{}", player.context());
    let slot = |player: &mut Player| {
        let targets = player.session.runtime.targets().expect("the targets");
        targets.entries[targets.selected.unwrap()].ground.unwrap().slot
    };
    assert_eq!(slot(&mut player), 0, "Fei first");
    player.gesture("swipe-left", None);
    player.rest(20);
    assert_eq!(slot(&mut player), 3, "the enemy, left of Fei");
    let targets = player.session.runtime.targets().unwrap();
    let (index, member) = targets.entries.iter().enumerate()
        .find(|(_, e)| e.ground.unwrap().slot < 3 && (0.0..320.0).contains(&e.rect.centre().0) && (0.0..224.0).contains(&e.rect.centre().1))
        .expect("a party member on the screen");
    let wanted = member.ground.unwrap().slot;
    player.gesture("tap", Some(member.rect.centre()));
    for _ in 0..120 {
        if player.context()["targeting"] != true { break; }
        player.rest(1);
    }
    assert_eq!(player.context()["targeting"], false, "confirmed: {}", player.context());
    let cursor = run(&mut player.session, json!({"cmd": "inspect", "path": "battle_target_cursor_slot"}));
    assert_eq!(cursor, json!(wanted), "the cursor reached entry {index}");
}

/// A battle's target list beside the screen (for targets that overlap or
/// hide one another): a tap with the gaze on a key chooses its target the
/// same way, the selected one lit.
#[test]
fn battle_target_list_beside_the_screen_takes_gaze_and_taps() {
    let Some(mut player) = battle(false) else { return };
    player.gesture("swipe-down", None);
    player.rest(60);
    player.gesture("tap", None);
    player.rest(40);
    assert_eq!(player.context()["targeting"], true, "{}", player.context());
    let targets = player.session.runtime.targets().unwrap();
    let keys = xem_assist::overlay::side_keys(player.session.runtime.context(), Some(&targets));
    let labels: Vec<_> = keys.iter().map(|k| (k.label.as_str(), k.lit)).collect();
    assert_eq!(labels, [("ALLY 1", true), ("ALLY 2", false), ("ALLY 3", false), ("FOE 1", false)]);
    let quads = xem_assist::draw::side_keys(&player.screen, keys.len());
    let local = quads[1].1.pose.position - player.screen.quad.pose.position;
    let aim = ((local.x / player.screen.quad.size.0 + 0.5) * 320.0, (0.5 - local.y / player.screen.quad.size.1) * 224.0);
    player.gesture("tap", Some(aim));
    for _ in 0..120 {
        if player.context()["targeting"] != true { break; }
        player.rest(1);
    }
    assert_eq!(player.context()["targeting"], false, "confirmed: {}", player.context());
    let cursor = run(&mut player.session, json!({"cmd": "inspect", "path": "battle_target_cursor_slot"}));
    assert_eq!(cursor, json!(1), "Elly, the second ally");
}

/// The world map's control deck: a pinch captures the stick, the hand's
/// travel ahead holds Up (the vehicle flies on), a second pinch lets go,
/// and the Board key beside the screen presses L2 and R2 together.
#[test]
fn the_world_maps_deck_steers_by_the_captured_stick() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "boot_mode", "mode": 3}));
    run(&mut session, json!({"cmd": "frames", "count": 900}));
    let mut player = Player::new(session);
    assert_eq!(player.context()["kind"], "world_map", "{}", player.context());
    player.rest(4);
    player.gesture("pinch", None);
    assert!(player.headset.arcade.steering(), "the stick is captured");
    let rest = synthetic::gesture("pause").unwrap()[0];
    let ahead = [rest[0], rest[1].map(|h| synthetic::place(h, Quat::IDENTITY, Vec3::new(0.0, 0.0, -0.07)))];
    let before = run(&mut player.session, json!({"cmd": "memory_hash"}));
    let mut up = 0;
    for _ in 0..30 {
        player.frame(ahead, None);
        up += usize::from(player.session.runtime.services().pads.0[0].buttons & xem_core::pad::button::UP != 0);
    }
    assert!(up >= 28, "Up held while the hand is ahead ({up} of 30)");
    assert_ne!(run(&mut player.session, json!({"cmd": "memory_hash"})), before);
    player.gesture("pinch", None);
    assert!(!player.headset.arcade.steering(), "let go");
    player.rest(2);
    assert_eq!(player.session.runtime.services().pads.0[0].buttons & xem_core::pad::button::UP, 0);
    // Board: the key beside the screen, gazed at and tapped.
    let keys = xem_assist::draw::side_keys(&player.screen, xem_assist::arcade::keys(xem_assist::arcade::Deck::WorldMap).len());
    let board = xem_assist::arcade::keys(xem_assist::arcade::Deck::WorldMap).iter().position(|k| k.label == "BOARD").unwrap();
    let local = keys[board].1.pose.position - player.screen.quad.pose.position;
    let aim = ((local.x / player.screen.quad.size.0 + 0.5) * 320.0, (0.5 - local.y / player.screen.quad.size.1) * 224.0);
    let both = xem_core::pad::button::L2 | xem_core::pad::button::R2;
    let mut pressed = false;
    for hands in synthetic::gesture("tap").unwrap() {
        player.frame(hands, Some(aim));
        pressed |= player.session.runtime.services().pads.0[0].buttons & both == both;
    }
    for _ in 0..4 {
        player.frame(rest, Some(aim));
        pressed |= player.session.runtime.services().pads.0[0].buttons & both == both;
    }
    assert!(pressed, "L2 and R2 together");
}

/// A Battling bout's control deck: steering by the captured stick, a tap
/// for A (Cross), a swipe down holding Dash (Circle) until the next, and
/// the Pause grab available throughout.
#[test]
fn a_battling_bout_plays_one_handed() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "boot_mode", "mode": 0}));
    run(&mut session, json!({"cmd": "run_until", "max_frames": 3000, "until": {"path": "mode_kernel_menu_running", "equals": 1}}));
    // A bout (entry kind 0) of the first model against the third.
    let mut writes = vec![json!({"path": "mode_arena_task", "value": 0})];
    writes.extend([0, 0, 0, 2, 2, 0].iter().enumerate().map(|(i, v)| json!({"path": format!("mode_arena_task_parameters[{i}]"), "value": v})));
    run(&mut session, json!({"cmd": "scenario", "mode": 4, "writes": writes}));
    let mut player = Player::new(session);
    for _ in 0..40 {
        if player.context() == json!({"kind": "arena", "state": 1}) { break; }
        run(&mut player.session, json!({"cmd": "frames", "count": 30}));
    }
    assert_eq!(player.context(), json!({"kind": "arena", "state": 1}));
    player.rest(4);
    let pad = |player: &mut Player| player.session.runtime.services().pads.0[0].buttons;
    use xem_core::pad::button;
    // A tap: A.
    let mut a = false;
    for hands in synthetic::gesture("tap").unwrap() {
        player.frame(hands, None);
        a |= pad(&mut player) & button::CROSS != 0;
    }
    assert!(a, "A pressed");
    // A swipe down holds Dash until the next.
    player.gesture("swipe-down", None);
    player.rest(10);
    assert_ne!(pad(&mut player) & button::CIRCLE, 0, "Dash held");
    player.gesture("swipe-down", None);
    player.rest(2);
    assert_eq!(pad(&mut player) & button::CIRCLE, 0, "Dash let go");
    // The captured stick steers toward the opponent (right).
    player.gesture("pinch", None);
    let rest = synthetic::gesture("pause").unwrap()[0];
    let right = [rest[0], rest[1].map(|h| synthetic::place(h, Quat::IDENTITY, Vec3::new(0.07, 0.0, 0.0)))];
    player.frame(right, None);
    player.frame(right, None);
    assert_ne!(pad(&mut player) & button::RIGHT, 0, "Right held");
    // Pause: the bout stands still, the stick let go when it resumes.
    player.gesture("pause", None);
    assert!(player.headset.pauses.player);
    player.gesture("pause", None);
    assert!(!player.headset.pauses.player);
}

/// A running jump on an assisted route: a swipe up while the route runs
/// jumps (the jump is the player's: the gesture's frames to the game's
/// jump are counted), the route holds its direction through it and goes
/// on to arrive after landing.
#[test]
fn a_running_jump_keeps_the_route() {
    let Some(mut player) = walking(30) else { return };
    let goal = xem_core::field::V3::new(-80.0, 0.0, -90.0);
    let aim = player.field().camera.project(goal).unwrap();
    player.gesture("pinch", Some(aim));
    player.rest(8);
    assert!(player.session.assist.is_some());
    let airborne = |player: &mut Player| player.field().player().unwrap().airborne();
    let swipe = synthetic::gesture("swipe-up").unwrap();
    let mut frames = 0;
    let mut jumped = None;
    for hands in swipe.iter().copied().chain(std::iter::repeat_n(synthetic::gesture("pause").unwrap()[0], 30)) {
        player.frame(hands, None);
        frames += 1;
        if jumped.is_none() && airborne(&mut player) {
            jumped = Some(frames);
        }
    }
    // From the thumb's touch on the finger (the swipe's second frame) to
    // the player in the air, game frames.
    let latency = jumped.expect("the player jumped") - 2;
    eprintln!("gesture to jump: {latency} frames from the thumb's touch");
    assert!(latency <= 8, "{latency} frames");
    for _ in 0..300 {
        if player.session.assist.is_none() { break; }
        player.rest(1);
    }
    assert_eq!(player.session.assist_outcome, Some(xem_core::assist::Outcome::Arrived), "the route went on after landing");
}
