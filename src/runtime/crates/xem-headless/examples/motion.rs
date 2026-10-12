//! Motion and cost check (local only: it needs the user's disc 1 and the
//! game module; the frames are pictures of the original game and stay in
//! build/validation/motion, which is ignored).
//!
//!     cargo run --release -p xem-headless --example motion -- [--map N | --worldmap | --battle [--gears] [--win]
//!         | --title | --script LOG | --state FILE] [--from-boot] [--skip N] [--load-at N]
//!         [--hold right,cross] [--tap BUTTON,FRAMES] [--frames 120] [--present 2]
//!         [--profile modern|hd2d|ps1] [--only effect,...] [--widescreen] [--zoom PERCENT] [--interpolate]
//!         [--scale N] [--size WxH] [--every N] [--crop x,y,w,h] [--compare] [--bench] [--no-trace]
//!         [--probe x,y] [--original-actors]
//!         [--name NAME] [--out DIR]
//!
//! `--win` ends the battle in victory first (the enemies' HP set to 1, then
//! the confirm button until the results module runs), for the result
//! screens. `--title` starts at boot; `--state` from a saved state (a
//! host's state.xemstate); `--script` plays a session log (its pads, not
//! `--hold`); `--from-boot` traces and presents every frame of the
//! setup too, as a host does from boot, and `--skip` presents that many
//! frames before the ones kept; `--load-at` starts at the title screen and
//! loads a state of the field `--map` at that frame. From the scene, the game runs `frames` frames holding the given
//! buttons (and tapping one every so many frames, through menus and result
//! screens); each game frame is presented `present` times at evenly spaced
//! host times through the hosts' GameView. Written: every `every`th
//! presented frame (cropped when asked; with `--compare` beside the
//! console's own picture of it, scaled up) in a contact sheet `NAME-sheet.png`,
//! the last 240 as an animated PNG `NAME.png`, the last one and the
//! console's own picture of it (`NAME-last.png`, `NAME-console.png`), and
//! `NAME.json` with the mean and worst cost per stage (the game frame, which
//! includes the port's presentation records and the trace's matching;
//! taking and pushing the trace; the presentation's CPU work; the GPU's)
//! and, per presented frame, the share of pixels that changed since the one
//! before (a still camera over still scenery changes only where something
//! moves), and per game frame how many faces the game culled were drawn.
//! `--bench` reads nothing back and writes only the costs;
//! `--no-trace` runs the game alone, presenting nothing (its cost without
//! any presentation work). `--probe` lists the last frame's primitives
//! covering a point of the original screen (class, projection, depth,
//! owner). The field's characters keep moving in the wider or zoomed-out
//! view as in the hosts (`Session::set_actor_view`), unless
//! `--original-actors`; the JSON counts the sprite actors the field stops
//! as off screen per frame (`still`).

use serde_json::{Value, json};
use std::path::PathBuf;
use std::time::Instant;
use xem_render::ps1::{GameView, HostClass, ViewConfig};
use xem_render::{Gpu, read_rgba8};
use xem_settings::{Graphics, GraphicsProfile};


struct Stage {
    name: &'static str,
    samples: Vec<f64>,
}

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
const ANIMATED: usize = 240;

/// `rgba` (of `from` pixels) on a black picture of `to` pixels, top left.
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

