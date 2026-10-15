//! The settings panel through the custom platform, on the same device as the
//! scene. Run on lavapipe: `VK_ICD_FILENAMES=$XEM_LAVAPIPE_ICD`.

use i_slint_backend_testing::{AccessibleRole, ElementHandle};
use std::cell::{Cell, RefCell};
use std::rc::Rc;
use xem_render::{Gpu, SceneRenderer, ViewTarget, Viewport, demo_camera, read_rgba8, wgpu};
use xem_settings::{MemoryStore, SettingsService};
use xem_ui::{LogicalPosition, PointerEventButton, SettingsUi, SharedGpu, WindowEvent};

fn click(ui: &SettingsUi, position: LogicalPosition) {
    ui.dispatch(WindowEvent::PointerMoved { position });
    ui.dispatch(WindowEvent::PointerPressed {
        position,
        button: PointerEventButton::Left,
    });
    ui.dispatch(WindowEvent::PointerReleased {
        position,
        button: PointerEventButton::Left,
    });
}

/// A click at the middle of `element`.
fn press(ui: &SettingsUi, element: &ElementHandle) {
    let (at, size) = (element.absolute_position(), element.size());
    click(ui, LogicalPosition::new(at.x + size.width / 2.0, at.y + size.height / 2.0));
}

fn key(ui: &SettingsUi, key: xem_ui::Key) {
    ui.dispatch(WindowEvent::KeyPressed { text: key.into() });
    ui.dispatch(WindowEvent::KeyReleased { text: key.into() });
}

fn element(ui: &SettingsUi, label: &str, role: AccessibleRole) -> ElementHandle {
    ElementHandle::find_by_accessible_label(ui.component(), label)
        .find(|element| element.accessible_role() == Some(role))
        .unwrap_or_else(|| panic!("no {role:?} labelled {label:?}"))
}

fn scroll(ui: &mut SettingsUi, delta_y: f32) {
    ui.dispatch(WindowEvent::PointerScrolled {
        position: LogicalPosition::new(220.0, 250.0),
        delta_x: 0.0,
        delta_y,
    });
    ui.update().unwrap();
}

fn section(ui: &mut SettingsUi, label: &str, index: i32) {
    let tab = element(ui, label, AccessibleRole::Tab);
    let at = tab.absolute_position();
    let size = tab.size();
    click(
        ui,
        LogicalPosition::new(at.x + size.width / 2.0, at.y + size.height / 2.0),
    );
    ui.update().unwrap();
    assert_eq!(ui.component().get_selected_section(), index);
    assert!(
        !ui.text_editor_focused(),
        "switching sections releases text focus"
    );
}

