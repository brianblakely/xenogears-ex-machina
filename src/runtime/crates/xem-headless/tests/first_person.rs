//! First-person inspection qualified on the original game as a camera mode
//! (docs/runtime.md, First-person inspection and Headset qualification;
//! local only: it needs the user's disc 1 and a game module build; the GPU
//! is whatever wgpu finds, lavapipe in the runtime shell). A harness plays a
//! host's part, as `inspection.rs` and `diorama.rs` do: the session told
//! before each frame whether the view shows and the view the field's
//! characters keep moving in, the game's trace pushed and each vertical
//! blank presented, flat (the desktop's look: `GameView::present`) or to a
//! headset's two eyes (`GameView::present_headset`). Beside it, from the
//! same trace: the game camera's flat frame, that frame without what the
//! game draws before its scene, and the same look from within with each of
//! the scene's primitives drawn plain in a colour of its own (which one
//! shows at a pixel).
//!
//! - Holes: in fields 1, 12, 22 and 30, from the controlled character's eye
//!   turned to eight headings at two pitches (level and 30 degrees down),
//!   flat and in a headset's eyes, the walkmesh's floors in view (every
//!   actor layer's, as `diorama.rs` takes them) show the scene: fewer than
//!   0.5% of their pixels show none, in all four fields (measured: none);
//!   what shows none lies above them (ceilings and skies the game never
//!   models), measured, not bounded.
//! - The interface: where the game draws it opaque after its scene, the
//!   flat look shows it as the game camera's flat frame does, and in a
//!   headset all of it is on the pane in that colour, nothing on the pane
//!   unlike the flat frame.
//! - The game's order in a plane: in fields 1, 30 and 12 from sixteen
//!   looks, flat and in a headset, wherever two primitives the look draws
//!   meet in one plane the picture shows the one the game drew last (a
//!   sprite's parts, split sprites, faces drawn over others: none lost); a
//!   face the game draws from both sides (twice, its corners in another
//!   order) is counted apart. Sprites the game draws reaching below their
//!   floor points stand on them; in a headset both eyes see each sprite as
//!   one billboard.
//! - Transitions in a headset: map 25's story movie begun while inspecting
//!   shows on the screen with no eyes and the view goes on after it until
//!   map 23 ends it; the Mountain Path's random battle entered while
//!   inspecting (the host's pad walking the character) keeps the eyes and
//!   the pane while it loads, and its shatter, the eye's picture broken up,
//!   ends inspection on the screen.
//!
//! `XEM_FIRST_PERSON_PICTURES=<dir>` saves the pictures looked at (local
//! review pictures of original game assets: keep them local).
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test first_person

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::collections::{BTreeMap, HashMap};
use std::path::{Path, PathBuf};
use xem_core::control::Session;
use xem_core::field::{Field, V3};
use xem_game::{NativeModule, boot};
use xem_render::Gpu;
use xem_render::glam::{Quat, Vec2, Vec3};
use xem_render::ps1::{AUX_INTERFACE, AUX_MODEL, AUX_SPRITE, AUX_WORLD, GameView, HeadsetEye, HostClass, Look, ViewConfig, effective_class, first_person, scene_offset};
use xem_render::xem_gpu::{Camera, Class, DisplayInfo, Trace, TraceOp};
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

/// The output a host's view is configured for.
const OUTPUT: (u32, u32) = (640, 480);
/// A headset eye's field of view (tangents left, right, up, down).
const FOV: [f32; 4] = [-1.0, 1.0, 0.75, -0.75];
/// The headings the view is turned to from the one it began with, and the
/// pitches (degrees: level, and down).
const YAWS: [f32; 8] = [0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0];
const PITCHES: [f32; 2] = [0.0, -30.0];

/// A head's two eyes, 64 mm apart, turned `yaw` degrees to the right and
/// `pitch` up.
fn head(yaw: f32, pitch: f32) -> [HeadsetEye; 2] {
    let turn = Quat::from_rotation_y(-yaw.to_radians()) * Quat::from_rotation_x(pitch.to_radians());
    let right = turn * Vec3::X;
    [-0.032f32, 0.032].map(|x| HeadsetEye { orientation: turn.to_array(), position: (right * x).to_array(), fov: FOV })
}

/// What a presentation showed.
#[derive(Clone, Debug)]
struct Shown {
    /// Vertical blanks run when it presented, and the session's scene.
    blank: u64,
    scene: String,
    /// The game's mode (1 a field, 2 a battle).
    mode: u64,
    /// First-person inspection on, and looking from within the scene (the
    /// flat picture's look, or a headset's eyes).
    on: bool,
    within: bool,
    /// The screen's picture: its size, and whether a movie frame rebuilt
    /// from what the MDEC decoded; or the device's own picture.
    flat: Option<(u32, u32)>,
    movie: bool,
    device: bool,
}

/// A picture a look from within the scene drew: its colours and classes
/// (`AUX_*` in blue), its size, the look, and where it lies in VRAM (its
/// top left corner's VRAM point and its pixels a VRAM pixel).
struct Seen {
    picture: Vec<u8>,
    classes: Vec<u8>,
    size: (u32, u32),
    look: Look,
    origin: (f32, f32),
    scale: f32,
}

impl Seen {
    /// A point of the game camera's view space in the look's camera space
    /// (right, down, forward).
    fn camera(&self, v: [f32; 3]) -> Vec3 {
        let d = [v[0] - self.look.eye[0], v[1] - self.look.eye[1], v[2] - self.look.eye[2]];
        Vec3::from(self.look.axes.map(|axis| axis[0] * d[0] + axis[1] * d[1] + axis[2] * d[2]))
    }

    /// Where a point of the look's camera space in front of its near plane
    /// shows in the picture, projected about `centre` (VRAM pixels), as
    /// `frame::first_person` projects it.
    fn pixel(&self, centre: (f32, f32), m: Vec3) -> Vec2 {
        let x = centre.0 + self.look.shift[0] + self.look.h[0] * m.x / m.z;
        let y = centre.1 + self.look.shift[1] + self.look.h[1] * m.y / m.z;
        Vec2::new((x - self.origin.0) * self.scale, (y - self.origin.1) * self.scale)
    }

    fn class(&self, i: usize) -> u32 {
        u32::from(self.classes[i * 4 + 2])
    }

    fn save(&self, name: &str) {
        let Some(directory) = std::env::var_os("XEM_FIRST_PERSON_PICTURES") else { return };
        let opaque: Vec<u8> = self.picture.chunks(4).flat_map(|p| [p[0], p[1], p[2], 255]).collect();
        save_png(&PathBuf::from(directory).join(format!("{name}.png")), self.size, &opaque);
    }
}

fn save_png(path: &Path, size: (u32, u32), pixels: &[u8]) {
    let mut encoder = png::Encoder::new(std::fs::File::create(path).unwrap(), size.0, size.1);
    encoder.set_color(png::ColorType::Rgba);
    encoder.write_header().unwrap().write_image_data(pixels).unwrap();
}

/// A host on the game: the player's view, flat or a headset's, and beside
/// it from the same trace the game camera's flat frame (`camera`), that
/// frame of the trace without what the game draws before its scene
/// (`after`: the interface drawn after it) and the player's look of the
/// trace with each scene primitive in a colour of its own (`ids`).
struct Host {
    session: Session<NativeModule>,
    gpu: Gpu,
    headset: bool,
    view: GameView,
    camera: GameView,
    after: GameView,
    ids: GameView,
    next_id: u32,
    eyes: [HeadsetEye; 2],
    /// The pad the harness holds (the host's `Session::set_pad`).
    pad: u16,
    blanks: u64,
    /// The views beside the player's are presented too.
    beside: bool,
    /// The display the last presentation showed.
    display: Option<DisplayInfo>,
}

