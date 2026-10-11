//! Seated virtual-screen play in an OpenXR headset (`--xr`).
//!
//! The game runs as in the window: paced by the wall clock at 60 frames a
//! second (`xem_host::Game`), never per eye and never by the headset's
//! frame rate. Each headset frame (`xrWaitFrame` paces this loop) presents
//! the newest game frame once through the renderer (`GameView`, the
//! Headset class: low tier, no depth of field unless asked), then draws it
//! for both eyes as a flat screen placed in front of the seated player with
//! each eye's own view and projection: monoscopic content shown as a
//! screen, not reconstructed 3D. The settings panel is a quad layer beside
//! it (left menu button); the right controller points at it.
//!
//! The controllers stand in for a pad: left stick the D-pad, A Cross, B
//! Circle, X Square, Y Triangle, triggers L1/R1, grips L2/R2, left stick
//! click Start, right stick click Select. (Phase 6 owns the full headset
//! controls.)
//!
//! Every frame prints one `key=value` line for scripts/xr-game-smoke.sh.

use crate::Options;
use std::cell::RefCell;
use std::rc::Rc;
use std::time::Instant;
use xem_host::Game;
use xem_render::glam::{Mat4, Quat, Vec2, Vec3};
use xem_render::ps1::{GameView, HostClass, ViewConfig};
use xem_render::{Compositor, Quad, wgpu};
use xem_settings::{Filter, SettingsService};
use xem_ui::{LogicalPosition, PointerEventButton, SettingsUi, SharedGpu, WindowEvent};
use xem_xr::{HandInput, InputState, PanelPlacement, Pose, SessionConfig, SessionEvent, XrContext, XrSession};

const PANEL_LOGICAL: (f32, f32) = (440.0, 560.0);
const PANEL_SCALE: f32 = 2.0;
const PANEL_WIDTH: f32 = 0.55;
/// The room around the screen.
const BACKDROP: wgpu::Color = wgpu::Color { r: 0.012, g: 0.014, b: 0.02, a: 1.0 };

/// The PS1 pad halfword from the controllers (bit 0 Select .. bit 15 Square).
pub fn pad_buttons(input: &InputState) -> u16 {
    let [left, right]: &[HandInput; 2] = &input.hands;
    let mut buttons = 0u16;
    let mut set = |on: bool, bit: u16| {
        if on {
            buttons |= 1 << bit;
        }
    };
    let [x, y] = left.thumbstick;
    set(y > 0.5, 4);
    set(x > 0.5, 5);
    set(y < -0.5, 6);
    set(x < -0.5, 7);
    set(right.primary, 14);
    set(right.secondary, 13);
    set(left.primary, 15);
    set(left.secondary, 12);
    set(left.trigger > 0.5, 10);
    set(right.trigger > 0.5, 11);
    set(left.squeeze, 8);
    set(right.squeeze, 9);
    set(left.thumbstick_click, 3);
    set(right.thumbstick_click, 0);
    buttons
}

/// Points at the panel with the right controller's aim ray.
#[derive(Default)]
struct PanelPointer {
    pressed: bool,
}

impl PanelPointer {
    fn update(&mut self, ui: &SettingsUi, panel: &PanelPlacement, panel_px: (u32, u32), hand: &HandInput) -> bool {
        let Some(aim) = hand.aim else { return false };
        let local_from_panel = panel.pose.matrix().inverse();
        let origin = local_from_panel.transform_point3(aim.position);
        let direction = local_from_panel.transform_vector3(aim.orientation * Vec3::NEG_Z);
        if direction.z.abs() < 1e-6 {
            return false;
        }
        let t = -origin.z / direction.z;
        let point = origin + direction * t;
        let (u, v) = (point.x / panel.size.0 + 0.5, 0.5 - point.y / panel.size.1);
        if t <= 0.0 || !(0.0..1.0).contains(&u) || !(0.0..1.0).contains(&v) {
            return false;
        }
        let scale = ui.scale_factor();
        let position = LogicalPosition::new(u * panel_px.0 as f32 / scale, v * panel_px.1 as f32 / scale);
        ui.dispatch(WindowEvent::PointerMoved { position });
        if hand.select != self.pressed {
            self.pressed = hand.select;
            let button = PointerEventButton::Left;
            ui.dispatch(if hand.select {
                WindowEvent::PointerPressed { position, button }
            } else {
                WindowEvent::PointerReleased { position, button }
            });
        }
        true
    }
}

/// The device's own picture (24-bit movie frames) as an sRGB texture.
#[derive(Default)]
struct MovieFrame {
    texture: Option<wgpu::Texture>,
}