#[test]
fn panel_renders_on_the_shared_device_and_applies_settings() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");

    // The scene and the panel use one device and queue.
    let scene_target = gpu.device.create_texture(&wgpu::TextureDescriptor {
        label: None,
        size: wgpu::Extent3d {
            width: 64,
            height: 64,
            depth_or_array_layers: 1,
        },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format: wgpu::TextureFormat::Rgba8UnormSrgb,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
        view_formats: &[],
    });
    let mut scene = SceneRenderer::new(&gpu.device, &gpu.queue, scene_target.format());
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    let (view, proj) = demo_camera(1.0, 0.0);
    let target = scene_target.create_view(&Default::default());
    scene.render_views(
        &mut encoder,
        &[ViewTarget {
            view,
            proj,
            target: &target,
            viewport: Viewport::full(64, 64),
        }],
        0.0,
    );
    gpu.queue.submit([encoder.finish()]);

    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(
        MemoryStore::default(),
    )));
    let acks = Rc::new(RefCell::new(Vec::new()));
    settings.borrow_mut().subscribe({
        let acks = acks.clone();
        move |ack| acks.borrow_mut().push(ack.change.clone())
    });
    let shared = SharedGpu {
        instance: gpu.instance.clone(),
        device: gpu.device.clone(),
        queue: gpu.queue.clone(),
    };
    let mut ui = SettingsUi::new(shared, settings.clone(), (640, 560), 1.0).unwrap();

    // (b) The panel renders into a texture and carries Slint's attribution.
    assert!(ui.update().unwrap(), "the first update renders");
    assert!(!ui.update().unwrap(), "nothing changed, nothing rendered");
    let pixels = read_rgba8(&gpu.device, &gpu.queue, ui.texture());
    let opaque = pixels.chunks_exact(4).filter(|p| p[3] > 200).count();
    let distinct: std::collections::HashSet<_> = pixels.chunks_exact(4).collect();
    assert!(
        opaque > 640 * 560 / 2,
        "the panel background is drawn ({opaque} opaque pixels)"
    );
    assert!(
        distinct.len() > 20,
        "the panel has content ({} colours)",
        distinct.len()
    );
    section(&mut ui, "About", 7);
    assert!(
        ElementHandle::find_by_accessible_label(ui.component(), "#MadeWithSlint")
            .next()
            .is_some(),
        "the AboutSlint attribution is in the panel"
    );
    assert!(
        ElementHandle::find_by_element_type_name(ui.component(), "AboutSlint")
            .next()
            .is_some(),
        "AboutSlint element present"
    );

    // (c) Pointer input on the panel goes through the settings service.
    section(&mut ui, "Graphics", 1);
    let checkbox = element(&ui, "Show FPS", AccessibleRole::Switch);
    let at = checkbox.absolute_position();
    let size = checkbox.size();
    click(
        &ui,
        LogicalPosition::new(at.x + 8.0, at.y + size.height / 2.0),
    );
    assert!(settings.borrow().settings().presentation.show_fps);
    assert_eq!(
        acks.borrow().as_slice(),
        [xem_settings::SettingChange::ShowFps(true)]
    );
    assert!(
        ui.update().unwrap(),
        "the acknowledged change redraws the panel"
    );

    // Changes from elsewhere appear in the current section.
    settings
        .borrow_mut()
        .apply(xem_settings::SettingChange::ShowFps(false))
        .unwrap();
    assert_eq!(checkbox.accessible_checked(), Some(false));
    // A toggle's whole row toggles it, its label too.
    assert!(ElementHandle::find_by_accessible_label(ui.component(), "A frame counter in the corner.").next().is_none());
    let label = element(&ui, "Show FPS", AccessibleRole::Switch);
    let at = label.absolute_position();
    click(&ui, LogicalPosition::new(at.x + 4.0, at.y + label.size().height / 2.0));
    assert!(settings.borrow().settings().presentation.show_fps, "clicking the row toggles it");
    settings
        .borrow_mut()
        .apply(xem_settings::SettingChange::ShowFps(false))
        .unwrap();
    section(&mut ui, "Audio", 0);
    let slider = element(&ui, "Master volume", AccessibleRole::Slider);
    let at = slider.absolute_position();
    let size = slider.size();
    click(
        &ui,
        LogicalPosition::new(at.x + size.width * 0.25, at.y + size.height / 2.0),
    );
    let volume = settings.borrow().settings().master_volume;
    assert!(
        (15..=35).contains(&volume),
        "volume follows the click: {volume}"
    );
    assert_eq!(
        slider
            .accessible_value()
            .unwrap()
            .parse::<f32>()
            .unwrap()
            .round() as u8,
        volume
    );

    // Pointer edits select discrete rates, with the thumb staying exactly on
    // the selected notch even while the pointer is between notches.
    section(&mut ui, "Time", 4);
    ui.component()
        .set_time_status("29 of 30 seconds available · 184.2 MiB used".into());
    ui.update().unwrap();
    assert!(
        ElementHandle::find_by_accessible_label(
            ui.component(),
            "29 of 30 seconds available · 184.2 MiB used"
        )
        .next()
        .is_some()
    );
    assert!(
        ElementHandle::find_by_accessible_label(ui.component(), "Fast-forward")
            .all(|e| e.accessible_role() != Some(AccessibleRole::Checkbox)),
        "FF has no checkbox"
    );
    assert_eq!(ui.component().get_rewind_interval_seconds(), 1);
    ui.component().invoke_rewind_interval_edited(3);
    assert_eq!(settings.borrow().settings().rewind_interval_seconds, 3);
    assert_eq!(ui.component().get_rewind_interval_seconds(), 3);
    assert_eq!(ui.component().get_rewind_history_seconds(), 30);
    ui.component().invoke_rewind_history_edited(10);
    assert_eq!(settings.borrow().settings().rewind_history_seconds, 10);
    assert_eq!(ui.component().get_rewind_history_seconds(), 10);
    for label in [
        "Rewind interval in seconds",
        "Rewind history length in seconds",
    ] {
        let field = ElementHandle::find_by_accessible_label(ui.component(), label)
            .next()
            .unwrap();
        assert!(field.size().width <= 105.0, "{label} is compact");
    }
    scroll(&mut ui, -200.0);
    let rate_slider = element(&ui, "Fast-forward rate", AccessibleRole::Slider);
    assert_eq!(rate_slider.accessible_value().as_deref(), Some("2"));
    let at = rate_slider.absolute_position();
    let size = rate_slider.size();
    let center = LogicalPosition::new(
        at.x + 10.0 + (size.width - 20.0) * 0.4,
        at.y + size.height / 2.0,
    );
    ui.dispatch(WindowEvent::PointerMoved { position: center });
    ui.dispatch(WindowEvent::PointerPressed {
        position: center,
        button: PointerEventButton::Left,
    });
    let between = LogicalPosition::new(center.x + (size.width - 20.0) * 0.06, center.y);
    ui.dispatch(WindowEvent::PointerMoved { position: between });
    assert_eq!(
        rate_slider.accessible_value().as_deref(),
        Some("2"),
        "thumb snaps back to the 10× notch during the drag"
    );
    assert_eq!(
        settings.borrow().settings().fast_forward_rate,
        xem_settings::FastForwardRate::Fixed(10)
    );
    let next = LogicalPosition::new(center.x + (size.width - 20.0) * 0.15, center.y);
    ui.dispatch(WindowEvent::PointerMoved { position: next });
    assert_eq!(rate_slider.accessible_value().as_deref(), Some("3"));
    assert_eq!(
        settings.borrow().settings().fast_forward_rate,
        xem_settings::FastForwardRate::Fixed(15)
    );
    ui.dispatch(WindowEvent::PointerReleased {
        position: next,
        button: PointerEventButton::Left,
    });
    ui.dispatch(WindowEvent::KeyPressed {
        text: xem_ui::Key::End.into(),
    });
    ui.dispatch(WindowEvent::KeyReleased {
        text: xem_ui::Key::End.into(),
    });
    assert_eq!(rate_slider.accessible_value().as_deref(), Some("5"));
    assert_eq!(
        settings.borrow().settings().fast_forward_rate,
        xem_settings::FastForwardRate::Uncapped
    );
    section(&mut ui, "Controls", 2);
    let inversion = element(&ui, "Invert LT/RT chords", AccessibleRole::Switch);
    let at = inversion.absolute_position();
    let size = inversion.size();
    click(
        &ui,
        LogicalPosition::new(at.x + 8.0, at.y + size.height / 2.0),
    );
    assert!(settings.borrow().settings().invert_trigger_chords);
    assert_eq!(inversion.accessible_checked(), Some(true));

    let requests = Rc::new(RefCell::new(Vec::new()));
    ui.on_app_request({
        let requests = requests.clone();
        move |request| requests.borrow_mut().push(request)
    });
    section(&mut ui, "Time", 4);
    scroll(&mut ui,-2000.0);
    let focus_pause = element(&ui,"Pause when the window loses focus",AccessibleRole::Switch);
    assert_eq!(focus_pause.accessible_checked(),Some(true));
    let at = focus_pause.absolute_position();
    let size = focus_pause.size();
    click(&ui,LogicalPosition::new(at.x+8.0,at.y+size.height/2.0));
    assert!(!settings.borrow().settings().pause_on_focus_loss);
    assert_eq!(focus_pause.accessible_checked(),Some(false));
    section(&mut ui, "Controls", 2);
    section(&mut ui, "Time", 4);
    requests.borrow_mut().clear();
    let hold = element(&ui, "Hold to rewind", AccessibleRole::Button);
    let at = hold.absolute_position();
    let size = hold.size();
    let position = LogicalPosition::new(at.x + size.width / 2.0, at.y + size.height / 2.0);
    ui.dispatch(WindowEvent::PointerMoved { position });
    ui.dispatch(WindowEvent::PointerPressed {
        position,
        button: PointerEventButton::Left,
    });
    assert_eq!(
        requests.borrow().as_slice(),
        [xem_settings::AppRequest::UiRewind(true)]
    );
    ui.update().unwrap();
    ui.cancel_pointer();
    assert_eq!(
        requests.borrow().as_slice(),
        [
            xem_settings::AppRequest::UiRewind(true),
            xem_settings::AppRequest::UiRewind(false)
        ]
    );
    ui.update().unwrap();
    assert_eq!(
        requests.borrow().len(),
        2,
        "a later widget update cannot repeat the release"
    );
    section(&mut ui, "Shortcuts", 3);
    press(&ui, &element(&ui, "Save state", AccessibleRole::ListItem));
    ui.update().unwrap();
    let editor = element(&ui, "Keyboard binding", AccessibleRole::TextInput);
    let at = editor.absolute_position();
    let size = editor.size();
    click(
        &ui,
        LogicalPosition::new(at.x + size.width / 2.0, at.y + size.height / 2.0),
    );
    assert!(
        ui.text_editor_focused(),
        "the host can distinguish binding entry from app shortcuts"
    );

    // A narrow window shows the sections as a strip of tabs above the same form.
    section(&mut ui, "Time", 4);
    ui.resize((360, 440), 1.0);
    ui.update().unwrap();
    section(&mut ui, "Audio", 0);
    let strip = element(&ui, "Audio", AccessibleRole::Tab);
    assert!(strip.size().height <= 56.0, "the strip keeps its height: {:?}", strip.size());
    ui.component().set_selected_section(4);
    ui.update().unwrap();
    for label in [
        "Rewind interval in seconds",
        "Rewind history length in seconds",
    ] {
        let field = ElementHandle::find_by_accessible_label(ui.component(), label)
            .next()
            .unwrap();
        let at = field.absolute_position();
        let size = field.size();
        assert!(
            size.width <= 105.0 && at.x + size.width < 332.0,
            "{label} leaves room for the scrollbar"
        );
    }
    ui.resize((360, 300), 1.0);
    ui.update().unwrap();
    let rate_below = |ui: &SettingsUi| {
        ElementHandle::find_by_accessible_label(ui.component(), "Fast-forward rate")
            .filter(|e| e.accessible_role() == Some(AccessibleRole::Slider))
            .all(|e| e.absolute_position().y >= 300.0)
    };
    assert!(rate_below(&ui), "the speed slider starts below the compact viewport");
    let row = element(&ui, "Rewind history", AccessibleRole::Switch);
    let at = row.absolute_position();
    let size = row.size();
    let from = LogicalPosition::new(at.x + size.width / 2.0, at.y + size.height / 2.0);
    let to = LogicalPosition::new(from.x, 20.0);
    ui.dispatch(WindowEvent::PointerMoved { position: from });
    ui.dispatch(WindowEvent::PointerPressed {
        position: from,
        button: PointerEventButton::Left,
    });
    ui.dispatch(WindowEvent::PointerMoved {
        position: LogicalPosition::new(from.x, from.y - 15.0),
    });
    ui.dispatch(WindowEvent::PointerMoved { position: to });
    ui.dispatch(WindowEvent::PointerReleased {
        position: to,
        button: PointerEventButton::Left,
    });
    ui.update().unwrap();
    assert!(!rate_below(&ui), "dragging the compact form reveals the speed slider");

    // Touch events on the Android host arrive as logical pointer events.
    ui.resize((1064, 1470), 2.625);
    ui.component().set_selected_section(0);
    ui.update().unwrap();
    section(&mut ui, "Saves", 5);
}

