//! The Slint settings panel as a custom Slint platform under the host loop.
//!
//! There is no Slint event loop and no Slint window: the host forwards its input
//! as [`WindowEvent`]s, drives timers and animations from its frame loop and gets
//! the panel as a wgpu texture rendered on the host's own device and queue (when
//! Slint asks for a redraw). Panel edits become [`SettingChange`]s applied
//! through [`SettingsService::apply`]; the panel shows the acknowledged values.

use slint::platform::{Platform, Renderer, WindowAdapter};
use slint::{ComponentHandle, Model, PhysicalSize, PlatformError};
use std::cell::{Cell, RefCell};
use std::rc::{Rc, Weak};
use std::time::Duration;
use xem_settings::{
    APP_ACTIONS, AppAction, AppBinding, AppRequest, AudioOutput, DioramaTurns, EffectLevels, FAST_FORWARD_RATES, Fidelity, Filter, Graphics,
    GraphicsProfile, HeadsetView, QualityTier, RandomBattles, Rejected, Scale, SettingChange, Settings, SettingsService,
    GAME_ACTION_ROWS, GameBinding, TextSpeed, EncounterRate, Language,
};

// Slint compiles its FemtoVG renderer module out on Android; the renderer
// crate itself builds there.
#[cfg(target_os = "android")]
use i_slint_renderer_femtovg::FemtoVGWGPURenderer;
#[cfg(not(target_os = "android"))]
use slint::platform::femtovg_renderer::FemtoVGWGPURenderer;

pub use slint::platform::{Key, PointerEventButton, WindowEvent};
pub use slint::{LogicalPosition, LogicalSize, SharedString};

slint::include_modules!();

mod gamepad;

/// The app's version (a release's, else the workspace's), as About shows it.
pub const VERSION: &str = match option_env!("XEM_RELEASE_VERSION") {
    Some(version) => version,
    None => env!("CARGO_PKG_VERSION"),
};

/// The panel texture's format. It holds sRGB-encoded values: composite it with
/// `srgb_encoded` set.
pub const PANEL_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8Unorm;

/// The wgpu objects the panel renders with: the host's, shared.
#[derive(Clone)]
pub struct SharedGpu {
    pub instance: wgpu::Instance,
    pub device: wgpu::Device,
    pub queue: wgpu::Queue,
}

thread_local! {
    // The device the next window adapter renders with, and that adapter once
    // Slint has created it for a new component.
    static NEXT_GPU: RefCell<Option<SharedGpu>> = const { RefCell::new(None) };
    static CREATED: RefCell<Option<Rc<PanelWindow>>> = const { RefCell::new(None) };
}

/// Slint's platform on the host thread: it only creates texture-backed windows.
struct HostPlatform;

fn provide_gpu(gpu: SharedGpu) -> Result<(), PlatformError> {
    NEXT_GPU.set(Some(gpu));
    match slint::platform::set_platform(Box::new(HostPlatform)) {
        Ok(()) | Err(slint::platform::SetPlatformError::AlreadySet) => Ok(()),
        Err(error) => Err(PlatformError::Other(error.to_string())),
    }
}

impl Platform for HostPlatform {
    fn create_window_adapter(&self) -> Result<Rc<dyn WindowAdapter>, PlatformError> {
        let gpu = NEXT_GPU
            .with_borrow(Clone::clone)
            .ok_or_else(|| PlatformError::Other("no GPU for the Slint window".into()))?;
        let renderer = FemtoVGWGPURenderer::new(gpu.instance, gpu.device, gpu.queue)?;
        let window = Rc::new_cyclic(|adapter: &Weak<PanelWindow>| PanelWindow {
            window: slint::Window::new(adapter.clone() as Weak<dyn WindowAdapter>),
            renderer,
            size: Cell::new(PhysicalSize::new(1, 1)),
            redraw: Cell::new(true),
        });
        CREATED.set(Some(window.clone()));
        Ok(window)
    }
}

struct PanelWindow {
    window: slint::Window,
    renderer: FemtoVGWGPURenderer,
    size: Cell<PhysicalSize>,
    redraw: Cell<bool>,
}

impl WindowAdapter for PanelWindow {
    fn window(&self) -> &slint::Window {
        &self.window
    }

    fn size(&self) -> PhysicalSize {
        self.size.get()
    }

    fn renderer(&self) -> &dyn Renderer {
        &self.renderer
    }

    fn request_redraw(&self) {
        self.redraw.set(true);
    }
}

/// Whether the player is signed in to RetroAchievements (the panel's
/// Achievements section).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum AchievementsAccount {
    #[default]
    SignedOut,
    SigningIn,
    SignedIn,
}

/// What the Achievements section shows of the host's RetroAchievements
/// client ([`SettingsUi::set_achievements`]).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct AchievementsView {
    pub account: AchievementsAccount,
    /// Who is signed in and their points.
    pub user: String,
    /// What the running game's achievements do.
    pub game: String,
    /// Why the last sign-in or load failed (empty: nothing failed).
    pub error: String,
}

/// What the Achievements section asks of the host's client.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum AchievementsRequest {
    SignIn { username: String, password: String },
    SignOut,
}

/// The settings panel bound to a [`SettingsService`].
pub struct SettingsUi {
    window: Rc<PanelWindow>,
    panel: SettingsPanel,
    settings: Rc<RefCell<SettingsService>>,
    rewind_held: Rc<Cell<bool>>,
    /// The gamepad on the open panel ([`SettingsUi::gamepad`]).
    pad: Cell<gamepad::Pad>,
    device: wgpu::Device,
    texture: wgpu::Texture,
    view: wgpu::TextureView,
}

impl SettingsUi {
    /// Creates the panel at `size` physical pixels and `scale_factor` physical
    /// pixels per logical pixel. Installs the custom Slint platform on this
    /// thread the first time; all panels of a thread must use the same device.
    pub fn new(
        gpu: SharedGpu,
        settings: Rc<RefCell<SettingsService>>,
        size: (u32, u32),
        scale_factor: f32,
    ) -> Result<Self, PlatformError> {
        let device = gpu.device.clone();
        provide_gpu(gpu)?;
        let panel = SettingsPanel::new()?;
        let window = CREATED
            .take()
            .ok_or_else(|| PlatformError::Other("Slint created no window".into()))?;
        bind(&panel, &settings);
        let (texture, view) = panel_texture(&device, size);
        let ui = Self {
            window,
            panel,
            settings,
            rewind_held: Rc::new(Cell::new(false)),
            pad: Cell::default(),
            device,
            texture,
            view,
        };
        ui.resize_window(size, scale_factor);
        ui.panel.show()?;
        Ok(ui)
    }

    fn resize_window(&self, size: (u32, u32), scale_factor: f32) {
        self.panel
            .set_wide_layout(size.0 as f32 / scale_factor >= 560.0);
        self.window.size.set(PhysicalSize::new(size.0, size.1));
        let window = &self.window.window;
        window.dispatch_event(WindowEvent::ScaleFactorChanged { scale_factor });
        window.dispatch_event(WindowEvent::Resized {
            size: PhysicalSize::new(size.0, size.1).to_logical(scale_factor),
        });
        self.window.redraw.set(true);
    }

    /// Changes the panel's physical size and scale factor.
    pub fn resize(&mut self, size: (u32, u32), scale_factor: f32) {
        if size != self.size() {
            (self.texture, self.view) = panel_texture(&self.device, size);
        }
        self.resize_window(size, scale_factor);
    }

