//! The Android host loop: SDL3 window, wgpu surface (Vulkan, GLES only when
//! Vulkan is unavailable), the shared view, settings in the app's files
//! directory, disc import and the activity lifecycle.

use sdl3_sys::everything::*;
use std::cell::{Cell, RefCell};
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::path::PathBuf;
use std::rc::Rc;
use std::sync::atomic::{AtomicU32, AtomicU64, Ordering};
use std::time::Instant;
use xem_host::{Presenter, View, sdl};
use xem_render::{Gpu, wgpu};
use xem_settings::{FileStore, SettingsService};

use crate::clock::SimClock;
use crate::disc::DiscImport;

/// The file bundled in the APK's assets that startup reads back.
const BUNDLED_ASSET: &CStr = c"xem-asset-check.txt";
/// Frames between progress lines in the log.
const LOG_EVERY: u64 = 120;
/// SDL_main runs so far in this process.
static RUNS: AtomicU32 = AtomicU32::new(0);

/// Writes `xem: <message>` to logcat (tag SDL/APP).
pub fn log(message: &str) {
    let line = CString::new(format!("xem: {message}")).unwrap_or_default();
    // SAFETY: a "%s" format with one NUL-terminated string.
    unsafe { SDL_Log(c"%s".as_ptr(), line.as_ptr()) };
}

/// SDLActivity calls this from its SDL thread with the activity's arguments
/// (`XemActivity` passes the `xem.args` string-array extra).
#[unsafe(no_mangle)]
pub extern "C" fn SDL_main(argc: c_int, argv: *mut *mut c_char) -> c_int {
    std::panic::set_hook(Box::new(|info| log(&format!("panic: {info}"))));
    // SAFETY: SDL passes argc NUL-terminated strings.
    let args: Vec<String> = (1..argc.max(1) as usize)
        .map(|n| {
            unsafe { CStr::from_ptr(*argv.add(n)) }
                .to_string_lossy()
                .into_owned()
        })
        .collect();
    match run(&args) {
        Ok(()) => {
            log("exit");
            0
        }
        Err(message) => {
            log(&format!("error: {message}"));
            1
        }
    }
}

struct Options {
    /// A document URI or path to import at startup (tests).
    open: Option<String>,
    /// Show the document picker at startup (tests).
    pick: bool,
}

fn options(args: &[String]) -> Result<Options, String> {
    let mut options = Options {
        open: None,
        pick: false,
    };
    let mut args = args.iter();
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--open" => options.open = Some(args.next().ok_or("--open needs a URI")?.clone()),
            "--pick-disc" => options.pick = true,
            other => return Err(format!("unknown argument {other:?}")),
        }
    }
    Ok(options)
}

fn run(args: &[String]) -> Result<(), String> {
    let options = options(args)?;
    let run = RUNS.fetch_add(1, Ordering::Relaxed) + 1;
    log(&format!("start (run {run} in this process)"));
    // SAFETY: called before SDL_Init with NUL-terminated strings.
    unsafe {
        // Back toggles the settings panel instead of leaving the activity.
        SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, c"1".as_ptr());
        // When Android recreates the activity (a change of the app's asset
        // paths, which no configChanges entry covers), SDL quits this run
        // and starts SDL_main again in the same process instead of exiting.
        SDL_SetHint(SDL_HINT_ANDROID_ALLOW_RECREATE_ACTIVITY, c"1".as_ptr());
    }
    let sdl = sdl::Sdl::init()?;
    let window = sdl::Window::new(&sdl, "Xenogears Ex Machina", 1280, 800)?;
    let result = Host::new(&window).and_then(|mut host| host.run(&window, &options));
    drop(window);
    drop(sdl);
    result
}

/// The app's internal files directory (`Context.getFilesDir()`).
fn files_dir() -> Result<PathBuf, String> {
    // SAFETY: returns a string SDL owns, or NULL.
    let path = unsafe { SDL_GetAndroidInternalStoragePath() };
    if path.is_null() {
        return Err(format!("no internal storage: {}", sdl::error()));
    }
    Ok(PathBuf::from(
        unsafe { CStr::from_ptr(path) }
            .to_string_lossy()
            .into_owned(),
    ))
}

