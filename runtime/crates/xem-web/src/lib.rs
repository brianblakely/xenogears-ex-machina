//! The browser host (docs/runtime.md, Browser host).
//!
//! The recovered C runs as `game.wasm`, instantiated by the page beside this
//! module ([`game`]); the shared command layer (`xem_core::control::Session`,
//! the one xem-headless serves) drives it.
//! The page's animation callback calls [`XemApp::frame`], which advances the
//! game by the frames of the virtual clock that are due, in a bounded batch,
//! and renders the canvas ([`render`]); Web Audio pulls
//! samples with [`XemApp::audio_render`]; the user's disc is read in place
//! through [`disc`]. Every operation the page's controls use is a method here,
//! and the automation API (`window.xem`, runtime/web/xem.js) calls the same
//! methods: there is no DOM-input path to the game.
//!
//! Methods take and return JSON text where the value is structured;
//! [`XemApp::command`] takes the control layer's JSON commands as they are.
#![cfg(target_arch = "wasm32")]

mod audio;
mod disc;
mod game;
mod render;
mod storage;

use std::cell::{Cell, RefCell};
use std::rc::Rc;

use serde_json::{Value, json};
use wasm_bindgen::prelude::*;
use xem_core::control::Session;
use xem_core::inspect::Schema;
use xem_core::{Runtime, exe};
use xem_settings::{MemoryStore, Rejected, SettingChange, SettingsService};

pub use game::{WebModule, xem_game_import};
pub use render::WebRenderer;

/// One game frame (a vertical blank of the virtual clock) per 1/60 s of page time…
const FRAME_MS: f64 = 1000.0 / 60.0;
/// …at most this many per animation frame; time beyond that is dropped…
const MAX_FRAMES_PER_ANIMATION: u64 = 2;
/// …and no further frame once a batch has taken this long.
const BATCH_BUDGET_MS: f64 = 12.0;
/// The bound on one `frames` or `run_until` command, which runs inside one
/// call; the caller repeats it to go further.
pub const MAX_FRAMES_PER_COMMAND: u64 = 10_000;

const SETTINGS_KEY: &str = "xem.settings";

fn now_ms() -> f64 {
    js_sys::Date::now()
}

fn to_json(value: &impl serde::Serialize) -> String {
    serde_json::to_string(value).expect("serializes")
}

fn error(message: impl std::fmt::Display) -> JsValue {
    js_sys::Error::new(&message.to_string()).into()
}

#[derive(Default)]
struct LoopStats {
    animation_frames: u64,
    game_frames: u64,
    last_batch: u64,
    max_batch: u64,
    /// Game frames the page's time called for but the loop dropped (a
    /// throttled, hidden or frozen page, or a batch over budget).
    dropped: u64,
}

/// The browser application: settings, the disc, the game session, the
/// renderer and the audio source.
#[wasm_bindgen]
pub struct XemApp {
    settings: Rc<RefCell<SettingsService>>,
    settings_error: Option<String>,
    stub_names: Vec<String>,
    schema: Option<String>,
    session: Option<Session<WebModule>>,
    memory_bytes: u64,
    disc: Option<disc::DiscImport>,
    renderer: Option<WebRenderer>,
    panel_closed: Rc<Cell<bool>>,
    tone: audio::Tone,
    running: bool,
    /// Page time up to which game frames have been run or dropped.
    paced: Option<f64>,
    stats: LoopStats,
}

#[wasm_bindgen]
impl XemApp {
    #[wasm_bindgen(constructor)]
    pub fn new() -> XemApp {
        // A panic aborts the module: say why on the console first.
        std::panic::set_hook(Box::new(|info| web_sys::console::error_1(&format!("xem-web panicked: {info}").into())));
        let (service, settings_error) =
            match SettingsService::open(storage::LocalStorageStore { key: SETTINGS_KEY }) {
                Ok(service) => (service, None),
                Err(error) => (
                    SettingsService::with_defaults(storage::LocalStorageStore { key: SETTINGS_KEY }),
                    Some(error.to_string()),
                ),
            };
        // Without localStorage (blocked storage) settings still work for the page's life.
        let service = if web_sys::window().and_then(|w| w.local_storage().ok().flatten()).is_some() {
            service
        } else {
            SettingsService::with_defaults(MemoryStore::default())
        };
        XemApp {
            settings: Rc::new(RefCell::new(service)),
            settings_error,
            stub_names: Vec::new(),
            schema: None,
            session: None,
            memory_bytes: 0,
            disc: None,
            renderer: None,
            panel_closed: Rc::new(Cell::new(false)),
            tone: audio::Tone::new(),
            running: false,
            paced: None,
            stats: LoopStats::default(),
        }
    }

