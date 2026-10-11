//! The settings panel through the custom platform, on the same device as the
//! scene. Run on lavapipe: `VK_ICD_FILENAMES=$XEM_LAVAPIPE_ICD`.

use i_slint_backend_testing::{AccessibleRole, ElementHandle};
use std::cell::RefCell;
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
    section(&mut ui, "Saves", 4);
    scroll(&mut ui, -2000.0);
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
    scroll(&mut ui, 2000.0);

    // (c) Pointer input on the panel goes through the settings service.
    section(&mut ui, "Graphics", 1);
    let checkbox = element(&ui, "Show FPS", AccessibleRole::Checkbox);
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
    section(&mut ui, "Time", 3);
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
    let inversion = element(&ui, "Invert LT/RT chords", AccessibleRole::Checkbox);
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
    section(&mut ui, "Time", 3);
    scroll(&mut ui,-2000.0);
    let focus_pause = element(&ui,"Pause when window loses focus",AccessibleRole::Checkbox);
    assert_eq!(focus_pause.accessible_checked(),Some(true));
    let at = focus_pause.absolute_position();
    let size = focus_pause.size();
    click(&ui,LogicalPosition::new(at.x+8.0,at.y+size.height/2.0));
    assert!(!settings.borrow().settings().pause_on_focus_loss);
    assert_eq!(focus_pause.accessible_checked(),Some(false));
    section(&mut ui, "Controls", 2);
    section(&mut ui, "Time", 3);
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
    section(&mut ui, "Controls", 2);
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

    // A narrow window uses the same form behind a section picker.
    section(&mut ui, "Time", 3);
    ui.resize((360, 440), 1.0);
    ui.update().unwrap();
    assert!(
        ElementHandle::find_by_accessible_label(ui.component(), "Time")
            .all(|e| e.accessible_role() != Some(AccessibleRole::Tab))
    );
    let picker = element(&ui, "Settings section", AccessibleRole::Combobox);
    let at = picker.absolute_position();
    let size = picker.size();
    click(
        &ui,
        LogicalPosition::new(at.x + size.width / 2.0, at.y + size.height / 2.0),
    );
    for _ in 0..3 {
        ui.dispatch(WindowEvent::KeyPressed {
            text: xem_ui::Key::UpArrow.into(),
        });
        ui.dispatch(WindowEvent::KeyReleased {
            text: xem_ui::Key::UpArrow.into(),
        });
    }
    ui.dispatch(WindowEvent::KeyPressed {
        text: xem_ui::Key::Return.into(),
    });
    ui.dispatch(WindowEvent::KeyReleased {
        text: xem_ui::Key::Return.into(),
    });
    ui.update().unwrap();
    assert_eq!(ui.component().get_selected_section(), 0);
    ui.component().set_selected_section(3);
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
    assert!(
        ElementHandle::find_by_accessible_label(ui.component(), "Fast-forward rate")
            .all(|e| e.accessible_role() != Some(AccessibleRole::Slider)),
        "the speed slider starts below the compact viewport"
    );
    let help = ElementHandle::find_by_accessible_label(
        ui.component(),
        "Snapshots use elapsed play time, including during fast-forward.",
    )
    .next()
    .unwrap();
    let at = help.absolute_position();
    let size = help.size();
    let from = LogicalPosition::new(at.x + size.width / 2.0, at.y + size.height / 2.0);
    let to = LogicalPosition::new(from.x, from.y - 100.0);
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
    let rate = element(&ui, "Fast-forward rate", AccessibleRole::Slider);
    assert!(
        rate.absolute_position().y < 400.0,
        "dragging the compact form reveals the speed slider"
    );

    // Touch events on the Android host arrive as logical pointer events.
    ui.resize((1064, 1470), 2.625);
    ui.component().set_selected_section(0);
    ui.update().unwrap();
    let picker = element(&ui, "Settings section", AccessibleRole::Combobox);
    let at = picker.absolute_position();
    let size = picker.size();
    click(
        &ui,
        LogicalPosition::new(at.x + size.width / 2.0, at.y + size.height / 2.0),
    );
    ui.update().unwrap();
    click(
        &ui,
        LogicalPosition::new(at.x + size.width / 2.0, at.y + 4.0 + 4.0 * 40.0 + 20.0),
    );
    ui.update().unwrap();
    assert_eq!(
        ui.component().get_selected_section(),
        4,
        "the compact section menu supports pointer selection at Android density"
    );
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
    scroll(&mut ui, -400.0);
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
