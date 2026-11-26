//! Start the game natively from the user's disc, as the BIOS does: identify
//! the disc, copy the executable's text to its address, insert the disc into
//! the drive, attach the memory cards and the Kanji font (a BIOS image's, or
//! the substitute glyphs, `xem_core::bios`). Every native host (headless,
//! desktop, Android) starts the game here; [`boot`] verifies the disc as
//! Redump's dump (`xem_disc::verify`) and gives the drive's changer the
//! game's other disc when it is beside the first.

use sha2::{Digest, Sha256};
use std::error::Error;
use std::path::{Path, PathBuf};

use xem_core::control::Session;
use crate::storage::FileStorage;
use xem_core::inspect::Schema;
use xem_core::{Runtime, bios, exe};
use xem_disc::{Identification, SectorSource};

use crate::NativeModule;

#[derive(Debug, Clone, Default)]
pub struct BootOptions {
    /// Where card1.mcd and card2.mcd live; no card is inserted without it.
    pub cards: Option<PathBuf>,
    /// A BIOS image named for this run (`--bios FILE`) for the Kanji font:
    /// one that cannot be read or that `xem_core::bios::check` refuses fails
    /// the boot, naming it.
    pub bios: Option<PathBuf>,
    /// Without `bios`, the BIOS image the player imported
    /// (`storage::stored_bios`): one that cannot be read or that the check
    /// refuses stays as it is, the substitute glyphs are placed as without
    /// one, and `Services::kanji_font_notice` says why (the storage line), as
    /// a card file that does not load leaves its slot empty and says why.
    pub stored_bios: Option<PathBuf>,
    /// build/game/stubs.txt, to name functions the port does not define.
    pub stubs: Option<PathBuf>,
    /// Keep the SPU's samples for the host to play (or record).
    pub audio_output: bool,
}

pub fn boot(disc: &Path, options: &BootOptions) -> Result<(Session<NativeModule>, Identification), Box<dyn Error>> {
    let mut source = xem_disc::open_path(disc)?;
    let known = xem_disc::identify(source.as_mut())?.disc.ok_or("not a known Xenogears disc")?;
    xem_disc::verify(source.as_mut(), known)?;
    let (mut session, identity) = boot_source(source, options)?;
    let mut us: [Option<std::path::PathBuf>; 2] = [None, None];
    us[usize::from(known.number() - 1)] = Some(disc.to_path_buf());
    for (other, path, source) in xem_disc::discs_beside(disc, known) {
        match source {
            Ok(source) => {
                eprintln!("disc {} beside it: {}", other.number(), path.display());
                session.runtime.services().add_disc(other.number(), source);
                us[usize::from(other.number() - 1)] = Some(path);
            }
            Err(error) => eprintln!("disc {} beside it not used: {}: {error}", other.number(), path.display()),
        }
    }
    install_japanese(&mut session.runtime, disc, &us);
    Ok((session, identity))
}

fn hex_digest(bytes: &[u8]) -> String {
    use std::fmt::Write;
    let mut result = String::with_capacity(bytes.len() * 2);
    for b in bytes { write!(result, "{b:02x}").expect("write to string"); }
    result
}

/// Cache the expensive raw-track hash by the backing file's identity. A cue
/// also contributes its track boundaries, even when several cues share a bin.
fn track_stamp(path: &Path) -> Option<String> {
    let mut stamp = Sha256::new();
    let track = if path.extension().is_some_and(|ext| ext.eq_ignore_ascii_case("cue")) {
        let cue = std::fs::read_to_string(path).ok()?;
        stamp.update(cue.as_bytes());
        path.parent()?.join(xem_disc::parse_cue(&cue).ok()?.file)
    } else { path.to_owned() };
    let track = track.canonicalize().ok()?;
    let meta = std::fs::metadata(&track).ok()?;
    stamp.update(track.as_os_str().as_encoded_bytes());
    stamp.update(meta.len().to_le_bytes());
    stamp.update(meta.modified().ok()?.duration_since(std::time::UNIX_EPOCH).ok()?.as_nanos().to_le_bytes());
    #[cfg(unix)]
    {
        use std::os::unix::fs::MetadataExt;
        for word in [meta.dev(), meta.ino(), meta.ctime() as u64, meta.ctime_nsec() as u64] {
            stamp.update(word.to_le_bytes());
        }
    }
    Some(hex_digest(&stamp.finalize()))
}

