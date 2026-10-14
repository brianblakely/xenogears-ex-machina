//! Native file operations at the host boundary; never executed from a session log.
use std::path::{Path, PathBuf};
use serde_json::{Value, json};
use xem_core::{control::Session, module::GameModule};
use xem_core::devices::card::{CardStorage, formatted, CARD_SIZE};
use xem_core::devices::card_file::MAX_FILE;

/// Images as files `card1.mcd` and `card2.mcd` in a directory; a missing
/// file is a fresh formatted card.
pub struct FileStorage {
    pub dir: PathBuf,
}

impl FileStorage {
    pub fn path(&self, slot: usize) -> PathBuf {
        self.dir.join(format!("card{}.mcd", slot + 1))
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
}

/// Explicit commands submitted by the native caller, outside the scheduled game log.
pub fn command<M: GameModule>(session: &mut Session<M>, command: &Value) -> Value {
    let cmd = command.get("cmd").and_then(Value::as_str).unwrap_or("");
    if !matches!(cmd, "state_save" | "state_load" | "card_export" | "card_import") { return session.execute(command); }
    let result = (|| -> Result<Value,String> {
        let path = Path::new(command.get("path").and_then(Value::as_str).ok_or("file operation needs path")?);
        match cmd {
            "state_save" => {
                let data = session.snapshot_bytes()?;
                xem_storage::replace(path, &data).map_err(|e| e.to_string())?;
                Ok(json!({"saved":true,"bytes":data.len()}))
            }
            "state_load" => {
                let data = xem_storage::read(path,xem_core::snapshot::MAX_STATE).map_err(|e| e.to_string())?;
                session.restore_bytes(&data)?;
                Ok(session.execute(&json!({"cmd":"status"})))
            }
            _ => {
                let slot = command.get("slot").and_then(Value::as_u64).unwrap_or(0);
                if slot > 1 { return Err("card slot is 0 or 1".into()); }
                let slot = slot as usize;
                let services = session.runtime.services();
                let now = services.clock.now;
                let card = &mut services.card;
                if cmd == "card_import" {
                    // Any card or save file, by its contents (a raw single
                    // save by its file name), as Import original card 1/2.
                    let bytes = xem_storage::read(path,MAX_FILE).map_err(|e| format!("{}: {e}",path.display()))?;
                    let file_name = path.file_name().map(|name| name.to_string_lossy().into_owned()).unwrap_or_default();
                    let message = card.import_file(slot,&bytes,&file_name,now)?;
                    Ok(json!({"ok":true,"slot":slot,"message":message,"card":card.status(slot,now)}))
                } else {
                    let image = card.image(slot).ok_or("no card in slot")?;
                    xem_storage::replace(path,image).map_err(|e| e.to_string())?;
                    Ok(json!({"ok":true,"slot":slot}))
                }
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
}