#[test]
fn binaural_is_a_fourth_sound_mode_with_its_options() {
    use xem_settings::{AudioOutput, SettingChange};
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(MemoryStore::default())));
    let acks = Rc::new(RefCell::new(Vec::new()));
    settings.borrow_mut().subscribe({
        let acks = acks.clone();
        move |ack| acks.borrow_mut().push(ack.change.clone())
    });
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut ui = SettingsUi::new(shared, settings.clone(), (640, 560), 1.0).unwrap();
    ui.update().unwrap();
    assert!(ElementHandle::find_by_accessible_label(ui.component(), "Music immersion").next().is_none(), "only under Binaural");
    ui.component().invoke_audio_output_edited(2);
    ui.component().invoke_audio_output_edited(3);
    ui.update().unwrap();
    {
        let current = settings.borrow();
        assert!(current.settings().binaural.enabled);
        assert_eq!(current.settings().audio_output, AudioOutput::Wide, "Wide is what disabling gives back");
        assert_eq!(current.settings().game_sound_mode(), 1, "the game plays in Stereo");
    }
    assert_eq!(ui.component().get_audio_output(), 3);
    element(&ui, "Music immersion", AccessibleRole::Slider);
    ui.component().invoke_binaural_edited("immersion".into(), 40);
    ui.component().invoke_binaural_edited("effects".into(), 0);
    ui.component().invoke_binaural_edited("head".into(), 1);
    element(&ui, "VR: Stationary music", AccessibleRole::Switch);
    assert!(ElementHandle::find_by_accessible_label(ui.component(), "Compare with Stereo").next().is_none());
    let binaural = settings.borrow().settings().binaural;
    assert_eq!((binaural.music_immersion, binaural.positional_effects, binaural.head_tracked_music, binaural.spatial_music), (40, false, true, true));
    ui.component().invoke_audio_output_edited(1);
    ui.update().unwrap();
    assert!(!settings.borrow().settings().binaural.enabled, "an original mode ends Binaural");
    assert_eq!(settings.borrow().settings().binaural.music_immersion, 40, "its options are kept");
    assert!(acks.borrow().iter().all(|change| matches!(change, SettingChange::Binaural(_) | SettingChange::AudioOutput(_))));
}

