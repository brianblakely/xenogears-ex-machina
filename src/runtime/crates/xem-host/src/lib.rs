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
#[cfg(has_game_module)]
pub mod xr_play;
mod operations;

pub use game::Game;

use sdl3_sys::everything::*;
use std::cell::{Cell, RefCell};
use std::rc::Rc;
use std::time::Instant;
use xem_render::ps1::{GameView, HostClass, ViewConfig};
use xem_render::xem_gpu::DisplayInfo;
use xem_render::{
    Compositor, Gpu, Quad, Rect, SceneRenderer, ViewTarget, Viewport, demo_camera, present_rect,
    wgpu,
};
use xem_settings::{AppInput, AppRequest, Filter, SettingsService};
use xem_ui::{FrameRateUi, Key, LogicalPosition, PointerEventButton, SettingsUi, SharedGpu, WindowEvent};

/// The stand-in game picture's size (the original's typical display mode).
pub const GAME_SIZE: (u32, u32) = (320, 240);
const GAME_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8UnormSrgb;
/// The panel's size in logical pixels.
const PANEL_SIZE: (f32, f32) = (640.0, 560.0);

/// What the game's state says of the player's buttons ([`Game::pad_context`]).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PadContext {
    /// The battle's attack inputs take the buttons (the keyboard's combo
    /// layout, `xem_settings::game_keys`).
    pub combo: bool,
    /// The field's: first-person inspection's toggle and the buttons it
    /// holds back (`xem_settings::look`).
    pub field: xem_core::FieldControls,
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

