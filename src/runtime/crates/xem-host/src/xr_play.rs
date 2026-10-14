//! Seated virtual-screen play in an OpenXR headset (`xem-desktop --xr`,
//! and the Android host's `--xr` on a standalone headset such as Horizon).
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
//! The controllers are the game bindings' gamepad ([`gamepad`]): by default
//! the left stick moves, A confirms, B cancels and runs, Y jumps, X opens
//! the menu, the triggers and the right stick turn the camera.
//!
//! Every frame prints one `key=value` line for scripts/xr-game-smoke.sh.

use std::cell::{Cell, RefCell};
use std::rc::Rc;
use std::time::Instant;
use crate::Game;
use xem_render::glam::{Mat4, Quat, Vec2, Vec3};
use xem_render::ps1::{GameView, HostClass, ViewConfig};
use xem_render::{Compositor, Quad, wgpu};
use xem_settings::{Filter, SettingsService};
use xem_ui::{LogicalPosition, PointerEventButton, SettingsUi, SharedGpu, WindowEvent};
use xem_xr::{HandInput, InputState, PanelPlacement, Pose, SessionConfig, SessionEvent, XrContext, XrSession};
use crate::assisted::{self, Assisted};

const PANEL_LOGICAL: (f32, f32) = (440.0, 560.0);
const PANEL_SCALE: f32 = 2.0;
const PANEL_WIDTH: f32 = 0.55;
/// The room around the screen.
const BACKDROP: wgpu::Color = wgpu::Color { r: 0.012, g: 0.014, b: 0.02, a: 1.0 };

/// The controllers as a standard gamepad (the `xr-standard` layout the
/// game bindings name, `xem_settings::bindings`): A South, B East, X West,
/// Y North, triggers L1/R1, grips LT/RT, the right stick's click Back, the
/// left's Start; the sticks are its sticks (negative y up).
pub fn gamepad(input: &InputState) -> ([u8; 16], [f32; 4]) {
    let [left, right]: &[HandInput; 2] = &input.hands;
    let mut pressed = [0u8; 16];
    for (i, on) in [right.primary, right.secondary, left.primary, left.secondary,
        left.trigger > 0.5, right.trigger > 0.5, left.squeeze, right.squeeze,
        right.thumbstick_click, left.thumbstick_click].into_iter().enumerate() {
        pressed[i] = u8::from(on);
    }
    (pressed, [left.thumbstick[0], -left.thumbstick[1], right.thumbstick[0], -right.thumbstick[1]])
}

/// Points at the panel with the right controller's aim ray.
#[derive(Default)]
struct PanelPointer {
    pressed: bool,
    /// The ray was on the panel at the last update.
    inside: bool,
}

impl PanelPointer {
    fn update(&mut self, ui: &SettingsUi, panel: &PanelPlacement, panel_px: (u32, u32), hand: &HandInput) -> bool {
        let on = self.on_panel(ui, panel, panel_px, hand);
        // The ray leaving the panel leaves nothing on it hovered.
        if self.inside && !on {
            ui.dispatch(WindowEvent::PointerExited);
        }
        self.inside = on;
        on
    }