/// Reads the bundled asset through SDL (the APK's AssetManager).
fn check_bundled_asset() -> Result<String, String> {
    // SAFETY: SDL is initialised; the stream is closed by SDL_LoadFile_IO.
    unsafe {
        let stream = SDL_IOFromFile(BUNDLED_ASSET.as_ptr(), c"rb".as_ptr());
        if stream.is_null() {
            return Err(sdl::error());
        }
        let mut size = 0usize;
        let data = SDL_LoadFile_IO(stream, &mut size, true);
        if data.is_null() {
            return Err(sdl::error());
        }
        let bytes = std::slice::from_raw_parts(data as *const u8, size).to_vec();
        SDL_free(data);
        let text = String::from_utf8_lossy(&bytes);
        Ok(format!(
            "{size} bytes: {:?}",
            text.lines().next().unwrap_or_default()
        ))
    }
}

/// Opens the device: Vulkan first, the GLES backend only without Vulkan.
fn open_gpu(window: &sdl::Window) -> Result<(Gpu, wgpu::Surface<'static>), String> {
    let descriptor =
        || wgpu::InstanceDescriptor::new_with_display_handle_from_env(Box::new(window.display()));
    match xem_host::open_gpu(window, descriptor(), Some(wgpu::Backends::VULKAN)) {
        Ok(opened) => Ok(opened),
        Err(error) => {
            log(&format!(
                "Vulkan unavailable ({error}); falling back to GLES"
            ));
            xem_host::open_gpu(window, descriptor(), Some(wgpu::Backends::GL))
        }
    }
}

/// What the lifecycle watch changes: SDL 3.4 delivers the application
/// lifecycle events only to event watches, synchronously, on the SDL thread
/// inside SDL_PollEvent (before it blocks while the activity is paused), so
/// the surface is dropped before Android destroys the native window.
struct Live {
    presenter: Presenter,
    clock: SimClock,
    frames: u64,
    background: bool,
    terminating: bool,
    log_next_frame: bool,
}

impl Live {
    fn status(&self) -> String {
        format!(
            "frame={} sim={:.3}s",
            self.frames,
            self.clock.time().as_secs_f64()
        )
    }
}

/// The SDL thread, the only one whose lifecycle events the watch handles.
static SDL_THREAD: AtomicU64 = AtomicU64::new(0);

