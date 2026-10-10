//! Run the game headless from the user's disc: no window, display server,
//! GPU, input or audio device.

use std::path::PathBuf;
use std::process::ExitCode;

fn main() -> ExitCode {
    let mut disc = None;
    let mut steps = 1000u64;
    let mut stubs = PathBuf::from("build/game/stubs.txt");
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--disc" => disc = args.next().map(PathBuf::from),
            "--steps" => steps = args.next().and_then(|v| v.parse().ok()).unwrap_or(steps),
            "--stubs" => stubs = args.next().map(PathBuf::from).unwrap_or(stubs),
            _ => {
                eprintln!("usage: xem-headless --disc <chd|cue|bin> [--steps N] [--stubs build/game/stubs.txt]");
                return ExitCode::FAILURE;
            }
        }
    }
    let Some(disc) = disc else {
        eprintln!("--disc is required");
        return ExitCode::FAILURE;
    };
    match run(&disc, steps, &stubs) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("error: {error}");
            ExitCode::FAILURE
        }
    }
}

#[cfg(not(has_game_module))]
fn run(_: &std::path::Path, _: u64, _: &std::path::Path) -> Result<(), Box<dyn std::error::Error>> {
    if !xem_game::AVAILABLE {
        return Err("this build has no game module: run tools/game_module.py, then rebuild".into());
    }
    unreachable!()
}

#[cfg(has_game_module)]
fn run(disc: &std::path::Path, steps: u64, stubs: &std::path::Path) -> Result<(), Box<dyn std::error::Error>> {
    use xem_core::{Runtime, Stop, exe};

    let mut source = xem_disc::open_path(disc)?;
    let identity = xem_disc::identify(source.as_mut())?;
    let known = identity.disc.ok_or("not a known Xenogears disc")?;
    println!("disc {} ({}), {}", known.number(), known.serial(), identity.boot_path);
    let header = exe::parse(&identity.executable)?;

    let mut runtime = Runtime::new(xem_game::NativeModule::new());
    if let Ok(text) = std::fs::read_to_string(stubs) {
        runtime.services().stub_names =
            text.lines().filter_map(|l| l.split_once(' ').map(|(_, n)| n.to_string())).collect();
    }
    // The BIOS copies the executable's text to its address and jumps to pc0.
    runtime.memory().write(header.text_address, exe::text(&identity.executable, &header))?;
    let mut yields = 0u64;
    for step in 0..steps {
        match runtime.step() {
            Ok(Stop::Yield(reason)) => {
                yields += 1;
                if yields <= 5 {
                    println!("step {step}: yield {reason:?}");
                }
            }
            Ok(Stop::Restart { kind, arg }) => println!("step {step}: restart kind {kind} arg {arg:#x}"),
            Ok(Stop::Returned) => {
                println!("step {step}: the game returned");
                break;
            }
            Err(trap) => {
                println!("step {step}: {trap}");
                break;
            }
        }
    }
    for line in &runtime.services().log {
        println!("log: {line}");
    }
    println!("{yields} yields");
    Ok(())
}
