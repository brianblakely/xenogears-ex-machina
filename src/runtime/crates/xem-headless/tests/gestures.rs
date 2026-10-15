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
use xem_assist::synthetic::{self, Runtime, Step};
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
    fn toggle_auto_advance(&mut self) {}
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
    /// The interface context to play in instead of the one read each frame.
    ui: Option<xem_core::context::Context>,
    /// What the runtime offers of the standard gestures.
    runtime: Runtime,
    /// The viewpoint the host's picture shows the scene from when not the
    /// game camera's own ([`Player::pulled_back`]): pointing goes through it.
    viewpoint: Option<xem_render::ps1::Viewpoint>,
}

/// Where the harness's head is (the room's floor at y 0).
const EYES: Vec3 = Vec3::new(0.0, 1.6, 0.0);

impl Player {
    fn new(session: Session<NativeModule>) -> Player {
        let screen = Screen::flat(Quad { pose: Pose { position: Vec3::new(0.0, 1.6, -2.5), orientation: Quat::IDENTITY }, size: (3.2, 2.24) }, (320.0, 224.0), 0.0);
        let mut headset = Headset::default();
        headset.source = PointerSource::Eye;
        Player { session, input: PlayerInput::default(), headset, scaling: Scaling::default(), bindings: Default::default(), screen, time: 0.0, ran: 0, ui: None, runtime: Runtime::HORIZON, viewpoint: None }
    }

    fn context(&mut self) -> Value {
        self.session.execute(&json!({"cmd": "context"}))
    }

    /// The diorama of the frame the game draws next, from a seat at the
    /// eyes, as a headset shows it: the port records the frame's camera
    /// (the diorama's view from anywhere asks it to), and the table takes
    /// that camera's point and heading at once. From then on gestures aim
    /// at its pane and through its table.
    fn diorama(&mut self) -> xem_render::ps1::DioramaSpace {
        let device = self.session.runtime.services().gpu.device_mut();
        device.set_trace(true);
        device.set_reveals(Some(xem_render::xem_gpu::Reveals { reach: [32, 32], pull_back: 0, around: xem_render::xem_gpu::Reveals::ANYWHERE }));
        // The port takes the bits with the frame after they change.
        let mut camera = None;
        for _ in 0..4 {
            run(&mut self.session, json!({"cmd": "frames", "count": 1}));
            let trace = self.session.runtime.services().gpu.device_mut().take_trace();
            camera = trace.ops.iter().rev().find_map(|op| match op {
                xem_render::xem_gpu::TraceOp::Camera(camera) => Some(*camera),
                _ => None,
            }).or(camera);
        }
        self.session.runtime.services().gpu.device_mut().set_trace(false);
        let camera = camera.expect("the scene's camera recorded");
        // A battle projects with 512 (xem_core::menu's targets), a field
        // with its camera's.
        let h = if camera.mode == xem_render::xem_gpu::CameraMode::Battle { 512.0 } else { self.field().camera.h };
        let seat = xem_render::glam::Mat4::from_translation(EYES);
        let space = xem_render::ps1::DioramaSpace::first(&camera, h, xem_render::glam::Vec2::new(320.0, 224.0), &xem_render::ps1::Table::default(), seat).expect("a camera that spans space");
        self.screen = Screen::pane(space, EYES);
        space
    }