    pub fn size(&self) -> (u32, u32) {
        let size = self.window.size.get();
        (size.width, size.height)
    }

    pub fn scale_factor(&self) -> f32 {
        self.window.window.scale_factor()
    }

    /// Forwards host input. Positions are logical pixels from the panel's top left.
    pub fn dispatch(&self, event: WindowEvent) {
        // Keys show where the focus is; the pointer's press hides it again.
        match event {
            WindowEvent::PointerPressed { .. } => self.panel.global::<PadFocus>().set_shown(false),
            WindowEvent::KeyPressed { .. } | WindowEvent::KeyPressRepeated { .. } => self.panel.global::<PadFocus>().set_shown(true),
            _ => {}
        }
        self.window.window.dispatch_event(event);
        // Publish held-control transitions before the host's next game frame.
        slint::platform::update_timers_and_animations();
    }

    /// Advances Slint's timers and animations to now and, if Slint asked for a
    /// redraw, renders the panel into its texture. Returns whether it rendered.
    pub fn update(&mut self) -> Result<bool, PlatformError> {
        slint::platform::update_timers_and_animations();
        if !self.window.redraw.take() {
            return Ok(false);
        }
        self.window.renderer.render_to_texture(&self.texture)?;
        if self.window.window.has_active_animations() {
            self.window.redraw.set(true);
        }
        Ok(true)
    }

    /// The rendered panel, premultiplied alpha, [`PANEL_FORMAT`].
    pub fn texture(&self) -> &wgpu::Texture {
        &self.texture
    }

    pub fn view(&self) -> &wgpu::TextureView {
        &self.view
    }

    /// Called when the panel's Close button is pressed.
    pub fn on_close(&self, f: impl FnMut() + 'static) {
        self.panel.on_close_requested(f);
    }

    /// Shows the Disc group; `f` is called when its Import button is pressed.
    pub fn on_import_disc(&self, f: impl FnMut() + 'static) {
        self.panel.set_can_import_disc(true);
        self.panel.on_import_disc_requested(f);
    }

    /// Shows Quit in the header; `f` is called once the player confirms it.
    pub fn on_quit(&self, f: impl FnMut() + 'static) {
        self.panel.set_can_quit(true);
        self.panel.on_quit_requested(f);
    }

    /// Shows Reset in the header; hosts route it to the console reset command.
    pub fn on_reset(&self, f: impl FnMut() + 'static) {
        self.panel.set_can_reset(true);
        self.panel.on_reset_requested(f);
    }

    /// Escape or Back on the open panel, and the panel hidden: closes the
    /// quit confirmation, saying whether it showed (then the panel stays).
    pub fn cancel_dialog(&self) -> bool {
        let shown = self.panel.get_quit_confirming();
        self.panel.set_quit_confirming(false);
        shown
    }

    /// Whether this host places its own window (Graphics offers Fullscreen).
    pub fn set_fullscreen_offered(&self, offered: bool) {
        self.panel.set_can_fullscreen(offered);
    }

    /// The host keeps saves in a browser's storage: Saves and Shortcuts offer
    /// Request browser persistence, which means nothing to a native host.
    pub fn set_browser_storage(&self, offered: bool) {
        self.panel.set_browser_storage(offered);
        set_backup_actions(&self.panel);
        show_binding(&self.panel, self.settings.borrow().settings());
    }

    /// The game waits for disc `number`, which the drive's changer has not
    /// got: the error bar asks for it over the Saves section (where a host
    /// that imports discs has its button).
    pub fn ask_for_disc(&self, number: u8) {
        let text = if self.panel.get_can_import_disc() {
            format!("The game asks for disc {number}: import it below to go on.")
        } else {
            format!(
                "The game asks for disc {number}. Put its image (a CHD, a cue sheet or a Redump-verified ISO) \
                 beside the one the game started from, then start again."
            )
        };
        self.panel.set_error(text.into());
        self.select_section(SAVES_SECTION);
    }

    /// Whether the host has a RetroAchievements client (the native hosts):
    /// the Achievements section offers sign-in, else says why a browser
    /// cannot reach the server.
    pub fn set_achievements_offered(&self, offered: bool) {
        self.panel.set_ra_offered(offered);
    }

    /// What the host's RetroAchievements client says now.
    pub fn set_achievements(&self, view: &AchievementsView) {
        let panel = &self.panel;
        panel.set_ra_account(match view.account {
            AchievementsAccount::SignedOut => 0,
            AchievementsAccount::SigningIn => 1,
            AchievementsAccount::SignedIn => 2,
        });
        for (current, next, set) in [
            (panel.get_ra_user(), &view.user, SettingsPanel::set_ra_user as fn(&SettingsPanel, SharedString)),
            (panel.get_ra_game(), &view.game, SettingsPanel::set_ra_game),
            (panel.get_ra_error(), &view.error, SettingsPanel::set_ra_error),
        ] {
            if current.as_str() != next {
                set(panel, next.as_str().into());
            }
        }
    }

    /// The Achievements section's Sign in (with the name and password the
    /// player typed; the password field empties at once) and Sign out.
    pub fn on_achievements_request(&self, f: impl FnMut(AchievementsRequest) + 'static) {
        let f = Rc::new(RefCell::new(f));
        let panel = self.panel.as_weak();
        self.panel.on_ra_sign_in_requested({
            let f = f.clone();
            move |username, password| {
                if let Some(panel) = panel.upgrade() {
                    panel.set_ra_password("".into());
                }
                f.borrow_mut()(AchievementsRequest::SignIn { username: username.into(), password: password.into() });
            }
        });
        self.panel.on_ra_sign_out_requested(move || f.borrow_mut()(AchievementsRequest::SignOut));
    }

    /// What the Disc group says about the imported disc.
    pub fn set_disc_status(&self, text: &str) {
        self.panel.set_disc_status(text.into());
    }

    pub fn on_app_request(&self, f: impl FnMut(AppRequest) + 'static) {
        let f = Rc::new(RefCell::new(f));
        self.panel.on_debug_requested({
            let f = f.clone();
            let panel = self.panel.as_weak();
            move |tool| {
                let Some(panel) = panel.upgrade() else { return; };
                if !panel.get_debug_available() || panel.get_debug_busy() { return; }
                let request = debug_request(&panel, tool);
                match request {
                    Ok(request) => f.borrow_mut()(AppRequest::Debug(request)),
                    Err(error) => panel.set_error(error.into()),
                }
            }
        });
        self.on_reset({
            let f = f.clone();
            move || f.borrow_mut()(AppRequest::Action(AppAction::Reset))
        });
        self.panel.on_xr_recenter_requested({
            let f = f.clone();
            move || f.borrow_mut()(AppRequest::Action(AppAction::Recenter))
        });
        self.panel.on_app_action_requested({
            let f = f.clone();
            let panel = self.panel.as_weak();
            move |index| {
                let browser = panel.upgrade().is_some_and(|panel| panel.get_browser_storage());
                if let Some((action, _)) = usize::try_from(index).ok().and_then(|i| offered_actions(browser).nth(i)) {
                    f.borrow_mut()(AppRequest::Action(action));
                }
            }
        });
        self.panel.on_backup_action_requested({
            let f = f.clone();
            let panel = self.panel.as_weak();
            move |index| {
                let browser = panel.upgrade().is_some_and(|panel| panel.get_browser_storage());
                if let Some((action, _)) = usize::try_from(index).ok().and_then(|i| backup_actions(browser).nth(i)) {
                    f.borrow_mut()(AppRequest::Action(action));
                }
            }
        });
        let rewind_held = self.rewind_held.clone();
        self.panel.on_rewind_held(move |held| {
            if rewind_held.replace(held) != held {
                f.borrow_mut()(AppRequest::UiRewind(held));
            }
        });
    }

