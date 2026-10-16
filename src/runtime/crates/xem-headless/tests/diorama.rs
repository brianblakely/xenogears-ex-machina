//! The seated diorama measured on the original game (docs/runtime.md,
//! Headset qualification; local only: it needs the user's disc 1 and a game
//! module build; the GPU is whatever wgpu finds, lavapipe in the runtime
//! shell). A harness plays a headset host's part, as `xr_play` and the
//! browser's `render_xr` do: the view configured as a headset's every frame
//! with the diorama as its headset view, the session told whether
//! first-person inspection shows, the game's trace pushed and each vertical
//! blank presented to a seated head's two eyes (first-person inspection's
//! from within the scene through `present_headset`, the device's own
//! pictures through `away`).
//!
//! - Holes: seen from eight sides at two heights, a footprint on the table
//!   (within the table's volume as each eye sees it) where the eye shows
//!   the room around the table or the backdrop's wall instead of the scene.
//!   In fields 1, 12, 22 and 30 the walkmesh's (every actor layer's
//!   floors): fewer than 0.5% of its pixels (the qualification's bound, a
//!   step's criterion; the plan asks the missing surfaces measured and
//!   documented) in fields 1, 12 and 30, and of the floors the controlled
//!   character may stand on in all four; in field 22 more (measured: a
//!   strip of floor the party may not walk, beyond where the path's
//!   modelled ground ends), beyond the bound. In the debug battle and the
//!   Mountain Path's random battle their stage's ground (the models
//!   reaching across more than the table, their faces within 45 degrees of
//!   level), and on the world map at the end of Lahan's exit its terrain
//!   near its target: fewer than 0.5% in all three.
//! - Sprites and eyes: in Lahan, fields 30 and 12, the debug battle, the
//!   Mountain Path's random battle and the world map on foot (at 400%) the
//!   thickest stack of a sprite's parts stands within a millimetre of its
//!   billboard's plane; two eyes share each game frame's one build, each
//!   showing exactly what it shows presented alone; where the sprites stand
//!   apart (Lahan's townspeople seen from behind the table) each of their
//!   rows holds as many sprite pixels in either eye.
//! - The pane: field 12's dialogue, the debug battle's interface and the
//!   world map's are on the pane exactly as the flat frame draws them where
//!   they are opaque (but for 21 pixels at the world map compass cone's
//!   edge, one 15-bit step off, the cause not established), and nothing but
//!   the interface is; the interface drawn after the scene is all on the
//!   pane in its colour, and where the game draws no pictures before its
//!   scene, all of the flat frame's opaque interface.
//! - Transitions: the title movie shows on the screen in its own shape, and
//!   the title after it; a field's diorama eases in, its menu, a random
//!   battle and its end, leaving Lahan for the world map and entering a
//!   field from it, a story movie in a field and the field after it ease
//!   between the diorama and the screen (the screen's share moves by no
//!   more than the time passed over the switch's 0.3 s), the movie's frames
//!   in their own shape; first-person inspection takes the eyes over from
//!   the diorama and gives it back at once at the scale and place it had,
//!   the route it stopped not restarted; Graphics' headset view set to the
//!   screen in a session shows the screen at once, and set to the diorama
//!   again eases in from it.
//!
//! `XEM_DIORAMA_HOLES=<dir>`, `XEM_DIORAMA_EYES=<dir>` and
//! `XEM_DIORAMA_PANE=<dir>` save the pictures looked at (local review
//! pictures of original game assets: keep them local).
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test diorama

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::path::{Path, PathBuf};
use xem_core::control::Session;
use xem_core::field::Field;
use xem_game::{NativeModule, boot};
use xem_render::Gpu;
use xem_render::glam::{Mat4, Vec2, Vec3};
use xem_render::ps1::{AUX_INTERFACE, AUX_MODEL, AUX_SPRITE, AUX_WORLD, DioramaEye, DioramaSpace, GameView, HeadsetEye, HostClass, Table, ViewConfig, XrView, effective_class, scene_offset};
use xem_render::xem_gpu::{Camera, CameraMode, Class, Shape, Trace, TraceOp};
use xem_settings::Graphics;

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

/// The picture a headset's view is configured for (half an eye's).
const OUTPUT: (u32, u32) = (640, 480);
/// The screen's share moves by at most this much a second (its switch takes
/// 0.3 s: ps1/diorama.rs `SWITCH`).
const SWITCH_RATE: f32 = 1.0 / 0.3;
/// Small eyes for runs that look at what is shown rather than at pixels.
const SMALL: (u32, u32) = (96, 72);

/// The default table's centre in the room, the seat at the origin.
fn centre() -> Vec3 {
    Table::default().room_from_table(Mat4::IDENTITY).transform_point3(Vec3::ZERO)
}

/// A head's two eyes at `at`, 64 mm apart, looking along the way from there
/// to the table's centre (parallel, as a headset's are), `fov` degrees high,
/// of `size` pixels.
fn head(at: Vec3, size: (u32, u32), fov: f32) -> [DioramaEye; 2] {
    let forward = (centre() - at).normalize();
    let right = forward.cross(Vec3::Y).normalize();
    [-0.032f32, 0.032].map(|x| DioramaEye {
        view: Mat4::look_to_rh(at + right * x, forward, Vec3::Y),
        proj: Mat4::perspective_rh(fov.to_radians(), size.0 as f32 / size.1 as f32, 0.05, 20.0),
        size,
    })
}

/// A seated head's two eyes (at the seat, the reference space's origin).
fn seated(size: (u32, u32), fov: f32) -> [DioramaEye; 2] {
    head(Vec3::ZERO, size, fov)
}

/// An eye `distance` metres from the table's centre, `yaw` degrees round it
/// from the seat's side (positive to the right) and `pitch` up, looking at
/// the centre with a field of view 60 degrees high, of `size` pixels.
fn review(yaw: f32, pitch: f32, distance: f32, size: (u32, u32)) -> DioramaEye {
    let (yaw, pitch) = (yaw.to_radians(), pitch.to_radians());
    let at = centre() + Vec3::new(yaw.sin() * pitch.cos(), pitch.sin(), yaw.cos() * pitch.cos()) * distance;
    DioramaEye { view: Mat4::look_at_rh(at, centre(), Vec3::Y), proj: Mat4::perspective_rh(60f32.to_radians(), size.0 as f32 / size.1 as f32, 0.01, 20.0), size }
}

/// What a presentation showed.
#[derive(Clone, Debug)]
struct Shown {
    /// Vertical blanks run when it presented, and the session's scene.
    blank: u64,
    scene: String,
    /// The screen's share of the view (1: the screen alone).
    screen: f32,
    /// The diorama's eyes drawn, at this opacity, and where it lay.
    diorama: bool,
    opacity: f32,
    space: Option<DioramaSpace>,
    /// A new game frame came with it.
    new_frame: bool,
    /// The flat frame for the screen (its size) and whether it is a movie
    /// frame rebuilt from what the MDEC decoded.
    flat: Option<(u32, u32)>,
    movie: bool,
    /// The display's pixels.
    display: (u32, u32),
    /// The device's own picture (`away`), and first-person inspection's eyes
    /// from within the scene.
    device: bool,
    within: bool,
}

impl Shown {
    fn mode(&self) -> Option<CameraMode> {
        self.space.map(|space| space.camera.mode)
    }
}

/// Each eye's diorama picture (premultiplied, sRGB-encoded) and its classes
/// (`AUX_*` in blue), and the pane's picture.
struct Pictures {
    eyes: Vec<(Vec<u8>, Vec<u8>)>,
    pane: Option<Vec<u8>>,
}

/// A headset host on the game, and beside it a view showing the screen from
/// the same trace and one showing it from the trace without what the game
/// draws before its scene (for the pane's checks).
struct Host {
    session: Session<NativeModule>,
    gpu: Gpu,
    view: GameView,
    screen: Option<GameView>,
    after_scene: Option<GameView>,
    graphics: Graphics,
    table: Table,
    /// Graphics' headset view the screen (else the diorama on `table`).
    screen_view: bool,
    /// Vertical blanks run (the presentations' clock: 60 a second).
    blanks: u64,
}

impl Host {
    fn new(disc: &Path) -> Host {
        let (session, _) = boot(disc, &Default::default()).expect("boot");
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
        Host { session, gpu, view, screen: None, after_scene: None, graphics: Graphics::default(), table: Table::default(), screen_view: false, blanks: 0 }
    }

    /// Adds the views beside the headset's for the pane's checks.
    fn beside(&mut self) {
        for side in [&mut self.screen, &mut self.after_scene] {
            *side = Some(GameView::new(&self.gpu.device, &self.gpu.queue, Some(&self.gpu.adapter)));
        }
    }

