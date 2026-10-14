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
//! Stereo only in what the sound-mode findings name. On the same walk the
//! field reader, apart from the presenter, puts each emitter where it is
//! heard (front and back, up and down, the original's side; the camera's
//! turns turn it, the character's do not); each emitter start is one
//! source following its own actor, its tail keeping its last direction;
//! the music keeps its places through camera turns, walking and looking
//! around, and turns against a tracked head; switching sources and
//! settings adds no clicks; a tap capture mixes again what was presented
//! and shows each placed source mixed once; drawing zoomed out changes
//! nothing heard, and with characters in view the game's own gains stay.
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
#[derive(Clone, Copy, Debug, PartialEq)]
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
    /// Binaural on while a game view draws every frame zoomed out four
    /// times with characters in view off, so the game's actor view stays
    /// the original's.
    Zoomed,
    /// Binaural on with characters in view: the game keeps the field's
    /// actors moving in that zoomed view (`Session::set_actor_view`), a
    /// change to the game itself.
    InView,
}

/// The Binaural mode's defaults, on.
const ON: Config = Config { enabled: true, spatial_music: true, positional_effects: true, music_immersion: 70, head_tracked_music: false, bypass: false };

/// The sessions the walk's tests read, all played once ([`walks`]).
const PRESENTATIONS: [Presentation; 12] = [
    Presentation::Plain,
    Presentation::Settings(ON),
    Presentation::Settings(Config { bypass: true, ..ON }),
    Presentation::Settings(Config { positional_effects: false, ..ON }),
    Presentation::Settings(Config { spatial_music: false, ..ON }),
    Presentation::Settings(Config { music_immersion: 0, ..ON }),
    Presentation::Settings(Config { music_immersion: 100, ..ON }),
    Presentation::Head,
    Presentation::View,
    Presentation::Toggled,
    Presentation::Zoomed,
    Presentation::InView,
];
const PLAIN: usize = 0;
const PLACED: usize = 1;
const BYPASS: usize = 2;
const HEAD: usize = 7;
const VIEW: usize = 8;
const TOGGLED: usize = 9;
const ZOOMED: usize = 10;
const IN_VIEW: usize = 11;

/// The walk's frames (counted from its input's start) the placed session
/// keeps a tap capture of: the camera's quarter turn, turns in place,
/// walking, and the menu opened and closed with its interface sounds.
const TAP: std::ops::Range<u64> = 330..830;

/// The zoomed view: four times pulled back, wider framing, characters in
/// view off, on a 16:9 desktop output.
fn zoomed_graphics() -> xem_settings::Graphics {
    xem_settings::Graphics { widescreen: true, zoom_out: 400, actors_in_view: false, ..Default::default() }
}
const ZOOMED_OUTPUT: (u32, u32) = (640, 360);

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

/// The first-person axes a [`Presentation::View`] session sets before walk
/// frame `n`: looking `sweep(n)` to the right.
fn view_axes(n: u64) -> [[f32; 3]; 3] {
    let (s, c) = sweep(n).sin_cos();
    [[c, 0.0, -s], [0.0, 1.0, 0.0], [s, 0.0, c]]
}

/// The head a [`Presentation::Head`] session sets before walk frame `n`:
/// turned `sweep(n)` to the right (a rotation about +Y by minus that).
fn head_of(n: u64) -> [[f32; 3]; 3] {
    let half = sweep(n) / 2.0;
    xem_core::binaural::head_axes([0.0, -half.sin(), 0.0, half.cos()])
}

fn apply(rows: &[[f32; 3]; 3], v: [f32; 3]) -> [f32; 3] {
    rows.map(|row| row[0] * v[0] + row[1] * v[1] + row[2] * v[2])
}

fn unit(v: [f32; 3]) -> [f32; 3] {
    let length = v.iter().map(|c| c * c).sum::<f32>().sqrt();
    v.map(|c| c / length)
}

/// The direction (right, down, forward) of an azimuth clockwise from ahead
/// and an elevation up, degrees, as the status reports them.
fn direction_of(azimuth: f64, elevation: f64) -> [f32; 3] {
    let (a, e) = (azimuth.to_radians(), elevation.to_radians());
    [(a.sin() * e.cos()) as f32, -e.sin() as f32, (a.cos() * e.cos()) as f32]
}

/// The angle between two directions, degrees.
fn between(a: [f32; 3], b: [f32; 3]) -> f32 {
    let (a, b) = (unit(a), unit(b));
    let cos = f64::from(a[0] * b[0] + a[1] * b[1] + a[2] * b[2]).clamp(-1.0, 1.0);
    // acos loses precision near 1: the chord's half-angle instead.
    let chord = a.iter().zip(&b).map(|(x, y)| f64::from(x - y).powi(2)).sum::<f64>().sqrt();
    (if cos > 0.9 { 2.0 * (chord / 2.0).asin() } else { cos.acos() }).to_degrees() as f32
}

/// A source's target in the status as a direction (none: it keeps its own).
fn target(source: &Value) -> Option<[f32; 3]> {
    let target = source.get("target").filter(|t| !t.is_null())?;
    Some(direction_of(target["azimuth"].as_f64()?, target["elevation"].as_f64()?))
}

/// The field after a frame as `xem_core::field::Field` reads it, apart from
/// the presenter: the camera (`scaled_world`'s rows), each actor's position
/// and facing (`FieldActor.heading_goal`'s direction, 0x1000 a turn; bit 15
/// only marks the turn done), the controlled actor, and the emitter slots
/// (`field_sound_emitter_slots` at 0x800AFE88: the actor each follows and
/// its sound, 0xFFFF free).
#[derive(Clone, Debug)]
struct Seen {
    camera: [[f32; 3]; 3],
    actors: Vec<Option<[f32; 3]>>,
    facings: Vec<Option<i16>>,
    player: Option<usize>,
    slots: [(u16, u16); 3],
}

impl Seen {
    fn read(memory: &dyn GameMemory) -> Option<Seen> {
        let field = xem_core::field::Field::read(memory)?;
        let mut bytes = [0u8; 18];
        memory.read(0x800A_FE88, &mut bytes).ok()?;
        let half = |at: usize| u16::from_le_bytes([bytes[at], bytes[at + 1]]);
        Some(Seen {
            camera: field.camera.m,
            actors: field.actors.iter().map(|a| a.map(|a| [a.position.x, a.position.y, a.position.z])).collect(),
            facings: field.actors.iter().map(|a| a.map(|a| a.heading_goal & 0xFFF)).collect(),
            player: field.player,
            slots: [0, 1, 2].map(|slot| (half(slot * 6), half(slot * 6 + 2))),
        })
    }

