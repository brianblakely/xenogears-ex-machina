//! Hand gestures from tracked hand joints (plan.md, the shared headset
//! hand/gaze contract): the same recognizers serve OpenXR
//! (XR_EXT_hand_tracking) and WebXR hand input, both reduced to WebXR's 25
//! joints in room space (metres, +Y up).
//!
//! - A **pinch** joins the thumb and index fingertips, the other fingers
//!   open.
//! - A **thumb tap** touches the radial side of the index finger and lifts
//!   again without travelling; a **thumb swipe** travels along it (toward
//!   the fingertip Up, back Down) or across it (toward the back of the
//!   hand Right for the right hand, Left for the left: as the player sees a
//!   hand held sideways). A runtime's own microgestures replace these where offered.
//! - A **five-finger grab** curls the four fingers into the palm with the
//!   thumb closing around them: palm down (overhand) toggles Pause, palm up
//!   (underhand) first-person inspection, sideways neither.
//! - A **whole-hand pinch** gathers all five fingertips together, away from
//!   the palm; two of them spreading or contracting scale the diorama.
//! - **Prayer hands** bring open, aligned palms together: app settings.
//!
//! Grabs fire on closure from a fresh open hand, with the palm's
//! orientation classified as closing begins and kept through closure; no
//! hold, dwell or cooldown: reopening rearms. A fist already closed (at
//! rest, rotated, or reacquired after tracking loss) fires nothing.
//! Simultaneous matching grabs of both hands coalesce into one toggle.
//! Arbitration: a hand closing into a grab emits no pinch or thumb event;
//! two-hand scaling owns both hands until both have let go.
use glam::Vec3;
use serde::Serialize;

/// WebXR's joint order (XRHand): the wrist, the thumb's metacarpal,
/// proximal, distal and tip, then each finger's metacarpal, proximal,
/// intermediate, distal and tip (index, middle, ring, little).
pub const JOINTS: usize = 25;
pub const WRIST: usize = 0;
pub const THUMB_TIP: usize = 4;
/// The index finger's joints.
pub const INDEX: usize = 5;
pub const MIDDLE: usize = 10;
pub const RING: usize = 15;
pub const LITTLE: usize = 20;
/// Each finger's joints from its first: metacarpal, proximal,
/// intermediate, distal, tip.
pub const METACARPAL: usize = 0;
pub const PROXIMAL: usize = 1;
pub const INTERMEDIATE: usize = 2;
pub const DISTAL: usize = 3;
pub const TIP: usize = 4;
pub const FINGERS: [usize; 4] = [INDEX, MIDDLE, RING, LITTLE];

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
pub enum Hand {
    Left,
    Right,
}

impl Hand {
    pub const BOTH: [Hand; 2] = [Hand::Left, Hand::Right];
    pub fn index(self) -> usize {
        self as usize
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
pub enum Swipe {
    Up,
    Down,
    Left,
    Right,
}

/// The palm's facing relative to the room's vertical.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
pub enum Palm {
    Down,
    Up,
    Sideways,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize)]
#[serde(tag = "gesture")]
pub enum Gesture {
    /// Overhanded grab: toggle gameplay Pause.
    Pause { hand: Hand },
    /// Underhanded grab: toggle first-person inspection.
    Inspect { hand: Hand },
    /// Prayer hands: toggle app settings.
    Settings,
    /// Two whole-hand pinches: scaling began, the factor since it began,
    /// and its end.
    ScaleBegin,
    Scale { factor: f32 },
    ScaleEnd,
    Pinch { hand: Hand },
    Tap { hand: Hand },
    Swipe { hand: Hand, direction: Swipe },
    /// A hand that was tracked is no longer.
    TrackingLost { hand: Hand },
}

/// A runtime's own thumb microgesture (XR_META_hand_tracking_microgestures).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Micro {
    Tap,
    Swipe(Swipe),
}

/// One frame's hand input.
#[derive(Clone, Debug, Default)]
pub struct Frame {
    /// Seconds, increasing.
    pub time: f64,
    /// Left and right hands' joints in room space, when tracked.
    pub hands: [Option<[Vec3; JOINTS]>; 2],
    /// The viewer's right (room space), for swipes across the finger.
    pub view_right: Vec3,
    /// Microgestures the runtime recognised this frame. A hand with
    /// runtime microgestures (`micro_capable`) gets no joint-derived ones.
    pub micro: [Option<Micro>; 2],
    pub micro_capable: [bool; 2],
}