#[test]
fn graphics_choices_go_through_the_settings_service() {
    use xem_settings::{GraphicsProfile, SettingChange};
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(MemoryStore::default())));
    let acks = Rc::new(RefCell::new(Vec::new()));
    settings.borrow_mut().subscribe({
        let acks = acks.clone();
        move |ack| acks.borrow_mut().push(ack.change.clone())
    });
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut ui = SettingsUi::new(shared, settings.clone(), (640, 560), 1.0).unwrap();
    ui.update().unwrap();
    section(&mut ui, "Graphics", 1);
    assert!(
        ElementHandle::find_by_accessible_label(ui.component(), "Bloom").next().is_none(),
        "Modern (the default) shows no effect sliders"
    );
    let panel = ui.component();
    panel.invoke_graphics_edited("profile".into(), 1);
    ui.update().unwrap();
    assert_eq!(settings.borrow().settings().graphics.profile, GraphicsProfile::Hd2d);
    // The effect rows are created as the section scrolls to them.
    ui.update().unwrap();
    for _ in 0..8 {
        if ElementHandle::find_by_accessible_label(ui.component(), "Bloom").any(|e| e.accessible_role() == Some(AccessibleRole::Slider)) {
            break;
        }
        scroll(&mut ui, -200.0);
    }
    element(&ui, "Bloom", AccessibleRole::Slider);
    ui.component().invoke_effect_edited(5, 20);
    assert_eq!(settings.borrow().settings().graphics.hd2d.bloom, 20);
    ui.component().invoke_graphics_edited("profile".into(), 3);
    ui.component().invoke_fidelity_edited(3, true);
    ui.component().invoke_effect_edited(8, -40);
    let graphics = settings.borrow().settings().graphics;
    assert!(graphics.custom_fidelity.dithering, "Custom's own limits change");
    assert_eq!(graphics.custom_effects.focus, -40, "Custom's own levels change");
    assert_eq!(graphics.hd2d.bloom, 20, "HD-2D's levels are kept");
    ui.component().invoke_graphics_edited("scale".into(), 99);
    assert_eq!(settings.borrow().settings().graphics.internal_scale, 3 - 3, "an invalid scale is refused");
    ui.component().invoke_graphics_edited("widescreen".into(), 1);
    assert!(settings.borrow().settings().graphics.widescreen);
    assert!(acks.borrow().iter().all(|change| matches!(change, SettingChange::Graphics(_))));
}

