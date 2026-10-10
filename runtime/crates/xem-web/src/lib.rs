//! The browser host (docs/runtime.md, Browser host).
//!
//! The recovered C runs as `game.wasm`, instantiated by the page beside this
//! module ([`game`]); the shared command layer (`xem_core::Session`) drives it.
//! The page's animation callback calls [`XemApp::frame`], which advances the
//! game in a bounded batch and renders the canvas ([`render`]); Web Audio pulls
//! samples with [`XemApp::audio_render`]; the user's disc is read in place
//! through [`disc`]. Every operation the page's controls use is a method here,
//! and the automation API (`window.xem`, runtime/web/xem.js) calls the same
//! methods: there is no DOM-input path to the game.
//!
//! Methods take and return JSON text where the value is structured.
#![cfg(target_arch = "wasm32")]

mod audio;
mod disc;
mod game;
mod render;
mod storage;

use std::cell::{Cell, RefCell};
use std::rc::Rc;

use serde::Deserialize;
use serde_json::json;
use wasm_bindgen::prelude::*;
use xem_core::session::{Condition, Outcome, Session, Snapshot, StepLog, parse_stub_names};
use xem_settings::{MemoryStore, Rejected, SettingChange, SettingsService};

pub use game::{WebModule, xem_game_import};
pub use render::WebRenderer;

/// At most this many steps per animation frame…
const MAX_STEPS_PER_FRAME: u64 = 16;
/// …and no more after this much wall time in the frame's batch.
const FRAME_BUDGET_MS: f64 = 8.0;
/// The bound on one `run_until`; the caller repeats it to go further.
pub const MAX_RUN_UNTIL_STEPS: u64 = 100_000;

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
    frames: u64,
    loop_steps: u64,
    last_batch: u64,
    max_batch: u64,
}

