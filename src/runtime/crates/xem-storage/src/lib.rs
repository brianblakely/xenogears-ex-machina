use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};

static SERIAL: AtomicU64 = AtomicU64::new(0);

/// Portable application data belongs beside the downloaded executable, never
/// inside an AppImage mount or a macOS application bundle. XEM_DATA_DIR is an
/// explicit override for development, tests and read-only installations.
pub fn application_directory() -> io::Result<PathBuf> {
    if let Some(path) = std::env::var_os("XEM_DATA_DIR").filter(|p| !p.is_empty()) {
        return std::path::absolute(PathBuf::from(path));
    }
    let executable = if cfg!(target_os = "linux") {
        std::env::var_os("APPIMAGE").filter(|p| !p.is_empty()).map(PathBuf::from)
    } else { None };
    directory_for_executable(&executable.unwrap_or(std::env::current_exe()?))
}

fn directory_for_executable(executable: &Path) -> io::Result<PathBuf> {
    let executable = std::path::absolute(executable)?;
    let bundle = executable.ancestors().find(|p| p.extension().is_some_and(|e| e == "app"));
    bundle.unwrap_or(&executable).parent().map(PathBuf::from)
        .ok_or_else(|| io::Error::other("the executable has no parent directory"))
}

/// Write, sync, replace, and sync the containing directory before acknowledging.
/// The previous file survives a failed write; temporary files are never loaded.
pub fn replace(path: &Path, data: &[u8]) -> io::Result<()> {
    let dir = path.parent().filter(|p| !p.as_os_str().is_empty()).unwrap_or(Path::new("."));
    std::fs::create_dir_all(dir)?;
    let name = path.file_name().ok_or_else(|| io::Error::other("missing file name"))?.to_string_lossy();
    let partial = dir.join(format!(".{name}.{}.{}.partial", std::process::id(), SERIAL.fetch_add(1, Ordering::Relaxed)));
    let result = (|| {
        let mut file = std::fs::OpenOptions::new().write(true).create_new(true).open(&partial)?;
        file.write_all(data)?;
        file.sync_all()?;
        drop(file);
        std::fs::rename(&partial, path)?;
        #[cfg(unix)]
        std::fs::File::open(dir)?.sync_all()?;
        Ok(())
    })();
    if result.is_err() { let _ = std::fs::remove_file(&partial); }
    result
}

pub fn read(path: &Path, limit: usize) -> io::Result<Vec<u8>> {
    let mut out = Vec::new();
    std::fs::File::open(path)?.take(limit as u64 + 1).read_to_end(&mut out)?;
    if out.len() > limit { return Err(io::Error::other("file exceeds size limit")); }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn portable_paths_follow_the_executable_or_application_bundle() {
        let root = std::env::temp_dir().join("Xenogears installation");
        assert_eq!(directory_for_executable(&root.join("Xenogears.AppImage")).unwrap(), root);
        assert_eq!(directory_for_executable(&root.join("xem-desktop.exe")).unwrap(), root);
        assert_eq!(directory_for_executable(&root.join("Xenogears.app/Contents/MacOS/xem-desktop")).unwrap(), root);
    }
    #[test]
    fn durable_replacement_ignores_interrupted_files_and_bounds_reads() {
        let dir = std::env::temp_dir().join(format!("xem-storage-{}", std::process::id()));
        let path = dir.join("state.xem");
        replace(&path, b"first").unwrap();
        std::fs::write(dir.join("state.xem.partial"), b"unfinished").unwrap();
        assert_eq!(read(&path, 5).unwrap(), b"first");
        replace(&path, b"second").unwrap();
        assert_eq!(read(&path, 6).unwrap(), b"second");
        assert!(read(&path, 5).is_err());
        assert!(replace(&dir, b"cannot replace a directory").is_err());
        assert_eq!(read(&path, 6).unwrap(), b"second");
        std::fs::remove_dir_all(dir).unwrap();
    }
}
