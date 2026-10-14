//! The party's running jump is what tools/analysis/platforming.py's
//! measured arcs say it is (docs/runtime.md, Random battles in platforming
//! areas): Fei on foot (map 22) rises 118.25 and touches the floor again
//! 240 units on, the party in its gears (map 414, `game_data.inGear` set)
//! rises 131.25 and touches down 204 units on. Local only: it needs the
//! user's disc 1 and a game module build.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test jumps

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::path::PathBuf;
use xem_core::control::Session;
use xem_game::{NativeModule, boot};

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn run(session: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = session.execute(&command);
    assert!(reply.get("error").is_none(), "{command} -> {reply}");
    reply
}

/// Enters `map` as `enter_field` does (the kernel menu's recipe), with the
/// party riding its gears when `gears`: `game_data.inGear` set for the
/// three slots before the field loads, which `enter_field` leaves clear.
fn enter(session: &mut Session<NativeModule>, map: u64, gears: bool) {
    if gears {
        let until = |path: &str| json!({"cmd": "run_until", "max_frames": 6000, "until": {"path": path, "equals": 1}});
        run(session, until("cd_movie_request_kind"));
        run(session, json!({"cmd": "scenario", "writes": [], "mode": 0}));
        run(session, until("mode_kernel_menu_running"));
        for (path, value) in [
            ("game_data.map", map), ("game_data.inGear[0]", 1), ("game_data.inGear[1]", 1), ("game_data.inGear[2]", 1),
            ("mode_next_mode", 1), ("mode_kernel_menu_running", 0), ("mode_field_entered_once", 0),
        ] {
            run(session, json!({"cmd": "write", "path": path, "value": value}));
        }
        run(session, until("mode_field_entered_once"));
    } else {
        run(session, json!({"cmd": "enter_field", "map": map}));
    }
    for _ in 0..300 {
        run(session, json!({"cmd": "frames", "count": 10}));
        if run(session, json!({"cmd": "context"}))["kind"] == "walk" {
            run(session, json!({"cmd": "frames", "count": 30}));
            return;
        }
    }
    panic!("map {map}: the player never got control");
}

/// The party leader's `FieldActor`: its flags and its position (16.16).
fn leader(session: &mut Session<NativeModule>, actor: u32) -> (u32, [f64; 3]) {
    let mut bytes = [0; 0x2C];
    session.runtime.memory().read(actor, &mut bytes).expect("the leader's actor");
    let word = |at: usize| i32::from_le_bytes(bytes[at..at + 4].try_into().expect("four bytes"));
    (word(0) as u32, [0x20, 0x24, 0x28].map(|at| f64::from(word(at)) / 65536.0))
}

/// A running jump up the screen: the rise above the floor it left, and how
/// far on it touches that floor's height again.
fn running_jump(session: &mut Session<NativeModule>) -> (f64, f64) {
    let player = run(session, json!({"cmd": "field"}))["player"].as_u64().expect("a player");
    let descriptor = run(session, json!({"cmd": "inspect", "path": format!("field_view.components.descriptors[{player}]"), "depth": 1}));
    let actor = u32::from_str_radix(descriptor["actor"].as_str().expect("a pointer").trim_start_matches("0x"), 16).expect("hex");
    run(session, json!({"cmd": "hold", "actions": ["Up", "Run"]}));
    run(session, json!({"cmd": "frames", "count": 8}));
    run(session, json!({"cmd": "hold", "actions": ["Up", "Run", "Jump"]}));
    let mut track = Vec::new();
    for frame in 0..120 {
        run(session, json!({"cmd": "frames", "count": 1}));
        if frame == 2 {
            run(session, json!({"cmd": "hold", "actions": ["Up", "Run"]}));
        }
        track.push(leader(session, actor));
    }
    // Jumping or in the air: actor flags 0x1800.
    let left = track.iter().position(|(flags, _)| flags & 0x1800 != 0).expect("a jump");
    let from = track[left].1;
    let top = (left..track.len()).min_by(|a, b| track[*a].1[1].total_cmp(&track[*b].1[1])).expect("a top");
    let down = (top..track.len()).find(|n| track[*n].1[1] >= from[1]).expect("a landing");
    let to = track[down].1;
    (from[1] - track[top].1[1], ((to[0] - from[0]).powi(2) + (to[2] - from[2]).powi(2)).sqrt())
}

#[test]
fn the_party_jumps_as_the_platforming_classifier_measured() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    for (map, gears, rise, reach) in [(22, false, 118.25, 240.0), (414, true, 131.25, 204.0)] {
        let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
        enter(&mut session, map, gears);
        let (measured_rise, measured_reach) = running_jump(&mut session);
        assert_eq!(measured_rise, rise, "map {map}: the rise");
        assert!((measured_reach - reach).abs() < 0.05, "map {map}: touches down {measured_reach} on, not {reach}");
    }
}
