//! The desktop host: an SDL3 window presenting through wgpu, the test scene as
//! the stand-in game view and the Slint settings panel as an overlay. One wgpu
//! device and queue serve the scene, the compositor and Slint.

mod sdl;

use sdl3_sys::everything::*;
use std::cell::RefCell;
use std::path::PathBuf;
use std::rc::Rc;
use std::time::Instant;
use xem_render::{
    Compositor, Gpu, Quad, Rect, SceneRenderer, ViewTarget, Viewport, demo_camera, present_rect,
    read_rgba8, wgpu,
};
use xem_settings::{FileStore, Filter, MemoryStore, SettingsService};
use xem_ui::{Key, LogicalPosition, PointerEventButton, SettingsUi, SharedGpu, WindowEvent};

/// The stand-in game picture's size (the original's typical display mode).
const GAME_SIZE: (u32, u32) = (320, 240);
const GAME_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8UnormSrgb;
/// The panel's size in logical pixels.
const PANEL_SIZE: (f32, f32) = (440.0, 560.0);

struct Options {
    exit_after_frames: Option<u64>,
    screenshot: Option<PathBuf>,
    show_settings: bool,
}

fn options() -> Result<Options, String> {
    let mut options = Options {
        exit_after_frames: None,
        screenshot: None,
        show_settings: false,
    };
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--exit-after-frames" => {
                let count = args.next().ok_or("--exit-after-frames needs a count")?;
                options.exit_after_frames = Some(
                    count
                        .parse()
                        .map_err(|_| format!("bad frame count {count:?}"))?,
                );
            }
            "--screenshot" => {
                options.screenshot = Some(args.next().ok_or("--screenshot needs a path")?.into());
            }
            "--show-settings" => options.show_settings = true,
            "--help" => {
                println!(
                    "xem-desktop [--show-settings] [--exit-after-frames N] [--screenshot out.png]\n\
                     F1, Escape or a gamepad's Start button toggles the settings panel."
                );
                std::process::exit(0);
            }
            other => return Err(format!("unknown argument {other:?} (see --help)")),
        }
    }
    if options.screenshot.is_some() && options.exit_after_frames.is_none() {
        return Err("--screenshot is taken at --exit-after-frames".into());
    }
    Ok(options)
}

fn main() {
    if let Err(message) = options().and_then(run) {
        eprintln!("xem-desktop: {message}");
        std::process::exit(1);
    }
}

fn open_settings() -> SettingsService {
    let Some(path) = FileStore::default_path() else {
        eprintln!("xem-desktop: no configuration directory; settings are not saved");
        return SettingsService::with_defaults(MemoryStore::default());
    };
    SettingsService::open(FileStore::new(&path)).unwrap_or_else(|error| {
        eprintln!("xem-desktop: {}: {error}; using defaults", path.display());
        SettingsService::with_defaults(FileStore::new(path))
    })
}

fn run(options: Options) -> Result<(), String> {
    let sdl = sdl::Sdl::init()?;
    let window = sdl::Window::new(&sdl, "Xenogears Ex Machina", 1280, 800)?;
    let result = Host::new(&window, &options).and_then(|mut host| host.run(&window, &options));
    drop(window);
    drop(sdl);
    result
}

/// Everything presented in the window. Dropped before the window.
struct Host {
    gpu: Gpu,
    surface: wgpu::Surface<'static>,
    config: wgpu::SurfaceConfiguration,
    scene: SceneRenderer,
    compositor: Compositor,
    game_view: wgpu::TextureView,
    settings: Rc<RefCell<SettingsService>>,
    ui: SettingsUi,
    panel_visible: Rc<RefCell<bool>>,
    gamepads: Vec<*mut SDL_Gamepad>,
    start: Instant,
}

fn create_surface(
    gpu_instance: &wgpu::Instance,
    window: &sdl::Window,
) -> Result<wgpu::Surface<'static>, String> {
    // SAFETY: the surface is dropped (with the Host) before the window.
    unsafe {
        let target = wgpu::SurfaceTargetUnsafe::from_display_and_window(window, window)
            .map_err(|error| error.to_string())?;
        gpu_instance
            .create_surface_unsafe(target)
            .map_err(|error| error.to_string())
    }
}

