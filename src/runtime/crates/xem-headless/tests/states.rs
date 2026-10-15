//! Game states go on identically wherever the game is (local only: the
//! user's discs and a game module build; the held rewind's case renders with
//! whatever wgpu finds, lavapipe in the runtime shell). `continues_identically`
//! takes a state, runs frames keeping each one's RAM/scratchpad hash, the
//! SPU's output, the data sectors the drive delivers and the XA sectors it
//! plays, restores the state (whose own snapshot is then the state, byte for
//! byte) and runs the same frames again: the same hashes, samples and
//! sectors, and the same state at the end, byte for byte. Where:
//!
//! - the new game's opening movie with its XA sound, twice
//!   (tests/reference-inputs/new-game-sound-mode-stereo.json about frames
//!   4000 and 9000), then fast-forwarded at 10x over it;
//! - the world map, the party walking on (tests/reference-inputs/world-map-lahan-exit.json),
//!   the drive streaming its terrain;
//! - a Battling bout, which runs as the arena's task fiber beside the game's
//!   (a debug launch: the bout's task and parameters written as
//!   gestures.rs's bout writes them, and mode 4 selected; this bout stops at
//!   frame 724 on its own, before which its frames here end);
//! - the story's disc change during its five-second wait (discs.rs's route,
//!   no cards): restored into a fresh session booted from disc 1 with disc 2
//!   beside it, disc 2 goes in at the same cycle and the game goes on to the
//!   same RAM;
//! - a held rewind from the slice's forest battle (tests/reference-inputs/slice-forest.json)
//!   back into the field it came from: each step shows a frame the game drew
//!   after its state (the renderer replays its draws, as render.rs checks in
//!   a field and a battle), but where the game draws nothing new (the
//!   battle's opening while it loads), the steps cross from the battle into
//!   the field, and the field goes on identically from there.
//!
//!     XEM_DISC1=<disc 1> XEM_DISC2=<disc 2> cargo test --release -p xem-headless --test states

#![cfg(all(has_game_module, not(debug_assertions)))]

use std::cell::Cell;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::time::Duration;

use serde_json::{Value, json};
use xem_core::context::Context;
use xem_core::control::{Session, button_bit};
use xem_core::devices::cd::STORY_CHANGE_CYCLES;
use xem_core::snapshot::{Reader, checked};
use xem_game::{BootOptions, NativeModule, boot};
use xem_render::Gpu;
use xem_render::ps1::{GameView, HostClass, ViewConfig};
use xem_settings::Graphics;

fn disc(number: u8) -> Option<PathBuf> {
    let path = std::env::var_os(format!("XEM_DISC{number}")).map(PathBuf::from).unwrap_or_else(|| {
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join(format!("../../../../discs/Xenogears disc {number}.chd"))
    });
    path.exists().then_some(path)
}

/// The game with its sound kept for a host (`audio_output`), as hosts boot it.
fn session(disc: &Path) -> Session<NativeModule> {
    boot(disc, &BootOptions { audio_output: true, ..Default::default() }).expect("boot").0
}

fn ok(s: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = s.execute(&command);
    assert!(reply.get("error").is_none(), "{command}: {reply}");
    reply
}

fn reference(name: &str) -> Value {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../tests/reference-inputs").join(name);
    serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap()
}

fn step(s: &mut Session<NativeModule>) {
    assert!(s.frame().expect("frame"), "a native disc never blocks");
}

fn run_to(s: &mut Session<NativeModule>, to: u64) {
    while s.runtime.frames() < to {
        step(s);
    }
}

fn frames(s: &mut Session<NativeModule>, count: u64) {
    for _ in 0..count {
        step(s);
    }
}

/// The samples the SPU has queued for a host.
fn take_audio(s: &mut Session<NativeModule>) -> Vec<i16> {
    let spu = &mut s.runtime.services().spu;
    let mut samples = vec![0; spu.audio_queued()];
    spu.take_audio(&mut samples);
    samples
}

