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
    assert!(pixels[304 * 32 * 4..].chunks_exact(4).all(|p| p[3] == 0), "the unused HUD area stays transparent");
    hud.set_activity(false, true);
    hud.frame(at(2), 1.0, false).unwrap();
    assert!(ElementHandle::find_by_accessible_label(hud.component(), "Rewinding").next().is_some());
    assert!(ElementHandle::find_by_accessible_label(hud.component(), "Fast-forwarding").next().is_none());

    hud.notify_state(true, None, at(2));
    hud.frame(at(2), 1.0, false).unwrap();
    assert_eq!(hud.notice().as_deref(), Some("State saved"));
    // Every element one chip: the result below the activity, as tall as it.
    let success = ElementHandle::find_by_accessible_label(hud.component(), "State saved").next().unwrap();
    assert_eq!((success.absolute_position().x, success.absolute_position().y), (0.0, 38.0));
    assert_eq!(success.size().height, 32.0);
    assert!((96.0..160.0).contains(&success.size().width), "as wide as its words: {:?}", success.size());
    capture(&gpu, &hud, "saved-with-rewind");
    hud.frame(at(6), 1.0, false).unwrap();
    assert!(hud.notice().is_none());
    assert!(hud.visible(), "rewind remains after the success expires");
    hud.set_activity(false, false);
    hud.frame(at(6), 1.0, false).unwrap();
    assert!(!hud.visible());

    // The runtime's refusal of a state another game module build saved
    // (`StateIdentity::difference`): the chip shows its start, the field.
    let refused = "incompatible state: the state's game module is \
        \"0d5be8a3f1c2479e6b0a8d3c5e7f9182a4b6c8d0e2f4a6b8c0d2e4f6a8b0c2d4\", this session's is \
        \"26cf0a08a5bfa7e219f5f3450068fc68406e6b788f8270ea05282a43758d0d53\"";
    hud.notify_state(false, Some(refused), at(6));
    hud.frame(at(13), 2.0, false).unwrap();
    assert_eq!(hud.size(), (608, 220));
    assert_eq!(hud.notice(), Some(format!("Could not load state: {refused}")));
    assert!(hud.visible());
    let error = ElementHandle::find_by_accessible_label(hud.component(), &hud.notice().unwrap()).next().unwrap();
    assert_eq!((error.absolute_position().x, error.absolute_position().y), (0.0, 0.0));
    assert_eq!((error.size().width, error.size().height), (304.0, 72.0), "the chip grown to give the cause");
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
    assert_eq!((fps.absolute_position().x + fps.size().width / 2.0, fps.absolute_position().y + fps.size().height / 2.0), (44.0, 16.0));
    for (label, x) in [("Fast-forwarding", 94.0), ("Rewinding", 132.0)] {
        let activity = ElementHandle::find_by_accessible_label(hud.component(), label).next().unwrap();
        assert_eq!((activity.absolute_position().x, activity.absolute_position().y), (x, 0.0));
        assert_eq!((activity.size().width, activity.size().height), (32.0, 32.0), "{label}: a chip like the others");
    }
    capture(&gpu, &hud, "fps-with-activity");
}

#[test]
fn retroachievements_notices_show_their_detail_one_after_another() {
    use xem_ui::NoticeKind;
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut hud = FrameRateUi::new(shared, 1.0).unwrap();
    let at = Duration::from_secs;
    hud.notify(NoticeKind::Information, "Xenogears", "1 of 2 achievements unlocked", None, 4, at(0));
    hud.frame(at(0), 1.0, false).unwrap();
    assert_eq!(hud.notice().as_deref(), Some("Xenogears: 1 of 2 achievements unlocked"));
    // An unlock while the game's line shows waits for it, then shows its
    // own, as tall as a failure's to give the description.
    hud.notify(NoticeKind::Achievement, "Seven", "RAM holds seven · 5 points", None, 6, at(1));
    hud.notify(NoticeKind::Achievement, "Scratchpad", "The scratchpad holds nine · 10 points", None, 6, at(1));
    hud.frame(at(3), 1.0, false).unwrap();
    assert_eq!(hud.notice().as_deref(), Some("Xenogears: 1 of 2 achievements unlocked"));
    hud.frame(at(4), 1.0, false).unwrap();
    assert_eq!(hud.notice().as_deref(), Some("Seven: RAM holds seven · 5 points"));
    let chip = ElementHandle::find_by_accessible_label(hud.component(), &hud.notice().unwrap()).next().unwrap();
    assert_eq!((chip.absolute_position().x, chip.absolute_position().y), (0.0, 0.0));
    assert_eq!((chip.size().width, chip.size().height), (304.0, 72.0), "the chip grown to give the description");
    capture(&gpu, &hud, "achievement");
    hud.frame(at(10), 1.0, false).unwrap();
    assert_eq!(hud.notice().as_deref(), Some("Scratchpad: The scratchpad holds nine · 10 points"));
    // A state's result shows at once, over a queued notice's turn.
    hud.notify_state(true, None, at(11));
    hud.frame(at(11), 1.0, false).unwrap();
    assert_eq!(hud.notice().as_deref(), Some("State saved"));
    hud.frame(at(15), 1.0, false).unwrap();
    assert!(hud.notice().is_none() && !hud.visible(), "nothing more queued");
}

