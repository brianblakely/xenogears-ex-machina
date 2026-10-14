//! The cameras a view from anywhere asks the port for (docs/runtime.md,
//! Camera records), on the original game: with the culled-face ring's
//! `around` bit 2 (`Reveals::ANYWHERE`), the port records the camera each
//! frame is drawn with in a field (Lahan, map 1), a battle (the one the
//! debug battle selector starts as it is, and its desert, whose stage is
//! drawn over the last frame instead of a cleared buffer) and on the world
//! map, and each frame shown keeps the one its frame was drawn with.
//! Through the inverse of each frame's own camera the scenery that holds
//! still stays where it is while the game's camera turns or moves (through
//! the frame before's, it moves), and in a battle and on the world map the
//! camera looks at the target it records. The records change nothing of
//! the game: from the same state, the same frames end at the same RAM,
//! VRAM, sound and clock with `around` 0, 1 and 2 as without a trace.
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
/// trace asking for every face and the cameras), holding `buttons(n)` in
/// frame `n`; returns the frames shown, in order.
fn present(session: &mut Session<NativeModule>, gpu: &Gpu, frames: u64, buttons: impl Fn(u64) -> Vec<&'static str>) -> Vec<Shown> {
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    let mut shown: Vec<Shown> = Vec::new();
    let mut identity = None;
    for n in 0..frames {
        session.execute(&json!({"cmd": "pad", "buttons": buttons(n)}));
        assert!(session.frame().expect("frame"));
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

/// The median, over the primitives of `keys` two frames both drew, of the
/// farthest any of a primitive's points lies from where it was in the world
/// (the first frame's points through `a`, the second's through `b`), and
/// how many primitives that is.
fn moved(keys: &HashSet<Key>, first: &Points, second: &Points, a: &Camera, b: &Camera) -> (f32, usize) {
    let mut distances: Vec<f32> = keys
        .iter()
        .filter_map(|key| {
            let (points, others) = (first.get(key)?, second.get(key)?);
            points.iter().zip(others).try_fold(0f32, |far, (p, q)| Some(far.max(distance(a.world_point(*p)?, b.world_point(*q)?))))
        })
        .collect();
    distances.sort_by(f32::total_cmp);
    (distances.get(distances.len() / 2).copied().unwrap_or(f32::NAN), distances.len())
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
/// moves: less than `tolerance` world units (the median over the primitives
/// two frames share), against more than four times that through the frame
/// before's camera where it turned more than a degree or moved more than
/// that. A battle's and the world map's look-at camera has the target it
/// records on its axis, at its recorded depth.
fn scenery_stays_put_as_the_camera_turns(mode: Mode, frames: u64, tolerance: f32) {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut session = mode.enter(&disc);
    let shown = present(&mut session, &gpu, frames, |n| mode.buttons(n));
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
    for pair in frames.windows(2) {
        let [Some((a, first)), Some((b, second))] = pair else { continue };
        if same_view(a, b) {
            continue;
        }
        let (own, shared) = moved(&still, first, second, a, b);
        if shared < 30 {
            continue;
        }
        let (angle, travel) = (turn(a, b), distance(eye(a), eye(b)));
        let (stale, _) = moved(&still, first, second, a, a);
        assert!(own <= tolerance, "{mode:?}: the scenery moved {own} world units in a frame through each frame's camera (the camera turned {angle} degrees and moved {travel}; {shared} primitives)");
        (turned, travelled, checked, worst) = (turned + angle, travelled + travel, checked + 1, worst.max(own));
        if angle > 1.0 || travel > tolerance {
            through_previous.push(stale);
        }
    }
    through_previous.sort_by(f32::total_cmp);
    let stale = through_previous.get(through_previous.len() / 2).copied().unwrap_or(0.0);
    eprintln!(
        "{mode:?}: {recorded} of {} frames shown keep a camera; {} primitives hold still over {alike} frame pairs seen alike; over {checked} frame pairs the camera turned {turned:.1} degrees and moved {travelled:.0} units, the scenery at most {worst:.2} a frame through each frame's camera, a median {stale:.1} through the frame before's ({} pairs)",
        shown.len(), still.len(), through_previous.len()
    );
    assert!(checked > 20 && through_previous.len() > 10, "{mode:?}: the camera moved in {} of {checked} frame pairs sharing still scenery", through_previous.len());
    assert!(stale > tolerance * 4.0, "{mode:?}: through the frame before's camera the scenery moves only {stale} (the camera record would not matter)");
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
    scenery_stays_put_as_the_camera_turns(Mode::Field, 600, 4.0);
}

#[test]
fn a_battles_stage_stays_put_through_each_frames_camera_as_the_camera_moves() {
    scenery_stays_put_as_the_camera_turns(Mode::Battle, 900, 4.0);
}

#[test]
fn a_stage_drawn_over_the_last_frame_stays_put_through_each_frames_camera_as_the_camera_comes_in() {
    scenery_stays_put_as_the_camera_turns(Mode::Desert, 1500, 4.0);
}

#[test]
fn the_world_maps_terrain_stays_put_through_each_frames_camera_as_the_camera_turns() {
    scenery_stays_put_as_the_camera_turns(Mode::Worldmap, 600, 4.0);
}

/// The game's state after `frames` frames from `state` with the trace
/// asking `reveals` of the port (no trace for `None`), with the buttons of
/// the tests above: the RAM hash, the GPU (VRAM), the SPU and the clock.
fn state_after(session: &mut Session<NativeModule>, state: &[u8], mode: Mode, frames: u64, reveals: Option<Reveals>) -> (u64, Vec<u8>, Vec<u8>, u64) {
    session.restore_bytes(state).unwrap();
    let device = session.runtime.services().gpu.device_mut();
    device.set_trace(reveals.is_some());
    device.set_reveals(reveals);
    let mut cameras = 0;
    for n in 0..frames {
        session.execute(&json!({"cmd": "pad", "buttons": mode.buttons(n)}));
        assert!(session.frame().unwrap());
        let trace = session.runtime.services().gpu.device_mut().take_trace();
        cameras += trace.ops.iter().filter(|op| matches!(op, TraceOp::Camera(c) if c.mode == mode.camera())).count();
    }
    // A field records its camera for any view from within or around it;
    // battles and the world map only for one from anywhere.
    let around = reveals.map_or(0, |r| r.around);
    let recording = around & Reveals::ANYWHERE != 0 || (matches!(mode, Mode::Field) && around != 0);
    assert_eq!(cameras > 0, recording, "{mode:?}: {cameras} cameras recorded with around {around}");
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
