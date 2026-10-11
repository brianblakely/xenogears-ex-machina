//! The PS1 renderer against xem-gpu's software rasterizer: at scale 1 with
//! every original limit, the same command words give (nearly) the same
//! pixels. Runs on whatever adapter wgpu finds (lavapipe in the runtime shell).

use xem_render::ps1::{DrawOptions, Fidelity, Ps1Renderer};
use xem_render::{Gpu, read_rgba8};

fn expand5(v: u16) -> u8 {
    let v = (v & 31) as u8;
    v << 3 | v >> 2
}

/// Fills a 4-bit texture page with a pattern and its CLUT.
fn upload_texture(gpu: &mut xem_gpu::Gpu) {
    // CLUT at (0, 480): 16 colours, entry 0 transparent, 15 semi.
    gpu.write_gp0_words(&[0xA000_0000, 480 << 16, 1 << 16 | 16]);
    let mut clut = [0u16; 16];
    for (i, c) in clut.iter_mut().enumerate().skip(1) {
        *c = (i as u16 * 2) | ((31 - i as u16 * 2) << 5) | ((i as u16 % 4) * 8) << 10;
    }
    clut[15] |= 0x8000;
    for pair in clut.chunks(2) {
        gpu.write_gp0(u32::from(pair[0]) | u32::from(pair[1]) << 16);
    }
    // Texture page 8 (x 512, y 0), 64x64 texels (16 VRAM pixels wide).
    gpu.write_gp0_words(&[0xA000_0000, 512, 64 << 16 | 16]);
    for y in 0..64u32 {
        for x in (0..16u32).step_by(2) {
            let px = |x: u32| -> u32 {
                let mut word = 0;
                for n in 0..4 {
                    let u = x * 4 + n;
                    word |= ((u / 4 + y / 4 + u * y / 64) % 16) << (4 * n);
                }
                word
            };
            gpu.write_gp0(px(x) | px(x + 1) << 16);
        }
    }
}

fn scene(gpu: &mut xem_gpu::Gpu) {
    // Drawing area 0,0-319,239, offset 0.
    gpu.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000, 0xE100_0200]);
    gpu.write_gp0_words(&[0x0200_2040, 0, 240 << 16 | 320]);
    // Flat triangle, Gouraud quad.
    gpu.write_gp0_words(&[0x2000_80FF, 10 << 16 | 10, 20 << 16 | 120, 110 << 16 | 40]);
    gpu.write_gp0_words(&[0x3800_00FF, 10 << 16 | 140, 0x0000_FF00, 15 << 16 | 300, 0x00FF_0000, 90 << 16 | 150, 0x00FF_FFFF, 100 << 16 | 290]);
    // Textured quad, page 8, CLUT (0,480), modulated.
    let clut = (480 << 6) << 16;
    let page = 8u32 << 16;
    gpu.write_gp0_words(&[0x2C80_8080, 120 << 16 | 20, clut, 130 << 16 | 120, page | 63, 220 << 16 | 25, 63 << 8, 210 << 16 | 110, 63 << 8 | 63]);
    // Semi-transparent untextured quads in the four modes.
    for mode in 0..4u32 {
        gpu.write_gp0(0xE100_0200 | mode << 5);
        let x = 130 + mode * 45;
        gpu.write_gp0_words(&[0x2A40_80C0, 20 << 16 | x, 20 << 16 | (x + 40), 100 << 16 | x, 100 << 16 | (x + 40)]);
    }
    // Sprite (raw textured rectangle) with semi texels, mode 1.
    gpu.write_gp0(0xE100_0028);
    gpu.write_gp0_words(&[0x6700_0000, 160 << 16 | 230, clut, 48 << 16 | 64]);
    // Lines.
    gpu.write_gp0_words(&[0x4000_FFFF, 200 << 16 | 20, 230 << 16 | 120]);
    gpu.write_gp0_words(&[0x5000_00FF, 210 << 16 | 130, 0x00FF_0000, 225 << 16 | 310]);
}

thread_local! {
    static DEVICE: std::cell::RefCell<xem_gpu::Gpu> = std::cell::RefCell::new(xem_gpu::Gpu::new());
}

/// The trace's first take (all of VRAM, as a trace starts) and the commands
/// `commands` sends after it; the device stays in `DEVICE`.
fn traced(commands: impl FnOnce(&mut xem_gpu::Gpu)) -> (xem_gpu::Trace, xem_gpu::Trace) {
    DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        *device = xem_gpu::Gpu::new();
        device.set_trace(true);
        let sync = device.take_trace();
        commands(&mut device);
        (sync, device.take_trace())
    })
}