/// Measures of one hand's pose.
#[derive(Clone, Copy, Debug)]
pub struct Shape {
    /// Each finger's curl, 0 straight to 1 folded into the palm.
    pub curl: [f32; 4],
    /// The palm's normal (out of the palm) and its centre.
    pub normal: Vec3,
    pub palm: Vec3,
    /// Along the hand toward the fingers.
    pub forward: Vec3,
    pub pinch: f32,
    /// The farthest of the index, middle and ring fingertips from the
    /// thumb's (whole-hand pinch).
    pub gather: f32,
    /// How far the thumb tip lies from the index finger's radial side,
    /// and where along the finger (0 knuckle .. 1 tip).
    pub thumb_side: f32,
    pub thumb_along: f32,
    pub thumb_tip: Vec3,
    /// The thumb tip's distance to the middle and index fingers' middle
    /// joints (closing around a fist).
    pub thumb_wrap: f32,
}

/// Recognition thresholds (metres unless noted).
const PINCH_ON: f32 = 0.018;
const PINCH_OFF: f32 = 0.035;
const OPEN_CURL: f32 = 0.35;
const FIST_CURL: f32 = 0.7;
const THUMB_WRAP: f32 = 0.045;
/// The palm's normal within this of straight down (or up) is overhand
/// (underhand); between the two regions it is sideways.
const PALM_CONE: f32 = 0.55; // cos ~57 degrees
const GATHER_ON: f32 = 0.03;
const GATHER_OFF: f32 = 0.045;
const PRAYER_NEAR: f32 = 0.07;
const PRAYER_APART: f32 = 0.15;
const THUMB_CONTACT: f32 = 0.014;
const THUMB_LIFT: f32 = 0.024;
const SWIPE_TRAVEL: f32 = 0.015;
const TAP_SECONDS: f64 = 0.35;
/// Frames a pinch must hold its shape before it commits (a closing fist
/// passes through pinch-like shapes for a frame or so).
const PINCH_FRAMES: u32 = 2;
/// Seconds within which the other hand's matching grab coalesces.
const COALESCE_SECONDS: f64 = 0.2;

fn finger(joints: &[Vec3; JOINTS], base: usize, joint: usize) -> Vec3 {
    joints[base + joint]
}

impl Shape {
    pub fn of(joints: &[Vec3; JOINTS], hand: Hand) -> Shape {
        let wrist = joints[WRIST];
        let mut curl = [0.0; 4];
        for (i, base) in FINGERS.into_iter().enumerate() {
            let chain = (PROXIMAL..=TIP).map(|j| finger(joints, base, j).distance(finger(joints, base, j - 1))).sum::<f32>();
            let reach = finger(joints, base, TIP).distance(finger(joints, base, METACARPAL));
            let ratio = reach / chain.max(1e-4);
            curl[i] = ((0.92 - ratio) / (0.92 - 0.45)).clamp(0.0, 1.0);
        }
        let index_base = finger(joints, INDEX, PROXIMAL);
        let little_base = finger(joints, LITTLE, PROXIMAL);
        let sign = if hand == Hand::Right { 1.0 } else { -1.0 };
        let normal = ((index_base - wrist).cross(little_base - wrist) * sign).normalize_or_zero();
        let palm = (wrist + index_base + finger(joints, MIDDLE, PROXIMAL) + finger(joints, RING, PROXIMAL) + little_base) / 5.0;
        let forward = (finger(joints, MIDDLE, PROXIMAL) - wrist).normalize_or_zero();
        let thumb_tip = joints[THUMB_TIP];
        let pinch = thumb_tip.distance(finger(joints, INDEX, TIP));
        // All fingertips at the thumb's: the farthest of index, middle, ring.
        let gather = [INDEX, MIDDLE, RING].map(|b| thumb_tip.distance(finger(joints, b, TIP))).into_iter().fold(0.0, f32::max);
        // The radial side of the index finger: its proximal and
        // intermediate bones.
        let (a, b, c) = (index_base, finger(joints, INDEX, INTERMEDIATE), finger(joints, INDEX, DISTAL));
        let (d1, t1) = segment(thumb_tip, a, b);
        let (d2, t2) = segment(thumb_tip, b, c);
        let (thumb_side, thumb_along) = if d1 <= d2 { (d1, t1 * 0.5) } else { (d2, 0.5 + t2 * 0.5) };
        let thumb_wrap = thumb_tip.distance(finger(joints, MIDDLE, INTERMEDIATE)).min(thumb_tip.distance(b));
        Shape { curl, normal, palm, forward, pinch, gather, thumb_side, thumb_along, thumb_tip, thumb_wrap }
    }

