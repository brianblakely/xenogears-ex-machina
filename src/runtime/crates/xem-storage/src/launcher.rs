//! RecompLauncher's data folder on Linux. It runs the AppImage unpacked, with
//! APPIMAGE unset, so earlier versions kept their data beside the executable:
//! `{install}/nix/store/<hash>-xem-<version>/libexec`, a folder of the release
//! whose name changes with every version. Older launchers ran the AppImage
//! itself, which kept it beside the AppImage, in `{install}`. The first start
//! with RECOMPLAUNCHER_DATA copies the newest of those in; the originals stay.
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::sync::OnceLock;

/// What the game keeps in its data folder, except `cache` and `discs`: the
/// launcher passes the player's disc, and the images stay where they are.
const ITEMS: [&str; 4] = ["settings.json", "bios.bin", "cards", "saves"];
/// RecompLauncher writes the settings before every start; on the start that
/// copies the data in, the player's own settings replace them.
const SETTINGS: &str = "settings.json";
/// Written last, once every item is copied or absent: the only sign of done.
const MARKER: &str = ".copied-from-program-folder";
/// The items renamed into place by a copy that has not finished.
const JOURNAL: &str = ".copying";

static CHOSEN: OnceLock<PathBuf> = OnceLock::new();

/// `data`, or the old folder for this start while its data is not all in.
pub fn directory(data: &Path) -> PathBuf {
    CHOSEN.get_or_init(|| {
        let install = data.parent().unwrap_or(data);
        choose(data, old_folders(install).first().map(PathBuf::as_path))
    }).clone()
}

fn choose(data: &Path, old: Option<&Path>) -> PathBuf {
    if data.join(MARKER).exists() {
        return data.to_path_buf();
    }
    match copy_in(data, old) {
        Ok(()) => data.to_path_buf(),
        Err(error) => {
            let old = old.unwrap_or(data);
            eprintln!("xem: copying {} into {}: {error}; this start uses the old folder", old.display(), data.display());
            old.to_path_buf()
        }
    }
}

/// The folders an earlier version kept its data in, newest first: the
/// release's store folders by version, then the game's folder.
fn old_folders(install: &Path) -> Vec<PathBuf> {
    let mut stores: Vec<(Vec<u32>, PathBuf)> = fs::read_dir(install.join("nix/store")).into_iter().flatten().flatten()
        .filter_map(|entry| {
            let name = entry.file_name().into_string().ok()?;
            let version = name.split_once("-xem-v")?.1.split('.').map(str::parse).collect::<Result<_, _>>().ok()?;
            Some((version, entry.path().join("libexec")))
        })
        .collect();
    stores.sort_by(|a, b| b.0.cmp(&a.0));
    stores.into_iter().map(|(_, path)| path).chain([install.to_path_buf()]).filter(|p| holds_data(p)).collect()
}

fn holds_data(folder: &Path) -> bool {
    ITEMS.iter().any(|name| present(&folder.join(name)).unwrap_or(false))
}

/// An item that exists; an empty folder (one the launcher made to open it) does not count.
fn present(path: &Path) -> io::Result<bool> {
    match fs::symlink_metadata(path) {
        Err(error) if error.kind() == io::ErrorKind::NotFound => Ok(false),
        Err(error) => Err(error),
        Ok(meta) if meta.is_dir() => Ok(fs::read_dir(path)?.next().is_some()),
        Ok(_) => Ok(true),
    }
}

fn copy_in(data: &Path, old: Option<&Path>) -> io::Result<()> {
    fs::create_dir_all(data)?;
    take_back(data)?;
    let result = old.map_or(Ok(()), |old| copy_items(data, old))
        .and_then(|()| crate::replace(&data.join(MARKER), b""));
    match result {
        Ok(()) => remove(&data.join(JOURNAL)),
        Err(error) => {
            let _ = remove(&data.join(MARKER));
            let _ = take_back(data);
            Err(error)
        }
    }
}