    /// The game's next frames presented as a desktop's picture with the
    /// field's camera pulled back (wider views' zoom out at `zoom_out`
    /// percent): the viewpoint the picture shows the scene from, which the
    /// harness points through from then on, as hosts do.
    fn pulled_back(&mut self, zoom_out: u16) -> xem_render::ps1::Viewpoint {
        use xem_render::ps1::{GameView, HostClass, ViewConfig};
        let gpu = pollster::block_on(xem_render::Gpu::headless()).expect("a wgpu adapter");
        let mut view = GameView::new(&gpu.device, &gpu.queue, None);
        let graphics = xem_settings::Graphics { zoom_out, ..Default::default() };
        view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, (1280, 960), view.max_scale()));
        let _ = view.take_restart();
        self.session.runtime.services().gpu.device_mut().set_trace(true);
        for frame in 0..8 {
            let device = self.session.runtime.services().gpu.device_mut();
            device.set_reveals(view.reveals());
            run(&mut self.session, json!({"cmd": "frames", "count": 1}));
            let device = self.session.runtime.services().gpu.device_mut();
            view.push(device.take_trace());
            let display = device.display();
            let scene = self.session.scene();
            let mut encoder = gpu.device.create_command_encoder(&Default::default());
            let _ = view.present(&mut encoder, &display, &scene, f64::from(frame) / 60.0);
            gpu.queue.submit([encoder.finish()]);
        }
        self.session.runtime.services().gpu.device_mut().set_trace(false);
        let viewpoint = view.viewpoint().expect("the picture shows the field pulled back");
        self.viewpoint = Some(viewpoint);
        viewpoint
    }

    /// One headset frame of this step (its hands, and the runtime's
    /// standard signals as `self.runtime` reports them) with the pointer on
    /// `aim` (a pixel of the screen).
    fn frame(&mut self, step: Step, aim: Option<(f32, f32)>) {
        let toward = aim.map(|aim| self.screen.point(aim));
        self.frame_toward(step, toward);
    }

    /// One headset frame with the pointer toward the room point `toward`.
    fn frame_toward(&mut self, step: Step, toward: Option<Vec3>) {
        // The aim comes from the pointer source chosen: the eyes, the head
        // turned toward it, or a hand's ray (a controller's or the
        // runtime's) from beside the body.
        let eyes = EYES;
        let toward = |from: Vec3| toward.map(|target| Pose { position: from, orientation: Quat::from_rotation_arc(Vec3::NEG_Z, (target - from).normalize()) });
        let (head, gaze, rays) = match self.headset.source {
            PointerSource::Eye => (Pose { position: eyes, orientation: Quat::IDENTITY }, toward(eyes), [None, None]),
            PointerSource::Head => (toward(eyes).unwrap_or(Pose { position: eyes, orientation: Quat::IDENTITY }), None, [None, None]),
            PointerSource::LeftRay | PointerSource::RightRay => {
                let hand = Vec3::new(0.2, 1.3, -0.3);
                (Pose { position: eyes, orientation: Quat::IDENTITY }, None, [None, toward(hand)])
            }
        };
        let hands = synthetic::in_front(step.hands, head.position, Quat::IDENTITY);
        let signals = step.frame(self.time, hands, Vec3::X, self.runtime);
        let input = Input {
            time: self.time,
            head: Some(head),
            gaze,
            rays,
            hands,
            pinch: signals.pinch,
            micro: signals.micro,
            micro_capable: signals.micro_capable,
            focused: true,
        };
        self.time += 1.0 / 60.0;
        let ui = self.ui.unwrap_or_else(|| self.session.runtime.context());
        let field = (self.session.runtime.services().mode == Some(1)).then(|| xem_core::field::Field::read(self.session.runtime.memory())).flatten()
            .map(|f| xem_assist::overlay::seen(f, self.viewpoint.as_ref()));
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
            paused: self.session.runtime.game_paused(),
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
        let toward = aim.map(|aim| self.screen.point(aim));
        self.gesture_toward(name, toward);
    }

    /// A gesture's frames with the gaze toward the room point `toward`.
    fn gesture_toward(&mut self, name: &str, toward: Option<Vec3>) {
        for hands in synthetic::gesture(name).unwrap() {
            self.frame_toward(hands, toward);
        }
        let open = synthetic::gesture("pause").unwrap()[0];
        for _ in 0..4 {
            self.frame_toward(open, toward);
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

/// Whether the world point `p` stands on the diorama's table (inside its
/// volume, `margin` metres in from its sides).
fn on_table(space: &xem_render::ps1::DioramaSpace, p: xem_core::field::V3, margin: f32) -> bool {
    let at = (space.table_from_room() * space.room_from_world).transform_point3(Vec3::new(p.x, p.y, p.z));
    let (min, max) = space.volume;
    at.cmpge(min + Vec3::splat(margin)).all() && at.cmple(max - Vec3::splat(margin)).all()
}

/// Through a diorama of field 30 (its table placed from the frame's own
/// camera record, the seat at the eyes), a pinch with the gaze on a floor
/// point of the table walks there (the same `navigate` the screen's pinch
/// sends, to that world point), and one on the table's pane walks nowhere:
/// it shows no floor.
#[test]
fn through_a_diorama_a_pinch_walks_to_the_floor_gazed_at_and_the_pane_shows_none() {
    let Some(mut player) = walking(30) else { return };
    let space = player.diorama();
    let field = player.field();
    let me = *field.player().unwrap();
    // A floor point the player walks to, on the table, away from everyone.
    let mesh = &field.layers[me.layer];
    let goal = (0..mesh.triangles.len())
        .map(|t| (t, mesh.centroid(t)))
        .filter(|(t, c)| field.reach(&me, *t, *c) == xem_core::field::Reach::Walk && on_table(&space, *c, 0.05))
        .filter(|(_, c)| (60.0..220.0).contains(&c.planar(me.position)))
        .filter(|(_, c)| field.actors.iter().flatten().all(|a| a.index == me.index || a.position.planar(*c) > 40.0))
        .max_by(|a, b| a.1.planar(me.position).total_cmp(&b.1.planar(me.position)))
        .map(|(_, c)| c)
        .expect("walkable floor on the table");
    let room = space.room_from_world.transform_point3(Vec3::new(goal.x, goal.y, goal.z));
    player.gesture_toward("pinch", Some(room));
    let Some(xem_core::assist::Intent::Walk { to, .. }) = player.session.assist.as_ref().map(|a| a.intent.clone()) else {
        panic!("walking: {:?}", player.session.assist_outcome)
    };
    assert!(to.planar(goal) < 2.0, "the walk heads for the floor gazed at: {to:?} for {goal:?}");
    for _ in 0..400 {
        if player.session.assist.is_none() { break; }
        player.rest(1);
    }
    assert_eq!(player.session.assist_outcome, Some(xem_core::assist::Outcome::Arrived));
    let at = player.field().player().unwrap().position;
    assert!(at.planar(goal) < 16.0, "arrived at {at:?} for {goal:?}");
    eprintln!("through the diorama: walked {:.0} units to {goal:?} ({:.2} m from the eyes)", me.position.planar(goal), room.distance(EYES));
    // The pane shows the interface alone: a pinch there walks nowhere.
    let pane = space.pane.transform_point3(Vec3::ZERO);
    player.gesture_toward("pinch", Some(pane));
    assert!(player.session.assist.is_none(), "nothing walks to the pane: {:?}", player.session.assist.as_ref().map(|a| &a.intent));
}

/// Through a diorama of Lahan (map 5), a thumb tap with the gaze beside a
/// villager on the table approaches and talks to them, as on the screen.
#[test]
fn through_a_diorama_a_tap_by_a_villager_talks_to_them() {
    let Some(mut player) = walking(5) else { return };
    let space = player.diorama();
    let field = player.field();
    let villager = field.actors[11].unwrap();
    let middle = xem_core::field::V3::new(villager.position.x, villager.position.y - f32::from(villager.height.max(0)) / 2.0, villager.position.z);
    assert!(on_table(&space, villager.position, 0.0), "the villager stands on the table");
    let room = space.room_from_world.transform_point3(Vec3::new(middle.x, middle.y, middle.z));
    player.gesture_toward("tap", Some(room));
    assert!(matches!(player.session.assist.as_ref().map(|a| &a.intent), Some(xem_core::assist::Intent::Interact { actor: 11 })), "approaching the villager: {:?}", player.session.assist.as_ref().map(|a| &a.intent));
    for _ in 0..600 {
        if player.session.assist.is_none() { break; }
        player.rest(1);
    }
    assert_eq!(player.session.assist_outcome, Some(xem_core::assist::Outcome::Confirmed));
    player.rest(30);
    assert_eq!(player.context()["kind"], "dialogue");
}

/// With the field's camera pulled back (wider views' zoom out at 250%), a
/// pinch with the gaze where the picture shows a floor point walks there
/// and the cursor and the destination's mark lie there: the harness points
/// through the picture's viewpoint, as hosts do. The game's own camera
/// shows that point elsewhere.
#[test]
fn pulled_back_a_pinch_walks_to_the_floor_the_picture_shows_and_marks_it_there() {
    use xem_assist::overlay::{Kind, marks, seen};
    use xem_core::field::Reach;
    let Some(mut player) = walking(30) else { return };
    let viewpoint = player.pulled_back(250);
    let field = player.field();
    let shown = seen(field.clone(), Some(&viewpoint));
    let me = *field.player().unwrap();
    let mesh = &field.layers[me.layer];
    let apart = |a: (f32, f32), b: (f32, f32)| ((a.0 - b.0).powi(2) + (a.1 - b.1).powi(2)).sqrt();
    // A walkable floor point some way off and away from everyone, on the
    // picture: the one the game's camera shows farthest from there.
    let (goal, at, own) = (0..mesh.triangles.len())
        .map(|t| (t, mesh.centroid(t)))
        .filter(|(t, c)| field.reach(&me, *t, *c) == Reach::Walk && (60.0..240.0).contains(&c.planar(me.position)))
        .filter(|(_, c)| field.actors.iter().flatten().all(|a| a.index == me.index || a.position.planar(*c) > 40.0))
        .filter_map(|(_, c)| {
            let at = shown.camera.project(c).filter(|p| (16.0..304.0).contains(&p.0) && (16.0..208.0).contains(&p.1))?;
            Some((c, at, field.camera.project(c)?))
        })
        .max_by(|a, b| apart(a.1, a.2).total_cmp(&apart(b.1, b.2)))
        .expect("walkable floor on the picture");
    eprintln!("pulled back {:.0} units: the floor at {goal:?} shows at {at:?}, the game's camera puts it at {own:?}", viewpoint.offset[2]);
    assert!(apart(at, own) > 20.0, "the picture shows the point elsewhere than the game's camera: {at:?} and {own:?}");
    player.gesture("pinch", Some(at));
    let Some(xem_core::assist::Intent::Walk { to, .. }) = player.session.assist.as_ref().map(|a| a.intent.clone()) else {
        panic!("walking: {:?}", player.session.assist_outcome)
    };
    assert!(to.planar(goal) < 16.0, "the walk heads for the floor the picture shows there: {to:?} for {goal:?}");
    let marked = marks(xem_core::context::Context::Walk, Some(xem_assist::dispatch::Pointed::Screen(at)), Some(&shown), Some(to), None, false);
    let at_mark = |kind: Kind| marked.iter().find(|m| m.kind == kind).map(|m| match m.place {
        xem_assist::overlay::Place::Screen { centre, .. } => apart(centre, at),
        xem_assist::overlay::Place::Floor { .. } => f32::INFINITY,
    });
    assert!(at_mark(Kind::Floor(Reach::Walk)).is_some_and(|d| d < 0.5), "the cursor lies on walkable floor there: {marked:?}");
    assert!(at_mark(Kind::Destination).is_some_and(|d| d < 2.0), "the destination's mark lies there: {marked:?}");
    for _ in 0..400 {
        if player.session.assist.is_none() { break; }
        player.rest(1);
    }
    assert_eq!(player.session.assist_outcome, Some(xem_core::assist::Outcome::Arrived));
    let arrived = player.field().player().unwrap().position;
    assert!(arrived.planar(goal) < 16.0, "arrived at {arrived:?} for {goal:?}");
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
    // The stock list is a column of rows: a tap with the gaze on the second
    // selects it, then confirms.
    let list = player.session.runtime.targets().expect("the stock list");
    assert_eq!((list.what, list.entries.len(), list.selected), ("list", 8, Some(0)));
    player.gesture("tap", Some(list.entries[1].rect.centre()));
    let mut last = None;
    for _ in 0..60 {
        if let Some(t) = player.session.runtime.targets().filter(|t| t.what == "list") {
            last = t.selected;
        }
        player.rest(1);
    }
    assert_eq!(last, Some(1), "the second row");
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
    let keys = xem_assist::overlay::side_keys(player.session.runtime.context(), Some(&targets), false);
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

/// Through a diorama of the battle, its table placed by the battle's stage
/// as a headset places it, a tap with the gaze on a party member standing
/// on the table (Elly, at her middle) chooses that target the game's own
/// way, as the gaze on the screen does.
#[test]
fn through_a_diorama_a_tap_on_a_battle_target_chooses_it() {
    let Some(mut player) = battle(false) else { return };
    player.gesture("swipe-down", None);
    player.rest(60);
    player.gesture("tap", None);
    player.rest(40);
    assert_eq!(player.context()["targeting"], true, "{}", player.context());
    let space = player.diorama();
    let targets = player.session.runtime.targets().expect("the targets");
    let elly = targets.entries.iter().filter_map(|e| e.ground).find(|g| g.slot == 1).expect("Elly among the targets");
    let feet = xem_core::field::V3::new(elly.sprite[0] as f32, elly.sprite[1] as f32, elly.sprite[2] as f32);
    assert!(on_table(&space, feet, 0.05), "Elly stands on the table: {elly:?}");
    let middle = Vec3::new(feet.x, feet.y - f32::from(elly.height) / 2.0, feet.z);
    let room = space.room_from_world.transform_point3(middle);
    player.gesture_toward("tap", Some(room));
    for _ in 0..120 {
        if player.context()["targeting"] != true { break; }
        player.rest(1);
    }
    assert_eq!(player.context()["targeting"], false, "confirmed: {}", player.context());
    let cursor = run(&mut player.session, json!({"cmd": "inspect", "path": "battle_target_cursor_slot"}));
    assert_eq!(cursor, json!(1), "Elly");
    eprintln!("through the diorama: Elly chosen at {middle:?}, {:.2} m from the eyes", room.distance(EYES));
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
    let ahead = Step::of([rest.hands[0], rest.hands[1].map(|h| synthetic::place(h, Quat::IDENTITY, Vec3::new(0.0, 0.0, -0.07)))]);
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
    let right = Step::of([rest.hands[0], rest.hands[1].map(|h| synthetic::place(h, Quat::IDENTITY, Vec3::new(0.07, 0.0, 0.0)))]);
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

fn pad_held(player: &mut Player) -> u16 {
    let mut bytes = [0u8; 2];
    player.session.runtime.memory().read(0x8005_9570, &mut bytes).expect("pad_port0_held");
    u16::from_le_bytes(bytes)
}

/// Every field map whose scripts test the pad (84, `events.py --buttons`):
/// its event reports the map and those buttons as the game runs it, and
/// each key of its activity deck, gazed at and tapped, reaches the game as
/// exactly the button the scripts test (`pad_port0_held`, after the game's
/// own button assignment: every other map under a swapped one); where the
/// scripts read directions, the captured stick holds a diagonal.
#[test]
fn every_script_activity_deck_reaches_the_buttons_its_scripts_test() {
    let Some(disc) = disc() else { return };
    let mut checked = Vec::new();
    for (n, &(map, buttons)) in xem_core::context::script_button_maps().iter().enumerate() {
        let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
        run(&mut session, json!({"cmd": "enter_field", "map": map}));
        if n % 2 == 1 {
            // Confirm and Cancel swapped from the default.
            run(&mut session, json!({"cmd": "write_memory", "address": 0x8005_0238u64, "hex": "0001030204050607"}));
        }
        let mut event = None;
        for _ in 0..600 {
            let ui = session.runtime.context();
            if let xem_core::context::Context::Event { .. } = ui {
                event = Some(ui);
                break;
            }
            run(&mut session, json!({"cmd": "frames", "count": 1}));
        }
        let Some(ui) = event else {
            checked.push(format!("{map}: no event"));
            continue;
        };
        assert_eq!(ui, xem_core::context::Context::Event { map, buttons }, "map {map}");
        let mut player = Player::new(session);
        player.ui = Some(ui);
        let deck = xem_assist::arcade::deck(ui).expect("a deck");
        let keys = xem_assist::arcade::keys(deck);
        let quads = xem_assist::draw::side_keys(&player.screen, keys.len());
        for (i, key) in keys.iter().enumerate() {
            let local = quads[i].1.pose.position - player.screen.quad.pose.position;
            let aim = ((local.x / player.screen.quad.size.0 + 0.5) * 320.0, (0.5 - local.y / player.screen.quad.size.1) * 224.0);
            let mut seen = 0;
            for hands in synthetic::gesture("tap").unwrap() {
                player.frame(hands, Some(aim));
                seen |= pad_held(&mut player);
            }
            let open = synthetic::gesture("pause").unwrap()[0];
            for _ in 0..8 {
                player.frame(open, Some(aim));
                seen |= pad_held(&mut player);
            }
            let bit = xem_assist::arcade::activity_button(key.actions[0]).unwrap();
            assert_eq!(seen & buttons, bit, "map {map}: key {} gave {seen:#06x}", key.label);
        }
        if buttons & 0x3000 == 0x3000 {
            // Pinch to capture the stick, then the hand ahead and right.
            let pinch = synthetic::gesture("pinch").unwrap();
            for hands in &pinch {
                player.frame(*hands, None);
            }
            let open = synthetic::gesture("pause").unwrap()[0];
            let moved = Step::of(open.hands.map(|hand| hand.map(|j| j.map(|p| p + Vec3::new(0.06, 0.0, -0.06)))));
            let mut seen = 0;
            for _ in 0..8 {
                player.frame(moved, None);
                seen |= pad_held(&mut player);
            }
            assert_eq!(seen & 0xF000, 0x3000, "map {map}: the stick's diagonal");
        }
        checked.push(format!("{map}: {} keys", keys.len()));
    }
    eprintln!("{}", checked.join("\n"));
    let missing: Vec<_> = checked.iter().filter(|c| c.ends_with("no event")).collect();
    assert!(missing.is_empty(), "{missing:?}");
}

/// An approach to talk is interrupted by the menu's swipe: the route
/// stops, its Confirm never reaches the menu (it opens on its own top
/// command, nothing chosen), and closing the menu does not restart it. A
/// new pinch replaces an approach.
#[test]
fn interrupted_approaches_never_confirm_into_another_interface() {
    let Some(mut player) = walking(5) else { return };
    let field = player.field();
    let villager = field.actors[11].unwrap().position;
    let aim = field.camera.project(villager).unwrap();
    player.gesture("tap", Some(aim));
    assert!(player.session.assist.is_some(), "approaching");
    player.rest(6);
    player.gesture("swipe-down", None);
    assert!(player.session.assist.is_none());
    assert_eq!(player.session.assist_outcome, Some(xem_core::assist::Outcome::Stopped));
    player.rest(200);
    let menu = player.context();
    assert_eq!(menu["kind"], "menu", "{menu}");
    let targets = player.session.runtime.targets().expect("the commands");
    assert_eq!(targets.what, "commands", "no command was entered");
    for _ in 0..6 {
        if player.context()["kind"] == "walk" { break; }
        player.gesture("swipe-right", None);
        player.rest(60);
    }
    assert_eq!(player.context()["kind"], "walk");
    let at = player.field().player().unwrap().position;
    player.rest(120);
    assert!(player.session.assist.is_none(), "the route did not restart");
    assert_eq!(player.context()["kind"], "walk", "no talk began");
    assert!(player.field().player().unwrap().position.planar(at) < 1.0, "stood still");
    // An approach replaced by a pinch on the floor.
    let field = player.field();
    let aim = field.camera.project(villager).unwrap();
    player.gesture("tap", Some(aim));
    player.rest(4);
    let floor = field.camera.project(xem_core::field::V3::new(at.x + 60.0, at.y, at.z)).unwrap();
    player.gesture("pinch", Some(floor));
    assert!(matches!(player.session.assist.as_ref().map(|a| &a.intent), Some(xem_core::assist::Intent::Walk { .. })), "the new route");
}

/// Eye gaze lost: a pinch walks nowhere (no other target replaces it).
/// The explicit alternatives aim the same: the head, or a hand's ray.
#[test]
fn without_gaze_nothing_is_aimed_and_the_head_or_a_ray_aims_instead() {
    let Some(mut player) = walking(30) else { return };
    let goal = xem_core::field::V3::new(-80.0, 0.0, -90.0);
    player.gesture("pinch", None);
    assert!(player.session.assist.is_none(), "no gaze, no destination");
    for source in [PointerSource::Head, PointerSource::RightRay] {
        player.headset.source = source;
        // The camera follows the player: the goal as it shows now.
        let aim = player.field().camera.project(goal).unwrap();
        player.gesture("pinch", Some(aim));
        assert!(player.session.assist.is_some(), "{source:?} aims: {:?}", player.session.assist_outcome);
        player.gesture("tap", Some((160.0, 20.0)));
        player.rest(2);
        assert!(player.session.assist.is_none(), "{source:?}: a tap on the ground stops");
    }
}

/// Hands without the thumb's microgestures (a browser's WebXR) play as a
/// pointer: a pinch on the villager approaches and talks, a pinch on the
/// picture advances the dialogue (a choice under the pointer answers it),
/// the MENU key beside the screen opens the menu and BACK leaves it.
#[test]
fn hands_without_microgestures_play_as_a_pointer_with_keys() {
    let Some(mut player) = walking(5) else { return };
    player.runtime = Runtime::WEBXR;
    let key = |player: &Player, label: &str| {
        let keys = xem_assist::overlay::POINTER_KEYS;
        let i = keys.iter().position(|k| k.0 == label).unwrap();
        let quad = xem_assist::draw::pad_keys(&player.screen, keys.len())[i].1;
        let local = quad.pose.position - player.screen.quad.pose.position;
        ((local.x / player.screen.quad.size.0 + 0.5) * 320.0, (0.5 - local.y / player.screen.quad.size.1) * 224.0)
    };
    let field = player.field();
    let villager = field.actors[11].unwrap().position;
    player.gesture("pinch", Some(field.camera.project(villager).unwrap()));
    assert!(player.headset.pointer, "no microgestures: a pointer");
    for _ in 0..600 {
        if player.session.assist.is_none() { break; }
        player.rest(1);
    }
    assert_eq!(player.session.assist_outcome, Some(xem_core::assist::Outcome::Confirmed));
    player.rest(30);
    let mut pinches = 0;
    while player.context()["kind"] != "walk" {
        let aim = match player.context()["choice"].is_object() {
            true => player.session.runtime.targets().expect("the choice's lines").entries[0].rect.centre(),
            false => (160.0, 112.0),
        };
        player.gesture("pinch", Some(aim));
        player.rest(20);
        pinches += 1;
        assert!(pinches < 120, "the dialogue ends: {}", player.context());
    }
    let aim = key(&player, "MENU");
    player.gesture("pinch", Some(aim));
    player.rest(200);
    assert_eq!(player.context()["kind"], "menu", "{}", player.context());
    for _ in 0..4 {
        if player.context()["kind"] == "walk" { break; }
        let aim = key(&player, "BACK");
        player.gesture("pinch", Some(aim));
        player.rest(60);
    }
    assert_eq!(player.context()["kind"], "walk");
}