    pub fn open(&self) -> bool {
        self.curl.iter().sum::<f32>() / 4.0 < OPEN_CURL
    }

    pub fn fist(&self) -> bool {
        self.curl.iter().all(|c| *c > FIST_CURL) && self.thumb_wrap < THUMB_WRAP
    }

    /// Fingertips gathered together, the fingers not folded into the palm.
    pub fn whole_pinch(&self) -> bool {
        self.gather < GATHER_ON && self.curl[1] < FIST_CURL
    }

    /// Thumb and index tips together, the other fingers open.
    fn pinching(&self) -> bool {
        self.pinch < PINCH_ON && self.gather > GATHER_ON + 0.01 && self.curl[1..].iter().all(|c| *c < 0.55)
    }

    pub fn palm(&self) -> Palm {
        let down = self.normal.dot(Vec3::NEG_Y);
        if down > PALM_CONE {
            Palm::Down
        } else if down < -PALM_CONE {
            Palm::Up
        } else {
            Palm::Sideways
        }
    }
}

/// The distance from `p` to the segment `a`-`b`, and where along it.
fn segment(p: Vec3, a: Vec3, b: Vec3) -> (f32, f32) {
    let ab = b - a;
    let t = ((p - a).dot(ab) / ab.length_squared().max(1e-8)).clamp(0.0, 1.0);
    (p.distance(a + ab * t), t)
}

#[derive(Clone, Copy, Debug, PartialEq)]
enum Grab {
    /// Open: the next closure fires.
    Armed,
    /// Closing from an open hand, the palm's region as it began.
    Closing(Palm),
    /// Closed, or not freshly opened: nothing fires until the hand opens.
    Spent,
}

#[derive(Clone, Copy, Debug, PartialEq)]
enum Thumb {
    Off,
    /// On the finger since `since`, from `along` and `at`.
    On { since: f64, along: f32, at: Vec3, travelled: bool },
}

#[derive(Clone, Debug)]
struct HandState {
    tracked: bool,
    grab: Grab,
    pinch_frames: u32,
    pinched: bool,
    thumb: Thumb,
    whole_pinch: bool,
    last: Option<Shape>,
}

impl Default for HandState {
    fn default() -> Self {
        // A hand first seen fires nothing until it has been open.
        HandState { tracked: false, grab: Grab::Spent, pinch_frames: 0, pinched: false, thumb: Thumb::Off, whole_pinch: false, last: None }
    }
}

/// The recognizers' state across frames.
#[derive(Clone, Debug, Default)]
pub struct Recognizer {
    hands: [HandState; 2],
    /// Two-hand scaling: the hands' distance as it began.
    scaling: Option<f32>,
    /// After scaling, both hands must let go before anything else.
    scale_release: bool,
    prayer: bool,
    prayer_armed: bool,
    /// The last grab toggle (gesture kind, time) for coalescing.
    last_toggle: Option<(bool, f64)>,
}

impl Recognizer {
    /// The shapes last seen.
    pub fn shape(&self, hand: Hand) -> Option<Shape> {
        self.hands[hand.index()].last
    }

    /// Forget everything in progress (focus lost, the app took the input):
    /// held and pending gestures are cancelled; every hand must open again.
    pub fn cancel(&mut self) {
        *self = Recognizer { prayer_armed: false, ..Recognizer::default() };
    }

