//! First-person inspection on the original game (local only: it needs the
//! user's disc 1 and a game module build; the GPU is whatever wgpu finds,
//! lavapipe in the runtime shell). A harness plays a flat host's part: the
//! player's buttons through `xem_settings::look`, the view's state given to
//! `Session::set_inspecting` before every frame (with the view the field's
//! characters keep moving in, as the hosts give it), and the view presented
//! from the game's draw trace beside one that never looks around. In field
//! 30, turning inspection on stops the route under way and refuses new
//! ones; the character stays where it stands, and the game's camera too,
//! while the player holds a direction, jump and a camera turn; the view
//! keeps its heading in the world for 120 frames while an agent's presses
//! turn the character about and the game's camera a quarter; turning it
//! off moves no camera, shows the game's camera again, pixel for pixel,
//! and restarts no route, and a new route walks. A headset's view
//! (`GameView::present_headset`) of the same field looks through each eye
//! from the character's eye: the eyes see apart, follow the head while no
//! game frame runs (the listener staying on the view's heading), follow the
//! character walking without turning, leave the interface to its pane, and
//! change nothing in the game; through the field's menu, which draws no
//! field, they keep the room while the menu shows on the pane.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test inspection

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::path::PathBuf;
use xem_core::control::Session;
use xem_core::field::Field;
use xem_core::pad::button;
use xem_game::{NativeModule, boot};
use xem_render::Gpu;
use xem_render::ps1::{GameView, HostClass, ViewConfig};

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

/// A flat host: the view the player sees and one that only ever shows the
/// game's camera, both presented from the same draw trace.
struct Host {
    session: Session<NativeModule>,
    gpu: Gpu,
    view: GameView,
    camera: GameView,
    frames: u64,
}

impl Host {
    fn new(session: Session<NativeModule>) -> Host {
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let view = |gpu: &Gpu| {
            let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
            view.configure(ViewConfig::resolve(&Default::default(), HostClass::Desktop, (640, 448), view.max_scale()));
            view
        };
        let (view, camera) = (view(&gpu), view(&gpu));
        Host { session, gpu, view, camera, frames: 0 }
    }

    /// One frame with the player holding `held` (through the view's rules)
    /// unless an agent's `pad` stands in for it, then both views presented.
    fn frame(&mut self, held: u16, agent: Option<u16>) {
        let on = self.view.first_person();
        let field = self.session.runtime.field_controls();
        let assignment = self.session.runtime.button_assignment();
        let buttons = agent.unwrap_or_else(|| xem_settings::look::game_buttons(held, on, field, &assignment));
        self.session.set_pad(0, buttons);
        self.session.set_inspecting(on);
        self.session.set_actor_view(self.view.actor_view()).expect("the view's actor view");
        assert!(self.session.frame().expect("frame"));
        let device = self.session.runtime.services().gpu.device_mut();
        let (a, b) = (self.view.take_restart(), self.camera.take_restart());
        if a || b || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(self.view.reveals());
        let trace = device.take_trace();
        let display = device.display();
        let scene = self.session.scene();
        self.view.push(trace.clone());
        self.camera.push(trace);
        let time = self.frames as f64 / 60.0;
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        let _ = self.view.present(&mut encoder, &display, &scene, time);
        let _ = self.camera.present(&mut encoder, &display, &scene, time);
        self.gpu.queue.submit([encoder.finish()]);
        self.frames += 1;
    }

    fn field(&mut self) -> Field {
        Field::read(self.session.runtime.memory()).expect("a field")
    }

    fn player(&mut self) -> xem_core::field::Actor {
        *self.field().player().expect("a player")
    }

    /// Where the view looks in the world: its heading (radians, from the
    /// world's +z toward +x) through the axes of the frame it shows.
    fn look_heading(&mut self) -> Option<f32> {
        let look = self.view.listener_axes()?;
        let eye = self.view.frame().eye()?;
        let unit = |v: [f32; 3]| {
            let length = v.iter().map(|c| c * c).sum::<f32>().sqrt();
            v.map(|c| c / length)
        };
        // The world's axes in the game camera's view space; the look's
        // forward in it, back in the world.
        let world = eye.axes.map(unit).map(|axis| axis.iter().zip(look[2]).map(|(a, f)| a * f).sum::<f32>());
        Some(world[0].atan2(world[2]))
    }

    fn picture(&self, view: &GameView) -> Vec<u8> {
        xem_render::read_rgba8(&self.gpu.device, &self.gpu.queue, view.presented_texture().expect("a presented frame"))
    }
}

