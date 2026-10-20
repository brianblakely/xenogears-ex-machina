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

/// [`traced`]'s on one device, a trace for each of `takes` in turn: frames
/// a host takes from the device a presentation at a time.
fn traced_takes(takes: &[fn(&mut xem_gpu::Gpu)]) -> (xem_gpu::Trace, Vec<xem_gpu::Trace>) {
    DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        *device = xem_gpu::Gpu::new();
        device.set_trace(true);
        let sync = device.take_trace();
        let traces = takes.iter().map(|take| {
            take(&mut device);
            device.take_trace()
        });
        (sync, traces.collect())
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

use xem_gpu::{Camera, CameraMode, Class, Environment, Placement, Primitive, Projection, Reveal, Reveals, Shape, SxyStore, TraceOp, TraceVertex};
use xem_render::ps1::{
    Culling, DisplayArea, Effects, EffectsRenderer, FrameRenderer, Framing, Lighting, Look, Quality, first_person, interpolate, pull_back, weld,
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

/// A model's triangle drawn through the GTE wound against every face of
/// [`projected_scene`] (facing away), at x 240-300, y 30-90.
fn back_facing_triangle(gpu: &mut xem_gpu::Gpu) {
    let points = [(250.5, 30.5), (240.5, 90.5), (300.5, 60.5)];
    let projections: Vec<Projection> = points
        .iter()
        .map(|&(x, y)| Projection {
            sxy: xy(x as i32, y as i32),
            sz: 800,
            class: 1,
            x: (x * 65536.0) as i32,
            y: (y * 65536.0) as i32,
            offset: (160, 120),
            h: 256,
            owner: 0x8010_0000,
            view: view(x, y, 800.0),
        })
        .collect();
    let word = |(x, y): (f32, f32)| xy(x as i32, y as i32);
    submit(gpu, &projections, &[(vec![0x2000_FFFF, word(points[0]), word(points[1]), word(points[2])], vec![(1, 0), (2, 1), (3, 2)])]);
}

#[test]
fn culling_is_off_by_default_and_counted_apart_from_the_gpu_limits() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let trace = traced(|device| {
        projected_scene(device);
        back_facing_triangle(device);
        // A triangle beyond the GPU's size limit.
        device.write_gp0_words(&[0x2000_FFFF, xy(-600, 10), xy(500, 10), xy(0, 100)]);
    });
    let (plain, all) = draw(&gpu, &trace, Fidelity::MODERN, Culling::default());
    let stats = plain.stats();
    assert_eq!(stats.culled_back_faces, 0, "nothing is culled by default");
    assert_eq!(stats.rejected_size, 1, "the GPU's own size limit applies always");
    let (back, culled) = draw(&gpu, &trace, Fidelity::MODERN, Culling { back_faces: true, viewport: false });
    assert_eq!(back.stats().culled_back_faces, 1, "the face wound against the frame's majority is dropped when asked, and only it");
    // The quads facing the camera keep both their triangles (a quad's
    // second triangle runs 1-2-3, the other way round from its first):
    // the picture changes only where the dropped face was.
    let mut changed = 0;
    for (i, (a, b)) in all.chunks(4).zip(culled.chunks(4)).enumerate() {
        let (x, y) = (i % 2048 / 2, i / 2048 / 2);
        if a != b {
            assert!((240..=301).contains(&x) && (30..=91).contains(&y), "({x}, {y}) changed beside the dropped face");
            changed += 1;
        }
    }
    assert!(changed > 1000, "the dropped face's pixels change ({changed})");
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
fn copying_a_display_buffer_keeps_the_presented_zoom_and_sides_under_pause_uploads() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let trace = traced(projected_scene);
    let (mut renderer, _) = draw(&gpu, &trace, Fidelity::MODERN, Culling::default());
    let area = DisplayArea { x: 0, y: 0, width: 320, height: 240 };
    renderer.set_display(xem_gpu::VramRect { x: 0, y: 0, width: 320, height: 240 });
    renderer.set_display(xem_gpu::VramRect { x: 0, y: 256, width: 320, height: 240 });
    let mut frame = FrameRenderer::new(&gpu.device);
    let framing = Framing { side: 40, pull_back: 1000.0, ..Default::default() };
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, area, framing);
    frame.draw_overlay(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, None);
    gpu.queue.submit([encoder.finish()]);
    let expected = read_rgba8(&gpu.device, &gpu.queue, frame.color());
    renderer.set_presented(Some(xem_render::ps1::PresentedPicture { texture: frame.color().clone(), side: 40, scale: 2 }));

    // Pause takes the current screen into the other display buffer. Later
    // drawing may reuse the source, and PAUSE is an ordinary small upload.
    let copied = DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        device.write_gp0_words(&[0x8000_0000, xy(0, 0), xy(0, 256), 240 << 16 | 320]);
        device.write_gp0_words(&[0x0200_FF00, xy(0, 0), 240 << 16 | 320]);
        device.write_gp0_words(&[0xA000_0000, xy(136, 356), 1 << 16 | 2, 0x7FFF_7FFF]);
        device.take_trace()
    });
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    renderer.draw(&mut encoder, &copied);
    frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, DisplayArea { y: 256, ..area }, framing);
    frame.draw_overlay(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, None);
    gpu.queue.submit([encoder.finish()]);
    let paused = read_rgba8(&gpu.device, &gpu.queue, frame.color());
    let (width, _) = frame.size();
    for (i, (before, after)) in expected.chunks_exact(4).zip(paused.chunks_exact(4)).enumerate() {
        let (x, y) = (i as u32 % width, i as u32 / width);
        if (352..356).contains(&x) && (200..202).contains(&y) {
            assert_eq!(&after[..3], [255, 255, 255], "the pause label stays over the held picture");
        } else {
            assert_eq!(before, after, "the displayed picture is preserved at ({x}, {y})");
        }
    }
}

#[test]
fn terrain_slivers_culled_at_whole_pixels_fill_seams_at_one_times_zoom() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let (sync, mut scene) = traced(|device| {
        device.write_gp0_words(&[0x0200_0000, 0, 240 << 16 | 320]);
    });
    let TraceOp::Draw(mut sliver) = primitive(0, 0, 0) else { unreachable!() };
    sliver.shape = Shape::Triangle;
    sliver.class = Class::World;
    sliver.revealed = true;
    sliver.textured = false;
    for (v, (x, fy)) in sliver.vertices.iter_mut().zip([(50, 100.1), (250, 100.1), (150, 100.9)]) {
        v.x = x;
        v.y = 100;
        v.fx = x as f32;
        v.fy = fy;
        v.color = 0x0000FF;
    }
    scene.ops.push(TraceOp::Draw(sliver));
    for snapping in [false, true] {
        let mut renderer = Ps1Renderer::new(&gpu.device, &gpu.queue, 2);
        renderer.set_options(DrawOptions { fidelity: Fidelity { vertex_snapping: snapping, ..Fidelity::MODERN }, ..Default::default() }, 2);
        renderer.set_seal(true);
        for trace in [&sync, &scene] {
            let mut encoder = gpu.device.create_command_encoder(&Default::default());
            renderer.draw(&mut encoder, trace);
            gpu.queue.submit([encoder.finish()]);
        }
        // Both the normal picture and a widescreen replay must restore
        // the sliver inside the original screen, without pulling back.
        let pixels = read_rgba8(&gpu.device, &gpu.queue, renderer.picture().0);
        assert_eq!(pixels[(201 * 2048 + 300) * 4] != 0, !snapping);
        let mut frame = FrameRenderer::new(&gpu.device);
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer,
            DisplayArea { x: 0, y: 0, width: 320, height: 240 }, Framing { side: 40, ..Default::default() });
        gpu.queue.submit([encoder.finish()]);
        let pixels = read_rgba8(&gpu.device, &gpu.queue, frame.color());
        assert_eq!(pixels[(201 * 800 + 380) * 4] != 0, !snapping);
    }
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

/// `op` with these points before the division (projected at (160, 120), H 256).
fn with_view(op: TraceOp, points: &[[f32; 3]]) -> TraceOp {
    let TraceOp::Draw(mut p) = op else { unreachable!() };
    p.projection = Some(((160, 120), 256));
    for (v, point) in p.vertices.iter_mut().zip(points) {
        v.view = Some(*point);
    }
    TraceOp::Draw(p)
}

/// The game's own camera as a view from within the scene.
const AHEAD: Look = Look {
    eye: [0.0; 3],
    axes: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
    up: [0.0, -1.0, 0.0],
    h: [256.0; 2],
    shift: [0.0; 2],
    near: 1.0,
    hide: 0,
    facing: None,
};

#[test]
fn looking_around_moves_the_scene_into_the_views_camera_and_leaves_the_interface() {
    let mut interface = primitive(260, 120, 0);
    if let TraceOp::Draw(p) = &mut interface {
        p.class = Class::Flat;
        p.projection = None;
    }
    // Behind the game's camera, and a triangle wholly behind the view's near plane.
    let mut ops = vec![
        with_view(primitive(0, 0, 0), &[[0.0, 0.0, -1000.0], [100.0, 0.0, -1000.0], [0.0, 100.0, -1000.0]]),
        with_view(primitive(0, 0, 0), &[[0.0, 0.0, 5.0], [10.0, 0.0, 5.0], [0.0, 10.0, 5.0]]),
        interface.clone(),
    ];
    // Turned around (forward along the game camera's -z, right along its -x).
    let around = Look { axes: [[-1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, -1.0]], near: 10.0, ..AHEAD };
    first_person(&mut ops, &around);
    assert_eq!(ops.len(), 2, "what lies wholly behind the view is dropped");
    let TraceOp::Draw(p) = &ops[0] else { unreachable!() };
    let expected = [(160.0, 120.0), (160.0 - 25.6, 120.0), (160.0, 120.0 + 25.6)];
    for (v, (x, y)) in p.vertices[..3].iter().zip(expected) {
        assert!((v.fx - x).abs() < 1e-3 && (v.fy - y).abs() < 1e-3 && v.z == 1000.0, "{v:?}");
        assert_eq!((v.x, v.y), (v.fx.floor() as i32, v.fy.floor() as i32));
    }
    assert_eq!(p.vertices[0].view, Some([0.0, 0.0, 1000.0]), "the point in the view's camera space");
    assert_eq!(ops[1], interface);
}

#[test]
fn sprites_stand_facing_the_view_at_their_feet_and_the_viewers_own_is_left_out() {
    let sprite = |owner: u32| {
        let TraceOp::Draw(mut p) = primitive(0, 0, 0) else { unreachable!() };
        p.shape = Shape::Quad;
        p.class = Class::Sprite;
        p.owner = owner;
        p.vertices[3] = p.vertices[0];
        // Facing the game's camera, a thousand units to its right.
        with_view(TraceOp::Draw(p), &[[950.0, -200.0, 0.0], [1050.0, -200.0, 0.0], [950.0, 0.0, 0.0], [1050.0, 0.0, 0.0]])
    };
    let mut ops = vec![sprite(0x8010_0000), sprite(0x8020_0000)];
    // Turned right, towards it: seen from the side the sprite would be an
    // edge; it faces the view instead, as wide as the game showed it.
    let right = Look { axes: [[0.0, 0.0, -1.0], [0.0, 1.0, 0.0], [1.0, 0.0, 0.0]], hide: 0x8020_0000, ..AHEAD };
    first_person(&mut ops, &right);
    assert_eq!(ops.len(), 1, "the viewer's own sprite is left out");
    let TraceOp::Draw(p) = &ops[0] else { unreachable!() };
    for (v, (x, y)) in p.vertices.iter().zip([(147.2, 68.8), (172.8, 68.8), (147.2, 120.0), (172.8, 120.0)]) {
        assert!((v.fx - x).abs() < 1e-3 && (v.fy - y).abs() < 1e-3 && v.z == 1000.0, "{v:?}");
    }
}

/// From within, the game's order among faces in one plane is kept, which
/// depth cannot tell apart: a sprite's part drawn over another stands a
/// little nearer (where it shows as before), a part beside it does not; and
/// a sprite the game draws reaching below its floor point (over the floor,
/// whatever their depths) stands on the floor, its lowest part there.
#[test]
fn from_within_a_part_drawn_over_another_stands_nearer_and_a_sprite_on_its_floor() {
    // Sprite `owner`'s part at view-space `x0..x1` across and `y0..y1` down,
    // 1000 ahead, its floor point (0, 200, 1000) as the game's projection
    // shows it (centre 160, 120; H 256).
    let part = |owner: u32, x0: f32, x1: f32, y0: f32, y1: f32, ground: [f32; 2]| {
        let TraceOp::Draw(mut p) = primitive(0, 0, 0) else { unreachable!() };
        p.shape = Shape::Quad;
        p.class = Class::Sprite;
        p.owner = owner;
        p.ground = Some([160.0 + 256.0 * ground[0] / 1000.0, 120.0 + 256.0 * ground[1] / 1000.0, 1000.0]);
        with_view(TraceOp::Draw(p), &[[x0, y0, 1000.0], [x1, y0, 1000.0], [x0, y1, 1000.0], [x1, y1, 1000.0]])
    };
    // The first reaches 50 below its floor point; the second, drawn over it,
    // lies within it; another sprite's beside them stands on its floor point.
    let mut ops = vec![
        part(1, -50.0, 50.0, 50.0, 250.0, [0.0, 200.0]),
        part(1, -30.0, 30.0, 80.0, 180.0, [0.0, 200.0]),
        part(2, 300.0, 400.0, 50.0, 200.0, [350.0, 200.0]),
    ];
    first_person(&mut ops, &AHEAD);
    let points: Vec<Vec<[f32; 3]>> = ops.iter().map(|op| match op {
        TraceOp::Draw(p) => p.vertices.iter().map(|v| v.view.expect("in the view's camera")).collect(),
        _ => unreachable!(),
    }).collect();
    // Standing on its floor point (y 200, as near as the projection takes it
    // back): its lowest part there, its top 200 above, as tall as the game
    // drew it.
    let heights: Vec<f32> = points[0].iter().map(|m| m[1]).collect();
    assert!(heights.iter().zip([0.0, 0.0, 200.0, 200.0]).all(|(y, expected)| (y - expected).abs() < 1e-3), "the first part stands on its floor point: {heights:?}");
    assert!(points[0].iter().all(|m| m[2] == 1000.0), "the first part in its plane: {:?}", points[0]);
    // The second, raised as much, a hundred-thousandth of its depth nearer
    // and where it was in the picture.
    for (m, (x, y)) in points[1].iter().zip([(-30.0, 30.0), (30.0, 30.0), (-30.0, 130.0), (30.0, 130.0)]) {
        let nearer = 1000.0 - m[2];
        assert!(nearer > 0.005 && nearer < 0.02, "the part drawn over it stands nearer ({nearer})");
        assert!((m[0] / m[2] - x / 1000.0).abs() < 1e-6 && (m[1] / m[2] - y / 1000.0).abs() < 1e-6, "where it was: {m:?}");
    }
    assert!(points[2].iter().all(|m| m[2] == 1000.0), "a part beside them stays in its plane: {:?}", points[2]);
}

