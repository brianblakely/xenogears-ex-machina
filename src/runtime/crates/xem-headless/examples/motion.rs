//! Motion and cost check (local only: it needs the user's disc 1 and the
//! game module; the frames are pictures of the original game and stay in
//! build/validation/motion, which is ignored).
//!
//!     cargo run --release -p xem-headless --example motion -- [--map N | --worldmap | --battle [--gears] [--win]
//!         | --title | --script LOG | --state FILE] [--from-boot] [--skip N] [--load-at N]
//!         [--hold right,cross] [--tap BUTTON,FRAMES] [--press BUTTON,FRAME] [--frames 120] [--present 2]
//!         [--ff FRAMES] [--original FROM,TO]
//!         [--profile modern|hd2d|ps1] [--only effect,...] [--widescreen] [--zoom PERCENT] [--interpolate]
//!         [--scale N] [--size WxH] [--every N] [--crop x,y,w,h] [--compare] [--bench] [--no-trace]
//!         [--probe x,y] [--original-actors] [--cards DIR] [--look YAW,PITCH[,TURN]]
//!         [--diorama YAW,PITCH,DIST[,SCALE[,FOV]]] [--turns smooth|snap|fixed]
//!         [--name NAME] [--out DIR]
//!
//! `--win` ends the battle in victory first (the enemies' HP set to 1, then
//! the confirm button until the results module runs), for the result
//! screens. `--title` starts at boot (`--cards`: a directory of memory
//! card images to insert); `--state` from a saved state (a
//! host's state.xemstate); `--script` plays a session log (its pads, not
//! `--hold`); `--from-boot` traces and presents every frame of the
//! setup too, as a host does from boot, and `--skip` presents that many
//! frames before the ones kept; `--load-at` starts at the title screen and
//! loads a state of the field `--map` at that frame. From the scene, the game runs `frames` frames holding the given
//! buttons (and tapping one every so many frames, through menus and result
//! screens, over a script's pads too; `--press` presses one at a kept
//! frame, for two frames); each game frame is presented `present` times at evenly spaced
//! host times through the hosts' GameView. `--ff` runs that many game
//! frames for each presented one, as a host's fast-forward does;
//! `--original` shows those kept frames with the Original renderer (no
//! trace) and then this view again, as a host switched to it and back. Written: every `every`th
//! presented frame (cropped when asked; with `--compare` beside the
//! console's own picture of it, scaled up) in a contact sheet `NAME-sheet.png`,
//! the last 240 as an animated PNG `NAME.png`, the last one and the
//! console's own picture of it (`NAME-last.png`, `NAME-console.png`), and
//! `NAME.json` with the mean and worst cost per stage (the game frame, which
//! includes the port's presentation records and the trace's matching;
//! taking and pushing the trace; the presentation's CPU work; the GPU's)
//! and, per presented frame, the share of pixels that changed since the one
//! before (a still camera over still scenery changes only where something
//! moves), and per game frame how many faces the game culled were drawn
//! (`revealed`; `shown_revealed`: in the frame shown), and whether a
//! 24-bit (movie) display was rebuilt from the MDEC's macroblocks (`movies`:
//! 1, else 0 for the device's own picture; -1 for any other display).
//! `--bench` reads nothing back and writes only the costs;
//! `--no-trace` runs the game alone, presenting nothing (its cost without
//! any presentation work). `--probe` lists the last frame's primitives
//! covering a point of the original screen (class, projection, depth,
//! owner). The field's characters keep moving in the wider or zoomed-out
//! view as in the hosts (`Session::set_actor_view`), unless
//! `--original-actors`; the JSON counts the sprite actors the field stops
//! as off screen per frame (`still`). `--look` shows the field in
//! first-person inspection, turned YAW degrees to the right and PITCH up
//! from the camera's heading (and TURN degrees more each kept frame), and
//! reports where the controlled character stood (`eye`). `--diorama` shows
//! the scene as the headsets' diorama (docs/runtime.md, Diorama) through a
//! flat review camera: from the default seat's table, YAW degrees round it
//! from the seat's side (positive to the right), PITCH up from the table's
//! top, DIST metres from its centre, at the player's scale SCALE (1 the
//! default; `--turns` how the table turns) with a vertical field of view
//! of FOV degrees (50), the interface pane where a headset shows it and,
//! while the view falls back to the screen, the flat screen 2.5 m ahead;
//! it reports what each build drew (`diorama`) and writes the last pane
//! (`NAME-pane.png`). From the default seat the table's centre lies 0.75 m
//! away, 36.9 degrees down: `--diorama 0,36.87,0.75,1,100` is roughly what
//! a seated player sees.

/// A build without the game module has nothing to run.
#[cfg(not(has_game_module))]
fn main() {
    eprintln!("this build has no game module (python3 tools/game_module.py builds it)");
    std::process::exit(1);
}

#[cfg(has_game_module)]
use serde_json::{Value, json};
#[cfg(has_game_module)]
use std::path::PathBuf;
#[cfg(has_game_module)]
use std::time::Instant;
#[cfg(has_game_module)]
use xem_render::ps1::{GameView, HostClass, ViewConfig};
#[cfg(has_game_module)]
use xem_render::{Gpu, read_rgba8};
#[cfg(has_game_module)]
use xem_settings::{Graphics, GraphicsProfile};


