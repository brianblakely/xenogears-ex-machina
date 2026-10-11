//! Phase 5's side-by-side validation sheet (local only: it needs the user's
//! disc 1 and the game module; the sheet holds pictures of the original game
//! and stays in build/validation, which is ignored).
//!
//!     cargo run --release -p xem-headless --example validation -- [--out DIR] [--scale N]
//!         [--scenes name,...]
//!
//! Each representative scene (exterior, interiors with characters and a
//! dialogue window and portrait, a dark forest, the world map, an on-foot
//! and a Gear battle, the main menu, an opening text scene, a movie frame)
//! is reached by game commands (enter_field, boot_mode, the original's
//! debug battle selector, pad presses), then drawn under Modern, HD-2D,
//! PS1, Modern with wider framing (16:9) and Modern with wider framing and
//! the field camera pulled back to 2.5x, from the same snapshot. Written:
//! sheet.png (rows: scenes; columns: the looks), one PNG per scene and look,
//! and report.json with, per scene and look, the frame size, the RAM hash
//! after drawing (equal across looks), the share of pixels the look changes
//! against Modern, the interface pixels (text, windows, portraits) HD-2D
//! changed against Modern (none), and the interface pixels that differ from
//! the console's own picture (the device's display, each native pixel
//! scaled up) by more than one 5-bit step: opaque ones (none expected) and
//! semi-transparent window pixels (over a scene drawn at a finer scale, so
//! some may).

use serde_json::{Value, json};
use std::path::PathBuf;
use xem_render::ps1::{GameView, HostClass, ViewConfig, wide_side};
use xem_render::{Gpu, read_rgba8};
use xem_settings::{Graphics, GraphicsProfile};

type Session = xem_core::control::Session<xem_game::NativeModule>;

fn press(button: &str) -> Vec<Value> {
    vec![json!({"cmd": "press", "buttons": [button], "frames": 1}), json!({"cmd": "frames", "count": 20})]
}

/// The original's debug battle selector (ovl2606): Start with the default
/// scene and party; `gears` sets each member's gear column to Nml first.
fn battle(gears: bool) -> Vec<Value> {
    let mut commands = vec![
        json!({"cmd": "frames", "count": 60}),
        json!({"cmd": "write", "path": "mode_battle_standalone", "value": 1}),
        json!({"cmd": "boot_mode", "mode": 2}),
        json!({"cmd": "frames", "count": 900}),
    ];
    if gears {
        for button in ["down", "down", "cross", "right", "cross", "right", "cross"] {
            commands.extend(press(button));
        }
    }
    commands.extend(press("start"));
    commands.push(json!({"cmd": "frames", "count": 1500}));
    commands
}

fn scenes() -> Vec<(&'static str, Vec<Value>)> {
    let field = |map: u32, frames: u32| vec![json!({"cmd": "enter_field", "map": map}), json!({"cmd": "frames", "count": frames})];
    vec![
        ("exterior-lahan", field(1, 400)),
        ("interior-dialogue", field(12, 400)),
        ("interior-characters", field(30, 400)),
        ("forest", field(22, 400)),
        ("world-map", vec![json!({"cmd": "boot_mode", "mode": 3}), json!({"cmd": "frames", "count": 900})]),
        ("battle-on-foot", battle(false)),
        ("battle-gear", battle(true)),
        ("menu", {
            let mut commands = field(30, 400);
            commands.extend(press("square"));
            commands.push(json!({"cmd": "frames", "count": 90}));
            commands
        }),
        ("opening-text", field(4, 400)),
        ("movie", vec![json!({"cmd": "frames", "count": 480})]),
    ]
}

const LOOKS: [&str; 5] = ["modern", "hd2d", "ps1", "modern-wide", "modern-zoom"];

fn graphics(look: &str, scale: u8) -> Graphics {
    let base = Graphics { internal_scale: scale, ..Default::default() };
    match look {
        "hd2d" => Graphics { profile: GraphicsProfile::Hd2d, ..base },
        "ps1" => Graphics { profile: GraphicsProfile::Ps1, ..base },
        "modern-wide" => Graphics { widescreen: true, ..base },
        "modern-zoom" => Graphics { widescreen: true, zoom_out: 250, ..base },
        _ => base,
    }
}

struct Picture {
    size: (u32, u32),
    rgba: Vec<u8>,
    /// Per pixel: the auxiliary class (1 = interface) and flags (255: a
    /// semi-transparent interface pixel), when the renderer drew it.
    classes: Option<Vec<u8>>,
    flags: Option<Vec<u8>>,
    /// The frame's scale and wider framing's side, and the console's own
    /// picture: (width, height, RGBA).
    scale: u32,
    side: u32,
    console: (u32, u32, Vec<u8>),
}

