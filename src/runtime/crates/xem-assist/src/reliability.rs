//! Measures this crate's own gesture recognizers (the grabs, prayer hands
//! and two-hand scaling, spreading and contracting; the pinch, taps and
//! swipes are the runtime's standard ones, its to qualify) under
//! hand-tracking noise (plan.md:
//! false activations, missed gestures, response latency, rapid pause and
//! resume, tracking-loss recovery). Hands are the synthetic model with
//! per-joint jitter (a tracker's), slow drift and tremor, varied speed,
//! size and orientation; idle play is relaxed and half-curled hands,
//! reaching, turning and resting fists that were never opened. All of it
//! is seeded and repeatable. A headset's own trackers differ: these numbers
//! qualify the recognizers' margins, not a device.
//!
//! Sustained use is measured by its physical proxies, not by people: the
//! gestures made from a relaxed posture (hands low, wrists dropped and
//! turned in, either hand alone), and an hour's session whose tremor,
//! slowness and wrist droop grow as a tiring hand's would.
use crate::gesture::{Frame, Gesture, Hand, Recognizer, Swipe, JOINTS};
use crate::synthetic::{self, Hands};

/// Frames of hands.
type Frames = Vec<Hands>;

/// A scripted gesture's hands.
fn script(name: &str) -> Option<Frames> {
    synthetic::gesture(name).map(|steps| steps.iter().map(|s| s.hands).collect())
}
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
        // A scaling's events are all its own.
        ("scale-out" | "scale-in", Gesture::ScaleBegin | Gesture::Scale { .. } | Gesture::ScaleEnd) => true,
        _ => false,
    }
}

/// The completing pose of each gesture (where its event is due): the frame
/// index of the last new pose in the unslowed sequence (a scaling's
/// gathered hands, where it begins).
fn completing(name: &str) -> usize {
    match name {
        "pause" | "inspect" => 3,
        "pinch" => 2,
        "tap" => 4,
        "prayer" => 1,
        "scale-out" | "scale-in" => 2,
        _ => 4,
    }
}

/// Whether a measured gesture takes both hands (the others are made with
/// either hand alone).
pub fn two_handed(name: &str) -> bool {
    matches!(name, "prayer" | "scale-out" | "scale-in")
}

/// Whether a scaling did what it was made for, by the last factor it gave
/// and its end: spread to twice the hands' distance (or contracted to
/// half) and let go of the hands. Other gestures need nothing of it.
fn scaled_as_made(name: &str, factor: Option<f32>, ended: bool) -> bool {
    match name {
        "scale-out" => ended && factor.is_some_and(|f| f > 1.5),
        "scale-in" => ended && factor.is_some_and(|f| f < 0.67),
        _ => true,
    }
}

fn open_hands() -> [Option<[Vec3; JOINTS]>; 2] {
    script("pause").expect("known")[0]
}

/// How the hands are held during a measure: turned as a whole (`posture`,
/// about the point between them) and, with `alone`, only that hand
/// tracked (the left one mirroring the right's gesture).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Setup {
    pub posture: Quat,
    pub alone: Option<Hand>,
}

impl Default for Setup {
    fn default() -> Self {
        Setup { posture: Quat::IDENTITY, alone: None }
    }
}

impl Setup {
    /// Resting: wrists dropped 35 degrees and turned in 15, as forearms
    /// lying on the lap or the armrests hold them.
    pub fn relaxed(alone: Option<Hand>) -> Setup {
        Setup { posture: Quat::from_rotation_x(-0.6) * Quat::from_rotation_y(0.26), alone }
    }

    fn arrange(&self, hands: [Option<[Vec3; JOINTS]>; 2]) -> [Option<[Vec3; JOINTS]>; 2] {
        let hands = hands.map(|h| h.map(|h| h.map(|j| self.posture * j)));
        match self.alone {
            None => hands,
            Some(Hand::Right) => [None, hands[1]],
            Some(Hand::Left) => [hands[1].map(synthetic::left), None],
        }
    }
}

/// Runs `tries` of a gesture, each from open hands at rest.
pub fn measure(name: &'static str, tries: usize, noise: &Noise, seed: u64) -> Measure {
    measure_with(name, tries, noise, seed, &Setup::default())
}

