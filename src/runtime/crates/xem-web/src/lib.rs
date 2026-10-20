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
// `XemApp::status`'s one `json!` literal expands deeper than the default
// limit (128).
#![recursion_limit = "256"]

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

/// An agent's `first_person` command (`window.xem.firstPerson`) or the app's
/// Toggle first-person view on the page's view (shared with its WebXR
/// sessions), in `field` under the `renderer` setting
/// (`xem_settings::look::Command`): its answer, or why nothing changed.
fn first_person(view: &mut WebRenderer, command: &xem_settings::look::Command, field: xem_core::FieldControls, renderer: xem_settings::Renderer) -> Result<Value, String> {
    let change = command.change(view.first_person(), view.look_angles(), field, renderer)?;
    if change.on != view.first_person() {
        view.set_first_person(change.on);
    }
    view.look_around(change.turn.0, change.turn.1);
    Ok(xem_settings::look::reply(view.first_person(), view.look_angles()))
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
    /// The user's imported images by id, oldest first: the last is the one
    /// the page shows; the verified Xenogears discs among them are the game's.
    discs: Vec<(u32, disc::DiscImport)>,
    next_disc: u32,
    /// The disc the game waits for that the drive's changer has not got.
    wanted_disc: Option<u8>,
    renderer: Option<WebRenderer>,
    panel_closed: Rc<Cell<bool>>,
    audio: audio::Mixer,
    audio_scratch: Vec<i16>,
    cards: storage::BrowserCards,
    /// The player's BIOS image (Import BIOS; IndexedDB keeps it beside the
    /// cards), whose Kanji font every boot installs; the substitute glyphs
    /// without one.
    bios: Option<Vec<u8>>,
    input: xem_settings::player::PlayerInput,
    mouse_captured: bool,
    /// The interface the player's input reached last (the wheel's) and its
    /// entries a pointer selects.
    ui: xem_core::context::Context,
    targets: Option<xem_core::menu::Targets>,
    app_input: AppInput,
    rewind_held: [bool;2],
    rewind_next: f64,
    hud_forward: Option<f64>,
    hud_rewind: Option<f64>,
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
    /// Touch: the control deck, whether a finger has touched the game, and
    /// what touches asked for (taps on the picture, shop quantity keys).
    deck: xem_assist::touch::Deck,
    touched: bool,
    taps: Vec<(f32, f32)>,
    quantity: Vec<xem_assist::overlay::QuantityKey>,
    /// A headset's hands and gaze (WebXR): the shared headset core, this
    /// frame's input, the screen as last shown, and two-hand scaling.
    headset: xem_assist::headset::Headset,
    xr_input: Option<xem_assist::headset::Input>,
    xr_screen: Option<xem_assist::headset::Screen>,
    xr_scaling: xem_assist::headset::Scaling,
    /// The settings panel's pointer inside a headset: a pinch or a
    /// controller's select presses it from its start to its end.
    xr_pointer: xem_assist::headset::PanelPointer,
    /// The last immersive frame's gestures (`status().xr.gestures`).
    xr_gestures: Vec<xem_assist::gesture::Gesture>,
    /// Each controller's select (trigger): held this frame, and begun since
    /// the pointer last took it.
    xr_select: [bool; 2],
    xr_select_began: [bool; 2],
    /// The gestures recognised in sessions (`status().xr.counts`, for
    /// tests: most leave no state behind them).
    xr_counts: xem_assist::gesture::Counts,
    /// The session's reference space resets (the runtime's recentring) and
    /// the player's Recenter actions.
    xr_resets: u32,
    xr_recenters: u32,
    /// The scales two-hand scaling of the diorama committed to the setting.
    xr_scale_commits: u32,
    /// The immersive session's environment blend mode (WebXR's
    /// `environmentBlendMode`), while one runs.
    xr_blend: Option<String>,
    /// What a headset's control deck holds.
    xr_held: std::collections::BTreeSet<xem_core::action::GameAction>,
    /// Where the pointer last met the panel in the session (u, v), and
    /// where it last clicked (logical pixels).
    xr_panel_hit: Option<(f32, f32)>,
    xr_panel_click: Option<(f32, f32)>,
    /// First-person inspection: the first gamepad's buttons and axes as
    /// they are (before app chords take theirs), the arrow keys held
    /// (whatever the bindings say), the toggle, the app's Toggle
    /// first-person view asked for since the last step (its key, its chord,
    /// the deck's LOOK), the mouse motion that turns the view, when it last
    /// turned (page time), and a headset's stepped turn.
    pad: [u8; 16],
    axes: [f32; 4],
    arrows: [bool; 4],
    look_toggle: xem_settings::look::Toggle,
    look_asked: bool,
    look_mouse: [f32; 2],
    looked: Option<f64>,
    snap: xem_settings::look::SnapTurn,
}

/// The arrow keys, in `look::Turn::arrows` order.
const ARROWS: [&str; 4] = ["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown"];

/// The browser's game and player's input, as the headset's effects reach them.
struct WebCarry<'a> {
    session: Option<&'a mut Session<WebModule>>,
    input: &'a mut xem_settings::player::PlayerInput,
    /// The underhand grab asked for first-person inspection's toggle.
    inspect: bool,
    /// AUTO beside the screen asked for auto-advance's toggle.
    auto_advance: bool,
}