    pub fn update(&mut self, frame: &Frame) -> Vec<Gesture> {
        let mut out = Vec::new();
        let shapes = [0, 1].map(|i| frame.hands[i].as_ref().map(|j| Shape::of(j, Hand::BOTH[i])));
        for hand in Hand::BOTH {
            let state = &mut self.hands[hand.index()];
            if shapes[hand.index()].is_none() && state.tracked {
                out.push(Gesture::TrackingLost { hand });
                if self.scaling.take().is_some() {
                    out.push(Gesture::ScaleEnd);
                }
                self.hands[hand.index()] = HandState::default();
            }
        }

        // Two-hand gestures first: scaling owns both hands.
        if let [Some(left), Some(right)] = shapes {
            let distance = left.palm.distance(right.palm);
            let both_whole = (left.whole_pinch() || self.hands[0].whole_pinch && left.gather < GATHER_OFF)
                && (right.whole_pinch() || self.hands[1].whole_pinch && right.gather < GATHER_OFF);
            match self.scaling {
                None if both_whole && !self.scale_release => {
                    self.scaling = Some(distance.max(0.01));
                    out.push(Gesture::ScaleBegin);
                }
                Some(start) if both_whole => out.push(Gesture::Scale { factor: distance / start }),
                Some(_) => {
                    self.scaling = None;
                    self.scale_release = true;
                    out.push(Gesture::ScaleEnd);
                }
                None => {}
            }
            // Prayer: open palms facing, aligned and near.
            let facing = left.normal.dot(right.normal) < -0.7 && left.forward.dot(right.forward) > 0.6;
            let prayer = left.open() && right.open() && facing && distance < PRAYER_NEAR;
            if prayer && !self.prayer && self.prayer_armed && self.scaling.is_none() {
                out.push(Gesture::Settings);
                self.prayer_armed = false;
            }
            self.prayer = prayer;
            if distance > PRAYER_APART || !(left.open() && right.open()) {
                self.prayer_armed = !prayer;
            }
        } else {
            if let Some(start) = self.scaling.take() {
                let _ = start;
                self.scale_release = true;
                out.push(Gesture::ScaleEnd);
            }
            self.prayer = false;
            self.prayer_armed = true;
        }
        if self.scale_release && shapes.iter().all(|s| s.is_none_or(|s| s.open())) {
            self.scale_release = false;
        }
        let two_handed = self.scaling.is_some() || self.scale_release || self.prayer;

        for hand in Hand::BOTH {
            let Some(shape) = shapes[hand.index()] else { continue };
            let state = &mut self.hands[hand.index()];
            state.tracked = true;
            state.whole_pinch = shape.whole_pinch() || (state.whole_pinch && shape.gather < GATHER_OFF);
            // Grabs.
            let open = shape.open();
            let mut grabbing = false;
            state.grab = match state.grab {
                _ if open => Grab::Armed,
                Grab::Armed if !shape.fist() => {
                    // Closing begins once the fingers start to fold.
                    if shape.curl.iter().sum::<f32>() / 4.0 > OPEN_CURL + 0.05 { Grab::Closing(shape.palm()) } else { Grab::Armed }
                }
                Grab::Armed => Grab::Spent, // closed within a frame: no orientation seen closing
                Grab::Closing(palm) if shape.palm() != palm => Grab::Spent,
                Grab::Closing(palm) if shape.fist() => {
                    let toggle = match palm {
                        Palm::Down => Some(Gesture::Pause { hand }),
                        Palm::Up => Some(Gesture::Inspect { hand }),
                        Palm::Sideways => None,
                    };
                    if let Some(gesture) = toggle && !two_handed {
                        let pause = matches!(gesture, Gesture::Pause { .. });
                        let coalesced = self.last_toggle.is_some_and(|(kind, at)| kind == pause && frame.time - at < COALESCE_SECONDS);
                        if !coalesced {
                            out.push(gesture);
                        }
                        self.last_toggle = Some((pause, frame.time));
                    }
                    Grab::Spent
                }
                other => other,
            };
            if matches!(state.grab, Grab::Closing(_)) || shape.fist() {
                grabbing = true;
            }
            if two_handed || state.whole_pinch {
                // Scaling and prayer own the hands: a grab must open again
                // after them.
                state.grab = Grab::Spent;
            }
            if two_handed || grabbing || state.whole_pinch {
                state.pinch_frames = 0;
                state.thumb = Thumb::Off;
                if shape.pinch > PINCH_OFF {
                    state.pinched = false;
                }
                state.last = Some(shape);
                continue;
            }
            // Pinch.
            if shape.pinching() && !state.pinched {
                state.pinch_frames += 1;
                if state.pinch_frames >= PINCH_FRAMES {
                    state.pinched = true;
                    out.push(Gesture::Pinch { hand });
                }
            } else if shape.pinch > PINCH_OFF {
                state.pinched = false;
                state.pinch_frames = 0;
            }
            // Thumb taps and swipes on the index finger.
            if frame.micro_capable[hand.index()] {
                match frame.micro[hand.index()] {
                    Some(Micro::Tap) => out.push(Gesture::Tap { hand }),
                    Some(Micro::Swipe(direction)) => out.push(Gesture::Swipe { hand, direction }),
                    None => {}
                }
            } else if !state.pinched && shape.pinch > PINCH_OFF {
                let forward = (shape_index_axis(frame.hands[hand.index()].as_ref().expect("tracked"))).normalize_or_zero();
                state.thumb = match state.thumb {
                    Thumb::Off if shape.thumb_side < THUMB_CONTACT => Thumb::On { since: frame.time, along: shape.thumb_along, at: shape.thumb_tip, travelled: false },
                    Thumb::On { since, travelled, .. } if shape.thumb_side > THUMB_LIFT => {
                        if !travelled && frame.time - since <= TAP_SECONDS {
                            out.push(Gesture::Tap { hand });
                        }
                        Thumb::Off
                    }
                    Thumb::On { since, along, at, travelled } => {
                        let moved = shape.thumb_tip - at;
                        if !travelled && moved.length() > SWIPE_TRAVEL {
                            let lengthwise = moved.dot(forward);
                            // Across the finger's side: toward the back of
                            // the hand is Right for the right hand (held
                            // sideways, its back faces right) and Left for
                            // the left.
                            let dorsal = -shape.normal * if hand == Hand::Right { 1.0 } else { -1.0 };
                            let across = moved.dot(dorsal);
                            let direction = if lengthwise.abs() >= across.abs() {
                                if lengthwise > 0.0 { Swipe::Up } else { Swipe::Down }
                            } else if across > 0.0 {
                                Swipe::Right
                            } else {
                                Swipe::Left
                            };
                            out.push(Gesture::Swipe { hand, direction });
                            Thumb::On { since, along, at, travelled: true }
                        } else {
                            Thumb::On { since, along, at, travelled }
                        }
                    }
                    off => off,
                };
            } else {
                state.thumb = Thumb::Off;
            }
            state.last = Some(shape);
        }
        out
    }
}