fn distance(a: xem_core::field::V3, b: xem_core::field::V3) -> f32 {
    ((a.x - b.x).powi(2) + (a.z - b.z).powi(2)).sqrt()
}

#[test]
fn inspection_stops_the_route_keeps_the_character_and_its_heading_and_restores_the_camera() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    assert_eq!(run(&mut session, json!({"cmd": "context"}))["kind"], "walk");
    let mut host = Host::new(session);
    for _ in 0..10 {
        host.frame(0, None);
    }
    let angle = host.field().camera.angle;

    // A route under way.
    run(&mut host.session, json!({"cmd": "navigate", "to": [-80.0, -90.0]}));
    let start = host.player().position;
    for _ in 0..15 {
        host.frame(0, None);
    }
    let walked = host.player().position;
    assert!(distance(start, walked) > 5.0, "the route walks: {start:?} to {walked:?}");
    assert!(host.session.assist.is_some(), "still under way");

    // Inspection on: the route stops, and no new one starts.
    host.view.set_first_person(true);
    host.frame(0, None);
    assert!(host.session.assist.is_none(), "turning inspection on stops the route");
    assert_eq!(host.session.assist_outcome, Some(xem_core::assist::Outcome::Stopped));
    assert_eq!(run(&mut host.session, json!({"cmd": "status"}))["inspecting"], true);
    for command in [json!({"cmd": "navigate", "to": [-80.0, -90.0]}), json!({"cmd": "interact", "actor": 1})] {
        let reply = host.session.execute(&command);
        assert!(reply["error"].as_str().is_some_and(|e| e.starts_with("first-person inspection")), "{command} -> {reply}");
    }

    // The player holds a direction, jump and a camera turn: the character
    // stays where it stands (once it has stopped).
    let held = button::UP | button::TRIANGLE | button::L1;
    for _ in 0..5 {
        host.frame(held, None);
    }
    let stood = host.player().position;
    let mut from_eyes = 0;
    for _ in 0..60 {
        host.frame(held, None);
        assert_eq!(host.player().position, stood, "the character stays where it stands");
        from_eyes += u32::from(host.view.frame().eye().is_some());
    }
    assert!(from_eyes > 50, "the view shows from the character's eyes ({from_eyes} of 60 frames)");
    assert_eq!(host.field().camera.angle, angle, "the game's camera stays");

    // An agent's presses (not the view's to hold back) turn the character
    // about, and the game's camera a quarter: the view keeps its heading in
    // the world.
    let heading = host.look_heading().expect("the view looks from the eyes");
    let mut facings = std::collections::BTreeSet::new();
    let mut worst: f32 = 0.0;
    for n in 0..120u32 {
        let agent = match n {
            0..3 => button::LEFT,
            30..33 => button::DOWN,
            60..80 => button::L1,
            90..93 => button::RIGHT,
            _ => 0,
        };
        host.frame(0, Some(agent));
        facings.insert(host.player().heading);
        if let Some(now) = host.look_heading() {
            let off = (now - heading + std::f32::consts::PI).rem_euclid(std::f32::consts::TAU) - std::f32::consts::PI;
            worst = worst.max(off.abs());
        }
    }
    let turned = host.field().camera.angle;
    eprintln!("the character faced {} ways, the game's camera turned from {angle} to {turned}; the view's heading moved at most {worst} radians", facings.len());
    assert!(facings.len() >= 3, "the character turned about: {facings:?}");
    assert_ne!(turned, angle, "the agent turned the game's camera");
    assert!(worst < 0.01, "the view keeps its heading in the world (off by {worst} radians)");

    // Off: no route restarts, the character stays, and the picture is the
    // game camera's again.
    host.view.set_first_person(false);
    host.frame(0, None);
    assert_eq!(run(&mut host.session, json!({"cmd": "status"}))["inspecting"], false);
    let stood = host.player().position;
    for _ in 0..30 {
        host.frame(0, None);
        assert!(host.session.assist.is_none(), "no route restarts");
    }
    assert_eq!(host.player().position, stood, "the character stays where it stood");
    assert!(host.view.listener_axes().is_none());
    assert_eq!(host.field().camera.angle, turned, "leaving the view moves no camera");
    let (seen, camera) = (host.picture(&host.view), host.picture(&host.camera));
    let different = seen.chunks_exact(4).zip(camera.chunks_exact(4)).filter(|(a, b)| a != b).count();
    assert_eq!(different, 0, "the view shows the game's camera again");
    // And it walks again.
    run(&mut host.session, json!({"cmd": "navigate", "to": [-80.0, -90.0]}));
    for _ in 0..15 {
        host.frame(0, None);
    }
    assert!(distance(stood, host.player().position) > 5.0, "a new route walks");
}