impl xem_assist::headset::Carry for WebCarry<'_> {
    fn command(&mut self, command: Value) {
        if let Some(session) = self.session.as_mut() {
            let reply = session.execute(&command);
            if let Some(error) = reply.get("error") {
                web_sys::console::warn_1(&format!("assist command {command} failed: {error}").into());
            }
        }
    }
    fn press(&mut self, actions: Vec<xem_core::action::GameAction>) {
        self.input.pulse(xem_settings::player::Pulse::new(actions));
    }
    fn select(&mut self, index: usize, confirm: bool) {
        self.input.select(index, confirm);
    }
    fn inspect(&mut self) {
        self.inspect = true;
    }
    fn release(&mut self) {
        self.input.clear();
    }
    fn toggle_auto_advance(&mut self) {
        self.auto_advance = true;
    }
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
            discs: Vec::new(),
            next_disc: 1,
            wanted_disc: None,
            renderer: None,
            panel_closed: Rc::new(Cell::new(false)),
            audio: audio::Mixer::default(),
            audio_scratch: Vec::new(),
            cards: storage::BrowserCards::default(),
            bios: None,
            input: Default::default(),
            mouse_captured: false,
            ui: xem_core::context::Context::None,
            targets: None,
            app_input: AppInput::default(),
            rewind_held: [false;2],
            rewind_next: 0.0,
            hud_forward: None,
            hud_rewind: None,
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
            deck: Default::default(),
            touched: false,
            taps: Vec::new(),
            quantity: Vec::new(),
            headset: Default::default(),
            xr_input: None,
            xr_screen: None,
            xr_scaling: Default::default(),
            xr_pointer: Default::default(),
            xr_gestures: Vec::new(),
            xr_select: [false; 2],
            xr_select_began: [false; 2],
            xr_counts: Default::default(),
            xr_resets: 0,
            xr_recenters: 0,
            xr_scale_commits: 0,
            xr_blend: None,
            xr_held: Default::default(),
            xr_panel_hit: None,
            xr_panel_click: None,
            pad: [0; 16],
            axes: [0.0; 4],
            arrows: [false; 4],
            look_toggle: Default::default(),
            look_asked: false,
            look_mouse: [0.0; 2],
            looked: None,
            snap: Default::default(),
        }
    }

    // --- Presentation -------------------------------------------------------

    /// Take the renderer [`create_renderer`] made and put the Slint panel on it.
    pub fn attach_renderer(&mut self, renderer: Renderer) {
        let mut renderer = renderer.0;
        let closed = self.panel_closed.clone();
        renderer.attach_panel(self.settings.clone(), move || closed.set(true));
        if let Some(panel) = renderer.panel() {
            // The Controls section lists first-person inspection's keys, and
            // Saves and Shortcuts the browser storage's persistence request.
            panel.set_first_person(true);
            panel.set_browser_storage(true);
            let requests = self.app_requests.clone();
            panel.on_app_request(move |request| requests.borrow_mut().push(request));
            // The panel's Import disc… is the page's file chooser.
            panel.on_import_disc(|| {
                use wasm_bindgen::JsCast;
                let input = web_sys::window()
                    .and_then(|window| window.document())
                    .and_then(|document| document.get_element_by_id("file"))
                    .and_then(|element| element.dyn_into::<web_sys::HtmlElement>().ok());
                if let Some(input) = input {
                    input.click();
                }
            });
            if let Some((_, disc)) = self.discs.last() {
                panel.set_disc_status(&disc_summary(disc));
            }
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
        // Game-paced presentation (no interpolation) draws only when the
        // game drew (or the panel shows); the canvas keeps the last picture
        // meanwhile.
        let game_paced = self.settings.borrow().settings().graphics.game_paced();
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
        // The panel's Recenter places this session's view (the page's
        // operations take the rest of the app's requests).
        let recentring = {
            let mut requests = self.app_requests.borrow_mut();
            let before = requests.len();
            requests.retain(|request| *request != AppRequest::Action(xem_settings::AppAction::Recenter));
            requests.len() != before
        };
        if recentring {
            let outcome = self.recenter();
            if let Some(panel) = self.renderer.as_ref().and_then(|r| r.panel()) {
                match outcome {
                    Ok(_) => panel.clear_operation_error(),
                    Err(error) => panel.set_operation_error(&error),
                }
            }
        }
        self.assist_xr();
        self.commit_scale();
        self.step(now)?;
        let eye_height = viewports.chunks_exact(4).map(|v| v[3]).max().unwrap_or(0).max(0) as u32;
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
        if let Some(renderer) = &mut self.renderer {
            renderer.set_xr_eye_height(eye_height);
            if let Some(view) = views.first() {
                renderer.set_xr_eye(view);
            }
        }
        self.take_game_frame(true);
        self.stats.xr_frames += 1;
        if let Some(renderer) = &mut self.renderer {
            renderer.render_xr(&layer, &views, now / 1000.0, &self.settings.borrow()).map_err(error)?;
            self.xr_screen = renderer.xr_screen(&self.settings.borrow());
        }
        Ok(())
    }

    /// A headset frame's input, before [`XemApp::xr_frame`]: `data` holds
    /// the viewer's pose (position, then orientation x, y, z, w), whether
    /// the session is visible (1), then for the left and right hands each:
    /// a target ray (present, position, orientation) and the 25 joints
    /// (present, then x, y, z each), all in the session's reference space.
    /// `select` has a bit for each controller whose select (its trigger) is
    /// held this frame, or began and ended since the last (1 the left's, 2
    /// the right's).
    pub fn xr_input(&mut self, now: f64, data: &[f32], select: i32) {
        use xem_render::glam::{Quat, Vec3};
        const HAND: usize = 8 + 1 + 75 + 2;
        // In the session, the Controls section lists first-person
        // inspection's headset controls, and Graphics offers Recenter.
        if let Some(panel) = self.renderer.as_ref().and_then(|r| r.panel()) {
            panel.set_first_person_headset(true);
            panel.set_in_headset(true);
        }
        if data.len() < 8 + 2 * HAND {
            self.xr_input = None;
            return;
        }
        let pose = |d: &[f32]| xem_assist::headset::Pose { position: Vec3::new(d[0], d[1], d[2]), orientation: Quat::from_xyzw(d[3], d[4], d[5], d[6]).normalize() };
        let mut input = xem_assist::headset::Input { time: now / 1000.0, head: Some(pose(&data[..7])), focused: data[7] != 0.0, ..Default::default() };
        for hand in 0..2 {
            let d = &data[8 + hand * HAND..][..HAND];
            if d[0] != 0.0 {
                input.rays[hand] = Some(pose(&d[1..8]));
            }
            if d[8] != 0.0 {
                input.hands[hand] = Some(std::array::from_fn(|j| Vec3::new(d[9 + j * 3], d[10 + j * 3], d[11 + j * 3])));
            }
            // A hand source's select: WebXR's standard pinch. WebXR has no
            // thumb microgestures: the hands play as a pointer.
            if d[84] != 0.0 {
                input.pinch[hand] = Some(d[85] != 0.0);
            }
        }
        // Each controller's select (its trigger), held or not, and whether
        // it began.
        for hand in 0..2 {
            let held = select & (1 << hand) != 0;
            self.xr_select_began[hand] |= held && !self.xr_select[hand];
            self.xr_select[hand] = held;
        }
        // The Binaural listener turns with the head as the seat sees it
        // (presentation only).
        let seat = self.renderer.as_ref().map(|r| r.xr_seat).unwrap_or_default();
        if let (Some(session), Some(head)) = (self.session.as_mut(), input.head) {
            session.set_head(Some(xem_core::binaural::head_axes(seat.relative(head.orientation).to_array())));
        }
        self.xr_input = Some(input);
    }

    /// An immersive session began, its environment blend mode WebXR's
    /// `environmentBlendMode`: any but `opaque` (immersive AR's
    /// `alpha-blend`) shows the real room around what the frames draw, which
    /// they leave transparent (no room backdrop).
    pub fn xr_session(&mut self, environment_blend_mode: &str) {
        self.xr_blend = Some(environment_blend_mode.to_string());
        if let Some(renderer) = &mut self.renderer {
            renderer.xr_room = environment_blend_mode != "opaque";
        }
    }

    /// A two-hand scaling of the diorama that ended becomes the player's
    /// setting, once, through the settings service (the panel shows it).
    fn commit_scale(&mut self) {
        if let Some(percent) = self.xr_scaling.reached() {
            match xem_assist::headset::commit_scale(&mut self.settings.borrow_mut(), percent) {
                Ok(_) => self.xr_scale_commits += 1,
                Err(error) => web_sys::console::warn_1(&format!("the diorama's scale {percent}% was not kept: {error}").into()),
            }
        }
    }

    /// The runtime reset the session's reference space (its `reset` event:
    /// the system recentred it), as OpenXR's hosts take theirs
    /// (`xem_assist::headset::reference_changed`).
    pub fn xr_reference_reset(&mut self) {
        if let Some(renderer) = self.renderer.as_mut() {
            let mut seat = renderer.xr_seat;
            xem_assist::headset::reference_changed(&mut self.headset, &mut self.xr_scaling, &mut seat, renderer.view_mut());
            renderer.xr_seat = seat;
        }
        self.xr_resets += 1;
    }

    /// The Recenter action: in a session, the view placed in front of the
    /// head as it is now; refused out of one.
    pub fn xr_recenter(&mut self) -> Result<String, JsValue> {
        self.recenter().map_err(error)
    }

    fn recenter(&mut self) -> Result<String, String> {
        let head = self.xr_input.as_ref().and_then(|input| input.head).ok_or("Recentering places a headset's view: enter VR first.")?;
        let renderer = self.renderer.as_mut().ok_or("no renderer")?;
        let mut seat = renderer.xr_seat;
        let text = xem_assist::headset::recenter(&mut seat, Some(head), renderer.view_mut())?;
        renderer.xr_seat = seat;
        self.xr_recenters += 1;
        Ok(text)
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

    /// The immersive session ended: the canvas shows the game again (a
    /// two-hand scaling of the diorama under way ends where it reached).
    pub fn xr_ended(&mut self) {
        self.xr_input = None;
        self.xr_blend = None;
        self.xr_scaling.end();
        self.commit_scale();
        if let Some(session) = self.session.as_mut() {
            session.set_head(None);
        }
        self.headset = Default::default();
        self.input.clear();
        if let Some(renderer) = &mut self.renderer {
            renderer.release_xr();
            if let Some(panel) = renderer.panel() {
                panel.set_first_person_headset(false);
                panel.set_in_headset(false);
            }
        }
        self.display_dirty = true;
    }

    /// Whether the renderer can draw immersive frames.
    pub fn xr_capable(&self) -> bool {
        self.renderer.as_ref().is_some_and(|r| r.xr_capable())
    }

    /// Touch play: the deck laid out by the picture, taps on the picture
    /// resolved in the game's context, quantity keys, the destination's
    /// mark, and the deck's held actions.
    fn assist_touch(&mut self) {
        let ui = self.ui;
        let picture = self.renderer.as_ref().and_then(|r| r.picture);
        if let (Some(picture), Some(renderer)) = (picture, &self.renderer) {
            let placed = xem_assist::touch::Rect { x: picture.placed.x, y: picture.placed.y, width: picture.placed.width, height: picture.placed.height };
            let (width, height) = renderer.size();
            self.deck.arrange((width as f32, height as f32), placed, renderer.scale_factor(), ui);
        }
        // First-person inspection shows no field camera to walk or point
        // through: its taps walk nowhere and talk to no one. Otherwise the
        // field as the picture shows it (pulled back, say).
        let inspecting = self.first_person();
        let viewpoint = self.renderer.as_ref().and_then(|r| r.viewpoint());
        let Some(session) = self.session.as_mut() else { return };
        let field = (session.runtime.services().mode == Some(1) && !inspecting).then(|| xem_core::field::Field::read(session.runtime.memory())).flatten()
            .map(|f| xem_assist::overlay::seen(f, viewpoint.as_ref()));
        for tap in std::mem::take(&mut self.taps) {
            let Some(at) = picture.and_then(|p| p.to_screen(tap.0, tap.1)) else { continue };
            // A tap on the paused game resumes it, as a click does.
            if self.input.unpause() { continue; }
            let outs = xem_assist::dispatch::tap_at(xem_assist::dispatch::Pointed::Screen(at), ui, field.as_ref(), self.targets.as_ref()).into_iter().collect();
            xem_assist::headset::carry(outs, &mut WebCarry { session: Some(session), input: &mut self.input, inspect: false, auto_advance: false }, &mut self.xr_scaling);
        }
        for key in std::mem::take(&mut self.quantity) {
            session.execute(&json!({"cmd": "shop_quantity", "op": key.op()}));
        }
        let destination = match session.assist.as_ref().map(|a| &a.intent) {
            Some(xem_core::assist::Intent::Walk { to, .. }) if !inspecting => Some(*to),
            _ => None,
        };
        let marks = xem_assist::overlay::marks(ui, None, field.as_ref(), destination, None, false);
        let shown = self.touched && self.settings.borrow().settings().assisted.touch_controls;
        let style = xem_render::glass::Style { reduced_transparency: self.settings.borrow().settings().assisted.reduced_transparency };
        let held = self.deck.held();
        if let Some(renderer) = &mut self.renderer {
            if let Some(panel) = renderer.panel() {
                panel.set_first_person_touch(shown);
            }
            renderer.overlay.marks = marks;
            renderer.overlay.ui = shown.then_some(ui);
            renderer.overlay.style = style;
            let auto_advance = self.settings.borrow().settings().auto_advance.enabled;
            renderer.overlay.deck = self.deck.controls.iter().map(|(c, r)| (*c, *r, xem_assist::draw::deck_state(*c, held.contains(c), auto_advance, inspecting))).collect();
        }
        self.input.touch = self.deck.actions().union(&self.xr_held).copied().collect();
    }

    /// A headset frame's gestures, before its game frames: aimed at the
    /// screen as last shown, resolved in the game's context and carried
    /// out; the panel opens and closes with prayer hands and takes the
    /// pointer's clicks (a pinch, or a controller's select).
    fn assist_xr(&mut self) {
        let Some(input) = self.xr_input.clone() else { return };
        let settings = self.settings.borrow().settings().clone();
        self.headset.source = match settings.assisted.pointer {
            xem_settings::XrPointer::Eye => xem_assist::headset::PointerSource::Eye,
            xem_settings::XrPointer::Head => xem_assist::headset::PointerSource::Head,
            xem_settings::XrPointer::LeftHand => xem_assist::headset::PointerSource::LeftRay,
            xem_settings::XrPointer::RightHand => xem_assist::headset::PointerSource::RightRay,
        };
        let panel_visible = self.panel_visible();
        if self.headset.settings_open != panel_visible {
            self.headset.set_settings(panel_visible);
        }
        let Some(screen) = self.xr_screen else { return };
        let ui = self.ui;
        // Two-hand scaling scales the diorama while it is the view (from its
        // setting's scale), else the screen.
        let diorama = matches!(xem_render::ps1::xr_view(&settings.graphics), xem_render::ps1::XrView::Diorama(_)).then_some(settings.graphics.xr.diorama_scale);
        self.xr_scaling.view(diorama);
        // First-person inspection keeps the character where it stands, and
        // shows no field camera to point through; otherwise the field as the
        // screen shows it (pulled back, say).
        let inspecting = self.first_person();
        self.headset.inspecting = inspecting;
        self.headset.auto_advance = settings.auto_advance.enabled;
        let viewpoint = self.renderer.as_ref().and_then(|r| r.viewpoint());
        let (field, targets, controls) = match self.session.as_mut() {
            Some(session) => (
                (session.runtime.services().mode == Some(1) && !inspecting).then(|| xem_core::field::Field::read(session.runtime.memory())).flatten()
                    .map(|f| xem_assist::overlay::seen(f, viewpoint.as_ref())),
                session.runtime.targets(),
                session.runtime.field_controls(),
            ),
            None => (None, None, Default::default()),
        };
        let effects = if settings.assisted.gestures {
            self.headset.update(&input, &screen, ui, field.as_ref(), targets.as_ref())
        } else {
            xem_assist::headset::Effects { aim: self.headset.aim(&input, &screen), ..Default::default() }
        };
        let pinched = effects.gestures.iter().find_map(|g| match g {
            xem_assist::gesture::Gesture::Pinch { hand } => Some(hand.index()),
            _ => None,
        });
        self.xr_counts.add(&effects.gestures);
        self.xr_held = effects.held.clone();
        let (inspect, auto_advance) = {
            let mut host = WebCarry { session: self.session.as_mut(), input: &mut self.input, inspect: false, auto_advance: false };
            xem_assist::headset::carry(effects.outs, &mut host, &mut self.xr_scaling);
            (host.inspect, host.auto_advance)
        };
        if auto_advance {
            self.app_requests.borrow_mut().push(AppRequest::Action(xem_settings::AppAction::ToggleAutoAdvance));
        }
        // The underhand grab toggles inspection in a field but its menu,
        // where Ex Machina draws it.
        if inspect && xem_settings::look::grab_toggles(controls, settings.graphics.renderer) && let Some(renderer) = &mut self.renderer {
            renderer.set_first_person(!inspecting);
        }
        self.xr_gestures = effects.gestures;
        if self.headset.settings_open != panel_visible {
            self.set_panel_visible(self.headset.settings_open);
        }
        // The panel takes the pointer's ray and clicks; the frame shows the
        // screen or the diorama as the scaling has them.
        if let Some(renderer) = &mut self.renderer {
            renderer.xr_scaling = self.xr_scaling;
            let destination = match self.session.as_ref().and_then(|s| s.assist.as_ref()).map(|a| &a.intent) {
                Some(xem_core::assist::Intent::Walk { to, .. }) if !inspecting => Some(*to),
                _ => None,
            };
            let marks = if settings.assisted.gestures { xem_assist::overlay::marks(ui, effects.aim, field.as_ref(), destination, targets.as_ref(), screen.diorama.is_some()) } else { vec![] };
            renderer.overlay.room = xem_assist::draw::room(&marks, &screen, &self.headset.key_marks(ui, targets.as_ref()), &self.headset.pad_marks(ui));
            renderer.overlay.style = xem_render::glass::Style { reduced_transparency: settings.assisted.reduced_transparency };
            let seat = renderer.xr_seat;
            if renderer.panel_visible && let Some(panel) = renderer.panel() {
                use xem_assist::headset::{PanelEvent, Presser};
                let quad = render::xr_panel_quad(&seat, &self.settings.borrow(), panel.size(), screen.quad.size, screen.diorama.is_some());
                // A controller's select presses where that controller
                // points; otherwise the pointer source aims and a pinch
                // presses. Either holds the pointer until it ends.
                let selected = (0..2).find(|&hand| self.xr_select_began[hand] && input.rays[hand].is_some());
                let began = selected.map(Presser::Select).or(pinched.map(Presser::Pinch));
                let hit = match selected.and_then(|hand| input.rays[hand]) {
                    Some(ray) => quad.hit(ray.position, ray.forward()),
                    None => self.headset.hit(&input, &quad),
                };
                let selects = self.xr_select;
                let held = |presser| match presser {
                    Presser::Select(hand) => selects[hand],
                    Presser::Pinch(hand) => input.pinch[hand] == Some(true),
                };
                let scale = panel.scale_factor();
                let (w, h) = panel.size();
                let at = |(u, v): (f32, f32)| xem_ui::LogicalPosition::new(u * w as f32 / scale, v * h as f32 / scale);
                let button = xem_ui::PointerEventButton::Left;
                for event in self.xr_pointer.update(hit.map(|(uv, _)| uv), began, held) {
                    panel.dispatch(match event {
                        PanelEvent::Moved(uv) => {
                            self.xr_panel_hit = Some(uv);
                            xem_ui::WindowEvent::PointerMoved { position: at(uv) }
                        }
                        PanelEvent::Pressed(uv) => {
                            let position = at(uv);
                            self.xr_panel_click = Some((position.x, position.y));
                            xem_ui::WindowEvent::PointerPressed { position, button }
                        }
                        PanelEvent::Released(uv) => xem_ui::WindowEvent::PointerReleased { position: at(uv), button },
                        PanelEvent::Exited => {
                            self.xr_panel_hit = None;
                            xem_ui::WindowEvent::PointerExited
                        }
                    });
                }
            }
        }
        self.xr_select_began = [false; 2];
    }

    /// Whether first-person inspection shows.
    fn first_person(&self) -> bool {
        self.renderer.as_ref().is_some_and(|r| r.first_person())
    }

    /// First-person inspection this frame, at page time `now`
    /// (`xem_settings::look`), paused or not: toggled in a field, where Ex
    /// Machina draws it, by Select's keys, the touch deck's SELECT, the
    /// gamepad's Back released alone or its right stick's click, and
    /// anywhere in one but its menu by the app's Toggle first-person view;
    /// while it shows, turned by the arrow keys, the right stick, a finger on
    /// the picture and the mouse the game holds, or in a headset by the right
    /// stick in steps (the head looks around). Whether it shows. Neither
    /// happens while the settings panel has the input or the page has lost
    /// focus, when the game's buttons are let go too (the page reads the
    /// gamepad whatever the focus). The Original renderer ends it.
    fn look(&mut self, bindings: &xem_settings::GameBindings, field: xem_core::FieldControls, now: f64) -> bool {
        // The mouse's and a finger's motion are the view's this frame, or no
        // one's, as Toggle first-person view asked for since the last.
        let mut mouse = std::mem::take(&mut self.look_mouse);
        let (dx, dy) = self.deck.take_drag();
        let asked = std::mem::take(&mut self.look_asked);
        let looked = self.looked.replace(now);
        let setting = self.settings.borrow().settings().graphics.renderer;
        let drawn = setting == xem_settings::Renderer::ExMachina;
        let away = self.panel_visible() || !self.app_input.focused();
        let headset = self.xr_input.is_some();
        let Some(renderer) = self.renderer.as_mut() else { return false };
        // The console's own picture has nothing to look around in, whether
        // a game frame runs or not (a headset would go on drawing the eyes).
        if !drawn {
            renderer.set_first_person(false);
        }
        if away {
            self.look_toggle.clear();
            self.snap.clear();
            self.looked = None;
            return renderer.first_person();
        }
        let select = bindings.play.get(&xem_settings::GameAction::Select).is_some_and(|b| b.keyboard.iter().any(|k| self.input.key_held(k)))
            || self.deck.held().contains(&xem_assist::touch::Control::Select);
        let was = renderer.first_person();
        if self.look_toggle.update(select, &self.pad, field) && drawn {
            renderer.set_first_person(!was);
        } else if asked {
            // Refused (nothing changes) where the grab toggles nothing.
            let _ = first_person(renderer, &xem_settings::look::Command::TOGGLE, field, setting);
        }
        if renderer.first_person() != was {
            self.input.mouse.clear();
            mouse = [0.0; 2];
        }
        let on = renderer.first_person();
        if xem_settings::look::turns(on, field) {
            if headset {
                renderer.look_around(self.snap.update(self.axes[2]), 0.0);
            } else {
                let seconds = looked.map_or(0.0, |then| ((now - then) / 1000.0).clamp(0.0, 0.1)) as f32;
                let density = renderer.scale_factor();
                let turn = xem_settings::look::Turn { arrows: self.arrows, stick: [self.axes[2], self.axes[3]], mouse, touch: [dx / density, dy / density] };
                let (yaw, pitch) = turn.radians(seconds);
                renderer.look_around(yaw, pitch);
            }
        }
        on
    }

    /// Whether a headset holds the game still: the player's Pause, or
    /// settings' own pause, which a held rewind goes on through (a chord
    /// while the panel shows).
    fn xr_paused(&self) -> bool {
        let pauses = &self.headset.pauses;
        self.xr_input.is_some() && (pauses.player || (pauses.settings && !self.rewind_held.iter().any(|held| *held)))
    }

    /// The game frames due since the last step (see [`XemApp::frame`]).
    fn step(&mut self, now: f64) -> Result<(), JsValue> {
        self.sync_time_controls()?;
        // First-person inspection is toggled and turned whether the game
        // runs or not (a headset's Pause stands apart from it).
        let field = self.session.as_mut().map(|s| s.runtime.field_controls()).unwrap_or_default();
        let bindings = self.settings.borrow().settings().game_bindings.clone();
        let inspecting = self.look(&bindings, field, now);
        // The Binaural listener looks where the presentation looks, whether
        // the game runs or not, as the head turns it (presentation only).
        if let Some(session) = self.session.as_mut() {
            session.set_listener_view(self.renderer.as_mut().and_then(|r| r.listener_axes()));
        }
        // Paused in a headset: time stands still, nothing to catch up
        // afterwards, and the rewind history's play time stands still too
        // (as the native hosts' `Game::resume` restarts its timer).
        if self.xr_paused() {
            self.hud_forward = None;
            self.hud_rewind = None;
            // The panel's own Close still closes it (and its pause).
            if self.panel_closed.replace(false) {
                self.set_panel_visible(false);
            }
            self.paced = Some(now);
            self.music_paced = Some(now);
            if let Some(session) = self.session.as_mut() {
                session.reset_rewind_timer();
            }
            self.stats.last_batch = 0;
            return Ok(());
        }
        if self.running && self.rewind_held.iter().any(|held| *held) {
            if now >= self.rewind_next {
                if let Some(session) = self.session.as_mut() {
                    let advanced = session.rewind_step().map_err(error)?;
                    self.hud_rewind = advanced.then_some(now);
                    // The frames played on from the state, picture and sound.
                    if advanced { self.ran_frames(); }
                }
                self.rewind_next = now + xem_core::rewind::REWIND_STEP_MS as f64;
            }
            self.paced = Some(now);
            self.music_paced = Some(now);
            self.stats.last_batch = 0;
        } else if self.running {
            // The player's input reaches pad 1 when it changes, so a pad an
            // automation client set stays until the player presses something.
            let context = self.session.as_mut().map_or_else(Default::default, |s| xem_settings::player::Context {
                attack: s.runtime.combo_input(),
                field: s.runtime.field_controls(),
                assignment: s.runtime.button_assignment(),
                ui: s.runtime.context(),
                paused: s.runtime.game_paused(),
            });
            self.ui = context.ui;
            self.targets = self.session.as_mut().and_then(|s| s.runtime.targets());
            self.assist_touch();
            if self.panel_visible() { self.input.clear(); }
            self.input.browse(self.targets.as_ref());
            // Running by default would leave Run pressed alone once the view
            // holds the directions back.
            self.input.run_by_default = self.settings.borrow().settings().run_by_default && !inspecting;
            let buttons = if self.panel_visible() { 0 } else { self.input.buttons(&bindings, now, context) };
            let buttons = xem_settings::look::game_buttons(buttons, inspecting, context.field, &context.assignment);
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
            let live_music = match self.session.as_mut() {
                Some(session) => session.begin_live_music().map_err(error)?,
                None => false,
            };
            let due = factor.map(|factor| elapsed_frames.saturating_mul(u64::from(factor)));
            let limit = due.map(|due| due.min(MAX_FRAMES_PER_ANIMATION * u64::from(factor.unwrap())));
            let start = now_ms();
            let mut batch = 0;
            let mut fetching = false;
            let (pause_on_focus_loss, sound_mode, binaural, random_battles_off, half_encounters, text_speed, movie_skip, auto_advance) = {
                let settings = self.settings.borrow();
                let settings = settings.settings();
                (settings.pause_on_focus_loss, settings.game_sound_mode(), settings.binaural, settings.random_battles_off(),
                 settings.half_encounters(), settings.text_speed.glyphs(), settings.movie_skip, settings.auto_advance_dwell())
            };
            if let Some(session) = self.session.as_mut() {
                session.set_binaural(binaural);
                // Inspection stops a route on entry and refuses new ones;
                // a presentation just started takes the listener's view.
                session.set_inspecting(inspecting);
                session.set_listener_view(self.renderer.as_mut().and_then(|r| r.listener_axes()));
            }
            while limit.is_none_or(|frames| batch < frames) && (batch == 0 || now_ms() - start < BATCH_BUDGET_MS) {
                let Some(session) = self.session.as_mut() else { break };
                session.set_window_focus(self.app_input.focused(),pause_on_focus_loss);
                session.set_actor_view(self.actor_view).map_err(error)?;
                session.set_attack_page(self.input.attack_pointed());
                session.set_sound_mode(sound_mode).map_err(error)?;
                session.set_random_battles(random_battles_off).map_err(error)?;
                session.set_half_encounters(half_encounters);
                session.set_text_speed(text_speed).map_err(error)?;
                session.set_movie_skip(movie_skip);
                session.set_auto_advance(auto_advance).map_err(error)?;
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
            if batch > 0 {
                self.hud_forward = (fast_forward && self.session.as_mut().is_some_and(|s| !s.runtime.game_paused())).then_some(now);
            }
            self.input.frames_ran(batch as u32);
            // The setting follows the game's own Sound screen.
            if let Some(output) = self.session.as_mut().and_then(|s| s.take_sound_mode_choice()).and_then(xem_settings::AudioOutput::from_game_mode) {
                let _ = self.settings.borrow_mut().apply(SettingChange::AudioOutput(output));
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
            // Adapt fixed fast-forward to the rate the batches actually reach;
            // the elapsed-time music clock keeps playing throughout.
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
        // A disc the game waits for and the changer has not got: ask for it.
        let wanted = self.session.as_mut().and_then(|session| session.runtime.services().cd.wanted());
        if wanted != self.wanted_disc {
            self.wanted_disc = wanted;
            if let (Some(number), Some(panel)) = (wanted, self.renderer.as_ref().and_then(|r| r.panel())) {
                panel.ask_for_disc(number);
                self.set_panel_visible(true);
            }
        }
        Ok(())
    }

    /// Hands what the game drew to the renderer (configured for the canvas or
    /// a headset) and updates the panel's status.
    fn take_game_frame(&mut self, headset: bool) {
        if (!self.running || self.xr_paused()) && let Some(session) = &mut self.session {
            if session.debug.busy() {
                if let Err(error) = session.tick_debug() {
                    if let Some(panel) = self.renderer.as_ref().and_then(|r| r.panel()) { panel.set_operation_error(&error); }
                }
                self.display_dirty = true;
            }
        }
        let now = now_ms();
        let held = self.rewind_held.iter().any(|held| *held);
        // What `step` runs: a held rewind through settings' pause too.
        let active = self.running && !self.xr_paused();
        let forward = active && !held && self.session.as_ref().is_some_and(|s| s.fast_forward && s.stopped.is_none())
            && self.hud_forward.is_some_and(|at| now - at < 150.0);
        let rewind = active && held && self.hud_rewind.is_some_and(|at| now - at < xem_core::rewind::REWIND_STEP_MS as f64 + 100.0);
        if let Some(renderer) = &mut self.renderer {
            if let Some(hud) = &mut renderer.frame_rate { hud.set_activity(forward,rewind); }
            if let (Some(panel),Some(session)) = (renderer.panel(),self.session.as_ref()) {
                panel.set_debug_status(true, session.debug.busy(), &session.debug.message);
                panel.set_time_status(Some(session.fast_forward), &format!("{:.0} of {} seconds available · {:.1} MiB used{}",
                    session.rewind.available_seconds(), session.rewind.history_seconds, session.rewind.bytes as f64/1048576.0,
                    session.rewind.error.as_ref().map(|e| format!(" — {e}")).unwrap_or_default()));
            } else if let Some(panel) = renderer.panel() {
                panel.set_debug_status(false, false, "Boot a game to use Debug.");
                panel.set_time_status(None,"Boot a game to use save states and time controls.");
            }
            match self.session.as_mut() {
                Some(session) if self.display_dirty => {
                    let graphics = self.settings.borrow().settings().graphics;
                    if graphics.renderer == xem_settings::Renderer::Original {
                        // The console's own picture, as the device shows it
                        // (nothing to look around in: `look` ends the view).
                        self.actor_view = 0;
                        let gpu = &mut session.runtime.services().gpu;
                        if gpu.device().trace_enabled() {
                            gpu.device_mut().set_trace(false);
                        }
                        renderer.set_display_image(gpu.display_rgba8());
                        self.display_dirty = false;
                        return;
                    }
                    let display = session.runtime.services().gpu.device().display();
                    let restart = renderer.configure_game(self.settings.borrow().settings(), headset, &display);
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
    /// pad 1's keys, or an arrow key while first-person inspection shows
    /// (the page then keeps it from the browser). Ignored while the
    /// settings panel has the input.
    pub fn game_key(&mut self, code: &str, down: bool) -> bool {
        if !self.app_input.focused() { return true; }
        // The arrow keys turn inspection's view whatever the bindings say.
        let arrow = ARROWS.iter().position(|a| *a == code);
        if let Some(i) = arrow && !down {
            self.arrows[i] = false;
        }
        let editing = self.text_editor_focused();
        if editing { self.app_input.release_keyboard(); } else { self.app_input.key(code, down); }
        let settings = self.settings.borrow();
        let bindings = &settings.settings().app_bindings;
        self.app_requests.borrow_mut().extend(self.app_input.requests(bindings,settings.settings().invert_trigger_chords));
        let captured = self.app_input.captures_key(code,bindings);
        if captured {
            let game = &settings.settings().game_bindings;
            for binding in bindings.values() { for key in &binding.keyboard { self.input.key(game,key,false); } }
        }
        drop(settings);
        if editing { return true; }
        if captured { return true; }
        if down && self.panel_visible() {
            return false;
        }
        if let Some(i) = arrow {
            self.arrows[i] = down;
        }
        let game = self.settings.borrow().settings().game_bindings.clone();
        self.input.key(&game, code, down) || (arrow.is_some() && self.first_person())
    }

    /// The first gamepad's buttons and axes (Gamepad API standard mapping,
    /// 1 = pressed). Back+Start toggles Settings.
    pub fn gamepad(&mut self, pressed: &[u8], axes: &[f32]) {
        // As they are, for first-person inspection's toggle (Back alone).
        self.pad = std::array::from_fn(|i| pressed.get(i).copied().unwrap_or(0));
        self.axes = std::array::from_fn(|i| axes.get(i).copied().unwrap_or(0.0));
        if self.app_input.pad(pressed) {
            self.set_panel_visible(!self.panel_visible());
        }
        if self.text_editor_focused() { self.app_input.release_keyboard(); }
        let settings = self.settings.borrow();
        let bindings = &settings.settings().app_bindings;
        self.app_requests.borrow_mut().extend(self.app_input.requests(bindings,settings.settings().invert_trigger_chords));
        self.input.gamepad(&self.app_input.gamepad_for_game(bindings,settings.settings().invert_trigger_chords), axes);
    }

    /// Release the player's keys and buttons (the page lost focus).
    pub fn release_input(&mut self) {
        self.input.clear();
        self.deck.release();
        self.arrows = [false; 4];
        self.look_toggle.clear();
        self.snap.clear();
        self.look_mouse = [0.0; 2];
        let _ = self.app_input.set_focused(false);
        if let Some(panel) = self.renderer.as_ref().and_then(|r| r.panel()) { panel.cancel_pointer(); }
        self.xr_pointer.release();
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

    /// The app operations asked for since the last call, for the page to
    /// carry out; Toggle first-person view (its key, its chord) stays for the
    /// next step's toggles ([`XemApp::look`]).
    pub fn take_app_requests(&mut self) -> String {
        let mut requests = std::mem::take(&mut *self.app_requests.borrow_mut());
        let asked = requests.len();
        requests.retain(|request| *request != AppRequest::Action(xem_settings::AppAction::ToggleFirstPerson));
        self.look_asked |= requests.len() != asked;
        to_json(&requests)
    }
    /// Save-state feedback after the host completes storage or restoration.
    pub fn state_notice(&mut self, saved: bool, failure: Option<String>) {
        if let Some(hud) = self.renderer.as_mut().and_then(|r| r.frame_rate.as_mut()) {
            hud.notify_state(saved,failure.as_deref(),std::time::Duration::from_secs_f64(now_ms()/1000.0));
        }
    }
    /// The panel's error bar: what a page operation's failure says, or none
    /// (`""`) once one succeeded.
    pub fn set_operation_error(&self, text: &str) {
        if let Some(panel) = self.renderer.as_ref().and_then(|r| r.panel()) { panel.set_operation_error(text); }
    }
    pub fn set_storage_status(&self, text: &str) {
        if let Some(panel) = self.renderer.as_ref().and_then(|r| r.panel()) { panel.set_storage_status(text); }
    }

    pub fn set_panel_visible(&mut self, visible: bool) {
        if let Some(renderer) = &mut self.renderer {
            if renderer.panel_visible && !visible && let Some(panel) = renderer.panel() {
                panel.cancel_pointer();
                self.xr_pointer.release();
            }
            renderer.panel_visible = visible && renderer.has_panel();
        }
    }

    pub fn panel_visible(&self) -> bool {
        self.renderer.as_ref().is_some_and(|r| r.panel_visible)
    }

    /// Whether play can hold the browser pointer now. A combo is a set of
    /// clickable commands, and the original pause screen releases it too.
    pub fn mouse_camera_active(&mut self) -> bool {
        if !self.running || self.panel_visible() || !self.app_input.focused() { return false; }
        let settings = self.settings.borrow();
        self.session.as_mut().is_some_and(|session|
            settings.settings().captures_mouse(session.runtime.context(), session.runtime.game_paused()))
    }

    pub fn set_pointer_captured(&mut self, captured: bool) {
        if captured != self.mouse_captured {
            self.mouse_captured = captured;
            self.input.mouse.clear();
        }
    }

    fn text_editor_focused(&self) -> bool {
        self.panel_visible() && self.renderer.as_ref().and_then(|r| r.panel()).is_some_and(|p| p.text_editor_focused())
    }

    /// Host pointer input over the canvas (canvas pixels); true when the
    /// panel took it, when the game took a button (`dx`, `dy`: the mouse's
    /// movement, which under the mouse camera setting turns the field camera
    /// sideways, or first-person inspection's view), or when the wheel
    /// zoomed the field camera (down pulls it back, up brings it in; `dy` in
    /// pixels, about 100 a notch; not while inspecting, which shows no
    /// pulled-back camera).
    pub fn pointer(&mut self, kind: &str, x: f32, y: f32, button: i32, dx: f32, dy: f32) -> bool {
        if self.renderer.as_mut().is_some_and(|r| r.pointer(kind, x, y, button, (dx, dy))) {
            return true;
        }
        if self.panel_visible() {
            if kind == "up" { self.input.mouse.button(button, false); }
            return false;
        }
        let screen = self.renderer.as_ref().and_then(|r| r.picture).and_then(|p| p.to_screen(x, y));
        match kind {
            "down" if button == 0 => {
                // A click on an entry of the game's interface selects it and
                // confirms it; elsewhere it is the mouse's binding.
                self.input.hover(screen);
                if self.input.click(self.targets.as_ref()) { return true; }
                return self.input.mouse.button(button, true);
            }
            "down" | "up" => return self.input.mouse.button(button, kind == "down"),
            "move" => {
                self.input.hover(screen);
                if self.mouse_captured && self.mouse_camera_active() {
                    if self.first_person() {
                        let scale = self.settings.borrow().settings().mouse_scale();
                        self.look_mouse[0] += dx * scale;
                        self.look_mouse[1] += dy * scale;
                    } else {
                        self.input.mouse.moved(dx, self.settings.borrow().settings());
                    }
                }
                return false;
            }
            // In a menu or a choice the wheel steps the cursor.
            "wheel" if self.input.wheel(dy / 100.0, self.ui) => return true,
            "wheel" if self.first_person() => return true,
            "wheel" => {}
            _ => return false,
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

    /// A finger on the canvas (`kind` down, move, up or cancel; canvas
    /// pixels): the control deck, and taps on the picture. True when it was
    /// the game's (the panel takes touches while it shows).
    pub fn touch(&mut self, kind: &str, id: u32, x: f32, y: f32, now: f64) -> bool {
        if self.panel_visible() {
            self.deck.release();
            return false;
        }
        self.touched = true;
        let time = now / 1000.0;
        use xem_assist::touch::Touched;
        match kind {
            "down" => match self.deck.down(u64::from(id), (x, y), time) {
                Touched::Settings => self.set_panel_visible(true),
                Touched::AutoAdvance => self.app_requests.borrow_mut().push(AppRequest::Action(xem_settings::AppAction::ToggleAutoAdvance)),
                Touched::FirstPerson => self.look_asked = true,
                Touched::Quantity(key) => self.quantity.push(key),
                _ => {}
            },
            "move" => self.deck.moved(u64::from(id), (x, y)),
            _ => {
                if let Touched::Tap(at) = self.deck.up(u64::from(id), (x, y), time, kind == "cancel") {
                    self.taps.push(at);
                }
            }
        }
        self.input.touch = self.deck.actions();
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
    /// Whether a held rewind steps back now (its sound plays at once).
    pub fn rewinding(&self) -> bool { self.running && self.rewind_held.iter().any(|held| *held) }

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
            let budget = if history && next.1 { xem_core::rewind::MAX_REWIND_BYTES } else { 0 };
            if self.applied_time_settings.is_none_or(|old| (old.1,old.2,old.3) != (next.1,next.2,next.3)) {
                session.configure_rewind(budget,next.2,next.3).map_err(error)?;
                if budget == 0 { self.rewind_held = [false;2]; }
            }
            self.applied_time_settings = Some(next);
        }
        Ok(())
    }

    // --- Disc import --------------------------------------------------------

    /// Start importing the user's image of `bytes` bytes (read in place, never
    /// uploaded); returns its id, which its byte ranges carry. Without a game,
    /// earlier imports that are no Xenogears disc are let go.
    pub fn open_disc(&mut self, name: String, bytes: f64) -> u32 {
        if self.session.is_none() {
            self.discs.retain(|(_, disc)| disc.verified_disc().is_some());
        }
        let id = self.next_disc;
        self.next_disc += 1;
        self.discs.push((id, disc::DiscImport::new(name, bytes as u64)));
        id
    }

    /// Progress import `id`: `[[start, end], ...]` to fetch, empty when done.
    /// A disc verified as Redump's dump joins the game's discs (without a
    /// game, the newest of each disc boots or waits for the changer; a
    /// running game's changer takes one it has not got).
    pub fn disc_poll(&mut self, id: u32) -> String {
        let Some(index) = self.discs.iter().position(|(n, _)| *n == id) else { return "[]".into() };
        let ranges = self.discs[index].1.poll();
        if ranges.is_empty() {
            self.place_disc(id);
        }
        to_json(&ranges.iter().map(|r| [r.start, r.end]).collect::<Vec<_>>())
    }

    fn place_disc(&mut self, id: u32) {
        let Some(index) = self.discs.iter().position(|(n, _)| *n == id) else { return };
        if let Some(panel) = self.renderer.as_ref().and_then(|r| r.panel()) {
            panel.set_disc_status(&disc_summary(&self.discs[index].1));
        }
        let Some(known) = self.discs[index].1.verified_disc() else { return };
        let Some(session) = self.session.as_mut() else {
            self.discs.retain(|(n, disc)| *n == id || disc.verified_disc() != Some(known));
            return;
        };
        let services = session.runtime.services();
        if !services.cd.holds(known.number()) {
            match self.discs[index].1.game_source() {
                Ok(source) => services.add_disc(known.number(), source),
                Err(e) => web_sys::console::error_1(&format!("xem: disc {}: {e}", known.number()).into()),
            }
        }
    }

    pub fn disc_fill(&mut self, id: u32, offset: f64, data: &[u8]) -> Result<(), JsValue> {
        let (_, disc) = self.discs.iter_mut().find(|(n, _)| *n == id).ok_or_else(|| error("no such disc import"))?;
        disc.fill(offset as u64, data).map_err(error)
    }

    /// Raw sectors of the newest identified image (`count` <= 64), or
    /// undefined while their bytes are fetched: fetch
    /// [`disc_missing`](Self::disc_missing), fill, retry.
    pub fn disc_read(&mut self, lba: u32, count: u32) -> Result<Option<Vec<u8>>, JsValue> {
        let (_, disc) = self.discs.last_mut().ok_or_else(|| error("no disc is open"))?;
        disc.read_sectors(lba, count).map_err(error)
    }

    /// The byte ranges `[[id, start, end], ...]` pending reads of every
    /// import wait for (the game's drive and changer among them).
    pub fn disc_missing(&mut self) -> String {
        let ranges: Vec<_> = self
            .discs
            .iter_mut()
            .flat_map(|(id, disc)| disc.take_missing().into_iter().map(move |r| (*id, r.start, r.end)))
            .collect();
        to_json(&ranges)
    }

    /// The newest import's status.
    pub fn disc_status(&self) -> String {
        to_json(&self.discs.last().map(|(_, d)| d.status()))
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
        // Disc 1 when the player has it, else disc 2; the other disc goes to
        // the drive's changer.
        let verified = |number: u8| {
            self.discs.iter().rev().map(|(_, disc)| disc).find(|disc| disc.verified_disc().is_some_and(|d| d.number() == number))
        };
        let (boot, other) = match (verified(1), verified(2)) {
            (Some(first), second) => (Some(first), second),
            (None, second) => (second, None),
        };
        let (executable, game_disc, identity, other) = if load_executable {
            let Some(disc) = boot else {
                let latest = self.discs.last().map(|(_, disc)| disc.status());
                return Err(error(match latest {
                    Some(status) if status.state == "failed" => status.error.unwrap_or_default(),
                    Some(status) if status.state == "identified" => "not a known Xenogears disc".into(),
                    _ => "no identified disc".into(),
                }));
            };
            let identification = disc.identification().expect("a verified disc is identified");
            let other = match other {
                Some(other) => Some((2, other.game_source().map_err(error)?)),
                None => None,
            };
            (
                Some(identification.executable.clone()),
                Some((disc.verified_disc().expect("verified").number(), disc.game_source().map_err(error)?)),
                Some(xem_disc::state_identity(identification)),
                other,
            )
        } else {
            (None, None, None, None)
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
        // As xem_game::boot does natively.
        let mut runtime = Runtime::new(module);
        if let Some(identity) = identity {
            runtime.identity.disc = identity;
        }
        runtime.services().stub_names = self.stub_names.clone();
        runtime.services().card.attach_storage(Box::new(self.cards.clone())).map_err(error)?;
        runtime.services().spu.set_output(true);
        if let Some(executable) = executable {
            // The player's BIOS image's Kanji font, else the substitute glyphs.
            runtime.install_kanji_font(self.bios.as_deref()).map_err(error)?;
            // The BIOS copies the executable's text to its address and jumps to pc0.
            let header = exe::parse(&executable).map_err(error)?;
            runtime.load_executable(header.text_address, exe::text(&executable, &header)).map_err(error)?;
        }
        if let Some((number, source)) = game_disc {
            runtime.services().insert_disc(number, source);
        }
        if let Some((number, source)) = other {
            runtime.services().add_disc(number, source);
        }
        self.wanted_disc = None;
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
        if !running { self.hud_forward = None; self.hud_rewind = None; }
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
    /// [`MAX_FRAMES_PER_COMMAND`]; errors come back as `{"error": ...}`. The
    /// page's own `first_person` ([`XemApp::first_person_command`]) is the
    /// view's, not the session's.
    pub fn command(&mut self, command: &str) -> String {
        let reply = match serde_json::from_str::<Value>(command) {
            Err(e) => json!({ "error": format!("bad json: {e}") }),
            Ok(command) if command.get("cmd").and_then(Value::as_str) == Some("first_person") => self.first_person_command(&command),
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

    /// `{"cmd": "first_person", "on": bool, "yaw": degrees, "pitch":
    /// degrees}`, each optional: the view on, off or toggled as the app's
    /// Toggle first-person view toggles it (anywhere in a field but its
    /// menu, under Ex Machina), then turned (`xem_settings::look::Command`),
    /// answering `{first_person, look}`; the session takes its state with
    /// the next batch of frames the page runs (`status().session.inspecting`).
    fn first_person_command(&mut self, command: &Value) -> Value {
        let field = self.session.as_mut().map(|s| s.runtime.field_controls()).unwrap_or_default();
        let setting = self.settings.borrow().settings().graphics.renderer;
        let Some(renderer) = self.renderer.as_mut() else { return json!({ "error": "no renderer: the page shows no game" }) };
        let was = renderer.first_person();
        let reply = xem_settings::look::Command::parse(command).and_then(|command| first_person(renderer, &command, field, setting));
        if renderer.first_person() != was {
            self.input.mouse.clear();
            self.look_mouse = [0.0; 2];
        }
        reply.unwrap_or_else(|error| json!({ "error": error }))
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
    /// fresh formatted card; the page sets both before a session boots, and
    /// one IndexedDB took since (an import), which a session's device takes
    /// too, between frames: refused, nothing changed, while the game has a
    /// file open on that card or is busy with it (`Card::change`).
    pub fn set_card_image(&mut self, slot: usize, image: Option<Vec<u8>>) -> Result<(), JsValue> {
        if slot > 1 {
            return Err(error("cards are 0 and 1"));
        }
        if image.as_ref().is_some_and(|i| i.len() != xem_core::devices::card::CARD_SIZE) {
            return Err(error("memory card image must be exactly 128 KiB (.mcd)"));
        }
        if let Some(session) = self.session.as_mut() {
            // The live device too: its next in-game save must use the
            // imported image rather than replace it with the old session card.
            let services = session.runtime.services();
            let card = image.clone().unwrap_or_else(xem_core::devices::card::formatted);
            services.card.change(slot, card, services.clock.now).map_err(error)?;
        }
        // IndexedDB holds it: not a write to make.
        self.cards.set_image(slot, image);
        Ok(())
    }

    /// What a card or save file the player picked holds, for card `slot`:
    /// `{image, message}`, its raw image (a whole card, or a fresh card
    /// holding a single save; `file_name` names a raw single save) and what
    /// was found ("Card 1: DexDrive card, 3 saves"). Refused, naming why, for a file no container holds, a card
    /// whose directory is damaged, and while a session's game has a file
    /// open on that card or is busy with it. Nothing changes: the page writes
    /// the image to IndexedDB, then sets it ([`set_card_image`](Self::set_card_image)).
    pub fn read_card_file(&mut self, slot: usize, bytes: &[u8], file_name: &str) -> Result<js_sys::Object, JsValue> {
        let (image, message) = match self.session.as_mut() {
            Some(session) => {
                let services = session.runtime.services();
                services.card.read_file(slot, bytes, file_name, services.clock.now)
            }
            None => xem_core::devices::card::Card::default().read_file(slot, bytes, file_name, 0),
        }
        .map_err(error)?;
        let found = js_sys::Object::new();
        js_sys::Reflect::set(&found, &"image".into(), &js_sys::Uint8Array::from(&image[..]))?;
        js_sys::Reflect::set(&found, &"message".into(), &message.into())?;
        Ok(found)
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

    // --- BIOS ---------------------------------------------------------------

    /// Why `bytes` are not a BIOS image whose Kanji font save titles can be
    /// drawn from (`xem_core::bios::check`), if they are not. The page checks
    /// a picked file before IndexedDB takes it.
    pub fn check_bios(&self, bytes: &[u8]) -> Result<(), JsValue> {
        bios::check(bytes).map_err(error)
    }

    /// Keep the player's BIOS image for every boot's Kanji font and give it
    /// to a running game now (`Runtime::install_kanji_font`; its next save
    /// title is drawn from it). Refused, nothing changed, for a file that is
    /// not one. The page sets the one IndexedDB holds before a session boots.
    pub fn set_bios(&mut self, bytes: Vec<u8>) -> Result<(), JsValue> {
        bios::check(&bytes).map_err(error)?;
        // A session on a module test fixture has no font to replace.
        if let Some(session) = self.session.as_mut()
            && session.runtime.services().kanji_font.is_some()
        {
            session.runtime.install_kanji_font(Some(&bytes)).map_err(error)?;
        }
        self.bios = Some(bytes);
        Ok(())
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
                "hud": r.frame_rate.as_ref().map(|hud| json!({"visible":hud.visible(),"notice":hud.notice(),"fast_forward":hud.component().get_fast_forward(),"rewinding":hud.component().get_rewinding()})),
                // Where the game's picture lies on the canvas (canvas pixels)
                // and the screen it shows (with the wider framing's sides,
                // none for a rebuilt movie frame).
                "picture": r.picture.map(|p| json!({"x": p.placed.x, "y": p.placed.y, "width": p.placed.width, "height": p.placed.height, "display": [p.display.0, p.display.1], "side": p.side})),
                // The last frame showed a movie frame rebuilt from what the
                // MDEC decoded (the Enhanced movies setting).
                "movie": r.movie,
                // First-person inspection, and how far its view has turned
                // from the heading it began with (radians, yaw right, pitch up).
                "firstPerson": r.first_person(),
                "look": r.look_angles().map(|(yaw, pitch)| json!({"yaw": yaw, "pitch": pitch})),
                "lost": r.lost(),
            })
        });
        let settings_revision = self.settings.borrow().revision();
        let card_errors = self.session.as_mut().map(|s| s.runtime.services().card.errors.clone()).unwrap_or_default();
        let drive = self.session.as_mut().map(|s| {
            let cd = &s.runtime.services().cd;
            json!({ "disc": cd.inserted(), "wanted": cd.wanted() })
        });
        to_json(&json!({
            "renderer": renderer,
            "disc": self.discs.last().map(|(_, d)| d.status()),
            "discs": self.discs.iter().filter_map(|(_, d)| d.verified_disc().map(|disc| disc.number())).collect::<Vec<_>>(),
            "drive": drive,
            "session": session,
            "gameMemoryBytes": self.memory_bytes,
            "running": self.running,
            // The assisted controls: the touch deck (canvas pixels, what
            // fingers hold) and a headset's gestures and pauses.
            "touch": {
                "touched": self.touched,
                "controls": self.deck.controls.iter().map(|(c, r)| json!({
                    "control": format!("{c:?}"), "x": r.x, "y": r.y, "width": r.width, "height": r.height,
                })).collect::<Vec<_>>(),
                "held": self.deck.held().iter().map(|c| format!("{c:?}")).collect::<Vec<_>>(),
            },
            "xr": {
                "input": self.xr_input.is_some(),
                "gestures": self.xr_gestures,
                "counts": self.xr_counts,
                // The keys beside the screen as last placed: each one's
                // centre in the session's space and its width (metres).
                "keys": self.renderer.as_ref().map_or_else(Vec::new, |r| r.overlay.room.iter()
                    .filter(|p| matches!(p.widget, xem_render::glass::Widget::Key(..)))
                    .map(|p| { let at = p.pose.w_axis; [at.x, at.y, at.z, p.size.x] }).collect()),
                "pointer": self.headset.pointer,
                "pauses": self.headset.pauses,
                "settings": self.headset.settings_open,
                // Two-hand scaling: the screen's scale (for the session), the
                // diorama's as the frames show it (the hands' while they scale
                // it, else the setting's), whether it is under way, and the
                // scales it committed to the setting.
                "scale": self.xr_scaling.scale,
                "dioramaScale": self.xr_scaling.diorama_scale().unwrap_or(f32::from(self.settings.borrow().settings().graphics.xr.diorama_scale) / 100.0),
                "scaling": self.xr_scaling.scaling(),
                "scaleCommits": self.xr_scale_commits,
                // A key's width beside the screen or the diorama's pane (metres).
                "keyWidth": self.xr_screen.map(|s| xem_assist::draw::side_keys(&s, 1)[0].1.size.0),
                // The session's environment blend mode, and whether the real
                // room shows around what the frames draw.
                "blend": self.xr_blend,
                "room": self.renderer.as_ref().is_some_and(|r| r.xr_room),
                // The virtual screen as the last immersive frame drew it: its
                // size (metres), the display pixels it shows and the wider
                // framing's sides (display pixels each side; none for a
                // rebuilt movie frame).
                "screen": self.xr_screen.map(|s| json!({"size": [s.quad.size.0, s.quad.size.1], "display": [s.display.0, s.display.1], "side": s.side})),
                // First-person inspection on, and the last immersive frame
                // drawn from within the scene through each view.
                "inspecting": self.first_person(),
                "within": self.renderer.as_ref().is_some_and(|r| r.xr_within),
                // The diorama: whether the last immersive frame showed it, the
                // screen's share of the view (1 the screen alone), and where it
                // lay: the room from the game's world (column major), metres
                // per world unit, the pane's centre, the camera's target (world
                // units) and where it stands in the room (the table's anchor);
                // the seat (its place and turn, radians to the left) and how
                // often the runtime reset the reference space and the player
                // recentred.
                "diorama": self.renderer.as_ref().is_some_and(|r| r.xr_space.is_some()),
                "share": self.renderer.as_ref().map_or(1.0, |r| r.xr_share),
                "space": self.renderer.as_ref().and_then(|r| r.xr_space).map(|space| json!({
                    "roomFromWorld": space.room_from_world.to_cols_array(),
                    "scale": space.scale,
                    "pane": space.pane.w_axis.truncate().to_array(),
                    "target": space.camera.target,
                    "anchor": space.room_from_world.transform_point3(xem_render::glam::Vec3::from(space.camera.target)).to_array(),
                })),
                "seat": self.renderer.as_ref().map(|r| {
                    let (at, forward) = (r.xr_seat.position(), r.xr_seat.orientation() * xem_render::glam::Vec3::NEG_Z);
                    json!({"position": at.to_array(), "yaw": (-forward.x).atan2(-forward.z)})
                }),
                "resets": self.xr_resets,
                "recenters": self.xr_recenters,
                "panelHit": self.xr_panel_hit,
                "panelClick": self.xr_panel_click,
                // The panel's logical size (where its controls are), and
                // where it stands in the session: its centre, orientation
                // (x, y, z, w) and size (metres).
                "panelSize": self.renderer.as_ref().and_then(|r| r.panel()).map(|p| { let (w, h) = p.size(); [w as f32 / p.scale_factor(), h as f32 / p.scale_factor()] }),
                "panel": self.renderer.as_ref().and_then(|r| Some((r.xr_seat, r.panel()?, self.xr_screen?))).map(|(seat, p, screen)| {
                    let quad = render::xr_panel_quad(&seat, &self.settings.borrow(), p.size(), screen.quad.size, screen.diorama.is_some());
                    json!({"position": quad.pose.position.to_array(), "orientation": quad.pose.orientation.to_array(), "size": [quad.size.0, quad.size.1]})
                }),
                "rays": self.xr_input.as_ref().map(|i| i.rays.map(|r| r.map(|r| { let f = r.forward(); [r.position.x, r.position.y, r.position.z, f.x, f.y, f.z] }))),
            },
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
            // Whether a BIOS image gives boots their Kanji font.
            "bios": self.bios.is_some(),
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

/// What the panel's Game disc card says of an import.
fn disc_summary(disc: &disc::DiscImport) -> String {
    let status = disc.status();
    match (status.state, status.identity) {
        ("failed", _) => format!("{}: {}", status.name, status.error.unwrap_or_default()),
        ("identified", Some(identity)) => match (identity.disc, identity.serial) {
            (Some(number), Some(serial)) => format!("Xenogears disc {number} ({serial})"),
            _ => format!("{}: not a Xenogears disc", status.name),
        },
        (state, _) => format!("{}: {state}", status.name),
    }
}