/// Eight frames under `graphics`, the last one's picture.
fn draw(gpu: &Gpu, session: &mut Session, graphics: &Graphics) -> Picture {
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    view.configure(ViewConfig::resolve(graphics, HostClass::Desktop, (1920, 1080), view.max_scale()));
    session.runtime.services().gpu.device_mut().set_trace(true);
    let _ = view.take_restart();
    let mut presented = None;
    for frame in 0..8 {
        session.frame().expect("frame");
        let device = session.runtime.services().gpu.device_mut();
        device.set_reveals(view.reveal_reach());
        view.push(device.take_trace());
        let display = device.display();
        let scene = session.scene();
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        presented = view.present(&mut encoder, &display, &scene, frame as f64 / 60.0).map(|p| p.size);
        gpu.queue.submit([encoder.finish()]);
    }
    session.runtime.services().gpu.device_mut().set_trace(false);
    let console = session.runtime.services().gpu.display_rgba8();
    match presented {
        Some(size) => {
            let mut rgba = read_rgba8(&gpu.device, &gpu.queue, view.presented_texture().expect("presented"));
            rgba.chunks_exact_mut(4).for_each(|p| p[3] = 255);
            let aux = read_rgba8(&gpu.device, &gpu.queue, view.frame().aux());
            let (scale, side) = view.frame().area().map_or((1, 0), |(_, scale)| (scale, view.frame().side()));
            Picture {
                size,
                rgba,
                classes: Some(aux.chunks(4).map(|p| p[2]).collect()),
                flags: Some(aux.chunks(4).map(|p| p[3]).collect()),
                scale,
                side,
                console,
            }
        }
        None => {
            // A 24-bit movie frame: the device's own picture, as hosts show it.
            let (width, height, rgba) = console.clone();
            Picture { size: (width, height), rgba, classes: None, flags: None, scale: 1, side: 0, console }
        }
    }
}

/// The interface pixels of `picture` against the console's own picture:
/// opaque and semi-transparent ones, and how many of each differ from the
/// native pixel under them by more than one 5-bit step in a channel. With
/// `diff`, an image of them: differing opaque interface pixels red,
/// differing semi-transparent ones orange, matching ones green, the rest
/// dimmed.
fn against_console(picture: &Picture, diff: Option<&std::path::Path>) -> Value {
    let (Some(classes), Some(flags)) = (&picture.classes, &picture.flags) else { return Value::Null };
    let (cw, ch, console) = &picture.console;
    let mut counts = [[0u64; 2]; 2];
    let mut image: Vec<u8> = picture.rgba.iter().enumerate().map(|(i, v)| if i % 4 == 3 { 255 } else { v / 4 }).collect();
    for y in 0..picture.size.1 {
        for x in 0..picture.size.0 {
            let i = (y * picture.size.0 + x) as usize;
            if classes[i] != 1 {
                continue;
            }
            let nx = (x / picture.scale) as i64 - i64::from(picture.side);
            let ny = (y / picture.scale) as i64;
            if nx < 0 || ny < 0 || nx >= i64::from(*cw) || ny >= i64::from(*ch) {
                continue;
            }
            let at = ((ny as u32 * cw + nx as u32) * 4) as usize;
            let differs = (0..3).any(|c| (i32::from(picture.rgba[i * 4 + c]) - i32::from(console[at + c])).abs() > 8);
            let semi = usize::from(flags[i] == 255);
            counts[semi][0] += 1;
            counts[semi][1] += u64::from(differs);
            image[i * 4..i * 4 + 3].copy_from_slice(match (differs, semi) {
                (true, 0) => &[255, 0, 0],
                (true, _) => &[255, 170, 0],
                (false, _) => &[0, 160, 0],
            });
        }
    }
    if let Some(path) = diff {
        write_png(path, picture.size, &image);
    }
    json!({"opaque": {"pixels": counts[0][0], "differ": counts[0][1]}, "semi": {"pixels": counts[1][0], "differ": counts[1][1]}})
}

fn write_png(path: &std::path::Path, (width, height): (u32, u32), rgba: &[u8]) {
    let file = std::fs::File::create(path).expect("output");
    let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), width, height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.write_header().unwrap().write_image_data(rgba).unwrap();
}

/// `picture` scaled (nearest) into a `cell`-sized box, centred, on black.
fn fit(picture: &Picture, cell: (u32, u32)) -> Vec<u8> {
    let mut out = vec![0u8; (cell.0 * cell.1 * 4) as usize];
    for chunk in out.chunks_exact_mut(4) {
        chunk[3] = 255;
    }
    let factor = (cell.0 as f32 / picture.size.0 as f32).min(cell.1 as f32 / picture.size.1 as f32);
    let (w, h) = ((picture.size.0 as f32 * factor) as u32, (picture.size.1 as f32 * factor) as u32);
    let (ox, oy) = ((cell.0 - w) / 2, (cell.1 - h) / 2);
    for y in 0..h {
        for x in 0..w {
            let sx = ((x as f32 / factor) as u32).min(picture.size.0 - 1);
            let sy = ((y as f32 / factor) as u32).min(picture.size.1 - 1);
            let from = ((sy * picture.size.0 + sx) * 4) as usize;
            let to = (((oy + y) * cell.0 + ox + x) * 4) as usize;
            out[to..to + 4].copy_from_slice(&picture.rgba[from..from + 4]);
        }
    }
    out
}

