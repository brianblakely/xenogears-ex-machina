//! What the SDL3 hosts (desktop, Android) share: the SDL window and its native
//! handles, the wgpu surface they present to (dropped and recreated when the
//! platform takes the native window away), and the view they present: the
//! game's display (or the test scene as its stand-in) under the Slint
//! settings panel, and the game's pacing and sound ([`Game`]). One wgpu
//! device and queue serve the scene, the compositor and Slint.

#[cfg(has_game_module)]
pub mod game;
#[cfg(not(has_game_module))]
#[path = "game_absent.rs"]
pub mod game;
pub mod sdl;
#[cfg(has_game_module)]
mod readback;
#[cfg(all(has_game_module, not(target_os = "macos")))]
pub mod xr_play;
#[cfg(all(has_game_module, not(target_os = "macos")))]
mod assisted;
mod operations;
pub mod achievements;
// A build without a game module plays in no headset: nothing takes xem-xr.
#[cfg(all(not(has_game_module), not(target_os = "macos")))]
use xem_xr as _;

pub use game::Game;
pub use operations::agent_request;

use sdl3_sys::everything::*;
use std::cell::{Cell, RefCell};
use std::rc::Rc;
use std::time::Instant;
use xem_render::ps1::{GameView, HostClass, ViewConfig};
use xem_render::subtitles::SubtitleLayer;
use xem_render::xem_gpu::DisplayInfo;
use xem_render::{
    Compositor, Gpu, Picture, Quad, Rect, SceneRenderer, ViewTarget, Viewport, demo_camera, present_rect,
    wgpu,
};
use xem_settings::{AppInput, AppRequest, Filter, SettingsService};
use xem_ui::{FrameRateUi, Key, LogicalPosition, PointerEventButton, SettingsUi, SharedGpu, WindowEvent};

/// The stand-in game picture's size (the original's typical display mode).
pub const GAME_SIZE: (u32, u32) = (320, 240);
const GAME_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8UnormSrgb;
/// The panel's size in logical pixels.
const PANEL_SIZE: (f32, f32) = (640.0, 560.0);

/// A finger's touch phase ([`View::finger`]).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Finger {
    Down,
    Moved,
    Up,
    Cancelled,
}

/// What the game's state says of the player's buttons ([`Game::pad_context`]).
pub use xem_settings::player::Context as PadContext;

/// `--control`: agents' JSON commands on stdin, one a line, each answered by
/// a line on stdout between the game's frames (in the desktop's window and
/// in a headset session, `xr_play::XrPlay::control`).
pub struct Control {
    lines: std::sync::mpsc::Receiver<String>,
}

impl Control {
    /// Reads stdin on a thread of its own until it ends.
    pub fn spawn() -> Control {
        let (send, lines) = std::sync::mpsc::channel();
        std::thread::spawn(move || {
            for line in std::io::stdin().lines() {
                let Ok(line) = line else { break };
                if send.send(line).is_err() {
                    break;
                }
            }
        });
        Control { lines }
    }

    /// Answers the commands that arrived since the last call, each with
    /// `answer`'s reply (one that is not JSON with the reason).
    pub fn poll(&self, mut answer: impl FnMut(&serde_json::Value) -> serde_json::Value) {
        use std::io::Write;
        while let Ok(line) = self.lines.try_recv() {
            if line.trim().is_empty() {
                continue;
            }
            let reply = match serde_json::from_str::<serde_json::Value>(&line) {
                Ok(command) => answer(&command),
                Err(error) => serde_json::json!({ "error": format!("not JSON: {error}") }),
            };
            let mut out = std::io::stdout().lock();
            let _ = writeln!(out, "{reply}");
            let _ = out.flush();
        }
    }
}

/// An app operation's outcome with the game's vertical blank and memory
/// hash just after it (a state saved or loaded is the RAM the hash says):
/// the line the Android host logs ([`View::on_operation`]) and a headset
/// session prints.
pub fn operation_line(request: &AppRequest, result: &Result<String, String>, game: Option<&mut Game>) -> String {
    let game = game.map_or_else(String::new, |game| format!(" game_vblank={} memory_hash {}", game.vblanks(), game.memory_hash()));
    match result {
        Ok(text) => format!("app {request:?}{game} ok: {text}"),
        Err(error) => format!("app {request:?}{game} error: {error}"),
    }
}

/// An agent's acknowledgement of an app operation (`app {request}`):
/// `{"ok": true, "message"}`, what it did, or `{"error"}`.
pub fn app_reply(result: Result<String, String>) -> serde_json::Value {
    match result {
        Ok(message) => serde_json::json!({ "ok": true, "message": message }),
        Err(error) => serde_json::json!({ "error": error }),
    }
}

/// `xem-desktop --save-state-at FRAME:PATH`, in the window and in a
/// headset: once the game has run FRAME frames (its frame limit holds it
/// there), its state written to `path` as Save state writes one; the line
/// the host prints (the game's frame and memory hash, which the state's
/// RAM is).
pub fn save_state_at(game: &mut Game, path: &std::path::Path) -> Result<String, String> {
    game.command(&serde_json::json!({"cmd": "state_save", "path": path})).map_err(|error| format!("--save-state-at {}: {error}", path.display()))?;
    Ok(format!("state saved frame {} memory_hash {} path {}", game.frames_run(), game.memory_hash(), path.display()))
}

/// A wgpu surface for `window`.
pub fn create_surface(
    instance: &wgpu::Instance,
    window: &sdl::Window,
) -> Result<wgpu::Surface<'static>, String> {
    // SAFETY: hosts drop the surface before the window, and on Android before
    // SDL releases the native window it was made from.
    unsafe {
        let target = wgpu::SurfaceTargetUnsafe::from_display_and_window(window, window)
            .map_err(|error| error.to_string())?;
        instance
            .create_surface_unsafe(target)
            .map_err(|error| error.to_string())
    }
}

/// Opens the host's one device with `backends`, able to present to `window`.
pub fn open_gpu(
    window: &sdl::Window,
    mut descriptor: wgpu::InstanceDescriptor,
    backends: Option<wgpu::Backends>,
) -> Result<(Gpu, wgpu::Surface<'static>), String> {
    if let Some(backends) = backends {
        descriptor.backends = backends;
    }
    let instance = wgpu::Instance::new(descriptor);
    let surface = create_surface(&instance, window)?;
    let gpu = pollster::block_on(Gpu::new(instance, Some(&surface))).map_err(|e| e.to_string())?;
    Ok((gpu, surface))
}

/// Configures `surface`, returning wgpu's error rather than raising it as
/// uncaptured (which ends the run): Android can take the native window away
/// between a check and the configuration, as when it pauses or recreates the
/// activity while the app starts (the release smoke's first start, after
/// #80's merge: "In Surface::configure: Invalid surface").
fn configure(gpu: &Gpu, surface: &wgpu::Surface, config: &wgpu::SurfaceConfiguration) -> Result<(), String> {
    let scope = gpu.device.push_error_scope(wgpu::ErrorFilter::Validation);
    surface.configure(&gpu.device, config);
    pollster::block_on(scope.pop()).map_or(Ok(()), |error| Err(error.to_string()))
}

/// The window's swapchain. The surface is absent while suspended: Android
/// takes the native window away in the background and may replace it (on a
/// configuration change, say) while the app runs; a surface that cannot be
/// configured is treated as gone.
pub struct Presenter {
    surface: Option<wgpu::Surface<'static>>,
    /// The native surface `surface` was made for.
    native: Option<usize>,
    pub config: wgpu::SurfaceConfiguration,
}

impl Presenter {
    /// Configures `surface` for `window` with an sRGB-encoded (non-sRGB view)
    /// format; `usage` adds to RENDER_ATTACHMENT.
    pub fn new(
        gpu: &Gpu,
        surface: wgpu::Surface<'static>,
        window: &sdl::Window,
        usage: wgpu::TextureUsages,
    ) -> Result<Self, String> {
        let capabilities = surface.get_capabilities(&gpu.adapter);
        if capabilities.formats.is_empty() {
            return Err("the surface has no formats (its native window is gone)".into());
        }
        if !capabilities.usages.contains(usage) {
            return Err(format!("this surface does not support {usage:?}"));
        }
        // The scene is lit in linear light in its own sRGB texture; the window
        // composites in sRGB-encoded values, as Slint blends the panel itself.
        let format = capabilities
            .formats
            .iter()
            .copied()
            .find(|format| !format.is_srgb())
            .unwrap_or(capabilities.formats[0]);
        let (width, height) = window.pixel_size();
        let config = wgpu::SurfaceConfiguration {
            usage: usage | wgpu::TextureUsages::RENDER_ATTACHMENT,
            format,
            width: width.max(1),
            height: height.max(1),
            present_mode: wgpu::PresentMode::Fifo,
            desired_maximum_frame_latency: 2,
            alpha_mode: capabilities.alpha_modes[0],
            color_space: wgpu::SurfaceColorSpace::Auto,
            view_formats: vec![],
        };
        configure(gpu, &surface, &config)?;
        Ok(Self {
            surface: Some(surface),
            native: window.native_surface(),
            config,
        })
    }

    pub fn format(&self) -> wgpu::TextureFormat {
        self.config.format
    }

    pub fn size(&self) -> (u32, u32) {
        (self.config.width, self.config.height)
    }

    pub fn is_suspended(&self) -> bool {
        self.surface.is_none()
    }

    /// Follows the window's pixel size; false while it has none. A surface
    /// whose native window is gone or replaced (an activity recreated while
    /// it runs) is dropped instead of reconfigured, as in [`Self::acquire`].
    pub fn resize(&mut self, gpu: &Gpu, window: &sdl::Window) -> bool {
        let (width, height) = window.pixel_size();
        if width == 0 || height == 0 {
            return false;
        }
        if (width, height) != self.size() {
            self.config.width = width;
            self.config.height = height;
            let gone = window.native_surface() != self.native
                || self
                    .surface
                    .as_ref()
                    .is_some_and(|surface| surface.get_capabilities(&gpu.adapter).formats.is_empty());
            if gone || self.surface.as_ref().is_some_and(|surface| configure(gpu, surface, &self.config).is_err()) {
                self.surface = None;
            }
        }
        true
    }

    /// Drops the surface (the native window is going away).
    pub fn suspend(&mut self) {
        self.surface = None;
    }

    /// Recreates the surface on the window's current native window, at its
    /// current size and the same format. On failure it stays suspended.
    pub fn resume(&mut self, gpu: &Gpu, window: &sdl::Window) -> Result<(), String> {
        self.surface = None;
        let native = window.native_surface();
        let surface = create_surface(&gpu.instance, window)?;
        if !surface
            .get_capabilities(&gpu.adapter)
            .formats
            .contains(&self.config.format)
        {
            return Err(format!(
                "the new surface cannot present {:?} (its native window is gone?)",
                self.config.format
            ));
        }
        let (width, height) = window.pixel_size();
        self.config.width = width.max(1);
        self.config.height = height.max(1);
        configure(gpu, &surface, &self.config)?;
        self.surface = Some(surface);
        self.native = native;
        Ok(())
    }

    /// The next frame to draw, or None if there is none to draw now. When
    /// the native window is gone or replaced the presenter suspends itself;
    /// the host resumes it once the window has one ([`Self::resume`]).
    pub fn acquire(
        &mut self,
        gpu: &Gpu,
        window: &sdl::Window,
    ) -> Result<Option<wgpu::SurfaceTexture>, String> {
        if window.native_surface() != self.native {
            self.surface = None;
        }
        let Some(surface) = &self.surface else {
            return Ok(None);
        };
        Ok(match surface.get_current_texture() {
            wgpu::CurrentSurfaceTexture::Success(frame) => Some(frame),
            wgpu::CurrentSurfaceTexture::Suboptimal(frame) => {
                drop(frame);
                if configure(gpu, surface, &self.config).is_err() {
                    self.surface = None;
                }
                None
            }
            wgpu::CurrentSurfaceTexture::Timeout | wgpu::CurrentSurfaceTexture::Occluded => None,
            wgpu::CurrentSurfaceTexture::Outdated => {
                self.resize(gpu, window);
                if self.surface.as_ref().is_some_and(|surface| configure(gpu, surface, &self.config).is_err()) {
                    self.surface = None;
                }
                None
            }
            wgpu::CurrentSurfaceTexture::Lost => {
                self.surface = None;
                None
            }
            wgpu::CurrentSurfaceTexture::Validation => {
                return Err("surface validation error".into());
            }
        })
    }
}

/// Scancodes pressed since the last frame poll, so a key pressed and released
/// between two slow frames (e.g. software rendering) is still observed once
/// before release.
#[derive(Default)]
struct KeyLatch([u64; 8]);

impl KeyLatch {
    fn set(&mut self, scancode: u32) {
        let i = scancode as usize;
        if i < self.0.len() * 64 {
            self.0[i / 64] |= 1u64 << (i % 64);
        }
    }

    fn contains(&self, scancode: u32) -> bool {
        let i = scancode as usize;
        i < self.0.len() * 64 && self.0[i / 64] & (1u64 << (i % 64)) != 0
    }

    fn clear(&mut self) {
        self.0 = [0; 8];
    }
}

/// What a host presents: the scene as the game image, placed by the
/// presentation settings, under the settings panel when it is visible.
pub struct View {
    scene: SceneRenderer,
    compositor: Compositor,
    game_view: wgpu::TextureView,
    target_size: (u32, u32),
    settings: Rc<RefCell<SettingsService>>,
    ui: SettingsUi,
    frame_rate: FrameRateUi,
    frame_rate_started: Instant,
    panel_visible: Rc<RefCell<bool>>,
    panel_was_visible: bool,
    /// The player confirmed the panel's Quit: the host ends its run.
    quit: Rc<Cell<bool>>,
    gamepads: Vec<*mut SDL_Gamepad>,
    /// The game's display as the device shows it (24-bit movie frames, and
    /// hosts that set a picture directly), instead of the test scene.
    game_image: Option<(wgpu::Texture, wgpu::TextureView, (u32, u32))>,
    /// The movie's subtitle line (the movie subtitles setting), drawn when
    /// it changes and shown over the picture.
    subtitles: SubtitleLayer<(xem_core::subtitles::Line, xem_core::locale::Language)>,
    /// The game drawn by the renderer: the trace is pushed into `renderer`
    /// as the game runs; `game_frame` is the display and scene to present.
    renderer: GameView,
    game_frame: Option<(DisplayInfo, String)>,
    /// A new trace arrived since the last presentation.
    fresh_frame: bool,
    /// The last presentation showed a movie frame rebuilt from what the
    /// MDEC decoded (the Enhanced movies setting).
    presented_movie: bool,
    host_class: HostClass,
    /// Where the game picture may go: the window's safe area, in pixels.
    safe_area: Option<Rect>,
    app_input: AppInput,
    app_requests: Rc<RefCell<Vec<AppRequest>>>,
    operations: operations::Operations,
    /// Where operations' outcomes are told ([`Self::on_operation`]).
    operation_log: Option<Box<dyn FnMut(&str)>>,
    /// Mouse wheel notches not yet turned into zoom steps (smooth wheels).
    wheel: f32,
    /// The game's mouse buttons and camera turn, and whether SDL's relative
    /// mouse mode is on (while the game has the input).
    player: xem_settings::player::PlayerInput,
    /// The interface the player's input reached last (the wheel's), its
    /// entries a pointer selects, and where the last frame showed the game.
    game_ui: xem_core::context::Context,
    targets: Option<xem_core::menu::Targets>,
    picture: Option<Picture>,
    /// The field as the picture showed it at the last frame while the
    /// player walked there (not inspecting), which mouse clicks point
    /// into, and the commands they gave since (walk there, go and talk to
    /// someone), carried out with the touches' (`View::assist`).
    walking: Option<xem_core::field::Field>,
    clicked: Vec<serde_json::Value>,
    /// Touch: the control deck (laid out for what the game plays), whether
    /// a finger has touched the game, what touches asked for (taps on the
    /// picture in target pixels, quantity keys), the marks over the
    /// picture, and the widgets' textures.
    deck: xem_assist::touch::Deck,
    touched: bool,
    taps: Vec<(f32, f32)>,
    quantity: Vec<xem_assist::overlay::QuantityKey>,
    marks: Vec<xem_assist::overlay::Mark>,
    /// Where assisted movement heads and the field camera as the game step
    /// read it: its mark is placed as each picture is presented, through the
    /// camera that picture was drawn with (`View::render`).
    heading: Option<(xem_core::field::V3, xem_core::field::Camera)>,
    atlas: xem_render::glass::Atlas,
    relative_mouse: bool,
    /// When a mouse last moved or pressed (touches aside), and whether the
    /// host hid its cursor over the game for it (`Settings::hides_cursor`).
    mouse_moved: Instant,
    cursor_hidden: bool,
    /// The gamepads' buttons as they are (before app chords take
    /// theirs), first-person inspection's toggle, the app's Toggle
    /// first-person view asked for since the last frame (its key, its chord,
    /// the deck's LOOK), the mouse motion that turns its view, when its
    /// view last turned, and Select hiding a dialogue's windows.
    pad: [u8; 16],
    axes: [f32; 4],
    look_toggle: xem_settings::look::Toggle,
    look_asked: bool,
    look_mouse: [f32; 2],
    looked: Option<Instant>,
    dialogue_hiding: xem_settings::look::DialogueHiding,
    /// The disc the game waits for that the drive's changer has not got.
    wanted_disc: Option<u8>,
    /// RetroAchievements, where the host enabled it ([`View::enable_achievements`]).
    achievements: Option<achievements::HostAchievements>,
    /// The game frames the last update ran ([`View::frames_ran`]).
    ran: u32,
    /// Android's on-screen keyboard is asked for (a panel text field has
    /// the focus).
    keyboard: bool,
    /// Key-down scancodes latched since the last app/game poll so quick taps
    /// are not dropped between slow frames.
    app_keys_latched: KeyLatch,
    game_keys_latched: KeyLatch,
}

