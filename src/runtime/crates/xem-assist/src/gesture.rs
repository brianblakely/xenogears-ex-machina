//! Hand gestures (plan.md, the shared headset hand/gaze contract). The
//! standard ones are the runtime's, never derived here:
//!
//! - A **pinch** is the runtime's: OpenXR's `XR_EXT_hand_interaction`
//!   (`pinch_ext`), WebXR's select of a hand input source. Without it a
//!   hand has no pinch.
//! - A **thumb tap** and the **thumb swipes** are the runtime's
//!   microgestures (`XR_META_hand_tracking_microgestures`: forward Up,
//!   backward Down, left, right). Without them a hand has none (WebXR has
//!   no such standard).
//!
//! The gestures plan.md describes beyond the standard ones are recognised
//! here from tracked joints (OpenXR's XR_EXT_hand_tracking and WebXR's,
//! both reduced to WebXR's 25 joints in room space, metres, +Y up):
//!
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

/// A runtime's thumb microgesture (XR_META_hand_tracking_microgestures).
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
    /// Each hand's standard pinch as the runtime reports it (pinching or
    /// not); `None` where the runtime gives that hand none.
    pub pinch: [Option<bool>; 2],
    /// Microgestures the runtime recognised this frame, and the hands it
    /// offers them for.
    pub micro: [Option<Micro>; 2],
    pub micro_capable: [bool; 2],
}

/// Measures of one hand's pose.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Shape {
    /// Each finger's curl, 0 straight to 1 folded into the palm.
    pub curl: [f32; 4],
    /// The palm's normal (out of the palm) and its centre.
    pub normal: Vec3,
    pub palm: Vec3,
    /// Along the hand toward the fingers.
    pub forward: Vec3,
    /// The farthest of the index, middle and ring fingertips from the
    /// thumb's (whole-hand pinch).
    pub gather: f32,
    /// The thumb tip's distance to the middle and index fingers' middle
    /// joints (closing around a fist).
    pub thumb_wrap: f32,
}

/// Recognition thresholds (metres unless noted).
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
        // All fingertips at the thumb's: the farthest of index, middle, ring.
        let gather = [INDEX, MIDDLE, RING].map(|b| thumb_tip.distance(finger(joints, b, TIP))).into_iter().fold(0.0, f32::max);
        let thumb_wrap = thumb_tip.distance(finger(joints, MIDDLE, INTERMEDIATE)).min(thumb_tip.distance(finger(joints, INDEX, INTERMEDIATE)));
        Shape { curl, normal, palm, forward, gather, thumb_wrap }
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

#[derive(Clone, Copy, Debug, PartialEq)]
enum Grab {
    /// Open: the next closure fires.
    Armed,
    /// Closing from an open hand, the palm's region as it began.
    Closing(Palm),
    /// Closed, or not freshly opened: nothing fires until the hand opens.
    Spent,
}

#[derive(Clone, Debug)]
struct HandState {
    tracked: bool,
    grab: Grab,
    /// The runtime's pinch was on (its rising edge is the gesture).
    pinched: bool,
    whole_pinch: bool,
    last: Option<Shape>,
}

impl Default for HandState {
    fn default() -> Self {
        // A hand first seen fires nothing until it has been open.
        HandState { tracked: false, grab: Grab::Spent, pinched: false, whole_pinch: false, last: None }
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
    /// The last grab toggle (gesture kind, time, hand) for coalescing the
    /// other hand's simultaneous one.
    last_toggle: Option<(bool, f64, Hand)>,
}

impl Recognizer {
    /// The shapes last seen.
    pub fn shape(&self, hand: Hand) -> Option<Shape> {
        self.hands[hand.index()].last
    }

    /// Whether two-hand scaling is under way.
    pub fn scaling(&self) -> bool {
        self.scaling.is_some()
    }

