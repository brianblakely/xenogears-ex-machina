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
//! First-person inspection (the underhand grab or Toggle first-person
//! view's chord anywhere in a field but its menu, the controllers' Back
//! released alone where Select toggles it, an agent's `first_person`)
//! looks from within the scene instead: each eye's own
//! picture from the character's eye, turned and moved as the eye is, over
//! its whole view, and the game's interface alone on a pane where the
//! screen was (`GameView::present_headset`), every headset frame, paused
//! or not; the right stick turns the view in 30-degree steps.
//!
//! With the diorama as the view (Graphics' headset view), each eye sees
//! the scene as a miniature on a table in front of the seat instead, from
//! where it is (`GameView::present_diorama`), and the game's interface on
//! a pane above the table's far edge; the screen shows where the diorama
//! gives way to it (movies, menus). The keys stand beside the pane, the
//! settings panel where it floats. The pointer aims through the table at
//! the game's world: a pinch walks to the floor it meets, a tap talks to
//! the actor beside it. Two whole-hand pinches scale the diorama about its
//! table's anchor, live, and the scale they end at becomes the setting
//! (`xem_assist::headset::Scaling`); with the screen as the view they scale
//! the screen for the session. Where the system can show the room (alpha
//! blending, or `XR_FB_passthrough`) and the player turns Passthrough on,
//! the room stands around the table in place of the backdrop and behind it
//! in place of the game's clear; elsewhere (Monado's simulated HMD, or
//! after the runtime refused it) the panel offers no Passthrough.
//!
//! Everything stands in front of the seat (`xem_assist::headset::Seat`):
//! LOCAL's origin, moved to the head by Recenter (the panel's button, an
//! app action) and back when the runtime recentres LOCAL itself
//! (`ReferenceSpaceChangePending`).
//!
//! The controllers are the game bindings' gamepad ([`gamepad`]): by default
//! the left stick moves, A confirms, B cancels and runs, Y jumps, X opens
//! the menu, the triggers and the right stick turn the camera. The app's
//! bindings take their chords from it first, as in the window
//! (`xem_settings::AppInput`): Back (the right stick's click) with A saves
//! the state, with Y loads it, with the left grip rewinds while held,
//! with the right grip toggles fast-forward and with the right trigger
//! first-person view; Back+Start toggles Settings.
//!
//! Every frame prints one `key=value` line for scripts/xr-game-smoke.sh,
//! and every app operation a line with the game's memory hash after it.

use std::cell::{Cell, RefCell};
use std::rc::Rc;
use std::time::Instant;
use crate::Game;
use xem_assist::headset::Seat;
use xem_render::glam::{Mat4, Quat, Vec2, Vec3};
use xem_render::ps1::{DioramaEye, GameView, HeadsetEye, HostClass, ViewConfig, XrView, headset_movie_output, xr_view};
use xem_render::{Compositor, Quad, Rect, wgpu};
use xem_settings::{AppAction, AppRequest, Filter, SettingsService};
use xem_ui::{LogicalPosition, PointerEventButton, SettingsUi, SharedGpu, WindowEvent};
use xem_xr::{HandInput, InputState, PanelPlacement, Pose, SessionConfig, SessionEvent, XrContext, XrSession};
use crate::assisted::{self, Assisted};

const PANEL_LOGICAL: (f32, f32) = (440.0, 560.0);
const PANEL_SCALE: f32 = 2.0;
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

/// An agent's `first_person` command (`--control`) or the app's Toggle
/// first-person view in the headset's view, in `field` under `renderer`
/// (`xem_settings::look::Command`): its answer, or why nothing changed.
fn first_person(view: &mut GameView, command: &xem_settings::look::Command, field: xem_core::FieldControls, renderer: xem_settings::Renderer) -> Result<serde_json::Value, String> {
    let change = command.change(view.first_person(), view.look_angles(), field, renderer)?;
    if change.on != view.first_person() {
        view.set_first_person(change.on);
    }
    view.look_around(change.turn.0, change.turn.1);
    Ok(xem_settings::look::reply(view.first_person(), view.look_angles()))
}

/// Points at the panel with the right controller's aim ray, or with the
/// hands' pointer source ([`xem_assist::headset::PanelPointer`]).
#[derive(Default)]
struct PanelPointer {
    pressed: bool,
    /// The ray was on the panel at the last update.
    inside: bool,
    /// The hands' pointer: a pinch presses it from its start to its end.
    hands: xem_assist::headset::PanelPointer,
}

/// A point of the panel at (u, v) across it, in its logical pixels.
fn panel_position(ui: &SettingsUi, panel_px: (u32, u32), (u, v): (f32, f32)) -> LogicalPosition {
    let scale = ui.scale_factor();
    LogicalPosition::new(u * panel_px.0 as f32 / scale, v * panel_px.1 as f32 / scale)
}