fn copy_items(data: &Path, old: &Path) -> io::Result<()> {
    let mut renamed = String::new();
    for name in ITEMS {
        let from = old.join(name);
        let to = data.join(name);
        if fs::symlink_metadata(&from).is_err_and(|e| e.kind() == io::ErrorKind::NotFound)
            || (name != SETTINGS && present(&to)?) {
            continue;
        }
        let part = data.join(format!("{name}.part"));
        remove(&part)?;
        copy(&from, &part)?;
        renamed.push_str(name);
        renamed.push('\n');
        crate::replace(&data.join(JOURNAL), renamed.as_bytes())?;
        fs::rename(&part, &to)?;
    }
    fs::File::open(data)?.sync_all()
}

fn copy(from: &Path, to: &Path) -> io::Result<()> {
    if fs::metadata(from)?.is_dir() {
        fs::create_dir(to)?;
        for entry in fs::read_dir(from)? {
            let entry = entry?;
            copy(&entry.path(), &to.join(entry.file_name()))?;
        }
        fs::File::open(to)?.sync_all()
    } else {
        fs::copy(from, to)?;
        fs::File::open(to)?.sync_all()
    }
}

/// Removes what an unfinished copy put in `data`: only the items it journaled
/// and its `.part` leftovers.
fn take_back(data: &Path) -> io::Result<()> {
    let journal = data.join(JOURNAL);
    let names = match fs::read_to_string(&journal) {
        Err(error) if error.kind() == io::ErrorKind::NotFound => String::new(),
        result => result?,
    };
    for name in names.lines().filter(|name| ITEMS.contains(name)) {
        remove(&data.join(name))?;
    }
    for name in ITEMS {
        remove(&data.join(format!("{name}.part")))?;
    }
    remove(&journal)
}

