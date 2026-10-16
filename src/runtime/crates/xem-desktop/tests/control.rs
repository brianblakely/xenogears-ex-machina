//! The desktop's agent channel changes the gameplay options as the panel
//! does (docs/runtime.md, Gameplay options): `xem-desktop --control` on a
//! headless Wayland compositor (weston's headless backend, lavapipe), with
//! a fresh settings directory, gives the game random battles off in the
//! platforming areas and the original's everything else; `settings_apply`
//! changes each option through the settings service and answers its
//! revision, refuses a dwell out of range with the reason and changes
//! nothing then, and the next frames' `status` shows each value; the
//! settings file keeps the choices, so a second launch starts with them.
//! The panel's and the bindings' operations reach agents as one command
//! (docs/runtime.md, Saves and time controls): `app {request}` holds
//! rewind in field 30 until it lets go, as R held does (the game goes
//! back, then plays on), retries the pending card writes, and refuses the
//! file pickers and Recenter, each with the reason; `status` shows the
//! storage line's results (a card file that is no card, a kept BIOS image
//! that is no image).
//! Agents look around in first person (docs/runtime.md, First-person
//! inspection): `first_person` is refused outside a field; in field 30 it
//! turns the view on, the next frames' `status` shows `inspecting` and the
//! route under way stopped, `navigate` and a walk by `act` are refused, it
//! turns the view to the look asked, and turns it off.
//! Local only: it needs the user's disc 1, a game module build and weston
//! and lavapipe (nix/runtime's shell).
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-desktop --test control

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::io::{BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, ChildStdin, ChildStdout, Command, Stdio};
use std::time::{Duration, Instant};

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

/// Weston's headless compositor in a runtime directory of its own (one a
/// test, `name`), and the desktop's configuration and cards beside it; both
/// stop when dropped.
struct Display {
    root: PathBuf,
    weston: Child,
}

impl Display {
    /// A display of its own for the test `name`.
    fn start(name: &str, lavapipe: &str) -> Display {
        let root = std::env::temp_dir().join(format!("xem-desktop-control-{name}-{}", std::process::id()));
        std::fs::create_dir_all(root.join("run")).expect("a runtime directory");
        let weston = Command::new("weston")
            .args(["--backend=headless", "--renderer=pixman", "--width=1280", "--height=800", "--socket=wl-xem", "--idle-time=0"])
            .env("XDG_RUNTIME_DIR", root.join("run"))
            .env("VK_ICD_FILENAMES", lavapipe)
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .spawn()
            .expect("weston starts");
        let display = Display { root, weston };
        let began = Instant::now();
        while !display.root.join("run/wl-xem").exists() {
            assert!(began.elapsed() < Duration::from_secs(20), "weston's socket never came");
            std::thread::sleep(Duration::from_millis(50));
        }
        display
    }
}

impl Drop for Display {
    fn drop(&mut self) {
        let _ = self.weston.kill();
        let _ = self.weston.wait();
        let _ = std::fs::remove_dir_all(&self.root);
    }
}

/// `xem-desktop --disc DISC --control` on the display, its settings and
/// cards in the display's directory; stopped when dropped.
struct Desktop {
    child: Child,
    input: ChildStdin,
    output: BufReader<ChildStdout>,
}

impl Desktop {
    fn launch(display: &Display, disc: &Path, lavapipe: &str) -> Desktop {
        let mut child = Command::new(env!("CARGO_BIN_EXE_xem-desktop"))
            .arg("--disc").arg(disc)
            .arg("--cards").arg(display.root.join("cards"))
            .arg("--control")
            .env("XDG_RUNTIME_DIR", display.root.join("run"))
            .env("WAYLAND_DISPLAY", "wl-xem")
            .env("SDL_VIDEODRIVER", "wayland")
            .env("SDL_AUDIO_DRIVER", "dummy")
            .env("VK_ICD_FILENAMES", lavapipe)
            .env("XDG_CONFIG_HOME", display.root.join("config"))
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::null())
            .spawn()
            .expect("xem-desktop starts");
        let input = child.stdin.take().expect("stdin");
        let output = BufReader::new(child.stdout.take().expect("stdout"));
        Desktop { child, input, output }
    }

    fn send(&mut self, command: Value) -> Value {
        writeln!(self.input, "{command}").expect("a command line");
        self.input.flush().expect("sent");
        let mut line = String::new();
        self.output.read_line(&mut line).expect("a reply line");
        serde_json::from_str(&line).unwrap_or_else(|e| panic!("{command} -> {line:?}: {e}"))
    }

    /// The game's status once it has run `frames` frames past `since`.
    fn status_after(&mut self, since: u64, frames: u64) -> Value {
        let began = Instant::now();
        loop {
            let status = self.send(json!({"cmd": "status"}));
            if status["frames"].as_u64().is_some_and(|f| f >= since + frames) {
                return status;
            }
            assert!(began.elapsed() < Duration::from_secs(60), "the game ran no frames: {status}");
            std::thread::sleep(Duration::from_millis(20));
        }
    }
}

