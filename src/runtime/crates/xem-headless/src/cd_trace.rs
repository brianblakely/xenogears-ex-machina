//! `--cd-trace`: the data sectors the drive delivered, as runs matched to the
//! disc's file index, each with where the file's bytes now are in game RAM
//! (checked against the file read straight from the disc).

use xem_core::devices::cd::DeliveredSector;
use xem_core::memory::{GameMemory, RAM_BASE, RAM_SIZE};
use xem_disc::{Index, SectorSource};

/// Cycles per NTSC frame (xem_core::clock::VBLANK_CYCLES).
const FRAME: u64 = xem_core::clock::VBLANK_CYCLES;

pub fn report(log: &[DeliveredSector], index: &Index, disc: &mut dyn SectorSource, memory: &dyn GameMemory) {
    let mut ram = vec![0u8; RAM_SIZE as usize];
    if memory.read(RAM_BASE, &mut ram).is_err() {
        println!("cd: game RAM unreadable");
        return;
    }
    let mut runs: Vec<(u32, u32, u64, u64)> = Vec::new();
    for sector in log {
        match runs.last_mut() {
            Some(run) if run.0 + run.1 == sector.lba => {
                run.1 += 1;
                run.3 = sector.cycle;
            }
            _ => runs.push((sector.lba, 1, sector.cycle, sector.cycle)),
        }
    }
    println!("cd: {} data sectors in {} runs", log.len(), runs.len());
    for (lba, count, first, last) in runs {
        println!(
            "cd: LBA {lba}+{count} frames {}..{} ({} cycles)",
            first / FRAME,
            last / FRAME,
            last - first
        );
        // The files the run read, file by file (the smallest record starting
        // at a sector: archives share their first file's).
        let mut at = lba;
        while at < lba + count {
            let Some((slot, record)) = index.entries().filter(|(_, r)| r.size > 0 && r.lba == at).min_by_key(|(_, r)| r.size) else {
                println!("cd:   LBA {at}..{}: no file starts here", lba + count);
                break;
            };
            let sectors = record.sectors().max(1);
            let mut line = format!("cd:   slot {slot}: LBA {at}+{sectors}, {} bytes", record.size);
            if at + sectors <= lba + count {
                let data = xem_disc::read_data(disc, record.lba, record.size as usize).unwrap_or_default();
                let found = ram.windows(data.len().max(1)).position(|w| !data.is_empty() && w == &data[..]);
                line += &match found {
                    Some(address) => format!(", in RAM at {:#010x}", RAM_BASE + address as u32),
                    None => ", not (or no longer) in RAM".to_string(),
                };
            }
            println!("{line}");
            at += sectors;
        }
    }
}
