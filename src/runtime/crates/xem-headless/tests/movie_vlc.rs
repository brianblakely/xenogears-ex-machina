//! Local-only: the game boots from the user's Disc 1 into the opening movie,
//! whose frames stream through the drive model, libcd and the St ring and
//! are decoded by the port's DecDCTvlc (src/port/libpress.c); the run-level
//! halfwords of the first frames it decodes equal what xem-media's reference
//! BS decoder makes of the same frames read straight from the disc. Needs the
//! game module with every library the boot reaches up to the movie, and the
//! disc (XEM_DISC1, default discs/Xenogears disc 1.chd in the repository);
//! skipped without the disc.

#![cfg(has_game_module)]

use xem_core::{Runtime, exe};
use xem_disc::sector::{SECTOR_SIZE, Sector};
use xem_media::{StrSectorHeader, bitstream};

/// mdec.c statics: movie_decoder (vlc_index, vlc_buffers[2]), movie_vlc_pending,
/// movie_ring_frame (the frame of the bitstream decoded last), movie_player_state.
const MOVIE_DECODER: u32 = 0x801E_8918;
const MOVIE_VLC_PENDING: u32 = 0x801D_68C0;
const MOVIE_RING_FRAME: u32 = 0x801E_8994;
const MOVIE_PLAYER_STATE: u32 = 0x801E_8964;
/// The opening movie's first sector on Disc 1 (the movie mode's first read).
const OPENING_MOVIE_LBA: u32 = 106_105;
/// Frames of the movie to check.
const CHECKED_FRAMES: usize = 60;

fn disc_path() -> std::path::PathBuf {
    std::env::var_os("XEM_DISC1").map(Into::into).unwrap_or_else(|| {
        std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd")
    })
}

/// Movie frame `number`'s bitstream and first sector header, from the disc.
fn movie_frame(disc: &mut dyn xem_disc::SectorSource, number: u32) -> (Vec<u8>, StrSectorHeader) {
    let mut raw = [0u8; SECTOR_SIZE];
    let mut data = Vec::new();
    let mut first = None;
    for lba in OPENING_MOVIE_LBA.. {
        disc.read_sector(lba, &mut raw).unwrap();
        let user = Sector::new(&raw).unwrap().form1_window();
        let Some(header) = StrSectorHeader::parse(user) else { continue };
        if header.frame < number {
            continue;
        }
        if header.frame > number {
            break;
        }
        first.get_or_insert(header);
        data.extend_from_slice(&user[0x20..]);
    }
    let first = first.expect("the frame is in the movie");
    data.truncate(first.frame_size as usize);
    (data, first)
}

#[test]
fn port_vlc_matches_the_reference_decoder_on_the_opening_movie() {
    let path = disc_path();
    if !path.exists() {
        eprintln!("skipped: no disc at {}", path.display());
        return;
    }
    let mut source = xem_disc::open_path(&path).unwrap();
    let identity = xem_disc::identify(source.as_mut()).unwrap();
    let header = exe::parse(&identity.executable).unwrap();
    let mut disc = xem_disc::open_path(&path).unwrap();
    // The movie library (index slot 19) at 0x801D3000 shows the movie mode.
    let library = xem_disc::game_index(&identity.executable).unwrap().record(19).unwrap();
    let library = xem_disc::read_data(disc.as_mut(), library.lba, 0x100).unwrap();

    let mut runtime = Runtime::new(xem_game::NativeModule::new());
    runtime.memory().write(header.text_address, exe::text(&identity.executable, &header)).unwrap();
    runtime.services().insert_disc(source);

    let mut checked = Vec::new();
    for frame in 0..3000 {
        if let Err(trap) = runtime.run_frame() {
            panic!("frame {frame}: {trap}");
        }
        let memory = runtime.memory();
        let mut loaded = vec![0u8; library.len()];
        memory.read(0x801D_3000, &mut loaded).unwrap();
        if loaded != library || (memory.read_u32(MOVIE_PLAYER_STATE).unwrap() & 0xFF) == 0 {
            continue;
        }
        let number = memory.read_u32(MOVIE_RING_FRAME).unwrap();
        if number == 0 || memory.read_u32(MOVIE_VLC_PENDING).unwrap() != 0 || checked.contains(&number) {
            continue;
        }
        let (bitstream_bytes, first) = movie_frame(disc.as_mut(), number);
        let macroblocks = (first.width as usize).div_ceil(16) * (first.height as usize).div_ceil(16);
        let reference = bitstream::decode(&bitstream_bytes, Some(macroblocks)).unwrap();
        let size = u16::from_le_bytes([bitstream_bytes[0], bitstream_bytes[1]]);
        // The frame decoded last is in either run-level buffer: movie_decode
        // may have handed it to the MDEC (DecDCTin sets the depth bits of its
        // command word) and switched buffers since.
        let count = 2 + reference.halfwords.len() + 65;
        let mut found = None;
        for buffer in [0, 1] {
            let address = memory.read_u32(MOVIE_DECODER + 4 + 4 * buffer).unwrap();
            let mut bytes = vec![0u8; count * 2];
            memory.read(address, &mut bytes).unwrap();
            let halfwords: Vec<u16> = bytes.chunks_exact(2).map(|h| u16::from_le_bytes([h[0], h[1]])).collect();
            if halfwords[0] == size && halfwords[1] & !0x0A00 == 0x3800 & !0x0A00 {
                found = Some(halfwords);
            }
        }
        let halfwords = found.unwrap_or_else(|| panic!("frame {number}: no run-level buffer holds it"));
        let body = &halfwords[2..2 + reference.halfwords.len()];
        let difference = body.iter().zip(&reference.halfwords).position(|(a, b)| a != b);
        assert_eq!(difference, None, "frame {number}: run-level halfwords differ");
        assert!(halfwords[2 + reference.halfwords.len()..].iter().all(|&h| h == 0xFE00), "frame {number}: end padding");
        eprintln!(
            "game frame {frame}: movie frame {number} ({}x{}, version {}): {} run-level halfwords match",
            first.width,
            first.height,
            reference.header.version,
            reference.halfwords.len()
        );
        checked.push(number);
        if checked.len() == CHECKED_FRAMES {
            return;
        }
    }
    panic!("the movie decoded only frames {checked:?}");
}
