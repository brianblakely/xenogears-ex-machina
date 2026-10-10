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
    let mut ui = SettingsUi::new(shared, settings.clone(), (440, 560), 1.0).unwrap();

    // (b) The panel renders into a texture and carries Slint's attribution.
    assert!(ui.update().unwrap(), "the first update renders");
    assert!(!ui.update().unwrap(), "nothing changed, nothing rendered");
    let pixels = read_rgba8(&gpu.device, &gpu.queue, ui.texture());
    let opaque = pixels.chunks_exact(4).filter(|p| p[3] > 200).count();
    let distinct: std::collections::HashSet<_> = pixels.chunks_exact(4).collect();
    assert!(
        opaque > 440 * 560 / 2,
        "the panel background is drawn ({opaque} opaque pixels)"
    );
    assert!(
        distinct.len() > 20,
        "the panel has content ({} colours)",
        distinct.len()
    );
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

    // Changes from elsewhere (an agent) show up in the panel.
    settings
        .borrow_mut()
        .apply(xem_settings::SettingChange::ShowFps(false))
        .unwrap();
    assert_eq!(checkbox.accessible_checked(), Some(false));
}
