//! The cameras a view from anywhere asks the port for (docs/runtime.md,
//! Camera records), on the original game: with the culled-face ring's
//! `around` bit 2 (`Reveals::ANYWHERE`), the port records the camera each
//! frame is drawn with in a field (Lahan, map 1), a battle (the one the
//! debug battle selector starts as it is, and its desert, whose stage is
//! drawn over the last frame instead of a cleared buffer) and on the world
//! map, and each frame shown keeps the one its frame was drawn with, also
//! when a presentation takes several game frames (the desert, four at a
//! time). Through the inverse of each frame's own camera the scenery that
//! holds still stays where it is while the game's camera turns or moves:
//! from a frame to the next, the median still primitive moves less than 4
//! world units and 95 in 100 of them do (through the frame before's camera,
//! the median moves more than four times that). In a battle and on the
//! world map the camera looks at the target it records. A battle's cameras
//! carry its stage: at rest, its combatants at their sprites' places and
//! heights as the battle keeps them, its scene data's start view, and its
//! turns running. The records change
//! nothing of the game: from the same state, the same frames end at the
//! same RAM, VRAM, sound and clock with `around` 0, 1 and 2 as without a
//! trace.
//! Local only: it needs the user's disc 1 and a game module build; the GPU
//! is whatever wgpu finds (lavapipe in the runtime shell).
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test camera

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::json;
use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};
use xem_core::control::Session;
use xem_game::{NativeModule, boot};
use xem_gpu::{Camera, CameraMode, Class, Primitive, Reveals, TraceOp};
use xem_render::Gpu;
use xem_render::ps1::{GameView, HostClass, ViewConfig};
use xem_settings::Graphics;

/// What a view from anywhere asks of the trace.
const ANYWHERE: Reveals = Reveals { reach: [32, 32], pull_back: 0, around: Reveals::ANYWHERE };

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

/// The game's three 3D modes, each reached from boot as the other tests
/// reach them, a battle twice.
#[derive(Clone, Copy, Debug)]
enum Mode {
    Field,
    /// The battle the debug battle selector starts as it is, as
    /// tests/render.rs starts it to watch its attacks.
    Battle,
    /// Its desert (tests/render.rs `start_desert_battle`), whose stage the
    /// game draws over the last frame instead of clearing its buffer.
    Desert,
    Worldmap,
}

impl Mode {
    fn camera(self) -> CameraMode {
        match self {
            Mode::Field => CameraMode::Field,
            Mode::Battle | Mode::Desert => CameraMode::Battle,
            Mode::Worldmap => CameraMode::Worldmap,
        }
    }