    /// Runs one vertical blank and presents it to `eyes`.
    fn step(&mut self, eyes: &[DioramaEye]) -> Shown {
        let config = ViewConfig::resolve(&self.graphics, HostClass::Headset, OUTPUT, self.view.max_scale());
        self.view.configure(config);
        self.view.set_xr_view(if self.screen_view { XrView::Screen } else { XrView::Diorama(self.table) });
        self.session.set_inspecting(self.view.first_person());
        assert!(self.session.frame().expect("frame"), "a native disc never blocks");
        self.blanks += 1;
        let device = self.session.runtime.services().gpu.device_mut();
        let mut restart = self.view.take_restart();
        for side in [&mut self.screen, &mut self.after_scene].into_iter().flatten() {
            side.configure(config);
            restart |= side.take_restart();
        }
        if restart || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(self.view.reveals());
        let trace = device.take_trace();
        if let Some(screen) = &mut self.screen {
            screen.push(trace.clone());
        }
        if let Some(after) = &mut self.after_scene {
            after.push(after_scene(&trace));
        }
        self.view.push(trace);
        self.present(eyes, false).0
    }

    /// `count` vertical blanks, each presented to `eyes`.
    fn steps(&mut self, count: u64, eyes: &[DioramaEye]) -> Vec<Shown> {
        (0..count).map(|_| self.step(eyes)).collect()
    }

    /// Presents the frame shown to `eyes` (as each headset frame does, the
    /// game running or not); with `read`, the pictures it drew too.
    fn present(&mut self, eyes: &[DioramaEye], read: bool) -> (Shown, Pictures) {
        let display = self.session.runtime.services().gpu.device().display();
        let scene = self.session.scene();
        let now = self.blanks as f64 / 60.0;
        let mut shown = Shown {
            blank: self.blanks,
            scene: scene.clone(),
            screen: 1.0,
            diorama: false,
            opacity: 0.0,
            space: None,
            new_frame: false,
            flat: None,
            movie: false,
            display: (display.width, display.height),
            device: false,
            within: false,
        };
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        let mut textures = (Vec::new(), None);
        if !(GameView::shows(&display) || self.view.shows_movie(&display)) {
            // The device's own picture (a 24-bit movie frame the view does
            // not rebuild): no diorama lies anywhere meanwhile.
            self.view.away();
            shown.device = true;
        } else if self.view.first_person() || self.view.xr_view() == XrView::Screen {
            // Inspection, or the screen as the headset view: the screen (its
            // interface alone while inspection looks from within).
            let heads = [-0.032f32, 0.032].map(|x| HeadsetEye { orientation: [0.0, 0.0, 0.0, 1.0], position: [x, 0.0, 0.0], fov: [-1.0, 1.0, 1.0, -1.0] });
            if let Some(presented) = self.view.present_headset(&mut encoder, &display, &scene, now, &heads) {
                shown.within = !presented.eyes.is_empty();
                shown.new_frame = presented.screen.new_frame;
                shown.flat = Some(presented.screen.size);
                shown.movie = presented.screen.movie;
            }
        } else if let Some(presented) = self.view.present_diorama(&mut encoder, &display, &scene, now, eyes, Mat4::IDENTITY) {
            shown.screen = presented.screen;
            shown.diorama = !presented.eyes.is_empty();
            shown.opacity = presented.opacity;
            shown.space = presented.space;
            shown.new_frame = presented.new_frame;
            shown.flat = presented.frame.as_ref().map(|frame| frame.size);
            shown.movie = presented.frame.as_ref().is_some_and(|frame| frame.movie);
            if read {
                textures = (presented.eyes.iter().map(|eye| eye.texture().clone()).collect(), presented.pane.map(|pane| pane.texture().clone()));
            }
        }
        for side in [&mut self.screen, &mut self.after_scene].into_iter().flatten() {
            let _ = side.present(&mut encoder, &display, &scene, now);
        }
        self.gpu.queue.submit([encoder.finish()]);
        let read_texture = |texture: &xem_render::wgpu::Texture| xem_render::read_rgba8(&self.gpu.device, &self.gpu.queue, texture);
        let eyes = textures.0.iter().enumerate()
            .map(|(eye, picture)| (read_texture(picture), read_texture(self.view.diorama_aux(eye).expect("the diorama's classes"))))
            .collect();
        let pane = textures.1.as_ref().map(read_texture);
        (shown, Pictures { eyes, pane })
    }

    fn mode(&mut self) -> u64 {
        run(&mut self.session, json!({"cmd": "readiness"}))["mode"].as_u64().expect("a mode")
    }

    fn context(&mut self) -> String {
        run(&mut self.session, json!({"cmd": "context"}))["kind"].as_str().unwrap_or_default().to_string()
    }

    /// Field 15, the Mountain Path, from its first entrance: walked into
    /// once the player may (its scripts' dialogue confirmed), Up held until
    /// its random battle comes, then let go. The frames shown to `eyes` and
    /// the blank the battle came at.
    fn mountain_path_battle(&mut self, eyes: &[DioramaEye]) -> (Vec<Shown>, u64) {
        run(&mut self.session, json!({"cmd": "enter_field", "map": 15, "entry": 0}));
        let mut shown = Vec::new();
        for _ in 0..300 {
            shown.extend(self.steps(10, eyes));
            match self.context().as_str() {
                "walk" => break,
                "dialogue" => {
                    run(&mut self.session, json!({"cmd": "act", "actions": ["Confirm"], "frames": 2}));
                }
                _ => {}
            }
        }
        shown.extend(self.steps(60, eyes));
        run(&mut self.session, json!({"cmd": "hold", "actions": ["Up"]}));
        let mut came = None;
        for _ in 0..400 {
            shown.extend(self.steps(10, eyes));
            if self.mode() == 2 {
                came = Some(self.blanks);
                break;
            }
        }
        let came = came.expect("the random battle comes");
        run(&mut self.session, json!({"cmd": "hold", "actions": []}));
        (shown, came)
    }

    /// Lahan left for the world map on foot along
    /// tests/reference-inputs/world-map-lahan-exit.json, presented to `eyes`
    /// until 300 game frames past the route's end: the frames shown.
    fn lahan_exit(&mut self, eyes: &[DioramaEye]) -> Vec<Shown> {
        let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../tests/reference-inputs/world-map-lahan-exit.json");
        let route: Value = serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();
        run(&mut self.session, json!({"cmd": "schedule", "commands": route["commands"]}));
        let end = route["frames"].as_u64().unwrap();
        let mut shown = Vec::new();
        while self.session.runtime.frames() < end + 300 {
            shown.extend(self.steps(10, eyes));
        }
        shown
    }
}

/// The debug battle on foot from boot: Start on the battle selector's
/// defaults (as the browser's and Monado's battle runs start it), then 500
/// frames into it.
fn debug_battle() -> Vec<Value> {
    vec![
        json!({"cmd": "frames", "count": 60}),
        json!({"cmd": "write", "path": "mode_battle_standalone", "value": 1}),
        json!({"cmd": "boot_mode", "mode": 2}),
        json!({"cmd": "frames", "count": 900}),
        json!({"cmd": "press", "buttons": ["start"], "frames": 1}),
        json!({"cmd": "frames", "count": 500}),
    ]
}

/// A scene the diorama shows, reached from boot.
#[derive(Clone, Copy, Debug, PartialEq)]
enum Scene {
    /// A field entered, 400 frames on (field 30 with Fei walked into the
    /// room, as the headset runs do).
    Field(u64),
    /// The debug battle on foot ([`debug_battle`]).
    DebugBattle,
    /// The Mountain Path's random battle ([`Host::mountain_path_battle`]).
    MountainPath,
    /// The world map on foot at the end of Lahan's exit route
    /// ([`Host::lahan_exit`]).
    WorldMap,
}

impl Scene {
    fn name(self) -> String {
        match self {
            Scene::Field(1) => "Lahan".into(),
            Scene::Field(map) => format!("field {map}"),
            Scene::DebugBattle => "the debug battle".into(),
            Scene::MountainPath => "the Mountain Path's random battle".into(),
            Scene::WorldMap => "the world map".into(),
        }
    }