/// What `count` frames did: each frame's RAM/scratchpad hash, the samples
/// the SPU gave, the data sectors the drive delivered (cycle, address), the
/// XA audio sectors it played and the state at the end.
#[derive(PartialEq, Eq)]
struct Run {
    hashes: Vec<u64>,
    audio: Vec<i16>,
    sectors: Vec<(u64, u32)>,
    xa: u64,
    end: Vec<u8>,
}

fn run(s: &mut Session<NativeModule>, count: u64) -> Run {
    // The drive's tallies are the host's, outside states (a restore starts
    // its XA count afresh): counted within the run.
    s.runtime.services().cd.sector_log = Some(Vec::new());
    let xa = s.runtime.services().cd.xa_sectors;
    let mut run = Run { hashes: Vec::new(), audio: Vec::new(), sectors: Vec::new(), xa: 0, end: Vec::new() };
    for _ in 0..count {
        step(s);
        run.hashes.push(s.memory_hash().unwrap());
        run.audio.extend(take_audio(s));
    }
    run.sectors = s.runtime.services().cd.sector_log.take().unwrap().iter().map(|d| (d.cycle, d.lba)).collect();
    run.xa = s.runtime.services().cd.xa_sectors - xa;
    run.end = s.snapshot_bytes().unwrap();
    run
}

/// Takes a state, runs `count` frames, restores the state, runs them again:
/// they do the same. The pads hold what they held (no input is scheduled in
/// these frames). Returns the first run.
fn continues_identically(s: &mut Session<NativeModule>, count: u64) -> Run {
    take_audio(s);
    let pads = [0, 1].map(|port| s.runtime.services().pads.0[port].buttons);
    let state = s.snapshot_bytes().unwrap();
    let first = run(s, count);
    s.restore_bytes(&state).unwrap();
    assert!(s.snapshot_bytes().unwrap() == state, "a restored state is the state");
    for (port, buttons) in pads.into_iter().enumerate() {
        s.set_pad(port as u32, buttons);
    }
    assert!(take_audio(s).is_empty(), "the restore leaves no sound of the run it left");
    let again = run(s, count);
    if let Some(frame) = (0..first.hashes.len()).find(|&i| first.hashes[i] != again.hashes[i]) {
        panic!("RAM differs {} frames after the state", frame + 1);
    }
    if let Some(at) = (0..first.audio.len().min(again.audio.len())).find(|&i| first.audio[i] != again.audio[i]) {
        panic!("sound differs at sample {at} of {}", first.audio.len());
    }
    assert_eq!(first.audio.len(), again.audio.len(), "as many samples");
    assert!(first.sectors == again.sectors, "the drive delivers the same sectors at the same cycles");
    assert_eq!(first.xa, again.xa, "the drive plays as many XA sectors");
    if let Some(at) = (0..first.end.len().min(again.end.len())).find(|&i| first.end[i] != again.end[i]) {
        panic!("the states at the end differ from byte {at} of {}", first.end.len());
    }
    assert_eq!(first.end.len(), again.end.len());
    first
}

/// Loud enough to be sound, not silence.
fn sounds(audio: &[i16]) -> bool {
    audio.iter().filter(|s| s.unsigned_abs() > 1000).count() > 1000
}

/// Pad 1 by frame from a retained route of holds (tests/reference-inputs),
/// as a schedule of pad commands.
fn holds(name: &str) -> Value {
    let mut commands = Vec::new();
    for hold in reference(name).as_array().unwrap() {
        commands.push(json!([hold["start"], {"cmd": "pad", "port": 0, "buttons": hold["buttons"]}]));
        commands.push(json!([hold["end"], {"cmd": "pad", "port": 0, "buttons": []}]));
    }
    commands.sort_by_key(|c| c[0].as_u64());
    json!({"cmd": "schedule", "commands": commands})
}

