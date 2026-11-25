//! The movies' subtitles on the original game (local only: it needs the
//! user's discs and a game module build). Through the new-game movie (field
//! 4's movie 13: file 15 of directory (0x18, 1)) the runtime reads each of
//! its 36 lines from the first movie frame Perfect Works Build times it on
//! until its end and no line on its other frames, and draws a line in the
//! game's own font in every language of the game's text (in Japanese from
//! the Japanese release's font where the Japanese discs lie beside disc 1).
//! Every other movie with subtitles, played by the debug menu's movie
//! request on its own disc (the movie mode, which plays the same files),
//! shows each of its lines likewise.
//!
//!     XEM_DISC1=<disc 1 image> XEM_DISC2=<disc 2 image> cargo test --release -p xem-headless --test subtitles
//!
//! `XEM_SUBTITLE_PNGS=<directory>` also writes each drawing there as a PNG.

#![cfg(all(has_game_module, not(debug_assertions)))]

use std::collections::BTreeMap;
use std::path::PathBuf;
use std::time::{Duration, Instant};
use xem_core::control::button_bit;
use xem_core::locale::Language;
use xem_core::debug::Request;
use xem_core::subtitles::{self, Line, Picture};
use xem_game::boot;

/// The movie player's file and the directory it started it in.
const PLAYER_FILE: u32 = 0x801E_8974;
const PLAYER_DIRECTORY: u32 = 0x801E_899C;

fn disc_n(number: u8) -> Option<PathBuf> {
    let path = std::env::var_os(format!("XEM_DISC{number}"))
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join(format!("../../../../discs/Xenogears disc {number}.chd")));
    path.exists().then_some(path)
}

fn disc() -> Option<PathBuf> {
    disc_n(1)
}

/// The file the movie player streams and its directory's group, as the
/// game's memory holds them.
fn player(session: &mut xem_core::control::Session<xem_game::NativeModule>) -> (u16, u32) {
    let mut player = [0; 8];
    session.runtime.memory().read(PLAYER_FILE, &mut player[..2]).unwrap();
    session.runtime.memory().read(PLAYER_DIRECTORY, &mut player[4..]).unwrap();
    (u16::from_le_bytes([player[0], player[1]]), u32::from_le_bytes(player[4..].try_into().unwrap()))
}

fn save(picture: &Picture, name: &str) {
    let Some(dir) = std::env::var_os("XEM_SUBTITLE_PNGS").map(PathBuf::from) else { return };
    std::fs::create_dir_all(&dir).unwrap();
    let file = std::fs::File::create(dir.join(format!("{name}.png"))).unwrap();
    let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), picture.width, picture.height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    encoder.write_header().unwrap().write_image_data(&picture.rgba).unwrap();
}