/// The browser application: settings, the disc, the game session, the
/// renderer and the audio source.
#[wasm_bindgen]
pub struct XemApp {
    settings: Rc<RefCell<SettingsService>>,
    settings_error: Option<String>,
    stub_names: Vec<String>,
    session: Option<Session<WebModule>>,
    memory_bytes: u64,
    log: StepLog,
    disc: Option<disc::DiscImport>,
    renderer: Option<WebRenderer>,
    panel_closed: Rc<Cell<bool>>,
    tone: audio::Tone,
    running: bool,
    stats: LoopStats,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct RunUntil {
    max_steps: u64,
    condition: Condition,
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
            session: None,
            memory_bytes: 0,
            log: StepLog::default(),
            disc: None,
            renderer: None,
            panel_closed: Rc::new(Cell::new(false)),
            tone: audio::Tone::new(),
            running: false,
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

    /// One animation frame: a bounded batch of game steps, then the canvas.
    /// A throttled or hidden page simply gets fewer frames; nothing catches up.
    pub fn frame(&mut self, now: f64) -> Result<(), JsValue> {
        self.stats.frames += 1;
        if self.running {
            let start = now_ms();
            let mut batch = 0;
            while batch < MAX_STEPS_PER_FRAME && now_ms() - start < FRAME_BUDGET_MS {
                let Some(outcome) = self.step_once() else { break };
                batch += 1;
                if matches!(&outcome, Outcome::Yield { reason } if reason == "VSync") {
                    break;
                }
            }
            if self.session.as_ref().is_some_and(|s| s.is_halted()) {
                self.running = false;
            }
            self.stats.loop_steps += batch;
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
        self.stub_names = parse_stub_names(text);
    }

    /// Start a session on a fresh instance of game.wasm whose "xem" imports,
    /// in `import_names` order, forward to [`xem_game_import`]. Loads the
    /// identified known disc's executable as the BIOS does, unless
    /// `load_executable` is false (module test fixtures without a disc).
    pub fn boot(
        &mut self,
        instance: js_sys::WebAssembly::Instance,
        import_names: Vec<String>,
        load_executable: bool,
    ) -> Result<String, JsValue> {
        let executable = if load_executable {
            let identification = self.disc.as_ref().and_then(|d| d.identification()).ok_or_else(|| error("no identified disc"))?;
            identification.disc.ok_or_else(|| error("not a known Xenogears disc"))?;
            Some(identification.executable.clone())
        } else {
            None
        };
        self.session = None;
        self.running = false;
        self.log = StepLog::default();
        let module = WebModule::new(&instance, import_names).map_err(error)?;
        self.memory_bytes = module.memory_bytes();
        let mut session = Session::new(module, self.stub_names.clone());
        if let Some(executable) = executable {
            session.load_executable(&executable).map_err(error)?;
        }
        self.session = Some(session);
        Ok(self.status())
    }

    /// Let the animation loop advance the game (bounded per frame) or stop it.
    pub fn set_running(&mut self, running: bool) {
        self.running = running && self.session.as_ref().is_some_and(|s| !s.is_halted());
    }

    /// Run up to `count` steps now: a `Report`.
    pub fn step(&mut self, count: u32) -> Result<String, JsValue> {
        let session = self.session.as_mut().ok_or_else(|| error("no game session"))?;
        let log = &mut self.log;
        Ok(to_json(&session.step_n(u64::from(count), |index, outcome| {
            log.record(index, outcome);
        })))
    }

    /// `{maxSteps, condition}` with a `Condition` (`{"until": "halt"}`,
    /// `{"until": "yield", "reason": "VSync"}`, `{"until": "restart"}`,
    /// `{"until": "word", "address": ..., "value": ...}`): a `Report`.
    pub fn run_until(&mut self, request: &str) -> Result<String, JsValue> {
        let request: RunUntil = serde_json::from_str(request).map_err(|e| error(format!("bad request: {e}")))?;
        if request.max_steps > MAX_RUN_UNTIL_STEPS {
            return Err(error(format!("maxSteps is at most {MAX_RUN_UNTIL_STEPS} per call")));
        }
        let session = self.session.as_mut().ok_or_else(|| error("no game session"))?;
        let log = &mut self.log;
        let report = session.run_until(&request.condition, request.max_steps, |index, outcome| {
            log.record(index, outcome);
        });
        Ok(to_json(&report))
    }

    /// The session as a snapshot file (`xem_core::Snapshot::to_bytes`).
    pub fn snapshot(&mut self) -> Result<Vec<u8>, JsValue> {
        let session = self.session.as_mut().ok_or_else(|| error("no game session"))?;
        Ok(session.snapshot().map_err(error)?.to_bytes())
    }

    pub fn restore(&mut self, bytes: &[u8]) -> Result<String, JsValue> {
        let snapshot = Snapshot::from_bytes(bytes).map_err(error)?;
        let session = self.session.as_mut().ok_or_else(|| error("no game session"))?;
        session.restore(&snapshot).map_err(error)?;
        self.log.lines.push(format!("restored the snapshot of step {}", snapshot.steps));
        Ok(self.status())
    }

    /// FNV-1a of game RAM and the scratchpad, as xem-headless prints it.
    pub fn digest(&mut self) -> Result<String, JsValue> {
        let session = self.session.as_mut().ok_or_else(|| error("no game session"))?;
        Ok(format!("{:016x}", session.digest().map_err(error)?))
    }

    /// The save placeholder: a formatted empty memory card.
    pub fn blank_memory_card(&self) -> Vec<u8> {
        storage::blank_memory_card()
    }

    /// Everything a client can inspect.
    pub fn status(&mut self) -> String {
        let session = self.session.as_mut().map(|s| s.status());
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
            "bootLines": self.log.lines,
            "running": self.running,
            "loop": {
                "frames": self.stats.frames,
                "steps": self.stats.loop_steps,
                "lastBatch": self.stats.last_batch,
                "maxBatch": self.stats.max_batch,
                "maxStepsPerFrame": MAX_STEPS_PER_FRAME,
                "frameBudgetMs": FRAME_BUDGET_MS,
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

impl XemApp {
    fn step_once(&mut self) -> Option<Outcome> {
        let session = self.session.as_mut()?;
        let index = session.status().steps;
        let outcome = session.step()?;
        self.log.record(index, &outcome);
        Some(outcome)
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