    /// Reaches the scene in `host` (presenting to small seated eyes where
    /// it plays on with frames shown): a battle until its diorama shows
    /// alone with its turns running (its opening over, its combatants
    /// standing where the table stands).
    fn reach(self, host: &mut Host) {
        let eyes = seated(SMALL, 90.0);
        match self {
            Scene::Field(map) => {
                run(&mut host.session, json!({"cmd": "enter_field", "map": map}));
                run(&mut host.session, json!({"cmd": "frames", "count": 400}));
                if map == 30 {
                    run(&mut host.session, json!({"cmd": "press", "buttons": ["down"], "frames": 60}));
                    run(&mut host.session, json!({"cmd": "frames", "count": 30}));
                }
            }
            Scene::DebugBattle => {
                for command in debug_battle() {
                    run(&mut host.session, command);
                }
            }
            Scene::MountainPath => {
                host.mountain_path_battle(&eyes);
            }
            Scene::WorldMap => {
                host.lahan_exit(&eyes);
            }
        }
        if matches!(self, Scene::DebugBattle | Scene::MountainPath) {
            let turns = |s: &Shown| s.diorama && s.screen == 0.0 && s.space.and_then(|space| space.camera.stage).is_some_and(|stage| stage.turns);
            let mut shown = Vec::new();
            while !shown.last().is_some_and(turns) {
                assert!(shown.len() < 3000, "{}: the battle's diorama with its turns running: {}", self.name(), timeline(&shown));
                shown.push(host.step(&eyes));
            }
        }
    }
}

/// `trace` without the pictures a frame draws before its scene and those it
/// projects behind it (the diorama's wall: the backdrop), where the trace
/// holds a scene (each frame's commands come within one vertical blank): its
/// flat frame shows the interface drawn after the scene over the scene and
/// the game's clear.
fn after_scene(trace: &Trace) -> Trace {
    let first = trace.ops.iter().position(|op| matches!(op, TraceOp::Draw(p) if p.class.is_scene() && !p.backdrop));
    let ops = trace.ops.iter().enumerate()
        .filter(|(i, op)| !matches!(op, TraceOp::Draw(p) if p.backdrop || (!p.class.is_scene() && first.is_some_and(|first| *i < first))))
        .map(|(_, op)| op.clone())
        .collect();
    Trace { ops, focus: trace.focus }
}

/// `polygon` (in the table's space) cut by the plane `axis` = `bound`,
/// keeping its part at most `bound` (`below`) or at least `bound`.
fn clip(polygon: &[Vec3], axis: usize, bound: f32, below: bool) -> Vec<Vec3> {
    let inside = |p: &Vec3| if below { p[axis] <= bound } else { p[axis] >= bound };
    let mut out = Vec::new();
    for (i, &a) in polygon.iter().enumerate() {
        let b = polygon[(i + 1) % polygon.len()];
        if inside(&a) {
            out.push(a);
        }
        if inside(&a) != inside(&b) {
            let t = (bound - a[axis]) / (b[axis] - a[axis]);
            out.push(a + (b - a) * t);
        }
    }
    out
}

/// Sets `mask`'s pixels (`size`, top row first) whose centres lie in convex
/// `polygon` (pixel coordinates).
fn fill(mask: &mut [bool], (width, height): (usize, usize), polygon: &[Vec2]) {
    let n = polygon.len();
    let area: f32 = (0..n).map(|i| polygon[i].perp_dot(polygon[(i + 1) % n])).sum();
    if area.abs() < 1e-6 {
        return;
    }
    let (lo, hi) = polygon.iter().fold((Vec2::MAX, Vec2::MIN), |(lo, hi), p| (lo.min(*p), hi.max(*p)));
    let (x0, x1) = (lo.x.floor().max(0.0) as usize, (hi.x.ceil().max(0.0) as usize).min(width));
    let (y0, y1) = (lo.y.floor().max(0.0) as usize, (hi.y.ceil().max(0.0) as usize).min(height));
    for y in y0..y1 {
        for x in x0..x1 {
            let p = Vec2::new(x as f32 + 0.5, y as f32 + 0.5);
            if (0..n).all(|i| (polygon[(i + 1) % n] - polygon[i]).perp_dot(p - polygon[i]) * area.signum() >= 0.0) {
                mask[y * width + x] = true;
            }
        }
    }
}

/// Whether triangle `t` of walkmesh layer `layer` is a floor: its attribute
/// word lacks bit 0x800000, which the game's floor search
/// (field_collision_find_actor_floor, field_motion.c) takes as no floor on
/// that layer (a wall on layer 0; above it a pit, through which an actor
/// falls to the floor below).
fn floor(field: &Field, layer: usize, t: usize) -> bool {
    let index = field.layers[layer].triangles[t].attribute as usize;
    field.attributes.get(index).copied().unwrap_or(0) & 0x80_0000 == 0
}

/// Polygons in the table's space.
type Polygons = Vec<Vec<Vec3>>;

/// `triangles` of walkmesh layer `layer` in the table's space of `space`.
fn in_table(field: &Field, layer: usize, triangles: &[usize], space: &DioramaSpace) -> Polygons {
    let table_from_world = space.table_from_room() * space.room_from_world;
    let layer = &field.layers[layer];
    triangles.iter().map(|&t| layer.corners(t).iter().map(|v| table_from_world.transform_point3(Vec3::new(v.x, v.y, v.z))).collect()).collect()
}

/// The ground the frame shown drew (its `ops`, through the `camera` it was
/// drawn with) in the table's space of `space`, as the diorama places it
/// (each face at its points before the division), each face a triangle
/// within 45 degrees of level: in a battle its stage's, drawn as models
/// (those whose faces reach across more than the table's width: the floor
/// and the ground around it; a combatant's parts are smaller, its sprites
/// are no model); on the world map its terrain's, the world's opaque
/// triangles (its billboards, clouds, shadows and the sky's bands are
/// quads). Also how many faces of models or of the terrain the frame drew,
/// how many of them had every point before the division and how many of
/// those are the stage's or the terrain's.
fn ground(ops: &[TraceOp], camera: &Camera, space: &DioramaSpace) -> (Polygons, (usize, usize, usize)) {
    let table_from_world = space.table_from_room() * space.room_from_world;
    let scene = scene_offset(ops);
    let (mut faces, mut placed) = (0, 0);
    let mut owned: Vec<(u32, Vec<Vec3>)> = Vec::new();
    for op in ops {
        let TraceOp::Draw(p) = op else { continue };
        let class = effective_class(p, scene);
        let kept = !p.backdrop && match camera.mode {
            CameraMode::Battle => class == Class::Model && matches!(p.shape, Shape::Triangle | Shape::Quad),
            CameraMode::Worldmap => class == Class::World && p.shape == Shape::Triangle && !p.semi,
            CameraMode::Field => false,
        };
        if !kept {
            continue;
        }
        faces += 1;
        let Some(points) = p.vertices[..p.vertex_count()].iter().map(|v| v.view.and_then(|view| camera.world_point(view))).collect::<Option<Vec<_>>>() else { continue };
        placed += 1;
        owned.push((p.owner, points.into_iter().map(|p| table_from_world.transform_point3(Vec3::from(p))).collect()));
    }
    // A battle's stage: the models reaching across more than the table.
    let width = Table::VOLUME.1.x - Table::VOLUME.0.x;
    let mut reach: std::collections::HashMap<u32, (Vec2, Vec2)> = std::collections::HashMap::new();
    for (owner, points) in &owned {
        let span = reach.entry(*owner).or_insert((Vec2::MAX, Vec2::MIN));
        for p in points {
            let across = Vec2::new(p.x, p.z);
            *span = (span.0.min(across), span.1.max(across));
        }
    }
    let stage = |owner: &u32| camera.mode != CameraMode::Battle || reach[owner].1.cmpgt(reach[owner].0 + width).any();
    let mut ground = Vec::new();
    for (_, points) in owned.iter().filter(|(owner, _)| stage(owner)) {
        let triangles: &[[usize; 3]] = if points.len() == 4 { &[[0, 1, 2], [1, 3, 2]] } else { &[[0, 1, 2]] };
        for t in triangles {
            let [a, b, c] = t.map(|i| points[i]);
            if (b - a).cross(c - a).normalize_or_zero().y.abs() >= std::f32::consts::FRAC_1_SQRT_2 {
                ground.push(vec![a, b, c]);
            }
        }
    }
    let stage_faces = owned.iter().filter(|(owner, _)| stage(owner)).count();
    (ground, (faces, placed, stage_faces))
}

