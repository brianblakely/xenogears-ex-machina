//! The Android host loop: SDL3 window, wgpu surface (Vulkan, GLES only when
//! Vulkan is unavailable), the shared view, settings in the app's files
//! directory, disc import, the game (booted from an imported Xenogears disc,
//! its memory cards in the files directory) and the activity lifecycle.

use sdl3_sys::everything::*;
use std::cell::{Cell, RefCell};
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fs::File;
use std::path::PathBuf;
use std::rc::Rc;
use std::sync::atomic::{AtomicU32, AtomicU64, Ordering};
use std::time::{Duration, Instant};
use xem_host::{Game, Presenter, View, sdl};
use xem_render::{Gpu, wgpu};
use xem_settings::{FileStore, SettingsService};

use crate::clock::SimClock;
use crate::disc::DiscImport;

/// The file bundled in the APK's assets that startup reads back.
const BUNDLED_ASSET: &CStr = c"xem-asset-check.txt";
/// Frames between progress lines in the log.
const LOG_EVERY: u64 = 120;
/// The span over which the rendered and game frame rates are measured.
const FPS_SPAN: Duration = Duration::from_secs(5);
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
    /// Play in the headset (OpenXR) once the game is booted: a standalone
    /// headset (Horizon OS) launches the app with this.
    xr: bool,
}

fn options(args: &[String]) -> Result<Options, String> {
    let mut options = Options {
        open: None,
        pick: false,
        xr: false,
    };
    let mut args = args.iter();
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--open" => options.open = Some(args.next().ok_or("--open needs a URI")?.clone()),
            "--pick-disc" => options.pick = true,
            "--xr" => options.xr = true,
            other => return Err(format!("unknown argument {other:?}")),
        }
    }
    Ok(options)
}

fn run(args: &[String]) -> Result<(), String> {
    let options = options(args)?;
    let run = RUNS.fetch_add(1, Ordering::Relaxed) + 1;
    log(&format!("start (run {run} in this process)"));
    // Back toggles the settings panel instead of leaving the activity. (A
    // recreated activity starts SDL_main again: XemActivity allows it.)
    // SAFETY: called before SDL_Init with NUL-terminated strings.
    unsafe { SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, c"1".as_ptr()) };
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
    let directory = PathBuf::from(
        unsafe { CStr::from_ptr(path) }
            .to_string_lossy()
            .into_owned(),
    );
    for name in ["discs", "cards", "saves"] {
        std::fs::create_dir_all(directory.join(name)).map_err(|e| format!("{}: {e}", directory.display()))?;
    }
    Ok(directory)
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
    /// The game, once an imported Xenogears disc booted it; its clock stops
    /// with the simulation clock.
    game: Option<Game>,
    /// Pad 1's buttons last frame (changes are logged).
    buttons: u16,
    /// The frame a session log (files/script.json) ends at: its memory hash
    /// is logged there and the game waits.
    script_end: Option<u64>,
    frames: u64,
    background: bool,
    terminating: bool,
    /// The touch deck's layout was logged.
    deck_logged: bool,
    log_next_frame: bool,
    /// Start of the current frame-rate span: (time, rendered frames, game vblanks).
    fps: (Instant, u64, u64),
}

impl Live {
    fn status(&mut self) -> String {
        let game = match &mut self.game {
            Some(game) => format!(" game_vblank={} fast_forward={}", game.vblanks(), game.time_status().0),
            None => String::new(),
        };
        format!(
            "frame={} sim={:.3}s{game}",
            self.frames,
            self.clock.time().as_secs_f64()
        )
    }

