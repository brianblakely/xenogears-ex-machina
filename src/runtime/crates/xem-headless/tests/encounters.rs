//! The field's random battles stay off where the host turns them off, and
//! nothing else changes (docs/runtime.md, Random battles in platforming
//! areas). On Babel Tower's map 414 a held direction brings a random battle
//! under the original (no control), while under the default setting's maps
//! (`Settings::default().random_battles_off()`) a walk five of the draw's
//! longest windows long brings none and the draw's period and step stay
//! as the map's script dealt them; the map's door still leads
//! on to map 415; map 172's Repair Bot still starts its scripted battle
//! when the player picks it; and on a map the setting leaves on (the
//! Mountain Path, map 15) the same walk brings the same random battle and
//! the same RAM with the control as without it. Local only: it needs the
//! user's disc 1 and a game module build.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test encounters

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::path::PathBuf;
use xem_core::control::Session;
use xem_game::{NativeModule, boot};
use xem_settings::Settings;

/// `field_encounter_battle_pending`: 1 once a battle is requested, until
/// the field leaves for it.
const BATTLE_PENDING: u32 = 0x800A_DBD0;
const FIELD_MODE: u64 = 1;
const BATTLE_MODE: u64 = 2;

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

/// A session on disc 1 whose random battles are off on `off`'s maps.
fn session(disc: &PathBuf, off: &[u16]) -> Session<NativeModule> {
    let (mut session, _) = boot(disc, &Default::default()).expect("boot");
    session.set_random_battles(off).expect("field maps");
    session
}

/// Enters `map` at `entry` and closes what its scripts say first, until
/// the player walks.
fn enter(session: &mut Session<NativeModule>, map: u64, entry: u64) {
    run(session, json!({"cmd": "enter_field", "map": map, "entry": entry}));
    for _ in 0..300 {
        run(session, json!({"cmd": "frames", "count": 10}));
        match run(session, json!({"cmd": "context"}))["kind"].as_str() {
            Some("walk") => {
                run(session, json!({"cmd": "frames", "count": 30}));
                if run(session, json!({"cmd": "context"}))["kind"] == "walk" {
                    return;
                }
            }
            Some("dialogue") => {
                run(session, json!({"cmd": "act", "actions": ["Confirm"], "frames": 2}));
            }
            _ => {}
        }
    }
    panic!("map {map}: the player never got control");
}

fn draw(session: &mut Session<NativeModule>) -> Value {
    run(session, json!({"cmd": "field"}))["random_draw"].clone()
}

fn mode(session: &mut Session<NativeModule>) -> u64 {
    run(session, json!({"cmd": "readiness"}))["mode"].as_u64().expect("a mode")
}

/// Whether the field has requested a battle or left for one.
fn battle(session: &mut Session<NativeModule>) -> bool {
    let mut pending = [0];
    session.runtime.memory().read(BATTLE_PENDING, &mut pending).expect("the field's flag");
    pending[0] == 1 || mode(session) == BATTLE_MODE
}

/// Holds `actions` for at most `frames` frames: the frames it took until a
/// battle came, if one did.
fn walk_until_battle(session: &mut Session<NativeModule>, actions: &[&str], frames: u64) -> Option<u64> {
    run(session, json!({"cmd": "hold", "actions": actions}));
    for frame in (10..=frames).step_by(10) {
        run(session, json!({"cmd": "frames", "count": 10}));
        if battle(session) {
            return Some(frame);
        }
    }
    None
}

#[test]
fn held_directions_bring_babel_towers_random_battle_only_under_the_original() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // The original's draw: map 414's script arms 300 counted field frames
    // with one battle; a field frame takes at most 4 vertical blanks.
    let mut original = session(&disc, &[]);
    enter(&mut original, 414, 0);
    let armed = draw(&mut original);
    assert_eq!((armed["map"].clone(), armed["period"].clone(), armed["count"].clone(), armed["off_here"].clone()), (json!(414), json!(300), json!(1), json!(false)));
    let came = walk_until_battle(&mut original, &["Up"], 2 * 301 * 4).expect("the original's random battle comes within two windows");
    eprintln!("map 414: the original's random battle after {came} frames; its draw as dealt: {armed}");
    // The default setting turns Babel Tower's off: a walk of five windows
    // brings none, and the draw stays as it was dealt.
    let off = Settings::default().random_battles_off();
    assert!(off.contains(&414));
    let mut suppressed = session(&disc, off);
    enter(&mut suppressed, 414, 0);
    let dealt = draw(&mut suppressed);
    assert_eq!(dealt["off_here"], json!(true));
    assert_eq!(dealt["steps"], armed["steps"], "the same step dealt as the original's");
    assert_eq!(walk_until_battle(&mut suppressed, &["Up"], 5 * 301 * 4), None, "no random battle where they are off (the original's came after {came} frames)");
    assert_eq!(draw(&mut suppressed), dealt, "no step counted down");
    assert_eq!(mode(&mut suppressed), FIELD_MODE);
    let status = run(&mut suppressed, json!({"cmd": "status"}));
    assert_eq!(status["random_battles_off"], json!(off));
}