impl View {
    /// A view presented into `format` targets of `target_size` pixels at
    /// `scale` pixels per logical UI pixel.
    pub fn new(
        gpu: &Gpu,
        format: wgpu::TextureFormat,
        target_size: (u32, u32),
        scale: f32,
        settings: Rc<RefCell<SettingsService>>,
        show_panel: bool,
    ) -> Result<Self, String> {
        let game_view = gpu
            .device
            .create_texture(&wgpu::TextureDescriptor {
                label: Some("game image"),
                size: wgpu::Extent3d {
                    width: GAME_SIZE.0,
                    height: GAME_SIZE.1,
                    depth_or_array_layers: 1,
                },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format: GAME_FORMAT,
                usage: wgpu::TextureUsages::RENDER_ATTACHMENT
                    | wgpu::TextureUsages::TEXTURE_BINDING,
                view_formats: &[],
            })
            .create_view(&Default::default());
        let shared = SharedGpu {
            instance: gpu.instance.clone(),
            device: gpu.device.clone(),
            queue: gpu.queue.clone(),
        };
        let ui = SettingsUi::new(
            shared.clone(),
            settings.clone(),
            panel_size(target_size, scale),
            scale,
        )
        .map_err(|error| error.to_string())?;
        let frame_rate = FrameRateUi::new(shared,scale).map_err(|error| error.to_string())?;
        ui.set_first_person(true);
        let panel_visible = Rc::new(RefCell::new(show_panel));
        ui.on_close({
            let visible = panel_visible.clone();
            move || *visible.borrow_mut() = false
        });
        let quit = Rc::new(Cell::new(false));
        ui.on_quit({
            let quit = quit.clone();
            move || quit.set(true)
        });
        let app_requests = Rc::new(RefCell::new(Vec::new()));
        ui.on_app_request({
            let requests = app_requests.clone();
            move |request| requests.borrow_mut().push(request)
        });
        Ok(Self {
            scene: SceneRenderer::new(&gpu.device, &gpu.queue, GAME_FORMAT),
            compositor: Compositor::new(&gpu.device, &gpu.queue, format),
            game_view,
            target_size,
            settings,
            ui,
            frame_rate,
            frame_rate_started: Instant::now(),
            panel_visible,
            panel_was_visible: false,
            quit,
            gamepads: Vec::new(),
            game_image: None,
            subtitles: SubtitleLayer::default(),
            renderer: GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter)),
            game_frame: None,
            fresh_frame: false,
            presented_movie: false,
            host_class: HostClass::Desktop,
            safe_area: None,
            app_input: AppInput::default(),
            app_requests,
            operations: operations::Operations::default(),
            operation_log: None,
            wheel: 0.0,
            player: Default::default(),
            game_ui: xem_core::context::Context::None,
            targets: None,
            picture: None,
            walking: None,
            clicked: Vec::new(),
            deck: Default::default(),
            touched: false,
            taps: Vec::new(),
            quantity: Vec::new(),
            marks: Vec::new(),
            heading: None,
            atlas: Default::default(),
            relative_mouse: false,
            mouse_moved: Instant::now(),
            cursor_hidden: false,
            pad: [0; 16],
            axes: [0.0; 4],
            look_toggle: Default::default(),
            look_asked: false,
            look_mouse: [0.0; 2],
            looked: None,
            dialogue_hiding: Default::default(),
            wanted_disc: None,
            achievements: None,
            ran: 0,
            keyboard: false,
            app_keys_latched: KeyLatch::default(),
            game_keys_latched: KeyLatch::default(),
        })
    }

    /// RetroAchievements for this host (desktop, Android): the account is
    /// kept in `directory` (beside the settings), the panel's Achievements
    /// section offers sign-in.
    pub fn enable_achievements(&mut self, directory: Option<&std::path::Path>) {
        let achievements = achievements::HostAchievements::new(directory);
        achievements.attach(&self.ui);
        self.achievements = Some(achievements);
    }

    /// The RetroAchievements client, for a headset loop that shares it.
    pub fn achievements_client(&self) -> Option<Rc<RefCell<xem_achievements::Achievements>>> {
        self.achievements.as_ref().map(achievements::HostAchievements::client)
    }

    /// An agent's RetroAchievements command (`--control`): `achievements`,
    /// `achievements_sign_in {username, password}`, `achievements_sign_out`.
    pub fn achievements_command(&mut self, command: &serde_json::Value) -> Option<serde_json::Value> {
        self.achievements.as_mut()?.command(command)
    }

    /// The mouse wheel over the game: down pulls the field camera back,
    /// up brings it in (graphics `zoom_out`, a step per notch); not while
    /// looking around in first person, which shows no pulled-back camera.
    fn zoom_wheel(&mut self, notches: f32) {
        if self.renderer.first_person() {
            return;
        }
        self.wheel += notches;
        let steps = self.wheel.trunc();
        if steps == 0.0 {
            return;
        }
        self.wheel -= steps;
        self.zoom(steps as i32);
    }

    fn zoom(&mut self, steps: i32) {
        if steps == 0 { return; }
        let graphics = self.settings.borrow().settings().graphics;
        let zoomed = graphics.zoomed(steps);
        if zoomed != graphics {
            let _ = self.settings.borrow_mut().apply(xem_settings::SettingChange::Graphics(zoomed));
        }
    }

    pub fn set_state_directory(&mut self, path: std::path::PathBuf) { self.operations.directory = Some(path); }

    /// `log` is told each app operation's outcome ([`operation_line`]): the
    /// Android host's logcat, where its smoke test reads them.
    pub fn on_operation(&mut self, log: impl FnMut(&str) + 'static) { self.operation_log = Some(Box::new(log)); }

    fn log_operation(&mut self, request: &AppRequest, result: &Result<String,String>, game: Option<&mut Game>) {
        if let Some(log) = &mut self.operation_log { log(&operation_line(request,result,game)); }
    }

    /// Import BIOS of a file an agent names (`xem-desktop --control`'s
    /// `bios_import {path}`), as the panel's picker gives one: checked, kept
    /// in the app's directory for every boot and given to `game` if one
    /// runs; a failure shows in the error bar as the panel's does.
    pub fn import_bios(&self, game: Option<&mut Game>, path: &str) -> Result<String,String> {
        let result = self.operations.import_bios_file(game,path);
        self.show_result(result.clone());
        result
    }

    /// An agent's app operation (`xem-desktop --control`'s `app {request}`,
    /// [`agent_request`]), carried out as the panel's and the bindings' are
    /// ([`Self::update_app`]): a failure in the error bar, a state's outcome
    /// on the HUD, and its line where operations are told.
    pub fn app_command(&mut self, mut game: Option<&mut Game>, request: AppRequest) -> Result<String,String> {
        let result = self.operations.execute(request,None,game.as_deref_mut(),&mut self.settings.borrow_mut());
        if let AppRequest::Action(action) = request { self.state_notice(action,&result); }
        self.log_operation(&request,&result,game);
        self.show_result(result.clone());
        result
    }

    /// Runs Settings requests and bound actions under the host's one game owner.
    /// Returns false while the platform file picker has paused play.
    pub fn update_app(&mut self, window: &sdl::Window, game: &mut Option<Game>) -> bool {
        self.poll_gamepad();
        if self.panel_was_visible && !self.panel_visible() { self.ui.cancel_pointer(); }
        self.panel_was_visible = self.panel_visible();
        // With the mouse camera on, the game holds the mouse (hidden,
        // unbounded motion) while it has the input.
        // It lets the mouse go while the game shows entries to point at (a
        // menu, a choice), and takes it again when walking resumes.
        let relative = !cfg!(target_os = "android") && !self.panel_visible() && self.app_input.focused()
            && game.as_mut().is_some_and(|game| game.mouse_camera_active(self.settings.borrow().settings()));
        if relative != self.relative_mouse {
            // SAFETY: the window is SDL's and live.
            unsafe { SDL_SetWindowRelativeMouseMode(window.raw, relative) };
            self.relative_mouse = relative;
            self.player.mouse.clear();
            self.look_mouse = [0.0; 2];
        }
        // A still mouse's cursor hides over the game (the relative mode
        // hides it itself) and shows again as it moves.
        let hidden = !relative && !self.panel_visible()
            && self.settings.borrow().settings().hides_cursor(self.mouse_moved.elapsed().as_secs_f64());
        if hidden != self.cursor_hidden {
            // SAFETY: SDL's video subsystem runs the window.
            unsafe { if hidden { SDL_HideCursor() } else { SDL_ShowCursor() } };
            self.cursor_hidden = hidden;
        }
        let was_busy = self.operations.busy;
        let completed = self.operations.poll(game.as_mut(),&mut self.settings.borrow_mut());
        if let Some((action,result)) = completed {
            self.state_notice(action,&result);
            self.log_operation(&AppRequest::Action(action),&result,game.as_mut());
            self.show_result(result);
        }
        let settings_owner = Rc::clone(&self.settings);
        let settings = settings_owner.borrow();
        let bindings = &settings.settings().app_bindings;
        let invert_triggers = self.settings.borrow().settings().invert_trigger_chords;
        let typing = self.panel_visible() && self.ui.text_editor_focused();
        // A binding listening for an input takes the keys, as a text field does.
        let editing = typing || self.panel_visible() && self.ui.listening();
        if editing {
            self.app_input.release_keyboard();
            self.app_keys_latched.clear();
        }
        // Android shows its on-screen keyboard while a panel text field has
        // the focus (RetroAchievements' sign-in, a number); the desktop
        // takes text whenever the panel shows (xem-desktop).
        if cfg!(target_os = "android") && typing != self.keyboard {
            window.set_text_input(typing);
            self.keyboard = typing;
        }
        let mut count = 0;
        let keys = unsafe { SDL_GetKeyboardState(&mut count) };
        let keys = if keys.is_null() { &[][..] } else { unsafe { std::slice::from_raw_parts(keys, count.max(0) as usize) } };
        if !editing {
            for binding in bindings.values() {
                for code in &binding.keyboard {
                    let sc = scancode(code).0 as u32;
                    let live = keys.get(sc as usize).copied().unwrap_or(false);
                    let latched = self.app_keys_latched.contains(sc);
                    if latched {
                        self.app_input.key(code, false);
                    }
                    self.app_input.key(code, live || latched);
                }
            }
        }
        self.app_keys_latched.clear();
        // A binding listening for an input: the buttons are no chord.
        if !(self.panel_visible() && self.ui.listening()) {
            self.app_requests.borrow_mut().extend(self.app_input.requests(&bindings,invert_triggers));
        }
        drop(settings);
        let requests = std::mem::take(&mut *self.app_requests.borrow_mut());
        for request in requests {
            // The view's own: the next frame's toggles carry it out.
            if request == AppRequest::Action(xem_settings::AppAction::ToggleFirstPerson) {
                self.look_asked = true;
                continue;
            }
            let result = self.operations.execute(request,Some(window),game.as_mut(),&mut self.settings.borrow_mut());
            if let AppRequest::Action(action) = request {
                if result.is_err() || !self.operations.busy { self.state_notice(action,&result); }
            }
            self.log_operation(&request,&result,game.as_mut());
            self.show_result(result);
        }
        if let Some(achievements) = &mut self.achievements {
            achievements.update(game.as_mut(), self.ran > 0, &self.ui, &mut self.frame_rate, self.frame_rate_started.elapsed());
        }
        if let Some(game) = game {
            let (forward,rewind) = game.time_activity();
            self.frame_rate.set_activity(forward,rewind);
            let settings = self.settings.borrow();
            let result = game.sync_time_settings(settings.settings());
            drop(settings);
            if let Err(error) = result { self.ui.set_operation_error(&error); }
            if !was_busy && self.operations.busy { game.pause(); }
            if was_busy && !self.operations.busy { game.resume(); }
            if self.panel_visible() {
                let (debug_busy, debug_message) = game.debug_status();
                self.ui.set_debug_status(true, debug_busy, debug_message);
                let (fast_forward,status) = game.time_status();
                self.ui.set_time_status(Some(fast_forward),&status);
                self.ui.set_storage_status(&game.storage_status());
                let japanese = game.japanese();
                self.ui.set_japanese_offered(japanese, self.settings.borrow().settings());
            }
            // A disc the game waits for and the changer has not got: ask for it.
            let wanted = game.wanted_disc();
            if wanted != self.wanted_disc {
                self.wanted_disc = wanted;
                if let Some(number) = wanted {
                    self.ui.ask_for_disc(number);
                    self.set_panel_visible(true);
                }
            }
        } else {
            self.frame_rate.set_activity(false,false);
            if self.panel_visible() {
                self.ui.set_debug_status(false, false, "Boot a game to use Debug.");
                self.ui.set_time_status(None,"Boot a game to use save states and time controls.");
            }
        }
        !self.operations.busy
    }

    /// The gamepads as they are: Back+Start toggles the panel, and the
    /// open panel takes the gamepad (`SettingsUi::gamepad`); what it holds
    /// as the panel closes stays the panel's until let go.
    fn poll_gamepad(&mut self) {
        (self.pad, self.axes) = standard_pad(&self.gamepads);
        if self.app_input.pad(&self.pad) {
            self.set_panel_visible(!self.panel_visible());
        }
        if self.panel_visible() && self.app_input.focused() {
            self.ui.gamepad(&self.pad, &self.axes, self.frame_rate_started.elapsed());
            if !self.panel_visible() { self.app_input.hold_pad(); }
        }
    }

    fn state_notice(&mut self, action: xem_settings::AppAction, result: &Result<String,String>) {
        use xem_settings::AppAction::*;
        let saved = match action { SaveState | ExportState => true, LoadState | ImportState => false, _ => return };
        if result.as_ref().is_ok_and(|text| text == "File operation canceled.") { return; }
        self.frame_rate.notify_state(saved,result.as_ref().err().map(String::as_str),self.frame_rate_started.elapsed());
    }

    /// An operation's outcome in the panel's error bar: a failure shows,
    /// a success clears an earlier one.
    fn show_result(&self, result: Result<String, String>) {
        match result {
            Ok(_) => self.ui.clear_operation_error(),
            Err(error) => self.ui.set_operation_error(&error),
        }
    }

    pub fn settings(&self) -> &Rc<RefCell<SettingsService>> {
        &self.settings
    }

    pub fn ui(&mut self) -> &mut SettingsUi {
        &mut self.ui
    }

    pub fn panel_visible(&self) -> bool {
        *self.panel_visible.borrow()
    }

    pub fn set_panel_visible(&mut self, visible: bool) {
        if visible {
            self.deck.release();
            self.game_keys_latched.clear();
        }
        if self.panel_visible() && !visible {
            self.ui.cancel_pointer();
            self.panel_was_visible = false;
            self.ui.cancel_dialog();
        }
        *self.panel_visible.borrow_mut() = visible;
    }

    /// Whether the player confirmed Quit on the panel.
    pub fn quit_requested(&self) -> bool {
        self.quit.get()
    }

    /// Follows a new target size or display scale.
    pub fn resize(&mut self, target_size: (u32, u32), scale: f32) {
        self.target_size = target_size;
        self.ui.resize(panel_size(target_size, scale), scale);
    }

    /// Where the panel sits in the target, in pixels.
    pub fn panel_rect(&self) -> Rect {
        let (width, height) = self.ui.size();
        Rect {
            x: (self.target_size.0.saturating_sub(width) / 2) as f32,
            y: (self.target_size.1.saturating_sub(height) / 2) as f32,
            width: width as f32,
            height: height as f32,
        }
    }

    /// An SDL window position as a logical position on the panel.
    /// SDL's window coordinates in target pixels.
    fn window_pixels(&self, window: &sdl::Window, x: f32, y: f32) -> (f32, f32) {
        let density = window.pixel_density();
        (x * density, y * density)
    }

    fn panel_position(&self, window: &sdl::Window, x: f32, y: f32) -> LogicalPosition {
        let rect = self.panel_rect();
        let density = window.pixel_density();
        let scale = self.ui.scale_factor();
        LogicalPosition::new(
            (x * density - rect.x) / scale,
            (y * density - rect.y) / scale,
        )
    }

    /// Routes input to the panel and toggles it with Escape or Android's Back
    /// key (which close an open confirmation first). The gamepad's
    /// Back+Start chord is polled in `update_app`.
    ///
    /// # Safety
    /// `event` must come from SDL_PollEvent.
    pub unsafe fn handle(&mut self, window: &sdl::Window, event: &SDL_Event) {
        let visible = self.panel_visible();
        let kind = SDL_EventType(unsafe { event.r#type });
        let mouse = match kind {
            SDL_EVENT_MOUSE_MOTION => Some(unsafe { event.motion }.which),
            SDL_EVENT_MOUSE_BUTTON_DOWN => Some(unsafe { event.button }.which),
            _ => None,
        };
        if mouse.is_some_and(|which| which != SDL_TOUCH_MOUSEID) {
            self.mouse_moved = Instant::now();
        }
        match kind {
            SDL_EVENT_KEY_DOWN | SDL_EVENT_KEY_UP => {
                let key = unsafe { event.key };
                if key.down
                    && !key.repeat
                    && (key.key == SDLK_ESCAPE || key.key == SDLK_AC_BACK)
                {
                    if !(visible && self.ui.cancel_dialog()) {
                        self.set_panel_visible(!visible);
                    }
                } else if visible && (self.ui.listening() || !key.down) && !key.repeat
                    && self.ui.listen_key(&key_code(key.scancode).unwrap_or_default(), key.down)
                {
                    // A binding listening for an input took it.
                } else if visible {
                    if key.down && !key.repeat && !self.ui.text_editor_focused() && !self.ui.listening() {
                        self.app_keys_latched.set(key.scancode.0 as u32);
                    }
                    for event in panel_key_events(&key, shortcut_key_held()) {
                        self.ui.dispatch(event);
                    }
                } else if key.down && !key.repeat {
                    let sc = key.scancode.0 as u32;
                    self.app_keys_latched.set(sc);
                    self.game_keys_latched.set(sc);
                }
            }
            // A binding listening for an input takes the keys, not their text.
            SDL_EVENT_TEXT_INPUT if visible && !self.ui.listening() => {
                let text = unsafe { std::ffi::CStr::from_ptr(event.text.text) }.to_string_lossy();
                for character in text.chars() {
                    let text: xem_ui::SharedString = character.to_string().into();
                    self.ui
                        .dispatch(WindowEvent::KeyPressed { text: text.clone() });
                    self.ui.dispatch(WindowEvent::KeyReleased { text });
                }
            }
            SDL_EVENT_MOUSE_MOTION if visible => {
                let motion = unsafe { event.motion };
                let position = self.panel_position(window, motion.x, motion.y);
                self.ui.dispatch(WindowEvent::PointerMoved { position });
            }
            SDL_EVENT_MOUSE_BUTTON_DOWN | SDL_EVENT_MOUSE_BUTTON_UP if visible => {
                let button = unsafe { event.button };
                let position = self.panel_position(window, button.x, button.y);
                if button.down {
                    self.ui.set_pointer_touch(button.which == SDL_TOUCH_MOUSEID);
                }
                let button_kind = match i32::from(button.button) {
                    SDL_BUTTON_LEFT => PointerEventButton::Left,
                    SDL_BUTTON_RIGHT => PointerEventButton::Right,
                    SDL_BUTTON_MIDDLE => PointerEventButton::Middle,
                    _ => PointerEventButton::Other,
                };
                self.ui.dispatch(if button.down {
                    WindowEvent::PointerPressed {
                        position,
                        button: button_kind,
                    }
                } else {
                    WindowEvent::PointerReleased {
                        position,
                        button: button_kind,
                    }
                });
                // A lifted finger leaves nothing hovered (a touch has no hover).
                if !button.down && button.which == SDL_TOUCH_MOUSEID {
                    self.ui.dispatch(WindowEvent::PointerExited);
                }
            }
            // A real mouse over the game (touches stay the game view's own):
            // it turns the field camera, or the first-person view.
            SDL_EVENT_MOUSE_MOTION if unsafe { event.motion }.which != SDL_TOUCH_MOUSEID => {
                let motion = unsafe { event.motion };
                if !self.relative_mouse {
                    let (x, y) = self.window_pixels(window, motion.x, motion.y);
                    self.player.hover(self.picture.and_then(|p| p.to_screen(x, y)));
                }
                if self.relative_mouse && self.renderer.first_person() {
                    let scale = self.settings.borrow().settings().mouse_scale();
                    self.look_mouse[0] += motion.xrel * scale;
                    self.look_mouse[1] += motion.yrel * scale;
                } else if self.relative_mouse {
                    self.player.mouse.moved(motion.xrel, self.settings.borrow().settings());
                }
            }
            SDL_EVENT_MOUSE_BUTTON_DOWN | SDL_EVENT_MOUSE_BUTTON_UP if unsafe { event.button }.which != SDL_TOUCH_MOUSEID => {
                let button = unsafe { event.button };
                let index = match i32::from(button.button) { SDL_BUTTON_LEFT => 0, SDL_BUTTON_RIGHT => 2, SDL_BUTTON_MIDDLE => 1, _ => -1 };
                let at = self.window_pixels(window, button.x, button.y);
                self.mouse_button(index, button.down, at);
            }
            SDL_EVENT_MOUSE_WHEEL if visible => {
                let wheel = unsafe { event.wheel };
                let position = self.panel_position(window, wheel.mouse_x, wheel.mouse_y);
                // SDL reports notches; Slint wants logical pixels.
                self.ui.dispatch(WindowEvent::PointerScrolled {
                    position,
                    delta_x: wheel.x * 40.0,
                    delta_y: wheel.y * 40.0,
                });
            }
            SDL_EVENT_MOUSE_WHEEL => {
                let wheel = unsafe { event.wheel };
                // Positive y scrolls up (away from the user) and positive x
                // to the right, unless flipped.
                let flip = if wheel.direction == SDL_MOUSEWHEEL_FLIPPED { -1.0 } else { 1.0 };
                let (up, right) = (wheel.y * flip, wheel.x * flip);
                // In a menu or a choice it steps the cursor; elsewhere it zooms.
                if up != 0.0 && !(self.app_input.focused() && self.player.wheel(-up, self.game_ui)) {
                    self.zoom_wheel(-up);
                }
                // Sideways it turns the camera, walking or on the world map.
                if right != 0.0 && self.app_input.focused() && !self.renderer.first_person() {
                    self.player.scroll_sideways(right, self.game_ui);
                }
            }
            // Touches on the game: the control deck, and taps on the picture.
            SDL_EVENT_FINGER_DOWN | SDL_EVENT_FINGER_MOTION | SDL_EVENT_FINGER_UP | SDL_EVENT_FINGER_CANCELED if !visible => {
                let finger = unsafe { event.tfinger };
                let phase = match kind {
                    SDL_EVENT_FINGER_DOWN => Finger::Down,
                    SDL_EVENT_FINGER_MOTION => Finger::Moved,
                    SDL_EVENT_FINGER_UP => Finger::Up,
                    _ => Finger::Cancelled,
                };
                self.finger(phase, finger.fingerID.0, (finger.x, finger.y));
            }
            SDL_EVENT_WINDOW_FOCUS_LOST => {
                self.ui.cancel_pointer();
                self.player.mouse.clear();
                self.app_keys_latched.clear();
                self.game_keys_latched.clear();
                self.app_requests.borrow_mut().extend(self.app_input.set_focused(false));
            }
            SDL_EVENT_WINDOW_FOCUS_GAINED => {
                let _ = self.app_input.set_focused(true);
            }
            SDL_EVENT_WINDOW_MOUSE_LEAVE if visible => self.ui.dispatch(WindowEvent::PointerExited),
            SDL_EVENT_GAMEPAD_ADDED => self.gamepad_added(unsafe { event.gdevice.which }),
            SDL_EVENT_GAMEPAD_REMOVED => self.gamepad_removed(unsafe { event.gdevice.which }),
            _ => {}
        }
    }

    /// A gamepad connected (again): it plays from the next frame, along
    /// with any others.
    pub fn gamepad_added(&mut self, id: SDL_JoystickID) {
        // SAFETY: SDL opens the device it just announced.
        let pad = unsafe { SDL_OpenGamepad(id) };
        if !pad.is_null() {
            self.gamepads.push(pad);
        }
    }

    /// A gamepad disconnected: what it held lets go at once, and the other
    /// gamepads (if any) play on.
    pub fn gamepad_removed(&mut self, id: SDL_JoystickID) {
        self.gamepads.retain(|&pad| {
            // SAFETY: the pads are SDL's open gamepads, each closed once.
            if unsafe { SDL_GetGamepadID(pad) } == id { unsafe { SDL_CloseGamepad(pad); } false } else { true }
        });
        (self.pad, self.axes) = standard_pad(&self.gamepads);
        self.app_input.pad(&self.pad);
    }

    /// The window's safe area (x, y, width, height in pixels): the game
    /// picture is placed inside it; the panel and counters may use all of
    /// the window.
    pub fn set_safe_area(&mut self, (x, y, width, height): (u32, u32, u32, u32)) {
        self.safe_area = Some(Rect { x: x as f32, y: y as f32, width: width as f32, height: height as f32 });
    }

    /// The kind of host (its automatic graphics choices: quality tier).
    pub fn set_host_class(&mut self, class: HostClass) {
        self.host_class = class;
    }

    /// Where the game picture may go: the safe area the host gave, when it
    /// lies within the target, else all of it.
    fn safe_rect(&self) -> Rect {
        let (width, height) = (self.target_size.0 as f32, self.target_size.1 as f32);
        self.safe_area
            .filter(|a| a.x + a.width <= width && a.y + a.height <= height && a.width >= 1.0 && a.height >= 1.0)
            .unwrap_or(Rect { x: 0.0, y: 0.0, width, height })
    }

    /// Whether the last frame showed a movie frame rebuilt from what the
    /// MDEC decoded (`GameView::presented_movie`), for status reports.
    pub fn presented_movie(&self) -> bool {
        self.presented_movie
    }

    /// Whether a movie's subtitle shows over the picture.
    pub fn subtitle_shown(&self) -> bool {
        self.subtitles.shown().is_some()
    }

    /// The render passes the game's picture last took (VRAM's picture as
    /// the game drew it and the frame replayed from it), for status reports.
    pub fn render_passes(&self) -> u32 {
        self.renderer.stats().passes + self.renderer.frame().passes()
    }

    /// Takes what `game` drew since the last call for the renderer (the
    /// graphics settings choose how it is drawn). A 24-bit display (movie
    /// frames) is shown as the device holds it.
    pub fn set_game_frame(&mut self, gpu: &Gpu, game: &mut Game) {
        let (forward,rewind) = game.time_activity();
        self.frame_rate.set_activity(forward,rewind);
        let line = self.settings.borrow().settings().movie_subtitles.then(|| game.subtitle()).flatten();
        self.subtitles.update(&gpu.device, &gpu.queue, line, |(line, language)| game.subtitle_picture(line, *language).map(|p| (p.width, p.height, p.rgba)));
        let graphics = self.settings.borrow().settings().graphics;
        if graphics.renderer == xem_settings::Renderer::Original {
            // The console's own picture, as the device shows it (nothing to
            // look around in).
            game.stop_trace();
            game.want_actor_view(0);
            game.set_listener_view(None);
            self.renderer.set_first_person(false);
            self.game_frame = None;
            let (width, height, rgba) = game.display();
            self.set_game_image(gpu, (width, height), &rgba);
            return;
        }
        let mut config = ViewConfig::resolve(&graphics, self.host_class, self.target_size, self.renderer.max_scale());
        // A rebuilt movie frame is made at the size it is shown: the display
        // placed in the safe area as the presentation settings place it.
        let display = game.display_info();
        let safe = self.safe_rect();
        let placed = present_rect((display.width, display.height), (safe.width as u32, safe.height as u32), self.settings.borrow().settings().presentation.scale, graphics.forced_aspect());
        config.movies = config.movies.map(|_| (placed.width.round() as u32, placed.height.round() as u32));
        self.renderer.configure(config);
        let restart = self.renderer.take_restart();
        game.want_actor_view(if graphics.actors_in_view { self.renderer.actor_view() } else { 0 });
        game.set_listener_view(self.renderer.listener_axes());
        let (trace, display, scene) = game.take_frame(restart, self.renderer.reveals());
        self.fresh_frame |= !trace.ops.is_empty();
        self.renderer.push(trace);
        if GameView::shows(&display) || self.renderer.shows_movie(&display) {
            self.game_frame = Some((display, scene));
        } else {
            self.game_frame = None;
            let (width, height, rgba) = game.display();
            self.set_game_image(gpu, (width, height), &rgba);
        }
    }

    /// Whether a presentation is due: at the display's rate, or while the
    /// presentation is game-paced (no interpolation) only when the game drew
    /// or the panel shows.
    pub fn wants_present(&self) -> bool {
        !self.settings.borrow().settings().graphics.game_paced()
            || self.fresh_frame
            || self.panel_visible()
            || (self.game_frame.is_none() && self.game_image.is_none())
    }

    /// Show the game's display: `rgba` is `width` x `height` RGBA8 pixels
    /// (sRGB-encoded, as the console outputs them).
    pub fn set_game_image(&mut self, gpu: &Gpu, (width, height): (u32, u32), rgba: &[u8]) {
        if width == 0 || height == 0 {
            return;
        }
        self.fresh_frame = true;
        if self.game_image.as_ref().is_none_or(|(_, _, size)| *size != (width, height)) {
            let texture = gpu.device.create_texture(&wgpu::TextureDescriptor {
                label: Some("game display"),
                size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format: GAME_FORMAT,
                usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
                view_formats: &[],
            });
            let view = texture.create_view(&Default::default());
            self.game_image = Some((texture, view, (width, height)));
        }
        let (texture, _, _) = self.game_image.as_ref().expect("created above");
        gpu.queue.write_texture(
            texture.as_image_copy(),
            rgba,
            wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(width * 4), rows_per_image: Some(height) },
            wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
        );
    }

    /// The PS1 pad buttons the keyboard, mouse and the gamepads hold,
    /// as the pad's data halfword has them (bit 0 Select .. bit 15 Square),
    /// unless the settings panel has the input (`context.combo`: the
    /// battle's attack inputs have the buttons, `xem_settings::bindings`).
    /// In a field they also toggle and turn first-person inspection, which
    /// holds back the buttons that would walk, jump or turn the camera
    /// (`xem_settings::look`), as the app's Toggle first-person view does;
    /// Select held over a dialogue hides its windows.
    pub fn pad_buttons(&mut self, context: PadContext, targets: Option<xem_core::menu::Targets>) -> u16 {
        self.game_ui = context.ui;
        self.targets = targets;
        if self.panel_visible() || !self.app_input.focused() {
            self.player.clear();
            self.game_keys_latched.clear();
            self.look_toggle.clear();
            self.look_asked = false;
            self.look_mouse = [0.0; 2];
            self.deck.take_drag();
            self.looked = None;
            self.dialogue_hiding.clear();
            self.renderer.set_dialogue_hidden(false);
            return 0;
        }
        let now = self.frame_rate_started.elapsed().as_secs_f64() * 1000.0;
        let settings_owner = Rc::clone(&self.settings);
        let settings = settings_owner.borrow();
        let bindings = &settings.settings().game_bindings;
        let app_bindings = &settings.settings().app_bindings;
        let mut count = 0;
        // SAFETY: SDL owns the array and keeps it valid while SDL runs.
        let keys = unsafe { SDL_GetKeyboardState(&mut count) };
        let keys = if keys.is_null() { &[][..] } else { unsafe { std::slice::from_raw_parts(keys, count.max(0) as usize) } };
        let latched = std::mem::take(&mut self.game_keys_latched);
        let held = |code: &str| {
            let sc = scancode(code).0 as u32;
            !self.app_input.captures_key(code, app_bindings)
                && (keys.get(sc as usize).copied().unwrap_or(false) || latched.contains(sc))
        };
        for code in bindings.keys() {
            self.player.key(bindings, code, held(code));
        }
        let invert_triggers = settings.settings().invert_trigger_chords;
        self.player.gamepad(&self.app_input.gamepad_for_game(app_bindings, invert_triggers), &self.axes);
        self.player.browse(self.targets.as_ref());
        // First-person inspection: toggled in a field (Ex Machina draws it)
        // by Select's keys, the touch deck's SELECT and the gamepad, and
        // anywhere in one but its menu by the app's Toggle first-person view
        // (an agent's bare `first_person`), and while it is on, turned by the
        // arrows, the right stick, a finger on the picture and the mouse the
        // game holds.
        let field = context.field;
        let select = bindings.play.get(&xem_settings::GameAction::Select).is_some_and(|b| b.keyboard.iter().any(|k| held(k)))
            || self.deck.held().contains(&xem_assist::touch::Control::Select);
        // Held over a dialogue, Select hides its windows (presentation only),
        // presented anew as it changes, paused or not.
        let hidden = self.dialogue_hiding.update(select, &self.pad, context.ui, now);
        self.fresh_frame |= hidden != self.renderer.dialogue_hidden();
        self.renderer.set_dialogue_hidden(hidden);
        let setting = settings.settings().graphics.renderer;
        let drawn = setting == xem_settings::Renderer::ExMachina;
        let asked = std::mem::take(&mut self.look_asked);
        let was = self.renderer.first_person();
        if self.look_toggle.update(select, &self.pad, field) && drawn {
            self.renderer.set_first_person(!was);
        } else if asked && let Ok(change) = xem_settings::look::Command::TOGGLE.change(was, self.renderer.look_angles(), field, setting) {
            // (Refused where the view cannot change, the menu or the
            // Original renderer: nothing happens, as for the grab.)
            self.renderer.set_first_person(change.on);
        }
        if self.renderer.first_person() != was {
            self.player.mouse.clear();
            self.look_mouse = [0.0; 2];
        }
        let on = self.renderer.first_person();
        let zoom = self.player.zoom_steps(now, !on);
        // Running by default would leave Run pressed alone once the view
        // holds the directions back.
        self.player.run_by_default = settings.settings().run_by_default && !on;
        let buttons = self.player.buttons(bindings, now, context);
        let now = Instant::now();
        let seconds = self.looked.map_or(0.0, |then| now.duration_since(then).as_secs_f32().min(0.1));
        self.looked = Some(now);
        // The mouse's and a finger's motion are the view's this frame, or
        // no one's.
        let mouse = std::mem::take(&mut self.look_mouse);
        let (dx, dy) = self.deck.take_drag();
        if xem_settings::look::turns(on, field) {
            let density = self.ui.scale_factor();
            let turn = xem_settings::look::Turn {
                arrows: ["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown"].map(held),
                stick: right_stick(&self.gamepads),
                mouse,
                touch: [dx / density, dy / density],
            };
            let (yaw, pitch) = turn.radians(seconds);
            self.renderer.look_around(yaw, pitch);
        }
        drop(settings);
        self.zoom(zoom);
        xem_settings::look::game_buttons(buttons, on, field, &context.assignment)
    }

    /// Whether first-person inspection shows (`Game::want_inspection`).
    pub fn first_person(&self) -> bool {
        self.renderer.first_person()
    }

    /// An agent's `first_person` command (`xem-desktop --control`) in
    /// `field` (`Game::pad_context`): the view on, off or turned as `command`
    /// says (`xem_settings::look`: the app's Toggle first-person view is the
    /// bare command), answering where it looks; the next update gives the
    /// session its state (`Game::want_inspection`).
    pub fn first_person_command(&mut self, command: &xem_settings::look::Command, field: xem_core::FieldControls) -> Result<serde_json::Value, String> {
        let renderer = self.settings.borrow().settings().graphics.renderer;
        let change = command.change(self.renderer.first_person(), self.renderer.look_angles(), field, renderer)?;
        if change.on != self.renderer.first_person() {
            // The game's mouse camera and the view's mouse motion let go.
            self.renderer.set_first_person(change.on);
            self.player.mouse.clear();
            self.look_mouse = [0.0; 2];
        }
        self.renderer.look_around(change.turn.0, change.turn.1);
        Ok(xem_settings::look::reply(self.renderer.first_person(), self.renderer.look_angles()))
    }

    /// A mouse button over the game going down or up (`index` 0 left, 1
    /// middle, 2 right; `at` in target pixels). A left click on an entry of
    /// the game's interface selects it and confirms it, and one on the
    /// picture while the player walks goes where a tap there would (to the
    /// person there, to talk, or to the floor:
    /// [`xem_assist::dispatch::click_at`]). Elsewhere, and while the game
    /// holds the mouse for its camera, it is the mouse's binding.
    pub fn mouse_button(&mut self, index: i32, down: bool, at: (f32, f32)) {
        let taken = index == 0 && down && !self.relative_mouse && {
            let screen = self.picture.and_then(|p| p.to_screen(at.0, at.1));
            self.player.hover(screen);
            self.player.click(self.targets.as_ref())
                || match screen.zip(self.walking.as_ref()).and_then(|(at, field)| xem_assist::dispatch::click_at(at, field)) {
                    Some(command) => {
                        self.clicked.push(command);
                        true
                    }
                    None => false,
                }
        };
        if !taken {
            self.player.mouse.button(index, down);
        }
    }

    /// A finger on the game (`at` across the target, 0..1).
    pub fn finger(&mut self, phase: Finger, id: u64, at: (f32, f32)) {
        let at = (at.0 * self.target_size.0 as f32, at.1 * self.target_size.1 as f32);
        let time = self.frame_rate_started.elapsed().as_secs_f64();
        self.touched = true;
        use xem_assist::touch::Touched;
        match phase {
            Finger::Down => match self.deck.down(id, at, time) {
                Touched::Settings => self.set_panel_visible(true),
                Touched::AutoAdvance => self.app_requests.borrow_mut().push(AppRequest::Action(xem_settings::AppAction::ToggleAutoAdvance)),
                Touched::FirstPerson => self.look_asked = true,
                Touched::Run => {
                    // Run by default, the setting every input shares.
                    let on = self.settings.borrow().settings().run_by_default;
                    let _ = self.settings.borrow_mut().apply(xem_settings::SettingChange::RunByDefault(!on));
                }
                Touched::Quantity(key) => self.quantity.push(key),
                _ => {}
            },
            Finger::Moved => self.deck.moved(id, at),
            Finger::Up | Finger::Cancelled => {
                if let Touched::Tap(at) = self.deck.up(id, at, time, phase == Finger::Cancelled) {
                    self.taps.push(at);
                }
            }
        }
        self.player.touch = self.deck.actions();
    }

    /// Lays the touch deck out by the picture last shown, for what the game
    /// plays (`scene`: its context, and whether Select shows a map's picture
    /// there) and whether first-person inspection shows (no stick then).
    fn arrange(&mut self, scene: impl Into<xem_assist::touch::Scene>) {
        if let Some(picture) = self.picture {
            let placed = xem_assist::touch::Rect { x: picture.placed.x, y: picture.placed.y, width: picture.placed.width, height: picture.placed.height };
            let density = self.ui.scale_factor();
            let scene = xem_assist::touch::Scene { inspecting: self.renderer.first_person(), ..scene.into() };
            self.deck.arrange((self.target_size.0 as f32, self.target_size.1 as f32), placed, density, scene);
        }
    }

    /// Touch play, before [`View::pad_buttons`]: the deck laid out by the
    /// picture, taps on the picture resolved in the game's context (walk or
    /// talk there, select a menu entry), the walks and talks mouse clicks
    /// gave ([`View::mouse_button`]), shop quantity keys, and the marks
    /// (the committed destination) to draw. First-person inspection shows
    /// no field camera to walk or point through: its taps and clicks walk
    /// nowhere and talk to no one, and no floor marks show.
    pub fn assist(&mut self, game: &mut Game) {
        let ui = game.context();
        // Whether Select shows the field map's picture there (the deck's
        // MAP), and the buttons the map's scripts test (an activity's).
        let (picture, tested) = game.deck_scene();
        self.arrange(xem_assist::touch::Scene { ui, picture, tested, inspecting: false });
        let taps = std::mem::take(&mut self.taps);
        let inspecting = self.renderer.first_person();
        // The field as the picture shows it: the camera it was drawn with,
        // seen from its viewpoint (pulled back, say).
        let read = if inspecting { None } else { game.field() };
        let camera = read.as_ref().map(|f| f.camera);
        let field = read.map(|f| xem_assist::overlay::seen(f, self.renderer.drawn_camera(), self.renderer.viewpoint().as_ref()));
        if !taps.is_empty() {
            let targets = game.targets();
            for tap in taps {
                let Some(at) = self.picture.and_then(|p| p.to_screen(tap.0, tap.1)) else { continue };
                // A tap on the paused game resumes it, as a click does.
                if self.player.unpause() { continue; }
                match xem_assist::dispatch::tap_at(xem_assist::dispatch::Pointed::Screen(at), ui, field.as_ref(), targets.as_ref()) {
                    Some(xem_assist::dispatch::Out::Command { command }) => {
                        let _ = game.command(&command);
                    }
                    Some(xem_assist::dispatch::Out::Select { index, confirm }) => self.player.select(index, confirm),
                    Some(xem_assist::dispatch::Out::Press { actions }) => self.player.pulse(xem_settings::player::Pulse::new(actions)),
                    _ => {}
                }
            }
        }
        for command in std::mem::take(&mut self.clicked) {
            let _ = game.command(&command);
        }
        for key in std::mem::take(&mut self.quantity) {
            let _ = game.command(&serde_json::json!({"cmd": "shop_quantity", "op": key.op()}));
        }
        let destination = if inspecting { None } else { game.destination(field.as_ref()) };
        self.heading = destination.zip(camera);
        self.walking = field.filter(|_| ui == xem_core::context::Context::Walk);
        self.player.touch = self.deck.actions();
    }

    /// The touch deck's controls and where they stand (target pixels).
    pub fn deck_controls(&self) -> Vec<(xem_assist::touch::Control, xem_assist::touch::Rect)> {
        self.deck.controls.clone()
    }

    /// The touch deck's stick: where it rests and its reach (target pixels).
    pub fn deck_stick(&self) -> Option<xem_assist::touch::Stick> {
        self.deck.stick
    }

    /// Whether the touch controls show: the setting, on Android, or once a
    /// finger has touched the game.
    fn touch_shown(&self) -> bool {
        self.settings.borrow().settings().assisted.touch_controls && (cfg!(target_os = "android") || self.touched)
    }

    /// The game ran `frames` frames with the last [`View::pad_buttons`].
    pub fn frames_ran(&mut self, frames: u32) {
        self.player.frames_ran(frames);
        self.ran = frames;
    }

    /// Whether the battle's attack sequence under way began with a pointer's
    /// press of its attacks (`Game::want_attack_page`).
    pub fn attack_pointed(&self) -> bool {
        self.player.attack_pointed()
    }

    /// Records one frame into `output` (a target of the view's size) at scene
    /// time `time` seconds and submits it.
    pub fn render(
        &mut self,
        gpu: &Gpu,
        output: &wgpu::TextureView,
        time: f32,
    ) -> Result<(), String> {
        let visible = self.panel_visible();
        if visible {
            self.ui.update().map_err(|error| error.to_string())?;
        }

        let panel_rect = self.panel_rect();
        let touch_shown = self.touch_shown();
        // The deck's LOOK is lit while the view shows.
        let first_person = self.renderer.first_person();
        self.ui.set_first_person_touch(touch_shown);
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        self.fresh_frame = false;
        // The picture goes inside the safe area (all of the target when the
        // host gave none).
        let safe = self.safe_rect();
        let side = self.renderer.config().map_or(0, |c| c.side) as f32;
        let presented = match &self.game_frame {
            Some((display, scene)) => self.renderer.present(&mut encoder, display, scene, f64::from(time)).map(|p| (p.view.clone(), p.size, p.movie)),
            // The device's own picture (the Original renderer, 24-bit movie
            // frames): taps point through the game's camera.
            None => {
                self.renderer.away();
                None
            }
        };
        let encoded = presented.is_some();
        // Where assisted movement heads, on the floor as this picture shows
        // it: through the camera it was drawn with.
        self.marks = self.heading.and_then(|(point, camera)| {
            let camera = xem_assist::overlay::shown(camera, self.renderer.drawn_camera(), self.renderer.viewpoint().as_ref());
            xem_assist::overlay::destination(&camera, point)
        }).into_iter().collect();
        // A rebuilt movie frame is the display alone, without wider
        // framing's sides.
        self.presented_movie = presented.as_ref().is_some_and(|(_, _, movie)| *movie);
        let side = if self.presented_movie { 0.0 } else { side };
        let presented = presented.as_ref().map(|(view, size, _)| (view, *size));
        let (game_view, game_size) = match (presented, &self.game_image) {
            (Some(presented), _) => presented,
            (None, Some((_, view, size))) => (view, *size),
            (None, None) => {
                // No game: the test scene stands in for it.
                let (view, proj) = demo_camera(GAME_SIZE.0 as f32 / GAME_SIZE.1 as f32, time);
                self.scene.render_views(
                    &mut encoder,
                    &[ViewTarget {
                        view,
                        proj,
                        target: &self.game_view,
                        viewport: Viewport::full(GAME_SIZE.0, GAME_SIZE.1),
                    }],
                    time,
                );
                (&self.game_view, GAME_SIZE)
            }
        };

        let target_size = self.target_size;
        let presentation = self.settings.borrow().settings().presentation.clone();
        // The HUD stands at the safe area's top left; while the touch deck
        // shows, it keeps clear of the deck's controls level with it (its
        // top buttons on a phone).
        let scale = self.ui.scale_factor();
        let hud_at = (safe.x + 8.0 * scale, safe.y + 8.0 * scale);
        let deck = self.deck.controls.iter().filter(|_| touch_shown).map(|(_, rect)| (rect.x, rect.y, rect.width, rect.height));
        self.frame_rate.keep_clear(hud_at, scale, deck);
        self.frame_rate.frame(self.frame_rate_started.elapsed(),scale,presentation.show_fps)
            .map_err(|error| error.to_string())?;
        // Under Force 4:3 the picture's place follows the game's own lines,
        // whatever size the frame was drawn at: game frames, rebuilt movies
        // and the device's picture take the same place.
        let graphics = self.settings.borrow().settings().graphics;
        let ratio = graphics.forced_aspect();
        let image = match (&self.game_frame, ratio) {
            (Some((display, _)), Some(_)) => (display.width, display.height),
            _ => game_size,
        };
        let mut placed = present_rect(image, (safe.width as u32, safe.height as u32), presentation.scale, ratio);
        placed.x += safe.x;
        placed.y += safe.y;
        self.picture = match (&self.game_frame, encoded) {
            (Some((display, _)), true) => Some(Picture {
                placed,
                display: (display.width as f32, display.height as f32),
                side,
            }),
            (_, false) if self.game_image.is_some() => Some(Picture { placed, display: (game_size.0 as f32, game_size.1 as f32), side: 0.0 }),
            _ => None,
        };
        let mut game = Quad::screen(game_view, placed, target_size);
        // The renderer's frames hold sRGB-encoded values in a linear format.
        game.srgb_encoded = encoded;
        game.opaque = true;
        game.filter = match presentation.filter {
            Filter::Nearest => wgpu::FilterMode::Nearest,
            Filter::Linear => wgpu::FilterMode::Linear,
        };
        // Movie frames the device shows itself (the Original movies setting,
        // or one Ex Machina cannot rebuild) stay as the decoder made them,
        // each pixel a block; the Original renderer keeps the chosen filter.
        if !encoded && self.game_frame.is_none() && self.game_image.is_some() && graphics.renderer == xem_settings::Renderer::ExMachina {
            game.filter = wgpu::FilterMode::Nearest;
        }
        // The assisted controls' marks and the touch deck, as Seraph Glass.
        let style = xem_render::glass::Style { reduced_transparency: self.settings.borrow().settings().assisted.reduced_transparency };
        let mut glass = Vec::new();
        if let Some(picture) = self.picture {
            // Each axis as the picture is enlarged along it (Force 4:3 or
            // Stretch reshapes it).
            let (sx, sy) = (picture.placed.width / (picture.display.0 + 2.0 * picture.side), picture.placed.height / picture.display.1);
            for mark in &self.marks {
                // A flat picture's marks lie on its screen.
                let xem_assist::overlay::Place::Screen { centre, x, y } = mark.place else { continue };
                let widget = xem_assist::draw::mark_widget(mark.kind);
                let view = self.atlas.view(&gpu.device, &gpu.queue, &widget, 128, style).clone();
                let centre = picture.from_screen(centre);
                glass.push((view, Some((centre, (x.0 * sx, x.1 * sy), (y.0 * sx, y.1 * sy))), None, 1.0));
            }
        }
        // The deck at the setting's opacity (what a finger holds whole); it
        // stays under the open panel, where it holds nothing, so that its
        // opacity shows beside the panel as the setting changes.
        if touch_shown {
            let (lit, opacity) = {
                let settings = self.settings.borrow();
                let settings = settings.settings();
                (xem_assist::draw::Lit { auto_advance: settings.auto_advance.enabled, first_person, run: settings.run_by_default }, settings.assisted.touch_alpha())
            };
            for drawn in xem_assist::draw::deck_widgets(&self.deck, lit, opacity) {
                let rect = Rect { x: drawn.rect.x, y: drawn.rect.y, width: drawn.rect.width, height: drawn.rect.height };
                let view = self.atlas.view(&gpu.device, &gpu.queue, &drawn.widget, drawn.pixels, style).clone();
                glass.push((view, None, Some(rect), drawn.opacity));
            }
        }
        let mut quads = vec![game];
        // The movie's subtitle over the picture, under everything else.
        if let (Some(picture), Some((view, size))) = (&self.picture, self.subtitles.shown()) {
            let mut subtitle = Quad::screen(view, xem_render::subtitles::flat(picture, size), target_size);
            subtitle.srgb_encoded = true;
            subtitle.filter = wgpu::FilterMode::Nearest;
            quads.push(subtitle);
        }
        for (view, affine, rect, opacity) in &glass {
            let mut quad = match (affine, rect) {
                (Some((centre, x, y)), _) => Quad::screen_affine(view, *centre, *x, *y, target_size),
                (None, Some(rect)) => Quad::screen(view, *rect, target_size),
                (None, None) => continue,
            };
            quad.srgb_encoded = true;
            quad.opacity = *opacity;
            quads.push(quad);
        }
        if self.frame_rate.visible() {
            let (width,height) = self.frame_rate.size();
            let mut counter = Quad::screen(self.frame_rate.view(),Rect { x:hud_at.0,y:hud_at.1,width:width as f32,height:height as f32 },target_size);
            counter.srgb_encoded = true;
            counter.filter = wgpu::FilterMode::Nearest;
            quads.push(counter);
        }
        if visible {
            let mut panel = Quad::screen(self.ui.view(), panel_rect, target_size);
            panel.srgb_encoded = true;
            panel.filter = wgpu::FilterMode::Nearest;
            quads.push(panel);
        }
        self.compositor
            .draw(&mut encoder, output, Some(wgpu::Color::BLACK), &quads);
        drop(quads);
        gpu.queue.submit([encoder.finish()]);
        Ok(())
    }
}

/// KeyboardEvent.code to SDL's physical scancode names, shared with browser
/// bindings. Settings validation only accepts codes this mapping supports.
fn scancode(code: &str) -> SDL_Scancode {
    static CODES: std::sync::OnceLock<std::collections::HashMap<String, SDL_Scancode>> = std::sync::OnceLock::new();
    let codes = CODES.get_or_init(|| xem_settings::controls::keyboard_codes().map(|code| {
        let value = resolve_scancode(&code);
        (code, value)
    }).collect());
    codes.get(code).copied().unwrap_or(SDL_SCANCODE_UNKNOWN)
}

fn resolve_scancode(code: &str) -> SDL_Scancode {
    let name = if let Some(key) = code.strip_prefix("Key") { key.to_owned() }
        else if let Some(digit) = code.strip_prefix("Digit") { digit.to_owned() }
        else if let Some(digit) = code.strip_prefix("Numpad") {
            format!("Keypad {}",match digit { "Decimal" => ".", "Add" => "+", "Subtract" => "-", "Multiply" => "*", "Divide" => "/", "Enter" => "Enter", _ => digit })
        } else {
            match code {
                "ControlLeft" => "Left Ctrl", "ControlRight" => "Right Ctrl", "ShiftLeft" => "Left Shift", "ShiftRight" => "Right Shift",
                "AltLeft" => "Left Alt", "AltRight" => "Right Alt", "MetaLeft" => "Left GUI", "MetaRight" => "Right GUI",
                "ArrowUp" => "Up", "ArrowDown" => "Down", "ArrowLeft" => "Left", "ArrowRight" => "Right",
                "Enter" => "Return", "PageUp" => "PageUp", "PageDown" => "PageDown", "CapsLock" => "CapsLock",
                "NumLock" => "Numlock", "ScrollLock" => "ScrollLock", "PrintScreen" => "PrintScreen",
                "BracketLeft" => "[", "BracketRight" => "]", "Backslash" => "\\", "Semicolon" => ";", "Quote" => "'",
                "Backquote" => "`", "Comma" => ",", "Period" => ".", "Slash" => "/", "Minus" => "-", "Equal" => "=",
                other => other,
            }.to_owned()
        };
    let Ok(name) = std::ffi::CString::new(name) else { return SDL_SCANCODE_UNKNOWN };
    unsafe { SDL_GetScancodeFromName(name.as_ptr()) }
}

/// A key's `KeyboardEvent.code` name from SDL's scancode, among those a
/// binding may name (`xem_settings::controls::keyboard_codes`).
fn key_code(key: SDL_Scancode) -> Option<String> {
    xem_settings::controls::keyboard_codes().find(|code| scancode(code).0 == key.0)
}

/// The open gamepads' right stick, -1 to 1 (x to the right, y down).
fn right_stick(pads: &[*mut SDL_Gamepad]) -> [f32; 2] {
    stick(pads, [SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY])
}

/// A stick of the open gamepads, -1 to 1: the one pushed furthest. Each
/// reads its own whole stick, so the x and y are the same stick's.
fn stick(pads: &[*mut SDL_Gamepad], axes: [SDL_GamepadAxis; 2]) -> [f32; 2] {
    pads.iter()
        .map(|&pad| axes.map(|axis| f32::from(unsafe { SDL_GetGamepadAxis(pad, axis) }) / 32768.0))
        .fold([0.0; 2], |most, [x, y]| if x * x + y * y > most[0] * most[0] + most[1] * most[1] { [x, y] } else { most })
}

/// The open gamepads' standard buttons and their axes (left X/Y, right
/// X/Y), all of them together: a handheld's built-in controller can come
/// with other devices Android counts as gamepads (a second, silent pad, a
/// D-pad of keys), and any of them is the one the player holds.
fn standard_pad(pads: &[*mut SDL_Gamepad]) -> ([u8;16], [f32; 4]) {
    let mut pressed = [0;16];
    for &pad in pads {
        for (i,button) in [(0,SDL_GAMEPAD_BUTTON_SOUTH),(1,SDL_GAMEPAD_BUTTON_EAST),(2,SDL_GAMEPAD_BUTTON_WEST),(3,SDL_GAMEPAD_BUTTON_NORTH),
            (4,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER),(5,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER),(8,SDL_GAMEPAD_BUTTON_BACK),(9,SDL_GAMEPAD_BUTTON_START),
            (10,SDL_GAMEPAD_BUTTON_LEFT_STICK),(11,SDL_GAMEPAD_BUTTON_RIGHT_STICK),(12,SDL_GAMEPAD_BUTTON_DPAD_UP),(13,SDL_GAMEPAD_BUTTON_DPAD_DOWN),
            (14,SDL_GAMEPAD_BUTTON_DPAD_LEFT),(15,SDL_GAMEPAD_BUTTON_DPAD_RIGHT)] {
            pressed[i] |= u8::from(unsafe { SDL_GetGamepadButton(pad,button) });
        }
        pressed[6] |= u8::from(unsafe { SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFT_TRIGGER) } > 16000);
        pressed[7] |= u8::from(unsafe { SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) } > 16000);
    }
    let [lx, ly] = stick(pads, [SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY]);
    let [rx, ry] = right_stick(pads);
    (pressed, [lx, ly, rx, ry])
}

