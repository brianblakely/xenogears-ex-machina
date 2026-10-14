//! The Binaural sound mode on the original game (local only: it needs the
//! user's disc 1 and a game module build): it reads and never writes, its
//! bypass is the original output sample for sample, it knows the music's
//! parts and the field's emitters, it places the emitters on the side the
//! original pans them to, and a restored state starts its sound afresh.
//! Along a walk among field 509's emitters (the session log
//! tests/reference-inputs/binaural-field509-walk.json), every presentation
//! setting leaves the game, the driver's voice volumes and the SPU as they
//! are in plain Stereo frame by frame, each placed source reports the gain
//! the driver resolved before panning, and Mono and Wide differ from
//! Stereo only in what the sound-mode findings name.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test binaural

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::path::{Path, PathBuf};
use xem_core::binaural::Config;
use xem_core::control::Session;
use xem_core::memory::GameMemory;
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
    let arranged = frames(&mut music_only, 900);
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
    // The arranged music leans the way the original leans (less far: heard
    // around the head, each ear also hears the other side).
    let side = |s: &[i16], e: usize| s.iter().skip(e).step_by(2).map(|x| f64::from(*x).powi(2)).sum::<f64>();
    let balance = |s: &[i16]| 10.0 * (side(s, 0) / side(s, 1)).log10();
    let (before, after) = (balance(&original), balance(&arranged));
    eprintln!("left/right balance: original {before:+.2} dB, arranged music {after:+.2} dB");
    assert!(before.abs() < 2.0 || (after.signum() == before.signum() && after.abs() <= before.abs() + 1.0),
        "the music's balance is kept: {before:+.2} against {after:+.2} dB");
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

#[test]
fn battle_sounds_come_from_their_combatants_around_a_fixed_centre() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // The slice's forest route: its encounter's battle runs from about
    // frame 6000.
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../tests/reference-inputs/slice-forest.json");
    let log: Value = serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();
    let (mut session, _) = boot(&disc, &BootOptions { audio_output: true, ..Default::default() }).expect("boot");
    session.set_sound_mode(1).unwrap();
    session.set_binaural(on());
    execute(&mut session, json!({"cmd": "schedule", "commands": log["commands"]}));
    frames(&mut session, 6000);
    let mut anchors = Vec::new();
    let mut placed = std::collections::HashSet::new();
    for _ in 0..180 {
        frames(&mut session, 20);
        if session.runtime.services().mode != Some(2) {
            continue;
        }
        let status = execute(&mut session, json!({"cmd": "binaural"}));
        if status["listening_position"].is_null() {
            // The battle is placing its combatants, or has removed them.
            continue;
        }
        assert!(status["listener"].as_str().unwrap().starts_with("battle"), "{status}");
        anchors.push(status["listening_position"].clone());
        for source in status["sources"].as_array().unwrap().iter().filter(|s| s["emitter"] == "sprite") {
            // Where the sprite is now, as the scene read it at the frame's end.
            let sprite = u32::from_str_radix(source["follows"].as_str().unwrap().trim_start_matches("0x"), 16).unwrap();
            let mut bytes = [0u8; 12];
            session.runtime.memory().read(sprite, &mut bytes).unwrap();
            let at: Vec<f64> = bytes.chunks(4).map(|c| f64::from(i32::from_le_bytes([c[0], c[1], c[2], c[3]])) / 65536.0).collect();
            let position: Vec<f64> = source["position"].as_array().unwrap().iter().map(|v| v.as_f64().unwrap()).collect();
            assert_eq!(position, at, "{source}");
            placed.insert(source["effect"].as_str().unwrap().to_owned());
        }
    }
    assert!(anchors.len() > 50, "the battle ran");
    // The listener stays at the formation's centre on the floor (its height
    // follows the combatants' average) whoever acts.
    let floor = |a: &Value| (a[0].as_f64().unwrap(), a[2].as_f64().unwrap());
    assert!(anchors.iter().all(|a| floor(a) == floor(&anchors[0])), "{anchors:?}");
    assert!(placed.len() >= 5, "the combatants' sounds are placed at them: {placed:?}");
    eprintln!("battle: {} effects placed at their sprites: {placed:?}", placed.len());
}

/// The walk through field 509, whose six emitter actors play three sounds
/// and whose original keeps the three nearest the character sounding
/// (`field_sound_keep_nearest_emitters`): walking among them (the nearest
/// three change about ten times), the camera turned by 45 and 90 degrees
/// with L1 and R1, turns in place, the menu opened and closed, a jump. A
/// session log: `enter_field` at frame 0, then pad 1 by frame, ending at
/// `frames` with the RAM/scratchpad hash `memory_hash` (in Stereo).
const WALK: &str = "binaural-field509-walk.json";

struct Log {
    commands: Vec<Value>,
    frames: u64,
    memory_hash: String,
}