/// `polygons` (in the table's space of `space`) in `eye`'s picture, within
/// the table's volume as the eye sees it, a pixel in from its edges (each
/// pixel counted with its four neighbours in it).
fn footprint(polygons: &[Vec<Vec3>], space: &DioramaSpace, eye: &DioramaEye) -> Vec<bool> {
    let (width, height) = (eye.size.0 as usize, eye.size.1 as usize);
    let clip_from_table = eye.proj * eye.view * space.room_from_table;
    let (lo, hi) = Table::VOLUME;
    let mut mask = vec![false; width * height];
    for polygon in polygons {
        let mut polygon = polygon.clone();
        for axis in 0..3 {
            polygon = clip(&polygon, axis, lo[axis], false);
            polygon = clip(&polygon, axis, hi[axis], true);
        }
        if polygon.len() < 3 {
            continue;
        }
        let pixels: Vec<Vec2> = polygon.iter().map(|p| {
            let c = clip_from_table * p.extend(1.0);
            Vec2::new((c.x / c.w + 1.0) / 2.0 * width as f32, (1.0 - c.y / c.w) / 2.0 * height as f32)
        }).collect();
        fill(&mut mask, (width, height), &pixels);
    }
    let at = |x: usize, y: usize| mask[y * width + x];
    (0..width * height).map(|i| {
        let (x, y) = (i % width, i / width);
        x > 0 && y > 0 && x + 1 < width && y + 1 < height && at(x, y) && at(x - 1, y) && at(x + 1, y) && at(x, y - 1) && at(x, y + 1)
    }).collect()
}

/// Whether a class (`AUX_*`) is the scene's.
fn scene_class(class: u8) -> bool {
    [AUX_WORLD, AUX_MODEL, AUX_SPRITE].contains(&u32::from(class))
}

#[test]
fn the_scene_covers_the_walkmesh_seen_from_eight_sides_at_two_heights() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    const SIZE: (u32, u32) = (640, 480);
    let mut results = Vec::new();
    for map in [1u64, 12, 22, 30] {
        let mut host = Host::new(&disc);
        run(&mut host.session, json!({"cmd": "enter_field", "map": map}));
        run(&mut host.session, json!({"cmd": "frames", "count": 400}));
        // Eased into the diorama: the trace asks the port for every face and
        // the camera from the frame after the view changes.
        let seat = seated(SMALL, 90.0);
        let shown = host.steps(40, &seat);
        let last = shown.last().expect("presented");
        assert!(last.diorama && last.screen == 0.0, "map {map}: the diorama alone shows ({}, {})", last.diorama, last.screen);
        let space = last.space.expect("the diorama's space");
        let field = Field::read(host.session.runtime.memory()).expect("a field");
        let player = *field.player().expect("the controlled character");
        // The walkmesh's floors on every actor layer: all but the top one, the
        // camera's floor (field_camera_walk_top_layer walks layer_count - 1;
        // actors' floor searches take the layers below it); and of them those
        // the controlled character may stand on.
        let layers: Vec<(Vec<usize>, Vec<usize>)> = (0..field.layers.len() - 1).map(|layer| {
            let floors: Vec<usize> = (0..field.layers[layer].triangles.len()).filter(|&t| floor(&field, layer, t)).collect();
            let walkable = floors.iter().copied().filter(|&t| !field.blocked(layer, t, &player)).collect();
            (floors, walkable)
        }).collect();
        let actors: Vec<Vec<usize>> = (0..layers.len())
            .map(|layer| field.actors.iter().flatten().filter(|a| a.layer == layer && a.flags & 1 == 0).map(|a| a.index).collect())
            .collect();
        let pixels = (SIZE.0 * SIZE.1) as usize;
        let (mut walkmesh, mut walkable) = (Holes::default(), Holes::default());
        let mut by_layer: Vec<Holes> = layers.iter().map(|_| Holes::default()).collect();
        let polygons: Vec<(Polygons, Polygons)> = layers.iter().enumerate()
            .map(|(layer, (all, standable))| (in_table(&field, layer, all, &space), in_table(&field, layer, standable, &space)))
            .collect();
        for pitch in [30.0f32, 60.0] {
            for yaw in (0..8).map(|k| k as f32 * 45.0) {
                let eye = review(yaw, pitch, 1.1, SIZE);
                let (shown, pictures) = host.present(&[eye], true);
                assert!(shown.diorama, "map {map}: the diorama from {yaw}, {pitch}");
                let classes = &pictures.eyes[0].1;
                let view = format!("{yaw:.0}/{pitch:.0}");
                let (mut floors, mut walks) = (vec![false; pixels], vec![false; pixels]);
                for (layer, (all, standable)) in polygons.iter().enumerate() {
                    let mask = footprint(all, &space, &eye);
                    by_layer[layer].add(&view, &mask, classes);
                    floors.iter_mut().zip(&mask).for_each(|(a, &m)| *a |= m);
                    let mask = footprint(standable, &space, &eye);
                    walks.iter_mut().zip(&mask).for_each(|(a, &m)| *a |= m);
                }
                walkmesh.add(&view, &floors, classes);
                walkable.add(&view, &walks, classes);
                if let Some(directory) = std::env::var_os("XEM_DIORAMA_HOLES") {
                    // Local review pictures: the eye, the floors the controlled
                    // character may stand on in green (their holes red), the
                    // walkmesh's other floors in blue (their holes magenta).
                    let mut picture = pictures.eyes[0].0.clone();
                    for i in (0..pixels).filter(|&i| floors[i]) {
                        let hole = !scene_class(classes[i * 4 + 2]);
                        let p = &mut picture[i * 4..i * 4 + 4];
                        match (hole, walks[i]) {
                            (true, true) => p.copy_from_slice(&[255, 0, 0, 255]),
                            (true, false) => p.copy_from_slice(&[255, 0, 255, 255]),
                            (false, true) => (p[1], p[3]) = (p[1].saturating_add(40), 255),
                            (false, false) => (p[2], p[3]) = (p[2].saturating_add(80), 255),
                        }
                    }
                    let path = PathBuf::from(&directory).join(format!("holes-{map}-{yaw:.0}-{pitch:.0}.png"));
                    let mut encoder = png::Encoder::new(std::fs::File::create(path).unwrap(), SIZE.0, SIZE.1);
                    encoder.set_color(png::ColorType::Rgba);
                    encoder.write_header().unwrap().write_image_data(&picture).unwrap();
                }
            }
        }
        let layer_lines: Vec<String> = by_layer.iter().enumerate().map(|(layer, holes)| {
            format!("layer {layer} ({} of {} triangles floors, {} of them the character's; actors {:?}): {}",
                layers[layer].0.len(), field.layers[layer].triangles.len(), layers[layer].1.len(), actors[layer], holes.summary())
        }).collect();
        eprintln!("map {map} (the controlled character on layer {}, {} layers): the walkmesh's floors {}; the floors the character may stand on {}; by layer: {}; by view (yaw/pitch), the floors: {}",
            player.layer, field.layers.len(), walkmesh.summary(), walkable.summary(), layer_lines.join("; "), walkmesh.views.join(", "));
        results.push((map, walkmesh, walkable));
    }
    for (map, walkmesh, walkable) in results {
        assert!(walkmesh.least >= 1000, "map {map}: the floor on the table from every side ({} pixels at least): {}", walkmesh.least, walkmesh.views.join(", "));
        assert!(walkable.missing * 200 < walkable.total, "map {map}: {} of the floors the character may stand on show no scene: {}", walkable.summary(), walkable.views.join(", "));
        // The whole walkmesh: under 0.5% but in field 22, where a strip of
        // floor the party may not walk, beyond where the path's modelled
        // ground ends, shows the room from the table's right (measured, not
        // met: docs/runtime.md, Diorama). Should that change, so must they.
        assert_eq!(walkmesh.missing * 200 < walkmesh.total, map != 22, "map {map}: {} of the walkmesh's floors show no scene: {}", walkmesh.summary(), walkmesh.views.join(", "));
    }
}