fn main() {
    let mut out = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../build/validation");
    let mut scale = 3u8;
    let mut only: Option<Vec<String>> = None;
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--out" => out = args.next().expect("a directory").into(),
            "--scale" => scale = args.next().expect("a scale").parse().unwrap(),
            "--scenes" => only = Some(args.next().expect("scene names").split(',').map(String::from).collect()),
            other => panic!("unknown option {other}"),
        }
    }
    std::fs::create_dir_all(&out).unwrap();
    let disc = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let cell = (480u32, 324u32);
    let mut scenes = scenes();
    if let Some(only) = &only {
        scenes.retain(|(name, _)| only.iter().any(|o| o == name));
    }
    let mut sheet = vec![0u8; (cell.0 * LOOKS.len() as u32 * cell.1 * scenes.len() as u32 * 4) as usize];
    let mut report = serde_json::Map::new();
    for (row, (name, setup)) in scenes.iter().enumerate() {
        let (mut session, _) = xem_game::boot(&disc, &Default::default()).expect("boot");
        for command in setup {
            let reply = session.execute(command);
            assert!(reply.get("error").is_none(), "{name}: {command} -> {reply}");
        }
        let start = session.snapshot_bytes().expect("snapshot");
        let scene = session.scene();
        let mut pictures = Vec::new();
        let mut hashes = Vec::new();
        for look in LOOKS {
            session.restore_bytes(&start).unwrap();
            let picture = draw(&gpu, &mut session, &graphics(look, scale));
            hashes.push(format!("{:016x}", session.memory_hash().unwrap()));
            write_png(&out.join(format!("{name}-{look}.png")), picture.size, &picture.rgba);
            write_png(&out.join(format!("{name}-console.png")), (picture.console.0, picture.console.1), &picture.console.2);
            pictures.push(picture);
        }
        assert!(hashes.iter().all(|h| *h == hashes[0]), "{name}: the looks changed the game: {hashes:?}");
        let modern = &pictures[0];
        let mut looks = serde_json::Map::new();
        for (look, picture) in LOOKS.iter().zip(&pictures) {
            let comparable = picture.size == modern.size;
            let changed = comparable.then(|| {
                picture.rgba.chunks(4).zip(modern.rgba.chunks(4)).filter(|(a, b)| a[..3] != b[..3]).count() as f64
                    / (picture.size.0 * picture.size.1) as f64
            });
            let interface = match (&modern.classes, comparable) {
                (Some(classes), true) => {
                    let (mut total, mut differ) = (0, 0);
                    for (i, class) in classes.iter().enumerate() {
                        if *class == 1 {
                            total += 1;
                            if picture.rgba[i * 4..i * 4 + 3] != modern.rgba[i * 4..i * 4 + 3] {
                                differ += 1;
                            }
                        }
                    }
                    json!({"pixels": total, "changed": differ})
                }
                _ => Value::Null,
            };
            looks.insert(
                look.to_string(),
                json!({"size": [picture.size.0, picture.size.1], "changed_vs_modern": changed, "interface": interface,
                       "interface_vs_console": against_console(picture, Some(&out.join(format!("{name}-{look}-ui.png")))),
                       "rendered": picture.classes.is_some()}),
            );
            let tile = fit(picture, cell);
            let column = LOOKS.iter().position(|l| l == look).unwrap() as u32;
            for y in 0..cell.1 {
                let from = (y * cell.0 * 4) as usize;
                let to = (((row as u32 * cell.1 + y) * cell.0 * LOOKS.len() as u32 + column * cell.0) * 4) as usize;
                sheet[to..to + (cell.0 * 4) as usize].copy_from_slice(&tile[from..from + (cell.0 * 4) as usize]);
            }
        }
        report.insert(name.to_string(), json!({"scene": scene, "memory_hash": hashes[0], "looks": looks}));
        println!("{name} ({scene}): {}", serde_json::to_string(&report[*name]["looks"]).unwrap());
    }
    write_png(&out.join("sheet.png"), (cell.0 * LOOKS.len() as u32, cell.1 * scenes.len() as u32), &sheet);
    std::fs::write(out.join("report.json"), serde_json::to_string_pretty(&json!({
        "looks": LOOKS, "scale": scale, "wide_side": wide_side((1920, 1080)), "scenes": report,
    })).unwrap()).unwrap();
    println!("{}", out.join("sheet.png").display());
}