    /// Its scene's name (`Session::scene`, before a field's map).
    fn scene(self) -> &'static str {
        match self {
            Mode::Field => "field",
            Mode::Battle | Mode::Desert => "battle",
            Mode::Worldmap => "worldmap",
        }
    }

    /// A session in the mode.
    fn enter(self, disc: &Path) -> Session<NativeModule> {
        let (mut session, _) = boot(disc, &Default::default()).expect("boot");
        let execute = |session: &mut Session<NativeModule>, commands: &[serde_json::Value]| {
            for command in commands {
                assert!(session.execute(command).get("error").is_none(), "{command}");
            }
        };
        let start = json!({"cmd": "press", "buttons": ["start"], "frames": 1});
        match self {
            // A field whose camera turns (not map 12's: its view flags hold it).
            Mode::Field => execute(&mut session, &[json!({"cmd": "enter_field", "map": 1}), json!({"cmd": "frames", "count": 400})]),
            Mode::Worldmap => execute(&mut session, &[json!({"cmd": "boot_mode", "mode": 3}), json!({"cmd": "frames", "count": 900})]),
            Mode::Battle | Mode::Desert => {
                execute(&mut session, &[
                    json!({"cmd": "frames", "count": 60}),
                    json!({"cmd": "write", "path": "mode_battle_standalone", "value": 1}),
                    json!({"cmd": "boot_mode", "mode": 2}),
                    json!({"cmd": "frames", "count": 900}),
                ]);
                if let Mode::Battle = self {
                    execute(&mut session, &[start, json!({"cmd": "frames", "count": 1520})]);
                } else {
                    // Formation 15 of the selector's first event file, the
                    // party in their Gears (`battle_scene_select_values`);
                    // from its start, while its camera comes in.
                    for (row, column, value) in [(0u32, 0u32, 15u32), (3, 0, 253), (2, 0, 1), (2, 1, 1), (2, 2, 1)] {
                        session.runtime.memory().write(0x801E_1DA0 + (row * 3 + column) * 4, &value.to_le_bytes()).unwrap();
                    }
                    execute(&mut session, &[start, json!({"cmd": "frames", "count": 300})]);
                }
            }
        }
        session
    }

    /// The buttons held in frame `n`: L1 in turns in a field and on the
    /// world map (their camera's turn, which eases to rest in the frames
    /// between), Cross now and then in the first battle (its commands: the
    /// camera follows the attacks), none in the desert (Cross ends that
    /// debug battle).
    fn buttons(self, n: u64) -> Vec<&'static str> {
        match self {
            Mode::Field | Mode::Worldmap => if n % 150 < 30 { vec!["l1"] } else { vec![] },
            Mode::Battle => if n % 10 < 2 { vec!["cross"] } else { vec![] },
            Mode::Desert => vec![],
        }
    }
}

/// A frame shown: the camera its log kept, and what it drew.
struct Shown {
    camera: Option<Camera>,
    ops: Vec<TraceOp>,
}

/// Presents `frames` frames as a host would with a view from anywhere (the
/// trace asking for every face and the cameras), `per` game frames a
/// presentation (more than one as in fast-forward), holding `buttons(n)` in
/// frame `n`; returns the frames shown, in order.
fn present(session: &mut Session<NativeModule>, gpu: &Gpu, frames: u64, per: u64, buttons: impl Fn(u64) -> Vec<&'static str>) -> Vec<Shown> {
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    let mut shown: Vec<Shown> = Vec::new();
    let mut identity = None;
    for n in 0..frames {
        session.execute(&json!({"cmd": "pad", "buttons": buttons(n)}));
        assert!(session.frame().expect("frame"));
        if (n + 1) % per != 0 {
            continue;
        }
        view.configure(ViewConfig::resolve(&Graphics::default(), HostClass::Desktop, (640, 448), view.max_scale()));
        let device = session.runtime.services().gpu.device_mut();
        if view.take_restart() || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(Some(ANYWHERE));
        view.push(device.take_trace());
        let display = device.display();
        let scene = session.scene();
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let _ = view.present(&mut encoder, &display, &scene, n as f64 / 60.0);
        gpu.queue.submit([encoder.finish()]);
        let frame = view.frame();
        let Some(now) = frame.identity() else { continue };
        if identity == Some(now) {
            // The same frame, perhaps drawn further.
            let last = shown.last_mut().expect("a frame");
            (last.camera, last.ops) = (frame.camera(), frame.newest().to_vec());
        } else {
            identity = Some(now);
            shown.push(Shown { camera: frame.camera(), ops: frame.newest().to_vec() });
        }
    }
    shown
}

/// What tells a scenery primitive from the others of its frame: its owner,
/// shape, texture page and CLUT, and its points' texture coordinates and
/// colours.
type Key = (u32, u8, u16, u32, [(i16, i16, u32); 4]);

/// The view-space points of each scenery primitive (models and the world,
/// not sprites or the interface) a frame drew once.
type Points = HashMap<Key, Vec<[f32; 3]>>;