#[cfg(has_game_module)]
struct Stage {
    name: &'static str,
    samples: Vec<f64>,
}

#[cfg(has_game_module)]
impl Stage {
    fn new(name: &'static str) -> Self {
        Self { name, samples: Vec::new() }
    }

    fn report(&self) -> Value {
        let mean = self.samples.iter().sum::<f64>() / self.samples.len().max(1) as f64;
        let mut sorted = self.samples.clone();
        sorted.sort_by(f64::total_cmp);
        let p95 = sorted.get(sorted.len() * 95 / 100).copied().unwrap_or(0.0);
        let worst = sorted.last().copied().unwrap_or(0.0);
        json!({"mean_ms": mean, "p95_ms": p95, "worst_ms": worst})
    }
}

/// The animated PNG keeps the last this many presented frames.
#[cfg(has_game_module)]
const ANIMATED: usize = 240;

/// `--diorama`'s flat review camera: its picture, and the compositor that
/// lays the pane (and the screen, while the view falls back to it) over it.
#[cfg(has_game_module)]
struct Review {
    target: xem_render::wgpu::Texture,
    compositor: xem_render::Compositor,
    eye: (xem_render::glam::Mat4, xem_render::glam::Mat4),
    /// The screen's share and what the diorama drew, per presentation, and
    /// the last pane drawn.
    screens: Vec<f32>,
    builds: Vec<Value>,
    pane: Option<xem_render::wgpu::Texture>,
}

#[cfg(has_game_module)]
impl Review {
    /// A review camera YAW degrees round the default table from the seat's
    /// side, PITCH up from its top, DIST metres from its centre, looking at
    /// that centre with a vertical field of view of FOV degrees, for
    /// pictures of `size`.
    fn new(gpu: &Gpu, [yaw, pitch, distance, _, fov]: [f32; 5], table: &xem_render::ps1::Table, size: (u32, u32)) -> Review {
        use xem_render::glam::{Mat4, Vec3};
        let target = gpu.device.create_texture(&xem_render::wgpu::TextureDescriptor {
            label: Some("review"),
            size: xem_render::wgpu::Extent3d { width: size.0, height: size.1, depth_or_array_layers: 1 },
            mip_level_count: 1,
            sample_count: 1,
            dimension: xem_render::wgpu::TextureDimension::D2,
            format: xem_render::wgpu::TextureFormat::Rgba8Unorm,
            usage: xem_render::wgpu::TextureUsages::RENDER_ATTACHMENT | xem_render::wgpu::TextureUsages::COPY_SRC | xem_render::wgpu::TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        });
        let centre = table.room_from_table(Mat4::IDENTITY).transform_point3(Vec3::ZERO);
        let (yaw, pitch) = (yaw.to_radians(), pitch.to_radians());
        let at = centre + Vec3::new(yaw.sin() * pitch.cos(), pitch.sin(), yaw.cos() * pitch.cos()) * distance;
        let eye = (Mat4::look_at_rh(at, centre, Vec3::Y), Mat4::perspective_rh(fov.to_radians(), size.0 as f32 / size.1 as f32, 0.01, 50.0));
        let compositor = xem_render::Compositor::new(&gpu.device, &gpu.queue, xem_render::wgpu::TextureFormat::Rgba8Unorm);
        Review { target, compositor, eye, screens: Vec::new(), builds: Vec::new(), pane: None }
    }

    /// Presents the view's diorama (or its screen) through the camera.
    fn present(&mut self, gpu: &Gpu, view: &mut GameView, display: &xem_render::xem_gpu::DisplayInfo, scene: &str, now: f64) -> Option<(u32, u32)> {
        use xem_render::glam::{Mat4, Vec2, Vec3};
        let target = self.target.create_view(&Default::default());
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let backdrop = xem_render::wgpu::Color { r: 0.06, g: 0.06, b: 0.07, a: 1.0 };
        self.compositor.draw(&mut encoder, &target, Some(backdrop), &[]);
        let eyes = [xem_render::ViewTarget { view: self.eye.0, proj: self.eye.1, target: &target, viewport: xem_render::Viewport::full(self.target.width(), self.target.height()) }];
        let presented = view.present_diorama(&mut encoder, display, scene, now, &eyes, Mat4::IDENTITY);
        if let Some(presented) = &presented {
            let view_proj = self.eye.1 * self.eye.0;
            let mut quads = Vec::new();
            if let (Some(pane), Some(space)) = (presented.pane, presented.space) {
                self.pane = Some(pane.texture().clone());
                let mut quad = xem_render::Quad::world(pane, view_proj, space.pane, space.pane_size);
                quad.srgb_encoded = true;
                quads.push(quad);
            }
            if let Some(frame) = presented.frame.as_ref().filter(|_| presented.screen > 0.0) {
                let (width, height) = (frame.size.0 as f32, frame.size.1 as f32);
                let mut quad = xem_render::Quad::world(frame.view, view_proj, Mat4::from_translation(Vec3::new(0.0, 0.0, -2.5)), Vec2::new(3.0, 3.0 * height / width));
                quad.opacity = presented.screen;
                quad.srgb_encoded = true;
                quads.push(quad);
            }
            self.screens.push(presented.screen);
            self.compositor.draw(&mut encoder, &target, None, &quads);
        }
        let shown = presented.is_some();
        gpu.queue.submit([encoder.finish()]);
        let stats = view.diorama_stats();
        self.builds.push(json!({"scene": stats.scene, "sprites": stats.sprites, "one_sided": stats.one_sided, "revealed": stats.revealed,
            "wall": stats.wall, "pane": stats.pane, "tints": stats.tints, "unplaced": stats.unplaced, "lifted": stats.lifted, "runs": stats.runs}));
        shown.then(|| (self.target.width(), self.target.height()))
    }
}

