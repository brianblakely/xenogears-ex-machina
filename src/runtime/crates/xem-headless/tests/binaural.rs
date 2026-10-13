//! The Binaural sound mode on the original game (local only: it needs the
//! user's disc 1 and a game module build): it reads and never writes, its
//! bypass is the original output sample for sample, it knows the music's
//! parts and the field's emitters, it places the emitters on the side the
//! original pans them to, and a restored state starts its sound afresh.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test binaural

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::path::PathBuf;
use xem_core::binaural::Config;
use xem_core::control::Session;
use xem_game::{BootOptions, NativeModule, boot};

/// A field with three positional emitters (actors following fe 13 sounds)
/// and its music.
const EMITTER_FIELD: u32 = 15;

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn execute(session: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = session.execute(&command);
    assert!(reply.get("error").is_none(), "{command}: {reply}");
    reply
}

/// The game booted with its sound kept for a host, in Stereo, `binaural`
/// set, in the emitter field.
fn field(binaural: Option<Config>) -> Option<Session<NativeModule>> {
    let disc = disc()?;
    let (mut session, _) = boot(&disc, &BootOptions { audio_output: true, ..Default::default() }).expect("boot");
    session.set_sound_mode(1).unwrap();
    if let Some(config) = binaural {
        session.set_binaural(config);
    }
    execute(&mut session, json!({"cmd": "enter_field", "map": EMITTER_FIELD}));
    // Loading the field ran frames whose sound nobody took: the ring kept
    // only its last two seconds. Start from an empty ring.
    let spu = &mut session.runtime.services().spu;
    let mut stale = vec![0; spu.audio_queued()];
    spu.take_audio(&mut stale);
    Some(session)
}

/// Runs `count` frames, returning their sound.
fn frames(session: &mut Session<NativeModule>, count: u64) -> Vec<i16> {
    let mut audio = Vec::new();
    for _ in 0..count {
        assert!(session.frame().expect("a frame"));
        let spu = &mut session.runtime.services().spu;
        let start = audio.len();
        audio.resize(start + spu.audio_queued(), 0);
        spu.take_audio(&mut audio[start..]);
    }
    audio
}

fn on() -> Config {
    Config { enabled: true, ..Config::default() }
}

#[test]
fn binaural_reads_and_never_writes() {
    let Some(mut plain) = field(None) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut placed = field(Some(on())).unwrap();
    let mut bypassed = field(Some(Config { bypass: true, ..on() })).unwrap();
    let mut music_only = field(Some(Config { positional_effects: false, ..on() })).unwrap();
    let mut effects_only = field(Some(Config { spatial_music: false, music_immersion: 0, ..on() })).unwrap();
    let original = frames(&mut plain, 900);
    let binaural = frames(&mut placed, 900);
    let bypass = frames(&mut bypassed, 900);
    frames(&mut music_only, 900);
    frames(&mut effects_only, 900);
    // The game, the driver and the SPU run exactly as without it, whatever
    // it presents: the effects the game starts, their voices and gains.
    let hash = |s: &mut Session<NativeModule>| execute(s, json!({"cmd": "memory_hash"}));
    let spu = |s: &mut Session<NativeModule>| s.runtime.services().spu.chip().save();
    let (memory, chip) = (hash(&mut plain), spu(&mut plain));
    for session in [&mut placed, &mut bypassed, &mut music_only, &mut effects_only] {
        assert_eq!(hash(session), memory, "game memory");
        assert_eq!(spu(session), chip, "the SPU, voice volumes (the driver's pre-pan gains) and all");
    }
    // The bypass is the original output, sample for sample, a block later:
    // it starts with the samples its block held when the ring was emptied.
    let held = (0..xem_binaural::BLOCK)
        .find(|held| bypass.len() >= 2 * held && bypass.len() - 2 * held <= original.len() && bypass[2 * held..] == original[..bypass.len() - 2 * held])
        .expect("the bypass is the original mix, delayed by less than a block");
    eprintln!("bypass: identical to the original, {held} samples behind");
    // Placed, the same samples are mixed differently, at the same level.
    assert_eq!(binaural.len(), bypass.len());
    assert_ne!(binaural, bypass);
    let energy = |s: &[i16]| s.iter().map(|x| f64::from(*x).powi(2)).sum::<f64>();
    let ratio = 10.0 * (energy(&binaural) / energy(&original)).log10();
    assert!(ratio.abs() < 2.0, "the placed mix is as loud as the original within 2 dB: {ratio:.2} dB");
    eprintln!("binaural level against the original: {ratio:+.2} dB");
}