    pub fn set_time_status(&self, fast_forward: Option<bool>, text: &str) {
        self.panel.set_fast_forward(fast_forward.unwrap_or(false));
        self.panel.set_time_status(text.into());
    }

    pub fn set_debug_status(&self, available: bool, busy: bool, text: &str) {
        self.panel.set_debug_available(available);
        self.panel.set_debug_busy(busy);
        self.panel.set_debug_status(text.into());
    }

    pub fn set_storage_status(&self, text: &str) {
        self.panel.set_storage_status(text.into());
    }

    /// An operation succeeded: the error bar of an earlier failure goes.
    pub fn clear_operation_error(&self) {
        self.panel.set_error("".into());
    }

    /// The error bar: a problem (an operation failed, the game stopped).
    pub fn set_operation_error(&self, text: &str) {
        self.panel.set_error(text.into());
    }

    /// Whether this host offers first-person inspection (the Controls
    /// section lists its keys and buttons).
    /// The Japanese release's discs are beside the game's: the panel offers
    /// Japanese text and the movies' and voices' language.
    pub fn set_japanese_offered(&self, offered: bool, settings: &Settings) {
        if self.panel.get_japanese_offered() != offered {
            self.panel.set_japanese_offered(offered);
            set_languages(&self.panel);
            show(&self.panel, settings);
        }
    }

    pub fn set_first_person(&self, offered: bool) {
        self.panel.set_first_person(offered);
    }

    /// Whether the host's touch controls show: the Controls section lists a
    /// finger dragged on the picture among inspection's looks (the touch
    /// deck's SELECT is Select's).
    pub fn set_first_person_touch(&self, touch: bool) {
        self.panel.set_first_person_touch(touch);
    }

    /// Whether the host plays in a headset: the Controls section lists the
    /// underhand grab and the controllers' Back (their right stick's click)
    /// as inspection's toggles, their right stick as its look, and that Back
    /// held as what hides a dialogue (elsewhere Select).
    pub fn set_first_person_headset(&self, headset: bool) {
        self.panel.set_first_person_headset(headset);
    }

    /// Whether the panel shows in a headset: Graphics' headset card offers
    /// Recenter (the view placed in front of the head again).
    pub fn set_in_headset(&self, headset: bool) {
        self.panel.set_in_headset(headset);
    }

    /// Whether the headset offers passthrough: the diorama's Passthrough
    /// row shows.
    pub fn set_passthrough_offered(&self, offered: bool) {
        self.panel.set_xr_passthrough_offered(offered);
    }
    pub fn text_editor_focused(&self) -> bool {
        self.panel.get_text_editor_focused()
    }

    /// A hidden panel must release a held pointer control even when its next
    /// mouse-up belongs to the game or another window, and what the gamepad
    /// holds on it ([`SettingsUi::gamepad`]).
    pub fn cancel_pointer(&self) {
        self.release_gamepad();
        if self.rewind_held.get() {
            self.panel.invoke_rewind_held(false);
        }
        self.dispatch(WindowEvent::PointerReleased {
            position: LogicalPosition::new(-1.0, -1.0),
            button: PointerEventButton::Left,
        });
    }

    /// The Slint component, for inspection.
    pub fn component(&self) -> &SettingsPanel {
        &self.panel
    }
}

fn debug_request(panel: &SettingsPanel, tool: i32) -> Result<xem_settings::DebugRequest, String> {
    use xem_settings::DebugRequest;
    let byte = |n| u8::try_from(n).map_err(|_| "Debug value must be between 0 and 255.".to_string());
    let word = |n| u16::try_from(n).map_err(|_| "Debug value must be between 0 and 65535.".to_string());
    let request = match tool {
        0 => DebugRequest::Field { map: word(panel.get_debug_map())?, entry: word(panel.get_debug_entry())? },
        1 => {
            let (party, gears) = (panel.get_debug_party(), panel.get_debug_gears());
            let mut members = [11; 3];
            let mut modes = [0; 3];
            for i in 0..3 {
                members[i] = byte(party.row_data(i).ok_or("Missing party slot.")?)?;
                modes[i] = byte(gears.row_data(i).ok_or("Missing Gear slot.")?)?;
            }
            DebugRequest::Battle { formation: byte(panel.get_debug_formation())?, file: byte(panel.get_debug_battle_file())?, party: members, gears: modes }
        }
        2 => DebugRequest::Worldmap,
        3 => DebugRequest::Battling,
        4 => DebugRequest::Menu { screen: byte(panel.get_debug_menu())?, parameter: byte(panel.get_debug_parameter())? },
        5 => DebugRequest::Movie { movie: byte(panel.get_debug_movie())?, kind: byte(panel.get_debug_movie_kind())? },
        6 => DebugRequest::Field { map: 0, entry: 0 },
        _ => return Err("Unknown debug operation.".into()),
    };
    request.validate()?;
    Ok(request)
}

fn panel_texture(
    device: &wgpu::Device,
    (width, height): (u32, u32),
) -> (wgpu::Texture, wgpu::TextureView) {
    let texture = device.create_texture(&wgpu::TextureDescriptor {
        label: Some("settings panel"),
        size: wgpu::Extent3d {
            width: width.max(1),
            height: height.max(1),
            depth_or_array_layers: 1,
        },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format: PANEL_FORMAT,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT
            | wgpu::TextureUsages::TEXTURE_BINDING
            | wgpu::TextureUsages::COPY_SRC,
        view_formats: &[],
    });
    let view = texture.create_view(&Default::default());
    (texture, view)
}

const AUDIO_OUTPUTS: [AudioOutput; 3] = [AudioOutput::Mono, AudioOutput::Stereo, AudioOutput::Wide];
const SCALES: [Scale; 3] = [Scale::Fit, Scale::Integer, Scale::Stretch];
const FILTERS: [Filter; 2] = [Filter::Nearest, Filter::Linear];
const MOVIE_FILTERS: [xem_settings::MovieFilter; 2] = [xem_settings::MovieFilter::Enhanced, xem_settings::MovieFilter::Original];

const RENDERERS: [xem_settings::Renderer; 2] = [xem_settings::Renderer::Original, xem_settings::Renderer::ExMachina];
const PROFILES: [GraphicsProfile; 4] = [GraphicsProfile::Modern, GraphicsProfile::Hd2d, GraphicsProfile::Ps1, GraphicsProfile::Custom];
const QUALITIES: [QualityTier; 5] = [QualityTier::Auto, QualityTier::Low, QualityTier::Medium, QualityTier::High, QualityTier::Ultra];

fn fidelity_flags(f: &Fidelity) -> [bool; 6] {
    [f.affine_textures, f.vertex_snapping, f.color_15bit, f.dithering, f.native_resolution, f.painter_ordering]
}

/// The effect levels the panel edits: the selected profile's own.
fn edited_levels(graphics: &Graphics) -> EffectLevels {
    match graphics.profile {
        GraphicsProfile::Custom => graphics.custom_effects,
        _ => graphics.hd2d,
    }
}

