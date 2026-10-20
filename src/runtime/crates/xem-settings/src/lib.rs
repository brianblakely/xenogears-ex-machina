//! Application settings as a typed service, independent of any widget toolkit.
//!
//! GUIs and agents change settings through the same operation,
//! [`SettingsService::apply`]: every change is validated, persisted through a
//! [`SettingsStore`] and then acknowledged to the caller and to every
//! subscriber. A setting never changes by any other path.

use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::fmt;
use std::path::{Path, PathBuf};
pub mod bindings;
pub mod controls;
pub mod graphics;
pub mod look;
pub mod player;
pub use graphics::{DioramaTurns, EffectLevels, Fidelity, Graphics, GraphicsProfile, HeadsetView, MAX_ZOOM_OUT, MovieFilter, QualityTier, Renderer, TextureFiltering, XrScreen, ZOOM_STEP};
pub use bindings::{GameAction, GameBinding, GameBindings, InputContext, GAME_ACTION_ROWS};
pub use controls::{AppAction, AppBinding, AppInput, AppRequest, APP_ACTIONS};
pub use xem_core::debug::Request as DebugRequest;
/// The platforming areas whose random battles NotWhilePlatforming turns off.
pub use xem_core::encounters::PLATFORMING_AREAS;

pub const MAX_VOLUME: u8 = 100;
/// The mouse camera's deadzone, default and largest, in pixels of motion.
pub const DEFAULT_MOUSE_DEADZONE: u16 = 40;
pub const MAX_MOUSE_DEADZONE: u16 = 100;
pub use xem_core::time::{FastForwardRate, FAST_FORWARD_RATES, DEFAULT_REWIND_INTERVAL_SECONDS, MAX_REWIND_INTERVAL_SECONDS, DEFAULT_REWIND_HISTORY_SECONDS, MAX_REWIND_HISTORY_SECONDS};
/// The Binaural sound mode's settings (`xem_core::binaural::Config`): the
/// mode is on with `enabled`; [`Settings::audio_output`] keeps the original
/// mode it returns to.
pub use xem_core::binaural::Config as Binaural;

/// The game's own sound mode, as its Sound screen chooses it (the sound
/// driver's output modes; the host plays what the console outputs).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum AudioOutput {
    Mono,
    /// The game's default.
    #[default]
    Stereo,
    /// The game's Wide-Stereo.
    Wide,
}

impl AudioOutput {
    /// The driver's mode (`Session::set_sound_mode`).
    pub fn game_mode(self) -> u32 {
        match self {
            AudioOutput::Mono => 0,
            AudioOutput::Stereo => 1,
            AudioOutput::Wide => 2,
        }
    }

    /// The setting for the driver's mode `mode` (the game's own choice).
    pub fn from_game_mode(mode: u32) -> Option<Self> {
        [AudioOutput::Mono, AudioOutput::Stereo, AudioOutput::Wide].into_iter().find(|output| output.game_mode() == mode)
    }
}

/// How the game image is scaled into the presentation area.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum Scale {
    /// Largest size that keeps the aspect ratio.
    #[default]
    Fit,
    /// Largest whole multiple of the image size.
    Integer,
    /// Fill the area, ignoring the aspect ratio.
    Stretch,
}

/// How the game image is sampled when scaled.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum Filter {
    #[default]
    Nearest,
    Linear,
}

#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default)]
pub struct Presentation {
    pub scale: Scale,
    pub filter: Filter,
    pub show_fps: bool,
    /// The desktop window covers its display (hosts that place their own
    /// window; a window manager's own fullscreen leaves this as it is).
    pub fullscreen: bool,
}

/// Where a headset's pointer comes from (the assisted controls' aim).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum XrPointer {
    /// Eye gaze, where the runtime offers it and it is permitted.
    Eye,
    /// The head's direction.
    #[default]
    Head,
    LeftHand,
    RightHand,
}

/// The assisted controls (plan.md, the headset hand/gaze contract; touch
/// shares the same widgets).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct Assisted {
    /// Hand gestures in a headset (controllers and pointers always work).
    pub gestures: bool,
    pub pointer: XrPointer,
    /// Assisted walking stops at a jump's takeoff instead of pressing on.
    pub ledge_brake: bool,
    /// The bespoke widgets' glass drawn solid, and their motion still.
    pub reduced_transparency: bool,
    pub reduced_motion: bool,
    /// Touch screens show the game's controls over the picture.
    pub touch_controls: bool,
}

impl Default for Assisted {
    fn default() -> Self {
        Self { gestures: true, pointer: XrPointer::Head, ledge_brake: false, reduced_transparency: false, reduced_motion: false, touch_controls: true }
    }
}

/// Where the field's random battles happen (plan.md, Phase 7). The setting
/// stores the policy's name, so a revised table of platforming areas applies
/// to the default and a deliberate Original is never overwritten.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum RandomBattles {
    /// Off in the platforming-heavy areas
    /// (`xem_core::encounters::PLATFORMING_AREAS`), the game's elsewhere.
    #[default]
    NotWhilePlatforming,
    /// Everywhere the game has them.
    Original,
}

/// How often the field's random battles come where the game has them
/// (docs/runtime.md, Random battles in platforming areas).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum EncounterRate {
    /// As the game's own draw deals them.
    #[default]
    Original,
    /// Half as often for the same walk: the draw runs every other field
    /// frame (`Session::set_half_encounters`).
    Half,
}

/// How fast the game's message windows show their text (docs/runtime.md,
/// Text speed). Every speed keeps the windows' pages, pauses, timed waits
/// and authored slow text.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum TextSpeed {
    /// The game's own: a glyph per field frame (two where a scene sets a
    /// slower frame).
    #[default]
    Original,
    /// Four glyphs per field frame.
    Fast,
    /// A line per field frame.
    Instant,
}

impl TextSpeed {
    pub const ALL: [TextSpeed; 3] = [TextSpeed::Original, TextSpeed::Fast, TextSpeed::Instant];

    /// The glyphs a window's step reveals at least
    /// (`Session::set_text_speed`); 0 the window's own.
    pub fn glyphs(self) -> u32 {
        match self {
            TextSpeed::Original => 0,
            TextSpeed::Fast => 4,
            TextSpeed::Instant => 255,
        }
    }
}

/// Auto-advance dialogue (docs/runtime.md, Auto-advance dialogue): a field
/// dialogue's page goes on by itself once its prompt has shown `dwell`
/// vertical blanks (a sixtieth of a second each); choices always wait for
/// the player.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct AutoAdvance {
    pub enabled: bool,
    pub dwell: u32,
}

/// The auto-advance dwell's default and range, in vertical blanks.
pub const DEFAULT_AUTO_ADVANCE_DWELL: u32 = 120;
pub const MIN_AUTO_ADVANCE_DWELL: u32 = 30;
pub const MAX_AUTO_ADVANCE_DWELL: u32 = 600;
const _: () = assert!(MAX_AUTO_ADVANCE_DWELL <= xem_core::control::MAX_AUTO_ADVANCE_DWELL, "the session takes every dwell the setting offers");

impl Default for AutoAdvance {
    fn default() -> Self {
        Self { enabled: false, dwell: DEFAULT_AUTO_ADVANCE_DWELL }
    }
}