#[test]
fn original_fidelity_matches_the_software_gpu() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let (sync, trace) = traced(|device| {
        upload_texture(device);
        scene(device);
    });
    assert!(trace.ops.len() > 10, "the commands are traced");
    let mut renderer = Ps1Renderer::new(&gpu.device, &gpu.queue, 1);
    renderer.set_options(DrawOptions { fidelity: Fidelity::ORIGINAL, ..Default::default() }, 1);
    for trace in [&sync, &trace] {
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        renderer.draw(&mut encoder, trace);
        gpu.queue.submit([encoder.finish()]);
    }
    let device = DEVICE.with(|d| d.borrow().clone());
    let pixels = read_rgba8(&gpu.device, &gpu.queue, renderer.picture().0);
    let vram = device.vram();
    let (mut differ, mut total) = (0, 0);
    let mut worst = 0;
    for y in 0..240 {
        for x in 0..320 {
            let want = vram[y * 1024 + x];
            let at = (y * 1024 + x) * 4;
            let got = &pixels[at..at + 3];
            let want = [expand5(want), expand5(want >> 5), expand5(want >> 10)];
            let d = (0..3).map(|c| (i32::from(got[c]) - i32::from(want[c])).abs()).max().unwrap();
            worst = worst.max(d);
            total += 1;
            if d > 8 {
                differ += 1;
                if std::env::var_os("XEM_PS1_DIFFS").is_some() {
                    eprintln!("diff at {x},{y}: got {got:?} want {want:?}");
                }
            }
        }
    }
    eprintln!("{differ} of {total} pixels differ by more than one 5-bit step (worst {worst})");
    let mut colors = std::collections::HashSet::new();
    for y in 0..240 {
        for x in 0..320 {
            let at = (y * 1024 + x) * 4;
            colors.insert([pixels[at], pixels[at + 1], pixels[at + 2]]);
        }
    }
    assert!(colors.len() > 64, "the scene draws: {} colours", colors.len());
    if let Some(path) = std::env::var_os("XEM_PS1_DUMP") {
        let mut ppm = format!("P6 320 240 255\n").into_bytes();
        for y in 0..240 {
            for x in 0..320 {
                let at = (y * 1024 + x) * 4;
                ppm.extend_from_slice(&pixels[at..at + 3]);
            }
        }
        std::fs::write(path, ppm).unwrap();
    }
    // Edges and interpolation rounding may differ; the picture may not.
    assert!(differ * 100 < total, "{differ} of {total} pixels differ");
}

use xem_gpu::{Class, Environment, Primitive, Projection, Shape, TraceOp, TraceVertex};
use xem_render::ps1::{
    Culling, DisplayArea, Effects, EffectsRenderer, FrameRenderer, Framing, Lighting, Quality, interpolate,
};

/// A packed GP0 vertex word.
fn xy(x: i32, y: i32) -> u32 {
    (x as u32 & 0xFFFF) | ((y as u32 & 0xFFFF) << 16)
}

