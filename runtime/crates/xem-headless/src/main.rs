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
    use xem_core::session::{Condition, Session, StepLog, parse_stub_names};

    let mut source = xem_disc::open_path(disc)?;
    let identity = xem_disc::identify(source.as_mut())?;
    let known = identity.disc.ok_or("not a known Xenogears disc")?;
    println!("disc {} ({}), {}", known.number(), known.serial(), identity.boot_path);

    let stub_names = std::fs::read_to_string(stubs).map(|text| parse_stub_names(&text)).unwrap_or_default();
    let mut session = Session::new(xem_game::NativeModule::new(), stub_names);
    // The BIOS copies the executable's text to its address and jumps to pc0.
    session.load_executable(&identity.executable)?;
    // The browser host logs the same lines (runtime/crates/xem-web).
    let mut log = StepLog::default();
    session.run_until(&Condition::Halt, steps, |step, outcome| {
        if let Some(line) = log.record(step, outcome) {
            println!("{line}");
        }
    });
    let status = session.status();
    for line in &status.log {
        println!("log: {line}");
    }
    println!("{} yields", status.yields);
    println!("ram digest {:016x}", session.digest()?);
    Ok(())
}