#[test]
fn first_person_replaces_a_directional_sprite_once_and_restores_its_texture_cells() {
    use xem_gpu::{Projection, SpriteFacing, SpriteFacingPart, VramRect};
    let rect = VramRect { x:512,y:256,width:1,height:1 };
    let points = [[-50,-100,1000],[50,-100,1000],[-50,0,1000],[50,0,1000]].map(|view| Projection {
        view, sz:1000, class:2, offset:(160,120), h:256, owner:1,
        x:((160.0 + view[0] as f32 * 256.0 / 1000.0)*65536.0) as i32,
        y:((120.0 + view[1] as f32 * 256.0 / 1000.0)*65536.0) as i32,
        ..Default::default()
    });
    let facing = std::sync::Arc::new(SpriteFacing {
        parts:vec![SpriteFacingPart { words:[0x2C808080,0,24,0,32,0,24|16<<8,0,32|16<<8],points }],
        textures:vec![(rect,vec![0x1234].into())], restore:vec![(rect,vec![0x5678].into())],
    });
    let TraceOp::Draw(mut sprite) = primitive(150,95,0) else { unreachable!() };
    sprite.class = Class::Sprite;
    sprite.owner = 1;
    sprite.ground = Some([160.0,120.0,1000.0]);
    sprite.facing = Some(facing);
    // An alternative frame can have a different number of parts. Two
    // original parts are replaced by its one assembled part, exactly once.
    let original = vec![TraceOp::Draw(sprite.clone()),TraceOp::Draw(sprite)];
    let mut ops = original.clone();
    first_person(&mut ops,&AHEAD);
    assert_eq!(ops.len(),3);
    assert!(matches!(&ops[0], TraceOp::Write { pixels, .. } if pixels.as_ref()==[0x1234]));
    let TraceOp::Draw(p) = &ops[1] else { unreachable!() };
    assert_eq!(p.vertices.map(|v| (v.u,v.v)),[(24,0),(32,0),(24,16),(32,16)]);
    assert!(p.facing.is_none());
    assert!(matches!(&ops[2], TraceOp::Write { pixels, .. } if pixels.as_ref()==[0x5678]));
    assert_eq!(original.len(),2,"the normal-camera trace remains available unchanged");
}

/// A red floor through the GTE (offset 160, 120; H 256): 200 units below
/// the camera, 300 wide, from 100 to 3000 ahead, over a black screen.
fn floor_scene(gpu: &mut xem_gpu::Gpu) {
    gpu.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000, 0xE100_0200]);
    gpu.write_gp0_words(&[0x0200_0000, 0, 240 << 16 | 320]);
    let points = [[-150.0, 200.0, 100.0], [150.0, 200.0, 100.0], [-150.0, 200.0, 3000.0], [150.0, 200.0, 3000.0]];
    let projections: Vec<Projection> = points
        .iter()
        .map(|&[x, y, z]: &[f32; 3]| {
            let (sx, sy) = (160.0 + 256.0 * x / z, 120.0 + 256.0 * y / z);
            Projection {
                sxy: xy(sx.floor() as i32, sy.floor() as i32),
                sz: z as u16,
                class: 0,
                x: (sx * 65536.0) as i32,
                y: (sy * 65536.0) as i32,
                offset: (160, 120),
                h: 256,
                owner: 0,
                view: [x as i32, y as i32, z as i32],
            }
        })
        .collect();
    let words: Vec<u32> = std::iter::once(0x2800_00FF).chain(projections.iter().map(|p| p.sxy)).collect();
    submit(gpu, &projections, &[(words, vec![(1, 0), (2, 1), (3, 2), (4, 3)])]);
}

#[test]
fn a_view_from_within_the_scene_shows_its_picture_and_clips_what_reaches_behind_it() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let frame = |trace: &(xem_gpu::Trace, xem_gpu::Trace), framing: Framing| -> Vec<u8> {
        let (mut renderer, _) = draw(&gpu, trace, Fidelity::MODERN, Culling::default());
        let mut frame = FrameRenderer::new(&gpu.device);
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, DisplayArea { x: 0, y: 0, width: 320, height: 240 }, framing);
        gpu.queue.submit([encoder.finish()]);
        read_rgba8(&gpu.device, &gpu.queue, frame.color())
    };
    // Its points before the division are exact: looking along the game's
    // camera shows the game's picture.
    let floor = traced(floor_scene);
    let plain = frame(&floor, Framing { replay: true, ..Default::default() });
    let ahead = frame(&floor, Framing { look: Some(AHEAD), ..Default::default() });
    let differ = (0..640 * 480).filter(|i| plain[i * 4..i * 4 + 3] != ahead[i * 4..i * 4 + 3]).count();
    assert!(differ * 1000 < 640 * 480, "looking along the game's camera shows its picture ({differ} pixels differ)");
    // Stepped 400 units in, the floor's near end lies behind the view:
    // clipped at its near plane, the rest still reaches the screen's bottom.
    let inside = frame(&floor, Framing { look: Some(Look { eye: [0.0, 0.0, 400.0], near: 10.0, ..AHEAD }), ..Default::default() });
    let at = |pixels: &[u8], x: usize, y: usize| pixels[(y * 640 + x) * 4..(y * 640 + x) * 4 + 3].to_vec();
    assert_eq!(at(&inside, 320, 470), [255, 0, 0], "the clipped floor reaches the screen's bottom");
    assert_eq!(at(&inside, 320, 200), [0, 0, 0], "above its far end, nothing");
}

/// From within, a face the game draws over another in its plane shows at
/// every pixel they share however far away they lie: here on a wall 5 200
/// to 6 800 units ahead, slanting away to the right. Its lift (a
/// hundred-thousandth of its depth nearer) is under a float's step near the
/// far plane of GL's clip space, through which wgpu's GL backend (WebGL2,
/// GLES) takes a vertex's depth (2z - 1), so the fragment writes its depth
/// itself; scripts/suite.sh runs this on that backend too.
#[test]
fn from_within_a_face_drawn_over_another_in_its_plane_shows_wherever_they_overlap_far_away() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    // The wall z = 6000 + x / 2 in view space, red, and a green face the
    // game draws over it there, every corner exactly in its plane.
    let wall = |gpu: &mut xem_gpu::Gpu| {
        gpu.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000, 0xE100_0000]);
        gpu.write_gp0_words(&[0x0200_0000, 0, 240 << 16 | 320]);
        let corners = |x: i32, y: i32| {
            [[-x, -y], [x, -y], [-x, y], [x, y]].map(|[x, y]| {
                let z = 6000 + x / 2;
                let (sx, sy) = (160.0 + 256.0 * x as f32 / z as f32, 120.0 + 256.0 * y as f32 / z as f32);
                Projection { sxy: xy(sx.floor() as i32, sy.floor() as i32), sz: z as u16, class: 0, x: (sx * 65536.0) as i32, y: (sy * 65536.0) as i32, offset: (160, 120), h: 256, owner: 0, view: [x, y, z] }
            })
        };
        let projections: Vec<Projection> = corners(1600, 2000).into_iter().chain(corners(800, 1000)).collect();
        let packet = |colour: u32, first: usize| -> (Vec<u32>, Vec<(usize, usize)>) {
            let words = std::iter::once(0x2800_0000 | colour).chain(projections[first..first + 4].iter().map(|p| p.sxy)).collect();
            (words, (0..4).map(|i| (1 + i, first + i)).collect())
        };
        submit(gpu, &projections, &[packet(0x0000FF, 0), packet(0x00FF00, 4)]);
    };
    let trace = traced(wall);
    let (mut renderer, _) = draw(&gpu, &trace, Fidelity::MODERN, Culling::default());
    let mut frame = FrameRenderer::new(&gpu.device);
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, DisplayArea { x: 0, y: 0, width: 320, height: 240 }, Framing { look: Some(AHEAD), ..Default::default() });
    gpu.queue.submit([encoder.finish()]);
    let pixels = read_rgba8(&gpu.device, &gpu.queue, frame.color());
    // Well inside the green face (x 123.4 to 192, y 74.3 to 165.7 at its
    // near side and 80 to 160 at its far side), at twice the original.
    let (mut shared, mut red) = (0, 0);
    for y in 2 * 82..2 * 158 {
        for x in 2 * 126..2 * 189 {
            let i = (y * 640 + x) * 4;
            shared += 1;
            red += usize::from(pixels[i] > pixels[i + 1]);
        }
    }
    let info = gpu.adapter.get_info();
    assert_eq!(red, 0, "{:?} ({}): the face drawn over the wall shows at all {shared} pixels they share", info.backend, info.name);
}

/// [`FIELD`] with its floor and wall as the field's own scenery (the world,
/// as a map is drawn), the level camera's record, the depth it looks at
/// (1200) and the sprite as the controlled character (its eye record at
/// its feet), two frames of it presented flat on a desktop at twice the
/// original under `graphics` (a scene's first frame forgets the depth
/// recorded before it began), first-person inspection on with
/// `first_person`: the viewpoint the picture shows the scene from, the
/// picture and its scale.
fn shown_from(gpu: &Gpu, graphics: xem_settings::Graphics, first_person: bool) -> (GameView, Option<xem_render::ps1::Viewpoint>, Vec<u8>, f32) {
    let frame = |gpu: &mut xem_gpu::Gpu| {
        let mut faces = FIELD;
        faces[0].class = 0;
        faces[1].class = 0;
        gte_frame(gpu, LEVEL, &faces, true);
        let focus = Projection { sz: 1200, ..projection(FEET, 4, 0) };
        gpu.trace_projections(0, &[projection(FEET, 7, SPRITE), focus], &[], &[]);
    };
    let traces = traced(frame);
    let mut view = GameView::new(&gpu.device, &gpu.queue, None);
    view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, (640, 480), view.max_scale()));
    let _ = view.take_restart();
    view.set_first_person(first_person);
    let present = |view: &mut GameView, now: f64| {
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let size = view.present(&mut encoder, &DISPLAY, "field:1", now).expect("a picture").size;
        gpu.queue.submit([encoder.finish()]);
        size
    };
    for trace in [&traces.0, &traces.1] {
        view.push(trace.clone());
    }
    present(&mut view, 0.0);
    view.push(DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        frame(&mut device);
        device.take_trace()
    }));
    let size = present(&mut view, 0.1);
    let pixels = read_rgba8(&gpu.device, &gpu.queue, view.presented_texture().expect("presented"));
    let viewpoint = view.viewpoint();
    (view, viewpoint, pixels, size.0 as f32 / 320.0)
}

/// Where `viewpoint` shows the view-space point `v` (picture pixels), and
/// the ray through a picture pixel (origin, direction in view space).
fn shows(viewpoint: &xem_render::ps1::Viewpoint, v: [f32; 3]) -> (f32, f32) {
    let q: [f32; 3] = std::array::from_fn(|r| (0..3).map(|k| viewpoint.rows[r][k] * v[k]).sum::<f32>() + viewpoint.offset[r]);
    (viewpoint.centre.0 + viewpoint.h * q[0] / q[2], viewpoint.centre.1 + viewpoint.h * q[1] / q[2])
}

fn ray(viewpoint: &xem_render::ps1::Viewpoint, (x, y): (f32, f32)) -> ([f32; 3], [f32; 3]) {
    let local = [(x - viewpoint.centre.0) / viewpoint.h, (y - viewpoint.centre.1) / viewpoint.h, 1.0];
    let back = |v: [f32; 3]| -> [f32; 3] { std::array::from_fn(|i| (0..3).map(|r| viewpoint.rows[r][i] * v[r]).sum()) };
    let offset = viewpoint.offset;
    (back([-offset[0], -offset[1], -offset[2]]), back(local))
}

/// The colour of the picture at picture point `at` (`scale` pixels each).
fn colour_at(pixels: &[u8], scale: f32, (x, y): (f32, f32)) -> [u8; 3] {
    let (px, py) = ((x * scale) as usize, (y * scale) as usize);
    let i = (py * (320.0 * scale) as usize + px) * 4;
    [pixels[i], pixels[i + 1], pixels[i + 2]]
}

#[test]
fn a_pulled_back_or_inspecting_picture_has_the_viewpoint_it_shows_the_scene_from() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let green = [0, 255, 0];
    // The game camera's own picture: no other viewpoint.
    assert_eq!(shown_from(&gpu, xem_settings::Graphics::default(), false).1, None, "the game camera's own");
    // Pulled back by wider views' zoom out at 250%: from 1.5 times the
    // depth the camera looks at (1200, its record's) further back.
    let (mut view, viewpoint, pixels, scale) = shown_from(&gpu, xem_settings::Graphics { zoom_out: 250, ..Default::default() }, false);
    let viewpoint = viewpoint.expect("pulled back");
    // The host shows the device's own picture (the Original renderer): the
    // game camera's, until the view presents again.
    view.away();
    assert_eq!(view.viewpoint(), None, "the device's own picture is the game camera's");
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    view.present(&mut encoder, &DISPLAY, "field:1", 0.2).expect("a picture");
    gpu.queue.submit([encoder.finish()]);
    assert_eq!(view.viewpoint(), Some(viewpoint), "presented again, pulled back");
    assert_eq!((viewpoint.offset, viewpoint.h, viewpoint.centre), ([0.0, 0.0, 1800.0], 256.0, (160.0, 120.0)));
    // Floor points (the level camera's view space is the world's), where
    // the picture shows the floor and its ray meets it, where the game's own
    // projection would put them below the floor the picture shows (its
    // floor 10 lines tall, from 134 to 143).
    for floor in [[200.0, 200.0, 1000.0], [-250.0, 200.0, 600.0], [120.0, 200.0, 1100.0], [-150.0, 200.0, 1300.0]] {
        let at = shows(&viewpoint, floor);
        assert_eq!(colour_at(&pixels, scale, at), green, "the floor at {floor:?} shows at {at:?}");
        let (origin, direction) = ray(&viewpoint, at);
        let t = (floor[1] - origin[1]) / direction[1];
        let met: [f32; 3] = std::array::from_fn(|i| origin[i] + direction[i] * t);
        assert!((0..3).all(|i| (met[i] - floor[i]).abs() < 0.5), "the ray through {at:?} meets the floor at {met:?}");
        let game = (160.0 + 256.0 * floor[0] / floor[2], 120.0 + 256.0 * floor[1] / floor[2]);
        assert_ne!(colour_at(&pixels, scale, game), green, "the game camera's {game:?} for {floor:?} is below the floor shown");
    }
    // Looking from within the scene: from the character's eye (0.9 of its
    // sprite's 100 above its feet), along the camera's heading.
    let (mut view, viewpoint, pixels, scale) = shown_from(&gpu, xem_settings::Graphics::default(), true);
    let viewpoint = viewpoint.expect("looking from within");
    assert_eq!(viewpoint.offset, [0.0, -110.0, -1000.0], "{viewpoint:?}");
    for (point, colour) in [([60.0, 200.0, 1300.0], green), ([-150.0, 200.0, 1700.0], green), ([50.0, 100.0, 1500.0], [255, 0, 0])] {
        let at = shows(&viewpoint, point);
        assert_eq!(colour_at(&pixels, scale, at), colour, "{point:?} shows at {at:?}");
    }
    // The Original renderer ends inspection and the host shows the device's
    // own picture: no look's viewpoint stays behind.
    view.set_first_person(false);
    view.away();
    assert_eq!(view.viewpoint(), None, "the device's own picture after inspection is the game camera's");
}

