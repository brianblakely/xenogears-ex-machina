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

use xem_gpu::{Class, Environment, Primitive, Projection, Reveal, Shape, SxyStore, TraceOp, TraceVertex};
use xem_render::ps1::{
    Culling, DisplayArea, Effects, EffectsRenderer, FrameRenderer, Framing, Lighting, Quality, interpolate, pull_back,
};

/// A packed GP0 vertex word.
fn xy(x: i32, y: i32) -> u32 {
    (x as u32 & 0xFFFF) | ((y as u32 & 0xFFFF) << 16)
}

/// The view-space point the GTE (offset 160, 120; H 256) projects to the
/// screen point (x, y) at depth z.
fn view(x: f32, y: f32, z: f32) -> [i32; 3] {
    [((x - 160.0) * z / 256.0) as i32, ((y - 120.0) * z / 256.0) as i32, z as i32]
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
        view: view(x, y, f32::from(z)),
    };
    let quad = [(100.3, 20.6, 3000), (220.7, 20.2, 3000), (40.4, 200.9, 400), (280.6, 200.3, 400)];
    let tri = [(30.5, 30.5, 1000), (90.75, 40.25, 1000), (50.25, 110.6, 1000)];
    let near = [(150.2, 100.4, 500), (200.6, 100.3, 500), (150.4, 150.7, 500), (200.3, 150.2, 500)];
    let far = [(170.4, 120.6, 2500), (230.2, 120.3, 2500), (170.6, 170.2, 2500), (230.3, 170.7, 2500)];
    let mut projections = Vec::new();
    for &(x, y, z) in quad.iter().chain(&tri).chain(&near).chain(&far) {
        projections.push(project(x, y, z));
    }
    let word = |(x, y, _): (f32, f32, u16)| xy(x.floor() as i32, y.floor() as i32);
    let clut = (480 << 6) << 16;
    let page = 8u32 << 16;
    // The packets go by DMA, each point stored by the game from its
    // projection (as the port records it).
    let packets: [(Vec<u32>, Vec<(usize, usize)>); 4] = [
        (vec![0x2C80_8080, word(quad[0]), clut, word(quad[1]), page | 63, word(quad[2]), 63 << 8, word(quad[3]), 63 << 8 | 63], vec![(1, 0), (3, 1), (5, 2), (7, 3)]),
        (vec![0x3000_40C0, word(tri[0]), 0x00C0_4000, word(tri[1]), 0x0040_C080, word(tri[2])], vec![(1, 4), (3, 5), (5, 6)]),
        (vec![0x2800_20E0, word(near[0]), word(near[1]), word(near[2]), word(near[3])], vec![(1, 7), (2, 8), (3, 9), (4, 10)]),
        (vec![0x2800_E020, word(far[0]), word(far[1]), word(far[2]), word(far[3])], vec![(1, 11), (2, 12), (3, 13), (4, 14)]),
    ];
    submit(gpu, &projections, &packets);
    // Interface: a flat rectangle drawn last.
    gpu.write_gp0_words(&[0x6000_FFFF, xy(250, 200), 30 << 16 | 60]);
}

/// Game memory for DMA in the tests: 64 KB of words.
struct Ram(Vec<u32>);

impl xem_gpu::Memory for Ram {
    fn read_u32(&self, address: u32) -> u32 {
        self.0[(address as usize & 0xFFFF) >> 2]
    }
}

