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
    let options = BootOptions { cards: Some(cards.to_path_buf()), ..Default::default() };
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
    while session.runtime.frames() < to {
        assert!(session.frame().expect("frame"), "a native disc never blocks");
    }
}

#[test]
fn original_start_pause_preserves_the_rewind_queue_until_play_resumes() {
    use std::{cell::Cell,rc::Rc,time::Duration};
    let Some(disc) = disc() else { eprintln!("skipping: no disc 1 (set XEM_DISC1)"); return };
    let options = BootOptions { cards: Some(cards("pause-rewind")), ..Default::default() };
    let booted = boot(&disc,&options).expect("boot").0;
    let clock = Rc::new(Cell::new(Duration::ZERO));
    let source = clock.clone();
    let mut s = Session::new(booted.runtime,booted.schema,move || source.get());
    let reply = s.execute(&json!({"cmd":"enter_field","map":22}));
    assert!(reply.get("error").is_none(),"{reply}");
    s.execute(&json!({"cmd":"frames","count":300}));
    s.configure_rewind(xem_core::rewind::MAX_REWIND_BYTES,1,30).unwrap();
    clock.set(Duration::from_secs(1));
    assert!(s.frame().unwrap());
    let retained = s.rewind.status();
    let press_start = |s: &mut Session<NativeModule>| {
        s.execute(&json!({"cmd":"press","buttons":["start"],"frames":4}));
        let reply = s.execute(&json!({"cmd":"frames","count":8}));
        assert!(reply.get("error").is_none(),"{reply}");
    };
    let silenced = |s: &mut Session<NativeModule>| {
        s.execute(&json!({"cmd":"inspect","path":"sound_driver_flags"})).as_i64().unwrap() & 0x40 != 0
    };
    press_start(&mut s);
    assert!(silenced(&mut s),"Start must enter the original field pause screen");
    for second in 2..=40 {
        clock.set(Duration::from_secs(second));
        assert!(s.frame().unwrap());
    }
    assert_eq!(s.rewind.status(),retained,"the original pause loop must not replace playing snapshots");
    clock.set(Duration::from_secs(4000));
    press_start(&mut s);
    assert!(!silenced(&mut s),"Start must resume play");
    assert_eq!(s.rewind.status(),retained,"resume must exclude time spent in the pause loop");
    clock.set(Duration::from_secs(4001));
    assert!(s.frame().unwrap());
    assert_eq!(s.rewind.status()["states"],3);
    assert_eq!(s.rewind.available_seconds(),2.0);
    let (was, newest) = (s.runtime.frames(), retained["newest_frame"].as_u64().unwrap());
    assert!(s.rewind_step().unwrap());
    // The newest playing state, played on until a frame drawn since shows.
    let landed = s.runtime.frames();
    assert!(landed > newest && landed <= newest + xem_core::rewind::REWIND_PLAY_BLANKS && landed < was, "{newest} -> {landed}");
    assert!(!silenced(&mut s),"rewind must land on playing history");
}

