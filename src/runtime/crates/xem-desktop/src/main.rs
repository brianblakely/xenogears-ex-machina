//! The desktop host: an SDL3 window presenting through wgpu, the game (with
//! `--disc`, else the test scene as a stand-in) and the Slint settings panel as
//! an overlay (`xem_host::View`). One wgpu device and queue serve the game
//! image, the compositor and Slint.

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
    /// The user's disc (CHD, cue or bin): play the game.
    disc: Option<PathBuf>,
    cards: Option<PathBuf>,
    bios: Option<PathBuf>,
}

fn options() -> Result<Options, String> {
    let mut options = Options {
        exit_after_frames: None,
        screenshot: None,
        show_settings: false,
        disc: None,
        cards: None,
        bios: None,
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
            "--disc" => options.disc = Some(args.next().ok_or("--disc needs a path")?.into()),
            "--cards" => options.cards = Some(args.next().ok_or("--cards needs a directory")?.into()),
            "--bios" => options.bios = Some(args.next().ok_or("--bios needs a path")?.into()),
            "--help" => {
                println!(
                    "xem-desktop [--disc DISC [--cards DIR] [--bios FILE]] [--show-settings] \
                     [--exit-after-frames N] [--screenshot out.png]\n\
                     F1 or Escape toggles the settings panel. Pad: arrows, Z cross, X circle, \
                     A square, S triangle, Q/W L1/R1, 1/2 L2/R2, Enter start, right Shift select, \
                     or a gamepad."
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
    game: Option<Game>,
}

/// The game, paced at the console's 60 frames a second, with its sound.
struct Game {
    session: xem_core::control::Session<xem_game::NativeModule>,
    audio: *mut SDL_AudioStream,
    paced: Instant,
    samples: Vec<i16>,
}

const FRAME: std::time::Duration = std::time::Duration::from_nanos(1_000_000_000 / 60);

impl Game {
    fn open(options: &Options, disc: &std::path::Path) -> Result<Self, String> {
        let cards = options.cards.clone().or_else(|| {
            FileStore::default_path().and_then(|p| p.parent().map(|d| d.join("cards")))
        });
        if let Some(dir) = &cards {
            std::fs::create_dir_all(dir).map_err(|e| format!("{}: {e}", dir.display()))?;
        }
        let boot = xem_game::BootOptions {
            cards,
            bios: options.bios.clone(),
            audio_output: true,
            ..Default::default()
        };
        let (session, identity) = xem_game::boot(disc, &boot).map_err(|e| e.to_string())?;
        eprintln!("xem-desktop: {}", identity.boot_path);
        let spec = SDL_AudioSpec { format: SDL_AUDIO_S16LE, channels: 2, freq: 44100 };
        // SAFETY: SDL is initialised with audio; a null callback queues data.
        let audio = unsafe {
            SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, None, std::ptr::null_mut())
        };
        if audio.is_null() {
            eprintln!("xem-desktop: no audio device; the game runs silent");
        } else {
            unsafe { SDL_ResumeAudioStreamDevice(audio) };
        }
        Ok(Game { session, audio, paced: Instant::now(), samples: Vec::new() })
    }

    /// Run the frames due by the wall clock (at most two per call: a stalled
    /// host drops time rather than catching up), then queue their sound.
    fn update(&mut self, buttons: u16, settings: &xem_settings::Settings) -> Result<(), String> {
        let mut due = 0;
        while self.paced + FRAME <= Instant::now() && due < 2 {
            self.paced += FRAME;
            due += 1;
        }
        if self.paced + FRAME <= Instant::now() {
            self.paced = Instant::now();
        }
        for _ in 0..due {
            self.session.runtime.services().pads.0[0].buttons = buttons;
            self.session.frame().map_err(|trap| trap.to_string())?;
        }
        let spu = &mut self.session.runtime.services().spu;
        self.samples.resize(spu.audio_queued(), 0);
        spu.take_audio(&mut self.samples);
        if !self.audio.is_null() && !self.samples.is_empty() {
            let gain = f32::from(settings.master_volume.min(xem_settings::MAX_VOLUME))
                / f32::from(xem_settings::MAX_VOLUME);
            for frame in self.samples.chunks_exact_mut(2) {
                let (left, right) = settings.audio_output.mix(f32::from(frame[0]), f32::from(frame[1]));
                frame[0] = (left * gain) as i16;
                frame[1] = (right * gain) as i16;
            }
            // SAFETY: an open stream; SDL copies the bytes.
            unsafe {
                SDL_PutAudioStreamData(
                    self.audio,
                    self.samples.as_ptr().cast(),
                    (self.samples.len() * 2) as i32,
                );
            }
        }
        Ok(())
    }

    fn display(&mut self) -> (u32, u32, Vec<u8>) {
        self.session.runtime.services().gpu.display_rgba8()
    }
}

impl Drop for Game {
    fn drop(&mut self) {
        if !self.audio.is_null() {
            // SAFETY: opened by SDL_OpenAudioDeviceStream, destroyed once.
            unsafe { SDL_DestroyAudioStream(self.audio) };
        }
    }
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
        let game = options.disc.as_deref().map(|disc| Game::open(options, disc)).transpose()?;
        Ok(Self {
            view,
            presenter,
            gpu,
            start: Instant::now(),
            game,
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
        if let Some(game) = &mut self.game {
            let settings = self.view.settings().borrow().settings().clone();
            game.update(self.view.pad_buttons(), &settings)?;
            let (width, height, rgba) = game.display();
            self.view.set_game_image(&self.gpu, (width, height), &rgba);
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
