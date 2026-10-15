//! A long run goes on identically however it is carried (local only: the
//! user's disc 1 and a game module build). From Lahan (field 1) 300 frames
//! after entering it, two runs play the same frames with the same input,
//! a pure function of the frame number: pad 1 holds a seeded xorshift's
//! choice of the d-pad, Cross and Circle for 1 to 30 frames at a time, and
//! fast-forward runs at 10x over 600 frames around every third checkpoint.
//! The reference stays in one session. The other saves its state to a
//! file at every checkpoint (every 1 800 frames) and goes on in a freshly
//! booted session that loads it, as a load on another host does; and
//! every third checkpoint interval it turns rewind history on (a fake
//! clock, a state each 60 frames, a 24 MiB budget) at the start of a quiet
//! spell (no button held, no fast-forward: a rewind step plays on from a
//! state with the state's own buttons, `Session::rewind_step`), steps back
//! 1 to 4 times at its end, and plays on from where it lands. At every
//! checkpoint the two have the same RAM and scratchpad hash and the same
//! whole state, byte for byte (`Session::snapshot_bytes`); the rewind
//! history stays within its budget and history length.
//!
//! The run is 36 000 frames (ten minutes of game time; XEM_LONG_RUN_FRAMES
//! for more, a multiple of 1 800).
//!
//!     XEM_DISC1=<disc 1 image> [XEM_LONG_RUN_FRAMES=216000] cargo test --release -p xem-headless --test long_run

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::cell::Cell;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::time::{Duration, Instant};
use xem_core::control::{Session, button_bit};
use xem_game::{NativeModule, boot};

/// Frames between checkpoints.
const CHECKPOINT: u64 = 1800;
/// A quiet spell before each rewind: long enough for the states its steps
/// take (a state each 60 frames, four steps) and their play-on.
const QUIET: u64 = 420;
/// Rewind's memory budget: about eight of these states.
const BUDGET: usize = 24 * 1024 * 1024;
const HISTORY_SECONDS: u32 = 30;

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn ok(s: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = xem_game::storage::command(s, &command);
    assert!(reply.get("error").is_none(), "{command}: {reply}");
    reply
}

/// The input and the time controls by frame (counted from the start).
struct Plan {
    pads: Vec<u16>,
    checkpoints: u64,
}

impl Plan {
    fn new(frames: u64) -> Plan {
        let buttons = ["up", "down", "left", "right", "cross", "circle"].map(|name| button_bit(name).unwrap());
        let mut seed = 0x9E37_79B9_7F4A_7C15u64;
        let mut next = move || {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            seed
        };
        let mut plan = Plan { pads: Vec::with_capacity(frames as usize), checkpoints: frames / CHECKPOINT };
        while (plan.pads.len() as u64) < frames {
            let draw = next();
            // Nothing held a fifth of the time; else a direction, at times
            // with Cross or Circle, or one of them alone.
            let held = match draw % 10 {
                0 | 1 => 0,
                2 => buttons[4],
                3 => buttons[5],
                n => buttons[(n % 4) as usize] | if draw & 0x100 != 0 { buttons[4 + (draw >> 9 & 1) as usize] } else { 0 },
            };
            let hold = 1 + (draw >> 16) % 30;
            for _ in 0..hold {
                plan.pads.push(held);
            }
        }
        plan.pads.truncate(frames as usize);
        // The quiet spells: nothing held before each rewind.
        for interval in 1..=plan.checkpoints {
            if let Some(at) = plan.rewind(interval) {
                plan.pads[(at - QUIET) as usize..at as usize].fill(0);
            }
        }
        plan
    }

    /// The frame (from the start) of the rewind in checkpoint interval
    /// `interval` (the one ending at its checkpoint), every third one.
    fn rewind(&self, interval: u64) -> Option<u64> {
        (interval % 3 == 1).then(|| interval * CHECKPOINT - 900)
    }

    /// The steps that rewind takes back: 1 to 4.
    fn steps(&self, interval: u64) -> u64 {
        1 + (interval / 3) % 4
    }

    /// Whether fast-forward runs before frame `t`: 300 frames each side of
    /// every third checkpoint.
    fn fast(&self, t: u64) -> bool {
        let nearest = (t + CHECKPOINT / 2) / CHECKPOINT;
        nearest % 3 == 2 && t + 300 >= nearest * CHECKPOINT && t < nearest * CHECKPOINT + 300
    }
}

/// One frame of the plan: the frame's pad and fast-forward, then the frame.
fn step(s: &mut Session<NativeModule>, plan: &Plan, start: u64) {
    let t = s.runtime.frames() - start;
    s.set_pad(0, plan.pads[t as usize]);
    let fast = plan.fast(t);
    if s.fast_forward != fast {
        ok(s, json!({"cmd": "fast_forward", "enabled": fast}));
    }
    assert!(s.frame().expect("frame"), "a native disc never blocks");
}

/// A freshly booted session whose rewind history keeps `clock`'s time,
/// fast-forward at 10x, the state in `file` loaded.
fn fresh(disc: &Path, file: &Path, clock: &Rc<Cell<Duration>>) -> Session<NativeModule> {
    let booted = boot(disc, &Default::default()).expect("boot").0;
    let source = clock.clone();
    let mut s = Session::new(booted.runtime, booted.schema, move || source.get());
    ok(&mut s, json!({"cmd": "fast_forward_configure", "rate": {"Fixed": 10}}));
    ok(&mut s, json!({"cmd": "state_load", "path": file}));
    s
}

/// Each device's saved state.
fn devices(s: &mut Session<NativeModule>) -> [Vec<u8>; 5] {
    use xem_core::devices::Device;
    let s = s.runtime.services();
    [s.gpu.save(), s.cd.save(), s.mdec.save(), s.spu.save(), s.card.save()]
}

