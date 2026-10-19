//! The desktop host: an SDL3 window presenting through wgpu, the game (with
//! `--disc`, else the test scene as a stand-in) and the Slint settings panel as
//! an overlay (`xem_host::View`). One wgpu device and queue serve the game
//! image, the compositor and Slint.

use sdl3_sys::everything::*;
use std::cell::RefCell;
use std::path::PathBuf;
use std::rc::Rc;
use std::time::Instant;
use xem_host::{Game, Presenter, View, sdl};
use xem_render::{Gpu, read_rgba8, wgpu};
use xem_settings::{FileStore, MemoryStore, SettingsService};

#[derive(Clone)]
struct Options {
    exit_after_frames: Option<u64>,
    screenshot: Option<PathBuf>,
    show_settings: bool,
    /// The user's disc (a CHD, a cue or bin, or a Redump-verified ISO): play
    /// the game, the other disc beside it serving its disc changes.
    disc: Option<PathBuf>,
    cards: Option<PathBuf>,
    bios: Option<PathBuf>,
    /// A session log to play (pads and setup) instead of the player's input.
    script: Option<PathBuf>,
    /// Exit when the game has run this many frames, reporting its memory hash.
    exit_at_frame: Option<u64>,
    /// Load this state file once the game has booted, before either loop
    /// (as Load state loads one).
    load_state: Option<PathBuf>,
    /// Write the game's state to the path once it has run this many frames.
    save_state_at: Option<(u64, PathBuf)>,
    /// Play in an OpenXR headset on a virtual screen (xr.rs).
    xr: bool,
    /// Headset tests: pause the game at headset frame N for M frames.
    xr_pause: Option<(u64, u64)>,
    /// Headset tests: save both eyes of the last frame here (or of the
    /// headset frames listed, each in a directory named by its number).
    xr_capture: Option<PathBuf>,
    xr_capture_at: Vec<u64>,
    /// Headset tests: scripted hand gestures (headset frame, gesture name).
    xr_hands: Vec<(u64, String)>,
    /// Headset tests: Recenter at these headset frames, as the app action does.
    xr_recenter_at: Vec<u64>,
    /// Headset tests: headset frames N to N+M-1 as the runtime reports lost
    /// tracking.
    xr_lose_tracking: Option<(u64, u64)>,
    /// Headset tests: scripted controller buttons (headset frame, the
    /// gamepad's buttons as `xem_settings::controls::PAD_NAMES` bits, the
    /// settings panel's logical pixel the right controller points at).
    xr_pad: Vec<(u64, u16, Option<(f32, f32)>)>,
    /// Headset tests: the scripted right stick (headset frame, x and y).
    xr_stick: Vec<(u64, [f32; 2])>,
    /// Agents' commands on stdin while the game plays (`--control`).
    control: bool,
}