/// Along the index finger, knuckle to tip.
fn shape_index_axis(joints: &[Vec3; JOINTS]) -> Vec3 {
    finger(joints, INDEX, TIP) - finger(joints, INDEX, PROXIMAL)
}

/// Gestures a source of hand input can produce: joints give every hand
/// gesture; without them none is available (controllers and pointers
/// remain).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize)]
pub struct Availability {
    pub hands: bool,
    /// Thumb microgestures from the runtime (else derived from joints).
    pub runtime_microgestures: bool,
    pub gaze: bool,
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use crate::synthetic::*;
    use glam::{Quat, Vec3};

    /// IWER 2.5.0's left-hand "default" pose (oculusHandConfig, MIT, Meta), its
    /// joints in its grip space.
    pub const IWER_DEFAULT: [[f32; 3]; 25] = [[-0.0491, 0.0021, 0.117], [-0.0106, 0.0007, 0.0874], [0.0155, -0.0003, 0.0681], [0.0433, 0.0034, 0.0492], [0.062, 0.0041, 0.0332], [-0.0201, 0.0088, 0.0866], [-0.0002, 0.0389, 0.0391], [0.0162, 0.0548, 0.0088], [0.026, 0.0599, -0.0129], [0.0336, 0.0642, -0.0335], [-0.0363, 0.0116, 0.0855], [-0.0213, 0.0459, 0.0366], [-0.0146, 0.0679, 0.0004], [-0.0104, 0.0741, -0.0261], [-0.007, 0.0802, -0.0501], [-0.054, 0.0158, 0.0815], [-0.0416, 0.0417, 0.0358], [-0.04, 0.0644, 0.0041], [-0.0378, 0.0733, -0.0208], [-0.0345, 0.0802, -0.0439], [-0.0619, 0.0136, 0.0785], [-0.0615, 0.0327, 0.0371], [-0.0661, 0.0498, 0.0119], [-0.0688, 0.0549, -0.0076], [-0.0695, 0.0614, -0.0285]];
    /// IWER 2.5.0's left-hand "pinch" pose (oculusHandConfig, MIT, Meta), its
    /// joints in its grip space.
    pub const IWER_PINCH: [[f32; 3]; 25] = [[-0.0687, -0.0094, 0.1063], [-0.0321, -0.012, 0.0719], [-0.0176, -0.0197, 0.0439], [-0.0071, -0.021, 0.0118], [0.0003, -0.0245, -0.0114], [-0.038, -0.002, 0.0763], [-0.0069, 0.0339, 0.0421], [0.0033, 0.0148, 0.0109], [0.0022, -0.0058, -0.0018], [-0.0013, -0.0252, -0.0124], [-0.054, 0.0031, 0.074], [-0.0272, 0.0416, 0.0368], [-0.0103, 0.053, -0.001], [-0.0005, 0.051, -0.0267], [0.0082, 0.05, -0.0501], [-0.0673, 0.0079, 0.0721], [-0.0471, 0.0381, 0.0321], [-0.0358, 0.0613, 0.0029], [-0.0263, 0.068, -0.0209], [-0.0163, 0.073, -0.0426], [-0.0746, 0.0062, 0.0676], [-0.067, 0.0295, 0.029], [-0.0645, 0.0514, 0.0077], [-0.0619, 0.0604, -0.0104], [-0.0568, 0.0704, -0.0292]];
    /// IWER 2.5.0's left-hand "point" pose (oculusHandConfig, MIT, Meta), its
    /// joints in its grip space.
    pub const IWER_POINT: [[f32; 3]; 25] = [[-0.0579, 0.0167, 0.1118], [-0.0165, 0.0127, 0.0886], [0.0131, 0.0125, 0.0752], [0.0415, 0.016, 0.0572], [0.0655, 0.0168, 0.0517], [-0.0259, 0.02, 0.0848], [-0.0016, 0.0431, 0.0347], [0.014, 0.0528, 0.0015], [0.0235, 0.0563, -0.0206], [0.031, 0.0628, -0.0407], [-0.0418, 0.0225, 0.0818], [-0.0221, 0.0504, 0.0297], [-0.0172, 0.0226, -0.0027], [-0.0217, -0.0006, 0.0115], [-0.0244, -0.0027, 0.0363], [-0.0594, 0.0265, 0.0748], [-0.0425, 0.0467, 0.0277], [-0.0375, 0.0163, 0.0039], [-0.0381, -0.0021, 0.0231], [-0.0391, 0.0042, 0.0467], [-0.0672, 0.0242, 0.0714], [-0.0627, 0.0386, 0.0283], [-0.0543, 0.0139, 0.0121], [-0.0517, 0.0014, 0.0279], [-0.051, 0.0085, 0.0487]];

