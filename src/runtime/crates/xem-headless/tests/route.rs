//! The original game's new-game route through the real native services
//! (local only: it needs the user's disc 1 and a game module build). The
//! authoritative outcome (RAM and scratchpad) must not depend on presentation
//! capture or on a snapshot taken and restored on the way. `enter_field`
//! reaches the forest map and puts the party at the entrance it names, and
//! from a field that runs it leaves that field as the field's own end does.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test route

#![cfg(has_game_module)]

use std::path::PathBuf;

use xem_core::control::{Session, button_bit};
use xem_game::{BootOptions, NativeModule, boot};

/// tests/reference-inputs/new-game-movie-skip.json: skip the opening movie,
/// start a new game, skip the next movie into the story text.
const ROUTE: [(u64, u64, &str); 6] = [
    (901, 910, "start"),
    (1300, 1310, "start"),
    (1500, 1510, "cross"),
    (1750, 1760, "up"),
    (1810, 1820, "cross"),
    (4000, 4010, "cross"),
];

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn session(disc: &std::path::Path) -> Session<NativeModule> {
    let options = BootOptions::default();
    boot(disc, &options).expect("boot").0
}

/// Run frames `from..to` of the route.
fn run(session: &mut Session<NativeModule>, from: u64, to: u64) {
    for frame in from..to {
        let held = ROUTE
            .iter()
            .filter(|(start, end, _)| (*start..*end).contains(&frame))
            .fold(0, |bits, (_, _, name)| bits | button_bit(name).unwrap());
        session.runtime.services().pads.0[0].buttons = held;
        assert!(session.frame().expect("frame"), "a native disc never blocks");
    }
}

#[test]
fn outcome_ignores_presentation_and_snapshots() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    const END: u64 = 4500;

    let mut plain = session(&disc);
    run(&mut plain, 0, END);
    let reference = plain.memory_hash().unwrap();
    assert_eq!(plain.runtime.services().mode, Some(1), "the route ends in the field, which plays the movie");

    let mut presented = session(&disc);
    presented.runtime.services().presentation.enabled = true;
    run(&mut presented, 0, END);
    assert_eq!(presented.memory_hash().unwrap(), reference, "presentation capture changed the game");

    let mut restored = session(&disc);
    run(&mut restored, 0, 2500);
    let snapshot = restored.runtime.snapshot().unwrap();
    run(&mut restored, 2500, END);
    assert_eq!(restored.memory_hash().unwrap(), reference);
    restored.runtime.restore(&snapshot).unwrap();
    run(&mut restored, 2500, END);
    assert_eq!(restored.memory_hash().unwrap(), reference, "a restored snapshot diverged");
}

#[test]
fn enter_field_reaches_the_forest_map() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut session = session(&disc);
    let reply = session.execute(&serde_json::json!({ "cmd": "enter_field", "map": 23 }));
    assert_eq!(reply["map"], 23, "{reply}");
    for _ in 0..600 {
        assert!(session.frame().expect("frame"));
    }
    assert_eq!(session.runtime.services().mode, Some(1));
    // The map's opening dialogue window is up (field_dialogue_windows[0] has a message).
    let window = session.execute(&serde_json::json!({ "cmd": "inspect", "path": "field_dialogue_windows[0].message", "depth": 0 }));
    assert_ne!(window, serde_json::json!(0), "{window}");
}