/// A scene drawn through the GTE: a textured quad receding in depth (near
/// at the bottom), a dithered Gouraud triangle at sub-pixel positions, then
/// a far flat quad drawn after a near one where they overlap, and an
/// interface rectangle (no projection) on top.
fn projected_scene(gpu: &mut xem_gpu::Gpu) {
    upload_texture(gpu);
    gpu.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000, 0xE100_0200]);
    gpu.write_gp0_words(&[0x0200_0000, 0, 240 << 16 | 320]);
    let project = |x: f32, y: f32, z: u16| Projection {
        sxy: xy(x.floor() as i32, y.floor() as i32),
        sz: z,
        class: 1,
        x: (x * 65536.0) as i32,
        y: (y * 65536.0) as i32,
        offset: (160, 120),
        h: 256,
        owner: 0x8010_0000,
    };
    let quad = [(100.3, 20.6, 3000), (220.7, 20.2, 3000), (40.4, 200.9, 400), (280.6, 200.3, 400)];
    let tri = [(30.5, 30.5, 1000), (90.75, 40.25, 1000), (50.25, 110.6, 1000)];
    let near = [(150.2, 100.4, 500), (200.6, 100.3, 500), (150.4, 150.7, 500), (200.3, 150.2, 500)];
    let far = [(170.4, 120.6, 2500), (230.2, 120.3, 2500), (170.6, 170.2, 2500), (230.3, 170.7, 2500)];
    let mut projections = Vec::new();
    for &(x, y, z) in quad.iter().chain(&tri).chain(&near).chain(&far) {
        projections.push(project(x, y, z));
    }
    gpu.trace_projections(projections);
    let word = |(x, y, _): (f32, f32, u16)| xy(x.floor() as i32, y.floor() as i32);
    let clut = (480 << 6) << 16;
    let page = 8u32 << 16;
    gpu.write_gp0_words(&[0x2C80_8080, word(quad[0]), clut, word(quad[1]), page | 63, word(quad[2]), 63 << 8, word(quad[3]), 63 << 8 | 63]);
    gpu.write_gp0_words(&[0x3000_40C0, word(tri[0]), 0x00C0_4000, word(tri[1]), 0x0040_C080, word(tri[2])]);
    gpu.write_gp0_words(&[0x2800_20E0, word(near[0]), word(near[1]), word(near[2]), word(near[3])]);
    gpu.write_gp0_words(&[0x2800_E020, word(far[0]), word(far[1]), word(far[2]), word(far[3])]);
    // Interface: a flat rectangle drawn last.
    gpu.write_gp0_words(&[0x6000_FFFF, xy(250, 200), 30 << 16 | 60]);
}

fn draw(gpu: &Gpu, traces: &(xem_gpu::Trace, xem_gpu::Trace), fidelity: Fidelity, culling: Culling) -> (Ps1Renderer, Vec<u8>) {
    let mut renderer = Ps1Renderer::new(&gpu.device, &gpu.queue, 2);
    renderer.set_options(DrawOptions { fidelity, culling, ..Default::default() }, 2);
    for trace in [&traces.0, &traces.1] {
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        renderer.draw(&mut encoder, trace);
        gpu.queue.submit([encoder.finish()]);
    }
    let pixels = read_rgba8(&gpu.device, &gpu.queue, renderer.picture().0);
    (renderer, pixels)
}

#[test]
fn each_original_limit_changes_the_picture_on_its_own() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let trace = traced(projected_scene);
    let modern = Fidelity::MODERN;
    let (_, base) = draw(&gpu, &trace, modern, Culling::default());
    let variants = [
        ("affine textures", Fidelity { affine_textures: true, ..modern }),
        ("vertex snapping", Fidelity { vertex_snapping: true, ..modern }),
        ("15-bit colour", Fidelity { color_15bit: true, ..modern }),
        ("dithering", Fidelity { dithering: true, ..modern }),
        ("depth-tested ordering", Fidelity { painter_ordering: false, ..modern }),
    ];
    let mut pictures = vec![base.clone()];
    for (name, fidelity) in variants {
        let (_, pixels) = draw(&gpu, &trace, fidelity, Culling::default());
        let differ = pixels.chunks(4).zip(base.chunks(4)).filter(|(a, b)| a != b).count();
        eprintln!("{name}: {differ} pixels differ from Modern");
        assert!(differ > 50, "{name} changes the picture");
        assert!(pictures.iter().all(|p| *p != pixels), "{name} is its own picture");
        pictures.push(pixels);
    }
    // Native resolution draws at scale 1 whatever is asked for.
    let (renderer, _) = draw(&gpu, &trace, Fidelity { native_resolution: true, ..modern }, Culling::default());
    assert_eq!(renderer.scale(), 1);
    // Depth-tested ordering keeps the near quad over the far one drawn after it.
    let at = |pixels: &[u8], x: usize, y: usize| pixels[(y * 2048 + x) * 4..(y * 2048 + x) * 4 + 3].to_vec();
    let (_, depth) = draw(&gpu, &trace, Fidelity { painter_ordering: false, ..modern }, Culling::default());
    assert_ne!(at(&base, 2 * 185, 2 * 140), at(&depth, 2 * 185, 2 * 140), "painter's order and depth order differ where the quads overlap");
}