/// The graphics settings with one panel edit applied (`None`: not a choice).
fn graphics_edit(graphics: &Graphics, key: &str, value: i32) -> Option<Graphics> {
    let mut next = *graphics;
    let index = usize::try_from(value).ok();
    let filter = || index.and_then(|i| FILTERS.get(i)).copied();
    match key {
        "renderer" => next.renderer = *RENDERERS.get(index?)?,
        "profile" => next.profile = *PROFILES.get(index?)?,
        "profile-reset" => next = next.profile_reset(),
        "scale" => next.internal_scale = u8::try_from(value).ok()?,
        "quality" => next.quality = *QUALITIES.get(index?)?,
        "interpolation" => next.interpolation = value != 0,
        "widescreen" => next.widescreen = value != 0,
        "force-4x3" => next.force_4x3 = value != 0,
        "full-size-sprites" => next.full_size_sprites = value != 0,
        "filter-world" => next.filtering.world = filter()?,
        "filter-sprites" => next.filtering.sprites = filter()?,
        "filter-interface" => next.filtering.interface = filter()?,
        "filter-movies" => next.filtering.movies = *MOVIE_FILTERS.get(index?)?,
        "zoom-out" => next.zoom_out = u16::try_from(value).ok()?,
        "xr-distance" => next.xr.distance_cm = u16::try_from(value).ok()?,
        "xr-width" => next.xr.width_cm = u16::try_from(value).ok()?,
        "xr-dof" => next.xr.depth_of_field = value != 0,
        "xr-view" => next.xr.view = *HEADSET_VIEWS.get(index?)?,
        "xr-scale" => next.xr.diorama_scale = u16::try_from(value).ok()?,
        "xr-height" => next.xr.diorama_height_cm = i16::try_from(value).ok()?,
        "xr-table-distance" => next.xr.diorama_distance_cm = u16::try_from(value).ok()?,
        "xr-tilt" => next.xr.diorama_tilt_deg = u8::try_from(value).ok()?,
        "xr-turns" => next.xr.diorama_turns = *DIORAMA_TURNS.get(index?)?,
        "xr-passthrough" => next.xr.passthrough = value != 0,
        "xr-diorama-reset" => next.xr = next.xr.diorama_reset(),
        _ => return None,
    }
    Some(next)
}

const HEADSET_VIEWS: [HeadsetView; 2] = [HeadsetView::Screen, HeadsetView::Diorama];
const DIORAMA_TURNS: [DioramaTurns; 3] = [DioramaTurns::Smooth, DioramaTurns::Snap, DioramaTurns::Fixed];

/// What the headset card says of the diorama: that it leaves out the
/// profile's HD-2D effects (the screen it gives way to keeps them).
fn diorama_note(graphics: &Graphics) -> &'static str {
    if graphics.xr.view == HeadsetView::Diorama && graphics.headset_effects() {
        "The diorama does not draw the profile's HD-2D effects; the screen it gives way to keeps them."
    } else {
        ""
    }
}

/// The game text languages the panel lists: Japanese only beside the
/// Japanese discs (`SettingsPanel::japanese_offered`).
fn set_languages(panel: &SettingsPanel) {
    let offered = panel.get_japanese_offered();
    let names: Vec<slint::SharedString> = Language::ALL.iter()
        .filter(|l| offered || **l != Language::Japanese)
        .map(|l| slint::SharedString::from(l.name())).collect();
    panel.set_languages(Rc::new(slint::VecModel::from(names)).into());
}

fn index_of<T: PartialEq>(options: &[T], value: &T) -> i32 {
    options
        .iter()
        .position(|option| option == value)
        .unwrap_or(0) as i32
}

/// Shows acknowledged settings in the panel.
fn show(panel: &SettingsPanel, settings: &Settings) {
    panel.set_master_volume(f32::from(settings.master_volume));
    // The game's own modes, then Binaural (the mode it returns to stays in
    // `audio_output`).
    panel.set_audio_output(if settings.binaural.enabled { AUDIO_OUTPUTS.len() as i32 } else { index_of(&AUDIO_OUTPUTS, &settings.audio_output) });
    panel.set_binaural_music(settings.binaural.spatial_music);
    panel.set_binaural_effects(settings.binaural.positional_effects);
    panel.set_binaural_immersion(f32::from(settings.binaural.music_immersion));
    panel.set_binaural_head(settings.binaural.head_tracked_music);
    panel.set_scale(index_of(&SCALES, &settings.presentation.scale));
    panel.set_filter(index_of(&FILTERS, &settings.presentation.filter));
    panel.set_show_fps(settings.presentation.show_fps);
    panel.set_fullscreen(settings.presentation.fullscreen);
    let graphics = &settings.graphics;
    panel.set_gfx_renderer(index_of(&RENDERERS, &graphics.renderer));
    panel.set_gfx_profile(index_of(&PROFILES, &graphics.profile));
    panel.set_gfx_scale(i32::from(graphics.internal_scale));
    panel.set_gfx_quality(index_of(&QUALITIES, &graphics.quality));
    panel.set_gfx_interpolation(graphics.interpolation);
    // Under Force 4:3 the switch shows wider framing off (its choice kept).
    panel.set_gfx_widescreen(graphics.widened());
    panel.set_gfx_force_4x3(graphics.force_4x3);
    panel.set_gfx_full_size_sprites(graphics.full_size_sprites);
    panel.set_gfx_zoom_out(i32::from(graphics.zoom_out));
    panel.set_gfx_filter_world(index_of(&FILTERS, &graphics.filtering.world));
    panel.set_gfx_filter_sprites(index_of(&FILTERS, &graphics.filtering.sprites));
    panel.set_gfx_filter_interface(index_of(&FILTERS, &graphics.filtering.interface));
    panel.set_gfx_filter_movies(index_of(&MOVIE_FILTERS, &graphics.filtering.movies));
    panel.set_gfx_fidelity(slint::ModelRc::new(slint::VecModel::from(fidelity_flags(&graphics.custom_fidelity).to_vec())));
    let levels = edited_levels(graphics);
    let levels: Vec<i32> = EffectLevels::NAMES.iter().map(|name| levels.get(name).unwrap_or(0)).collect();
    panel.set_gfx_effects(slint::ModelRc::new(slint::VecModel::from(levels)));
    panel.set_gfx_xr_distance(i32::from(graphics.xr.distance_cm));
    panel.set_gfx_xr_width(i32::from(graphics.xr.width_cm));
    panel.set_gfx_xr_dof(graphics.xr.depth_of_field);
    panel.set_gfx_xr_view(index_of(&HEADSET_VIEWS, &graphics.xr.view));
    panel.set_gfx_xr_scale(i32::from(graphics.xr.diorama_scale));
    panel.set_gfx_xr_height(i32::from(graphics.xr.diorama_height_cm));
    panel.set_gfx_xr_table_distance(i32::from(graphics.xr.diorama_distance_cm));
    panel.set_gfx_xr_tilt(i32::from(graphics.xr.diorama_tilt_deg));
    panel.set_gfx_xr_turns(index_of(&DIORAMA_TURNS, &graphics.xr.diorama_turns));
    panel.set_gfx_xr_passthrough(graphics.xr.passthrough);
    panel.set_gfx_xr_note(diorama_note(graphics).into());
    panel.set_pause_on_focus_loss(settings.pause_on_focus_loss);
    panel.set_fast_forward_rate(index_of(&FAST_FORWARD_RATES, &settings.fast_forward_rate));
    panel.set_fast_forward_position(panel.get_fast_forward_rate() as f32);
    panel.set_invert_trigger_chords(settings.invert_trigger_chords);
    panel.set_mouse_camera(settings.mouse_camera);
    panel.set_mouse_sensitivity(i32::from(settings.mouse_sensitivity));
    panel.set_mouse_deadzone(i32::from(settings.mouse_deadzone));
    panel.set_run_by_default(settings.run_by_default);
    panel.set_hide_idle_cursor(settings.hide_idle_cursor);
    panel.set_random_battles(match settings.random_battles {
        RandomBattles::NotWhilePlatforming => 0,
        RandomBattles::Original => 1,
    });
    panel.set_encounter_rate(i32::from(settings.encounter_rate == EncounterRate::Half));
    panel.set_text_speed(index_of(&TextSpeed::ALL, &settings.text_speed));
    panel.set_auto_advance(settings.auto_advance.enabled);
    panel.set_auto_advance_dwell(settings.auto_advance.dwell as i32);
    panel.set_movie_skip(settings.movie_skip);
    panel.set_fast_disc(settings.fast_disc);
    // Japanese (the last language) only beside the Japanese discs; without
    // them the game shows its English.
    let language = index_of(&Language::ALL, &settings.language);
    panel.set_language(if panel.get_japanese_offered() || settings.language != Language::Japanese { language } else { 0 });
    panel.set_voices(i32::from(settings.voices == xem_settings::Voices::Japanese));
    let assisted = settings.assisted;
    panel.set_assist_gestures(assisted.gestures);
    panel.set_assist_pointer(match assisted.pointer {
        xem_settings::XrPointer::Eye => 0,
        xem_settings::XrPointer::Head => 1,
        xem_settings::XrPointer::LeftHand => 2,
        xem_settings::XrPointer::RightHand => 3,
    });
    panel.set_assist_ledge_brake(assisted.ledge_brake);
    panel.set_assist_touch(assisted.touch_controls);
    panel.set_assist_touch_opacity(f32::from(assisted.touch_opacity));
    panel.set_assist_solid(assisted.reduced_transparency);
    panel.set_assist_still(assisted.reduced_motion);
    panel.set_rewind_enabled(settings.rewind_enabled);
    panel.set_rewind_interval_seconds(settings.rewind_interval_seconds as i32);
    panel.set_rewind_interval_max(xem_settings::MAX_REWIND_INTERVAL_SECONDS as i32);
    panel.set_rewind_history_seconds(settings.rewind_history_seconds as i32);
    panel.set_rewind_history_max(xem_settings::MAX_REWIND_HISTORY_SECONDS as i32);
    show_binding(panel, settings);
    show_game_binding(panel, settings);
}

