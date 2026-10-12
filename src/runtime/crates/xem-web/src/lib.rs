//! The browser host (docs/runtime.md, Browser host).
//!
//! The recovered C runs as `game.wasm`, instantiated by the page beside this
//! module ([`game`]); the shared command layer (`xem_core::control::Session`,
//! the one xem-headless serves) drives it.
//! The page's animation callback calls [`XemApp::frame`], which applies the
//! player's keyboard and gamepad to pad 1 ([`input`]), advances the game by the
//! frames of the virtual clock that are due, in a bounded batch, and renders
//! the GPU's display on the canvas ([`render`]); the page then posts the SPU's
//! samples ([`XemApp::take_audio`]) to its AudioWorklet and the memory cards'
//! changed images ([`XemApp::take_card_write`]) to IndexedDB. The user's disc
//! is read in place through [`disc`]: a frame whose sector has not arrived
//! stops without advancing the clock and continues once the page fetched it. Every operation the page's controls use is a method here,
//! and the automation API (`window.xem`, src/runtime/web/xem.js) calls the same
//! methods: there is no DOM-input path to the game.
//!
//! Methods take and return JSON text where the value is structured;
//! [`XemApp::command`] takes the control layer's JSON commands as they are.
#![cfg(target_arch = "wasm32")]

mod audio;
mod disc;
mod game;
mod input;
mod render;
mod storage;

use std::cell::{Cell, RefCell};
use std::rc::Rc;

use serde_json::{Value, json};
use wasm_bindgen::prelude::*;
use xem_core::control::Session;
use xem_core::inspect::Schema;
use xem_core::{Runtime, bios, exe};
use xem_settings::{AppInput, AppRequest, Rejected, SettingChange, SettingsService};

pub use game::{WebModule, xem_game_import};
pub use render::WebRenderer;

/// One game frame (a vertical blank of the virtual clock) per 1/60 s of page time…
const FRAME_MS: f64 = 1000.0 / 60.0;
/// …at most this many per animation frame (a page or headset session
/// presenting as slowly as 15 Hz still runs every game frame); time beyond
/// that is dropped…
const MAX_FRAMES_PER_ANIMATION: u64 = 4;
/// …and no further frame once a batch has taken this long.
const BATCH_BUDGET_MS: f64 = 12.0;
/// The bound on one `frames` or `run_until` command, which runs inside one
/// call; the caller repeats it to go further.
pub const MAX_FRAMES_PER_COMMAND: u64 = 10_000;

const SETTINGS_KEY: &str = "xem.settings";

fn now_ms() -> f64 {
    web_sys::window().expect("browser window").performance().expect("browser performance clock").now()
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
    /// Immersive frames (the session's animation frames).
    xr_frames: u64,
    game_frames: u64,
    last_batch: u64,
    max_batch: u64,
    /// Game frames the page's time called for but the loop dropped (a
    /// throttled, hidden or frozen page, or a batch over budget).
    dropped: u64,
}