/// US tracks have already passed Redump verification. CHDs carry a raw-data
/// digest. For other Japanese images hash the data track, including the one
/// named by a cue, so replacing a disc invalidates its extracted package.
fn japanese_cache(us: &[Option<PathBuf>; 2], japanese: &mut [(u8, PathBuf, Box<dyn SectorSource>)], hash_tracks: bool) -> Option<PathBuf> {
    let dir = xem_storage::application_directory().ok()?.join("cache/japanese");
    let mut key = Sha256::new();
    key.update(b"XEM Japanese package 1");
    for (path, disc) in us.iter().zip([xem_disc::KnownDisc::Disc1, xem_disc::KnownDisc::Disc2]) {
        key.update(if path.is_some() { disc.track_sha1() } else { [0; 20] });
    }
    for number in 1..=2 {
        let Some((_, path, source)) = japanese.iter_mut().find(|(n, _, _)| *n == number) else {
            key.update([0]);
            continue;
        };
        if let Some(hash) = source.chd_raw_sha1() {
            key.update([1]);
            key.update(hash);
        } else {
            key.update([2]);
            let stamp = track_stamp(path);
            let index = stamp.as_ref().map(|stamp| dir.join("tracks").join(stamp));
            if let Some(hash) = index.as_ref().and_then(|p| xem_storage::read(p, 32).ok()).filter(|b| b.len() == 32) {
                key.update(hash);
                continue;
            }
            if !hash_tracks { return None; }
            let mut digest = Sha256::new();
            let mut buffer = vec![0; 128 * xem_disc::SECTOR_SIZE];
            for lba in (0..source.sector_count()).step_by(128) {
                let n = (source.sector_count() - lba).min(128) as usize * xem_disc::SECTOR_SIZE;
                source.read_sectors(lba, &mut buffer[..n]).ok()?;
                digest.update(&buffer[..n]);
            }
            let hash = digest.finalize();
            if let Some(index) = index && track_stamp(path) == stamp { let _ = xem_storage::replace(&index, &hash); }
            key.update(hash);
        }
    }
    Some(dir.join(hex_digest(&key.finalize())))
}

const PACKAGE_LIMIT: usize = 32 * 1024 * 1024;

fn cached_japanese(path: &Path) -> Option<xem_core::japanese::Japanese> {
    let bytes = xem_storage::read(path, PACKAGE_LIMIT + 32).ok()?;
    let (checksum, package) = bytes.split_at_checked(32)?;
    if Sha256::digest(package).as_slice() != checksum { return None; }
    let (found, used) = bincode::serde::decode_from_slice(package, bincode::config::standard().with_limit::<PACKAGE_LIMIT>()).ok()?;
    (used == package.len()).then_some(found)
}