fn scenery(ops: &[TraceOp]) -> Points {
    let mut found: HashMap<Key, Option<Vec<[f32; 3]>>> = HashMap::new();
    for op in ops {
        let TraceOp::Draw(p) = op else { continue };
        if !matches!(p.class, Class::Model | Class::World) {
            continue;
        }
        let Some(points) = p.vertices[..p.vertex_count()].iter().map(|v| v.view).collect::<Option<Vec<_>>>() else { continue };
        found.entry(key(p)).and_modify(|seen| *seen = None).or_insert(Some(points));
    }
    found.into_iter().filter_map(|(key, points)| Some((key, points?))).collect()
}

fn key(p: &Primitive) -> Key {
    let points = std::array::from_fn(|i| {
        let v = &p.vertices[i];
        (v.u, v.v, if p.textured { 0 } else { v.color })
    });
    (p.owner, p.shape as u8, p.clut, p.environment.draw_mode & 0x1FF, points)
}

fn distance(p: [f32; 3], q: [f32; 3]) -> f32 {
    ((p[0] - q[0]).powi(2) + (p[1] - q[1]).powi(2) + (p[2] - q[2]).powi(2)).sqrt()
}

/// For each of the primitives of `keys` two frames both drew, the farthest
/// any of its points lies from where it was in the world (the first frame's
/// points through `a`, the second's through `b`), in order.
fn moved(keys: &HashSet<Key>, first: &Points, second: &Points, a: &Camera, b: &Camera) -> Vec<f32> {
    let mut distances: Vec<f32> = keys
        .iter()
        .filter_map(|key| {
            let (points, others) = (first.get(key)?, second.get(key)?);
            points.iter().zip(others).try_fold(0f32, |far, (p, q)| Some(far.max(distance(a.world_point(*p)?, b.world_point(*q)?))))
        })
        .collect();
    distances.sort_by(f32::total_cmp);
    distances
}

/// The value `share` of the ordered `values` lie below or at (the median
/// for a half).
fn quantile(values: &[f32], share: f64) -> f32 {
    let at = ((values.len() as f64 * share) as usize).min(values.len().saturating_sub(1));
    values.get(at).copied().unwrap_or(f32::NAN)
}

/// The direction a camera looks along in its world (its matrix's third row).
fn forward(camera: &Camera) -> [f32; 3] {
    let row = [camera.axes[0][2], camera.axes[1][2], camera.axes[2][2]];
    let length = row.iter().map(|c| c * c).sum::<f32>().sqrt();
    row.map(|c| c / length)
}

/// The angle between two cameras' directions, in degrees.
fn turn(a: &Camera, b: &Camera) -> f32 {
    let (a, b) = (forward(a), forward(b));
    (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]).clamp(-1.0, 1.0).acos().to_degrees()
}

/// Where a camera stands in its world.
fn eye(camera: &Camera) -> [f32; 3] {
    camera.world_point([0.0; 3]).expect("a camera spanning space")
}

/// Whether two cameras place the world alike (the point they look at may
/// differ).
fn same_view(a: &Camera, b: &Camera) -> bool {
    (a.axes, a.translation) == (b.axes, b.translation)
}