/// SDL_EventFilter for SDL_AddEventWatch; `userdata` is the host's
/// `RefCell<Live>`. Other threads' events (input from Java threads, the
/// picker's wake-up) are passed over.
unsafe extern "C" fn lifecycle_watch(userdata: *mut c_void, event: *mut SDL_Event) -> bool {
    // SAFETY: SDL passes a valid event; userdata outlives the watch.
    let kind = SDL_EventType(unsafe { (*event).r#type });
    if !matches!(
        kind,
        SDL_EVENT_WILL_ENTER_BACKGROUND
            | SDL_EVENT_DID_ENTER_BACKGROUND
            | SDL_EVENT_WILL_ENTER_FOREGROUND
            | SDL_EVENT_DID_ENTER_FOREGROUND
            | SDL_EVENT_LOW_MEMORY
            | SDL_EVENT_TERMINATING
    ) {
        return true;
    }
    if SDL_GetCurrentThreadID().0 != SDL_THREAD.load(Ordering::Relaxed) {
        log(&format!(
            "lifecycle event {:#x} off the SDL thread ignored",
            kind.0
        ));
        return true;
    }
    let live = unsafe { &*(userdata as *const RefCell<Live>) };
    let Ok(mut live) = live.try_borrow_mut() else {
        log(&format!(
            "lifecycle event {:#x} while rendering ignored",
            kind.0
        ));
        return true;
    };
    match kind {
        SDL_EVENT_WILL_ENTER_BACKGROUND => {
            log(&format!(
                "lifecycle WILL_ENTER_BACKGROUND {}",
                live.status()
            ));
            live.background = true;
            live.clock.pause();
            live.presenter.suspend();
            log("surface dropped");
        }
        SDL_EVENT_DID_ENTER_BACKGROUND => {
            // Settings are written when applied: nothing to flush.
            log(&format!("lifecycle DID_ENTER_BACKGROUND {}", live.status()));
        }
        SDL_EVENT_WILL_ENTER_FOREGROUND => {
            log(&format!(
                "lifecycle WILL_ENTER_FOREGROUND {}",
                live.status()
            ));
        }
        SDL_EVENT_DID_ENTER_FOREGROUND => {
            log(&format!("lifecycle DID_ENTER_FOREGROUND {}", live.status()));
            live.background = false;
            live.clock.resume(Instant::now());
            live.log_next_frame = true;
        }
        SDL_EVENT_LOW_MEMORY => log(&format!("lifecycle LOW_MEMORY {}", live.status())),
        _ => {
            log(&format!("lifecycle TERMINATING {}", live.status()));
            live.terminating = true;
        }
    }
    true
}

/// Everything presented in the window. Dropped before the window.
struct Host {
    view: View,
    live: Rc<RefCell<Live>>,
    gpu: Gpu,
    discs: DiscImport,
    import_requested: Rc<Cell<bool>>,
}

impl Host {
    fn new(window: &sdl::Window) -> Result<Self, String> {
        let files = files_dir()?;
        match check_bundled_asset() {
            Ok(summary) => log(&format!(
                "bundled asset {}: {summary}",
                BUNDLED_ASSET.to_string_lossy()
            )),
            Err(error) => log(&format!(
                "bundled asset {} unreadable: {error}",
                BUNDLED_ASSET.to_string_lossy()
            )),
        }

        let (gpu, surface) = open_gpu(window)?;
        let info = gpu.adapter.get_info();
        log(&format!(
            "gpu {} ({:?}, {} {})",
            info.name, info.backend, info.driver, info.driver_info
        ));
        // The activity's surface can be replaced while the app starts (a
        // configuration change): retry on the current one.
        let mut surface = Some(surface);
        let mut attempts = 0;
        let presenter = loop {
            let presenter = match surface.take() {
                Some(surface) => Ok(surface),
                None => xem_host::create_surface(&gpu.instance, window),
            }
            .and_then(|surface| {
                Presenter::new(&gpu, surface, window, wgpu::TextureUsages::empty())
            });
            match presenter {
                Ok(presenter) => break presenter,
                Err(error) if attempts < 50 => {
                    log(&format!("surface not ready ({error}); retrying"));
                    attempts += 1;
                    // SAFETY: SDL is initialised.
                    unsafe {
                        SDL_PumpEvents();
                        SDL_Delay(100);
                    }
                }
                Err(error) => return Err(error),
            }
        };
        let (width, height) = presenter.size();
        log(&format!(
            "surface {width}x{height} {:?}, scale {}",
            presenter.format(),
            window.display_scale()
        ));

        let path = files.join("settings.json");
        let settings = SettingsService::open(FileStore::new(&path)).unwrap_or_else(|error| {
            log(&format!("{}: {error}; using defaults", path.display()));
            SettingsService::with_defaults(FileStore::new(&path))
        });
        log(&format!(
            "settings {} revision {}: {:?}",
            path.display(),
            settings.revision(),
            settings.settings()
        ));
        let settings = Rc::new(RefCell::new(settings));
        settings
            .borrow_mut()
            .subscribe(|ack| log(&format!("settings applied {:?}", ack.change)));

        let mut view = View::new(
            &gpu,
            presenter.format(),
            presenter.size(),
            window.display_scale(),
            settings,
            true,
        )?;
        let import_requested = Rc::new(Cell::new(false));
        view.ui().on_import_disc({
            let requested = import_requested.clone();
            move || requested.set(true)
        });

        let live = Rc::new(RefCell::new(Live {
            presenter,
            clock: SimClock::start(Instant::now()),
            frames: 0,
            background: false,
            terminating: false,
            log_next_frame: true,
        }));
        // SAFETY: a plain query; the watch is removed in Drop, before `live`
        // is released.
        unsafe {
            SDL_THREAD.store(SDL_GetCurrentThreadID().0, Ordering::Relaxed);
            if !SDL_AddEventWatch(Some(lifecycle_watch), Rc::as_ptr(&live) as *mut c_void) {
                return Err(format!("SDL_AddEventWatch: {}", sdl::error()));
            }
        }
        Ok(Self {
            view,
            live,
            gpu,
            discs: DiscImport::new(&files),
            import_requested,
        })
    }

    fn run(&mut self, window: &sdl::Window, options: &Options) -> Result<(), String> {
        if let Some(uri) = self.discs.persisted() {
            log(&format!("reopening imported disc {uri}"));
            self.discs.open(&uri);
        }
        if let Some(source) = &options.open {
            self.discs.open(source);
        }
        if options.pick {
            self.discs.pick(window.raw);
        }
        loop {
            let mut event = SDL_Event::default();
            // SAFETY: SDL is initialised; event is a valid out pointer. While
            // the activity is paused SDL blocks in here until it resumes, after
            // calling the lifecycle watch. No borrow of `live` is held.
            while unsafe { SDL_PollEvent(&mut event) } {
                // SAFETY: the union is read according to its type tag.
                match SDL_EventType(unsafe { event.r#type }) {
                    SDL_EVENT_QUIT => return Ok(()),
                    SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
                    | SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED => {
                        let mut live = self.live.borrow_mut();
                        if live.presenter.resize(&self.gpu, window) {
                            self.view
                                .resize(live.presenter.size(), window.display_scale());
                            let (width, height) = live.presenter.size();
                            log(&format!("resized {width}x{height}"));
                        }
                    }
                    // SAFETY: event comes from SDL_PollEvent.
                    _ => unsafe { self.view.handle(window, &event) },
                }
            }
            if self.live.borrow().terminating {
                return Ok(());
            }
            if self.import_requested.take() {
                self.discs.pick(window.raw);
            }
            for identified in self.discs.poll() {
                let text = match &identified.result {
                    Ok(summary) => {
                        log(&format!(
                            "disc identified source={} {summary}",
                            identified.source
                        ));
                        summary
                            .rsplit("disc=")
                            .next()
                            .unwrap_or_default()
                            .to_string()
                    }
                    Err(error) => {
                        log(&format!(
                            "disc not identified source={}: {error}",
                            identified.source
                        ));
                        format!("Not a disc image: {error}")
                    }
                };
                self.view.ui().set_disc_status(&text);
            }
            self.frame(window)?;
        }
    }

    /// Renders and presents one frame if the app is in the foreground and
    /// has a surface, recreating the surface first if it was dropped.
    fn frame(&mut self, window: &sdl::Window) -> Result<(), String> {
        let mut live = self.live.borrow_mut();
        if live.background || !window.has_native_surface() {
            drop(live);
            // Nothing to present: wait for an event.
            // SAFETY: a null event pointer leaves the event queued.
            unsafe { SDL_WaitEventTimeout(std::ptr::null_mut(), 100) };
            return Ok(());
        }
        if live.presenter.is_suspended() {
            if let Err(error) = live.presenter.resume(&self.gpu, window) {
                drop(live);
                log(&format!("surface not recreated ({error}); retrying"));
                // SAFETY: as above.
                unsafe { SDL_WaitEventTimeout(std::ptr::null_mut(), 100) };
                return Ok(());
            }
            self.view
                .resize(live.presenter.size(), window.display_scale());
            let (width, height) = live.presenter.size();
            log(&format!("surface recreated {width}x{height}"));
        }

        let time = live.clock.tick(Instant::now());
        let Some(frame) = live.presenter.acquire(&self.gpu, window)? else {
            return Ok(());
        };
        let output = frame.texture.create_view(&Default::default());
        self.view.render(&self.gpu, &output, time.as_secs_f32())?;
        self.gpu.queue.present(frame);
        live.frames += 1;
        if live.log_next_frame || live.frames.is_multiple_of(LOG_EVERY) {
            log(&format!("frame rendered {}", live.status()));
            live.log_next_frame = false;
        }
        Ok(())
    }
}

impl Drop for Host {
    fn drop(&mut self) {
        // SAFETY: the watch was added with this userdata in Host::new.
        unsafe {
            SDL_RemoveEventWatch(Some(lifecycle_watch), Rc::as_ptr(&self.live) as *mut c_void)
        };
    }
}
