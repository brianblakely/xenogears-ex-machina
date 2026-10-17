//! The desktop's window plays a state another host made and saves one that
//! goes on natively: field 30's state from a plain session (frame F),
//! `xem-desktop --load-state` on a headless Wayland compositor (weston's
//! headless backend, lavapipe), with a fresh settings directory under the
//! HD-2D profile and no focus pause, played with no input to frame F+600
//! (`--exit-at-frame`), writing its state at F+300 (`--save-state-at`): its
//! RAM and scratchpad hash at both frames is the plain session's, the window
//! presented frames meanwhile (XEM_FRAME_STATS), and its state is the plain
//! session's at that frame byte for byte (the window asks for no reveals),
//! loads in a plain session to its hash and goes on to the window's end. Local only:
//! it needs the user's disc 1, a game module build and weston and lavapipe
//! (nix/runtime's shell).
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-desktop --test states

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::json;
use std::path::PathBuf;
use std::process::{Child, Command, Stdio};
use std::time::{Duration, Instant};
use xem_core::control::Session;
use xem_game::{NativeModule, boot};

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

/// Weston's headless compositor in a directory of its own (its runtime
/// directory, the desktop's configuration, cards and states); stopped and
/// removed when dropped.
struct Display {
    root: PathBuf,
    weston: Child,
}

impl Display {
    fn start(lavapipe: &str) -> Display {
        let root = std::env::temp_dir().join(format!("xem-desktop-states-{}", std::process::id()));
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

fn hash(session: &mut Session<NativeModule>) -> String {
    format!("{:016x}", session.memory_hash().unwrap())
}

fn frames(session: &mut Session<NativeModule>, count: u64) {
    for _ in 0..count {
        assert!(session.frame().expect("frame"));
    }
}

#[test]
fn the_desktops_window_plays_a_native_state_on_and_saves_one_that_goes_on_natively() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (Ok(lavapipe), true) = (std::env::var("XEM_LAVAPIPE_ICD"), Command::new("weston").arg("--version").output().is_ok()) else {
        eprintln!("skipping: no weston or lavapipe (run in nix/runtime's shell)");
        return;
    };
    let display = Display::start(&lavapipe);
    // Field 30, 400 frames in: the state another host made, and where a
    // plain session takes it 300 and 600 frames on.
    let mut plain = boot(&disc, &Default::default()).expect("boot").0;
    assert!(plain.execute(&json!({"cmd": "enter_field", "map": 30})).get("error").is_none());
    frames(&mut plain, 400);
    let state = plain.snapshot_bytes().unwrap();
    let at = plain.runtime.frames();
    let made = display.root.join("field30.xemstate");
    std::fs::write(&made, &state).unwrap();
    frames(&mut plain, 300);
    let halfway = hash(&mut plain);
    let plain_halfway = plain.snapshot_bytes().unwrap();
    frames(&mut plain, 300);
    let end = hash(&mut plain);

    let config = display.root.join("config/xenogears-ex-machina");
    std::fs::create_dir_all(&config).unwrap();
    std::fs::write(config.join("settings.json"), r#"{"graphics": {"profile": "Hd2d"}, "pause_on_focus_loss": false}"#).unwrap();
    let saved = display.root.join("window.xemstate");
    let began = Instant::now();
    let mut child = Command::new(env!("CARGO_BIN_EXE_xem-desktop"))
        .arg("--disc").arg(&disc)
        .arg("--cards").arg(display.root.join("cards"))
        .arg("--load-state").arg(&made)
        .arg("--save-state-at").arg(format!("{}:{}", at + 300, saved.display()))
        .arg("--exit-at-frame").arg((at + 600).to_string())
        .env("XDG_RUNTIME_DIR", display.root.join("run"))
        .env("WAYLAND_DISPLAY", "wl-xem")
        .env("SDL_VIDEODRIVER", "wayland")
        .env("SDL_AUDIO_DRIVER", "dummy")
        .env("VK_ICD_FILENAMES", &lavapipe)
        .env("XEM_DATA_DIR", display.root.join("config/xenogears-ex-machina"))
        .env("XEM_FRAME_STATS", "1")
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .expect("xem-desktop starts");
    let (out, err) = (child.stdout.take().unwrap(), child.stderr.take().unwrap());
    let reader = |stream: Box<dyn std::io::Read + Send>| std::thread::spawn(move || std::io::read_to_string(stream).unwrap_or_default());
    let (out, err) = (reader(Box::new(out)), reader(Box::new(err)));
    let status = loop {
        if let Some(status) = child.try_wait().unwrap() {
            break status;
        }
        if began.elapsed() > Duration::from_secs(180) {
            let _ = child.kill();
            panic!("xem-desktop did not reach frame {} in 180 s", at + 600);
        }
        std::thread::sleep(Duration::from_millis(100));
    };
    let (out, err) = (out.join().unwrap(), err.join().unwrap());
    assert!(status.success(), "xem-desktop failed: {err}");
    let line = |prefix: &str| out.lines().find_map(|l| l.strip_prefix(prefix)).map(str::to_owned);
    let written = line("xem-desktop: state saved frame ").unwrap_or_else(|| panic!("no saved state: {out}"));
    let exited = line("xem-desktop: frame ").unwrap_or_else(|| panic!("no exit line: {out}"));
    eprintln!("the window, {:.1} s: {written}; {exited}", began.elapsed().as_secs_f64());
    assert!(written.starts_with(&format!("{} memory_hash {halfway} ", at + 300)), "the window at frame {}: {written}, natively {halfway}", at + 300);
    assert_eq!(exited, format!("{} memory_hash {end}", at + 600), "the window at frame {}, natively {end}", at + 600);
    // The window presented as it played (a stats line a second of loop
    // iterations that presented a frame).
    let presented: Vec<u64> = err.lines().filter_map(|l| l.strip_prefix("xem-desktop stats: ")).filter_map(|l| l.split(' ').next()?.parse().ok()).collect();
    assert!(presented.iter().sum::<u64>() > 100, "the window presented frames: {presented:?}");
    // The window's state, natively: its RAM, and on to the window's end. It
    // asked for no reveals (no wider framing, pull-back or inspection), so
    // it is the plain session's state at that frame, byte for byte.
    let window = std::fs::read(&saved).unwrap();
    assert!(window == plain_halfway, "the window's state is the plain session's at frame {}, byte for byte", at + 300);
    let mut other = boot(&disc, &Default::default()).expect("boot").0;
    other.restore_bytes(&window).expect("the window's state loads");
    assert_eq!((other.runtime.frames(), hash(&mut other)), (at + 300, halfway), "the window's state natively");
    frames(&mut other, 300);
    assert_eq!(hash(&mut other), end, "goes on natively to the window's end");
}