    // --- Presentation -------------------------------------------------------

    /// Take the renderer [`create_renderer`] made and put the Slint panel on it.
    pub fn attach_renderer(&mut self, renderer: Renderer) {
        let mut renderer = renderer.0;
        let closed = self.panel_closed.clone();
        renderer.attach_panel(self.settings.clone(), move || closed.set(true));
        self.renderer = Some(renderer);
    }

    /// Why the renderer's device was lost, if it was (the page then attaches a new one).
    pub fn renderer_lost(&self) -> Option<String> {
        self.renderer.as_ref().and_then(|r| r.lost())
    }

    pub fn resize(&mut self, width: u32, height: u32, scale: f32) {
        if let Some(renderer) = &mut self.renderer {
            renderer.resize(width, height, scale);
        }
    }

    /// One animation frame at page time `now` (ms): the game frames due since
    /// the last one, at most [`MAX_FRAMES_PER_ANIMATION`] and within the batch
    /// budget, then the canvas. Time a throttled or frozen page missed is
    /// dropped, not caught up.
    pub fn frame(&mut self, now: f64) -> Result<(), JsValue> {
        self.stats.animation_frames += 1;
        if self.running {
            let paced = *self.paced.get_or_insert(now - FRAME_MS);
            let due = ((now - paced) / FRAME_MS).floor().max(0.0) as u64;
            let start = now_ms();
            let mut batch = 0;
            while batch < due.min(MAX_FRAMES_PER_ANIMATION) && (batch == 0 || now_ms() - start < BATCH_BUDGET_MS) {
                let Some(session) = self.session.as_mut() else { break };
                batch += 1;
                if session.frame().is_err() {
                    break;
                }
            }
            if batch < due {
                self.stats.dropped += due - batch;
                self.paced = Some(now);
            } else {
                self.paced = Some(paced + due as f64 * FRAME_MS);
            }
            if self.session.as_ref().is_none_or(|s| s.stopped.is_some()) {
                self.running = false;
            }
            self.stats.game_frames += batch;
            self.stats.last_batch = batch;
            self.stats.max_batch = self.stats.max_batch.max(batch);
        }
        if self.panel_closed.replace(false) {
            self.set_panel_visible(false);
        }
        if let Some(renderer) = &mut self.renderer {
            renderer.frame((now / 1000.0) as f32, &self.settings.borrow()).map_err(error)?;
        }
        Ok(())
    }

    pub fn set_panel_visible(&mut self, visible: bool) {
        if let Some(renderer) = &mut self.renderer {
            renderer.panel_visible = visible && renderer.has_panel();
        }
    }

    pub fn panel_visible(&self) -> bool {
        self.renderer.as_ref().is_some_and(|r| r.panel_visible)
    }

    /// Host pointer input over the canvas (canvas pixels); true when the panel took it.
    pub fn pointer(&mut self, kind: &str, x: f32, y: f32, button: i32, dx: f32, dy: f32) -> bool {
        self.renderer.as_mut().is_some_and(|r| r.pointer(kind, x, y, button, (dx, dy)))
    }

    /// Host key input for the panel, as Slint key text.
    pub fn key(&mut self, text: &str, down: bool) {
        if let Some(renderer) = &mut self.renderer {
            renderer.key(text, down);
        }
    }

    /// `frames` interleaved stereo frames for Web Audio at `rate` Hz.
    pub fn audio_render(&mut self, frames: u32, rate: f32) -> Vec<f32> {
        self.tone.render(frames as usize, rate, self.settings.borrow().settings())
    }

    // --- Settings -----------------------------------------------------------

    /// `{settings, revision, error}`.
    pub fn settings(&self) -> String {
        let service = self.settings.borrow();
        to_json(&json!({
            "settings": service.settings(),
            "revision": service.revision(),
            "error": self.settings_error,
        }))
    }