fn main() {
    let mut map = 1u32;
    let mut mode: Option<&str> = None;
    let mut win = false;
    let mut gears = false;
    let mut tap: Option<(u16, u32)> = None;
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
            "--out" => out = value().into(),
            other => panic!("unknown option {other}"),
        }
    }
    std::fs::create_dir_all(&out).unwrap();
    let disc = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    let (mut session, _) = xem_game::boot(&disc, &Default::default()).expect("boot");
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
    // From boot the trace and the presentation run through the setup too,
    // as a host's do (a frame at a time, nothing kept).
    let presented_frame = |session: &mut xem_core::control::Session<xem_game::NativeModule>, view: &mut GameView| {
        view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, output, view.max_scale()));
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
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let _ = view.present(&mut encoder, &display, &scene, 0.0);
        gpu.queue.submit([encoder.finish()]);
    };
    for command in &setup {
        match command.get("count").and_then(Value::as_u64).filter(|_| from_boot && command["cmd"] == "frames") {
            Some(count) => (0..count).for_each(|_| presented_frame(&mut session, &mut view)),
            None => {
                let reply = session.execute(command);
                assert!(reply.get("error").is_none(), "{command} -> {reply}");
            }
        }
    }
    for _ in 0..skip {
        presented_frame(&mut session, &mut view);
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
    let mut still = Vec::new();
    let mut previous: Option<Vec<u8>> = None;
    let mut size = (0, 0);
    let mut clock = 0.0f64;
    if !from_boot && skip == 0 {
        session.runtime.services().gpu.device_mut().set_trace(traced);
        let _ = view.take_restart();
    }
    for frame in 0..frames {
        view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, output, view.max_scale()));
        if load_at == Some(frame) {
            session.restore_bytes(saved.as_ref().expect("saved")).expect("load");
        }
        let tapped = tap.filter(|(_, period)| frame % period < 2).map_or(0, |(button, _)| button);
        if script.is_none() {
            session.set_pad(0, buttons | tapped);
        }
        session.set_actor_view(if graphics.actors_in_view { view.actor_view() } else { 0 }).expect("actor view");
        let started = Instant::now();
        session.frame().expect("frame");
        stages[0].samples.push(started.elapsed().as_secs_f64() * 1000.0);
        still.push(still_actors(&mut session));
        if !traced {
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
        revealed.push(view.frame().newest().iter().filter(|op| matches!(op, xem_render::xem_gpu::TraceOp::Draw(p) if p.revealed)).count());
        revealed_sprites.push(view.frame().newest().iter().filter(|op| matches!(op, xem_render::xem_gpu::TraceOp::Draw(p) if p.revealed && p.class == xem_render::xem_gpu::Class::Sprite)).count());
        stages[1].samples.push(started.elapsed().as_secs_f64() * 1000.0);
        let scene = session.scene();
        for step in 0..present {
            clock += 1.0 / 60.0 / f64::from(present);
            let started = Instant::now();
            let mut encoder = gpu.device.create_command_encoder(&Default::default());
            let presented = view.present(&mut encoder, &display, &scene, clock).map(|p| p.size);
            gpu.queue.submit([encoder.finish()]);
            stages[2].samples.push(started.elapsed().as_secs_f64() * 1000.0);
            let started = Instant::now();
            gpu.device.poll(xem_render::wgpu::PollType::wait_indefinitely()).unwrap();
            stages[3].samples.push(started.elapsed().as_secs_f64() * 1000.0);
            let Some(presented) = presented else { continue };
            size = presented;
            if bench {
                continue;
            }
            let mut rgba = read_rgba8(&gpu.device, &gpu.queue, view.presented_texture().expect("presented"));
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
    if !animation.is_empty() {
        write(out.join(format!("{name}.png")), size, &animation);
    }
    let mean_change = changes.iter().sum::<f64>() / changes.len().max(1) as f64;
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
        "still": still,
    });
    std::fs::write(out.join(format!("{name}.json")), serde_json::to_string_pretty(&report).unwrap()).unwrap();
    println!("{}", serde_json::to_string(&json!({"stages": report["stages"], "changed_mean": mean_change, "points": report["points"], "revealed": [revealed.first(), revealed.iter().min(), revealed.iter().max(), revealed.last()], "still": [still.first(), still.iter().min(), still.iter().max(), still.last()], "revealed_sprites": [revealed_sprites.iter().min(), revealed_sprites.iter().max()], "memory_hash": session.execute(&json!({"cmd": "memory_hash"}))})).unwrap());
}

/// The field's sprite actors (not the 801e layer's) marked off screen
/// (layer_flags 0x200: they stop moving), outside the field none.
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