    fn position(&self, actor: usize) -> Option<[f32; 3]> {
        *self.actors.get(actor)?
    }

    /// Actor `actor` as the camera sees it from the controlled actor: their
    /// difference turned into view space (right, down, forward), unscaled.
    fn view_of(&self, actor: usize) -> Option<[f32; 3]> {
        let (at, from) = (self.position(actor)?, self.position(self.player?)?);
        Some(apply(&self.camera, [0, 1, 2].map(|i| at[i] - from[i])))
    }

    /// The same through the camera's rows each made unit length (its
    /// orientation alone): nothing of any actor's facing in it.
    fn oriented_view_of(&self, actor: usize) -> Option<[f32; 3]> {
        let (at, from) = (self.position(actor)?, self.position(self.player?)?);
        Some(apply(&self.camera.map(unit), [0, 1, 2].map(|i| at[i] - from[i])))
    }

    /// The slot an effect on effect channels `pair` and the next plays from
    /// (`field_sound_start_emitter` plays slot i on channels 2i ^ 8).
    fn slot_of(pair: u64) -> usize {
        ((pair as usize) ^ 8) / 2
    }
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
fn check_gains(session: &mut Session<NativeModule>, status: &Value, previous: &Voices, frame: u64, gains: &mut Gains) -> Voices {
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
    /// Each frame's `binaural` status (placed, head, view, zoomed and in
    /// view: the sessions whose directions are checked).
    statuses: Vec<Value>,
    /// The field as the reader saw it after each frame (plain, placed, head,
    /// view and in view).
    seen: Vec<Option<Seen>>,
    /// The output as presented, and where each frame's ended in it.
    audio: Vec<i16>,
    ends: Vec<usize>,
    /// The placed session's tap capture over [`TAP`]: its journal, and the
    /// output captured alongside it (`audio_capture`).
    tap: Option<(xem_binaural::Journal, Vec<i16>)>,
    /// The view session's `audio_capture` with `tap: true` over the first
    /// two seconds of [`TAP`], as the command replies.
    tap_reply: Option<Value>,
}

/// A view drawing a session's frames as a desktop host would ([`Presentation::Zoomed`]).
struct Drawn {
    gpu: xem_render::Gpu,
    view: xem_render::ps1::GameView,
}

impl Drawn {
    fn new() -> Drawn {
        let gpu = pollster::block_on(xem_render::Gpu::headless()).expect("a wgpu adapter");
        let mut view = xem_render::ps1::GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
        view.configure(Self::config(&view));
        Drawn { gpu, view }
    }

    fn config(view: &xem_render::ps1::GameView) -> xem_render::ps1::ViewConfig {
        xem_render::ps1::ViewConfig::resolve(&zoomed_graphics(), xem_render::ps1::HostClass::Desktop, ZOOMED_OUTPUT, view.max_scale())
    }

    /// The frame the session just ran, drawn (the draw trace on and the
    /// port recording what the wider picture reveals, as the host does).
    fn draw(&mut self, session: &mut Session<NativeModule>, n: u64) {
        let config = Self::config(&self.view);
        self.view.configure(config);
        let device = session.runtime.services().gpu.device_mut();
        if self.view.take_restart() || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(self.view.reveals());
        self.view.push(device.take_trace());
        let display = device.display();
        let scene = session.scene();
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        let _ = self.view.present(&mut encoder, &display, &scene, n as f64 / 60.0);
        self.gpu.queue.submit([encoder.finish()]);
    }
}

fn walk(disc: &Path, state: &[u8], log: &Log, presentation: Presentation) -> Walked {
    let mut session = walker(disc, state, log, 1);
    let mut drawn = None;
    match presentation {
        Presentation::Plain => {}
        Presentation::Settings(config) => session.set_binaural(config),
        Presentation::Head => session.set_binaural(Config { head_tracked_music: true, ..ON }),
        Presentation::View | Presentation::Toggled => session.set_binaural(ON),
        Presentation::Zoomed => {
            session.set_binaural(ON);
            drawn = Some(Drawn::new());
        }
        Presentation::InView => {
            session.set_binaural(ON);
            // What the zoomed view asks of the game with characters in view
            // (the hosts' `want_actor_view(renderer.actor_view())`).
            let view = Drawn::new().view.actor_view();
            assert!(view & 0xFFFF == 400 && view >> 16 > 0, "pulled back four times with wider sides: {view:#x}");
            session.set_actor_view(view).unwrap();
        }
    }
    let placed = presentation == PRESENTATIONS[PLACED];
    let observed = placed || matches!(presentation, Presentation::Head | Presentation::View | Presentation::Zoomed | Presentation::InView);
    let reads = observed && presentation != Presentation::Zoomed || presentation == Presentation::Plain;
    let mut frames = Vec::new();
    let mut gains = Gains::default();
    let mut previous = Voices::default();
    let (mut statuses, mut seen, mut audio, mut ends, mut tap, mut tap_reply) = (Vec::new(), Vec::new(), Vec::new(), Vec::new(), None, None);
    let (mut slots, mut slot_changes, mut angles, mut menu) = (Vec::new(), 0, std::collections::BTreeSet::new(), 0);
    for n in 0..log.frames - log.start() {
        match presentation {
            Presentation::Head => session.set_head(Some(head_of(n))),
            Presentation::View => session.set_listener_view(Some(view_axes(n))),
            Presentation::Toggled if n % 120 == 0 && n > 0 => session.set_binaural(TOGGLES[(n / 120) as usize % TOGGLES.len()]),
            _ => {}
        }
        if (placed || presentation == Presentation::View) && n == TAP.start {
            let reply = execute(&mut session, json!({"cmd": "audio_capture", "action": "start", "tap": true}));
            assert_eq!(reply, json!({"capturing": true, "tap": true}));
        }
        if presentation == Presentation::View && n == TAP.start + 120 {
            tap_reply = Some(execute(&mut session, json!({"cmd": "audio_capture", "action": "stop"})));
        }
        assert!(session.frame().expect("a frame"), "a native disc never blocks");
        if let Some(drawn) = drawn.as_mut() {
            drawn.draw(&mut session, n);
        }
        let spu = &mut session.runtime.services().spu;
        let start = audio.len();
        audio.resize(start + spu.audio_queued(), 0);
        spu.take_audio(&mut audio[start..]);
        ends.push(audio.len());
        if placed && n + 1 == TAP.end {
            let journal = spu.take_tap_capture().expect("a tap capture");
            let output = spu.take_capture().expect("the output alongside");
            tap = Some((journal, output));
        }
        let at = log.start() + n + 1;
        let chip = session.runtime.services().spu.chip();
        let registers: Vec<u16> = (0..xem_spu::VOICES)
            .flat_map(|v| [xem_spu::reg::VOICE_VOLUME_LEFT, xem_spu::reg::VOICE_VOLUME_RIGHT].map(|r| chip.read16(xem_spu::reg::voice(v) + r)))
            .collect();
        let volumes: Vec<[i16; 2]> = (0..xem_spu::VOICES).map(|v| chip.voice(v).volume).collect();
        let chip_state = (at % 60 == 0).then(|| fnv(&chip.save()));
        if !matches!(presentation, Presentation::Plain) {
            let status = execute(&mut session, json!({"cmd": "binaural"}));
            previous = check_gains(&mut session, &status, &previous, at, &mut gains);
            if observed {
                statuses.push(status);
            }
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
        if reads {
            seen.push(Seen::read(session.runtime.memory()));
        }
        frames.push(Frame { memory: session.memory_hash().unwrap(), registers, volumes, chip: chip_state });
    }
    let memory_hash = format!("{:016x}", session.memory_hash().unwrap());
    let coverage = matches!(presentation, Presentation::Plain).then_some((slot_changes, angles.len(), menu));
    Walked { presentation, frames, gains, memory_hash, coverage, statuses, seen, audio, ends, tap, tap_reply }
}

/// Every session of the walk ([`PRESENTATIONS`]), played once for all the
/// tests that read them.
struct Walks {
    log: Log,
    walked: Vec<Walked>,
}

static WALKS: std::sync::OnceLock<Result<Walks, String>> = std::sync::OnceLock::new();

/// The walk's sessions, each on its own thread from one state (the
/// sessions are deterministic and share nothing); none without the disc.
fn walks() -> Option<&'static Walks> {
    let disc = disc()?;
    let walks = WALKS.get_or_init(|| {
        let played = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            let log = Log::read(WALK);
            let state = walk_state(&disc, &log);
            let started = std::time::Instant::now();
            let walked: Vec<Walked> = std::thread::scope(|scope| {
                let runs: Vec<_> = PRESENTATIONS.iter().map(|p| scope.spawn(|| walk(&disc, &state, &log, *p))).collect();
                runs.into_iter().map(|run| run.join().unwrap_or_else(|panic| std::panic::resume_unwind(panic))).collect()
            });
            eprintln!("{} sessions, {} frames each, in {:.1} s", walked.len(), log.frames - log.start(), started.elapsed().as_secs_f64());
            Walks { log, walked }
        }));
        played.map_err(|panic| panic.downcast_ref::<String>().cloned().or_else(|| panic.downcast_ref::<&str>().map(|s| (*s).to_owned())).unwrap_or_default())
    });
    match walks {
        Ok(walks) => Some(walks),
        Err(error) => panic!("the walk's sessions failed: {error}"),
    }
}