/// `enter_field`'s `entry` is the entrance the party arrives at, as a
/// door's map change names it (`game_data.entry[2]`): on map 414 the
/// leader stands at the map's entry record of that number (the event
/// bytecode opens with ff; record n at 1 + 7n holds x and z).
#[test]
fn enter_field_places_the_party_at_the_entrance_it_names() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let placed: Vec<([f64; 2], [f64; 2])> = [0u32, 1].into_iter().map(|entry| {
        let mut session = session(&disc);
        let reply = session.execute(&serde_json::json!({ "cmd": "enter_field", "map": 414, "entry": entry }));
        assert_eq!(reply["map"], 414, "{reply}");
        for _ in 0..3000 {
            assert!(session.frame().expect("frame"));
            if session.runtime.context() == xem_core::context::Context::Walk {
                break;
            }
        }
        let mut word = [0; 4];
        session.runtime.memory().read(0x800A_DC00, &mut word).expect("field_event_bytecode");
        let mut record = [0; 5];
        session.runtime.memory().read(u32::from_le_bytes(word) + 1 + 7 * entry, &mut record).expect("the entry record");
        let half = |at: usize| f64::from(i16::from_le_bytes([record[at], record[at + 1]]));
        let field = session.execute(&serde_json::json!({ "cmd": "field" }));
        let leader = field["actors"].as_array().expect("actors").iter().find(|a| a["index"] == field["player"]).expect("the leader").clone();
        let at = |axis: &str| leader["position"][axis].as_f64().expect("a coordinate");
        ([at("x"), at("z")], [half(0), half(2)])
    }).collect();
    for (entry, (leader, record)) in placed.iter().enumerate() {
        assert_eq!(leader, record, "entrance {entry}: the leader stands on its entry record");
    }
    assert_ne!(placed[0].0, placed[1].0, "the two entrances are different places");
}

/// `enter_field` from a field that runs leaves it as its scripts' "end
/// field mode" event does: the field loop frees its party sprite blocks,
/// heap and sound bank before the kernel menu enters the next map. A
/// restart in the middle of field 12's dialogue (after the boot's 1650
/// frames and 650 in the field) left them to field 30, whose load made the
/// party's sprite from garbage and trapped about 125 frames on. With the
/// field's menu open, which runs a loop of its own, it is refused with the
/// reason, and goes once the menu closes.
#[test]
fn enter_field_from_a_running_field_leaves_it_as_the_field_ends() {
    use serde_json::{Value, json};
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut session = session(&disc);
    let ok = |session: &mut Session<NativeModule>, command: Value| {
        let reply = session.execute(&command);
        assert!(reply.get("error").is_none(), "{command} -> {reply}");
        reply
    };
    let menu = |session: &mut Session<NativeModule>| ok(session, json!({"cmd": "inspect", "path": "field_menu_request", "depth": 0}));
    ok(&mut session, json!({"cmd": "frames", "count": 1650}));
    assert_eq!(ok(&mut session, json!({"cmd": "enter_field", "map": 12}))["map"], 12);
    ok(&mut session, json!({"cmd": "frames", "count": 650}));
    assert_eq!(ok(&mut session, json!({"cmd": "context"}))["kind"], "dialogue", "field 12's dialogue shows");
    assert_eq!(ok(&mut session, json!({"cmd": "enter_field", "map": 30}))["map"], 30);
    ok(&mut session, json!({"cmd": "frames", "count": 400}));
    assert_eq!(ok(&mut session, json!({"cmd": "context"}))["kind"], "walk", "the player walks in field 30");
    // The field's menu open: refused; closed: the field is left.
    ok(&mut session, json!({"cmd": "act", "actions": ["Menu"], "frames": 2}));
    ok(&mut session, json!({"cmd": "frames", "count": 90}));
    assert_ne!(menu(&mut session), json!(0xFF), "the field's menu runs");
    let refused = session.execute(&json!({"cmd": "enter_field", "map": 12}));
    assert!(refused["error"].as_str().is_some_and(|e| e.contains("the field's menu is open")), "{refused}");
    for _ in 0..20 {
        if menu(&mut session) == json!(0xFF) {
            break;
        }
        ok(&mut session, json!({"cmd": "act", "actions": ["Cancel"], "frames": 2}));
        ok(&mut session, json!({"cmd": "frames", "count": 30}));
    }
    assert_eq!(menu(&mut session), json!(0xFF), "the menu closed");
    assert_eq!(ok(&mut session, json!({"cmd": "enter_field", "map": 12}))["map"], 12);
    ok(&mut session, json!({"cmd": "frames", "count": 200}));
}
