//! The hosts' sound mode is the game's own (src/port/sound.c; local only: it
//! needs the user's disc 1 and a game module build): the driver's flags
//! follow the host's mode, and the host's mode follows the game's own Sound
//! screen. The driver's modulator shapes that switch a modulator off play
//! through the port's thunk (src/port/adapters.c).
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test sound_mode

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::json;
use std::path::PathBuf;
use xem_core::control::Session;
use xem_core::pad::button;
use xem_game::{NativeModule, boot};

/// The sound driver's flags (`sound_driver_flags`); bits 0x700 hold the
/// output mode: 0 Mono, 0x100 Stereo, 0x300 Wide.
const SOUND_DRIVER_FLAGS: u32 = 0x8005_957C;

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn frames(session: &mut Session<NativeModule>, count: u64) {
    let reply = session.execute(&json!({"cmd": "frames", "count": count}));
    assert!(reply.get("error").is_none(), "{reply}");
}

fn flags(session: &mut Session<NativeModule>) -> u16 {
    let mut bytes = [0; 2];
    session.runtime.memory().read(SOUND_DRIVER_FLAGS, &mut bytes).unwrap();
    u16::from_le_bytes(bytes)
}

fn mode(session: &mut Session<NativeModule>) -> u16 {
    flags(session) & 0x700
}

#[test]
fn the_game_takes_the_hosts_sound_mode() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    session.set_sound_mode(2).unwrap();
    frames(&mut session, 120);
    assert_eq!(mode(&mut session), 0x300, "Wide");
    let wide = session.snapshot_bytes().unwrap();
    session.set_sound_mode(0).unwrap();
    frames(&mut session, 2);
    assert_eq!(mode(&mut session), 0, "Mono");
    // A state from another mode takes the host's.
    session.restore_bytes(&wide).unwrap();
    assert_eq!(mode(&mut session), 0x300);
    frames(&mut session, 2);
    assert_eq!(mode(&mut session), 0, "Mono after the restore");
}

/// The original route that chooses Wide on the title screen's Sound screen
/// (tests/reference-inputs/new-game-sound-mode-wide.json, to its confirm).
#[test]
fn the_hosts_sound_mode_follows_the_games_sound_screen() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let presses = [(901, button::START), (1300, button::START), (1500, button::CROSS), (1750, button::DOWN),
                   (1810, button::CROSS), (2000, button::DOWN), (2300, button::CROSS)];
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    for frame in 0..2400u64 {
        let held = presses.iter().filter(|(start, _)| (*start..*start + 10).contains(&frame)).fold(0, |a, p| a | p.1);
        session.set_pad(0, held);
        // The host's setting, Stereo, as a host sets it before each frame.
        session.set_sound_mode(1).unwrap();
        session.frame().expect("frame");
    }
    assert_eq!(mode(&mut session), 0x300, "the game chose Wide, and a host that has not seen it yet keeps it");
    assert_eq!(session.take_sound_mode_choice(), Some(2), "the host's setting follows");
    assert_eq!(session.take_sound_mode_choice(), None);
    assert_eq!(session.execute(&json!({"cmd": "status"}))["sound_mode"], json!(2));
    session.set_sound_mode(2).unwrap();
    frames(&mut session, 2);
    assert_eq!(mode(&mut session), 0x300);
    session.set_sound_mode(1).unwrap();
    frames(&mut session, 2);
    assert_eq!(mode(&mut session), 0x100, "and the game the setting again");
}

/// The driver's modulator shapes 8-15 are `sound_switch_modulator_off`,
/// which returns nothing, in its table of functions returning the wave's
/// value: the module reaches it through the port's thunk of that type. The
/// debug Gear battle (the original's debug selector, each member's gear
/// column set to Nml, as the motion example's `--battle --gears`) with
/// Cross tapped every 30 frames plays sounds of those shapes: without the
/// thunk the module trapped on the call within 900 frames.
#[test]
fn the_debug_gear_battle_plays_the_modulator_off_shapes() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    let press = |session: &mut Session<NativeModule>, name: &str| {
        let reply = session.execute(&json!({"cmd": "press", "buttons": [name], "frames": 1}));
        assert!(reply.get("error").is_none(), "{reply}");
    };
    frames(&mut session, 60);
    let reply = session.execute(&json!({"cmd": "write", "path": "mode_battle_standalone", "value": 1}));
    assert!(reply.get("error").is_none(), "{reply}");
    let reply = session.execute(&json!({"cmd": "boot_mode", "mode": 2}));
    assert!(reply.get("error").is_none(), "{reply}");
    frames(&mut session, 900);
    for name in ["down", "down", "cross", "right", "cross", "right", "cross", "start"] {
        press(&mut session, name);
        frames(&mut session, 20);
    }
    frames(&mut session, 1500);
    assert_eq!(session.runtime.services().mode, Some(2), "the battle runs");
    for _ in 0..50 {
        press(&mut session, "cross");
        frames(&mut session, 29);
    }
    let status = session.execute(&json!({"cmd": "status"}));
    assert!(status["stopped"].is_null(), "the battle played on: {}", status["stopped"]);
}

