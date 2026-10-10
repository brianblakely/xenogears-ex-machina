//! Print what a disc image is and what its file index holds.
//!
//! cargo run -p xem-disc --example disc-info -- <image.chd|track.cue|track.bin>

use std::path::PathBuf;
use std::process::ExitCode;

use xem_disc::identify::hex;
use xem_disc::{Index, game_index, identify, open_path};

fn run(path: PathBuf) -> xem_disc::Result<()> {
    let mut source = open_path(&path)?;
    println!("image        {}", path.display());
    println!("sectors      {}", source.sector_count());
    let id = identify(&mut source)?;
    match id.disc {
        Some(disc) => println!(
            "disc         Xenogears disc {} ({})",
            disc.number(),
            disc.serial()
        ),
        None => println!("disc         unknown"),
    }
    println!(
        "boot         {} at LBA {}, {} bytes",
        id.boot_path,
        id.boot_lba,
        id.executable.len()
    );
    println!("boot sha256  {}", hex(&id.sha256));
    let index = match game_index(&id.executable) {
        Ok(index) => index,
        Err(err) => {
            println!("index        none ({err})");
            return Ok(());
        }
    };
    let files = index.entries().filter(|(_, r)| r.size > 0).count();
    let directories = index.entries().filter(|(_, r)| r.size < 0).count();
    let bytes: u64 = index.entries().map(|(_, r)| r.size.max(0) as u64).sum();
    let last = index.entries().last().map_or(0, |(slot, _)| slot);
    let selectable = (0..xem_disc::index::DISC_NUMBER_ENTRY)
        .filter(|&d| index.directory_start(d).is_some())
        .count();
    println!(
        "index        {} records used (last slot {last}): {files} files, {directories} directory records, {bytes} bytes",
        index.entries().count()
    );
    println!(
        "directories  {selectable} table entries in use, disc number {:?}",
        index.disc_number()
    );
    let on_disc = Index::from_disc(&mut source)?;
    let same = on_disc.records() == index.records()
        && on_disc.directory_table() == &index.directory_table()[..on_disc.directory_table().len()];
    println!(
        "disc copy    {}",
        if same {
            "matches the embedded index"
        } else {
            "differs from the embedded index"
        }
    );
    Ok(())
}

fn main() -> ExitCode {
    let Some(path) = std::env::args_os().nth(1) else {
        eprintln!("usage: disc-info <image.chd|track.cue|track.bin>");
        return ExitCode::from(2);
    };
    match run(PathBuf::from(path)) {
        Ok(()) => ExitCode::SUCCESS,
        Err(err) => {
            eprintln!("disc-info: {err}");
            ExitCode::FAILURE
        }
    }
}
