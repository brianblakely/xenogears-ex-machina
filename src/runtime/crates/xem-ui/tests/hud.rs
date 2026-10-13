use i_slint_backend_testing::ElementHandle;
use std::time::Duration;
use xem_render::{Gpu, read_rgba8};
use xem_ui::{FrameRateUi, SharedGpu};

#[test]
fn activity_and_state_results_show_without_fps_and_expire_independently() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut hud = FrameRateUi::new(shared, 1.0).unwrap();
    let at = Duration::from_secs;
    hud.frame(at(0), 1.0, false).unwrap();
    assert_eq!(hud.size(), (304, 110));
    assert!(!hud.visible());
    assert!(read_rgba8(&gpu.device, &gpu.queue, hud.texture()).chunks_exact(4).all(|p| p[3] == 0));

    hud.set_activity(true, false);
    hud.frame(at(1), 1.0, false).unwrap();
    assert!(hud.visible());
    assert!(ElementHandle::find_by_accessible_label(hud.component(), "Fast-forwarding").next().is_some());
    let pixels = read_rgba8(&gpu.device, &gpu.queue, hud.texture());
    assert!(pixels.chunks_exact(4).any(|p| p[3] > 0), "the activity icon is drawn with FPS disabled");
    assert!(pixels[304 * 28 * 4..].chunks_exact(4).all(|p| p[3] == 0), "the unused HUD area stays transparent");
    hud.set_activity(false, true);
    hud.frame(at(2), 1.0, false).unwrap();
    assert!(ElementHandle::find_by_accessible_label(hud.component(), "Rewinding").next().is_some());
    assert!(ElementHandle::find_by_accessible_label(hud.component(), "Fast-forwarding").next().is_none());

    hud.notify_state(true, None, at(2));
    hud.frame(at(2), 1.0, false).unwrap();
    assert_eq!(hud.notice().as_deref(), Some("State saved"));
    let success = ElementHandle::find_by_accessible_label(hud.component(), "State saved").next().unwrap();
    assert_eq!((success.absolute_position().x, success.absolute_position().y), (0.0, 34.0));
    assert_eq!((success.size().width, success.size().height), (160.0, 40.0));
    capture(&gpu, &hud, "saved-with-rewind");
    hud.frame(at(6), 1.0, false).unwrap();
    assert!(hud.notice().is_none());
    assert!(hud.visible(), "rewind remains after the success expires");
    hud.set_activity(false, false);
    hud.frame(at(6), 1.0, false).unwrap();
    assert!(!hud.visible());

    hud.notify_state(false, Some("The selected state belongs to a different game module."), at(6));
    hud.frame(at(13), 2.0, false).unwrap();
    assert_eq!(hud.size(), (608, 220));
    assert_eq!(hud.notice().as_deref(), Some("Could not load state: The selected state belongs to a different game module."));
    assert!(hud.visible());
    let error = ElementHandle::find_by_accessible_label(hud.component(), &hud.notice().unwrap()).next().unwrap();
    assert_eq!((error.absolute_position().x, error.absolute_position().y), (0.0, 0.0));
    assert_eq!((error.size().width, error.size().height), (304.0, 76.0));
    capture(&gpu, &hud, "load-error-2x");
    hud.frame(at(14), 2.0, false).unwrap();
    assert!(!hud.visible(), "errors remain for eight seconds");
    hud.notify_state(false, None, at(14));
    hud.frame(at(14), 1.0, false).unwrap();
    assert_eq!(hud.notice().as_deref(), Some("State loaded"));
    let loaded = ElementHandle::find_by_accessible_label(hud.component(), "State loaded").next().unwrap();
    assert_eq!((loaded.absolute_position().x, loaded.absolute_position().y), (0.0, 0.0));
    capture(&gpu, &hud, "loaded");

    for n in 0..=60 {
        hud.frame(at(18) + Duration::from_secs_f64(n as f64 / 60.0), 1.0, true).unwrap();
    }
    assert_eq!(hud.label(), "60 FPS");
    assert!(hud.notice().is_none());
    assert!(hud.visible(), "the FPS counter remains independent");
    hud.set_activity(true, true);
    hud.frame(at(19), 1.0, true).unwrap();
    let fps = ElementHandle::find_by_accessible_label(hud.component(), "60 FPS").next().unwrap();
    assert_eq!((fps.absolute_position().x + fps.size().width / 2.0, fps.absolute_position().y + fps.size().height / 2.0), (48.0, 14.0));
    for (label, x) in [("Fast-forwarding", 102.0), ("Rewinding", 144.0)] {
        let activity = ElementHandle::find_by_accessible_label(hud.component(), label).next().unwrap();
        assert_eq!((activity.absolute_position().x, activity.absolute_position().y), (x, 0.0));
    }
}

/// Optional review captures use the same texture and assertions as the test.
fn capture(gpu: &Gpu, hud: &FrameRateUi, name: &str) {
    let Some(dir) = std::env::var_os("XEM_UI_CAPTURE_DIR") else { return };
    let dir = std::path::PathBuf::from(dir);
    std::fs::create_dir_all(&dir).unwrap();
    let pixels = read_rgba8(&gpu.device, &gpu.queue, hud.texture());
    let rgb: Vec<u8> = pixels.chunks_exact(4).flat_map(|p| {
        [0x42u16, 0x4b, 0x56].into_iter().enumerate().map(move |(c, background)|
            (u16::from(p[c]) + background * (255 - u16::from(p[3])) / 255).min(255) as u8)
    }).collect();
    let file = std::fs::File::create(dir.join(format!("hud-{name}.png"))).unwrap();
    let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), hud.size().0, hud.size().1);
    encoder.set_color(png::ColorType::Rgb);
    encoder.write_header().unwrap().write_image_data(&rgb).unwrap();
}
