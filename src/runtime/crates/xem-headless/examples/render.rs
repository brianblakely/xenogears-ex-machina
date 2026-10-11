//! Renders the game's display with the wgpu PS1 renderer (local only: it
//! needs the user's disc 1 and the game module).
//!
//!     cargo run --release -p xem-headless --example render -- [--map N] [--frames N]
//!         [--scale N] [--original] [--out out.png]

use serde_json::json;
use xem_render::ps1::{DisplayArea, DrawOptions, Effects, EffectsRenderer, Fidelity, FrameRenderer, Lighting, Ps1Renderer, Quality};
use xem_render::{Gpu, read_rgba8};

fn main() {
    let mut map = 22u64;
    let mut frames = 400u64;
    let mut scale = 4u32;
    let mut original = false;
    let mut hd2d = false;
    let mut only: Option<String> = None;
    let mut out = std::path::PathBuf::from("render.png");
    let mut disc = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd");
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        let mut value = || args.next().expect("a value");
        match arg.as_str() {
            "--map" => map = value().parse().unwrap(),
            "--frames" => frames = value().parse().unwrap(),
            "--scale" => scale = value().parse().unwrap(),
            "--original" => original = true,
            "--hd2d" => hd2d = true,
            "--only" => { hd2d = true; only = Some(value()); }
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
    let mut renderer = Ps1Renderer::new(&gpu.device, &gpu.queue, scale);
    let fidelity = if original { Fidelity::ORIGINAL } else { Fidelity::MODERN };
    renderer.set_options(DrawOptions { fidelity, ..Default::default() }, scale);
    let device = session.runtime.services().gpu.device_mut();
    device.set_trace(true);
    for _ in 0..8 {
        session.frame().expect("frame");
        let trace = session.runtime.services().gpu.device_mut().take_trace();
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        renderer.draw(&mut encoder, &trace);
        gpu.queue.submit([encoder.finish()]);
    }
    let info = session.runtime.services().gpu.device().display();
    let area = DisplayArea { x: info.x.into(), y: info.y.into(), width: info.width, height: info.height };
    let mut frame = FrameRenderer::new(&gpu.device);
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    frame.prepare(&gpu.device, &mut encoder, &renderer, area);
    gpu.queue.submit([encoder.finish()]);
    let (width, height) = frame.size();
    let mut effects = EffectsRenderer::new(&gpu.device, Some(&gpu.adapter));
    let modern = read_rgba8(&gpu.device, &gpu.queue, frame.color());
    let rgba = if hd2d {
        let ops = renderer.frame_log(area).map(|log| log.ops.clone()).unwrap_or_default();
        println!("frame log: {} ops", ops.len());
        let analysis = xem_render::ps1::SceneAnalysis::of(&ops, area);
        for g in &analysis.sprites {
            println!("sprite group {:?} depth {} parts {}", g.bounds, g.depth, g.primitives.len());
        }
        println!("depths {:?} focus {:?}", analysis.depths, analysis.focus);
        let mut chosen = Effects::HD2D;
        if let Some(only) = &only {
            let full = Effects::HD2D;
            chosen = Effects { focus: 0.0, ..Effects::OFF };
            for name in only.split(',') {
                match name {
                    "lighting" => chosen.lighting = full.lighting,
                    "shadows" => chosen.shadows = full.shadows,
                    "ao" => chosen.ambient_occlusion = full.ambient_occlusion,
                    "fog" => chosen.fog = full.fog,
                    "shafts" => chosen.light_shafts = full.light_shafts,
                    "bloom" => chosen.bloom = full.bloom,
                    "grading" => chosen.color_grading = full.color_grading,
                    "dof" => chosen.depth_of_field = full.depth_of_field,
                    other => panic!("unknown effect {other}"),
                }
            }
        }
        effects.set_debug_view(match std::env::var("XEM_DEBUG_VIEW").as_deref() {
            Ok("shadows") => xem_render::ps1::DebugView::Shadows,
            Ok("depth") => xem_render::ps1::DebugView::Depth,
            Ok("class") => xem_render::ps1::DebugView::Class,
            _ => xem_render::ps1::DebugView::Off,
        });
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        effects.render(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, &frame, &ops, &chosen, Quality::High, &Lighting::for_scene(&format!("field:{map}")));
        gpu.queue.submit([encoder.finish()]);
        if std::env::var_os("XEM_SHADOW_MASK").is_some() {
            let mask = read_rgba8(&gpu.device, &gpu.queue, effects.shadow_mask().unwrap());
            let lit = mask.chunks(4).filter(|p| p[0] > 0).count();
            println!("shadow mask: {lit} pixels");
            let mut grey: Vec<u8> = mask.chunks(4).flat_map(|p| [p[0], p[1], p[2], 255]).collect();
            grey.truncate((width * height * 4) as usize);
            let file = std::fs::File::create(out.with_extension("mask.png")).unwrap();
            let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), width, height);
            encoder.set_color(png::ColorType::Rgba);
            encoder.write_header().unwrap().write_image_data(&grey).unwrap();
        }
        read_rgba8(&gpu.device, &gpu.queue, effects.output().unwrap())
    } else {
        read_rgba8(&gpu.device, &gpu.queue, frame.color())
    };
    let (width, rgba) = if hd2d {
        // Side by side: the plain frame, then the effects.
        let mut both = Vec::with_capacity(rgba.len() * 2);
        for (a, b) in modern.chunks(width as usize * 4).zip(rgba.chunks(width as usize * 4)) {
            both.extend_from_slice(a);
            both.extend_from_slice(b);
        }
        (width * 2, both)
    } else {
        (width, rgba)
    };
    let mut rgba = rgba;
    rgba.chunks_exact_mut(4).for_each(|p| p[3] = 255);
    let file = std::fs::File::create(&out).expect("output");
    let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), width, height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.write_header().unwrap().write_image_data(&rgba).unwrap();
    println!("{} ({width}x{height}, display {area:?})", out.display());
}