#[test]
fn the_opening_movie_with_its_xa_sound_goes_on_identically_also_fast_forwarded() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut s = session(&disc);
    // The new game in Stereo; its movie plays from about frame 2960 to
    // 20780, and the route presses nothing between 2630 and 23500.
    ok(&mut s, holds("new-game-sound-mode-stereo.json"));
    for at in [4000, 9000] {
        run_to(&mut s, at);
        let movie = continues_identically(&mut s, 600);
        assert!(movie.xa > 0, "the drive plays the movie's XA audio at {at}");
        assert!(sounds(&movie.audio), "the movie sounds at {at}");
        assert!(!movie.sectors.is_empty(), "the movie streams its pictures from the disc at {at}");
    }
    // Fast-forward at 10x: the game's frames as fast, its music and the
    // SPU on the game's clock divided by the rate (headless and scripted).
    ok(&mut s, json!({"cmd": "fast_forward_configure", "rate": {"Fixed": 10}}));
    assert_eq!(ok(&mut s, json!({"cmd": "fast_forward", "enabled": true}))["fast_forward"], true);
    let fast = continues_identically(&mut s, 1200);
    assert!(s.fast_forward && s.frame_factor() == Some(10), "the restore keeps the host's fast-forward");
    assert!(fast.xa > 0, "the drive plays the movie's XA audio fast-forwarded");
    let seconds = fast.audio.len() as f64 / 2.0 / 44_100.0;
    assert!((1.8..2.2).contains(&seconds), "{seconds} s of sound for 20 s of the movie at 10x");
    assert!(sounds(&fast.audio), "the fast-forwarded movie sounds");
    assert_eq!(s.runtime.services().mode, Some(1), "still the field's movie");
}

#[test]
fn the_world_map_goes_on_identically_while_the_party_walks() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let route = reference("world-map-lahan-exit.json");
    let mut s = session(&disc);
    ok(&mut s, json!({"cmd": "schedule", "commands": route["commands"]}));
    run_to(&mut s, route["frames"].as_u64().unwrap());
    assert_eq!(format!("{:016x}", s.memory_hash().unwrap()), route["memory_hash"].as_str().unwrap());
    assert!(matches!(s.runtime.context(), Context::WorldMap { movement: 1, .. }));
    // Walking south, away from Lahan.
    s.set_pad(0, button_bit("down").unwrap());
    let walk = continues_identically(&mut s, 600);
    assert!(matches!(s.runtime.context(), Context::WorldMap { movement: 1, .. }), "still walking the world map");
    assert!(walk.hashes.windows(2).filter(|pair| pair[0] != pair[1]).count() > 500, "the world map moves");
    assert!(!walk.sectors.is_empty(), "the drive streams the world map's terrain");
}

/// Which fiber a state runs and what each of its two is (0 none, 1 not
/// started, 2 suspended): runtime.rs `snapshot_content` writes them after
/// the magic, the version, the identity and the entry.
fn fibers(state: &[u8]) -> (u32, [u32; 2]) {
    let mut r = Reader::new(checked(state).unwrap());
    r.u64().unwrap();
    r.u32().unwrap();
    r.bytes().unwrap();
    r.u32().unwrap();
    r.u32().unwrap();
    let current = r.u32().unwrap();
    let mut kinds = [0; 2];
    for kind in &mut kinds {
        *kind = r.u32().unwrap();
        // Not started: its stack's top; suspended: its stack pointer and area.
        for _ in 0..*kind {
            r.u32().unwrap();
        }
    }
    (current, kinds)
}

#[test]
fn a_battling_bout_goes_on_identically_beside_its_task_fiber() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut s = session(&disc);
    // Debug launch: from the kernel menu, a bout (entry kind 0) of the first
    // model against the third (gestures.rs), in mode 4.
    ok(&mut s, json!({"cmd": "boot_mode", "mode": 0}));
    ok(&mut s, json!({"cmd": "run_until", "max_frames": 3000, "until": {"path": "mode_kernel_menu_running", "equals": 1}}));
    let mut writes = vec![json!({"path": "mode_arena_task", "value": 0})];
    writes.extend([0, 0, 0, 2, 2, 0].iter().enumerate().map(|(i, v)| json!({"path": format!("mode_arena_task_parameters[{i}]"), "value": v})));
    ok(&mut s, json!({"cmd": "scenario", "mode": 4, "writes": writes}));
    for _ in 0..40 {
        if s.runtime.context() == (Context::Arena { state: 1 }) {
            break;
        }
        frames(&mut s, 30);
    }
    assert_eq!(s.runtime.context(), Context::Arena { state: 1 }, "the bout under way");
    frames(&mut s, 30);
    let (_, kinds) = fibers(&s.snapshot_bytes().unwrap());
    assert_ne!(kinds[1], 0, "the state holds the arena's task fiber");
    // Toward the opponent, through "READY" and "FIGHT!!". (This launch's
    // bout stops at frame 724, input or not, no state taken: the GPU's
    // display list links to address 0. These frames end before it.)
    s.set_pad(0, button_bit("right").unwrap());
    let bout = continues_identically(&mut s, 150);
    assert!(s.runtime.frames() < 724);
    assert_eq!(s.runtime.services().mode, Some(4), "still the bout");
    assert!(bout.hashes.windows(2).filter(|pair| pair[0] != pair[1]).count() > 140, "the bout moves");
    assert_ne!(fibers(&bout.end).1[1], 0, "and its task fiber with it");
}