#[test]
fn the_scene_covers_a_battles_ground_and_the_world_maps_terrain_seen_from_eight_sides_at_two_heights() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    const SIZE: (u32, u32) = (640, 480);
    let mut results = Vec::new();
    for scene in [Scene::DebugBattle, Scene::MountainPath, Scene::WorldMap] {
        let name = scene.name();
        let mut host = Host::new(&disc);
        scene.reach(&mut host);
        let seat = seated(SMALL, 90.0);
        let shown = host.steps(40, &seat);
        let last = shown.last().expect("presented");
        assert!(last.diorama && last.screen == 0.0, "{name}: the diorama alone shows ({}, {})", last.diorama, last.screen);
        let space = last.space.expect("the diorama's space");
        // The ground of the frame the diorama shows, as the game drew it.
        let frame = host.view.frame();
        let camera = frame.camera().expect("the frame's camera");
        assert_eq!(camera, space.camera, "{name}: the frame shown is the one the diorama stands on");
        let (polygons, (faces, placed, stage)) = ground(frame.newest(), &camera, &space);
        let pixels = (SIZE.0 * SIZE.1) as usize;
        let mut holes = Holes::default();
        for pitch in [30.0f32, 60.0] {
            for yaw in (0..8).map(|k| k as f32 * 45.0) {
                let eye = review(yaw, pitch, 1.1, SIZE);
                let (shown, pictures) = host.present(&[eye], true);
                assert!(shown.diorama, "{name}: the diorama from {yaw}, {pitch}");
                let classes = &pictures.eyes[0].1;
                let mask = footprint(&polygons, &space, &eye);
                holes.add(&format!("{yaw:.0}/{pitch:.0}"), &mask, classes);
                if let Some(directory) = std::env::var_os("XEM_DIORAMA_HOLES") {
                    // Local review pictures: the eye, the ground in green
                    // (its holes red).
                    let mut picture = pictures.eyes[0].0.clone();
                    for i in (0..pixels).filter(|&i| mask[i]) {
                        let p = &mut picture[i * 4..i * 4 + 4];
                        if scene_class(classes[i * 4 + 2]) {
                            (p[1], p[3]) = (p[1].saturating_add(40), 255);
                        } else {
                            p.copy_from_slice(&[255, 0, 0, 255]);
                        }
                    }
                    let path = PathBuf::from(&directory).join(format!("holes-{}-{yaw:.0}-{pitch:.0}.png", name.replace([' ', '\''], "-")));
                    let mut encoder = png::Encoder::new(std::fs::File::create(path).unwrap(), SIZE.0, SIZE.1);
                    encoder.set_color(png::ColorType::Rgba);
                    encoder.write_header().unwrap().write_image_data(&picture).unwrap();
                }
            }
        }
        eprintln!("{name} ({faces} faces of models or the terrain, {placed} with every point before the division, {stage} of them the stage's or the terrain's; \
            {} ground triangles; {} m a unit): the ground {}; by view (yaw/pitch): {}", polygons.len(), space.scale, holes.summary(), holes.views.join(", "));
        results.push((name, holes));
    }
    for (name, holes) in results {
        assert!(holes.least >= 1000, "{name}: the ground on the table from every side ({} pixels at least): {}", holes.least, holes.views.join(", "));
        // Under 0.5% in all three (measured: docs/runtime.md, Diorama).
        assert!(holes.missing * 200 < holes.total, "{name}: {} of the ground shows no scene: {}", holes.summary(), holes.views.join(", "));
    }
}

/// The pixels of a footprint over the views that show no scene.
#[derive(Default)]
struct Holes {
    /// The footprint's pixels and those without the scene, over the views.
    total: usize,
    missing: usize,
    /// The fewest footprint pixels in a view.
    least: usize,
    views: Vec<String>,
}

impl Holes {
    /// Adds a view's footprint `mask` over its eye's `classes`.
    fn add(&mut self, view: &str, mask: &[bool], classes: &[u8]) {
        let covered = mask.iter().filter(|&&m| m).count();
        let holes = mask.iter().enumerate().filter(|&(i, &m)| m && !scene_class(classes[i * 4 + 2])).count();
        self.least = if self.views.is_empty() { covered } else { self.least.min(covered) };
        self.views.push(format!("{view}: {holes} of {covered}"));
        self.total += covered;
        self.missing += holes;
    }

    fn summary(&self) -> String {
        format!("{} of {} pixels without the scene ({:.3}%)", self.missing, self.total, 100.0 * self.missing as f64 / self.total.max(1) as f64)
    }
}

#[test]
fn sprites_stand_flat_and_both_eyes_see_one_diorama_apart_along_their_rows() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    const SIZE: (u32, u32) = (640, 480);
    // Each scene, and whether its sprites stand apart in the eyes' view (in
    // fields 30 and 12 characters stand one behind another and behind
    // furniture, so each eye sees another part of the farther one).
    for (scene, apart) in [(Scene::Field(1), true), (Scene::Field(30), false), (Scene::Field(12), false),
        (Scene::DebugBattle, false), (Scene::MountainPath, false), (Scene::WorldMap, false)]
    {
        let name = scene.name();
        let mut host = Host::new(&disc);
        scene.reach(&mut host);
        if scene == Scene::WorldMap {
            // At the largest scale (400%), where its walker stands tall
            // enough on the table to count its rows.
            host.table = Table { scale: 4.0, ..Table::default() };
        }
        // A headset's field of view, the table whole in both eyes: from the
        // seat, or in Lahan, whose townspeople stand behind its platform
        // seen from the seat, as far from the table's centre behind it.
        let at = if scene == Scene::Field(1) { Vec3::new(0.0, 0.0, 2.0 * centre().z) } else { Vec3::ZERO };
        let eyes = head(at, SIZE, 90.0);
        let eased = host.steps(40, &eyes);
        assert!(eased.last().is_some_and(|s| s.diorama && s.screen == 0.0), "{name}: the diorama alone shows");
        // Both eyes share each game frame's one build.
        let before = host.view.diorama_stats().builds;
        let shown = host.steps(120, &eyes);
        let stats = host.view.diorama_stats();
        let frames = shown.iter().filter(|s| s.new_frame).count() as u32;
        // Each eye of the pair sees what it sees alone: one diorama through
        // two views.
        let (_, pair) = host.present(&eyes, true);
        let (_, left) = host.present(&eyes[..1], true);
        let (_, right) = host.present(&eyes[1..], true);
        let alone = [&left.eyes[0].0, &right.eyes[0].0];
        let unlike: Vec<usize> = (0..2).map(|eye| pair.eyes[eye].0.iter().zip(alone[eye]).filter(|(a, b)| a != b).count()).collect();
        // As many sprite pixels on each of their rows in either eye, where
        // the sprites stand apart: a billboard faces the seat's way, upright,
        // so each of its rows lies at one depth before both eyes, apart by
        // one disparity.
        let (width, height) = (SIZE.0 as usize, SIZE.1 as usize);
        let rows: Vec<(usize, usize, usize)> = (0..height).map(|y| {
            let count = |classes: &[u8]| (0..width).filter(|&x| u32::from(classes[(y * width + x) * 4 + 2]) == AUX_SPRITE).count();
            (y, count(&pair.eyes[0].1), count(&pair.eyes[1].1))
        }).filter(|&(_, l, r)| l + r > 0).collect();
        let unlike_rows: Vec<&(usize, usize, usize)> = rows.iter().filter(|&&(_, l, r)| l.abs_diff(r) > 2.max(l.max(r) / 10)).collect();
        eprintln!("{name}: {} builds for {frames} game frames over {} presentations to two eyes; {} sprite parts, {} lifted, the thickest stack {} um; \
            the pair's eyes unlike each alone in {unlike:?} bytes; sprite pixels alike on {} of {} rows (unlike: row, left, right {unlike_rows:?})",
            stats.builds - before, shown.len(), stats.sprites, stats.lifted, stats.thickest, rows.len() - unlike_rows.len(), rows.len());
        if let Some(directory) = std::env::var_os("XEM_DIORAMA_EYES") {
            for (eye, (picture, classes)) in pair.eyes.iter().enumerate() {
                for (what, pixels) in [("picture", picture), ("classes", classes)] {
                    let path = PathBuf::from(&directory).join(format!("eyes-{}-{eye}-{what}.png", name.replace([' ', '\''], "-")));
                    let mut encoder = png::Encoder::new(std::fs::File::create(path).unwrap(), SIZE.0, SIZE.1);
                    encoder.set_color(png::ColorType::Rgba);
                    let shown: Vec<u8> = pixels.chunks(4).flat_map(|p| if what == "classes" { [p[2] * 60, 0, 0, 255] } else { [p[0], p[1], p[2], 255] }).collect();
                    encoder.write_header().unwrap().write_image_data(&shown).unwrap();
                }
            }
        }
        assert_eq!(stats.builds - before, frames, "{name}: one build a game frame for both eyes");
        assert!(stats.sprites > 0, "{name}: sprites on the table");
        assert!(stats.thickest <= 1000, "{name}: a sprite's parts within a millimetre of its billboard ({} um)", stats.thickest);
        assert_eq!(unlike, [0, 0], "{name}: each eye of the pair shows what it shows alone");
        assert!(rows.len() >= 20, "{name}: sprites in both eyes ({} rows)", rows.len());
        if apart {
            assert!(unlike_rows.is_empty(), "{name}: sprite rows alike in both eyes, but {unlike_rows:?}");
        }
    }
}

