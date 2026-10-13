//! Measures the gesture recognizers under hand-tracking noise (plan.md:
//! false activations, missed gestures, response latency, rapid pause and
//! resume, tracking-loss recovery). Hands are the synthetic model with
//! per-joint jitter (a tracker's), slow drift and tremor, varied speed,
//! size and orientation; idle play is relaxed and half-curled hands,
//! reaching, turning and resting fists that were never opened. All of it
//! is seeded and repeatable. A headset's own trackers differ: these numbers
//! qualify the recognizers' margins, not a device.
use crate::gesture::{Frame, Gesture, Recognizer, Swipe, JOINTS};
use crate::synthetic::{self, Frames};
use glam::{Quat, Vec3};

/// A small seeded generator (xorshift64*).
pub struct Rng(u64);

impl Rng {
    pub fn new(seed: u64) -> Rng {
        Rng(seed.max(1))
    }
    pub fn next(&mut self) -> f32 {
        self.0 ^= self.0 >> 12;
        self.0 ^= self.0 << 25;
        self.0 ^= self.0 >> 27;
        ((self.0.wrapping_mul(0x2545_F491_4F6C_DD1D) >> 40) as f32) / (1u64 << 24) as f32
    }
    pub fn range(&mut self, a: f32, b: f32) -> f32 {
        a + (b - a) * self.next()
    }
    /// Roughly normal, unit deviation.
    pub fn normal(&mut self) -> f32 {
        (0..4).map(|_| self.next()).sum::<f32>() * 1.732 - 3.464
    }
}

/// How a run's hands are disturbed.
#[derive(Clone, Copy, Debug)]
pub struct Noise {
    /// Per-joint jitter, metres (deviation).
    pub jitter: f32,
    /// Whole-hand tremor, metres.
    pub tremor: f32,
    /// Frames a gesture's poses are held (its speed), at most.
    pub slowest: usize,
    /// The hand's turn about its own axes, radians, at most.
    pub turn: f32,
}

impl Default for Noise {
    fn default() -> Self {
        // About what headset hand tracking shows at rest: a few millimetres.
        Noise { jitter: 0.002, tremor: 0.002, slowest: 4, turn: 0.35 }
    }
}

fn disturb(rng: &mut Rng, hand: [Vec3; JOINTS], noise: &Noise, scale: f32, rotation: Quat) -> [Vec3; JOINTS] {
    let tremor = Vec3::new(rng.normal(), rng.normal(), rng.normal()) * noise.tremor;
    hand.map(|j| rotation * (j * scale) + tremor + Vec3::new(rng.normal(), rng.normal(), rng.normal()) * noise.jitter)
}

/// A gesture's frames slowed (each pose held 1..`slowest` frames) and
/// disturbed. The gesture's own palm turn is kept; `turn` varies it.
fn varied(rng: &mut Rng, frames: &Frames, noise: &Noise) -> Frames {
    let scale = rng.range(0.9, 1.1);
    let rotation = Quat::from_euler(glam::EulerRot::XYZ, rng.range(-noise.turn, noise.turn) * 0.5, rng.range(-noise.turn, noise.turn), rng.range(-noise.turn, noise.turn) * 0.5);
    let mut out = Vec::new();
    for hands in frames {
        let hold = 1 + (rng.next() * noise.slowest as f32) as usize;
        for _ in 0..hold.min(noise.slowest) {
            out.push(hands.map(|h| h.map(|h| disturb(rng, h, noise, scale, rotation))));
        }
    }
    out
}

/// Idle play: relaxed, half-curled and pointing hands, reaching about,
/// turning, and fists that close from half-open hands slowly enough to be
/// a resting fist never opened (no gesture should fire).
fn idle(rng: &mut Rng, frames: usize, noise: &Noise) -> Frames {
    let mut out = Vec::new();
    let mut curl = [0.3f32; 4];
    let mut drift = Vec3::ZERO;
    let mut yaw = 0.0f32;
    while out.len() < frames {
        // A new target pose every second or so, approached gradually.
        let target: [f32; 4] = std::array::from_fn(|_| rng.range(0.1, 0.6));
        let reach = Vec3::new(rng.range(-0.15, 0.15), rng.range(-0.1, 0.1), rng.range(-0.1, 0.1));
        let turn = rng.range(-0.6, 0.6);
        for step in 0..60 {
            let t = step as f32 / 60.0;
            for f in 0..4 {
                curl[f] += (target[f] - curl[f]) * 0.05;
            }
            drift += (reach - drift) * 0.03;
            yaw += (turn - yaw) * 0.03;
            let hand = synthetic::right_hand(curl, synthetic::THUMB_REST);
            let rotation = Quat::from_rotation_y(yaw) * Quat::from_rotation_z(0.3 * (t * 6.0).sin());
            let right = synthetic::place(disturb(rng, hand, noise, 1.0, rotation), Quat::IDENTITY, Vec3::new(0.12, 0.0, 0.0) + drift);
            let left = synthetic::place(synthetic::left(disturb(rng, synthetic::right_hand([0.3; 4], synthetic::THUMB_REST), noise, 1.0, Quat::IDENTITY)), Quat::IDENTITY, Vec3::new(-0.12, 0.0, 0.0));
            out.push([Some(left), Some(right)]);
        }
    }
    out.truncate(frames);
    out
}