fn remove(path: &Path) -> io::Result<()> {
    match fs::symlink_metadata(path) {
        Err(error) if error.kind() == io::ErrorKind::NotFound => Ok(()),
        Err(error) => Err(error),
        Ok(meta) if meta.is_dir() => fs::remove_dir_all(path),
        Ok(_) => fs::remove_file(path),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn folder(name: &str) -> PathBuf {
        let root = std::env::temp_dir().join(format!("xem-launcher-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        root
    }

    fn write(path: &Path, text: &str) {
        fs::create_dir_all(path.parent().unwrap()).unwrap();
        fs::write(path, text).unwrap();
    }

    fn text(path: &Path) -> String {
        fs::read_to_string(path).unwrap()
    }

    fn unfinished(data: &Path) -> bool {
        !data.join(MARKER).exists() && !data.join(JOURNAL).exists()
            && ITEMS.iter().all(|name| !data.join(format!("{name}.part")).exists())
    }

    #[test]
    fn a_fresh_start_uses_the_data_folder() {
        let root = folder("fresh");
        let data = root.join("recompdata");
        assert_eq!(choose(&data, old_folders(&root).first().map(PathBuf::as_path)), data);
        assert!(data.join(MARKER).exists());
        assert!(!data.join(JOURNAL).exists());
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn old_data_is_copied_in_once_and_the_old_settings_win_on_that_start() {
        let root = folder("adopt");
        let old = root.join("nix/store/aaaa-xem-v1.22/libexec");
        let data = root.join("recompdata");
        write(&old.join("cards/card1.mcd"), "card");
        write(&old.join("saves/state.xemstate"), "state");
        write(&old.join("settings.json"), "player");
        write(&old.join("bios.bin"), "bios");
        write(&old.join("discs/Disc 1.chd"), "disc");
        write(&data.join("settings.json"), "launcher");
        fs::create_dir_all(data.join("cards")).unwrap();
        assert_eq!(choose(&data, old_folders(&root).first().map(PathBuf::as_path)), data);
        assert!(!data.join("discs").exists() && old.join("discs/Disc 1.chd").exists());
        assert_eq!(text(&data.join("cards/card1.mcd")), "card");
        assert_eq!(text(&data.join("saves/state.xemstate")), "state");
        assert_eq!(text(&data.join("settings.json")), "player");
        assert_eq!(text(&data.join("bios.bin")), "bios");
        assert!(data.join(MARKER).exists() && !data.join(JOURNAL).exists() && !data.join("cards.part").exists());
        assert_eq!(text(&old.join("cards/card1.mcd")), "card");
        // The next start keeps what the launcher and the game wrote since.
        write(&data.join("settings.json"), "launcher again");
        write(&old.join("cards/card1.mcd"), "older card");
        assert_eq!(choose(&data, old_folders(&root).first().map(PathBuf::as_path)), data);
        assert_eq!(text(&data.join("settings.json")), "launcher again");
        assert_eq!(text(&data.join("cards/card1.mcd")), "card");
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn a_failed_copy_keeps_the_old_folder_and_the_next_start_brings_newer_saves() {
        let root = folder("failed");
        let old = root.join("nix/store/aaaa-xem-v1.22/libexec");
        let data = root.join("recompdata");
        write(&old.join("cards/card1.mcd"), "first");
        write(&old.join("settings.json"), "player");
        write(&data.join("settings.json"), "launcher");
        fs::create_dir_all(old.join("saves")).unwrap();
        std::os::unix::fs::symlink(root.join("missing"), old.join("saves/state.xemstate")).unwrap();
        assert_eq!(choose(&data, Some(&old)), old);
        assert!(unfinished(&data));
        assert!(!data.join("cards").exists() && !data.join("settings.json").exists());
        fs::remove_file(old.join("saves/state.xemstate")).unwrap();
        write(&old.join("cards/card1.mcd"), "newer");
        assert_eq!(choose(&data, Some(&old)), data);
        assert_eq!(text(&data.join("cards/card1.mcd")), "newer");
        assert_eq!(text(&data.join("settings.json")), "player");
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn an_interrupted_copy_is_taken_back_and_finished_on_the_next_start() {
        let root = folder("interrupted");
        let old = root.join("nix/store/aaaa-xem-v1.22/libexec");
        let data = root.join("recompdata");
        write(&old.join("cards/card1.mcd"), "newer");
        write(&old.join("saves/state.xemstate"), "state");
        // What a copy killed after renaming cards and while copying saves leaves.
        write(&data.join("cards/card1.mcd"), "older");
        write(&data.join("saves.part/state.xemstate"), "sta");
        write(&data.join(JOURNAL), "cards\n");
        write(&data.join("discs/kept.txt"), "not journaled");
        assert_eq!(choose(&data, Some(&old)), data);
        assert_eq!(text(&data.join("cards/card1.mcd")), "newer");
        assert_eq!(text(&data.join("saves/state.xemstate")), "state");
        assert_eq!(text(&data.join("discs/kept.txt")), "not journaled");
        assert!(data.join(MARKER).exists() && !data.join(JOURNAL).exists() && !data.join("saves.part").exists());
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn the_newest_version_holding_data_is_chosen_then_the_game_folder() {
        let root = folder("versions");
        let store = root.join("nix/store");
        write(&store.join("aaaa-xem-v1.9/libexec/cards/card1.mcd"), "1.9");
        write(&store.join("bbbb-xem-v1.22/libexec/cards/card1.mcd"), "1.22");
        fs::create_dir_all(store.join("cccc-xem-v1.23/libexec/cards")).unwrap();
        fs::create_dir_all(store.join("dddd-xem-release-input")).unwrap();
        write(&store.join("eeee-xem-v1.24/libexec/discs/Disc 1.chd"), "discs alone are not data");
        write(&root.join("settings.json"), "beside the AppImage");
        let folders = old_folders(&root);
        assert_eq!(folders, [store.join("bbbb-xem-v1.22/libexec"), store.join("aaaa-xem-v1.9/libexec"), root.clone()]);
        fs::remove_dir_all(store).unwrap();
        assert_eq!(old_folders(&root), std::slice::from_ref(&root));
        fs::remove_dir_all(root).unwrap();
    }
}