impl Drop for View {
    fn drop(&mut self) {
        for pad in self.gamepads.drain(..) {
            // SAFETY: opened by SDL_OpenGamepad and closed once.
            unsafe { SDL_CloseGamepad(pad) };
        }
    }
}

/// The panel's pixel size for a target of `width` x `height` pixels.
fn panel_size((width, height): (u32, u32), scale: f32) -> (u32, u32) {
    let fit = |logical: f32, available: u32| {
        ((logical * scale) as u32)
            .min(available.saturating_sub(16))
            .max(1)
    };
    (fit(PANEL_SIZE.0, width), fit(PANEL_SIZE.1, height))
}

/// Whether a key of the shortcut modifier (Ctrl; Command on macOS) is
/// down, as SDL's keyboard state has it from the keys' own events.
fn shortcut_key_held() -> bool {
    let keys = if cfg!(target_os = "macos") { [SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI] } else { [SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL] };
    let mut count = 0;
    // SAFETY: SDL's array of every scancode's state, valid while SDL runs.
    let state = unsafe { SDL_GetKeyboardState(&mut count) };
    !state.is_null() && keys.iter().any(|key| key.0 < count && unsafe { *state.add(key.0 as usize) })
}

/// What a key event gives the panel. Printable text arrives as SDL text
/// input, which gives none for a letter with the shortcut modifier (Ctrl;
/// Command on macOS) held: that letter comes as its key, for Slint's
/// shortcuts (Ctrl+V pastes, Ctrl+C copies, Ctrl+A selects all). The
/// modifier is the letter's event's own: a shortcut another program sends
/// (Omarchy's Super+V has Hyprland send Ctrl+V) presses no Ctrl key, so
/// while none is down (`held`) the letter comes between a press and a
/// release of Slint's Control.
fn panel_key_events(key: &SDL_KeyboardEvent, held: bool) -> Vec<WindowEvent> {
    let event = |text: xem_ui::SharedString| match (key.down, key.repeat) {
        (true, false) => WindowEvent::KeyPressed { text },
        (true, true) => WindowEvent::KeyPressRepeated { text },
        (false, _) => WindowEvent::KeyReleased { text },
    };
    let shortcut = if cfg!(target_os = "macos") { SDL_KMOD_GUI } else { SDL_KMOD_CTRL };
    if (key.r#mod & shortcut) != SDL_Keymod(0) && (SDLK_A.0..=SDLK_Z.0).contains(&key.key.0) {
        let letter = event(char::from_u32(key.key.0).unwrap_or_default().to_string().into());
        return if held || !key.down {
            vec![letter]
        } else {
            vec![WindowEvent::KeyPressed { text: Key::Control.into() }, letter, WindowEvent::KeyReleased { text: Key::Control.into() }]
        };
    }
    slint_key(key.key).map(event).into_iter().collect()
}