#[test]
fn the_pane_holds_the_interface_as_the_flat_frame_draws_it() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let scenes: [(&str, Vec<Value>); 3] = [
        ("field 12's dialogue", vec![json!({"cmd": "enter_field", "map": 12})]),
        ("the debug battle", debug_battle()),
        ("the world map", vec![json!({"cmd": "boot_mode", "mode": 3}), json!({"cmd": "frames", "count": 800})]),
    ];
    let mut results = Vec::new();
    for (name, commands) in scenes {
        let mut host = Host::new(&disc);
        for command in commands {
            run(&mut host.session, command);
        }
        host.beside();
        let eyes = seated(SMALL, 90.0);
        let mut shown = host.steps(60, &eyes);
        if name.starts_with("field 12") {
            // Until the dialogue's window shows, and its text with it.
            for _ in 0..600 {
                if host.context() == "dialogue" {
                    break;
                }
                shown.extend(host.steps(10, &eyes));
            }
            assert_eq!(host.context(), "dialogue", "field 12's dialogue opens");
            shown.extend(host.steps(40, &eyes));
        }
        let last = shown.last().expect("presented");
        assert!(last.diorama && last.screen == 0.0, "{name}: the diorama alone shows ({}, {})", last.diorama, last.screen);
        let (_, pictures) = host.present(&eyes, true);
        let pane = pictures.pane.expect("the pane");
        let read = |view: &Option<GameView>| {
            let frame = view.as_ref().expect("a view beside").frame();
            let picture = |texture| xem_render::read_rgba8(&host.gpu.device, &host.gpu.queue, texture);
            (frame.size(), picture(frame.color()), picture(frame.aux()))
        };
        let (size, flat, classes) = read(&host.screen);
        let (_, after_flat, after_classes) = read(&host.after_scene);
        assert_eq!(pane.len(), flat.len(), "{name}: the pane is the flat frame's size {size:?}");
        if let Some(directory) = std::env::var_os("XEM_DIORAMA_PANE") {
            // Opaque pane pixels: white where they are the flat frame's, red where not.
            let diff: Vec<u8> = (0..pane.len() / 4).flat_map(|i| match pane[i * 4 + 3] {
                255 if (0..3).all(|c| pane[i * 4 + c].abs_diff(flat[i * 4 + c]) <= 2) => [255, 255, 255, 255],
                255 => [255, 0, 0, 255],
                0 => [0, 0, 0, 255],
                _ => [0, 0, 128, 255],
            }).collect();
            for (what, pixels) in [("pane", &pane), ("flat", &flat), ("classes", &classes), ("diff", &diff)] {
                let path = PathBuf::from(&directory).join(format!("{}-{what}.png", name.replace([' ', '\''], "-")));
                let mut encoder = png::Encoder::new(std::fs::File::create(path).unwrap(), size.0, size.1);
                encoder.set_color(png::ColorType::Rgba);
                let shown: Vec<u8> = if what == "classes" {
                    pixels.chunks(4).flat_map(|p| [p[2] * 60, if p[3] == 255 { 255 } else { 0 }, (p[0] / 2).saturating_add(p[1] / 128), 255]).collect()
                } else {
                    pixels.chunks(4).flat_map(|p| [p[0], p[1], p[2], 255]).collect()
                };
                encoder.write_header().unwrap().write_image_data(&shown).unwrap();
            }
        }
        // Where the pane is opaque, the flat frame's colour and the
        // interface's class there; where the flat frame's interface is
        // opaque, the pane is too, when the game drew no pictures before its
        // scene (those are the wall's: the debug battle's forest); and
        // wherever the interface drawn after the scene is opaque (the flat
        // frame of the trace without the wall's pictures), the pane is, in
        // its colour.
        let pixels = pane.len() / 4;
        let opaque: Vec<usize> = (0..pixels).filter(|&i| pane[i * 4 + 3] == 255).collect();
        let level = |i: usize, other: &[u8]| (0..3).map(|c| pane[i * 4 + c].abs_diff(other[i * 4 + c])).max().unwrap_or(0);
        let differing: Vec<(usize, usize, u8)> = opaque.iter().copied().filter(|&i| level(i, &flat) > 0)
            .map(|i| (i % size.0 as usize, i / size.0 as usize, level(i, &flat))).collect();
        let interface = opaque.iter().filter(|&&i| u32::from(classes[i * 4 + 2]) == AUX_INTERFACE).count();
        let opaque_interface = |classes: &[u8]| -> Vec<usize> { (0..pixels).filter(|&i| u32::from(classes[i * 4 + 2]) == AUX_INTERFACE && classes[i * 4 + 3] != 255).collect() };
        let flat_interface = opaque_interface(&classes);
        let on_pane = flat_interface.iter().filter(|&&i| pane[i * 4 + 3] == 255).count();
        let after = opaque_interface(&after_classes);
        let after_on_pane = after.iter().filter(|&&i| pane[i * 4 + 3] == 255 && level(i, &after_flat) == 0).count();
        let wall = host.view.diorama_stats().wall;
        eprintln!("{name}: the pane {size:?}: {} opaque pixels, {} of them unlike the flat frame ({differing:?}), {interface} the interface's class; \
            of the flat frame's {} opaque interface pixels {on_pane} opaque on the pane, of the {} drawn after the scene {after_on_pane} on it in their colour; {wall} primitives on the wall",
            opaque.len(), differing.len(), flat_interface.len(), after.len());
        results.push((name, opaque.len(), differing, interface, (flat_interface.len(), on_pane), (after.len(), after_on_pane), wall));
    }
    for (name, opaque, differing, interface, (flat_interface, on_pane), (after, after_on_pane), wall) in results {
        assert!(opaque >= 500, "{name}: the interface on the pane ({opaque} opaque pixels)");
        if name == "the world map" {
            // Measured, its cause not established (an open issue,
            // docs/runtime.md Diorama): the first native pixel of six rows of
            // the compass's red cone, its left edge, one 15-bit step off.
            assert!(differing.len() <= 21 && differing.iter().all(|&(x, y, off)| COMPASS_EDGE.0.contains(&x) && COMPASS_EDGE.1.contains(&y) && off <= 8),
                "{name}: the pane's opaque pixels are the flat frame's but for the compass cone's edge: {differing:?}");
        } else {
            assert!(differing.is_empty(), "{name}: the pane's opaque pixels are the flat frame's: {differing:?}");
        }
        assert_eq!(interface, opaque, "{name}: the pane holds the interface alone");
        if wall <= 1 {
            assert_eq!(on_pane, flat_interface, "{name}: the flat frame's opaque interface is on the pane");
        }
        assert!(after >= 500 && after_on_pane == after, "{name}: the interface drawn after the scene on the pane in its colour ({after_on_pane} of {after})");
    }
}

/// Where the world map's compass cone's left edge lies on its pane (its
/// 640x432 picture, twice the native 320x216): the 21 pixels one 15-bit step
/// off the flat frame (native columns 286 to 293, rows 141 to 150: the first
/// pixel of six rows).
const COMPASS_EDGE: (std::ops::RangeInclusive<usize>, std::ops::RangeInclusive<usize>) = (572..=587, 282..=301);

/// Checks that the screen's share eases: between two presentations of the
/// diorama view it moves by at most the time passed over the switch (but
/// where the device's own picture or first-person inspection's eyes came
/// between, which take the view at once).
fn eases(name: &str, shown: &[Shown]) {
    for pair in shown.windows(2) {
        let (a, b) = (&pair[0], &pair[1]);
        if a.device || b.device || a.within || b.within {
            continue;
        }
        let allowed = (b.blank - a.blank) as f32 / 60.0 * SWITCH_RATE + 1e-3;
        assert!((b.screen - a.screen).abs() <= allowed, "{name}: the screen's share jumped from {} to {} between blanks {} and {} ({} to {})",
            a.screen, b.screen, a.blank, b.blank, a.scene, b.scene);
        // While the diorama shows its eyes, at one minus the screen's share
        // (dimmed after a jump of its table).
        if b.diorama {
            assert!(b.opacity <= 1.0 - b.screen + 1e-4, "{name}: the diorama at {} over a screen at {}", b.opacity, b.screen);
        }
    }
}

/// The runs of `shown` (blank ranges) in which `what` holds.
fn spans(shown: &[Shown], what: impl Fn(&Shown) -> bool) -> Vec<(u64, u64)> {
    let mut spans: Vec<(u64, u64)> = Vec::new();
    for s in shown.iter().filter(|s| what(s)) {
        match spans.last_mut() {
            Some(span) if span.1 + 1 == s.blank => span.1 = s.blank,
            _ => spans.push((s.blank, s.blank)),
        }
    }
    spans
}

