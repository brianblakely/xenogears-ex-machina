//! Native Settings operations, including SDL's asynchronous platform file picker.
use crate::{Game, sdl};
use sdl3_sys::everything::*;
use std::ffi::{CStr, CString, c_char, c_void};
use std::path::PathBuf;
use std::sync::mpsc::{self, Receiver, Sender};
use xem_core::devices::card_file::MAX_FILE;
use xem_settings::{AppAction, AppRequest, FileStore, SettingChange, SettingsService};

struct Dialog {
    action: AppAction,
    path: Result<Option<String>,String>,
}
struct Callback {
    action: AppAction,
    sender: Sender<Dialog>,
}

/// The operations that open a file picker, which only a player answers.
const PICKERS: [AppAction; 9] = [
    AppAction::ExportState, AppAction::ImportState, AppAction::ExportCard1, AppAction::ImportCard1, AppAction::ExportCard2,
    AppAction::ImportCard2, AppAction::ExportSettings, AppAction::ImportSettings, AppAction::ImportBios,
];

/// An agent's app operation (`--control`'s `app {request}`, in the window
/// and in a headset): the panel's and the bindings' operations as
/// `xem_settings::AppRequest` (`{"Action": "RetryStorage"}`; `{"Rewind":
/// true}` holds rewind as a held binding does until `{"Rewind": false}`),
/// carried out as theirs are ([`Operations::execute`]), but not the file
/// pickers: agents name the file instead (`state_save`, `state_load`,
/// `card_import`, `card_export`, `bios_import`), nor Toggle first-person
/// view, whose command (`first_person`) answers the view it leaves.
pub fn agent_request(command: &serde_json::Value) -> Result<AppRequest,String> {
    let request = command.get("request").cloned()
        .ok_or(r#"app needs request: an app operation, such as {"Action": "RetryStorage"} or {"Rewind": true}"#)?;
    let request: AppRequest = serde_json::from_value(request).map_err(|e| format!("not an app operation: {e}"))?;
    if let AppRequest::Action(action) = request && PICKERS.contains(&action) {
        return Err(format!("{action:?} opens a file picker, which only a player answers: agents name the file \
            (state_save, state_load, card_import, card_export, bios_import)"));
    }
    if request == AppRequest::Action(AppAction::ToggleFirstPerson) {
        return Err("agents turn the first-person view with first_person {on, yaw, pitch}, which answers the view".into());
    }
    Ok(request)
}

pub struct Operations {
    pub directory: Option<PathBuf>,
    sender: Sender<Dialog>,
    results: Receiver<Dialog>,
    pub busy: bool,
    export: Option<Vec<u8>>,
}
impl Default for Operations {
    fn default() -> Self {
        let (sender,results) = mpsc::channel();
        Self { directory: FileStore::default_path().and_then(|p| p.parent().map(PathBuf::from)), sender, results, busy: false, export: None }
    }
}
impl Operations {
    pub fn poll(&mut self, mut game: Option<&mut Game>, settings: &mut SettingsService) -> Option<(AppAction,Result<String,String>)> {
        let dialog = self.results.try_recv().ok()?;
        self.busy = false;
        let export = self.export.take();
        Some((dialog.action,(|| {
            let Some(path) = dialog.path? else { return Ok("File operation canceled.".into()) };
            if let Some(bytes) = export {
                write(&path,&bytes)?;
                return Ok("Backup written and closed.".into());
            }
            match dialog.action {
                AppAction::ImportSettings => {
                    let bytes = read(&path,1024*1024)?;
                    settings.import(std::str::from_utf8(&bytes).map_err(|e| e.to_string())?)?;
                }
                AppAction::ImportState => game.as_deref_mut().ok_or("Boot a game before importing a state.")?.restore_bytes(&read(&path,64*1024*1024)?)?,
                AppAction::ImportCard1 | AppAction::ImportCard2 => {
                    // Any card or save file, by its contents; the message
                    // says what was found.
                    let slot = usize::from(dialog.action == AppAction::ImportCard2);
                    let game = game.as_deref_mut().ok_or("Boot a game before importing a card.")?;
                    return game.import_card(slot,&read(&path,MAX_FILE)?,&file_name(&path));
                }
                AppAction::ImportBios => return self.import_bios_file(game.as_deref_mut(),&path),
                _ => return Err("Unexpected file operation.".into()),
            }
            Ok("Backup imported.".into())
        })()))
    }

    /// Import BIOS once the player picked `path` (a file, or an Android
    /// document), or an agent named it (`xem-desktop --control`'s
    /// `bios_import`): [`Self::import_bios`] of what it holds.
    pub(crate) fn import_bios_file(&self, game: Option<&mut Game>, path: &str) -> Result<String,String> {
        self.import_bios(game,&read(path,xem_core::bios::MAX_FILE)?)
    }

    /// Import BIOS's work once the player picked `rom`: checked
    /// (`xem_core::bios::check`), kept in the app's directory for every boot
    /// (`xem_game::storage::BIOS_FILE`), then given to the running game.
    /// What it did; a refused file or a failed write keeps nothing.
    fn import_bios(&self, game: Option<&mut Game>, rom: &[u8]) -> Result<String,String> {
        xem_core::bios::check(rom).map_err(|e| format!("BIOS not imported: {e}."))?;
        let directory = self.directory.as_ref().ok_or("No application storage directory.")?;
        xem_storage::replace(&directory.join(xem_game::storage::BIOS_FILE),rom).map_err(|e| format!("BIOS not imported: {e}."))?;
        if let Some(game) = game { game.install_bios(rom)?; }
        Ok("BIOS imported: save titles use its Kanji font.".into())
    }

    pub fn execute(&mut self, request: AppRequest, window: Option<&sdl::Window>, mut game: Option<&mut Game>, settings: &mut SettingsService) -> Result<String,String> {
        if let AppRequest::Rewind(held) | AppRequest::UiRewind(held) = request {
            game.as_deref_mut().ok_or("No game session.")?.set_rewind_held(held,matches!(request,AppRequest::UiRewind(_)))?;
            return Ok(if held { "Rewinding while held." } else { "Rewind released." }.into());
        }
        if self.busy { return Err("Finish the open file picker first.".into()); }
        if let AppRequest::Debug(request) = request {
            game.as_deref_mut().ok_or("Boot a game before using Debug.")?
                .command(&serde_json::json!({"cmd":"debug", "request":request}))?;
            return Ok("Starting debug test…".into());
        }
        let AppRequest::Action(action) = request else { unreachable!() };
        match action {
            AppAction::Reset => {
                game.as_deref_mut().ok_or("No game session.")?.command(&serde_json::json!({"cmd":"reset"}))?;
                Ok("Console reset.".into())
            }
            AppAction::SaveState | AppAction::LoadState => {
                let path = self.directory.as_ref().ok_or("No application storage directory.")?.join("saves/state.xemstate");
                let game = game.as_deref_mut().ok_or("No game session.")?;
                game.command(&serde_json::json!({"cmd":if action == AppAction::SaveState { "state_save" } else { "state_load" },"path":path}))?;
                Ok(format!("State {}: {}",if action == AppAction::SaveState {"written"} else {"restored"},path.display()))
            }
            AppAction::Rewind => Err("Rewind requires a held input.".into()),
            AppAction::TogglePauseOnFocusLoss => {
                settings.apply(SettingChange::PauseOnFocusLoss(!settings.settings().pause_on_focus_loss)).map_err(|e| e.to_string())?;
                Ok("Focus pause preference saved.".into())
            }
            AppAction::ToggleRewind => {
                let enabled = settings.settings().rewind_enabled;
                settings.apply(SettingChange::RewindEnabled(!enabled)).map_err(|e| e.to_string())?;
                Ok(if enabled { "Rewind history disabled." } else { "Rewind history enabled." }.into())
            }
            AppAction::ToggleTriggerInversion => {
                settings.apply(SettingChange::InvertTriggerChords(!settings.settings().invert_trigger_chords)).map_err(|e| e.to_string())?;
                Ok("LT/RT chord preference saved.".into())
            }
            AppAction::ToggleFastForward => {
                let game = game.as_deref_mut().ok_or("No game session.")?;
                let reply = game.command(&serde_json::json!({"cmd":"fast_forward"}))?;
                Ok(if reply["fast_forward"] == true { "Fast-forward on. Music keeps its normal tempo and pitch." } else { "Fast-forward off." }.into())
            }
            AppAction::IncreaseFastForwardRate | AppAction::DecreaseFastForwardRate => {
                let rate = settings.settings().fast_forward_rate;
                settings.apply(SettingChange::FastForwardRate(if action == AppAction::IncreaseFastForwardRate { rate.increase() } else { rate.decrease() }))
                    .map_err(|e| e.to_string())?;
                Ok("Fast-forward rate saved.".into())
            }
            AppAction::IncreaseRewindInterval | AppAction::DecreaseRewindInterval => {
                let seconds = settings.settings().rewind_interval_seconds;
                let next = if action == AppAction::IncreaseRewindInterval { seconds.saturating_add(1).min(settings.settings().rewind_history_seconds) }
                    else { seconds.saturating_sub(1).max(1) };
                settings.apply(SettingChange::RewindIntervalSeconds(next)).map_err(|e| e.to_string())?;
                Ok("Rewind interval saved.".into())
            }
            AppAction::IncreaseRewindHistory | AppAction::DecreaseRewindHistory => {
                let seconds = settings.settings().rewind_history_seconds;
                let next = if action == AppAction::IncreaseRewindHistory { seconds.saturating_add(1).min(xem_settings::MAX_REWIND_HISTORY_SECONDS) }
                    else { seconds.saturating_sub(1).max(settings.settings().rewind_interval_seconds) };
                settings.apply(SettingChange::RewindHistorySeconds(next)).map_err(|e| e.to_string())?;
                Ok("Rewind history length saved.".into())
            }
            AppAction::ToggleFps => {
                settings.apply(SettingChange::ShowFps(!settings.settings().presentation.show_fps)).map_err(|e| e.to_string())?;
                Ok("FPS display preference saved.".into())
            }
            AppAction::ToggleAutoAdvance => {
                let auto = settings.settings().auto_advance;
                settings.apply(SettingChange::AutoAdvance(xem_settings::AutoAdvance { enabled: !auto.enabled, ..auto })).map_err(|e| e.to_string())?;
                Ok(if auto.enabled { "Auto-advance dialogue off." } else { "Auto-advance dialogue on: choices still wait for you." }.into())
            }
            AppAction::RetryStorage => {
                game.as_deref_mut().ok_or("No game session.")?.retry_storage()?;
                Ok("Pending card writes completed.".into())
            }
            AppAction::RequestPersistence => Ok("Native saves use application files. Export backups for another device.".into()),
            // A headset's loop takes it before it comes here (xr_play).
            AppAction::Recenter => Err("Recentering places a headset's view; play in a headset to use it.".into()),
            // The views take it before it comes here (`View::pad_buttons`, xr_play).
            AppAction::ToggleFirstPerson => Err("First-person view toggles in a view that shows the game.".into()),
            _ => {
                let window = window.ok_or("File pickers are available in windowed play.")?;
                self.export = match action {
                    AppAction::ExportSettings => Some(settings.export().into_bytes()),
                    AppAction::ExportState => Some(game.as_deref_mut().ok_or("No game session.")?.snapshot_bytes()?),
                    AppAction::ExportCard1 | AppAction::ExportCard2 => {
                        let slot = usize::from(action == AppAction::ExportCard2);
                        Some(game.as_deref_mut().ok_or("No game session.")?.card_image(slot).ok_or("No card in slot.")?)
                    }
                    _ => None,
                };
                let callback = Box::new(Callback { action, sender: self.sender.clone() });
                self.busy = true;
                // SAFETY: SDL calls the callback once; it owns this box until
                // then. No borrowed game/UI data crosses the callback thread.
                unsafe {
                    let userdata = Box::into_raw(callback).cast();
                    if self.export.is_some() {
                        SDL_ShowSaveFileDialog(Some(chosen),userdata,window.raw,std::ptr::null(),0,std::ptr::null());
                    } else {
                        SDL_ShowOpenFileDialog(Some(chosen),userdata,window.raw,std::ptr::null(),0,std::ptr::null(),false);
                    }
                }
                Ok(if action == AppAction::ImportBios { "Choose your PS1 BIOS image." } else { "Choose a backup file." }.into())
            }
        }
    }
}
unsafe extern "C" fn chosen(userdata: *mut c_void, files: *const *const c_char, _: i32) {
    // SAFETY: the box was handed to this callback once; SDL's strings remain
    // valid until return and are copied before sending to the main thread.
    let callback = unsafe { Box::from_raw(userdata.cast::<Callback>()) };
    let path = if files.is_null() { Err(sdl::error()) }
        else if unsafe { *files }.is_null() { Ok(None) }
        else { unsafe { CStr::from_ptr(*files) }.to_str().map(|s| Some(s.to_owned())).map_err(|e| e.to_string()) };
    let _ = callback.sender.send(Dialog { action: callback.action, path });
}

/// A picked file's name, which names a raw single save: a path's last
/// part, or an Android document URI's (its decoded document id after the
/// last '/': the file's name where the provider puts its path there).
fn file_name(path: &str) -> String {
    let Some(document) = path.strip_prefix("content://") else {
        return std::path::Path::new(path).file_name().map(|name| name.to_string_lossy().into_owned()).unwrap_or_default();
    };
    let mut bytes = Vec::new();
    let mut at = 0;
    let raw = document.as_bytes();
    while at < raw.len() {
        let hex = raw.get(at + 1..at + 3).and_then(|h| std::str::from_utf8(h).ok()).and_then(|h| u8::from_str_radix(h, 16).ok());
        match (raw[at], hex) {
            (b'%', Some(byte)) => { bytes.push(byte); at += 3; }
            (byte, _) => { bytes.push(byte); at += 1; }
        }
    }
    String::from_utf8_lossy(&bytes).rsplit('/').next().unwrap_or_default().to_string()
}

fn read(path: &str, limit: usize) -> Result<Vec<u8>,String> {
    if !path.starts_with("content://") { return xem_storage::read(std::path::Path::new(path),limit).map_err(|e| e.to_string()); }
    let name = CString::new(path).map_err(|e| e.to_string())?;
    // SAFETY: valid strings; the stream is owned here and closed on all paths.
    let stream = unsafe { SDL_IOFromFile(name.as_ptr(),c"rb".as_ptr()) };
    if stream.is_null() { return Err(sdl::error()); }
    let result = (|| {
        let mut bytes = Vec::new(); let mut buffer = [0u8;8192];
        loop {
            let len = (limit+1-bytes.len()).min(buffer.len());
            let count = unsafe { SDL_ReadIO(stream,buffer.as_mut_ptr().cast(),len) };
            bytes.extend_from_slice(&buffer[..count]);
            if bytes.len() > limit { return Err("Backup exceeds the size limit.".into()); }
            if count == 0 {
                if unsafe { SDL_GetIOStatus(stream) } != SDL_IO_STATUS_EOF { return Err(sdl::error()); }
                return Ok(bytes);
            }
        }
    })();
    let closed = unsafe { SDL_CloseIO(stream) };
    if !closed && result.is_ok() { Err(sdl::error()) } else { result }
}
fn write(path: &str, bytes: &[u8]) -> Result<(),String> {
    if !path.starts_with("content://") { return xem_storage::replace(std::path::Path::new(path),bytes).map_err(|e| e.to_string()); }
    let name = CString::new(path).map_err(|e| e.to_string())?;
    // SAFETY: as in read; Android's picker supplies an SDL-supported URI.
    let stream = unsafe { SDL_IOFromFile(name.as_ptr(),c"wb".as_ptr()) };
    if stream.is_null() { return Err(sdl::error()); }
    let count = unsafe { SDL_WriteIO(stream,bytes.as_ptr().cast(),bytes.len()) };
    let flushed = unsafe { SDL_FlushIO(stream) };
    let closed = unsafe { SDL_CloseIO(stream) };
    if count != bytes.len() || !flushed || !closed { Err(sdl::error()) } else { Ok(()) }
}

#[cfg(test)]
mod tests {
    #[test]
    fn an_imported_bios_is_kept_for_every_boot_and_a_file_that_is_not_one_is_refused() {
        let directory = std::env::temp_dir().join(format!("xem-import-bios-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&directory);
        let operations = super::Operations { directory: Some(directory.clone()), ..Default::default() };
        let refused = operations.import_bios(None, &[7; 1000]).unwrap_err();
        assert_eq!(refused, "BIOS not imported: a PS1 BIOS image is 512 KiB (524288 bytes), not 1000 bytes.");
        let blank = operations.import_bios(None, &vec![0; xem_core::bios::BIOS_SIZE]).unwrap_err();
        assert!(blank.ends_with("a full-width letter or digit is blank."), "{blank}");
        assert_eq!(xem_game::storage::stored_bios(&directory), None, "nothing kept");
        // A plausible font made here: glyphs 147-208, the full-width digits
        // and Latin letters, each different.
        let mut rom = vec![0; xem_core::bios::BIOS_SIZE];
        for glyph in 147..=208 { rom[0x66000 + glyph * 30 + 2] = (glyph - 146) as u8; }
        assert_eq!(operations.import_bios(None, &rom).unwrap(), "BIOS imported: save titles use its Kanji font.");
        let stored = xem_game::storage::stored_bios(&directory).expect("kept for the boots");
        assert_eq!(stored, directory.join("bios.bin"));
        assert_eq!(std::fs::read(&stored).unwrap(), rom);
        std::fs::remove_dir_all(directory).unwrap();
    }

    #[test]
    fn a_picked_files_name_comes_from_its_path_or_its_android_document() {
        assert_eq!(super::file_name("/home/user/saves/BASLUS-006640"), "BASLUS-006640");
        assert_eq!(super::file_name("content://com.android.externalstorage.documents/document/primary%3ADownload%2FBASLUS-00664%3A"), "BASLUS-00664:");
        assert_eq!(super::file_name("content://com.android.externalstorage.documents/document/primary%3ADownload%2Fcard1.mcd"), "card1.mcd");
        assert_eq!(super::file_name("content://com.android.providers.downloads.documents/document/msf%3A1234"), "msf:1234", "a provider that names no file");
    }
}