/// One gesture's measure: how often it was recognised as itself, missed,
/// or taken for another, and how many frames after its last pose arrived.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Measure {
    pub name: &'static str,
    pub tries: usize,
    pub recognised: usize,
    pub missed: usize,
    pub wrong: usize,
    /// Mean frames from the gesture's completing pose to its event.
    pub latency: f32,
}

fn matches(name: &str, gesture: &Gesture) -> bool {
    match (name, gesture) {
        ("pause", Gesture::Pause { .. }) | ("inspect", Gesture::Inspect { .. }) | ("pinch", Gesture::Pinch { .. }) | ("tap", Gesture::Tap { .. }) | ("prayer", Gesture::Settings) => true,
        ("swipe-up", Gesture::Swipe { direction: Swipe::Up, .. }) | ("swipe-down", Gesture::Swipe { direction: Swipe::Down, .. }) => true,
        ("swipe-left", Gesture::Swipe { direction: Swipe::Left, .. }) | ("swipe-right", Gesture::Swipe { direction: Swipe::Right, .. }) => true,
        _ => false,
    }
}

/// The completing pose of each gesture (where its event is due): the frame
/// index of the last new pose in the unslowed sequence.
fn completing(name: &str) -> usize {
    match name {
        "pause" | "inspect" => 3,
        "pinch" => 2,
        "tap" => 4,
        "prayer" => 1,
        _ => 4,
    }
}

fn open_hands() -> [Option<[Vec3; JOINTS]>; 2] {
    synthetic::gesture("pause").expect("known")[0]
}

/// Runs `tries` of a gesture, each from open hands at rest.
pub fn measure(name: &'static str, tries: usize, noise: &Noise, seed: u64) -> Measure {
    let mut rng = Rng::new(seed);
    let base = synthetic::gesture(name).expect("a known gesture");
    let mut measure = Measure { name, tries, ..Measure::default() };
    let mut latency = 0usize;
    for _ in 0..tries {
        let mut recognizer = Recognizer::default();
        let mut time = 0.0;
        let mut frame = |recognizer: &mut Recognizer, hands: [Option<[Vec3; JOINTS]>; 2]| {
            time += 1.0 / 72.0;
            recognizer.update(&Frame { time, hands, view_right: Vec3::X, ..Frame::default() })
        };
        let rest = varied(&mut rng, &vec![open_hands(); 4], noise);
        for hands in rest {
            frame(&mut recognizer, hands);
        }
        // Each pose held a varied number of frames; where the completing
        // pose begins.
        let mut frames = Vec::new();
        let mut due = 0;
        let scale = rng.range(0.9, 1.1);
        let rotation = Quat::from_rotation_y(rng.range(-noise.turn, noise.turn));
        for (i, hands) in base.iter().enumerate() {
            if i == completing(name) {
                due = frames.len();
            }
            let hold = 1 + (rng.next() * noise.slowest as f32) as usize;
            for _ in 0..hold.min(noise.slowest) {
                frames.push(hands.map(|h| h.map(|h| disturb(&mut rng, h, noise, scale, rotation))));
            }
        }
        let mut seen = None;
        let mut wrong = false;
        for (i, hands) in frames.into_iter().chain(varied(&mut rng, &vec![open_hands(); 6], noise)).enumerate() {
            for gesture in frame(&mut recognizer, hands) {
                if matches(name, &gesture) && seen.is_none() {
                    seen = Some(i.saturating_sub(due));
                } else if !matches(name, &gesture) && !matches!(gesture, Gesture::TrackingLost { .. }) {
                    wrong = true;
                }
            }
        }
        match seen {
            Some(frames) => {
                measure.recognised += 1;
                latency += frames;
            }
            None => measure.missed += 1,
        }
        if wrong {
            measure.wrong += 1;
        }
    }
    measure.latency = latency as f32 / measure.recognised.max(1) as f32;
    measure
}