/// A headset's eyes, 32 mm either side of the head turned `yaw` radians to
/// the right and moved `x` metres, with fields of view wider away from the
/// nose (tangents left, right, up, down).
fn headset(yaw: f32, x: f32) -> [xem_render::ps1::HeadsetEye; 2] {
    let orientation = [0.0, (-yaw / 2.0).sin(), 0.0, (-yaw / 2.0).cos()];
    let (cos, sin) = (yaw.cos(), yaw.sin());
    let eye = |side: f32, fov: [f32; 4]| xem_render::ps1::HeadsetEye { orientation, position: [x + side * 0.032 * cos, 0.0, side * 0.032 * sin], fov };
    [eye(-1.0, [-1.1, 0.9, 0.9, -1.1]), eye(1.0, [-0.9, 1.1, 0.9, -1.1])]
}

#[test]
fn a_headset_looks_from_the_characters_eye_through_each_eye_paused_or_not_and_changes_nothing() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut session, json!({"cmd": "frames", "count": 400}));
    // Into the room, the view's heading across it.
    run(&mut session, json!({"cmd": "press", "buttons": ["down"], "frames": 60}));
    run(&mut session, json!({"cmd": "frames", "count": 30}));
    let start = session.snapshot_bytes().unwrap();
    // An agent's pads: still, then walking left for 20 frames, then still.
    let pad = |n: u64| if (40..60).contains(&n) { button::LEFT } else { 0 };
    for n in 0..100 {
        session.set_pad(0, pad(n));
        assert!(session.frame().unwrap());
    }
    let unseen = session.memory_hash().unwrap();

    session.restore_bytes(&start).unwrap();
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    view.configure(ViewConfig::resolve(&Default::default(), HostClass::Headset, (640, 640), view.max_scale()));
    view.set_first_person(true);
    // Presents the game's newest frame to `eyes`; the eyes' pictures and the screen's.
    let present = |session: &mut Session<NativeModule>, view: &mut GameView, eyes: &[xem_render::ps1::HeadsetEye]| -> (Vec<Vec<u8>>, Vec<u8>) {
        let device = session.runtime.services().gpu.device_mut();
        if view.take_restart() || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(view.reveals());
        view.push(device.take_trace());
        let display = device.display();
        let scene = session.scene();
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let count = view.present_headset(&mut encoder, &display, &scene, 0.0, eyes).expect("presented").eyes.len();
        gpu.queue.submit([encoder.finish()]);
        let read = |texture: &xem_render::wgpu::Texture| xem_render::read_rgba8(&gpu.device, &gpu.queue, texture);
        ((0..count).map(|i| read(view.eye_texture(i).expect("an eye"))).collect(), read(view.presented_texture().expect("the screen")))
    };
    let frame = |session: &mut Session<NativeModule>, n: u64| {
        session.set_pad(0, pad(n));
        assert!(session.frame().unwrap());
    };
    let differ = |a: &[u8], b: &[u8]| a.chunks_exact(4).zip(b.chunks_exact(4)).filter(|(p, q)| p[..3] != q[..3]).count() as f32 / (a.len() / 4) as f32;
    let colours = |picture: &[u8]| picture.chunks_exact(4).map(|p| [p[0], p[1], p[2]]).collect::<std::collections::BTreeSet<_>>().len();
    // Where the view looks in the world (as `Host::look_heading`).
    let heading = |view: &mut GameView| -> f32 {
        let look = view.listener_axes().expect("looking");
        let eye = view.frame().eye().expect("the character's record");
        let unit = |v: [f32; 3]| v.map(|c| c / v.iter().map(|c| c * c).sum::<f32>().sqrt());
        let world = eye.axes.map(unit).map(|axis| axis.iter().zip(look[2]).map(|(a, f)| a * f).sum::<f32>());
        world[0].atan2(world[2])
    };

    let level = headset(0.0, 0.0);
    let mut seen = (Vec::new(), Vec::new());
    for n in 0..40 {
        frame(&mut session, n);
        seen = present(&mut session, &mut view, &level);
    }
    let (eyes, pane) = seen;
    assert_eq!(eyes.len(), 2, "both eyes look from within the scene");
    let covered = pane.chunks_exact(4).filter(|p| p[3] > 0).count() as f32 / (pane.len() / 4) as f32;
    eprintln!("eyes: {} and {} colours, {:.3} apart; the pane covers {covered:.3}", colours(&eyes[0]), colours(&eyes[1]), differ(&eyes[0], &eyes[1]));
    if let Some(directory) = std::env::var_os("XEM_HEADSET_CAPTURE") {
        // Optional local pictures of the eyes and the pane (original game content stays local).
        let size = view.eye_texture(0).map(|t| (t.width(), t.height())).unwrap();
        let screen = view.presented_texture().map(|t| (t.width(), t.height())).unwrap();
        for (name, pixels, (width, height)) in [("left", &eyes[0], size), ("right", &eyes[1], size), ("pane", &pane, screen)] {
            std::fs::create_dir_all(&directory).unwrap();
            // The eyes' alpha is the PS1's mask bit: shown opaque; the pane's is its coverage.
            let mut pixels = pixels.clone();
            if name != "pane" {
                pixels.chunks_exact_mut(4).for_each(|p| p[3] = 255);
            }
            let file = std::fs::File::create(PathBuf::from(&directory).join(format!("{name}.png"))).unwrap();
            let mut encoder = png::Encoder::new(file, width, height);
            encoder.set_color(png::ColorType::Rgba);
            encoder.write_header().unwrap().write_image_data(&pixels).unwrap();
        }
    }
    assert!(colours(&eyes[0]) > 100 && colours(&eyes[1]) > 100, "the eyes see the room");
    assert!(differ(&eyes[0], &eyes[1]) > 0.05, "each eye sees from its own place and field of view");
    // The interface alone on the screen's pane: the compass, the rest clear.
    assert!(covered > 0.005 && covered < 0.2, "the pane holds the interface alone ({covered})");
    let facing = heading(&mut view);

    // No game frame (a pause): the head turns and moves, the eyes follow
    // it; the listener stays on the view's heading, the pane as it was.
    let listener = view.listener_axes();
    let (turned, still) = present(&mut session, &mut view, &headset(0.5, 0.1));
    assert!(differ(&turned[0], &eyes[0]) > 0.3, "turning the head turns the view ({})", differ(&turned[0], &eyes[0]));
    assert_eq!(view.listener_axes(), listener, "the listener does not follow the head");
    assert_eq!(still, pane, "the interface stays on its pane");
    let (back, _) = present(&mut session, &mut view, &level);
    assert_eq!(back, eyes, "the head back where it was, the view is too");

    // The character walks (the agent's pads, which inspection does not hold
    // back): the eyes go with it, the view keeps its heading.
    let position = |session: &mut Session<NativeModule>| Field::read(session.runtime.memory()).and_then(|field| field.player().map(|actor| actor.position));
    let stood = position(&mut session).expect("the player");
    let mut worst: f32 = 0.0;
    let mut walked = 0.0;
    for n in 40..100 {
        frame(&mut session, n);
        let (now, _) = present(&mut session, &mut view, &level);
        let off = (heading(&mut view) - facing + std::f32::consts::PI).rem_euclid(std::f32::consts::TAU) - std::f32::consts::PI;
        worst = worst.max(off.abs());
        walked = differ(&now[0], &eyes[0]);
    }
    let went = position(&mut session).expect("the player");
    eprintln!("the character walked from {stood:?} to {went:?}; the eyes' picture changed by {walked:.3}, the heading by at most {worst} radians");
    assert!(distance(stood, went) > 5.0, "the character walked");
    assert!(walked > 0.1, "the eyes moved with the character ({walked})");
    assert!(worst < 0.01, "the view keeps its heading while the character walks (off by {worst} radians)");
    assert_eq!(session.memory_hash().unwrap(), unseen, "the headset's view changes nothing in the game");

    // The field's menu (the menu button, which inspection leaves to the
    // game) draws no field: the eyes keep the room as the last frame that
    // drew it, every frame though the menu draws in both buffers by turns,
    // and the menu is on the pane.
    let mut menu = 0;
    for n in 0..240u64 {
        session.set_pad(0, if n < 2 { button::SQUARE } else { 0 });
        assert!(session.frame().unwrap());
        let (eyes, pane) = present(&mut session, &mut view, &level);
        assert_eq!(eyes.len(), 2, "frame {n} of the menu: the eyes go on");
        assert!(colours(&eyes[0]) > 100, "frame {n} of the menu: the eyes keep the room ({} colours)", colours(&eyes[0]));
        if matches!(session.runtime.context(), xem_core::context::Context::Menu { commands: true, .. }) {
            let covered = pane.chunks_exact(4).filter(|p| p[3] > 0).count() as f32 / (pane.len() / 4) as f32;
            assert!(covered > 0.3, "frame {n} of the menu: the pane shows it ({covered})");
            menu += 1;
        }
    }
    assert!(menu > 30, "{menu} frames of the menu's commands");
}