impl PanelPointer {
    fn update(&mut self, ui: &SettingsUi, panel: &PanelPlacement, panel_px: (u32, u32), hand: &HandInput) -> bool {
        // The controller takes the pointer: a pinch that held it lets go.
        if let Some(uv) = self.hands.release() {
            ui.dispatch(WindowEvent::PointerReleased { position: panel_position(ui, panel_px, uv), button: PointerEventButton::Left });
        }
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
    /// The hands' pointer on the panel at (u, v): a pinch beginning there
    /// (`began`, the hand's whose pinch began this frame) presses it, and
    /// the end of that hand's pinch (`pinch`, each hand's as the runtime
    /// reports it) lets it go where it pressed, the pointer moving nowhere
    /// meanwhile (no pinch-and-drag scrolling), so a pinch held on Hold to
    /// rewind holds it.
    fn hand(&mut self, ui: &SettingsUi, panel_px: (u32, u32), uv: Option<(f32, f32)>, began: Option<usize>, pinch: [Option<bool>; 2]) {
        use xem_assist::headset::{PanelEvent, Presser};
        let held = |presser| matches!(presser, Presser::Pinch(hand) if pinch[hand] == Some(true));
        let button = PointerEventButton::Left;
        for event in self.hands.update(uv, began.map(Presser::Pinch), held) {
            ui.dispatch(match event {
                PanelEvent::Moved(uv) => WindowEvent::PointerMoved { position: panel_position(ui, panel_px, uv) },
                PanelEvent::Pressed(uv) => WindowEvent::PointerPressed { position: panel_position(ui, panel_px, uv), button },
                PanelEvent::Released(uv) => WindowEvent::PointerReleased { position: panel_position(ui, panel_px, uv), button },
                PanelEvent::Exited => WindowEvent::PointerExited,
            });
        }
    }
}

/// One app operation in the headset (the panel's, a chord's, a gesture's,
/// an agent's `app`): Recenter places this loop's view (the seat moves to
/// the head); the rest go through `Operations::execute`, as in the window.
/// A failure shows in the error bar (a state's outcome on the HUD), and a
/// line with the game's memory hash after it is printed.
#[allow(clippy::too_many_arguments)]
fn operate(
    request: AppRequest, operations: &mut crate::operations::Operations, game: &mut Game, settings: &RefCell<SettingsService>, assist: &mut Assisted,
    head: Option<Pose>, view: &mut GameView, recenters: &mut u32, hud: &mut xem_ui::FrameRateUi, elapsed: std::time::Duration, ui: &SettingsUi,
) -> Result<String, String> {
    let result = if request == AppRequest::Action(AppAction::Recenter) {
        let result = xem_assist::headset::recenter(&mut assist.seat, head.map(assist_pose), view);
        if result.is_ok() {
            *recenters += 1;
            let seat = assist.seat.position();
            println!("recenter seat={:.4},{:.4},{:.4}", seat.x, seat.y, seat.z);
        }
        result
    } else {
        operations.execute(request, None, Some(game), &mut settings.borrow_mut())
    };
    if let AppRequest::Action(action) = request {
        use xem_settings::AppAction::*;
        let saved = match action { SaveState | ExportState => Some(true), LoadState | ImportState => Some(false), _ => None };
        if let Some(saved) = saved {
            hud.notify_state(saved, result.as_ref().err().map(String::as_str), elapsed);
            println!("hud notice={:?}", hud.notice().unwrap_or_default());
        }
    }
    println!("{}", crate::operation_line(&request, &result, Some(game)));
    match &result {
        Ok(_) => ui.clear_operation_error(),
        Err(error) => ui.set_operation_error(error),
    }
    result
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

/// The game screen's pose: `distance` metres in front of the seat (LOCAL's
/// origin, the head at entry, until recentred), at eye height.
fn screen_pose(seat: &Seat, distance: f32) -> Mat4 {
    let pose = seat.place(xem_assist::headset::Pose { position: Vec3::new(0.0, 0.0, -distance), orientation: Quat::IDENTITY });
    Mat4::from_rotation_translation(pose.orientation, pose.position)
}

/// The settings panel's logical pixel (x, y) in LOCAL.
fn panel_point(panel: &PanelPlacement, (x, y): (f32, f32)) -> Vec3 {
    let (u, v) = (x / PANEL_LOGICAL.0, y / PANEL_LOGICAL.1);
    panel.pose.position + panel.pose.orientation * Vec3::new((u - 0.5) * panel.size.0, (0.5 - v) * panel.size.1, 0.0)
}

/// An eye as the renderer looks through it from within the scene: its pose
/// as the seat sees it (whose -Z faces the screen) and its field of view's
/// tangents.
fn headset_eye(eye: &xem_xr::EyeView, seat: &Seat) -> HeadsetEye {
    let fov = eye.fov;
    HeadsetEye {
        orientation: seat.relative(eye.pose.orientation).to_array(),
        position: seat.pose.inverse().transform_point3(eye.pose.position).to_array(),
        fov: [fov.left.tan(), fov.right.tan(), fov.up.tan(), fov.down.tan()],
    }
}

fn assist_pose(pose: Pose) -> xem_assist::headset::Pose {
    xem_assist::headset::Pose { position: pose.position, orientation: pose.orientation }
}

/// What the Binaural mode presents, for the frame log
/// (`xem_core::binaural::Heard`, not the whole status, every frame): the
/// presenter's head and its listener's view (first-person inspection's or
/// the diorama's), each as `[yaw, pitch]` (degrees, yaw to the right), the
/// listening position (world units), and where each placed effect and
/// each music part heads (`[id, azimuth, elevation]`, degrees), as JSON
/// without spaces; `-` while nothing presents.
fn binaural_line(game: &mut Game) -> String {
    let Some(heard) = game.session.runtime.services().spu.binaural().map(|presenter| presenter.heard()) else { return "-".into() };
    let heading = |sources: &[(u32, [f32; 2])]| -> Vec<serde_json::Value> {
        sources.iter().map(|(id, [azimuth, elevation])| serde_json::json!([id, azimuth, elevation])).collect()
    };
    serde_json::json!({"head": heard.head, "view": heard.view, "at": heard.at, "effects": heading(&heard.effects), "music": heading(&heard.music)}).to_string()
}

/// A two-hand scaling of the diorama that ended becomes the player's
/// setting, once, through the settings service (logged).
fn commit_scale(settings: &RefCell<SettingsService>, scaling: &mut xem_assist::headset::Scaling) {
    if let Some(percent) = scaling.reached() {
        match xem_assist::headset::commit_scale(&mut settings.borrow_mut(), percent) {
            Ok(ack) => println!("scale commit diorama_scale={percent} revision={}", ack.revision),
            Err(error) => println!("scale commit diorama_scale={percent} refused: {error}"),
        }
    }
}

/// Where a scripted gesture aims (`--xr-hands NAME@...`): a pixel of the
/// screen (or the diorama's pane), a logical pixel of the settings panel,
/// or a point of the game's world through the diorama.
#[derive(Clone, Copy, Debug)]
enum ScriptedAim {
    Screen((f32, f32)),
    Panel((f32, f32)),
    World(Vec3),
}

impl ScriptedAim {
    /// `x/y`, `panel:x/y` or `world:x/y/z`.
    fn parse(aim: &str) -> Option<ScriptedAim> {
        let numbers = |text: &str| text.split('/').map(str::parse::<f32>).collect::<Result<Vec<_>, _>>().ok();
        if let Some(rest) = aim.strip_prefix("world:") {
            let [x, y, z] = numbers(rest)?[..] else { return None };
            return Some(ScriptedAim::World(Vec3::new(x, y, z)));
        }
        let (panel, rest) = match aim.strip_prefix("panel:") {
            Some(rest) => (true, rest),
            None => (false, aim),
        };
        let [x, y] = numbers(rest)?[..] else { return None };
        Some(if panel { ScriptedAim::Panel((x, y)) } else { ScriptedAim::Screen((x, y)) })
    }
}

/// What a diorama presentation gave this headset frame: the screen's share
/// of the view (0 the diorama alone, 1 the screen alone), the screen's
/// picture and its size while it shows, and while the diorama shows each
/// eye's picture of it (drawn at `opacity`), the pane and where it lies.
struct DioramaShown {
    share: f32,
    frame: Option<(wgpu::TextureView, (u32, u32))>,
    eyes: Vec<wgpu::TextureView>,
    opacity: f32,
    pane: Option<wgpu::TextureView>,
    space: Option<xem_render::ps1::DioramaSpace>,
}

/// What a headset run is asked for (tests and captures besides play).
#[derive(Clone, Debug, Default)]
pub struct XrPlay {
    /// Show the settings panel at entry.
    pub show_settings: bool,
    /// Pause the game at headset frame N for M frames.
    pub pause: Option<(u64, u64)>,
    /// Save both eyes of a late frame here (with `capture_at`, of those
    /// headset frames instead, each in a directory named by its number).
    pub capture: Option<std::path::PathBuf>,
    pub capture_at: Vec<u64>,
    /// End the session after this many headset frames.
    pub exit_after_frames: Option<u64>,
    /// End the session when the game has run this many frames, reporting its RAM hash.
    pub exit_at_frame: Option<u64>,
    /// Write the game's state to the path once it has run this many frames
    /// (`crate::save_state_at`; the host's frame limit holds the game at the
    /// frame), then play on to `exit_at_frame`.
    pub save_state_at: Option<(u64, std::path::PathBuf)>,
    /// Synthetic hand gestures (headset frame, `xem_assist::synthetic`
    /// gesture name) for runtimes without hand tracking: from the first,
    /// the hands rest open in front of the head between gestures.
    pub hands: Vec<(u64, String)>,
    /// Recenter at these headset frames, as the app action does.
    pub recenter_at: Vec<u64>,
    /// Headset frames N to N+M-1 taken as the runtime reports lost
    /// tracking (no located head, no valid views), for runtimes whose
    /// simulated headset never loses it (Monado's).
    pub lose_tracking: Option<(u64, u64)>,
    /// Scripted controller buttons for runtimes whose controllers have
    /// none to press (headset frame, the gamepad's buttons as bits in
    /// [`gamepad`]'s order, `xem_settings::controls::PAD_NAMES`): held from that
    /// frame until the next entry's, beside the controllers' own. With a
    /// settings panel's logical pixel, the right controller points from
    /// the head at it for as long, its trigger (select) the entry's R1
    /// (Monado's simulated controllers point where no test chooses).
    pub pad: Vec<(u64, u16, Option<(f32, f32)>)>,
    /// Scripted right stick for runtimes whose controllers have none
    /// (headset frame, x and y from -1 to 1, y up): held from that frame
    /// until the next entry's, added to the controllers' own.
    pub stick: Vec<(u64, [f32; 2])>,
    /// Where Save state keeps its state and Import BIOS its image (the
    /// Android host's files directory); without one, beside the settings,
    /// as in the window.
    pub state_directory: Option<std::path::PathBuf>,
    /// Answer agents' JSON commands on stdin between the game's frames
    /// (`--control`, [`crate::Control`]): the command layer's, `settings`,
    /// `settings_apply {change}`, `panel {visible}`, `bios_import {path}` and
    /// `app {request}` (the panel's and the bindings' operations,
    /// [`crate::agent_request`]: a held rewind, Retry pending card writes,
    /// Recenter and the rest).
    pub control: bool,
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
    // Whether the room can show behind the diorama: alpha blending among
    // the blend modes, else XR_FB_passthrough; else not at all (Monado's
    // simulated HMD), and the panel offers no Passthrough.
    let offered = match caps.passthrough {
        Some(xem_xr::Passthrough::AlphaBlend) => "alpha_blend",
        Some(xem_xr::Passthrough::Fb) => "fb",
        None => "unavailable",
    };
    let modes: Vec<String> = caps.blend_modes.iter().map(|mode| format!("{mode:?}")).collect();
    println!("xr room: passthrough={offered} blend_modes={}", modes.join(","));
    if let Some(error) = &caps.passthrough_error {
        println!("xr room: XR_FB_passthrough unusable ({error})");
    }
    let gpu = context.gpu().clone();
    let panel_px = ((PANEL_LOGICAL.0 * PANEL_SCALE) as u32, (PANEL_LOGICAL.1 * PANEL_SCALE) as u32);
    let mut ui = SettingsUi::new(
        SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() },
        settings.clone(),
        panel_px,
        PANEL_SCALE,
    )
    .map_err(|e| e.to_string())?;
    // The Controls section lists first-person inspection's headset controls;
    // Graphics offers Recenter, and Passthrough where the system can show
    // the room.
    ui.set_first_person(true);
    ui.set_first_person_headset(true);
    ui.set_in_headset(true);
    ui.set_passthrough_offered(caps.passthrough.is_some());
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
    if let Some(directory) = &options.state_directory {
        operations.directory = Some(directory.clone());
    }
    let mut app_input = xem_settings::AppInput::default();
    let control = options.control.then(crate::Control::spawn);
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
    // How often the runtime moved LOCAL and the player recentred.
    let (mut resets, mut recenters) = (0u32, 0u32);
    let mut pointer = PanelPointer::default();
    // Whether the panel showed in the last frame.
    let mut panel_shown = options.show_settings;
    let mut menu_held = false;
    let mut player = xem_settings::player::PlayerInput::default();
    // First-person inspection's toggle on the controllers (their Back), the
    // app's Toggle first-person view asked for this frame (its chord), its
    // turn on the right stick, and the last eyes tracked (held through
    // frames the runtime cannot locate them in).
    let mut look_toggle = xem_settings::look::Toggle::default();
    let mut look_asked = false;
    let mut snap = xem_settings::look::SnapTurn::default();
    let mut tracked_eyes: Option<[HeadsetEye; 2]> = None;
    let mut assist = Assisted::new();
    // The virtual screen as the last frame showed it (where gestures aim).
    let mut last_screen: Option<xem_assist::headset::Screen> = None;
    let mut scripted = std::collections::VecDeque::new();
    let mut pinched = [false; 2];
    let mut scripted_aim: Option<ScriptedAim> = None;
    let mut wanted_disc = None;
    let start = Instant::now();
    let mut frames = 0u64;
    let mut exit_requested = false;
    let mut presented_size = (320u32, 216u32);
    let mut game_paused = false;
    let mut captured = false;
    let mut state_saved = false;
    let mut movie = MovieFrame::default();
    loop {
        for event in session.poll_events().map_err(|e| e.to_string())? {
            match event {
                SessionEvent::StateChanged { state, .. } => println!("event state={state:?}"),
                // The runtime recentred LOCAL: the seat is its origin again.
                SessionEvent::ReferenceSpaceChangePending { time } => {
                    xem_assist::headset::reference_changed(&mut assist.headset, &mut assist.scaling, &mut assist.seat, &mut view);
                    resets += 1;
                    println!("event reference_space_change time_ns={time}");
                }
                _ => {}
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
        // Lost tracking, as the runtime reports it: no head and no valid
        // views (a test's frames stand in for it where the runtime never
        // loses it).
        let tracked = !options.lose_tracking.is_some_and(|(at, count)| frames >= at && frames < at + count);
        if frame.views_valid && tracked {
            tracked_eyes = Some(eyes.each_ref().map(|eye| headset_eye(eye, &assist.seat)));
        }
        let mut input = frame.input.clone();
        let head = frame.head.filter(|_| tracked);
        let period = frame.predicted_display_period.as_nanos();
        let display_time = frame.predicted_display_time.as_nanos();
        let graphics = settings.borrow().settings().graphics;
        // Two-hand scaling scales the diorama while it is the view (from its
        // setting's scale), else the screen.
        assist.scaling.view(matches!(xr_view(&graphics), XrView::Diorama(_)).then_some(graphics.xr.diorama_scale));
        // The settings panel: below the screen (as large as two-hand scaling
        // makes it, in the last picture's shape), off it, or beside the
        // diorama's pane, off it and its keys, above the table, while the
        // last frame showed the diorama (which the Original renderer never
        // shows; where it gives way, the screen stands in for it).
        let panel = {
            let width = f32::from(graphics.xr.width_cm) / 100.0 * assist.scaling.scale;
            let screen = (width, width * presented_size.1 as f32 / presented_size.0 as f32);
            let distance = f32::from(graphics.xr.distance_cm) / 100.0;
            let aspect = panel_px.1 as f32 / panel_px.0 as f32;
            let diorama = last_screen.is_some_and(|screen| screen.diorama.is_some());
            let quad = match xr_view(&graphics) {
                XrView::Diorama(table) if diorama => xem_assist::draw::panel(&assist.seat, xem_assist::draw::Beside::Table(&table), aspect),
                _ => xem_assist::draw::panel(&assist.seat, xem_assist::draw::Beside::Screen { size: screen, distance }, aspect),
            };
            PanelPlacement { pose: Pose { position: quad.pose.position, orientation: quad.pose.orientation }, size: quad.size }
        };

        // The scripted controller input of this frame: a ray at a panel
        // pixel is the right controller's, pulled by the entry's R1.
        let script = options.pad.iter().filter(|(at, ..)| *at <= frames).last();
        if let (Some(&(_, buttons, Some(pixel))), Some(head)) = (script, head) {
            let right = &mut input.hands[1];
            right.active = true;
            right.aim = Some(Pose { position: head.position, orientation: Quat::from_rotation_arc(Vec3::NEG_Z, (panel_point(&panel, pixel) - head.position).normalize()) });
            right.select = buttons & (1 << 5) != 0;
            right.trigger = f32::from(u8::from(right.select));
        }
        // The controllers as the gamepad (with the scripted buttons), as
        // they are: the app's chords take theirs from it before the game has
        // the rest, as in the window, from the frame the session has the
        // input (losing it lets go of a held rewind, and of what the panel's
        // pointer held, as the window losing focus does).
        let focused = session.state() == xem_xr::SessionState::FOCUSED;
        let (mut pressed, mut axes) = gamepad(&input);
        if let Some((_, buttons, _)) = script {
            for (i, button) in pressed.iter_mut().enumerate() {
                *button |= u8::from(buttons & (1 << i) != 0);
            }
        }
        if let Some((_, [x, y])) = options.stick.iter().rfind(|(at, _)| *at <= frames) {
            axes[2] = (axes[2] + x).clamp(-1.0, 1.0);
            axes[3] = (axes[3] - y).clamp(-1.0, 1.0);
        }
        if focused != app_input.focused() {
            requests.borrow_mut().extend(app_input.set_focused(focused));
            if !focused {
                ui.cancel_pointer();
                pointer.hands.release();
            }
        }
        let settings_chord = app_input.pad(&pressed);
        // The panel: left menu (or Back+Start) toggles it; while it shows,
        // the pad is idle.
        if (input.hands[0].menu && !menu_held) || settings_chord {
            let visible = !*panel_visible.borrow();
            *panel_visible.borrow_mut() = visible;
        }
        menu_held = input.hands[0].menu;
        // Agents' commands (`--control`), between the game's frames.
        if let Some(control) = &control {
            control.poll(|command| {
                if let Some(reply) = settings.borrow_mut().command(command) {
                    return reply;
                }
                match command.get("cmd").and_then(serde_json::Value::as_str) {
                    Some("panel") => {
                        if let Some(visible) = command.get("visible").and_then(serde_json::Value::as_bool) {
                            *panel_visible.borrow_mut() = visible;
                        }
                        serde_json::json!({ "visible": *panel_visible.borrow() })
                    }
                    // As the app's Toggle first-person view, or on, off or
                    // turned; this frame's update gives the session its state.
                    Some("first_person") => {
                        let renderer = settings.borrow().settings().graphics.renderer;
                        xem_settings::look::Command::parse(command)
                            .and_then(|command| first_person(&mut view, &command, game.pad_context().field, renderer))
                            .unwrap_or_else(|error| serde_json::json!({ "error": error }))
                    }
                    // As the panel's Import BIOS once its picker chose a
                    // file, a failure in the error bar too.
                    Some("bios_import") => match command.get("path").and_then(serde_json::Value::as_str) {
                        Some(path) => match operations.import_bios_file(Some(&mut game), path) {
                            Ok(message) => {
                                ui.clear_operation_error();
                                serde_json::json!({ "ok": true, "message": message })
                            }
                            Err(error) => {
                                ui.set_operation_error(&error);
                                serde_json::json!({ "error": error })
                            }
                        },
                        None => serde_json::json!({ "error": "bios_import needs path" }),
                    },
                    // The panel's and the bindings' operations, as theirs
                    // run (Recenter, a held rewind, Retry pending card
                    // writes...).
                    Some("app") => crate::app_reply(crate::agent_request(command).and_then(|request| {
                        operate(request, &mut operations, &mut game, &settings, &mut assist, head, &mut view, &mut recenters, &mut hud, start.elapsed(), &ui)
                    })),
                    _ => game.command(command).unwrap_or_else(|error| serde_json::json!({ "error": error })),
                }
            });
        }
        // The assisted controls: gestures aimed at the screen as last shown
        // (prayer hands toggle the panel; a pinch clicks on it).
        let mut assisted_input = assisted::input(&input, head, start.elapsed().as_secs_f64(), focused, &mut pinched);
        if !options.hands.is_empty() {
            for (_, entry) in options.hands.iter().filter(|(at, _)| *at == frames) {
                // `name@x/y` also aims eye gaze at that screen pixel (the
                // scripted gaze stands in for eye tracking from then on);
                // `name@panel:x/y` at the settings panel's logical pixel,
                // `name@world:x/y/z` at that point of the game's world on the
                // diorama's table.
                let (name, aim) = entry.split_once('@').map_or((entry.as_str(), None), |(n, a)| (n, Some(a)));
                if let Some(aim) = aim {
                    scripted_aim = Some(ScriptedAim::parse(aim).expect("an aim: x/y, panel:x/y or world:x/y/z"));
                }
                scripted.extend(xem_assist::synthetic::gesture(name).expect("a known gesture"));
            }
            if let (Some(aim), Some(screen), Some(head)) = (scripted_aim, last_screen, head) {
                let target = match aim {
                    ScriptedAim::Panel(pixel) => Some(panel_point(&panel, pixel)),
                    ScriptedAim::Screen(at) => Some(screen.point(at)),
                    // Where the diorama shows that point (none while it does not).
                    ScriptedAim::World(point) => screen.diorama.map(|space| space.room_from_world.transform_point3(point)),
                };
                if let Some(target) = target {
                    let direction = (target - head.position).normalize();
                    assisted_input.gaze = Some(xem_assist::headset::Pose { position: head.position, orientation: Quat::from_rotation_arc(Vec3::NEG_Z, direction) });
                }
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
            requests.borrow_mut().append(&mut assist.requests);
            *panel_visible.borrow_mut() = open;
        }
        // A scaling of the diorama that ended (the hands let go, or the
        // focus or the reference was lost) becomes the player's setting.
        commit_scale(&settings, &mut assist.scaling);
        let visible = *panel_visible.borrow();
        // However it closed (the menu button or Back+Start, prayer hands,
        // its Close, an agent's `panel`), the panel lets go of what its
        // pointer held, whose release it no longer sees (Hold to rewind
        // under a controller's trigger), and of its quit confirmation, as
        // the window's does.
        if panel_shown && !visible {
            ui.cancel_pointer();
            ui.cancel_dialog();
            pointer.hands.release();
        }
        panel_shown = visible;
        if visible {
            if input.hands[1].active && assisted_input.hands.iter().all(Option::is_none) {
                pointer.update(&ui, &panel, panel_px, &input.hands[1]);
            } else {
                // Hands tracked (or no controller): the pointer source's ray,
                // pressed by a pinch for as long as it holds.
                let quad = xem_assist::headset::Quad { pose: xem_assist::headset::Pose { position: panel.pose.position, orientation: panel.pose.orientation }, size: panel.size };
                let began = assist.gestures.iter().find_map(|g| match g {
                    xem_assist::gesture::Gesture::Pinch { hand } => Some(hand.index()),
                    _ => None,
                });
                pointer.hand(&ui, panel_px, assist.headset.hit(&assisted_input, &quad).map(|(uv, _)| uv), began, assisted_input.pinch);
            }
        }

        if options.recenter_at.contains(&frames) {
            requests.borrow_mut().push(AppRequest::Action(AppAction::Recenter));
        }
        // The app's operations: the panel's, then the bound chords' (the
        // bindings stay live while the panel shows, as in the window).
        {
            let current = settings.borrow();
            let current = current.settings();
            requests.borrow_mut().extend(app_input.requests(&current.app_bindings, current.invert_trigger_chords));
        }
        for request in std::mem::take(&mut *requests.borrow_mut()) {
            // Toggle first-person view is the view's: its toggles below carry
            // it out.
            if request == AppRequest::Action(AppAction::ToggleFirstPerson) {
                look_asked = true;
                continue;
            }
            let _ = operate(request, &mut operations, &mut game, &settings, &mut assist, head, &mut view, &mut recenters, &mut hud, start.elapsed(), &ui);
        }
        let (fast_forward,status) = game.time_status();
        if assist.headset.pauses.settings && game.session.debug.busy() {
            if let Err(error) = game.session.tick_debug() { ui.set_operation_error(&error); }
            game.pause();
        }
        ui.set_debug_status(true, game.session.debug.busy(), &game.session.debug.message);
        ui.set_time_status(Some(fast_forward),&status);
        ui.set_storage_status(&game.storage_status());

        // The game: whatever frames are due by the wall clock, once per
        // headset frame (never per eye), unless paused for a test or by the
        // player (the Pause grab, lost tracking) or by settings' own pause,
        // which a held rewind goes on through (the panel's Hold to rewind,
        // a chord while the panel shows).
        let pauses = &assist.headset.pauses;
        let pause = options.pause.is_some_and(|(at, count)| frames >= at && frames < at + count)
            || pauses.player || (pauses.settings && !game.rewind_held());
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
        // The Binaural listener turns with the head as the seat sees it,
        // paused or not (lost tracking keeps the last turn); the game never
        // sees it.
        if let Some(head) = head {
            game.set_head(Some(xem_core::binaural::head_axes(assist.seat.relative(head.orientation).to_array())));
        }
        // First-person inspection, paused or not: the controllers' Back (the
        // right stick's click) released alone toggles it in a field, as a
        // gamepad's does, where Ex Machina draws it, Toggle first-person
        // view's chord (Back with the right trigger) anywhere in one but its
        // menu, and the right stick turns it in steps (the head does the
        // looking); none while the panel has the controllers.
        let context = game.pad_context();
        let drawn = current.graphics.renderer == xem_settings::Renderer::ExMachina;
        let asked = std::mem::take(&mut look_asked);
        if visible {
            look_toggle.clear();
            snap.clear();
        } else {
            if look_toggle.update(false, &pressed, context.field) && drawn {
                view.set_first_person(!view.first_person());
            } else if asked {
                // Refused (nothing changes) where the grab toggles nothing.
                let _ = first_person(&mut view, &xem_settings::look::Command::TOGGLE, context.field, current.graphics.renderer);
            }
            if xem_settings::look::turns(view.first_person(), context.field) {
                view.look_around(snap.update(axes[2]), 0.0);
            }
        }
        let mut ran = 0;
        if !game_paused {
            if visible { player.clear() } else { player.gamepad(&app_input.gamepad_for_game(&current.app_bindings, current.invert_trigger_chords), &axes) }
            // Inspection keeps the character where it stands
            // (`xem_settings::look`); running by default would leave Run
            // pressed alone.
            let inspecting = view.first_person();
            player.run_by_default = current.run_by_default && !inspecting;
            let buttons = if visible { 0 } else { player.buttons(&current.game_bindings, start.elapsed().as_secs_f64() * 1000.0, context) };
            let buttons = xem_settings::look::game_buttons(buttons, inspecting, context.field, &context.assignment);
            game.want_inspection(inspecting);
            ran = game.update(buttons, &current, || focused)?;
            game.follow_sound_mode(&settings);
            player.frames_ran(ran);
        }
        let (forward,rewind) = game.time_activity();
        hud.set_activity(forward,rewind);
        hud.frame(start.elapsed(),PANEL_SCALE,current.presentation.show_fps).map_err(|e| e.to_string())?;
        // The virtual screen fills about half of a view's height. A rebuilt
        // movie frame is made as wide as the screen spans in an eye (its
        // width scaled by two-hand scaling, over its distance).
        let mut config = ViewConfig::resolve(&current.graphics, HostClass::Headset, (session.eye_size().0 / 2, session.eye_size().1 / 2), view.max_scale());
        let xr = current.graphics.xr;
        let fov = headset_eye(&eyes[0], &assist.seat).fov;
        let spanned = headset_movie_output(f32::from(xr.width_cm) / 100.0 * assist.scaling.scale, f32::from(xr.distance_cm) / 100.0, session.eye_size().0, fov);
        config.movies = config.movies.map(|_| spanned);
        view.configure(config);
        // The screen or the diorama (never under the Original renderer; at
        // the hands' scale while they scale it), before the frame is taken:
        // the diorama asks the trace for every face and the camera.
        view.set_xr_view(assist.scaling.shown(xr_view(&current.graphics)));
        // The room behind the diorama, where the session can show it and the
        // player turned it on (the panel's row shows only then). A runtime
        // refusing it leaves it out for the rest of the session, the row
        // with it; the session goes on.
        let wanted = current.graphics.xr.passthrough && matches!(view.xr_view(), XrView::Diorama(_));
        let room = match session.show_room(wanted) {
            xem_xr::Room::Shown(_) => true,
            xem_xr::Room::Hidden => false,
            xem_xr::Room::Refused(error) => {
                println!("xr room: refused ({error}): passthrough=unavailable for the session");
                ui.set_passthrough_offered(false);
                false
            }
        };
        // Where the room shows, it stands in for the game's clear behind the
        // table.
        view.set_room(room);
        let restart = view.take_restart();
        // The Original renderer shows the display as the device holds it
        // (nothing to look around in).
        let original = current.graphics.renderer == xem_settings::Renderer::Original;
        if original {
            view.set_first_person(false);
        }
        game.want_actor_view(if !original && current.graphics.actors_in_view { view.actor_view() } else { 0 });
        // The Binaural listener looks where inspection looks, or from the
        // seat through the diorama's table, paused or not; the head turns it
        // once more (`set_head` above).
        game.set_listener_view(view.listener_axes());
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
        // Inspection looks from within the scene through the eyes (held
        // through frames the runtime cannot locate them in), every headset
        // frame, paused or not; the screen's picture is then the interface's.
        let looking: Vec<HeadsetEye> = tracked_eyes.filter(|_| view.first_person()).map_or_else(Vec::new, |eyes| eyes.to_vec());
        let mut inside: Vec<wgpu::TextureView> = Vec::new();
        // The diorama, unless inspection looks from within the scene: each
        // eye's picture of it, the screen while it eases between the two,
        // and the pane.
        let diorama_view = from_view && matches!(view.xr_view(), XrView::Diorama(_)) && !view.first_person();
        let mut shown: Option<DioramaShown> = None;
        // The presentation's own time on this thread (the frame log's).
        let presenting = Instant::now();
        let now = start.elapsed().as_secs_f64();
        // A movie frame rebuilt from what the MDEC decoded (Enhanced movies).
        let mut movie_rebuilt = false;
        let picture = if diorama_view {
            let size = session.eye_size();
            let diorama_eyes = [0, 1].map(|eye| DioramaEye { view: eyes[eye].view, proj: eyes[eye].proj, size });
            shown = view.present_diorama(&mut encoder, &display, &scene, now, &diorama_eyes, assist.seat.pose).map(|p| {
                // (The screen a diorama gives way to may show one.)
                movie_rebuilt = p.frame.as_ref().is_some_and(|frame| frame.movie);
                DioramaShown {
                    share: p.screen,
                    frame: p.frame.map(|frame| (frame.view.clone(), frame.size)),
                    eyes: p.eyes.into_iter().cloned().collect(),
                    opacity: p.opacity,
                    pane: p.pane.cloned(),
                    space: p.space,
                }
            });
            shown.as_ref().and_then(|s| s.frame.as_ref().map(|(_, size)| *size)).or(shown.as_ref().map(|_| presented_size))
        } else if from_view {
            view.present_headset(&mut encoder, &display, &scene, now, &looking).map(|p| {
                inside = p.eyes.iter().map(|&eye| eye.clone()).collect();
                movie_rebuilt = p.screen.movie;
                p.screen.size
            })
        } else {
            // The device's own picture (the Original renderer, 24-bit movie
            // frames): pointing goes through the game's camera, and no
            // diorama lies anywhere meanwhile.
            view.away();
            let (width, height, rgba) = game.display();
            movie.set(&gpu, (width, height), &rgba);
            (width > 0 && height > 0).then_some((width, height))
        };
        gpu.queue.submit([encoder.finish()]);
        let present_us = presenting.elapsed().as_micros();
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

        // The virtual screen in front of the seat: its size (the setting's,
        // scaled by two-hand scaling) and the display pixels it spans (a
        // rebuilt movie frame without wider framing's sides). While the
        // diorama shows, gestures aim at its pane and through its table.
        let width = f32::from(xr.width_cm) / 100.0 * assist.scaling.scale;
        let height = width * presented_size.1 as f32 / presented_size.0 as f32;
        let pose = screen_pose(&assist.seat, f32::from(xr.distance_cm) / 100.0);
        let side = if from_view && !movie_rebuilt { view.config().map_or(0, |c| c.side) as f32 } else { 0.0 };
        let space = shown.as_ref().and_then(|s| s.space);
        let screen = match space {
            Some(space) => xem_assist::headset::Screen::pane(space, assist.seat.position()),
            None => xem_assist::headset::Screen::flat(
                xem_assist::headset::Quad { pose: xem_assist::headset::Pose { position: pose.w_axis.truncate(), orientation: assist.seat.orientation() }, size: (width, height) },
                if from_view { (display.width as f32, display.height as f32) } else { (presented_size.0 as f32, presented_size.1 as f32) },
                side,
            ),
        };
        last_screen = Some(screen);
        let style = Assisted::style(&current);
        let placed = assist.placed(&current, &assisted_input, &screen, &mut game, view.viewpoint());
        // The keys beside the screen (the frame log's): how many, the widest
        // and narrowest (metres), and the nearest and farthest a key's inner
        // edge stands from the screen's edge beside it.
        let keys = {
            let inverse = screen.quad.pose.orientation.inverse();
            let (widths, gaps): (Vec<f32>, Vec<f32>) = placed.iter().filter(|p| matches!(p.widget, xem_render::glass::Widget::Key(..))).map(|p| {
                let local = inverse * (p.pose.w_axis.truncate() - screen.quad.pose.position);
                (p.size.x, local.x.abs() - screen.quad.size.0 / 2.0 - p.size.x / 2.0)
            }).unzip();
            let range = |v: &[f32]| format!("{:.4}-{:.4}", v.iter().copied().fold(f32::MAX, f32::min), v.iter().copied().fold(f32::MIN, f32::max));
            if widths.is_empty() { "-".to_string() } else { format!("{}:{}:{}", widths.len(), range(&widths), range(&gaps)) }
        };
        let marks: Vec<(wgpu::TextureView, Mat4, Vec2)> = placed.into_iter()
            .map(|placed| (assist.texture(&gpu, &placed.widget, style), placed.pose, placed.size))
            .collect();

        if should_render {
            // The HUD inside the screen's corner, or the pane's (as large seen
            // from the seat as on the default screen).
            let hud_quad = hud.visible().then(|| {
                let (placement, size) = match &space {
                    Some(space) => xem_assist::draw::pane_hud(space, hud.size(), PANEL_SCALE),
                    None => xem_assist::draw::hud(pose, Vec2::new(width, height), hud.size(), PANEL_SCALE),
                };
                (hud.view().clone(),placement,size)
            });
            let filter = match current.presentation.filter {
                Filter::Nearest => wgpu::FilterMode::Nearest,
                Filter::Linear => wgpu::FilterMode::Linear,
            };
            // The screen's picture: the diorama's while the screen shares the
            // view with it, else the frame presented (or the movie's).
            let (texture, share) = if diorama_view {
                match &shown {
                    Some(shown) => (shown.frame.as_ref().filter(|_| shown.share > 0.0).map(|(view, _)| view.clone()), shown.share),
                    None => (None, 1.0),
                }
            } else {
                let texture = match (picture, from_view) {
                    (None, _) => None,
                    (Some(_), true) => view.presented_texture().map(|t| t.create_view(&Default::default())),
                    (Some(_), false) => movie.texture.as_ref().map(|t| t.create_view(&Default::default())),
                };
                (texture, 1.0)
            };
            // The diorama's picture of each eye (at its opacity) and its pane,
            // while it shows.
            let (dioramas, diorama_opacity) = match shown.as_ref().filter(|_| space.is_some()) {
                Some(shown) => (shown.eyes.as_slice(), shown.opacity),
                None => (&[][..], 0.0),
            };
            let pane = shown.as_ref().and_then(|s| s.pane.clone()).zip(space);
            // Capture once, in the run's last third, facing the screen (or
            // at the headset frames asked for).
            let late = options.exit_after_frames.is_some_and(|n| frames * 3 >= n * 2)
                || options.exit_at_frame.is_some_and(|at| game.frames_run() * 3 >= at * 2);
            let facing = head.is_some_and(|head| (assist.seat.relative(head.orientation) * Vec3::NEG_Z).dot(Vec3::NEG_Z) > 0.95);
            let capture = match options.capture_at.is_empty() {
                true => options.capture.clone().filter(|_| late && facing && !captured),
                false => options.capture.as_ref().filter(|_| options.capture_at.contains(&frames)).map(|dir| dir.join(frames.to_string())),
            };
            let eye_size = session.eye_size();
            let mut readback = None;
            let mut pictures = Vec::new();
            session
                .render_eyes(|encoder, target| {
                    let quads: [Vec<Quad>; 2] = [0, 1].map(|eye| {
                        let view_proj: Mat4 = eyes[eye].proj * eyes[eye].view;
                        let whole = Rect { x: 0.0, y: 0.0, width: eye_size.0 as f32, height: eye_size.1 as f32 };
                        // Looking from within the scene: the eye's picture over its whole view.
                        let within = inside.get(eye).map(|picture| {
                            let mut quad = Quad::screen(picture, whole, eye_size);
                            quad.srgb_encoded = true;
                            quad.opaque = true;
                            quad.filter = filter;
                            quad
                        });
                        let screen = texture.as_ref().map(|screen| {
                            // The screen, or beside the eyes' pictures the
                            // interface's pane where it was.
                            let mut quad = Quad::world(screen, view_proj, pose, Vec2::new(width, height));
                            // The renderer's frames hold sRGB-encoded values in a linear
                            // format; movie frames are an sRGB texture. The game's
                            // picture covers the room; the pane covers by its alpha.
                            quad.srgb_encoded = from_view;
                            quad.opaque = inside.is_empty();
                            quad.opacity = share;
                            quad.filter = filter;
                            // Movie frames the device shows itself stay as the
                            // decoder made them, each pixel a block.
                            if !from_view && !original {
                                quad.filter = wgpu::FilterMode::Nearest;
                            }
                            quad
                        });
                        // The diorama over the screen it eases with (its
                        // picture premultiplied, faded and dimmed), then its
                        // pane at the diorama's share.
                        let diorama = dioramas.get(eye).map(|picture| {
                            let mut quad = Quad::screen(picture, whole, eye_size);
                            quad.srgb_encoded = true;
                            quad.filter = wgpu::FilterMode::Nearest;
                            quad.opacity = diorama_opacity;
                            quad
                        });
                        let pane = pane.as_ref().map(|(pane, space)| {
                            let mut quad = Quad::world(pane, view_proj, space.pane, space.pane_size);
                            quad.srgb_encoded = true;
                            quad.opacity = 1.0 - share;
                            quad.filter = filter;
                            quad
                        });
                        let glass = marks.iter().chain(hud_quad.iter()).map(|(texture, pose, size)| {
                            let mut quad = Quad::world(texture, view_proj, *pose, *size);
                            quad.srgb_encoded = true;
                            quad
                        });
                        within.into_iter().chain(screen).chain(diorama).chain(pane).chain(glass).collect()
                    });
                    // The room around the screen (with passthrough the real
                    // one: nothing, the drawn colour premultiplied, its
                    // alpha what covers the room); the eyes' pictures cover it.
                    let clear = Some(match (inside.is_empty(), room) {
                        (false, _) => wgpu::Color::BLACK,
                        (true, true) => wgpu::Color::TRANSPARENT,
                        (true, false) => BACKDROP,
                    });
                    compositor.draw_targets(encoder, &[(target.views[0], clear, &quads[0]), (target.views[1], clear, &quads[1])]);
                    if capture.is_some() {
                        readback = Some(crate::readback::Readback::record(&gpu.device, encoder, target.texture));
                        // From within the scene, each eye's picture as the
                        // renderer drew it too (what the eyes should show);
                        // with the diorama, its pictures of each eye and the
                        // pane.
                        pictures = (0..inside.len()).filter_map(|eye| view.eye_texture(eye))
                            .chain(dioramas.iter().map(wgpu::TextureView::texture))
                            .chain(pane.as_ref().map(|(pane, _)| pane.texture()))
                            .map(|picture| crate::readback::Readback::record(&gpu.device, encoder, picture))
                            .collect();
                    }
                })
                .map_err(|e| e.to_string())?;
            if let (Some(dir), Some(readback)) = (capture, readback) {
                readback.save(&gpu.device, &[dir.join("left.png"), dir.join("right.png")])?;
                let names: &[&str] = if inside.is_empty() { &["left-diorama.png", "right-diorama.png", "pane.png"] } else { &["left-picture.png", "right-picture.png"] };
                for (picture, name) in pictures.into_iter().zip(names) {
                    picture.save(&gpu.device, &[dir.join(name)])?;
                }
                // Where the pane stands and how each eye sees the room
                // (column-major matrices: the pane's pose, metres; each eye's
                // projection times view), for checks of the eyes' pictures.
                if let Some((_, space)) = &pane {
                    let numbers = |m: Mat4| m.to_cols_array().map(|v| format!("{v:.6}")).join(",");
                    let [left, right] = [0, 1].map(|eye| numbers(eyes[eye].proj * eyes[eye].view));
                    println!("capture pane n={frames} pose={} size={:.4},{:.4} left={left} right={right}", numbers(space.pane), space.pane_size.x, space.pane_size.y);
                }
                println!("capture dir={}", dir.display());
                captured = true;
            }
        }
        let ended = session.end_frame(visible.then_some(panel)).map_err(|e| e.to_string())?;
        let head = head.map_or("-".into(), |p| format!("{:.5},{:.5},{:.5};{:.5},{:.5},{:.5},{:.5}", p.position.x, p.position.y, p.position.z, p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w));
        // First-person inspection: on, drawn from within the scene (the
        // eyes' pictures), and the view's turn (yaw, pitch; radians).
        let look = view.look_angles().map_or("-".into(), |(yaw, pitch)| format!("{yaw:.4},{pitch:.4}"));
        // Whose picture the screen shows (the renderer's, or the device's
        // own), its wider framing's sides (display pixels each side), and
        // the first eye's field of view's tangents (left, right, up, down).
        let fov = format!("{:.4},{:.4},{:.4},{:.4}", fov[0], fov[1], fov[2], fov[3]);
        // The seat: where it stands and its turn (radians, left positive).
        let seat = {
            let (at, forward) = (assist.seat.position(), assist.seat.orientation() * Vec3::NEG_Z);
            format!("{:.4},{:.4},{:.4};{:.4}", at.x, at.y, at.z, (-forward.x).atan2(-forward.z))
        };
        // What the HUD shows over the screen: fast-forward's and rewind's
        // icons and a state result.
        let chips: Vec<&str> = [("ff", hud.component().get_fast_forward()), ("rewind", hud.component().get_rewinding()), ("notice", hud.notice().is_some())]
            .into_iter().filter_map(|(chip, shown)| shown.then_some(chip)).collect();
        // Fast-forward's factor as the session runs it (a fixed rate the
        // host cannot reach is paced at the rate it does), 0 when off.
        let factor = if fast_forward { game.session.frame_factor().map_or("uncapped".to_string(), |f| f.to_string()) } else { "0".to_string() };
        // Where the settings panel stands (its middle, in LOCAL), and the
        // diorama's pane while it shows.
        let panel_at = format!("{:.3},{:.3},{:.3}", panel.pose.position.x, panel.pose.position.y, panel.pose.position.z);
        let pane_at = space.as_ref().map_or("-".into(), |space| format!("{:.3},{:.3},{:.3}", space.pane.w_axis.x, space.pane.w_axis.y, space.pane.w_axis.z));
        // The diorama's scale as shown (the hands' while they scale it; 0 for
        // the screen) and, while it shows, the table's metres per world unit,
        // the camera's target (world units) and where it stands in the room
        // (the table's anchor: metres), the world point the table's centre
        // shows (world units), and a key's width beside the screen or the
        // pane (metres).
        let diorama_scale = match view.xr_view() {
            XrView::Diorama(table) => table.scale,
            XrView::Screen => 0.0,
        };
        let point = |p: Vec3, digits: usize| format!("{:.digits$},{:.digits$},{:.digits$}", p.x, p.y, p.z);
        let (world_scale, target, anchor, centre) = match &space {
            Some(space) => {
                let target = Vec3::from(space.camera.target);
                let centre = space.world_from_room().transform_point3(space.room_from_table.transform_point3(Vec3::ZERO));
                (format!("{:.6e}", space.scale), point(target, 2), point(space.room_from_world.transform_point3(target), 5), point(centre, 1))
            }
            None => ("-".into(), "-".into(), "-".into(), "-".into()),
        };
        let key = xem_assist::draw::side_keys(&screen, 1)[0].1.size.0;
        let binaural = binaural_line(&mut game);
        // Where the field's controlled character stands (whole world units).
        let player = game.field().and_then(|field| field.player().map(|actor| actor.position))
            .map_or("-".into(), |p| format!("{:.0},{:.0},{:.0}", p.x, p.y, p.z));
        println!(
            "xrframe n={frames} wall_us={} pdt_ns={display_time} period_ns={period} render={} layers={} game_ran={ran} game_frames={} game_vblanks={} paused={} picture={} view={} movie={} side={:.0} fov={fov} head={head} gestures={} player_pause={} marks={} assist={} context={} player={player} fp={} within={} look={look} scale={:.4} present_us={present_us} presented_us={} xr_view={} diorama={} share={:.3} builds={} seat={seat} resets={resets} recenters={recenters} ff={} hud={} keys={keys} panel={panel_at} pane={pane_at} diorama_scale={diorama_scale:.4} world_scale={world_scale} target={target} anchor={anchor} table={centre} key={key:.5} passthrough={} binaural={binaural}",
            start.elapsed().as_micros(),
            u8::from(should_render),
            ended.layers.len(),
            game.frames_run(),
            game.vblanks(),
            u8::from(game_paused),
            picture.map_or("-".into(), |(w, h)| format!("{w}x{h}")),
            u8::from(from_view),
            u8::from(movie_rebuilt),
            screen.side,
            serde_json::to_string(&assist.gestures).unwrap_or_default(),
            u8::from(assist.headset.pauses.player),
            marks.len(),
            game.assist_state(),
            serde_json::to_string(&game.pad_context().ui).unwrap_or_default(),
            u8::from(view.first_person()),
            u8::from(!inside.is_empty()),
            assist.scaling.scale,
            // When the frame was presented (the time the view eases by).
            (now * 1e6) as u64,
            if matches!(view.xr_view(), XrView::Diorama(_)) { "diorama" } else { "screen" },
            u8::from(space.is_some()),
            shown.as_ref().map_or(1.0, |s| s.share),
            view.diorama_stats().builds,
            factor,
            if chips.is_empty() { "-".to_string() } else { chips.join(",") },
            u8::from(ended.passthrough.is_some()),
        );
        frames += 1;
        if let Some((at, path)) = options.save_state_at.as_ref().filter(|_| !state_saved)
            && game.frames_run() >= *at
        {
            println!("{}", crate::save_state_at(&mut game, path)?);
            state_saved = true;
            game.set_frame_limit(options.exit_at_frame);
        }
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
    // A scaling of the diorama under way as the session ended ends where it
    // reached.
    assist.scaling.end();
    commit_scale(&settings, &mut assist.scaling);
    // The game goes back to a flat host (Android's window): no head, and the
    // camera's own view until that host gives the listener another.
    game.set_head(None);
    game.set_listener_view(None);
    println!("exit frames={frames} game_frames={} memory_hash {} binaural={}", game.frames_run(), game.memory_hash(), binaural_line(&mut game));
    Ok(Ended { game, quit: quit.get() })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_right_controllers_stick_click_with_its_trigger_toggles_first_person_anywhere_in_a_field_but_its_menu() {
        // Toggle first-person view's default chord on a headset's
        // controllers (`gamepad`: no R3, the right stick's click is Back):
        // the stick's click, then the trigger with it.
        let bindings = xem_settings::Settings::default().app_bindings;
        let mut app = xem_settings::AppInput::default();
        let mut input = InputState::default();
        input.hands[1].thumbstick_click = true;
        app.pad(&gamepad(&input).0);
        assert!(app.requests(&bindings, false).is_empty(), "Back alone asks nothing");
        input.hands[1].trigger = 1.0;
        let (pressed, _) = gamepad(&input);
        assert_eq!((pressed[8], pressed[5], pressed[11]), (1, 1, 0), "Back and R1, no R3");
        app.pad(&pressed);
        assert_eq!(app.requests(&bindings, false), vec![AppRequest::Action(AppAction::ToggleFirstPerson)]);
        assert!(app.requests(&bindings, false).is_empty(), "held, once");
        assert_eq!(app.gamepad_for_game(&bindings, false), [0; 16], "the game has neither button meanwhile");
        // The view takes it as the grab would: in a dialogue's field (where
        // Select toggles nothing), not under its menu nor the Original renderer.
        let gpu = pollster::block_on(xem_render::Gpu::headless()).expect("a wgpu adapter");
        let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
        let toggle = xem_settings::look::Command::TOGGLE;
        let dialogue = xem_core::FieldControls { field: true, walking: false, menu: false, picture: false };
        let menu = xem_core::FieldControls { menu: true, ..dialogue };
        let exmachina = xem_settings::Renderer::ExMachina;
        assert!(first_person(&mut view, &toggle, dialogue, xem_settings::Renderer::Original).is_err());
        assert_eq!(first_person(&mut view, &toggle, dialogue, exmachina).unwrap(), serde_json::json!({"first_person": true, "look": {"yaw": 0.0, "pitch": 0.0}}));
        assert!(first_person(&mut view, &toggle, menu, exmachina).is_err() && view.first_person(), "the menu keeps it on");
        let turned = xem_settings::look::Command { yaw: Some(30.0), ..toggle };
        assert_eq!(first_person(&mut view, &turned, dialogue, exmachina).unwrap()["look"]["yaw"], 30.0);
        assert_eq!(first_person(&mut view, &toggle, dialogue, exmachina).unwrap()["first_person"], false);
    }
}
