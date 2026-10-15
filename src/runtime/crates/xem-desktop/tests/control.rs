//! The desktop's agent channel changes the gameplay options as the panel
//! does (docs/runtime.md, Gameplay options): `xem-desktop --control` on a
//! headless Wayland compositor (weston's headless backend, lavapipe), with
//! a fresh settings directory, gives the game random battles off in the
//! platforming areas and the original's everything else; `settings_apply`
//! changes each option through the settings service and answers its
//! revision, refuses a dwell out of range with the reason and changes
//! nothing then, and the next frames' `status` shows each value; the
//! settings file keeps the choices, so a second launch starts with them.
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

/// Weston's headless compositor in a runtime directory of its own, and the
/// desktop's configuration and cards beside it; both stop when dropped.
struct Display {
    root: PathBuf,
    weston: Child,
}

impl Display {
    fn start(lavapipe: &str) -> Display {
        let root = std::env::temp_dir().join(format!("xem-desktop-control-{}", std::process::id()));
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
    let display = Display::start(&lavapipe);
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
