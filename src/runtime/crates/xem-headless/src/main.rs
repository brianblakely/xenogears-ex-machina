//! Run the game headless from the user's disc: no window, display server,
//! GPU, input or audio device.
//!
//! `--frames N` runs N frames and reports; `--control` reads one JSON command
//! per line from stdin and answers each on stdout (xem_core::control).

use std::path::PathBuf;
use std::process::ExitCode;

#[cfg(has_game_module)]
mod cd_trace;

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
    screenshot: Option<PathBuf>,
    /// Report the drive's data sectors at the end (cd_trace.rs).
    cd_trace: bool,
    /// Pad 1 input by frame: the retained routes' format,
    /// [{"start": F, "end": F, "buttons": ["start", ...]}], held start <= frame < end.
    inputs: Option<PathBuf>,
    /// Every N frames write frame-NNNNNN.png (the display) and .ram (2 MB).
    checkpoint_every: u64,
    checkpoint_dir: Option<PathBuf>,
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
        screenshot: None,
        cd_trace: false,
        inputs: None,
        checkpoint_every: 0,
        checkpoint_dir: None,
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
            "--screenshot" => options.screenshot = args.next().map(PathBuf::from),
            "--inputs" => options.inputs = args.next().map(PathBuf::from),
            "--checkpoint-every" => options.checkpoint_every = args.next().and_then(|v| v.parse().ok()).unwrap_or(0),
            "--checkpoint-dir" => options.checkpoint_dir = args.next().map(PathBuf::from),
            "--cd-trace" => options.cd_trace = true,
            _ => {
                eprintln!(
                    "usage: xem-headless --disc <chd|cue|bin> [--frames N | --control] [--stubs FILE] [--schema FILE] \
                     [--screenshot out.png] [--cd-trace] [--cards DIR] [--bios FILE] [--wav FILE]"
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

    let boot = xem_game::BootOptions {
        cards: options.cards.clone(),
        bios: options.bios.clone(),
        stubs: Some(options.stubs.clone()),
        schema: Some(options.schema.clone()),
        audio_output: options.wav.is_some(),
    };
    let (mut session, identity) = xem_game::boot(&options.disc, &boot)?;
    let known = identity.disc.ok_or("not a known Xenogears disc")?;
    eprintln!("disc {} ({}), {}", known.number(), known.serial(), identity.boot_path);
    if options.cd_trace {
        session.runtime.services().cd.sector_log = Some(Vec::new());
    }

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
    let inputs = match &options.inputs {
        Some(path) => input_route(&std::fs::read_to_string(path)?)?,
        None => Vec::new(),
    };
    if let Some(dir) = &options.checkpoint_dir {
        std::fs::create_dir_all(dir)?;
    }
    for frame in 0..options.frames {
        if !inputs.is_empty() {
            let held = inputs.iter().filter(|(start, end, _)| (*start..*end).contains(&frame)).fold(0, |a, i| a | i.2);
            session.runtime.services().pads.0[0].buttons = held;
        }
        // A native disc source never blocks.
        let result = session.frame();
        let spu = &mut session.runtime.services().spu;
        let start = audio.len();
        audio.resize(start + spu.audio_queued(), 0);
        spu.take_audio(&mut audio[start..]);
        if let Err(trap) = result {
            println!("frame {frame}: {trap}");
            break;
        }
        if let (Some(dir), true) = (&options.checkpoint_dir, options.checkpoint_every > 0) {
            if (frame + 1) % options.checkpoint_every == 0 {
                let name = format!("frame-{:06}", frame + 1);
                let (width, height, rgba) = session.runtime.services().gpu.display_rgba8();
                write_png(&dir.join(format!("{name}.png")), width, height, &rgba)?;
                let mut ram = vec![0; 0x20_0000];
                session.runtime.memory().read(0x8000_0000, &mut ram)?;
                std::fs::write(dir.join(format!("{name}.ram")), ram)?;
            }
        }
    }
    if let Some(log) = session.runtime.services().cd.sector_log.take() {
        let index = xem_disc::game_index(&identity.executable)?;
        let mut disc = xem_disc::open_path(&options.disc)?;
        cd_trace::report(&log, &index, disc.as_mut(), session.runtime.memory());
        println!("cd: {} XA audio sectors played", session.runtime.services().cd.xa_sectors);
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
    if let Some(path) = &options.screenshot {
        let (width, height, rgba) = services.gpu.display_rgba8();
        write_png(path, width, height, &rgba)?;
        println!("screenshot: {width}x{height} {}", path.display());
    }
    Ok(())
}

/// The displayed picture as an RGBA PNG (an empty display area gives 1x1).
/// The retained routes' input format (tests/reference-inputs): pad 1 button holds.
#[cfg(has_game_module)]
fn input_route(text: &str) -> Result<Vec<(u64, u64, u16)>, Box<dyn std::error::Error>> {
    let route: serde_json::Value = serde_json::from_str(text)?;
    let mut out = Vec::new();
    for hold in route.as_array().ok_or("an input route is a list")? {
        let start = hold["start"].as_u64().ok_or("start")?;
        let end = hold["end"].as_u64().ok_or("end")?;
        let names = hold["buttons"].as_array().ok_or("buttons")?;
        let mut bits = 0;
        for name in names {
            let name = name.as_str().ok_or("button names are strings")?;
            bits |= xem_core::control::button_bit(name).ok_or_else(|| format!("unknown button {name}"))?;
        }
        out.push((start, end, bits));
    }
    Ok(out)
}

#[cfg(has_game_module)]
fn write_png(path: &std::path::Path, width: u32, height: u32, rgba: &[u8]) -> Result<(), Box<dyn std::error::Error>> {
    let (width, height, rgba) =
        if width == 0 || height == 0 { (1, 1, &[0, 0, 0, 0xFF][..]) } else { (width, height, rgba) };
    let mut encoder = png::Encoder::new(std::io::BufWriter::new(std::fs::File::create(path)?), width, height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    encoder.write_header()?.write_image_data(rgba)?;
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