impl MovieFrame {
    fn set(&mut self, gpu: &xem_render::Gpu, (width, height): (u32, u32), rgba: &[u8]) {
        if width == 0 || height == 0 {
            return;
        }
        if self.texture.as_ref().is_none_or(|t| (t.width(), t.height()) != (width, height)) {
            self.texture = Some(gpu.device.create_texture(&wgpu::TextureDescriptor {
                label: Some("movie frame"),
                size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format: wgpu::TextureFormat::Rgba8UnormSrgb,
                usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
                view_formats: &[],
            }));
        }
        let texture = self.texture.as_ref().expect("created above");
        gpu.queue.write_texture(
            texture.as_image_copy(),
            rgba,
            wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(width * 4), rows_per_image: Some(height) },
            wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
        );
    }
}

/// The game screen's pose: `distance` metres in front of the LOCAL origin
/// (the head at entry), at eye height.
fn screen_pose(distance: f32) -> Pose {
    Pose { position: Vec3::new(0.0, 0.0, -distance), orientation: Quat::IDENTITY }
}

pub fn run(options: &Options, settings: Rc<RefCell<SettingsService>>, mut game: Game) -> Result<(), String> {
    let context = XrContext::new(&()).map_err(|e| e.to_string())?;
    let caps = context.capabilities();
    println!("xr runtime={:?} system={:?} eye_size={}x{} blend={:?}", caps.runtime, caps.system, caps.eye_size.0, caps.eye_size.1, caps.blend_mode);
    let gpu = context.gpu().clone();
    let panel_px = ((PANEL_LOGICAL.0 * PANEL_SCALE) as u32, (PANEL_LOGICAL.1 * PANEL_SCALE) as u32);
    let mut ui = SettingsUi::new(
        SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() },
        settings.clone(),
        panel_px,
        PANEL_SCALE,
    )
    .map_err(|e| e.to_string())?;
    let panel_visible = Rc::new(RefCell::new(options.show_settings));
    ui.on_close({
        let visible = panel_visible.clone();
        move || *visible.borrow_mut() = false
    });
    let mut session: XrSession = context
        .create_session(&SessionConfig { panel: Some(panel_px), readback: options.xr_capture.is_some(), ..Default::default() })
        .map_err(|e| e.to_string())?;
    let mut compositor = Compositor::new(&gpu.device, &gpu.queue, session.eye_format());
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    let panel = PanelPlacement {
        pose: Pose { position: Vec3::new(0.0, -0.75, -1.1), orientation: Quat::from_rotation_x(-25f32.to_radians()) },
        size: (PANEL_WIDTH, PANEL_WIDTH * panel_px.1 as f32 / panel_px.0 as f32),
    };
    let mut pointer = PanelPointer::default();
    let mut menu_held = false;
    let start = Instant::now();
    let mut frames = 0u64;
    let mut exit_requested = false;
    let mut presented_size = (320u32, 216u32);
    let mut game_paused = false;
    let mut captured = false;
    let mut movie = MovieFrame::default();
    loop {
        for event in session.poll_events().map_err(|e| e.to_string())? {
            if let SessionEvent::StateChanged { state, .. } = event {
                println!("event state={state:?}");
            }
        }
        if session.should_exit() {
            break;
        }
        if !session.is_running() {
            if start.elapsed().as_secs() > 20 {
                return Err(format!("no running session after 20 s (state {:?})", session.state()));
            }
            std::thread::sleep(std::time::Duration::from_millis(5));
            continue;
        }
        let Some(frame) = session.begin_frame().map_err(|e| e.to_string())? else { continue };
        let should_render = frame.should_render;
        let eyes = frame.eyes;
        let input = frame.input.clone();
        let head = frame.head;
        let period = frame.predicted_display_period.as_nanos();
        let display_time = frame.predicted_display_time.as_nanos();

        // The panel: left menu toggles it; while it shows, the pad is idle.
        if input.hands[0].menu && !menu_held {
            let visible = !*panel_visible.borrow();
            *panel_visible.borrow_mut() = visible;
        }
        menu_held = input.hands[0].menu;
        let visible = *panel_visible.borrow();
        if visible {
            pointer.update(&ui, &panel, panel_px, &input.hands[1]);
        }

        // The game: whatever frames are due by the wall clock, once per
        // headset frame (never per eye), unless paused for a test.
        let pause = options.xr_pause.is_some_and(|(at, count)| frames >= at && frames < at + count);
        if pause != game_paused {
            game_paused = pause;
            if pause { game.pause() } else { game.resume() }
            println!("game paused={}", u8::from(pause));
        }
        let current = settings.borrow().settings().clone();
        let mut ran = 0;
        if !game_paused {
            let buttons = if visible { 0 } else { pad_buttons(&input) };
            let focused = session.state() == xem_xr::SessionState::FOCUSED;
            ran = game.update(buttons, &current, || focused)?;
        }
        // The virtual screen fills about half of a view's height.
        let config = ViewConfig::resolve(&current.graphics, HostClass::Headset, session.eye_size().1 / 2, view.max_scale());
        view.configure(config);
        let restart = view.take_restart();
        let (trace, display, scene) = game.take_frame(restart);
        view.push(trace);
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let picture = if GameView::shows(&display) {
            view.present(&mut encoder, &display, &scene, start.elapsed().as_secs_f64()).map(|p| p.size)
        } else {
            // 24-bit movie frames, as the device shows them.
            let (width, height, rgba) = game.display();
            movie.set(&gpu, (width, height), &rgba);
            (width > 0 && height > 0).then_some((width, height))
        };
        gpu.queue.submit([encoder.finish()]);
        if let Some(size) = picture {
            presented_size = size;
        }
        let from_view = GameView::shows(&display);

        if visible && ui.update().map_err(|e| e.to_string())? {
            session
                .render_panel(|encoder, target| {
                    encoder.copy_texture_to_texture(ui.texture().as_image_copy(), target.as_image_copy(), ui.texture().size());
                })
                .map_err(|e| e.to_string())?;
        }

        if should_render {
            let xr = current.graphics.xr;
            let width = f32::from(xr.width_cm) / 100.0;
            let height = width * presented_size.1 as f32 / presented_size.0 as f32;
            let pose = screen_pose(f32::from(xr.distance_cm) / 100.0).matrix();
            let filter = match current.presentation.filter {
                Filter::Nearest => wgpu::FilterMode::Nearest,
                Filter::Linear => wgpu::FilterMode::Linear,
            };
            let texture = match (picture, from_view) {
                (None, _) => None,
                (Some(_), true) => view.presented_texture().cloned(),
                (Some(_), false) => movie.texture.clone(),
            };
            // Capture once, in the run's last third, facing the screen.
            let late = options.exit_after_frames.is_some_and(|n| frames * 3 >= n * 2)
                || options.exit_at_frame.is_some_and(|at| game.frames_run() * 3 >= at * 2);
            let facing = head.is_some_and(|head| (head.orientation * Vec3::NEG_Z).dot(Vec3::NEG_Z) > 0.95);
            let capture = options.xr_capture.as_ref().filter(|_| late && facing && !captured);
            let mut readback = None;
            session
                .render_eyes(|encoder, target| {
                    let screen = texture.as_ref().map(|t| t.create_view(&Default::default()));
                    let quads: [Vec<Quad>; 2] = [0, 1].map(|eye| {
                        let view_proj: Mat4 = eyes[eye].proj * eyes[eye].view;
                        screen
                            .iter()
                            .map(|screen| {
                                let mut quad = Quad::world(screen, view_proj, pose, Vec2::new(width, height));
                                // The renderer's frames hold sRGB-encoded values in a linear
                                // format; movie frames are an sRGB texture.
                                quad.srgb_encoded = from_view;
                                quad.filter = filter;
                                quad
                            })
                            .collect()
                    });
                    compositor.draw_targets(
                        encoder,
                        &[(target.views[0], Some(BACKDROP), &quads[0]), (target.views[1], Some(BACKDROP), &quads[1])],
                    );
                    if capture.is_some() {
                        readback = Some(crate::readback::Readback::record(&gpu.device, encoder, target.texture));
                    }
                })
                .map_err(|e| e.to_string())?;
            if let (Some(dir), Some(readback)) = (capture, readback) {
                readback.save(&gpu.device, &[dir.join("left.png"), dir.join("right.png")])?;
                println!("capture dir={}", dir.display());
                captured = true;
            }
        }
        let ended = session.end_frame(visible.then_some(panel)).map_err(|e| e.to_string())?;
        let head = head.map_or("-".into(), |p| format!("{:.5},{:.5},{:.5};{:.5},{:.5},{:.5},{:.5}", p.position.x, p.position.y, p.position.z, p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w));
        println!(
            "xrframe n={frames} pdt_ns={display_time} period_ns={period} render={} layers={} game_ran={ran} game_frames={} game_vblanks={} paused={} picture={} head={head}",
            u8::from(should_render),
            ended.layers.len(),
            game.frames_run(),
            game.vblanks(),
            u8::from(game_paused),
            picture.map_or("-".into(), |(w, h)| format!("{w}x{h}")),
        );
        frames += 1;
        let at_frame = options.exit_at_frame.is_some_and(|at| game.frames_run() >= at);
        if (options.exit_after_frames.is_some_and(|n| frames >= n) || at_frame) && !exit_requested {
            if let Some(at) = options.exit_at_frame.filter(|_| at_frame) {
                println!("xem-desktop: frame {} memory_hash {} (exit at {at})", game.frames_run(), game.memory_hash());
            }
            session.request_exit().map_err(|e| e.to_string())?;
            exit_requested = true;
        }
    }
    println!("exit frames={frames} game_frames={} memory_hash {}", game.frames_run(), game.memory_hash());
    Ok(())
}
