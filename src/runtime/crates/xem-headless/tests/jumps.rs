//! The party moves as tools/analysis/platforming.py's walkmesh model has it
//! (docs/runtime.md, Random battles in platforming areas): Fei's running
//! jump on foot (map 22) rises 118.25 and touches the floor again 240 units
//! on, the party's in its gears (map 414, `game_data.inGear` set) 131.25
//! and 204; a slide (attribute 0x400000) carries the leader down to its
//! foot with the D-pad held up it (map 22) and never lets it back up; and a
//! triangle of attribute 0x800000 on an upper layer has no floor: walking
//! into one (map 15, layer 1) drops the leader to layer 0's floor below.
//! Local only: it needs the user's disc 1 and a game module build.
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

/// An address an `inspect` reply gives as "0x…" or "[n] @0x…".
fn address(text: &Value) -> u32 {
    let text = text.as_str().expect("an address");
    let hex = text.rsplit('@').next().expect("a pointer").trim().trim_start_matches("0x");
    u32::from_str_radix(hex, 16).expect("hex")
}

/// The party leader's descriptor: its `FieldActor`, its sprite and the
/// translations of its two matrices.
struct Leader {
    actor: u32,
    model: u32,
    translations: [u32; 2],
}

fn party_leader(session: &mut Session<NativeModule>) -> Leader {
    let player = run(session, json!({"cmd": "field"}))["player"].as_u64().expect("a player");
    let descriptor = run(session, json!({"cmd": "inspect", "path": format!("field_view.components.descriptors[{player}]"), "depth": 2}));
    Leader {
        actor: address(&descriptor["actor"]),
        model: address(&descriptor["model"]),
        translations: [address(&descriptor["matrix"]["t"]), address(&descriptor["transform"]["t"])],
    }
}

/// The leader's flags, its layer and its position (16.16).
fn leader(session: &mut Session<NativeModule>, actor: u32) -> (u32, i16, [f64; 3]) {
    let mut bytes = [0; 0x2C];
    session.runtime.memory().read(actor, &mut bytes).expect("the leader's actor");
    let word = |at: usize| i32::from_le_bytes(bytes[at..at + 4].try_into().expect("four bytes"));
    let layer = i16::from_le_bytes([bytes[0x10], bytes[0x11]]);
    (word(0) as u32, layer, [0x20, 0x24, 0x28].map(|at| f64::from(word(at)) / 65536.0))
}

/// Puts the leader at `at` (whole units) on `layer`, standing on
/// `triangles` (one per actor layer) whose attribute word on its layer is
/// `attribute`, as field_event_place_actor_on_floor places an actor: its
/// position, triangles, layer, attribute and floor height, its matrices'
/// translations, and its sprite's position and floor.
fn place(session: &mut Session<NativeModule>, leader: &Leader, at: [i32; 3], layer: i16, triangles: &[i16], attribute: u32) {
    let mut write = |address: u32, bytes: Vec<u8>| session.runtime.memory().write(address, &bytes).expect("a write");
    let fixed: Vec<u8> = at.iter().flat_map(|v| (v << 16).to_le_bytes()).collect();
    let whole: Vec<u8> = at.iter().flat_map(|v| v.to_le_bytes()).collect();
    let mut per_layer = [0i16; 4];
    per_layer[..triangles.len()].copy_from_slice(triangles);
    write(leader.actor + 0x20, fixed.clone());
    write(leader.actor + 0x08, per_layer.iter().flat_map(|t| t.to_le_bytes()).collect());
    write(leader.actor + 0x10, layer.to_le_bytes().to_vec());
    write(leader.actor + 0x14, attribute.to_le_bytes().to_vec());
    write(leader.actor + 0x72, (at[1] as i16).to_le_bytes().to_vec());
    for translation in leader.translations {
        write(translation, whole.clone());
    }
    write(leader.model, fixed);
    write(leader.model + 0x84, (at[1] as i16).to_le_bytes().to_vec());
}