impl Log {
    fn read(name: &str) -> Log {
        let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../tests/reference-inputs").join(name);
        let value: Value = serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();
        Log {
            commands: value["commands"].as_array().unwrap().clone(),
            frames: value["frames"].as_u64().unwrap(),
            memory_hash: value["memory_hash"].as_str().unwrap().to_owned(),
        }
    }

    /// The frame its input starts at (its first pad command).
    fn start(&self) -> u64 {
        self.commands.iter().filter(|c| c[1]["cmd"] == "pad").filter_map(|c| c[0].as_u64()).min().unwrap()
    }

    /// Its commands before frame `at` (`before`), or from it.
    fn commands(&self, at: u64, before: bool) -> Value {
        Value::Array(self.commands.iter().filter(|c| (c[0].as_u64().unwrap() < at) == before).cloned().collect())
    }
}

/// The game where the walk's input starts, as one session played the log
/// there: every session under test starts from it.
fn walk_state(disc: &Path, log: &Log) -> Vec<u8> {
    let (mut session, _) = boot(disc, &BootOptions::default()).expect("boot");
    execute(&mut session, json!({"cmd": "schedule", "commands": log.commands(log.start(), true)}));
    while session.runtime.frames() < log.start() {
        assert!(session.frame().expect("a frame"));
    }
    session.snapshot_bytes().unwrap()
}

/// A session from the walk's state in sound mode `mode` (as a host sets
/// it), its sound kept for a host, the rest of the walk scheduled.
fn walker(disc: &Path, state: &[u8], log: &Log, mode: u32) -> Session<NativeModule> {
    let (mut session, _) = boot(disc, &BootOptions { audio_output: true, ..Default::default() }).expect("boot");
    session.set_sound_mode(mode).unwrap();
    session.restore_bytes(state).unwrap();
    assert_eq!(session.runtime.frames(), log.start());
    execute(&mut session, json!({"cmd": "schedule", "commands": log.commands(log.start(), false)}));
    session
}

/// One frame of the walk and its sound (the ring emptied).
fn step(session: &mut Session<NativeModule>) {
    assert!(session.frame().expect("a frame"), "a native disc never blocks");
    let spu = &mut session.runtime.services().spu;
    let mut sound = vec![0; spu.audio_queued()];
    spu.take_audio(&mut sound);
}

/// The sound driver's records as src/decomp/src/resident/sound.c keeps them
/// (offsets from src/decomp/include/resident/sound.h), read here
/// independently of the presenter: an oracle for what the driver resolved.
mod driver {
    use xem_core::memory::GameMemory;

    /// `sound_driver_flags` (u16): bits 0x700 hold the mode (0 Mono, 0x100
    /// Stereo, 0x300 Wide); 0x100 chooses the pan law.
    pub const FLAGS: u32 = 0x8005_957C;
    /// `sound_volumes.attr.mvol`, the master pair (left, right).
    pub const MASTER: u32 = 0x8005_A3C4;
    /// `sound_reverb_depth`, the reverb output pair (left, right), and
    /// libspu's copy of it (`libspu_reverb_depth_left`, `_right`), which
    /// `SpuSetReverbModeDepth` (8004E574) keeps as it writes the registers.
    pub const REVERB_DEPTH: u32 = 0x8005_940C;
    pub const LIBSPU_REVERB_DEPTH: u32 = 0x8005_89BC;
    /// `sound_voice_owners[24]`: the SoundChannel (a channel record's
    /// `state`, +0x30) holding each hardware voice.
    const VOICE_OWNERS: u32 = 0x8006_252C;
    /// `sound_effect_channels`, its channel count, `sound_playing_seq_list`.
    const EFFECT_CHANNELS: u32 = 0x8005_95D8;
    const EFFECT_CHANNEL_COUNT: u32 = 0x8005_9478;
    const PLAYING: u32 = 0x8005_9564;
    /// SoundSeq: channel count (u8), the whole parts of its fade and pan,
    /// its channel records (0x158 bytes each).
    const SEQ_CHANNELS: u32 = 0x14;
    const SEQ_FADE: u32 = 0x72;
    const SEQ_PAN: u32 = 0x8A;
    const SEQ_CHANNEL_ARRAY: u32 = 0x94;
    const CHANNEL_SIZE: u32 = 0x158;
    /// SoundSeqChannel: flags2 (0x100: the volume and pan to be staged at
    /// the next tick), its SoundChannel `state` with the registers still to
    /// write (`flags`, 1: the volumes) and the staged volumes, pan, volume,
    /// the whole part of its level, the level and pan modulators' outputs.
    const FLAGS2: u32 = 0x02;
    pub const STATE: u32 = 0x30;
    const STATE_FLAGS: u32 = 0x36;
    pub const STAGED: u32 = 0x38;
    const PAN: u32 = 0x74;
    const VOLUME: u32 = 0x76;
    const LEVEL: u32 = 0x7A;
    const LEVEL_MOD: u32 = 0xD2;
    const PAN_MOD: u32 = 0xD4;

