//! First-run organization of the user's portable installation. Only identified
//! Xenogears images are moved; cue sheets retain all of their companion files.
use std::collections::BTreeSet;
use std::fs;
use std::io;
use std::path::{Component, Path, PathBuf};

fn candidates(directory: &Path) -> io::Result<Vec<PathBuf>> {
    let mut paths = Vec::new();
    for entry in fs::read_dir(directory)? {
        let entry = entry?;
        if entry.file_type()?.is_file() && entry.path().extension().is_some_and(|ext| {
            ["cue", "chd", "iso", "bin"].iter().any(|kind| ext.eq_ignore_ascii_case(kind))
        }) {
            paths.push(entry.path());
        }
    }
    // A cue sheet claims its tracks before bare .bin candidates do.
    paths.sort_by_key(|p| (!p.extension().is_some_and(|e| e.eq_ignore_ascii_case("cue")), p.clone()));
    Ok(paths)
}

fn known(path: &Path) -> Option<crate::KnownDisc> {
    let mut source = crate::open_path(path).ok()?;
    crate::identify(source.as_mut()).ok()?.disc
}

fn companions(path: &Path) -> io::Result<Vec<PathBuf>> {
    let mut files = vec![path.to_path_buf()];
    if path.extension().is_some_and(|e| e.eq_ignore_ascii_case("cue")) {
        for name in crate::cue::files(&fs::read_to_string(path)?) {
            // Relocating an absolute path or ../ reference changes its meaning.
            // Keep those user-managed layouts in place; --disc can open them.
            let relative = Path::new(&name);
            if relative.components().count() != 1 || !matches!(relative.components().next(), Some(Component::Normal(_)))
                || name.contains('\\') || name.contains(':')
            {
                return Err(io::Error::other(format!("{} references {name:?}: place its tracks beside the cue sheet before installing", path.display())));
            }
            files.push(path.with_file_name(name));
        }
    }
    files.sort();
    files.dedup();
    Ok(files)
}

/// Move a set without overwriting anything. All sources survive until every
/// destination is complete and synced. Hard links avoid copying large images;
/// create_new + copy also works on filesystems without hard links (e.g. exFAT).
fn move_files(files: &BTreeSet<PathBuf>, destination: &Path) -> io::Result<()> {
    for source in files {
        if !fs::symlink_metadata(source)?.file_type().is_file() {
            return Err(io::Error::other(format!("{} is not a regular file", source.display())));
        }
        let target = destination.join(source.file_name().unwrap());
        if target.try_exists()? || target.is_symlink() {
            return Err(io::Error::new(io::ErrorKind::AlreadyExists, format!("{} already exists; neither image was replaced", target.display())));
        }
    }
    let mut created = Vec::new();
    let result = (|| {
        for source in files {
            let target = destination.join(source.file_name().unwrap());
            if fs::hard_link(source, &target).is_err() {
                let mut output = fs::OpenOptions::new().write(true).create_new(true).open(&target)?;
                created.push(target);
                io::copy(&mut fs::File::open(source)?, &mut output)?;
                output.sync_all()?;
            } else {
                created.push(target);
                // A hard link writes no new file data. Sync its directory below;
                // flushing a read-only source handle is not valid on Windows.
            }
        }
        #[cfg(unix)]
        fs::File::open(destination)?.sync_all()?;
        Ok(())
    })();
    if let Err(error) = result {
        for target in created { let _ = fs::remove_file(target); }
        return Err(error);
    }
    for source in files { fs::remove_file(source)?; }
    #[cfg(unix)]
    if let Some(parent) = files.first().and_then(|p| p.parent()) { fs::File::open(parent)?.sync_all()?; }
    Ok(())
}