#[test]
fn fast_forward_preserves_the_original_music_tick_and_sample_rates() {
    let Some(disc) = disc() else { eprintln!("skipping: no disc 1 (set XEM_DISC1)"); return };
    let options = BootOptions { cards: Some(cards("music-time")), audio_output: true, ..Default::default() };
    let mut s = boot(&disc,&options).expect("boot").0;
    let reply = s.execute(&json!({"cmd":"enter_field","map":22}));
    assert!(reply.get("error").is_none(),"{reply}");
    s.execute(&json!({"cmd":"frames","count":300}));
    let saved = s.snapshot_bytes().unwrap();
    let ticks = s.execute(&json!({"cmd":"inspect","path":"sound_tick_count","depth":0})).as_u64().unwrap();
    let audio = s.runtime.services().spu.chip().clock();
    let frame = s.runtime.frames();
    for speed in [1,5,10,15,20] {
        s.restore_bytes(&saved).unwrap();
        assert!(s.execute(&json!({"cmd":"fast_forward_configure","rate":{"Fixed":speed}})).get("error").is_none());
        assert_eq!(s.execute(&json!({"cmd":"fast_forward","enabled":true}))["fast_forward"],true);
        let reply = s.execute(&json!({"cmd":"frames","count":60*speed}));
        assert!(reply.get("error").is_none(),"{reply}");
        assert_eq!(s.runtime.frames()-frame,60*speed);
        let now = s.execute(&json!({"cmd":"inspect","path":"sound_tick_count","depth":0})).as_u64().unwrap();
        assert_eq!(now-ticks,240,"original driver at {speed}×");
        assert_eq!(s.runtime.services().spu.chip().clock()-audio,44100,"SPU at {speed}×");
        let fast = s.snapshot_bytes().unwrap();
        s.execute(&json!({"cmd":"frames","count":30*speed}));
        let reference = s.snapshot_bytes().unwrap();
        s.restore_bytes(&fast).unwrap();
        assert!(s.fast_forward);
        assert_eq!(s.frame_factor(),Some(speed as u32));
        s.execute(&json!({"cmd":"frames","count":30*speed}));
        assert_eq!(s.snapshot_bytes().unwrap(),reference,"music and gameplay continuation at {speed}×");
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
    while stepped.runtime.frames() < forest.frames {
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
    while session.runtime.frames() < forest.frames {
        let frame = session.runtime.frames();
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
    assert_eq!(session.runtime.frames(), forest.frames, "or its pacing");
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

    // Restore the battle's game progress, then supply the remaining external
    // inputs again. App command queues are not part of a game save.
    session.execute(&json!({ "cmd": "restore", "name": "battle" }));
    let remaining: Vec<_> = forest.commands.iter().filter(|c| c[0].as_u64().unwrap() >= 7000).cloned().collect();
    session.execute(&json!({"cmd":"schedule","commands":remaining}));
    assert_eq!(session.runtime.frames(), 7000);
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
    let before_save = saving.snapshot_bytes().unwrap();
    saving.execute(&json!({ "cmd": "frames", "count": save.frames }));
    assert_eq!(hash(&mut saving), save.memory_hash);
    let saved_data = saving.execute(&json!({ "cmd": "inspect", "path": "game_data.itemIds", "depth": 1 }));
    let persisted = std::fs::read(save_cards.join("card2.mcd")).unwrap();
    saving.restore_bytes(&before_save).unwrap();
    saving.runtime.services().card.retry_writes().unwrap();
    assert_eq!(saving.runtime.services().card.image(1).unwrap(),persisted,"loading an older state keeps the original in-game save on card 2");
    assert_eq!(std::fs::read(save_cards.join("card2.mcd")).unwrap(),persisted,"state restore must never write an older card to disk");
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
    drop(loading);

    // The same save as a single save (.mcs), imported into a fresh card 1
    // before the load: the card is the one the game made, and the title's
    // Continue loads it to the same end.
    let listing = xem_core::devices::card_file::list(&card);
    assert_eq!(listing.saves[0].kind, xem_core::devices::card_file::Kind::Xenogears { file: 1, disc: Some(1) });
    assert!(listing.saves[0].problems.is_empty(), "the game's checks pass: {:?}", listing.saves[0].problems);
    let name = listing.saves[0].name.clone();
    let imported_cards = cards("imported");
    let file = imported_cards.join("save.mcs");
    std::fs::write(&file, xem_core::devices::card_file::export_save(&card, name.as_bytes()).unwrap()).unwrap();
    let mut importing = session(&disc, &imported_cards, &load);
    let reply = xem_game::storage::command(&mut importing, &json!({ "cmd": "save_import", "slot": 0, "path": file }));
    assert!(reply.get("error").is_none(), "{reply}");
    assert!(std::fs::read(imported_cards.join("card1.mcd")).unwrap() == card, "the card the game made");
    importing.execute(&json!({ "cmd": "frames", "count": load.frames }));
    assert_eq!(hash(&mut importing), load.memory_hash, "the imported save loads as the game's own");
}
