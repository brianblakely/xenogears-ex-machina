//! The Japanese release beside the North American one (docs/runtime.md,
//! Japanese): with the player's Japanese discs beside disc 1, the game's
//! text reads the Japanese release's own in Japanese, drawn from its font,
//! the threshold following the language; its movies and battle voices play
//! the Japanese release's where the setting asks for them, the field going
//! on after a movie as in English; a state saved without the Japanese discs
//! restores beside them, not the other way; the movies' and voices'
//! language is recorded, so a replay reaches the recording's memory. Local
//! only: it needs the user's North American disc 1 and Japanese discs
//! beside it, and a game module build.
//!
//!     cargo test --release -p xem-headless --test japanese

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::path::PathBuf;
use xem_core::control::Session;
use xem_core::locale::Language;
use xem_game::{BootOptions, NativeModule, boot, boot_source};

/// `text_font_two_byte_threshold`, the field's dialogue windows (their
/// message +0xA8 and text pointer +0x34, 4 windows 0x498 apart), the
/// field's movie request and frame, and the field map.
const THRESHOLD: u32 = 0x8005_934C;
const WINDOWS: u32 = 0x800C_2698;
const WINDOW_SIZE: u32 = 0x498;
const MOVIE_REQUESTED: u32 = 0x800A_DB70;
const MOVIE_FRAME: u32 = 0x800B_06A0;
const MAP: u32 = 0x8004_F34C;
/// `cd_file_index`: the game's file index.
const FILE_INDEX: u32 = 0x8004_FDF0;

/// Disc 1 with the Japanese discs beside it.
fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    (path.exists() && !xem_disc::japanese_discs_beside(&path).is_empty()).then_some(path)
}

fn run(session: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = session.execute(&command);
    assert!(reply.get("error").is_none(), "{command} -> {reply}");
    reply
}

fn word(session: &mut Session<NativeModule>, address: u32) -> u32 {
    let mut bytes = [0; 4];
    session.runtime.memory().read(address, &mut bytes).expect("game memory");
    u32::from_le_bytes(bytes)
}

/// Where the Japanese text lies: after the shipped languages' image.
fn in_japanese(pointer: u32) -> bool {
    let shipped = xem_core::locale::IMAGE.as_ref().unwrap().bytes.len() as u32;
    pointer >= xem_core::locale::BASE + shipped && pointer < xem_core::locale::BASE + 0x0100_0000
}

/// Field 12's opening message in a window: its text pointer.
fn opening_message(session: &mut Session<NativeModule>) -> u32 {
    run(session, json!({"cmd": "enter_field", "map": 12}));
    let reply = run(session, json!({"cmd": "run_until", "max_frames": 3000, "until": {"path": "field_work.open_windows", "not_equals": 0}}));
    assert_eq!(reply["met"], true, "{reply}");
    run(session, json!({"cmd": "frames", "count": 30}));
    let window = (0..4).find(|w| word(session, WINDOWS + w * WINDOW_SIZE + 0xA8) != 0).expect("an open window");
    word(session, WINDOWS + window * WINDOW_SIZE + 0x18 + 0x1C)
}

/// In Japanese a field window reads the Japanese release's message in its
/// font's codes (`F0 c`, `F1`-`F7` two-byte), the threshold lowered while
/// Japanese is active; in English the original's in RAM, the threshold the
/// font's own.
#[test]
fn a_window_shows_the_japanese_releases_message_in_japanese() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 with the Japanese discs beside it");
        return;
    };
    let (mut session, _) = boot(&disc, &BootOptions::default()).expect("boot");
    assert_eq!(run(&mut session, json!({"cmd": "status"}))["japanese"], true, "the Japanese discs are installed");
    session.set_language(Language::Japanese);
    let pointer = opening_message(&mut session);
    assert!(in_japanese(pointer), "the window reads the Japanese text ({pointer:#x})");
    let mut text = [0u8; 8];
    session.runtime.memory().read(pointer, &mut text).unwrap();
    assert!((0xF0..=0xF7).contains(&text[0]) || text[0] == 0x0F, "the Japanese font's codes: {text:02x?}");
    assert_eq!(word(&mut session, THRESHOLD), 0xF0, "the threshold lowered");
    session.set_language(Language::English);
    run(&mut session, json!({"cmd": "frames", "count": 1}));
    assert_eq!(word(&mut session, THRESHOLD), 0xFE, "the font's own threshold");

    let (mut english, _) = boot(&disc, &BootOptions::default()).expect("boot");
    let pointer = opening_message(&mut english);
    assert!((0x8000_0000..0x8020_0000).contains(&pointer), "English: the message in RAM ({pointer:#x})");
}