/// In `mode`, the frames shown with a view from anywhere keep their cameras,
/// and the scenery that holds still (the primitives whose view-space points
/// stay the same from a frame to the next while the camera record does
/// too) stays put through each frame's own camera while the camera turns or
/// moves: the median over the primitives two frames share moves less than
/// `tolerance` world units, against more than four times that through the
/// frame before's camera where it turned more than a degree or moved more
/// than that, and 95 in 100 of all the distances measured are within
/// `tolerance` (some primitives the key cannot tell apart from another
/// elsewhere, and quads that move with the camera, move further). A
/// battle's and the world map's look-at camera has the target it records
/// on its axis, at its recorded depth. The frames are presented `per` game
/// frames at a time.
fn scenery_stays_put_as_the_camera_turns(mode: Mode, frames: u64, per: u64, tolerance: f32) {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut session = mode.enter(&disc);
    let shown = present(&mut session, &gpu, frames, per, |n| mode.buttons(n));
    let recorded = shown.iter().filter(|frame| frame.camera.is_some_and(|c| c.mode == mode.camera())).count();
    assert!(shown.len() > 100 && recorded * 10 >= shown.len() * 9, "{mode:?}: {recorded} of {} frames shown keep a camera", shown.len());
    let frames: Vec<Option<(Camera, Points)>> = shown.iter().map(|frame| frame.camera.map(|camera| (camera, scenery(&frame.ops)))).collect();
    let (mut holds, mut alike): (HashMap<Key, bool>, usize) = (HashMap::new(), 0);
    for pair in frames.windows(2) {
        let [Some((a, first)), Some((b, second))] = pair else { continue };
        if !same_view(a, b) {
            continue;
        }
        alike += 1;
        for (key, points) in first {
            if let Some(others) = second.get(key) {
                let same = points.iter().zip(others).all(|(p, q)| distance(*p, *q) <= 1.0);
                *holds.entry(*key).or_insert(true) &= same;
            }
        }
    }
    let still: HashSet<Key> = holds.into_iter().filter_map(|(key, same)| same.then_some(key)).collect();
    let (mut turned, mut travelled, mut checked, mut worst, mut through_previous) = (0.0, 0.0, 0, 0f32, Vec::new());
    let mut distances = Vec::new();
    for pair in frames.windows(2) {
        let [Some((a, first)), Some((b, second))] = pair else { continue };
        if same_view(a, b) {
            continue;
        }
        let own = moved(&still, first, second, a, b);
        if own.len() < 30 {
            continue;
        }
        let (median, angle, travel) = (quantile(&own, 0.5), turn(a, b), distance(eye(a), eye(b)));
        let stale = quantile(&moved(&still, first, second, a, a), 0.5);
        assert!(median <= tolerance, "{mode:?}: the median still primitive moved {median} world units in a frame through each frame's camera (the camera turned {angle} degrees and moved {travel}; {} primitives)", own.len());
        (turned, travelled, checked, worst) = (turned + angle, travelled + travel, checked + 1, worst.max(median));
        if angle > 1.0 || travel > tolerance {
            through_previous.push(stale);
        }
        distances.extend(own);
    }
    through_previous.sort_by(f32::total_cmp);
    let stale = quantile(&through_previous, 0.5);
    distances.sort_by(f32::total_cmp);
    let within = distances.iter().filter(|d| **d <= tolerance).count() as f64 / distances.len().max(1) as f64;
    eprintln!(
        "{mode:?}, {per} a presentation: {recorded} of {} frames shown keep a camera; {} primitives hold still over {alike} frame pairs seen alike; over {checked} frame pairs the camera turned {turned:.1} degrees and moved {travelled:.0} units; through each frame's camera the median still primitive moved at most {worst:.2} a frame, and of all {} distances {:.2}% are within {tolerance} (90th percentile {:.2}, 99th {:.2}); the median {stale:.1} through the frame before's ({} pairs)",
        shown.len(), still.len(), distances.len(), within * 100.0, quantile(&distances, 0.9), quantile(&distances, 0.99), through_previous.len()
    );
    assert!(checked > 20 && through_previous.len() > 10, "{mode:?}: the camera moved in {} of {checked} frame pairs sharing still scenery", through_previous.len());
    assert!(stale > tolerance * 4.0, "{mode:?}: through the frame before's camera the scenery moves only {stale} (the camera record would not matter)");
    assert!(within >= 0.95, "{mode:?}: only {:.2}% of the still primitives' distances are within {tolerance} world units", within * 100.0);
    if matches!(mode, Mode::Field | Mode::Worldmap) {
        assert!(turned > 30.0, "{mode:?}: L1 turned the camera {turned} degrees");
    }
    // A look-at camera puts the point it looks at on its axis, at its depth
    // (on the world map, its height above the map's zero is the record's
    // own: seen negated, it would lie off the axis).
    if !matches!(mode, Mode::Field) {
        let mut heights = (f32::MAX, f32::MIN);
        for camera in frames.iter().flatten().map(|(camera, _)| camera) {
            let target = camera.view_point(camera.target);
            assert!(target[0].abs() < 0.02 * target[2] && target[1].abs() < 0.02 * target[2], "{mode:?}: the camera looks at {:?}, seen at {target:?}", camera.target);
            assert!((target[2] - camera.focus).abs() <= 2.0, "{mode:?}: its depth {} against {}", target[2], camera.focus);
            heights = (heights.0.min(camera.target[1]), heights.1.max(camera.target[1]));
            let negated = camera.view_point([camera.target[0], -camera.target[1], camera.target[2]]);
            if matches!(mode, Mode::Worldmap) && camera.target[1] != 0.0 {
                assert!(negated[0].hypot(negated[1]) > 0.02 * negated[2], "{mode:?}: the target's height negated, {negated:?}, lies on the axis too");
            }
        }
        eprintln!("{mode:?}: the camera's target heights {heights:?}");
        if matches!(mode, Mode::Worldmap) {
            assert!(heights.0.abs().max(heights.1.abs()) > 50.0, "{mode:?}: target heights {heights:?} tell no sign");
        }
    }
}