impl Host {
    fn new(disc: &Path, headset: bool) -> Host {
        let (session, _) = boot(disc, &Default::default()).expect("boot");
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let [view, camera, after, ids] = [(); 4].map(|_| GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter)));
        Host { session, gpu, headset, view, camera, after, ids, next_id: 0, eyes: head(0.0, 0.0), pad: 0, blanks: 0, beside: true, display: None }
    }

    fn config(&self) -> ViewConfig {
        let class = if self.headset { HostClass::Headset } else { HostClass::Desktop };
        ViewConfig::resolve(&Graphics::default(), class, OUTPUT, self.view.max_scale())
    }

    /// First-person inspection on or off (the player's view and its
    /// identified twin alike).
    fn inspect(&mut self, on: bool) {
        self.view.set_first_person(on);
        self.ids.set_first_person(on);
    }

    /// The look turned `yaw` degrees right of the heading it began with and
    /// `pitch` up: a flat host's view turned so, a headset's head.
    fn look(&mut self, yaw: f32, pitch: f32) {
        if self.headset {
            self.eyes = head(yaw, pitch);
            return;
        }
        for view in [&mut self.view, &mut self.ids] {
            if let Some((y, p)) = view.look_angles() {
                view.look_around(yaw.to_radians() - y, pitch.to_radians() - p);
            }
        }
    }

    /// Runs one vertical blank and presents it.
    fn step(&mut self) -> Shown {
        let config = self.config();
        for view in [&mut self.view, &mut self.camera, &mut self.after, &mut self.ids] {
            view.configure(config);
        }
        self.session.set_pad(0, self.pad);
        self.session.set_inspecting(self.view.first_person());
        self.session.set_actor_view(self.view.actor_view()).expect("the view's actor view");
        assert!(self.session.frame().expect("frame"), "a native disc never blocks");
        self.blanks += 1;
        let device = self.session.runtime.services().gpu.device_mut();
        let mut restart = false;
        for view in [&mut self.view, &mut self.camera, &mut self.after, &mut self.ids] {
            restart |= view.take_restart();
        }
        if restart || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(self.view.reveals());
        let trace = device.take_trace();
        if self.beside {
            self.camera.push(trace.clone());
            self.after.push(after_scene(&trace));
            self.ids.push(identified(&trace, &mut self.next_id));
        }
        self.view.push(trace);
        self.present()
    }

    fn steps(&mut self, count: u64) -> Vec<Shown> {
        (0..count).map(|_| self.step()).collect()
    }

    /// Presents the newest frame (as each host frame does, the game running
    /// or not).
    fn present(&mut self) -> Shown {
        let display = self.session.runtime.services().gpu.device().display();
        self.display = Some(display);
        let scene = self.session.scene();
        let mode = u64::from(self.session.runtime.services().mode.unwrap_or(0));
        let now = self.blanks as f64 / 60.0;
        let mut shown = Shown { blank: self.blanks, scene: scene.clone(), mode, on: self.view.first_person(), within: false, flat: None, movie: false, device: false };
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        if !(GameView::shows(&display) || self.view.shows_movie(&display)) {
            // The device's own picture (a 24-bit movie frame the view does
            // not rebuild).
            for view in [&mut self.view, &mut self.camera, &mut self.after, &mut self.ids] {
                view.away();
            }
            shown.device = true;
        } else {
            let beside = self.beside;
            if self.headset {
                for view in [&mut self.view, &mut self.ids].into_iter().take(if beside { 2 } else { 1 }) {
                    let presented = view.present_headset(&mut encoder, &display, &scene, now, &self.eyes);
                    if let Some(presented) = presented.filter(|_| shown.flat.is_none()) {
                        shown.within = !presented.eyes.is_empty();
                        shown.flat = Some(presented.screen.size);
                        shown.movie = presented.screen.movie;
                    }
                }
            } else {
                for view in [&mut self.view, &mut self.ids].into_iter().take(if beside { 2 } else { 1 }) {
                    let presented = view.present(&mut encoder, &display, &scene, now).map(|p| (p.size, p.movie));
                    if let Some((size, movie)) = presented.filter(|_| shown.flat.is_none()) {
                        shown.flat = Some(size);
                        shown.movie = movie;
                        shown.within = view.screen_look().is_some();
                    }
                }
            }
            for view in [&mut self.camera, &mut self.after].into_iter().filter(|_| beside) {
                let _ = view.present(&mut encoder, &display, &scene, now);
            }
        }
        self.gpu.queue.submit([encoder.finish()]);
        shown.on = self.view.first_person();
        shown
    }

    fn read(&self, texture: &xem_render::wgpu::Texture) -> Vec<u8> {
        xem_render::read_rgba8(&self.gpu.device, &self.gpu.queue, texture)
    }

    /// What `view` (the player's, or its identified twin) last drew from
    /// within the scene: the flat picture, or each eye's.
    fn seen(&self, view: &GameView) -> Vec<Seen> {
        let display = self.display.expect("a presentation");
        let side = view.frame().side() as f32;
        let place = |size: (u32, u32)| ((f32::from(display.x) - side, f32::from(display.y)), size.0 as f32 / (display.width as f32 + 2.0 * side));
        if !self.headset {
            let Some(look) = view.screen_look() else { return Vec::new() };
            let texture = view.presented_texture().expect("a picture");
            let size = (texture.width(), texture.height());
            let (origin, scale) = place(size);
            return vec![Seen { picture: self.read(texture), classes: self.read(view.frame().aux()), size, look, origin, scale }];
        }
        (0..2).filter_map(|eye| {
            let (look, aux) = view.eye_look(eye)?;
            let texture = view.eye_texture(eye)?;
            let size = (texture.width(), texture.height());
            let (origin, scale) = place(size);
            Some(Seen { picture: self.read(texture), classes: self.read(aux), size, look, origin, scale })
        }).collect()
    }

    /// The flat frame `view` (the game camera's) last presented: its
    /// colours and classes.
    fn flat(&self, view: &GameView) -> (Vec<u8>, Vec<u8>) {
        (self.read(view.presented_texture().expect("a flat frame")), self.read(view.frame().aux()))
    }

    fn context(&mut self) -> String {
        run(&mut self.session, json!({"cmd": "context"}))["kind"].as_str().unwrap_or_default().to_string()
    }
}

/// `trace` without the pictures a frame draws before its scene and those it
/// projects behind it (as `diorama.rs` takes it): its flat frame shows the
/// interface drawn after the scene over the scene and the game's clear.
fn after_scene(trace: &Trace) -> Trace {
    let first = trace.ops.iter().position(|op| matches!(op, TraceOp::Draw(p) if p.class.is_scene() && !p.backdrop));
    let ops = trace.ops.iter().enumerate()
        .filter(|(i, op)| !matches!(op, TraceOp::Draw(p) if p.backdrop || (!p.class.is_scene() && first.is_some_and(|first| *i < first))))
        .map(|(_, op)| op.clone())
        .collect();
    Trace { ops, focus: trace.focus }
}

