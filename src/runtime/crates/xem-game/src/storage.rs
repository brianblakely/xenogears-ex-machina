//! Native file operations at the host boundary; never executed from a session log.
use std::path::{Path, PathBuf};
use serde_json::{Value, json};
use xem_core::{control::Session, module::GameModule};
use xem_core::devices::card::{CardStorage, formatted, CARD_SIZE};
use xem_core::devices::card_file::{CardFormat, MAX_FILE};

/// Images as files `card1.mcd` and `card2.mcd` in a directory; a missing
/// file is a fresh formatted card. The card a change from outside the game
/// replaced is `card1.previous.mcd` (`card2.previous.mcd`) beside it.
pub struct FileStorage {
    pub dir: PathBuf,
}

impl FileStorage {
    pub fn path(&self, slot: usize) -> PathBuf {
        self.dir.join(format!("card{}.mcd", slot + 1))
    }

    pub fn previous_path(&self, slot: usize) -> PathBuf {
        self.dir.join(format!("card{}.previous.mcd", slot + 1))
    }
}

impl CardStorage for FileStorage {
    fn load(&mut self, slot: usize) -> std::io::Result<Option<Vec<u8>>> {
        match xem_storage::read(&self.path(slot),CARD_SIZE) {
            Ok(image) => Ok(Some(image)),
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(Some(formatted())),
            Err(error) => Err(error),
        }
    }

    fn store(&mut self, slot: usize, image: &[u8]) -> std::io::Result<()> {
        xem_storage::replace(&self.path(slot), image)
    }

    fn load_previous(&mut self, slot: usize) -> std::io::Result<Option<Vec<u8>>> {
        match xem_storage::read(&self.previous_path(slot),CARD_SIZE) {
            Ok(image) => Ok(Some(image)),
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(None),
            Err(error) => Err(error),
        }
    }

    fn store_previous(&mut self, slot: usize, image: &[u8]) -> std::io::Result<()> {
        xem_storage::replace(&self.previous_path(slot), image)
    }
}

/// The explicit file commands: states, and memory cards and their saves
/// (`card_list` is the shared control layer's).
const FILE_COMMANDS: [&str; 8] = ["state_save", "state_load", "card_import", "card_export", "card_restore_previous", "save_import", "save_export", "save_delete"];