#[test]
fn an_unlock_shows_its_badge_in_place_of_the_trophy_below_the_fps_and_the_activity() {
    use xem_ui::NoticeKind;
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut hud = FrameRateUi::new(shared, 1.0).unwrap();
    // During play: the FPS counter and fast-forward.
    for n in 0..=60 {
        hud.frame(Duration::from_secs_f64(n as f64 / 60.0), 1.0, true).unwrap();
    }
    hud.set_activity(true, false);
    let at = Duration::from_secs;
    let unlock = "Welcome to Lahan: Arrive in Lahan village · 5 points";
    hud.notify(NoticeKind::Achievement, "Welcome to Lahan", "Arrive in Lahan village · 5 points", Some(&badge_png()), 6, at(1));
    hud.frame(at(1), 1.0, true).unwrap();
    let badge = hud.component().get_notice_badge().size();
    assert_eq!((badge.width, badge.height), (64, 64));
    let chip = ElementHandle::find_by_accessible_label(hud.component(), unlock).next().unwrap();
    assert_eq!((chip.absolute_position().x, chip.absolute_position().y), (0.0, 38.0), "below the FPS counter and the activity");
    assert_eq!((chip.size().width, chip.size().height), (304.0, 72.0));
    for label in ["60 FPS", "Fast-forwarding"] {
        assert!(ElementHandle::find_by_accessible_label(hud.component(), label).next().is_some(), "{label} stays");
    }
    // Drawn where the trophy was: the badge's gold disc at the chip's left.
    let pixels = read_rgba8(&gpu.device, &gpu.queue, hud.texture());
    let centre = &pixels[((38 + 36) * 304 + 36) * 4..][..4];
    assert!(centre[0] > 200 && centre[1] > 160 && centre[2] < 130 && centre[3] == 255, "the badge's centre: {centre:?}");
    capture(&gpu, &hud, "achievement-badge-with-fps-and-fast-forward");

    // Data that is no badge: the trophy, as without one.
    hud.notify(NoticeKind::Achievement, "Seven", "RAM holds seven · 5 points", Some(b"badge 101"), 6, at(1));
    hud.frame(at(7), 1.0, true).unwrap();
    assert_eq!(hud.notice().as_deref(), Some("Seven: RAM holds seven · 5 points"));
    assert_eq!(hud.component().get_notice_badge().size().width, 0);
    capture(&gpu, &hud, "achievement-trophy-with-fps-and-fast-forward");
}

#[test]
fn the_hud_keeps_clear_of_controls_level_with_it_and_no_narrower_than_its_least() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut hud = FrameRateUi::new(shared, 2.0).unwrap();
    let at = (16.0, 16.0);
    // A phone's top buttons from x 600 (target pixels at 2x), level with
    // the HUD, and a control below it: 8 logical pixels before the buttons.
    hud.keep_clear(at, 2.0, [(600.0, 40.0, 100.0, 100.0), (100.0, 600.0, 200.0, 200.0)]);
    assert_eq!(hud.size(), (2 * 284, 220));
    // Nothing level with it: whole; buttons crowding it: its least width.
    hud.keep_clear(at, 2.0, [(100.0, 600.0, 200.0, 200.0)]);
    assert_eq!(hud.size(), (2 * 304, 220));
    hud.keep_clear(at, 2.0, [(300.0, 40.0, 100.0, 100.0)]);
    assert_eq!(hud.size(), (2 * 200, 220));
}

/// A badge as RetroAchievements' server gives one: a 64-pixel PNG (a gold
/// disc on blue).
fn badge_png() -> Vec<u8> {
    let mut rgba = Vec::with_capacity(64 * 64 * 4);
    for y in 0..64u8 {
        for x in 0..64u8 {
            let distance = (f32::from(x) - 31.5).hypot(f32::from(y) - 31.5);
            rgba.extend(if distance < 19.0 {
                [240, 199, 94, 255]
            } else if distance < 23.0 {
                [255, 236, 170, 255]
            } else {
                [24 + y, 48 + x / 2, 112, 255]
            });
        }
    }
    let mut png = Vec::new();
    let mut encoder = png::Encoder::new(&mut png, 64, 64);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    encoder.write_header().unwrap().write_image_data(&rgba).unwrap();
    png
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