#[test]
fn a_door_and_a_scripted_battle_still_work_where_random_battles_are_off() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let off = Settings::default().random_battles_off();
    assert!(off.contains(&172) && off.contains(&414) && off.contains(&415));
    // Map 414's entry 1 is at its door from map 415: down through it.
    let mut doors = session(&disc, off);
    enter(&mut doors, 414, 1);
    assert_eq!(draw(&mut doors)["off_here"], json!(true));
    run(&mut doors, json!({"cmd": "hold", "actions": ["Down"]}));
    let mut through = false;
    for _ in 0..30 {
        run(&mut doors, json!({"cmd": "frames", "count": 10}));
        if draw(&mut doors)["map"] == json!(415) {
            through = true;
            break;
        }
    }
    assert!(through, "the door leads on to map 415");
    // Map 172's Repair Bot (actor 5): talked to, its third choice starts
    // the script's battle with it (request_battle 3: formation 3).
    let mut bot = session(&disc, off);
    enter(&mut bot, 172, 0);
    assert_eq!(draw(&mut bot)["off_here"], json!(true));
    run(&mut bot, json!({"cmd": "interact", "actor": 5}));
    let mut choice = None;
    for _ in 0..300 {
        run(&mut bot, json!({"cmd": "frames", "count": 10}));
        let context = run(&mut bot, json!({"cmd": "context"}));
        if context["kind"] == "dialogue" {
            if !context["choice"].is_null() {
                choice = Some(context["choice"].clone());
                break;
            }
            // Its pages before the choice: Confirm turns each.
            run(&mut bot, json!({"cmd": "act", "actions": ["Confirm"], "frames": 2}));
        }
    }
    assert_eq!(choice, Some(json!({"count": 4, "index": 0})), "the bot offers its four choices");
    for _ in 0..2 {
        run(&mut bot, json!({"cmd": "act", "actions": ["Down"], "frames": 2}));
        run(&mut bot, json!({"cmd": "frames", "count": 6}));
    }
    assert_eq!(run(&mut bot, json!({"cmd": "context"}))["choice"]["index"], json!(2));
    assert!(!battle(&mut bot), "no battle before the pick");
    run(&mut bot, json!({"cmd": "act", "actions": ["Confirm"], "frames": 2}));
    let started = (0..60).any(|_| {
        run(&mut bot, json!({"cmd": "frames", "count": 10}));
        mode(&mut bot) == BATTLE_MODE
    });
    assert!(started, "the scripted battle starts");
    assert_eq!(run(&mut bot, json!({"cmd": "inspect", "path": "formation_selected_index"})), json!(3), "the script's formation");
}

#[test]
fn a_map_the_setting_leaves_on_draws_its_random_battle_as_without_the_control() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let off = Settings::default().random_battles_off();
    assert!(!off.contains(&15), "the Mountain Path keeps its random battles");
    let battles: Vec<(u64, Value)> = [&[] as &[u16], off]
        .into_iter()
        .map(|off| {
            let mut session = session(&disc, off);
            enter(&mut session, 15, 0);
            assert_eq!(draw(&mut session)["off_here"], json!(false));
            let came = walk_until_battle(&mut session, &["Up"], 2 * 241 * 4).expect("its random battle comes");
            (came, run(&mut session, json!({"cmd": "memory_hash"})))
        })
        .collect();
    assert_eq!(battles[0], battles[1], "the same battle at the same frame, the same RAM: the control changes nothing on a map it leaves on");
}
