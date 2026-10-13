//! Synthetic hands for tests and emulator runs without hand tracking
//! (Monado's simulated headset has none): poses built from a simple hand
//! model, and each gesture's frames, placed in front of a head.
use crate::gesture::{FINGERS, INDEX, INTERMEDIATE, JOINTS, LITTLE, MIDDLE, PROXIMAL, THUMB_TIP, TIP};
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

/// A gesture's frames (left, right hands in the hand model's frame).
pub type Frames = Vec<[Option<[Vec3; JOINTS]>; 2]>;

/// The frames of a named gesture with the right hand (the left open): `pause` (an
/// overhand grab), `inspect` (underhand), `pinch`, `tap`, `swipe-up`,
/// `swipe-down`, `swipe-left`, `swipe-right`; `prayer` with both, `lose`
/// (the right hand untracked for a while).
pub fn gesture(name: &str) -> Option<Frames> {
    // The left hand rests open beside the right's gestures.
    let right = |poses: &[[Vec3; JOINTS]]| -> Frames {
        poses.iter().map(|p| [Some(place(left(open()), Quat::IDENTITY, Vec3::new(-0.12, 0.0, 0.0))), Some(place(*p, Quat::IDENTITY, Vec3::new(0.12, 0.0, 0.0)))]).collect()
    };
    let up = Quat::from_rotation_z(std::f32::consts::PI);
    Some(match name {
        "pause" => right(&[open(), open(), half(), fist(), fist(), open(), open()]),
        "inspect" => right(&[open(), open(), half(), fist(), fist(), open()].map(|p| place(p, up, Vec3::new(0.0, 0.05, 0.0)))),
        "pinch" => right(&[open(), open(), pinch(), pinch(), pinch(), open(), open()]),
        "tap" => right(&[relaxed(), relaxed(), thumb_on_index(0.5), thumb_on_index(0.5), thumb_lifted(0.5), relaxed(), relaxed()]),
        // Swipes travel over a few frames, as a thumb does (80 ms and more).
        "swipe-up" => right(&[relaxed(), thumb_on_index(0.1), thumb_on_index(0.37), thumb_on_index(0.63), thumb_on_index(0.9), thumb_lifted(0.9), relaxed(), relaxed()]),
        "swipe-down" => right(&[relaxed(), thumb_on_index(0.9), thumb_on_index(0.63), thumb_on_index(0.37), thumb_on_index(0.1), thumb_lifted(0.1), relaxed(), relaxed()]),
        "swipe-left" | "swipe-right" => {
            // Across the finger's side: toward the back of the hand (+Y for
            // the palm-down model) is Right for the right hand.
            // Across the finger at its middle joint (perpendicular to its
            // axis there, toward the back of the hand for Right), 2 cm,
            // staying in contact.
            let on = thumb_on_index(0.5);
            let (a, b, c) = (on[INDEX + PROXIMAL], on[INDEX + INTERMEDIATE], on[INDEX + 3]);
            let axis = ((b - a).normalize() + (c - b).normalize()).normalize();
            let back = (Vec3::Y - axis * axis.y).normalize();
            let side = if name == "swipe-left" { -0.02 } else { 0.02 };
            let step = |t: f32| {
                let mut moved = on;
                moved[THUMB_TIP] += (back * side + Vec3::new(0.004, 0.0, 0.0)) * t;
                moved
            };
            let mut lifted = step(1.0);
            lifted[THUMB_TIP] += Vec3::new(-0.03, 0.0, 0.0);
            right(&[relaxed(), on, step(0.33), step(0.67), step(1.0), lifted, relaxed(), relaxed()])
        }
        "prayer" => {
            let r = place(open(), Quat::from_rotation_z(-std::f32::consts::FRAC_PI_2), Vec3::new(0.02, 0.0, 0.0));
            let l = place(left(open()), Quat::from_rotation_z(std::f32::consts::FRAC_PI_2), Vec3::new(-0.02, 0.0, 0.0));
            let apart = |x: f32, h: [Vec3; JOINTS]| place(h, Quat::IDENTITY, Vec3::new(x, 0.0, 0.0));
            vec![[Some(apart(-0.2, l)), Some(apart(0.2, r))], [Some(l), Some(r)], [Some(l), Some(r)], [Some(apart(-0.2, l)), Some(apart(0.2, r))], [Some(apart(-0.2, l)), Some(apart(0.2, r))]]
        }
        "lose" => std::iter::repeat_n([None, None], 30).collect(),
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
    use crate::gesture::{Frame, Gesture, Hand, Recognizer, Swipe};

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
            let mut recognizer = Recognizer::default();
            let mut seen = Vec::new();
            // Hands first seen open (so grabs are armed), then the gesture.
            let mut frames = vec![[Some(place(left(open()), Quat::IDENTITY, Vec3::new(-0.12, 0.0, 0.0))), Some(place(open(), Quat::IDENTITY, Vec3::new(0.12, 0.0, 0.0)))]; 3];
            frames.extend(gesture(name).unwrap());
            for (i, hands) in frames.into_iter().enumerate() {
                let hands = in_front(hands, head.0, head.1);
                seen.extend(recognizer.update(&Frame { time: i as f64 / 72.0, hands, view_right: head.1 * Vec3::X, ..Frame::default() }));
            }
            assert_eq!(seen, [expected], "{name}");
        }
    }
}