/// First-person inspection begins only on a frame that draws the field's
/// scene: over a field's frames of its interface alone (the title screen's
/// field draws its logo, the game culling its map whole) it ends at the
/// second with the field camera's record, the first proving nothing (a
/// trace started again mid-frame logs the rest of a frame whose scene came
/// before it): over such a frame and then [`FIELD`]'s it begins; over
/// [`FIELD`] it stays. A headset's eyes then never hold an earlier
/// inspection's picture: turned on again over another frame, they show
/// that frame, not the one they drew before.
#[test]
fn inspection_begins_only_over_the_fields_scene_and_its_eyes_begin_anew() {
    use xem_render::ps1::HeadsetEye;
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let view_of = |traces: &(xem_gpu::Trace, xem_gpu::Trace), host: HostClass| {
        let mut view = GameView::new(&gpu.device, &gpu.queue, None);
        view.configure(ViewConfig::resolve(&xem_settings::Graphics::default(), host, (640, 480), view.max_scale()));
        let _ = view.take_restart();
        for trace in [&traces.0, &traces.1] {
            view.push(trace.clone());
        }
        view
    };
    let present = |view: &mut GameView| {
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        view.present(&mut encoder, &DISPLAY, "field:490", 0.0).expect("a picture");
        gpu.queue.submit([encoder.finish()]);
    };
    // A field's frame of its camera's record and its interface alone, and
    // FIELD's of the controlled character's record with its floor green;
    // the camera's record goes with a frame's first primitive, so a field's
    // first frame has none.
    fn interface(gpu: &mut xem_gpu::Gpu) {
        gpu.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000, 0xE100_0200]);
        gpu.write_gp0_words(&[0x0200_0000, 0, 240 << 16 | 320]);
        gpu.trace_projections(0, &camera_entries(LEVEL, [0; 3], [0, 200, 1200], 1200, Reveals::LOOK), &[], &[]);
        gpu.write_gp0_words(&[0x6000_FFFF, xy(100, 80), 60 << 16 | 120]);
    }
    fn green_floor(gpu: &mut xem_gpu::Gpu) {
        let mut faces = FIELD;
        faces[0].command = 0x2800_0000 | 0x00FF00;
        gte_frame(gpu, LEVEL, &faces, true);
        gpu.trace_projections(0, &[projection(FEET, 7, SPRITE)], &[], &[]);
    }
    let shown = |takes: &[fn(&mut xem_gpu::Gpu)]| {
        let (sync, frames) = traced_takes(takes);
        let mut view = GameView::new(&gpu.device, &gpu.queue, None);
        view.configure(ViewConfig::resolve(&xem_settings::Graphics::default(), HostClass::Desktop, (640, 480), view.max_scale()));
        let _ = view.take_restart();
        view.push(sync);
        (view, frames)
    };
    // The interface's frames, presented one at a time: inspection waits
    // through the trace's start, then a frame with the camera's record and
    // none of the scene, and ends at the second.
    let (mut view, frames) = shown(&[interface, interface]);
    view.set_first_person(true);
    present(&mut view);
    assert!(view.first_person(), "no frame with the field camera's record yet (the trace's start): inspection waits");
    view.push(frames[0].clone());
    present(&mut view);
    assert!(view.first_person(), "one frame of the record and none of the scene proves nothing: inspection waits");
    present(&mut view);
    assert!(view.first_person(), "that frame shown again is not a second one");
    view.push(frames[1].clone());
    present(&mut view);
    assert!(!view.first_person(), "nothing of the field's scene to look around in: inspection ends");
    // A frame of the record alone, then the field's scene: it begins there.
    let (mut view, frames) = shown(&[interface, green_floor, green_floor]);
    view.set_first_person(true);
    view.push(frames[0].clone());
    present(&mut view);
    for frame in &frames[1..] {
        view.push(frame.clone());
        present(&mut view);
        assert!(view.first_person(), "the frame after the record alone draws the field's scene: inspection begins and stays");
    }
    // FIELD, the sprite as the controlled character (its eye record at its
    // feet, as the port records it before the next frame's drawing), its
    // floor in `floor`'s colour, two frames of it.
    let field = |floor: u32| traced(move |gpu| {
        let mut faces = FIELD;
        faces[0].command = 0x2800_0000 | floor;
        for _ in 0..2 {
            gte_frame(gpu, LEVEL, &faces, true);
            gpu.trace_projections(0, &[projection(FEET, 7, SPRITE)], &[], &[]);
        }
    });
    let mut view = view_of(&field(0x00FF00), HostClass::Desktop);
    view.set_first_person(true);
    present(&mut view);
    assert!(view.first_person(), "over the field's scene it stays");
    // A headset's eye looking ahead and down at the floor (green), then the
    // view off, another frame (the floor blue) and the view on again.
    let eye = HeadsetEye { orientation: [-0.3826834, 0.0, 0.0, 0.9238795], position: [0.0; 3], fov: [-1.0, 1.0, 1.0, -1.0] };
    let floor_seen = |view: &mut GameView| -> [u8; 3] {
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let picture = view.present_headset(&mut encoder, &DISPLAY, "field:490", 0.0, &[eye]).expect("presented").eyes.len();
        gpu.queue.submit([encoder.finish()]);
        assert_eq!(picture, 1, "the eye looks from within the scene");
        let pixels = read_rgba8(&gpu.device, &gpu.queue, view.eye_texture(0).expect("the eye's picture"));
        let (w, h) = (view.eye_texture(0).unwrap().width() as usize, view.eye_texture(0).unwrap().height() as usize);
        let i = (h * 3 / 4 * w + w / 2) * 4;
        [pixels[i], pixels[i + 1], pixels[i + 2]]
    };
    let mut view = view_of(&field(0x00FF00), HostClass::Headset);
    view.set_first_person(true);
    assert_eq!(floor_seen(&mut view), [0, 255, 0], "the green floor below the eye");
    view.set_first_person(false);
    let blue = field(0xFF0000);
    for trace in [&blue.0, &blue.1] {
        view.push(trace.clone());
    }
    present(&mut view);
    view.set_first_person(true);
    assert_eq!(floor_seen(&mut view), [0, 0, 255], "the frame shown, not the earlier inspection's picture");
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
        placement: 0,
        source: 0,
        revealed: false,
        ground: None,
        facing: None,
        backdrop: false,
    })
}

#[test]
fn scene_points_weld_to_their_own_game_frames_only() {
    // A face of the scene a quarter of a pixel to the right of `primitive`'s,
    // its points before the division where it lies.
    let face = |dx: f32| {
        let mut op = primitive(100, 100, 0);
        if let TraceOp::Draw(p) = &mut op {
            for v in &mut p.vertices[..3] {
                v.fx += dx;
                v.view = Some([v.fx, v.fy, 1000.0]);
            }
        }
        op
    };
    let first = |op: &TraceOp| match op {
        TraceOp::Draw(p) => (p.vertices[0].fx, p.vertices[0].view),
        _ => unreachable!("a face"),
    };
    let mut ops = vec![face(0.0), face(0.25)];
    weld(&mut ops);
    assert_eq!(first(&ops[1]), (100.0, Some([100.0, 100.0, 1000.0])), "within a frame, the later face takes the first one's point");
    // The same two faces in two game frames of one trace (the host ran both
    // before presenting): each keeps its own.
    let mut ops = vec![face(0.0), TraceOp::VBlank, face(0.25)];
    weld(&mut ops);
    assert_eq!(first(&ops[2]), (100.25, Some([100.25, 100.0, 1000.0])), "a frame's face is not welded to one an earlier frame drew there");
}

/// A field camera turned `yaw` degrees about the world's up from looking
/// along +z, 1000 units from the world's origin, which it looks at.
fn orbiting(yaw: f32) -> Camera {
    let (s, c) = yaw.to_radians().sin_cos();
    Camera {
        axes: [[c * 4096.0, 0.0, s * 4096.0], [0.0, 4096.0, 0.0], [-s * 4096.0, 0.0, c * 4096.0]],
        translation: [0.0, 0.0, 1000.0],
        target: [0.0; 3],
        focus: 1000.0,
        mode: CameraMode::Field,
        stage: None,
        ground: None,
        origin: [0.0; 3],
        around: 0,
    }
}

/// Where `camera` sees the world point `w` (offset 160, 120; H 256).
fn seen(camera: &Camera, w: [f32; 3]) -> ([f32; 3], (f32, f32)) {
    let v: [f32; 3] = std::array::from_fn(|i| (0..3).map(|k| camera.axes[k][i] * w[k]).sum::<f32>() / 4096.0 + camera.translation[i]);
    (v, (160.0 + 256.0 * v[0] / v[2], 120.0 + 256.0 * v[1] / v[2]))
}

/// A triangle of the scene at view-space `points`, of `class`, owned by
/// `owner` with its `placement`.
fn placed_face(points: [[f32; 3]; 3], class: Class, owner: u32, placement: u32) -> TraceOp {
    let TraceOp::Draw(mut p) = primitive(0, 0, 0) else { unreachable!() };
    (p.class, p.owner, p.placement) = (class, owner, placement);
    for (v, view) in p.vertices.iter_mut().zip(points) {
        let (fx, fy) = (160.0 + 256.0 * view[0] / view[2], 120.0 + 256.0 * view[1] / view[2]);
        *v = TraceVertex { x: fx.floor() as i32, y: fy.floor() as i32, fx, fy, z: view[2], view: Some(view), ..*v };
    }
    TraceOp::Draw(p)
}

/// A model's (with `packets`, a buffer 0x100 bytes) or a sprite's placement:
/// unturned, at `at`.
fn placement(seq: u32, owner: u32, packets: u32, at: [f32; 3]) -> Placement {
    let size = if packets == 0 { 0 } else { 0x100 };
    Placement { seq, owner, packets, size, axes: [[4096.0, 0.0, 0.0], [0.0, 4096.0, 0.0], [0.0, 0.0, 4096.0]], translation: at }
}

fn first_point(op: &TraceOp) -> (f32, f32) {
    match op {
        TraceOp::Draw(p) => (p.vertices[0].fx, p.vertices[0].fy),
        _ => unreachable!("a primitive"),
    }
}

#[test]
fn interpolation_turns_the_camera_between_frames_with_still_scenery_in_place() {
    // The camera turned 6 degrees about what it looks at between two frames
    // a thirtieth of a second apart.
    let (before, after) = (orbiting(0.0), orbiting(6.0));
    let corners = [[-200.0, 0.0, 100.0], [200.0, 0.0, 100.0], [0.0, -150.0, -50.0]];
    let newest = vec![placed_face(corners.map(|w| seen(&after, w).0), Class::World, 0, 0)];
    let frames = [(0.0, &before, &[][..]), (1.0 / 30.0, &after, &[][..])];
    let close = |a: (f32, f32), b: (f32, f32)| (a.0 - b.0).abs() < 1e-3 && (a.1 - b.1).abs() < 1e-3;
    for (alpha, yaw) in [(0.0, 0.0), (0.25, 1.5), (0.5, 3.0)] {
        let mut ops = newest.clone();
        assert!(interpolate(&mut ops, &frames, (1.0 + alpha) / 30.0));
        let expected = seen(&orbiting(yaw), corners[0]).1;
        assert!(close(first_point(&ops[0]), expected), "alpha {alpha}: {:?} where a camera turned {yaw} degrees sees it, {expected:?}", first_point(&ops[0]));
    }
    let mut ops = newest.clone();
    assert!(!interpolate(&mut ops, &frames, 2.0 / 30.0), "the newest frame as drawn");
    assert_eq!(ops, newest);
    // A camera turned further than a frame's turn cut to another view.
    assert!(!interpolate(&mut ops, &[frames[0], (1.0 / 30.0, &orbiting(90.0), &[])], 1.5 / 30.0), "a cut is not motion");
    assert_eq!(ops, newest);
}

#[test]
fn interpolation_moves_models_and_sprites_from_their_own_placements() {
    let camera = orbiting(0.0);
    let face = |at: [f32; 3], class, owner, placement| {
        placed_face([[at[0], at[1], at[2]], [at[0] + 10.0, at[1], at[2]], [at[0], at[1] + 10.0, at[2]]], class, owner, placement)
    };
    // Two instances of one model in adjacent blocks (buffers 0x1000 and
    // 0x1100, 0x1200 and 0x1300), both stepping to their second buffer; a
    // sprite; a model drawn somewhere else entirely (another thing in that
    // memory); one not drawn before; and the scenery.
    let previous = [
        placement(1, 0x8010_0000, 0x1000, [0.0, 0.0, 1000.0]),
        placement(2, 0x8010_0000, 0x1200, [-300.0, 0.0, 1000.0]),
        placement(3, 0x8020_0000, 0, [100.0, 50.0, 1000.0]),
        placement(4, 0x8030_0000, 0x4000, [0.0, 0.0, 1000.0]),
    ];
    let newest = [
        placement(11, 0x8010_0000, 0x1100, [40.0, 0.0, 1000.0]),
        placement(12, 0x8010_0000, 0x1300, [-300.0, 20.0, 1000.0]),
        placement(13, 0x8020_0000, 0, [80.0, 50.0, 1000.0]),
        placement(14, 0x8030_0000, 0x4100, [0.0, 0.0, 11_000.0]),
        placement(15, 0x8040_0000, 0x5000, [0.0, 0.0, 1000.0]),
    ];
    let mut ops = vec![
        face([40.0, 0.0, 1000.0], Class::Model, 0x8010_0000, 11),
        face([-300.0, 20.0, 1000.0], Class::Model, 0x8010_0000, 12),
        face([80.0, 50.0, 1000.0], Class::Sprite, 0x8020_0000, 13),
        face([0.0, 0.0, 11_000.0], Class::Model, 0x8030_0000, 14),
        face([0.0, 0.0, 1000.0], Class::Model, 0x8040_0000, 15),
        face([0.0, 100.0, 1000.0], Class::World, 0, 0),
    ];
    let drawn = ops.clone();
    assert!(interpolate(&mut ops, &[(0.0, &camera, &previous[..]), (1.0 / 30.0, &camera, &newest[..])], 1.5 / 30.0));
    let screen = |v: [f32; 3]| (160.0 + 256.0 * v[0] / v[2], 120.0 + 256.0 * v[1] / v[2]);
    let close = |a: (f32, f32), b: (f32, f32)| (a.0 - b.0).abs() < 1e-3 && (a.1 - b.1).abs() < 1e-3;
    assert!(close(first_point(&ops[0]), screen([20.0, 0.0, 1000.0])), "halfway from its own instance's place, not the other's");
    assert!(close(first_point(&ops[1]), screen([-300.0, 10.0, 1000.0])), "the other instance from its own");
    assert!(close(first_point(&ops[2]), screen([90.0, 50.0, 1000.0])), "a sprite from its own place");
    for (k, why) in [(3, "a model further than the camera's distance is another"), (4, "one not drawn before holds still"), (5, "the scenery holds still")] {
        assert_eq!(first_point(&ops[k]), first_point(&drawn[k]), "{why}");
    }
}