impl Drop for Desktop {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

/// The session's gameplay controls in a `status`.
fn gameplay(status: &Value) -> [Value; 5] {
    ["random_battles_off", "half_encounters", "text_speed", "auto_advance", "movie_skip"].map(|key| status[key].clone())
}

#[test]
fn agents_change_the_gameplay_options_on_the_desktops_control_channel_and_the_next_frames_show_them() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (Ok(lavapipe), true) = (std::env::var("XEM_LAVAPIPE_ICD"), Command::new("weston").arg("--version").output().is_ok()) else {
        eprintln!("skipping: no weston or lavapipe (run in nix/runtime's shell)");
        return;
    };
    let display = Display::start("gameplay", &lavapipe);
    let maps = json!(xem_settings::Settings::default().random_battles_off());
    let chosen = [json!([]), json!(true), json!(255), json!(90), json!(true)];
    {
        let mut desktop = Desktop::launch(&display, &disc, &lavapipe);
        let fresh = desktop.status_after(0, 10);
        assert_eq!(gameplay(&fresh), [maps.clone(), json!(false), json!(0), json!(0), json!(false)], "a fresh settings directory: the defaults");
        let settings = desktop.send(json!({"cmd": "settings"}));
        assert_eq!((&settings["random_battles"], &settings["encounter_rate"], &settings["text_speed"]), (&json!("NotWhilePlatforming"), &json!("Original"), &json!("Original")));
        for (n, change) in [json!({"RandomBattles": "Original"}), json!({"EncounterRate": "Half"}), json!({"TextSpeed": "Instant"}),
            json!({"AutoAdvance": {"enabled": true, "dwell": 90}}), json!({"MovieSkip": true})].into_iter().enumerate() {
            let ack = desktop.send(json!({"cmd": "settings_apply", "change": change}));
            assert_eq!(ack["revision"], json!(n + 1), "{change} -> {ack}");
        }
        let refused = desktop.send(json!({"cmd": "settings_apply", "change": {"AutoAdvance": {"enabled": true, "dwell": 5}}}));
        assert!(refused["error"].as_str().is_some_and(|e| e.contains("auto-advance dwell must be 30–600")), "{refused}");
        let applied = desktop.send(json!({"cmd": "status"}))["frames"].as_u64().expect("frames");
        let status = desktop.status_after(applied, 3);
        assert_eq!(gameplay(&status), chosen, "the next frames' status shows each choice");
        assert_eq!(desktop.send(json!({"cmd": "settings"}))["auto_advance"], json!({"enabled": true, "dwell": 90}), "the refused dwell changed nothing");
    }
    // The desktop saved them: a second launch starts with the choices.
    let saved: Value = serde_json::from_str(&std::fs::read_to_string(display.root.join("config/xenogears-ex-machina/settings.json")).expect("the settings file"))
        .expect("JSON");
    assert_eq!((&saved["random_battles"], &saved["movie_skip"]), (&json!("Original"), &json!(true)));
    let mut again = Desktop::launch(&display, &disc, &lavapipe);
    assert_eq!(gameplay(&again.status_after(0, 10)), chosen, "kept for the next launch");
}