fn options() -> Result<Options, String> {
    let mut options = Options {
        exit_after_frames: None,
        screenshot: None,
        show_settings: false,
        disc: None,
        cards: None,
        bios: None,
        script: None,
        exit_at_frame: None,
        load_state: None,
        save_state_at: None,
        xr: false,
        xr_pause: None,
        xr_capture: None,
        xr_capture_at: Vec::new(),
        xr_hands: Vec::new(),
        xr_recenter_at: Vec::new(),
        xr_lose_tracking: None,
        xr_pad: Vec::new(),
        xr_stick: Vec::new(),
        control: false,
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
            "--script" => options.script = Some(args.next().ok_or("--script needs a session log")?.into()),
            "--exit-at-frame" => {
                let count = args.next().ok_or("--exit-at-frame needs a frame")?;
                options.exit_at_frame = Some(count.parse().map_err(|_| format!("bad frame {count:?}"))?);
            }
            "--load-state" => options.load_state = Some(args.next().ok_or("--load-state needs a state file")?.into()),
            "--save-state-at" => {
                let value = args.next().ok_or("--save-state-at needs FRAME:PATH")?;
                let (at, path) = value.split_once(':').ok_or("--save-state-at needs FRAME:PATH")?;
                options.save_state_at = Some((at.parse().map_err(|_| format!("bad --save-state-at frame {at:?}"))?, path.into()));
            }
            "--xr" => options.xr = true,
            "--control" => options.control = true,
            "--xr-pause" => {
                let value = args.next().ok_or("--xr-pause needs FRAME,COUNT")?;
                let (at, count) = value.split_once(',').ok_or("--xr-pause needs FRAME,COUNT")?;
                options.xr_pause = Some((at.parse().map_err(|_| "bad --xr-pause")?, count.parse().map_err(|_| "bad --xr-pause")?));
            }
            "--xr-capture" => options.xr_capture = Some(args.next().ok_or("--xr-capture needs a directory")?.into()),
            "--xr-capture-at" => {
                let value = args.next().ok_or("--xr-capture-at needs FRAME,...")?;
                for item in value.split(',') {
                    options.xr_capture_at.push(item.parse().map_err(|_| format!("bad --xr-capture-at frame {item:?}"))?);
                }
            }
            "--xr-recenter-at" => {
                let value = args.next().ok_or("--xr-recenter-at needs FRAME,...")?;
                for item in value.split(',') {
                    options.xr_recenter_at.push(item.parse().map_err(|_| format!("bad --xr-recenter-at frame {item:?}"))?);
                }
            }
            "--xr-lose-tracking" => {
                let value = args.next().ok_or("--xr-lose-tracking needs FRAME,COUNT")?;
                let (at, count) = value.split_once(',').ok_or("--xr-lose-tracking needs FRAME,COUNT")?;
                options.xr_lose_tracking = Some((at.parse().map_err(|_| "bad --xr-lose-tracking")?, count.parse().map_err(|_| "bad --xr-lose-tracking")?));
            }
            "--xr-hands" => {
                let value = args.next().ok_or("--xr-hands needs FRAME:GESTURE,...")?;
                for item in value.split(',') {
                    let (at, name) = item.split_once(':').ok_or("--xr-hands needs FRAME:GESTURE,...")?;
                    if xem_assist::synthetic::gesture(name.split('@').next().unwrap_or(name)).is_none() {
                        return Err(format!("unknown gesture {name}"));
                    }
                    options.xr_hands.push((at.parse().map_err(|_| "bad --xr-hands frame")?, name.to_string()));
                }
            }
            "--xr-pad" => {
                let value = args.next().ok_or("--xr-pad needs FRAME:BUTTONS,...")?;
                for item in value.split(',') {
                    let (at, names) = item.split_once(':').ok_or("--xr-pad needs FRAME:BUTTONS,...")?;
                    // `BUTTONS@panel:X/Y` points the right controller at the panel's logical pixel too.
                    let (names, pixel) = match names.split_once("@panel:") {
                        Some((names, pixel)) => {
                            let parsed = pixel.split_once('/').and_then(|(x, y)| x.parse().ok().zip(y.parse().ok()));
                            (names, Some(parsed.ok_or_else(|| format!("bad --xr-pad panel pixel {pixel:?} (X/Y)"))?))
                        }
                        None => (names, None),
                    };
                    let mut buttons = 0u16;
                    for name in names.split('+').filter(|name| *name != "none") {
                        let bit = xem_settings::controls::PAD_NAMES.iter().position(|n| *n == name)
                            .ok_or_else(|| format!("unknown button {name} (buttons: {})", xem_settings::controls::PAD_NAMES.join(", ")))?;
                        buttons |= 1 << bit;
                    }
                    options.xr_pad.push((at.parse().map_err(|_| "bad --xr-pad frame")?, buttons, pixel));
                }
                options.xr_pad.sort_by_key(|(at, ..)| *at);
            }
            "--xr-stick" => {
                let value = args.next().ok_or("--xr-stick needs FRAME:X/Y,...")?;
                for item in value.split(',') {
                    let parsed = item.split_once(':').and_then(|(at, xy)| {
                        let (x, y) = xy.split_once('/')?;
                        Some((at.parse().ok()?, [x.parse::<f32>().ok()?.clamp(-1.0, 1.0), y.parse::<f32>().ok()?.clamp(-1.0, 1.0)]))
                    });
                    options.xr_stick.push(parsed.ok_or_else(|| format!("bad --xr-stick entry {item:?} (FRAME:X/Y)"))?);
                }
                options.xr_stick.sort_by_key(|(at, _)| *at);
            }
            "--help" => {
                println!(
                    "xem-desktop [--disc DISC [--cards DIR] [--bios FILE] [--script LOG] \
                     [--exit-at-frame N] [--load-state FILE] [--save-state-at N:FILE] [--xr [--xr-pause N,M] [--xr-capture DIR [--xr-capture-at N,...]] [--xr-hands N:GESTURE,...] [--xr-recenter-at N,...] [--xr-lose-tracking N,M] \
                     [--xr-pad N:BUTTONS[@panel:X/Y],...] [--xr-stick N:X/Y,...]]] [--show-settings] [--exit-after-frames N] [--screenshot out.png] [--control]\n\
                     DISC is a CHD, a cue sheet or a Redump-verified ISO; the game's other disc, \
                     when it is beside it, goes in when the game asks for it.\n\
                     --xr plays in an OpenXR headset on a virtual screen: the controllers are the \
                     game bindings' gamepad, hands gesture (overhand grab Pause, prayer hands Settings, \
                     pinch to walk, thumb taps and swipes), the left menu button toggles Settings; \
                     in a field the underhand grab (anywhere but its menu), Toggle first-person view's chord \
                     (the right stick's click with the right trigger) or, where Select toggles it, the right stick's \
                     click released alone looks around from the character's eyes, the head looking and the right \
                     stick turning in steps; \
                     the app's chords take their buttons first (the right stick's click, Back, with A saves \
                     the state, with Y loads it, with the left grip rewinds, with the right grip toggles fast-forward, \
                     with the right trigger the first-person view); \
                     --xr-hands plays synthetic gestures (pause, inspect, pinch, tap, swipe-up/down/left/right, \
                     prayer, spread and contract (two whole-hand pinches parting or closing: two-hand scaling), lose; \
                     NAME@X/Y also aims eye gaze at that screen pixel, NAME@panel:X/Y at the settings panel's \
                     logical pixel, NAME@world:X/Y/Z at that point of the game's world on the diorama's table) at headset frames, \
                     for tests without hand tracking; --xr-recenter-at recenters the view at those headset frames \
                     (as the panel's Recenter does); --xr-lose-tracking takes M headset frames from N as the \
                     runtime reports lost tracking (no head, no valid views), for runtimes that never lose it; \
                     --xr-pad holds the gamepad's BUTTONS (South, East, West, North, L1, R1, LT, RT, Back, Start \
                     joined by +, or none) from headset frame N to the next entry, for tests without controller buttons \
                     (@panel:X/Y also points the right controller at the panel's logical pixel, R1 its trigger); \
                     --xr-stick holds the right stick at X/Y (each -1 to 1, Y up) from headset frame N to the next entry; \
                     --xr-capture-at saves both eyes at those headset frames (looking from within the scene, \
                     each eye's picture as the renderer drew it too: left-picture.png, right-picture.png).\n\
                     --control answers JSON commands on stdin, a line each, while the game plays: \
                     the command layer's (status, context, navigate, act, ...; status adds renderer.movie, \
                     whether the window last showed a rebuilt movie frame), settings, \
                     settings_apply {{change}} (the panel's own typed change), panel {{visible}}, \
                     first_person {{on, yaw, pitch}} (each optional: the first-person view on, off, or toggled as Toggle \
                     first-person view does, anywhere in a field but its menu under Ex Machina, then turned to yaw degrees \
                     right of the heading it began with and pitch up; it answers first_person and look, and status's \
                     inspecting follows from the next frame), \
                     import_disc {{path}} (as the panel's Import disc…: plays it, or gives it to the changer; not with --xr), \
                     bios_import {{path}} (as the panel's Import BIOS: kept for every boot, the running game's at once) and \
                     app {{request}} (the panel's and the bindings' operations as they run them, answering {{ok, message}} or {{error}}: \
                     {{\"Rewind\": true}} holds rewind until {{\"Rewind\": false}}, {{\"Action\": \"RetryStorage\"}} retries pending card writes, \
                     {{\"Action\": \"Recenter\"}} recentres a headset's view; not the file pickers, whose files agents name instead).\n\
                     --script plays a session log (tools/native_scenario.py --log); \
                     --exit-at-frame prints the memory hash at that game frame and exits; \
                     --load-state loads a state file (Save state's, xem-headless's state_save, a browser's \
                     export) once the game has booted, before the window or the headset plays it; \
                     --save-state-at writes the game's state to FILE when it has run N frames (the game's \
                     count since boot, which a loaded state goes on from), printing its memory hash, and plays on.\n\
                     Escape toggles the settings panel (Graphics has Fullscreen; Quit ends the app); its \
                     Controls section lists and rebinds the game's keys, buttons and mouse buttons \
                     (by default WASD move, E confirm, F cancel, Shift run, Space jump, Tab the game menu, \
                     which Tab also closes, Left/Right the camera, Enter start, Backspace select). \
                     The mouse points at menu entries and the wheel scrolls them; Back+Start toggles settings.\n\
                     Walking in a field, Select (Backspace, or Back alone) toggles a first-person view where \
                     Select shows no map, as does the right stick's click anywhere in a field but its menu, and so \
                     does Toggle first-person view (V, Back+R1, the touch deck's LOOK; rebindable in Shortcuts); \
                     in it the arrow keys, the right stick and (with the mouse camera setting) the mouse look around, \
                     and the character stays put."
                );
                std::process::exit(0);
            }
            "--version" => {
                println!("Xenogears Ex Machina {}", option_env!("XEM_RELEASE_VERSION").unwrap_or(env!("CARGO_PKG_VERSION")));
                std::process::exit(0);
            }
            other => return Err(format!("unknown argument {other:?} (see --help)")),
        }
    }
    if options.screenshot.is_some() && options.exit_after_frames.is_none() {
        return Err("--screenshot is taken at --exit-after-frames".into());
    }
    if let (Some((at, _)), Some(exit)) = (&options.save_state_at, options.exit_at_frame)
        && *at > exit
    {
        return Err(format!("--save-state-at frame {at} comes after --exit-at-frame {exit}"));
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

fn run(mut options: Options) -> Result<(), String> {
    let directory = FileStore::default_path().and_then(|p| p.parent().map(std::path::Path::to_path_buf))
        .ok_or("cannot locate the portable installation directory")?;
    let explicit = options.disc.as_ref().map(std::path::absolute).transpose().map_err(|e| e.to_string())?;
    let disc = xem_disc::install::prepare(&directory).map_err(|e| format!("{}: {e}", directory.display()))?;
    if let Some(path) = explicit && path.parent() == Some(directory.as_path()) && !path.exists() {
        let relocated = directory.join("discs").join(path.file_name().unwrap());
        if relocated.is_file() { options.disc = Some(relocated); }
    }
    if options.disc.is_none() { options.disc = disc; }
    if (options.load_state.is_some() || options.save_state_at.is_some()) && options.disc.is_none() {
        return Err("no disc found; put your disc images beside the executable or use --disc".into());
    }
    if options.xr {
        return run_xr(&options);
    }
    let sdl = sdl::Sdl::init()?;
    let settings = Rc::new(RefCell::new(open_settings()));
    let window = sdl::Window::new(&sdl, "Xenogears Ex Machina", 1280, 800)?;
    // The saved choice before anything is drawn (the window manager's own
    // fullscreen is left alone: windowed asks nothing of it).
    let fullscreen = settings.borrow().settings().presentation.fullscreen;
    if fullscreen {
        window.set_fullscreen(true);
    }
    let result = Host::new(&window, &options, settings, fullscreen).and_then(|mut host| host.run(&window, &options));
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
    /// What the window was last asked: the Fullscreen setting when it was.
    fullscreen: bool,
    /// XEM_FRAME_STATS: a line a second of where the frame loop's time goes.
    stats: Option<Stats>,
    /// The panel's Import disc…: asked for, and the file SDL's dialog chose.
    disc_asked: Rc<std::cell::Cell<bool>>,
    disc_chosen: (std::sync::mpsc::Sender<Option<String>>, std::sync::mpsc::Receiver<Option<String>>),
    options: Options,
}

/// The frame loop's time per stage, printed once a second.
struct Stats {
    since: Instant,
    iterations: u32,
    frames: u32,
    /// Total and worst milliseconds of: settings and actions, game frames,
    /// taking the trace, acquiring the surface, rendering, presenting, and
    /// the whole iteration.
    stages: [(f64, f64); 7],
    slow: u32,
}

impl Stats {
    const NAMES: [&str; 7] = ["app", "game", "take", "acquire", "render", "present", "loop"];

    fn new() -> Self {
        Stats { since: Instant::now(), iterations: 0, frames: 0, stages: [(0.0, 0.0); 7], slow: 0 }
    }

    fn add(&mut self, stage: usize, from: Instant, to: Instant) {
        let ms = to.duration_since(from).as_secs_f64() * 1000.0;
        let (total, worst) = &mut self.stages[stage];
        *total += ms;
        *worst = worst.max(ms);
    }

    fn end_iteration(&mut self, began: Instant, frames: u32) {
        let now = Instant::now();
        self.add(6, began, now);
        self.iterations += 1;
        self.frames += frames;
        if now.duration_since(began).as_secs_f64() > 0.02 {
            self.slow += 1;
        }
        if now.duration_since(self.since).as_secs_f64() >= 1.0 {
            let n = f64::from(self.iterations.max(1));
            let stages: Vec<String> = Self::NAMES.iter().zip(&self.stages)
                .map(|(name, (total, worst))| format!("{name} {:.2}/{:.1}", total / n, worst)).collect();
            eprintln!("xem-desktop stats: {} iterations, {} game frames, {} over 20 ms; ms mean/worst: {}",
                self.iterations, self.frames, self.slow, stages.join(", "));
            *self = Stats::new();
        }
    }
}

#[cfg(all(has_game_module, not(target_os = "macos")))]
fn run_xr(options: &Options) -> Result<(), String> {
    let _sdl = sdl::Sdl::init_without_video()?;
    let disc = options.disc.as_deref().ok_or("--xr needs --disc")?;
    let mut game = open_game(options, disc)?;
    load_state(&mut game, options)?;
    if let Some(path) = &options.script {
        let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
        let log: serde_json::Value = serde_json::from_str(&text).map_err(|e| format!("{}: {e}", path.display()))?;
        game.schedule(&log)?;
    }
    game.set_frame_limit(frame_limit(options));
    let settings = Rc::new(RefCell::new(open_settings()));
    // Save state keeps its state beside the settings, as in the window.
    let play = xem_host::xr_play::XrPlay {
        show_settings: options.show_settings,
        pause: options.xr_pause,
        capture: options.xr_capture.clone(),
        capture_at: options.xr_capture_at.clone(),
        hands: options.xr_hands.clone(),
        recenter_at: options.xr_recenter_at.clone(),
        lose_tracking: options.xr_lose_tracking,
        pad: options.xr_pad.clone(),
        stick: options.xr_stick.clone(),
        exit_after_frames: options.exit_after_frames,
        exit_at_frame: options.exit_at_frame,
        save_state_at: options.save_state_at.clone(),
        state_directory: None,
        control: options.control,
    };
    xem_host::xr_play::run(&(), &play, settings, game).map(|_| ())
}

#[cfg(all(not(has_game_module), not(target_os = "macos")))]
fn run_xr(_options: &Options) -> Result<(), String> {
    Err("this build has no game module (python3 tools/game_module.py builds it)".into())
}

#[cfg(target_os = "macos")]
fn run_xr(_options: &Options) -> Result<(), String> {
    Err("native OpenXR is not supported on macOS; flat play uses Metal".into())
}

/// A build without the game module cannot play a disc.
#[cfg(not(has_game_module))]
fn open_game(_options: &Options, _disc: &std::path::Path) -> Result<Game, String> {
    Err("this build has no game module (python3 tools/game_module.py builds it)".into())
}

/// `--load-state`: the state file loaded into the booted game, as Load
/// state loads one (a refusal names what differs).
fn load_state(game: &mut Game, options: &Options) -> Result<(), String> {
    if let Some(path) = &options.load_state {
        game.command(&serde_json::json!({"cmd": "state_load", "path": path})).map_err(|error| format!("--load-state {}: {error}", path.display()))?;
    }
    Ok(())
}

/// The frame the game may run to: `--save-state-at`'s (the loops then
/// write the state and raise it), else `--exit-at-frame`'s.
fn frame_limit(options: &Options) -> Option<u64> {
    options.save_state_at.as_ref().map(|(at, _)| *at).or(options.exit_at_frame)
}

/// Boots the user's disc and plays it, with cards under the settings'
/// directory unless `--cards` names one, and the Kanji font of the BIOS
/// image `--bios` names, else of the one imported into the settings'
/// directory (Import BIOS; the storage line says why when it is not used),
/// else the substitute glyphs.
#[cfg(has_game_module)]
fn open_game(options: &Options, disc: &std::path::Path) -> Result<Game, String> {
    let directory = FileStore::default_path().and_then(|p| p.parent().map(std::path::Path::to_path_buf));
    let cards = options.cards.clone().or_else(|| directory.as_ref().map(|d| d.join("cards")));
    if let Some(dir) = &cards {
        std::fs::create_dir_all(dir).map_err(|e| format!("{}: {e}", dir.display()))?;
    }
    let boot = xem_game::BootOptions {
        cards,
        bios: options.bios.clone(),
        stored_bios: directory.as_deref().and_then(xem_game::storage::stored_bios),
        audio_output: true,
        ..Default::default()
    };
    let (session, identity) = xem_game::boot(disc, &boot).map_err(|e| e.to_string())?;
    eprintln!("xem-desktop: {}", identity.boot_path);
    let game = Game::new(session);
    if !game.has_audio() {
        eprintln!("xem-desktop: no audio device; the game runs silent");
    }
    Ok(game)
}

impl Host {
    fn new(
        window: &sdl::Window,
        options: &Options,
        settings: Rc<RefCell<SettingsService>>,
        fullscreen: bool,
    ) -> Result<Self, String> {
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
        let mut view = View::new(
            &gpu,
            presenter.format(),
            presenter.size(),
            window.display_scale(),
            settings,
            options.show_settings,
        )?;
        view.ui().set_fullscreen_offered(true);
        // The panel is the launcher too: Import disc… plays a disc (or gives
        // the running game its other one).
        let disc_asked = Rc::new(std::cell::Cell::new(false));
        view.ui().on_import_disc({
            let asked = disc_asked.clone();
            move || asked.set(true)
        });
        window.set_text_input(options.show_settings);
        let mut game = options.disc.as_deref().map(|disc| open_game(options, disc)).transpose()?;
        if let Some(game) = &mut game {
            load_state(game, options)?;
        }
        if let (Some(game), Some(path)) = (&mut game, &options.script) {
            let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
            let log: serde_json::Value = serde_json::from_str(&text).map_err(|e| format!("{}: {e}", path.display()))?;
            game.schedule(&log)?;
        }
        if let Some(game) = &mut game {
            game.set_frame_limit(frame_limit(options));
        }
        Ok(Self {
            view,
            presenter,
            gpu,
            start: Instant::now(),
            game,
            fullscreen,
            stats: std::env::var_os("XEM_FRAME_STATS").map(|_| Stats::new()),
            disc_asked,
            disc_chosen: std::sync::mpsc::channel(),
            options: options.clone(),
        })
    }

    fn run(&mut self, window: &sdl::Window, options: &Options) -> Result<(), String> {
        let mut frames = 0u64;
        let mut minimized = false;
        let mut state_saved = false;
        let control = options.control.then(xem_host::Control::spawn);
        loop {
            if let Some(control) = &control {
                control.poll(|command| self.command(command));
            }
            self.import_disc(window, options);
            let was_visible = self.view.panel_visible();
            let mut event = SDL_Event::default();
            // SAFETY: SDL is initialised; event is a valid out pointer.
            while unsafe { SDL_PollEvent(&mut event) } {
                // SAFETY: the union is read according to its type tag.
                match SDL_EventType(unsafe { event.r#type }) {
                    SDL_EVENT_QUIT | SDL_EVENT_WINDOW_CLOSE_REQUESTED => return Ok(()),
                    SDL_EVENT_WINDOW_MINIMIZED => {
                        minimized = true;
                        if let Some(game) = &mut self.game { game.pause(); }
                    },
                    SDL_EVENT_WINDOW_RESTORED => {
                        minimized = false;
                        if let Some(game) = &mut self.game { game.resume(); }
                    },
                    SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
                    | SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED
                    | SDL_EVENT_WINDOW_SAFE_AREA_CHANGED => self.resize(window),
                    // SAFETY: event comes from SDL_PollEvent.
                    _ => unsafe { self.view.handle(window, &event) },
                }
            }
            if self.view.quit_requested() {
                return Ok(());
            }
            let visible = self.view.panel_visible();
            if visible != was_visible {
                window.set_text_input(visible);
            }
            self.follow_fullscreen(window);
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
            }
            if last && frames > 0 {
                return Ok(());
            }
            if let (Some(game), Some((at, path))) = (&mut self.game, &options.save_state_at)
                && !state_saved
                && game.frames_run() >= *at
            {
                println!("xem-desktop: {}", xem_host::save_state_at(game, path)?);
                state_saved = true;
                game.set_frame_limit(options.exit_at_frame);
            }
            if let (Some(game), Some(at)) = (&mut self.game, options.exit_at_frame)
                && game.frames_run() >= at
            {
                println!("xem-desktop: frame {} memory_hash {}", game.frames_run(), game.memory_hash());
                return Ok(());
            }
        }
    }

    /// The panel's Import disc…: SDL's file dialog, then the chosen disc
    /// booted (when no game runs) or given to the game's disc changer.
    fn import_disc(&mut self, window: &sdl::Window, options: &Options) {
        if self.disc_asked.take() {
            let sender = Box::new(self.disc_chosen.0.clone());
            let filters = [SDL_DialogFileFilter { name: c"Disc images".as_ptr(), pattern: c"chd;cue;bin;iso".as_ptr() }];
            // SAFETY: SDL calls `chosen` once with the box, which it owns
            // until then; the filters are static strings.
            unsafe {
                SDL_ShowOpenFileDialog(Some(chosen), Box::into_raw(sender).cast(), window.raw, filters.as_ptr(), filters.len() as i32, std::ptr::null(), false);
            }
        }
        let Ok(Some(path)) = self.disc_chosen.1.try_recv() else { return };
        // The panel shows the outcome.
        let _ = self.play_disc(&PathBuf::from(path), options);
    }

    /// A disc chosen to play: booted when no game runs, else given to the
    /// game's disc changer. The outcome shows on the panel.
    fn play_disc(&mut self, path: &std::path::Path, options: &Options) -> Result<String, String> {
        let result = match &mut self.game {
            None => open_game(options, path).map(|game| {
                self.game = Some(game);
                self.view.set_panel_visible(false);
                format!("Playing {}", path.display())
            }),
            Some(game) => (|| {
                let mut source = xem_disc::open_path(path).map_err(|e| e.to_string())?;
                let known = xem_disc::identify(source.as_mut()).map_err(|e| e.to_string())?.disc.ok_or("not a known Xenogears disc")?;
                xem_disc::verify(source.as_mut(), known).map_err(|e| e.to_string())?;
                game.add_disc(known.number(), source);
                Ok(format!("Disc {} is in the changer for the game's disc changes.", known.number()))
            })(),
        };
        match &result {
            Ok(text) => {
                self.view.ui().set_disc_status(text);
                self.view.ui().clear_operation_error();
            }
            Err(error) => self.view.ui().set_operation_error(&format!("{}: {error}", path.display())),
        }
        result
    }

    /// One agent command (`--control`): settings through the settings
    /// service (as the panel's edits), the panel shown or hidden, the rest
    /// to the game's command layer.
    fn command(&mut self, command: &serde_json::Value) -> serde_json::Value {
        if let Some(reply) = self.view.settings().borrow_mut().command(command) {
            return reply;
        }
        match command.get("cmd").and_then(serde_json::Value::as_str) {
            Some("import_disc") => {
                // As the panel's Import disc… does once its dialog chose a file.
                let Some(path) = command.get("path").and_then(serde_json::Value::as_str) else {
                    return serde_json::json!({ "error": "import_disc needs path" });
                };
                let options = self.options.clone();
                match self.play_disc(std::path::Path::new(path), &options) {
                    Ok(text) => serde_json::json!({ "status": text }),
                    Err(error) => serde_json::json!({ "error": error }),
                }
            }
            Some("bios_import") => {
                // As the panel's Import BIOS does once its dialog chose a
                // file: kept beside the settings for every boot and given to
                // the running game at once.
                let Some(path) = command.get("path").and_then(serde_json::Value::as_str) else {
                    return serde_json::json!({ "error": "bios_import needs path" });
                };
                match self.view.import_bios(self.game.as_mut(), path) {
                    Ok(message) => serde_json::json!({ "ok": true, "message": message }),
                    Err(error) => serde_json::json!({ "error": error }),
                }
            }
            Some("panel") => {
                if let Some(visible) = command.get("visible").and_then(serde_json::Value::as_bool) {
                    self.view.set_panel_visible(visible);
                }
                serde_json::json!({ "visible": self.view.panel_visible() })
            }
            // The panel's and the bindings' operations, as theirs run (a
            // held rewind, Retry pending card writes, ...).
            Some("app") => xem_host::app_reply(xem_host::agent_request(command).and_then(|request| self.view.app_command(self.game.as_mut(), request))),
            Some("first_person") => {
                // As the app's Toggle first-person view, or on, off or
                // turned: the next frames give the session its state (a
                // route under way stops).
                let field = self.game.as_mut().map(|game| game.pad_context().field).unwrap_or_default();
                xem_settings::look::Command::parse(command)
                    .and_then(|command| self.view.first_person_command(&command, field))
                    .unwrap_or_else(|error| serde_json::json!({ "error": error }))
            }
            _ => match &mut self.game {
                Some(game) => {
                    let mut reply = game.command(command).unwrap_or_else(|error| serde_json::json!({ "error": error }));
                    // `status` also tells what the window showed last, as a
                    // browser's does: `movie`, a movie frame rebuilt from what
                    // the MDEC decoded (the Enhanced movies setting).
                    if command.get("cmd").and_then(serde_json::Value::as_str) == Some("status")
                        && let Some(status) = reply.as_object_mut()
                    {
                        status.insert("renderer".into(), serde_json::json!({ "movie": self.view.presented_movie() }));
                    }
                    reply
                }
                None => serde_json::json!({ "error": "no game: play a disc (--disc)" }),
            },
        }
    }

    /// Asks the window for the Fullscreen setting when it changes, so a
    /// window manager's own fullscreen is not undone.
    fn follow_fullscreen(&mut self, window: &sdl::Window) {
        let fullscreen = self.view.settings().borrow().settings().presentation.fullscreen;
        if fullscreen != self.fullscreen {
            window.set_fullscreen(fullscreen);
            self.fullscreen = fullscreen;
        }
    }

    fn resize(&mut self, window: &sdl::Window) {
        if self.presenter.resize(&self.gpu, window) {
            self.view
                .resize(self.presenter.size(), window.display_scale());
        }
        self.view.set_safe_area(window.safe_area());
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
        let began = Instant::now();
        let mut ran = 0;
        let mut marks = [began; 6];
        if self.view.update_app(window,&mut self.game) && let Some(game) = &mut self.game {
            marks[0] = Instant::now();
            let settings = self.view.settings().borrow().settings().clone();
            self.view.assist(game);
            let (context, targets) = (game.pad_context(), game.targets());
            let buttons = self.view.pad_buttons(context, targets);
            game.want_attack_page(self.view.attack_pointed());
            game.want_inspection(self.view.first_person());
            match game.update(buttons, &settings, || window.focused()) {
                Ok(frames) => {
                    ran = frames;
                    self.view.frames_ran(frames);
                    game.follow_sound_mode(self.view.settings());
                }
                Err(error) => {
                    self.view.ui().set_operation_error(&format!("Game stopped: {error}. Load a state to recover."));
                    self.view.set_panel_visible(true);
                    game.pause();
                }
            }
            marks[1] = Instant::now();
            self.view.set_game_frame(&self.gpu, game);
        }
        marks[2] = Instant::now();
        if !self.view.wants_present() {
            // Game-paced presentation: nothing new to show yet.
            std::thread::sleep(std::time::Duration::from_millis(1));
            return Ok(false);
        }
        let Some(frame) = self.presenter.acquire(&self.gpu, window)? else {
            return Ok(false);
        };
        marks[3] = Instant::now();
        let output = frame.texture.create_view(&Default::default());
        self.view
            .render(&self.gpu, &output, self.start.elapsed().as_secs_f32())?;
        if let Some(path) = screenshot {
            let pixels = read_rgba8(&self.gpu.device, &self.gpu.queue, &frame.texture);
            save_png(path, self.presenter.size(), &pixels)?;
            eprintln!("xem-desktop: wrote {}", path.display());
        }
        marks[4] = Instant::now();
        self.gpu.queue.present(frame);
        marks[5] = Instant::now();
        if let Some(stats) = &mut self.stats {
            let mut from = began;
            for (stage, &to) in marks.iter().enumerate() {
                stats.add(stage, from, to.max(from));
                from = to.max(from);
            }
            stats.end_iteration(began, ran);
        }
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

/// SDL's file dialog answered: the chosen path (or none) to the main loop.
unsafe extern "C" fn chosen(userdata: *mut std::ffi::c_void, files: *const *const std::ffi::c_char, _filter: i32) {
    // SAFETY: the box was handed to this callback once; SDL's strings stay
    // valid until it returns and are copied first.
    let sender = unsafe { Box::from_raw(userdata.cast::<std::sync::mpsc::Sender<Option<String>>>()) };
    let path = if files.is_null() || unsafe { *files }.is_null() {
        None
    } else {
        unsafe { std::ffi::CStr::from_ptr(*files) }.to_str().ok().map(str::to_owned)
    };
    let _ = sender.send(path);
}
