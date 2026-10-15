//! Start the game natively from the user's disc, as the BIOS does: identify
//! the disc, copy the executable's text to its address, insert the disc into
//! the drive, attach the memory cards and the Kanji font (a BIOS image's, or
//! the substitute glyphs, `xem_core::bios`). Every native host (headless,
//! desktop, Android) starts the game here; [`boot`] verifies the disc as
//! Redump's dump (`xem_disc::verify`) and gives the drive's changer the
//! game's other disc when it is beside the first.

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
    /// A BIOS image for the Kanji font (`xem_core::bios::check` must accept
    /// it); the substitute glyphs without one.
    pub bios: Option<PathBuf>,
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
    for (other, path, source) in xem_disc::discs_beside(disc, known) {
        match source {
            Ok(source) => {
                eprintln!("disc {} beside it: {}", other.number(), path.display());
                session.runtime.services().add_disc(other.number(), source);
            }
            Err(error) => eprintln!("disc {} beside it not used: {}: {error}", other.number(), path.display()),
        }
    }
    Ok((session, identity))
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
    let rom = match &options.bios {
        Some(path) => Some(xem_storage::read(path, bios::MAX_FILE).map_err(|e| format!("BIOS image {}: {e}", path.display()))?),
        None => None,
    };
    let font = runtime.install_kanji_font(rom.as_deref()).map_err(|e| match &options.bios {
        Some(path) => format!("BIOS image {}: {e}", path.display()),
        None => e,
    })?;
    if font == bios::KanjiFont::Substitute {
        eprintln!("kanji font: substitute glyphs for save titles (no BIOS image)");
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