/// `trace` with each scene primitive (and each part of a sprite's
/// directional alternative) drawn untextured and opaque in a colour of its
/// own, numbered on from `next` (0xRRGGBB: never 0): which primitive shows
/// at a pixel, in the game's order and places.
fn identified(trace: &Trace, next: &mut u32) -> Trace {
    let mut id = || {
        *next = *next % 0xFF_FFFE + 1;
        *next
    };
    let ops = trace.ops.iter().map(|op| match op {
        TraceOp::Draw(p) if p.class.is_scene() => {
            let mut p = p.clone();
            (p.textured, p.semi, p.dither) = (false, false, false);
            let colour = id();
            for v in &mut p.vertices {
                v.color = colour;
            }
            if let Some(facing) = &p.facing {
                let mut facing = (**facing).clone();
                for part in &mut facing.parts {
                    // Its colour, neither raw nor semi-transparent.
                    part.words[0] = id();
                }
                p.facing = Some(std::sync::Arc::new(facing));
            }
            TraceOp::Draw(p)
        }
        op => op.clone(),
    }).collect();
    Trace { ops, focus: trace.focus }
}

/// The colour of a picture's pixel `i`, 0xBBGGRR.
fn rgb(picture: &[u8], i: usize) -> u32 {
    u32::from(picture[i * 4]) | u32::from(picture[i * 4 + 1]) << 8 | u32::from(picture[i * 4 + 2]) << 16
}

/// A world point of the field (whole units, y down) in the game camera's
/// view space through its record (view = axes · world / 4096 + translation).
fn view_point(camera: &Camera, w: V3) -> [f32; 3] {
    std::array::from_fn(|i| (camera.axes[0][i] * w.x + camera.axes[1][i] * w.y + camera.axes[2][i] * w.z) / 4096.0 + camera.translation[i])
}

/// Where the field's world is projected about in VRAM (as
/// `frame::first_person` takes a world primitive's centre): its projection
/// offset plus the drawing offset.
fn world_centre(ops: &[TraceOp]) -> Option<(f32, f32)> {
    let scene = scene_offset(ops)?;
    ops.iter().find_map(|op| match op {
        TraceOp::Draw(p) if p.class == Class::World && p.projection.is_some_and(|(offset, _)| offset == scene) => {
            Some((f32::from(scene.0) + f32::from(p.environment.offset.0), f32::from(scene.1) + f32::from(p.environment.offset.1)))
        }
        _ => None,
    })
}

/// `polygon` cut by the plane `axis` = `bound`, keeping its part at most
/// `bound` (`below`) or at least `bound` (as `diorama.rs`).
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

/// `mask` less its edge: each pixel kept with its four neighbours in it.
fn inner(mask: &[bool], (width, height): (usize, usize)) -> Vec<bool> {
    let at = |x: usize, y: usize| mask[y * width + x];
    (0..width * height).map(|i| {
        let (x, y) = (i % width, i / width);
        x > 0 && y > 0 && x + 1 < width && y + 1 < height && at(x, y) && at(x - 1, y) && at(x + 1, y) && at(x, y - 1) && at(x, y + 1)
    }).collect()
}

/// Whether triangle `t` of walkmesh layer `layer` is a floor (as
/// `diorama.rs` takes it: its attribute word lacks bit 0x800000, a wall on
/// layer 0 and a pit above it to the game's floor search).
fn floor(field: &Field, layer: usize, t: usize) -> bool {
    let index = field.layers[layer].triangles[t].attribute as usize;
    field.attributes.get(index).copied().unwrap_or(0) & 0x80_0000 == 0
}

/// `triangles` of walkmesh layer `layer` in `seen`'s picture, in front of
/// its near plane, through the frame's camera record and the world's
/// projection centre, a pixel in from its edges.
fn footprint(field: &Field, layer: usize, triangles: &[usize], camera: &Camera, centre: (f32, f32), seen: &Seen) -> Vec<bool> {
    let size = (seen.size.0 as usize, seen.size.1 as usize);
    let mut mask = vec![false; size.0 * size.1];
    for &t in triangles {
        let corners: Vec<Vec3> = field.layers[layer].corners(t).iter().map(|&c| seen.camera(view_point(camera, c))).collect();
        let polygon = clip(&corners, 2, seen.look.near, false);
        if polygon.len() < 3 {
            continue;
        }
        let pixels: Vec<Vec2> = polygon.iter().map(|&m| seen.pixel(centre, m)).collect();
        fill(&mut mask, size, &pixels);
    }
    inner(&mask, size)
}

/// Whether a class (`AUX_*`) is the scene's.
fn scene_class(class: u32) -> bool {
    [AUX_WORLD, AUX_MODEL, AUX_SPRITE].contains(&class)
}

/// The pixels of a walkmesh footprint over the views that show no scene.
#[derive(Default)]
struct Holes {
    total: usize,
    missing: usize,
    /// The fewest footprint pixels in a view.
    least: usize,
    views: Vec<String>,
}

impl Holes {
    /// Adds a view's footprint `mask` over its `holes` (pixels showing no scene).
    fn add(&mut self, view: &str, mask: &[bool], holes: &[bool]) {
        let covered = mask.iter().filter(|&&m| m).count();
        let missing = mask.iter().zip(holes).filter(|&(&m, &h)| m && h).count();
        self.least = if self.views.is_empty() { covered } else { self.least.min(covered) };
        self.views.push(format!("{view}: {missing} of {covered}"));
        self.total += covered;
        self.missing += missing;
    }

    fn met(&self) -> bool {
        self.missing * 200 < self.total
    }

    fn summary(&self) -> String {
        format!("{} of {} pixels without the scene ({:.3}%)", self.missing, self.total, 100.0 * self.missing as f64 / self.total.max(1) as f64)
    }
}

