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

pub const MAX_VOLUME: u8 = 100;

/// How the audio mix is sent to the output device.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum AudioOutput {
    Mono,
    #[default]
    Stereo,
    /// Stereo with the side channel widened.
    Wide,
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
#[serde(default)]
pub struct Settings {
    pub master_volume: u8,
    pub audio_output: AudioOutput,
    pub presentation: Presentation,
    /// Placeholder until the input phase defines the binding model: an action
    /// mapped to a host input name. Unbound actions use the host default.
    pub bindings: BTreeMap<Action, String>,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            master_volume: 80,
            audio_output: AudioOutput::default(),
            presentation: Presentation::default(),
            bindings: BTreeMap::new(),
        }
    }
}

/// One requested change. Values are as received from the caller and are
/// validated by [`SettingsService::apply`].
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum SettingChange {
    MasterVolume(i32),
    AudioOutput(AudioOutput),
    Scale(Scale),
    Filter(Filter),
    ShowFps(bool),
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
        Ok(next)
    }

    /// Checks values that the types alone do not constrain (for loaded files).
    fn validate(&self) -> Result<(), String> {
        if self.master_volume > MAX_VOLUME {
            return Err(format!("volume must be 0-{MAX_VOLUME}"));
        }
        if self.bindings.values().any(|input| input.trim().is_empty()) {
            return Err("input name is empty".into());
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
        if let Some(dir) = self.path.parent() {
            std::fs::create_dir_all(dir)?;
        }
        let temporary = self.path.with_extension("json.tmp");
        std::fs::write(&temporary, json)?;
        std::fs::rename(&temporary, &self.path)
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