/// [`measure`] with the hands held as `setup` says.
pub fn measure_with(name: &'static str, tries: usize, noise: &Noise, seed: u64, setup: &Setup) -> Measure {
    let mut rng = Rng::new(seed);
    let expected = name;
    let base = script(name).expect("a known gesture");
    let mut measure = Measure { name, tries, ..Measure::default() };
    let mut latency = 0usize;
    for _ in 0..tries {
        let mut recognizer = Recognizer::default();
        let mut time = 0.0;
        let mut frame = |recognizer: &mut Recognizer, hands: [Option<[Vec3; JOINTS]>; 2]| {
            time += 1.0 / 72.0;
            recognizer.update(&Frame { time, hands: setup.arrange(hands), view_right: Vec3::X, ..Frame::default() })
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
        // A scaling's last factor and its end.
        let mut factor = None;
        let mut ended = false;
        for (i, hands) in frames.into_iter().chain(varied(&mut rng, &vec![open_hands(); 6], noise)).enumerate() {
            for gesture in frame(&mut recognizer, hands) {
                match gesture {
                    Gesture::Scale { factor: f } => factor = Some(f),
                    Gesture::ScaleEnd => ended = true,
                    _ => {}
                }
                if matches(expected, &gesture) && seen.is_none() {
                    seen = Some(i.saturating_sub(due));
                } else if !matches(expected, &gesture) && !matches!(gesture, Gesture::TrackingLost { .. }) {
                    wrong = true;
                }
            }
        }
        // A scaling counts when it scaled as made and let go of the hands.
        if !scaled_as_made(name, factor, ended) {
            seen = None;
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
    false_activations_with(minutes, noise, seed, &Setup::default())
}

/// [`false_activations`] with the hands held as `setup` says.
pub fn false_activations_with(minutes: f32, noise: &Noise, seed: u64, setup: &Setup) -> Vec<Gesture> {
    let mut rng = Rng::new(seed);
    let frames = (minutes * 60.0 * 72.0) as usize;
    let mut recognizer = Recognizer::default();
    let mut fired = Vec::new();
    for (i, hands) in idle(&mut rng, frames, noise).into_iter().enumerate() {
        fired.extend(recognizer.update(&Frame { time: i as f64 / 72.0, hands: setup.arrange(hands), view_right: Vec3::X, ..Frame::default() }));
    }
    fired
}

/// A stretch of a sustained session: every gesture tried (one-hand ones
/// with the hands taking turns alone) and idle play, under that stretch's
/// fatigue.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Stretch {
    /// Minutes into the session at its start.
    pub from: f32,
    pub noise: (f32, f32, usize),
    /// The wrists' droop, degrees.
    pub droop: f32,
    pub tries: usize,
    pub recognised: usize,
    pub wrong: usize,
    pub latency: f32,
    pub idle_activations: usize,
    pub idle_toggles: usize,
}

/// A session of `stretches` stretches of `minutes` each: from rested hands
/// (2 mm tremor, poses held up to 4 frames, wrists dropped 10 degrees) to
/// tired ones (6 mm tremor, poses held up to 8 frames, wrists dropped 40
/// degrees), `tries` of each gesture a stretch. The tracker's per-joint
/// jitter stays its 2 mm: it is the device's, not the hand's.
pub fn sustained(stretches: usize, minutes: f32, tries: usize, seed: u64) -> Vec<Stretch> {
    (0..stretches).map(|k| {
        let f = k as f32 / (stretches.max(2) - 1) as f32;
        let noise = Noise { jitter: 0.002, tremor: 0.002 + 0.004 * f, slowest: 4 + (4.0 * f).round() as usize, turn: 0.35 };
        let droop = 10.0 + 30.0 * f;
        let posture = Quat::from_rotation_x(-droop.to_radians()) * Quat::from_rotation_y(0.26);
        let mut stretch = Stretch { from: k as f32 * minutes, noise: (noise.jitter, noise.tremor, noise.slowest), droop, ..Stretch::default() };
        let mut latency = 0.0;
        for (i, name) in GESTURES.into_iter().enumerate() {
            let alone = if two_handed(name) { None } else { Some(Hand::BOTH[(i + k) % 2]) };
            let m = measure_with(name, tries, &noise, seed + (k * 16 + i) as u64, &Setup { posture, alone });
            stretch.tries += m.tries;
            stretch.recognised += m.recognised;
            stretch.wrong += m.wrong;
            latency += m.latency * m.recognised as f32;
        }
        stretch.latency = latency / stretch.recognised.max(1) as f32;
        let fired = false_activations_with(minutes, &noise, seed + 1000 + k as u64, &Setup { posture, alone: None });
        stretch.idle_activations = fired.len();
        stretch.idle_toggles = fired.iter().filter(|g| matches!(g, Gesture::Pause { .. } | Gesture::Inspect { .. } | Gesture::Settings | Gesture::ScaleBegin)).count();
        stretch
    }).collect()
}

/// Pause grabs `count` times in quick succession (each closing and
/// reopening within `frames` frames): the toggles recognised.
pub fn rapid_pauses(count: usize, frames: usize, noise: &Noise, seed: u64) -> usize {
    let mut rng = Rng::new(seed);
    let mut recognizer = Recognizer::default();
    let mut toggles = 0;
    let mut time = 0.0;
    let pause = script("pause").expect("known");
    let cycle = [open_hands(), pause[2], pause[3]];
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
    let pause = script("pause").expect("known");
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

/// The gestures recognised here (the standard ones are the runtime's).
pub const GESTURES: [&str; 5] = ["pause", "inspect", "prayer", "scale-out", "scale-in"];

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
    fn a_relaxed_posture_works_with_either_hand_alone() {
        let noise = Noise::default();
        for alone in [Hand::Right, Hand::Left] {
            for (i, name) in GESTURES.into_iter().filter(|n| !two_handed(n)).enumerate() {
                let m = measure_with(name, 100, &noise, 31 + i as u64, &Setup::relaxed(Some(alone)));
                assert!(m.recognised >= 92, "{alone:?} {m:?}");
                assert!(m.latency <= 4.0, "{alone:?} {m:?}");
            }
        }
    }

    #[test]
    fn tiring_hands_keep_their_gestures_and_fire_no_toggles() {
        for stretch in sustained(3, 1.0, 40, 77) {
            assert!(stretch.recognised * 100 >= stretch.tries * 93, "{stretch:?}");
            assert_eq!(stretch.idle_toggles, 0, "{stretch:?}");
        }
    }

    #[test]
    fn rapid_pauses_toggle_each_time_and_reacquired_fists_never() {
        assert_eq!(rapid_pauses(40, 6, &Noise::default(), 3), 40);
        assert_eq!(reacquired_fists(40, &Noise::default(), 5), 0, "a hand found closed toggles nothing");
    }
}