/// discs.rs's story change: the forest map, the menu kind the last event of
/// disc 1 requests, its questions answered (Cross, then Circle: there is no
/// card) until the changer waits; returns the session there.
fn story_change_wait(disc1: &Path) -> Session<NativeModule> {
    let mut s = boot(disc1, &BootOptions::default()).expect("boot").0;
    ok(&mut s, json!({"cmd": "enter_field", "map": 23}));
    frames(&mut s, 600);
    // field_menu_request (field_mode.h, 0x800ADB64 in the field overlay).
    s.runtime.memory().write(0x800A_DB64, &6i32.to_le_bytes()).unwrap();
    for frame in 0..3000u64 {
        let button = match frame % 60 {
            0..6 => button_bit("cross"),
            30..36 => button_bit("circle"),
            _ => None,
        };
        s.set_pad(0, button.unwrap_or(0));
        step(&mut s);
        if s.runtime.services().cd.change_due().is_some() {
            s.set_pad(0, 0);
            return s;
        }
    }
    panic!("the disc change menu asked for no disc");
}

/// Runs until disc 2 is in: the frame, the cycle, then the RAM hash 300
/// frames on.
fn until_disc_2(s: &mut Session<NativeModule>) -> (u64, u64, u64) {
    while s.runtime.services().cd.inserted() != 2 {
        assert_eq!(s.runtime.services().cd.inserted(), 1, "disc 1 stays in for the five seconds");
        step(s);
    }
    let (frame, cycle) = (s.runtime.frames(), s.runtime.services().clock.now);
    frames(s, 300);
    (frame, cycle, s.memory_hash().unwrap())
}

#[test]
fn a_state_in_the_story_disc_changes_wait_puts_disc_2_in_at_the_same_cycle() {
    let (Some(disc1), Some(_)) = (disc(1), disc(2)) else {
        eprintln!("skipping: needs both discs (XEM_DISC1, XEM_DISC2), side by side");
        return;
    };
    let mut s = story_change_wait(&disc1);
    let due = s.runtime.services().cd.change_due().unwrap();
    assert_eq!(due.0, 2);
    let waiting = s.snapshot_bytes().unwrap();
    // Within the wait (far less than its five seconds).
    assert!(due.1 - s.runtime.services().clock.now > STORY_CHANGE_CYCLES / 2);
    continues_identically(&mut s, 120);
    s.restore_bytes(&waiting).unwrap();
    let here = until_disc_2(&mut s);
    drop(s);

    // A fresh session booted from disc 1, disc 2 beside it.
    let mut fresh = boot(&disc1, &BootOptions::default()).expect("boot").0;
    fresh.restore_bytes(&waiting).expect("the waiting state restores");
    assert_eq!(fresh.runtime.services().cd.change_due(), Some(due), "the same change, due at the same cycle");
    assert_eq!(fresh.runtime.services().cd.wanted(), None);
    assert_eq!(until_disc_2(&mut fresh), here, "disc 2 in at the same frame and cycle, the game on to the same RAM");
    assert!(here.1 >= due.1);
}

/// What a host does each frame: the game's draws since the last given to
/// the renderer, which draws them; how many of the shown frame's ops it
/// replayed (render.rs).
struct Presenter {
    gpu: Gpu,
    view: GameView,
    tick: u64,
}