/// Keys Slint treats specially.
fn slint_key(key: SDL_Keycode) -> Option<xem_ui::SharedString> {
    // Slint's shortcuts read its Control modifier: on macOS, Command's.
    let (control, meta) = if cfg!(target_os = "macos") { (Key::Meta, Key::Control) } else { (Key::Control, Key::Meta) };
    let (control_r, meta_r) = if cfg!(target_os = "macos") { (Key::MetaR, Key::ControlR) } else { (Key::ControlR, Key::MetaR) };
    let key = match key {
        SDLK_BACKSPACE => Key::Backspace,
        SDLK_TAB => Key::Tab,
        SDLK_RETURN | SDLK_KP_ENTER => Key::Return,
        SDLK_ESCAPE => Key::Escape,
        SDLK_DELETE => Key::Delete,
        SDLK_LEFT => Key::LeftArrow,
        SDLK_RIGHT => Key::RightArrow,
        SDLK_UP => Key::UpArrow,
        SDLK_DOWN => Key::DownArrow,
        SDLK_HOME => Key::Home,
        SDLK_END => Key::End,
        SDLK_PAGEUP => Key::PageUp,
        SDLK_PAGEDOWN => Key::PageDown,
        SDLK_LSHIFT => Key::Shift,
        SDLK_RSHIFT => Key::ShiftR,
        SDLK_LCTRL => control,
        SDLK_RCTRL => control_r,
        SDLK_LALT => Key::Alt,
        SDLK_RALT => Key::AltGr,
        SDLK_LGUI => meta,
        SDLK_RGUI => meta_r,
        _ => return None,
    };
    Some(key.into())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_letter_with_the_shortcut_modifier_reaches_the_panel_as_a_key_with_control_held() {
        let (shortcut, other) = if cfg!(target_os = "macos") { (SDL_KMOD_LGUI, SDL_KMOD_LCTRL) } else { (SDL_KMOD_LCTRL, SDL_KMOD_LGUI) };
        let key = |key: SDL_Keycode, modifiers: SDL_Keymod, down: bool| SDL_KeyboardEvent { key, r#mod: modifiers, down, ..Default::default() };
        let text = |events: Vec<WindowEvent>| events.into_iter().map(|event| match event {
            WindowEvent::KeyPressed { text } => format!("+{text}"),
            WindowEvent::KeyReleased { text } => format!("-{text}"),
            other => format!("{other:?}"),
        }).collect::<Vec<_>>();
        let control = xem_ui::SharedString::from(Key::Control).to_string();
        // Ctrl+V with the Ctrl key down: Slint has its Control already.
        assert_eq!(text(panel_key_events(&key(SDLK_V, shortcut, true), true)), ["+v"]);
        assert_eq!(text(panel_key_events(&key(SDLK_V, shortcut | SDL_KMOD_LSHIFT, true), true)), ["+v"]);
        // Sent with the modifier and no Ctrl key (Omarchy's Super+V): Control
        // around the letter.
        assert_eq!(text(panel_key_events(&key(SDLK_V, shortcut | SDL_KMOD_LGUI, true), false)), [format!("+{control}"), "+v".into(), format!("-{control}")]);
        assert_eq!(text(panel_key_events(&key(SDLK_V, shortcut, false), false)), ["-v"]);
        // Typed letters come as SDL's text input; other keys as themselves.
        assert!(panel_key_events(&key(SDLK_V, SDL_KMOD_NONE, true), false).is_empty());
        assert!(panel_key_events(&key(SDLK_V, other, true), false).is_empty());
        assert!(panel_key_events(&key(SDLK_1, shortcut, true), true).is_empty());
        let modifier = if cfg!(target_os = "macos") { SDLK_LGUI } else { SDLK_LCTRL };
        assert_eq!(text(panel_key_events(&key(modifier, SDL_KMOD_NONE, true), false)), [format!("+{control}")]);
    }

    #[test]
    fn show_fps_draws_over_the_game_with_settings_closed() {
        let gpu=pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings=Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        let mut view=View::new(&gpu,wgpu::TextureFormat::Rgba8UnormSrgb,(640,480),1.0,settings.clone(),false).unwrap();
        let black:Vec<_>=(0..320*240).flat_map(|_| [0,0,0,255]).collect();
        view.set_game_image(&gpu,(320,240),&black);
        let target=gpu.device.create_texture(&wgpu::TextureDescriptor {
            label:Some("FPS regression target"),
            size:wgpu::Extent3d { width:640,height:480,depth_or_array_layers:1 },
            mip_level_count:1,sample_count:1,dimension:wgpu::TextureDimension::D2,
            format:wgpu::TextureFormat::Rgba8UnormSrgb,
            usage:wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
            view_formats:&[],
        });
        let output=target.create_view(&Default::default());
        let pixels=|view:&mut View| {
            view.render(&gpu,&output,0.0).unwrap();
            xem_render::read_rgba8(&gpu.device,&gpu.queue,&target)
        };
        assert!(pixels(&mut view).chunks_exact(4).all(|p| p[..3]==[0,0,0]));
        settings.borrow_mut().apply(xem_settings::SettingChange::ShowFps(true)).unwrap();
        let image=pixels(&mut view);
        let bright:Vec<_>=image.chunks_exact(4).enumerate().filter(|(_,p)| p[0]>180 && p[1]>180 && p[2]>180).map(|(i,_)| (i%640,i/640)).collect();
        assert!(bright.len()>30,"the counter text is visible over the game");
        assert!(bright.iter().all(|&(x,y)| (8..104).contains(&x) && (8..36).contains(&y)),"the counter stays in its corner");
        assert!(!view.panel_visible());
        settings.borrow_mut().apply(xem_settings::SettingChange::ShowFps(false)).unwrap();
        assert!(pixels(&mut view).chunks_exact(4).all(|p| p[..3]==[0,0,0]),"disabling Show FPS removes the counter");
        view.frame_rate.set_activity(true,false);
        assert!(pixels(&mut view).chunks_exact(4).any(|p| p[0]>180),"FF icon is visible with FPS off");
        view.frame_rate.set_activity(false,true);
        assert!(pixels(&mut view).chunks_exact(4).any(|p| p[0]>180),"rewind icon is visible with FPS off");
        view.frame_rate.set_activity(false,false);
        view.state_notice(xem_settings::AppAction::ImportCard1,&Err("card error".into()));
        assert!(view.frame_rate.notice().is_none(),"memory-card operations do not create state notices");
        view.state_notice(xem_settings::AppAction::SaveState,&Ok("written".into()));
        assert_eq!(view.frame_rate.notice().as_deref(),Some("State saved"));
        assert!(pixels(&mut view).chunks_exact(4).any(|p| p[0]>180),"state notice is visible with Settings closed");
        view.state_notice(xem_settings::AppAction::LoadState,&Err("checksum failed".into()));
        assert!(view.frame_rate.notice().unwrap().contains("checksum failed"));
        assert!(!view.panel_visible());
    }

    #[test]
    fn touch_controls_hold_their_buttons_taps_on_the_picture_are_kept_and_auto_asks_the_app() {
        use xem_core::pad::button;
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        let (width, height) = (1280u32, 720u32);
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (width, height), 1.0, settings, false).unwrap();
        let black: Vec<_> = (0..320 * 240).flat_map(|_| [0, 0, 0, 255]).collect();
        view.set_game_image(&gpu, (320, 240), &black);
        let target = gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("touch target"),
            size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8UnormSrgb,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        let output = target.create_view(&Default::default());
        view.render(&gpu, &output, 0.0).unwrap();
        let picture = view.picture.expect("the picture's place").placed;
        assert!(picture.x >= 150.0, "a letterboxed 4:3 picture: {picture:?}");
        // No touch yet (not Android): no deck.
        let lit = |image: &[u8]| image.chunks_exact(4).enumerate()
            .filter(|(i, p)| { let x = (i % width as usize) as f32; (x < picture.x || x >= picture.x + picture.width) && p[0] > 120 })
            .count();
        assert_eq!(lit(&xem_render::read_rgba8(&gpu.device, &gpu.queue, &target)), 0);
        // A finger on the stick pushed up: the deck shows, Up is held.
        view.arrange(xem_core::context::Context::Walk);
        let stick = view.deck.stick.expect("a stick");
        assert!(stick.rest.0 + stick.radius() <= picture.x, "beside the picture: {stick:?}");
        let at = |x: f32, y: f32| (x / width as f32, y / height as f32);
        let (cx, cy) = stick.rest;
        view.finger(Finger::Down, 7, at(cx, cy));
        assert_eq!(view.pad_buttons(PadContext::default(), None), 0, "held still, it holds nothing");
        view.finger(Finger::Moved, 7, at(cx, cy - stick.reach));
        assert_eq!(view.pad_buttons(PadContext::default(), None), button::UP);
        view.render(&gpu, &output, 0.0).unwrap();
        assert!(lit(&xem_render::read_rgba8(&gpu.device, &gpu.queue, &target)) > 1000, "the glass deck is drawn");
        view.finger(Finger::Up, 7, at(cx, cy - stick.reach));
        assert_eq!(view.pad_buttons(PadContext::default(), None), 0);
        // A tap on the picture is kept for the game's context.
        let (px, py) = (picture.x + picture.width / 2.0, picture.y + picture.height / 2.0);
        view.finger(Finger::Down, 8, (px / width as f32, py / height as f32));
        view.finger(Finger::Up, 8, (px / width as f32, py / height as f32));
        assert_eq!(view.taps.len(), 1);
        let screen = view.picture.unwrap().to_screen(view.taps[0].0, view.taps[0].1).unwrap();
        assert!((screen.0 - 160.0).abs() < 1.0 && (screen.1 - 120.0).abs() < 1.0, "{screen:?}");
        // In a field's scene AUTO asks the app to toggle auto-advance, as
        // its shortcut does, and holds no button of the game's.
        view.arrange(xem_core::context::Context::Event { map: 4, buttons: 0 });
        let auto = view.deck.controls.iter().find(|(c, _)| *c == xem_assist::touch::Control::Auto).expect("AUTO in a scene").1;
        let (ax, ay) = auto.centre();
        let requests = view.app_requests.borrow().len();
        view.finger(Finger::Down, 9, (ax / width as f32, ay / height as f32));
        assert_eq!(view.pad_buttons(PadContext::default(), None), 0);
        assert_eq!(view.app_requests.borrow()[requests..], [AppRequest::Action(xem_settings::AppAction::ToggleAutoAdvance)]);
        view.finger(Finger::Up, 9, (ax / width as f32, ay / height as f32));
        assert_eq!(view.app_requests.borrow().len(), requests + 1, "one request a touch");
        assert_eq!(view.taps.len(), 1, "not a tap on the picture");
    }

    /// The touch controls' opacity (Settings' slider): the deck is drawn as
    /// opaque as the setting says, but a control a finger holds, drawn
    /// whole; it stays under the open settings panel, holding nothing, so
    /// that the setting shows beside the panel as it changes. The deck's RUN
    /// On a phone the touch deck's top buttons stand at the screen's top
    /// right, level with the HUD: the HUD (the FPS counter, fast-forward,
    /// an unlock's notice with its badge) narrows to keep a margin from
    /// them, and takes its whole width again without the deck. For a review
    /// capture (`XEM_UI_CAPTURE_DIR`), `XEM_HUD_PICTURE` and `XEM_HUD_BADGE`
    /// may give the game's picture and the badge (PNGs).
    #[test]
    fn the_hud_keeps_clear_of_the_touch_decks_top_buttons() {
        use xem_assist::touch::Control;
        use xem_core::context::Context;
        use xem_settings::{Assisted, SettingChange};
        let rgba = |path: &std::ffi::OsStr| {
            let mut decoder = png::Decoder::new(std::io::BufReader::new(std::fs::File::open(path).unwrap()));
            decoder.set_transformations(png::Transformations::normalize_to_color8() | png::Transformations::ALPHA);
            let mut reader = decoder.read_info().unwrap();
            let mut pixels = vec![0; reader.output_buffer_size().unwrap()];
            let frame = reader.next_frame(&mut pixels).unwrap();
            assert_eq!(frame.color_type, png::ColorType::Rgba);
            pixels.truncate(frame.buffer_size());
            ((frame.width, frame.height), pixels)
        };
        let encode = |(width, height): (u32, u32), pixels: &[u8]| {
            let mut out = Vec::new();
            let mut encoder = png::Encoder::new(&mut out, width, height);
            encoder.set_color(png::ColorType::Rgba);
            encoder.set_depth(png::BitDepth::Eight);
            encoder.write_header().unwrap().write_image_data(pixels).unwrap();
            out
        };
        let (picture_size, picture) = std::env::var_os("XEM_HUD_PICTURE").map(|path| rgba(&path))
            .unwrap_or_else(|| ((320, 240), (0..320 * 240).flat_map(|_| [96, 96, 96, 255]).collect()));
        let badge = std::env::var_os("XEM_HUD_BADGE").map(|path| { let (size, pixels) = rgba(&path); encode(size, &pixels) })
            .unwrap_or_else(|| encode((64, 64), &(0..64 * 64).flat_map(|i| if (i % 64 - 32i32).pow(2) + (i / 64 - 32i32).pow(2) < 400 { [240, 199, 94, 255] } else { [30, 50, 110, 255] }).collect::<Vec<u8>>()));
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        settings.borrow_mut().apply(SettingChange::ShowFps(true)).unwrap();
        // The Android smoke's emulator: 1080x2400 at density 2.625.
        let (width, height, scale) = (1080u32, 2400u32, 2.625f32);
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8Unorm, (width, height), scale, settings.clone(), false).unwrap();
        view.set_game_image(&gpu, picture_size, &picture);
        let target = gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("phone target"),
            size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8Unorm,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        let output = target.create_view(&Default::default());
        view.render(&gpu, &output, 0.0).unwrap();
        let whole = (xem_ui::FrameRateUi::WIDTH * scale).round() as u32;
        assert_eq!(view.frame_rate.size().0, whole, "no deck: the HUD's whole width");
        // A finger has touched the game: the deck shows, its top buttons
        // level with the HUD; fast-forward runs and an achievement unlocks.
        view.touched = true;
        view.arrange(Context::Walk);
        view.frame_rate.set_activity(true, false);
        let now = view.frame_rate_started.elapsed();
        view.frame_rate.notify(xem_ui::NoticeKind::Achievement, "Welcome to Lahan", "Arrive in Lahan village · 5 points", Some(&badge), 6, now);
        view.render(&gpu, &output, 0.0).unwrap();
        let top = view.deck.controls.iter().filter(|(control, _)| matches!(control, Control::Start | Control::Settings)).map(|(_, rect)| rect.x).fold(f32::MAX, f32::min);
        let margin = 8.0 * scale;
        let right = margin + view.frame_rate.size().0 as f32;
        assert!(view.frame_rate.size().0 < whole && right + margin <= top + 1.0, "the HUD ends at {right}, a margin before the deck's top buttons at {top}");
        assert_eq!(view.frame_rate.notice().as_deref(), Some("Welcome to Lahan: Arrive in Lahan village · 5 points"));
        if let Some(directory) = std::env::var_os("XEM_UI_CAPTURE_DIR") {
            let directory = std::path::PathBuf::from(directory);
            std::fs::create_dir_all(&directory).unwrap();
            let image = xem_render::read_rgba8(&gpu.device, &gpu.queue, &target);
            std::fs::write(directory.join("phone-unlock-with-touch-deck.png"), encode((width, height), &image)).unwrap();
        }
        // Touch controls turned off: the deck goes, the HUD is whole again.
        let assisted = settings.borrow().settings().assisted;
        settings.borrow_mut().apply(SettingChange::Assisted(Assisted { touch_controls: false, ..assisted })).unwrap();
        view.render(&gpu, &output, 0.0).unwrap();
        assert_eq!(view.frame_rate.size().0, whole);
    }

    /// is the Run by default setting, lit while on.
    #[test]
    fn the_touch_deck_is_drawn_at_the_settings_opacity_and_its_run_key_is_the_run_by_default_setting() {
        use xem_assist::touch::Control;
        use xem_core::FieldControls;
        use xem_core::context::Context;
        use xem_core::pad::button;
        use xem_settings::{Assisted, SettingChange};
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        let (width, height) = (1280u32, 720u32);
        // A target as the windows': sRGB-encoded values in a linear format,
        // which the deck is composited in.
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8Unorm, (width, height), 1.0, settings.clone(), false).unwrap();
        let black: Vec<_> = (0..320 * 240).flat_map(|_| [0, 0, 0, 255]).collect();
        view.set_game_image(&gpu, (320, 240), &black);
        let target = gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("touch target"),
            size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8Unorm,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        let output = target.create_view(&Default::default());
        view.render(&gpu, &output, 0.0).unwrap();
        // A finger has touched the game: the deck shows, beside the picture.
        view.touched = true;
        view.arrange(Context::Walk);
        let place = |view: &View, control: Control| view.deck.controls.iter().find(|(k, _)| *k == control).unwrap_or_else(|| panic!("no {control:?}")).1;
        let across = |rect: xem_assist::touch::Rect| (rect.centre().0 / width as f32, rect.centre().1 / height as f32);
        // What is drawn in a control's place over the black letterbox: its
        // summed brightness.
        let drawn = |view: &mut View, rect: xem_assist::touch::Rect| -> f64 {
            view.render(&gpu, &output, 0.0).unwrap();
            let image = xem_render::read_rgba8(&gpu.device, &gpu.queue, &target);
            let inside = |i: usize| rect.contains(((i % width as usize) as f32, (i / width as usize) as f32));
            image.chunks_exact(4).enumerate().filter(|(i, _)| inside(*i)).map(|(_, p)| f64::from(p[0]) + f64::from(p[1]) + f64::from(p[2])).sum()
        };
        let opacity = |percent: u8| {
            let assisted = settings.borrow().settings().assisted;
            settings.borrow_mut().apply(SettingChange::Assisted(Assisted { touch_opacity: percent, ..assisted })).unwrap();
        };
        // TALK, and Settings in its corner, away from it.
        let (talk, gear) = (place(&view, Control::Confirm), place(&view, Control::Settings));
        let picture = view.picture.expect("the picture's place").placed;
        assert!(talk.x >= picture.x + picture.width, "TALK beside the picture, over black: {talk:?}");
        let whole = (drawn(&mut view, talk), drawn(&mut view, gear));
        assert!(whole.0 > 100_000.0 && whole.1 > 30_000.0, "the deck as designed: {whole:?}");
        // Half as opaque over black is half as bright; a tenth, faint.
        opacity(50);
        let half = (drawn(&mut view, talk), drawn(&mut view, gear));
        assert!((half.0 / whole.0 - 0.5).abs() < 0.02 && (half.1 / whole.1 - 0.5).abs() < 0.02, "at 50%: {half:?} of {whole:?}");
        opacity(10);
        let faint = (drawn(&mut view, talk), drawn(&mut view, gear));
        assert!((faint.0 / whole.0 - 0.1).abs() < 0.02 && faint.0 > 0.0, "at 10%: {faint:?} of {whole:?}");
        // A finger on TALK: it shows whole, pressed; Settings stays faint.
        view.finger(Finger::Down, 1, across(talk));
        assert_eq!(view.pad_buttons(PadContext::default(), None), button::CROSS);
        let held = (drawn(&mut view, talk), drawn(&mut view, gear));
        assert!(held.0 > 5.0 * faint.0 && (held.1 - faint.1).abs() < 1.0, "held: {held:?} against {faint:?}");
        // The settings panel opens: the deck lets go, and stays drawn under
        // it (the panel stands over the picture's middle).
        view.set_panel_visible(true);
        assert_eq!(view.pad_buttons(PadContext::default(), None), 0, "the panel has the input");
        let panel = view.panel_rect();
        assert!(talk.x >= panel.x + panel.width, "TALK beside the panel: {talk:?} {panel:?}");
        let under = drawn(&mut view, talk);
        assert!((under - faint.0).abs() < 1.0, "under the open panel at the setting's opacity: {under} against {}", faint.0);
        opacity(100);
        let shown = drawn(&mut view, talk);
        assert!((shown - whole.0).abs() < 1.0, "the setting shows as it changes: {shown}");
        view.set_panel_visible(false);
        view.finger(Finger::Up, 1, across(talk));
        // RUN: the Run by default setting, lit while on; the stick then runs.
        let run = place(&view, Control::Run);
        let walking = PadContext { field: FieldControls { field: true, walking: true, menu: false, picture: false }, ui: Context::Walk, ..PadContext::default() };
        let stick = view.deck.stick.expect("a stick");
        let at = |x: f32, y: f32| (x / width as f32, y / height as f32);
        view.finger(Finger::Down, 2, at(stick.rest.0, stick.rest.1));
        view.finger(Finger::Moved, 2, at(stick.rest.0, stick.rest.1 - stick.reach));
        assert_eq!(view.pad_buttons(walking, None), button::UP, "walking");
        let unlit = drawn(&mut view, run);
        view.finger(Finger::Down, 3, across(run));
        assert!(settings.borrow().settings().run_by_default, "RUN turns Run by default on");
        assert_eq!(view.pad_buttons(walking, None), button::UP | button::CIRCLE, "the stick runs; RUN itself holds nothing");
        view.finger(Finger::Up, 3, across(run));
        assert_ne!(drawn(&mut view, run), unlit, "lit while on");
        view.finger(Finger::Down, 3, across(run));
        view.finger(Finger::Up, 3, across(run));
        assert!(!settings.borrow().settings().run_by_default, "and off again");
        assert_eq!(view.pad_buttons(walking, None), button::UP);
        assert_eq!(drawn(&mut view, run), unlit);
    }

    #[test]
    fn a_reconnected_gamepad_plays_again_and_settings_change_live() {
        use xem_core::pad::button;
        assert!(unsafe { SDL_Init(SDL_INIT_GAMEPAD) }, "SDL's gamepads");
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (640, 480), 1.0, settings.clone(), false).unwrap();
        let attach = || {
            let desc = SDL_VirtualJoystickDesc {
                r#type: SDL_JOYSTICK_TYPE_GAMEPAD.0 as u16,
                naxes: SDL_GAMEPAD_AXIS_COUNT.0 as u16,
                nbuttons: SDL_GAMEPAD_BUTTON_COUNT.0 as u16,
                name: c"reconnect test pad".as_ptr(),
                ..Default::default()
            };
            unsafe { SDL_AttachVirtualJoystick(&desc) }
        };
        let press = |view: &View, held: &[SDL_GamepadButton]| unsafe {
            let joystick = SDL_GetGamepadJoystick(*view.gamepads.first().expect("a pad"));
            for button in 0..SDL_GAMEPAD_BUTTON_COUNT.0 {
                SDL_SetJoystickVirtualButton(joystick, button, held.iter().any(|b| b.0 == button));
            }
            SDL_UpdateGamepads();
        };
        let frame = |view: &mut View| {
            (view.pad, view.axes) = standard_pad(&view.gamepads);
            view.app_input.pad(&view.pad);
            view.pad_buttons(PadContext::default(), None)
        };
        let id = attach();
        view.gamepad_added(id);
        press(&view, &[SDL_GAMEPAD_BUTTON_SOUTH]);
        assert_eq!(frame(&mut view), button::CROSS);
        // Pulled out while South is held: nothing stays held.
        view.gamepad_removed(id);
        unsafe { SDL_DetachVirtualJoystick(id) };
        assert_eq!(frame(&mut view), 0, "a disconnected pad holds nothing");
        // Plugged in again: it plays.
        let again = attach();
        view.gamepad_added(again);
        press(&view, &[SDL_GAMEPAD_BUTTON_NORTH]);
        assert_eq!(frame(&mut view), button::TRIANGLE, "the reconnected pad plays");
        press(&view, &[]);
        let tilt = |view: &View, x: f32, y: f32| unsafe {
            let joystick = SDL_GetGamepadJoystick(*view.gamepads.first().unwrap());
            SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTX.0, (x * 32767.0) as i16);
            SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTY.0, (y * 32767.0) as i16);
            SDL_UpdateGamepads();
        };
        // Even during a scripted scene (no walking controls), the player
        // can bring the camera back in to see through the house's walls.
        tilt(&view, 0.8, 0.9);
        assert_eq!(frame(&mut view), 0, "the lower slice zooms without turning");
        assert_eq!(settings.borrow().settings().graphics.zoom_out, 125);
        tilt(&view, 0.8, -0.9);
        assert_eq!(frame(&mut view), 0);
        assert_eq!(settings.borrow().settings().graphics.zoom_out, 100, "up restores the original zoom");
        tilt(&view, -0.9, 0.8);
        assert_eq!(frame(&mut view), button::L1, "the left slice still turns");
        tilt(&view, 0.9, -0.8);
        assert_eq!(frame(&mut view), button::R1, "the right slice still turns");
        assert_eq!(settings.borrow().settings().graphics.zoom_out, 100, "turning never zooms");
        tilt(&view, 0.0, 0.0);
        // Settings changed live, from the open panel: Confirm and Menu
        // swapped (Menu cleared first: an input holds one button).
        view.set_panel_visible(true);
        tilt(&view, 0.0, 0.9);
        assert_eq!(frame(&mut view), 0, "the panel has the input");
        assert_eq!(settings.borrow().settings().graphics.zoom_out, 100, "settings takes the stick too");
        tilt(&view, 0.0, 0.0);
        settings.borrow_mut().apply(xem_settings::SettingChange::BindGame {
            context: xem_settings::InputContext::Play,
            action: xem_settings::GameAction::Menu,
            binding: xem_settings::GameBinding::parse("Tab", "", ""),
        }).unwrap();
        settings.borrow_mut().apply(xem_settings::SettingChange::BindGame {
            context: xem_settings::InputContext::Play,
            action: xem_settings::GameAction::Confirm,
            binding: xem_settings::GameBinding::parse("KeyE", "West", "MouseLeft"),
        }).unwrap();
        settings.borrow_mut().apply(xem_settings::SettingChange::BindGame {
            context: xem_settings::InputContext::Play,
            action: xem_settings::GameAction::Menu,
            binding: xem_settings::GameBinding::parse("Tab", "South", ""),
        }).unwrap();
        // Back to play: the new bindings hold.
        view.set_panel_visible(false);
        press(&view, &[SDL_GAMEPAD_BUTTON_WEST]);
        assert_eq!(frame(&mut view), button::CROSS, "West confirms now");
        press(&view, &[]);
        assert_eq!(frame(&mut view), 0);
        // The open panel takes the gamepad: East closes it, and what closed
        // it reaches the game only when pressed again.
        view.set_panel_visible(true);
        let poll = |view: &mut View| {
            view.poll_gamepad();
            view.pad_buttons(PadContext::default(), None)
        };
        assert_eq!(poll(&mut view), 0);
        press(&view, &[SDL_GAMEPAD_BUTTON_EAST]);
        assert_eq!(poll(&mut view), 0, "East closed the panel");
        assert!(!view.panel_visible());
        assert_eq!(poll(&mut view), 0, "and is the panel's while held");
        press(&view, &[]);
        assert_eq!(poll(&mut view), 0);
        press(&view, &[SDL_GAMEPAD_BUTTON_EAST]);
        assert_ne!(poll(&mut view), 0, "pressed again, it plays");
        press(&view, &[]);
        view.gamepad_removed(again);
        unsafe { SDL_DetachVirtualJoystick(again) };
    }

    #[test]
    fn a_confirmed_quit_ends_the_run_and_hiding_the_panel_drops_the_question() {
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (640, 480), 1.0, settings, true).unwrap();
        assert!(view.ui().component().get_can_quit(), "desktop and Android offer Quit");
        view.ui().component().set_quit_confirming(true);
        view.set_panel_visible(false);
        assert!(!view.ui().component().get_quit_confirming(), "a hidden panel asks nothing");
        assert!(!view.quit_requested());
        view.set_panel_visible(true);
        view.ui().component().invoke_quit_requested();
        assert!(view.quit_requested(), "the confirmed Quit");
    }

    #[test]
    fn every_open_gamepad_plays_not_only_the_first() {
        use xem_core::pad::button;
        // A handheld's built-in controller can follow another device Android
        // counts as a gamepad and that never moves (issue #92).
        assert!(unsafe { SDL_Init(SDL_INIT_GAMEPAD) }, "SDL's gamepads");
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (640, 480), 1.0, settings, false).unwrap();
        let attach = |name: &std::ffi::CStr| {
            let desc = SDL_VirtualJoystickDesc {
                r#type: SDL_JOYSTICK_TYPE_GAMEPAD.0 as u16,
                naxes: SDL_GAMEPAD_AXIS_COUNT.0 as u16,
                nbuttons: SDL_GAMEPAD_BUTTON_COUNT.0 as u16,
                name: name.as_ptr(),
                ..Default::default()
            };
            unsafe { SDL_AttachVirtualJoystick(&desc) }
        };
        let silent = attach(c"silent test pad");
        let played = attach(c"played test pad");
        view.gamepad_added(silent);
        view.gamepad_added(played);
        assert_eq!(view.gamepads.len(), 2, "both open");
        let joystick = unsafe { SDL_GetGamepadJoystick(view.gamepads[1]) };
        let frame = |view: &mut View| {
            view.poll_gamepad();
            view.pad_buttons(PadContext::default(), None)
        };
        unsafe {
            SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH.0, true);
            SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX.0, -32768);
            SDL_UpdateGamepads();
        }
        assert_eq!(frame(&mut view) & button::CROSS, button::CROSS, "the second pad's South is Cross");
        assert_eq!(view.axes[0], -1.0, "and its stick is the left stick");
        unsafe {
            SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH.0, false);
            SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX.0, 0);
            SDL_UpdateGamepads();
        }
        assert_eq!(frame(&mut view) & button::CROSS, 0, "let go");
        drop(view);
        unsafe {
            SDL_DetachVirtualJoystick(silent);
            SDL_DetachVirtualJoystick(played);
        }
    }

    #[test]
    fn a_gamepad_toggles_first_person_holding_back_the_walking_buttons_while_the_player_walks() {
        use xem_core::FieldControls;
        use xem_core::pad::button;
        // SDL's gamepads, with a virtual one (no device or video needed).
        assert!(unsafe { SDL_Init(SDL_INIT_GAMEPAD) }, "SDL's gamepads");
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (640, 480), 1.0, settings, false).unwrap();
        let desc = SDL_VirtualJoystickDesc {
            r#type: SDL_JOYSTICK_TYPE_GAMEPAD.0 as u16,
            naxes: SDL_GAMEPAD_AXIS_COUNT.0 as u16,
            nbuttons: SDL_GAMEPAD_BUTTON_COUNT.0 as u16,
            name: c"first-person test pad".as_ptr(),
            ..Default::default()
        };
        let id = unsafe { SDL_AttachVirtualJoystick(&desc) };
        let pad = unsafe { SDL_OpenGamepad(id) };
        assert!(!pad.is_null(), "the virtual gamepad opens");
        view.gamepads.push(pad);
        let joystick = unsafe { SDL_GetGamepadJoystick(pad) };
        let set = |held: &[SDL_GamepadButton]| unsafe {
            for button in 0..SDL_GAMEPAD_BUTTON_COUNT.0 {
                SDL_SetJoystickVirtualButton(joystick, button, held.iter().any(|b| b.0 == button));
            }
            SDL_UpdateGamepads();
        };
        // A host frame: the pad as update_app takes it, then the game's buttons.
        let frame = |view: &mut View, field: FieldControls| {
            (view.pad, view.axes) = standard_pad(&view.gamepads);
            view.app_input.pad(&view.pad);
            view.pad_buttons(PadContext { field, ..PadContext::default() }, None)
        };
        let walking = FieldControls { field: true, walking: true, menu: false, picture: false };
        let menu = FieldControls { walking: false, menu: true, ..walking };
        let walk = button::UP | button::TRIANGLE | button::L1;
        set(&[SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER]);
        assert_eq!(frame(&mut view, walking) & walk, walk, "off, the character walks, jumps and turns the camera");
        set(&[SDL_GAMEPAD_BUTTON_RIGHT_STICK, SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER]);
        let held = frame(&mut view, walking);
        assert!(view.renderer.first_person(), "the right stick's click toggles the view");
        assert_eq!(held & (walk | button::R3), 0, "on, those are held back and the click is the view's");
        assert_eq!(frame(&mut view, menu) & walk, walk, "a menu keeps them");
        set(&[SDL_GAMEPAD_BUTTON_BACK]);
        assert_eq!(frame(&mut view, walking) & button::SELECT, 0, "Select is the view's here");
        set(&[]);
        frame(&mut view, walking);
        assert!(!view.renderer.first_person(), "Back released alone toggles it off");
        for chord in [&[SDL_GAMEPAD_BUTTON_BACK][..], &[SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_START], &[]] {
            set(chord);
            frame(&mut view, walking);
        }
        assert!(!view.renderer.first_person(), "Back in a chord (Back+Start) does not");
        set(&[SDL_GAMEPAD_BUTTON_BACK]);
        assert_eq!(frame(&mut view, FieldControls { picture: true, ..walking }) & button::SELECT, button::SELECT, "where the map's picture shows, Select is the game's");
        drop(view);
        unsafe {
            SDL_DetachVirtualJoystick(id);
            SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        }
    }

    #[test]
    fn the_touch_decks_look_takes_the_stick_away_a_finger_on_the_picture_turns_the_view_and_map_is_the_games_select() {
        use xem_assist::touch::{Control, Scene};
        use xem_core::FieldControls;
        use xem_core::context::Context;
        use xem_core::pad::button;
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        settings.borrow_mut().apply(xem_settings::SettingChange::RunByDefault(true)).unwrap();
        let (width, height) = (1280u32, 720u32);
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (width, height), 1.0, settings, false).unwrap();
        let black: Vec<_> = (0..320 * 240).flat_map(|_| [0, 0, 0, 255]).collect();
        view.set_game_image(&gpu, (320, 240), &black);
        let target = gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("touch target"),
            size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8UnormSrgb,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        view.render(&gpu, &target.create_view(&Default::default()), 0.0).unwrap();
        view.arrange(Context::Walk);
        let walking = FieldControls { field: true, walking: true, menu: false, picture: false };
        let context = PadContext { field: walking, ui: Context::Walk, ..PadContext::default() };
        let across = |(x, y): (f32, f32)| (x / width as f32, y / height as f32);
        let place = |view: &View, control: Control| view.deck.controls.iter().find(|(k, _)| *k == control).map(|(_, r)| r.centre());
        let look = place(&view, Control::Look).expect("LOOK where the player walks");
        // The walking deck has no SELECT (the game's shows nothing here):
        // LOOK is the view's toggle.
        assert_eq!(place(&view, Control::Select), None);
        // Where the stick rests (beside the picture), and pushed up from there.
        let stick = view.deck.stick.expect("a stick");
        let (rest, up) = (stick.rest, (stick.rest.0, stick.rest.1 - stick.reach));
        // Off, the stick pushed up walks and (by default) runs.
        view.finger(Finger::Down, 1, across(rest));
        view.finger(Finger::Moved, 1, across(up));
        assert_eq!(view.pad_buttons(context, None), button::UP | button::CIRCLE);
        view.finger(Finger::Up, 1, across(up));
        // LOOK toggles the view, and holds nothing of the game's.
        view.finger(Finger::Down, 2, across(look));
        assert_eq!(view.pad_buttons(context, None), 0);
        assert!(view.first_person(), "the deck's LOOK toggles the view");
        view.finger(Finger::Up, 2, across(look));
        assert_eq!(view.pad_buttons(context, None), 0);
        // On, the deck (laid out again, as each frame) has no stick, jump
        // or camera turns: a finger pushed up where the stick rested holds
        // nothing for the game, no direction and no Run left alone.
        view.arrange(Context::Walk);
        assert!(view.deck.stick.is_none(), "no stick while the view shows");
        assert_eq!((place(&view, Control::Jump), place(&view, Control::CameraLeft), place(&view, Control::CameraRight)), (None, None, None), "nor what the view holds back");
        assert_eq!(place(&view, Control::Look), Some(look), "LOOK stays where it was, to leave the view");
        view.finger(Finger::Down, 3, across(rest));
        view.finger(Finger::Moved, 3, across(up));
        assert_eq!(view.pad_buttons(context, None), 0, "the character stays where it stands");
        view.finger(Finger::Up, 3, across(up));
        // A finger dragged right on the picture turns the view left (the
        // scene follows it), and that drag is no tap.
        let picture = view.picture.expect("the picture's place").placed;
        let middle = (picture.x + picture.width / 2.0, picture.y + picture.height / 2.0);
        view.finger(Finger::Down, 4, across(middle));
        view.finger(Finger::Moved, 4, across((middle.0 + 100.0, middle.1)));
        view.pad_buttons(context, None);
        let (yaw, pitch) = view.renderer.look_angles().expect("on");
        assert!((yaw + 0.3).abs() < 1e-3 && pitch.abs() < 1e-3, "100 logical pixels turn it 0.3 radians: {yaw} {pitch}");
        view.finger(Finger::Up, 4, across((middle.0 + 100.0, middle.1)));
        assert!(view.taps.is_empty(), "a drag is no tap");
        // LOOK again: the view goes, and the stick walks again.
        view.finger(Finger::Down, 2, across(look));
        view.pad_buttons(context, None);
        view.finger(Finger::Up, 2, across(look));
        assert!(!view.first_person());
        view.arrange(Context::Walk);
        view.finger(Finger::Down, 1, across(rest));
        view.finger(Finger::Moved, 1, across(up));
        assert_eq!(view.pad_buttons(context, None), button::UP | button::CIRCLE);
        view.finger(Finger::Up, 1, across(up));
        // Where Select shows the map's picture the deck has MAP: the
        // game's Select, toggling no view.
        view.arrange(Scene { picture: true, ..Context::Walk.into() });
        let map = place(&view, Control::Select).expect("MAP where Select shows a picture");
        assert_eq!(Control::Select.label(view.deck.scene()), "MAP");
        view.finger(Finger::Down, 2, across(map));
        assert_eq!(view.pad_buttons(PadContext { field: FieldControls { picture: true, ..walking }, ..context }, None), button::SELECT);
        assert!(!view.first_person());
    }

    #[test]
    fn select_held_over_a_dialogue_hides_its_windows_and_stays_the_games() {
        use xem_assist::touch::Control;
        use xem_core::FieldControls;
        use xem_core::context::Context;
        use xem_core::pad::button;
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        let (width, height) = (1280u32, 720u32);
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (width, height), 1.0, settings, false).unwrap();
        let black: Vec<_> = (0..320 * 240).flat_map(|_| [0, 0, 0, 255]).collect();
        view.set_game_image(&gpu, (320, 240), &black);
        let target = gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("touch target"),
            size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8UnormSrgb,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        view.render(&gpu, &target.create_view(&Default::default()), 0.0).unwrap();
        let ui = Context::Dialogue { choice: None };
        let dialogue = PadContext { field: FieldControls { field: true, ..FieldControls::default() }, ui, ..PadContext::default() };
        let at = |view: &View| {
            let (x, y) = view.deck.controls.iter().find(|(c, _)| *c == Control::Select).unwrap().1.centre();
            (x / width as f32, y / height as f32)
        };
        view.arrange(ui);
        let select = at(&view);
        // The deck's SELECT held: the game has Select, the view hides the
        // dialogue's windows.
        view.finger(Finger::Down, 1, select);
        assert_eq!(view.pad_buttons(dialogue, None), button::SELECT);
        assert!(view.renderer.dialogue_hidden() && !view.first_person(), "hidden, and no view toggled");
        view.finger(Finger::Up, 1, select);
        assert_eq!(view.pad_buttons(dialogue, None), 0);
        assert!(!view.renderer.dialogue_hidden(), "let go, they show");
        // Held as the settings panel opens: they show under it.
        view.finger(Finger::Down, 2, select);
        view.pad_buttons(dialogue, None);
        view.set_panel_visible(true);
        assert_eq!(view.pad_buttons(dialogue, None), 0);
        assert!(!view.renderer.dialogue_hidden(), "the panel has the input");
        view.set_panel_visible(false);
        view.finger(Finger::Up, 2, select);
        assert_eq!(view.pad_buttons(dialogue, None), 0);
        // Where the player walks no dialogue shows: the deck has no HIDE
        // there (nor a SELECT: LOOK toggles the view).
        view.arrange(Context::Walk);
        assert!(view.deck.controls.iter().all(|(c, _)| *c != Control::Select));
        assert_eq!(Control::Select.label(ui.into()), "HIDE");
    }

    #[test]
    fn the_decks_look_toggles_first_person_anywhere_in_a_field_but_its_menu_and_is_lit_while_it_shows() {
        use xem_assist::touch::Control;
        use xem_core::FieldControls;
        use xem_core::context::Context;
        use xem_settings::look::Command;
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        let (width, height) = (1280u32, 720u32);
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (width, height), 1.0, settings.clone(), false).unwrap();
        let black: Vec<_> = (0..320 * 240).flat_map(|_| [0, 0, 0, 255]).collect();
        view.set_game_image(&gpu, (320, 240), &black);
        let target = gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("touch target"),
            size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8UnormSrgb,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        let output = target.create_view(&Default::default());
        view.render(&gpu, &output, 0.0).unwrap();
        // A finger has touched the game: the deck shows.
        view.touched = true;
        let across = |(x, y): (f32, f32)| (x / width as f32, y / height as f32);
        // The key's pixels as drawn (summed brightness): lit while the view shows.
        let drawn = |view: &mut View, key: xem_assist::touch::Rect| -> u64 {
            view.render(&gpu, &output, 0.0).unwrap();
            let image = xem_render::read_rgba8(&gpu.device, &gpu.queue, &target);
            let inside = |i: usize| key.contains(((i % width as usize) as f32, (i / width as usize) as f32));
            image.chunks_exact(4).enumerate().filter(|(i, _)| inside(*i)).map(|(_, p)| u64::from(p[0]) + u64::from(p[1]) + u64::from(p[2])).sum()
        };
        // A dialogue's (where Select toggles nothing) and a walk where
        // Select shows the map's picture: LOOK toggles the view in both, and
        // presses nothing of the game's.
        let dialogue = FieldControls { field: true, walking: false, menu: false, picture: false };
        let picture = FieldControls { field: true, walking: true, menu: false, picture: true };
        for (field, ui) in [(dialogue, Context::Dialogue { choice: None }), (picture, Context::Walk)] {
            view.arrange(ui);
            let look = view.deck.controls.iter().find(|(c, _)| *c == Control::Look).expect("LOOK in a field").1;
            let context = PadContext { field, ui, ..PadContext::default() };
            let off = drawn(&mut view, look);
            for on in [true, false] {
                view.finger(Finger::Down, 1, across(look.centre()));
                assert_eq!(view.pad_buttons(context, None), 0, "{ui:?}: LOOK holds none of the game's buttons");
                assert_eq!(view.first_person(), on, "{ui:?}: LOOK toggles the view");
                view.finger(Finger::Up, 1, across(look.centre()));
                assert_eq!(view.pad_buttons(context, None), 0);
                assert_eq!(view.first_person(), on, "{ui:?}: once a press");
                if on {
                    assert_ne!(drawn(&mut view, look), off, "{ui:?}: LOOK is lit while the view shows");
                }
            }
            assert_eq!(drawn(&mut view, look), off, "{ui:?}: and unlit after");
        }
        // Not under the field's menu (it has the screen) nor the Original
        // renderer (no view to look around in): nothing changes.
        let menu = FieldControls { menu: true, ..dialogue };
        assert!(view.first_person_command(&Command::TOGGLE, menu).unwrap_err().contains("menu"));
        assert!(!view.first_person());
        view.arrange(Context::Walk);
        let look = view.deck.controls.iter().find(|(c, _)| *c == Control::Look).unwrap().1;
        let graphics = settings.borrow().settings().graphics;
        settings.borrow_mut().apply(xem_settings::SettingChange::Graphics(xem_settings::Graphics { renderer: xem_settings::Renderer::Original, ..graphics })).unwrap();
        view.finger(Finger::Down, 2, across(look.centre()));
        view.pad_buttons(PadContext { field: picture, ui: Context::Walk, ..PadContext::default() }, None);
        view.finger(Finger::Up, 2, across(look.centre()));
        assert!(!view.first_person(), "the Original renderer has no view to toggle");
        settings.borrow_mut().apply(xem_settings::SettingChange::Graphics(graphics)).unwrap();
        // An agent's command: on, looking 30 degrees right and 10 down.
        let reply = view.first_person_command(&Command { on: Some(true), yaw: Some(30.0), pitch: Some(-10.0) }, picture).unwrap();
        assert_eq!(reply, serde_json::json!({"first_person": true, "look": {"yaw": 30.0, "pitch": -10.0}}));
        assert!(view.first_person());
    }

    /// The title movie through the desktop's and Android's view (local only:
    /// disc 1 and the game module; lavapipe), from its frame 840 with wider
    /// framing on, in a 1366x768 target whose safe area leaves out 60 pixels
    /// each side and 34 above and below (as a phone's insets do): under the
    /// Fit and the Integer scale the frame is rebuilt at the display's size
    /// placed in the safe area (not fitted to the whole target) and shown
    /// there at that size (enlarged once), centred, its picture without
    /// wider framing's sides (the view's own frames have them); under Force
    /// 4:3 likewise, reshaped and inside the safe area (Integer stepping the
    /// display's lines), the view's frames without sides (Widescreen's
    /// choice kept); under the Original movies setting the device's own
    /// picture.
    #[cfg(all(has_game_module, not(debug_assertions)))]
    #[test]
    fn a_rebuilt_movie_frame_is_made_at_its_placed_size_in_the_safe_area_without_sides() {
        use xem_settings::{MovieFilter, Scale, SettingChange};
        let disc = std::env::var_os("XEM_DISC1").map(std::path::PathBuf::from)
            .unwrap_or_else(|| std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
        if !disc.exists() {
            eprintln!("skipping: no disc 1 (set XEM_DISC1)");
            return;
        }
        let (session, _) = xem_game::boot(&disc, &Default::default()).unwrap();
        let mut game = Game::new(session);
        for _ in 0..840 {
            assert!(game.session.frame().unwrap());
        }
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        let wide = xem_settings::Graphics { widescreen: true, ..settings.borrow().settings().graphics };
        settings.borrow_mut().apply(SettingChange::Graphics(wide)).unwrap();
        let (width, height) = (1366u32, 768u32);
        let safe = Rect { x: 60.0, y: 34.0, width: (width - 120) as f32, height: (height - 68) as f32 };
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (width, height), 1.0, settings.clone(), false).unwrap();
        view.set_safe_area((60, 34, width - 120, height - 68));
        let target = gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("movie target"),
            size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8UnormSrgb,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        let output = target.create_view(&Default::default());
        // A host frame: a game frame, taken and presented.
        let step = |view: &mut View, game: &mut Game| {
            assert!(game.session.frame().unwrap());
            view.set_game_frame(&gpu, game);
            view.render(&gpu, &output, 0.0).unwrap();
        };
        for (force_4x3, scale) in [(false, Scale::Fit), (false, Scale::Integer), (true, Scale::Fit), (true, Scale::Integer)] {
            let forced = xem_settings::Graphics { force_4x3, ..wide };
            settings.borrow_mut().apply(SettingChange::Graphics(forced)).unwrap();
            settings.borrow_mut().apply(SettingChange::Scale(scale)).unwrap();
            let case = format!("{scale:?}{}", if force_4x3 { ", Force 4:3" } else { "" });
            // Rebuilt within two movie frames (the trace starts with the
            // view's first frame).
            let mut waited = 0;
            while { step(&mut view, &mut game); !view.presented_movie() } {
                waited += 1;
                assert!(waited <= 8, "{case}: no rebuilt frame within two movie frames");
            }
            let display = game.display_info();
            let placed = present_rect((display.width, display.height), (safe.width as u32, safe.height as u32), scale, forced.forced_aspect());
            let made = view.renderer.presented_texture().map(|t| (t.width(), t.height())).expect("the rebuilt frame");
            let picture = view.picture.expect("the picture's place");
            let shown = picture.placed;
            eprintln!("{case}: {}x{} display rebuilt at {made:?}, placed at {shown:?}", display.width, display.height);
            assert!(display.color24 && (display.width, display.height) == (320, 216), "{case}: the title movie's display ({display:?})");
            assert_eq!(made, (placed.width.round() as u32, placed.height.round() as u32), "{case}: made at the display's size placed in the safe area");
            assert_eq!((shown.width.round() as u32, shown.height.round() as u32), made, "{case}: shown at the size it was made at");
            assert_eq!(
                (shown.x, shown.y),
                (safe.x + ((safe.width - shown.width) / 2.0).round(), safe.y + ((safe.height - shown.height) / 2.0).round()),
                "{case}: centred in the safe area"
            );
            assert!(
                shown.x >= safe.x && shown.y >= safe.y
                    && shown.x + shown.width <= safe.x + safe.width + 0.5 && shown.y + shown.height <= safe.y + safe.height + 0.5,
                "{case}: inside the safe area"
            );
            assert_eq!((picture.display, picture.side), ((320.0, 216.0), 0.0), "{case}: the display alone, without wider framing's sides");
            assert_eq!(
                view.renderer.config().map(|c| c.side > 0),
                Some(!force_4x3),
                "{case}: the view's own frames have sides unless forced to 4:3"
            );
        }
        // The Original movies setting: the device's own picture, as it is.
        let original = xem_settings::Graphics {
            filtering: xem_settings::TextureFiltering { movies: MovieFilter::Original, ..wide.filtering },
            ..wide
        };
        settings.borrow_mut().apply(SettingChange::Graphics(original)).unwrap();
        for _ in 0..4 {
            step(&mut view, &mut game);
            assert!(!view.presented_movie(), "rebuilt under Original");
        }
        let picture = view.picture.expect("the picture's place");
        assert_eq!((picture.display, picture.side), ((320.0, 216.0), 0.0), "the device's own 320x216 picture");
    }

    /// Taps point through the picture the view last presented, and through
    /// the game's camera while the host shows the device's own picture: in
    /// field 30 with the camera pulled back (wider views' zoom out at 150%;
    /// at 250% the middle of every walkable triangle that picture shows
    /// lies within a tap's reach of an actor) a tap where the picture shows
    /// a floor point walks there, its mark lying there; under the Original
    /// renderer the view gives no viewpoint and a tap walks to the floor the
    /// game's camera shows there (through the last picture's viewpoint it
    /// would pick elsewhere or nothing), after the pulled-back picture and
    /// after first-person inspection (which the Original renderer ends);
    /// presented again, the pulled-back viewpoint returns.
    #[cfg(all(has_game_module, not(debug_assertions)))]
    #[test]
    fn taps_point_through_the_picture_presented_and_through_the_game_camera_under_the_original_renderer() {
        use xem_assist::dispatch::{Out, Pointed, tap_at};
        use xem_assist::overlay::{Kind, Place, seen};
        use xem_core::context::Context;
        use xem_core::field::{Camera, Field, Reach, V3};
        use xem_settings::{Renderer, SettingChange};
        let disc = std::env::var_os("XEM_DISC1").map(std::path::PathBuf::from)
            .unwrap_or_else(|| std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
        if !disc.exists() {
            eprintln!("skipping: no disc 1 (set XEM_DISC1)");
            return;
        }
        let (session, _) = xem_game::boot(&disc, &Default::default()).unwrap();
        let mut game = Game::new(session);
        let run = |game: &mut Game, command: serde_json::Value| {
            game.command(&command).unwrap_or_else(|error| panic!("{command}: {error}"));
        };
        run(&mut game, serde_json::json!({"cmd": "enter_field", "map": 30}));
        run(&mut game, serde_json::json!({"cmd": "frames", "count": 300}));
        for _ in 0..120 {
            match game.context() {
                Context::Walk => break,
                Context::Dialogue { .. } => run(&mut game, serde_json::json!({"cmd": "act", "actions": ["Confirm"], "frames": 2})),
                _ => {}
            }
            run(&mut game, serde_json::json!({"cmd": "frames", "count": 30}));
        }
        assert_eq!(game.context(), Context::Walk, "Fei walks in field 30");
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        let graphics = |renderer: Renderer| SettingChange::Graphics(xem_settings::Graphics { renderer, zoom_out: 150, ..Default::default() });
        settings.borrow_mut().apply(graphics(Renderer::ExMachina)).unwrap();
        // A wide window: the touch deck stands beside the picture.
        let (width, height) = (1600u32, 720u32);
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (width, height), 1.0, settings.clone(), false).unwrap();
        let target = gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("pointing target"),
            size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8UnormSrgb,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        let output = target.create_view(&Default::default());
        // Host frames: a game frame each, taken and presented.
        let mut time = 0.0;
        let mut frames = |view: &mut View, game: &mut Game, count: usize| {
            for _ in 0..count {
                assert!(game.session.frame().unwrap());
                view.set_game_frame(&gpu, game);
                time += 1.0 / 60.0;
                view.render(&gpu, &output, time).unwrap();
            }
        };
        // A tap on the picture where it shows the screen point `at`, carried
        // out as the host's frame does (`View::assist`): where the walk it
        // begins heads and where the destination's mark lies (the walk is
        // stopped again, Fei standing where he stands).
        let tap = |view: &mut View, game: &mut Game, at: (f32, f32)| {
            view.assist(game);
            let (x, y) = view.picture.expect("the picture's place").from_screen(at);
            let touched = (x / width as f32, y / height as f32);
            view.finger(Finger::Down, 1, touched);
            view.finger(Finger::Up, 1, touched);
            view.assist(game);
            // The next picture presented places the mark.
            view.render(&gpu, &output, 1000.0).unwrap();
            let to = game.destination(None);
            let mark = view.marks.iter().find(|mark| mark.kind == Kind::Destination).and_then(|mark| match mark.place {
                Place::Screen { centre, .. } => Some(centre),
                Place::Floor { .. } => None,
            });
            run(game, serde_json::json!({"cmd": "assist_stop"}));
            (to, mark)
        };
        let apart = |a: (f32, f32), b: (f32, f32)| ((a.0 - b.0).powi(2) + (a.1 - b.1).powi(2)).sqrt();
        // Walkable floor some way off Fei and away from everyone, where the
        // picture `shown` sees shows it and a tap there walks (no one within
        // its reach): the point `other` puts farthest from there, and how far.
        let floor = |shown: &Field, other: &Camera| -> (V3, (f32, f32), f32) {
            let me = *shown.player().expect("Fei");
            let mesh = &shown.layers[me.layer];
            (0..mesh.triangles.len())
                .map(|t| (t, mesh.centroid(t)))
                .filter(|(t, c)| shown.reach(&me, me.layer, *t, *c) == Reach::Walk && (60.0..240.0).contains(&c.planar(me.position)))
                .filter(|(_, c)| shown.actors.iter().flatten().all(|a| a.index == me.index || a.position.planar(*c) > 40.0))
                .filter_map(|(_, c)| {
                    let at = shown.camera.project(c).filter(|p| (16.0..304.0).contains(&p.0) && (16.0..208.0).contains(&p.1))?;
                    let walks = matches!(tap_at(Pointed::Screen(at), Context::Walk, Some(shown), None),
                        Some(Out::Command { command }) if command["cmd"] == "navigate");
                    if !walks {
                        return None;
                    }
                    Some((c, at, apart(at, other.project(c)?)))
                })
                .max_by(|a, b| a.2.total_cmp(&b.2))
                .expect("walkable floor a tap reaches on the picture")
        };
        // The Original renderer's picture after `last` (the viewpoint the
        // view presented before it): no viewpoint, and a tap walks to the
        // floor the game's camera shows; then the view's renderer again.
        let original = |view: &mut View, game: &mut Game, frames: &mut dyn FnMut(&mut View, &mut Game, usize), last: xem_render::ps1::Viewpoint, what: &str| {
            settings.borrow_mut().apply(graphics(Renderer::Original)).unwrap();
            frames(view, game, 2);
            assert!(!view.first_person(), "{what}: the Original renderer ends inspection");
            assert_eq!(view.renderer.viewpoint(), None, "{what}: the device's own picture is the game camera's");
            let field = game.field().expect("field 30");
            let stale = seen(field.clone(), None, Some(&last));
            let (goal, at, shift) = floor(&field, &stale.camera);
            let elsewhere = stale.pick(at).map(|hit| hit.point.planar(goal));
            eprintln!("{what}: the floor at {goal:?} shows at {at:?}, {shift:.1} pixels from the last picture's place; through it a tap there picks {elsewhere:?} units away");
            assert!(elsewhere.is_none_or(|d| d > 32.0), "{what}: through the last picture's viewpoint {at:?} picks elsewhere");
            let (to, mark) = tap(view, game, at);
            assert!(to.is_some_and(|to| to.planar(goal) < 16.0), "{what}: a tap walks to the floor the game's camera shows: {to:?} for {goal:?}");
            assert!(mark.is_some_and(|mark| apart(mark, at) < 2.0), "{what}: the destination's mark lies there: {mark:?} for {at:?}");
            settings.borrow_mut().apply(graphics(Renderer::ExMachina)).unwrap();
        };
        // Pulled back: through the picture's viewpoint.
        frames(&mut view, &mut game, 8);
        let pulled = view.renderer.viewpoint().expect("the picture shows the field pulled back");
        let field = game.field().expect("field 30");
        let shown = seen(field.clone(), view.renderer.drawn_camera(), Some(&pulled));
        let (goal, at, shift) = floor(&shown, &field.camera);
        eprintln!("pulled back {:.0} units: the floor at {goal:?} shows at {at:?}, {shift:.1} pixels from where the game's camera puts it", pulled.offset[2]);
        assert!(shift > 10.0, "the game's camera shows {goal:?} elsewhere than {at:?}");
        let (to, mark) = tap(&mut view, &mut game, at);
        assert!(to.is_some_and(|to| to.planar(goal) < 16.0), "pulled back, a tap walks to the floor the picture shows: {to:?} for {goal:?}");
        assert!(mark.is_some_and(|mark| apart(mark, at) < 2.0), "the destination's mark lies there: {mark:?} for {at:?}");
        let last = view.renderer.viewpoint().expect("pulled back");
        original(&mut view, &mut game, &mut frames, last, "after the pulled-back picture");
        frames(&mut view, &mut game, 8);
        assert!(view.renderer.viewpoint().is_some_and(|v| v.rows == pulled.rows && v.offset[2] > 0.0), "presented again, pulled back");
        // From within the scene: the look's viewpoint, then the Original
        // renderer.
        view.renderer.set_first_person(true);
        frames(&mut view, &mut game, 8);
        assert!(view.first_person(), "inspection looks from within field 30");
        let look = view.renderer.viewpoint().expect("the look's viewpoint");
        assert_ne!(look.rows, pulled.rows, "from the character's eye, not the camera pulled back");
        original(&mut view, &mut game, &mut frames, look, "after first-person inspection");
    }

    /// Mouse clicks while Fei walks in field 30 go where taps would,
    /// through the picture the view presented (the camera pulled back at
    /// 150%), with the mouse's binding (Confirm) never held: one where the
    /// picture shows a floor point walks there, its mark lying there, and
    /// one on someone the picture shows goes to talk to them. A click beside
    /// the picture is the mouse's binding, held until the button comes up,
    /// and so is one during first-person inspection, which shows no field
    /// camera to walk through.
    #[cfg(all(has_game_module, not(debug_assertions)))]
    #[test]
    fn mouse_clicks_walk_and_talk_where_taps_would_and_elsewhere_are_the_mouses_binding() {
        use xem_assist::dispatch::click_at;
        use xem_assist::overlay::{Kind, Place};
        use xem_core::assist::Intent;
        use xem_core::context::Context;
        use xem_core::field::Reach;
        use xem_settings::SettingChange;
        let disc = std::env::var_os("XEM_DISC1").map(std::path::PathBuf::from)
            .unwrap_or_else(|| std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
        if !disc.exists() {
            eprintln!("skipping: no disc 1 (set XEM_DISC1)");
            return;
        }
        let (session, _) = xem_game::boot(&disc, &Default::default()).unwrap();
        let mut game = Game::new(session);
        let run = |game: &mut Game, command: serde_json::Value| {
            game.command(&command).unwrap_or_else(|error| panic!("{command}: {error}"));
        };
        run(&mut game, serde_json::json!({"cmd": "enter_field", "map": 30}));
        run(&mut game, serde_json::json!({"cmd": "frames", "count": 300}));
        for _ in 0..120 {
            match game.context() {
                Context::Walk => break,
                Context::Dialogue { .. } => run(&mut game, serde_json::json!({"cmd": "act", "actions": ["Confirm"], "frames": 2})),
                _ => {}
            }
            run(&mut game, serde_json::json!({"cmd": "frames", "count": 30}));
        }
        assert_eq!(game.context(), Context::Walk, "Fei walks in field 30");
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let settings = Rc::new(RefCell::new(SettingsService::with_defaults(xem_settings::MemoryStore::default())));
        settings.borrow_mut().apply(SettingChange::Graphics(xem_settings::Graphics { renderer: xem_settings::Renderer::ExMachina, zoom_out: 150, ..Default::default() })).unwrap();
        let (width, height) = (1600u32, 720u32);
        let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (width, height), 1.0, settings.clone(), false).unwrap();
        let target = gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("clicking target"),
            size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8UnormSrgb,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        let output = target.create_view(&Default::default());
        let mut time = 0.0;
        let mut frames = |view: &mut View, game: &mut Game, count: usize| {
            for _ in 0..count {
                assert!(game.session.frame().unwrap());
                view.set_game_frame(&gpu, game);
                time += 1.0 / 60.0;
                view.render(&gpu, &output, time).unwrap();
            }
        };
        frames(&mut view, &mut game, 8);
        assert!(view.renderer.viewpoint().is_some(), "the picture shows the field pulled back");
        view.assist(&mut game);
        let shown = view.walking.clone().expect("the field the picture shows while Fei walks");
        let picture = view.picture.expect("the picture's place");
        // A left click at `pixels` (target pixels), carried out as the
        // host's frame does (`View::assist`): the mouse's buttons it held.
        let click = |view: &mut View, game: &mut Game, pixels: (f32, f32)| -> u8 {
            view.mouse_button(0, true, pixels);
            let held = view.player.mouse.buttons();
            view.mouse_button(0, false, pixels);
            assert_eq!(view.player.mouse.buttons(), 0, "up, the button holds nothing");
            view.assist(game);
            // The next picture presented places the mark.
            view.render(&gpu, &output, 1000.0).unwrap();
            held
        };
        let on_picture = |at: &(f32, f32)| (16.0..304.0).contains(&at.0) && (16.0..208.0).contains(&at.1);
        // Walkable floor some way off Fei and away from everyone that the
        // picture shows.
        let me = *shown.player().expect("Fei");
        let mesh = &shown.layers[me.layer];
        let (goal, at) = (0..mesh.triangles.len())
            .map(|t| (t, mesh.centroid(t)))
            .filter(|(t, c)| shown.reach(&me, me.layer, *t, *c) == Reach::Walk && (60.0..240.0).contains(&c.planar(me.position)))
            .filter(|(_, c)| shown.actors.iter().flatten().all(|a| a.index == me.index || a.position.planar(*c) > 40.0))
            .find_map(|(_, c)| {
                let at = shown.camera.project(c).filter(on_picture)?;
                click_at(at, &shown).is_some_and(|command| command["cmd"] == "navigate").then_some((c, at))
            })
            .expect("walkable floor a click reaches on the picture");
        assert_eq!(click(&mut view, &mut game, picture.from_screen(at)), 0, "a click on the floor holds no binding");
        let to = game.destination(None);
        assert!(to.is_some_and(|to| to.planar(goal) < 16.0), "a click walks to the floor the picture shows: {to:?} for {goal:?}");
        let mark = view.marks.iter().find(|mark| mark.kind == Kind::Destination).and_then(|mark| match mark.place {
            Place::Screen { centre, .. } => Some(centre),
            Place::Floor { .. } => None,
        });
        assert!(mark.is_some_and(|mark| (mark.0 - at.0).abs() < 2.0 && (mark.1 - at.1).abs() < 2.0), "the destination's mark lies there: {mark:?} for {at:?}");
        run(&mut game, serde_json::json!({"cmd": "assist_stop"}));
        // Someone the picture shows.
        let (actor, at) = shown.actors.iter().flatten()
            .filter(|a| a.index != me.index)
            .find_map(|a| {
                let at = shown.camera.project(a.position).filter(on_picture)?;
                let command = click_at(at, &shown)?;
                (command["cmd"] == "interact").then(|| (command["actor"].as_u64().expect("an actor") as usize, at))
            })
            .expect("someone a click reaches on the picture");
        assert_eq!(click(&mut view, &mut game, picture.from_screen(at)), 0, "a click on someone holds no binding");
        let intent = game.session.assist.as_ref().map(|assist| assist.intent.clone());
        assert!(matches!(intent, Some(Intent::Interact { actor: a }) if a == actor), "a click goes to talk to actor {actor}: {intent:?}");
        run(&mut game, serde_json::json!({"cmd": "assist_stop"}));
        // Beside the picture: the mouse's binding.
        let beside = (2.0, height as f32 / 2.0);
        assert_eq!(picture.to_screen(beside.0, beside.1), None, "{picture:?}");
        assert_eq!(click(&mut view, &mut game, beside), 1, "beside the picture the click is the mouse's binding");
        assert!(game.session.assist.is_none(), "it walks nowhere");
        // First-person inspection: no field camera to walk through.
        view.renderer.set_first_person(true);
        frames(&mut view, &mut game, 8);
        assert!(view.first_person(), "inspection looks from within field 30");
        view.assist(&mut game);
        assert!(view.walking.is_none(), "inspecting, clicks point into no field");
        assert_eq!(click(&mut view, &mut game, picture.from_screen(at)), 1, "inspecting, the click is the mouse's binding");
        assert!(game.session.assist.is_none(), "inspecting, it goes to no one");
    }

    #[test]
    fn shared_keyboard_codes_map_to_physical_sdl_keys() {
        let mut codes = vec!["ControlLeft".to_string(),"ControlRight".into(),"ShiftLeft".into(),"ShiftRight".into(),
            "AltLeft".into(),"AltRight".into(),"MetaLeft".into(),"MetaRight".into(),"ArrowUp".into(),"ArrowDown".into(),
            "ArrowLeft".into(),"ArrowRight".into(),"Enter".into(),"Space".into(),"Tab".into(),"Backspace".into(),"Delete".into(),
            "Insert".into(),"Home".into(),"End".into(),"PageUp".into(),"PageDown".into(),"CapsLock".into(),"NumLock".into(),
            "ScrollLock".into(),"Pause".into(),"PrintScreen".into(),"BracketLeft".into(),"BracketRight".into(),"Backslash".into(),
            "Semicolon".into(),"Quote".into(),"Backquote".into(),"Comma".into(),"Period".into(),"Slash".into(),"Minus".into(),"Equal".into()];
        codes.extend(('A'..='Z').map(|key| format!("Key{key}")));
        codes.extend((0..=9).flat_map(|n| [format!("Digit{n}"),format!("Numpad{n}")]));
        codes.extend((2..=24).map(|n| format!("F{n}")));
        codes.extend(["Decimal","Add","Subtract","Multiply","Divide","Enter"].map(|key| format!("Numpad{key}")));
        for code in codes { assert_ne!(scancode(&code).0,SDL_SCANCODE_UNKNOWN.0,"{code}"); }
    }

    #[test]
    fn key_latch_preserves_taps_between_frames() {
        let mut latch = KeyLatch::default();
        let sc = scancode("Backquote").0 as u32;
        assert!(!latch.contains(sc));
        latch.set(sc);
        assert!(latch.contains(sc));
        // Out-of-range scancodes are ignored without panicking.
        latch.set(9999);
        assert!(!latch.contains(9999));

        // Simulate a key pressed and released before the next frame's poll:
        // SDL_GetKeyboardState reports false, yet the latched tap fires once.
        let mut app_input = AppInput::default();
        let bindings = xem_settings::Settings::default().app_bindings;
        let latched = latch.contains(sc);
        if latched {
            app_input.key("Backquote", false);
        }
        app_input.key("Backquote", latched);
        latch.clear();
        assert_eq!(
            app_input.requests(&bindings, false),
            vec![AppRequest::Action(xem_settings::AppAction::ToggleFastForward)]
        );
        // Next frame without another press releases the key cleanly.
        assert!(!latch.contains(sc));
        app_input.key("Backquote", false);
        assert!(app_input.requests(&bindings, false).is_empty());
    }
}