/// Looking from within each field, flat and in a headset: the walkmesh's
/// floors in view where the picture shows no scene (the room the game never
/// modelled), how much of each whole picture shows none, and whether the
/// interface stays the flat frame's.
#[test]
fn from_within_each_field_the_walkmesh_in_view_shows_the_scene_and_the_interface_the_flat_frames() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut results = Vec::new();
    for map in [1u64, 12, 22, 30] {
        for headset in [false, true] {
            let host_name = if headset { "headset" } else { "flat" };
            let mut host = Host::new(&disc, headset);
            run(&mut host.session, json!({"cmd": "enter_field", "map": map}));
            run(&mut host.session, json!({"cmd": "frames", "count": 400}));
            host.inspect(true);
            let shown = host.steps(30);
            assert!(shown.last().is_some_and(|s| s.on && s.within), "map {map}, {host_name}: looking from within ({:?})", shown.last());
            let field = Field::read(host.session.runtime.memory()).expect("a field");
            let player = *field.player().expect("the controlled character");
            let layers: Vec<(Vec<usize>, Vec<usize>)> = (0..field.layers.len() - 1).map(|layer| {
                let floors: Vec<usize> = (0..field.layers[layer].triangles.len()).filter(|&t| floor(&field, layer, t)).collect();
                let walkable = floors.iter().copied().filter(|&t| !field.blocked(layer, t, &player)).collect();
                (floors, walkable)
            }).collect();
            let camera = host.view.frame().camera().expect("the field camera's record");
            let centre = world_centre(host.view.frame().newest()).expect("the field's world");
            // The interface the game draws after its scene, in the game
            // camera's frame (a flat look keeps it where the game drew it).
            let (after_colours, after_classes) = host.flat(&host.after);
            let (flat_colours, _) = host.flat(&host.camera);
            let after_interface: Vec<bool> = (0..after_classes.len() / 4).map(|i| u32::from(after_classes[i * 4 + 2]) == AUX_INTERFACE).collect();
            let (mut walkmesh, mut walkable) = (Holes::default(), Holes::default());
            let mut by_layer: Vec<Holes> = layers.iter().map(|_| Holes::default()).collect();
            let (mut empty, mut pixels) = (0usize, 0usize);
            let mut empty_views = Vec::new();
            let mut interface = None;
            for pitch in PITCHES {
                for yaw in YAWS {
                    host.look(yaw, pitch);
                    host.present();
                    let seen = host.seen(&host.view);
                    assert_eq!(seen.len(), if headset { 2 } else { 1 }, "map {map}, {host_name}: looking from within at {yaw}/{pitch}");
                    for (eye, seen) in seen.iter().enumerate() {
                        let count = (seen.size.0 * seen.size.1) as usize;
                        // No scene: neither the world, a model nor a sprite;
                        // on the flat picture the interface drawn after the
                        // scene stands over it where the game drew it.
                        let holes: Vec<bool> = (0..count).map(|i| !scene_class(seen.class(i)) && !(!headset && after_interface[i])).collect();
                        let view = if headset { format!("{yaw:.0}/{pitch:.0}/{eye}") } else { format!("{yaw:.0}/{pitch:.0}") };
                        let (mut floors, mut walks) = (vec![false; count], vec![false; count]);
                        for (layer, (all, standable)) in layers.iter().enumerate() {
                            let mask = footprint(&field, layer, all, &camera, centre, seen);
                            by_layer[layer].add(&view, &mask, &holes);
                            floors.iter_mut().zip(&mask).for_each(|(a, &m)| *a |= m);
                            let mask = footprint(&field, layer, standable, &camera, centre, seen);
                            walks.iter_mut().zip(&mask).for_each(|(a, &m)| *a |= m);
                        }
                        walkmesh.add(&view, &floors, &holes);
                        walkable.add(&view, &walks, &holes);
                        let none = holes.iter().filter(|&&h| h).count();
                        empty += none;
                        pixels += count;
                        empty_views.push(format!("{view}: {:.1}%", 100.0 * none as f64 / count as f64));
                        seen.save(&format!("holes-{map}-{host_name}-{yaw:.0}-{pitch:.0}-{eye}"));
                        if let Some(directory) = std::env::var_os("XEM_FIRST_PERSON_PICTURES") {
                            // The floors where the scene shows (green) and where
                            // it does not (red), the rest of what shows no scene
                            // blue.
                            let mut picture = seen.picture.clone();
                            for i in 0..count {
                                let p = &mut picture[i * 4..i * 4 + 4];
                                match (floors[i], holes[i]) {
                                    (true, true) => p.copy_from_slice(&[255, 0, 0, 255]),
                                    (true, false) => (p[1], p[3]) = (p[1].saturating_add(60), 255),
                                    (false, true) => p.copy_from_slice(&[0, 0, 160, 255]),
                                    (false, false) => p[3] = 255,
                                }
                            }
                            save_png(&PathBuf::from(&directory).join(format!("floors-{map}-{host_name}-{yaw:.0}-{pitch:.0}-{eye}.png")), seen.size, &picture);
                        }
                    }
                    if yaw == 0.0 && pitch == 0.0 {
                        // The interface: on the flat picture, where the game
                        // drew it opaque after its scene, the flat frame's
                        // colour; in a headset all of it on the pane in its
                        // colour, and nothing else there.
                        let opaque: Vec<usize> = (0..after_interface.len()).filter(|&i| after_interface[i] && after_classes[i * 4 + 3] != 255).collect();
                        let level = |a: &[u8], b: &[u8], i: usize| (0..3).map(|c| a[i * 4 + c].abs_diff(b[i * 4 + c])).max().unwrap_or(0);
                        interface = Some(if headset {
                            let pane = host.read(host.view.presented_texture().expect("the pane"));
                            let on_pane = opaque.iter().filter(|&&i| pane[i * 4 + 3] == 255 && level(&pane, &after_colours, i) == 0).count();
                            let unlike = (0..pane.len() / 4).filter(|&i| pane[i * 4 + 3] == 255 && level(&pane, &flat_colours, i) > 0).count();
                            let covered = (0..pane.len() / 4).filter(|&i| pane[i * 4 + 3] > 0).count();
                            (opaque.len(), on_pane, unlike, covered)
                        } else {
                            let seen = &seen[0];
                            let same = opaque.iter().filter(|&&i| level(&seen.picture, &flat_colours, i) == 0 && seen.class(i) == AUX_INTERFACE).count();
                            (opaque.len(), same, 0, 0)
                        });
                    }
                }
            }
            let layer_lines: Vec<String> = by_layer.iter().enumerate().map(|(layer, holes)| {
                format!("layer {layer} ({} floors, {} the character's): {}", layers[layer].0.len(), layers[layer].1.len(), holes.summary())
            }).collect();
            let (opaque, kept, unlike, covered) = interface.expect("the interface looked at");
            eprintln!("map {map}, {host_name} (the character on layer {}): the walkmesh's floors {}; the character's {}; by layer: {}; \
                no scene in {:.2}% of the pictures ({}); the interface: {opaque} opaque pixels drawn after the scene, {kept} of them as the flat frame shows them, \
                {unlike} opaque pane pixels unlike the flat frame, {covered} pane pixels covered; by view, the floors: {}",
                player.layer, walkmesh.summary(), walkable.summary(), layer_lines.join("; "), 100.0 * empty as f64 / pixels as f64,
                empty_views.join(", "), walkmesh.views.join(", "));
            results.push((map, headset, walkmesh, walkable, (opaque, kept, unlike)));
        }
    }
    for (map, headset, walkmesh, walkable, (opaque, kept, unlike)) in results {
        let host = if headset { "headset" } else { "flat" };
        // Level, a view may show no floor (a wall or a door close by).
        assert!(walkmesh.total >= 100_000, "map {map}, {host}: floor in view ({} pixels)", walkmesh.total);
        // Under 0.5% in all four fields, flat and in a headset (measured: no
        // pixel at all): from the character's eye the floors show the
        // scene; what shows none is above them (docs/runtime.md,
        // First-person inspection). Should that change, so must they.
        assert!(walkmesh.met(), "map {map}, {host}: {} of the walkmesh's floors show no scene: {}", walkmesh.summary(), walkmesh.views.join(", "));
        assert!(walkable.met(), "map {map}, {host}: {} of the character's floors show no scene", walkable.summary());
        assert!(opaque >= 200, "map {map}, {host}: the interface drawn after the scene ({opaque} pixels)");
        assert_eq!(kept, opaque, "map {map}, {host}: the interface as the flat frame draws it");
        assert_eq!(unlike, 0, "map {map}, {host}: nothing on the pane unlike the flat frame");
    }
}

/// A scene primitive a look from within moved into its camera: its place
/// in the game's order, number, class and owner, and its corners in the
/// picture with the reciprocals of their depths.
struct Drawn {
    id: u32,
    class: Class,
    corners: Vec<(Vec2, f32)>,
    /// Its texture coordinates at its corners.
    uv: Vec<(i16, i16)>,
}

impl Drawn {
    /// Whether `other` is this face drawn again from its other side (its
    /// corners in another order, a pixel apart at most), and with the same
    /// texture coordinates at each corner.
    fn other_side(&self, other: &Drawn) -> Option<bool> {
        if self.corners.len() != other.corners.len() {
            return None;
        }
        let mut textured = true;
        for (i, (p, _)) in self.corners.iter().enumerate() {
            let j = other.corners.iter().position(|(q, _)| (*q - *p).length() < 1.0)?;
            textured &= self.uv[i] == other.uv[j];
        }
        Some(textured)
    }
}

