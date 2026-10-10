//! The Phase 3 slice through the real native services (local only: it needs
//! the user's disc 1 and a game module build), from the session logs in
//! tests/reference-inputs/slice-*.json: the forest route (map 23's entry
//! dialogue, the walk into an encounter, the battle, its rewards, the return
//! and the menu), an agent's walk to field 22's save point and its save to
//! card 2, and the load of that card from the title screen. Each log ends at
//! the RAM/scratchpad hash its recording ended at (tools/native_scenario.py
//! and tools/field_agent.py `--log`).
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test slice
//!
//! Release builds only: a debug build runs the game too slowly for its
//! 60 000 frames.

#![cfg(all(has_game_module, not(debug_assertions)))]

use std::path::{Path, PathBuf};

use serde_json::{Value, json};
use xem_core::control::Session;
use xem_game::{BootOptions, NativeModule, boot};

struct Log {
    commands: Vec<Value>,
    frames: u64,
    memory_hash: String,
}

fn log(name: &str) -> Log {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join(format!("../../../../tests/reference-inputs/slice-{name}.json"));
    let value: Value = serde_json::from_str(&std::fs::read_to_string(path).expect("log")).expect("json");
    Log {
        commands: value["commands"].as_array().unwrap().clone(),
        frames: value["frames"].as_u64().unwrap(),
        memory_hash: value["memory_hash"].as_str().unwrap().to_string(),
    }
}

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

/// A session on the disc with its cards in `cards`, the log scheduled.
fn session(disc: &Path, cards: &Path, log: &Log) -> Session<NativeModule> {
    let schema = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../build/game/schema.json");
    let options = BootOptions { cards: Some(cards.to_path_buf()), schema: Some(schema), ..Default::default() };
    let mut session = boot(disc, &options).expect("boot").0;
    let reply = session.execute(&json!({ "cmd": "schedule", "commands": log.commands }));
    assert!(reply.get("error").is_none(), "{reply}");
    session
}

fn cards(name: &str) -> PathBuf {
    let dir = std::env::temp_dir().join(format!("xem-slice-{name}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();
    dir
}

fn hash(session: &mut Session<NativeModule>) -> String {
    format!("{:016x}", session.memory_hash().unwrap())
}

/// Run to `to` frames (counted from boot) one frame per call.
fn step_to(session: &mut Session<NativeModule>, to: u64) {
    while session.frames_run < to {
        assert!(session.frame().expect("frame"), "a native disc never blocks");
    }
}

#[test]
fn forest_route_runs_stepped_or_unthrottled_to_the_same_rewarded_end() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let forest = log("forest");

    // Unthrottled: one command runs every frame.
    let mut fast = session(&disc, &cards("forest-fast"), &forest);
    let reply = fast.execute(&json!({ "cmd": "frames", "count": forest.frames }));
    assert_eq!(reply["frames"], forest.frames, "{reply}");
    assert_eq!(hash(&mut fast), forest.memory_hash, "the recorded outcome");

    // Stepped frame by frame, with the modes it passes through.
    let mut stepped = session(&disc, &cards("forest-stepped"), &forest);
    let mut modes = Vec::new();
    while stepped.frames_run < forest.frames {
        stepped.frame().expect("frame");
        let mode = stepped.runtime.services().mode;
        if modes.last() != Some(&mode) {
            modes.push(mode);
        }
    }
    assert_eq!(hash(&mut stepped), forest.memory_hash);
    assert_eq!(stepped.runtime.services().clock.vblanks, fast.runtime.services().clock.vblanks);
    assert!(modes.contains(&Some(2)), "the route fights a battle: {modes:?}");
    assert_eq!(modes.last(), Some(&Some(1)), "and returns to the field: {modes:?}");
    // The battle's reward: the original gives item 0x39 on this route.
    let item = stepped.execute(&json!({ "cmd": "inspect", "path": "game_data.itemIds[0]", "depth": 0 }));
    let count = stepped.execute(&json!({ "cmd": "inspect", "path": "game_data.itemCounts[0]", "depth": 0 }));
    assert_eq!((item, count), (json!(0x39), json!(1)));
}

#[test]
fn spectators_attach_change_and_detach_without_changing_the_game() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let forest = log("forest");
    let mut session = session(&disc, &cards("spectated"), &forest);
    let mut samples = vec![0i16; 0];
    let mut pictures = 0;
    while session.frames_run < forest.frames {
        let frame = session.frames_run;
        let services = session.runtime.services();
        // Presentation capture and sound output attach, change and detach on
        // the way (the battle runs from about frame 5950 to 8600).
        match frame {
            1000 => services.presentation.enabled = true,
            3000 => services.spu.set_output(true),
            6500 => services.presentation.enabled = false,
            7000 => services.presentation.enabled = true,
            9000 => {
                services.presentation.enabled = false;
                services.spu.set_output(false);
            }
            _ => {}
        }
        if frame % 500 == 0 {
            let (width, height, rgba) = services.gpu.display_rgba8();
            assert_eq!(rgba.len(), (width * height * 4) as usize);
            pictures += 1;
        }
        let queued = services.spu.audio_queued();
        samples.resize(queued, 0);
        services.spu.take_audio(&mut samples);
        session.frame().expect("frame");
    }
    assert!(pictures > 20);
    assert_eq!(hash(&mut session), forest.memory_hash, "spectators changed the game");
    assert_eq!(session.frames_run, forest.frames, "or its pacing");
}

