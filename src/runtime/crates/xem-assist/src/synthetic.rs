//! Synthetic hands for tests and emulator runs without hand tracking
//! (Monado's simulated headset has none): poses built from a simple hand
//! model, and each gesture's steps, placed in front of a head. A step also
//! carries what a runtime offering the standard gestures reports with it
//! (`XR_EXT_hand_interaction`'s pinch, `XR_META_hand_tracking_microgestures`'
//! taps and swipes): those are the runtime's, not derived from the joints.
use crate::gesture::{FINGERS, Frame, INDEX, INTERMEDIATE, JOINTS, LITTLE, MIDDLE, Micro, PROXIMAL, Swipe, THUMB_TIP, TIP};
use glam::{Quat, Vec3};

/// A right hand in its own frame: wrist at the origin, fingers along -Z,
/// thumb toward -X, palm facing -Y (down), each finger curled by
/// `curl` (0 straight, 1 folded) and the thumb's tip at `thumb`.
pub fn right_hand(curl: [f32; 4], thumb: Vec3) -> [Vec3; JOINTS] {
    let mut joints = [Vec3::ZERO; JOINTS];
    let lateral = [-0.035, -0.012, 0.01, 0.03];
    let bones = [[0.065, 0.04, 0.025, 0.022], [0.064, 0.045, 0.028, 0.024], [0.06, 0.042, 0.027, 0.024], [0.056, 0.034, 0.02, 0.02]];
    for (f, base) in FINGERS.into_iter().enumerate() {
        joints[base] = Vec3::new(lateral[f] * 0.6, 0.0, 0.0);
        // Each joint bends the next bone further down toward the palm.
        let mut angle = 0.0f32;
        let mut at = Vec3::new(lateral[f], 0.0, -bones[f][0]);
        joints[base + PROXIMAL] = at;
        for (k, length) in bones[f][1..].iter().enumerate() {
            angle += curl[f] * [1.4, 1.6, 1.2][k];
            at += Vec3::new(0.0, -angle.sin(), -angle.cos()) * *length;
            joints[base + PROXIMAL + k + 1] = at;
        }
    }
    joints[1] = Vec3::new(-0.02, -0.01, -0.02);
    joints[2] = Vec3::new(-0.04, -0.015, -0.04);
    joints[3] = (joints[2] + thumb) / 2.0;
    joints[THUMB_TIP] = thumb;
    joints
}

/// The thumb resting beside the index finger, off it.
pub const THUMB_REST: Vec3 = Vec3::new(-0.06, -0.01, -0.06);

pub fn place(joints: [Vec3; JOINTS], rotation: Quat, at: Vec3) -> [Vec3; JOINTS] {
    joints.map(|j| rotation * j + at)
}

/// The left hand mirroring a right one.
pub fn left(joints: [Vec3; JOINTS]) -> [Vec3; JOINTS] {
    joints.map(|j| Vec3::new(-j.x, j.y, j.z))
}

pub fn open() -> [Vec3; JOINTS] {
    right_hand([0.0; 4], THUMB_REST)
}

pub fn half() -> [Vec3; JOINTS] {
    right_hand([0.5; 4], THUMB_REST)
}

/// A fist: the thumb's tip wrapped over the middle finger's middle bone.
pub fn fist() -> [Vec3; JOINTS] {
    let mut hand = right_hand([1.0; 4], Vec3::ZERO);
    hand[THUMB_TIP] = hand[MIDDLE + INTERMEDIATE] + Vec3::new(-0.01, -0.012, 0.0);
    hand[3] = (hand[2] + hand[THUMB_TIP]) / 2.0;
    hand
}

/// Thumb and index tips together, the other fingers open.
pub fn pinch() -> [Vec3; JOINTS] {
    let mut hand = open();
    hand[THUMB_TIP] = hand[INDEX + TIP] + Vec3::new(0.004, 0.0, 0.0);
    hand
}

/// A relaxed hand, the thumb on its index finger's side at `along` (0
/// knuckle .. 1 the middle joint).
pub fn thumb_on_index(along: f32) -> [Vec3; JOINTS] {
    let mut hand = right_hand([0.25; 4], THUMB_REST);
    let (a, b) = (hand[INDEX + PROXIMAL], hand[INDEX + INTERMEDIATE]);
    hand[THUMB_TIP] = a + (b - a) * along + Vec3::new(-0.009, 0.0, 0.0);
    hand
}