/// The Japanese release's text, movies and voices from its discs beside
/// the North American ones (`us`, by number), when there are any
/// (`xem_core::japanese`): a disc that does not read leaves the game as it
/// is, saying why.
fn install_japanese(runtime: &mut Runtime<NativeModule>, disc: &Path, us: &[Option<PathBuf>; 2]) {
    let mut japanese = xem_disc::japanese_discs_beside(disc);
    if japanese.is_empty() {
        return;
    }
    for (number, path, _) in &japanese {
        eprintln!("Japanese disc {number} beside it: {}", path.display());
    }
    let cache = japanese_cache(us, &mut japanese, false);
    let paths: Vec<_> = japanese.iter().map(|(n, path, _)| (*n, path.clone())).collect();
    let us = us.clone();
    let (send, receive) = std::sync::mpsc::channel();
    let worker = std::thread::Builder::new().name("Japanese disc reader".into()).spawn(move || {
        if let Some(package) = cache.as_deref().and_then(cached_japanese) {
            let _ = send.send(Ok(package));
            return;
        }
        // Each reader owns its file handle and CHD cache. Playback can seek
        // either disc while this worker extracts the immutable assets.
        let mut sources = Vec::with_capacity(paths.len());
        let read = (|| {
            for (n, path) in paths {
                let source = xem_disc::open_path(&path).map_err(|e| e.to_string())?;
                sources.push((n, path, source));
            }
            let open_us = |path: &Option<PathBuf>| path.as_deref().map(xem_disc::open_path).transpose().map_err(|e| e.to_string());
            let (mut us1, mut us2) = (open_us(&us[0])?, open_us(&us[1])?);
            let mut jp: [Option<&mut dyn SectorSource>; 2] = [None, None];
            for (n, _, source) in &mut sources { jp[usize::from(*n - 1)] = Some(source.as_mut()); }
            xem_core::japanese::read([us1.as_deref_mut().map(|s| s as _), us2.as_deref_mut().map(|s| s as _)], jp)
        })();
        let package = read.as_ref().ok().and_then(|found| bincode::serde::encode_to_vec(found, bincode::config::standard()).ok());
        let _ = send.send(read);
        // A first-use language selection can proceed before raw track
        // fingerprinting finishes. The versioned package is published atomically.
        if let Some(package) = package.filter(|p| p.len() <= PACKAGE_LIMIT)
            && let Some(path) = cache.or_else(|| japanese_cache(&us, &mut sources, true))
        {
            let mut bytes = Vec::with_capacity(32 + package.len());
            bytes.extend_from_slice(&Sha256::digest(&package));
            bytes.extend_from_slice(&package);
            if let Err(error) = xem_storage::replace(&path, &bytes) {
                eprintln!("Japanese package cache: {error}");
            }
        }
    });
    match worker {
        Ok(_) => {
            let discs = japanese.into_iter().map(|(n, _, source)| (n, source)).collect();
            runtime.prepare_japanese(discs, move || receive.recv().map_err(|e| e.to_string())?);
        }
        Err(error) => eprintln!("Japanese discs not used: {error}"),
    }
}

/// [`boot`] from an opened disc image its host has verified
/// (`xem_disc::verify`; `xem_disc::open_reader` over a file descriptor, as
/// Android's document picker gives one).
pub fn boot_source(
    mut source: Box<dyn SectorSource>,
    options: &BootOptions,
) -> Result<(Session<NativeModule>, Identification), Box<dyn Error>> {
    let identity = xem_disc::identify(source.as_mut())?;
    identity.disc.ok_or("not a known Xenogears disc")?;
    let header = exe::parse(&identity.executable)?;

    let mut runtime = Runtime::new(NativeModule::new());
    runtime.identity.disc = xem_disc::state_identity(&identity);
    if let Some(text) = options.stubs.as_ref().and_then(|p| std::fs::read_to_string(p).ok()) {
        runtime.services().stub_names =
            text.lines().filter_map(|l| l.split_once(' ').map(|(_, n)| n.to_string())).collect();
    }
    if let Some(dir) = &options.cards {
        runtime.services().card.attach_storage(Box::new(FileStorage { dir: dir.clone() }))?;
        for error in &runtime.services().card.errors { eprintln!("{error}"); }
    }
    let install = |runtime: &mut Runtime<NativeModule>, path: &Path| {
        let rom = xem_storage::read(path, bios::MAX_FILE).map_err(|e| e.to_string())?;
        runtime.install_kanji_font(Some(&rom))
    };
    match (&options.bios, &options.stored_bios) {
        (Some(path), _) => {
            install(&mut runtime, path).map_err(|e| format!("BIOS image {}: {e}", path.display()))?;
        }
        (None, Some(path)) => {
            if let Err(error) = install(&mut runtime, path) {
                let notice = format!("Imported BIOS image {} not used: {error}; save titles use the substitute glyphs.", path.display());
                eprintln!("{notice}");
                runtime.install_kanji_font(None)?;
                runtime.services().kanji_font_notice = Some(notice);
            }
        }
        (None, None) => {
            runtime.install_kanji_font(None)?;
            eprintln!("kanji font: substitute glyphs for save titles (no BIOS image)");
        }
    }
    runtime.services().spu.set_output(options.audio_output);
    // The BIOS copies the executable's text to its address and jumps to pc0.
    runtime.load_executable(header.text_address, exe::text(&identity.executable, &header))?;
    runtime.services().insert_disc(identity.disc.map_or(0, |disc| disc.number()), source);
    // The module's schema (build/game/schema.json, embedded by build.rs).
    let schema = Schema::parse(include_str!(concat!(env!("OUT_DIR"), "/schema.json")))?;
    let started = std::time::Instant::now();
    Ok((Session::new(runtime,Some(schema),move || started.elapsed()),identity))
}