impl Presenter {
    fn new() -> Self {
        let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
        let view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
        Presenter { gpu, view, tick: 0 }
    }

    fn present(&mut self, s: &mut Session<NativeModule>) -> usize {
        let graphics = Graphics { widescreen: true, ..Graphics::default() };
        self.view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, (640, 448), self.view.max_scale()));
        let device = s.runtime.services().gpu.device_mut();
        if self.view.take_restart() || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(self.view.reveals());
        self.view.push(device.take_trace());
        let display = device.display();
        let scene = s.scene();
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        self.tick += 1;
        assert!(self.view.present(&mut encoder, &display, &scene, self.tick as f64 / 60.0).is_some());
        self.gpu.queue.submit([encoder.finish()]);
        self.view.frame().drawn()
    }
}

/// slice-forest.json's battle runs from frame 5886 to 9514; it draws its
/// stage from about frame 6000.
const BATTLE_FROM: u64 = 5886;
const BATTLE_OPENED: u64 = 6000;

#[test]
fn a_held_rewind_from_a_battle_lands_in_its_field_showing_frames_the_game_drew() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let forest = reference("slice-forest.json");
    let booted = boot(&disc, &BootOptions { audio_output: true, ..Default::default() }).expect("boot").0;
    // The rewind's clock: the host's time, a frame's sixtieth of a second.
    let clock = Rc::new(Cell::new(Duration::ZERO));
    let source = clock.clone();
    let mut s = Session::new(booted.runtime, booted.schema, move || source.get());
    ok(&mut s, json!({"cmd": "schedule", "commands": forest["commands"]}));
    run_to(&mut s, 5400);
    assert_eq!(s.runtime.services().mode, Some(1), "in the forest");
    s.configure_rewind(xem_core::rewind::MAX_REWIND_BYTES, 1, 30).unwrap();
    let mut presenter = Presenter::new();
    // Into the battle, a state a second, the last 300 frames presented.
    while s.runtime.frames() < 6400 {
        clock.set(Duration::from_millis(s.runtime.frames() * 1000 / 60));
        step(&mut s);
        if s.runtime.frames() > 6100 {
            presenter.present(&mut s);
        }
    }
    assert_eq!(s.runtime.services().mode, Some(2), "in the battle");
    // Held: step after step, each a frame drawn after its state, until one
    // lands in the field before the battle.
    let mut modes = Vec::new();
    let mut undrawn = Vec::new();
    for step in 0..16 {
        let was = s.runtime.frames();
        assert!(s.rewind_step().unwrap(), "step {step}: history left");
        let landed = s.runtime.frames();
        assert!(landed < was, "step {step}: {was} -> {landed}");
        let mode = s.runtime.services().mode;
        modes.push(mode);
        if presenter.present(&mut s) == 0 {
            undrawn.push(landed);
        }
        if mode == Some(1) {
            break;
        }
    }
    assert!(modes.contains(&Some(2)) && modes.last() == Some(&Some(1)), "from the battle into the field: {modes:?}");
    // Each step shows a frame the game drew after its state, but where the
    // game draws nothing new: the battle's opening while it loads, where the
    // picture before it shows (measured here: the step landing at frame 5953
    // draws nothing, the one at 6007 draws).
    assert!(undrawn.len() <= 1 && undrawn.iter().all(|f| (BATTLE_FROM..BATTLE_OPENED).contains(f)), "steps showing the restored VRAM: {undrawn:?}");
    assert!(s.runtime.frames() < BATTLE_FROM, "before the encounter: frame {}", s.runtime.frames());
    let map = s.execute(&json!({"cmd": "inspect", "path": "mode_field_map_id", "depth": 0}));
    assert_eq!(map.as_u64().map(|m| m & 0xFFF), Some(23), "the forest");
    // Let go: the field goes on, identically from the state it is in.
    let field = continues_identically(&mut s, 300);
    assert!(presenter.present(&mut s) > 0, "the field drawn on");
    assert!(field.hashes.windows(2).any(|pair| pair[0] != pair[1]));
}