#[test]
fn quit_asks_first_and_fullscreen_is_a_setting_where_hosts_offer_them() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(MemoryStore::default())));
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut ui = SettingsUi::new(shared, settings.clone(), (640, 560), 1.0).unwrap();
    ui.update().unwrap();
    section(&mut ui, "Graphics", 1);
    let shown = |ui: &SettingsUi, label: &str| ElementHandle::find_by_accessible_label(ui.component(), label).next().is_some();
    assert!(!shown(&ui, "Quit") && !shown(&ui, "Fullscreen"), "a host that offers neither (the browser) shows neither");

    let quits = Rc::new(Cell::new(0));
    ui.on_quit({
        let quits = quits.clone();
        move || quits.set(quits.get() + 1)
    });
    ui.set_fullscreen_offered(true);
    ui.update().unwrap();
    press(&ui, &element(&ui, "Fullscreen", AccessibleRole::Switch));
    assert!(settings.borrow().settings().presentation.fullscreen, "Fullscreen goes through the settings service");

    let header_quit = element(&ui, "Quit", AccessibleRole::Button);
    let open = |ui: &mut SettingsUi| {
        press(ui, &header_quit);
        ui.update().unwrap();
        assert!(ui.component().get_quit_confirming(), "Quit asks first");
    };
    // The confirmation's own Quit, below the header's.
    let confirm = |ui: &SettingsUi| {
        ElementHandle::find_by_accessible_label(ui.component(), "Quit")
            .find(|e| e.accessible_role() == Some(AccessibleRole::Button) && e.absolute_position().y > 100.0)
            .expect("the confirmation's Quit")
    };
    open(&mut ui);
    press(&ui, &element(&ui, "Cancel", AccessibleRole::Button));
    assert!(!ui.component().get_quit_confirming(), "Cancel closes it");
    open(&mut ui);
    click(&ui, LogicalPosition::new(24.0, 540.0));
    assert!(!ui.component().get_quit_confirming(), "so does a press beside it");
    open(&mut ui);
    assert!(ui.cancel_dialog(), "the host's Escape closes the open confirmation");
    assert!(!ui.cancel_dialog(), "and then has none to close (the host closes the panel)");
    // Keys stay in it: Tab moves between Cancel and Quit only, and Return
    // presses the one it is on.
    open(&mut ui);
    key(&ui, xem_ui::Key::Tab);
    key(&ui, xem_ui::Key::Return);
    assert!(!ui.component().get_quit_confirming(), "Tab first reaches Cancel");
    open(&mut ui);
    for _ in 0..3 {
        key(&ui, xem_ui::Key::Tab);
    }
    key(&ui, xem_ui::Key::Return);
    assert!(!ui.component().get_quit_confirming(), "the third Tab is back on Cancel");
    assert_eq!(quits.get(), 0, "nothing so far quits");
    open(&mut ui);
    key(&ui, xem_ui::Key::Tab);
    key(&ui, xem_ui::Key::Tab);
    key(&ui, xem_ui::Key::Return);
    assert_eq!(quits.get(), 1, "Return on its Quit quits");
    open(&mut ui);
    press(&ui, &confirm(&ui));
    assert_eq!(quits.get(), 2, "and so does a press on it");
    assert!(!ui.component().get_quit_confirming());
}

#[test]
fn random_battles_are_chosen_in_the_gameplay_section_through_the_settings_service() {
    use xem_settings::{RandomBattles, SettingChange};
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(MemoryStore::default())));
    let acks = Rc::new(RefCell::new(Vec::new()));
    settings.borrow_mut().subscribe({
        let acks = acks.clone();
        move |ack| acks.borrow_mut().push(ack.change.clone())
    });
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut ui = SettingsUi::new(shared, settings.clone(), (640, 560), 1.0).unwrap();
    ui.update().unwrap();
    section(&mut ui, "Gameplay", 6);
    let choice = element(&ui, "Random battles", AccessibleRole::Combobox);
    assert_eq!(choice.accessible_value().as_deref(), Some("Not while platforming"), "the default policy");
    let note = ui.component().get_random_battles_note().to_string();
    assert!(note.contains("Babel Tower") && note.contains("Blackmoon Forest"), "{note}");
    // A choice is the service's: applied, acknowledged and saved.
    press(&ui, &choice);
    key(&ui, xem_ui::Key::DownArrow);
    key(&ui, xem_ui::Key::Return);
    ui.update().unwrap();
    assert_eq!(settings.borrow().settings().random_battles, RandomBattles::Original);
    assert_eq!(acks.borrow().last(), Some(&SettingChange::RandomBattles(RandomBattles::Original)));
    assert_eq!(choice.accessible_value().as_deref(), Some("Original"));
    assert!(ui.component().get_random_battles_note().contains("Wherever the original game has them"));
    // An agent's change shows in the panel.
    settings.borrow_mut().apply(SettingChange::RandomBattles(RandomBattles::NotWhilePlatforming)).unwrap();
    ui.update().unwrap();
    assert_eq!(ui.component().get_random_battles(), 0);
    assert_eq!(choice.accessible_value().as_deref(), Some("Not while platforming"));
    let mut reloaded = SettingsService::with_defaults(MemoryStore::default());
    reloaded.import(&settings.borrow().export()).unwrap();
    assert_eq!(reloaded.settings().random_battles, RandomBattles::NotWhilePlatforming);
    // The rate of the ones left on, beside it.
    use xem_settings::EncounterRate;
    element(&ui, "Encounter rate", AccessibleRole::RadioGroup);
    assert_eq!(ui.component().get_encounter_rate(), 0, "the original's rate");
    ui.component().invoke_gameplay_edited("encounter-rate".into(), 1);
    ui.update().unwrap();
    assert_eq!(settings.borrow().settings().encounter_rate, EncounterRate::Half);
    assert_eq!(acks.borrow().last(), Some(&SettingChange::EncounterRate(EncounterRate::Half)));
    assert_eq!(ui.component().get_encounter_rate(), 1);
}