/// `packets` as a DMA list from 0x1000 (a node each), their words at
/// `(word index, projection index)` stored from those projections.
fn submit(gpu: &mut xem_gpu::Gpu, projections: &[Projection], packets: &[(Vec<u32>, Vec<(usize, usize)>)]) {
    let mut ram = Ram(vec![0; 0x4000]);
    let mut stores = Vec::new();
    let mut node = 0x1000u32;
    for (i, (words, points)) in packets.iter().enumerate() {
        let next = if i + 1 == packets.len() { 0xFF_FFFF } else { node + 0x100 };
        ram.0[node as usize >> 2] = (words.len() as u32) << 24 | next;
        for (k, &w) in words.iter().enumerate() {
            ram.0[(node as usize >> 2) + 1 + k] = w;
        }
        for &(at, projection) in points {
            stores.push(SxyStore { address: node + 4 + 4 * at as u32, value: words[at], source: Some(projection as u32) });
        }
        node += 0x100;
    }
    gpu.trace_projections(0, projections, &stores, &[]);
    gpu.dma_linked_list(&ram, 0x1000, 1 << 16).unwrap();
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
        // A triangle beyond the GPU's size limit.
        device.write_gp0_words(&[0x2000_FFFF, xy(-600, 10), xy(500, 10), xy(0, 100)]);
    });
    let (plain, _) = draw(&gpu, &trace, Fidelity::MODERN, Culling::default());
    let stats = plain.stats();
    assert_eq!(stats.culled_back_faces, 0, "nothing is culled by default");
    assert_eq!(stats.rejected_size, 1, "the GPU's own size limit applies always");
    let (back, _) = draw(&gpu, &trace, Fidelity::MODERN, Culling { back_faces: true, viewport: false });
    assert!(back.stats().culled_back_faces > 0, "the minority winding is dropped when asked");
}

/// A frame whose OT entry carries a face the game culled beside its
/// screen (a red triangle spanning x 330-360), and a drawn blue one.
fn revealing_scene() -> (xem_gpu::Trace, xem_gpu::Trace) {
    let mut ram = Ram(vec![0; 0x4000]);
    let mut put = |address: usize, words: &[u32]| ram.0[address >> 2..(address >> 2) + words.len()].copy_from_slice(words);
    put(0x100, &[0x0000_0200]);
    put(0x200, &[0x04FF_FFFF, 0x2000_FF00, xy(100, 50), xy(140, 50), xy(100, 90)]);
    let project = |x: f32, y: f32| Projection { sxy: xy(x as i32, y as i32), sz: 1000, class: 0, x: (x * 65536.0) as i32, y: (y * 65536.0) as i32, offset: (160, 120), h: 256, owner: 0, view: view(x, y, 1000.0) };
    let projections = [project(330.0, 50.0), project(360.0, 50.0), project(330.0, 90.0)];
    let reveal = Reveal { entry: 0x100, vertices: 3, projections: 3, order: 0, words: xem_gpu::FaceWords::from(&[0x2000_00FF, xy(330, 50), xy(360, 50), xy(330, 90)][..]) };
    traced(|device| {
        device.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000]);
        device.write_gp0_words(&[0x0200_0000, 0, 240 << 16 | 320]);
        device.trace_projections(0, &projections, &[], &[reveal]);
        device.dma_linked_list(&ram, 0x100, 1000).unwrap();
    })
}

#[test]
fn culled_faces_show_beside_the_screen_and_wherever_a_pulled_back_camera_sees_them() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let trace = revealing_scene();
    let red_at = |culling: Culling, framing: Framing| -> Vec<u8> {
        let (mut renderer, picture) = draw(&gpu, &trace, Fidelity::MODERN, culling);
        // The VRAM picture never shows the culled face.
        assert!(!(0..2048 * 480).any(|i| picture[i * 4..i * 4 + 3] == [255, 0, 0]), "no culled face in VRAM");
        let mut frame = FrameRenderer::new(&gpu.device);
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, DisplayArea { x: 0, y: 0, width: 320, height: 240 }, framing);
        gpu.queue.submit([encoder.finish()]);
        let (width, height) = frame.size();
        let pixels = read_rgba8(&gpu.device, &gpu.queue, frame.color());
        (0..width * height).filter(|&i| pixels[i as usize * 4..i as usize * 4 + 3] == [255, 0, 0]).map(|i| (i % width) as u8 / 1).collect()
    };
    let wide = Framing { side: 60, ..Default::default() };
    assert!(!red_at(Culling::default(), wide).is_empty(), "wider framing shows the culled face in its side");
    assert!(red_at(Culling { viewport: true, ..Default::default() }, wide).is_empty(), "viewport culling keeps the game's gaps");
    assert!(!red_at(Culling::default(), Framing { pull_back: 1000.0, ..Default::default() }).is_empty(), "pulled back, it falls inside the frame");
}