#[test]
fn music_parts_and_field_emitters_are_known_and_placed_on_the_original_side() {
    let Some(mut session) = field(Some(on())) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut checked_sides = 0;
    let mut part_sources = std::collections::HashMap::new();
    // The field's scripts start its emitters once it has loaded.
    frames(&mut session, 600);
    for step in 0..12 {
        frames(&mut session, 60);
        let status = execute(&mut session, json!({"cmd": "binaural"}));
        assert_eq!(status["presenting"], true);
        assert!(status["listener"].as_str().unwrap().starts_with("field"));
        let sources = status["sources"].as_array().unwrap();
        // Each music part keeps one source for the whole song.
        for source in sources.iter().filter(|s| s["kind"] == "music") {
            let part = (source["song"].as_str().unwrap().to_owned(), source["part"].as_u64().unwrap());
            let id = source["id"].as_u64().unwrap();
            assert_eq!(*part_sources.entry(part.clone()).or_insert(id), id, "part {part:?} changed source at step {step}");
        }
        // Every emitter voice is placed; none is a coverage gap.
        let effects: Vec<&Value> = sources.iter().filter(|s| s["kind"] == "effect").collect();
        assert!(effects.len() >= 2, "the field's emitters play: {status}");
        assert!(effects.iter().all(|s| s["emitter"] == "field emitter"), "{status}");
        assert!(status["coverage_gaps"].as_array().unwrap().is_empty(), "{status}");
        // The original pans an emitter by its place on the screen; the
        // binaural direction, from the character in the camera's
        // orientation, falls on the same side when the pan is clear of the
        // centre.
        for voice in status["voices"].as_array().unwrap().iter().filter(|v| v.get("effect_channel").is_some()) {
            let v = voice["voice"].as_u64().unwrap() as usize;
            let Some(source) = effects.iter().find(|s| s["id"] == voice["route"]) else { continue };
            let volume = session.runtime.services().spu.chip().voice(v).volume;
            let Some(unpanned) = xem_binaural::pan::unpan(volume) else { continue };
            let azimuth = source["azimuth"].as_f64().unwrap();
            if (unpanned.pan - 16384.0).abs() > 4096.0 && azimuth.abs() > 10.0 && azimuth.abs() < 170.0 {
                assert_eq!((unpanned.pan > 16384.0), azimuth > 0.0, "voice {v}: pan {} against azimuth {azimuth}", unpanned.pan);
                checked_sides += 1;
            }
        }
    }
    assert!(part_sources.len() >= 4, "the field's music has parts: {part_sources:?}");
    assert!(checked_sides > 0, "an emitter was clearly off the centre at some point");
}

#[test]
fn a_restored_state_starts_its_sound_afresh() {
    let Some(mut session) = field(Some(on())) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    frames(&mut session, 300);
    let state = session.snapshot_bytes().unwrap();
    session.restore_bytes(&state).unwrap();
    let first = frames(&mut session, 240);
    // Somewhere else in the timeline, with other sound under way.
    frames(&mut session, 500);
    session.restore_bytes(&state).unwrap();
    let again = frames(&mut session, 240);
    assert_eq!(first, again, "nothing of the abandoned timeline is heard, and the rest is deterministic");
    assert!(first.iter().any(|s| *s != 0));
}