    fn read<const N: usize>(memory: &dyn GameMemory, address: u32) -> [u8; N] {
        let mut bytes = [0; N];
        memory.read(address, &mut bytes).unwrap();
        bytes
    }
    pub fn u32_at(memory: &dyn GameMemory, address: u32) -> u32 {
        u32::from_le_bytes(read(memory, address))
    }
    pub fn s16_at(memory: &dyn GameMemory, address: u32) -> i16 {
        i16::from_le_bytes(read(memory, address))
    }
    fn ram(pointer: u32) -> bool {
        (0x8000_0000..0x8020_0000).contains(&pointer)
    }

    /// A channel record and the sequence it belongs to.
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub struct Channel {
        pub seq: u32,
        pub record: u32,
    }

    /// Every channel of the sequences the driver plays: the effect
    /// channels', then the playing list's.
    pub fn channels(memory: &dyn GameMemory) -> Vec<Channel> {
        let mut out = Vec::new();
        let effects = u32_at(memory, EFFECT_CHANNELS);
        if ram(effects) {
            for i in 0..u32_at(memory, EFFECT_CHANNEL_COUNT).min(24) {
                out.push(Channel { seq: effects, record: effects + SEQ_CHANNEL_ARRAY + i * CHANNEL_SIZE });
            }
        }
        let mut seq = u32_at(memory, PLAYING);
        for _ in 0..16 {
            if !ram(seq) {
                break;
            }
            if seq != effects {
                for i in 0..u32::from(read::<1>(memory, seq + SEQ_CHANNELS)[0]).min(32) {
                    out.push(Channel { seq, record: seq + SEQ_CHANNEL_ARRAY + i * CHANNEL_SIZE });
                }
            }
            seq = u32_at(memory, seq);
        }
        out
    }

    /// The channel holding hardware voice `v`.
    pub fn owner(memory: &dyn GameMemory, channels: &[Channel], v: usize) -> Option<Channel> {
        let owner = u32_at(memory, VOICE_OWNERS + 4 * v as u32);
        channels.iter().copied().find(|c| c.record + STATE == owner)
    }

    /// `sound_stage_seq_voices` (8003EBF0): the channel's gain before the
    /// pan law (its volume, level and level modulation, and the sequence's
    /// fade) and its pan (its own, its modulation's and the sequence's,
    /// 0x4000 the centre).
    pub fn pre_pan(memory: &dyn GameMemory, channel: Channel) -> (i32, i32) {
        let s16 = |address| i32::from(s16_at(memory, address));
        let level = s16(channel.record + LEVEL);
        let volume = (level - ((level * s16(channel.record + LEVEL_MOD)) >> 15)).clamp(0, 0x7FFF);
        let volume = (s16(channel.record + VOLUME) * volume) >> 15;
        let pan = s16(channel.record + PAN) + s16(channel.record + PAN_MOD) + s16(channel.seq + SEQ_PAN);
        ((s16(channel.seq + SEQ_FADE) * volume) >> 16, pan.clamp(0, 0x7F00))
    }

    /// The voice registers the driver stages from them: Stereo's and Wide's
    /// pan law (two linear ramps, 0x5A00 on both sides at the centre), or
    /// Mono's centre gain on both sides.
    pub fn staged(volume: i32, pan: i32, pan_law: bool) -> [i16; 2] {
        if !pan_law {
            let both = ((volume * 0x5A00) >> 15) as i16;
            return [both, both];
        }
        let (left, right) = if pan < 0x4000 {
            (0x7F00 - ((pan * 0x2500) >> 14), (pan * 0x5A00) >> 14)
        } else {
            let mirrored = 0x8000 - pan;
            ((mirrored * 0x5A00) >> 14, 0x7F00 - ((mirrored * 0x2500) >> 14))
        };
        [((left * volume) >> 15) as i16, ((right * volume) >> 15) as i16]
    }

    /// What the channel staged last, and whether a change waits for the
    /// next tick.
    pub fn staged_at(memory: &dyn GameMemory, channel: Channel) -> ([i16; 2], bool) {
        let staged = [s16_at(memory, channel.record + STAGED), s16_at(memory, channel.record + STAGED + 2)];
        (staged, i32::from(s16_at(memory, channel.record + FLAGS2)) & 0x100 != 0)
    }

    /// Whether the staged volumes still wait to be written to the voice.
    pub fn unwritten(memory: &dyn GameMemory, channel: Channel) -> bool {
        s16_at(memory, channel.record + STATE_FLAGS) & 1 != 0
    }
}