impl Drawn {
    /// The reciprocal depth at picture point `p`, where `p` lies in it at
    /// least `margin` pixels in from its outline.
    fn at(&self, p: Vec2, margin: f32) -> Option<f32> {
        let outline: &[usize] = if self.corners.len() == 4 { &[0, 1, 3, 2] } else { &[0, 1, 2] };
        let n = outline.len();
        let point = |i: usize| self.corners[outline[i]].0;
        let area: f32 = (0..n).map(|i| point(i).perp_dot(point((i + 1) % n))).sum();
        if area.abs() < 1e-3 {
            return None;
        }
        for i in 0..n {
            let (a, b) = (point(i), point((i + 1) % n));
            let edge = b - a;
            let distance = edge.perp_dot(p - a) * area.signum() / edge.length().max(1e-6);
            if distance < margin {
                return None;
            }
        }
        // The triangle of the two (0 1 2, 1 2 3) holding it, the reciprocal
        // depth linear across the picture in its plane.
        let triangles: &[[usize; 3]] = if self.corners.len() == 4 { &[[0, 1, 2], [1, 3, 2]] } else { &[[0, 1, 2]] };
        triangles.iter().find_map(|&[a, b, c]| {
            let (pa, pb, pc) = (self.corners[a].0, self.corners[b].0, self.corners[c].0);
            let d = (pb - pa).perp_dot(pc - pa);
            if d.abs() < 1e-6 {
                return None;
            }
            let u = (p - pa).perp_dot(pc - pa) / d;
            let v = (pb - pa).perp_dot(p - pa) / d;
            (u >= -1e-4 && v >= -1e-4 && u + v <= 1.0 + 1e-4).then(|| self.corners[a].1 * (1.0 - u - v) + self.corners[b].1 * u + self.corners[c].1 * v)
        })
    }
}

/// The scene primitives `seen`'s look moves into its camera from a frame's
/// `ops` (the identified trace's), in the game's order (a directional
/// sprite's alternative parts in theirs), but those reaching behind its
/// near plane.
fn drawn(ops: &[TraceOp], seen: &Seen) -> Vec<Drawn> {
    let mut ops = ops.to_vec();
    first_person(&mut ops, &seen.look);
    let scene = scene_offset(&ops);
    ops.iter().filter_map(|op| {
        let TraceOp::Draw(p) = op else { return None };
        if !effective_class(p, scene).is_scene() || p.projection.is_none() || p.shape == xem_render::xem_gpu::Shape::Line {
            return None;
        }
        let n = p.vertex_count();
        let corners = p.vertices[..n].iter().map(|v| {
            let m = v.view?;
            (m[2] >= seen.look.near * 1.001).then(|| (Vec2::new((v.fx - seen.origin.0) * seen.scale, (v.fy - seen.origin.1) * seen.scale), 1.0 / m[2]))
        }).collect::<Option<Vec<_>>>()?;
        Some(Drawn { id: p.vertices[0].color & 0xFF_FFFF, class: p.class, corners, uv: p.vertices[..n].iter().map(|v| (v.u, v.v)).collect() })
    }).collect()
}

/// How a look from within keeps the game's order where its depth cannot
/// tell primitives apart: each picture pixel inside two primitives in one
/// plane (their depths there within a ten-thousandth) shows the one the
/// game drew last; counted by the classes of the pair (earlier, later).
#[derive(Default)]
struct Order {
    /// Pixels where primitives in one plane overlap, the game's last shown,
    /// and where an earlier one showed instead, by classes.
    kept: BTreeMap<(String, String), usize>,
    lost: BTreeMap<(String, String), usize>,
    /// Pixels where one face the game draws from both sides (twice, its
    /// corners in another order) shows its first drawing over its second,
    /// and of them where the two sides' texture coordinates differ.
    sides: usize,
    unlike_sides: usize,
    /// Pixels where a nearer primitive than the one shown covers it.
    deeper: usize,
}

impl Order {
    /// Adds a picture's pixels (`ids`: the identified look's), returning
    /// where the order was lost (the classes of the pair there).
    fn add(&mut self, seen: &Seen, ids: &[u8], drawn: &[Drawn]) -> Vec<Option<(Class, Class)>> {
        const TIE: f32 = 1e-4;
        let index: HashMap<u32, usize> = drawn.iter().enumerate().map(|(i, d)| (d.id, i)).collect();
        let (width, height) = (seen.size.0 as usize, seen.size.1 as usize);
        let class = |c: Class| format!("{c:?}");
        // The pixels each primitive covers well inside its outline, the
        // primitive shown there and whether a later one in its plane, or a
        // nearer one, covers it too.
        let mut verdict: Vec<Option<(usize, usize, bool)>> = vec![None; width * height];
        for (j, later) in drawn.iter().enumerate() {
            let (lo, hi) = later.corners.iter().fold((Vec2::MAX, Vec2::MIN), |(lo, hi), (p, _)| (lo.min(*p), hi.max(*p)));
            let (x0, x1) = (lo.x.floor().max(0.0) as usize, (hi.x.ceil().max(0.0) as usize).min(width));
            let (y0, y1) = (lo.y.floor().max(0.0) as usize, (hi.y.ceil().max(0.0) as usize).min(height));
            for y in y0..y1 {
                for x in x0..x1 {
                    let i = y * width + x;
                    let p = Vec2::new(x as f32 + 0.5, y as f32 + 0.5);
                    let Some(&k) = index.get(&rgb(ids, i)) else { continue };
                    if k == j {
                        continue;
                    }
                    let Some(zj) = later.at(p, 1.5) else { continue };
                    let Some(zk) = drawn[k].at(p, 0.5) else { continue };
                    // Reciprocal depths: larger is nearer. In one plane where
                    // they meet a few pixels about too (two faces crossing meet
                    // along a line).
                    let tie = (zj - zk).abs() <= TIE * zk && {
                        let around = [Vec2::new(4.0, 0.0), Vec2::new(-4.0, 0.0), Vec2::new(0.0, 4.0), Vec2::new(0.0, -4.0)];
                        let met: Vec<bool> = around.iter().filter_map(|d| Some((later.at(p + *d, 0.0)?, drawn[k].at(p + *d, 0.0)?))).map(|(a, b)| (a - b).abs() <= TIE * b).collect();
                        met.len() >= 2 && met.iter().all(|&m| m)
                    };
                    if tie && j > k && let Some(textured) = drawn[k].other_side(later) {
                        // The same face drawn from its other side: either shows it.
                        if verdict[i].is_none() {
                            self.sides += 1;
                            self.unlike_sides += usize::from(!textured);
                        }
                        continue;
                    }
                    if tie && j > k {
                        // A later primitive in the shown one's plane: the game's
                        // order lost at this pixel.
                        verdict[i] = Some((k, j, false));
                    } else if !tie && zj > zk {
                        self.deeper += 1;
                    } else if tie && verdict[i].is_none() {
                        // An earlier one in its plane: the game's last shows.
                        verdict[i] = Some((j, k, true));
                    }
                }
            }
        }
        for &(earlier, later, kept) in verdict.iter().flatten() {
            let pair = (class(drawn[earlier].class), class(drawn[later].class));
            *(if kept { &mut self.kept } else { &mut self.lost }).entry(pair).or_default() += 1;
        }
        verdict.iter().map(|v| v.filter(|&(_, _, kept)| !kept).map(|(earlier, later, _)| (drawn[earlier].class, drawn[later].class))).collect()
    }