    /// Forget everything in progress (focus lost, the app took the input):
    /// held and pending gestures are cancelled; each hand must open again
    /// before its grab fires. A two-hand scaling under way (or just ended)
    /// keeps both hands as when it ends: hands still gathered begin no new
    /// scaling, and nothing else fires until both have opened.
    pub fn cancel(&mut self) {
        let scale_release = self.scaling.is_some() || self.scale_release;
        *self = Recognizer { prayer_armed: false, scale_release, ..Recognizer::default() };
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
        // Scaling owns the hands through their release: the frame it ends
        // on too, when they let go at once.
        let released = out.contains(&Gesture::ScaleEnd);
        let two_handed = self.scaling.is_some() || self.scale_release || self.prayer || released;

        for hand in Hand::BOTH {
            let Some(shape) = shapes[hand.index()] else { continue };
            let state = &mut self.hands[hand.index()];
            state.tracked = true;
            state.whole_pinch = shape.whole_pinch() || (state.whole_pinch && shape.gather < GATHER_OFF);
            // Grabs.
            let open = shape.open();
            let mut grabbing = false;
            // An armed hand that closed between two frames closed from open:
            // its palm's region now is the one it closed in.
            let closing = match state.grab {
                Grab::Armed if shape.fist() => Grab::Closing(shape.palm()),
                other => other,
            };
            state.grab = match closing {
                _ if open => Grab::Armed,
                // Closing begins once the fingers start to fold.
                Grab::Armed => Grab::Closing(shape.palm()),
                Grab::Closing(palm) if shape.palm() != palm => Grab::Spent,
                Grab::Closing(palm) if shape.fist() => {
                    let toggle = match palm {
                        Palm::Down => Some(Gesture::Pause { hand }),
                        Palm::Up => Some(Gesture::Inspect { hand }),
                        Palm::Sideways => None,
                    };
                    if let Some(gesture) = toggle && !two_handed {
                        let pause = matches!(gesture, Gesture::Pause { .. });
                        let coalesced = self.last_toggle.is_some_and(|(kind, at, by)| kind == pause && by != hand && frame.time - at < COALESCE_SECONDS);
                        if !coalesced {
                            out.push(gesture);
                        }
                        self.last_toggle = Some((pause, frame.time, hand));
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
            let pinching = frame.pinch[hand.index()] == Some(true);
            if two_handed || grabbing || state.whole_pinch {
                // A grab, scaling or prayer owns the hand: a pinch held
                // through it fires nothing, now or when it ends.
                state.pinched |= pinching;
                if !pinching {
                    state.pinched = false;
                }
                state.last = Some(shape);
                continue;
            }
            // The runtime's pinch: its start.
            if pinching && !state.pinched {
                out.push(Gesture::Pinch { hand });
            }
            state.pinched = pinching;
            // The runtime's microgestures.
            if frame.micro_capable[hand.index()] {
                match frame.micro[hand.index()] {
                    Some(Micro::Tap) => out.push(Gesture::Tap { hand }),
                    Some(Micro::Swipe(direction)) => out.push(Gesture::Swipe { hand, direction }),
                    None => {}
                }
            }
            state.last = Some(shape);
        }
        out
    }
}


/// How many of each gesture a host has recognised (its status, for tests:
/// a grab, prayer hands or a scaling leaves no state behind it that says it
/// was seen, such as an underhand grab outside a field).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize)]
pub struct Counts {
    pub pause: u32,
    pub inspect: u32,
    pub settings: u32,
    /// Two-hand scalings begun.
    pub scale: u32,
    pub pinch: u32,
    pub tap: u32,
    pub swipe: u32,
    /// Hands whose tracking was lost.
    pub lost: u32,
}

impl Counts {
    pub fn add(&mut self, gestures: &[Gesture]) {
        for gesture in gestures {
            let count = match gesture {
                Gesture::Pause { .. } => &mut self.pause,
                Gesture::Inspect { .. } => &mut self.inspect,
                Gesture::Settings => &mut self.settings,
                Gesture::ScaleBegin => &mut self.scale,
                Gesture::Pinch { .. } => &mut self.pinch,
                Gesture::Tap { .. } => &mut self.tap,
                Gesture::Swipe { .. } => &mut self.swipe,
                Gesture::TrackingLost { .. } => &mut self.lost,
                Gesture::Scale { .. } | Gesture::ScaleEnd => continue,
            };
            *count += 1;
        }
    }
}

/// Gestures a source of hand input can produce: joints give the grabs,
/// the whole-hand pinch and prayer hands; the runtime's pinch and
/// microgestures give theirs; without them those are unavailable (the
/// pointer, its keys and controllers remain).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize)]
pub struct Availability {
    pub hands: bool,
    pub pinch: bool,
    pub microgestures: bool,
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

    /// Frames of the right hand with the runtime's standard signals.
    fn signals(time: f64, hand: [Vec3; JOINTS], pinch: Option<bool>, micro: Option<Micro>, micro_capable: bool) -> Frame {
        Frame { time, hands: [None, Some(hand)], view_right: Vec3::X, pinch: [None, pinch], micro: [None, micro], micro_capable: [false, micro_capable] }
    }

    #[test]
    fn the_runtimes_pinch_commits_once_and_not_through_a_grab() {
        let mut recognizer = Recognizer::default();
        let states = [false, true, true, true, false, true, false];
        let gestures: Vec<_> = states.iter().enumerate().flat_map(|(i, p)| recognizer.update(&signals(i as f64 / 90.0, open(), Some(*p), None, false))).collect();
        assert_eq!(gestures, [Gesture::Pinch { hand: Hand::Right }, Gesture::Pinch { hand: Hand::Right }], "each start once");
        // A pinch the runtime reports while the hand closes into a grab,
        // and still after it, fires nothing.
        let mut recognizer = Recognizer::default();
        let poses = [(open(), false), (half(), true), (fist(), true), (fist(), true), (open(), true), (open(), false)];
        let gestures: Vec<_> = poses.iter().enumerate().flat_map(|(i, (h, p))| recognizer.update(&signals(i as f64 / 90.0, *h, Some(*p), None, false))).collect();
        assert_eq!(gestures, [Gesture::Pause { hand: Hand::Right }], "{gestures:?}");
        // No runtime pinch, no pinch: the joints' closeness is not one.
        let mut recognizer = Recognizer::default();
        let iwer = IWER_PINCH.map(Vec3::from);
        assert!(recognizer.update(&signals(0.0, open(), None, None, false)).is_empty());
        assert!(recognizer.update(&signals(0.1, iwer, None, None, false)).is_empty());
    }

    #[test]
    fn taps_and_swipes_are_the_runtimes_microgestures_only() {
        let relaxed = right_hand([0.25; 4], THUMB_REST);
        let mut recognizer = Recognizer::default();
        let tap = recognizer.update(&signals(0.0, relaxed, Some(false), Some(Micro::Tap), true));
        assert_eq!(tap, [Gesture::Tap { hand: Hand::Right }]);
        let swipe = recognizer.update(&signals(0.1, relaxed, Some(false), Some(Micro::Swipe(Swipe::Left)), true));
        assert_eq!(swipe, [Gesture::Swipe { hand: Hand::Right, direction: Swipe::Left }]);
        // A hand the runtime offers none for has none.
        assert!(recognizer.update(&signals(0.2, relaxed, Some(false), Some(Micro::Tap), false)).is_empty());
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
    fn the_runtimes_pinch_and_thumb_through_two_hand_scaling_fire_nothing_but_the_scale() {
        // Gathered fingertips a runtime may take for its pinch, and a thumb
        // tap and swipe it reports in the middle, from the gather to after
        // the hands let go: only the scale's own events.
        let at = |x: f32, hand: [Vec3; JOINTS]| place(hand, Quat::IDENTITY, Vec3::new(x, 0.0, 0.0));
        let poses = [
            (at(-0.1, left(open())), at(0.1, open()), false, None),
            (at(-0.1, left(whole_pinch())), at(0.1, whole_pinch()), true, None),
            (at(-0.15, left(whole_pinch())), at(0.15, whole_pinch()), true, Some(Micro::Tap)),
            (at(-0.2, left(whole_pinch())), at(0.2, whole_pinch()), true, Some(Micro::Swipe(Swipe::Up))),
            (at(-0.2, left(open())), at(0.2, open()), true, Some(Micro::Tap)),
            (at(-0.2, left(open())), at(0.2, open()), false, None),
        ];
        let mut recognizer = Recognizer::default();
        let gestures: Vec<Gesture> = poses.iter().enumerate().flat_map(|(i, (l, r, pinch, micro))| recognizer.update(&Frame {
            time: i as f64 / 90.0,
            hands: [Some(*l), Some(*r)],
            view_right: Vec3::X,
            pinch: [Some(*pinch); 2],
            micro: [*micro; 2],
            micro_capable: [true; 2],
        })).collect();
        let Gesture::Scale { factor } = gestures[1] else { panic!("{gestures:?}") };
        assert!(factor > 1.0, "{gestures:?}");
        assert!(gestures.iter().all(|g| matches!(g, Gesture::ScaleBegin | Gesture::Scale { .. } | Gesture::ScaleEnd)), "{gestures:?}");
        assert_eq!((gestures.first(), gestures.last()), (Some(&Gesture::ScaleBegin), Some(&Gesture::ScaleEnd)), "{gestures:?}");
        // The hands open again, the runtime's next pinch and tap are theirs.
        let next = recognizer.update(&Frame { time: 1.0, hands: [Some(poses[5].0), Some(poses[5].1)], view_right: Vec3::X, pinch: [None, Some(true)], micro: [None, Some(Micro::Tap)], micro_capable: [true; 2] });
        assert_eq!(next, [Gesture::Pinch { hand: Hand::Right }, Gesture::Tap { hand: Hand::Right }]);
    }

    #[test]
    fn after_a_cancel_one_hand_gestures_alone_and_a_cut_scaling_keeps_both_hands_until_both_open() {
        // Cancelled with no scaling under way (the focus lost, a moved
        // reference), the left hand tracked and half curled throughout: the
        // right hand's overhand grab (from open), the runtime's pinch and its
        // tap fire as on a fresh recognizer.
        let curled = Some(left(half()));
        let mut recognizer = Recognizer::default();
        run(&mut recognizer, 0.0, &[(Some(left(open())), Some(open())), (curled, Some(open()))]);
        recognizer.cancel();
        let grab = [(curled, Some(open())), (curled, Some(half())), (curled, Some(fist()))];
        assert_eq!(run(&mut recognizer, 1.0, &grab), [Gesture::Pause { hand: Hand::Right }]);
        let signals = |time: f64, pinch: bool, micro: Option<Micro>| Frame {
            time,
            hands: [curled, Some(open())],
            view_right: Vec3::X,
            pinch: [Some(false), Some(pinch)],
            micro: [None, micro],
            micro_capable: [true, true],
        };
        assert!(recognizer.update(&signals(2.0, false, None)).is_empty());
        recognizer.cancel();
        assert_eq!(recognizer.update(&signals(2.1, true, None)), [Gesture::Pinch { hand: Hand::Right }]);
        recognizer.cancel();
        assert_eq!(recognizer.update(&signals(2.2, false, Some(Micro::Tap))), [Gesture::Tap { hand: Hand::Right }]);
        // Cancelled while two hands scale: they keep both hands as when a
        // scaling ends. Still gathered they begin no new scaling, and a hand
        // closing on the way out grabs nothing, until both have opened.
        let at = |x: f32, hand: [Vec3; JOINTS]| place(hand, Quat::IDENTITY, Vec3::new(x, 0.0, 0.0));
        let both = |l: [Vec3; JOINTS], r: [Vec3; JOINTS]| (Some(at(-0.1, left(l))), Some(at(0.1, r)));
        let mut recognizer = Recognizer::default();
        let begun = run(&mut recognizer, 3.0, &[both(open(), open()), both(whole_pinch(), whole_pinch())]);
        assert_eq!(begun, [Gesture::ScaleBegin]);
        recognizer.cancel();
        let held = [both(whole_pinch(), whole_pinch()), both(whole_pinch(), whole_pinch()), both(open(), half()), both(open(), fist())];
        let gestures = run(&mut recognizer, 4.0, &held);
        assert!(gestures.is_empty() && !recognizer.scaling(), "{gestures:?}");
        let opened = [both(open(), open()), both(open(), half()), both(open(), fist())];
        assert_eq!(run(&mut recognizer, 5.0, &opened), [Gesture::Pause { hand: Hand::Right }], "both opened: the grabs are back");
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
    fn iwers_recorded_poses_are_no_grab_or_whole_hand_pinch() {
        let pose = |p: [[f32; 3]; 25]| p.map(Vec3::from);
        let relaxed = Shape::of(&pose(IWER_DEFAULT), Hand::Left);
        assert!(!relaxed.fist() && !relaxed.whole_pinch(), "{relaxed:?}");
        let pinch = Shape::of(&pose(IWER_PINCH), Hand::Left);
        assert!(!pinch.fist() && !pinch.whole_pinch(), "a pinch is no grab or whole-hand pinch: {pinch:?}");
        let point = Shape::of(&pose(IWER_POINT), Hand::Left);
        assert!(!point.fist(), "{point:?}");
    }
}