/// The thumb lifted off its index finger's side at `along`: 3 cm out from it.
pub fn thumb_lifted(along: f32) -> [Vec3; JOINTS] {
    let mut hand = thumb_on_index(along);
    hand[THUMB_TIP] += Vec3::new(-0.03, -0.005, 0.0);
    hand
}

pub fn relaxed() -> [Vec3; JOINTS] {
    right_hand([0.25; 4], THUMB_REST)
}

/// All five fingertips gathered.
pub fn whole_pinch() -> [Vec3; JOINTS] {
    let mut hand = right_hand([0.45; 4], THUMB_REST);
    let centre = (hand[INDEX + TIP] + hand[LITTLE + TIP]) / 2.0;
    for tip in [THUMB_TIP, INDEX + TIP, MIDDLE + TIP, crate::gesture::RING + TIP, LITTLE + TIP] {
        hand[tip] = centre + (hand[tip] - centre) * 0.15;
    }
    hand
}

/// Left and right hands' joints, when tracked.
pub type Hands = [Option<[Vec3; JOINTS]>; 2];

/// One frame of a scripted gesture: the hands (in the hand model's frame)
/// and the runtime's standard signals with them.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Step {
    pub hands: Hands,
    /// The runtime's pinch, per hand.
    pub pinch: [bool; 2],
    /// The runtime's microgesture this frame, per hand.
    pub micro: [Option<Micro>; 2],
}

/// A gesture's steps.
pub type Frames = Vec<Step>;

/// What a runtime offers of the standard gestures (a Quest's Horizon OS
/// both; a browser's WebXR the pinch only).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Runtime {
    pub pinch: bool,
    pub microgestures: bool,
}

impl Runtime {
    pub const HORIZON: Runtime = Runtime { pinch: true, microgestures: true };
    pub const WEBXR: Runtime = Runtime { pinch: true, microgestures: false };
}

impl Step {
    /// Hands alone, no signal.
    pub fn of(hands: Hands) -> Step {
        Step { hands, ..Step::default() }
    }

    /// The recognizer's frame for this step at `time` with `hands` (the
    /// step's, placed), as `runtime` reports it.
    pub fn frame(&self, time: f64, hands: Hands, view_right: Vec3, runtime: Runtime) -> Frame {
        Frame {
            time,
            hands,
            view_right,
            pinch: [0, 1].map(|i| (runtime.pinch && hands[i].is_some()).then_some(self.pinch[i])),
            micro: if runtime.microgestures { self.micro } else { [None; 2] },
            micro_capable: [0, 1].map(|i| runtime.microgestures && hands[i].is_some()),
        }
    }

    /// The left hand doing the right's step (mirrored), the right absent.
    pub fn mirrored(&self) -> Step {
        Step { hands: [self.hands[1].map(left), None], pinch: [self.pinch[1], false], micro: [self.micro[1].map(|m| match m {
            Micro::Swipe(Swipe::Left) => Micro::Swipe(Swipe::Right),
            Micro::Swipe(Swipe::Right) => Micro::Swipe(Swipe::Left),
            other => other,
        }), None] }
    }
}