/// A copy of game RAM, read at the addresses the game uses.
struct Ram(Vec<u8>);

impl GameMemory for Ram {
    fn read(&self, address: u32, out: &mut [u8]) -> Result<(), xem_core::memory::OutOfBounds> {
        let at = address.wrapping_sub(0x8000_0000) as usize;
        let bytes = self.0.get(at..at + out.len()).ok_or(xem_core::memory::OutOfBounds { address, len: out.len() })?;
        out.copy_from_slice(bytes);
        Ok(())
    }

    fn write(&mut self, address: u32, data: &[u8]) -> Result<(), xem_core::memory::OutOfBounds> {
        Err(xem_core::memory::OutOfBounds { address, len: data.len() })
    }
}

/// How a session under test presents the walk.
#[derive(Clone, Copy, Debug)]
enum Presentation {
    /// No Binaural: the game's own Stereo, the reference.
    Plain,
    /// These settings throughout.
    Settings(Config),
    /// Head-tracked music, the head turning from 90 degrees left to 90
    /// right and back every four seconds.
    Head,
    /// First-person axes (desktop inspection) sweeping the same way.
    View,
    /// The settings changing every 120 frames ([`TOGGLES`]).
    Toggled,
}

/// The settings a [`Presentation::Toggled`] session goes through: on, off
/// (the presentation fades out and ends), bypassed, music or effects only,
/// both immersion ends, head-tracked music without a head.
const TOGGLES: [Config; 8] = [
    Config { enabled: true, spatial_music: true, positional_effects: true, music_immersion: 70, head_tracked_music: false, bypass: false },
    Config { enabled: false, spatial_music: true, positional_effects: true, music_immersion: 70, head_tracked_music: false, bypass: false },
    Config { enabled: true, spatial_music: true, positional_effects: true, music_immersion: 70, head_tracked_music: false, bypass: true },
    Config { enabled: true, spatial_music: true, positional_effects: false, music_immersion: 70, head_tracked_music: false, bypass: false },
    Config { enabled: true, spatial_music: false, positional_effects: true, music_immersion: 0, head_tracked_music: false, bypass: false },
    Config { enabled: true, spatial_music: true, positional_effects: true, music_immersion: 100, head_tracked_music: false, bypass: false },
    Config { enabled: false, spatial_music: true, positional_effects: true, music_immersion: 100, head_tracked_music: false, bypass: false },
    Config { enabled: true, spatial_music: true, positional_effects: true, music_immersion: 0, head_tracked_music: true, bypass: false },
];

/// The sweep's yaw (radians) at walk frame `n`.
fn sweep(n: u64) -> f32 {
    std::f32::consts::FRAC_PI_2 * (n as f32 / 240.0 * std::f32::consts::TAU).sin()
}

/// What one frame of the walk left: game memory (`memory_hash`), the 24
/// voices' volume registers and current volumes, and every 60 frames a hash
/// of the SPU's whole saved state.
#[derive(Clone, Debug, PartialEq)]
struct Frame {
    memory: u64,
    registers: Vec<u16>,
    volumes: Vec<[i16; 2]>,
    chip: Option<u64>,
}

fn fnv(bytes: &[u8]) -> u64 {
    bytes.iter().fold(0xCBF2_9CE4_8422_2325, |h, b| (h ^ u64::from(*b)).wrapping_mul(0x0000_0100_0000_01B3))
}

/// The reported-gain check's tally: sources whose voices held still
/// through the frame and sounded, checked exactly; those whose voices
/// moved; those with a voice whose sample was zero (a silent stretch).
#[derive(Debug, Default)]
struct Gains {
    checked: u64,
    moving: u64,
    silent: u64,
    effects: u64,
    music: u64,
    worst_ratio: f64,
    failures: Vec<String>,
}

/// The voices at a frame's end: each one's route (`binaural` status; none
/// without a presentation), whether it sounds (its envelope above zero),
/// its last sample and its current volumes.
#[derive(Clone, Default)]
struct Voices {
    routes: Vec<Value>,
    sounding: Vec<bool>,
    outputs: Vec<i16>,
    volumes: Vec<[i16; 2]>,
}