    /// Apply a `SettingChange` (JSON, e.g. `{"MasterVolume": 40}`) through the
    /// settings service: `{ok, revision, settings}` or `{ok: false, rejected, reason}`.
    pub fn apply_setting(&mut self, change: &str) -> Result<String, JsValue> {
        let change: SettingChange = serde_json::from_str(change).map_err(|e| error(format!("bad change: {e}")))?;
        let result = self.settings.borrow_mut().apply(change);
        Ok(to_json(&match result {
            Ok(ack) => json!({ "ok": true, "revision": ack.revision, "settings": ack.settings }),
            Err(Rejected::Invalid { reason, .. }) => json!({ "ok": false, "rejected": "invalid", "reason": reason }),
            Err(Rejected::Storage { reason, .. }) => json!({ "ok": false, "rejected": "storage", "reason": reason }),
        }))
    }

    // --- Disc import --------------------------------------------------------

    /// Start importing the user's image of `bytes` bytes (read in place, never uploaded).
    pub fn open_disc(&mut self, name: String, bytes: f64) {
        self.disc = Some(disc::DiscImport::new(name, bytes as u64));
    }

    /// Progress the import: `[[start, end], ...]` to fetch, empty when done.
    pub fn disc_poll(&mut self) -> String {
        let ranges = self.disc.as_mut().map(|d| d.poll()).unwrap_or_default();
        to_json(&ranges.iter().map(|r| [r.start, r.end]).collect::<Vec<_>>())
    }

    pub fn disc_fill(&mut self, offset: f64, data: &[u8]) -> Result<(), JsValue> {
        self.disc.as_mut().ok_or_else(|| error("no disc is open"))?.fill(offset as u64, data).map_err(error)
    }

    /// Raw sectors of the identified disc (`count` <= 64), or undefined while
    /// their bytes are fetched: fetch [`disc_missing`](Self::disc_missing), fill, retry.
    pub fn disc_read(&mut self, lba: u32, count: u32) -> Result<Option<Vec<u8>>, JsValue> {
        self.disc.as_mut().ok_or_else(|| error("no disc is open"))?.read_sectors(lba, count).map_err(error)
    }

    /// The byte ranges `[[start, end], ...]` a pending read waits for.
    pub fn disc_missing(&mut self) -> String {
        let ranges = self.disc.as_mut().map(|d| d.take_missing()).unwrap_or_default();
        to_json(&ranges.iter().map(|r| [r.start, r.end]).collect::<Vec<_>>())
    }

    pub fn disc_status(&self) -> String {
        to_json(&self.disc.as_ref().map(|d| d.status()))
    }

    // --- Game session -------------------------------------------------------

    /// The trapping stubs' names (build/game/stubs.txt), for readable stops.
    pub fn set_stub_names(&mut self, text: &str) {
        self.stub_names = text.lines().filter_map(|l| l.split_once(' ').map(|(_, n)| n.to_string())).collect();
    }

    /// The game's globals and types (build/game/schema.json) for `inspect`,
    /// `write` and path conditions.
    pub fn set_schema(&mut self, text: String) {
        self.schema = Some(text);
    }

    /// Start a session on a fresh instance of game.wasm whose "xem" imports,
    /// in `import_names` order, forward to [`xem_game_import`]. Loads the
    /// identified known disc's executable as the BIOS does (as xem-headless
    /// does), unless `load_executable` is false (module test fixtures).
    pub fn boot(
        &mut self,
        instance: js_sys::WebAssembly::Instance,
        import_names: Vec<String>,
        load_executable: bool,
    ) -> Result<String, JsValue> {
        let executable = if load_executable {
            let identification =
                self.disc.as_ref().and_then(|d| d.identification()).ok_or_else(|| error("no identified disc"))?;
            identification.disc.ok_or_else(|| error("not a known Xenogears disc"))?;
            Some(identification.executable.clone())
        } else {
            None
        };
        let schema = self.schema.as_deref().map(Schema::parse).transpose().map_err(|e| error(format!("schema: {e}")))?;
        self.session = None;
        self.running = false;
        let module = WebModule::new(&instance, import_names).map_err(error)?;
        self.memory_bytes = module.memory_bytes();
        let mut runtime = Runtime::new(module);
        runtime.services().stub_names = self.stub_names.clone();
        if let Some(executable) = executable {
            // The BIOS copies the executable's text to its address and jumps to pc0.
            let header = exe::parse(&executable).map_err(error)?;
            runtime.memory().write(header.text_address, exe::text(&executable, &header)).map_err(error)?;
        }
        self.session = Some(Session::new(runtime, schema));
        Ok(self.status())
    }