    /// Starts a frame-rate span now.
    fn restart_fps(&mut self) {
        let vblanks = self.game.as_mut().map_or(0, Game::vblanks);
        self.fps = (Instant::now(), self.frames, vblanks);
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
            if let Some(game) = &mut live.game {
                game.pause();
                log("game paused");
            }
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
            if let Some(game) = &mut live.game {
                game.resume();
                log("game resumed");
            }
            live.restart_fps();
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
    /// Xenogears discs identified and not yet the game's, by number.
    known: std::collections::BTreeMap<u8, (File, String)>,
    /// The documents reopened at start still identifying: the game boots
    /// once they are, from disc 1 when it is among them.
    reopening: Vec<String>,
    import_requested: Rc<Cell<bool>>,
    /// The memory cards' directory (card1.mcd, card2.mcd).
    cards: PathBuf,
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
                        // The activity ended while this run waited for it
                        // (XemActivity's onDestroy waits for the run).
                        if SDL_HasEvent(SDL_EVENT_QUIT.0) {
                            return Err("the activity ended before its surface was ready".into());
                        }
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
        view.set_host_class(xem_render::ps1::HostClass::Mobile);
        view.set_state_directory(files.clone());
        // Each app operation's outcome with the game's memory hash after it
        // (android-smoke.sh compares a state's save and load).
        view.on_operation(log);
        view.ui().on_import_disc({
            let requested = import_requested.clone();
            move || requested.set(true)
        });

        let live = Rc::new(RefCell::new(Live {
            presenter,
            clock: SimClock::start(Instant::now()),
            game: None,
            buttons: 0,
            script_end: None,
            frames: 0,
            background: false,
            terminating: false,
            deck_logged: false,
            log_next_frame: true,
            fps: (Instant::now(), 0, 0),
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
            known: Default::default(),
            reopening: Vec::new(),
            import_requested,
            cards: files.join("cards"),
        })
    }

    fn run(&mut self, window: &sdl::Window, options: &Options) -> Result<(), String> {
        self.reopening = self.discs.persisted();
        for uri in &self.reopening {
            log(&format!("reopening imported disc {uri}"));
            self.discs.open(uri);
        }
        if let Some(source) = &options.open {
            self.discs.open(source);
        }
        if options.pick {
            self.discs.pick(window.raw);
        }
        let mut headset = options.xr;
        loop {
            #[cfg(has_game_module)]
            if headset && self.live.borrow().game.is_some() {
                headset = false;
                if self.play_in_headset() {
                    return Ok(());
                }
            }
            #[cfg(not(has_game_module))]
            let _ = &mut headset;
            let mut event = SDL_Event::default();
            // SAFETY: SDL is initialised; event is a valid out pointer. While
            // the activity is paused SDL blocks in here until it resumes, after
            // calling the lifecycle watch. No borrow of `live` is held.
            while unsafe { SDL_PollEvent(&mut event) } {
                // SAFETY: the union is read according to its type tag.
                match SDL_EventType(unsafe { event.r#type }) {
                    SDL_EVENT_QUIT => return Ok(()),
                    SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
                    | SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED
                    | SDL_EVENT_WINDOW_SAFE_AREA_CHANGED => {
                        let mut live = self.live.borrow_mut();
                        if live.presenter.resize(&self.gpu, window) {
                            self.view
                                .resize(live.presenter.size(), window.display_scale());
                            let (width, height) = live.presenter.size();
                            log(&format!("resized {width}x{height}"));
                        }
                        // The game picture stays clear of cutouts and system bars.
                        let safe = window.safe_area();
                        self.view.set_safe_area(safe);
                        log(&format!("safe area {safe:?}"));
                    }
                    // SAFETY: event comes from SDL_PollEvent.
                    _ => unsafe { self.view.handle(window, &event) },
                }
            }
            if self.live.borrow().terminating || self.view.quit_requested() {
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
                self.reopening.retain(|uri| *uri != identified.source);
                if let Some((number, disc)) = identified.disc {
                    self.known.insert(number, (disc, identified.source));
                }
            }
            self.place_discs();
            self.frame(window)?;
        }
    }

    /// Hands the identified discs to the game: with none running (and the
    /// documents reopened at start identified) disc 1 boots, else disc 2;
    /// the others go to its drive's changer, for the game's disc changes.
    fn place_discs(&mut self) {
        if !self.reopening.is_empty() || self.known.is_empty() {
            return;
        }
        if self.live.borrow().game.is_none() {
            let number = if self.known.contains_key(&1) { 1 } else { 2 };
            if let Some((disc, source)) = self.known.remove(&number) {
                self.boot(disc, &source);
            }
        }
        let mut live = self.live.borrow_mut();
        let Some(game) = live.game.as_mut() else { return };
        for (number, (disc, source)) in std::mem::take(&mut self.known) {
            match xem_disc::FileReadAt::new(disc)
                .map_err(|e| e.to_string())
                .and_then(|reader| xem_disc::open_reader(reader).map_err(|e| e.to_string()))
            {
                Ok(disc) => {
                    game.add_disc(number, disc);
                    log(&format!("disc {number} for the game's disc changes: {source}"));
                }
                Err(error) => log(&format!("disc {number} not opened: {source}: {error}")),
            }
        }
    }

    /// A build without the game module cannot play a disc.
    #[cfg(not(has_game_module))]
    fn boot(&mut self, _disc: File, source: &str) {
        log(&format!("game not booted source={source}: this build has no game module"));
    }

    /// Boots the game from an identified Xenogears disc and hides the
    /// settings panel.
    #[cfg(has_game_module)]
    fn boot(&mut self, disc: File, source: &str) {
        let booted = std::fs::create_dir_all(&self.cards)
            .map_err(|error| format!("{}: {error}", self.cards.display()))
            .and_then(|()| {
                let reader = xem_disc::FileReadAt::new(disc).map_err(|e| e.to_string())?;
                let disc = xem_disc::open_reader(reader).map_err(|e| e.to_string())?;
                // The Kanji font of the BIOS image imported into the files
                // directory (Import BIOS; the storage line says why when it
                // is not used), else the substitute glyphs.
                let options = xem_game::BootOptions {
                    cards: Some(self.cards.clone()),
                    stored_bios: self.cards.parent().and_then(xem_game::storage::stored_bios),
                    audio_output: true,
                    ..Default::default()
                };
                xem_game::boot_source(disc, &options).map_err(|e| e.to_string())
            });
        let mut live = self.live.borrow_mut();
        live.game = None;
        match booted {
            Ok((mut session, identity)) => {
                // Where the save titles' font came from (android-smoke.sh
                // checks an imported image's), and why an imported one is
                // not used.
                let services = session.runtime.services();
                let font = match &services.kanji_font_notice {
                    Some(notice) => format!("substitute ({notice})"),
                    None => serde_json::json!(services.kanji_font).as_str().unwrap_or("none").to_string(),
                };
                let mut game = Game::new(session);
                // A session log pushed beside the cards (tools/native_scenario.py
                // --log) plays instead of the player: android-smoke.sh's slice.
                live.script_end = None;
                let script = self.cards.with_file_name("script.json");
                if let Ok(text) = std::fs::read_to_string(&script) {
                    match serde_json::from_str::<serde_json::Value>(&text)
                        .map_err(|e| e.to_string())
                        .and_then(|value| game.schedule(&value).map(|()| value))
                    {
                        Ok(value) => {
                            let end = value["frames"].as_u64();
                            game.set_frame_limit(end);
                            live.script_end = end;
                            log(&format!("script {} to frame {end:?}", script.display()));
                        }
                        Err(error) => log(&format!("script {}: {error}", script.display())),
                    }
                }
                log(&format!(
                    "game booted source={source} boot={} cards={} font={font} audio={}",
                    identity.boot_path,
                    self.cards.display(),
                    if game.has_audio() {
                        "stream opened (S16LE stereo 44100 Hz)".to_string()
                    } else {
                        format!("none ({}); the game runs silent", sdl::error())
                    }
                ));
                live.game = Some(game);
                live.restart_fps();
                self.view.set_panel_visible(false);
            }
            Err(error) => log(&format!("game not booted source={source}: {error}")),
        }
    }

    /// Hands the booted game to an OpenXR session (xem_host::xr_play) until
    /// it ends, then takes it back to the flat window; returns whether the
    /// player chose Quit there (the app ends).
    #[cfg(has_game_module)]
    fn play_in_headset(&mut self) -> bool {
        let Some(game) = self.live.borrow_mut().game.take() else { return false };
        // SAFETY: SDL's JNI environment and activity on its thread; the
        // JavaVM pointer and the activity reference outlive the session.
        let platform = unsafe {
            let env = SDL_GetAndroidJNIEnv() as *mut jni_sys::JNIEnv;
            let mut vm: *mut jni_sys::JavaVM = std::ptr::null_mut();
            ((**env).v1_1.GetJavaVM)(env, &mut vm);
            xem_xr::openxr::AndroidPlatformInfo::new(vm.cast(), SDL_GetAndroidActivity().cast())
        };
        log("entering the headset");
        let settings = self.view.settings().clone();
        // Save state keeps its state in the files directory, as the window does.
        let play = xem_host::xr_play::XrPlay { state_directory: self.cards.parent().map(PathBuf::from), ..Default::default() };
        match xem_host::xr_play::run(&platform, &play, settings, game) {
            Ok(ended) => {
                log("headset session ended");
                self.live.borrow_mut().game = Some(ended.game);
                ended.quit
            }
            Err(error) => {
                log(&format!("headset: {error}"));
                false
            }
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
            self.view.set_safe_area(window.safe_area());
            let (width, height) = live.presenter.size();
            log(&format!("surface recreated {width}x{height}"));
        }

        let live = &mut *live;
        let time = live.clock.tick(Instant::now());
        if self.view.update_app(window,&mut live.game) && let Some(game) = &mut live.game {
            let settings = self.view.settings().borrow().settings().clone();
            self.view.assist(game);
            // The touch deck's place, once laid out (for tests that touch it).
            let deck = self.view.deck_controls();
            if !deck.is_empty() && !live.deck_logged {
                live.deck_logged = true;
                for (control, rect) in &deck {
                    log(&format!("touch deck {control:?} {:.0} {:.0} {:.0} {:.0}", rect.x, rect.y, rect.width, rect.height));
                }
            }
            let (context, targets) = (game.pad_context(), game.targets());
            let buttons = self.view.pad_buttons(context, targets);
            if buttons != live.buttons {
                log(&format!("pad buttons {buttons:#06x} game_vblank={}", game.vblanks()));
                live.buttons = buttons;
            }
            game.want_attack_page(self.view.attack_pointed());
            game.want_inspection(self.view.first_person());
            let result = game.update(buttons, &settings, || window.focused());
            if let Ok(frames) = result {
                self.view.frames_ran(frames);
                game.follow_sound_mode(self.view.settings());
            }
            if let Err(trap) = result {
                log(&format!("game stopped: {trap}"));
                game.pause();
                self.view.ui().set_operation_error(&format!("Game stopped: {trap}. Load a state to recover."));
                self.view.set_panel_visible(true);
            } else {
                self.view.set_game_frame(&self.gpu, game);
                if live.script_end.is_some_and(|end| game.frames_run() >= end) {
                    log(&format!("script frame {} memory_hash {}", game.frames_run(), game.memory_hash()));
                    live.script_end = None;
                }
            }
        }
        let Some(frame) = live.presenter.acquire(&self.gpu, window)? else {
            return Ok(());
        };
        let output = frame.texture.create_view(&Default::default());
        self.view.render(&self.gpu, &output, time.as_secs_f32())?;
        self.gpu.queue.present(frame);
        live.frames += 1;
        if live.log_next_frame || live.frames.is_multiple_of(LOG_EVERY) {
            // movie=1: the frame showed a movie frame rebuilt from what the
            // MDEC decoded (the Enhanced movies setting).
            let status = live.status();
            log(&format!("frame rendered {status} movie={}", u8::from(self.view.presented_movie())));
            live.log_next_frame = false;
        }
        let elapsed = live.fps.0.elapsed();
        if elapsed >= FPS_SPAN {
            let seconds = elapsed.as_secs_f64();
            let rendered = (live.frames - live.fps.1) as f64 / seconds;
            let game = match live.game.as_mut() {
                // A state loaded or a rewind takes the count back: no rate then.
                Some(game) => match game.vblanks().checked_sub(live.fps.2) {
                    Some(ran) => format!(" game={:.1}", ran as f64 / seconds),
                    None => " game=- (the vblank count went back)".to_string(),
                },
                None => String::new(),
            };
            log(&format!("fps rendered={rendered:.1}{game} over {seconds:.1}s"));
            live.restart_fps();
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
