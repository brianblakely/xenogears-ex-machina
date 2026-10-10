//! Headless rendering on whatever adapter wgpu finds; CI and the runtime shell
//! use lavapipe (`VK_ICD_FILENAMES=$XEM_LAVAPIPE_ICD`).

use std::collections::HashSet;
use xem_render::glam::Mat4;
use xem_render::{
    Compositor, Gpu, Quad, Rect, SceneRenderer, ViewTarget, Viewport, demo_camera, read_rgba8, wgpu,
};

const FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8UnormSrgb;

fn target(gpu: &Gpu, width: u32, height: u32) -> wgpu::Texture {
    gpu.device.create_texture(&wgpu::TextureDescriptor {
        label: Some("test target"),
        size: wgpu::Extent3d {
            width,
            height,
            depth_or_array_layers: 1,
        },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format: FORMAT,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT
            | wgpu::TextureUsages::TEXTURE_BINDING
            | wgpu::TextureUsages::COPY_SRC,
        view_formats: &[],
    })
}

fn pixel(pixels: &[u8], width: u32, x: u32, y: u32) -> [u8; 4] {
    let i = ((y * width + x) * 4) as usize;
    pixels[i..i + 4].try_into().unwrap()
}

#[test]
fn scene_renders_stereo_views_and_composites() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    eprintln!("adapter: {:?}", gpu.adapter.get_info());
    let (width, height) = (256, 128);
    let image = target(&gpu, width, height);
    let view = image.create_view(&Default::default());

    // Two eyes side by side in one target, 6.4 cm apart.
    let (center, proj) = demo_camera(1.0, 2.0);
    let eye = |offset: f32| Mat4::from_translation(glam_x(-offset)) * center;
    let mut scene = SceneRenderer::new(&gpu.device, &gpu.queue, FORMAT);
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    scene.render_views(
        &mut encoder,
        &[
            ViewTarget {
                view: eye(-0.032),
                proj,
                target: &view,
                viewport: Viewport {
                    x: 0.0,
                    y: 0.0,
                    width: 128.0,
                    height: 128.0,
                },
            },
            ViewTarget {
                view: eye(0.032),
                proj,
                target: &view,
                viewport: Viewport {
                    x: 128.0,
                    y: 0.0,
                    width: 128.0,
                    height: 128.0,
                },
            },
        ],
        2.0,
    );
    gpu.queue.submit([encoder.finish()]);
    let pixels = read_rgba8(&gpu.device, &gpu.queue, &image);

    let colors: HashSet<[u8; 4]> = pixels
        .chunks_exact(4)
        .map(|p| p.try_into().unwrap())
        .collect();
    assert!(colors.len() > 200, "only {} distinct colours", colors.len());
    assert!(
        pixels.chunks_exact(4).all(|p| p[3] == 255),
        "every pixel is drawn"
    );
    // Sky above, the red centre cube in the middle of each eye, the eyes differ.
    for eye_x in [0, 128] {
        let sky = pixel(&pixels, width, eye_x + 64, 2);
        assert!(sky[2] > sky[0], "sky is blue: {sky:?}");
        let cube = pixel(&pixels, width, eye_x + 64, 64);
        assert!(
            cube[0] > cube[1] + 40 && cube[0] > cube[2] + 40,
            "cube is red: {cube:?}"
        );
    }
    let left: Vec<_> = (0..128)
        .flat_map(|y| (0..128).map(move |x| (x, y)))
        .collect();
    let differing = left
        .iter()
        .filter(|&&(x, y)| pixel(&pixels, width, x, y) != pixel(&pixels, width, x + 128, y))
        .count();
    assert!(
        differing > 100,
        "the eyes see different images ({differing} pixels differ)"
    );

    // Composite the left eye, scaled, under a half-transparent copy of the right.
    let output = target(&gpu, 64, 64);
    let mut compositor = Compositor::new(&gpu.device, &gpu.queue, FORMAT);
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    let mut overlay = Quad::screen(
        &view,
        Rect {
            x: 32.0,
            y: 0.0,
            width: 32.0,
            height: 64.0,
        },
        (64, 64),
    );
    overlay.opacity = 0.5;
    compositor.draw(
        &mut encoder,
        &output.create_view(&Default::default()),
        Some(wgpu::Color::GREEN),
        &[
            Quad::screen(
                &view,
                Rect {
                    x: 0.0,
                    y: 16.0,
                    width: 64.0,
                    height: 32.0,
                },
                (64, 64),
            ),
            overlay,
        ],
    );
    gpu.queue.submit([encoder.finish()]);
    let composed = read_rgba8(&gpu.device, &gpu.queue, &output);
    assert_eq!(
        pixel(&composed, 64, 4, 4),
        [0, 255, 0, 255],
        "clear colour outside the quads"
    );
    let sky = pixel(&composed, 64, 4, 17);
    assert!(
        sky[2] > sky[0] && sky[1] < 250,
        "game image inside its rect: {sky:?}"
    );
    let blended = pixel(&composed, 64, 40, 4);
    assert!(
        blended[1] > 100 && blended[2] > 40,
        "overlay blends with the clear colour: {blended:?}"
    );
}

fn glam_x(x: f32) -> xem_render::glam::Vec3 {
    xem_render::glam::Vec3::new(x, 0.0, 0.0)
}