/// Each source the presentation places reports, as its gain, the gain the
/// driver resolved before panning: for each of its sounding voices the
/// driver's staged registers, as the SPU's current volumes, unpanned
/// (`xem_binaural::pan::unpan`); the largest of them. The gain is the last
/// mixed block's (of the voices whose samples were not zero there), inside
/// the frame, so it is checked exactly where the source's voices held still
/// through the frame (the same voices, routes, sounding and volumes at its
/// start and end, and the registers holding what the driver staged from
/// its pre-pan gain) and sounded (the source was heard in its last block,
/// no voice's sample zero at either end: a sample's silent loop gives no
/// gain), and against the driver's own pre-pan volume within the pan law's
/// rounding.
fn check_gains(session: &mut Session<NativeModule>, previous: &Voices, frame: u64, gains: &mut Gains) -> Voices {
    let status = execute(session, json!({"cmd": "binaural"}));
    let chip = session.runtime.services().spu.chip();
    let voice: Vec<xem_spu::VoiceInfo> = (0..xem_spu::VOICES).map(|v| chip.voice(v)).collect();
    let mut now = Voices {
        routes: vec![Value::Null; xem_spu::VOICES],
        sounding: voice.iter().map(|v| v.phase != xem_spu::Phase::Off && v.envelope > 0).collect(),
        outputs: voice.iter().map(|v| v.output).collect(),
        volumes: voice.iter().map(|v| v.volume).collect(),
    };
    for entry in status["voices"].as_array().into_iter().flatten() {
        now.routes[entry["voice"].as_u64().unwrap() as usize] = entry["route"].clone();
    }
    if status["presenting"] != true || status["config"]["bypass"] == true || previous.routes.is_empty() {
        return now;
    }
    let memory = session.runtime.memory();
    let channels = driver::channels(memory);
    for source in status["sources"].as_array().unwrap() {
        let id = &source["id"];
        let voices: Vec<usize> = (0..xem_spu::VOICES).filter(|&v| &now.routes[v] == id).collect();
        let before: Vec<usize> = (0..xem_spu::VOICES).filter(|&v| &previous.routes[v] == id).collect();
        let held = voices == before && voices.iter().all(|&v| now.sounding[v] == previous.sounding[v] && now.volumes[v] == previous.volumes[v]);
        let sounding: Vec<usize> = voices.iter().copied().filter(|&v| now.sounding[v]).collect();
        if sounding.is_empty() {
            continue;
        }
        if held && (source["level_db"].is_null() || sounding.iter().any(|&v| now.outputs[v] == 0 || previous.outputs[v] == 0)) {
            gains.silent += 1;
            continue;
        }
        let mut expected: Option<f32> = None;
        let mut driver_volume = 0;
        let mut still = held;
        for &v in sounding.iter().take_while(|_| still) {
            let volume = now.volumes[v];
            let Some(owner) = driver::owner(memory, &channels, v) else {
                still = false;
                break;
            };
            let (pre, pan) = driver::pre_pan(memory, owner);
            let (staged, pending) = driver::staged_at(memory, owner);
            // The registers hold what the driver staged from its pre-pan
            // gain now.
            if pending || staged != driver::staged(pre, pan, true) || volume != staged.map(|s| s << 1) {
                still = false;
                break;
            }
            let gain = xem_binaural::pan::unpan(volume).expect("the pan law's volumes").gain;
            if expected.is_none_or(|g| gain > g) {
                expected = Some(gain);
                driver_volume = pre;
            }
        }
        if !still {
            gains.moving += 1;
            continue;
        }
        let voices = sounding;
        let expected = expected.unwrap();
        let reported = source["gain"].as_f64().unwrap() as f32;
        gains.checked += 1;
        if source["kind"] == "effect" {
            gains.effects += 1;
        } else {
            gains.music += 1;
        }
        if reported != expected {
            gains.failures.push(format!("frame {frame}: source {source} reports {reported}, its voices {voices:?} unpan to {expected}"));
        }
        if driver_volume >= 0x100 {
            let ratio = f64::from(reported) * 16384.0 / f64::from(driver_volume);
            if (ratio - 1.0).abs() > gains.worst_ratio.abs() {
                gains.worst_ratio = ratio - 1.0;
            }
        }
    }
    now
}

/// What a session's walk left: its frames, its gains and its end.
struct Walked {
    presentation: Presentation,
    frames: Vec<Frame>,
    gains: Gains,
    memory_hash: String,
    /// Plain only: what the walk did (emitter slot changes, camera angles,
    /// frames in the menu).
    coverage: Option<(u32, usize, u32)>,
}