    fn on_panel(&mut self, ui: &SettingsUi, panel: &PanelPlacement, panel_px: (u32, u32), hand: &HandInput) -> bool {
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

impl PanelPointer {
    /// A hand's pointer on the panel at (u, v), and a pinch: one click there.
    fn hand(&mut self, ui: &SettingsUi, panel_px: (u32, u32), uv: Option<(f32, f32)>, pinched: bool) {
        let Some((u, v)) = uv else {
            if self.inside {
                ui.dispatch(WindowEvent::PointerExited);
            }
            self.inside = false;
            return;
        };
        self.inside = true;
        let scale = ui.scale_factor();
        let position = LogicalPosition::new(u * panel_px.0 as f32 / scale, v * panel_px.1 as f32 / scale);
        ui.dispatch(WindowEvent::PointerMoved { position });
        if pinched {
            let button = PointerEventButton::Left;
            ui.dispatch(WindowEvent::PointerPressed { position, button });
            ui.dispatch(WindowEvent::PointerReleased { position, button });
        }
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

/// What a headset run is asked for (tests and captures besides play).
#[derive(Clone, Debug, Default)]
pub struct XrPlay {
    /// Show the settings panel at entry.
    pub show_settings: bool,
    /// Pause the game at headset frame N for M frames.
    pub pause: Option<(u64, u64)>,
    /// Save both eyes of a late frame here.
    pub capture: Option<std::path::PathBuf>,
    /// End the session after this many headset frames.
    pub exit_after_frames: Option<u64>,
    /// End the session when the game has run this many frames, reporting its RAM hash.
    pub exit_at_frame: Option<u64>,
    /// Synthetic hand gestures (headset frame, `xem_assist::synthetic`
    /// gesture name) for runtimes without hand tracking: from the first,
    /// the hands rest open in front of the head between gestures.
    pub hands: Vec<(u64, String)>,
}

/// How a headset run ended.
pub struct Ended {
    pub game: Game,
    /// The player confirmed the panel's Quit: the app ends, not only the session.
    pub quit: bool,
}

/// Plays `game` in a headset until the session ends.
pub fn run(platform: &xem_xr::Platform, options: &XrPlay, settings: Rc<RefCell<SettingsService>>, mut game: Game) -> Result<Ended, String> {
    let context = XrContext::new(platform).map_err(|e| e.to_string())?;
    let caps = context.capabilities();
    println!("xr runtime={:?} system={:?} eye_size={}x{} blend={:?}", caps.runtime, caps.system, caps.eye_size.0, caps.eye_size.1, caps.blend_mode);
    // The standard hand gestures the runtime offers (none is imitated).
    println!("xr hands: tracking={} hand_interaction={} microgestures={} eye_gaze={}", caps.hand_tracking, caps.hand_interaction, caps.microgestures, caps.eye_gaze);
    let gpu = context.gpu().clone();
    let panel_px = ((PANEL_LOGICAL.0 * PANEL_SCALE) as u32, (PANEL_LOGICAL.1 * PANEL_SCALE) as u32);
    let mut ui = SettingsUi::new(
        SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() },
        settings.clone(),
        panel_px,
        PANEL_SCALE,
    )
    .map_err(|e| e.to_string())?;
    // The Controls section lists first-person inspection's headset controls.
    ui.set_first_person(true);
    ui.set_first_person_headset(true);
    let panel_visible = Rc::new(RefCell::new(options.show_settings));
    ui.on_close({
        let visible = panel_visible.clone();
        move || *visible.borrow_mut() = false
    });
    let requests = Rc::new(RefCell::new(Vec::new()));
    ui.on_app_request({
        let requests = requests.clone();
        move |request| requests.borrow_mut().push(request)
    });
    let mut operations = crate::operations::Operations::default();
    let mut hud = xem_ui::FrameRateUi::new(
        SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() },
        PANEL_SCALE,
    ).map_err(|e| e.to_string())?;
    let quit = Rc::new(Cell::new(false));
    ui.on_quit({
        let quit = quit.clone();
        move || quit.set(true)
    });
    let mut session: XrSession = context
        .create_session(&SessionConfig { panel: Some(panel_px), readback: options.capture.is_some(), ..Default::default() })
        .map_err(|e| e.to_string())?;
    let mut compositor = Compositor::new(&gpu.device, &gpu.queue, session.eye_format());
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    let panel = PanelPlacement {
        pose: Pose { position: Vec3::new(0.0, -0.75, -1.1), orientation: Quat::from_rotation_x(-25f32.to_radians()) },
        size: (PANEL_WIDTH, PANEL_WIDTH * panel_px.1 as f32 / panel_px.0 as f32),
    };
    let mut pointer = PanelPointer::default();
    let mut menu_held = false;
    let mut player = xem_settings::player::PlayerInput::default();
    // First-person inspection's toggle on the controllers (their Back), and
    // when its view last turned.
    let mut look_toggle = xem_settings::look::Toggle::default();
    let mut looked: Option<Instant> = None;
    let mut assist = Assisted::new();
    // The virtual screen as the last frame showed it (where gestures aim).
    let mut last_screen: Option<xem_assist::headset::Screen> = None;
    let mut scripted = std::collections::VecDeque::new();
    let mut pinched = [false; 2];
    let mut scripted_aim: Option<(bool, (f32, f32))> = None;
    let mut wanted_disc = None;
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
            if !visible {
                ui.cancel_dialog();
            }
        }
        menu_held = input.hands[0].menu;
        // The assisted controls: gestures aimed at the screen as last shown
        // (prayer hands toggle the panel; a pinch clicks on it).
        let focused = session.state() == xem_xr::SessionState::FOCUSED;
        let mut assisted_input = assisted::input(&input, head, start.elapsed().as_secs_f64(), focused, &mut pinched);
        if !options.hands.is_empty() {
            for (_, entry) in options.hands.iter().filter(|(at, _)| *at == frames) {
                // `name@x/y` also aims eye gaze at that screen pixel (the
                // scripted gaze stands in for eye tracking from then on).
                let (name, aim) = entry.split_once('@').map_or((entry.as_str(), None), |(n, a)| (n, Some(a)));
                // `name@panel:x/y` aims at the settings panel's logical pixel.
                let (on_panel, aim) = match aim.and_then(|a| a.strip_prefix("panel:")) {
                    Some(rest) => (true, Some(rest)),
                    None => (false, aim),
                };
                if let Some((x, y)) = aim.and_then(|a| a.split_once('/')) {
                    scripted_aim = x.parse().ok().zip(y.parse().ok()).map(|p| (on_panel, p));
                }
                scripted.extend(xem_assist::synthetic::gesture(name).expect("a known gesture"));
            }
            if let (Some((on_panel, aim)), Some(screen), Some(head)) = (scripted_aim, last_screen, head) {
                let target = if on_panel {
                    let (u, v) = (aim.0 / PANEL_LOGICAL.0, aim.1 / PANEL_LOGICAL.1);
                    panel.pose.position + panel.pose.orientation * Vec3::new((u - 0.5) * panel.size.0, (0.5 - v) * panel.size.1, 0.0)
                } else {
                    screen.point(aim)
                };
                let direction = (target - head.position).normalize();
                assisted_input.gaze = Some(xem_assist::headset::Pose { position: head.position, orientation: Quat::from_rotation_arc(Vec3::NEG_Z, direction) });
            }
            if let Some(head) = head {
                // The scripted hands stand in for a runtime with the
                // standard gestures (its pinch and microgestures with them).
                let rest = xem_assist::synthetic::gesture("pause").expect("a known gesture")[0];
                let step = scripted.pop_front().unwrap_or(rest);
                let hands = xem_assist::synthetic::in_front(step.hands, head.position, head.orientation);
                let frame = step.frame(assisted_input.time, hands, head.orientation * Vec3::X, xem_assist::synthetic::Runtime::HORIZON);
                assisted_input.hands = hands;
                assisted_input.pinch = frame.pinch;
                assisted_input.micro = frame.micro;
                assisted_input.micro_capable = frame.micro_capable;
            }
        }
        if let Some(screen) = last_screen {
            let current = settings.borrow().settings().clone();
            let open = assist.update(&current, &assisted_input, &screen, &mut game, &mut player, &mut view, *panel_visible.borrow());
            if open != *panel_visible.borrow() {
                *panel_visible.borrow_mut() = open;
                if !open {
                    ui.cancel_dialog();
                }
            }
        }
        let visible = *panel_visible.borrow();
        if visible {
            if input.hands[1].active && assisted_input.hands.iter().all(Option::is_none) {
                pointer.update(&ui, &panel, panel_px, &input.hands[1]);
            } else {
                // Hands tracked (or no controller): the pointer source's ray,
                // and a pinch to click.
                let quad = xem_assist::headset::Quad { pose: xem_assist::headset::Pose { position: panel.pose.position, orientation: panel.pose.orientation }, size: panel.size };
                let pinched = assist.gestures.iter().any(|g| matches!(g, xem_assist::gesture::Gesture::Pinch { .. }));
                pointer.hand(&ui, panel_px, assist.headset.hit(&assisted_input, &quad).map(|(uv, _)| uv), pinched);
            }
        }

        for request in std::mem::take(&mut *requests.borrow_mut()) {
            let result = operations.execute(request,None,Some(&mut game),&mut settings.borrow_mut());
            if let xem_settings::AppRequest::Action(action) = request {
                use xem_settings::AppAction::*;
                let saved = match action { SaveState | ExportState => Some(true), LoadState | ImportState => Some(false), _ => None };
                if let Some(saved) = saved { hud.notify_state(saved,result.as_ref().err().map(String::as_str),start.elapsed()); }
            }
            match result {
                Ok(message) => ui.set_operation_status(&message),
                Err(error) => ui.set_operation_error(&error),
            }
        }
        let (fast_forward,status) = game.time_status();
        ui.set_time_status(Some(fast_forward),&status);
        ui.set_storage_status(&game.storage_status());

        // The game: whatever frames are due by the wall clock, once per
        // headset frame (never per eye), unless paused for a test or by the
        // player (the Pause grab, settings' own pause, lost tracking).
        let pause = options.pause.is_some_and(|(at, count)| frames >= at && frames < at + count) || assist.headset.pauses.paused();
        if pause != game_paused {
            game_paused = pause;
            if pause { game.pause() } else { game.resume() }
            println!("game paused={}", u8::from(pause));
        }
        // A disc the game waits for and the changer has not got: ask for it.
        let wanted = game.wanted_disc();
        if wanted != wanted_disc {
            wanted_disc = wanted;
            if let Some(number) = wanted {
                ui.ask_for_disc(number);
                *panel_visible.borrow_mut() = true;
            }
        }
        let current = settings.borrow().settings().clone();
        // The Binaural listener turns with the head, paused or not (lost
        // tracking keeps the last turn); the game never sees it.
        if let Some(head) = head {
            game.set_head(Some(xem_core::binaural::head_axes(head.orientation.to_array())));
        }
        let mut ran = 0;
        if !game_paused {
            let (pressed, axes) = gamepad(&input);
            if visible { player.clear() } else { player.gamepad(&pressed, &axes) }
            let context = game.pad_context();
            // The controllers' Back (the right stick's click) released alone
            // toggles first-person inspection in a field, as a gamepad's
            // does, where Ex Machina draws it. It keeps the character where
            // it stands (`xem_settings::look`); running by default would
            // leave Run pressed alone.
            let drawn = current.graphics.renderer == xem_settings::Renderer::ExMachina;
            if visible {
                look_toggle.clear();
            } else if look_toggle.update(false, &pressed, context.field) && drawn {
                view.set_first_person(!view.first_person());
            }
            let inspecting = view.first_person();
            // The right stick turns the view.
            let now = Instant::now();
            let seconds = looked.replace(now).map_or(0.0, |then| now.duration_since(then).as_secs_f32().min(0.1));
            if !visible && xem_settings::look::turns(inspecting, context.field) {
                let (yaw, pitch) = xem_settings::look::Turn { stick: [axes[2], axes[3]], ..Default::default() }.radians(seconds);
                view.look_around(yaw, pitch);
            }
            player.run_by_default = current.run_by_default && !inspecting;
            let buttons = if visible { 0 } else { player.buttons(&current.game_bindings, start.elapsed().as_secs_f64() * 1000.0, context) };
            let buttons = xem_settings::look::game_buttons(buttons, inspecting, context.field, &context.assignment);
            game.want_inspection(inspecting);
            let focused = session.state() == xem_xr::SessionState::FOCUSED;
            ran = game.update(buttons, &current, || focused)?;
            game.follow_sound_mode(&settings);
            player.frames_ran(ran);
        }
        let (forward,rewind) = game.time_activity();
        hud.set_activity(forward,rewind);
        hud.frame(start.elapsed(),PANEL_SCALE,current.presentation.show_fps).map_err(|e| e.to_string())?;
        // The virtual screen fills about half of a view's height.
        let config = ViewConfig::resolve(&current.graphics, HostClass::Headset, (session.eye_size().0 / 2, session.eye_size().1 / 2), view.max_scale());
        view.configure(config);
        let restart = view.take_restart();
        // The Original renderer shows the display as the device holds it
        // (nothing to look around in).
        let original = current.graphics.renderer == xem_settings::Renderer::Original;
        if original {
            view.set_first_person(false);
        }
        game.want_actor_view(if !original && current.graphics.actors_in_view { view.actor_view() } else { 0 });
        // The Binaural listener looks where inspection looks; the head turns
        // it once more (`set_head` above).
        game.want_listener_view(view.listener_axes());
        let (display, scene) = if original {
            game.stop_trace();
            (game.display_info(), String::new())
        } else {
            let (trace, display, scene) = game.take_frame(restart, view.reveals());
            view.push(trace);
            (display, scene)
        };
        let from_view = !original && (GameView::shows(&display) || view.shows_movie(&display));
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let picture = if from_view {
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

        if visible && ui.update().map_err(|e| e.to_string())? {
            session
                .render_panel(|encoder, target| {
                    encoder.copy_texture_to_texture(ui.texture().as_image_copy(), target.as_image_copy(), ui.texture().size());
                })
                .map_err(|e| e.to_string())?;
        }

        // The virtual screen: its size (the setting's, scaled by two-hand
        // scaling) and the display pixels it spans.
        let xr = current.graphics.xr;
        let width = f32::from(xr.width_cm) / 100.0 * assist.scaling.scale;
        let height = width * presented_size.1 as f32 / presented_size.0 as f32;
        let side = if from_view { view.config().map_or(0, |c| c.side) as f32 } else { 0.0 };
        let screen = xem_assist::headset::Screen {
            quad: xem_assist::headset::Quad {
                pose: xem_assist::headset::Pose { position: screen_pose(f32::from(xr.distance_cm) / 100.0).position, orientation: Quat::IDENTITY },
                size: (width, height),
            },
            display: if from_view { (display.width as f32, display.height as f32) } else { (presented_size.0 as f32, presented_size.1 as f32) },
            side,
        };
        last_screen = Some(screen);
        let style = Assisted::style(&current);
        let marks: Vec<(wgpu::TextureView, Mat4, Vec2)> = assist.placed(&current, &assisted_input, &screen, &mut game).into_iter()
            .map(|placed| (assist.texture(&gpu, &placed.widget, style), placed.pose, placed.size))
            .collect();

        if should_render {
            let pose = screen_pose(f32::from(xr.distance_cm) / 100.0).matrix();
            let hud_quad = hud.visible().then(|| {
                let (w,h) = hud.size();
                let unit = width / 640.0 / PANEL_SCALE;
                let size = Vec2::new(w as f32*unit,h as f32*unit);
                let placement = pose * Mat4::from_translation(Vec3::new(-width/2.0+size.x/2.0+width/80.0,height/2.0-size.y/2.0-width/80.0,0.002));
                (hud.view().clone(),placement,size)
            });
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
            let capture = options.capture.as_ref().filter(|_| late && facing && !captured);
            let mut readback = None;
            session
                .render_eyes(|encoder, target| {
                    let screen = texture.as_ref().map(|t| t.create_view(&Default::default()));
                    let quads: [Vec<Quad>; 2] = [0, 1].map(|eye| {
                        let view_proj: Mat4 = eyes[eye].proj * eyes[eye].view;
                        let glass = marks.iter().chain(hud_quad.iter()).map(|(texture, pose, size)| {
                            let mut quad = Quad::world(texture, view_proj, *pose, *size);
                            quad.srgb_encoded = true;
                            quad
                        });
                        screen
                            .iter()
                            .map(|screen| {
                                let mut quad = Quad::world(screen, view_proj, pose, Vec2::new(width, height));
                                // The renderer's frames hold sRGB-encoded values in a linear
                                // format; movie frames are an sRGB texture.
                                quad.srgb_encoded = from_view;
                                quad.filter = filter;
                                // Movie frames the device shows itself stay as the
                                // decoder made them, each pixel a block.
                                if !from_view && !original {
                                    quad.filter = wgpu::FilterMode::Nearest;
                                }
                                quad
                            })
                            .chain(glass)
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
            "xrframe n={frames} wall_us={} pdt_ns={display_time} period_ns={period} render={} layers={} game_ran={ran} game_frames={} game_vblanks={} paused={} picture={} head={head} gestures={} player_pause={} marks={} assist={} context={}",
            start.elapsed().as_micros(),
            u8::from(should_render),
            ended.layers.len(),
            game.frames_run(),
            game.vblanks(),
            u8::from(game_paused),
            picture.map_or("-".into(), |(w, h)| format!("{w}x{h}")),
            serde_json::to_string(&assist.gestures).unwrap_or_default(),
            u8::from(assist.headset.pauses.player),
            marks.len(),
            game.assist_state(),
            serde_json::to_string(&game.pad_context().ui).unwrap_or_default(),
        );
        frames += 1;
        if quit.get() && !exit_requested {
            session.request_exit().map_err(|e| e.to_string())?;
            exit_requested = true;
        }
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
    Ok(Ended { game, quit: quit.get() })
}
