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
pub mod controls;
pub mod graphics;
pub use graphics::{EffectLevels, Fidelity, Graphics, GraphicsProfile, MAX_ZOOM_OUT, PresentRate, QualityTier, XrScreen, ZOOM_STEP};
pub use controls::{AppAction, AppBinding, AppInput, AppRequest, APP_ACTIONS};

pub const MAX_VOLUME: u8 = 100;
pub use xem_core::time::{FastForwardRate, FAST_FORWARD_RATES, DEFAULT_REWIND_INTERVAL_SECONDS, MAX_REWIND_INTERVAL_SECONDS, DEFAULT_REWIND_HISTORY_SECONDS, MAX_REWIND_HISTORY_SECONDS};

/// How the audio mix is sent to the output device.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum AudioOutput {
    Mono,
    #[default]
    Stereo,
    /// Stereo with the side channel widened.
    Wide,
}

impl AudioOutput {
    /// One stereo frame as this output sends it (Wide widens the side
    /// channel by half; the game's own Wide sound mode is separate).
    pub fn mix(self, left: f32, right: f32) -> (f32, f32) {
        let (mid, side) = ((left + right) / 2.0, (left - right) / 2.0);
        match self {
            AudioOutput::Mono => (mid, mid),
            AudioOutput::Stereo => (left, right),
            AudioOutput::Wide => (mid + side * 1.5, mid - side * 1.5),
        }
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

/// Game actions that can be rebound.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub enum Action {
    Confirm,
    Cancel,
    Run,
    Jump,
    CameraLeft,
    CameraRight,
}

#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default)]
pub struct Presentation {
    pub scale: Scale,
    pub filter: Filter,
    pub show_fps: bool,
}

/// The complete application settings.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct Settings {
    pub master_volume: u8,
    pub audio_output: AudioOutput,
    pub presentation: Presentation,
    pub graphics: Graphics,
    /// Placeholder until the input phase defines the binding model: an action
    /// mapped to a host input name. Unbound actions use the host default.
    pub bindings: BTreeMap<Action, String>,
    pub app_bindings: BTreeMap<AppAction, AppBinding>,
    pub fast_forward_rate: FastForwardRate,
    pub invert_trigger_chords: bool,
    pub rewind_enabled: bool,
    pub rewind_interval_seconds: u32,
    pub rewind_history_seconds: u32,
    pub pause_on_focus_loss: bool,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            master_volume: 80,
            audio_output: AudioOutput::default(),
            presentation: Presentation::default(),
            graphics: Graphics::default(),
            bindings: BTreeMap::new(),
            app_bindings: controls::defaults(),
            fast_forward_rate: FastForwardRate::default(),
            invert_trigger_chords: false,
            rewind_enabled: true,
            rewind_interval_seconds: DEFAULT_REWIND_INTERVAL_SECONDS,
            rewind_history_seconds: DEFAULT_REWIND_HISTORY_SECONDS,
            pause_on_focus_loss: true,
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
    RewindEnabled(bool),
    RewindIntervalSeconds(u32),
    RewindHistorySeconds(u32),
    MasterVolume(i32),
    AudioOutput(AudioOutput),
    Scale(Scale),
    Filter(Filter),
    ShowFps(bool),
    /// The whole graphics block, validated.
    Graphics(Graphics),
    PauseOnFocusLoss(bool),
    Bind { action: Action, input: String },
    Unbind(Action),
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
    fn with(&self, change: &SettingChange) -> Result<Settings, String> {
        let mut next = self.clone();
        match change {
            SettingChange::Replace(settings) => { settings.validate()?; next = settings.clone(); }
            SettingChange::BindApp { action, binding } => { binding.validate()?; next.app_bindings.insert(*action,binding.clone()); }
            SettingChange::FastForwardRate(rate) => next.fast_forward_rate = *rate,
            SettingChange::InvertTriggerChords(invert) => next.invert_trigger_chords = *invert,
            SettingChange::RewindEnabled(enabled) => next.rewind_enabled = *enabled,
            SettingChange::RewindIntervalSeconds(seconds) => next.rewind_interval_seconds = *seconds,
            SettingChange::RewindHistorySeconds(seconds) => next.rewind_history_seconds = *seconds,
            SettingChange::MasterVolume(volume) => {
                next.master_volume = u8::try_from(*volume)
                    .ok()
                    .filter(|v| *v <= MAX_VOLUME)
                    .ok_or_else(|| format!("volume must be 0-{MAX_VOLUME}"))?;
            }
            SettingChange::AudioOutput(mode) => next.audio_output = *mode,
            SettingChange::Scale(scale) => next.presentation.scale = *scale,
            SettingChange::Filter(filter) => next.presentation.filter = *filter,
            SettingChange::ShowFps(show) => next.presentation.show_fps = *show,
            SettingChange::Graphics(graphics) => next.graphics = *graphics,
            SettingChange::PauseOnFocusLoss(enabled) => next.pause_on_focus_loss = *enabled,
            SettingChange::Bind { action, input } => {
                let input = input.trim();
                if input.is_empty() {
                    return Err("input name is empty".into());
                }
                next.bindings.insert(*action, input.to_owned());
            }
            SettingChange::Unbind(action) => {
                next.bindings.remove(action);
            }
        }
        next.validate()?;
        Ok(next)
    }