/// The complete application settings.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct Settings {
    pub master_volume: u8,
    /// The game's own sound mode; under Binaural, the one it returns to.
    pub audio_output: AudioOutput,
    /// The Binaural sound mode (headphones): on, the game runs in Stereo and
    /// the hosts present its voices binaurally.
    pub binaural: Binaural,
    pub presentation: Presentation,
    pub graphics: Graphics,
    /// The game's bindings: the inputs that hold each typed action.
    pub game_bindings: GameBindings,
    pub app_bindings: BTreeMap<AppAction, AppBinding>,
    pub fast_forward_rate: FastForwardRate,
    pub invert_trigger_chords: bool,
    /// Sideways mouse motion turns the field camera; the game then holds the
    /// mouse (hidden, unbounded motion) while it has the input.
    pub mouse_camera: bool,
    /// Mouse camera rate as a percentage of the original host sensitivity.
    pub mouse_sensitivity: u16,
    /// Pixels the mouse may move from where it rested before its motion
    /// turns the field camera (first-person look takes every pixel).
    pub mouse_deadzone: u16,
    /// Field movement runs unless the player holds the run button.
    pub run_by_default: bool,
    pub rewind_enabled: bool,
    pub rewind_interval_seconds: u32,
    pub rewind_history_seconds: u32,
    pub pause_on_focus_loss: bool,
    pub assisted: Assisted,
    /// Where the field's random battles happen (the hosts give the session
    /// [`Settings::random_battles_off`]).
    pub random_battles: RandomBattles,
    /// How often the random battles it leaves on come.
    pub encounter_rate: EncounterRate,
    /// How fast message windows show their text.
    pub text_speed: TextSpeed,
    /// Field dialogue goes on by itself.
    pub auto_advance: AutoAdvance,
    /// The player's Confirm ends a field movie (Cross or Start the movie
    /// mode's), as the game lets it end some; window movies stay the
    /// scripts'.
    pub movie_skip: bool,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            master_volume: 80,
            audio_output: AudioOutput::default(),
            binaural: Binaural::default(),
            presentation: Presentation::default(),
            graphics: Graphics::default(),
            game_bindings: GameBindings::default(),
            app_bindings: controls::defaults(),
            fast_forward_rate: FastForwardRate::default(),
            invert_trigger_chords: false,
            mouse_camera: false,
            mouse_sensitivity: 50,
            mouse_deadzone: DEFAULT_MOUSE_DEADZONE,
            run_by_default: false,
            rewind_enabled: true,
            rewind_interval_seconds: DEFAULT_REWIND_INTERVAL_SECONDS,
            rewind_history_seconds: DEFAULT_REWIND_HISTORY_SECONDS,
            pause_on_focus_loss: true,
            assisted: Assisted::default(),
            random_battles: RandomBattles::default(),
            encounter_rate: EncounterRate::default(),
            text_speed: TextSpeed::default(),
            auto_advance: AutoAdvance::default(),
            movie_skip: false,
        }
    }
}

/// One requested change. Values are as received from the caller and are
/// validated by [`SettingsService::apply`].
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum SettingChange {
    Replace(Settings),
    BindApp { action: AppAction, binding: AppBinding },
    FastForwardRate(FastForwardRate),
    InvertTriggerChords(bool),
    MouseCamera(bool),
    MouseSensitivity(u16),
    MouseDeadzone(u16),
    RunByDefault(bool),
    RewindEnabled(bool),
    RewindIntervalSeconds(u32),
    RewindHistorySeconds(u32),
    MasterVolume(i32),
    /// An original sound mode: Binaural, if on, goes off.
    AudioOutput(AudioOutput),
    /// The Binaural block, whole: on, off (back to the original mode
    /// `audio_output` keeps), and its options.
    Binaural(Binaural),
    Scale(Scale),
    Filter(Filter),
    ShowFps(bool),
    Fullscreen(bool),
    /// The whole graphics block, validated.
    Graphics(Graphics),
    PauseOnFocusLoss(bool),
    /// One action's inputs in a context, validated with the rest.
    BindGame { context: InputContext, action: GameAction, binding: GameBinding },
    /// Every game binding back to the defaults.
    ResetGameBindings,
    /// The assisted controls' block, whole.
    Assisted(Assisted),
    RandomBattles(RandomBattles),
    EncounterRate(EncounterRate),
    TextSpeed(TextSpeed),
    /// The auto-advance block, whole: on or off and its dwell.
    AutoAdvance(AutoAdvance),
    MovieSkip(bool),
}

/// An applied and persisted change.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Ack {
    /// Increments with every acknowledged change.
    pub revision: u64,
    pub change: SettingChange,
    pub settings: Settings,
}

/// A change that was not applied; the settings are unchanged.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Rejected {
    Invalid {
        change: SettingChange,
        reason: String,
    },
    Storage {
        change: SettingChange,
        reason: String,
    },
}

impl fmt::Display for Rejected {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Rejected::Invalid { change, reason } => write!(f, "{change:?} is invalid: {reason}"),
            Rejected::Storage { change, reason } => {
                write!(f, "{change:?} could not be saved: {reason}")
            }
        }
    }
}

impl std::error::Error for Rejected {}

impl Settings {
    /// Pointing at battle commands (including combo buttons) and pausing
    /// always release the cursor, even when camera turning is enabled.
    pub fn captures_mouse(&self, ui: xem_core::context::Context, paused: bool) -> bool {
        self.mouse_camera && !paused && !ui.navigates()
            && !matches!(ui, xem_core::context::Context::Battle { .. })
    }

    pub fn mouse_scale(&self) -> f32 { f32::from(self.mouse_sensitivity) / 100.0 }

    /// The game's sound mode the hosts run it in (`Session::set_sound_mode`):
    /// Stereo under Binaural, whose placement reads the Stereo pan law;
    /// else the original mode chosen.
    pub fn game_sound_mode(&self) -> u32 {
        if self.binaural.enabled { AudioOutput::Stereo.game_mode() } else { self.audio_output.game_mode() }
    }