#[test]
fn pulling_back_moves_the_scene_towards_the_projection_centre_and_leaves_the_interface() {
    let scene = primitive(260, 120, 0);
    let mut interface = primitive(260, 120, 0);
    if let TraceOp::Draw(p) = &mut interface {
        p.class = Class::Flat;
        p.projection = None;
    }
    let mut pulled = vec![scene, interface.clone()];
    pull_back(&mut pulled, 1000.0);
    let TraceOp::Draw(p) = &pulled[0] else { unreachable!() };
    // z 1000 seen from 1000 further back: halfway to the centre (160, 120).
    assert_eq!((p.vertices[0].fx, p.vertices[0].fy, p.vertices[0].z), (210.0, 120.0, 2000.0));
    assert_eq!(pulled[1], interface);
}

#[test]
fn a_pulled_back_camera_projects_points_from_view_space_and_drops_what_lies_behind_it() {
    // Points before the GTE's division: one in front of the game's camera,
    // one behind it (its screen point is the saturated garbage the GTE left,
    // its depth clamped to 1), and, in a second triangle, one that is
    // behind the pulled-back camera too.
    let with_view = |op: TraceOp, points: [[f32; 3]; 3]| {
        let TraceOp::Draw(mut p) = op else { unreachable!() };
        p.projection = Some(((160, 120), 256));
        for (v, point) in p.vertices.iter_mut().zip(points) {
            v.view = Some(point);
        }
        TraceOp::Draw(p)
    };
    let mut ops = vec![
        with_view(primitive(0, 0, 0), [[100.0, 0.0, 1000.0], [-200.0, 50.0, -500.0], [0.0, 0.0, 1000.0]]),
        with_view(primitive(0, 0, 0), [[0.0, 0.0, 1000.0], [0.0, 0.0, -1950.0], [10.0, 0.0, 1000.0]]),
    ];
    pull_back(&mut ops, 2000.0);
    assert_eq!(ops.len(), 1, "the triangle reaching behind the pulled-back camera's near plane is dropped");
    let TraceOp::Draw(p) = &ops[0] else { unreachable!() };
    let points: Vec<_> = p.vertices[..3].iter().map(|v| (v.fx, v.fy, v.z)).collect();
    // x = 160 + 256 * X / (Z + 2000), y likewise.
    assert_eq!(points, [(160.0 + 256.0 * 100.0 / 3000.0, 120.0, 3000.0), (160.0 - 256.0 * 200.0 / 1500.0, 120.0 + 256.0 * 50.0 / 1500.0, 1500.0), (160.0, 120.0, 3000.0)]);
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
        view: None,
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
        environment: Environment { draw_mode: 8, texture_window: 0, area: [0, 0, 319, 239], mask: 0, offset: (0, 0) },
        class: Class::Model,
        projection: Some(((160, 120), 256)),
        owner: 0,
        source: 0,
        revealed: false,
        ground: None,
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

#[test]
fn the_interface_draws_as_the_original_at_any_scale() {
    // The 2D scene (windows, text sprites, bars, slanted polygons, lines: no
    // projections) at three times the original under Modern against the
    // software GPU's own pixels scaled up: whole native pixels alike, slanted
    // edges and lines included, but for semi-transparent blends (the GPU
    // blends at 5 bits) and a few texels and colours its exact interpolation
    // rounds the other way.
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let (sync, trace) = traced(|device| {
        upload_texture(device);
        scene(device);
    });
    let mut renderer = Ps1Renderer::new(&gpu.device, &gpu.queue, 3);
    renderer.set_options(DrawOptions { fidelity: Fidelity::MODERN, ..Default::default() }, 3);
    for trace in [&sync, &trace] {
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        renderer.draw(&mut encoder, trace);
        gpu.queue.submit([encoder.finish()]);
    }
    let pixels = read_rgba8(&gpu.device, &gpu.queue, renderer.picture().0);
    let vram = DEVICE.with(|d| d.borrow().vram().to_vec());
    let (mut differ, mut total) = (0, 0);
    for y in 0..240 * 3 {
        for x in 0..320 * 3 {
            let want = vram[(y / 3) * 1024 + x / 3];
            let want = [expand5(want), expand5(want >> 5), expand5(want >> 10)];
            let at = (y * 3072 + x) * 4;
            total += 1;
            if (0..3).any(|c| (i32::from(pixels[at + c]) - i32::from(want[c])).abs() > 8) {
                differ += 1;
            }
        }
    }
    eprintln!("{differ} of {total} scaled pixels differ from the original's");
    assert!(differ * 1000 < total, "{differ} of {total} pixels differ");
}

#[test]
fn a_jumping_sprite_leaves_its_shadow_on_the_floor() {
    // A floor across the screen and a sprite whose feet are at y 100 but
    // whose floor point (the game's ground under it) is at y 140: it jumps.
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let project = |x: f32, y: f32, z: u16| Projection { sxy: xy(x as i32, y as i32), sz: z, class: 0, x: (x * 65536.0) as i32, y: (y * 65536.0) as i32, offset: (160, 120), h: 256, owner: 0, view: view(x, y, f32::from(z)) };
    let sprite = |x: f32, y: f32| Projection { class: 2, owner: 0x8010_0000, ..project(x, y, 1000) };
    let ground = Projection { class: 3, owner: 0x8010_0000, ..project(160.0, 140.0, 1000) };
    let projections = [
        project(0.0, 0.0, 1000), project(319.0, 0.0, 1000), project(0.0, 239.0, 1000), project(319.0, 239.0, 1000),
        ground,
        sprite(150.0, 60.0), sprite(170.0, 60.0), sprite(150.0, 100.0), sprite(170.0, 100.0),
    ];
    let (sync, trace) = traced(|device| {
        device.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000]);
        let floor = vec![0x2860_6060, xy(0, 0), xy(319, 0), xy(0, 239), xy(319, 239)];
        let body = vec![0x2880_8080, xy(150, 60), xy(170, 60), xy(150, 100), xy(170, 100)];
        submit(device, &projections, &[(floor, vec![(1, 0), (2, 1), (3, 2), (4, 3)]), (body, vec![(1, 5), (2, 6), (3, 7), (4, 8)])]);
    });
    let mut renderer = Ps1Renderer::new(&gpu.device, &gpu.queue, 2);
    renderer.set_options(DrawOptions::default(), 2);
    for trace in [&sync, &trace] {
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        renderer.draw(&mut encoder, trace);
        gpu.queue.submit([encoder.finish()]);
    }
    let area = DisplayArea { x: 0, y: 0, width: 320, height: 240 };
    let mut frame = FrameRenderer::new(&gpu.device);
    let mut effects = EffectsRenderer::new(&gpu.device, Some(&gpu.adapter));
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, area, Framing::default());
    let shadows = Effects { shadows: 1.0, ..Effects::OFF };
    effects.render(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, &frame, &trace.ops, &shadows, Quality::High, &Lighting::default());
    gpu.queue.submit([encoder.finish()]);
    let mask = read_rgba8(&gpu.device, &gpu.queue, effects.shadow_mask().unwrap());
    let at = |x: usize, y: usize| mask[(y * 2 * 640 + x * 2) * 4];
    // A body's height up, the contact shadow is fainter (0.4 of its strength).
    assert!(at(160, 139) > 60, "the contact shadow lies at the floor point ({})", at(160, 139));
    assert_eq!(at(160, 101), 0, "not at the raised feet");
}