    /// Checks values that the types alone do not constrain (for loaded files).
    fn validate(&self) -> Result<(), String> {
        self.fast_forward_rate.validate()?;
        self.graphics.validate()?;
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
        if self.bindings.values().any(|input| input.trim().is_empty()) {
            return Err("input name is empty".into());
        }
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

    /// `settings.json` in the platform configuration directory:
    /// `$XDG_CONFIG_HOME` or `~/.config` on Unix, `%APPDATA%` on Windows,
    /// `~/Library/Application Support` on macOS. Hosts without these (Android,
    /// browsers) pass their own location to [`FileStore::new`] or use another store.
    pub fn default_path() -> Option<PathBuf> {
        let env = |name| {
            std::env::var_os(name)
                .filter(|v| !v.is_empty())
                .map(PathBuf::from)
        };
        let base = if cfg!(windows) {
            env("APPDATA")
        } else if cfg!(target_os = "macos") {
            env("HOME").map(|home| home.join("Library/Application Support"))
        } else {
            env("XDG_CONFIG_HOME").or_else(|| env("HOME").map(|home| home.join(".config")))
        }?;
        Some(base.join("xenogears-ex-machina").join("settings.json"))
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
        struct Backup { version: u32, settings: Settings }
        let backup: Backup = serde_json::from_str(text).map_err(|e| e.to_string())?;
        if backup.version != 1 { return Err("incompatible settings backup version".into()); }
        self.apply(SettingChange::Replace(backup.settings)).map_err(|e| e.to_string())
    }
    /// Opens the settings saved in `store`, or the defaults if none were saved.
    pub fn open(store: impl SettingsStore + 'static) -> Result<Self, LoadError> {
        let settings = match store.load().map_err(LoadError::Io)? {
            Some(json) => {
                let settings: Settings = serde_json::from_str(&json)
                    .map_err(|error| LoadError::Invalid(error.to_string()))?;
                settings.validate().map_err(LoadError::Invalid)?;
                settings
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
}

#[cfg(test)]
mod tests {
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

    #[test]
    fn audio_outputs_mix_mid_and_side() {
        assert_eq!(AudioOutput::Mono.mix(1.0, 0.0), (0.5, 0.5));
        assert_eq!(AudioOutput::Stereo.mix(1.0, 0.0), (1.0, 0.0));
        assert_eq!(AudioOutput::Wide.mix(1.0, 0.0), (1.25, -0.25));
    }

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
        assert!(
            service
                .apply(SettingChange::Bind {
                    action: Action::Jump,
                    input: " ".into()
                })
                .is_err()
        );
        assert_eq!(service.settings().master_volume, 35);
        assert_eq!(service.revision(), 1);

        service
            .apply(SettingChange::AudioOutput(AudioOutput::Wide))
            .unwrap();
        service
            .apply(SettingChange::Bind {
                action: Action::Jump,
                input: "Space".into(),
            })
            .unwrap();
        assert_eq!(*seen.borrow(), vec![1, 2, 3]);
        assert_eq!(service.settings().bindings[&Action::Jump], "Space");
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
        let store = MemoryStore {
            json: Some(r#"{"master_volume": 250}"#.into()),
        };
        assert!(matches!(
            SettingsService::open(store),
            Err(LoadError::Invalid(_))
        ));
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