    /// The field maps whose random battles the hosts turn off
    /// (`Session::set_random_battles`): the platforming areas' under
    /// NotWhilePlatforming, none under Original.
    pub fn random_battles_off(&self) -> &'static [u16] {
        match self.random_battles {
            RandomBattles::NotWhilePlatforming => xem_core::encounters::platforming_maps(),
            RandomBattles::Original => &[],
        }
    }

    /// Whether the hosts halve the session's random battles
    /// (`Session::set_half_encounters`).
    pub fn half_encounters(&self) -> bool {
        self.encounter_rate == EncounterRate::Half
    }

    /// The auto-advance dwell the hosts give the session
    /// (`Session::set_auto_advance`): the setting's while it is on, else 0.
    pub fn auto_advance_dwell(&self) -> u32 {
        if self.auto_advance.enabled { self.auto_advance.dwell } else { 0 }
    }

    fn with(&self, change: &SettingChange) -> Result<Settings, String> {
        let mut next = self.clone();
        match change {
            SettingChange::Replace(settings) => { settings.validate()?; next = settings.clone(); }
            SettingChange::BindApp { action, binding } => { binding.validate()?; next.app_bindings.insert(*action,binding.clone()); }
            SettingChange::FastForwardRate(rate) => next.fast_forward_rate = *rate,
            SettingChange::InvertTriggerChords(invert) => next.invert_trigger_chords = *invert,
            SettingChange::MouseCamera(enabled) => next.mouse_camera = *enabled,
            SettingChange::MouseSensitivity(percent) => next.mouse_sensitivity = *percent,
            SettingChange::MouseDeadzone(pixels) => next.mouse_deadzone = *pixels,
            SettingChange::RunByDefault(enabled) => next.run_by_default = *enabled,
            SettingChange::RewindEnabled(enabled) => next.rewind_enabled = *enabled,
            SettingChange::RewindIntervalSeconds(seconds) => next.rewind_interval_seconds = *seconds,
            SettingChange::RewindHistorySeconds(seconds) => next.rewind_history_seconds = *seconds,
            SettingChange::MasterVolume(volume) => {
                next.master_volume = u8::try_from(*volume)
                    .ok()
                    .filter(|v| *v <= MAX_VOLUME)
                    .ok_or_else(|| format!("volume must be 0-{MAX_VOLUME}"))?;
            }
            SettingChange::AudioOutput(mode) => {
                next.audio_output = *mode;
                next.binaural.enabled = false;
            }
            SettingChange::Binaural(binaural) => next.binaural = *binaural,
            SettingChange::Scale(scale) => next.presentation.scale = *scale,
            SettingChange::Filter(filter) => next.presentation.filter = *filter,
            SettingChange::ShowFps(show) => next.presentation.show_fps = *show,
            SettingChange::Fullscreen(on) => next.presentation.fullscreen = *on,
            SettingChange::Graphics(graphics) => next.graphics = *graphics,
            SettingChange::PauseOnFocusLoss(enabled) => next.pause_on_focus_loss = *enabled,
            SettingChange::BindGame { context, action, binding } => {
                next.game_bindings.context_mut(*context).insert(*action, binding.clone());
            }
            SettingChange::ResetGameBindings => next.game_bindings = GameBindings::default(),
            SettingChange::Assisted(assisted) => next.assisted = *assisted,
            SettingChange::RandomBattles(battles) => next.random_battles = *battles,
            SettingChange::EncounterRate(rate) => next.encounter_rate = *rate,
            SettingChange::TextSpeed(speed) => next.text_speed = *speed,
            SettingChange::AutoAdvance(auto) => next.auto_advance = *auto,
            SettingChange::MovieSkip(enabled) => next.movie_skip = *enabled,
        }
        next.validate()?;
        Ok(next)
    }

    /// Settings read from `value` one setting at a time: an object whole
    /// when the settings it makes read and validate, else each of its values
    /// in turn; a value that does not (a variant no longer offered, a number
    /// out of range) leaves its setting at the default, and its path (such as
    /// `graphics.filtering.movies`) is returned. A shortcut list saved
    /// before an action with a default binding existed gives that action its
    /// default, less any chord another action holds. Only a value that is
    /// not an object is refused.
    pub fn read(value: &serde_json::Value) -> Result<(Settings, Vec<String>), String> {
        fn take(settings: &mut serde_json::Value, path: &mut Vec<String>, value: &serde_json::Value, reset: &mut Vec<String>) {
            let mut candidate = settings.clone();
            let mut at = &mut candidate;
            for key in path.iter() {
                if !at.get(key).is_some_and(serde_json::Value::is_object) {
                    at[key.as_str()] = serde_json::Value::Object(Default::default());
                }
                at = &mut at[key.as_str()];
            }
            *at = value.clone();
            if serde_json::from_value::<Settings>(candidate.clone()).is_ok_and(|read| read.validate().is_ok()) {
                *settings = candidate;
                return;
            }
            match value {
                serde_json::Value::Object(map) if !map.is_empty() => {
                    for (key, value) in map {
                        path.push(key.clone());
                        take(settings, path, value, reset);
                        path.pop();
                    }
                }
                _ => reset.push(path.join(".")),
            }
        }
        if !value.is_object() {
            return Err("settings are not a JSON object".into());
        }
        let mut settings = serde_json::to_value(Settings::default()).expect("settings serialize");
        let mut reset = Vec::new();
        take(&mut settings, &mut Vec::new(), value, &mut reset);
        let mut settings: Settings = serde_json::from_value(settings).expect("read when taken");
        for (action, default) in controls::defaults() {
            if settings.app_bindings.contains_key(&action) {
                continue;
            }
            // A chord is held when another action's is the same keys.
            let free = |chord: &Vec<String>, of: fn(&AppBinding) -> &Vec<String>| {
                !settings.app_bindings.values().any(|binding| {
                    let held = of(binding);
                    !held.is_empty() && held.len() == chord.len() && chord.iter().all(|key| held.contains(key))
                })
            };
            let keyboard = if free(&default.keyboard, |b| &b.keyboard) { default.keyboard.clone() } else { Vec::new() };
            let gamepad = if free(&default.gamepad, |b| &b.gamepad) { default.gamepad.clone() } else { Vec::new() };
            settings.app_bindings.insert(action, AppBinding { keyboard, gamepad });
        }
        Ok((settings, reset))
    }

    /// Checks values that the types alone do not constrain (for loaded files).
    fn validate(&self) -> Result<(), String> {
        self.fast_forward_rate.validate()?;
        self.graphics.validate()?;
        if !(10..=200).contains(&self.mouse_sensitivity) {
            return Err("mouse sensitivity must be 10–200 percent".into());
        }
        if self.mouse_deadzone > MAX_MOUSE_DEADZONE {
            return Err(format!("mouse deadzone must be 0–{MAX_MOUSE_DEADZONE} pixels"));
        }
        if !(1..=MAX_REWIND_INTERVAL_SECONDS).contains(&self.rewind_interval_seconds) {
            return Err(format!("rewind interval must be 1–{MAX_REWIND_INTERVAL_SECONDS} seconds"));
        }
        if !(1..=MAX_REWIND_HISTORY_SECONDS).contains(&self.rewind_history_seconds) {
            return Err(format!("rewind history must be 1–{MAX_REWIND_HISTORY_SECONDS} seconds"));
        }
        if self.rewind_interval_seconds > self.rewind_history_seconds {
            return Err("rewind interval cannot exceed the history length".into());
        }
        if self.master_volume > MAX_VOLUME {
            return Err(format!("volume must be 0-{MAX_VOLUME}"));
        }
        if !(MIN_AUTO_ADVANCE_DWELL..=MAX_AUTO_ADVANCE_DWELL).contains(&self.auto_advance.dwell) {
            return Err(format!("the auto-advance dwell must be {MIN_AUTO_ADVANCE_DWELL}–{MAX_AUTO_ADVANCE_DWELL} vertical blanks"));
        }
        self.binaural.validate()?;
        self.game_bindings.validate()?;
        for binding in self.app_bindings.values() { binding.validate()?; }
        let bindings: Vec<_> = self.app_bindings.iter().collect();
        for (i,(action,binding)) in bindings.iter().enumerate() {
            for (other,candidate) in &bindings[i+1..] {
                for (a,b) in [(&binding.keyboard,&candidate.keyboard),(&binding.gamepad,&candidate.gamepad)] {
                    if !a.is_empty() && a.len() == b.len() && a.iter().all(|key| b.contains(key)) {
                        return Err(format!("{action:?} and {other:?} use the same binding chord"));
                    }
                }
            }
        }
        Ok(())
    }
}

/// Where settings are persisted, as JSON text.
pub trait SettingsStore {
    /// The stored text, or `None` when nothing has been saved yet.
    fn load(&self) -> std::io::Result<Option<String>>;
    fn save(&mut self, json: &str) -> std::io::Result<()>;
}