/// The D-pad eighth that walks the leader from `from` toward `to` (x, z)
/// under the field camera's heading.
fn toward(session: &mut Session<NativeModule>, from: [f64; 2], to: [f64; 2]) -> Vec<&'static str> {
    const EIGHTHS: [&[&str]; 8] =
        [&["Right"], &["Right", "Down"], &["Down"], &["Down", "Left"], &["Left"], &["Left", "Up"], &["Up"], &["Up", "Right"]];
    let camera = run(session, json!({"cmd": "field"}))["camera_angle"].as_f64().expect("a camera angle");
    let heading = (from[1] - to[1]).atan2(to[0] - from[0]) / std::f64::consts::TAU * 4096.0;
    EIGHTHS[((heading + camera).rem_euclid(4096.0) / 512.0).round() as usize % 8].to_vec()
}

/// A running jump up the screen: the rise above the floor it left, and how
/// far on it touches that floor's height again.
fn running_jump(session: &mut Session<NativeModule>) -> (f64, f64) {
    let actor = party_leader(session).actor;
    run(session, json!({"cmd": "hold", "actions": ["Up", "Run"]}));
    run(session, json!({"cmd": "frames", "count": 8}));
    run(session, json!({"cmd": "hold", "actions": ["Up", "Run", "Jump"]}));
    let mut track = Vec::new();
    for frame in 0..120 {
        run(session, json!({"cmd": "frames", "count": 1}));
        if frame == 2 {
            run(session, json!({"cmd": "hold", "actions": ["Up", "Run"]}));
        }
        let (flags, _, position) = leader(session, actor);
        track.push((flags, position));
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

#[test]
fn a_slide_carries_the_leader_down_and_never_back_up_whatever_the_d_pad_holds() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    enter(&mut session, 22, false);
    let leader_parts = party_leader(&mut session);
    // Map 22's layer-0 triangle 1118, a slide (0x400000): its centroid on
    // its plane, 23 above the floor at the slide's foot (y 0). Triangle
    // 953's centroid lies up the slope.
    place(&mut session, &leader_parts, [-786, -23, -1088], 0, &[1118], 0x400000);
    let up = [-813.0, -1078.7];
    let mut heights = Vec::new();
    for _ in 0..40 {
        let (_, _, at) = leader(&mut session, leader_parts.actor);
        let keys = toward(&mut session, [at[0], at[2]], up);
        run(&mut session, json!({"cmd": "hold", "actions": keys}));
        run(&mut session, json!({"cmd": "frames", "count": 2}));
        heights.push(leader(&mut session, leader_parts.actor).2[1]);
    }
    run(&mut session, json!({"cmd": "hold", "actions": []}));
    let foot = heights.iter().position(|y| *y >= 0.0).expect("at the slide's foot");
    assert!(foot <= 10, "down after {foot} updates: {heights:?}");
    assert!(heights[foot..].iter().all(|y| *y >= -8.0), "never back up the slide: {heights:?}");
}

#[test]
fn walking_into_an_upper_layers_floorless_triangle_drops_the_leader_to_the_layer_below() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    enter(&mut session, 15, false);
    let leader_parts = party_leader(&mut session);
    // Map 15's layer-1 triangle 0, a floor, at its centroid on its plane,
    // over layer 0's triangle 869. Layer 1's triangle 117 beside it, whose
    // centroid the leader walks toward, has attribute 0x800000.
    place(&mut session, &leader_parts, [101, -327, 74], 1, &[869, 0], 0);
    let keys = toward(&mut session, [101.0, 74.0], [162.7, 169.7]);
    run(&mut session, json!({"cmd": "hold", "actions": keys}));
    let mut track = Vec::new();
    for _ in 0..40 {
        run(&mut session, json!({"cmd": "frames", "count": 2}));
        track.push(leader(&mut session, leader_parts.actor));
    }
    run(&mut session, json!({"cmd": "hold", "actions": []}));
    let off = track.iter().position(|(_, layer, _)| *layer != 1).expect("off layer 1");
    let (flags, layer, from) = track[off];
    assert_eq!((layer, flags & 0x1000), (0, 0x1000), "falling on layer 0: {track:?}");
    let (flags, layer, to) = track[track.len() - 1];
    assert_eq!((layer, flags & 0x1800), (0, 0), "standing on layer 0: {track:?}");
    assert!(to[1] - from[1] >= 150.0, "fell {} from {}: {track:?}", to[1] - from[1], from[1]);
}