#[test]
fn agents_hold_rewind_retry_card_writes_and_read_the_storage_line_through_the_desktops_app_command() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (Ok(lavapipe), true) = (std::env::var("XEM_LAVAPIPE_ICD"), Command::new("weston").arg("--version").output().is_ok()) else {
        eprintln!("skipping: no weston or lavapipe (run in nix/runtime's shell)");
        return;
    };
    let display = Display::start("app", &lavapipe);
    // Card 1's file is no card, and the BIOS image Import BIOS kept is no
    // image: the boot leaves both unused and says why. No focus pause, so
    // the field plays whatever focus the headless compositor gives.
    std::fs::create_dir_all(display.root.join("cards")).unwrap();
    std::fs::write(display.root.join("cards/card1.mcd"), b"not a card").unwrap();
    let config = display.root.join("config/xenogears-ex-machina");
    std::fs::create_dir_all(&config).unwrap();
    std::fs::write(config.join("bios.bin"), [0u8; 3]).unwrap();
    std::fs::write(config.join("settings.json"), r#"{"pause_on_focus_loss": false}"#).unwrap();
    let mut desktop = Desktop::launch(&display, &disc, &lavapipe);
    let storage = desktop.status_after(0, 10)["storage"].clone();
    assert_eq!(storage["pending_writes"], 0, "{storage}");
    assert!(storage["errors"][0].as_str().is_some_and(|e| e.starts_with("memory card 1: expected a raw 128 KiB image")), "{storage}");
    assert!(storage["kanji_font_notice"].as_str().is_some_and(|n| n.contains("bios.bin not used") && n.ends_with("not 3 bytes; save titles use the substitute glyphs.")),
        "{storage}");

    // Field 30, played five seconds: its rewind history (the window's
    // settings keep one). Held from the agent, rewind goes back a state
    // every 250 ms as a held R does; let go, the game plays on.
    let entered = desktop.send(json!({"cmd": "enter_field", "map": 30}));
    let at = entered["status"]["frames"].as_u64().unwrap_or_else(|| panic!("{entered}"));
    let ahead = desktop.status_after(at, 300)["frames"].as_u64().unwrap();
    assert_eq!(desktop.send(json!({"cmd": "app", "request": {"Rewind": true}})), json!({"ok": true, "message": "Rewinding while held."}));
    let began = Instant::now();
    let back = loop {
        let frames = desktop.send(json!({"cmd": "status"}))["frames"].as_u64().unwrap();
        if frames + 120 < ahead {
            break frames;
        }
        assert!(began.elapsed() < Duration::from_secs(30), "the held rewind went from frame {ahead} only to {frames}");
        std::thread::sleep(Duration::from_millis(50));
    };
    assert_eq!(desktop.send(json!({"cmd": "app", "request": {"Rewind": false}})), json!({"ok": true, "message": "Rewind released."}));
    let released = desktop.send(json!({"cmd": "status"}))["frames"].as_u64().unwrap();
    assert!(released <= back + 2 * xem_core::rewind::REWIND_PLAY_BLANKS, "it went on rewinding past frame {back}: {released}");
    let on = desktop.status_after(released, 60)["frames"].as_u64().unwrap();
    eprintln!("held rewind from frame {ahead} back to {released}, then on to {on}");

    // Retried, the card writes the boot met are done (card 1 stays unused).
    assert_eq!(desktop.send(json!({"cmd": "app", "request": {"Action": "RetryStorage"}})), json!({"ok": true, "message": "Pending card writes completed."}));
    let storage = desktop.send(json!({"cmd": "status"}))["storage"].clone();
    assert_eq!((&storage["pending_writes"], &storage["errors"]), (&json!(0), &json!([])), "{storage}");
    // A file picker is the player's (agents name the file), and only a
    // headset recentres.
    let picker = desktop.send(json!({"cmd": "app", "request": {"Action": "ExportState"}}));
    assert!(picker["error"].as_str().is_some_and(|e| e.starts_with("ExportState opens a file picker")), "{picker}");
    assert_eq!(desktop.send(json!({"cmd": "app", "request": {"Action": "Recenter"}})), json!({"error": "Recentering places a headset's view; play in a headset to use it."}));
}

#[test]
fn agents_turn_first_person_on_turn_it_and_turn_it_off_on_the_desktops_control_channel() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (Ok(lavapipe), true) = (std::env::var("XEM_LAVAPIPE_ICD"), Command::new("weston").arg("--version").output().is_ok()) else {
        eprintln!("skipping: no weston or lavapipe (run in nix/runtime's shell)");
        return;
    };
    let display = Display::start("first-person", &lavapipe);
    let mut desktop = Desktop::launch(&display, &disc, &lavapipe);
    desktop.status_after(0, 10);
    let refused = desktop.send(json!({"cmd": "first_person", "on": true}));
    assert!(refused["error"].as_str().is_some_and(|e| e.contains("field")), "the title is no field: {refused}");
    assert!(desktop.send(json!({"cmd": "first_person", "enabled": true}))["error"].is_string(), "an unknown field is refused");
    assert!(desktop.send(json!({"cmd": "enter_field", "map": 30})).get("error").is_none());
    let began = Instant::now();
    while desktop.send(json!({"cmd": "context"}))["kind"] != "walk" {
        assert!(began.elapsed() < Duration::from_secs(120), "field 30 never let the player walk");
        std::thread::sleep(Duration::from_millis(100));
    }
    let route = desktop.send(json!({"cmd": "navigate", "to": [-80.0, -90.0]}));
    assert!(route.get("error").is_none(), "{route}");
    let on = desktop.send(json!({"cmd": "first_person", "on": true}));
    assert_eq!(on, json!({"first_person": true, "look": {"yaw": 0.0, "pitch": 0.0}}), "it looks along the camera's heading");
    let since = desktop.send(json!({"cmd": "status"}))["frames"].as_u64().expect("frames");
    let status = desktop.status_after(since, 2);
    assert_eq!(status["inspecting"], true, "the next frames give the session the view: {status}");
    assert_eq!((&status["assist"], &status["assist_outcome"]), (&json!(null), &json!("stopped")), "the route under way stopped");
    for command in [json!({"cmd": "navigate", "to": [-80.0, -90.0]}), json!({"cmd": "act", "actions": ["Up"], "frames": 30})] {
        let reply = desktop.send(command.clone());
        assert!(reply["error"].as_str().is_some_and(|e| e.starts_with("first-person inspection keeps the character")), "{command} -> {reply}");
    }
    let turned = desktop.send(json!({"cmd": "first_person", "yaw": 30, "pitch": -10}));
    assert_eq!(turned, json!({"first_person": true, "look": {"yaw": 30.0, "pitch": -10.0}}), "turned to the look asked");
    let off = desktop.send(json!({"cmd": "first_person", "on": false}));
    assert_eq!(off, json!({"first_person": false, "look": null}));
    let since = desktop.send(json!({"cmd": "status"}))["frames"].as_u64().expect("frames");
    assert_eq!(desktop.status_after(since, 2)["inspecting"], false, "and the session has it off");
}