    /// Let the animation loop advance the game (in bounded batches) or stop it.
    pub fn set_running(&mut self, running: bool) {
        self.running = running && self.session.as_ref().is_some_and(|s| s.stopped.is_none());
        self.paced = None;
    }

    /// One command of the shared control layer (`xem_core::control`, as
    /// `xem-headless --control` reads them), JSON in and out: `status`,
    /// `frames`, `run_until`, `pad`, `press`, `inspect`, `write`, `snapshot`,
    /// `restore`, `memory_hash`, ... A command that runs frames runs at most
    /// [`MAX_FRAMES_PER_COMMAND`]; errors come back as `{"error": ...}`.
    pub fn command(&mut self, command: &str) -> String {
        let reply = match serde_json::from_str::<Value>(command) {
            Err(e) => json!({ "error": format!("bad json: {e}") }),
            Ok(command) => {
                let frames = match command.get("cmd").and_then(Value::as_str) {
                    Some("frames") => command.get("count").and_then(Value::as_u64).unwrap_or(1),
                    Some("run_until") => command.get("max_frames").and_then(Value::as_u64).unwrap_or(0),
                    _ => 0,
                };
                match self.session.as_mut() {
                    None => json!({ "error": "no game session" }),
                    Some(_) if frames > MAX_FRAMES_PER_COMMAND => {
                        json!({ "error": format!("a command runs at most {MAX_FRAMES_PER_COMMAND} frames") })
                    }
                    Some(session) => session.execute(&command),
                }
            }
        };
        if self.session.as_ref().is_some_and(|s| s.stopped.is_some()) {
            self.running = false;
        }
        to_json(&reply)
    }

    /// The runtime between frames as snapshot bytes (`Runtime::snapshot`), for saves.
    pub fn snapshot_bytes(&mut self) -> Result<Vec<u8>, JsValue> {
        let session = self.session.as_mut().ok_or_else(|| error("no game session"))?;
        session.runtime.snapshot().map_err(error)
    }

    /// Return to snapshot bytes of the same game module.
    pub fn restore_bytes(&mut self, bytes: &[u8]) -> Result<(), JsValue> {
        let session = self.session.as_mut().ok_or_else(|| error("no game session"))?;
        session.restore_bytes(bytes).map_err(error)
    }

    /// The save placeholder: a formatted empty memory card.
    pub fn blank_memory_card(&self) -> Vec<u8> {
        storage::blank_memory_card()
    }

    /// Everything a client can inspect.
    pub fn status(&mut self) -> String {
        let session = self.session.as_mut().map(|s| s.execute(&json!({ "cmd": "status" })));
        let renderer = self.renderer.as_ref().map(|r| {
            json!({
                "backend": r.backend,
                "adapter": r.adapter,
                "size": r.size(),
                "frames": r.frames,
                "panel": if r.has_panel() { "slint" } else { "none" },
                "panelError": r.panel_error,
                "panelVisible": r.panel_visible,
                "lost": r.lost(),
            })
        });
        let settings_revision = self.settings.borrow().revision();
        to_json(&json!({
            "renderer": renderer,
            "disc": self.disc.as_ref().map(|d| d.status()),
            "session": session,
            "gameMemoryBytes": self.memory_bytes,
            "running": self.running,
            "loop": {
                "animationFrames": self.stats.animation_frames,
                "gameFrames": self.stats.game_frames,
                "lastBatch": self.stats.last_batch,
                "maxBatch": self.stats.max_batch,
                "dropped": self.stats.dropped,
                "maxFramesPerAnimation": MAX_FRAMES_PER_ANIMATION,
                "batchBudgetMs": BATCH_BUDGET_MS,
            },
            "audioFrames": self.tone.frames,
            "settingsRevision": settings_revision,
        }))
    }
}

impl Default for XemApp {
    fn default() -> Self {
        Self::new()
    }
}

/// Create the canvas renderer: WebGPU unless unavailable or `prefer` is "webgl2".
#[wasm_bindgen]
pub async fn create_renderer(canvas: web_sys::HtmlCanvasElement, prefer: String) -> Result<Renderer, JsValue> {
    render::create(canvas, &prefer).await.map(Renderer).map_err(error)
}

/// A renderer on its way to [`XemApp::attach_renderer`].
#[wasm_bindgen]
pub struct Renderer(WebRenderer);

/// The current settings' JSON shape, for documentation and clients.
#[wasm_bindgen]
pub fn default_settings() -> String {
    to_json(&xem_settings::Settings::default())
}

