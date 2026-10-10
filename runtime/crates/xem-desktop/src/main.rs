//! The desktop host: an SDL3 window presenting through wgpu, the test scene as
//! the stand-in game view and the Slint settings panel as an overlay
//! (`xem_host::View`). One wgpu device and queue serve the scene, the
//! compositor and Slint.

use sdl3_sys::everything::*;
use std::cell::RefCell;
use std::path::PathBuf;
use std::rc::Rc;
use std::time::Instant;
use xem_host::{Presenter, View, sdl};
use xem_render::{Gpu, read_rgba8, wgpu};
use xem_settings::{FileStore, MemoryStore, SettingsService};

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
    view: View,
    presenter: Presenter,
    gpu: Gpu,
    start: Instant,
}

impl Host {
    fn new(window: &sdl::Window, options: &Options) -> Result<Self, String> {
        let descriptor =
            wgpu::InstanceDescriptor::new_with_display_handle_from_env(Box::new(window.display()));
        let (gpu, surface) = xem_host::open_gpu(window, descriptor, None)?;
        let info = gpu.adapter.get_info();
        eprintln!(
            "xem-desktop: {} ({:?}, {})",
            info.name, info.backend, info.driver
        );
        let usage = if options.screenshot.is_some() {
            wgpu::TextureUsages::COPY_SRC
        } else {
            wgpu::TextureUsages::empty()
        };
        let presenter = Presenter::new(&gpu, surface, window, usage)?;
        let settings = Rc::new(RefCell::new(open_settings()));
        let view = View::new(
            &gpu,
            presenter.format(),
            presenter.size(),
            window.display_scale(),
            settings,
            options.show_settings,
        )?;
        window.set_text_input(options.show_settings);
        Ok(Self {
            view,
            presenter,
            gpu,
            start: Instant::now(),
        })
    }

    fn run(&mut self, window: &sdl::Window, options: &Options) -> Result<(), String> {
        let mut frames = 0u64;
        let mut minimized = false;
        let mut fps_window = (Instant::now(), 0u32);
        loop {
            let was_visible = self.view.panel_visible();
            let mut event = SDL_Event::default();
            // SAFETY: SDL is initialised; event is a valid out pointer.
            while unsafe { SDL_PollEvent(&mut event) } {
                // SAFETY: the union is read according to its type tag.
                match SDL_EventType(unsafe { event.r#type }) {
                    SDL_EVENT_QUIT | SDL_EVENT_WINDOW_CLOSE_REQUESTED => return Ok(()),
                    SDL_EVENT_WINDOW_MINIMIZED => minimized = true,
                    SDL_EVENT_WINDOW_RESTORED => minimized = false,
                    SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
                    | SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED => self.resize(window),
                    // SAFETY: event comes from SDL_PollEvent.
                    _ => unsafe { self.view.handle(window, &event) },
                }
            }
            let visible = self.view.panel_visible();
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
                if self
                    .view
                    .settings()
                    .borrow()
                    .settings()
                    .presentation
                    .show_fps
                {
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

    fn resize(&mut self, window: &sdl::Window) {
        if self.presenter.resize(&self.gpu, window) {
            self.view
                .resize(self.presenter.size(), window.display_scale());
        }
    }

    /// Renders and presents one frame; false if no frame could be acquired.
    fn frame(
        &mut self,
        window: &sdl::Window,
        screenshot: Option<&std::path::Path>,
    ) -> Result<bool, String> {
        if self.presenter.is_suspended() {
            // The surface was lost.
            self.presenter.resume(&self.gpu, window)?;
        }
        let Some(frame) = self.presenter.acquire(&self.gpu, window)? else {
            return Ok(false);
        };
        let output = frame.texture.create_view(&Default::default());
        self.view
            .render(&self.gpu, &output, self.start.elapsed().as_secs_f32())?;
        if let Some(path) = screenshot {
            let pixels = read_rgba8(&self.gpu.device, &self.gpu.queue, &frame.texture);
            save_png(path, self.presenter.size(), &pixels)?;
            eprintln!("xem-desktop: wrote {}", path.display());
        }
        self.gpu.queue.present(frame);
        Ok(true)
    }
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