impl Host {
    fn new(window: &sdl::Window, options: &Options) -> Result<Self, String> {
        let instance = wgpu::Instance::new(
            wgpu::InstanceDescriptor::new_with_display_handle_from_env(Box::new(window.display())),
        );
        let surface = create_surface(&instance, window)?;
        let gpu =
            pollster::block_on(Gpu::new(instance, Some(&surface))).map_err(|e| e.to_string())?;
        let info = gpu.adapter.get_info();
        eprintln!(
            "xem-desktop: {} ({:?}, {})",
            info.name, info.backend, info.driver
        );

        let capabilities = surface.get_capabilities(&gpu.adapter);
        // The scene is lit in linear light in its own sRGB texture; the window
        // composites in sRGB-encoded values, as Slint blends the panel itself.
        let format = capabilities
            .formats
            .iter()
            .copied()
            .find(|format| !format.is_srgb())
            .unwrap_or(capabilities.formats[0]);
        let mut usage = wgpu::TextureUsages::RENDER_ATTACHMENT;
        if options.screenshot.is_some() {
            if !capabilities.usages.contains(wgpu::TextureUsages::COPY_SRC) {
                return Err("this surface cannot be read back for --screenshot".into());
            }
            usage |= wgpu::TextureUsages::COPY_SRC;
        }
        let (width, height) = window.pixel_size();
        let config = wgpu::SurfaceConfiguration {
            usage,
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

        let settings = Rc::new(RefCell::new(open_settings()));
        let shared = SharedGpu {
            instance: gpu.instance.clone(),
            device: gpu.device.clone(),
            queue: gpu.queue.clone(),
        };
        let scale = window.display_scale();
        let ui = SettingsUi::new(
            shared,
            settings.clone(),
            panel_size(config.width, config.height, scale),
            scale,
        )
        .map_err(|error| error.to_string())?;
        let panel_visible = Rc::new(RefCell::new(options.show_settings));
        ui.on_close({
            let visible = panel_visible.clone();
            move || *visible.borrow_mut() = false
        });
        window.set_text_input(options.show_settings);

        Ok(Self {
            scene: SceneRenderer::new(&gpu.device, &gpu.queue, GAME_FORMAT),
            compositor: Compositor::new(&gpu.device, &gpu.queue, format),
            gpu,
            surface,
            config,
            game_view,
            settings,
            ui,
            panel_visible,
            gamepads: Vec::new(),
            start: Instant::now(),
        })
    }

    fn run(&mut self, window: &sdl::Window, options: &Options) -> Result<(), String> {
        let mut frames = 0u64;
        let mut minimized = false;
        let mut fps_window = (Instant::now(), 0u32);
        loop {
            let was_visible = *self.panel_visible.borrow();
            let mut event = SDL_Event::default();
            // SAFETY: SDL is initialised; event is a valid out pointer.
            while unsafe { SDL_PollEvent(&mut event) } {
                // SAFETY: the union is read according to its type tag.
                match unsafe { self.handle(window, &event) } {
                    Control::Quit => return Ok(()),
                    Control::Minimized(state) => minimized = state,
                    Control::Continue => {}
                }
            }
            let visible = *self.panel_visible.borrow();
            if visible != was_visible {
                window.set_text_input(visible);
            }
            if minimized {
                // Nothing is presented while minimized: sleep until the next event.
                // SAFETY: a null event pointer leaves the event queued.
                unsafe { SDL_WaitEvent(std::ptr::null_mut()) };
                continue;
            }

            let last = options
                .exit_after_frames
                .is_some_and(|count| frames + 1 >= count);
            let screenshot = if last {
                options.screenshot.as_deref()
            } else {
                None
            };
            if self.frame(window, screenshot)? {
                frames += 1;
                fps_window.1 += 1;
            }
            if last && frames > 0 {
                return Ok(());
            }
            let elapsed = fps_window.0.elapsed().as_secs_f32();
            if elapsed >= 1.0 {
                if self.settings.borrow().settings().presentation.show_fps {
                    window.set_title(&format!(
                        "Xenogears Ex Machina - {:.0} fps",
                        fps_window.1 as f32 / elapsed
                    ));
                } else {
                    window.set_title("Xenogears Ex Machina");
                }
                fps_window = (Instant::now(), 0);
            }
        }
    }

    /// # Safety
    /// `event` must come from SDL_PollEvent.
    unsafe fn handle(&mut self, window: &sdl::Window, event: &SDL_Event) -> Control {
        let visible = *self.panel_visible.borrow();
        let kind = SDL_EventType(unsafe { event.r#type });
        match kind {
            SDL_EVENT_QUIT | SDL_EVENT_WINDOW_CLOSE_REQUESTED => return Control::Quit,
            SDL_EVENT_WINDOW_MINIMIZED => return Control::Minimized(true),
            SDL_EVENT_WINDOW_RESTORED => return Control::Minimized(false),
            SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED | SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED => {
                self.resize(window);
            }
            SDL_EVENT_KEY_DOWN | SDL_EVENT_KEY_UP => {
                let key = unsafe { event.key };
                if key.down
                    && !key.repeat
                    && (key.key == SDLK_F1 || (key.key == SDLK_ESCAPE && visible))
                {
                    *self.panel_visible.borrow_mut() = !visible;
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
                    *self.panel_visible.borrow_mut() = !visible;
                }
            }
            _ => {}
        }
        Control::Continue
    }

    fn resize(&mut self, window: &sdl::Window) {
        let (width, height) = window.pixel_size();
        if width == 0 || height == 0 {
            return;
        }
        if (width, height) != (self.config.width, self.config.height) {
            self.config.width = width;
            self.config.height = height;
            self.surface.configure(&self.gpu.device, &self.config);
        }
        let scale = window.display_scale();
        self.ui.resize(panel_size(width, height, scale), scale);
    }

    /// Where the panel sits in the window, in pixels.
    fn panel_rect(&self) -> Rect {
        let (width, height) = self.ui.size();
        Rect {
            x: ((self.config.width - width) / 2) as f32,
            y: ((self.config.height - height) / 2) as f32,
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

    /// Renders and presents one frame; false if no frame could be acquired.
    fn frame(
        &mut self,
        window: &sdl::Window,
        screenshot: Option<&std::path::Path>,
    ) -> Result<bool, String> {
        let frame = match self.surface.get_current_texture() {
            wgpu::CurrentSurfaceTexture::Success(frame) => frame,
            wgpu::CurrentSurfaceTexture::Suboptimal(frame) => {
                drop(frame);
                self.surface.configure(&self.gpu.device, &self.config);
                return Ok(false);
            }
            wgpu::CurrentSurfaceTexture::Timeout | wgpu::CurrentSurfaceTexture::Occluded => {
                return Ok(false);
            }
            wgpu::CurrentSurfaceTexture::Outdated => {
                self.resize(window);
                self.surface.configure(&self.gpu.device, &self.config);
                return Ok(false);
            }
            wgpu::CurrentSurfaceTexture::Lost => {
                self.surface = create_surface(&self.gpu.instance, window)?;
                self.surface.configure(&self.gpu.device, &self.config);
                return Ok(false);
            }
            wgpu::CurrentSurfaceTexture::Validation => {
                return Err("surface validation error".into());
            }
        };
        let visible = *self.panel_visible.borrow();
        if visible {
            self.ui.update().map_err(|error| error.to_string())?;
        }

        let time = self.start.elapsed().as_secs_f32();
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
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

        let target_size = (self.config.width, self.config.height);
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
        let output = frame.texture.create_view(&Default::default());
        self.compositor
            .draw(&mut encoder, &output, Some(wgpu::Color::BLACK), &quads);
        drop(quads);
        self.gpu.queue.submit([encoder.finish()]);

        if let Some(path) = screenshot {
            let pixels = read_rgba8(&self.gpu.device, &self.gpu.queue, &frame.texture);
            save_png(path, target_size, &pixels)?;
            eprintln!("xem-desktop: wrote {}", path.display());
        }
        self.gpu.queue.present(frame);
        Ok(true)
    }
}

impl Drop for Host {
    fn drop(&mut self) {
        for pad in self.gamepads.drain(..) {
            // SAFETY: opened by SDL_OpenGamepad and closed once.
            unsafe { SDL_CloseGamepad(pad) };
        }
    }
}

enum Control {
    Continue,
    Minimized(bool),
    Quit,
}

/// The panel's pixel size for a window of `width` x `height` pixels.
fn panel_size(width: u32, height: u32, scale: f32) -> (u32, u32) {
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

fn save_png(
    path: &std::path::Path,
    (width, height): (u32, u32),
    rgba: &[u8],
) -> Result<(), String> {
    let file =
        std::fs::File::create(path).map_err(|error| format!("{}: {error}", path.display()))?;
    let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), width, height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder.write_header().map_err(|error| error.to_string())?;
    writer
        .write_image_data(rgba)
        .map_err(|error| error.to_string())
}