/// Map 25's movie (#7) with Japanese voices: the game's index gives the
/// Japanese release's movie, which plays the same frames to the same end
/// with other sound, and the field goes on to map 23 as in English.
#[test]
fn a_movie_plays_the_japanese_releases_sound_to_the_same_end() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 with the Japanese discs beside it");
        return;
    };
    let play = |japanese: bool| {
        let (mut session, _) = boot(&disc, &BootOptions { audio_output: true, ..Default::default() }).expect("boot");
        session.set_voices(japanese);
        session.set_auto_advance(1).unwrap();
        run(&mut session, json!({"cmd": "enter_field", "map": 25}));
        run(&mut session, json!({"cmd": "run_until", "max_frames": 12000, "until": {"path": "field_movie_requested", "not_equals": 0}}));
        session.set_auto_advance(0).unwrap();
        // The movie's file (US slot 8) as the index gives it.
        let index = word(&mut session, FILE_INDEX);
        let mut record = [0u8; 7];
        session.runtime.memory().read(index + 7 * 8, &mut record).unwrap();
        let size = i32::from_le_bytes(record[3..].try_into().unwrap());
        let mut sound = Vec::new();
        let mut frames = 0;
        while word(&mut session, MOVIE_REQUESTED) != 0 && frames < 9000 {
            let last = word(&mut session, MOVIE_FRAME);
            session.frame().unwrap();
            let spu = &mut session.runtime.services().spu;
            let start = sound.len();
            sound.resize(start + spu.audio_queued(), 0i16);
            spu.take_audio(&mut sound[start..]);
            frames += 1;
            if word(&mut session, MOVIE_REQUESTED) == 0 {
                assert_eq!(last, 1059, "the movie's last frame");
            }
        }
        run(&mut session, json!({"cmd": "frames", "count": 600}));
        (size, frames, word(&mut session, MAP) & 0xFFF, sound)
    };
    let (english_size, english_frames, english_map, english_sound) = play(false);
    let (size, frames, map, sound) = play(true);
    assert_eq!(english_size, 24_985_856, "the North American movie");
    assert_eq!(size, 24_929_792, "the Japanese release's movie");
    assert_eq!((frames, map), (english_frames, english_map), "the same frames, and map 23 after");
    assert_eq!(map, 23);
    let differ = english_sound.chunks(44_100).zip(sound.chunks(44_100))
        .filter(|(a, b)| a.iter().zip(*b).map(|(x, y)| (i32::from(*x) - i32::from(*y)).unsigned_abs()).max().unwrap_or(0) > 2000).count();
    assert!(differ * 2 > english_sound.len() / 44_100, "other sound through most of the movie ({differ} half-seconds)");
}

