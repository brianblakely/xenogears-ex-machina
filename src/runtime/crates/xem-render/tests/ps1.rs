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

#[test]
fn original_fidelity_matches_the_software_gpu() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut device = xem_gpu::Gpu::new();
    device.set_trace(true);
    upload_texture(&mut device);
    scene(&mut device);
    let trace = device.take_trace();
    let mut renderer = Ps1Renderer::new(&gpu.device, &gpu.queue, 1);
    renderer.set_options(DrawOptions { fidelity: Fidelity::ORIGINAL, ..Default::default() }, 1);
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    renderer.draw(&mut encoder, &trace);
    gpu.queue.submit([encoder.finish()]);
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