/// The window's swapchain. The surface is absent while suspended: Android
/// takes the native window away in the background and may replace it (on a
/// configuration change, say) while the app runs.
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
        surface.configure(&gpu.device, &config);
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
            if gone {
                self.surface = None;
            } else if let Some(surface) = &self.surface {
                surface.configure(&gpu.device, &self.config);
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
        surface.configure(&gpu.device, &self.config);
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
                surface.configure(&gpu.device, &self.config);
                None
            }
            wgpu::CurrentSurfaceTexture::Timeout | wgpu::CurrentSurfaceTexture::Occluded => None,
            wgpu::CurrentSurfaceTexture::Outdated => {
                self.resize(gpu, window);
                if let Some(surface) = &self.surface {
                    surface.configure(&gpu.device, &self.config);
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
    /// The player confirmed the panel's Quit: the host ends its run.
    quit: Rc<Cell<bool>>,
    gamepads: Vec<*mut SDL_Gamepad>,
    /// The game's display as the device shows it (24-bit movie frames, and
    /// hosts that set a picture directly), instead of the test scene.
    game_image: Option<(wgpu::Texture, wgpu::TextureView, (u32, u32))>,
    /// The game drawn by the renderer: the trace is pushed into `renderer`
    /// as the game runs; `game_frame` is the display and scene to present.
    renderer: GameView,
    game_frame: Option<(DisplayInfo, String)>,
    /// A new trace arrived since the last presentation.
    fresh_frame: bool,
    host_class: HostClass,
    /// Where the game picture may go: the window's safe area, in pixels.
    safe_area: Option<Rect>,
    app_input: AppInput,
    app_requests: Rc<RefCell<Vec<AppRequest>>>,
    operations: operations::Operations,
    /// Mouse wheel notches not yet turned into zoom steps (smooth wheels).
    wheel: f32,
    /// The game's mouse buttons and camera turn, and whether SDL's relative
    /// mouse mode is on (while the game has the input).
    mouse: xem_settings::game_keys::Mouse,
    relative_mouse: bool,
    /// The first gamepad's buttons as they are (before app chords take
    /// theirs), first-person inspection's toggle, the mouse motion that
    /// turns its view, and when its view last turned.
    pad: [u8; 16],
    look_toggle: xem_settings::look::Toggle,
    look_mouse: [f32; 2],
    looked: Option<Instant>,
    /// The disc the game waits for that the drive's changer has not got.
    wanted_disc: Option<u8>,
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
        ui.set_close_hint(if cfg!(target_os = "android") { "Back closes Settings" } else { "Esc or Back+Start closes Settings" });
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
            quit,
            gamepads: Vec::new(),
            game_image: None,
            renderer: GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter)),
            game_frame: None,
            fresh_frame: false,
            host_class: HostClass::Desktop,
            safe_area: None,
            app_input: AppInput::default(),
            app_requests,
            operations: operations::Operations::default(),
            wheel: 0.0,
            mouse: Default::default(),
            relative_mouse: false,
            pad: [0; 16],
            look_toggle: Default::default(),
            look_mouse: [0.0; 2],
            looked: None,
            wanted_disc: None,
        })
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
        let graphics = self.settings.borrow().settings().graphics;
        let zoomed = graphics.zoomed(steps as i32);
        if zoomed != graphics {
            let _ = self.settings.borrow_mut().apply(xem_settings::SettingChange::Graphics(zoomed));
        }
    }

    pub fn set_state_directory(&mut self, path: std::path::PathBuf) { self.operations.directory = Some(path); }

    /// Runs Settings requests and bound actions under the host's one game owner.
    /// Returns false while the platform file picker has paused play.
    pub fn update_app(&mut self, window: &sdl::Window, game: &mut Option<Game>) -> bool {
        self.pad = standard_pad(self.gamepads.first().copied());
        if self.app_input.pad(&self.pad) {
            self.set_panel_visible(!self.panel_visible());
        }
        if !self.panel_visible() { self.ui.cancel_pointer(); }
        // With the mouse camera on, the game holds the mouse (hidden,
        // unbounded motion) while it has the input.
        let relative = !cfg!(target_os = "android") && self.settings.borrow().settings().mouse_camera
            && !self.panel_visible() && self.app_input.focused();
        if relative != self.relative_mouse {
            // SAFETY: the window is SDL's and live.
            unsafe { SDL_SetWindowRelativeMouseMode(window.raw, relative) };
            self.relative_mouse = relative;
            self.mouse.clear();
        }
        let was_busy = self.operations.busy;
        if let Some(result) = self.operations.poll(game,&mut self.settings.borrow_mut()) {
            self.show_result(result);
        }
        let bindings = self.settings.borrow().settings().app_bindings.clone();
        let invert_triggers = self.settings.borrow().settings().invert_trigger_chords;
        let editing = self.panel_visible() && self.ui.text_editor_focused();
        if editing { self.app_input.release_keyboard(); }
        let mut count = 0;
        let keys = unsafe { SDL_GetKeyboardState(&mut count) };
        if !keys.is_null() {
            let keys = unsafe { std::slice::from_raw_parts(keys,count.max(0) as usize) };
            for binding in bindings.values() {
                for code in &binding.keyboard {
                    let held = !editing && keys.get(scancode(code).0 as usize).copied().unwrap_or(false);
                    self.app_input.key(code,held);
                }
            }
        }
        self.app_requests.borrow_mut().extend(self.app_input.requests(&bindings,invert_triggers));
        let requests = std::mem::take(&mut *self.app_requests.borrow_mut());
        for request in requests {
            let result = self.operations.execute(request,window,game,&mut self.settings.borrow_mut());
            self.show_result(result);
        }
        if let Some(game) = game {
            let settings = self.settings.borrow();
            let result = game.sync_time_settings(settings.settings());
            drop(settings);
            if let Err(error) = result { self.ui.set_operation_error(&error); }
            if !was_busy && self.operations.busy { game.pause(); }
            if was_busy && !self.operations.busy { game.resume(); }
            let (fast_forward,status) = game.time_status();
            self.ui.set_time_status(Some(fast_forward),&status);
            self.ui.set_storage_status(&game.storage_status());
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
            self.ui.set_time_status(None,"Boot a game to use save states and time controls.");
        }
        !self.operations.busy
    }

    /// An operation's outcome on the panel's status line.
    fn show_result(&self, result: Result<String, String>) {
        match result {
            Ok(text) => self.ui.set_operation_status(&text),
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

    pub fn set_panel_visible(&self, visible: bool) {
        if self.panel_visible() && !visible {
            self.ui.cancel_pointer();
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
                } else if visible && let Some(text) = slint_key(key.key) {
                    self.ui.dispatch(match (key.down, key.repeat) {
                        (true, false) => WindowEvent::KeyPressed { text },
                        (true, true) => WindowEvent::KeyPressRepeated { text },
                        (false, _) => WindowEvent::KeyReleased { text },
                    });
                }
            }
            SDL_EVENT_TEXT_INPUT if visible => {
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
                if self.relative_mouse && self.renderer.first_person() {
                    self.look_mouse[0] += motion.xrel;
                    self.look_mouse[1] += motion.yrel;
                } else if self.relative_mouse {
                    self.mouse.moved(motion.xrel);
                }
            }
            SDL_EVENT_MOUSE_BUTTON_DOWN | SDL_EVENT_MOUSE_BUTTON_UP if unsafe { event.button }.which != SDL_TOUCH_MOUSEID => {
                let button = unsafe { event.button };
                let index = match i32::from(button.button) { SDL_BUTTON_LEFT => 0, SDL_BUTTON_RIGHT => 2, _ => -1 };
                self.mouse.button(index, button.down);
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
                // Positive y scrolls up (away from the user) unless flipped.
                let up = if wheel.direction == SDL_MOUSEWHEEL_FLIPPED { -wheel.y } else { wheel.y };
                self.zoom_wheel(-up);
            }
            SDL_EVENT_WINDOW_FOCUS_LOST => {
                self.ui.cancel_pointer();
                self.mouse.clear();
                self.app_requests.borrow_mut().extend(self.app_input.set_focused(false));
            }
            SDL_EVENT_WINDOW_FOCUS_GAINED => {
                let _ = self.app_input.set_focused(true);
            }
            SDL_EVENT_WINDOW_MOUSE_LEAVE if visible => self.ui.dispatch(WindowEvent::PointerExited),
            SDL_EVENT_GAMEPAD_ADDED => {
                let pad = unsafe { SDL_OpenGamepad(event.gdevice.which) };
                if !pad.is_null() {
                    self.gamepads.push(pad);
                }
            }
            SDL_EVENT_GAMEPAD_REMOVED => {
                let id = unsafe { event.gdevice.which };
                self.gamepads.retain(|&pad| {
                    if unsafe { SDL_GetGamepadID(pad) } == id { unsafe { SDL_CloseGamepad(pad); } false } else { true }
                });
            }
            _ => {}
        }
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

    /// Takes what `game` drew since the last call for the renderer (the
    /// graphics settings choose how it is drawn). A 24-bit display (movie
    /// frames) is shown as the device holds it.
    pub fn set_game_frame(&mut self, gpu: &Gpu, game: &mut Game) {
        let graphics = self.settings.borrow().settings().graphics;
        if graphics.renderer == xem_settings::Renderer::Original {
            // The console's own picture, as the device shows it (nothing to
            // look around in).
            game.stop_trace();
            game.want_actor_view(0);
            self.renderer.set_first_person(false);
            self.game_frame = None;
            let (width, height, rgba) = game.display();
            self.set_game_image(gpu, (width, height), &rgba);
            return;
        }
        let config = ViewConfig::resolve(&graphics, self.host_class, self.target_size, self.renderer.max_scale());
        self.renderer.configure(config);
        let restart = self.renderer.take_restart();
        game.want_actor_view(if graphics.actors_in_view { self.renderer.actor_view() } else { 0 });
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

    /// Whether a presentation is due: always at the display's rate, or
    /// under the game-paced setting only when the game drew or the panel shows.
    pub fn wants_present(&self) -> bool {
        let graphics = self.settings.borrow().settings().graphics;
        graphics.renderer == xem_settings::Renderer::Original
            || graphics.present_rate == xem_settings::PresentRate::Display
            || self.fresh_frame
            || self.panel_visible()
            || self.game_frame.is_none()
    }

    /// Show the game's display: `rgba` is `width` x `height` RGBA8 pixels
    /// (sRGB-encoded, as the console outputs them).
    pub fn set_game_image(&mut self, gpu: &Gpu, (width, height): (u32, u32), rgba: &[u8]) {
        if width == 0 || height == 0 {
            return;
        }
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

    /// The PS1 pad buttons the keyboard, mouse and the first gamepad hold,
    /// as the pad's data halfword has them (bit 0 Select .. bit 15 Square),
    /// unless the settings panel has the input (`context.combo`: the
    /// battle's attack inputs have the buttons, `xem_settings::game_keys`).
    /// In a field they also toggle and turn first-person inspection, which
    /// holds back the buttons that would walk, jump or turn the camera
    /// (`xem_settings::look`).
    pub fn pad_buttons(&mut self, context: PadContext) -> u16 {
        if self.panel_visible() || !self.app_input.focused() {
            self.look_toggle.clear();
            self.look_mouse = [0.0; 2];
            self.looked = None;
            return 0;
        }
        const PAD: [u16;16] = [1 << 14,1 << 13,1 << 15,1 << 12,1 << 10,1 << 11,1 << 8,1 << 9,
            1 << 0,1 << 3,1 << 1,1 << 2,1 << 4,1 << 6,1 << 7,1 << 5];
        let mut buttons = self.mouse.buttons(self.frame_rate_started.elapsed().as_secs_f64() * 1000.0);
        let bindings = self.settings.borrow().settings().app_bindings.clone();
        let mut count = 0;
        // SAFETY: SDL owns the array and keeps it valid while SDL runs.
        let keys = unsafe { SDL_GetKeyboardState(&mut count) };
        let keys = if keys.is_null() { &[][..] } else { unsafe { std::slice::from_raw_parts(keys, count.max(0) as usize) } };
        let held = |code: &str| !self.app_input.captures_key(code, &bindings) && keys.get(scancode(code).0 as usize).copied().unwrap_or(false);
        for code in xem_settings::game_keys::game_keys() {
            if held(code) && let Some(bit) = xem_settings::game_keys::key_button(code, context.combo) {
                buttons |= bit;
            }
        }
        let invert_triggers = self.settings.borrow().settings().invert_trigger_chords;
        let gamepad = self.app_input.gamepad_for_game(&bindings,invert_triggers);
        for (pressed,bit) in gamepad.into_iter().zip(PAD) { if pressed != 0 { buttons |= bit; } }
        // First-person inspection: toggled in a field (Ex Machina draws it),
        // and while it is on, turned by the arrows, the right stick and the
        // mouse the game holds.
        let field = context.field;
        let drawn = self.settings.borrow().settings().graphics.renderer == xem_settings::Renderer::ExMachina;
        if self.look_toggle.update(held("Backspace"), &self.pad, field) && drawn {
            self.renderer.set_first_person(!self.renderer.first_person());
            self.mouse.clear();
            self.look_mouse = [0.0; 2];
        }
        let now = Instant::now();
        let seconds = self.looked.map_or(0.0, |then| now.duration_since(then).as_secs_f32().min(0.1));
        self.looked = Some(now);
        let on = self.renderer.first_person();
        // The mouse's motion is the view's this frame, or no one's.
        let mouse = std::mem::take(&mut self.look_mouse);
        if xem_settings::look::turns(on, field) {
            let turn = xem_settings::look::Turn {
                arrows: ["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown"].map(held),
                stick: right_stick(self.gamepads.first().copied()),
                mouse,
            };
            let (yaw, pitch) = turn.radians(seconds);
            self.renderer.look_around(yaw, pitch);
        }
        xem_settings::look::game_buttons(buttons, on, field)
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
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        self.fresh_frame = false;
        let presented = match &self.game_frame {
            Some((display, scene)) => self.renderer.present(&mut encoder, display, scene, f64::from(time)).map(|p| (p.view, p.size)),
            None => None,
        };
        let encoded = presented.is_some();
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
        self.frame_rate.frame(self.frame_rate_started.elapsed(),self.ui.scale_factor(),presentation.show_fps)
            .map_err(|error| error.to_string())?;
        // Inside the safe area (all of the target when the host gave none).
        let safe = self
            .safe_area
            .filter(|a| a.x + a.width <= target_size.0 as f32 && a.y + a.height <= target_size.1 as f32 && a.width >= 1.0 && a.height >= 1.0)
            .unwrap_or(Rect { x: 0.0, y: 0.0, width: target_size.0 as f32, height: target_size.1 as f32 });
        let mut placed = present_rect(game_size, (safe.width as u32, safe.height as u32), presentation.scale);
        placed.x += safe.x;
        placed.y += safe.y;
        let mut game = Quad::screen(game_view, placed, target_size);
        // The renderer's frames hold sRGB-encoded values in a linear format.
        game.srgb_encoded = encoded;
        game.filter = match presentation.filter {
            Filter::Nearest => wgpu::FilterMode::Nearest,
            Filter::Linear => wgpu::FilterMode::Linear,
        };
        // Movie frames the device shows itself (the Original movies setting,
        // or one Ex Machina cannot rebuild) stay as the decoder made them,
        // each pixel a block; the Original renderer keeps the chosen filter.
        let graphics = self.settings.borrow().settings().graphics;
        if !encoded && self.game_frame.is_none() && self.game_image.is_some() && graphics.renderer == xem_settings::Renderer::ExMachina {
            game.filter = wgpu::FilterMode::Nearest;
        }
        let mut quads = vec![game];
        if presentation.show_fps {
            let (width,height) = self.frame_rate.size();
            let margin = 8.0*self.ui.scale_factor();
            let mut counter = Quad::screen(self.frame_rate.view(),Rect { x:margin,y:margin,width:width as f32,height:height as f32 },target_size);
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

/// The first gamepad's right stick, -1 to 1 (x to the right, y down).
fn right_stick(pad: Option<*mut SDL_Gamepad>) -> [f32; 2] {
    let Some(pad) = pad else { return [0.0; 2] };
    [SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY].map(|axis| f32::from(unsafe { SDL_GetGamepadAxis(pad, axis) }) / 32768.0)
}

fn standard_pad(pad: Option<*mut SDL_Gamepad>) -> [u8;16] {
    let mut pressed = [0;16];
    let Some(pad) = pad else { return pressed };
    for (i,button) in [(0,SDL_GAMEPAD_BUTTON_SOUTH),(1,SDL_GAMEPAD_BUTTON_EAST),(2,SDL_GAMEPAD_BUTTON_WEST),(3,SDL_GAMEPAD_BUTTON_NORTH),
        (4,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER),(5,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER),(8,SDL_GAMEPAD_BUTTON_BACK),(9,SDL_GAMEPAD_BUTTON_START),
        (10,SDL_GAMEPAD_BUTTON_LEFT_STICK),(11,SDL_GAMEPAD_BUTTON_RIGHT_STICK),(12,SDL_GAMEPAD_BUTTON_DPAD_UP),(13,SDL_GAMEPAD_BUTTON_DPAD_DOWN),
        (14,SDL_GAMEPAD_BUTTON_DPAD_LEFT),(15,SDL_GAMEPAD_BUTTON_DPAD_RIGHT)] {
        pressed[i] = u8::from(unsafe { SDL_GetGamepadButton(pad,button) });
    }
    pressed[6] = u8::from(unsafe { SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFT_TRIGGER) } > 16000);
    pressed[7] = u8::from(unsafe { SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) } > 16000);
    let axes = [SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_AXIS_RIGHTX]
        .map(|axis| f32::from(unsafe { SDL_GetGamepadAxis(pad, axis) }) / 32768.0);
    xem_settings::controls::gamepad_buttons(&pressed, &axes)
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

/// Keys Slint treats specially; printable text arrives as SDL text input.
fn slint_key(key: SDL_Keycode) -> Option<xem_ui::SharedString> {
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
        SDLK_LCTRL => Key::Control,
        SDLK_RCTRL => Key::ControlR,
        SDLK_LALT => Key::Alt,
        SDLK_RALT => Key::AltGr,
        SDLK_LGUI => Key::Meta,
        SDLK_RGUI => Key::MetaR,
        _ => return None,
    };
    Some(key.into())
}

#[cfg(test)]
mod tests {
    use super::*;

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
            view.pad = standard_pad(view.gamepads.first().copied());
            view.app_input.pad(&view.pad);
            view.pad_buttons(PadContext { combo: false, field })
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
}
