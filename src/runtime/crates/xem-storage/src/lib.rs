use std::io::{self, Read, Write};
use std::path::Path;
use std::sync::atomic::{AtomicU64, Ordering};

static SERIAL: AtomicU64 = AtomicU64::new(0);

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