/// Where two sessions' states differ: the RAM hashes, each memory region a
/// state keeps whose bytes differ (how many, from where), each device's.
fn difference(a: &mut Session<NativeModule>, b: &mut Session<NativeModule>) -> String {
    let (x, y) = (a.memory_hash().unwrap(), b.memory_hash().unwrap());
    let mut found = vec![format!("RAM hash {x:016x} against {y:016x}")];
    for (start, len) in xem_core::snapshot::REGIONS {
        let (mut p, mut q) = (vec![0; len as usize], vec![0; len as usize]);
        a.runtime.memory().read(start, &mut p).unwrap();
        b.runtime.memory().read(start, &mut q).unwrap();
        if let Some(i) = (0..p.len()).find(|&i| p[i] != q[i]) {
            found.push(format!("{} bytes of memory differ from {:#010x}", (0..p.len()).filter(|&i| p[i] != q[i]).count(), start as usize + i));
        }
    }
    for ((x, y), name) in devices(a).iter().zip(&devices(b)).zip(["the GPU", "the drive", "the MDEC", "the SPU", "the cards"]) {
        if x != y {
            found.push(format!("{name}'s state"));
        }
    }
    found.join("; ")
}

#[test]
fn a_long_run_goes_on_identically_across_fresh_sessions_rewinds_and_fast_forward() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let frames: u64 = std::env::var("XEM_LONG_RUN_FRAMES").ok().map_or(36_000, |n| n.parse().expect("XEM_LONG_RUN_FRAMES is a frame count"));
    assert!(frames >= CHECKPOINT && frames % CHECKPOINT == 0, "XEM_LONG_RUN_FRAMES is a multiple of {CHECKPOINT}");
    let began = Instant::now();
    let plan = Plan::new(frames);
    let directory = std::env::temp_dir().join(format!("xem-long-run-{}", std::process::id()));
    std::fs::create_dir_all(&directory).unwrap();
    let file = directory.join("state.xemstate");

    // The reference: Lahan, then the plan in the same session.
    let mut reference = boot(&disc, &Default::default()).expect("boot").0;
    ok(&mut reference, json!({"cmd": "enter_field", "map": 1}));
    ok(&mut reference, json!({"cmd": "frames", "count": 300}));
    ok(&mut reference, json!({"cmd": "fast_forward_configure", "rate": {"Fixed": 10}}));
    let start = reference.runtime.frames();
    ok(&mut reference, json!({"cmd": "state_save", "path": &file}));
    let clock = Rc::new(Cell::new(Duration::ZERO));
    let mut carried = fresh(&disc, &file, &clock);

    let (mut hops, mut rewinds, mut fast) = (0, Vec::new(), 0u64);
    for interval in 1..=plan.checkpoints {
        let end = start + interval * CHECKPOINT;
        while reference.runtime.frames() < end {
            fast += u64::from(reference.fast_forward);
            step(&mut reference, &plan, start);
        }
        let mut rewind = plan.rewind(interval).map(|at| start + at);
        while carried.runtime.frames() < end {
            let now = carried.runtime.frames();
            if rewind.is_some_and(|at| now == at - QUIET + 20) && carried.rewind.budget == 0 {
                carried.configure_rewind(BUDGET, 1, HISTORY_SECONDS).unwrap();
            }
            if rewind == Some(now) {
                // Held for a few steps at the end of the quiet spell, once
                // (the frames played on from where it lands reach it again).
                rewind = None;
                let mut landed = Vec::new();
                for _ in 0..plan.steps(interval) {
                    let status = carried.rewind.status();
                    assert!(carried.rewind_step().unwrap(), "rewind history at frame {now}: {status}");
                    landed.push(carried.runtime.frames());
                }
                let status = carried.rewind.status();
                assert!(carried.rewind.bytes <= BUDGET, "rewind keeps within its budget: {status}");
                assert!(status["states"].as_u64().unwrap() <= u64::from(HISTORY_SECONDS) + 1, "and its history: {status}");
                assert!(landed.windows(2).all(|w| w[1] < w[0]) && *landed.last().unwrap() >= now - QUIET, "within the quiet spell: {landed:?} from {now}");
                rewinds.push((now - start, landed.iter().map(|frame| frame - start).collect::<Vec<_>>()));
                // Played on from where it landed: the same frames again.
            }
            clock.set(clock.get() + Duration::from_nanos(1_000_000_000 / 60));
            step(&mut carried, &plan, start);
        }
        // To a file, and on in a fresh session.
        ok(&mut carried, json!({"cmd": "state_save", "path": &file}));
        carried = fresh(&disc, &file, &clock);
        hops += 1;
        let same = reference.snapshot_bytes().unwrap() == carried.snapshot_bytes().unwrap();
        if !same {
            panic!("checkpoint {interval} (frame {}): the carried run differs: {}", end - start, difference(&mut reference, &mut carried));
        }
    }
    let map = reference.execute(&json!({"cmd": "inspect", "path": "mode_field_map_id", "depth": 0}));
    let (mode, hash) = (reference.runtime.services().mode, reference.memory_hash().unwrap());
    eprintln!("{frames} frames each in {:.1} s: {} checkpoints the same byte for byte, {hops} fresh sessions, {} rewinds ({rewinds:?}), \
        {fast} fast-forwarded frames, ending in mode {mode:?} (field map {map}) at RAM {hash:016x}",
        began.elapsed().as_secs_f64(), plan.checkpoints, rewinds.len());
    assert!(!rewinds.is_empty() && fast > 0, "the run rewinds and fast-forwards");
    std::fs::remove_dir_all(&directory).unwrap();
}