#[test]
fn each_camera_and_sprite_moves_over_the_time_between_its_own_moves() {
    // A battle draws every display frame, a sixtieth of a second apart,
    // while its camera turns every other one (4 degrees) and a sprite moves
    // every one (10 units across the world).
    let cameras: Vec<Camera> = (0..4).map(|frame| orbiting(if frame < 2 { 0.0 } else { 4.0 })).collect();
    let sprite = |frame: usize| [100.0 + 10.0 * frame as f32, 0.0, 0.0];
    let placements: Vec<[Placement; 1]> = (0..4)
        .map(|frame| [Placement { seq: 1 + frame as u32, owner: 0x8020_0000, axes: cameras[frame].axes, translation: seen(&cameras[frame], sprite(frame)).0, ..Placement::default() }])
        .collect();
    let still = [-200.0, 0.0, 100.0];
    let close = |a: (f32, f32), b: (f32, f32)| (a.0 - b.0).abs() < 1e-3 && (a.1 - b.1).abs() < 1e-3;
    // A quarter of a sixtieth after frames 2 and 3 arrive: the camera a
    // quarter and three quarters of its turn on, the sprite halfway on.
    for (newest, yaw, at) in [(2, 1.0, 115.0), (3, 3.0, 125.0)] {
        let frames: Vec<(f64, &Camera, &[Placement])> = (0..=newest).map(|frame| (frame as f64 / 60.0, &cameras[frame], &placements[frame][..])).collect();
        let camera = &cameras[newest];
        let corners = |w: [f32; 3]| [w, [w[0] + 10.0, w[1], w[2]], [w[0], w[1] + 10.0, w[2]]].map(|w| seen(camera, w).0);
        let mut ops = vec![placed_face(corners(still), Class::World, 0, 0), placed_face(corners(sprite(newest)), Class::Sprite, 0x8020_0000, 1 + newest as u32)];
        assert!(interpolate(&mut ops, &frames, (2.0 * newest as f64 + 1.0) / 120.0));
        let turned = orbiting(yaw);
        assert!(close(first_point(&ops[0]), seen(&turned, still).1), "frame {newest}: the scenery through the camera turned {yaw} degrees");
        assert!(close(first_point(&ops[1]), seen(&turned, [at, 0.0, 0.0]).1), "frame {newest}: the sprite at {at}");
    }
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

#[test]
fn framebuffer_distortion_keeps_the_reconstructed_scene_and_its_depth() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let area = DisplayArea { x: 0, y: 0, width: 320, height: 216 };
    let framing = Framing { side: 40, pull_back: 1000.0, ..Default::default() };
    let render = |effect: u8, fidelity: Fidelity| {
        let traces = traced(|device| {
            projected_scene(device);
            if effect != 0 {
                // The field saves strips, draws its grid 32 rows down, then
                // shifts the screen and restores its bottom saved strips.
                for row in 0..3 {
                    for column in 0..5 {
                        device.write_gp0_words(&[0x8000_0000, xy(column * 64, 192 + row * 16), xy(960, (row * 5 + column) * 16), 16 << 16 | 64]);
                    }
                }
                for x in (0..320).step_by(64) {
                    device.write_gp0_words(&[0x2D80_8080, xy(x, 32), 0, xy(x + 64, 32), ((0x100 + x as u32 / 64) << 16) | 64,
                        xy(x, 224), 192 << 8, xy(x + 64, 224), 192 << 8 | 64]);
                }
                device.write_gp0_words(&[0x8000_0000, xy(0, 32), 0, 192 << 16 | 320]);
                for row in 0..3 {
                    for column in 0..5 {
                        let (x, y, v) = (column * 64, 192 + row * 16 - if effect == 2 && row == 2 { 32 } else { 0 }, ((row * 5 + column) * 16) as u32);
                        device.write_gp0_words(&[0x2D80_8080, xy(x, y), v << 8, xy(x + 64, y), (0x10F << 16) | v << 8 | 64,
                            xy(x, y + 16), (v + 16) << 8, xy(x + 64, y + 16), (v + 16) << 8 | 64]);
                    }
                }
            }
        });
        let (mut renderer, _) = draw(&gpu, &traces, fidelity, Culling::default());
        let mut frame = FrameRenderer::new(&gpu.device);
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, area,
            if fidelity == Fidelity::ORIGINAL { Framing::default() } else { framing });
        frame.draw_overlay(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, None);
        gpu.queue.submit([encoder.finish()]);
        let color = read_rgba8(&gpu.device, &gpu.queue, frame.color());
        if fidelity == Fidelity::ORIGINAL {
            let native = read_rgba8(&gpu.device, &gpu.queue, renderer.picture().0);
            let expected: Vec<_> = native.chunks_exact(1024 * 4).take(216).flat_map(|row| row[..320 * 4].iter().copied()).collect();
            assert_eq!(color, expected, "exact PS1 fidelity keeps the original framebuffer path");
        }
        (color, read_rgba8(&gpu.device, &gpu.queue, frame.aux()))
    };
    render(1, Fidelity::ORIGINAL);
    let (before, depth_before) = render(0, Fidelity::MODERN);
    let (after, depth_after) = render(1, Fidelity::MODERN);
    let different = before.chunks_exact(4).zip(after.chunks_exact(4))
        .filter(|(a, b)| (0..3).any(|c| (i16::from(a[c]) - i16::from(b[c])).abs() > 12)).count();
    assert!(different < before.len() / 4 / 100, "the neutral grid keeps the pulled-back scene and sides: {different} pixels differ");
    let world_before = depth_before.chunks_exact(4).filter(|p| p[2] > 1).count();
    let world_after = depth_after.chunks_exact(4).filter(|p| p[2] > 1).count();
    assert!(world_after.abs_diff(world_before) < world_before / 20, "the distortion preserves depth/classes for HD-2D: {world_before} -> {world_after}");
    let (warped, _) = render(2, Fidelity::MODERN);
    let at = ((194 * 2 * 800) + (270 + 40) * 2) * 4;
    assert_eq!(&before[at..at + 3], [0, 0, 0]);
    assert!(warped[at] > 240 && warped[at + 1] > 240 && warped[at + 2] == 0,
        "the wave brings the yellow pixel from hidden row226 into visible row194");
}

#[test]
fn moving_the_back_buffer_does_not_retire_the_still_displayed_frame() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let traces = traced(projected_scene);
    let (mut renderer, _) = draw(&gpu, &traces, Fidelity::MODERN, Culling::default());
    let area = DisplayArea { x: 0, y: 0, width: 320, height: 216 };
    renderer.set_display(xem_gpu::VramRect { x: 0, y: 0, width: 320, height: 216 });
    let mut frame = FrameRenderer::new(&gpu.device);
    let framing = Framing { side: 40, pull_back: 1000.0, ..Default::default() };
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, area, framing);
    frame.draw_overlay(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, None);
    gpu.queue.submit([encoder.finish()]);
    let expected = read_rgba8(&gpu.device, &gpu.queue, frame.color());
    let clear = DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        device.write_gp0_words(&[0x0200_0000, xy(0, 216), 216 << 16 | 320]);
        device.take_trace()
    });
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    renderer.draw(&mut encoder, &clear);
    frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, area, framing);
    frame.draw_overlay(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, None);
    gpu.queue.submit([encoder.finish()]);
    assert_eq!(read_rgba8(&gpu.device, &gpu.queue, frame.color()), expected, "the old first buffer remains displayed during the mode change");
    renderer.set_presented(Some(xem_render::ps1::PresentedPicture { texture: frame.color().clone(), side: 40, scale: 2 }));
    let copy = DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        device.write_gp0_words(&[0x8000_0000, 0, xy(0, 216), 216 << 16 | 320]);
        device.take_trace()
    });
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    renderer.set_display(xem_gpu::VramRect { x: 0, y: 216, width: 320, height: 216 });
    renderer.draw(&mut encoder, &copy);
    frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, DisplayArea { y: 216, ..area }, framing);
    frame.draw_overlay(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, None);
    gpu.queue.submit([encoder.finish()]);
    assert_eq!(frame.identity().map(|(origin, _)| origin), Some((0, 216)), "a buffer-to-buffer copy starts the new back buffer, not an internal shift of the front");
    assert!(read_rgba8(&gpu.device, &gpu.queue, frame.color()) == expected, "the copied display keeps the presented frame");
}

#[test]
fn a_saved_screen_survives_a_second_copy_and_clearing_that_display_alias() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let traces = traced(projected_scene);
    let (mut renderer, _) = draw(&gpu, &traces, Fidelity::MODERN, Culling::default());
    let area = DisplayArea { x: 0, y: 0, width: 320, height: 240 };
    for y in [0, 256] { renderer.set_display(xem_gpu::VramRect { x: 0, y, width: 320, height: 240 }); }
    let mut frame = FrameRenderer::new(&gpu.device);
    let framing = Framing { side: 40, pull_back: 1000.0, ..Default::default() };
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, area, framing);
    frame.draw_overlay(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, None);
    gpu.queue.submit([encoder.finish()]);
    let expected = read_rgba8(&gpu.device, &gpu.queue, frame.color());
    renderer.set_presented(Some(xem_render::ps1::PresentedPicture { texture: frame.color().clone(), side: 40, scale: 2 }));
    // Field entry takes the picture, then copies the same source into its
    // other display buffer. That buffer is cleared before the saved pieces
    // are drawn; the offscreen picture must retain its independent lifetime.
    for destination in [xy(704, 256), xy(0, 256)] {
        let trace = DEVICE.with(|device| {
            let mut device = device.borrow_mut();
            device.write_gp0_words(&[0x8000_0000, 0, destination, 240 << 16 | 320]);
            device.take_trace()
        });
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        renderer.draw(&mut encoder, &trace);
        gpu.queue.submit([encoder.finish()]);
    }
    let trace = DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        device.write_gp0_words(&[0x0200_0000, xy(0, 256), 240 << 16 | 320]);
        // Copy the saved image back, as a neutral screen-piece transition.
        device.write_gp0_words(&[0x8000_0000, xy(704, 256), xy(0, 256), 240 << 16 | 320]);
        device.take_trace()
    });
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    renderer.draw(&mut encoder, &trace);
    frame.prepare(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, DisplayArea { y: 256, ..area }, framing);
    frame.draw_overlay(&gpu.device, &gpu.queue, &mut encoder, &mut renderer, None);
    gpu.queue.submit([encoder.finish()]);
    assert_eq!(read_rgba8(&gpu.device, &gpu.queue, frame.color()), expected, "both the pulled-back picture and its sides survive the cleared alias");
}

/// The port's record of a field camera (xem/gte.h): the world's axes
/// (axes 0-2), the translation (axis 3, its depth `depth` telling records
/// apart), then the point it looks at, owned by the mode (1: a field).
fn camera_records(depth: i32) -> Vec<Projection> {
    let entry = |class: u8, owner: u32, view: [i32; 3]| Projection { sxy: 0, sz: 0, class, x: 0, y: 0, offset: (160, 120), h: 256, owner, view };
    vec![entry(6, 0, [4096, 0, 0]), entry(6, 1, [0, 4096, 0]), entry(6, 2, [0, 0, 4096]), entry(6, 3, [0, 0, depth]), entry(8, 1, [0, 0, 0])]
}

#[test]
fn a_buffer_shown_lower_than_a_log_begun_before_starts_it_with_each_frame_drawn_all_over_it() {
    // The kernel menu clears its buffers at y 0 and 240 (320x224); the arena
    // then shows its own at y 0 and 256 (320x218), drawing all over each
    // frame without clearing it, and never the rows between.
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut renderer = Ps1Renderer::new(&gpu.device, &gpu.queue, 1);
    let draw = |renderer: &mut Ps1Renderer, trace: &xem_gpu::Trace| {
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        renderer.draw(&mut encoder, trace);
        gpu.queue.submit([encoder.finish()]);
    };
    let small = |k: i32| [0x2000_FFFF, xy(k, 10), xy(k + 4, 10), xy(k, 14)];
    renderer.set_display(xem_gpu::VramRect { x: 0, y: 240, width: 320, height: 224 });
    let (sync, menu) = traced(|device| {
        device.write_gp0_words(&[0xE300_0000 | 240 << 10, 0xE400_0000 | 463 << 10 | 319, 0xE500_0000 | 240 << 11]);
        device.write_gp0_words(&[0x0200_0000, 240 << 16, 224 << 16 | 320]);
        for k in 0..70 {
            device.write_gp0_words(&small(k * 4));
        }
    });
    draw(&mut renderer, &sync);
    draw(&mut renderer, &menu);
    renderer.set_display(xem_gpu::VramRect { x: 0, y: 256, width: 320, height: 218 });
    let area = DisplayArea { x: 0, y: 256, width: 320, height: 218 };
    let mut frame = FrameRenderer::new(&gpu.device);
    let mut identities = Vec::new();
    for n in 0..3 {
        let trace = DEVICE.with(|device| {
            let mut device = device.borrow_mut();
            device.vblank();
            device.write_gp0_words(&[0xE300_0000 | 256 << 10, 0xE400_0000 | 473 << 10 | 319, 0xE500_0000 | 256 << 11]);
            device.write_gp0_words(&[0x2800_0040 + n, xy(0, 0), xy(320, 0), xy(0, 218), xy(320, 218)]);
            for k in 0..70 {
                device.write_gp0_words(&small(k * 4));
            }
            device.take_trace()
        });
        draw(&mut renderer, &trace);
        frame.track(&renderer, area, None);
        let draws = renderer.frame_log(area).map_or(0, |log| log.ops.iter().filter(|op| matches!(op, TraceOp::Draw(_))).count());
        assert_eq!(draws, 71, "frame {n}: the buffer's log holds the frame drawn all over it alone");
        identities.push(frame.identity());
    }
    assert!(identities.windows(2).all(|pair| pair[0] != pair[1]), "each frame shown anew: {identities:?}");
}

#[test]
fn each_frame_keeps_the_camera_recorded_before_its_first_primitive() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut renderer = Ps1Renderer::new(&gpu.device, &gpu.queue, 1);
    renderer.set_display(xem_gpu::VramRect { x: 0, y: 0, width: 320, height: 240 });
    let area = DisplayArea { x: 0, y: 0, width: 320, height: 240 };
    let clear = [0x0200_0000, 0, 240 << 16 | 320];
    let small = |k: i32| [0x2000_FFFF, xy(k, 10), xy(k + 4, 10), xy(k, 14)];
    let draw = |renderer: &mut Ps1Renderer, trace: &xem_gpu::Trace| {
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        renderer.draw(&mut encoder, trace);
        gpu.queue.submit([encoder.finish()]);
    };
    let kept = |renderer: &Ps1Renderer| renderer.frame_log(area).and_then(|log| log.camera).map(|camera| camera.translation[2]);
    let draws = |renderer: &Ps1Renderer| renderer.frame_log(area).map_or(0, |log| log.ops.iter().filter(|op| matches!(op, TraceOp::Draw(_))).count());
    // A frame: its buffer cleared, then the camera the port records as the
    // game builds it, then its primitives.
    let (sync, first) = traced(|device| {
        device.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000]);
        device.write_gp0_words(&clear);
        device.trace_projections(0, &camera_records(1000), &[], &[]);
        for k in 0..70 {
            device.write_gp0_words(&small(k * 4));
        }
    });
    draw(&mut renderer, &sync);
    draw(&mut renderer, &first);
    assert_eq!(kept(&renderer), Some(1000.0), "the frame keeps the camera recorded before its first primitive");
    let mut frame = FrameRenderer::new(&gpu.device);
    assert!(frame.track(&renderer, area, None));
    assert_eq!(frame.camera().map(|camera| camera.translation[2]), Some(1000.0), "the frame shown keeps its log's");
    let state = DEVICE.with(|device| device.borrow().save());
    // A game frame (after its vertical blank) recorded as camera `depth` at
    // ring entry `at` and drawn over the last instead of a clear (a
    // battle's stage), or into its buffer cleared.
    let frame_drawn = |device: &mut xem_gpu::Gpu, at: u32, depth: i32, cleared: bool| {
        device.vblank();
        device.trace_projections(at, &camera_records(depth), &[], &[]);
        if cleared {
            device.write_gp0_words(&clear);
        } else {
            device.write_gp0_words(&[0x2800_0000 | depth as u32, xy(0, 0), xy(320, 0), xy(0, 240), xy(320, 240)]);
        }
        for k in 0..70 {
            device.write_gp0_words(&small(k * 4));
        }
    };
    let take = |frames: &[(u32, i32, bool)]| DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        for (k, &(at, depth, cleared)) in frames.iter().enumerate() {
            if k > 0 {
                // Two blanks a frame, as a field draws.
                device.vblank();
            }
            frame_drawn(&mut device, at, depth, cleared);
        }
        device.take_trace()
    });
    // The next frame drawn over the last: its log begins with the frame,
    // and keeps the camera recorded before its first primitive.
    draw(&mut renderer, &take(&[(5, 2000, false)]));
    assert_eq!(kept(&renderer), Some(2000.0), "a frame drawn over the last keeps its own camera, not the one before");
    assert!(frame.track(&renderer, area, None), "a new frame");
    assert_eq!(frame.camera().map(|camera| camera.translation[2]), Some(2000.0));
    // One presentation of several frames (the host ran them before it
    // presented: fast-forward, catching up): the buffer's log begins with
    // the last that draws all over it, cleared or drawn over.
    draw(&mut renderer, &take(&[(10, 3000, false), (15, 4000, false)]));
    assert_eq!((kept(&renderer), draws(&renderer)), (Some(4000.0), 71), "the last of two frames drawn over in one trace keeps its own camera and only its primitives");
    assert!(frame.track(&renderer, area, None), "a new frame");
    assert_eq!(frame.camera().map(|camera| camera.translation[2]), Some(4000.0));
    draw(&mut renderer, &take(&[(20, 5000, true), (25, 6000, true)]));
    assert_eq!((kept(&renderer), draws(&renderer)), (Some(6000.0), 70), "the last of two cleared frames in one trace");
    draw(&mut renderer, &take(&[(30, 6500, true), (35, 7000, false)]));
    assert_eq!((kept(&renderer), draws(&renderer)), (Some(7000.0), 71), "a frame drawn over one cleared in the same trace");
    // A state loaded (the trace starts again on its VRAM): the shown buffer
    // the game draws over starts a log with the last frame that draws all
    // over it.
    DEVICE.with(|device| device.borrow_mut().load(&state).expect("a state"));
    draw(&mut renderer, &take(&[(40, 8000, false), (45, 9000, false)]));
    assert_eq!((kept(&renderer), draws(&renderer)), (Some(9000.0), 71), "a state's first trace keeps the camera of its last frame drawn over");
    assert!(frame.track(&renderer, area, None), "a new frame");
    assert_eq!(frame.camera().map(|camera| camera.translation[2]), Some(9000.0));
    // Recorded more than four vertical blanks before a frame, it is not
    // that frame's.
    let stale = DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        for _ in 0..5 {
            device.vblank();
        }
        device.write_gp0_words(&clear);
        device.write_gp0_words(&small(0));
        device.take_trace()
    });
    draw(&mut renderer, &stale);
    assert_eq!(kept(&renderer), None, "no camera of its own");
}