/// Enhanced movie frames: converted as the MDEC converts, and colour kept
/// on its side of a brightness edge through the GPU's upsampling and
/// enlargement.
#[test]
fn movie_frames_convert_as_the_mdec_and_keep_colour_on_its_side() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    // FSR 1's enlargement, and the bilinear one of a GPU that rasterizes in
    // software.
    for software in [false, true] {
        check_movie_frame(&gpu, software);
    }
}

fn check_movie_frame(gpu: &Gpu, software: bool) {
    use std::sync::Arc;
    use xem_gpu::{DisplayInfo, MovieBlock};
    use xem_render::ps1::MovieRenderer;
    // A dark grey macroblock beside a bright red one.
    let grey = MovieBlock { luma: [-60; 256], cb: [0; 64], cr: [0; 64], quant: [2; 6], bits24: true };
    let red = MovieBlock { luma: [60; 256], cb: [0; 64], cr: [80; 64], quant: [2; 6], bits24: true };
    let mut movie = MovieRenderer::new(software);
    movie.observe(&[
        TraceOp::Movie { x: 0, y: 0, block: Arc::new(grey) },
        TraceOp::Movie { x: 24, y: 0, block: Arc::new(red) },
    ]);
    let display = DisplayInfo { enabled: true, x: 0, y: 0, width: 32, height: 16, mode_width: 320, color24: true, interlaced: false, pal: false };
    assert!(movie.shows(&display));
    // The native VRAM, the MDEC's own picture there blue (24-bit pixels
    // 0, 0, 240: three bytes each across the 16-bit words).
    let vram = gpu.device.create_texture(&wgpu::TextureDescriptor {
        label: None,
        size: wgpu::Extent3d { width: 1024, height: 512, depth_or_array_layers: 1 },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format: wgpu::TextureFormat::R16Uint,
        usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
        view_formats: &[],
    });
    let row: Vec<u8> = [0u8, 0, 240].repeat(32);
    let words: Vec<u8> = row.repeat(16);
    gpu.queue.write_texture(
        wgpu::TexelCopyTextureInfo { texture: &vram, mip_level: 0, origin: wgpu::Origin3d::ZERO, aspect: wgpu::TextureAspect::All },
        &words,
        wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(96), rows_per_image: Some(16) },
        wgpu::Extent3d { width: 48, height: 16, depth_or_array_layers: 1 },
    );
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    let (_, size, new) = movie.present(&gpu.device, &gpu.queue, &mut encoder, &display, (128, 72), &vram).expect("a movie frame");
    assert_eq!((size, new), ((128, 64), true));
    gpu.queue.submit([encoder.finish()]);
    let pixels = read_rgba8(&gpu.device, &gpu.queue, movie.output().expect("presented"));
    let pixel = |x: usize, y: usize| -> [i32; 3] { std::array::from_fn(|c| i32::from(pixels[(y * 128 + x) * 4 + c])) };
    // R = Y + 1.402 Cr and so on, plus 128, clamped: grey 68; red (255, 131, 188).
    let near = |got: [i32; 3], want: [i32; 3]| got.iter().zip(want).all(|(g, w)| (g - w).abs() <= 2);
    assert!(near(pixel(8, 32), [68, 68, 68]), "grey {:?} (software {software})", pixel(8, 32));
    assert!(near(pixel(120, 32), [255, 131, 188]), "red {:?} (software {software})", pixel(120, 32));
    // Two native pixels either side of the edge (output pixels 52 and 76):
    // no red on the dark side, all of it on the bright one.
    let redness = |[r, g, _]: [i32; 3]| r - g;
    assert!(redness(pixel(52, 32)) < 8, "dark side tinted {:?} (software {software})", pixel(52, 32));
    assert!(redness(pixel(76, 32)) > 110, "bright side {:?} (software {software})", pixel(76, 32));
    // Five frames from the end of a movie that ends on the game's picture,
    // a third of the rebuilt one over the MDEC's own; at its last frame, the
    // MDEC's own alone (the device's picture: none rebuilt).
    movie.observe(&[TraceOp::VBlank, TraceOp::MovieFramesLeft(5)]);
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    assert!(movie.present(&gpu.device, &gpu.queue, &mut encoder, &display, (128, 72), &vram).expect("a movie frame").2);
    gpu.queue.submit([encoder.finish()]);
    let pixels = read_rgba8(&gpu.device, &gpu.queue, movie.output().expect("presented"));
    let pixel = |x: usize, y: usize| -> [i32; 3] { std::array::from_fn(|c| i32::from(pixels[(y * 128 + x) * 4 + c])) };
    assert!(near(pixel(8, 32), [23, 23, 183]), "grey a third over blue {:?} (software {software})", pixel(8, 32));
    movie.observe(&[TraceOp::VBlank, TraceOp::MovieFramesLeft(0)]);
    assert!(!movie.shows(&display));
}