/// A timeline of `shown` for the test's output: each change of scene and of
/// what shows.
fn timeline(shown: &[Shown]) -> String {
    let mut lines = Vec::new();
    let key = |s: &Shown| (s.scene.clone(), s.diorama, s.screen == 0.0, s.screen == 1.0, s.device, s.within, s.movie, s.mode());
    let mut last = None;
    for s in shown {
        let k = key(s);
        if last.as_ref() != Some(&k) {
            lines.push(format!("{} {} diorama={} screen={:.3} device={} within={} movie={} mode={:?}", s.blank, s.scene, s.diorama, s.screen, s.device, s.within, s.movie, s.mode()));
            last = Some(k);
        }
    }
    lines.join("\n")
}

#[test]
fn the_title_movie_and_the_title_after_it_show_on_the_screen_the_movie_in_its_own_shape() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut host = Host::new(&disc);
    let eyes = seated(SMALL, 90.0);
    let shown = host.steps(2200, &eyes);
    eprintln!("from boot:\n{}", timeline(&shown));
    eases("from boot", &shown);
    let movie: Vec<&Shown> = shown.iter().filter(|s| s.scene == "movie").collect();
    assert!(movie.len() > 300, "the title movie plays ({} blanks)", movie.len());
    assert!(movie.iter().all(|s| !s.diorama && s.screen == 1.0), "the movie on the screen alone");
    let rebuilt: Vec<&&Shown> = movie.iter().filter(|s| s.movie).collect();
    let shapes: std::collections::BTreeSet<((u32, u32), (u32, u32))> = rebuilt.iter().filter_map(|s| Some((s.flat?, s.display))).collect();
    eprintln!("the title movie: {} blanks, {} of them rebuilt frames (frame, display: {shapes:?}), {} the device's own", movie.len(), rebuilt.len(),
        movie.iter().filter(|s| s.device).count());
    assert!(rebuilt.len() * 2 > movie.len(), "its frames rebuilt ({} of {})", rebuilt.len(), movie.len());
    for s in &rebuilt {
        let (w, h) = s.flat.expect("the movie's frame");
        let expected = w as f32 * s.display.1 as f32 / s.display.0 as f32;
        assert!((h as f32 - expected).abs() <= 1.0, "blank {}: the movie's frame {w}x{h} keeps the display's {:?} shape", s.blank, s.display);
    }
    // The title (field 490) after it: a field's mode, nothing a diorama
    // places, on the screen.
    let title: Vec<&Shown> = shown.iter().filter(|s| s.scene == "field:490").collect();
    assert!(title.len() > 300, "the title shows ({} blanks)", title.len());
    assert!(title.iter().all(|s| !s.diorama && s.screen == 1.0), "the title on the screen alone");
}

#[test]
fn a_fields_menu_and_first_person_inspection_take_the_view_and_give_the_diorama_back() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut host = Host::new(&disc);
    run(&mut host.session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut host.session, json!({"cmd": "frames", "count": 400}));
    run(&mut host.session, json!({"cmd": "press", "buttons": ["down"], "frames": 60}));
    run(&mut host.session, json!({"cmd": "frames", "count": 30}));
    // At twice its scale, two seconds for the table to settle on the still
    // camera's target.
    host.table = Table { scale: 2.0, ..Table::default() };
    let eyes = seated(SMALL, 90.0);
    let mut shown = host.steps(120, &eyes);
    let before = shown.last().cloned().expect("presented");
    assert!(before.diorama && before.screen == 0.0, "field 30's diorama shows");
    let space = before.space.expect("its space");
    let moved = |a: &DioramaSpace, b: &DioramaSpace| {
        let (a, b) = (a.room_from_world.to_cols_array(), b.room_from_world.to_cols_array());
        (0..16).map(|i| (a[i] - b[i]).abs()).fold(0.0, f32::max)
    };
    // Inspection's eyes take the view at once, and give the diorama back at
    // once, at the scale it had, standing where it stood.
    host.view.set_first_person(true);
    let inspecting = host.steps(90, &eyes);
    assert!(inspecting.iter().all(|s| s.within && !s.diorama), "inspection looks from within the scene from its first presentation");
    host.view.set_first_person(false);
    let back = host.steps(60, &eyes);
    let first = &back[0];
    assert!(first.diorama && first.screen == 0.0, "the diorama back at once ({}, {})", first.diorama, first.screen);
    let again = first.space.expect("its space");
    eprintln!("inspection over the diorama at {} m a unit: back at {} m a unit, within {} of where it stood", space.scale, again.scale, moved(&space, &again));
    assert_eq!(again.scale, space.scale, "the scale it had");
    assert!(moved(&space, &again) < 1e-4, "the table where it stood ({})", moved(&space, &again));
    assert!(back.iter().all(|s| s.diorama && s.screen == 0.0), "the diorama shows on");
    shown.extend(inspecting);
    shown.extend(back);
    // A route under way when inspection begins stops; no route restarts
    // when it ends, and the character stays where it stopped.
    let player = |host: &mut Host| {
        let field = run(&mut host.session, json!({"cmd": "field"}));
        field["actors"].as_array().unwrap().iter().find(|a| a["index"] == field["player"]).unwrap()["position"].clone()
    };
    let walk = run(&mut host.session, json!({"cmd": "navigate", "to": [100, 20]}));
    assert!(walk["status"]["assist"].is_object(), "a route: {walk}");
    shown.extend(host.steps(10, &eyes));
    host.view.set_first_person(true);
    shown.extend(host.steps(60, &eyes));
    let status = run(&mut host.session, json!({"cmd": "status"}));
    assert!(status["assist"].is_null() && status["assist_outcome"] == "stopped", "inspection stopped the route: {status}");
    let stopped = player(&mut host);
    host.view.set_first_person(false);
    let back = host.steps(120, &eyes);
    let status = run(&mut host.session, json!({"cmd": "status"}));
    let after = player(&mut host);
    eprintln!("inspection stopped the route with the character at {stopped}; 120 blanks after it, {after}, the route {}", status["assist"]);
    assert!(status["assist"].is_null(), "no route restarted: {status}");
    assert_eq!(stopped, after, "the character stays where inspection stopped it");
    assert!(back[0].diorama && back.iter().all(|s| s.diorama && s.screen == 0.0), "the diorama back at once and on");
    shown.extend(back);
    // The field's menu: the diorama eases out to the screen, and back once
    // the menu closes.
    run(&mut host.session, json!({"cmd": "act", "actions": ["Menu"], "frames": 2}));
    let mut menu = host.steps(240, &eyes);
    run(&mut host.session, json!({"cmd": "act", "actions": ["Cancel"], "frames": 2}));
    menu.extend(host.steps(300, &eyes));
    eprintln!("the menu:\n{}", timeline(&menu));
    eases("the menu", &menu);
    // (The field's menu is the field's mode drawing no scene, without a
    // camera record.)
    assert!(menu.iter().any(|s| !s.diorama && s.screen == 1.0), "the menu on the screen alone");
    let out = spans(&menu, |s| s.screen > 0.0 && s.screen < 1.0);
    assert!(out.len() >= 2, "the view eases out and back ({out:?})");
    assert!(menu.last().is_some_and(|s| s.diorama && s.screen == 0.0 && s.mode() == Some(CameraMode::Field)), "the field's diorama again");
    shown.extend(menu);
    eases("field 30", &shown);
}

#[test]
fn a_random_battle_and_its_end_ease_from_the_fields_diorama_to_the_battles_and_back() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut host = Host::new(&disc);
    let eyes = seated(SMALL, 90.0);
    let (mut shown, came) = host.mountain_path_battle(&eyes);
    // Its party attacks until it ends and the field shows again.
    let mut ended = None;
    let mut next = host.blanks + 300;
    for _ in 0..1200 {
        shown.extend(host.steps(10, &eyes));
        if host.blanks >= next {
            run(&mut host.session, json!({"cmd": "press", "buttons": ["cross"], "frames": 2}));
            next = host.blanks + 30;
        }
        if host.mode() == 1 && ended.is_none() && host.blanks > came + 600 {
            ended = Some(host.blanks);
            shown.extend(host.steps(240, &eyes));
            break;
        }
    }
    eprintln!("field 15, its random battle at {came}, back at {ended:?}:\n{}", timeline(&shown));
    let ended = ended.expect("the battle ends and the field shows again");
    eases("the random battle", &shown);
    let field = |s: &Shown| s.diorama && s.screen == 0.0 && s.mode() == Some(CameraMode::Field);
    let battle = |s: &Shown| s.diorama && s.screen == 0.0 && s.mode() == Some(CameraMode::Battle);
    assert!(shown.iter().any(|s| s.blank < came && field(s)), "the field's diorama before the battle");
    assert!(shown.iter().any(|s| s.blank > came && s.blank < ended && battle(s)), "the battle's diorama");
    assert!(shown.iter().any(|s| s.blank > ended && field(s)), "the field's diorama after it");
    let between = |from: u64, to: u64| shown.iter().filter(|s| s.blank > from && s.blank < to).any(|s| s.screen == 1.0);
    let into = shown.iter().filter(|s| s.blank < came && field(s)).map(|s| s.blank).max().unwrap();
    let fought = shown.iter().filter(|s| battle(s)).map(|s| s.blank).collect::<Vec<_>>();
    assert!(between(into, fought[0]), "the screen between the field's diorama and the battle's");
    let back = shown.iter().filter(|s| s.blank > ended && field(s)).map(|s| s.blank).min().unwrap();
    assert!(between(*fought.last().unwrap(), back), "the screen between the battle's diorama and the field's");
}