/// The debug battle on foot with Japanese voices: the battle voices'
/// files are the Japanese release's, Fei's attacks sound otherwise and the
/// battle goes on as in English.
#[test]
fn a_battle_plays_the_japanese_releases_voices() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 with the Japanese discs beside it");
        return;
    };
    let fight = |japanese: bool| {
        let (mut session, _) = boot(&disc, &BootOptions { audio_output: true, ..Default::default() }).expect("boot");
        session.set_voices(japanese);
        run(&mut session, json!({"cmd": "frames", "count": 60}));
        run(&mut session, json!({"cmd": "write", "path": "mode_battle_standalone", "value": 1}));
        run(&mut session, json!({"cmd": "boot_mode", "mode": 2}));
        run(&mut session, json!({"cmd": "frames", "count": 900}));
        run(&mut session, json!({"cmd": "press", "buttons": ["start"], "frames": 1}));
        run(&mut session, json!({"cmd": "frames", "count": 500}));
        let spu = &mut session.runtime.services().spu;
        let mut drop = vec![0i16; spu.audio_queued()];
        spu.take_audio(&mut drop);
        for buttons in ["cross", "triangle", "square", "cross"] {
            run(&mut session, json!({"cmd": "press", "buttons": [buttons], "frames": 1}));
            run(&mut session, json!({"cmd": "frames", "count": 60}));
        }
        let spu = &mut session.runtime.services().spu;
        let mut sound = vec![0i16; spu.audio_queued()];
        spu.take_audio(&mut sound);
        (session.runtime.services().mode, sound)
    };
    let (english_mode, english) = fight(false);
    let (mode, japanese) = fight(true);
    assert_eq!((mode, english_mode), (Some(2), Some(2)), "the battle goes on");
    let loud = |a: &[i16], b: &[i16]| a.iter().zip(b).filter(|(x, y)| (i32::from(**x) - i32::from(**y)).abs() > 4000).count();
    assert!(loud(&english, &japanese) > 1000, "the attacks' voices differ");
}

/// A state saved in a session without the Japanese discs restores beside
/// them; one saved beside them does not restore without them.
#[test]
fn a_state_without_the_japanese_discs_restores_beside_them() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 with the Japanese discs beside it");
        return;
    };
    let alone = || boot_source(xem_disc::open_path(&disc).unwrap(), &BootOptions::default()).expect("boot").0;
    let mut without = alone();
    run(&mut without, json!({"cmd": "frames", "count": 600}));
    assert_eq!(run(&mut without, json!({"cmd": "status"}))["japanese"], false);
    let state = without.snapshot_bytes().unwrap();
    let (mut beside, _) = boot(&disc, &BootOptions::default()).expect("boot");
    beside.restore_bytes(&state).expect("restores beside the Japanese discs");
    run(&mut beside, json!({"cmd": "frames", "count": 60}));
    let state = beside.snapshot_bytes().unwrap();
    assert!(alone().restore_bytes(&state).is_err(), "not without them");
}

/// The movies' and voices' language is recorded: a replay of a recording
/// that changes it while the boot's movies play reaches its memory.
#[test]
fn the_voices_are_recorded_and_a_replay_reaches_the_recordings_memory() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 with the Japanese discs beside it");
        return;
    };
    let (mut session, _) = boot(&disc, &BootOptions::default()).expect("boot");
    run(&mut session, json!({"cmd": "frames", "count": 60}));
    let initial = session.snapshot_bytes().expect("a state");
    run(&mut session, json!({"cmd": "record"}));
    session.set_voices(true);
    run(&mut session, json!({"cmd": "frames", "count": 900}));
    run(&mut session, json!({"cmd": "voices", "language": "en"}));
    run(&mut session, json!({"cmd": "frames", "count": 600}));
    let hash = run(&mut session, json!({"cmd": "memory_hash"}));
    let mut log = run(&mut session, json!({"cmd": "stop_recording"}));
    let voices: Vec<&str> = log["controls"].as_array().unwrap().iter()
        .filter(|c| c[1]["cmd"] == "voices").filter_map(|c| c[1]["language"].as_str()).collect();
    assert_eq!(voices, ["ja", "en"], "each change");
    log["cmd"] = json!("replay");
    let (mut other, _) = boot(&disc, &BootOptions::default()).expect("boot");
    other.restore_bytes(&initial).expect("the recording's start");
    run(&mut other, log);
    run(&mut other, json!({"cmd": "frames", "count": 1500}));
    assert_eq!(run(&mut other, json!({"cmd": "memory_hash"})), hash, "the replay takes the recording's voices");
}