#[test]
fn the_new_game_movies_lines_show_on_their_frames_and_draw_in_every_language() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    const ROUTE: [(u64, u64, &str); 5] = [(901, 910, "start"), (1300, 1310, "start"), (1500, 1510, "cross"), (1750, 1760, "up"), (1810, 1820, "cross")];
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    let japanese = session.runtime.japanese();
    let expected: Vec<Line> = subtitles::lines().iter().filter(|l| (l.disc, l.file) == (1, 15)).copied().collect();
    assert_eq!(expected.len(), 36);
    // The movie frames each line showed on, first and last; the time the
    // lookups took.
    let mut seen: BTreeMap<u16, (u32, u32)> = BTreeMap::new();
    let mut lookups = (0u32, Duration::ZERO);
    let mut drawn = false;
    for n in 0..13_200u64 {
        let held = ROUTE.iter().filter(|(start, end, _)| (*start..*end).contains(&n)).fold(0, |bits, (_, _, name)| bits | button_bit(name).unwrap());
        session.runtime.services().pads.0[0].buttons = held;
        assert!(session.frame().expect("frame"));
        let started = Instant::now();
        let shown = session.runtime.subtitle();
        lookups = (lookups.0 + 1, lookups.1 + started.elapsed());
        let mode = session.runtime.services().mode;
        let Some(movie) = xem_core::movie::movie(session.runtime.memory(), mode) else {
            assert_eq!(shown, None, "frame {n}: a line without a movie");
            continue;
        };
        let new_game = player(&mut session) == (15, 0x18);
        let frame = u32::try_from(movie.frame).unwrap_or(u32::MAX);
        let timed = expected.iter().find(|l| new_game && (l.first..l.end).contains(&frame));
        assert_eq!(shown.map(|(line, _)| line), timed.copied(), "frame {n}: movie frame {frame}");
        let Some((line, language)) = shown else { continue };
        assert_eq!(language, Language::English, "the session's language");
        let entry = seen.entry(line.number).or_insert((frame, frame));
        entry.1 = frame;
        if drawn {
            continue;
        }
        drawn = true;
        // Drawn from the fonts the game holds, in every language.
        for language in Language::ALL.into_iter().filter(|&l| l != Language::Japanese || japanese) {
            let picture = session.runtime.subtitle_picture(&line, language).unwrap_or_else(|| panic!("{} drawn", language.code()));
            let text = subtitles::text(&line, language).unwrap();
            let lines = text.matches("{n}").count() as u32 + 1;
            assert_eq!(picture.height, 11 + (lines - 1) * subtitles::LINE_PITCH + 2, "{}", language.code());
            assert!((40..=306).contains(&picture.width), "{}: {} wide", language.code(), picture.width);
            let ink = picture.rgba.chunks(4).filter(|p| p[3] == 0xFF && p[0] > 0x80).count();
            assert!(ink > 200, "{}: {ink} pixels of text", language.code());
            save(&picture, &format!("{}-{}", line.name(), language.code()));
        }
        // "This is an emergency. Level 1 alert.": 36 narrow glyphs.
        assert_eq!(session.runtime.subtitle_picture(&line, Language::English).unwrap().width, 36 * 8 + 2);
    }
    for line in &expected {
        let (first, last) = seen.get(&line.number).unwrap_or_else(|| panic!("{} never shown", line.name()));
        assert_eq!((*first, *last), (line.first, line.end - 1), "{}", line.name());
    }
    assert!(drawn);
    eprintln!("{} lookups, {:?} each; Japanese drawn: {japanese}", lookups.0, lookups.1 / lookups.0);
}

#[test]
fn every_movies_lines_show_on_their_frames_as_the_movie_mode_plays_them() {
    for number in [1u8, 2] {
        let Some(disc) = disc_n(number) else {
            eprintln!("skipping disc {number}: no image (set XEM_DISC{number})");
            continue;
        };
        let mut movies: Vec<u16> = subtitles::lines().iter().filter(|l| l.disc == number).map(|l| l.file).collect();
        movies.dedup();
        for file in movies {
            let expected: Vec<Line> = subtitles::lines().iter().filter(|l| (l.disc, l.file) == (number, file)).copied().collect();
            let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
            session.request_debug(Request::Movie { movie: (file - 2) as u8, kind: 1 }).unwrap();
            for _ in 0..1501 {
                if !session.debug.busy() {
                    break;
                }
                session.tick_debug().unwrap();
            }
            assert!(!session.debug.busy(), "disc {number} file {file}: {}", session.debug_status());
            let mut seen: BTreeMap<u16, (u32, u32)> = BTreeMap::new();
            let last = expected.last().unwrap().end;
            let mut shown_frame = 0;
            for n in 0..(last as u64 + 120) * 4 + 3000 {
                assert!(session.frame().expect("frame"));
                let shown = session.runtime.subtitle();
                let mode = session.runtime.services().mode;
                let Some(movie) = xem_core::movie::movie(session.runtime.memory(), mode) else {
                    assert_eq!(shown, None, "disc {number} file {file} frame {n}: a line without a movie");
                    continue;
                };
                assert_eq!(player(&mut session), (file, 0x18), "disc {number}: the movie mode plays the file asked for");
                let frame = u32::try_from(movie.frame).unwrap_or(0);
                let timed = expected.iter().find(|l| (l.first..l.end).contains(&frame));
                assert_eq!(shown.map(|(line, _)| line), timed.copied(), "disc {number} file {file} game frame {n}: movie frame {frame}");
                if let Some((line, _)) = shown {
                    seen.entry(line.number).or_insert((frame, frame)).1 = frame;
                }
                shown_frame = shown_frame.max(frame);
                if frame > last + 4 {
                    break;
                }
            }
            for line in &expected {
                let (first, end) = seen.get(&line.number).unwrap_or_else(|| panic!("{} never shown (the movie reached frame {shown_frame})", line.name()));
                assert_eq!((*first, *end), (line.first, line.end - 1), "{}", line.name());
            }
            eprintln!("disc {number} file {file}: {} lines on their frames (to movie frame {shown_frame})", expected.len());
        }
    }
}