#[test]
fn a_fields_scenery_stays_put_through_each_frames_camera_as_the_camera_turns() {
    scenery_stays_put_as_the_camera_turns(Mode::Field, 600, 1, 4.0);
}

#[test]
fn a_battles_stage_stays_put_through_each_frames_camera_as_the_camera_moves() {
    scenery_stays_put_as_the_camera_turns(Mode::Battle, 900, 1, 4.0);
}

#[test]
fn a_stage_drawn_over_the_last_frame_stays_put_through_each_frames_camera_as_the_camera_comes_in() {
    scenery_stays_put_as_the_camera_turns(Mode::Desert, 1500, 1, 4.0);
}

/// The desert battle draws a frame every vertical blank, into its two
/// buffers in turn, and shows the frame before the last: presented four at
/// a time, the buffer shown is drawn over twice a presentation.
#[test]
fn a_stage_drawn_over_twice_a_presentation_keeps_its_last_frames_camera() {
    scenery_stays_put_as_the_camera_turns(Mode::Desert, 1500, 4, 4.0);
}

#[test]
fn the_world_maps_terrain_stays_put_through_each_frames_camera_as_the_camera_turns() {
    scenery_stays_put_as_the_camera_turns(Mode::Worldmap, 600, 1, 4.0);
}

/// With a view from anywhere the world map's terrain lies all around its
/// camera's target, behind its camera too (the cells whose projection the
/// GTE flags behind or through its plane included): over the last frame
/// shown, the floor within 1 500 units of the target, the reach of a
/// diorama's table at 25%, is covered but for a few cells at most.
#[test]
fn the_world_maps_terrain_lies_all_around_its_target_for_a_view_from_anywhere() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut session = Mode::Worldmap.enter(&disc);
    let shown = present(&mut session, &gpu, 60, 1, |_| vec![]);
    let frame = shown.iter().rev().find(|f| f.camera.is_some_and(|c| c.mode == CameraMode::Worldmap)).expect("a world map frame");
    let camera = frame.camera.unwrap();
    // The floor in cells of 50 units across the target's surroundings.
    const CELL: f32 = 50.0;
    const REACH: f32 = 1500.0;
    let side = (2.0 * REACH / CELL) as usize;
    let mut covered = vec![false; side * side];
    for op in &frame.ops {
        let TraceOp::Draw(p) = op else { continue };
        if p.class != Class::World || p.backdrop {
            continue;
        }
        let Some(points) = (0..p.vertex_count()).map(|i| p.vertices[i].view.and_then(|v| camera.world_point(v))).collect::<Option<Vec<_>>>() else { continue };
        let triangles: &[[usize; 3]] = if points.len() == 4 { &[[0, 1, 2], [1, 3, 2]] } else if points.len() == 3 { &[[0, 1, 2]] } else { &[] };
        for t in triangles {
            let [a, b, c] = t.map(|i| (points[i][0], points[i][2]));
            let area = (b.0 - a.0) * (c.1 - a.1) - (c.0 - a.0) * (b.1 - a.1);
            if area.abs() < 1e-3 {
                continue;
            }
            let (x0, x1) = (a.0.min(b.0).min(c.0), a.0.max(b.0).max(c.0));
            let (z0, z1) = (a.1.min(b.1).min(c.1), a.1.max(b.1).max(c.1));
            let cells = |lo: f32, hi: f32| (((lo + REACH) / CELL).floor().max(0.0) as usize)..=(((hi + REACH) / CELL).floor().min(side as f32 - 1.0) as usize);
            for cz in cells(z0, z1) {
                for cx in cells(x0, x1) {
                    let (x, z) = (cx as f32 * CELL - REACH + CELL / 2.0, cz as f32 * CELL - REACH + CELL / 2.0);
                    let edge = |p: (f32, f32), q: (f32, f32)| ((q.0 - p.0) * (z - p.1) - (x - p.0) * (q.1 - p.1)) * area.signum() >= 0.0;
                    if edge(a, b) && edge(b, c) && edge(c, a) {
                        covered[cz * side + cx] = true;
                    }
                }
            }
        }
    }
    let within: Vec<usize> = (0..side * side).filter(|&i| {
        let (x, z) = ((i % side) as f32 * CELL - REACH + CELL / 2.0, (i / side) as f32 * CELL - REACH + CELL / 2.0);
        x.hypot(z) <= REACH
    }).collect();
    let bare = within.iter().filter(|&&i| !covered[i]).count();
    eprintln!("the world map's terrain within {REACH} units of its target: {} of {} cells of {CELL} covered", within.len() - bare, within.len());
    assert!(bare * 100 <= within.len(), "{bare} of {} cells bare", within.len());
}