/// A store in memory, for tests and hosts without persistent storage.
#[derive(Debug, Default)]
pub struct MemoryStore {
    pub json: Option<String>,
}

impl SettingsStore for MemoryStore {
    fn load(&self) -> std::io::Result<Option<String>> {
        Ok(self.json.clone())
    }

    fn save(&mut self, json: &str) -> std::io::Result<()> {
        self.json = Some(json.to_owned());
        Ok(())
    }
}

/// A JSON file, replaced atomically on save.
#[derive(Debug)]
pub struct FileStore {
    path: PathBuf,
}

impl FileStore {
    pub fn new(path: impl Into<PathBuf>) -> Self {
        Self { path: path.into() }
    }

    /// `settings.json` beside the portable executable (or in XEM_DATA_DIR).
    /// Android and browsers pass their own location or use another store.
    pub fn default_path() -> Option<PathBuf> {
        xem_storage::application_directory().ok().map(|base| base.join("settings.json"))
    }

    pub fn path(&self) -> &Path {
        &self.path
    }
}

impl SettingsStore for FileStore {
    fn load(&self) -> std::io::Result<Option<String>> {
        match std::fs::read_to_string(&self.path) {
            Ok(json) => Ok(Some(json)),
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(None),
            Err(error) => Err(error),
        }
    }

    fn save(&mut self, json: &str) -> std::io::Result<()> {
        xem_storage::replace(&self.path, json.as_bytes())
    }
}

/// Why stored settings could not be opened.
#[derive(Debug)]
pub enum LoadError {
    Io(std::io::Error),
    Invalid(String),
}

impl fmt::Display for LoadError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            LoadError::Io(error) => write!(f, "settings could not be read: {error}"),
            LoadError::Invalid(reason) => write!(f, "stored settings are invalid: {reason}"),
        }
    }
}

impl std::error::Error for LoadError {}

type Listener = Box<dyn FnMut(&Ack)>;

/// The single owner of the current settings.
pub struct SettingsService {
    settings: Settings,
    revision: u64,
    store: Box<dyn SettingsStore>,
    listeners: Vec<Listener>,
}

impl SettingsService {
    pub fn export(&self) -> String {
        serde_json::to_string_pretty(&serde_json::json!({ "version": 1, "settings": self.settings })).expect("settings backup")
    }

    pub fn import(&mut self, text: &str) -> Result<Ack, String> {
        #[derive(Deserialize)]
        #[serde(deny_unknown_fields)]
        struct Backup { version: u32, settings: serde_json::Value }
        let backup: Backup = serde_json::from_str(text).map_err(|e| e.to_string())?;
        if backup.version != 1 { return Err("incompatible settings backup version".into()); }
        let (settings, _) = Settings::read(&backup.settings)?;
        self.apply(SettingChange::Replace(settings)).map_err(|e| e.to_string())
    }
    /// Opens the settings saved in `store`, or the defaults if none were saved.
    pub fn open(store: impl SettingsStore + 'static) -> Result<Self, LoadError> {
        let settings = match store.load().map_err(LoadError::Io)? {
            Some(json) => {
                let value: serde_json::Value = serde_json::from_str(&json).map_err(|error| LoadError::Invalid(error.to_string()))?;
                Settings::read(&value).map_err(LoadError::Invalid)?.0
            }
            None => Settings::default(),
        };
        Ok(Self::with_settings(settings, store))
    }

    /// Starts from the defaults, replacing whatever `store` holds on the first change.
    pub fn with_defaults(store: impl SettingsStore + 'static) -> Self {
        Self::with_settings(Settings::default(), store)
    }

    fn with_settings(settings: Settings, store: impl SettingsStore + 'static) -> Self {
        Self {
            settings,
            revision: 0,
            store: Box::new(store),
            listeners: Vec::new(),
        }
    }

    pub fn settings(&self) -> &Settings {
        &self.settings
    }

    pub fn revision(&self) -> u64 {
        self.revision
    }

    /// Validates, persists and applies `change`, then notifies subscribers.
    pub fn apply(&mut self, change: SettingChange) -> Result<Ack, Rejected> {
        let next = match self.settings.with(&change) {
            Ok(next) => next,
            Err(reason) => return Err(Rejected::Invalid { change, reason }),
        };
        let json = serde_json::to_string_pretty(&next).expect("settings serialize");
        if let Err(error) = self.store.save(&json) {
            return Err(Rejected::Storage {
                change,
                reason: error.to_string(),
            });
        }
        self.settings = next;
        self.revision += 1;
        let ack = Ack {
            revision: self.revision,
            change,
            settings: self.settings.clone(),
        };
        for listener in &mut self.listeners {
            listener(&ack);
        }
        Ok(ack)
    }

    /// Calls `listener` after every acknowledged change. Listeners must not call
    /// back into the service; they observe the acknowledged settings in the [`Ack`].
    pub fn subscribe(&mut self, listener: impl FnMut(&Ack) + 'static) {
        self.listeners.push(Box::new(listener));
    }