// The diorama (docs/runtime.md, Diorama).

use xem_render::glam::{Mat4, Vec3, Vec4};
use xem_render::ps1::{DioramaEye, DioramaSpace, GameView, HostClass, Table, ViewConfig, XrView};

/// The projection (offset 160, 120; H 256) of view-space point `point`, made
/// for `class` (0 world, 1 model, 2 sprite, 3 a sprite's floor) by `owner`.
fn projection(point: [f32; 3], class: u8, owner: u32) -> Projection {
    let [x, y, z] = point;
    let (sx, sy) = (160.0 + 256.0 * x / z, 120.0 + 256.0 * y / z);
    Projection {
        sxy: xy(sx.floor() as i32, sy.floor() as i32),
        sz: z as u16,
        class,
        x: (sx * 65536.0) as i32,
        y: (sy * 65536.0) as i32,
        offset: (160, 120),
        h: 256,
        owner,
        view: point.map(|c| c as i32),
    }
}

/// The port's record of a field camera (xem/gte.h): its matrix's columns in
/// 4.12 (axes 0-2), its translation (axis 3), then the point it looks at and
/// that point's depth through it, with the host's `around` bits
/// ([`Reveals::around`]) as it was recorded.
fn camera_entries(columns: [[i32; 3]; 3], translation: [i32; 3], target: [i32; 3], focus: u16, around: u8) -> Vec<Projection> {
    let entry = |class: u8, owner: u32, view: [i32; 3], sz: u16| Projection { sxy: 0, sz, class, x: 0, y: 0, offset: (160, 120), h: 256, owner, view };
    vec![
        entry(6, 0, columns[0], 0),
        entry(6, 1, columns[1], 0),
        entry(6, 2, columns[2], 0),
        entry(6, 3, translation, 0),
        entry(8, 1 | u32::from(around) << 8, target, focus),
    ]
}

/// The level camera of [`diorama_scene`]: at the world's origin looking
/// along +z (view = world), at a point on the floor 1200 ahead.
const LEVEL: [[i32; 3]; 3] = [[4096, 0, 0], [0, 4096, 0], [0, 0, 4096]];

/// A face of a test field: its corners in the world (a quad's in Z order),
/// projection class (1 model, 2 sprite), owner and command word (its colour
/// 0xBBGGRR, untextured), and for a sprite part the floor point recorded
/// before it.
#[derive(Clone, Copy)]
struct Face {
    points: [[f32; 3]; 4],
    class: u8,
    owner: u32,
    command: u32,
    ground: Option<[f32; 3]>,
}

const MODEL: u32 = 0x8020_0000;
const SPRITE: u32 = 0x8010_0000;
const FEET: [f32; 3] = [0.0, 200.0, 1000.0];

/// A green floor 200 below the camera from 400 to 2000 ahead, a red wall
/// 1500 ahead, and a sprite standing on the floor 1000 ahead (a blue part,
/// then a yellow one over its right side), far to near as an ordering
/// table draws them.
const FIELD: [Face; 4] = [
    Face { points: [[-300.0, 200.0, 2000.0], [300.0, 200.0, 2000.0], [-300.0, 200.0, 400.0], [300.0, 200.0, 400.0]], class: 1, owner: MODEL, command: 0x2800_FF00, ground: None },
    Face { points: [[-100.0, 0.0, 1500.0], [100.0, 0.0, 1500.0], [-100.0, 200.0, 1500.0], [100.0, 200.0, 1500.0]], class: 1, owner: MODEL, command: 0x2800_00FF, ground: None },
    Face { points: [[-30.0, 100.0, 1000.0], [30.0, 100.0, 1000.0], [-30.0, 200.0, 1000.0], [30.0, 200.0, 1000.0]], class: 2, owner: SPRITE, command: 0x28FF_0000, ground: Some(FEET) },
    Face { points: [[0.0, 120.0, 1000.0], [40.0, 120.0, 1000.0], [0.0, 180.0, 1000.0], [40.0, 180.0, 1000.0]], class: 2, owner: SPRITE, command: 0x2800_FFFF, ground: None },
];

/// A field frame drawn through the GTE (offset 160, 120; H 256): the
/// clear, then `faces`, with a camera record (`columns` its matrix; view =
/// matrix · world, looking at a point on the floor 1200 ahead) when
/// `camera`.
fn gte_frame(gpu: &mut xem_gpu::Gpu, columns: [[i32; 3]; 3], faces: &[Face], camera: bool) {
    gpu.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000, 0xE100_0200]);
    gpu.write_gp0_words(&[0x0200_0000, 0, 240 << 16 | 320]);
    gte_faces(gpu, columns, faces, camera);
}

/// [`gte_frame`]'s camera record and `faces`, after whatever the frame drew
/// before them.
fn gte_faces(gpu: &mut xem_gpu::Gpu, columns: [[i32; 3]; 3], faces: &[Face], camera: bool) {
    let matrix = Mat3x3(columns);
    // The target in the world, and its depth through the camera.
    let target = [0.0, 200.0, 1200.0];
    let mut projections = if camera { camera_entries(columns, [0, 0, 0], target.map(|c| c as i32), matrix.view(target)[2] as u16, Reveals::ANYWHERE) } else { Vec::new() };
    let mut packets = Vec::new();
    for face in faces {
        if let Some(ground) = face.ground {
            projections.push(projection(matrix.view(ground), 3, face.owner));
        }
        let first = projections.len();
        let corners: Vec<Projection> = face.points.iter().map(|&p| projection(matrix.view(p), face.class, face.owner)).collect();
        let words: Vec<u32> = std::iter::once(face.command).chain(corners.iter().map(|p| p.sxy)).collect();
        projections.extend(corners);
        packets.push((words, (0..4).map(|i| (i + 1, first + i)).collect()));
    }
    submit(gpu, &projections, &packets);
}

/// [`FIELD`] seen by a camera with the matrix `columns`, then a yellow
/// interface rectangle (250..310, 200..230).
fn diorama_scene(gpu: &mut xem_gpu::Gpu, columns: [[i32; 3]; 3]) {
    gte_frame(gpu, columns, &FIELD, true);
    gpu.write_gp0_words(&[0x6000_FFFF, xy(250, 200), 30 << 16 | 60]);
}

/// A camera matrix in 4.12 by its columns: view = matrix · world.
struct Mat3x3([[i32; 3]; 3]);

impl Mat3x3 {
    fn view(&self, world: [f32; 3]) -> [f32; 3] {
        std::array::from_fn(|i| (0..3).map(|k| self.0[k][i] as f32 / 4096.0 * world[k]).sum())
    }
}

const DISPLAY: xem_gpu::DisplayInfo = xem_gpu::DisplayInfo { enabled: true, x: 0, y: 0, width: 320, height: 240, mode_width: 320, color24: false, interlaced: false, pal: false };

/// A view configured as a headset's (the picture at twice the original),
/// the diorama on `table`, given `traces`.
fn diorama_view(gpu: &Gpu, traces: &(xem_gpu::Trace, xem_gpu::Trace), table: Table) -> GameView {
    let mut view = GameView::new(&gpu.device, &gpu.queue, None);
    view.configure(ViewConfig::resolve(&xem_settings::Graphics::default(), HostClass::Headset, (640, 480), view.max_scale()));
    let _ = view.take_restart();
    view.set_xr_view(XrView::Diorama(table));
    for trace in [&traces.0, &traces.1] {
        view.push(trace.clone());
    }
    view
}

/// Render targets for eyes of `size` pixels.
fn eye_targets(gpu: &Gpu, count: usize, size: (u32, u32)) -> Vec<xem_render::wgpu::Texture> {
    (0..count).map(|_| gpu.device.create_texture(&xem_render::wgpu::TextureDescriptor {
        label: None,
        size: xem_render::wgpu::Extent3d { width: size.0, height: size.1, depth_or_array_layers: 1 },
        mip_level_count: 1,
        sample_count: 1,
        dimension: xem_render::wgpu::TextureDimension::D2,
        format: xem_render::wgpu::TextureFormat::Rgba8Unorm,
        usage: xem_render::wgpu::TextureUsages::RENDER_ATTACHMENT | xem_render::wgpu::TextureUsages::COPY_SRC | xem_render::wgpu::TextureUsages::TEXTURE_BINDING,
        view_formats: &[],
    })).collect()
}

/// One diorama presentation at `now` into eyes `(view, proj)` over black
/// targets of `size`: each eye's picture, the screen's share and the pane.
fn present_diorama(gpu: &Gpu, view: &mut GameView, eyes: &[(Mat4, Mat4)], size: (u32, u32), now: f64) -> (Vec<Vec<u8>>, f32, Option<Vec<u8>>) {
    present_over(gpu, view, eyes, size, now, "field:1", xem_render::wgpu::Color::BLACK)
}

/// [`present_diorama`] of `scene` over targets cleared to `clear`.
fn present_over(gpu: &Gpu, view: &mut GameView, eyes: &[(Mat4, Mat4)], size: (u32, u32), now: f64, scene: &str, clear: xem_render::wgpu::Color) -> (Vec<Vec<u8>>, f32, Option<Vec<u8>>) {
    present_shown(gpu, view, eyes, size, now, scene, clear, &DISPLAY).expect("a presentation")
}

/// [`present_over`] of `display`: `None` when there is nothing to show.
#[allow(clippy::too_many_arguments)]
fn present_shown(
    gpu: &Gpu,
    view: &mut GameView,
    eyes: &[(Mat4, Mat4)],
    size: (u32, u32),
    now: f64,
    scene: &str,
    clear: xem_render::wgpu::Color,
    display: &xem_gpu::DisplayInfo,
) -> Option<(Vec<Vec<u8>>, f32, Option<Vec<u8>>)> {
    let targets = eye_targets(gpu, eyes.len(), size);
    let views: Vec<_> = targets.iter().map(|t| t.create_view(&Default::default())).collect();
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    for target in &views {
        encoder.begin_render_pass(&xem_render::wgpu::RenderPassDescriptor {
            label: None,
            color_attachments: &[Some(xem_render::wgpu::RenderPassColorAttachment {
                view: target,
                depth_slice: None,
                resolve_target: None,
                ops: xem_render::wgpu::Operations { load: xem_render::wgpu::LoadOp::Clear(clear), store: xem_render::wgpu::StoreOp::Store },
            })],
            ..Default::default()
        });
    }
    let diorama_eyes: Vec<DioramaEye> = eyes.iter().map(|&(eye, proj)| DioramaEye { view: eye, proj, size }).collect();
    let presented = view.present_diorama(&mut encoder, display, scene, now, &diorama_eyes, Mat4::IDENTITY);
    // Each eye's picture over its target, as a host draws it.
    if let Some(presented) = &presented {
        let whole = xem_render::Rect { x: 0.0, y: 0.0, width: size.0 as f32, height: size.1 as f32 };
        let quads: Vec<[xem_render::Quad; 1]> = presented.eyes.iter().map(|picture| {
            let mut quad = xem_render::Quad::screen(picture, whole, size);
            quad.filter = xem_render::wgpu::FilterMode::Nearest;
            quad.opacity = presented.opacity;
            quad.srgb_encoded = true;
            [quad]
        }).collect();
        assert!(quads.is_empty() || quads.len() == views.len(), "a picture for each eye");
        let passes: Vec<_> = views.iter().zip(&quads).map(|(target, quad)| (target, None, &quad[..])).collect();
        xem_render::Compositor::new(&gpu.device, &gpu.queue, xem_render::wgpu::TextureFormat::Rgba8Unorm).draw_targets(&mut encoder, &passes);
    }
    let shown = presented.map(|presented| (presented.screen, presented.pane.map(|pane| pane.texture().clone())));
    gpu.queue.submit([encoder.finish()]);
    let (screen, pane) = shown?;
    let pictures = targets.iter().map(|t| read_rgba8(&gpu.device, &gpu.queue, t)).collect();
    Some((pictures, screen, pane.map(|pane| read_rgba8(&gpu.device, &gpu.queue, &pane))))
}

/// The eye at the game camera's pose (its view matrix, in the room of
/// `space`) and the game's projection (offset 160, 120; H 256) onto a
/// 320x240 screen, a native pixel's centre where the flat frame puts it.
fn game_eye(space: &DioramaSpace, columns: [[i32; 3]; 3]) -> (Mat4, Mat4) {
    let m = xem_render::glam::Mat3::from_cols_array_2d(&columns.map(|c| c.map(|v| v as f32 / 4096.0)));
    let units = m.x_axis.length();
    let view_from_world = Mat4::from_mat3(m);
    // Eye space: x right, y up, z backwards, metres.
    let k = space.scale / units;
    let eye = Mat4::from_scale(Vec3::new(k, -k, -k)) * view_from_world * space.room_from_world.inverse();
    let (near, far) = (0.005, 20.0);
    let proj = Mat4::from_cols(
        Vec4::new(256.0 / 160.0, 0.0, 0.0, 0.0),
        Vec4::new(0.0, 256.0 / 120.0, 0.0, 0.0),
        Vec4::new(-0.5 / 160.0, 0.5 / 120.0, far / (near - far), -1.0),
        Vec4::new(0.0, 0.0, near * far / (near - far), 0.0),
    );
    (eye, proj)
}

#[test]
fn a_diorama_eye_at_the_game_cameras_pose_shows_the_flat_frames_scene() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let traces = traced(|device| diorama_scene(device, LEVEL));
    // Small enough on the table that the whole scene lies inside its volume.
    let mut view = diorama_view(&gpu, &traces, Table { scale: 0.2, ..Table::default() });
    let size = (640, 480);
    let any = (Mat4::look_at_rh(Vec3::new(0.0, 0.2, 0.4), Vec3::new(0.0, -0.45, -0.6), Vec3::Y), Mat4::perspective_rh(1.2, 4.0 / 3.0, 0.01, 10.0));
    // The screen first, easing into the diorama.
    let (_, screen, pane) = present_diorama(&gpu, &mut view, &[any], size, 0.0);
    assert_eq!((screen, pane.is_some()), (1.0, false), "the diorama view begins on the screen");
    let flat = read_rgba8(&gpu.device, &gpu.queue, view.presented_texture().expect("the flat frame"));
    let (_, screen, _) = present_diorama(&gpu, &mut view, &[any], size, 1.0);
    assert_eq!(screen, 0.0, "a frame with a camera record shows as a diorama");
    let space = view.diorama_space().expect("a diorama shown");
    let (pictures, _, pane) = present_diorama(&gpu, &mut view, &[game_eye(&space, LEVEL)], size, 1.1);
    let eye = &pictures[0];
    if let Some(directory) = std::env::var_os("XEM_DIORAMA_DUMP") {
        for (name, pixels) in [("flat", &flat), ("eye", eye)] {
            let file = std::fs::File::create(std::path::Path::new(&directory).join(format!("identity-{name}.png"))).unwrap();
            let mut encoder = png::Encoder::new(file, size.0, size.1);
            encoder.set_color(png::ColorType::Rgba);
            let opaque: Vec<u8> = pixels.chunks(4).flat_map(|p| [p[0], p[1], p[2], 255]).collect();
            encoder.write_header().unwrap().write_image_data(&opaque).unwrap();
        }
    }
    // The interface rectangle (250..310, 200..230) is on the pane, not in the eye.
    let interface = |x: usize, y: usize| (500..620).contains(&x) && (400..460).contains(&y);
    let (mut differ, mut total) = (0, 0);
    for y in 0..480 {
        for x in 0..640 {
            if interface(x, y) {
                continue;
            }
            let at = (y * 640 + x) * 4;
            total += 1;
            differ += usize::from((0..3).any(|c| (i32::from(flat[at + c]) - i32::from(eye[at + c])).abs() > 24));
        }
    }
    eprintln!("{differ} of {total} pixels differ between the flat frame and the eye at the game camera");
    assert!(differ * 200 < total, "the eye at the game camera shows the flat frame's scene ({differ} of {total} pixels differ)");
    let pane = pane.expect("the pane");
    let at = |x: usize, y: usize| pane[(y * 640 + x) * 4..(y * 640 + x) * 4 + 4].to_vec();
    assert_eq!(at(560, 430), [255, 255, 0, 255], "the interface on the pane");
    assert_eq!(at(320, 300), [0, 0, 0, 0], "the pane is transparent elsewhere");
    let blue = (0..640 * 480).filter(|i| eye[i * 4..i * 4 + 3] == [0, 0, 255]).count();
    assert!(blue > 500, "the sprite stands in the eye ({blue} pixels)");
}