fn walk(disc: &Path, state: &[u8], log: &Log, presentation: Presentation) -> Walked {
    let mut session = walker(disc, state, log, 1);
    let on = Config { enabled: true, ..Config::default() };
    match presentation {
        Presentation::Plain => {}
        Presentation::Settings(config) => session.set_binaural(config),
        Presentation::Head => session.set_binaural(Config { head_tracked_music: true, ..on }),
        Presentation::View | Presentation::Toggled => session.set_binaural(on),
    }
    let mut frames = Vec::new();
    let mut gains = Gains::default();
    let mut previous = Voices::default();
    let (mut slots, mut slot_changes, mut angles, mut menu) = (Vec::new(), 0, std::collections::BTreeSet::new(), 0);
    for n in 0..log.frames - log.start() {
        let yaw = sweep(n);
        match presentation {
            Presentation::Head => {
                let half = yaw / 2.0;
                session.set_head(Some(xem_core::binaural::head_axes([0.0, -half.sin(), 0.0, half.cos()])));
            }
            Presentation::View => {
                let (s, c) = yaw.sin_cos();
                session.set_listener_view(Some([[c, 0.0, -s], [0.0, 1.0, 0.0], [s, 0.0, c]]));
            }
            Presentation::Toggled if n % 120 == 0 && n > 0 => session.set_binaural(TOGGLES[(n / 120) as usize % TOGGLES.len()]),
            _ => {}
        }
        step(&mut session);
        let at = log.start() + n + 1;
        let chip = session.runtime.services().spu.chip();
        let registers: Vec<u16> = (0..xem_spu::VOICES)
            .flat_map(|v| [xem_spu::reg::VOICE_VOLUME_LEFT, xem_spu::reg::VOICE_VOLUME_RIGHT].map(|r| chip.read16(xem_spu::reg::voice(v) + r)))
            .collect();
        let volumes: Vec<[i16; 2]> = (0..xem_spu::VOICES).map(|v| chip.voice(v).volume).collect();
        let chip_state = (at % 60 == 0).then(|| fnv(&chip.save()));
        if !matches!(presentation, Presentation::Plain) {
            previous = check_gains(&mut session, &previous, at, &mut gains);
        } else {
            let mut now = [0u8; 18];
            session.runtime.memory().read(0x800A_FE88, &mut now).unwrap();
            slot_changes += u32::from(!slots.is_empty() && slots != now);
            slots = now.to_vec();
            if let Some(angle) = execute(&mut session, json!({"cmd": "field"}))["camera_angle"].as_i64() {
                angles.insert(angle.rem_euclid(4096));
            }
            menu += u32::from(execute(&mut session, json!({"cmd": "context"}))["kind"] == "menu");
        }
        frames.push(Frame { memory: session.memory_hash().unwrap(), registers, volumes, chip: chip_state });
    }
    let memory_hash = format!("{:016x}", session.memory_hash().unwrap());
    let coverage = matches!(presentation, Presentation::Plain).then_some((slot_changes, angles.len(), menu));
    Walked { presentation, frames, gains, memory_hash, coverage }
}

#[test]
fn along_a_walk_every_presentation_setting_leaves_the_games_gains_and_emitters_as_they_are() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let log = Log::read(WALK);
    let state = walk_state(&disc, &log);
    let on = Config { enabled: true, ..Config::default() };
    let presentations = [
        Presentation::Plain,
        Presentation::Settings(on),
        Presentation::Settings(Config { bypass: true, ..on }),
        Presentation::Settings(Config { positional_effects: false, ..on }),
        Presentation::Settings(Config { spatial_music: false, ..on }),
        Presentation::Settings(Config { music_immersion: 0, ..on }),
        Presentation::Settings(Config { music_immersion: 100, ..on }),
        Presentation::Head,
        Presentation::View,
        Presentation::Toggled,
    ];
    // Each session on its own thread; the frames are compared afterwards,
    // frame by frame (the sessions are deterministic and share nothing).
    let started = std::time::Instant::now();
    let walked: Vec<Walked> = std::thread::scope(|scope| {
        let runs: Vec<_> = presentations.iter().map(|p| scope.spawn(|| walk(&disc, &state, &log, *p))).collect();
        runs.into_iter().map(|run| run.join().expect("a walk")).collect()
    });
    eprintln!("{} sessions, {} frames each, in {:.1} s", walked.len(), log.frames - log.start(), started.elapsed().as_secs_f64());
    let plain = &walked[0];
    assert_eq!(plain.memory_hash, log.memory_hash, "the walk ends where its log was recorded");
    let (slot_changes, angles, menu) = plain.coverage.unwrap();
    eprintln!("walk: the nearest three emitters changed {slot_changes} times, the camera took {angles} angles, {menu} frames in the menu");
    assert!(slot_changes >= 5 && angles >= 3 && menu >= 100, "the walk walks among the emitters, turns the camera and opens the menu");
    for run in &walked[1..] {
        for (n, (frame, reference)) in run.frames.iter().zip(&plain.frames).enumerate() {
            assert_eq!(frame, reference, "{:?}, frame {}: the game, the voices' volumes and the SPU as in plain Stereo", run.presentation, log.start() + n as u64 + 1);
        }
        assert_eq!(run.frames.len(), plain.frames.len());
        let gains = &run.gains;
        eprintln!("{:?}: {} source frames checked ({} effects, {} music), {} with moving voices, {} with a silent voice; worst gain against the driver's volume {:+.4}",
            run.presentation, gains.checked, gains.effects, gains.music, gains.moving, gains.silent, gains.worst_ratio);
        assert!(gains.failures.is_empty(), "{:?}: {} sources report another gain than their voices' driver gain, first: {}",
            run.presentation, gains.failures.len(), gains.failures.first().unwrap());
    }
    let placed = &walked[1].gains;
    assert!(placed.effects >= 500 && placed.music >= 500, "the emitters and the music were placed and checked: {placed:?}");
    // The registers truncate gain * volume >> 15, the nearer side's gain at
    // least 0x5A00, so for a volume of 0x100 or more the nearer register
    // (180 or more) is off by under 0.56%, and the pan read from the
    // farther one moves the unpanned gain by under 0.33%.
    assert!(placed.worst_ratio.abs() < 0.01, "the reported gain is the driver's pre-pan volume within its law's rounding: {:+.4}", placed.worst_ratio);
}