    /// The settings commands of an agent's JSON channel (`xem-desktop
    /// --control`): `{"cmd":"settings"}` answers the settings, and
    /// `{"cmd":"settings_apply","change":{…}}` applies a [`SettingChange`]
    /// as the panel's edits do and answers its acknowledgement,
    /// `{revision, settings}`, or `{error}` with the settings unchanged.
    /// Another command is not the settings' (`None`).
    pub fn command(&mut self, command: &serde_json::Value) -> Option<serde_json::Value> {
        match command.get("cmd").and_then(serde_json::Value::as_str)? {
            "settings" => Some(serde_json::to_value(&self.settings).expect("settings serialize")),
            "settings_apply" => {
                let change = command.get("change").cloned().unwrap_or_default();
                Some(match serde_json::from_value::<SettingChange>(change) {
                    Ok(change) => match self.apply(change) {
                        Ok(ack) => serde_json::json!({ "revision": ack.revision, "settings": ack.settings }),
                        Err(rejected) => serde_json::json!({ "error": rejected.to_string() }),
                    },
                    Err(error) => serde_json::json!({ "error": format!("not a setting change: {error}") }),
                })
            }
            _ => None,
        }
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn mouse_camera_releases_for_pause_menus_and_every_battle_page() {
        use super::*;
        use xem_core::context::{Context, Choice};
        let settings = Settings { mouse_camera: true, ..Settings::default() };
        for ui in [Context::Walk, Context::WorldMap { movement: 1, scene: 0 }] {
            assert!(settings.captures_mouse(ui, false));
            assert!(!settings.captures_mouse(ui, true));
            assert!(!Settings::default().captures_mouse(ui, false));
        }
        assert!(!settings.captures_mouse(Context::Dialogue { choice: Some(Choice { count: 2, index: 0 }) }, false));
        for page in 0..8 {
            for attack in [false, true] {
                assert!(!settings.captures_mouse(Context::Battle { page, attack, ap: 7, list: false, targeting: false }, false));
            }
        }
    }

    #[test]
    fn mouse_sensitivity_defaults_lower_and_validates_persists_and_scales_motion() {
        use super::*;
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        assert_eq!(service.settings().mouse_scale(), 0.5);
        for percent in [10, 50, 100, 200] {
            service.apply(SettingChange::MouseSensitivity(percent)).unwrap();
            assert_eq!(SettingsService::open(store.clone()).unwrap().settings().mouse_sensitivity, percent);
        }
        for percent in [0, 9, 201, u16::MAX] {
            assert!(service.apply(SettingChange::MouseSensitivity(percent)).is_err());
        }
        let duration = |percent| {
            let settings = Settings { mouse_sensitivity: percent, mouse_deadzone: 0, ..Default::default() };
            let mut mouse = bindings::Mouse::default();
            mouse.moved(20.0, &settings);
            (0..200).take_while(|ms| mouse.turn(f64::from(*ms)).is_some()).count()
        };
        assert!(duration(50) < duration(100));
        assert!(duration(100) < duration(200));
        assert_eq!(Settings::read(&serde_json::json!({"mouse_camera": true})).unwrap().0.mouse_sensitivity, 50);
    }

    #[test]
    fn mouse_deadzone_defaults_validates_and_persists() {
        use super::*;
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        assert_eq!(service.settings().mouse_deadzone, DEFAULT_MOUSE_DEADZONE);
        for pixels in [0, 1, 20, MAX_MOUSE_DEADZONE] {
            service.apply(SettingChange::MouseDeadzone(pixels)).unwrap();
            assert_eq!(SettingsService::open(store.clone()).unwrap().settings().mouse_deadzone, pixels);
        }
        for pixels in [MAX_MOUSE_DEADZONE + 1, u16::MAX] {
            assert!(service.apply(SettingChange::MouseDeadzone(pixels)).is_err());
        }
        // Saved before the setting existed, or out of range: the default.
        assert_eq!(Settings::read(&serde_json::json!({"mouse_camera": true})).unwrap().0.mouse_deadzone, DEFAULT_MOUSE_DEADZONE);
        let (read, reset) = Settings::read(&serde_json::json!({"mouse_deadzone": MAX_MOUSE_DEADZONE + 1})).unwrap();
        assert_eq!((read.mouse_deadzone, reset), (DEFAULT_MOUSE_DEADZONE, vec!["mouse_deadzone".to_string()]));
    }

    #[test]
    fn random_battles_stay_off_in_platforming_areas_unless_the_original_is_chosen() {
        use super::*;
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        assert_eq!(service.settings().random_battles, RandomBattles::NotWhilePlatforming);
        let off = service.settings().random_battles_off();
        assert_eq!(off, xem_core::encounters::platforming_maps());
        assert!(off.contains(&414) && !off.contains(&23), "Babel Tower's map 414, not Blackmoon Forest's map 23");
        // The default saves the policy's name, not its maps: a revised table
        // applies to it.
        service.apply(SettingChange::MasterVolume(40)).unwrap();
        let saved: serde_json::Value = serde_json::from_str(store.0.borrow().as_deref().unwrap()).unwrap();
        assert_eq!(saved["random_battles"], "NotWhilePlatforming");
        // A deliberate Original survives other changes and a reload.
        service.apply(SettingChange::RandomBattles(RandomBattles::Original)).unwrap();
        assert_eq!(service.settings().random_battles_off(), &[] as &[u16]);
        service.apply(SettingChange::MouseCamera(true)).unwrap();
        let reopened = SettingsService::open(store.clone()).unwrap();
        assert_eq!(reopened.settings().random_battles, RandomBattles::Original);
        // A backup carries it into another service.
        let mut other = SettingsService::with_defaults(MemoryStore::default());
        other.import(&reopened.export()).unwrap();
        assert_eq!(other.settings().random_battles, RandomBattles::Original);
        let agent: SettingChange = serde_json::from_str(r#"{"RandomBattles":"NotWhilePlatforming"}"#).unwrap();
        other.apply(agent).unwrap();
        assert_eq!(other.settings().random_battles_off(), off);
        // The rate of the ones left on: the original's by default, Half
        // saved, reloaded and carried by a backup.
        assert_eq!(other.settings().encounter_rate, EncounterRate::Original);
        service.apply(SettingChange::EncounterRate(EncounterRate::Half)).unwrap();
        assert_eq!(SettingsService::open(store.clone()).unwrap().settings().encounter_rate, EncounterRate::Half);
        other.import(&service.export()).unwrap();
        assert_eq!(other.settings().encounter_rate, EncounterRate::Half);
        let (read, reset) = Settings::read(&serde_json::json!({"encounter_rate": "Never"})).unwrap();
        assert_eq!((read.encounter_rate, reset), (EncounterRate::Original, vec!["encounter_rate".to_string()]));
        // Saved before the setting existed, or with a policy no longer
        // offered: the default, alone.
        assert_eq!(Settings::read(&serde_json::json!({"mouse_camera": true})).unwrap().0.random_battles, RandomBattles::NotWhilePlatforming);
        let (read, reset) = Settings::read(&serde_json::json!({"random_battles": "Never", "mouse_camera": true})).unwrap();
        assert_eq!((read.random_battles, read.mouse_camera, reset), (RandomBattles::NotWhilePlatforming, true, vec!["random_battles".to_string()]));
    }

    #[test]
    fn text_speed_auto_advance_and_movie_skip_default_to_the_original_validate_and_persist() {
        use super::*;
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        let settings = service.settings();
        assert_eq!((settings.text_speed, settings.auto_advance.enabled, settings.movie_skip), (TextSpeed::Original, false, false));
        assert_eq!((settings.text_speed.glyphs(), settings.auto_advance_dwell()), (0, 0), "the sessions get the original's");
        service.apply(SettingChange::TextSpeed(TextSpeed::Instant)).unwrap();
        service.apply(SettingChange::AutoAdvance(AutoAdvance { enabled: true, dwell: 90 })).unwrap();
        service.apply(SettingChange::MovieSkip(true)).unwrap();
        let reopened = SettingsService::open(store.clone()).unwrap();
        let settings = reopened.settings();
        assert_eq!((settings.text_speed.glyphs(), settings.auto_advance_dwell(), settings.movie_skip), (255, 90, true));
        for dwell in [0, MIN_AUTO_ADVANCE_DWELL - 1, MAX_AUTO_ADVANCE_DWELL + 1] {
            assert!(service.apply(SettingChange::AutoAdvance(AutoAdvance { enabled: true, dwell })).is_err(), "dwell {dwell}");
        }
        // Off keeps the dwell for the next time it is on.
        service.apply(SettingChange::AutoAdvance(AutoAdvance { enabled: false, dwell: 90 })).unwrap();
        assert_eq!((service.settings().auto_advance_dwell(), service.settings().auto_advance.dwell), (0, 90));
        // An agent's change, and a backup.
        let agent: SettingChange = serde_json::from_str(r#"{"TextSpeed":"Fast"}"#).unwrap();
        service.apply(agent).unwrap();
        assert_eq!(service.settings().text_speed.glyphs(), 4);
        let mut other = SettingsService::with_defaults(MemoryStore::default());
        other.import(&service.export()).unwrap();
        assert_eq!((other.settings().text_speed, other.settings().movie_skip), (TextSpeed::Fast, true));
        // Saved before the options existed, or with a value no longer
        // offered: the original's, alone.
        let (read, reset) = Settings::read(&serde_json::json!({"text_speed": "Warp", "auto_advance": {"enabled": true, "dwell": 5}, "mouse_camera": true})).unwrap();
        assert_eq!((read.text_speed, read.auto_advance, read.mouse_camera), (TextSpeed::Original, AutoAdvance { enabled: true, dwell: DEFAULT_AUTO_ADVANCE_DWELL }, true));
        assert_eq!(reset, ["auto_advance.dwell", "text_speed"]);
        assert!(!Settings::read(&serde_json::json!({"mouse_camera": true})).unwrap().0.movie_skip);
    }

    /// The gameplay options' keys as `Settings` saves them.
    const GAMEPLAY: [&str; 5] = ["random_battles", "encounter_rate", "text_speed", "auto_advance", "movie_skip"];

    #[test]
    fn the_gameplay_options_default_to_not_while_platforming_and_the_original_and_explicit_choices_survive_saves_backups_and_imports() {
        use super::*;
        let gameplay = |settings: &Settings| -> serde_json::Value {
            let value = serde_json::to_value(settings).unwrap();
            GAMEPLAY.iter().map(|key| (key.to_string(), value[key].clone())).collect()
        };
        let defaults = serde_json::json!({"random_battles": "NotWhilePlatforming", "encounter_rate": "Original", "text_speed": "Original",
            "auto_advance": {"enabled": false, "dwell": DEFAULT_AUTO_ADVANCE_DWELL}, "movie_skip": false});
        // A fresh settings file: random battles off in the platforming
        // areas, the original's everything else, as the first save writes.
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        service.apply(SettingChange::MasterVolume(40)).unwrap();
        let saved: serde_json::Value = serde_json::from_str(store.0.borrow().as_deref().unwrap()).unwrap();
        assert_eq!(gameplay(&serde_json::from_value(saved).unwrap()), defaults);
        let settings = service.settings();
        assert_eq!((settings.random_battles_off(), settings.half_encounters(), settings.text_speed.glyphs(), settings.auto_advance_dwell(), settings.movie_skip),
            (xem_core::encounters::platforming_maps(), false, 0, 0, false), "the sessions get the platforming maps and the original's");
        // A file saved before the options existed: every key but theirs,
        // with other choices of its own. It reads the defaults for them,
        // keeps the rest and resets nothing, from a real file too.
        let mut old = serde_json::to_value(Settings { master_volume: 40, mouse_camera: true, ..Settings::default() }).unwrap();
        for key in GAMEPLAY {
            old.as_object_mut().unwrap().remove(key);
        }
        let (read, reset) = Settings::read(&old).unwrap();
        assert_eq!((gameplay(&read), read.master_volume, read.mouse_camera, reset), (defaults.clone(), 40, true, Vec::<String>::new()));
        let dir = std::env::temp_dir().join(format!("xem-settings-gameplay-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("settings.json");
        std::fs::write(&path, serde_json::to_string_pretty(&old).unwrap()).unwrap();
        let opened = SettingsService::open(FileStore::new(&path)).unwrap();
        assert_eq!((gameplay(opened.settings()), opened.settings().master_volume), (defaults.clone(), 40));
        // An explicit choice of each, acknowledged in turn, survives other
        // changes, a reload, a backup and its import into another service.
        let chosen = [SettingChange::RandomBattles(RandomBattles::Original), SettingChange::EncounterRate(EncounterRate::Half),
            SettingChange::TextSpeed(TextSpeed::Fast), SettingChange::AutoAdvance(AutoAdvance { enabled: true, dwell: 45 }), SettingChange::MovieSkip(true)];
        let mut file = SettingsService::open(FileStore::new(&path)).unwrap();
        for (n, change) in chosen.into_iter().enumerate() {
            assert_eq!(file.apply(change).unwrap().revision, n as u64 + 1);
        }
        let explicit = serde_json::json!({"random_battles": "Original", "encounter_rate": "Half", "text_speed": "Fast",
            "auto_advance": {"enabled": true, "dwell": 45}, "movie_skip": true});
        file.apply(SettingChange::MouseCamera(false)).unwrap();
        file.apply(SettingChange::Replace(Settings { master_volume: 70, ..file.settings().clone() })).unwrap();
        let reopened = SettingsService::open(FileStore::new(&path)).unwrap();
        assert_eq!((gameplay(reopened.settings()), reopened.settings().master_volume), (explicit.clone(), 70), "kept through other changes and a reload");
        let mut other = SettingsService::with_defaults(MemoryStore::default());
        other.import(&reopened.export()).unwrap();
        assert_eq!(gameplay(other.settings()), explicit, "a backup carries them");
        // A backup saved before the options existed imports their defaults:
        // an import replaces the settings with the backup's.
        let backup = serde_json::json!({"version": 1, "settings": old}).to_string();
        other.import(&backup).unwrap();
        assert_eq!(gameplay(other.settings()), defaults);
        std::fs::remove_dir_all(&dir).unwrap();
    }

    #[test]
    fn agents_change_every_gameplay_option_through_the_settings_commands_and_read_the_acknowledgement() {
        use super::*;
        let mut service = SettingsService::with_defaults(MemoryStore::default());
        let reply = |service: &mut SettingsService, change: serde_json::Value| service.command(&serde_json::json!({"cmd": "settings_apply", "change": change})).unwrap();
        // As an agent sends them on `xem-desktop --control`.
        for (n, (change, key, value)) in [
            (serde_json::json!({"RandomBattles": "Original"}), "random_battles", serde_json::json!("Original")),
            (serde_json::json!({"EncounterRate": "Half"}), "encounter_rate", serde_json::json!("Half")),
            (serde_json::json!({"TextSpeed": "Instant"}), "text_speed", serde_json::json!("Instant")),
            (serde_json::json!({"AutoAdvance": {"enabled": true, "dwell": 90}}), "auto_advance", serde_json::json!({"enabled": true, "dwell": 90})),
            (serde_json::json!({"MovieSkip": true}), "movie_skip", serde_json::json!(true)),
            (serde_json::json!({"RandomBattles": "NotWhilePlatforming"}), "random_battles", serde_json::json!("NotWhilePlatforming")),
        ].into_iter().enumerate() {
            let ack = reply(&mut service, change.clone());
            assert_eq!((&ack["revision"], &ack["settings"][key]), (&serde_json::json!(n + 1), &value), "{change} -> {ack}");
            assert_eq!(service.command(&serde_json::json!({"cmd": "settings"})).unwrap()[key], value, "settings shows it");
        }
        // Refused changes say why and change nothing.
        for (change, why) in [
            (serde_json::json!({"AutoAdvance": {"enabled": true, "dwell": 5}}), "auto-advance dwell must be 30–600"),
            (serde_json::json!({"TextSpeed": "Warp"}), "not a setting change"),
            (serde_json::json!({"RandomBattles": "Never"}), "not a setting change"),
        ] {
            let refused = reply(&mut service, change.clone());
            assert!(refused["error"].as_str().is_some_and(|e| e.contains(why)), "{change} -> {refused}");
        }
        assert_eq!(service.revision(), 6, "nothing refused was applied");
        assert_eq!(service.settings().auto_advance, AutoAdvance { enabled: true, dwell: 90 });
        assert_eq!(service.command(&serde_json::json!({"cmd": "status"})), None, "another command is the game's");
    }

    #[test]
    fn focus_pause_is_on_by_default_and_persists_acknowledged_changes() {
        use super::*;
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        assert!(service.settings().pause_on_focus_loss);
        service.apply(SettingChange::PauseOnFocusLoss(false)).unwrap();
        assert!(!SettingsService::open(store.clone()).unwrap().settings().pause_on_focus_loss);
        *store.1.borrow_mut() = true;
        assert!(service.apply(SettingChange::PauseOnFocusLoss(true)).is_err());
        assert!(!service.settings().pause_on_focus_loss);
    }

    #[test]
    fn fullscreen_is_off_by_default_and_persists() {
        use super::*;
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        assert!(!service.settings().presentation.fullscreen);
        service.apply(SettingChange::Fullscreen(true)).unwrap();
        assert!(SettingsService::open(store.clone()).unwrap().settings().presentation.fullscreen);
        // Settings saved before it existed open windowed.
        let (read, reset) = Settings::read(&serde_json::json!({ "presentation": { "show_fps": true } })).unwrap();
        assert!(read.presentation.show_fps && !read.presentation.fullscreen && reset.is_empty());
    }

    #[test]
    fn interpolation_decides_whether_presentation_follows_the_game_or_the_display() {
        use super::*;
        let graphics = Graphics::default();
        assert!(!graphics.interpolation && graphics.game_paced(), "off by default: the game's frames only");
        assert!(!Graphics { interpolation: true, ..graphics }.game_paced(), "on: every display refresh");
        assert!(!Graphics { renderer: Renderer::Original, ..graphics }.game_paced(), "the original's picture at every refresh");
        // A file saved with the rate it used to choose keeps its other
        // choices; the rate goes alone.
        let (read, reset) = Settings::read(&serde_json::json!({ "graphics": { "present_rate": "Display", "interpolation": true, "zoom_out": 300 } })).unwrap();
        assert!(read.graphics.interpolation && !read.graphics.game_paced());
        assert_eq!((read.graphics.zoom_out, reset), (300, vec!["graphics.present_rate".to_string()]));
    }

    #[test]
    fn rewind_history_defaults_to_thirty_seconds_and_bounds_the_interval() {
        use super::*;
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        assert_eq!(service.settings().rewind_history_seconds,30);
        service.apply(SettingChange::RewindHistorySeconds(5)).unwrap();
        service.apply(SettingChange::RewindIntervalSeconds(5)).unwrap();
        for change in [SettingChange::RewindHistorySeconds(0),SettingChange::RewindHistorySeconds(4),
            SettingChange::RewindHistorySeconds(MAX_REWIND_HISTORY_SECONDS+1),SettingChange::RewindIntervalSeconds(6)] {
            assert!(service.apply(change).is_err());
        }
        assert_eq!(SettingsService::open(store.clone()).unwrap().settings().rewind_history_seconds,5);
        *store.1.borrow_mut() = true;
        assert!(service.apply(SettingChange::RewindHistorySeconds(30)).is_err());
        assert_eq!(service.settings().rewind_history_seconds,5);
    }
    #[test]
    fn rewind_interval_defaults_to_one_second_and_is_persisted_and_validated() {
        use super::*;
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        assert_eq!(service.settings().rewind_interval_seconds,1);
        service.apply(SettingChange::RewindIntervalSeconds(3)).unwrap();
        assert_eq!(SettingsService::open(store.clone()).unwrap().settings().rewind_interval_seconds,3);
        for invalid in [0,MAX_REWIND_INTERVAL_SECONDS+1] {
            assert!(service.apply(SettingChange::RewindIntervalSeconds(invalid)).is_err());
            assert_eq!(service.settings().rewind_interval_seconds,3);
        }
        *store.1.borrow_mut() = true;
        assert!(service.apply(SettingChange::RewindIntervalSeconds(2)).is_err());
        assert_eq!(service.settings().rewind_interval_seconds,3);
    }
    #[test]
    fn fast_forward_rates_are_validated_saved_and_rejected_when_storage_fails() {
        use super::*;
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        assert_eq!(service.settings().fast_forward_rate, FastForwardRate::Fixed(10));
        for rate in [FastForwardRate::Fixed(20), FastForwardRate::Uncapped] {
            service.apply(SettingChange::FastForwardRate(rate)).unwrap();
            assert_eq!(SettingsService::open(store.clone()).unwrap().settings().fast_forward_rate,rate);
        }
        for rate in [FastForwardRate::Fixed(0), FastForwardRate::Fixed(2), FastForwardRate::Fixed(25)] {
            assert!(service.apply(SettingChange::FastForwardRate(rate)).is_err());
        }
        assert_eq!(FastForwardRate::Fixed(20).increase(),FastForwardRate::Uncapped);
        assert_eq!(FastForwardRate::Uncapped.decrease(),FastForwardRate::Fixed(20));
        assert_eq!(FastForwardRate::Fixed(1).decrease(),FastForwardRate::Fixed(1));
        *store.1.borrow_mut() = true;
        assert!(service.apply(SettingChange::FastForwardRate(FastForwardRate::Fixed(3))).is_err());
        assert_eq!(service.settings().fast_forward_rate,FastForwardRate::Uncapped);
    }
    use super::*;
    use std::cell::RefCell;
    use std::rc::Rc;

    #[derive(Clone, Default)]
    struct SharedStore(Rc<RefCell<Option<String>>>, Rc<RefCell<bool>>);

    impl SettingsStore for SharedStore {
        fn load(&self) -> std::io::Result<Option<String>> {
            Ok(self.0.borrow().clone())
        }
        fn save(&mut self, json: &str) -> std::io::Result<()> {
            if *self.1.borrow() {
                return Err(std::io::Error::other("disk full"));
            }
            *self.0.borrow_mut() = Some(json.to_owned());
            Ok(())
        }
    }

    #[test]
    fn apply_validates_acknowledges_and_notifies() {
        let mut service = SettingsService::with_defaults(MemoryStore::default());
        let seen = Rc::new(RefCell::new(Vec::new()));
        let log = seen.clone();
        service.subscribe(move |ack| log.borrow_mut().push(ack.revision));

        let ack = service.apply(SettingChange::MasterVolume(35)).unwrap();
        assert_eq!(ack.revision, 1);
        assert_eq!(ack.settings.master_volume, 35);
        assert_eq!(service.settings().master_volume, 35);

        for bad in [-1, 101, 1000] {
            let rejected = service.apply(SettingChange::MasterVolume(bad)).unwrap_err();
            assert!(matches!(rejected, Rejected::Invalid { .. }));
        }
        let jump = |keyboard: &str| SettingChange::BindGame {
            context: InputContext::Play, action: GameAction::Jump, binding: GameBinding::parse(keyboard, "North", ""),
        };
        assert!(service.apply(jump("KeyE")).is_err());
        assert_eq!(service.settings().master_volume, 35);
        assert_eq!(service.revision(), 1);

        service
            .apply(SettingChange::AudioOutput(AudioOutput::Wide))
            .unwrap();
        service.apply(jump("KeyJ")).unwrap();
        assert_eq!(*seen.borrow(), vec![1, 2, 3]);
        assert_eq!(service.settings().game_bindings.play[&GameAction::Jump].keyboard, ["KeyJ"]);
        service.apply(SettingChange::ResetGameBindings).unwrap();
        assert_eq!(service.settings().game_bindings, GameBindings::default());
    }

    #[test]
    fn binaural_runs_the_game_in_stereo_and_gives_back_the_original_mode() {
        let mut service = SettingsService::with_defaults(MemoryStore::default());
        service.apply(SettingChange::AudioOutput(AudioOutput::Wide)).unwrap();
        assert_eq!(service.settings().game_sound_mode(), 2);
        let on = Binaural { enabled: true, ..service.settings().binaural };
        service.apply(SettingChange::Binaural(on)).unwrap();
        assert_eq!(service.settings().game_sound_mode(), 1, "Stereo under Binaural, not Wide");
        assert_eq!(service.settings().audio_output, AudioOutput::Wide, "the original preference is kept");
        service.apply(SettingChange::Binaural(Binaural { enabled: false, ..on })).unwrap();
        assert_eq!(service.settings().game_sound_mode(), 2, "disabling restores Wide");
        // Choosing an original mode (the panel, the game's own Sound screen)
        // ends Binaural.
        service.apply(SettingChange::Binaural(on)).unwrap();
        service.apply(SettingChange::AudioOutput(AudioOutput::Mono)).unwrap();
        assert!(!service.settings().binaural.enabled);
        assert_eq!(service.settings().game_sound_mode(), 0);
        let rejected = service.apply(SettingChange::Binaural(Binaural { music_immersion: 101, ..on })).unwrap_err();
        assert!(matches!(rejected, Rejected::Invalid { .. }));
        // An old file without the block reads with its defaults, a bad value
        // alone resets.
        let (read, reset) = Settings::read(&serde_json::json!({"audio_output": "Mono", "binaural": {"enabled": true, "music_immersion": 400}})).unwrap();
        assert!(read.binaural.enabled);
        assert_eq!(read.binaural.music_immersion, Binaural::default().music_immersion);
        assert_eq!(reset, ["binaural.music_immersion"]);
    }

    #[test]
    fn changes_persist_and_reload() {
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        assert_eq!(service.settings(), &Settings::default());
        service.apply(SettingChange::Scale(Scale::Integer)).unwrap();
        service
            .apply(SettingChange::Filter(Filter::Linear))
            .unwrap();
        service.apply(SettingChange::ShowFps(true)).unwrap();

        let reopened = SettingsService::open(store.clone()).unwrap();
        assert_eq!(reopened.settings(), service.settings());
        assert_eq!(reopened.settings().presentation.scale, Scale::Integer);
    }

    #[test]
    fn storage_failure_rejects_and_keeps_settings() {
        let store = SharedStore::default();
        let mut service = SettingsService::open(store.clone()).unwrap();
        *store.1.borrow_mut() = true;
        let rejected = service.apply(SettingChange::MasterVolume(10)).unwrap_err();
        assert!(matches!(rejected, Rejected::Storage { .. }));
        assert_eq!(
            service.settings().master_volume,
            Settings::default().master_volume
        );
        assert_eq!(service.revision(), 0);
    }

    #[test]
    fn invalid_or_partial_files() {
        // An invalid setting (a volume out of range, a movie mode no longer
        // offered) takes its default alone; the others stay as saved.
        let json = r#"{"master_volume": 250, "audio_output": "Mono",
            "app_bindings": {"ImportState": {"keyboard": ["F11"], "gamepad": ["Start", "North"]}},
            "graphics": {"zoom_out": 300, "filtering": {"movies": "Bicubic", "world": "Linear"}}}"#;
        let service = SettingsService::open(MemoryStore { json: Some(json.into()) }).unwrap();
        let settings = service.settings();
        // A binding is one setting: kept whole.
        assert_eq!(
            serde_json::to_value(&settings.app_bindings).unwrap()["ImportState"],
            serde_json::json!({"keyboard": ["F11"], "gamepad": ["Start", "North"]})
        );
        assert_eq!(settings.master_volume, Settings::default().master_volume);
        assert_eq!(settings.audio_output, AudioOutput::Mono);
        assert_eq!(settings.graphics.zoom_out, 300);
        assert_eq!(settings.graphics.filtering.movies, MovieFilter::Enhanced);
        assert_eq!(settings.graphics.filtering.world, Filter::Linear);
        let (_, reset) = Settings::read(&serde_json::from_str(json).unwrap()).unwrap();
        assert_eq!(reset, ["graphics.filtering.movies", "master_volume"]);
        let store = MemoryStore {
            json: Some("not json".into()),
        };
        assert!(matches!(
            SettingsService::open(store),
            Err(LoadError::Invalid(_))
        ));
        // Missing fields take their defaults.
        let store = MemoryStore {
            json: Some(r#"{"audio_output": "Mono"}"#.into()),
        };
        let service = SettingsService::open(store).unwrap();
        assert_eq!(service.settings().audio_output, AudioOutput::Mono);
        assert_eq!(service.settings().master_volume, 80);
    }

    #[test]
    fn a_shortcut_list_saved_before_an_action_existed_reads_its_default_binding_less_a_chord_another_holds() {
        // Saved before ToggleAutoAdvance existed: its default, T and
        // Back+West, and nothing reset.
        let mut old = serde_json::to_value(Settings::default()).unwrap();
        old["app_bindings"].as_object_mut().unwrap().remove("ToggleAutoAdvance");
        let (read, reset) = Settings::read(&old).unwrap();
        assert!(reset.is_empty(), "{reset:?}");
        assert_eq!(read, Settings::default());
        // A chord the player gave another action stays that action's; the
        // other chord is the new action's.
        old["app_bindings"]["ImportBios"] = serde_json::json!({"keyboard": ["KeyT"], "gamepad": []});
        let (read, _) = Settings::read(&old).unwrap();
        assert_eq!(read.app_bindings[&AppAction::ToggleAutoAdvance], AppBinding::parse("", "Back+West"));
        assert_eq!(read.app_bindings[&AppAction::ImportBios], AppBinding::parse("KeyT", ""));
        // An action the player unbound stays unbound, and a saved file
        // keeps the list it read.
        old["app_bindings"]["ToggleAutoAdvance"] = serde_json::json!({"keyboard": [], "gamepad": []});
        assert_eq!(Settings::read(&old).unwrap().0.app_bindings[&AppAction::ToggleAutoAdvance], AppBinding::default());
        let store = SharedStore::default();
        *store.0.borrow_mut() = Some(r#"{"app_bindings": {"SaveState": {"keyboard": ["F6"], "gamepad": []}}}"#.into());
        let mut service = SettingsService::open(store.clone()).unwrap();
        assert_eq!(service.settings().app_bindings[&AppAction::SaveState], AppBinding::parse("F6", ""), "the player's own binding");
        assert_eq!(service.settings().app_bindings[&AppAction::ToggleAutoAdvance], AppBinding::parse("KeyT", "Back+West"));
        service.apply(SettingChange::MasterVolume(10)).unwrap();
        let saved: serde_json::Value = serde_json::from_str(store.0.borrow().as_deref().unwrap()).unwrap();
        assert_eq!(saved["app_bindings"]["ToggleAutoAdvance"], serde_json::json!({"keyboard": ["KeyT"], "gamepad": ["Back", "West"]}));
    }

    #[test]
    fn file_store_round_trip() {
        let dir = std::env::temp_dir().join(format!("xem-settings-{}", std::process::id()));
        let path = dir.join("nested").join("settings.json");
        let mut service = SettingsService::open(FileStore::new(&path)).unwrap();
        service.apply(SettingChange::MasterVolume(42)).unwrap();
        let reopened = SettingsService::open(FileStore::new(&path)).unwrap();
        assert_eq!(reopened.settings().master_volume, 42);
        std::fs::remove_dir_all(dir).unwrap();
    }
}