    fn frame(time: f64, left: Option<[Vec3; JOINTS]>, right: Option<[Vec3; JOINTS]>) -> Frame {
        Frame { time, hands: [left, right], view_right: Vec3::X, ..Frame::default() }
    }




    #[test]
    fn shapes_measure_open_hands_fists_and_pinches() {
        let open = Shape::of(&open(), Hand::Right);
        assert!(open.open() && !open.fist(), "{open:?}");
        assert_eq!(open.palm(), Palm::Down);
        let fist = Shape::of(&fist(), Hand::Right);
        assert!(fist.fist(), "{fist:?}");
        let up = Shape::of(&place(super::tests::open(), Quat::from_rotation_z(std::f32::consts::PI), Vec3::ZERO), Hand::Right);
        assert_eq!(up.palm(), Palm::Up);
        let side = Shape::of(&place(super::tests::open(), Quat::from_rotation_z(std::f32::consts::FRAC_PI_2), Vec3::ZERO), Hand::Right);
        assert_eq!(side.palm(), Palm::Sideways);
        assert_eq!(Shape::of(&left(super::tests::open()), Hand::Left).palm(), Palm::Down, "the left hand mirrors");
        let hand = right_hand([0.0; 4], Vec3::ZERO);
        let mut pinch = hand;
        pinch[THUMB_TIP] = hand[INDEX + TIP] + Vec3::new(0.005, 0.0, 0.0);
        let shape = Shape::of(&pinch, Hand::Right);
        assert!(shape.pinching(), "{shape:?}");
    }

    /// Runs poses one frame each at 90 Hz.
    fn run(recognizer: &mut Recognizer, start: f64, poses: &[(Option<[Vec3; JOINTS]>, Option<[Vec3; JOINTS]>)]) -> Vec<Gesture> {
        poses.iter().enumerate().flat_map(|(i, (l, r))| recognizer.update(&frame(start + i as f64 / 90.0, *l, *r))).collect()
    }

    #[test]
    fn an_overhand_grab_pauses_on_closure_and_reopening_rearms() {
        let mut recognizer = Recognizer::default();
        let poses = [(None, Some(open())), (None, Some(half())), (None, Some(fist()))];
        assert_eq!(run(&mut recognizer, 0.0, &poses), [Gesture::Pause { hand: Hand::Right }]);
        // Held, or rotated while closed: nothing more.
        let rolled = place(fist(), Quat::from_rotation_z(std::f32::consts::PI), Vec3::ZERO);
        assert!(run(&mut recognizer, 1.0, &[(None, Some(fist())), (None, Some(rolled))]).is_empty());
        // Reopened, it toggles again at once: no cooldown.
        assert_eq!(run(&mut recognizer, 1.1, &poses), [Gesture::Pause { hand: Hand::Right }]);
    }