/// The steps of a named gesture with the right hand (the left open):
/// `pause` (an overhand grab), `inspect` (underhand), `pinch`, `tap`,
/// `swipe-up`, `swipe-down`, `swipe-left`, `swipe-right`; `prayer` with
/// both, `lose` (both hands untracked for a while). The grabs' other cases:
/// `pause-left` and `inspect-left` (the left hand's, the right resting
/// open), `pause-both` (both hands closing together in the same frames:
/// one toggle), `grab-sideways` (the palm sideways: neither), `fist-turn`
/// (a fist closed sideways, a handshake's grip, turned palm down, back,
/// palm up and back before it opens: nothing) and `fist-reacquire` (the
/// right hand lost while it closes and found again closed: its tracking
/// lost, no grab). Two-hand scaling: `scale-out` (two whole-hand pinches
/// spreading from 15 to 30 cm apart, then let go), `scale-in` (contracting
/// from 30 to 15 cm) and `scale-pinch` (`scale-out` with the runtime's
/// pinch reported on both hands from the gather until a frame after they
/// let go, as a runtime may take gathered fingertips for one: nothing but
/// the scale).
pub fn gesture(name: &str) -> Option<Frames> {
    // The left hand rests open beside the right's gestures.
    let rest_left = || Some(place(left(open()), Quat::IDENTITY, Vec3::new(-0.12, 0.0, 0.0)));
    let place_right = |p: [Vec3; JOINTS]| Some(place(p, Quat::IDENTITY, Vec3::new(0.12, 0.0, 0.0)));
    let place_both = |p: [Vec3; JOINTS]| -> Hands { [rest_left(), place_right(p)] };
    let right = |poses: &[[Vec3; JOINTS]]| -> Frames { poses.iter().map(|p| Step::of(place_both(*p))).collect() };
    // The left hand doing the right's poses (mirrored), the right resting open.
    let left_hand = |poses: &[[Vec3; JOINTS]]| -> Frames {
        poses.iter().map(|p| Step::of([place_right(*p).map(left), place_right(open())])).collect()
    };
    // A thumb gesture's poses, the runtime's event on the lifting one.
    let micro = |poses: &[[Vec3; JOINTS]], at: usize, event: Micro| -> Frames {
        poses.iter().enumerate().map(|(i, p)| Step { hands: place_both(*p), pinch: [false; 2], micro: [None, (i == at).then_some(event)] }).collect()
    };
    let up = Quat::from_rotation_z(std::f32::consts::PI);
    let roll = |degrees: f32, p: [Vec3; JOINTS]| place(p, Quat::from_rotation_z(degrees.to_radians()), Vec3::ZERO);
    let grab = [open(), open(), half(), fist(), fist(), open(), open()];
    let underhand = [open(), open(), half(), fist(), fist(), open()].map(|p| place(p, up, Vec3::new(0.0, 0.05, 0.0)));
    // Both hands `apart` metres between their wrists, each in its pose.
    let apart = |distance: f32, l: [Vec3; JOINTS], r: [Vec3; JOINTS]| -> Hands {
        [Some(place(left(l), Quat::IDENTITY, Vec3::new(-distance / 2.0, 0.0, 0.0))), Some(place(r, Quat::IDENTITY, Vec3::new(distance / 2.0, 0.0, 0.0)))]
    };
    // Two whole-hand pinches gathered `from` apart, moved to `to` in five
    // steps and let go there (the hands opening), with `pinched` the
    // runtime's pinch on both hands from the gather to a frame after.
    let scale = |from: f32, to: f32, pinched: bool| -> Frames {
        let gathered = (0..=5).map(|i| from + (to - from) * i as f32 / 5.0);
        std::iter::repeat_n(apart(from, open(), open()), 2)
            .chain(std::iter::once(apart(from, whole_pinch(), whole_pinch())))
            .chain(gathered.map(|d| apart(d, whole_pinch(), whole_pinch())))
            .chain(std::iter::once(apart(to, whole_pinch(), whole_pinch())))
            .chain(std::iter::repeat_n(apart(to, open(), open()), 2))
            .enumerate()
            .map(|(i, hands)| Step { hands, pinch: [pinched && (2..=10).contains(&i); 2], micro: [None; 2] })
            .collect()
    };
    Some(match name {
        "pause" => right(&grab),
        "inspect" => right(&underhand),
        "pause-left" => left_hand(&grab),
        "inspect-left" => left_hand(&underhand),
        "pause-both" => grab.iter().map(|p| Step::of([place_right(*p).map(left), place_right(*p)])).collect(),
        "grab-sideways" => right(&grab.map(|p| roll(90.0, p))),
        "fist-turn" => right(&[open(), roll(90.0, half()), roll(90.0, fist()), roll(45.0, fist()), fist(), fist(), roll(45.0, fist()),
            roll(90.0, fist()), roll(135.0, fist()), roll(180.0, fist()), roll(180.0, fist()), roll(135.0, fist()), roll(90.0, fist()),
            roll(90.0, open()), open()]),
        "fist-reacquire" => [place_both(open()), place_both(half()), [rest_left(), None], [rest_left(), None], [rest_left(), None],
            place_both(fist()), place_both(fist()), place_both(fist()), place_both(open()), place_both(open())].map(Step::of).to_vec(),
        "scale-out" => scale(0.15, 0.3, false),
        "scale-in" => scale(0.3, 0.15, false),
        "scale-pinch" => scale(0.15, 0.3, true),
        "pinch" => [open(), open(), pinch(), pinch(), pinch(), open(), open()].iter().enumerate()
            .map(|(i, p)| Step { hands: place_both(*p), pinch: [false, (2..5).contains(&i)], micro: [None; 2] }).collect(),
        "tap" => micro(&[relaxed(), relaxed(), thumb_on_index(0.5), thumb_on_index(0.5), thumb_lifted(0.5), relaxed(), relaxed()], 4, Micro::Tap),
        "swipe-up" => micro(&[relaxed(), thumb_on_index(0.1), thumb_on_index(0.5), thumb_on_index(0.9), thumb_lifted(0.9), relaxed()], 4, Micro::Swipe(Swipe::Up)),
        "swipe-down" => micro(&[relaxed(), thumb_on_index(0.9), thumb_on_index(0.5), thumb_on_index(0.1), thumb_lifted(0.1), relaxed()], 4, Micro::Swipe(Swipe::Down)),
        "swipe-left" => micro(&[relaxed(), thumb_on_index(0.5), thumb_on_index(0.5), thumb_lifted(0.5), relaxed()], 3, Micro::Swipe(Swipe::Left)),
        "swipe-right" => micro(&[relaxed(), thumb_on_index(0.5), thumb_on_index(0.5), thumb_lifted(0.5), relaxed()], 3, Micro::Swipe(Swipe::Right)),
        "prayer" => {
            let r = place(open(), Quat::from_rotation_z(-std::f32::consts::FRAC_PI_2), Vec3::new(0.02, 0.0, 0.0));
            let l = place(left(open()), Quat::from_rotation_z(std::f32::consts::FRAC_PI_2), Vec3::new(-0.02, 0.0, 0.0));
            let apart = |x: f32, h: [Vec3; JOINTS]| place(h, Quat::IDENTITY, Vec3::new(x, 0.0, 0.0));
            [[Some(apart(-0.2, l)), Some(apart(0.2, r))], [Some(l), Some(r)], [Some(l), Some(r)], [Some(apart(-0.2, l)), Some(apart(0.2, r))], [Some(apart(-0.2, l)), Some(apart(0.2, r))]].map(Step::of).to_vec()
        }
        "lose" => std::iter::repeat_n(Step::default(), 30).collect(),
        _ => return None,
    })
}

