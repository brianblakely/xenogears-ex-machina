//! Start the game natively from the user's disc, as the BIOS does: identify
//! the disc, copy the executable's text to its address, insert the disc into
//! the drive, attach the memory cards and the Kanji font. Every native host
//! (headless, desktop, Android) starts the game here.

use std::error::Error;
use std::path::{Path, PathBuf};

use xem_core::control::Session;
use xem_core::devices::card::FileStorage;
use xem_core::inspect::Schema;
use xem_core::{Runtime, bios, exe};
use xem_disc::Identification;

use crate::NativeModule;

#[derive(Debug, Clone, Default)]
pub struct BootOptions {
    /// Where card1.mcd and card2.mcd live; no card is inserted without it.
    pub cards: Option<PathBuf>,
    /// A BIOS image for the Kanji font (blank glyphs without one).
    pub bios: Option<PathBuf>,
    /// build/game/stubs.txt, to name functions the port does not define.
    pub stubs: Option<PathBuf>,
    /// build/game/schema.json, for introspection.
    pub schema: Option<PathBuf>,
    /// Keep the SPU's samples for the host to play (or record).
    pub audio_output: bool,
}

pub fn boot(disc: &Path, options: &BootOptions) -> Result<(Session<NativeModule>, Identification), Box<dyn Error>> {
    let mut source = xem_disc::open_path(disc)?;
    let identity = xem_disc::identify(source.as_mut())?;
    identity.disc.ok_or("not a known Xenogears disc")?;
    let header = exe::parse(&identity.executable)?;

    let mut runtime = Runtime::new(NativeModule::new());
    if let Some(text) = options.stubs.as_ref().and_then(|p| std::fs::read_to_string(p).ok()) {
        runtime.services().stub_names =
            text.lines().filter_map(|l| l.split_once(' ').map(|(_, n)| n.to_string())).collect();
    }
    if let Some(dir) = &options.cards {
        runtime.services().card.attach_storage(Box::new(FileStorage { dir: dir.clone() }))?;
    }
    let rom = options.bios.as_ref().map(std::fs::read).transpose()?;
    if bios::install_kanji_font(runtime.memory(), rom.as_deref())? == bios::KanjiFont::Blank {
        eprintln!("kanji font: blank glyphs (no BIOS image)");
    }
    runtime.services().spu.set_output(options.audio_output);
    // The BIOS copies the executable's text to its address and jumps to pc0.
    runtime.memory().write(header.text_address, exe::text(&identity.executable, &header))?;
    runtime.services().insert_disc(source);
    let schema = options
        .schema
        .as_ref()
        .and_then(|p| std::fs::read_to_string(p).ok())
        .map(|t| Schema::parse(&t))
        .transpose()?;
    Ok((Session::new(runtime, schema), identity))
}