#[test]
fn text_speed_auto_advance_and_movie_skipping_are_chosen_in_the_gameplay_section_through_the_settings_service() {
    use xem_settings::{AutoAdvance, SettingChange, TextSpeed};
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(MemoryStore::default())));
    let acks = Rc::new(RefCell::new(Vec::new()));
    settings.borrow_mut().subscribe({
        let acks = acks.clone();
        move |ack| acks.borrow_mut().push(ack.change.clone())
    });
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut ui = SettingsUi::new(shared, settings.clone(), (640, 900), 1.0).unwrap();
    ui.update().unwrap();
    section(&mut ui, "Gameplay", 6);
    let shown = |ui: &SettingsUi, label: &str| ElementHandle::find_by_accessible_label(ui.component(), label).next().is_some();
    assert_eq!(ui.component().get_text_speed(), 0, "the original's speed");
    assert!(!shown(&ui, "Auto-advance wait on each page"), "the wait shows only with auto-advance on");
    // Every choice is the service's: applied, acknowledged and saved.
    ui.component().invoke_gameplay_edited("text-speed".into(), 2);
    let auto = element(&ui, "Auto-advance dialogue", AccessibleRole::Switch);
    press(&ui, &auto);
    ui.update().unwrap();
    assert!(shown(&ui, "Auto-advance wait on each page"));
    ui.component().invoke_gameplay_edited("auto-advance-dwell".into(), 90);
    press(&ui, &element(&ui, "Movies can be skipped", AccessibleRole::Switch));
    ui.update().unwrap();
    {
        let current = settings.borrow();
        let current = current.settings();
        assert_eq!((current.text_speed, current.auto_advance, current.movie_skip), (TextSpeed::Instant, AutoAdvance { enabled: true, dwell: 90 }, true));
    }
    assert_eq!(*acks.borrow(), [SettingChange::TextSpeed(TextSpeed::Instant), SettingChange::AutoAdvance(AutoAdvance { enabled: true, dwell: 120 }),
        SettingChange::AutoAdvance(AutoAdvance { enabled: true, dwell: 90 }), SettingChange::MovieSkip(true)]);
    // A refused wait leaves the shown one; an agent's change shows.
    ui.component().invoke_gameplay_edited("auto-advance-dwell".into(), 5);
    ui.update().unwrap();
    assert_eq!(ui.component().get_auto_advance_dwell(), 90);
    settings.borrow_mut().apply(SettingChange::AutoAdvance(AutoAdvance { enabled: false, dwell: 90 })).unwrap();
    settings.borrow_mut().apply(SettingChange::TextSpeed(TextSpeed::Fast)).unwrap();
    ui.update().unwrap();
    assert!(!ui.component().get_auto_advance() && !shown(&ui, "Auto-advance wait on each page"));
    assert_eq!(ui.component().get_text_speed(), 1);
}

#[test]
fn every_section_tab_lies_inside_a_narrow_panel_and_answers_a_click() {
    const SECTIONS: [&str; 8] = ["Audio", "Graphics", "Controls", "Shortcuts", "Time", "Saves", "Gameplay", "About"];
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(MemoryStore::default())));
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut ui = SettingsUi::new(shared, settings, (344, 560), 1.0).unwrap();
    assert_eq!(ui.component().get_section_count(), SECTIONS.len() as i32);
    // Hosts make the panel min(640 × scale, width - 16) physical pixels
    // wide: a phone browser 360 CSS pixels wide, a 1080-pixel phone at
    // densities 3.0 (360 dp) and 2.75, the Android smoke's emulator (2.625),
    // a browser 320 wide, and the panel's least width.
    for (size, scale) in [
        ((344, 560), 1.0), ((1064, 1470), 3.0), ((1064, 1470), 2.75),
        ((1064, 1470), 2.625), ((304, 560), 1.0), ((280, 560), 1.0),
    ] {
        ui.resize(size, scale);
        ui.update().unwrap();
        let width = size.0 as f32 / scale;
        for (index, label) in SECTIONS.into_iter().enumerate() {
            let tab = element(&ui, label, AccessibleRole::Tab);
            let (at, extent) = (tab.absolute_position(), tab.size());
            assert!(
                at.x >= 0.0 && at.x + extent.width <= width,
                "the {label} tab lies inside a panel {width} wide: {at:?} {extent:?}"
            );
            section(&mut ui, label, index as i32);
        }
    }
    // The Android smoke taps Graphics and Saves at 210,722 and 727,722 on
    // its 1080x2400 screen, where the panel (1064x1470 at 2.625) is at 8,465.
    ui.resize((1064, 1470), 2.625);
    ui.update().unwrap();
    for (x, label, index) in [(210.0, "Graphics", 1), (727.0, "Saves", 5)] {
        click(&ui, LogicalPosition::new((x - 8.0) / 2.625, (722.0 - 465.0) / 2.625));
        ui.update().unwrap();
        assert_eq!(ui.component().get_selected_section(), index, "the smoke's tap selects {label}");
    }
}

#[test]
fn a_missing_disc_is_asked_for_over_the_saves_section() {
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(MemoryStore::default())));
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let ui = SettingsUi::new(shared, settings, (640, 560), 1.0).unwrap();
    ui.ask_for_disc(2);
    assert_eq!(ui.component().get_selected_section(), 5, "the Saves section");
    assert!(ui.component().get_status().contains("beside the one the game started from"), "a host without disc import");
    ui.component().set_selected_section(0);
    ui.on_import_disc(|| {});
    ui.ask_for_disc(2);
    assert_eq!(ui.component().get_status_kind(), 2, "a problem");
    assert!(ui.component().get_status().contains("disc 2: import it below"));
    assert_eq!(ui.component().get_selected_section(), 5);
}