/// Explicit commands submitted by the native caller, outside the scheduled game log.
pub fn command<M: GameModule>(session: &mut Session<M>, command: &Value) -> Value {
    let cmd = command.get("cmd").and_then(Value::as_str).unwrap_or("");
    if !FILE_COMMANDS.contains(&cmd) { return session.execute(command); }
    let result = (|| -> Result<Value,String> {
        let path = || command.get("path").and_then(Value::as_str).map(Path::new).ok_or_else(|| format!("{cmd} needs path"));
        match cmd {
            "state_save" => {
                let data = session.snapshot_bytes()?;
                xem_storage::replace(path()?, &data).map_err(|e| e.to_string())?;
                Ok(json!({"saved":true,"bytes":data.len()}))
            }
            "state_load" => {
                let data = xem_storage::read(path()?,xem_core::snapshot::MAX_STATE).map_err(|e| e.to_string())?;
                session.restore_bytes(&data)?;
                Ok(session.execute(&json!({"cmd":"status"})))
            }
            _ => {
                let slot = command.get("slot").and_then(Value::as_u64).ok_or_else(|| format!("{cmd} needs slot (0 or 1)"))?;
                if slot > 1 { return Err("memory cards are 0 and 1".into()); }
                let slot = slot as usize;
                let name = || command.get("name").and_then(Value::as_str).ok_or_else(|| format!("{cmd} needs the save's name"));
                let file = |path: &Path| -> Result<(Vec<u8>, String), String> {
                    let bytes = xem_storage::read(path,MAX_FILE).map_err(|e| format!("{}: {e}", path.display()))?;
                    Ok((bytes, path.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default()))
                };
                let services = session.runtime.services();
                let now = services.clock.now;
                let card = &mut services.card;
                let done = match cmd {
                    "card_import" => {
                        let (bytes, file_name) = file(path()?)?;
                        let imported = card.import_card(slot,&bytes,&file_name,now)?;
                        json!({"imported": imported, "message": imported.describe(slot)})
                    }
                    "save_import" => {
                        let (bytes, file_name) = file(path()?)?;
                        let name = command.get("name").and_then(Value::as_str);
                        let imported = card.import_save(slot,&bytes,&file_name,name,now)?;
                        json!({"imported": imported, "message": imported.describe(slot)})
                    }
                    "card_export" => {
                        let format = match command.get("format").map(|f| serde_json::from_value::<CardFormat>(f.clone())) {
                            None => CardFormat::Raw,
                            Some(format) => format.map_err(|_| "card_export format is \"raw\" or \"gme\"")?,
                        };
                        let bytes = card.export_card(slot,format)?;
                        xem_storage::replace(path()?,&bytes).map_err(|e| e.to_string())?;
                        json!({"exported": bytes.len()})
                    }
                    "save_export" => {
                        let bytes = card.export_save(slot,name()?)?;
                        xem_storage::replace(path()?,&bytes).map_err(|e| e.to_string())?;
                        json!({"exported": bytes.len()})
                    }
                    "save_delete" => json!({"deleted": name()?, "message": card.delete_save(slot,name()?,now)?}),
                    _ => json!({"restored": true, "message": card.restore_previous(slot,now)?}),
                };
                Ok(json!({"ok":true,"slot":slot,"done":done,"card":card.status(slot,now)}))
            }
        }
    })();
    result.unwrap_or_else(|error| json!({"error":error}))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn malformed_card_is_left_empty_without_losing_either_file_or_the_other_card() {
        let dir = std::env::temp_dir().join(format!("xem-card-storage-{}",std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("card1.mcd"),b"invalid card").unwrap();
        let card2 = formatted();
        std::fs::write(dir.join("card2.mcd"),&card2).unwrap();
        let mut card = xem_core::devices::card::Card::default();
        card.attach_storage(Box::new(FileStorage {dir:dir.clone()})).unwrap();
        assert!(card.image(0).is_none());
        assert_eq!(card.image(1).unwrap(),card2);
        assert!(card.errors[0].contains("128 KiB"));
        card.retry_writes().unwrap();
        assert_eq!(std::fs::read(dir.join("card1.mcd")).unwrap(),b"invalid card");
        assert_eq!(std::fs::read(dir.join("card2.mcd")).unwrap(),card2);
        std::fs::remove_dir_all(dir).unwrap();
    }

    #[test]
    fn a_change_keeps_the_card_it_replaces_beside_it_for_the_next_session() {
        let dir = std::env::temp_dir().join(format!("xem-card-previous-{}",std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let mut card = xem_core::devices::card::Card::default();
        card.attach_storage(Box::new(FileStorage {dir:dir.clone()})).unwrap();
        assert_eq!(card.previous(0),None,"no previous card yet");
        let mut other = formatted();
        other[0x2000] = 0x42;
        card.change(0,other.clone(),0).unwrap();
        assert_eq!(std::fs::read(dir.join("card1.mcd")).unwrap(),other);
        assert_eq!(std::fs::read(dir.join("card1.previous.mcd")).unwrap(),formatted(),"the fresh card it replaced");
        let mut later = xem_core::devices::card::Card::default();
        later.attach_storage(Box::new(FileStorage {dir:dir.clone()})).unwrap();
        assert_eq!(later.previous(0),Some(&formatted()[..]));
        later.restore_previous(0,0).unwrap();
        assert_eq!(std::fs::read(dir.join("card1.mcd")).unwrap(),formatted());
        assert_eq!(std::fs::read(dir.join("card1.previous.mcd")).unwrap(),other);
        assert!(!dir.join("card2.previous.mcd").exists(),"card 2 was not changed");
        std::fs::remove_dir_all(dir).unwrap();
    }
}
