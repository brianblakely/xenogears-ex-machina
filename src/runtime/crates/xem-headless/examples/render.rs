//! Renders the game's display through the hosts' `GameView` (local only:
//! it needs the user's disc 1 and the game module).
//!
//!     cargo run --release -p xem-headless --example render -- [--map N] [--frames N]
//!         [--scale N] [--profile modern|hd2d|ps1] [--only effect,...] [--widescreen]
//!         [--compare] [--out out.png]
//!
//! `--compare` puts the Modern picture beside the chosen profile's.
//! `--only` draws the Custom profile with just the named HD-2D effects
//! (lighting, shadows, ao, fog, shafts, bloom, grading, dof).
//! `XEM_DEBUG_VIEW=shadows|depth|class` shows an effects debug view.

use serde_json::json;
use xem_render::ps1::{DebugView, GameView, HostClass, ViewConfig};
use xem_render::{Gpu, read_rgba8};
use xem_settings::{EffectLevels, Graphics, GraphicsProfile};

type Session = xem_core::control::Session<xem_game::NativeModule>;

fn picture(gpu: &Gpu, view: &mut GameView, session: &mut Session, graphics: &Graphics, scene: &str) -> (u32, u32, Vec<u8>) {
    let config = ViewConfig::resolve(graphics, HostClass::Desktop, 224 * 4, view.max_scale());
    view.configure(config);
    session.runtime.services().gpu.device_mut().set_trace(true);
    let _ = view.take_restart();
    let mut size = None;
    for frame in 0..8 {
        session.frame().expect("frame");
        let trace = session.runtime.services().gpu.device_mut().take_trace();
        view.push(trace);
        let display = session.runtime.services().gpu.device().display();
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        size = view.present(&mut encoder, &display, scene, frame as f64 / 60.0).map(|p| p.size);
        gpu.queue.submit([encoder.finish()]);
    }
    let (width, height) = size.expect("a presented frame");
    let mut rgba = read_rgba8(&gpu.device, &gpu.queue, view.presented_texture().expect("presented"));
    rgba.chunks_exact_mut(4).for_each(|p| p[3] = 255);
    (width, height, rgba)
}

fn main() {
    let mut map = 22u64;
    let mut frames = 400u64;
    let mut graphics = Graphics::default();
    let mut compare = false;
    let mut out = std::path::PathBuf::from("render.png");
    let mut disc = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd");
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        let mut value = || args.next().expect("a value");
        match arg.as_str() {
            "--map" => map = value().parse().unwrap(),
            "--frames" => frames = value().parse().unwrap(),
            "--scale" => graphics.internal_scale = value().parse().unwrap(),
            "--profile" => {
                graphics.profile = match value().as_str() {
                    "modern" => GraphicsProfile::Modern,
                    "hd2d" => GraphicsProfile::Hd2d,
                    "ps1" => GraphicsProfile::Ps1,
                    other => panic!("unknown profile {other}"),
                }
            }
            "--only" => {
                graphics.profile = GraphicsProfile::Custom;
                let mut levels = EffectLevels::OFF;
                for name in value().split(',') {
                    let name = match name {
                        "ao" => "ambient_occlusion",
                        "shafts" => "light_shafts",
                        "grading" => "color_grading",
                        "dof" => "depth_of_field",
                        other => other,
                    };
                    levels.set(name, EffectLevels::HD2D.get(name).expect("an effect")).unwrap();
                }
                graphics.custom_effects = levels;
            }
            "--widescreen" => graphics.widescreen = true,
            "--compare" => compare = true,
            "--out" => out = value().into(),
            "--disc" => disc = value().into(),
            other => panic!("unknown option {other}"),
        }
    }
    let (mut session, _) = xem_game::boot(&disc, &Default::default()).expect("boot");
    if map != 0 {
        session.execute(&json!({"cmd": "enter_field", "map": map}));
    }
    session.execute(&json!({"cmd": "frames", "count": frames}));
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let debug = match std::env::var("XEM_DEBUG_VIEW").as_deref() {
        Ok("shadows") => DebugView::Shadows,
        Ok("depth") => DebugView::Depth,
        Ok("class") => DebugView::Class,
        _ => DebugView::Off,
    };
    let scene = session.scene();
    let snapshot = session.snapshot_bytes().expect("snapshot");
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    view.effects_mut().set_debug_view(debug);
    let (width, height, rgba) = picture(&gpu, &mut view, &mut session, &graphics, &scene);
    let (width, rgba) = if compare {
        // The same frames again under Modern.
        session.restore_bytes(&snapshot).expect("restore");
        let mut modern = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
        let plain = Graphics { profile: GraphicsProfile::Modern, widescreen: false, ..graphics };
        let (w, h, left) = picture(&gpu, &mut modern, &mut session, &plain, &scene);
        assert_eq!(h, height, "both pictures are as high");
        let mut both = Vec::with_capacity(left.len() + rgba.len());
        for (a, b) in left.chunks(w as usize * 4).zip(rgba.chunks(width as usize * 4)) {
            both.extend_from_slice(a);
            both.extend_from_slice(b);
        }
        (w + width, both)
    } else {
        (width, rgba)
    };
    let file = std::fs::File::create(&out).expect("output");
    let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), width, height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.write_header().unwrap().write_image_data(&rgba).unwrap();
    println!("{} ({width}x{height}, scene {scene})", out.display());
}