    fn summary(&self) -> String {
        let pairs = |counts: &BTreeMap<(String, String), usize>| counts.iter().map(|((a, b), n)| format!("{a}/{b} {n}")).collect::<Vec<_>>().join(", ");
        format!("kept at {} pixels ({}), lost at {} ({}); a face drawn from both sides showing its first drawing at {} ({} with the sides' texture coordinates unlike); {} pixels with a nearer primitive than the one shown",
            self.kept.values().sum::<usize>(), pairs(&self.kept), self.lost.values().sum::<usize>(), pairs(&self.lost), self.sides, self.unlike_sides, self.deeper)
    }
}

/// From within fields 1, 30 and 12, flat and in a headset: where primitives
/// lie in one plane (a sprite's parts, faces the game draws over others in
/// their plane) the picture shows the one the game drew last, as the PS1
/// does whatever their depths; the sprites the game draws reaching below
/// their floor points (over the floor) stand on them; and in a headset both
/// eyes see each sprite as one billboard, its parts in one plane.
#[test]
fn from_within_the_games_order_holds_in_a_plane_sprites_stand_on_the_floor_and_both_eyes_see_one_billboard() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut results = Vec::new();
    for map in [1u64, 30, 12] {
        for headset in [false, true] {
            let host_name = if headset { "headset" } else { "flat" };
            let mut host = Host::new(&disc, headset);
            run(&mut host.session, json!({"cmd": "enter_field", "map": map}));
            run(&mut host.session, json!({"cmd": "frames", "count": 400}));
            if map == 30 {
                // Fei walked into the room, his townspeople round the table.
                run(&mut host.session, json!({"cmd": "press", "buttons": ["down"], "frames": 60}));
                run(&mut host.session, json!({"cmd": "frames", "count": 30}));
            }
            host.inspect(true);
            assert!(host.steps(30).last().is_some_and(|s| s.within), "map {map}, {host_name}: looking from within");
            let (_, reach) = sprite_reach(host.ids.frame().newest());
            let below: Vec<f32> = reach.values().map(|&(below, _)| below).filter(|&below| below > 0.0).collect();
            let shares: Vec<f32> = reach.values().filter(|&&(below, _)| below > 0.0).map(|&(below, above)| below / (below - above)).collect();
            let mut order = Order::default();
            let (mut sprites, mut thickest, mut apart) = (0usize, 0f32, 0f32);
            // The lowest of each sprite's parts from within, above its floor
            // point (view-space units along the world's up), against what its
            // parts' lifts over one another may move it (a part moves towards
            // the eye by a hundred-thousandth of its depth a part below it:
            // within two ten-thousandths of the distance, and half a unit).
            let mut lowest: BTreeMap<u32, (f32, f32)> = BTreeMap::new();
            for pitch in PITCHES {
                for yaw in YAWS {
                    host.look(yaw, pitch);
                    host.present();
                    let seen = host.seen(&host.view);
                    let ids = host.seen(&host.ids);
                    assert_eq!(ids.len(), seen.len(), "map {map}, {host_name}: the identified look as the player's");
                    let ops = host.ids.frame().newest().to_vec();
                    let mut eyes = Vec::new();
                    for (eye, (seen, ids)) in seen.iter().zip(&ids).enumerate() {
                        assert_eq!(seen.look, ids.look, "map {map}, {host_name}: the identified look is the player's");
                        let drawn = drawn(&ops, ids);
                        let lost = order.add(ids, &ids.picture, &drawn);
                        if let Some(directory) = std::env::var_os("XEM_FIRST_PERSON_PICTURES") {
                            // Where the order was lost: magenta between a
                            // sprite's parts, cyan between models' faces,
                            // yellow otherwise.
                            let mut picture = seen.picture.clone();
                            for (i, pair) in lost.iter().enumerate() {
                                let colour = match pair {
                                    None => continue,
                                    Some((Class::Sprite, Class::Sprite)) => [255, 0, 255, 255],
                                    Some((Class::Model, Class::Model)) => [0, 255, 255, 255],
                                    Some(_) => [255, 255, 0, 255],
                                };
                                picture[i * 4..i * 4 + 4].copy_from_slice(&colour);
                            }
                            picture.chunks_mut(4).for_each(|p| p[3] = 255);
                            save_png(&PathBuf::from(&directory).join(format!("lost-{map}-{host_name}-{yaw:.0}-{pitch:.0}-{eye}.png")), seen.size, &picture);
                        }
                        ids.save(&format!("ids-{map}-{host_name}-{yaw:.0}-{pitch:.0}-{eye}"));
                        seen.save(&format!("order-{map}-{host_name}-{yaw:.0}-{pitch:.0}-{eye}"));
                        eyes.push(sprite_points(&ops, &seen.look));
                    }
                    let up = Vec3::from(seen[0].look.up);
                    for points in &eyes {
                        for (owner, (parts, ground)) in points {
                            sprites += 1;
                            thickest = thickest.max(thickness(parts));
                            if let Some(ground) = ground {
                                let eye = Vec3::from(seen[0].look.eye);
                                for p in parts.iter().flatten() {
                                    let (height, within) = ((*p - *ground).dot(up), 0.5 + 2e-4 * (*p - eye).length());
                                    let at = lowest.entry(*owner).or_insert((f32::MAX, 0.0));
                                    if height < at.0 {
                                        *at = (height, within);
                                    }
                                }
                            }
                        }
                    }
                    if let [left, right] = eyes.as_slice() {
                        for (owner, (parts, _)) in left {
                            if let Some((other, _)) = right.get(owner) {
                                for (a, b) in parts.iter().flatten().zip(other.iter().flatten()) {
                                    apart = apart.max((*a - *b).length());
                                }
                            }
                        }
                    }
                }
            }
            // Of the sprites raised, those whose lowest part stands on the
            // floor point (some may show only their upper parts, the rest
            // behind the eye), and the lowest part of all.
            let raised: Vec<(f32, f32)> = lowest.iter().filter(|(owner, _)| reach.get(owner).is_some_and(|&(below, _)| below > 0.01)).map(|(_, &h)| h).collect();
            let on_floor = raised.iter().filter(|(h, within)| h.abs() < *within).count() as f32 / raised.len().max(1) as f32;
            // How far below its floor point the lowest part of any sprite
            // lies, as a share of what the lifts may move it (under 1: none
            // below it).
            let above = lowest.values().map(|&(h, within)| -h / within).fold(f32::MIN, f32::max);
            eprintln!("map {map}, {host_name}: the game's order in a plane {}; {} of {} sprites drawn reaching below their floor points (by {:.0} to {:.0} view units, {:.0}% to {:.0}% of their height), \
                from within {:.0}% of them on their floor points, the lowest part of any sprite {above:.2} of its lifts' reach below its floor point; \
                {sprites} sprites seen, their parts at most {thickest} view units off one plane{}",
                order.summary(), below.len(), reach.len(), below.iter().copied().fold(f32::MAX, f32::min), below.iter().copied().fold(0.0, f32::max),
                100.0 * shares.iter().copied().fold(f32::MAX, f32::min), 100.0 * shares.iter().copied().fold(0.0, f32::max), 100.0 * on_floor,
                if headset { format!(", both eyes' sprites at most {apart} apart") } else { String::new() });
            results.push((map, headset, order, thickest, apart, on_floor, above, below.len()));
        }
    }
    for (map, headset, order, thickest, apart, on_floor, above, raised) in results {
        let host = if headset { "headset" } else { "flat" };
        assert!(order.kept.get(&("Sprite".into(), "Sprite".into())).is_some_and(|&n| n > 1000), "map {map}, {host}: a sprite's parts overlap in their plane: {}", order.summary());
        assert!(order.lost.is_empty(), "map {map}, {host}: the game's order in a plane: {}", order.summary());
        // The game draws most sprites reaching below their floor points;
        // from within they stand on them (their lowest part there, within
        // the lift), no part of any sprite below its floor point.
        assert!(raised > 0, "map {map}, {host}: sprites drawn reaching below their floor points");
        assert!(on_floor >= 0.9 && above < 1.0, "map {map}, {host}: sprites stand on their floor points ({:.0}% of those raised; the lowest part {above} of its lifts' reach below)", 100.0 * on_floor);
        // One billboard a sprite (its parts lifted over one another by a
        // hundred-thousandth of their depth: about a unit 10 000 away), the
        // same for both eyes.
        assert!(thickest < 2.0, "map {map}, {host}: a sprite's parts {thickest} view units off one plane");
        assert!(apart < 0.05, "map {map}, {host}: both eyes see one billboard ({apart} apart)");
    }
}