#[test]
fn along_a_walk_mono_and_wide_differ_from_stereo_only_where_the_sound_mode_findings_say() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let log = Log::read(WALK);
    let state = walk_state(&disc, &log);
    // In lockstep: Stereo, Mono, Wide.
    let mut sessions = [1, 0, 2].map(|mode| walker(&disc, &state, &log, mode));
    let mut differing = [std::collections::BTreeMap::<u32, u64>::new(), std::collections::BTreeMap::new()];
    let mut spu_differing = [std::collections::BTreeMap::<&str, u64>::new(), std::collections::BTreeMap::new()];
    let (mut staged_checked, mut registers_checked) = (0u64, 0u64);
    for n in 0..log.frames - log.start() {
        for session in sessions.iter_mut() {
            step(session);
        }
        let at = log.start() + n + 1;
        let ram = sessions.each_mut().map(|session| {
            let mut bytes = vec![0u8; 0x20_0000];
            session.runtime.memory().read(0x8000_0000, &mut bytes).unwrap();
            Ram(bytes)
        });
        let scratch = sessions.each_mut().map(|session| {
            let mut bytes = [0u8; 0x400];
            session.runtime.memory().read(0x1F80_0000, &mut bytes).unwrap();
            bytes
        });
        let channels = driver::channels(&ram[0]);
        // What the findings (analysis/formats/sound-modes.md) name: the mode
        // bits, Mono's centre-law staged volumes, Wide's negated master
        // right and reverb-depth left (the driver's pair and libspu's copy).
        let staged: std::collections::BTreeSet<u32> =
            channels.iter().flat_map(|c| c.record + driver::STAGED..c.record + driver::STAGED + 4).collect();
        let flags = driver::FLAGS..driver::FLAGS + 2;
        let named = |mode: usize, address: u32| match mode {
            0 => flags.contains(&address) || staged.contains(&address),
            _ => flags.contains(&address) || (driver::MASTER + 2..driver::MASTER + 4).contains(&address)
                || (driver::REVERB_DEPTH..driver::REVERB_DEPTH + 2).contains(&address)
                || (driver::LIBSPU_REVERB_DEPTH..driver::LIBSPU_REVERB_DEPTH + 2).contains(&address),
        };
        for (k, other) in [1usize, 2].into_iter().enumerate() {
            assert_eq!(scratch[other], scratch[0], "frame {at}: the scratchpad is the same in every mode");
            for (offset, (a, b)) in ram[0].0.iter().zip(&ram[other].0).enumerate() {
                if a != b {
                    let address = 0x8000_0000 + offset as u32;
                    *differing[k].entry(address).or_default() += 1;
                    assert!(named(k, address), "frame {at}: {} differs from Stereo at {address:#010x} ({a:#04x} against {b:#04x}), which the findings do not name",
                        ["Mono", "Wide"][k]);
                }
            }
        }
        // The values there are what the findings say they are.
        let pair = |memory: &Ram, address: u32| [driver::s16_at(memory, address), driver::s16_at(memory, address + 2)];
        let mode_bits = ram.each_ref().map(|memory| driver::s16_at(memory, driver::FLAGS) as u16 & 0x700);
        assert_eq!(mode_bits, [0x100, 0, 0x300], "frame {at}: the modes' bits");
        let (master, depth) = (pair(&ram[0], driver::MASTER), pair(&ram[0], driver::REVERB_DEPTH));
        assert_eq!(pair(&ram[2], driver::MASTER), [master[0], master[1].wrapping_neg()], "frame {at}: Wide negates the master volume's right side");
        assert_eq!(pair(&ram[2], driver::REVERB_DEPTH), [depth[0].wrapping_neg(), depth[1]], "frame {at}: Wide negates the reverb depth's left side");
        for memory in &ram {
            assert_eq!(pair(memory, driver::LIBSPU_REVERB_DEPTH), pair(memory, driver::REVERB_DEPTH), "frame {at}: libspu keeps the depth the driver set");
        }
        // Every channel's gain before its pan law is the same in every mode,
        // and each mode stages from it with its own law.
        for (i, memory) in ram.iter().enumerate() {
            assert_eq!(driver::channels(memory), channels, "frame {at}: the same channels in every mode");
            for channel in &channels {
                let (pre, pan) = driver::pre_pan(memory, *channel);
                assert_eq!((pre, pan), driver::pre_pan(&ram[0], *channel), "frame {at}: {channel:?}'s gain before its pan law in mode {i}");
                let (registers, pending) = driver::staged_at(memory, *channel);
                if !pending && driver::s16_at(memory, channel.record) != 0 {
                    assert_eq!(registers, driver::staged(pre, pan, i != 1), "frame {at}: {channel:?} as staged in mode {i}");
                    staged_checked += 1;
                }
            }
        }
        // The SPU: Mono's voice volumes (the centre law: both sides alike)
        // and the reverb they feed, Wide's master and reverb volumes as the
        // driver set them (libspu writes the master volume's 15 bits).
        let chips = sessions.each_mut().map(|s| &*s.runtime.services().spu.chip());
        for (k, other) in [1usize, 2].into_iter().enumerate() {
            for offset in (0..0x200).step_by(2) {
                if chips[0].read16(offset) != chips[other].read16(offset) {
                    let name = match offset {
                        o if o < 0x180 && o & 0xF <= 2 => "voice volume",
                        xem_spu::reg::MAIN_VOLUME_RIGHT => "master volume right",
                        xem_spu::reg::CURRENT_MAIN_VOLUME_RIGHT => "current master volume right",
                        xem_spu::reg::REVERB_VOLUME_LEFT => "reverb volume left",
                        _ => panic!("frame {at}: {} differs from Stereo in SPU register {offset:#05x}, which the findings do not name", ["Mono", "Wide"][k]),
                    };
                    *spu_differing[k].entry(name).or_default() += 1;
                }
            }
            for v in 0..xem_spu::VOICES {
                if chips[0].voice(v).volume != chips[other].voice(v).volume {
                    *spu_differing[k].entry("voice current volume").or_default() += 1;
                }
            }
            if at % 60 == 0 && chips[0].ram() != chips[other].ram() {
                let base = usize::from(chips[0].read16(xem_spu::reg::REVERB_BASE)) * 8;
                let below = chips[0].ram()[..base].iter().zip(&chips[other].ram()[..base]).position(|(a, b)| a != b);
                assert_eq!(below, None, "frame {at}: {}'s SPU RAM differs outside the reverb work area", ["Mono", "Wide"][k]);
                *spu_differing[k].entry("reverb work area").or_default() += 1;
            }
        }
        // Each voice a channel holds has the volumes the channel staged (by
        // its mode's law, above), once they are written.
        for (i, chip) in chips.iter().enumerate() {
            for v in 0..xem_spu::VOICES {
                let Some(owner) = driver::owner(&ram[i], &channels, v) else { continue };
                let (staged, pending) = driver::staged_at(&ram[i], owner);
                if pending || driver::unwritten(&ram[i], owner) {
                    continue;
                }
                let registers = [xem_spu::reg::VOICE_VOLUME_LEFT, xem_spu::reg::VOICE_VOLUME_RIGHT].map(|r| chip.read16(xem_spu::reg::voice(v) + r) as i16);
                assert_eq!(registers, staged, "frame {at}: voice {v} has what {owner:?} staged in mode {i}");
                registers_checked += 1;
            }
        }
        let wide = pair(&ram[2], driver::MASTER);
        assert_eq!(chips[2].read16(xem_spu::reg::MAIN_VOLUME_RIGHT), wide[1] as u16 & 0x7FFF, "frame {at}: Wide's master right as libspu writes it");
        assert_eq!(chips[2].read16(xem_spu::reg::REVERB_VOLUME_LEFT), pair(&ram[2], driver::REVERB_DEPTH)[0] as u16, "frame {at}: Wide's reverb left");
    }
    for k in 0..2 {
        let bytes: Vec<String> = differing[k].iter().map(|(a, n)| format!("{a:#010x}x{n}")).collect();
        eprintln!("{}: RAM differed from Stereo at {} bytes: {}", ["Mono", "Wide"][k], differing[k].len(), bytes.join(" "));
        eprintln!("{}: the SPU differed in {:?}", ["Mono", "Wide"][k], spu_differing[k]);
    }
    let names = |k: usize| spu_differing[k].keys().copied().collect::<Vec<_>>();
    assert_eq!(names(0), ["reverb work area", "voice current volume", "voice volume"], "Mono differs in its voices' volumes and what they feed the reverb");
    assert_eq!(names(1), ["current master volume right", "master volume right", "reverb volume left"], "Wide differs in its two negated volumes");
    eprintln!("staged volumes checked against the driver's law: {staged_checked}; voice registers against them: {registers_checked}");
    assert!(staged_checked > 10_000 && registers_checked > 10_000);
}