#[test]
fn culling_is_off_by_default_and_counted_apart_from_the_gpu_limits() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let trace = traced(|device| {
        projected_scene(device);
        // Outside the drawing area, and a triangle beyond the GPU's size limit.
        device.write_gp0_words(&[0x2000_FFFF, xy(400, 10), xy(420, 10), xy(410, 30)]);
        device.write_gp0_words(&[0x2000_FFFF, xy(-600, 10), xy(500, 10), xy(0, 100)]);
    });
    let (plain, plain_pixels) = draw(&gpu, &trace, Fidelity::MODERN, Culling::default());
    let stats = plain.stats();
    assert_eq!((stats.culled_back_faces, stats.culled_outside_frame), (0, 0), "nothing is culled by default");
    assert_eq!(stats.rejected_size, 1, "the GPU's own size limit applies always");
    let (culled, culled_pixels) = draw(&gpu, &trace, Fidelity::MODERN, Culling { back_faces: false, outside_frame: true });
    assert_eq!(culled.stats().culled_outside_frame, 1);
    assert_eq!(culled_pixels, plain_pixels, "skipping what the drawing area clips changes nothing");
    let (back, _) = draw(&gpu, &trace, Fidelity::MODERN, Culling { back_faces: true, outside_frame: false });
    assert!(back.stats().culled_back_faces > 0, "the minority winding is dropped when asked");
}

#[test]
fn interface_pixels_bypass_the_effects() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let trace = traced(projected_scene);
    let (mut renderer, _) = draw(&gpu, &trace, Fidelity::MODERN, Culling::default());
    let area = DisplayArea { x: 0, y: 0, width: 320, height: 240 };
    let mut frame = FrameRenderer::new(&gpu.device);
    let mut effects = EffectsRenderer::new(&gpu.device, Some(&gpu.adapter));
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, area, Framing::default());
    gpu.queue.submit([encoder.finish()]);
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    let ops = trace.1.ops.clone();
    effects.render(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, &frame, &ops, &Effects::HD2D, Quality::High, &Lighting::default());
    gpu.queue.submit([encoder.finish()]);
    let plain = read_rgba8(&gpu.device, &gpu.queue, frame.color());
    let lit = read_rgba8(&gpu.device, &gpu.queue, effects.output().unwrap());
    let at = |pixels: &[u8], x: usize, y: usize| pixels[(y * 640 + x) * 4..(y * 640 + x) * 4 + 3].to_vec();
    // The interface rectangle (250..310, 200..230 at scale 2) is untouched.
    for (x, y) in [(520, 410), (600, 450), (560, 430)] {
        assert_eq!(at(&lit, x, y), at(&plain, x, y), "interface pixel {x},{y}");
    }
    // The scene is graded and lit.
    let changed = (0..640 * 400).filter(|i| plain[i * 4..i * 4 + 3] != lit[i * 4..i * 4 + 3]).count();
    assert!(changed > 10_000, "the effects change the scene ({changed} pixels)");
}

fn primitive(x: i32, y: i32, u: i16) -> TraceOp {
    let vertex = |dx: i32, dy: i32| TraceVertex {
        x: x + dx,
        y: y + dy,
        fx: (x + dx) as f32,
        fy: (y + dy) as f32,
        z: 1000.0,
        color: 0x80_8080,
        u,
        v: 0,
    };
    TraceOp::Draw(Primitive {
        shape: Shape::Triangle,
        vertices: [vertex(0, 0), vertex(10, 0), vertex(0, 10), TraceVertex::default()],
        textured: true,
        raw: false,
        gouraud: false,
        semi: false,
        dither: false,
        clut: 0,
        environment: Environment { draw_mode: 8, texture_window: 0, area: [0, 0, 319, 239], mask: 0 },
        class: Class::Model,
        projection: Some(((160, 120), 256)),
        owner: 0,
        source: 0,
    })
}

#[test]
fn interpolation_moves_matched_primitives_and_leaves_cuts() {
    let x = |op: &TraceOp| match op {
        TraceOp::Draw(p) => p.vertices[0].fx,
        _ => f32::NAN,
    };
    // The same primitive (texture coordinates 4) moved 20 pixels right; one
    // new (16) and one that jumped across the screen (32).
    let previous = vec![primitive(10, 10, 4), primitive(10, 50, 32)];
    let newest = vec![primitive(30, 10, 4), primitive(100, 100, 16), primitive(250, 50, 32)];
    let half = interpolate(&newest, &previous, 0.5);
    assert_eq!(x(&half[0]), 20.0, "halfway between its two positions");
    assert_eq!(x(&half[1]), 100.0, "a new primitive stays where it is");
    assert_eq!(x(&half[2]), 250.0, "a jump is a cut, not motion");
    assert_eq!(x(&interpolate(&newest, &previous, 0.0)[0]), 10.0);
    assert_eq!(x(&interpolate(&newest, &previous, 1.0)[0]), 30.0);
}