#[test]
fn a_snapshot_taken_in_battle_and_a_recording_replay_the_same_end() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let forest = log("forest");
    let mut session = session(&disc, &cards("snapshot"), &forest);
    session.execute(&json!({ "cmd": "record" }));
    step_to(&mut session, 7000);
    assert_eq!(session.runtime.services().mode, Some(2), "in battle");
    session.execute(&json!({ "cmd": "snapshot", "name": "battle" }));
    step_to(&mut session, forest.frames);
    let recording = session.execute(&json!({ "cmd": "stop_recording" }));
    assert_eq!(hash(&mut session), forest.memory_hash);

    // Restored in battle (the schedule's later commands given again).
    session.execute(&json!({ "cmd": "restore", "name": "battle" }));
    assert_eq!(session.frames_run, 7000);
    let later: Vec<Value> = forest.commands.iter().filter(|c| c[0].as_u64().unwrap() >= 7000).cloned().collect();
    session.execute(&json!({ "cmd": "schedule", "commands": later }));
    step_to(&mut session, forest.frames);
    assert_eq!(hash(&mut session), forest.memory_hash, "a restored snapshot diverged");

    // A fresh session given the log's setup and the recorded pad states.
    let setup = Log {
        commands: forest.commands.iter().filter(|c| c[1]["cmd"] != "pad").cloned().collect(),
        frames: forest.frames,
        memory_hash: forest.memory_hash.clone(),
    };
    let mut replayed = self::session(&disc, &cards("replay"), &setup);
    replayed.execute(&json!({ "cmd": "replay", "changes": recording["changes"], "start_vblank": recording["start_vblank"] }));
    step_to(&mut replayed, forest.frames);
    assert_eq!(hash(&mut replayed), forest.memory_hash, "the replayed recording diverged");
}

#[test]
fn the_agent_saves_at_field_22_and_the_title_screen_loads_it() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // The save: card 2 starts unformatted, as on the original route; the
    // game formats it and saves to its first block.
    let save = log("save");
    let save_cards = cards("save");
    std::fs::write(save_cards.join("card2.mcd"), vec![0u8; 0x20000]).unwrap();
    let mut saving = session(&disc, &save_cards, &save);
    saving.execute(&json!({ "cmd": "frames", "count": save.frames }));
    assert_eq!(hash(&mut saving), save.memory_hash);
    let saved_data = saving.execute(&json!({ "cmd": "inspect", "path": "game_data.itemIds", "depth": 1 }));
    drop(saving);
    let card = std::fs::read(save_cards.join("card2.mcd")).unwrap();
    assert_eq!(&card[..2], b"MC", "formatted");
    assert_eq!(card[128], 0x51, "the first block holds a save");
    assert_eq!(&card[128 + 10..128 + 22], b"BASLUS-00664", "of this game");

    // The load: that card as card 1, through the title's Continue.
    let load = log("load");
    let load_cards = cards("load");
    std::fs::write(load_cards.join("card1.mcd"), &card).unwrap();
    let mut loading = session(&disc, &load_cards, &load);
    loading.execute(&json!({ "cmd": "frames", "count": load.frames }));
    assert_eq!(hash(&mut loading), load.memory_hash);
    assert_eq!(loading.runtime.services().mode, Some(1), "back in the field");
    let map = loading.execute(&json!({ "cmd": "inspect", "path": "mode_field_map_id", "depth": 0 }));
    assert_eq!(map, json!(22));
    let loaded_data = loading.execute(&json!({ "cmd": "inspect", "path": "game_data.itemIds", "depth": 1 }));
    assert_eq!(loaded_data, saved_data, "the loaded inventory is the saved one");
}