/// The browser application: settings, the disc, the game session, the
/// renderer, the sound, the memory cards and the player's input.
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
    audio: audio::Mixer,
    audio_scratch: Vec<i16>,
    cards: storage::BrowserCards,
    input: input::HostInput,
    app_input: AppInput,
    rewind_held: [bool;2],
    rewind_next: f64,
    app_requests: Rc<RefCell<Vec<AppRequest>>>,
    /// The buttons the player's input last put on pad 1.
    host_buttons: u16,
    /// Frames ran since the display was last shown.
    display_dirty: bool,
    running: bool,
    /// Page time up to which game frames have been run or dropped.
    paced: Option<f64>,
    music_paced: Option<f64>,
    /// Fast-forward's pace: the rate the batches reach, and the last batch's time.
    pacer: xem_core::time::FastForwardPacer,
    batched: Option<f64>,
    /// The view the field's characters keep moving in (see
    /// `Session::set_actor_view`), set by the presentation.
    actor_view: u32,
    stats: LoopStats,
    applied_time_settings: Option<(xem_settings::FastForwardRate, bool, u32, u32)>,
    /// Wheel movement (notches) not yet turned into zoom steps.
    wheel: f32,
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
            audio: audio::Mixer::default(),
            audio_scratch: Vec::new(),
            cards: storage::BrowserCards::default(),
            input: input::HostInput::default(),
            app_input: AppInput::default(),
            rewind_held: [false;2],
            rewind_next: 0.0,
            app_requests: Rc::new(RefCell::new(Vec::new())),
            host_buttons: 0,
            display_dirty: false,
            running: false,
            paced: None,
            music_paced: None,
            pacer: Default::default(),
            batched: None,
            actor_view: 0,
            stats: LoopStats::default(),
            applied_time_settings: None,
            wheel: 0.0,
        }
    }

    // --- Presentation -------------------------------------------------------

    /// Take the renderer [`create_renderer`] made and put the Slint panel on it.
    pub fn attach_renderer(&mut self, renderer: Renderer) {
        let mut renderer = renderer.0;
        let closed = self.panel_closed.clone();
        renderer.attach_panel(self.settings.clone(), move || closed.set(true));
        if let Some(panel) = renderer.panel() {
            let requests = self.app_requests.clone();
            panel.on_app_request(move |request| requests.borrow_mut().push(request));
        }
        self.renderer = Some(renderer);
        self.display_dirty = true;
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

    /// One animation frame at page time `now` (ms): the player's input on pad
    /// 1, the game frames due since the last one, at most
    /// [`MAX_FRAMES_PER_ANIMATION`] and within the batch budget, then the
    /// canvas. Time a throttled or frozen page missed, or a frame spent
    /// waiting for a disc sector, is dropped, not caught up.
    pub fn frame(&mut self, now: f64) -> Result<(), JsValue> {
        self.stats.animation_frames += 1;
        self.step(now)?;
        let fresh = self.display_dirty || self.session.is_none();
        self.take_game_frame(false);
        // Game-paced presentation draws only when the game drew (or the
        // panel shows); the canvas keeps the last picture meanwhile.
        let game_paced = self.settings.borrow().settings().graphics.present_rate == xem_settings::PresentRate::Game;
        if game_paced && !fresh && !self.panel_visible() {
            return Ok(());
        }
        if let Some(renderer) = &mut self.renderer {
            renderer.frame(now / 1000.0, &self.settings.borrow()).map_err(error)?;
        }
        Ok(())
    }

    /// One immersive frame at page time `now` (ms), from the session's
    /// animation frame: the game frames due, as [`XemApp::frame`] runs them
    /// (never per view), then every view of `layer`. `matrices` holds each
    /// view's world-to-eye and projection (WebXR's, OpenGL clip space) as 32
    /// floats, column major; `viewports` its layer rectangle (x, y from the
    /// bottom, width, height).
    pub fn xr_frame(&mut self, now: f64, layer: web_sys::XrWebGlLayer, matrices: &[f32], viewports: &[i32]) -> Result<(), JsValue> {
        self.step(now)?;
        let eye_height = viewports.chunks_exact(4).map(|v| v[3]).max().unwrap_or(0).max(0) as u32;
        if let Some(renderer) = &mut self.renderer {
            renderer.set_xr_eye_height(eye_height);
        }
        self.take_game_frame(true);
        // OpenGL clip depth (-1..1) to wgpu's (0..1).
        let depth = xem_render::glam::Mat4::from_cols(
            xem_render::glam::Vec4::X,
            xem_render::glam::Vec4::Y,
            xem_render::glam::Vec4::new(0.0, 0.0, 0.5, 0.0),
            xem_render::glam::Vec4::new(0.0, 0.0, 0.5, 1.0),
        );
        let views: Vec<render::XrView> = matrices
            .chunks_exact(32)
            .zip(viewports.chunks_exact(4))
            .map(|(m, v)| render::XrView {
                view: xem_render::glam::Mat4::from_cols_slice(&m[..16]),
                proj: depth * xem_render::glam::Mat4::from_cols_slice(&m[16..]),
                viewport: [v[0], v[1], v[2], v[3]],
            })
            .collect();
        self.stats.xr_frames += 1;
        if let Some(renderer) = &mut self.renderer {
            renderer.render_xr(&layer, &views, now / 1000.0, &self.settings.borrow()).map_err(error)?;
        }
        Ok(())
    }

    /// RGBA bytes at `points` (x, y pairs, layer pixels from the bottom left)
    /// of the immersive layer just drawn; for tests.
    pub fn xr_readback(&self, layer: web_sys::XrWebGlLayer, points: &[i32]) -> Result<Vec<u8>, JsValue> {
        let points: Vec<[i32; 2]> = points.chunks_exact(2).map(|p| [p[0], p[1]]).collect();
        match &self.renderer {
            Some(renderer) => renderer.read_layer_pixels(&layer, &points).map_err(error),
            None => Err(error("no renderer")),
        }
    }

    /// The immersive session ended: the canvas shows the game again.
    pub fn xr_ended(&mut self) {
        if let Some(renderer) = &mut self.renderer {
            renderer.release_xr();
        }
        self.display_dirty = true;
    }

    /// Whether the renderer can draw immersive frames.
    pub fn xr_capable(&self) -> bool {
        self.renderer.as_ref().is_some_and(|r| r.xr_capable())
    }

    /// The game frames due since the last step (see [`XemApp::frame`]).
    fn step(&mut self, now: f64) -> Result<(), JsValue> {
        self.sync_time_controls()?;
        if self.running && self.rewind_held.iter().any(|held| *held) {
            if now >= self.rewind_next {
                if let Some(session) = self.session.as_mut() {
                    if session.rewind_step().map_err(error)? { self.display_dirty = true; }
                }
                self.rewind_next = now + xem_core::rewind::REWIND_STEP_MS as f64;
            }
            self.paced = Some(now);
            self.music_paced = Some(now);
            self.stats.last_batch = 0;
        } else if self.running {
            // The player's input reaches pad 1 when it changes, so a pad an
            // automation client set stays until the player presses something.
            let buttons = if self.panel_visible() { 0 } else { self.input.buttons() };
            if buttons != self.host_buttons
                && let Some(session) = self.session.as_mut()
            {
                session.set_pad(0, buttons);
                self.host_buttons = buttons;
            }
            let paced = *self.paced.get_or_insert(now - FRAME_MS);
            let factor = self.session.as_ref().map_or(Some(1), |s| s.frame_factor());
            let fast_forward = self.session.as_ref().is_some_and(|s| s.fast_forward);
            let elapsed_frames = ((now - paced) / FRAME_MS).floor().max(0.0) as u64;
            let music_paced = self.music_paced.replace(now).unwrap_or(now - FRAME_MS);
            let music_cycles = ((now - music_paced).clamp(0.0, 100.0) * xem_core::clock::CPU_HZ as f64 / 1000.0) as u64;
            let live_music = self.session.as_ref().is_some_and(|s| s.live_music());
            let due = factor.map(|factor| elapsed_frames.saturating_mul(u64::from(factor)));
            let limit = due.map(|due| due.min(MAX_FRAMES_PER_ANIMATION * u64::from(factor.unwrap())));
            let start = now_ms();
            let mut batch = 0;
            let mut fetching = false;
            let pause_on_focus_loss = self.settings.borrow().settings().pause_on_focus_loss;
            while limit.is_none_or(|frames| batch < frames) && (batch == 0 || now_ms() - start < BATCH_BUDGET_MS) {
                let Some(session) = self.session.as_mut() else { break };
                session.set_window_focus(self.app_input.focused(),pause_on_focus_loss);
                session.set_actor_view(self.actor_view).map_err(error)?;
                match session.frame() {
                    Ok(true) => {
                        batch += 1;
                    },
                    // A disc sector is still being fetched: no time passed;
                    // the next animation frame retries after the fetch.
                    Ok(false) => {
                        fetching = true;
                        break;
                    }
                    Err(_) => {
                        batch += 1;
                        break;
                    }
                }
            }
            self.paced = match due {
                Some(due) if batch >= due => Some(paced + elapsed_frames as f64 * FRAME_MS),
                Some(due) => { self.stats.dropped += due - batch; Some(now) },
                None => Some(paced + elapsed_frames as f64 * FRAME_MS),
            };
            if self.session.as_ref().is_none_or(|s| s.stopped.is_some()) {
                self.running = false;
            }
            if live_music && let Some(session) = self.session.as_mut() {
                session.advance_live_music(music_cycles).map_err(error)?;
            }
            // A fixed fast-forward rate the batches cannot reach runs at the
            // rate they do reach, so the music it times keeps up with the sound.
            let seconds = self.batched.replace(now).map_or(0.0, |at| (now - at).max(0.0) / 1000.0);
            if let Some(session) = self.session.as_mut() {
                match (session.fast_forward, session.fast_forward_rate.factor(), factor) {
                    (true, Some(rate), Some(pace)) if !session.replaying() => {
                        let behind = !fetching && limit.is_some_and(|frames| batch < frames);
                        if let Some(next) = self.pacer.note(seconds, batch as u32, behind, pace, rate) {
                            session.pace_fast_forward(Some(next)).map_err(error)?;
                        }
                    }
                    _ => self.pacer.reset(),
                }
            }
            self.stats.game_frames += batch;
            self.stats.last_batch = batch;
            self.stats.max_batch = self.stats.max_batch.max(batch);
            if batch > 0 || (fast_forward && music_cycles > 0) {
                self.ran_frames();
            }
        } else {
            self.stats.last_batch = 0;
        }
        if self.panel_closed.replace(false) {
            self.set_panel_visible(false);
        }
        Ok(())
    }

    /// Hands what the game drew to the renderer (configured for the canvas or
    /// a headset) and updates the panel's status.
    fn take_game_frame(&mut self, headset: bool) {
        if let Some(renderer) = &mut self.renderer {
            if let (Some(panel),Some(session)) = (renderer.panel(),self.session.as_ref()) {
                panel.set_time_status(Some(session.fast_forward), &format!("{:.0} of {} seconds available · {:.1} MiB used{}",
                    session.rewind.available_seconds(), session.rewind.history_seconds, session.rewind.bytes as f64/1048576.0,
                    session.rewind.error.as_ref().map(|e| format!(" — {e}")).unwrap_or_default()));
            } else if let Some(panel) = renderer.panel() {
                panel.set_time_status(None,"Boot a game to use save states and time controls.");
            }
            match self.session.as_mut() {
                Some(session) if self.display_dirty => {
                    let graphics = self.settings.borrow().settings().graphics;
                    if graphics.renderer == xem_settings::Renderer::Original {
                        // The console's own picture, as the device shows it.
                        self.actor_view = 0;
                        let gpu = &mut session.runtime.services().gpu;
                        if gpu.device().trace_enabled() {
                            gpu.device_mut().set_trace(false);
                        }
                        renderer.set_display_image(gpu.display_rgba8());
                        self.display_dirty = false;
                        return;
                    }
                    let restart = renderer.configure_game(&graphics, headset);
                    let reveals = renderer.reveals();
                    self.actor_view = if graphics.actors_in_view { renderer.actor_view() } else { 0 };
                    let scene = session.scene();
                    let gpu = &mut session.runtime.services().gpu;
                    let device = gpu.device_mut();
                    if restart || !device.trace_enabled() {
                        device.set_trace(true);
                    }
                    device.set_reveals(reveals);
                    let trace = device.take_trace();
                    let display = device.display();
                    renderer.set_game_frame(trace, display, scene, || gpu.display_rgba8());
                    self.display_dirty = false;
                }
                Some(_) => {}
                None => renderer.clear_game_image(),
            }
        }
    }

    /// After game frames ran: keep the SPU's samples for the page and show
    /// the new display at the next render.
    fn ran_frames(&mut self) {
        self.display_dirty = true;
        let Some(session) = self.session.as_mut() else { return };
        let spu = &mut session.runtime.services().spu;
        self.audio_scratch.resize(spu.audio_queued(), 0);
        let taken = spu.take_audio(&mut self.audio_scratch);
        self.audio.push(&self.audio_scratch[..taken]);
    }

    /// A key (`KeyboardEvent.code`) went down or up; true when it is one of
    /// pad 1's keys (the page then keeps it from the browser). Ignored while
    /// the settings panel has the input.
    pub fn game_key(&mut self, code: &str, down: bool) -> bool {
        if !self.app_input.focused() { return true; }
        let editing = self.text_editor_focused();
        if editing { self.app_input.release_keyboard(); } else { self.app_input.key(code, down); }
        let settings = self.settings.borrow();
        let bindings = &settings.settings().app_bindings;
        self.app_requests.borrow_mut().extend(self.app_input.requests(bindings,settings.settings().invert_trigger_chords));
        let captured = self.app_input.captures_key(code,bindings);
        if captured {
            for binding in bindings.values() { for key in &binding.keyboard { self.input.key(key,false); } }
        }
        drop(settings);
        if editing { return true; }
        if captured { return true; }
        if down && self.panel_visible() {
            return false;
        }
        self.input.key(code, down)
    }

    /// The first gamepad's buttons (Gamepad API standard mapping, 1 = pressed).
    pub fn gamepad(&mut self, pressed: &[u8]) {
        if self.text_editor_focused() { self.app_input.release_keyboard(); }
        self.app_input.pad(pressed);
        let settings = self.settings.borrow();
        let bindings = &settings.settings().app_bindings;
        self.app_requests.borrow_mut().extend(self.app_input.requests(bindings,settings.settings().invert_trigger_chords));
        self.input.gamepad(&self.app_input.gamepad_for_game(bindings,settings.settings().invert_trigger_chords));
    }

    /// Release the player's keys and buttons (the page lost focus).
    pub fn release_input(&mut self) {
        self.input.clear();
        let _ = self.app_input.set_focused(false);
        if let Some(panel) = self.renderer.as_ref().and_then(|r| r.panel()) { panel.cancel_pointer(); }
        self.app_requests.borrow_mut().retain(|r| matches!(r,AppRequest::Action(_)));
        let _ = self.set_rewind_held(false,false);
        let _ = self.set_rewind_held(false,true);
    }

    /// Resume input polling after the page regains focus.
    pub fn resume_input(&mut self) {
        let _ = self.app_input.set_focused(true);
    }

    pub fn set_rewind_held(&mut self, held: bool, from_panel: bool) -> Result<(), JsValue> {
        self.sync_time_controls()?;
        let session = self.session.as_mut().ok_or_else(|| error("no game session"))?;
        if held && session.rewind.budget == 0 { return Err(error("Enable rewind history in Settings first.")); }
        if held && let Some(reason) = &session.rewind.error { return Err(error(reason)); }
        let was_held = self.rewind_held.iter().any(|held| *held);
        self.rewind_held[usize::from(from_panel)] = held;
        let held = self.rewind_held.iter().any(|held| *held);
        if was_held != held {
            self.rewind_next = 0.0;
            session.timeline_revision += 1;
            session.reset_rewind_timer();
            self.audio = Default::default();
            self.paced = None;
            self.music_paced = None;
        }
        Ok(())
    }

    pub fn take_app_requests(&self) -> String { to_json(&std::mem::take(&mut *self.app_requests.borrow_mut())) }
    pub fn set_operation_status(&self, text: &str) {
        if let Some(panel) = self.renderer.as_ref().and_then(|r| r.panel()) { panel.set_operation_status(text); }
    }
    pub fn set_storage_status(&self, text: &str) {
        if let Some(panel) = self.renderer.as_ref().and_then(|r| r.panel()) { panel.set_storage_status(text); }
    }

    pub fn set_panel_visible(&mut self, visible: bool) {
        if let Some(renderer) = &mut self.renderer {
            if renderer.panel_visible && !visible && let Some(panel) = renderer.panel() { panel.cancel_pointer(); }
            renderer.panel_visible = visible && renderer.has_panel();
        }
    }

    pub fn panel_visible(&self) -> bool {
        self.renderer.as_ref().is_some_and(|r| r.panel_visible)
    }

    fn text_editor_focused(&self) -> bool {
        self.panel_visible() && self.renderer.as_ref().and_then(|r| r.panel()).is_some_and(|p| p.text_editor_focused())
    }

    /// Host pointer input over the canvas (canvas pixels); true when the
    /// panel took it, or when the wheel zoomed the field camera (down pulls
    /// it back, up brings it in; `dy` in pixels, about 100 a notch).
    pub fn pointer(&mut self, kind: &str, x: f32, y: f32, button: i32, dx: f32, dy: f32) -> bool {
        if self.renderer.as_mut().is_some_and(|r| r.pointer(kind, x, y, button, (dx, dy))) {
            return true;
        }
        if kind != "wheel" || self.panel_visible() {
            return false;
        }
        self.wheel += dy / 100.0;
        let steps = self.wheel.trunc();
        if steps != 0.0 {
            self.wheel -= steps;
            let graphics = self.settings.borrow().settings().graphics;
            let zoomed = graphics.zoomed(steps as i32);
            if zoomed != graphics {
                let _ = self.settings.borrow_mut().apply(SettingChange::Graphics(zoomed));
            }
        }
        true
    }

    /// Host key input for the panel, as Slint key text.
    pub fn key(&mut self, text: &str, down: bool) {
        if let Some(renderer) = &mut self.renderer {
            renderer.key(text, down);
        }
    }

    /// The game's sound since the last call: interleaved stereo at
    /// [`audio_sample_rate`], mixed by the volume and output settings.
    pub fn take_audio(&mut self) -> Vec<f32> {
        self.audio.take(self.settings.borrow().settings())
    }
    pub fn timeline_revision(&self) -> u64 { self.session.as_ref().map_or(0, |s| s.timeline_revision) }

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
        if result.is_ok() { self.sync_time_controls()?; }
        Ok(to_json(&match result {
            Ok(ack) => json!({ "ok": true, "revision": ack.revision, "settings": ack.settings }),
            Err(Rejected::Invalid { reason, .. }) => json!({ "ok": false, "rejected": "invalid", "reason": reason }),
            Err(Rejected::Storage { reason, .. }) => json!({ "ok": false, "rejected": "storage", "reason": reason }),
        }))
    }

    pub fn export_settings(&self) -> String { self.settings.borrow().export() }

    pub fn step_fast_forward_rate(&mut self, increase: bool) -> Result<String, JsValue> {
        let rate = self.settings.borrow().settings().fast_forward_rate;
        self.apply_setting(&to_json(&SettingChange::FastForwardRate(if increase { rate.increase() } else { rate.decrease() })))
    }
    pub fn step_rewind_interval(&mut self, increase: bool) -> Result<String, JsValue> {
        let seconds = self.settings.borrow().settings().rewind_interval_seconds;
        let next = if increase { seconds.saturating_add(1).min(self.settings.borrow().settings().rewind_history_seconds) }
            else { seconds.saturating_sub(1).max(1) };
        self.apply_setting(&to_json(&SettingChange::RewindIntervalSeconds(next)))
    }
    pub fn step_rewind_history(&mut self, increase: bool) -> Result<String, JsValue> {
        let seconds = self.settings.borrow().settings().rewind_history_seconds;
        let next = if increase { seconds.saturating_add(1).min(xem_settings::MAX_REWIND_HISTORY_SECONDS) }
            else { seconds.saturating_sub(1).max(self.settings.borrow().settings().rewind_interval_seconds) };
        self.apply_setting(&to_json(&SettingChange::RewindHistorySeconds(next)))
    }

    pub fn import_settings(&mut self, text: &str) -> Result<String, JsValue> {
        self.settings.borrow_mut().import(text).map_err(error)?;
        self.sync_time_controls()?;
        Ok(self.settings())
    }

    fn sync_time_controls(&mut self) -> Result<(), JsValue> {
        self.apply_time_settings(true)
    }

    fn apply_time_settings(&mut self, history: bool) -> Result<(), JsValue> {
        if let Some(session) = self.session.as_mut() {
            let settings = self.settings.borrow();
            let settings = settings.settings();
            let next = (settings.fast_forward_rate, settings.rewind_enabled, settings.rewind_interval_seconds, settings.rewind_history_seconds);
            if self.applied_time_settings.is_none_or(|old| old.0 != next.0) { session.configure_fast_forward(next.0).map_err(error)?; }
            let budget = if history && next.1 { xem_core::rewind::DEFAULT_REWIND_BYTES } else { 0 };
            if self.applied_time_settings.is_none_or(|old| (old.1,old.2,old.3) != (next.1,next.2,next.3)) {
                session.configure_rewind(budget,next.2,next.3).map_err(error)?;
                if budget == 0 { self.rewind_held = [false;2]; }
            }
            self.applied_time_settings = Some(next);
        }
        Ok(())
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
        module_id: String,
        rewind: bool,
    ) -> Result<String, JsValue> {
        let (executable, game_disc) = if load_executable {
            let disc = self.disc.as_ref().ok_or_else(|| error("no identified disc"))?;
            let identification = disc.identification().ok_or_else(|| error("no identified disc"))?;
            identification.disc.ok_or_else(|| error("not a known Xenogears disc"))?;
            (Some(identification.executable.clone()), Some(disc.game_source().map_err(error)?))
        } else {
            (None, None)
        };
        let schema = self.schema.as_deref().map(Schema::parse).transpose().map_err(|e| error(format!("schema: {e}")))?;
        self.session = None;
        self.audio = Default::default();
        self.running = false;
        if module_id.len() != 64 || !module_id.bytes().all(|b| b.is_ascii_hexdigit()) {
            return Err(error("module identity must be its SHA-256"));
        }
        let module = WebModule::new(&instance, import_names, module_id).map_err(error)?;
        self.memory_bytes = module.memory_bytes();
        // As xem_game::boot does natively (without a BIOS image: blank Kanji glyphs).
        let mut runtime = Runtime::new(module);
        if load_executable {
            runtime.identity.disc = xem_disc::identify::hex(&self.disc.as_ref().unwrap().identification().unwrap().sha256);
        }
        runtime.services().stub_names = self.stub_names.clone();
        runtime.services().card.attach_storage(Box::new(self.cards.clone())).map_err(error)?;
        runtime.services().spu.set_output(true);
        if let Some(executable) = executable {
            bios::install_kanji_font(runtime.memory(), None).map_err(error)?;
            // The BIOS copies the executable's text to its address and jumps to pc0.
            let header = exe::parse(&executable).map_err(error)?;
            runtime.memory().write(header.text_address, exe::text(&executable, &header)).map_err(error)?;
        }
        if let Some(source) = game_disc {
            runtime.services().insert_disc(source);
        }
        self.session = Some(Session::new(runtime,schema,|| std::time::Duration::from_secs_f64(now_ms()/1000.0)));
        self.applied_time_settings = None;
        self.rewind_held = [false;2];
        self.rewind_next = 0.0;
        self.apply_time_settings(rewind)?;
        self.host_buttons = 0;
        self.display_dirty = true;
        Ok(self.status())
    }

    /// Let the animation loop advance the game (in bounded batches) or stop it.
    pub fn set_running(&mut self, running: bool) {
        self.running = running && self.session.as_ref().is_some_and(|s| s.stopped.is_none());
        self.paced = None;
        self.music_paced = None;
        if let Some(session) = self.session.as_mut() {
            session.reset_rewind_timer();
        }
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
                    Some(session) => {
                        let revision = session.timeline_revision;
                        let reply = session.execute(&command);
                        if session.timeline_revision != revision {
                            self.audio = Default::default();
                            self.display_dirty = true;
                            self.paced = None;
                            self.music_paced = None;
                        }
                        if frames > 0 {
                            self.ran_frames();
                        }
                        reply
                    }
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
        session.snapshot_bytes().map_err(error)
    }

    /// Return to snapshot bytes of the same game module.
    pub fn restore_bytes(&mut self, bytes: &[u8]) -> Result<(), JsValue> {
        let session = self.session.as_mut().ok_or_else(|| error("no game session"))?;
        session.restore_bytes(bytes).map_err(error)?;
        self.audio = Default::default();
        self.paced = None;
        self.music_paced = None;
        self.display_dirty = true;
        Ok(())
    }

    // --- Memory cards -------------------------------------------------------

    /// The image IndexedDB holds for card `slot` (0, 1), or undefined for a
    /// fresh formatted card; the page sets both before a session boots.
    pub fn set_card_image(&mut self, slot: usize, image: Option<Vec<u8>>) -> Result<(), JsValue> {
        if slot > 1 {
            return Err(error("cards are 0 and 1"));
        }
        if image.as_ref().is_some_and(|i| i.len() != xem_core::devices::card::CARD_SIZE) {
            return Err(error("memory card image must be exactly 128 KiB (.mcd)"));
        }
        self.cards.set_image(slot, image);
        if let Some(session) = self.session.as_mut() {
            // Update the live device too: its next in-game save must use the
            // imported image rather than replace it with the old session card.
            let image = self.cards.image(slot);
            session.runtime.services().card.insert(slot, image);
        }
        Ok(())
    }

    /// Card `slot`'s image when the card device changed it since the last
    /// call (the page writes it to IndexedDB), else undefined.
    pub fn take_card_write(&mut self, slot: usize) -> Option<Vec<u8>> {
        (slot < 2).then(|| self.cards.take_write(slot)).flatten()
    }

    /// The image the card device holds in `slot` (none without a session).
    pub fn card_image(&mut self, slot: usize) -> Option<Vec<u8>> {
        let session = self.session.as_mut()?;
        (slot < 2).then(|| session.runtime.services().card.image(slot).map(<[u8]>::to_vec)).flatten()
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
                "panelTextEditorFocused": r.panel_visible && r.panel().is_some_and(|panel| panel.text_editor_focused()),
                "panelSection": r.panel().map(|panel| panel.component().get_selected_section()),
                "fps": r.frame_rate.as_ref().map(|counter| counter.label()),
                "lost": r.lost(),
            })
        });
        let settings_revision = self.settings.borrow().revision();
        let card_errors = self.session.as_mut().map(|s| s.runtime.services().card.errors.clone()).unwrap_or_default();
        to_json(&json!({
            "renderer": renderer,
            "disc": self.disc.as_ref().map(|d| d.status()),
            "session": session,
            "gameMemoryBytes": self.memory_bytes,
            "running": self.running,
            "loop": {
                "animationFrames": self.stats.animation_frames,
                "xrFrames": self.stats.xr_frames,
                "gameFrames": self.stats.game_frames,
                "lastBatch": self.stats.last_batch,
                "maxBatch": self.stats.max_batch,
                "dropped": self.stats.dropped,
                "maxFramesPerAnimation": self.session.as_ref().map_or(Some(MAX_FRAMES_PER_ANIMATION), |s| s.frame_factor().map(|factor| MAX_FRAMES_PER_ANIMATION*u64::from(factor))),
                "batchBudgetMs": BATCH_BUDGET_MS,
            },
            "audio": { "frames": self.audio.frames, "dropped": self.audio.dropped, "sampleRate": audio::SAMPLE_RATE },
            "cards": { "unsaved": self.cards.dirty(), "errors": card_errors },
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

/// The rate of [`XemApp::take_audio`]'s samples (the SPU's 44.1 kHz).
#[wasm_bindgen]
pub fn audio_sample_rate() -> u32 {
    audio::SAMPLE_RATE
}

/// The current settings' JSON shape, for documentation and clients.
#[wasm_bindgen]
pub fn default_settings() -> String {
    to_json(&xem_settings::Settings::default())
}