/// Pad 1 by frame from a retained route (tests/reference-inputs), as a
/// schedule of pad commands.
fn route(name: &str) -> Value {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../tests/reference-inputs").join(name);
    let holds: Vec<Value> = serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();
    let mut commands = Vec::new();
    for hold in holds {
        commands.push(json!([hold["start"], {"cmd": "pad", "port": 0, "buttons": hold["buttons"]}]));
        commands.push(json!([hold["end"], {"cmd": "pad", "port": 0, "buttons": []}]));
    }
    commands.sort_by_key(|c| c[0].as_u64());
    json!({"cmd": "schedule", "commands": commands})
}

#[test]
fn movie_audio_keeps_its_original_mix() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // The new game's opening movie (its audio from the disc, 2960-20780 on
    // this route), plain and under Binaural.
    let mut runs = Vec::new();
    for binaural in [false, true] {
        let (mut session, _) = boot(&disc, &BootOptions { audio_output: true, ..Default::default() }).expect("boot");
        session.set_sound_mode(1).unwrap();
        if binaural {
            session.set_binaural(on());
        }
        execute(&mut session, route("new-game-sound-mode-stereo.json"));
        frames(&mut session, 3600);
        let movie = frames(&mut session, 600);
        if binaural {
            let status = execute(&mut session, json!({"cmd": "binaural"}));
            assert_eq!(status["cd"], "original (a movie plays)", "{status}");
        }
        runs.push(movie);
    }
    let (original, binaural) = (&runs[0], &runs[1]);
    assert!(original.iter().filter(|s| s.abs() > 1000).count() > 10_000, "the movie has sound");
    // The same mix a block later, within the rounding of mixing again.
    let held = (0..xem_binaural::BLOCK)
        .min_by_key(|held| {
            let n = binaural.len().min(original.len()) - 2 * xem_binaural::BLOCK;
            (0..n).step_by(97).map(|i| (i32::from(binaural[i + 2 * held]) - i32::from(original[i])).abs()).sum::<i32>()
        })
        .unwrap();
    let n = binaural.len().min(original.len()) - 2 * xem_binaural::BLOCK;
    let worst = (0..n).map(|i| (i32::from(binaural[i + 2 * held]) - i32::from(original[i])).abs()).max().unwrap();
    eprintln!("movie: the original mix {held} samples behind, within {worst}");
    assert!(worst <= 4, "movie audio is mixed as the original: {worst}");
}

#[test]
fn fast_forward_and_rewind_keep_presenting() {
    let Some(mut session) = field(Some(on())) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    execute(&mut session, json!({"cmd": "rewind_configure", "max_bytes": 64 << 20, "interval_seconds": 1, "history_seconds": 30}));
    frames(&mut session, 300);
    // Fast-forward: the music keeps its tempo, so a second of game time at
    // 10x gives a tenth of a second of sound, all presented.
    execute(&mut session, json!({"cmd": "fast_forward", "enabled": true}));
    let fast = frames(&mut session, 600);
    execute(&mut session, json!({"cmd": "fast_forward", "enabled": false}));
    let seconds = fast.len() as f64 / 2.0 / 44_100.0;
    assert!((0.8..1.2).contains(&seconds), "{seconds} s of sound for 10 s of game");
    assert!(fast.iter().any(|s| *s != 0));
    let status = execute(&mut session, json!({"cmd": "binaural"}));
    assert_eq!(status["presenting"], true);
    // Rewind (its checkpoints a second of the host's time apart): the
    // presentation starts again from the earlier state.
    let history = execute(&mut session, json!({"cmd": "status"}))["rewind"]["states"].as_u64().unwrap();
    assert!(history >= 1, "a checkpoint to go back to");
    execute(&mut session, json!({"cmd": "rewind", "steps": 1}));
    let after = frames(&mut session, 120);
    assert!(after.iter().any(|s| *s != 0));
    let status = execute(&mut session, json!({"cmd": "binaural"}));
    assert_eq!(status["presenting"], true);
    assert!(status["sources"].as_array().unwrap().iter().any(|s| s["kind"] == "effect"), "the emitters are placed again: {status}");
}