/// The heights (y down) at which the world triangles a frame drew pass over
/// the world map's target, (0, y, 0) in its camera's world.
fn heights_over_target(ops: &[TraceOp], camera: &Camera) -> Vec<f32> {
    let mut heights = Vec::new();
    for op in ops {
        let TraceOp::Draw(p) = op else { continue };
        if p.class != Class::World || p.backdrop {
            continue;
        }
        let Some(points) = (0..p.vertex_count()).map(|i| p.vertices[i].view.and_then(|v| camera.world_point(v))).collect::<Option<Vec<_>>>() else { continue };
        let triangles: &[[usize; 3]] = if points.len() == 4 { &[[0, 1, 2], [1, 3, 2]] } else if points.len() == 3 { &[[0, 1, 2]] } else { &[] };
        for t in triangles {
            let [a, b, c] = t.map(|i| points[i]);
            let area = (b[0] - a[0]) * (c[2] - a[2]) - (c[0] - a[0]) * (b[2] - a[2]);
            if area.abs() < 1e-3 {
                continue;
            }
            // Barycentric weights of (0, 0) across x and z.
            let wb = ((0.0 - a[0]) * (c[2] - a[2]) - (c[0] - a[0]) * (0.0 - a[2])) / area;
            let wc = ((b[0] - a[0]) * (0.0 - a[2]) - (0.0 - a[0]) * (b[2] - a[2])) / area;
            if wb >= 0.0 && wc >= 0.0 && wb + wc <= 1.0 {
                heights.push(a[1] + wb * (b[1] - a[1]) + wc * (c[1] - a[1]));
            }
        }
    }
    heights
}