#[test]
fn movement_preference_reset_and_binding_cancel_use_the_shared_services() {
    use xem_settings::{AppAction, AppRequest, SettingChange};
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(MemoryStore::default())));
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut ui = SettingsUi::new(shared, settings.clone(), (640, 560), 1.0).unwrap();
    let requests = Rc::new(RefCell::new(Vec::new()));
    ui.on_app_request({ let requests = requests.clone(); move |request| requests.borrow_mut().push(request) });
    ui.on_quit(|| {});
    ui.set_first_person(true);
    ui.update().unwrap();
    let reset = element(&ui, "Reset", AccessibleRole::Button);
    let quit = element(&ui, "Quit", AccessibleRole::Button);
    assert!(reset.absolute_position().x < quit.absolute_position().x);
    press(&ui, &reset);
    assert_eq!(requests.borrow().as_slice(), [AppRequest::Action(AppAction::Reset)]);
    section(&mut ui, "Controls", 2);
    press(&ui, &element(&ui, "Run by default", AccessibleRole::Switch));
    assert!(settings.borrow().settings().run_by_default);
    assert!(ElementHandle::find_by_accessible_label(ui.component(), "Walk (held)").next().is_some());
    let exported = settings.borrow().export();
    let mut reloaded = SettingsService::with_defaults(MemoryStore::default());
    reloaded.import(&exported).unwrap();
    assert!(reloaded.settings().run_by_default);
    settings.borrow_mut().apply(SettingChange::RunByDefault(false)).unwrap();
    assert!(!ui.component().get_run_by_default());
    ui.resize((640, 2000), 1.0);
    ui.update().unwrap();
    assert!(ElementHandle::find_by_accessible_label(ui.component(), "Stop at ledges").next().is_some());
    assert!(ElementHandle::find_by_accessible_label(ui.component(), "Look around in FPV").next().is_some());
    assert!(ElementHandle::find_by_accessible_label(ui.component(), "Drag").next().is_none(), "no touch controls, no drag");
    ui.set_first_person_touch(true);
    ui.update().unwrap();
    assert!(ElementHandle::find_by_accessible_label(ui.component(), "Drag").next().is_some(), "a touch screen lists the drag among the looks");
    // A headset lists its own: the underhand grab and the controllers'
    // Back (their right stick's click) toggle, the head and the right stick
    // look, within its panel's 440 logical pixels.
    let listed = |ui: &SettingsUi, label: &str| ElementHandle::find_by_accessible_label(ui.component(), label).next().is_some();
    assert!(!listed(&ui, "Underhand grab") && !listed(&ui, "Head"));
    ui.set_first_person_headset(true);
    ui.resize((440, 2000), 1.0);
    ui.update().unwrap();
    assert!(listed(&ui, "Underhand grab") && listed(&ui, "Right stick click"), "a headset lists its toggles");
    assert!(listed(&ui, "Head"), "and the head among its looks");
    assert!(!listed(&ui, "R3") && !listed(&ui, "Arrows") && !listed(&ui, "Drag"), "and none of the flat hosts' inputs");
    for label in ["Underhand grab", "Right stick click", "Head"] {
        let key = element(&ui, label, AccessibleRole::Text);
        let (at, size) = (key.absolute_position(), key.size());
        assert!(at.x >= 0.0 && at.x + size.width <= 440.0, "{label} at {at:?} {size:?} fits the headset's panel");
    }
    ui.set_first_person_headset(false);
    ui.resize((640, 2000), 1.0);
    ui.update().unwrap();
    assert!(listed(&ui, "R3") && listed(&ui, "Arrows") && !listed(&ui, "Underhand grab"), "out of the headset, the flat hosts' again");
    let saved = settings.borrow().settings().clone();
    press(&ui, &element(&ui, "Up", AccessibleRole::ListItem));
    ui.update().unwrap();
    ui.component().set_game_keys("KeyQ".into());
    let cancel = element(&ui, "Cancel", AccessibleRole::Button);
    let clear = element(&ui, "Clear", AccessibleRole::Button);
    assert!(cancel.absolute_position().x < clear.absolute_position().x);
    press(&ui, &cancel);
    assert_eq!(ui.component().get_selected_game_action(), -1);
    assert_eq!(*settings.borrow().settings(), saved, "Cancel discards unsaved edits");
    section(&mut ui, "Shortcuts", 3);
    press(&ui, &element(&ui, "Save state", AccessibleRole::ListItem));
    ui.update().unwrap();
    ui.component().set_keyboard_binding("KeyQ".into());
    press(&ui, &element(&ui, "Cancel", AccessibleRole::Button));
    assert_eq!(ui.component().get_selected_action(), -1);
    assert_eq!(*settings.borrow().settings(), saved);
    section(&mut ui, "Graphics", 1);
    for removed in ["Cull back faces", "Cull at the screen's edges", "How the game is drawn and shown."] {
        assert!(ElementHandle::find_by_accessible_label(ui.component(), removed).next().is_none(), "{removed}");
    }
}