/// The Saves section's index (settings.slint's `sections`).
const SAVES_SECTION: i32 = 5;

/// Every app operation the panel offers with its bindings ([`offered_actions`]),
/// and the selected one's in its editors.
fn show_binding(panel: &SettingsPanel, settings: &Settings) {
    let browser = panel.get_browser_storage();
    let rows: Vec<BindingRow> = offered_actions(browser)
        .map(|(action, title)| {
            let binding = settings.app_bindings.get(&action).cloned().unwrap_or_default();
            let keys: Vec<&str> = binding.keyboard.iter().map(|code| key_label(code)).collect();
            BindingRow { name: title.into(), keyboard: keys.join("+").into(), gamepad: binding.gamepad.join("+").into() }
        })
        .collect();
    if let Some((action, _)) = usize::try_from(panel.get_selected_action()).ok().and_then(|i| offered_actions(browser).nth(i)) {
        let binding = settings.app_bindings.get(&action).cloned().unwrap_or_default();
        panel.set_keyboard_binding(binding.keyboard.join("+").into());
        panel.set_gamepad_binding(binding.gamepad.join("+").into());
    }
    // Changed rows in place: the list keeps its rows (and the editor its focus).
    let model = panel.get_bindings();
    if model.row_count() == rows.len() {
        for (index, row) in rows.into_iter().enumerate() {
            if model.row_data(index).as_ref() != Some(&row) {
                model.set_row_data(index, row);
            }
        }
    } else {
        panel.set_bindings(slint::ModelRc::new(slint::VecModel::from(rows)));
    }
}

/// Every game action with its inputs, in [`GAME_ACTION_ROWS`] order, and the
/// selected one's in its editors.
fn show_game_binding(panel: &SettingsPanel, settings: &Settings) {
    let rows: Vec<BindingRow> = GAME_ACTION_ROWS
        .iter()
        .map(|(context, action, title)| {
            let binding = settings.game_bindings.context(*context).get(action).cloned().unwrap_or_default();
            let mut keys: Vec<&str> = binding.keyboard.iter().map(|code| key_label(code)).collect();
            keys.extend(binding.mouse.iter().map(|m| match m.as_str() { "MouseLeft" => "Click", "MouseRight" => "Right-click", _ => "Middle-click" }));
            keys.dedup();
            BindingRow { name: (if *action == xem_settings::GameAction::Run && settings.run_by_default { "Walk (held)" } else { *title }).into(), keyboard: keys.join(" ").into(), gamepad: binding.gamepad.join(" ").into() }
        })
        .collect();
    if let Some((context, action, _)) = usize::try_from(panel.get_selected_game_action()).ok().and_then(|i| GAME_ACTION_ROWS.get(i)) {
        let binding = settings.game_bindings.context(*context).get(action).cloned().unwrap_or_default();
        panel.set_game_keys(binding.keyboard.join(" ").into());
        panel.set_game_buttons(binding.gamepad.join(" ").into());
        panel.set_game_mouse(binding.mouse.join(" ").into());
    }
    let model = panel.get_game_bindings();
    if model.row_count() == rows.len() {
        for (index, row) in rows.into_iter().enumerate() {
            if model.row_data(index).as_ref() != Some(&row) {
                model.set_row_data(index, row);
            }
        }
    } else {
        panel.set_game_bindings(slint::ModelRc::new(slint::VecModel::from(rows)));
    }
}

/// The Backups card's buttons: the backup operations the panel offers.
fn set_backup_actions(panel: &SettingsPanel) {
    let titles: Vec<slint::SharedString> = backup_actions(panel.get_browser_storage()).map(|(_, title)| title.into()).collect();
    panel.set_backup_actions(Rc::new(slint::VecModel::from(titles)).into());
}