/// A frame's hands (placed about a point between them) in front of a head
/// at `position` facing `orientation`: 25 cm ahead and 20 cm down.
pub fn in_front(frame: [Option<[Vec3; JOINTS]>; 2], position: Vec3, orientation: Quat) -> [Option<[Vec3; JOINTS]>; 2] {
    let at = position + orientation * Vec3::new(0.0, -0.2, -0.25);
    frame.map(|hand| hand.map(|hand| place(hand, orientation, at)))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::gesture::{Gesture, Hand, Recognizer, Swipe};

    fn run(steps: &[Step], head: (Vec3, Quat), runtime: Runtime) -> Vec<Gesture> {
        let mut recognizer = Recognizer::default();
        steps.iter().enumerate().flat_map(|(i, step)| {
            let hands = in_front(step.hands, head.0, head.1);
            recognizer.update(&step.frame(i as f64 / 72.0, hands, head.1 * Vec3::X, runtime))
        }).collect()
    }

    fn opened() -> Vec<Step> {
        // Hands first seen open (so grabs are armed).
        vec![Step::of([Some(place(left(open()), Quat::IDENTITY, Vec3::new(-0.12, 0.0, 0.0))), Some(place(open(), Quat::IDENTITY, Vec3::new(0.12, 0.0, 0.0)))]); 3]
    }

    #[test]
    fn every_scripted_gesture_reads_as_itself_in_front_of_a_turned_head() {
        let head = (Vec3::new(0.3, 1.6, -0.2), Quat::from_rotation_y(0.7) * Quat::from_rotation_x(-0.2));
        let right = Hand::Right;
        for (name, expected) in [
            ("pause", Gesture::Pause { hand: right }),
            ("inspect", Gesture::Inspect { hand: right }),
            ("pinch", Gesture::Pinch { hand: right }),
            ("tap", Gesture::Tap { hand: right }),
            ("swipe-up", Gesture::Swipe { hand: right, direction: Swipe::Up }),
            ("swipe-down", Gesture::Swipe { hand: right, direction: Swipe::Down }),
            ("swipe-left", Gesture::Swipe { hand: right, direction: Swipe::Left }),
            ("swipe-right", Gesture::Swipe { hand: right, direction: Swipe::Right }),
            ("prayer", Gesture::Settings),
        ] {
            let steps: Vec<Step> = opened().into_iter().chain(gesture(name).unwrap()).collect();
            assert_eq!(run(&steps, head, Runtime::HORIZON), [expected], "{name}");
        }
    }

    #[test]
    fn the_grabs_other_hands_and_turns_and_the_scaling_scripts_read_as_the_contract_says() {
        let head = (Vec3::new(-0.2, 1.5, 0.3), Quat::from_rotation_y(-0.9) * Quat::from_rotation_x(-0.3));
        let seen = |name: &str| {
            let steps: Vec<Step> = opened().into_iter().chain(gesture(name).unwrap()).collect();
            run(&steps, head, Runtime::HORIZON)
        };
        assert_eq!(seen("pause-left"), [Gesture::Pause { hand: Hand::Left }]);
        assert_eq!(seen("inspect-left"), [Gesture::Inspect { hand: Hand::Left }]);
        let both = seen("pause-both");
        assert!(matches!(both[..], [Gesture::Pause { .. }]), "both hands closing together toggle once: {both:?}");
        assert_eq!(seen("grab-sideways"), [], "a sideways grab is neither");
        assert_eq!(seen("fist-turn"), [], "a closed fist turning through palm down and up fires nothing");
        assert_eq!(seen("fist-reacquire"), [Gesture::TrackingLost { hand: Hand::Right }], "a hand found closed fires no grab");
        for (name, reached) in [("scale-out", 2.0), ("scale-in", 0.5), ("scale-pinch", 2.0)] {
            let gestures = seen(name);
            let factors: Vec<f32> = gestures.iter().filter_map(|g| match g { Gesture::Scale { factor } => Some(*factor), _ => None }).collect();
            assert_eq!(gestures.first(), Some(&Gesture::ScaleBegin), "{name}: {gestures:?}");
            assert_eq!(gestures.last(), Some(&Gesture::ScaleEnd), "{name}: {gestures:?}");
            assert_eq!(gestures.len(), factors.len() + 2, "{name}: nothing but the scale (the runtime's pinch included): {gestures:?}");
            let last = *factors.last().unwrap();
            assert!((last / reached - 1.0).abs() < 0.05, "{name}: scaled by {last}");
        }
    }

    #[test]
    fn a_runtime_without_microgestures_has_no_taps_or_swipes() {
        let head = (Vec3::new(0.0, 1.6, 0.0), Quat::IDENTITY);
        for name in ["tap", "swipe-up", "swipe-down", "swipe-left", "swipe-right"] {
            let steps: Vec<Step> = opened().into_iter().chain(gesture(name).unwrap()).collect();
            assert!(run(&steps, head, Runtime::WEBXR).is_empty(), "{name}");
        }
        let steps: Vec<Step> = opened().into_iter().chain(gesture("pinch").unwrap()).collect();
        assert_eq!(run(&steps, head, Runtime::WEBXR), [Gesture::Pinch { hand: Hand::Right }], "WebXR's select is the pinch");
    }
}