    #[test]
    fn an_underhand_grab_inspects_and_a_sideways_one_does_nothing() {
        let up = Quat::from_rotation_z(std::f32::consts::PI);
        let side = Quat::from_rotation_z(std::f32::consts::FRAC_PI_2);
        for (rotation, expected) in [(up, vec![Gesture::Inspect { hand: Hand::Right }]), (side, vec![])] {
            let mut recognizer = Recognizer::default();
            let poses = [open(), half(), fist()].map(|p| (None, Some(place(p, rotation, Vec3::ZERO))));
            assert_eq!(run(&mut recognizer, 0.0, &poses), expected);
        }
        // Turning from palm down to palm up while closing fires neither.
        let mut recognizer = Recognizer::default();
        let poses = [(None, Some(open())), (None, Some(half())), (None, Some(place(fist(), up, Vec3::ZERO)))];
        assert!(run(&mut recognizer, 0.0, &poses).is_empty());
    }

    #[test]
    fn a_fist_already_closed_or_reacquired_fires_nothing() {
        let mut recognizer = Recognizer::default();
        assert!(run(&mut recognizer, 0.0, &[(None, Some(fist())), (None, Some(fist()))]).is_empty());
        let mut recognizer = Recognizer::default();
        let poses = [(None, Some(open())), (None, Some(half())), (None, None), (None, Some(fist()))];
        assert_eq!(run(&mut recognizer, 0.0, &poses), [Gesture::TrackingLost { hand: Hand::Right }]);
    }

    #[test]
    fn both_hands_grabbing_together_toggle_once() {
        let mut recognizer = Recognizer::default();
        let poses = [(Some(left(open())), Some(open())), (Some(left(half())), Some(half())), (Some(left(fist())), Some(fist()))];
        let gestures = run(&mut recognizer, 0.0, &poses);
        assert_eq!(gestures.len(), 1, "{gestures:?}");
        assert!(matches!(gestures[0], Gesture::Pause { .. }));
    }

    #[test]
    fn a_pinch_commits_once_and_a_closing_fist_emits_no_pinch() {
        let mut recognizer = Recognizer::default();
        let hand = open();
        let mut pinch = hand;
        pinch[THUMB_TIP] = hand[INDEX + TIP] + Vec3::new(0.004, 0.0, 0.0);
        let poses = [(None, Some(hand)), (None, Some(pinch)), (None, Some(pinch)), (None, Some(pinch)), (None, Some(hand))];
        assert_eq!(run(&mut recognizer, 0.0, &poses), [Gesture::Pinch { hand: Hand::Right }]);
        let mut recognizer = Recognizer::default();
        let gestures = run(&mut recognizer, 0.0, &[(None, Some(open())), (None, Some(half())), (None, Some(fist())), (None, Some(fist()))]);
        assert!(!gestures.iter().any(|g| matches!(g, Gesture::Pinch { .. } | Gesture::Tap { .. } | Gesture::Swipe { .. })), "{gestures:?}");
    }

    #[test]
    fn thumb_taps_and_swipes_on_the_index_finger() {
        let relaxed = right_hand([0.25; 4], THUMB_REST);
        let side = |along: f32| {
            let mut hand = relaxed;
            let a = hand[INDEX + PROXIMAL];
            let b = hand[INDEX + INTERMEDIATE];
            // On the radial side (toward the thumb, -X).
            hand[THUMB_TIP] = a + (b - a) * along + Vec3::new(-0.009, 0.0, 0.0);
            hand
        };
        let mut recognizer = Recognizer::default();
        let tap = [(None, Some(relaxed)), (None, Some(side(0.5))), (None, Some(side(0.5))), (None, Some(relaxed))];
        assert_eq!(run(&mut recognizer, 0.0, &tap), [Gesture::Tap { hand: Hand::Right }]);
        // Toward the fingertip: up; back: down.
        let up = [(None, Some(relaxed)), (None, Some(side(0.1))), (None, Some(side(0.9))), (None, Some(relaxed))];
        assert_eq!(run(&mut recognizer, 1.0, &up), [Gesture::Swipe { hand: Hand::Right, direction: Swipe::Up }]);
        let down = [(None, Some(relaxed)), (None, Some(side(0.9))), (None, Some(side(0.1))), (None, Some(relaxed))];
        assert_eq!(run(&mut recognizer, 2.0, &down), [Gesture::Swipe { hand: Hand::Right, direction: Swipe::Down }]);
        // A slow lift is no tap.
        let mut slow = vec![(None, Some(relaxed))];
        slow.extend(std::iter::repeat_n((None, Some(side(0.5))), 60));
        slow.push((None, Some(relaxed)));
        assert!(run(&mut recognizer, 3.0, &slow).is_empty());
        // A runtime's microgestures replace the joints'.
        let mut recognizer = Recognizer::default();
        let mut micro = frame(0.0, None, Some(relaxed));
        micro.micro_capable = [false, true];
        micro.micro[1] = Some(Micro::Swipe(Swipe::Left));
        assert_eq!(recognizer.update(&micro), [Gesture::Swipe { hand: Hand::Right, direction: Swipe::Left }]);
    }