#[test]
fn the_world_maps_camera_carries_the_ground_under_its_target_in_flight_too() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut session = Mode::Worldmap.enter(&disc);
    // The vehicle boot mode 3 begins in, hovering, then driven on by its
    // throttle (Triangle, the control deck's THROTTLE): it climbs as it goes.
    let mut above = Vec::new();
    for (what, buttons, frames) in [("hovering", vec![], 30), ("driven on", vec!["triangle"], 200)] {
        let shown = present(&mut session, &gpu, frames, 1, |_| buttons.clone());
        let frame = shown.iter().rev().find(|f| f.camera.is_some_and(|c| c.mode == CameraMode::Worldmap)).expect("a world map frame");
        let camera = frame.camera.unwrap();
        let ground = camera.ground.expect("the world map's camera carries its ground");
        let target = camera.target[1];
        // The terrain the frame drew passes over the target at the ground's
        // height (within 4 units: the original's rounding and fixed point).
        let surface = heights_over_target(&frame.ops, &camera);
        eprintln!("{what}: the target at {target}, the ground under it at {ground}; the world triangles over it at {surface:?}");
        assert!(surface.iter().any(|h| (h - ground).abs() <= 4.0), "{what}: no terrain drawn at the ground {ground}: {surface:?}");
        above.push(ground - target);
    }
    assert!(above[0] > 50.0 && above[1] > above[0] + 100.0, "the vehicle above its ground: {above:?} units");
}

/// `battle_area` (battle/area.h): its slots (0x1C bytes each, `hidden` at
/// +3) and the slots' sprites (x, y and z in 16.16, `height` at +0x36).
const BATTLE_AREA: u32 = 0x800C_3EB0;
const AREA_SLOTS: u32 = BATTLE_AREA + 4;
const AREA_SPRITES: u32 = BATTLE_AREA + 0x8C8C;
/// The scene data's camera 0, the battle's start view (battle/scene.h).
const START_VIEW: u32 = 0x47C;

#[test]
fn a_battles_cameras_carry_its_stage_as_the_game_keeps_it() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut session = Mode::Battle.enter(&disc);
    // Cross now and then (its attacks), then nothing for two seconds: the
    // combatants back at their places, at rest.
    let shown = present(&mut session, &gpu, 420, 1, |n| if n < 300 { Mode::Battle.buttons(n) } else { vec![] });
    let cameras: Vec<Camera> = shown.iter().filter_map(|frame| frame.camera.filter(|c| c.mode == CameraMode::Battle)).collect();
    assert!(cameras.len() > 300 && cameras.iter().all(|c| c.stage.is_some()), "every battle camera carries its stage ({} cameras)", cameras.len());
    let scene = session.execute(&json!({"cmd": "inspect", "path": "mode_battle_scene_data"}));
    let data = u32::from_str_radix(scene.as_str().expect("a pointer").trim_start_matches("0x"), 16).unwrap();
    // Each of a few more frames: the stage the port recorded as the game
    // drew it, against RAM before and after the frame (at rest the places
    // stay; a sprite's frame extent, its height, changes within a frame as
    // its animation steps: measured, 90 as the battle draws, 96 before and
    // after).
    let read = |session: &mut Session<NativeModule>| {
        let memory = session.runtime.memory();
        let word = |address: u32| {
            let mut bytes = [0u8; 4];
            memory.read(address, &mut bytes).expect("RAM");
            u32::from_le_bytes(bytes)
        };
        let half = |address: u32| (word(address & !3) >> (8 * (address & 2))) as u16;
        let combatants: Vec<Option<([f32; 3], f32)>> = (0..11u32).map(|slot| {
            let sprite = word(AREA_SPRITES + slot * 4);
            let hidden = word(AREA_SLOTS + slot * 0x1C) >> 24 != 0;
            (sprite != 0 && !hidden).then(|| {
                let at = |offset: u32| (word(sprite + offset) as i32 >> 16) as f32;
                ([at(0), at(4), at(8)], f32::from(half(sprite + 0x36)))
            })
        }).collect();
        let point = |at: u32| [0, 2, 4].map(|k| f32::from(half(data + START_VIEW + at + k) as i16));
        (combatants, point(0), point(6))
    };
    for _ in 0..8 {
        let (before, look_at, eye) = read(&mut session);
        assert!(session.frame().expect("frame"));
        let trace = session.runtime.services().gpu.device_mut().take_trace();
        let (after, _, _) = read(&mut session);
        let stages: Vec<_> = trace.ops.iter().filter_map(|op| match op {
            TraceOp::Camera(c) if c.mode == CameraMode::Battle => Some(c.stage),
            _ => None,
        }).collect();
        assert_eq!(stages.len(), 1, "one battle camera a frame");
        let stage = stages[0].expect("the battle camera carries its stage");
        for (slot, recorded) in stage.combatants.iter().enumerate() {
            let recorded = recorded.map(|c| (c.position, c.height));
            assert_eq!(recorded.map(|(at, _)| at), before[slot].map(|(at, _)| at), "slot {slot}: where the battle keeps it");
            assert!(recorded.is_none_or(|(_, height)| [before[slot], after[slot]].iter().flatten().any(|(_, h)| (h - height).abs() <= h / 5.0)),
                "slot {slot}: {recorded:?} as tall as before ({:?}) or after ({:?}) its frame, within a fifth", before[slot], after[slot]);
        }
        assert_eq!((stage.look_at, stage.eye), (look_at, eye), "the scene data's start view");
        assert!(stage.turns, "its turns run");
    }
    let (combatants, look_at, eye) = read(&mut session);
    eprintln!("the debug battle's stage at rest: {combatants:?}; start view {look_at:?} from {eye:?}");
}