#[cfg(test)]
mod one_hand {
    use super::*;
    use crate::gesture::{Gesture, Hand, Recognizer, Swipe};

    /// Each hand alone, the other absent from tracking: every one-hand
    /// gesture reads as itself, with the right hand's and (mirrored) the
    /// left's.
    #[test]
    fn every_one_hand_gesture_works_with_either_hand_alone() {
        for name in ["pause", "inspect", "pinch", "tap", "swipe-up", "swipe-down", "swipe-left", "swipe-right"] {
            for hand in [Hand::Right, Hand::Left] {
                let mut recognizer = Recognizer::default();
                let steps: Vec<Step> = std::iter::repeat_n(Step::of([None, Some(open())]), 3).chain(gesture(name).unwrap()).collect();
                let seen: Vec<Gesture> = steps.iter().enumerate().flat_map(|(i, step)| {
                    let step = match hand {
                        Hand::Right => Step { hands: [None, step.hands[1]], ..*step },
                        Hand::Left => step.mirrored(),
                    };
                    recognizer.update(&step.frame(i as f64 / 72.0, step.hands, Vec3::X, Runtime::HORIZON))
                }).collect();
                assert_eq!(seen.len(), 1, "{name} with {hand:?}: {seen:?}");
                let ok = match seen[0] {
                    Gesture::Pause { hand: h } | Gesture::Inspect { hand: h } | Gesture::Pinch { hand: h } | Gesture::Tap { hand: h } => h == hand,
                    // The left hand's mirrored swipe across goes the other way.
                    Gesture::Swipe { hand: h, direction } => h == hand && match (name, hand) {
                        ("swipe-left", Hand::Left) => direction == Swipe::Right,
                        ("swipe-right", Hand::Left) => direction == Swipe::Left,
                        _ => true,
                    },
                    _ => false,
                };
                assert!(ok, "{name} with {hand:?}: {seen:?}");
            }
        }
    }
}