#[test]
fn along_a_walk_every_presentation_setting_leaves_the_games_gains_and_emitters_as_they_are() {
    let Some(Walks { log, walked }) = walks() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let plain = &walked[PLAIN];
    assert_eq!(plain.memory_hash, log.memory_hash, "the walk ends where its log was recorded");
    let (slot_changes, angles, menu) = plain.coverage.unwrap();
    eprintln!("walk: the nearest three emitters changed {slot_changes} times, the camera took {angles} angles, {menu} frames in the menu");
    assert!(slot_changes >= 5 && angles >= 3 && menu >= 100, "the walk walks among the emitters, turns the camera and opens the menu");
    // Characters in view change the game (its own eligibility changes):
    // that session is checked on its own.
    for run in walked[1..].iter().filter(|run| run.presentation != Presentation::InView) {
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
    let placed = &walked[PLACED].gains;
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


/// The walk frame `n`'s number in the log.
fn frame_at(log: &Log, n: usize) -> u64 {
    log.start() + n as u64 + 1
}

/// The sources of a status that are effects placed at field emitters.
fn field_emitters(status: &Value) -> impl Iterator<Item = &Value> {
    status["sources"].as_array().into_iter().flatten().filter(|s| s["emitter"] == "field emitter")
}

/// What the direction checks of one session saw.
#[derive(Debug, Default)]
struct Directions {
    checked: u64,
    worst: f32,
    front: u64,
    back: u64,
    up: u64,
    down: u64,
    sides: u64,
    orbit_frames: u64,
    orbit_turn: f32,
    turns: u64,
    turn_worst: f32,
}

#[test]
fn along_a_walk_effects_are_heard_where_the_field_reader_puts_their_emitters() {
    let Some(Walks { log, walked }) = walks() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // The oracle: the controlled actor and the emitter's actor as the field
    // reader has them, their difference through the camera's matrix as the
    // game draws with it (`scaled_world`, whose rows' lengths differ by up
    // to 0.8%: the presenter takes each row's direction, hence the 0.5
    // degree bound), then the first-person view's or the head's turn.
    for index in [PLACED, VIEW, HEAD, IN_VIEW] {
        let run = &walked[index];
        let mut d = Directions::default();
        let mut previous: Option<(&Seen, std::collections::HashMap<u64, (usize, [f32; 3], [f32; 3])>)> = None;
        for (n, status) in run.statuses.iter().enumerate() {
            let at = frame_at(log, n);
            let seen = run.seen[n].as_ref().expect("the walk stays in the field");
            let turn = match index {
                VIEW => Some(view_axes(n as u64)),
                HEAD => Some(head_of(n as u64)),
                _ => None,
            };
            let mut heard = std::collections::HashMap::new();
            for source in field_emitters(status) {
                let (id, actor) = (source["id"].as_u64().unwrap(), source["follows"].as_u64().unwrap() as usize);
                let v = unit(seen.view_of(actor).expect("the emitter's actor and the controlled one"));
                let t = target(source).expect("an emitter apart from the character has a direction");
                let off = between(turn.map_or(v, |axes| apply(&axes, v)), t);
                d.worst = d.worst.max(off);
                d.checked += 1;
                assert!(off < 0.5, "{:?}, frame {at}: source {id} is heard {off:.3} degrees from where the field reader puts actor {actor}: {source}", run.presentation);
                if turn.is_none() {
                    // Ahead or behind by the camera-space depth, above or
                    // below by the camera-space y (down).
                    if v[2].abs() > 0.05 {
                        assert_eq!(t[2] > 0.0, v[2] > 0.0, "frame {at}: source {id} in front or behind as its depth says: {source}");
                        *(if v[2] > 0.0 { &mut d.front } else { &mut d.back }) += 1;
                    }
                    if v[1].abs() > 0.05 {
                        assert_eq!(t[1] < 0.0, v[1] < 0.0, "frame {at}: source {id} above or below as its height says: {source}");
                        *(if v[1] < 0.0 { &mut d.up } else { &mut d.down }) += 1;
                    }
                }
                let oriented = unit(seen.oriented_view_of(actor).expect("as above"));
                heard.insert(id, (actor, t, turn.map_or(oriented, |axes| apply(&axes, oriented))));
            }
            // The original pans an emitter by where the screen shows it;
            // heard from the character, it falls on that side when the pan
            // is clear of the centre.
            if turn.is_none() {
                for voice in status["voices"].as_array().unwrap().iter().filter(|v| v.get("effect_channel").is_some()) {
                    let Some(&(_, t, _)) = voice["route"].as_u64().and_then(|id| heard.get(&id)) else { continue };
                    let v = voice["voice"].as_u64().unwrap() as usize;
                    let Some(unpanned) = xem_binaural::pan::unpan(run.frames[n].volumes[v]) else { continue };
                    let azimuth = t[0].atan2(t[2]).to_degrees();
                    if (unpanned.pan - 16384.0).abs() > 4096.0 && azimuth.abs() > 10.0 && azimuth.abs() < 170.0 {
                        assert_eq!(unpanned.pan > 16384.0, azimuth > 0.0, "frame {at}: voice {v} panned {} but heard at {azimuth}", unpanned.pan);
                        d.sides += 1;
                    }
                }
            }
            if let Some((before, was)) = &previous {
                let player = seen.player.expect("a controlled actor");
                let stays = |actor: usize| before.position(actor) == seen.position(actor);
                for (id, &(actor, t, oriented)) in &heard {
                    let Some(&(_, earlier, was_oriented)) = was.get(id) else { continue };
                    if !stays(player) || !stays(actor) {
                        continue;
                    }
                    if before.facings.get(player) != seen.facings.get(player) {
                        // The character turns in place (the camera gliding
                        // on or not): the sound moves exactly as the camera's
                        // orientation and the positions, which hold nothing
                        // of its facing, move it.
                        let moved = (between(earlier, t) - between(was_oriented, oriented)).abs().max(between(oriented, t));
                        d.turn_worst = d.turn_worst.max(moved);
                        d.turns += 1;
                        assert!(moved < 0.1, "frame {at}: the character's turn moved source {id} by {moved} degrees beyond the geometry");
                    } else if before.camera != seen.camera {
                        // The camera orbits: where the sound is heard turns
                        // by the geometry (the check above, every frame).
                        d.orbit_frames += 1;
                        d.orbit_turn += between(earlier, t);
                    }
                }
            }
            previous = Some((seen, heard));
        }
        eprintln!("{:?}: {d:?}", run.presentation);
        assert!(d.checked > 2000, "{:?}: the emitters were placed and checked", run.presentation);
        if index == PLACED {
            assert!(d.front > 0 && d.back > 0 && d.up > 0 && d.down > 0 && d.sides > 100, "the walk heard emitters ahead, behind, above, below and to the sides: {d:?}");
            assert!(d.orbit_frames > 20 && d.orbit_turn > 30.0, "the camera's turns turned what is heard: {d:?}");
            assert!(d.turns > 0, "the character turned in place: {d:?}");
        }
    }
}

#[test]
fn along_a_walk_each_emitter_start_is_one_source_that_follows_its_own_actor() {
    let Some(Walks { log, walked }) = walks() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let run = &walked[PLACED];
    type Start = (u64, String, u64);
    let mut starts: std::collections::HashMap<u64, Start> = std::collections::HashMap::new();
    let mut sources: std::collections::HashMap<Start, u64> = std::collections::HashMap::new();
    let mut followed = std::collections::HashMap::new();
    let mut last = std::collections::HashMap::new();
    let (mut new_starts, mut tails, mut tail_frames, mut paired, mut original) = (0, std::collections::BTreeSet::new(), 0, 0, 0);
    for (n, status) in run.statuses.iter().enumerate() {
        let at = frame_at(log, n);
        let seen = run.seen[n].as_ref().expect("the walk stays in the field");
        for source in status["sources"].as_array().unwrap().iter().filter(|s| s["kind"] == "effect") {
            let id = source["id"].as_u64().unwrap();
            let start: Start = (source["channels"][0].as_u64().unwrap(), source["effect"].as_str().unwrap().to_owned(), source["start"].as_u64().unwrap());
            assert_eq!(*starts.entry(id).or_insert_with(|| start.clone()), start, "frame {at}: source {id} is one start, no other's");
            assert_eq!(*sources.entry(start.clone()).or_insert(id), id, "frame {at}: start {start:?} is one source");
            match source["emitter"].as_str().unwrap() {
                "field emitter" => {
                    let actor = source["follows"].as_u64().unwrap();
                    if !followed.contains_key(&id) {
                        // A new start: its slot follows that actor with
                        // that sound now (field_sound_start_emitter).
                        let slot = seen.slots[Seen::slot_of(start.0)];
                        let effect = u64::from_str_radix(start.1.trim_start_matches("0x"), 16).unwrap();
                        assert_eq!((u64::from(slot.0), u64::from(slot.1)), (actor, effect & 0xFFFF), "frame {at}: source {id} starts with its slot's actor and sound");
                        new_starts += 1;
                    }
                    assert_eq!(*followed.entry(id).or_insert(actor), actor, "frame {at}: source {id} follows one actor");
                    last.insert(id, target(source).expect("a direction"));
                }
                gone => {
                    assert_eq!(gone, "gone: keeps its last direction", "frame {at}: {source}");
                    assert!(source.get("follows").is_none(), "frame {at}: a tail follows nobody: {source}");
                    let placed = last.get(&id).expect("a tail was placed before");
                    assert_eq!(target(source), Some(*placed), "frame {at}: the tail of source {id} keeps its last direction");
                    tails.insert(id);
                    tail_frames += 1;
                }
            }
        }
        // Each voice routed to an effect's source is one of that start's
        // two channels; an effect voice in its original mix is a gap (with
        // positional effects on); a voice no channel holds keeps its mix.
        let mut voices_of = std::collections::HashMap::<u64, u32>::new();
        for voice in status["voices"].as_array().unwrap() {
            if voice.get("owner").is_some() {
                assert_eq!(voice["route"], "original", "frame {at}: {voice}");
            }
            let Some(channel) = voice["effect_channel"].as_u64() else { continue };
            match voice["route"].as_u64() {
                Some(id) => {
                    let start: Start = (channel & !1, voice["effect"].as_str().unwrap().to_owned(), voice["start"].as_u64().unwrap());
                    assert_eq!(starts.get(&id), Some(&start), "frame {at}: voice {} is routed to its own start's source", voice["voice"]);
                    *voices_of.entry(id).or_default() += 1;
                }
                None => {
                    assert_eq!(voice["route"], "original");
                    assert!(!voice["unresolved"].is_null(), "frame {at}: an effect voice in its original mix is reported: {voice}");
                    original += 1;
                }
            }
        }
        paired += voices_of.values().filter(|&&count| count == 2).count();
    }
    let gaps = &run.statuses.last().unwrap()["coverage_gaps"];
    eprintln!("emitters: {new_starts} starts, {} sources, {} tails over {tail_frames} frames ({tails:?}), {paired} frames of a start's two voices on one source, {original} effect voice frames in their original mix; gaps {gaps}", starts.len(), tails.len());
    assert!(new_starts >= 10, "the nearest three changed and new starts followed their new actors");
    assert!(!tails.is_empty(), "a start sounded on after its slot moved on");
    assert!(paired > 1000, "starts sounded on both their channels' voices");
    // What the menu played (its interface sounds) is reported, in the field
    // mode, and was mixed as the original; nothing else is.
    let gaps = gaps.as_array().unwrap();
    assert!(!gaps.is_empty() && gaps.iter().all(|gap| gap["mode"] == 1), "the menu's sounds are the field's coverage gaps: {gaps:?}");
    assert!(original > 0, "they played in the original mix");
}

#[test]
fn along_a_walk_the_music_keeps_its_places() {
    let Some(Walks { log, walked }) = walks() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let placed = &walked[PLACED];
    type Part = (String, u64);
    let music = |status: &Value| -> std::collections::BTreeMap<Part, Value> {
        status["sources"].as_array().unwrap().iter().filter(|s| s["kind"] == "music")
            .map(|s| ((s["song"].as_str().unwrap().to_owned(), s["part"].as_u64().unwrap()), s.clone())).collect()
    };
    // Where a part's place on the stage turns with its own pan.
    let stage = |source: &Value| {
        let p = &source["placement"];
        let (azimuth, elevation, home, spread) = (p["azimuth"].as_f64().unwrap(), p["elevation"].as_f64().unwrap(), p["home_pan"].as_f64().unwrap(), p["spread"].as_f64().unwrap());
        let pan = source["pan"].as_f64().unwrap_or(home);
        direction_of(azimuth + (pan - home) / 16384.0 * spread, elevation)
    };
    let mut places: std::collections::HashMap<Part, Value> = std::collections::HashMap::new();
    let (mut checked, mut turning, mut walking, mut pans, mut worst_pan) = (0, 0, 0, 0, 0f64);
    for (n, status) in placed.statuses.iter().enumerate() {
        let at = frame_at(log, n);
        let parts = music(status);
        for (part, source) in &parts {
            assert_eq!(*places.entry(part.clone()).or_insert_with(|| source["placement"].clone()), source["placement"], "frame {at}: part {part:?} keeps its place");
            let off = between(stage(source), target(source).expect("a place"));
            assert!(off < 0.01, "frame {at}: part {part:?} is heard {off} degrees from its place turned by its own pan: {source}");
            checked += 1;
            // Its own pan is its voice's (the SPU's volumes unpanned) where
            // the one voice it has held still through the frame.
            let id = &source["id"];
            let voices: Vec<usize> = status["voices"].as_array().unwrap().iter().filter(|v| &v["route"] == id).map(|v| v["voice"].as_u64().unwrap() as usize).collect();
            if let ([v], Some(pan), true) = (voices.as_slice(), source["pan"].as_f64(), n > 0) {
                let volume = placed.frames[n].volumes[*v];
                if volume == placed.frames[n - 1].volumes[*v] && !source["level_db"].is_null()
                    && let Some(unpanned) = xem_binaural::pan::unpan(volume)
                {
                    worst_pan = worst_pan.max((f64::from(unpanned.pan) - pan).abs());
                    pans += 1;
                }
            }
        }
        if n > 0 && !parts.is_empty() {
            let (before, now) = (placed.seen[n - 1].as_ref().unwrap(), placed.seen[n].as_ref().unwrap());
            turning += u32::from(before.camera != now.camera);
            walking += u32::from(now.player.and_then(|p| now.position(p)) != before.player.and_then(|p| before.position(p)));
        }
    }
    eprintln!("music: {} parts, {checked} part frames heard at their places, through {turning} frames of camera movement and {walking} of walking; its pan within {worst_pan} of its voice's in {pans} frames", places.len());
    assert!(places.len() >= 8 && checked > 10_000 && turning > 300 && walking > 300, "the song played through the walk's camera turns and walking");
    assert!(pans > 1000 && worst_pan <= 256.0, "the music's own pan is its voices' (unpan's precision)");
    // The first-person look leaves the music where it is.
    for (n, (view, plain)) in walked[VIEW].statuses.iter().zip(&placed.statuses).enumerate() {
        let strip = |parts: std::collections::BTreeMap<Part, Value>| -> Vec<(Part, Value, Value)> {
            parts.into_iter().map(|(part, s)| (part, s["target"].clone(), s["placement"].clone())).collect()
        };
        assert_eq!(strip(music(view)), strip(music(plain)), "frame {}: looking around in first person moves no music", frame_at(log, n));
    }
    // Head-tracked music: the stage stays put as the head turns, so it is
    // heard turned the other way.
    let mut worst = 0f32;
    for (n, status) in walked[HEAD].statuses.iter().enumerate() {
        for source in music(status).values() {
            let off = between(apply(&head_of(n as u64), stage(source)), target(source).expect("a place"));
            worst = worst.max(off);
            assert!(off < 0.05, "frame {}: head-tracked part {} heard {off} degrees from its place against the head", frame_at(log, n), source["part"]);
        }
    }
    eprintln!("head-tracked music: within {worst} degrees of its places against the head");
}

/// Each block's (`BLOCK` samples) energy in a band, both sides summed: from
/// `low` Hz up (two second-order Butterworth high-pass sections, RBJ's
/// cookbook), below `high` Hz when given (two low-pass sections).
fn band(audio: &[i16], low: f64, high: Option<f64>) -> Vec<f64> {
    let section = |f0: f64, pass_high: bool| {
        let (sin, cos) = (2.0 * std::f64::consts::PI * f0 / 44_100.0).sin_cos();
        let alpha = sin / std::f64::consts::SQRT_2;
        let a0 = 1.0 + alpha;
        let b = if pass_high { [(1.0 + cos) / 2.0, -(1.0 + cos), (1.0 + cos) / 2.0] } else { [(1.0 - cos) / 2.0, 1.0 - cos, (1.0 - cos) / 2.0] };
        (b.map(|x| x / a0), [-2.0 * cos / a0, (1.0 - alpha) / a0])
    };
    let mut sections = vec![section(low, true), section(low, true)];
    if let Some(high) = high {
        sections.extend([section(high, false), section(high, false)]);
    }
    let frames = audio.len() / 2;
    let mut blocks = vec![0f64; frames.div_ceil(xem_binaural::BLOCK)];
    for e in 0..2 {
        let mut states = vec![[0f64; 4]; sections.len()];
        for n in 0..frames {
            let mut x = f64::from(audio[2 * n + e]);
            for ((b, a), s) in sections.iter().zip(states.iter_mut()) {
                let y = b[0] * x + b[1] * s[0] + b[2] * s[1] - a[0] * s[2] - a[1] * s[3];
                *s = [x, s[0], y, s[2]];
                x = y;
            }
            blocks[n / xem_binaural::BLOCK] += x * x;
        }
    }
    blocks
}

/// The high band above 8 kHz, and its three parts the click metric reads.
fn high_band(audio: &[i16]) -> Vec<f64> {
    band(audio, 8000.0, None)
}
const HIGH_PARTS: [(f64, f64); 3] = [(8000.0, 11_000.0), (11_000.0, 15_000.0), (15_000.0, 20_000.0)];

/// Around each event (the samples `start..end` of `original`'s timeline,
/// which `presented` is aligned to): from two blocks before it to twenty
/// after (58 ms, fades and glides included), how much more than the
/// original's the presentation's high band stands out of its own
/// surroundings, in dB, in the least of its three parts (8-11, 11-15 and
/// 15-20 kHz); and, for the record, its largest block against the
/// original's over the whole band. A block stands out by its energy over
/// the louder of the medians of the 20 blocks (58 ms) before it and the 20
/// after, over a floor of four LSB in band. A click, a step or an impulse
/// from one sample to the next, is a block or two above both in every
/// part of the band; a sound placed elsewhere changes the band's level
/// for as long as it sounds and reshapes it part by part (its HRTF pair's
/// spectrum), its attack and its end included.
fn clicks(presented: &[i16], original: &[i16], events: &[(usize, usize)]) -> Vec<(f64, f64)> {
    let floor = (xem_binaural::BLOCK * 2 * 16) as f64;
    let standing = |e: &[f64]| -> Vec<f64> {
        let median = |range: std::ops::Range<usize>| {
            let mut v = e[range].to_vec();
            v.sort_by(f64::total_cmp);
            v.get(v.len() / 2).copied().unwrap_or(0.0)
        };
        (0..e.len())
            .map(|k| {
                let around = median(k.saturating_sub(20)..k).max(median((k + 1).min(e.len())..(k + 21).min(e.len())));
                (e[k] + floor) / (around + floor)
            })
            .collect()
    };
    let parts: Vec<(Vec<f64>, Vec<f64>)> =
        HIGH_PARTS.iter().map(|&(low, high)| (standing(&band(presented, low, Some(high))), standing(&band(original, low, Some(high))))).collect();
    let (b, o) = (high_band(presented), high_band(original));
    events
        .iter()
        .map(|&(start, end)| {
            let blocks = (start / xem_binaural::BLOCK).saturating_sub(2)..(end / xem_binaural::BLOCK + 21).min(b.len()).min(o.len());
            let peak = |e: &[f64]| blocks.clone().map(|k| e[k]).fold(0.0, f64::max);
            let excess = parts.iter().map(|(p, q)| 10.0 * (peak(p) / peak(q)).log10()).fold(f64::MAX, f64::min);
            (excess, 10.0 * ((peak(&b) + floor) / (peak(&o) + floor)).log10())
        })
        .collect()
}

/// The samples the bypass is behind the original (under a block): the
/// presentation's own delay, by which its output is aligned.
fn held(bypass: &[i16], original: &[i16]) -> usize {
    (0..xem_binaural::BLOCK)
        .find(|held| bypass.len() >= 2 * held && bypass[2 * held..] == original[..bypass.len() - 2 * held])
        .expect("the bypass is the original mix, less than a block behind")
}

#[test]
fn along_a_walk_switching_sources_adds_no_clicks() {
    let Some(Walks { log, walked }) = walks() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // The presentation delays when samples are output, not where they are:
    // the sessions' outputs share one timeline (the bypass is the original
    // itself from its first sample).
    let plain = &walked[PLAIN];
    assert_eq!(held(&walked[BYPASS].audio, &plain.audio), 0);
    // The frames whose sources changed (an emitter's start or end, a voice
    // routed anew), and those whose settings changed (sounding voices fade
    // between their place and the original mix, the mix in or out).
    let placed = &walked[PLACED];
    let routes = |status: &Value| -> Vec<(u64, Value)> {
        status["voices"].as_array().unwrap().iter().map(|v| (v["voice"].as_u64().unwrap(), v["route"].clone())).collect()
    };
    let frame = |run: &Walked, n: usize| (run.ends[n - 1] / 2, run.ends[n] / 2);
    let switches: Vec<_> = (1..placed.statuses.len()).filter(|&n| routes(&placed.statuses[n]) != routes(&placed.statuses[n - 1])).map(|n| frame(placed, n)).collect();
    let toggled = &walked[TOGGLED];
    let settings: Vec<_> = (1..toggled.ends.len()).filter(|n| n % 120 == 0).map(|n| frame(toggled, n)).collect();
    let mut worst = Vec::new();
    for (name, run, events) in [("sources switched", placed, &switches), ("settings changed", toggled, &settings)] {
        let rises = clicks(&run.audio, &plain.audio, events);
        let most = rises.iter().map(|r| r.0).fold(f64::MIN, f64::max);
        let level = rises.iter().map(|r| r.1).fold(f64::MIN, f64::max);
        let at: Vec<String> = events.iter().zip(&rises).map(|(e, r)| format!("{}:{:+.1}/{:+.1}", e.0, r.0, r.1)).collect();
        eprintln!("clicks: {} frames where {name} (from frame {}); there the high band stands out at most {most:+.2} dB more than the original's in all its parts, its level at most {level:+.2} dB over it; {}",
            events.len(), frame_at(log, 0), at.join(" "));
        assert!(events.len() >= 10, "{name} along the walk");
        worst.push((name, most));
    }
    for (name, most) in worst {
        assert!(most <= 3.0, "where {name}, the high band stands out at most 3 dB more than the original's: {most:+.2} dB");
    }
    let (b, o) = (high_band(&placed.audio).iter().sum::<f64>(), high_band(&plain.audio).iter().sum::<f64>());
    eprintln!("the placed walk's high band against the original's: {:+.2} dB", 10.0 * (b / o).log10());
}

#[test]
fn along_a_walk_each_placed_source_is_mixed_once() {
    use xem_binaural::{BLOCK, Route};
    use xem_spu::VOICES;
    let Some(Walks { walked, .. }) = walks() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (journal, output) = walked[PLACED].tap.as_ref().expect("the placed session's tap capture");
    assert!(!journal.truncated());
    assert_eq!(journal.replay(|_, _| {}), *output, "mixed again from the capture, the output is the same, sample for sample");
    assert!(journal.frames().iter().all(|f| f.cd == [0, 0]), "the field plays no CD audio");
    let lead = journal.lead();
    // From the 25th block on: what the block under way when the capture
    // started held has left the filters' tails and the bass shelf.
    let skip = 24 * BLOCK;
    let sat = |x: i32| x.clamp(-32768, 32767);
    let mixed = |frame: &xem_spu::TapFrame, e: usize, reverb: bool| -> i32 {
        if frame.muted {
            return 0;
        }
        let voices: i32 = (0..VOICES).map(|v| (i32::from(frame.voice[v]) * i32::from(frame.volume[v][e])) >> 15).sum();
        let m = voices + if reverb { i32::from(frame.reverb[e]) } else { 0 };
        sat((sat(m + frame.cd[e]) * i32::from(frame.main[e])) >> 15)
    };
    // Each source alone: its voices, nothing else. Its signal is theirs at
    // the gains the driver resolved before panning (unpanned), the main
    // volume; heard from one place through the window, it is that signal
    // through the place's HRTF pair and the bass shelf, once: its energy is
    // what the pair's spectrum makes of the signal's (one transform of the
    // whole window, Parseval), where twice would be 3 dB or more over it.
    // Its energy against its voices' in the original mix is the pair's
    // colouring of its spectrum: within 1.5 dB for the pink spectrum the
    // pairs are levelled with (xem-binaural's tests), more for sounds whose
    // energy lies where the pair lifts or lowers it.
    let window = &walked[PLACED].statuses[TAP.start as usize..TAP.end as usize];
    let ids: std::collections::BTreeSet<u32> = journal.block_routes().into_iter().flatten().map(|(id, _)| id).collect();
    let hrtf = xem_binaural::hrtf::Hrtf::kemar();
    let (mut colouring, mut once) = (Vec::new(), Vec::new());
    for id in ids {
        let (mut original, mut heard, mut dry, mut n) = (0f64, 0usize, 0usize, 0usize);
        let mut signal = Vec::new();
        let alone = journal.replay(|routes, frame| {
            for (v, route) in routes.iter().enumerate() {
                if *route != Route::Source(id) {
                    frame.voice[v] = 0;
                }
            }
            frame.reverb = [0; 2];
            if n >= skip {
                original += (0..2).map(|e| f64::from(mixed(frame, e, false)).powi(2)).sum::<f64>();
                heard += usize::from(frame.voice.iter().any(|s| *s != 0));
                let main = f32::from(frame.main[0]).abs().max(f32::from(frame.main[1]).abs()) / 32768.0;
                let mut s = 0.0;
                for v in (0..VOICES).filter(|&v| frame.voice[v] != 0 && !frame.muted) {
                    match xem_binaural::pan::unpan(frame.volume[v]) {
                        Some(unpanned) => s += f32::from(frame.voice[v]) / 32768.0 * unpanned.gain * main,
                        None => dry += 1,
                    }
                }
                signal.push(s);
            }
            n += 1;
        });
        let energy: f64 = alone.iter().skip(2 * (lead + skip)).map(|s| f64::from(*s).powi(2)).sum();
        // Sources heard for a tenth of a second at least, above 10 LSB.
        if heard < 4410 || original < 100.0 * 2.0 * heard as f64 {
            continue;
        }
        colouring.push((id, 10.0 * (energy / original).log10()));
        let statuses: Vec<&Value> = window.iter().filter_map(|s| s["sources"].as_array().unwrap().iter().find(|s| s["id"] == id)).collect();
        let first = statuses.first().and_then(|s| target(s));
        let fixed = statuses.len() == window.len()
            && statuses.iter().all(|s| target(s).zip(first).is_some_and(|(t, first)| between(t, first) < 0.25)
                && between(direction_of(s["azimuth"].as_f64().unwrap(), s["elevation"].as_f64().unwrap()), first.unwrap()) < 0.5);
        if fixed && dry == 0 {
            let place = statuses[0]["target"].clone();
            let mut filter = xem_binaural::hrtf::Filter::default();
            hrtf.filter(place["azimuth"].as_f64().unwrap() as f32, place["elevation"].as_f64().unwrap() as f32, &mut filter);
            let predicted = through_pair(&signal, &filter) * 32768.0 * 32768.0;
            once.push((id, statuses[0]["kind"].as_str().unwrap().to_owned(), 10.0 * (energy / predicted).log10()));
        }
    }
    eprintln!("each source alone against its voices in the original mix (id, dB): {colouring:?}");
    eprintln!("sources heard from one place through the window, alone against their signal through that place's pair (id, kind, dB): {once:?}");
    assert!(colouring.len() >= 8 && once.len() >= 5, "the window's music parts and emitters were checked");
    for (id, kind, db) in &once {
        assert!(db.abs() <= 0.2, "{kind} source {id} alone is its signal through its pair once: {db:+.2} dB");
    }
    let within = colouring.iter().filter(|(_, db)| db.abs() <= 1.5).count();
    eprintln!("{within} of {} sources within 1.5 dB of their voices' original energy", colouring.len());
    for (id, db) in &colouring {
        assert!(db.abs() <= 3.0, "source {id} alone keeps its original energy but for its pair's colouring: {db:+.2} dB");
    }
    // The direct path alone (every voice routed to a source silenced) is
    // the original minus those voices: the rest as the SPU mixed it, within
    // the SPU's truncations (one LSB a voice) and the presentation's
    // rounding.
    let (mut expected, mut n) = (Vec::new(), 0usize);
    let rest = journal.replay(|routes, frame| {
        for (v, route) in routes.iter().enumerate() {
            if *route != Route::Original {
                frame.voice[v] = 0;
            }
        }
        let voices = frame.voice.iter().filter(|s| **s != 0).count() as i32;
        expected.push(([mixed(frame, 0, true), mixed(frame, 1, true)], voices));
        n += 1;
    });
    let mut worst = 0;
    let mut differing = 0;
    for (i, (want, voices)) in expected.iter().enumerate().skip(skip) {
        let Some(got) = rest.get(2 * (lead + i)..2 * (lead + i) + 2) else { break };
        for e in 0..2 {
            let off = (i32::from(got[e]) - want[e]).abs();
            worst = worst.max(off);
            differing += usize::from(off != 0);
            assert!(off <= voices + 1, "sample {i} side {e}: the direct path {} against the original's rest {} ({voices} voices)", got[e], want[e]);
        }
    }
    eprintln!("direct path: within {worst} of the original minus the placed voices ({differing} of {} samples differ)", 2 * (n - skip));
    // The agent's view of a capture: which voices each source had, block by block.
    let reply = walked[VIEW].tap_reply.as_ref().expect("the view session's capture");
    let tap = &reply["tap"];
    let routes = tap["routes"].as_array().unwrap();
    assert_eq!(tap["block"], BLOCK);
    assert_eq!(routes.first().map(|r| r["blocks"][0].clone()), Some(json!(0)));
    assert_eq!(routes.last().map(|r| r["blocks"][1].as_u64().unwrap() + 1), Some(tap["frames"].as_u64().unwrap().div_ceil(BLOCK as u64)));
    assert!(routes.windows(2).all(|w| w[1]["blocks"][0].as_u64().unwrap() == w[0]["blocks"][1].as_u64().unwrap() + 1), "the runs follow each other");
    assert!(routes.iter().any(|r| r["sources"].as_object().unwrap().len() >= 4), "music parts and emitters: {routes:?}");
    eprintln!("tap reply: {} frames in {} runs of blocks", tap["frames"], routes.len());
}

#[test]
fn along_a_walk_zooming_out_changes_nothing_heard_and_characters_in_view_keep_the_games_gains() {
    let Some(Walks { log, walked }) = walks() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // Zoomed out with characters in view off, the game's view is the
    // original's: the presentation has no zoom of its own to apply.
    let (zoomed, placed, plain) = (&walked[ZOOMED], &walked[PLACED], &walked[PLAIN]);
    assert_eq!(zoomed.frames, plain.frames, "drawing zoomed out leaves the game, its voices and the SPU as they are");
    for (n, (z, p)) in zoomed.statuses.iter().zip(&placed.statuses).enumerate() {
        assert_eq!(z, p, "frame {}: what is presented is the same, zoomed out or not", frame_at(log, n));
    }
    assert_eq!(zoomed.audio, placed.audio, "and sounds the same");
    // With characters in view the game keeps actors moving that the
    // original stops: which emitters it keeps sounding is then its own
    // decision, and every placed source still has the gain the driver gave
    // its voices (directions from the game's camera: checked with the field
    // reader above).
    let in_view = &walked[IN_VIEW];
    assert_ne!(in_view.memory_hash, plain.memory_hash, "characters in view change the game");
    let slots = |run: &Walked| -> Vec<[(u16, u16); 3]> { run.seen.iter().map(|s| s.as_ref().unwrap().slots).collect() };
    let differing = slots(in_view).iter().zip(&slots(plain)).filter(|(a, b)| a != b).count();
    let gains = &in_view.gains;
    eprintln!("characters in view: the emitter slots differ from the original's in {differing} frames; {} source frames checked ({} effects, {} music), worst {:+.4}",
        gains.checked, gains.effects, gains.music, gains.worst_ratio);
    assert!(gains.failures.is_empty(), "{} sources report another gain than the driver gave their voices, first: {}", gains.failures.len(), gains.failures.first().unwrap());
    assert!(gains.effects >= 500 && gains.music >= 500 && gains.worst_ratio.abs() < 0.01, "{gains:?}");
}


/// The energy of the mono `signal` (full scale 1) through the filter pair
/// `filter` (the renderer's 256-point spectra of one direction) and the
/// bass shelf, both ears: over one transform of the whole signal
/// (Parseval), as for a source whose filter stays put.
fn through_pair(signal: &[f32], filter: &xem_binaural::hrtf::Filter) -> f64 {
    use xem_binaural::fft::{Complex, Fft};
    use xem_binaural::hrtf::{BINS, FFT_SIZE};
    let n = (signal.len() + FFT_SIZE).next_power_of_two();
    let fft = Fft::new(n);
    let mut spectrum: Vec<Complex> = (0..n).map(|i| Complex::new(signal.get(i).copied().unwrap_or(0.0), 0.0)).collect();
    fft.forward(&mut spectrum);
    let shelf = xem_binaural::hrtf::Hrtf::kemar().bass_shelf();
    let mut energy = 0.0;
    for ear in &filter.ears {
        // The ear's impulse response: its spectrum's inverse transform.
        let mut response = vec![Complex::ZERO; FFT_SIZE];
        for k in 0..BINS {
            response[k] = ear[k];
            if k > 0 && k < FFT_SIZE / 2 {
                response[FFT_SIZE - k] = ear[k].conj();
            }
        }
        Fft::new(FFT_SIZE).inverse(&mut response);
        let mut long: Vec<Complex> = (0..n).map(|i| Complex::new(response.get(i).map_or(0.0, |c| c.re), 0.0)).collect();
        fft.forward(&mut long);
        energy += (0..n)
            .map(|k| {
                let gain = f64::from(shelf.magnitude(k.min(n - k) as f32 * 44_100.0 / n as f32));
                f64::from(spectrum[k].norm_sqr()) * f64::from(long[k].norm_sqr()) * gain * gain
            })
            .sum::<f64>();
    }
    energy / n as f64
}


