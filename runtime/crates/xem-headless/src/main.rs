//! Run the game headless from the user's disc: no window, display server,
//! GPU, input or audio device.
//!
//! `--frames N` runs N frames and reports; `--control` reads one JSON command
//! per line from stdin and answers each on stdout (xem_core::control).

use std::path::PathBuf;
use std::process::ExitCode;

struct Options {
    disc: PathBuf,
    frames: u64,
    stubs: PathBuf,
    schema: PathBuf,
    control: bool,
    /// Memory card images (card1.mcd, card2.mcd) live here; no card is
    /// inserted without it.
    cards: Option<PathBuf>,
    /// A BIOS image for the Kanji font.
    bios: Option<PathBuf>,
    /// Write the SPU's output as a 16-bit stereo 44.1 kHz WAV file.
    wav: Option<PathBuf>,
}

fn main() -> ExitCode {
    let mut options = Options {
        disc: PathBuf::new(),
        frames: 600,
        stubs: PathBuf::from("build/game/stubs.txt"),
        schema: PathBuf::from("build/game/schema.json"),
        control: false,
        cards: None,
        bios: None,
        wav: None,
    };
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--disc" => options.disc = args.next().map(PathBuf::from).unwrap_or_default(),
            "--frames" => options.frames = args.next().and_then(|v| v.parse().ok()).unwrap_or(options.frames),
            "--stubs" => options.stubs = args.next().map(PathBuf::from).unwrap_or(options.stubs),
            "--schema" => options.schema = args.next().map(PathBuf::from).unwrap_or(options.schema),
            "--control" => options.control = true,
            "--cards" => options.cards = args.next().map(PathBuf::from),
            "--bios" => options.bios = args.next().map(PathBuf::from),
            "--wav" => options.wav = args.next().map(PathBuf::from),
            _ => {
                eprintln!(
                    "usage: xem-headless --disc <chd|cue|bin> [--frames N | --control] [--stubs FILE] [--schema FILE] \
                     [--cards DIR] [--bios FILE] [--wav FILE]"
                );
                return ExitCode::FAILURE;
            }
        }
    }
    if options.disc.as_os_str().is_empty() {
        eprintln!("--disc is required");
        return ExitCode::FAILURE;
    }
    match run(&options) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("error: {error}");
            ExitCode::FAILURE
        }
    }
}

#[cfg(not(has_game_module))]
fn run(_: &Options) -> Result<(), Box<dyn std::error::Error>> {
    Err("this build has no game module: run tools/game_module.py, then rebuild".into())
}

#[cfg(has_game_module)]
fn run(options: &Options) -> Result<(), Box<dyn std::error::Error>> {
    use std::io::BufRead;
    use xem_core::control::Session;
    use xem_core::inspect::Schema;
    use xem_core::devices::card::FileStorage;
    use xem_core::{Runtime, bios, exe};

    let mut source = xem_disc::open_path(&options.disc)?;
    let identity = xem_disc::identify(source.as_mut())?;
    let known = identity.disc.ok_or("not a known Xenogears disc")?;
    eprintln!("disc {} ({}), {}", known.number(), known.serial(), identity.boot_path);
    let header = exe::parse(&identity.executable)?;

    let mut runtime = Runtime::new(xem_game::NativeModule::new());
    if let Ok(text) = std::fs::read_to_string(&options.stubs) {
        runtime.services().stub_names =
            text.lines().filter_map(|l| l.split_once(' ').map(|(_, n)| n.to_string())).collect();
    }
    if let Some(dir) = &options.cards {
        runtime.services().card.attach_storage(Box::new(FileStorage { dir: dir.clone() }))?;
    }
    let rom = options.bios.as_ref().map(std::fs::read).transpose()?;
    if bios::install_kanji_font(runtime.memory(), rom.as_deref())? == bios::KanjiFont::Blank {
        eprintln!("kanji font: blank glyphs (no --bios image)");
    }
    runtime.services().spu.set_output(options.wav.is_some());
    // The BIOS copies the executable's text to its address and jumps to pc0.
    runtime.memory().write(header.text_address, exe::text(&identity.executable, &header))?;
    let schema = std::fs::read_to_string(&options.schema).ok().map(|t| Schema::parse(&t)).transpose()?;
    let mut session = Session::new(runtime, schema);

    if options.control {
        let stdin = std::io::stdin();
        for line in stdin.lock().lines() {
            let line = line?;
            if line.trim().is_empty() {
                continue;
            }
            let reply = match serde_json::from_str(&line) {
                Ok(command) => session.execute(&command),
                Err(error) => serde_json::json!({ "error": format!("bad json: {error}") }),
            };
            println!("{reply}");
        }
        return Ok(());
    }
    let mut audio = Vec::new();
    for frame in 0..options.frames {
        let result = session.frame();
        let spu = &mut session.runtime.services().spu;
        let start = audio.len();
        audio.resize(start + spu.audio_queued(), 0);
        spu.take_audio(&mut audio[start..]);
        if let Err(trap) = result {
            println!("frame {frame}: {trap}");
            break;
        }
    }
    let services = session.runtime.services();
    for line in &services.log {
        println!("log: {line}");
    }
    println!("clock: {} cycles, {} vblanks", services.clock.now, services.clock.vblanks);
    for error in &services.card.errors {
        println!("card: {error}");
    }
    if let Some(path) = &options.wav {
        std::fs::write(path, wav(&audio))?;
        let peak = audio.iter().map(|s| s.unsigned_abs()).max().unwrap_or(0);
        println!("audio: {} samples, peak {peak}, {}", audio.len() / 2, path.display());
    }
    Ok(())
}

/// A 16-bit stereo 44.1 kHz WAV file of interleaved samples.
#[cfg(has_game_module)]
fn wav(samples: &[i16]) -> Vec<u8> {
    let data = samples.len() as u32 * 2;
    let mut out = Vec::with_capacity(44 + data as usize);
    out.extend_from_slice(b"RIFF");
    out.extend_from_slice(&(36 + data).to_le_bytes());
    out.extend_from_slice(b"WAVEfmt ");
    out.extend_from_slice(&16u32.to_le_bytes());
    out.extend_from_slice(&1u16.to_le_bytes());
    out.extend_from_slice(&2u16.to_le_bytes());
    out.extend_from_slice(&44_100u32.to_le_bytes());
    out.extend_from_slice(&(44_100u32 * 4).to_le_bytes());
    out.extend_from_slice(&4u16.to_le_bytes());
    out.extend_from_slice(&16u16.to_le_bytes());
    out.extend_from_slice(b"data");
    out.extend_from_slice(&data.to_le_bytes());
    for sample in samples {
        out.extend_from_slice(&sample.to_le_bytes());
    }
    out
}
