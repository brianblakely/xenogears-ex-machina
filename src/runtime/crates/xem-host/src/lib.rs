//! What the SDL3 hosts (desktop, Android) share: the SDL window and its native
//! handles, the wgpu surface they present to (dropped and recreated when the
//! platform takes the native window away), and the view they present: the
//! test scene as the stand-in game image under the Slint settings panel. One
//! wgpu device and queue serve the scene, the compositor and Slint.

pub mod sdl;

use sdl3_sys::everything::*;
use std::cell::RefCell;
use std::rc::Rc;
use xem_render::{
    Compositor, Gpu, Quad, Rect, SceneRenderer, ViewTarget, Viewport, demo_camera, present_rect,
    wgpu,
};
use xem_settings::{Filter, SettingsService};
use xem_ui::{Key, LogicalPosition, PointerEventButton, SettingsUi, SharedGpu, WindowEvent};

/// The stand-in game picture's size (the original's typical display mode).
pub const GAME_SIZE: (u32, u32) = (320, 240);
const GAME_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8UnormSrgb;
/// The panel's size in logical pixels.
const PANEL_SIZE: (f32, f32) = (440.0, 560.0);

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

    /// Follows the window's pixel size; false while it has none.
    pub fn resize(&mut self, gpu: &Gpu, window: &sdl::Window) -> bool {
        let (width, height) = window.pixel_size();
        if width == 0 || height == 0 {
            return false;
        }
        if (width, height) != self.size() {
            self.config.width = width;
            self.config.height = height;
            if let Some(surface) = &self.surface {
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
    panel_visible: Rc<RefCell<bool>>,
    gamepads: Vec<*mut SDL_Gamepad>,
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
            shared,
            settings.clone(),
            panel_size(target_size, scale),
            scale,
        )
        .map_err(|error| error.to_string())?;
        let panel_visible = Rc::new(RefCell::new(show_panel));
        ui.on_close({
            let visible = panel_visible.clone();
            move || *visible.borrow_mut() = false
        });
        Ok(Self {
            scene: SceneRenderer::new(&gpu.device, &gpu.queue, GAME_FORMAT),
            compositor: Compositor::new(&gpu.device, &gpu.queue, format),
            game_view,
            target_size,
            settings,
            ui,
            panel_visible,
            gamepads: Vec::new(),
        })
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
        *self.panel_visible.borrow_mut() = visible;
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

    /// Routes input to the panel and toggles it (F1, Escape while shown, a
    /// gamepad's Start/Back, Android's Back key).
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
                    && (key.key == SDLK_F1
                        || key.key == SDLK_AC_BACK
                        || (key.key == SDLK_ESCAPE && visible))
                {
                    self.set_panel_visible(!visible);
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
            SDL_EVENT_WINDOW_MOUSE_LEAVE if visible => self.ui.dispatch(WindowEvent::PointerExited),
            SDL_EVENT_GAMEPAD_ADDED => {
                let pad = unsafe { SDL_OpenGamepad(event.gdevice.which) };
                if !pad.is_null() {
                    self.gamepads.push(pad);
                }
            }
            SDL_EVENT_GAMEPAD_BUTTON_DOWN => {
                let button = SDL_GamepadButton(i32::from(unsafe { event.gbutton.button }));
                if button == SDL_GAMEPAD_BUTTON_START || button == SDL_GAMEPAD_BUTTON_BACK {
                    self.set_panel_visible(!visible);
                }
            }
            _ => {}
        }
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

        let mut encoder = gpu.device.create_command_encoder(&Default::default());
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

        let target_size = self.target_size;
        let presentation = self.settings.borrow().settings().presentation.clone();
        let mut game = Quad::screen(
            &self.game_view,
            present_rect(GAME_SIZE, target_size, presentation.scale),
            target_size,
        );
        game.filter = match presentation.filter {
            Filter::Nearest => wgpu::FilterMode::Nearest,
            Filter::Linear => wgpu::FilterMode::Linear,
        };
        let mut quads = vec![game];
        if visible {
            let mut panel = Quad::screen(self.ui.view(), self.panel_rect(), target_size);
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