/// Create the installation directories, organize root-level discs, and choose
/// disc 1 when present (otherwise disc 2). Boot still performs the full image
/// verification. Running again preserves existing discs, cards and states.
pub fn prepare(root: &Path) -> io::Result<Option<PathBuf>> {
    for name in ["discs", "cards", "saves"] { fs::create_dir_all(root.join(name))?; }
    let mut files = BTreeSet::new();
    for path in candidates(root)? {
        if !files.contains(&path) && known(&path).is_some() {
            files.extend(companions(&path)?);
        }
    }
    move_files(&files, &root.join("discs"))?;
    let mut discs: Vec<_> = candidates(&root.join("discs"))?.into_iter()
        .filter_map(|path| known(&path).map(|disc| (disc.number(), path))).collect();
    discs.sort_by_key(|(number, _)| *number);
    Ok(discs.into_iter().next().map(|(_, path)| path))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn user_images_are_identified_relocated_and_found_on_the_second_launch() {
        let repository = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../..");
        let bin = repository.join(".local/discs/disc1.bin");
        let chd = repository.join("discs/Xenogears disc 2.chd");
        if !bin.is_file() || !chd.is_file() {
            eprintln!("skipping real-image installation: private disc 1 BIN and disc 2 CHD unavailable");
            return;
        }
        let root = repository.join(format!("build/install-test-{}", std::process::id()));
        fs::create_dir_all(&root).unwrap();
        // Only local hard links move; the user's original paths stay intact.
        fs::hard_link(&bin, root.join("Disc One.bin")).unwrap();
        fs::hard_link(&chd, root.join("Disc Two.CHD")).unwrap();
        fs::write(root.join("Disc One.CUE"), "FILE \"Disc One.bin\" BINARY\n TRACK 01 MODE2/2352\n INDEX 01 00:00:00\n").unwrap();
        let selected = prepare(&root).unwrap().unwrap();
        assert_eq!(selected, root.join("discs/Disc One.CUE"));
        assert_eq!(known(&selected), Some(crate::KnownDisc::Disc1));
        assert_eq!(known(&root.join("discs/Disc Two.CHD")), Some(crate::KnownDisc::Disc2));
        assert!(!root.join("Disc One.bin").exists());
        assert!(!root.join("Disc Two.CHD").exists());
        assert_eq!(prepare(&root).unwrap(), Some(selected));
        assert!(bin.is_file() && chd.is_file());
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn installation_preserves_user_files_and_never_clobbers_a_collision() {
        let root = std::env::temp_dir().join(format!("xem-install-{}", std::process::id()));
        fs::create_dir_all(&root).unwrap();
        fs::write(root.join("bios.bin"), b"not a disc").unwrap();
        assert_eq!(prepare(&root).unwrap(), None);
        for name in ["discs", "cards", "saves"] { assert!(root.join(name).is_dir()); }
        fs::write(root.join("cards/card1.mcd"), b"my card").unwrap();
        fs::write(root.join("saves/state.xemstate"), b"my state").unwrap();
        fs::write(root.join("Disc.CUE"), "FILE \"Track 1.bin\" BINARY\n TRACK 01 MODE2/2352\n INDEX 01 00:00:00\nFILE \"Track 2.bin\" BINARY\n TRACK 02 AUDIO\n INDEX 01 00:00:00\n").unwrap();
        fs::write(root.join("Track 1.bin"), b"track one").unwrap();
        fs::write(root.join("Track 2.bin"), b"track two").unwrap();
        let files = companions(&root.join("Disc.CUE")).unwrap().into_iter().collect();
        fs::write(root.join("discs/Track 2.bin"), b"existing image").unwrap();
        assert_eq!(move_files(&files, &root.join("discs")).unwrap_err().kind(), io::ErrorKind::AlreadyExists);
        assert!(root.join("Disc.CUE").exists());
        assert!(!root.join("discs/Track 1.bin").exists());
        assert_eq!(fs::read(root.join("discs/Track 2.bin")).unwrap(), b"existing image");
        fs::remove_file(root.join("discs/Track 2.bin")).unwrap();
        move_files(&files, &root.join("discs")).unwrap();
        assert!(!root.join("Disc.CUE").exists());
        assert_eq!(fs::read(root.join("discs/Track 2.bin")).unwrap(), b"track two");
        assert_eq!(prepare(&root).unwrap(), None);
        assert_eq!(fs::read(root.join("bios.bin")).unwrap(), b"not a disc");
        assert_eq!(fs::read(root.join("cards/card1.mcd")).unwrap(), b"my card");
        assert_eq!(fs::read(root.join("saves/state.xemstate")).unwrap(), b"my state");
        for name in ["../other.bin", "/other.bin", "nested/other.bin", "C:\\other.bin"] {
            fs::write(root.join("unsafe.cue"), format!("FILE \"{name}\" BINARY")).unwrap();
            assert!(companions(&root.join("unsafe.cue")).is_err());
        }
        fs::remove_dir_all(root).unwrap();
    }
}