/// An eye looking at the default table's centre from `yaw` degrees round it
/// (0 the seat's side, positive to the right), `pitch` up and `distance`
/// metres away, `shift` metres to its right (a stereo pair's half).
fn orbit(yaw: f32, pitch: f32, distance: f32, shift: f32) -> (Mat4, Mat4) {
    let centre = Table::default().room_from_table(Mat4::IDENTITY).transform_point3(Vec3::ZERO);
    let (yaw, pitch) = (yaw.to_radians(), pitch.to_radians());
    let at = centre + Vec3::new(yaw.sin() * pitch.cos(), pitch.sin(), yaw.cos() * pitch.cos()) * distance;
    let view = Mat4::look_at_rh(at, centre, Vec3::Y);
    let right = view.inverse().transform_vector3(Vec3::X);
    (Mat4::look_at_rh(at + right * shift, centre + right * shift, Vec3::Y), Mat4::perspective_rh(50f32.to_radians(), 4.0 / 3.0, 0.01, 20.0))
}

/// The pixels of `picture` (640 wide) whose colour is `rgb`.
fn pixels_of(picture: &[u8], rgb: [u8; 3]) -> Vec<(usize, usize)> {
    picture.chunks(4).enumerate().filter(|(_, p)| p[..3] == rgb).map(|(i, _)| (i % 640, i / 640)).collect()
}

/// A trace of [`gte_frame`] shown by a diorama view from `now` 0 (the
/// screen) to 1 (eased into the diorama), and the view.
fn shown_diorama(gpu: &Gpu, faces: &[Face], table: Table) -> GameView {
    let traces = traced(|device| gte_frame(device, LEVEL, faces, true));
    let mut view = diorama_view(gpu, &traces, table);
    for now in [0.0, 1.0] {
        present_diorama(gpu, &mut view, &[orbit(0.0, 40.0, 1.0, 0.0)], (640, 480), now);
    }
    view
}

#[test]
fn split_sprites_at_one_floor_point_keep_the_games_order_in_the_diorama() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    // The yellow part drawn by another sprite standing at the same floor
    // point, as the field splits a character into several.
    let mut faces = FIELD;
    faces[3].owner = SPRITE + 0x100;
    faces[3].ground = Some(FEET);
    let mut view = shown_diorama(&gpu, &faces, Table { scale: 2.0, ..Table::default() });
    // From the seat's side, and from beyond the billboards' plane (their
    // backs, round the table).
    for (yaw, pitch) in [(0.0, 40.0), (35.0, 30.0), (-50.0, 25.0), (180.0, 30.0), (150.0, 25.0)] {
        let (pictures, _, _) = present_diorama(&gpu, &mut view, &[orbit(yaw, pitch, 0.8, 0.0)], (640, 480), 2.0);
        let picture = &pictures[0];
        dump(&format!("split-{yaw}-{pitch}"), picture);
        let yellow = pixels_of(picture, [255, 255, 0]);
        assert!(yellow.len() > 300, "the later sprite shows ({} pixels from {yaw}, {pitch})", yellow.len());
        // Along each row, nothing of the earlier sprite between the later
        // one's leftmost and rightmost pixels: it lies over it whole.
        let mut through = 0;
        for y in 0..480 {
            let row: Vec<usize> = yellow.iter().filter(|p| p.1 == y).map(|p| p.0).collect();
            let (Some(&left), Some(&right)) = (row.iter().min(), row.iter().max()) else { continue };
            through += (left..=right).filter(|&x| picture[(y * 640 + x) * 4..(y * 640 + x) * 4 + 3] == [0, 0, 255]).count();
        }
        assert_eq!(through, 0, "the earlier sprite shows through the later one from {yaw}, {pitch}");
    }
}

#[test]
fn both_eyes_see_one_upright_sprite_apart_only_by_their_disparity() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut view = shown_diorama(&gpu, &FIELD, Table { scale: 2.0, ..Table::default() });
    let eyes = [orbit(0.0, 35.0, 0.7, -0.032), orbit(0.0, 35.0, 0.7, 0.032)];
    let (pictures, _, _) = present_diorama(&gpu, &mut view, &eyes, (640, 480), 2.0);
    let bounds = |picture: &[u8]| {
        let blue = pixels_of(picture, [0, 0, 255]);
        let xs = blue.iter().map(|p| p.0);
        let ys = blue.iter().map(|p| p.1);
        (xs.clone().min().unwrap(), xs.max().unwrap(), ys.clone().min().unwrap(), ys.max().unwrap())
    };
    let ((l0, r0, t0, b0), (l1, r1, t1, b1)) = (bounds(&pictures[0]), bounds(&pictures[1]));
    eprintln!("left eye x {l0}..{r0} y {t0}..{b0}, right eye x {l1}..{r1} y {t1}..{b1}");
    assert!(t0.abs_diff(t1) <= 1 && b0.abs_diff(b1) <= 1, "the same height in both eyes");
    assert!((r0 - l0).abs_diff(r1 - l1) <= 2, "the same width in both eyes");
    assert!(l0 > l1 + 2, "the right eye sees it further left: a disparity ({l0} against {l1})");
}

#[test]
fn the_diorama_drops_faces_an_eye_sees_from_behind_but_not_double_sided_ones() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    // A white face the game drew from behind (double-sided), behind the
    // red wall, both above the floor.
    let mut faces = FIELD[..2].to_vec();
    faces.push(Face { points: [[300.0, -100.0, 1800.0], [200.0, -100.0, 1800.0], [300.0, 100.0, 1800.0], [200.0, 100.0, 1800.0]], class: 1, owner: MODEL, command: 0x28FF_FFFF, ground: None });
    let mut view = shown_diorama(&gpu, &faces, Table { scale: 0.5, ..Table::default() });
    let count = |view: &mut GameView, yaw: f32, rgb: [u8; 3]| {
        let (pictures, _, _) = present_diorama(&gpu, view, &[orbit(yaw, 20.0, 1.0, 0.0)], (640, 480), 2.0);
        pixels_of(&pictures[0], rgb).len()
    };
    assert!(count(&mut view, 0.0, [255, 0, 0]) > 200, "the wall faces the seat's side");
    assert_eq!(count(&mut view, 180.0, [255, 0, 0]), 0, "seen from behind, it is cut away");
    assert!(count(&mut view, 0.0, [255, 255, 255]) > 50 && count(&mut view, 180.0, [255, 255, 255]) > 50, "the double-sided face shows from both sides");
}

#[test]
fn a_fade_tints_the_diorama_and_its_pane_but_not_the_room() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let grey = xem_render::wgpu::Color { r: 0.5, g: 0.5, b: 0.5, a: 1.0 };
    let picture = |fade: bool| {
        let traces = traced(|device| {
            diorama_scene(device, LEVEL);
            if fade {
                // A full-screen translucent quad subtracting 64 (mode 2).
                device.write_gp0(0xE100_0240);
                device.write_gp0_words(&[0x2A40_4040, xy(0, 0), xy(320, 0), xy(0, 240), xy(320, 240)]);
            }
        });
        let mut view = diorama_view(&gpu, &traces, Table::default());
        for now in [0.0, 1.0] {
            present_over(&gpu, &mut view, &[orbit(0.0, 40.0, 1.0, 0.0)], (640, 480), now, "field:1", grey);
        }
        let (pictures, screen, pane) = present_over(&gpu, &mut view, &[orbit(0.0, 40.0, 1.0, 0.0)], (640, 480), 2.0, "field:1", grey);
        assert_eq!(screen, 0.0);
        (pictures.into_iter().next().unwrap(), pane.expect("a pane"))
    };
    let ((plain, plain_pane), (faded, faded_pane)) = (picture(false), picture(true));
    let at = |p: &[u8], i: usize| [p[i * 4], p[i * 4 + 1], p[i * 4 + 2]];
    let room = (0..640 * 480).filter(|&i| at(&plain, i) == [128, 128, 128]).collect::<Vec<_>>();
    assert!(room.len() > 1000 && room.iter().all(|&i| at(&faded, i) == [128, 128, 128]), "the room around the table is left as it was");
    let green = (0..640 * 480).filter(|&i| at(&plain, i) == [0, 255, 0]).collect::<Vec<_>>();
    assert!(green.len() > 1000 && green.iter().all(|&i| at(&faded, i) == [0, 191, 0]), "the floor darkens by the fade's 64");
    let pane_at = |p: &[u8], x: usize, y: usize| p[(y * 640 + x) * 4..(y * 640 + x) * 4 + 4].to_vec();
    assert_eq!((pane_at(&plain_pane, 560, 430), pane_at(&faded_pane, 560, 430)), (vec![255, 255, 0, 255], vec![191, 191, 0, 255]), "the interface under the fade darkens on the pane");
    assert_eq!(pane_at(&faded_pane, 320, 300), [0, 0, 0, 0], "the rest of the pane stays transparent");
}

#[test]
fn a_subtracting_interface_quad_takes_its_colour_off_the_pane_as_off_the_screen() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    // A grey window (20..80, 20..50), then a quad subtracting 64 (mode 2)
    // from 10 to 60 across it: over the window and, left of it, over the
    // scene, where the pane holds nothing.
    let traces = traced(|device| {
        diorama_scene(device, LEVEL);
        device.write_gp0_words(&[0x6080_8080, xy(20, 20), 30 << 16 | 60]);
        device.write_gp0(0xE100_0240);
        device.write_gp0_words(&[0x2A40_4040, xy(10, 25), xy(60, 25), xy(10, 45), xy(60, 45)]);
    });
    let mut view = diorama_view(&gpu, &traces, Table::default());
    let eye = [orbit(0.0, 40.0, 1.0, 0.0)];
    let (_, screen, _) = present_diorama(&gpu, &mut view, &eye, (640, 480), 0.0);
    assert_eq!(screen, 1.0, "the screen first");
    let flat = read_rgba8(&gpu.device, &gpu.queue, view.presented_texture().expect("the flat frame"));
    present_diorama(&gpu, &mut view, &eye, (640, 480), 1.0);
    let (_, screen, pane) = present_diorama(&gpu, &mut view, &eye, (640, 480), 2.0);
    assert_eq!(screen, 0.0);
    let pane = pane.expect("a pane");
    // Pixels at twice the native ones; the flat frame's alpha is the mask bit.
    let at = |p: &[u8], (x, y): (usize, usize)| p[(y * 640 + x) * 4..(y * 640 + x) * 4 + 4].to_vec();
    let rgb = |p: &[u8], point| at(p, point)[..3].to_vec();
    // The window's grey alone (native 70, 30): 132, as the interface's 15
    // bits show 128.
    assert_eq!(rgb(&flat, (140, 60)), [132, 132, 132], "the window on the screen");
    assert_eq!(at(&pane, (140, 60)), [132, 132, 132, 255], "the window on the pane");
    // Under the quad (40, 35): its 64 is 66 in the interface's 15 bits.
    assert_eq!(rgb(&flat, (80, 70)), [66, 66, 66], "66 taken off the window on the screen");
    assert_eq!(at(&pane, (80, 70)), [66, 66, 66, 255], "and off the window on the pane (B-F where the pane covers)");
    // Over the scene (15, 35) the pane holds the darkening alone: nothing of
    // its own, covering by F's brightest channel.
    assert_eq!(at(&pane, (30, 70)), [0, 0, 0, 66], "over the scene, the pane darkens what shows through");
}

#[test]
fn a_picture_the_game_draws_over_its_clear_shows_whole_on_the_wall() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    // The game's clear, then a magenta panorama over the screen's top half
    // before the scene, in three panels from above the screen's top (as the
    // debug battle's forest), all on the wall in one plane; the camera
    // recorded first, as the port records it before its frame's draws.
    // Where the room shows, the wall leaves out the clear: the panels alone.
    let traces = traced(|device| {
        device.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000, 0xE100_0200]);
        device.trace_projections(0, &camera_entries(LEVEL, [0, 0, 0], [0, 200, 1200], 1200, Reveals::ANYWHERE), &[], &[]);
        device.write_gp0_words(&[0x0200_0000, 0, 240 << 16 | 320]);
        for x in [1, 106, 211] {
            device.write_gp0_words(&[0x28FF_00FF, xy(x, -18), xy(x + 105, -18), xy(x, 122), xy(x + 105, 122)]);
        }
        gte_faces(device, LEVEL, &FIELD, false);
    });
    let picture = |room: bool| {
        let mut view = diorama_view(&gpu, &traces, Table::default());
        view.set_room(room);
        let eye = [orbit(0.0, 10.0, 1.2, 0.0)];
        for now in [0.0, 1.0] {
            present_diorama(&gpu, &mut view, &eye, (640, 480), now);
        }
        let (pictures, screen, _) = present_diorama(&gpu, &mut view, &eye, (640, 480), 2.0);
        assert_eq!(screen, 0.0);
        pictures.into_iter().next().unwrap()
    };
    let magenta = |p: &[u8]| (0..640 * 480).filter(|&i| p[i * 4..i * 4 + 3] == [255, 0, 255]).collect::<Vec<_>>();
    let (over, alone) = (magenta(&picture(false)), magenta(&picture(true)));
    assert!(alone.len() > 5000, "the strip on the wall: {} pixels", alone.len());
    assert_eq!(over.len(), alone.len(), "the strip shows as whole over the clear as alone (with the wall's depth written, the clear's fought it)");
    assert_eq!(over, alone);
}

#[test]
fn frames_without_a_camera_record_or_in_other_modes_show_the_screen() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let eye = [orbit(0.0, 40.0, 1.0, 0.0)];
    // No camera record: the screen, and no diorama to hold.
    let traces = traced(|device| gte_frame(device, LEVEL, &FIELD, false));
    let mut view = diorama_view(&gpu, &traces, Table::default());
    for now in [0.0, 1.0, 2.0] {
        let (_, screen, pane) = present_diorama(&gpu, &mut view, &eye, (640, 480), now);
        assert_eq!((screen, pane.is_some()), (1.0, false), "a frame without a camera record shows the screen");
    }
    assert!(view.diorama_space().is_none());
    // A menu (or a movie, the arena, booting): the screen.
    let traces = traced(|device| diorama_scene(device, LEVEL));
    let mut view = diorama_view(&gpu, &traces, Table::default());
    for now in [0.0, 1.0] {
        let (_, screen, _) = present_over(&gpu, &mut view, &eye, (640, 480), now, "menu", xem_render::wgpu::Color::BLACK);
        assert_eq!(screen, 1.0, "a menu shows the screen");
    }
    // A frame the field draws next (the frame shown keeps the scene that
    // drew it): eased into the diorama over 0.3 s.
    let next = DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        device.vblank();
        diorama_scene(&mut device, LEVEL);
        device.take_trace()
    });
    view.push(next);
    let shares: Vec<f32> = [1.1, 1.2, 1.3, 1.5].iter().map(|&now| present_diorama(&gpu, &mut view, &eye, (640, 480), now).1).collect();
    assert!(shares.windows(2).all(|w| w[1] < w[0]) && shares[3] == 0.0, "the screen eases into the diorama: {shares:?}");
    // The screen view: the screen whatever the frame.
    view.set_xr_view(XrView::Screen);
    assert_eq!(present_diorama(&gpu, &mut view, &eye, (640, 480), 1.6).1, 1.0);
}