    #[test]
    fn two_whole_hand_pinches_scale_and_own_the_hands_until_release() {
        let mut recognizer = Recognizer::default();
        let at = |x: f32, hand: [Vec3; JOINTS]| place(hand, Quat::IDENTITY, Vec3::new(x, 0.0, 0.0));
        let poses = [
            (Some(left(at(0.1, open()))), Some(at(0.1, open()))),
            (Some(at(-0.1, left(whole_pinch()))), Some(at(0.1, whole_pinch()))),
            (Some(at(-0.2, left(whole_pinch()))), Some(at(0.2, whole_pinch()))),
            // One hand lets go into a fist on the way out: no Pause.
            (Some(at(-0.2, left(fist()))), Some(at(0.2, whole_pinch()))),
            (Some(at(-0.2, left(fist()))), Some(at(0.2, fist()))),
        ];
        let gestures = run(&mut recognizer, 0.0, &poses);
        assert_eq!(gestures[0], Gesture::ScaleBegin, "{gestures:?}");
        let Gesture::Scale { factor } = gestures[1] else { panic!("{gestures:?}") };
        assert!((factor - 2.0).abs() < 0.2, "{factor}");
        assert_eq!(gestures[2], Gesture::ScaleEnd);
        assert_eq!(gestures.len(), 3, "{gestures:?}");
    }

    #[test]
    fn prayer_hands_open_settings_once_and_separating_rearms() {
        // Palms facing: the right palm turned to face left (-X), the left's
        // to face right, fingers forward, a few centimetres apart.
        let right = place(open(), Quat::from_rotation_z(-std::f32::consts::FRAC_PI_2), Vec3::new(0.02, 0.0, 0.0));
        let left_hand = place(left(open()), Quat::from_rotation_z(std::f32::consts::FRAC_PI_2), Vec3::new(-0.02, 0.0, 0.0));
        let apart = |x: f32, h: [Vec3; JOINTS]| place(h, Quat::IDENTITY, Vec3::new(x, 0.0, 0.0));
        let mut recognizer = Recognizer::default();
        let poses = [
            (Some(apart(-0.2, left_hand)), Some(apart(0.2, right))),
            (Some(left_hand), Some(right)),
            (Some(left_hand), Some(right)),
            (Some(apart(-0.05, left_hand)), Some(apart(0.05, right))),
            (Some(left_hand), Some(right)),
        ];
        assert_eq!(run(&mut recognizer, 0.0, &poses), [Gesture::Settings], "near again without separating rearms nothing");
        let more = [(Some(apart(-0.2, left_hand)), Some(apart(0.2, right))), (Some(left_hand), Some(right))];
        assert_eq!(run(&mut recognizer, 1.0, &more), [Gesture::Settings]);
    }

    #[test]
    fn iwers_recorded_poses_read_as_a_relaxed_hand_and_a_pinch() {
        let pose = |p: [[f32; 3]; 25]| p.map(Vec3::from);
        let relaxed = Shape::of(&pose(IWER_DEFAULT), Hand::Left);
        assert!(!relaxed.pinching() && !relaxed.fist() && !relaxed.whole_pinch(), "{relaxed:?}");
        let pinch = Shape::of(&pose(IWER_PINCH), Hand::Left);
        assert!(pinch.pinching() && !pinch.fist(), "{pinch:?}");
        let point = Shape::of(&pose(IWER_POINT), Hand::Left);
        assert!(!point.pinching() && !point.fist(), "{point:?}");
    }
}
