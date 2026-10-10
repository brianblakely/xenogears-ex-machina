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
    screenshot: Option<PathBuf>,
}

fn main() -> ExitCode {
    let mut options = Options {
        disc: PathBuf::new(),
        frames: 600,
        stubs: PathBuf::from("build/game/stubs.txt"),
        schema: PathBuf::from("build/game/schema.json"),
        control: false,
        screenshot: None,
    };
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--disc" => options.disc = args.next().map(PathBuf::from).unwrap_or_default(),
            "--frames" => options.frames = args.next().and_then(|v| v.parse().ok()).unwrap_or(options.frames),
            "--stubs" => options.stubs = args.next().map(PathBuf::from).unwrap_or(options.stubs),
            "--schema" => options.schema = args.next().map(PathBuf::from).unwrap_or(options.schema),
            "--control" => options.control = true,
            "--screenshot" => options.screenshot = args.next().map(PathBuf::from),
            _ => {
                eprintln!(
                    "usage: xem-headless --disc <chd|cue|bin> [--frames N | --control] [--stubs FILE] [--schema FILE] \
                     [--screenshot out.png]"
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
    use xem_core::{Runtime, exe};

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
    for frame in 0..options.frames {
        if let Err(trap) = session.frame() {
            println!("frame {frame}: {trap}");
            break;
        }
    }
    let services = session.runtime.services();
    for line in &services.log {
        println!("log: {line}");
    }
    println!("clock: {} cycles, {} vblanks", services.clock.now, services.clock.vblanks);
    if let Some(path) = &options.screenshot {
        let (width, height, rgba) = services.gpu.display_rgba8();
        write_png(path, width, height, &rgba)?;
        println!("screenshot: {width}x{height} {}", path.display());
    }
    Ok(())
}

/// The displayed picture as an RGBA PNG (an empty display area gives 1x1).
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