/// `rgba` (of `from` pixels) on a black picture of `to` pixels, top left.
#[cfg(has_game_module)]
fn fit(rgba: &[u8], from: (u32, u32), to: (u32, u32)) -> ((u32, u32), Vec<u8>) {
    if from == to {
        return (to, rgba.to_vec());
    }
    let mut out = vec![0u8; (to.0 * to.1 * 4) as usize];
    out.chunks_exact_mut(4).for_each(|p| p[3] = 255);
    let w = (from.0.min(to.0) * 4) as usize;
    for row in 0..from.1.min(to.1) {
        let a = (row * from.0 * 4) as usize;
        let b = (row * to.0 * 4) as usize;
        out[b..b + w].copy_from_slice(&rgba[a..a + w]);
    }
    (to, out)
}

#[cfg(has_game_module)]
fn main() {
    let mut map = 1u32;
    let mut mode: Option<&str> = None;
    let mut win = false;
    let mut gears = false;
    let mut tap: Option<(u16, u32)> = None;
    let mut presses: Vec<(u16, u32)> = Vec::new();
    let mut fast_forward = 1u32;
    let mut original: Option<std::ops::Range<u32>> = None;
    let mut compare = false;
    let mut bench = false;
    let mut traced = true;
    let mut probe: Option<(f32, f32)> = None;
    let mut from_boot = false;
    let mut script: Option<PathBuf> = None;
    let mut skip = 0u32;
    let mut load_at: Option<u32> = None;
    let mut state: Option<PathBuf> = None;
    let mut hold: Vec<String> = Vec::new();
    let mut frames = 120u32;
    let mut present = 1u32;
    let mut every = 6u32;
    let mut crop: Option<[u32; 4]> = None;
    let mut output = (1920u32, 1080u32);
    let mut graphics = Graphics::default();
    let mut name = String::from("motion");
    let mut cards: Option<PathBuf> = None;
    let mut look: Option<(f32, f32, f32)> = None;
    let mut diorama: Option<[f32; 5]> = None;
    let mut turns = xem_render::ps1::Turns::Smooth;
    let mut out = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../build/validation/motion");
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        let mut value = || args.next().expect("a value");
        match arg.as_str() {
            "--map" => map = value().parse().unwrap(),
            "--worldmap" => mode = Some("worldmap"),
            "--title" => mode = Some("title"),
            "--script" => script = Some(value().into()),
            "--from-boot" => from_boot = true,
            "--skip" => skip = value().parse().unwrap(),
            "--load-at" => load_at = Some(value().parse().unwrap()),
            "--state" => state = Some(value().into()),
            "--battle" => mode = Some("battle"),
            "--win" => win = true,
            "--gears" => gears = true,
            "--compare" => compare = true,
            "--bench" => bench = true,
            "--no-trace" => (traced, bench) = (false, true),
            "--probe" => {
                let v = value();
                let (x, y) = v.split_once(',').expect("x,y");
                probe = Some((x.parse().unwrap(), y.parse().unwrap()));
            }
            "--tap" => {
                let v = value();
                let (button, period) = v.split_once(',').expect("BUTTON,FRAMES");
                tap = Some((xem_core::control::button_bit(button).expect("a button"), period.parse().unwrap()));
            }
            "--press" => {
                let v = value();
                let (button, at) = v.split_once(',').expect("BUTTON,FRAME");
                presses.push((xem_core::control::button_bit(button).expect("a button"), at.parse().unwrap()));
            }
            "--ff" => fast_forward = value().parse::<u32>().unwrap().max(1),
            "--original" => {
                let v = value();
                let (from, to) = v.split_once(',').expect("FROM,TO");
                original = Some(from.parse().unwrap()..to.parse().unwrap());
            }
            "--hold" => hold = value().split(',').map(String::from).collect(),
            "--frames" => frames = value().parse().unwrap(),
            "--present" => present = value().parse::<u32>().unwrap().max(1),
            "--every" => every = value().parse::<u32>().unwrap().max(1),
            "--crop" => {
                let v: Vec<u32> = value().split(',').map(|n| n.parse().unwrap()).collect();
                crop = Some([v[0], v[1], v[2], v[3]]);
            }
            "--size" => {
                let size = value();
                let (w, h) = size.split_once('x').expect("WxH");
                output = (w.parse().unwrap(), h.parse().unwrap());
            }
            "--profile" => {
                graphics.profile = match value().as_str() {
                    "modern" => GraphicsProfile::Modern,
                    "hd2d" => GraphicsProfile::Hd2d,
                    "ps1" => GraphicsProfile::Ps1,
                    other => panic!("unknown profile {other}"),
                }
            }
            "--only" => {
                // The Custom profile with just the named HD-2D effects.
                graphics.profile = GraphicsProfile::Custom;
                let mut levels = xem_settings::EffectLevels::OFF;
                for name in value().split(',').filter(|n| !n.is_empty()) {
                    levels.set(name, xem_settings::EffectLevels::HD2D.get(name).expect("an effect")).unwrap();
                }
                graphics.custom_effects = levels;
            }
            "--widescreen" => graphics.widescreen = true,
            "--zoom" => graphics.zoom_out = value().parse().unwrap(),
            "--interpolate" => graphics.interpolation = true,
            "--original-actors" => graphics.actors_in_view = false,
            "--scale" => graphics.internal_scale = value().parse().unwrap(),
            "--name" => name = value(),
            "--cards" => cards = Some(value().into()),
            "--look" => {
                let v: Vec<f32> = value().split(',').map(|n| n.parse().unwrap()).collect();
                look = Some((v[0], v.get(1).copied().unwrap_or(0.0), v.get(2).copied().unwrap_or(0.0)));
            }
            "--diorama" => {
                let v: Vec<f32> = value().split(',').map(|n| n.parse().unwrap()).collect();
                diorama = Some([v[0], v.get(1).copied().unwrap_or(40.0), v.get(2).copied().unwrap_or(1.0), v.get(3).copied().unwrap_or(1.0), v.get(4).copied().unwrap_or(50.0)]);
            }
            "--turns" => {
                turns = match value().as_str() {
                    "smooth" => xem_render::ps1::Turns::Smooth,
                    "snap" => xem_render::ps1::Turns::Snap,
                    "fixed" => xem_render::ps1::Turns::Fixed,
                    other => panic!("unknown turns {other}"),
                }
            }
            "--out" => out = value().into(),
            other => panic!("unknown option {other}"),
        }
    }
    std::fs::create_dir_all(&out).unwrap();
    let disc = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    let options = xem_game::BootOptions { cards, ..Default::default() };
    let (mut session, _) = xem_game::boot(&disc, &options).expect("boot");
    // `--load-at`: the scene's state, saved from a session of its own; this
    // one starts at the title screen and loads it at that frame (as a player
    // loads a save).
    let saved = load_at.map(|_| {
        let (mut scene, _) = xem_game::boot(&disc, &Default::default()).expect("boot");
        for command in [json!({"cmd": "enter_field", "map": map}), json!({"cmd": "frames", "count": 400})] {
            assert!(scene.execute(&command).get("error").is_none());
        }
        scene.snapshot_bytes().expect("a state")
    });
    if load_at.is_some() {
        mode = Some("title");
        skip = skip.max(700);
    }
    let setup: Vec<Value> = match mode {
        Some("worldmap") => vec![json!({"cmd": "boot_mode", "mode": 3}), json!({"cmd": "frames", "count": 900})],
        Some("battle") => {
            // The original's debug battle selector (ovl2606), as the
            // validation sheet drives it: each member's gear column to Nml
            // first with `--gears`.
            let mut commands = vec![
                json!({"cmd": "frames", "count": 60}),
                json!({"cmd": "write", "path": "mode_battle_standalone", "value": 1}),
                json!({"cmd": "boot_mode", "mode": 2}),
                json!({"cmd": "frames", "count": 900}),
            ];
            if gears {
                for button in ["down", "down", "cross", "right", "cross", "right", "cross"] {
                    commands.push(json!({"cmd": "press", "buttons": [button], "frames": 1}));
                    commands.push(json!({"cmd": "frames", "count": 20}));
                }
            }
            commands.push(json!({"cmd": "press", "buttons": ["start"], "frames": 1}));
            commands.push(json!({"cmd": "frames", "count": 1520}));
            commands
        }
        Some("title") => Vec::new(),
        _ if script.is_some() || state.is_some() => Vec::new(),
        _ => vec![json!({"cmd": "enter_field", "map": map}), json!({"cmd": "frames", "count": 400})],
    };
    if let Some(path) = &state {
        // A saved state (the hosts' state.xemstate), then a moment for the game to show it.
        let reply = xem_game::storage::command(&mut session, &json!({"cmd": "state_load", "path": path}));
        assert!(reply.get("error").is_none(), "state_load -> {reply}");
    }
    if let Some(path) = &script {
        let log: Value = serde_json::from_str(&std::fs::read_to_string(path).expect("a session log")).expect("JSON");
        let reply = session.execute(&json!({"cmd": "schedule", "commands": log["commands"]}));
        assert!(reply.get("error").is_none(), "schedule -> {reply}");
    }
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let adapter = gpu.adapter.get_info();
    eprintln!("adapter: {} ({:?}, {:?})", adapter.name, adapter.device_type, adapter.backend);
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    if let Some((yaw, pitch, _)) = look {
        view.set_first_person(true);
        view.look_around(yaw.to_radians(), pitch.to_radians());
    }
    // The diorama, seen through a flat review camera, with a headset's
    // settings (the Headset class).
    let host = if diorama.is_some() { HostClass::Headset } else { HostClass::Desktop };
    let mut review = diorama.map(|camera| {
        let table = xem_render::ps1::Table { scale: camera[3], turns, ..Default::default() };
        view.set_xr_view(xem_render::ps1::XrView::Diorama(table));
        Review::new(&gpu, camera, &table, output)
    });
    // From boot the trace and the presentation run through the setup too,
    // as a host's do (a frame at a time, nothing kept).
    let presented_frame = |session: &mut xem_core::control::Session<xem_game::NativeModule>, view: &mut GameView, review: &mut Option<Review>, clock: f64| {
        view.configure(ViewConfig::resolve(&graphics, host, output, view.max_scale()));
        session.set_actor_view(if graphics.actors_in_view { view.actor_view() } else { 0 }).expect("actor view");
        session.frame().expect("frame");
        let device = session.runtime.services().gpu.device_mut();
        if view.take_restart() || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(view.reveals());
        view.push(device.take_trace());
        let display = device.display();
        let scene = session.scene();
        match review {
            Some(review) => {
                let _ = review.present(&gpu, view, &display, &scene, clock);
            }
            None => {
                let mut encoder = gpu.device.create_command_encoder(&Default::default());
                let _ = view.present(&mut encoder, &display, &scene, clock);
                gpu.queue.submit([encoder.finish()]);
            }
        }
    };
    let mut setup_clock = 0.0;
    for command in &setup {
        match command.get("count").and_then(Value::as_u64).filter(|_| from_boot && command["cmd"] == "frames") {
            Some(count) => (0..count).for_each(|_| {
                setup_clock += 1.0 / 60.0;
                presented_frame(&mut session, &mut view, &mut review, setup_clock)
            }),
            None => {
                let reply = session.execute(command);
                assert!(reply.get("error").is_none(), "{command} -> {reply}");
            }
        }
    }
    for _ in 0..skip {
        setup_clock += 1.0 / 60.0;
        presented_frame(&mut session, &mut view, &mut review, setup_clock);
    }
    if win {
        // battle_work_area's combatants (0x170 bytes; slots 3-10 the
        // enemies, HP at +0x4C), and the results module's pointer to it
        // (battle_results_work_ptr) once that module is loaded.
        const WORK: u32 = 0x800C_CCE8;
        let hp = |session: &mut xem_core::control::Session<xem_game::NativeModule>, slot: u32| {
            session.execute(&json!({"cmd": "read_memory", "address": WORK + slot * 0x170 + 0x4C, "length": 2})).as_str().unwrap_or("").to_string()
        };
        eprintln!("HP by slot: {:?}", (0..11).map(|slot| hp(&mut session, slot)).collect::<Vec<_>>());
        // A gear's (or a monster's) HP is its gear record's, at +0xA4 + 0x60.
        for slot in 3..11u32 {
            if hp(&mut session, slot) != "0000" {
                session.execute(&json!({"cmd": "write_memory", "address": WORK + slot * 0x170 + 0x4C, "hex": "0100"}));
            }
            let gear = WORK + slot * 0x170 + 0x104;
            if session.execute(&json!({"cmd": "read_memory", "address": gear, "length": 4})).as_str() != Some("00000000") {
                session.execute(&json!({"cmd": "write_memory", "address": gear, "hex": "01000000"}));
            }
        }
        let mut won = false;
        for round in 0..400 {
            let button = ["cross", "circle", "triangle", "square"][round % 4];
            session.execute(&json!({"cmd": "press", "buttons": [button], "frames": 2}));
            session.execute(&json!({"cmd": "frames", "count": 10}));
            if round % 50 == 0 {
                eprintln!("round {round}: HP {:?}", (0..11).map(|slot| hp(&mut session, slot)).collect::<Vec<_>>());
            }
            let pointer = session.execute(&json!({"cmd": "read_memory", "address": 0x801E_44C8u32, "length": 4}));
            if pointer.as_str() == Some("e8cc0c80") {
                won = true;
                break;
            }
        }
        assert!(won, "the battle did not end in victory");
        session.execute(&json!({"cmd": "frames", "count": 120}));
    }
    let buttons = hold.iter().map(|b| xem_core::control::button_bit(b).unwrap_or_else(|| panic!("no button {b}"))).fold(0, |a, b| a | b);
    let mut stages = [Stage::new("game_frame"), Stage::new("trace"), Stage::new("present_cpu"), Stage::new("gpu")];
    let mut sheet_frames: Vec<((u32, u32), Vec<u8>)> = Vec::new();
    let mut animation = std::collections::VecDeque::new();
    let mut changes = Vec::new();
    let mut revealed = Vec::new();
    let mut revealed_sprites = Vec::new();
    let mut shown_revealed = Vec::new();
    let mut movies = Vec::new();
    let mut still = Vec::new();
    let mut previous: Option<Vec<u8>> = None;
    let mut size = (0, 0);
    let mut clock = setup_clock;
    if !from_boot && skip == 0 {
        session.runtime.services().gpu.device_mut().set_trace(traced);
        let _ = view.take_restart();
    }
    for frame in 0..frames {
        view.configure(ViewConfig::resolve(&graphics, host, output, view.max_scale()));
        if load_at == Some(frame) {
            session.restore_bytes(saved.as_ref().expect("saved")).expect("load");
        }
        let tapped = tap.filter(|(_, period)| frame % period < 2).map_or(0, |(button, _)| button);
        let pressed = presses.iter().filter(|(_, at)| (*at..*at + 2).contains(&frame)).fold(0, |all, (button, _)| all | button);
        // A script's pads stand unless a tap or a press is asked for too.
        if script.is_none() || tap.is_some() || !presses.is_empty() {
            session.set_pad(0, buttons | tapped | pressed);
        }
        session.set_actor_view(if graphics.actors_in_view { view.actor_view() } else { 0 }).expect("actor view");
        if let Some((_, _, turn)) = look {
            view.look_around(turn.to_radians(), 0.0);
        }
        // `--ff`: the frames a fast-forward batch runs before this one.
        for _ in 1..fast_forward {
            session.frame().expect("frame");
        }
        let started = Instant::now();
        session.frame().expect("frame");
        stages[0].samples.push(started.elapsed().as_secs_f64() * 1000.0);
        still.push(still_actors(&mut session));
        if !traced {
            continue;
        }
        // `--original`: the Original renderer shown for these frames (no
        // trace), then this view again, as a host switched does.
        if original.as_ref().is_some_and(|frames| frames.contains(&frame)) {
            session.runtime.services().gpu.device_mut().set_trace(false);
            continue;
        }
        let started = Instant::now();
        let device = session.runtime.services().gpu.device_mut();
        // A loaded state's device starts without a trace, as hosts find it.
        if view.take_restart() || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(view.reveals());
        view.push(device.take_trace());
        let display = device.display();
        movies.push(if display.color24 { i8::from(view.shows_movie(&display)) } else { -1 });
        revealed.push(view.frame().newest().iter().filter(|op| matches!(op, xem_render::xem_gpu::TraceOp::Draw(p) if p.revealed)).count());
        revealed_sprites.push(view.frame().newest().iter().filter(|op| matches!(op, xem_render::xem_gpu::TraceOp::Draw(p) if p.revealed && p.class == xem_render::xem_gpu::Class::Sprite)).count());
        stages[1].samples.push(started.elapsed().as_secs_f64() * 1000.0);
        let scene = session.scene();
        for step in 0..present {
            clock += 1.0 / 60.0 / f64::from(present);
            let started = Instant::now();
            let presented = match &mut review {
                Some(review) => review.present(&gpu, &mut view, &display, &scene, clock),
                None => {
                    let mut encoder = gpu.device.create_command_encoder(&Default::default());
                    let presented = view.present(&mut encoder, &display, &scene, clock).map(|p| p.size);
                    gpu.queue.submit([encoder.finish()]);
                    presented
                }
            };
            stages[2].samples.push(started.elapsed().as_secs_f64() * 1000.0);
            let started = Instant::now();
            gpu.device.poll(xem_render::wgpu::PollType::wait_indefinitely()).unwrap();
            stages[3].samples.push(started.elapsed().as_secs_f64() * 1000.0);
            let Some(presented) = presented else { continue };
            size = presented;
            if bench {
                continue;
            }
            let picture = match &review {
                Some(review) => &review.target,
                None => view.presented_texture().expect("presented"),
            };
            let mut rgba = read_rgba8(&gpu.device, &gpu.queue, picture);
            rgba.chunks_exact_mut(4).for_each(|p| p[3] = 255);
            if let Some([x, y, w, h]) = crop {
                let (w, h) = (w.min(size.0 - x), h.min(size.1 - y));
                let mut cut = Vec::with_capacity((w * h * 4) as usize);
                for row in y..y + h {
                    let start = ((row * size.0 + x) * 4) as usize;
                    cut.extend_from_slice(&rgba[start..start + (w * 4) as usize]);
                }
                rgba = cut;
                size = (w, h);
            }
            if let Some(before) = &previous {
                let changed = rgba.chunks(4).zip(before.chunks(4)).filter(|(a, b)| a[..3] != b[..3]).count();
                changes.push(changed as f64 / (size.0 * size.1) as f64);
            }
            let index = frame * present + step;
            if index % every == 0 {
                let mut cell = rgba.clone();
                if compare && crop.is_none() {
                    // The console's picture, each native pixel scaled to fit ours' height.
                    let (cw, ch, console) = session.runtime.services().gpu.display_rgba8();
                    let mut both = Vec::with_capacity(cell.len() * 2);
                    for row in 0..size.1 {
                        both.extend_from_slice(&cell[(row * size.0 * 4) as usize..((row + 1) * size.0 * 4) as usize]);
                        let cy = (row * ch / size.1.max(1)).min(ch.saturating_sub(1));
                        for x in 0..size.0 {
                            let cx = (x * ch / size.1.max(1)).min(cw.saturating_sub(1));
                            let at = ((cy * cw + cx) * 4) as usize;
                            both.extend_from_slice(if x * ch / size.1.max(1) < cw { &console[at..at + 4] } else { &[0, 0, 0, 255] });
                        }
                    }
                    cell = both;
                }
                let width = if compare && crop.is_none() { size.0 * 2 } else { size.0 };
                sheet_frames.push(((width, size.1), cell));
            }
            if animation.len() == ANIMATED {
                animation.pop_front();
            }
            animation.push_back((size, rgba.clone()));
            previous = Some(rgba);
        }
        // The culled faces drawn in the frame shown.
        shown_revealed.push(view.frame().newest().iter().filter(|op| matches!(op, xem_render::xem_gpu::TraceOp::Draw(p) if p.revealed)).count());
    }
    if let Some((x, y)) = probe {
        for op in view.frame().primitives() {
            let xem_render::xem_gpu::TraceOp::Draw(p) = op else { continue };
            // The point in the buffer the primitive was drawn into.
            let (x, y) = (x + f32::from(p.environment.area[0]), y + f32::from(p.environment.area[1]));
            let v = &p.vertices[..p.vertex_count()];
            let (x0, x1) = v.iter().fold((f32::MAX, f32::MIN), |(a, b), v| (a.min(v.fx), b.max(v.fx)));
            let (y0, y1) = v.iter().fold((f32::MAX, f32::MIN), |(a, b), v| (a.min(v.fy), b.max(v.fy)));
            if x >= x0 && x <= x1 && y >= y0 && y <= y1 {
                let depths: Vec<f32> = v.iter().map(|v| v.z).collect();
                eprintln!("{:?} {:?} textured {} semi {} offset {:?} owner {:#x} depths {depths:?} bounds {x0:.1},{y0:.1}-{x1:.1},{y1:.1} mode {:#x}",
                    p.class, p.shape, p.textured, p.semi, p.projection, p.owner, p.environment.draw_mode);
                eprintln!("    points (x, y, u, v): {:?}", v.iter().map(|v| (v.x, v.y, v.u, v.v)).collect::<Vec<_>>());
                eprintln!("    before the division: {:?}, floor {:?}", v.iter().map(|v| v.view).collect::<Vec<_>>(), p.ground);
                // A small texture's texels: '.' transparent, 's' translucent (its
                // semi-transparency bit), '#' opaque.
                let (u0, u1) = v.iter().fold((255, 0), |(a, b), v| (a.min(v.u), b.max(v.u)));
                let (v0, v1) = v.iter().fold((255, 0), |(a, b), v| (a.min(v.v), b.max(v.v)));
                if p.textured && u1 - u0 <= 32 && v1 - v0 <= 32 {
                    let vram = session.runtime.services().gpu.device().vram().to_vec();
                    let mode = p.environment.draw_mode;
                    let (page_x, page_y) = ((mode & 15) as usize * 64, (mode >> 4 & 1) as usize * 256);
                    let at = |x: usize, y: usize| vram[(y & 511) * 1024 + (x & 1023)];
                    let clut = |index: usize| at(usize::from(p.clut & 63) * 16 + index, usize::from(p.clut >> 6 & 511));
                    for tv in v0..=v1 {
                        let row: String = (u0..=u1).map(|tu| {
                            let (tu, tv) = (tu as usize, tv as usize);
                            let texel = match mode >> 7 & 3 {
                                0 => clut(usize::from(at(page_x + tu / 4, page_y + tv) >> (tu % 4 * 4) & 15)),
                                1 => clut(usize::from(at(page_x + tu / 2, page_y + tv) >> (tu % 2 * 8) & 255)),
                                _ => at(page_x + tu, page_y + tv),
                            };
                            if texel == 0 { '.' } else if texel & 0x8000 != 0 { 's' } else { '#' }
                        }).collect();
                        eprintln!("    {row}");
                    }
                }
            }
        }
    }
    let [matched, unmatched] = session.runtime.services().gpu.device().trace_matches();
    // Contact sheet: four columns (two pairs when comparing), each cell as
    // large as the largest picture (the display's size can change).
    let cell = sheet_frames.iter().fold((0, 0), |(w, h), ((pw, ph), _)| (w.max(*pw), h.max(*ph)));
    let columns = (if compare { 2u32 } else { 4 }).min(sheet_frames.len() as u32).max(1);
    let rows = (sheet_frames.len() as u32).div_ceil(columns);
    let (sw, sh) = (cell.0 * columns, cell.1 * rows);
    let mut sheet = vec![0u8; (sw * sh * 4) as usize];
    for (i, ((pw, ph), picture)) in sheet_frames.iter().enumerate() {
        let (cx, cy) = (i as u32 % columns * cell.0, i as u32 / columns * cell.1);
        for row in 0..*ph {
            let from = (row * pw * 4) as usize;
            let to = (((cy + row) * sw + cx) * 4) as usize;
            sheet[to..to + (pw * 4) as usize].copy_from_slice(&picture[from..from + (pw * 4) as usize]);
        }
    }
    // The animation at the last picture's size.
    let animation: Vec<Vec<u8>> = animation.iter().map(|(picture_size, rgba)| fit(rgba, *picture_size, size)).map(|(_, rgba)| rgba).collect();
    let write = |path: PathBuf, (w, h): (u32, u32), pictures: &[Vec<u8>]| {
        let file = std::fs::File::create(&path).expect("output");
        let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), w, h);
        encoder.set_color(png::ColorType::Rgba);
        if pictures.len() > 1 {
            encoder.set_animated(pictures.len() as u32, 0).unwrap();
            encoder.set_frame_delay(1, (60 * present) as u16).unwrap();
        }
        let mut writer = encoder.write_header().unwrap();
        for picture in pictures {
            writer.write_image_data(picture).unwrap();
        }
        writer.finish().unwrap();
        eprintln!("{}", path.display());
    };
    if !sheet_frames.is_empty() {
        write(out.join(format!("{name}-sheet.png")), (sw, sh), &[sheet]);
    }
    // The console's own picture of the last frame, for comparison.
    let (cw, ch, console) = session.runtime.services().gpu.display_rgba8();
    write(out.join(format!("{name}-console.png")), (cw, ch), &[console]);
    if let Some(last) = animation.last() {
        write(out.join(format!("{name}-last.png")), size, std::slice::from_ref(last));
    }
    // The diorama's last interface pane (transparent where it shows nothing).
    if let Some(pane) = review.as_ref().and_then(|review| review.pane.as_ref()) {
        write(out.join(format!("{name}-pane.png")), (pane.width(), pane.height()), &[read_rgba8(&gpu.device, &gpu.queue, pane)]);
    }
    if !animation.is_empty() {
        write(out.join(format!("{name}.png")), size, &animation);
    }
    let mean_change = changes.iter().sum::<f64>() / changes.len().max(1) as f64;
    // FNV-1a over the native VRAM: the software GPU's picture of everything drawn.
    let vram_hash = format!("{:016x}", session.runtime.services().gpu.device().vram().iter()
        .fold(0xCBF2_9CE4_8422_2325u64, |h, &p| (h ^ u64::from(p)).wrapping_mul(0x100_0000_01B3)));
    let report = json!({
        "adapter": format!("{} ({:?})", adapter.name, adapter.backend),
        "scene": session.scene(),
        "graphics": graphics,
        "frames": frames, "present": present, "size": [size.0, size.1],
        "stages": stages.iter().map(|s| (s.name.to_string(), s.report())).collect::<serde_json::Map<_, _>>(),
        "changed_pixels": {"mean": mean_change, "per_frame": changes},
        "points": {"matched": matched, "unmatched": unmatched},
        "revealed": revealed,
        "revealed_sprites": revealed_sprites,
        "shown_revealed": shown_revealed,
        "movies": movies,
        "still": still,
        "eye": view.frame().eye().map(|eye| json!({"position": eye.position, "axes": eye.axes, "sprite": eye.sprite})),
        "first_person": view.first_person(),
        "diorama": review.as_ref().map(|review| json!({"screen": review.screens, "builds": review.builds})),
    });
    std::fs::write(out.join(format!("{name}.json")), serde_json::to_string_pretty(&report).unwrap()).unwrap();
    println!("{}", serde_json::to_string(&json!({"stages": report["stages"], "changed_mean": mean_change, "points": report["points"], "revealed": [revealed.first(), revealed.iter().min(), revealed.iter().max(), revealed.last()], "still": [still.first(), still.iter().min(), still.iter().max(), still.last()], "revealed_sprites": [revealed_sprites.iter().min(), revealed_sprites.iter().max()], "memory_hash": session.execute(&json!({"cmd": "memory_hash"})), "vram_hash": vram_hash})).unwrap());
}

/// The field's sprite actors (not the 801e layer's) marked off screen
/// (layer_flags 0x200: they stop moving), outside the field none.
#[cfg(has_game_module)]
fn still_actors(session: &mut xem_core::control::Session<xem_game::NativeModule>) -> usize {
    if !session.scene().starts_with("field") {
        return 0;
    }
    // field_event_actor_count (only the linker script names it).
    let mut count = [0u8; 4];
    if session.runtime.memory().read(0x800A_DBFC, &mut count).is_err() {
        return 0;
    }
    let count = u64::from(u32::from_le_bytes(count));
    let mut read = |path: String| session.execute(&json!({"cmd": "inspect", "path": path})).as_u64();
    (0..count.min(64))
        .filter(|i| {
            let flags = read(format!("field_view.components.descriptors[{i}].flags")).unwrap_or(0);
            let layer = read(format!("field_view.components.descriptors[{i}].actor->layer_flags")).unwrap_or(0);
            flags & 0x40 != 0 && layer & 0x2200 == 0x200
        })
        .count()
}