#[test]
fn the_listener_takes_the_tables_axes_only_while_the_diorama_is_presented() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let eye = [orbit(0.0, 40.0, 1.0, 0.0)];
    let traces = traced(|device| diorama_scene(device, LEVEL));
    let mut view = diorama_view(&gpu, &traces, Table::default());
    assert_eq!(view.listener_axes(), None, "nothing presented yet");
    present_diorama(&gpu, &mut view, &eye, (640, 480), 0.0);
    assert_eq!(present_diorama(&gpu, &mut view, &eye, (640, 480), 1.0).1, 0.0, "the diorama alone");
    let space = view.diorama_space().expect("a diorama shown");
    assert!(space.listener_axes().is_some());
    assert_eq!(view.listener_axes(), space.listener_axes(), "the seat's axes through the table");
    // The host shows the device's own picture (a 24-bit movie frame the view
    // does not draw): the diorama lies nowhere until presented again, and
    // then eases in from the screen.
    view.away();
    assert_eq!((view.listener_axes(), view.diorama_space()), (None, None), "no table's axes while it is not presented");
    let shares: Vec<f32> = [5.0, 5.1, 5.2, 5.4].iter().map(|&now| present_diorama(&gpu, &mut view, &eye, (640, 480), now).1).collect();
    assert!(shares[0] == 1.0 && shares.windows(2).all(|w| w[1] < w[0]) && shares[3] == 0.0, "eased in from the screen: {shares:?}");
    assert_eq!(view.listener_axes(), view.diorama_space().and_then(|space| space.listener_axes()));
    assert!(view.listener_axes().is_some());
    // The Original renderer draws no scene of its own: the settings give the
    // screen whatever the headset view, and the listener no table's axes.
    let original = xem_settings::Graphics {
        renderer: xem_settings::Renderer::Original,
        xr: xem_settings::XrScreen { view: xem_settings::HeadsetView::Diorama, ..Default::default() },
        ..Default::default()
    };
    view.set_xr_view(xem_render::ps1::xr_view(&original));
    assert_eq!((view.listener_axes(), view.diorama_space()), (None, None), "no table's axes under the Original renderer");
}

/// The game's next frame of [`FIELD`] through the camera `columns`,
/// `blanks` vertical blanks after the one before: with a camera record when
/// `camera`; without its scene (`scene` false) only the clear and a
/// full-screen fade.
fn next_frame(blanks: usize, columns: [[i32; 3]; 3], camera: bool, scene: bool) -> xem_gpu::Trace {
    DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        for _ in 0..blanks {
            device.vblank();
        }
        if scene {
            gte_frame(&mut device, columns, &FIELD, camera);
        } else {
            device.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000, 0xE100_0240]);
            device.write_gp0_words(&[0x0200_0000, 0, 240 << 16 | 320]);
            device.write_gp0_words(&[0x2A40_4040, xy(0, 0), xy(320, 0), xy(0, 240), xy(320, 240)]);
        }
        device.take_trace()
    })
}

/// The floor's pixels in an eye's picture over black (pure green, faded by
/// the diorama's share) and their mean green.
fn floor(picture: &[u8]) -> (usize, f32) {
    let green: Vec<u8> = picture.chunks(4).filter(|p| p[0] == 0 && p[2] == 0 && p[1] > 0).map(|p| p[1]).collect();
    (green.len(), green.iter().map(|&g| f32::from(g)).sum::<f32>() / green.len().max(1) as f32)
}

#[test]
fn leaving_the_diorama_eases_to_the_screen_as_the_last_diorama_fades_out() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let eye = [orbit(0.0, 40.0, 1.0, 0.0)];
    let black = xem_render::wgpu::Color::BLACK;
    let movie = xem_gpu::DisplayInfo { color24: true, ..DISPLAY };
    // A menu, a 24-bit movie frame, first-person inspection: each after a
    // diorama (eased in by now 1), presented every 0.1 s.
    for case in ["menu", "movie", "first person"] {
        let mut view = shown_diorama(&gpu, &FIELD, Table::default());
        let (pictures, screen, _) = present_diorama(&gpu, &mut view, &eye, (640, 480), 1.0);
        assert_eq!(screen, 0.0);
        let (shown, _) = floor(&pictures[0]);
        let (scene, display) = match case {
            // The next frame drawn by the menu (no scene, no camera record).
            "menu" => {
                view.push(next_frame(5, LEVEL, false, false));
                ("menu", &DISPLAY)
            }
            "movie" => ("field:1", &movie),
            _ => {
                view.set_first_person(true);
                ("field:1", &DISPLAY)
            }
        };
        let mut shares = Vec::new();
        for now in [1.1, 1.2, 1.35] {
            match present_shown(&gpu, &mut view, &eye, (640, 480), now, scene, black, display) {
                Some((pictures, screen, pane)) => {
                    shares.push(screen);
                    if screen < 1.0 {
                        // The last diorama, fading out over the screen.
                        let (count, green) = floor(&pictures[0]);
                        assert!(count * 10 > shown * 9, "{case}: the last diorama's floor shows ({count} of {shown} pixels)");
                        assert!((green - 255.0 * (1.0 - screen)).abs() < 3.0, "{case}: at one minus the screen's share ({green} at {screen})");
                        assert!(pane.is_some(), "{case}: and its pane");
                    }
                }
                // A movie the view does not draw: nothing left to show.
                None => shares.push(1.0),
            }
        }
        eprintln!("{case}: {shares:?}");
        assert!((shares[0] - 1.0 / 3.0).abs() < 0.01 && (shares[1] - 2.0 / 3.0).abs() < 0.01 && shares[2] == 1.0, "{case}: the screen eases in over 0.3 s: {shares:?}");
        assert!(view.diorama_space().is_none(), "{case}: no diorama once the screen shows alone");
    }
}

#[test]
fn frames_without_a_camera_record_or_a_scene_keep_the_diorama_for_two_frames_then_ease_to_the_screen() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let eye = [orbit(0.0, 40.0, 1.0, 0.0)];
    for case in ["no scene", "no camera"] {
        let mut view = shown_diorama(&gpu, &FIELD, Table::default());
        let (pictures, _, _) = present_diorama(&gpu, &mut view, &eye, (640, 480), 1.0);
        let (shown, _) = floor(&pictures[0]);
        let mut shares = Vec::new();
        for (n, now) in [1.1, 1.133, 1.166, 1.3, 1.5].into_iter().enumerate() {
            if n < 3 {
                // A frame without its scene drawn a vertical blank after the
                // last (taking that camera record, as the field menu's
                // opening fades over the field do), or a field frame five
                // blanks after it (no record of its own, the last expired).
                let frame = if case == "no scene" { next_frame(1, LEVEL, false, false) } else { next_frame(5, LEVEL, false, true) };
                view.push(frame);
            }
            let (pictures, screen, _) = present_diorama(&gpu, &mut view, &eye, (640, 480), now);
            shares.push(screen);
            if screen < 1.0 {
                let (count, green) = floor(&pictures[0]);
                assert!(count * 10 > shown * 9, "{case}: the diorama before shows, not an empty table ({count} of {shown} floor pixels)");
                assert!((green - 255.0 * (1.0 - screen)).abs() < 3.0, "{case}: at one minus the screen's share ({green} at {screen})");
            }
        }
        eprintln!("{case}: {shares:?}");
        assert_eq!(&shares[..2], [0.0, 0.0], "{case}: two frames keep the diorama before");
        assert!(shares.windows(2).skip(1).all(|w| w[1] > w[0]) && shares[4] == 1.0, "{case}: then the screen eases in: {shares:?}");
        // The field drawn again with its camera: eased back into the diorama.
        view.push(next_frame(1, LEVEL, true, true));
        let (_, screen, _) = present_diorama(&gpu, &mut view, &eye, (640, 480), 1.6);
        assert!((screen - 2.0 / 3.0).abs() < 0.01, "{case}: the diorama eases in again: {screen}");
    }
}

/// A camera turned `degrees` about the world's up from [`LEVEL`].
fn turned(degrees: f32) -> [[i32; 3]; 3] {
    let (sin, cos) = degrees.to_radians().sin_cos();
    // view = R · world with R = Ry(-degrees): its columns.
    [[cos, 0.0, sin], [0.0, 1.0, 0.0], [-sin, 0.0, cos]].map(|column| column.map(|c| (c * 4096.0).round() as i32))
}

#[test]
fn the_diorama_is_built_once_a_game_frame_while_the_table_turns() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let eye = [orbit(0.0, 40.0, 1.0, 0.0)];
    let mut view = shown_diorama(&gpu, &FIELD, Table::default());
    let builds = |view: &GameView| view.diorama_stats().builds;
    let before = builds(&view);
    // The camera turning 3 degrees a game frame (90 a second, as L1 turns
    // Lahan's), each frame presented three times at 90 Hz: the table turns
    // after it at every presentation.
    let mut now = 1.0;
    for frame in 1..=10 {
        view.push(next_frame(2, turned(3.0 * frame as f32), true, true));
        for _ in 0..3 {
            now += 1.0 / 90.0;
            assert_eq!(present_diorama(&gpu, &mut view, &eye, (640, 480), now).1, 0.0);
        }
    }
    assert_eq!(builds(&view) - before, 10, "one build a game frame");
    // The game paused while the table turns on towards its camera: built
    // again only as it turns ten degrees from the last build.
    let before = builds(&view);
    for _ in 0..90 {
        now += 1.0 / 90.0;
        present_diorama(&gpu, &mut view, &eye, (640, 480), now);
    }
    let paused = builds(&view) - before;
    eprintln!("{paused} builds in 90 presentations of a paused game");
    assert!((1..=2).contains(&paused), "{paused} builds while paused");
}

#[test]
fn after_inspection_the_table_eases_on_from_where_it_stood_at_a_frames_step() {
    use xem_render::ps1::HeadsetEye;
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let eye = [orbit(0.0, 40.0, 1.0, 0.0)];
    let mut view = shown_diorama(&gpu, &FIELD, Table::default());
    // The table's heading in the room (the world's x axis seen from above).
    let heading = |view: &GameView| {
        let x = view.diorama_space().expect("a diorama").room_from_world.transform_vector3(Vec3::X);
        x.z.atan2(x.x).to_degrees()
    };
    present_diorama(&gpu, &mut view, &eye, (640, 480), 1.1);
    let before = heading(&view);
    // First-person inspection takes the eyes for a second (the hosts present
    // through `present_headset`) while the camera turns 20 degrees.
    view.set_first_person(true);
    let head = [HeadsetEye { orientation: [0.0, 0.0, 0.0, 1.0], position: [0.0; 3], fov: [-1.0, 1.0, 1.0, -1.0] }];
    for k in 1..=10 {
        view.push(next_frame(6, turned(2.0 * k as f32), true, true));
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        view.present_headset(&mut encoder, &DISPLAY, "field:1", 1.1 + 0.1 * k as f64, &head).expect("presented");
        gpu.queue.submit([encoder.finish()]);
    }
    view.set_first_person(false);
    // The diorama back at once where it stood, then easing towards the
    // camera a frame's step at a time (72 Hz).
    let mut now = 2.2;
    let (_, screen, _) = present_diorama(&gpu, &mut view, &eye, (640, 480), now);
    let back = heading(&view);
    now += 1.0 / 72.0;
    present_diorama(&gpu, &mut view, &eye, (640, 480), now);
    let next = heading(&view);
    for _ in 0..144 {
        now += 1.0 / 72.0;
        present_diorama(&gpu, &mut view, &eye, (640, 480), now);
    }
    let settled = heading(&view);
    eprintln!("the table's heading {before} before inspection, {back} back, {next} a frame later, {settled} two seconds on");
    assert_eq!(screen, 0.0, "the diorama back at once");
    assert!((back - before).abs() < 1e-3, "the table where it stood ({before} then {back})");
    // An ordinary frame's step: 20 degrees times 1 - e^(-1/72/0.3), 0.9.
    let step = (next - back).abs();
    assert!(step > 0.5 && step < 1.5, "a frame's step towards the camera: {step} degrees");
    assert!(((settled - before).abs() - 20.0).abs() < 0.2, "the table faces the turned camera: {before} to {settled}");
}

#[test]
fn nothing_outside_the_tables_volume_is_drawn() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    // A floor far wider than the table at this scale.
    let mut faces = FIELD[..1].to_vec();
    faces[0].points = [[-3000.0, 200.0, 2000.0], [3000.0, 200.0, 2000.0], [-3000.0, 200.0, 400.0], [3000.0, 200.0, 400.0]];
    let mut view = shown_diorama(&gpu, &faces, Table { scale: 2.0, ..Table::default() });
    let eye = orbit(0.0, 60.0, 1.4, 0.0);
    let (pictures, _, _) = present_diorama(&gpu, &mut view, &[eye], (640, 480), 2.0);
    let space = view.diorama_space().expect("a diorama");
    // Where the table's top at x 0.4 and 0.5 m (inside and beyond its
    // volume's half width, 0.45) shows.
    let pixel = |x: f32| {
        let clip = eye.1 * eye.0 * space.room_from_table * Vec4::new(x, 0.0, 0.0, 1.0);
        let (u, v) = (clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5);
        let (px, py) = ((u * 640.0) as usize, (v * 480.0) as usize);
        pictures[0][(py * 640 + px) * 4..(py * 640 + px) * 4 + 3].to_vec()
    };
    assert_eq!((pixel(0.4), pixel(-0.4)), (vec![0, 255, 0], vec![0, 255, 0]), "the floor on the table");
    assert_eq!((pixel(0.5), pixel(-0.5)), (vec![0, 0, 0], vec![0, 0, 0]), "nothing beyond the table's volume");
}

/// With XEM_DIORAMA_DUMP set, writes a 640x480 `picture` there as NAME.png.
fn dump(name: &str, picture: &[u8]) {
    let Some(directory) = std::env::var_os("XEM_DIORAMA_DUMP") else { return };
    let file = std::fs::File::create(std::path::Path::new(&directory).join(format!("{name}.png"))).unwrap();
    let mut encoder = png::Encoder::new(file, 640, 480);
    encoder.set_color(png::ColorType::Rgba);
    let opaque: Vec<u8> = picture.chunks(4).flat_map(|p| [p[0], p[1], p[2], 255]).collect();
    encoder.write_header().unwrap().write_image_data(&opaque).unwrap();
}

/// Where the headset tests' controlled character stands (the game camera's
/// view space, y down) and the height of its eye there: 0.9 of the 74 units
/// a field's characters stand where the frame draws no sprite of theirs.
const HEADSET_FEET: [f32; 3] = [0.0, 100.0, 500.0];
const EYE_Y: f32 = 100.0 - 66.6;

/// Flat quads through the GTE (offset 160, 120; H 256) from their view-space
/// corners (0 1 / 2 3) and 0xBBGGRR colours, over a black screen, after the
/// port's records (with `eye`) of the field camera (the world's axes as the
/// view's) and of the controlled character standing at `HEADSET_FEET` (its
/// sprite drawing nothing); then the interface: a white rectangle and a grey
/// semi-transparent one.
fn headset_scene(gpu: &mut xem_gpu::Gpu, eye: bool, quads: &[([[f32; 3]; 4], u32)]) {
    gpu.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000, 0xE100_0200]);
    gpu.write_gp0_words(&[0x0200_0000, 0, 240 << 16 | 320]);
    if eye {
        let entry = |class: u8, owner: u32, view: [i32; 3]| Projection { sxy: 0, sz: 0, class, x: 0, y: 0, offset: (160, 120), h: 256, owner, view };
        let records = [entry(6, 0, [4096, 0, 0]), entry(6, 1, [0, 4096, 0]), entry(6, 2, [0, 0, 4096]), entry(7, 0x8020_0000, HEADSET_FEET.map(|c| c as i32))];
        gpu.trace_projections(0, &records, &[], &[]);
    }
    let mut projections = Vec::new();
    let mut packets = Vec::new();
    for (corners, color) in quads {
        let mut words = vec![0x2800_0000 | color];
        let mut points = Vec::new();
        for &[x, y, z] in corners {
            let (sx, sy) = (160.0 + 256.0 * x / z, 120.0 + 256.0 * y / z);
            let sxy = xy(sx.floor() as i32, sy.floor() as i32);
            points.push((words.len(), projections.len()));
            words.push(sxy);
            projections.push(Projection {
                sxy,
                sz: z as u16,
                class: 0,
                x: (sx * 65536.0) as i32,
                y: (sy * 65536.0) as i32,
                offset: (160, 120),
                h: 256,
                owner: 0,
                view: [x as i32, y as i32, z as i32],
            });
        }
        packets.push((words, points));
    }
    submit(gpu, &projections, &packets);
    gpu.write_gp0_words(&[0x60FF_FFFF, xy(250, 200), 30 << 16 | 60]);
    gpu.write_gp0_words(&[0x6280_8080, xy(20, 200), 20 << 16 | 40]);
}