/// The game's state after `frames` frames from `state` with the trace
/// asking `reveals` of the port (no trace for `None`), with the buttons of
/// the tests above: the RAM hash, the GPU (VRAM), the SPU and the clock.
fn state_after(session: &mut Session<NativeModule>, state: &[u8], mode: Mode, frames: u64, reveals: Option<Reveals>) -> (u64, Vec<u8>, Vec<u8>, u64) {
    session.restore_bytes(state).unwrap();
    let device = session.runtime.services().gpu.device_mut();
    device.set_trace(reveals.is_some());
    device.set_reveals(reveals);
    let mut cameras = Vec::new();
    for n in 0..frames {
        session.execute(&json!({"cmd": "pad", "buttons": mode.buttons(n)}));
        assert!(session.frame().unwrap());
        let trace = session.runtime.services().gpu.device_mut().take_trace();
        cameras.extend(trace.ops.iter().filter_map(|op| match op {
            TraceOp::Camera(c) if c.mode == mode.camera() => Some(c.around),
            _ => None,
        }));
    }
    // Every frame the trace takes records its camera, with the bits the
    // host asked with (the first the game builds after they change, with
    // those before).
    let around = reveals.map_or(0, |r| r.around);
    assert_eq!(!cameras.is_empty(), reveals.is_some(), "{mode:?}: {} cameras recorded with around {around}", cameras.len());
    if reveals.is_some() {
        assert_eq!(cameras.last(), Some(&around), "{mode:?}: the cameras carry the host's bits");
    }
    let services = session.runtime.services();
    let state = (services.gpu.device().save(), services.spu.chip().save(), services.clock.now);
    (session.memory_hash().unwrap(), state.0, state.1, state.2)
}

#[test]
fn camera_records_leave_ram_vram_sound_and_time_unchanged() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    for mode in [Mode::Field, Mode::Battle, Mode::Desert, Mode::Worldmap] {
        let mut session = mode.enter(&disc);
        let state = session.snapshot_bytes().unwrap();
        let original = state_after(&mut session, &state, mode, 240, None);
        for around in [0, Reveals::LOOK, Reveals::ANYWHERE] {
            let traced = state_after(&mut session, &state, mode, 240, Some(Reveals { around, ..ANYWHERE }));
            assert_eq!(traced.0, original.0, "{mode:?}: around {around} changes the RAM");
            assert_eq!(traced, original, "{mode:?}: around {around} changes VRAM, sound or time");
            assert_eq!(session.scene().split(':').next(), Some(mode.scene()), "{mode:?}: the frames run in the mode");
        }
        eprintln!("{mode:?}: RAM hash {:016x} with around 0, 1 and 2 and without a trace", original.0);
    }
}