#[test]
fn settings_choices_are_relevant_and_wheel_does_not_change_dropdowns() {
    use xem_settings::{Scale, SettingChange};
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(MemoryStore::default())));
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut ui = SettingsUi::new(shared, settings.clone(), (640, 2000), 1.0).unwrap();
    ui.update().unwrap();
    section(&mut ui, "Graphics", 1);
    let shown = |ui: &SettingsUi, label: &str| ElementHandle::find_by_accessible_label(ui.component(), label).next().is_some();
    for (profile, resolution, quality) in [(0, true, false), (1, true, true), (2, false, false), (3, true, true)] {
        ui.component().invoke_graphics_edited("profile".into(), profile);
        ui.update().unwrap();
        assert_eq!(shown(&ui, "Internal resolution"), resolution, "resolution for profile {profile}");
        assert_eq!(shown(&ui, "HD-2D quality"), quality, "HD-2D quality for profile {profile}");
        assert!(!shown(&ui, "Characters keep moving"));
    }
    let heading = element(&ui, "VR", AccessibleRole::Text);
    let distance = element(&ui, "Headset screen distance in centimetres", AccessibleRole::Spinbox);
    assert!(heading.absolute_position().y < distance.absolute_position().y);

    // Every dropdown remains unchanged by the wheel, even after gaining focus.
    // The widget lets the containing panel or open menu scroll instead.
    for (section_name, section_index, label) in [
        ("Graphics", 1, "Scale"), ("Graphics", 1, "Graphics profile"),
        ("Graphics", 1, "Internal resolution"), ("Graphics", 1, "HD-2D quality"),
        ("Graphics", 1, "Presentation rate"), ("Controls", 2, "Headset pointer"),
        ("Gameplay", 6, "Random battles"),
    ] {
        section(&mut ui, section_name, section_index);
        let dropdown = element(&ui, label, AccessibleRole::Combobox);
        let before = settings.borrow().settings().clone();
        press(&ui, &dropdown);
        ui.update().unwrap();
        assert_eq!(dropdown.accessible_expanded(), Some(true));
        let selected = dropdown.accessible_value().unwrap();
        let menu_item = element(&ui, &selected, AccessibleRole::ListItem);
        menu_item.scroll(0.0, -120.0);
        menu_item.scroll(0.0, 120.0);
        assert_eq!(*settings.borrow().settings(), before, "wheel in {label} menu");
        key(&ui, xem_ui::Key::Escape);
        ui.update().unwrap();
        assert_eq!(dropdown.accessible_expanded(), Some(false));
        let at = dropdown.absolute_position();
        let position = LogicalPosition::new(at.x + dropdown.size().width / 2.0, at.y + dropdown.size().height / 2.0);
        for delta_y in [-120.0, 120.0] {
            ui.dispatch(WindowEvent::PointerScrolled { position, delta_x: 0.0, delta_y });
            ui.update().unwrap();
        }
        assert_eq!(*settings.borrow().settings(), before, "wheel over {label}");
    }

    // Mouse selection and arrow keys still go through the settings service.
    section(&mut ui, "Graphics", 1);
    let dropdown = element(&ui, "Scale", AccessibleRole::Combobox);
    press(&ui, &dropdown);
    key(&ui, xem_ui::Key::DownArrow);
    key(&ui, xem_ui::Key::Return);
    ui.update().unwrap();
    assert_eq!(settings.borrow().settings().presentation.scale, Scale::Integer);
    press(&ui, &dropdown);
    ui.update().unwrap();
    // Use real window events: the mock click helper advances a fake clock,
    // whereas this shared-GPU platform uses the host's monotonic clock.
    let at = dropdown.absolute_position();
    click(&ui, LogicalPosition::new(at.x + 40.0, at.y + dropdown.size().height + 4.0 + 4.0 + 2.0 * 32.0 + 16.0));
    ui.update().unwrap();
    assert_eq!(settings.borrow().settings().presentation.scale, Scale::Stretch);
    for key_name in [xem_ui::Key::Home, xem_ui::Key::End] {
        key(&ui, key_name);
        assert_eq!(settings.borrow().settings().presentation.scale, Scale::Stretch, "Home/End do not edit dropdowns");
    }
    key(&ui, xem_ui::Key::Tab);
    key(&ui, xem_ui::Key::UpArrow);
    assert_eq!(settings.borrow().settings().presentation.scale, Scale::Stretch, "Tab leaves the closed dropdown");

    section(&mut ui, "Controls", 2);
    assert!(!shown(&ui, "Mouse sensitivity"));
    press(&ui, &element(&ui, "Turn the camera with the mouse", AccessibleRole::Switch));
    ui.update().unwrap();
    let sensitivity = element(&ui, "Mouse sensitivity", AccessibleRole::Slider);
    assert_eq!(sensitivity.accessible_value().as_deref(), Some("50"));
    press(&ui, &sensitivity);
    assert_eq!(settings.borrow().settings().mouse_sensitivity, 105);
    settings.borrow_mut().apply(SettingChange::MouseSensitivity(75)).unwrap();
    assert_eq!(ui.component().get_mouse_sensitivity(), 75);
    // The deadzone beside it, in pixels of motion.
    let deadzone = element(&ui, "Mouse deadzone", AccessibleRole::Slider);
    assert_eq!(deadzone.accessible_value().as_deref(), Some("40"));
    press(&ui, &deadzone);
    assert_eq!(settings.borrow().settings().mouse_deadzone, 50);
    settings.borrow_mut().apply(SettingChange::MouseDeadzone(3)).unwrap();
    assert_eq!(ui.component().get_mouse_deadzone(), 3);
    press(&ui, &element(&ui, "Turn the camera with the mouse", AccessibleRole::Switch));
    ui.update().unwrap();
    assert!(!shown(&ui, "Mouse sensitivity"));
    assert!(!shown(&ui, "Mouse deadzone"));
    assert_eq!((settings.borrow().settings().mouse_sensitivity, settings.borrow().settings().mouse_deadzone), (75, 3));

    ui.resize((640, 560), 1.0);
    section(&mut ui, "Graphics", 1);
    let dropdown = element(&ui, "Scale", AccessibleRole::Combobox);
    press(&ui, &dropdown);
    key(&ui, xem_ui::Key::Escape);
    ui.update().unwrap();
    let before_y = dropdown.absolute_position().y;
    let before = settings.borrow().settings().clone();
    dropdown.scroll(0.0, -120.0);
    ui.update().unwrap();
    assert!(dropdown.absolute_position().y < before_y, "wheel over a focused dropdown scrolls the containing panel");
    assert_eq!(*settings.borrow().settings(), before);
}