#[test]
fn a_headset_sees_from_the_characters_eye_through_each_eyes_field_of_view_with_the_interface_on_its_pane() {
    use xem_render::ps1::{GameView, HeadsetEye, HostClass, ViewConfig};
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    // A thousand units ahead of the eye, four quadrants meeting on its line
    // of sight: red up left, green up right, blue down left, yellow down
    // right; and a magenta floor at the character's feet from 300 units
    // behind the eye to 900 ahead, which the near plane clips.
    let z = HEADSET_FEET[2] + 1000.0;
    let quad = |x0: f32, x1: f32, y0: f32, y1: f32| [[x0, y0, z], [x1, y0, z], [x0, y1, z], [x1, y1, z]];
    let floor = [[-300.0, HEADSET_FEET[1], HEADSET_FEET[2] - 300.0], [300.0, HEADSET_FEET[1], HEADSET_FEET[2] - 300.0], [-300.0, HEADSET_FEET[1], HEADSET_FEET[2] + 900.0], [300.0, HEADSET_FEET[1], HEADSET_FEET[2] + 900.0]];
    let scene = [
        (quad(-3000.0, 0.0, EYE_Y - 3000.0, EYE_Y), 0x0000FF),
        (quad(0.0, 3000.0, EYE_Y - 3000.0, EYE_Y), 0x00FF00),
        (quad(-3000.0, 0.0, EYE_Y, EYE_Y + 3000.0), 0xFF0000),
        (quad(0.0, 3000.0, EYE_Y, EYE_Y + 3000.0), 0x00FFFF),
        (floor, 0xFF00FF),
    ];
    let (sync, trace) = traced(|device| headset_scene(device, true, &scene));
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    view.configure(ViewConfig::resolve(&Default::default(), HostClass::Headset, (320, 240), view.max_scale()));
    view.push(sync);
    view.push(trace);
    view.set_first_person(true);
    let display = xem_gpu::DisplayInfo { enabled: true, x: 0, y: 0, width: 320, height: 240, mode_width: 320, color24: false, interlaced: false, pal: false };
    // Each eye 32 mm off the head's centre, its field of view wider away
    // from the nose and wider down than up.
    let eyes = |orientation: [f32; 4]| {
        [
            HeadsetEye { orientation, position: [-0.032, 0.0, 0.0], fov: [-1.0, 0.5, 0.5, -1.0] },
            HeadsetEye { orientation, position: [0.032, 0.0, 0.0], fov: [-0.5, 1.0, 0.5, -1.0] },
        ]
    };
    let present = |view: &mut GameView, eyes: &[HeadsetEye]| -> (Vec<Vec<u8>>, Vec<u8>) {
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let count = view.present_headset(&mut encoder, &display, "field:1", 0.0, eyes).expect("presented").eyes.len();
        gpu.queue.submit([encoder.finish()]);
        let pictures = (0..count).map(|i| read_rgba8(&gpu.device, &gpu.queue, view.eye_texture(i).expect("an eye"))).collect();
        (pictures, read_rgba8(&gpu.device, &gpu.queue, view.presented_texture().expect("the pane")))
    };
    let at = |pixels: &[u8], x: usize, y: usize| -> [u8; 4] { pixels[(y * 320 + x) * 4..][..4].try_into().unwrap() };
    let named = |p: [u8; 4]| match p[..3] {
        [r, g, b] if r > 200 && g < 60 && b < 60 => "red",
        [r, g, b] if r < 60 && g > 200 && b < 60 => "green",
        [r, g, b] if r < 60 && g < 60 && b > 200 => "blue",
        [r, g, b] if r > 200 && g > 200 && b < 60 => "yellow",
        [r, g, b] if r > 200 && g < 60 && b > 200 => "magenta",
        [r, g, b] if r > 200 && g > 200 && b > 200 => "white",
        [r, g, b] if r < 20 && g < 20 && b < 20 => "black",
        _ => "other",
    };
    let level = [0.0, 0.0, 0.0, 1.0];
    let (pictures, pane) = present(&mut view, &eyes(level));
    assert_eq!(pictures.len(), 2, "both eyes look from within the scene");
    // The left eye's line of sight at two thirds across (tangent 0 of -1 to
    // 0.5), its horizon a third down (0.5 to -1): the frustum's edges on
    // the frame's. The floor reaches its bottom edge, clipped at the near
    // plane, and ends 900 ahead, below the horizon.
    for (x, y, expected) in [
        (1, 1, "red"), (318, 1, "green"), (209, 76, "red"), (218, 76, "green"), (209, 84, "blue"), (218, 84, "yellow"),
        (1, 238, "magenta"), (318, 238, "magenta"), (160, 200, "magenta"),
    ] {
        assert_eq!(named(at(&pictures[0], x, y)), expected, "the left eye at {x},{y}: {:?}", at(&pictures[0], x, y));
    }
    // The right eye's a third across (-0.5 to 1).
    for (x, y, expected) in [(102, 76, "red"), (111, 76, "green"), (102, 84, "blue"), (111, 84, "yellow"), (318, 238, "magenta")] {
        assert_eq!(named(at(&pictures[1], x, y)), expected, "the right eye at {x},{y}: {:?}", at(&pictures[1], x, y));
    }
    // The interface is on the pane alone: opaque where drawn, half covering
    // where semi-transparent, transparent elsewhere; neither eye carries it.
    assert_eq!(at(&pane, 280, 215), [255, 255, 255, 255], "the white rectangle, opaque");
    let semi = at(&pane, 40, 210);
    assert!((120..=136).contains(&semi[3]) && (56..=72).contains(&semi[0]), "the grey semi-transparent rectangle, half covering: {semi:?}");
    for (x, y) in [(1, 1), (100, 50), (160, 200)] {
        assert_eq!(at(&pane, x, y)[3], 0, "the scene is left out of the pane at {x},{y}");
    }
    for (eye, picture) in pictures.iter().enumerate() {
        assert_eq!(named(at(picture, 280, 215)), "magenta", "eye {eye} shows the floor where the interface was drawn");
        assert_eq!(named(at(picture, 40, 210)), "magenta", "eye {eye} shows the floor where the interface was drawn");
    }
    // The listener looks along the view's level heading, whatever the head does.
    let forward = view.listener_axes().expect("looking").0[2];
    assert!((forward[2] - 1.0).abs() < 1e-4, "{forward:?}");

    // A quarter turn of the head to the right: the scene ahead leaves the
    // right of the view, the listener stays.
    let right = [0.0, -std::f32::consts::FRAC_1_SQRT_2, 0.0, std::f32::consts::FRAC_1_SQRT_2];
    let (turned, _) = present(&mut view, &eyes(right));
    assert_eq!(named(at(&turned[0], 318, 1)), "black", "nothing to the right");
    assert_ne!(turned[0], pictures[0]);
    assert!((view.listener_axes().expect("looking").0[2][2] - 1.0).abs() < 1e-4, "the listener is head-free");

    // A frame without the character's record: the eyes stay where they were
    // rather than cut to the game's camera.
    let (sync, trace) = traced(|device| headset_scene(device, false, &scene));
    view.push(sync);
    view.push(trace);
    let (held, _) = present(&mut view, &eyes(level));
    assert_eq!(held.len(), 2, "the eyes go on through a frame without the character's record");
    assert_eq!(named(at(&held[0], 209, 76)), "red");
    assert_eq!(named(at(&held[0], 218, 84)), "yellow");

    // A state loaded (all of VRAM again, another moment's grey screen):
    // nothing of the shown buffer to replay. The eyes keep the room, and
    // the pane shows nothing rather than the display, whose scene and
    // interface the game's camera saw.
    let loaded = DEVICE.with(|device| {
        let mut device = device.borrow_mut();
        *device = xem_gpu::Gpu::new();
        device.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 239 << 10 | 319, 0xE500_0000, 0xE100_0200]);
        device.write_gp0_words(&[0x0280_8080, 0, 240 << 16 | 320]);
        device.set_trace(true);
        device.take_trace()
    });
    view.push(loaded);
    let (held, pane) = present(&mut view, &eyes(level));
    assert_eq!(held.len(), 2, "the eyes go on through a state loaded");
    assert_eq!(named(at(&held[0], 209, 76)), "red");
    let shown = pane.chunks_exact(4).filter(|p| *p != [0, 0, 0, 0]).count();
    assert_eq!(shown, 0, "the pane shows nothing of the loaded display ({shown} pixels)");
}

#[test]
fn a_headsets_eye_keeps_its_colours_composited_and_the_pane_covers_by_its_alpha() {
    use xem_render::ps1::{GameView, HeadsetEye, HostClass, ViewConfig};
    use xem_render::{Compositor, Quad, Rect, wgpu};
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    // A wall of middle tones ahead of the eye (encoded to sRGB twice, they
    // would come out paler), drawn without the mask bit: the frame's alpha
    // there is 0, which is no coverage.
    let z = HEADSET_FEET[2] + 1000.0;
    let quad = |x0: f32, x1: f32, y0: f32, y1: f32| [[x0, y0, z], [x1, y0, z], [x0, y1, z], [x1, y1, z]];
    let wall = [
        quad(-3000.0, 0.0, EYE_Y - 3000.0, EYE_Y),
        quad(0.0, 3000.0, EYE_Y - 3000.0, EYE_Y),
        quad(-3000.0, 0.0, EYE_Y, EYE_Y + 3000.0),
        quad(0.0, 3000.0, EYE_Y, EYE_Y + 3000.0),
    ]
    .map(|corners| (corners, 0x60_4020));
    let (sync, trace) = traced(|device| headset_scene(device, true, &wall));
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    view.configure(ViewConfig::resolve(&Default::default(), HostClass::Headset, (320, 240), view.max_scale()));
    view.push(sync);
    view.push(trace);
    view.set_first_person(true);
    let display = xem_gpu::DisplayInfo { enabled: true, x: 0, y: 0, width: 320, height: 240, mode_width: 320, color24: false, interlaced: false, pal: false };
    let eye = HeadsetEye { orientation: [0.0, 0.0, 0.0, 1.0], position: [0.0; 3], fov: [-1.0, 1.0, 0.75, -0.75] };
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    let presented = view.present_headset(&mut encoder, &display, "field:1", 0.0, &[eye]).expect("presented");
    assert_eq!(presented.eyes.len(), 1, "the eye looks from within the scene");
    let (picture, pane) = (presented.eyes[0].clone(), presented.screen.view.clone());
    gpu.queue.submit([encoder.finish()]);
    let drawn = read_rgba8(&gpu.device, &gpu.queue, view.eye_texture(0).expect("an eye"));
    assert_eq!(drawn[(60 * 320 + 100) * 4..][..4], [32, 64, 96, 0], "the wall in the eye's picture, its mask bit clear");
    // Composited as the headsets composite them, the pane over the eye's
    // picture, onto an sRGB target (OpenXR's swapchains) and a linear one
    // (a WebXR layer's buffers).
    for format in [wgpu::TextureFormat::Rgba8UnormSrgb, wgpu::TextureFormat::Rgba8Unorm] {
        let target = gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("headset target"),
            size: wgpu::Extent3d { width: 320, height: 240, depth_or_array_layers: 1 },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        let whole = Rect { x: 0.0, y: 0.0, width: 320.0, height: 240.0 };
        let mut seen = Quad::screen(&picture, whole, (320, 240));
        seen.srgb_encoded = true;
        seen.opaque = true;
        seen.filter = wgpu::FilterMode::Nearest;
        let mut interface = Quad::screen(&pane, whole, (320, 240));
        interface.srgb_encoded = true;
        interface.filter = wgpu::FilterMode::Nearest;
        let mut compositor = Compositor::new(&gpu.device, &gpu.queue, format);
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        compositor.draw(&mut encoder, &target.create_view(&Default::default()), Some(wgpu::Color::BLACK), &[seen, interface]);
        gpu.queue.submit([encoder.finish()]);
        let shown = read_rgba8(&gpu.device, &gpu.queue, &target);
        let at = |x: usize, y: usize| -> [u8; 4] { shown[(y * 320 + x) * 4..][..4].try_into().unwrap() };
        for (x, y) in [(100, 60), (5, 5), (230, 100), (60, 180)] {
            let p = at(x, y);
            let close = p[..3].iter().zip([32u8, 64, 96]).all(|(a, b)| a.abs_diff(b) <= 1);
            assert!(close && p[3] == 255, "{format:?}: the wall in its own colours at {x},{y}: {p:?}");
        }
        assert_eq!(at(280, 215), [255, 255, 255, 255], "{format:?}: the pane's white rectangle covers the wall");
    }
}

#[test]
fn a_headsets_eyes_see_each_sprite_stand_in_one_place_facing_the_head() {
    // A sprite a thousand units right of the game's camera, facing it.
    let TraceOp::Draw(mut p) = primitive(0, 0, 0) else { unreachable!() };
    p.shape = Shape::Quad;
    p.class = Class::Sprite;
    p.owner = 0x8010_0000;
    p.vertices[3] = p.vertices[0];
    let ops = vec![with_view(TraceOp::Draw(p), &[[950.0, -200.0, 0.0], [1050.0, -200.0, 0.0], [950.0, 0.0, 0.0], [1050.0, 0.0, 0.0]])];
    // Two eyes 2.6 units apart, turned differently (canted displays): one
    // looks right along the game camera's +x, one 30 degrees off it.
    let turned = |angle: f32| {
        let (s, c) = angle.sin_cos();
        [[-s, 0.0, -c], [0.0, 1.0, 0.0], [c, 0.0, -s]]
    };
    let eyes = [([0.0, 0.0, -1.3], turned(0.0)), ([0.0, 0.0, 1.3], turned(0.5236))];
    // Each eye's points back in the game camera's view space.
    let seen = |facing: Option<[f32; 3]>| -> Vec<Vec<[f32; 3]>> {
        eyes.iter()
            .map(|&(eye, axes)| {
                let mut ops = ops.clone();
                first_person(&mut ops, &Look { eye, axes, facing, ..AHEAD });
                let TraceOp::Draw(p) = &ops[0] else { unreachable!() };
                p.vertices.iter().map(|v| {
                    let m = v.view.unwrap();
                    std::array::from_fn(|i| eye[i] + m[0] * axes[0][i] + m[1] * axes[1][i] + m[2] * axes[2][i])
                }).collect()
            })
            .collect()
    };
    let close = |a: [f32; 3], b: [f32; 3]| (0..3).all(|i| (a[i] - b[i]).abs() < 0.05);
    // Facing the head between the eyes: one sprite for both, its width
    // across the line to the head.
    let both = seen(Some([0.0; 3]));
    assert!(both[0].iter().zip(&both[1]).all(|(a, b)| close(*a, *b)), "one sprite for both eyes: {both:?}");
    let across = [both[0][1][0] - both[0][0][0], both[0][1][1] - both[0][0][1], both[0][1][2] - both[0][0][2]];
    assert!((across.iter().map(|c| c * c).sum::<f32>().sqrt() - 100.0).abs() < 0.05, "as wide as the game drew it: {across:?}");
    assert!(across[0].abs() < 0.05, "facing the head along the game camera's +x: {across:?}");
    // Each facing its own eye's plane, the eyes would see two.
    let apart = seen(None);
    assert!(!apart[0].iter().zip(&apart[1]).all(|(a, b)| close(*a, *b)), "each eye's own plane turns the sprite");
}