/// Each sprite of `ops` (by owner): its floor point in the game camera's
/// view space (taken back through the sprite's projection), and how far its
/// parts reach below it and above it (view-space units along the camera's
/// y, as `frame::first_person` lays them along the world's up).
fn sprite_reach(ops: &[TraceOp]) -> (BTreeMap<u32, [f32; 3]>, BTreeMap<u32, (f32, f32)>) {
    let mut grounds: BTreeMap<u32, [f32; 3]> = BTreeMap::new();
    for op in ops {
        let TraceOp::Draw(p) = op else { continue };
        if p.class != Class::Sprite {
            continue;
        }
        if let (Some([x, y, z]), Some(((ox, oy), h))) = (p.ground, p.projection) {
            let centre = (f32::from(ox) + f32::from(p.environment.offset.0), f32::from(oy) + f32::from(p.environment.offset.1));
            let h = f32::from(h).max(1.0);
            grounds.insert(p.owner, [(x - centre.0) * z / h, (y - centre.1) * z / h, z]);
        }
    }
    let mut reach: BTreeMap<u32, (f32, f32)> = BTreeMap::new();
    for op in ops {
        let TraceOp::Draw(p) = op else { continue };
        let Some(ground) = grounds.get(&p.owner).filter(|_| p.class == Class::Sprite) else { continue };
        for v in &p.vertices[..p.vertex_count()] {
            let Some(q) = v.view else { continue };
            let down = q[1] - ground[1];
            let (below, above) = reach.entry(p.owner).or_insert((f32::MIN, f32::MAX));
            *below = below.max(down);
            *above = above.min(down);
        }
    }
    (grounds, reach)
}

/// Each sprite's parts (by owner) a look from within `ops` draws, their
/// corners back in the game camera's view space, and the floor point it
/// stands them at (its ground's place in the look taken back there).
fn sprite_points(ops: &[TraceOp], look: &Look) -> BTreeMap<u32, (Vec<Vec<Vec3>>, Option<Vec3>)> {
    let mut ops = ops.to_vec();
    first_person(&mut ops, look);
    let back = |m: [f32; 3]| Vec3::from(std::array::from_fn(|i| look.eye[i] + m[0] * look.axes[0][i] + m[1] * look.axes[1][i] + m[2] * look.axes[2][i]));
    let mut sprites: BTreeMap<u32, (Vec<Vec<Vec3>>, Option<Vec3>)> = BTreeMap::new();
    for op in &ops {
        let TraceOp::Draw(p) = op else { continue };
        let Some(((ox, oy), _)) = p.projection.filter(|_| p.class == Class::Sprite) else { continue };
        let points = p.vertices[..p.vertex_count()].iter().filter_map(|v| v.view).map(back).collect();
        let sprite = sprites.entry(p.owner).or_default();
        sprite.0.push(points);
        if let Some([x, y, z]) = p.ground {
            let centre = (f32::from(ox) + f32::from(p.environment.offset.0), f32::from(oy) + f32::from(p.environment.offset.1));
            sprite.1 = Some(back([(x - centre.0 - look.shift[0]) * z / look.h[0], (y - centre.1 - look.shift[1]) * z / look.h[1], z]));
        }
    }
    sprites
}

/// How far a sprite's parts' corners stand off the plane of its first part
/// (view-space units).
fn thickness(parts: &[Vec<Vec3>]) -> f32 {
    let Some(first) = parts.iter().find(|p| p.len() >= 3) else { return 0.0 };
    let normal = (first[1] - first[0]).cross(first[2] - first[0]);
    if normal.length() < 1e-6 {
        return 0.0;
    }
    let normal = normal.normalize();
    parts.iter().flatten().map(|p| (*p - first[0]).dot(normal).abs()).fold(0.0, f32::max)
}

/// A timeline of `shown` for the test's output: each change of scene and of
/// what shows.
fn timeline(shown: &[Shown]) -> String {
    let mut lines = Vec::new();
    let key = |s: &Shown| (s.scene.clone(), s.mode, s.on, s.within, s.movie, s.device, s.flat);
    let mut last = None;
    for s in shown {
        let k = key(s);
        if last.as_ref() != Some(&k) {
            lines.push(format!("{} {} mode={} on={} within={} movie={} device={} flat={:?}", s.blank, s.scene, s.mode, s.on, s.within, s.movie, s.device, s.flat));
            last = Some(k);
        }
    }
    lines.join("\n")
}

/// The field movie: stopped and requested (text_and_movies.rs).
const MOVIE_STOPPED: u32 = 0x800A_DB6C;
const MOVIE_REQUESTED: u32 = 0x800A_DB70;

fn word(session: &mut Session<NativeModule>, address: u32) -> u32 {
    let mut bytes = [0; 4];
    session.runtime.memory().read(address, &mut bytes).expect("game memory");
    u32::from_le_bytes(bytes)
}

/// Inspection on in map 25's scene before its story movie (#7), in a
/// headset: the movie on the screen with no eyes, inspection on through
/// it; after it the eyes look from within again in map 25, until map 23's
/// first frame ends the view.
#[test]
fn a_story_movie_begun_while_inspecting_shows_on_the_screen_and_the_view_goes_on_after_it() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // Map 25's scene, its dialogue auto-advanced to game frame 5080 (as
    // xr-game-smoke.sh's state of it), the movie about 350 frames on.
    let mut host = Host::new(&disc, true);
    host.beside = false;
    run(&mut host.session, json!({"cmd": "enter_field", "map": 25}));
    host.session.set_auto_advance(1).expect("a dwell");
    run(&mut host.session, json!({"cmd": "frames", "count": 5080}));
    host.session.set_auto_advance(0).expect("off");
    host.inspect(true);
    let mut shown = host.steps(60);
    assert!(shown.last().is_some_and(|s| s.within), "looking from within before the movie: {}", timeline(&shown));
    let mut entered = None;
    let mut began = None;
    for _ in 0..7000 {
        shown.push(host.step());
        if began.is_none() && word(&mut host.session, MOVIE_REQUESTED) != 0 && word(&mut host.session, MOVIE_STOPPED) == 0 {
            began = Some(host.blanks);
        }
        if entered.is_none() && shown.last().is_some_and(|s| s.scene == "field:23") {
            entered = Some(host.blanks);
        }
        if entered.is_some_and(|at| host.blanks >= at + 120) {
            break;
        }
    }
    eprintln!("map 25 inspected, its movie requested at {began:?}, map 23 from {entered:?}:\n{}", timeline(&shown));
    let (began, entered) = (began.expect("map 25's movie begins"), entered.expect("map 23 comes"));
    let rebuilt: Vec<&Shown> = shown.iter().filter(|s| s.movie).collect();
    let (first, last) = (rebuilt.first().expect("the movie's frames rebuilt").blank, rebuilt.last().expect("rebuilt").blank);
    assert!(shown.iter().filter(|s| s.blank >= 60 && s.blank < began).all(|s| s.within), "from within before the movie");
    assert!(rebuilt.len() > 4000 && rebuilt.iter().all(|s| s.on && !s.within), "the movie on the screen with no eyes, inspection on: {} frames", rebuilt.len());
    assert!(shown.iter().filter(|s| s.blank > first && s.blank < last).all(|s| s.movie || s.device), "the movie's frames throughout");
    let after: Vec<&Shown> = shown.iter().filter(|s| s.blank > last + 2 && s.blank < entered).collect();
    assert!(after.len() > 60 && after.iter().all(|s| s.on && s.within && s.scene == "field:25"), "after the movie, from within in map 25 again: {}", timeline(&shown));
    assert!(shown.iter().filter(|s| s.blank > entered).all(|s| !s.on && !s.within), "map 23 ends the view");
}