/// Routes panel edits through the settings service and shows every
/// acknowledged change, whoever made it.
fn bind(panel: &SettingsPanel, settings: &Rc<RefCell<SettingsService>>) {
    panel.set_version(VERSION.trim_start_matches('v').into());
    set_backup_actions(panel);
    panel.set_fast_forward_rates(
        Rc::new(slint::VecModel::from(
            FAST_FORWARD_RATES
                .iter()
                .map(|rate| slint::SharedString::from(rate.to_string()))
                .collect::<Vec<_>>(),
        ))
        .into(),
    );
    set_languages(panel);
    show(panel, settings.borrow().settings());
    let weak = panel.as_weak();
    settings.borrow_mut().subscribe(move |ack| {
        if let Some(panel) = weak.upgrade() {
            show(&panel, &ack.settings);
            panel.set_error("".into());
        }
    });

    let apply = {
        let weak = panel.as_weak();
        let settings = Rc::downgrade(settings);
        move |change: Option<SettingChange>| {
            let (Some(panel), Some(settings)) = (weak.upgrade(), settings.upgrade()) else {
                return;
            };
            let Some(change) = change else { return };
            let result = settings.borrow_mut().apply(change);
            if let Err(rejected) = result {
                // The widget may show the refused value: restore the current one.
                show(&panel, settings.borrow().settings());
                panel.set_error(describe(&rejected).into());
            }
        }
    };
    panel.on_invert_trigger_chords_edited({
        let apply = apply.clone();
        move |invert| apply(Some(SettingChange::InvertTriggerChords(invert)))
    });
    panel.on_assist_edited({
        let apply = apply.clone();
        let settings = settings.clone();
        move |name, value| {
            let mut assisted = settings.borrow().settings().assisted;
            let on = value != 0;
            match name.as_str() {
                "gestures" => assisted.gestures = on,
                "pointer" => assisted.pointer = match value {
                    0 => xem_settings::XrPointer::Eye,
                    2 => xem_settings::XrPointer::LeftHand,
                    3 => xem_settings::XrPointer::RightHand,
                    _ => xem_settings::XrPointer::Head,
                },
                "ledge-brake" => assisted.ledge_brake = on,
                "touch" => assisted.touch_controls = on,
                // The slider's value, within the setting's range.
                "touch-opacity" => assisted.touch_opacity = value.clamp(i32::from(xem_settings::MIN_TOUCH_OPACITY), i32::from(xem_settings::MAX_TOUCH_OPACITY)) as u8,
                "solid" => assisted.reduced_transparency = on,
                "still" => assisted.reduced_motion = on,
                _ => return,
            }
            apply(Some(SettingChange::Assisted(assisted)))
        }
    });
    panel.on_run_by_default_edited({
        let apply = apply.clone();
        move |enabled| apply(Some(SettingChange::RunByDefault(enabled)))
    });
    panel.on_hide_idle_cursor_edited({
        let apply = apply.clone();
        move |enabled| apply(Some(SettingChange::HideIdleCursor(enabled)))
    });
    panel.on_random_battles_edited({
        let apply = apply.clone();
        move |index| {
            let battles = if index == 1 { RandomBattles::Original } else { RandomBattles::NotWhilePlatforming };
            apply(Some(SettingChange::RandomBattles(battles)))
        }
    });
    panel.on_gameplay_edited({
        let apply = apply.clone();
        let settings = settings.clone();
        move |name, value| {
            let mut auto = settings.borrow().settings().auto_advance;
            apply(match name.as_str() {
                "encounter-rate" => Some(SettingChange::EncounterRate(if value == 1 { EncounterRate::Half } else { EncounterRate::Original })),
                "text-speed" => usize::try_from(value).ok().and_then(|i| TextSpeed::ALL.get(i)).map(|speed| SettingChange::TextSpeed(*speed)),
                "auto-advance" => {
                    auto.enabled = value != 0;
                    Some(SettingChange::AutoAdvance(auto))
                }
                "auto-advance-dwell" => u32::try_from(value).ok().map(|dwell| SettingChange::AutoAdvance(xem_settings::AutoAdvance { dwell, ..auto })),
                "movie-skip" => Some(SettingChange::MovieSkip(value != 0)),
                "fast-disc" => Some(SettingChange::FastDisc(value != 0)),
                "language" => usize::try_from(value).ok().and_then(|i| Language::ALL.get(i)).map(|language| SettingChange::Language(*language)),
                "voices" => xem_settings::Voices::ALL.get(usize::try_from(value).unwrap_or(0)).map(|voices| SettingChange::Voices(*voices)),
                _ => None,
            })
        }
    });
    panel.on_mouse_camera_edited({
        let apply = apply.clone();
        move |enabled| apply(Some(SettingChange::MouseCamera(enabled)))
    });
    panel.on_mouse_sensitivity_edited({
        let apply = apply.clone();
        move |value| apply(u16::try_from(value).ok().map(SettingChange::MouseSensitivity))
    });
    panel.on_mouse_deadzone_edited({
        let apply = apply.clone();
        move |value| apply(u16::try_from(value).ok().map(SettingChange::MouseDeadzone))
    });
    panel.on_rewind_enabled_edited({
        let apply = apply.clone();
        move |enabled| apply(Some(SettingChange::RewindEnabled(enabled)))
    });
    panel.on_pause_on_focus_loss_edited({
        let apply = apply.clone();
        move |enabled| apply(Some(SettingChange::PauseOnFocusLoss(enabled)))
    });
    panel.on_rewind_interval_edited({
        let apply = apply.clone();
        move |seconds| {
            apply(
                u32::try_from(seconds)
                    .ok()
                    .map(SettingChange::RewindIntervalSeconds),
            )
        }
    });
    panel.on_rewind_history_edited({
        let apply = apply.clone();
        move |seconds| {
            apply(
                u32::try_from(seconds)
                    .ok()
                    .map(SettingChange::RewindHistorySeconds),
            )
        }
    });
    panel.on_master_volume_edited({
        let apply = apply.clone();
        move |volume| apply(Some(SettingChange::MasterVolume(volume)))
    });
    panel.on_audio_output_edited({
        let apply = apply.clone();
        let settings = settings.clone();
        move |index| {
            if index == AUDIO_OUTPUTS.len() as i32 {
                let binaural = settings.borrow().settings().binaural;
                apply(Some(SettingChange::Binaural(xem_settings::Binaural { enabled: true, ..binaural })))
            } else {
                apply(pick(&AUDIO_OUTPUTS, index).map(SettingChange::AudioOutput))
            }
        }
    });
    panel.on_binaural_edited({
        let apply = apply.clone();
        let settings = settings.clone();
        move |name, value| {
            let mut binaural = settings.borrow().settings().binaural;
            let on = value != 0;
            match name.as_str() {
                "music" => binaural.spatial_music = on,
                "effects" => binaural.positional_effects = on,
                "immersion" => binaural.music_immersion = value.clamp(0, 100) as u8,
                "head" => binaural.head_tracked_music = on,
                _ => return,
            }
            apply(Some(SettingChange::Binaural(binaural)))
        }
    });
    panel.on_scale_edited({
        let apply = apply.clone();
        move |index| apply(pick(&SCALES, index).map(SettingChange::Scale))
    });
    panel.on_filter_edited({
        let apply = apply.clone();
        move |index| apply(pick(&FILTERS, index).map(SettingChange::Filter))
    });
    panel.on_graphics_edited({
        let apply = apply.clone();
        let settings = Rc::downgrade(settings);
        move |key, value| {
            let Some(settings) = settings.upgrade() else { return };
            let graphics = settings.borrow().settings().graphics;
            apply(graphics_edit(&graphics, &key, value).map(SettingChange::Graphics));
        }
    });
    panel.on_fidelity_edited({
        let apply = apply.clone();
        let settings = Rc::downgrade(settings);
        move |index, on| {
            let Some(settings) = settings.upgrade() else { return };
            let mut graphics = settings.borrow().settings().graphics;
            let f = &mut graphics.custom_fidelity;
            let flag = match index {
                0 => &mut f.affine_textures,
                1 => &mut f.vertex_snapping,
                2 => &mut f.color_15bit,
                3 => &mut f.dithering,
                4 => &mut f.native_resolution,
                5 => &mut f.painter_ordering,
                _ => return,
            };
            *flag = on;
            apply(Some(SettingChange::Graphics(graphics)));
        }
    });
    panel.on_effect_edited({
        let apply = apply.clone();
        let settings = Rc::downgrade(settings);
        move |index, value| {
            let Some(settings) = settings.upgrade() else { return };
            let mut graphics = settings.borrow().settings().graphics;
            let Some(name) = usize::try_from(index).ok().and_then(|i| EffectLevels::NAMES.get(i)) else { return };
            let levels = match graphics.profile {
                GraphicsProfile::Custom => &mut graphics.custom_effects,
                _ => &mut graphics.hd2d,
            };
            if levels.set(name, value).is_ok() {
                apply(Some(SettingChange::Graphics(graphics)));
            }
        }
    });
    panel.on_show_fps_edited({
        let apply = apply.clone();
        move |show| apply(Some(SettingChange::ShowFps(show)))
    });
    panel.on_fullscreen_edited({
        let apply = apply.clone();
        move |on| apply(Some(SettingChange::Fullscreen(on)))
    });
    panel.on_fast_forward_rate_edited({
        let apply = apply.clone();
        move |index| apply(pick(&FAST_FORWARD_RATES, index).map(SettingChange::FastForwardRate))
    });
    panel.on_binding_edited({
        let apply = apply.clone();
        let panel = panel.as_weak();
        move |index, keyboard, gamepad| {
            let browser = panel.upgrade().is_some_and(|panel| panel.get_browser_storage());
            if let Some((action, _)) = usize::try_from(index).ok().and_then(|i| offered_actions(browser).nth(i)) {
                apply(Some(SettingChange::BindApp {
                    action,
                    binding: AppBinding::parse(&keyboard, &gamepad),
                }));
            }
        }
    });
    panel.on_game_binding_edited({
        let apply = apply.clone();
        move |index, keys, buttons, mouse| {
            if let Some((context, action, _)) = usize::try_from(index).ok().and_then(|i| GAME_ACTION_ROWS.get(i)) {
                apply(Some(SettingChange::BindGame {
                    context: *context,
                    action: *action,
                    binding: GameBinding::parse(&keys, &buttons, &mouse),
                }));
            }
        }
    });
    panel.on_game_bindings_reset(move || apply(Some(SettingChange::ResetGameBindings)));
    panel.on_game_action_selected({
        let weak = panel.as_weak();
        let settings = settings.clone();
        move |_| {
            if let Some(panel) = weak.upgrade() {
                show_game_binding(&panel, settings.borrow().settings());
            }
        }
    });
    panel.on_action_selected({
        let weak = panel.as_weak();
        let settings = settings.clone();
        move |_| {
            if let Some(panel) = weak.upgrade() {
                show_binding(&panel, settings.borrow().settings());
            }
        }
    });
}