#[test]
fn leaving_lahan_for_the_world_map_and_entering_a_field_from_it_ease_between_their_dioramas() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut host = Host::new(&disc);
    let eyes = seated(SMALL, 90.0);
    let mut shown = host.lahan_exit(&eyes);
    let left = shown.iter().find(|s| s.scene == "worldmap").map(|s| s.blank).expect("the world map");
    // Into a field from the world map.
    run(&mut host.session, json!({"cmd": "enter_field", "map": 1}));
    let entered = host.blanks;
    for _ in 0..90 {
        shown.extend(host.steps(10, &eyes));
    }
    eprintln!("Lahan, the world map from {left}, Lahan again from {entered}:\n{}", timeline(&shown));
    eases("Lahan and the world map", &shown);
    let field = |s: &Shown| s.diorama && s.screen == 0.0 && s.mode() == Some(CameraMode::Field);
    let world = |s: &Shown| s.diorama && s.screen == 0.0 && s.mode() == Some(CameraMode::Worldmap);
    assert!(shown.iter().any(|s| s.blank < left && field(s)), "Lahan's diorama before");
    assert!(shown.iter().any(|s| s.blank > left && s.blank < entered && world(s)), "the world map's diorama");
    assert!(shown.iter().any(|s| s.blank > entered && field(s)), "Lahan's diorama again");
}

/// The field movie: stopped and requested (text_and_movies.rs).
const MOVIE_STOPPED: u32 = 0x800A_DB6C;
const MOVIE_REQUESTED: u32 = 0x800A_DB70;

fn word(session: &mut Session<NativeModule>, address: u32) -> u32 {
    let mut bytes = [0; 4];
    session.runtime.memory().read(address, &mut bytes).expect("game memory");
    u32::from_le_bytes(bytes)
}

#[test]
fn a_story_movie_eases_out_of_the_fields_diorama_and_back_and_the_next_maps_follows() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // Map 25's scene, its dialogue auto-advanced, into its movie (#7), the
    // field after it and map 23 (text_and_movies.rs's route).
    let mut host = Host::new(&disc);
    run(&mut host.session, json!({"cmd": "enter_field", "map": 25}));
    host.session.set_auto_advance(1).expect("a dwell");
    let eyes = seated(SMALL, 90.0);
    let mut shown = Vec::new();
    let mut began = None;
    for _ in 0..8000 {
        shown.push(host.step(&eyes));
        if word(&mut host.session, MOVIE_REQUESTED) != 0 && word(&mut host.session, MOVIE_STOPPED) == 0 {
            host.session.set_auto_advance(0).expect("off");
            began = Some(host.blanks);
            break;
        }
    }
    let began = began.expect("map 25's movie begins");
    let mut entered = None;
    for _ in 0..8000 {
        shown.push(host.step(&eyes));
        if entered.is_none() && shown.last().is_some_and(|s| s.scene == "field:23") {
            entered = Some(host.blanks);
        }
        if entered.is_some_and(|at| host.blanks >= at + 300) {
            break;
        }
    }
    let entered = entered.expect("map 23 comes");
    eprintln!("map 25, its movie from blank {began}, map 23 from {entered}:\n{}", timeline(&shown));
    eases("map 25, its movie and map 23", &shown);
    let field = |s: &Shown, map: &str| s.diorama && s.screen == 0.0 && s.mode() == Some(CameraMode::Field) && s.scene == map;
    let rebuilt: Vec<&Shown> = shown.iter().filter(|s| s.movie).collect();
    let (first, last) = (rebuilt.first().expect("the movie's frames rebuilt").blank, rebuilt.last().expect("rebuilt").blank);
    // Map 25's diorama before the movie and after it (the field goes on
    // there a while), then map 23's.
    let before = shown.iter().filter(|s| s.blank < first && field(s, "field:25")).map(|s| s.blank).max().expect("map 25's diorama before the movie");
    let back = shown.iter().filter(|s| s.blank > last && field(s, "field:25")).map(|s| s.blank).min().expect("map 25's diorama after it");
    let next = shown.iter().filter(|s| field(s, "field:23")).map(|s| s.blank).min().expect("map 23's diorama");
    let easing = |from: u64, to: u64| spans(&shown, |s| s.blank > from && s.blank < to && s.screen > 0.0 && s.screen < 1.0);
    let alone = rebuilt.iter().filter(|s| s.screen == 1.0 && !s.diorama).map(|s| s.blank).collect::<Vec<_>>();
    let start = *alone.first().expect("the movie on the screen alone");
    let (out, into) = (easing(before, start), easing(last, back));
    let shapes: std::collections::BTreeSet<((u32, u32), (u32, u32))> = rebuilt.iter().filter_map(|s| Some((s.flat?, s.display))).collect();
    eprintln!("map 25's diorama last alone at {before}, the movie's rebuilt frames {first}..={last} ({} of them, {} on the screen alone from {start}; frame, display: {shapes:?}), \
        map 25's diorama again from {back}, map 23's from {next}; eased out {out:?}, back in {into:?}", rebuilt.len(), alone.len());
    assert!(before < first && last < back && back < next, "map 25's diorama, the movie, map 25's diorama, map 23's");
    assert!(out.len() == 1 && rebuilt.iter().any(|s| s.blank >= out[0].0 && s.blank <= out[0].1), "the diorama eases out over the movie's frames: {out:?}");
    assert_eq!(into.len(), 1, "map 25's diorama eases in after the movie: {into:?}");
    assert!(alone.len() * 10 > rebuilt.len() * 9, "the movie on the screen alone ({} of {} rebuilt frames)", alone.len(), rebuilt.len());
    for s in &rebuilt {
        let (w, h) = s.flat.expect("the movie's frame");
        let expected = w as f32 * s.display.1 as f32 / s.display.0 as f32;
        assert!((h as f32 - expected).abs() <= 1.0, "blank {}: the movie's frame {w}x{h} keeps the display's {:?} shape", s.blank, s.display);
    }
    assert!(shown.last().is_some_and(|s| field(s, "field:23")), "map 23's diorama shows on");
}

#[test]
fn switching_the_headset_view_cuts_to_the_screen_and_eases_back_into_the_diorama() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut host = Host::new(&disc);
    run(&mut host.session, json!({"cmd": "enter_field", "map": 30}));
    run(&mut host.session, json!({"cmd": "frames", "count": 400}));
    let eyes = seated(SMALL, 90.0);
    let shown = host.steps(60, &eyes);
    assert!(shown.last().is_some_and(|s| s.diorama && s.screen == 0.0), "field 30's diorama shows");
    // Graphics' headset view set to the screen in the session (the panel's
    // Headset card): the screen at once, as the hosts then present it.
    host.screen_view = true;
    let screen = host.steps(60, &eyes);
    assert!(screen.iter().all(|s| !s.diorama && s.screen == 1.0 && !s.within && s.flat.is_some()), "the screen at once and on: {}", timeline(&screen));
    // Set to the diorama again: it eases in from the screen over the switch.
    host.screen_view = false;
    let back = host.steps(60, &eyes);
    eprintln!("the view switched to the screen at blank {}, to the diorama at {}:\n{}", screen[0].blank, back[0].blank, timeline(&back));
    eases("back to the diorama", &back);
    assert_eq!(back[0].screen, 1.0, "from the screen");
    let easing = spans(&back, |s| s.screen > 0.0 && s.screen < 1.0);
    assert_eq!(easing.len(), 1, "eased in once: {easing:?}");
    assert!(back.last().is_some_and(|s| s.diorama && s.screen == 0.0 && s.mode() == Some(CameraMode::Field)), "the field's diorama again");
}