/// The Mountain Path's random battle entered while inspecting in a
/// headset (the character walked by the host's pad, which inspection does
/// not hold back): while the battle loads behind the field's last frame the
/// eyes keep looking from within and the pane holds the interface alone;
/// the battle's first frame (its shatter) ends inspection, on the screen,
/// its pieces the eye's picture (the screen the game takes is what the
/// player looked at), then the battle on the screen.
#[test]
fn a_random_battle_entered_while_inspecting_holds_the_eyes_while_it_loads_and_shatters_on_the_screen() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut host = Host::new(&disc, true);
    host.beside = false;
    run(&mut host.session, json!({"cmd": "enter_field", "map": 15, "entry": 0}));
    let mut shown = Vec::new();
    // Until the player walks (the map's scripts may speak first).
    for _ in 0..300 {
        shown.extend(host.steps(10));
        match host.context().as_str() {
            "walk" => break,
            "dialogue" => {
                run(&mut host.session, json!({"cmd": "act", "actions": ["Confirm"], "frames": 2}));
            }
            _ => {}
        }
    }
    // Once the field shows (it fades in as the player gains control).
    shown.extend(host.steps(120));
    host.inspect(true);
    shown.extend(host.steps(60));
    assert!(shown.last().is_some_and(|s| s.within), "looking from within on the Mountain Path: {}", timeline(&shown));
    // Up held until the random battle comes.
    host.pad = xem_core::pad::button::UP;
    let mut came = None;
    let mut panes = Vec::new();
    // The left eye's last picture, and how the screen's first after
    // inspection ends follows it (the correlation of their pixels'
    // brightness: the battle's flash brightens it as it begins).
    let mut eye = Vec::new();
    let mut taken = None;
    for _ in 0..4000 {
        let s = host.step();
        let mode = s.mode;
        shown.push(s);
        if came.is_none() && mode == 2 {
            came = Some(host.blanks);
            host.pad = 0;
        }
        if let Some(at) = came
            && let Some(directory) = std::env::var_os("XEM_FIRST_PERSON_PICTURES")
            && [10, 40, 50, 60, 80, 120].contains(&(host.blanks - at))
        {
            // Local review pictures: the screen (or the pane) and the left eye.
            for (what, texture) in [("screen", host.view.presented_texture()), ("eye", host.view.eye_texture(0))] {
                if let Some(texture) = texture {
                    let pixels: Vec<u8> = host.read(texture).chunks(4).flat_map(|p| [p[0], p[1], p[2], 255]).collect();
                    save_png(&PathBuf::from(&directory).join(format!("battle-{}-{what}.png", host.blanks - at)), (texture.width(), texture.height()), &pixels);
                }
            }
        }
        let within = shown.last().is_some_and(|s| s.within);
        if within {
            eye = host.view.eye_texture(0).map(|t| host.read(t)).unwrap_or_default();
        } else if came.is_some() && taken.is_none() {
            let screen = host.view.presented_texture().map(|t| host.read(t)).unwrap_or_default();
            taken = Some(correlation(&screen, &eye));
        }
        if came.is_some() {
            // The pane's cover through the entry.
            let pane = host.view.presented_texture().map(|t| host.read(t)).unwrap_or_default();
            let covered = pane.chunks_exact(4).filter(|p| p[3] > 0).count() as f32 / (pane.len() / 4).max(1) as f32;
            panes.push((host.blanks, within, covered));
        }
        if came.is_some_and(|at| host.blanks >= at + 400) {
            break;
        }
    }
    let lines: Vec<String> = panes.iter().step_by(5).map(|(b, w, c)| format!("{b}:{}:{c:.3}", u8::from(*w))).collect();
    eprintln!("field 15 inspected, its random battle from {came:?}:\n{}\npanes (blank:within:covered): {}\nthe screen's first picture after inspection ends follows the eye's last at {taken:?}",
        timeline(&shown), lines.join(" "));
    let came = came.expect("the random battle comes");
    assert!(shown.iter().filter(|s| s.blank >= 300 && s.blank < came).all(|s| s.within && s.mode == 1), "from within while walking the Mountain Path");
    // While the battle loads behind the field's last frame: the eyes, and
    // on the pane the interface alone (the compass: a few percent of it).
    let loading: Vec<&(u64, bool, f32)> = panes.iter().take_while(|(_, within, _)| *within).collect();
    assert!(loading.len() >= 20 && loading.iter().all(|(_, _, covered)| *covered < 0.2), "the eyes and the pane while the battle loads: {}", lines.join(" "));
    // The battle's first frame ends inspection: the screen, the battle's,
    // its shatter the eye's picture broken up.
    let ended = came + loading.len() as u64;
    assert!(shown.iter().filter(|s| s.blank >= ended).all(|s| !s.on && !s.within && s.mode == 2 && s.flat.is_some()), "the battle on the screen: {}", timeline(&shown));
    assert!(taken.is_some_and(|alike| alike > 0.8), "the shatter's first picture the eye's last ({taken:?})");
}

/// The correlation of two pictures' pixels' brightness (pictures of one
/// size, sRGB-encoded RGBA).
fn correlation(a: &[u8], b: &[u8]) -> f32 {
    let luma = |p: &[u8]| 0.299 * f64::from(p[0]) + 0.587 * f64::from(p[1]) + 0.114 * f64::from(p[2]);
    let pairs: Vec<(f64, f64)> = a.chunks_exact(4).zip(b.chunks_exact(4)).map(|(p, q)| (luma(p), luma(q))).collect();
    let n = pairs.len().max(1) as f64;
    let (ma, mb) = (pairs.iter().map(|p| p.0).sum::<f64>() / n, pairs.iter().map(|p| p.1).sum::<f64>() / n);
    let cov: f64 = pairs.iter().map(|(x, y)| (x - ma) * (y - mb)).sum();
    let (va, vb): (f64, f64) = (pairs.iter().map(|(x, _)| (x - ma).powi(2)).sum(), pairs.iter().map(|(_, y)| (y - mb).powi(2)).sum());
    (cov / (va * vb).sqrt().max(1e-9)) as f32
}