/// A key's name on its keycap: `KeyboardEvent.code` without its prefix, a
/// symbol for the punctuation keys (the editors keep the codes).
fn key_label(code: &str) -> &str {
    if let Some(key) = code.strip_prefix("Key").or_else(|| code.strip_prefix("Digit")).filter(|key| key.len() == 1) {
        return key;
    }
    match code {
        "ControlLeft" | "ControlRight" => "Ctrl",
        "ShiftLeft" | "ShiftRight" => "Shift",
        "AltLeft" | "AltRight" => "Alt",
        "MetaLeft" | "MetaRight" => "Meta",
        "ArrowUp" => "Up",
        "ArrowDown" => "Down",
        "ArrowLeft" => "Left",
        "ArrowRight" => "Right",
        "PageUp" => "Page Up",
        "PageDown" => "Page Down",
        "Backquote" => "`",
        "Minus" => "-",
        "Equal" => "=",
        "BracketLeft" => "[",
        "BracketRight" => "]",
        "Backslash" => "\\",
        "Semicolon" => ";",
        "Quote" => "'",
        "Comma" => ",",
        "Period" => ".",
        "Slash" => "/",
        other => other,
    }
}

/// The app operations a panel offers, in [`APP_ACTIONS`] order: Request
/// browser persistence only where the host keeps saves in a browser's
/// storage (`browser`).
fn offered_actions(browser: bool) -> impl Iterator<Item = (AppAction, &'static str)> {
    APP_ACTIONS.into_iter().filter(move |(action, _)| browser || *action != AppAction::RequestPersistence)
}

fn backup_actions(browser: bool) -> impl Iterator<Item = (AppAction, &'static str)> {
    offered_actions(browser).filter(|(action, _)| {
        matches!(
            action,
            AppAction::ExportState
                | AppAction::ImportState
                | AppAction::ExportCard1
                | AppAction::ImportCard1
                | AppAction::ExportCard2
                | AppAction::ImportCard2
                | AppAction::ExportSettings
                | AppAction::ImportSettings
                | AppAction::RetryStorage
                | AppAction::RequestPersistence
                | AppAction::ImportBios
        )
    })
}

/// What a notice over the game is (its icon and colour).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NoticeKind {
    Success,
    Failure,
    /// RetroAchievements' unlocks and completions.
    Achievement,
    Information,
}

impl NoticeKind {
    /// The overlay's `notice-kind`.
    fn index(self) -> i32 {
        match self {
            NoticeKind::Success => 0,
            NoticeKind::Failure => 1,
            NoticeKind::Achievement => 2,
            NoticeKind::Information => 3,
        }
    }
}

/// A notice waiting for the one shown to expire ([`FrameRateUi::notify`]).
struct Queued {
    kind: NoticeKind,
    title: String,
    detail: String,
    badge: Option<slint::Image>,
    seconds: u64,
}

/// An unlocked achievement's badge (a PNG RetroAchievements' server gave)
/// as the HUD shows it: none for data that is no PNG of a badge's size (at
/// most 512 pixels square; the server's are 64), read from its header
/// before it is decoded.
fn badge_image(png: &[u8]) -> Option<slint::Image> {
    let size = |at: usize| png.get(at..at + 4).map(|bytes| u32::from_be_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]));
    let (width, height) = (size(16)?, size(20)?);
    let header = png.starts_with(b"\x89PNG\r\n\x1a\n") && &png[12..16] == b"IHDR";
    if !header || !(1..=512).contains(&width) || !(1..=512).contains(&height) {
        return None;
    }
    slint::Image::load_from_data(png, Some("png")).ok()
}

/// The frame counter, playback activity, state results and RetroAchievements'
/// notices over the game.
pub struct FrameRateUi {
    window: Rc<PanelWindow>,
    component: FrameRateOverlay,
    device: wgpu::Device,
    texture: wgpu::Texture,
    view: wgpu::TextureView,
    started: Option<Duration>,
    previous: Duration,
    frames: u32,
    notice_until: Option<Duration>,
    queued: std::collections::VecDeque<Queued>,
    /// Its width in logical pixels ([`FrameRateUi::set_width`]).
    width: f32,
}

/// The HUD's size in physical pixels: `width` logical pixels by 110.
fn hud_size(scale_factor: f32, width: f32) -> (u32, u32) {
    ((width * scale_factor).round().max(1.0) as u32, (110.0 * scale_factor).round().max(1.0) as u32)
}

impl FrameRateUi {
    /// The HUD's widest, and its least where a host narrows it.
    pub const WIDTH: f32 = 304.0;
    pub const LEAST_WIDTH: f32 = 200.0;