/// Gestures fired over `minutes` of idle play at 72 frames a second.
pub fn false_activations(minutes: f32, noise: &Noise, seed: u64) -> Vec<Gesture> {
    let mut rng = Rng::new(seed);
    let frames = (minutes * 60.0 * 72.0) as usize;
    let mut recognizer = Recognizer::default();
    let mut fired = Vec::new();
    for (i, hands) in idle(&mut rng, frames, noise).into_iter().enumerate() {
        fired.extend(recognizer.update(&Frame { time: i as f64 / 72.0, hands, view_right: Vec3::X, ..Frame::default() }));
    }
    fired
}

/// Pause grabs `count` times in quick succession (each closing and
/// reopening within `frames` frames): the toggles recognised.
pub fn rapid_pauses(count: usize, frames: usize, noise: &Noise, seed: u64) -> usize {
    let mut rng = Rng::new(seed);
    let mut recognizer = Recognizer::default();
    let mut toggles = 0;
    let mut time = 0.0;
    let cycle = [open_hands(), synthetic::gesture("pause").expect("known")[2], synthetic::gesture("pause").expect("known")[3]];
    for _ in 0..count {
        for (k, hands) in cycle.iter().enumerate() {
            let hold = if k == 0 { frames.saturating_sub(2).max(1) } else { 1 };
            for _ in 0..hold {
                time += 1.0 / 72.0;
                let hands = hands.map(|h| h.map(|h| disturb(&mut rng, h, noise, 1.0, Quat::IDENTITY)));
                toggles += recognizer.update(&Frame { time, hands, view_right: Vec3::X, ..Frame::default() })
                    .iter().filter(|g| matches!(g, Gesture::Pause { .. })).count();
            }
        }
    }
    toggles
}

/// Tracking lost in the middle of each of `count` grabs, the hand coming
/// back closed: toggles fired on reacquisition (none are due).
pub fn reacquired_fists(count: usize, noise: &Noise, seed: u64) -> usize {
    let mut rng = Rng::new(seed);
    let mut recognizer = Recognizer::default();
    let mut fired = 0;
    let mut time = 0.0;
    let pause = synthetic::gesture("pause").expect("known");
    let sequence = [pause[0], pause[2], [pause[0][0], None], [pause[0][0], None], pause[3], pause[3], pause[0]];
    for _ in 0..count {
        for hands in sequence {
            time += 1.0 / 72.0;
            let hands = hands.map(|h| h.map(|h| disturb(&mut rng, h, noise, 1.0, Quat::IDENTITY)));
            fired += recognizer.update(&Frame { time, hands, view_right: Vec3::X, ..Frame::default() })
                .iter().filter(|g| matches!(g, Gesture::Pause { .. } | Gesture::Inspect { .. })).count();
        }
    }
    fired
}

pub const GESTURES: [&str; 9] = ["pause", "inspect", "pinch", "tap", "swipe-up", "swipe-down", "swipe-left", "swipe-right", "prayer"];

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn gestures_are_recognised_promptly_under_tracking_noise() {
        let noise = Noise::default();
        for (i, name) in GESTURES.into_iter().enumerate() {
            let m = measure(name, 200, &noise, 11 + i as u64);
            assert!(m.recognised >= 190, "{m:?}");
            assert!(m.wrong <= 10, "{m:?}");
            assert!(m.latency <= 4.0, "{m:?}");
        }
    }

    #[test]
    fn idle_hands_fire_no_toggle_and_rarely_anything() {
        let fired = false_activations(5.0, &Noise::default(), 7);
        let toggles = fired.iter().filter(|g| matches!(g, Gesture::Pause { .. } | Gesture::Inspect { .. } | Gesture::Settings | Gesture::ScaleBegin)).count();
        assert_eq!(toggles, 0, "{fired:?}");
        assert!(fired.len() <= 5, "{} activations in five minutes: {fired:?}", fired.len());
    }

    #[test]
    fn rapid_pauses_toggle_each_time_and_reacquired_fists_never() {
        assert_eq!(rapid_pauses(40, 6, &Noise::default(), 3), 40);
        assert_eq!(reacquired_fists(40, &Noise::default(), 5), 0, "a hand found closed toggles nothing");
    }
}