    pub fn new(gpu: SharedGpu, scale_factor: f32) -> Result<Self, PlatformError> {
        let device = gpu.device.clone();
        provide_gpu(gpu)?;
        let component = FrameRateOverlay::new()?;
        let window = CREATED
            .take()
            .ok_or_else(|| PlatformError::Other("Slint created no FPS window".into()))?;
        let size = hud_size(scale_factor, Self::WIDTH);
        let (texture, view) = panel_texture(&device, size);
        let mut ui = Self {
            window,
            component,
            device,
            texture,
            view,
            started: None,
            previous: Duration::ZERO,
            frames: 0,
            notice_until: None,
            queued: Default::default(),
            width: Self::WIDTH,
        };
        ui.resize(scale_factor);
        ui.component.show()?;
        Ok(ui)
    }
    /// The HUD as wide as `logical` pixels allow (between
    /// [`Self::LEAST_WIDTH`] and [`Self::WIDTH`]): a host keeps it clear of
    /// what stands beside it, the touch deck's top buttons. Its chips and
    /// notices fit the width; a notice's words wrap or elide in it.
    pub fn set_width(&mut self, logical: f32) {
        let width = logical.clamp(Self::LEAST_WIDTH, Self::WIDTH).round();
        if width != self.width {
            self.width = width;
            self.resize(self.window.window.scale_factor());
        }
    }
    /// Keeps the HUD, standing at `at` (its top left, in target pixels at
    /// `scale`), 8 logical pixels from what stands level with it
    /// (`controls`, each x, y, width and height in target pixels: the touch
    /// deck's, while it shows): it narrows to the room left of them
    /// ([`Self::set_width`]), whole beside none.
    pub fn keep_clear(&mut self, at: (f32, f32), scale: f32, controls: impl IntoIterator<Item = (f32, f32, f32, f32)>) {
        let (top, bottom) = (at.1, at.1 + 110.0 * scale);
        let width = controls
            .into_iter()
            .filter(|&(_, y, _, height)| y < bottom && y + height > top)
            .map(|(x, ..)| (x - at.0) / scale - 8.0)
            .fold(Self::WIDTH, f32::min);
        self.set_width(width);
    }
    fn resize(&mut self, scale_factor: f32) {
        let size = hud_size(scale_factor, self.width);
        if self.size() != size {
            (self.texture, self.view) = panel_texture(&self.device, size);
        }
        self.window.size.set(PhysicalSize::new(size.0, size.1));
        self.window
            .window
            .dispatch_event(WindowEvent::ScaleFactorChanged { scale_factor });
        self.window.window.dispatch_event(WindowEvent::Resized {
            size: PhysicalSize::new(size.0, size.1).to_logical(scale_factor),
        });
        self.window.redraw.set(true);
    }
    pub fn size(&self) -> (u32, u32) {
        let size = self.window.size.get();
        (size.width, size.height)
    }
    pub fn view(&self) -> &wgpu::TextureView {
        &self.view
    }
    pub fn label(&self) -> String {
        self.component.get_counter().into()
    }
    pub fn component(&self) -> &FrameRateOverlay {
        &self.component
    }
    pub fn texture(&self) -> &wgpu::Texture {
        &self.texture
    }
    pub fn set_activity(&mut self, fast_forward: bool, rewinding: bool) {
        self.component.set_fast_forward(fast_forward);
        self.component.set_rewinding(rewinding);
    }
    /// A result from an explicit save-state operation, never a memory-card
    /// write: shown at once, over whatever showed.
    pub fn notify_state(&mut self, saved: bool, error: Option<&str>, now: Duration) {
        let action = if saved { "save" } else { "load" };
        match error {
            Some(error) => self.show(NoticeKind::Failure, &format!("Could not {action} state"), error, None, 8, now),
            None => self.show(NoticeKind::Success, &format!("State {}", if saved { "saved" } else { "loaded" }), "", None, 4, now),
        }
    }

    /// A notice (RetroAchievements') shown for `seconds` once those before
    /// it expired: one does not hide another. An unlock's `badge` (the
    /// achievement's image, a PNG) shows in place of the trophy.
    pub fn notify(&mut self, kind: NoticeKind, title: &str, detail: &str, badge: Option<&[u8]>, seconds: u64, now: Duration) {
        let badge = badge.and_then(badge_image);
        if self.notice_until.is_some() {
            self.queued.push_back(Queued { kind, title: title.into(), detail: detail.into(), badge, seconds });
        } else {
            self.show(kind, title, detail, badge, seconds, now);
        }
    }

    fn show(&mut self, kind: NoticeKind, title: &str, detail: &str, badge: Option<slint::Image>, seconds: u64, now: Duration) {
        let notice = if detail.is_empty() { title.to_string() } else { format!("{title}: {detail}") };
        self.component.set_notice_title(title.into());
        self.component.set_notice_detail(detail.into());
        self.component.set_notice_badge(badge.unwrap_or_default());
        self.component.set_notice(notice.into());
        self.component.set_notice_kind(kind.index());
        self.notice_until = Some(now.saturating_add(Duration::from_secs(seconds)));
    }
    pub fn notice(&self) -> Option<String> {
        self.notice_until.map(|_| self.component.get_notice().into())
    }
    /// Call after `frame` so an expired notification has been removed.
    pub fn visible(&self) -> bool {
        self.component.get_show_fps() || self.component.get_fast_forward()
            || self.component.get_rewinding() || self.notice_until.is_some()
    }
    pub fn frame(
        &mut self,
        now: Duration,
        scale_factor: f32,
        enabled: bool,
    ) -> Result<(), PlatformError> {
        self.component.set_show_fps(enabled);
        if self.notice_until.is_some_and(|until| now >= until) {
            self.notice_until = None;
            self.component.set_notice("".into());
            if let Some(next) = self.queued.pop_front() {
                self.show(next.kind, &next.title, &next.detail, next.badge, next.seconds, now);
            }
        }
        if self.window.window.scale_factor() != scale_factor {
            self.resize(scale_factor);
        }
        if !enabled {
            self.started = None;
            self.frames = 0;
        } else if self.started.is_none()
            || now < self.previous
            || now.saturating_sub(self.previous) > Duration::from_secs(1)
        {
            self.started = Some(now);
            self.frames = 0;
            self.component.set_counter("— FPS".into());
        } else {
            self.frames += 1;
        }
        self.previous = now;
        let elapsed = self.started.map_or(Duration::ZERO, |started| now.saturating_sub(started));
        if elapsed >= Duration::from_secs(1) {
            self.component.set_counter(
                format!("{:.0} FPS", self.frames as f64 / elapsed.as_secs_f64()).into(),
            );
            self.started = Some(now);
            self.frames = 0;
        }
        if !self.visible() {
            // The host does not composite this texture while hidden. Keep
            // the next visible frame dirty without submitting a GPU pass.
            self.window.redraw.set(true);
            return Ok(());
        }
        slint::platform::update_timers_and_animations();
        if self.window.redraw.take() {
            self.window.renderer.render_to_texture(&self.texture)?;
        }
        Ok(())
    }
}

fn pick<T: Copy>(options: &[T], index: i32) -> Option<T> {
    usize::try_from(index)
        .ok()
        .and_then(|i| options.get(i).copied())
}

fn describe(rejected: &Rejected) -> String {
    match rejected {
        Rejected::Invalid { reason, .. } => format!("Not applied: {reason}"),
        Rejected::Storage { reason, .. } => format!("Not saved: {reason}"),
    }
}
