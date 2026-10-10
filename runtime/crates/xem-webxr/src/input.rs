//! Raw input-source state: what the session reports each frame, without
//! interpreting gestures. Hands appear only when the session granted hand
//! tracking and the platform tracks them.

use serde::Serialize;
use wasm_bindgen::{JsCast, JsValue};
use web_sys::{GamepadButton, XrFrame, XrHandJoint, XrInputSource, XrPose, XrSession, XrSpace};

/// The 25 joints of the WebXR Hand Input module, in its order.
pub const HAND_JOINTS: [&str; 25] = [
    "wrist",
    "thumb-metacarpal",
    "thumb-phalanx-proximal",
    "thumb-phalanx-distal",
    "thumb-tip",
    "index-finger-metacarpal",
    "index-finger-phalanx-proximal",
    "index-finger-phalanx-intermediate",
    "index-finger-phalanx-distal",
    "index-finger-tip",
    "middle-finger-metacarpal",
    "middle-finger-phalanx-proximal",
    "middle-finger-phalanx-intermediate",
    "middle-finger-phalanx-distal",
    "middle-finger-tip",
    "ring-finger-metacarpal",
    "ring-finger-phalanx-proximal",
    "ring-finger-phalanx-intermediate",
    "ring-finger-phalanx-distal",
    "ring-finger-tip",
    "pinky-finger-metacarpal",
    "pinky-finger-phalanx-proximal",
    "pinky-finger-phalanx-intermediate",
    "pinky-finger-phalanx-distal",
    "pinky-finger-tip",
];

/// A rigid pose in the session's reference space: metres and an xyzw quaternion.
#[derive(Clone, Debug, Serialize)]
pub struct Pose {
    pub position: [f32; 3],
    pub orientation: [f32; 4],
}

impl Pose {
    pub fn from_xr(pose: &XrPose) -> Self {
        let transform = pose.transform();
        let (p, o) = (transform.position(), transform.orientation());
        Self {
            position: [p.x() as f32, p.y() as f32, p.z() as f32],
            orientation: [o.x() as f32, o.y() as f32, o.z() as f32, o.w() as f32],
        }
    }
}

#[derive(Clone, Debug, Serialize)]
pub struct Button {
    pub pressed: bool,
    pub touched: bool,
    pub value: f64,
}

#[derive(Clone, Debug, Serialize)]
pub struct Gamepad {
    pub buttons: Vec<Button>,
    /// `None` where the gamepad reports no number (an absent touchpad).
    pub axes: Vec<Option<f64>>,
}

#[derive(Clone, Debug, Serialize)]
pub struct Joint {
    pub name: &'static str,
    pub pose: Pose,
    pub radius: f32,
}

#[derive(Clone, Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct InputSource {
    pub handedness: String,
    pub target_ray_mode: String,
    pub profiles: Vec<String>,
    pub target_ray: Option<Pose>,
    pub grip: Option<Pose>,
    pub gamepad: Option<Gamepad>,
    /// `None` when the source is not a hand; joints the frame did not track
    /// are left out.
    pub hand: Option<Vec<Joint>>,
}

/// Every input source of `session` as `frame` sees it in `space`.
pub fn snapshot(session: &XrSession, frame: &XrFrame, space: &XrSpace) -> Vec<InputSource> {
    let sources = session.input_sources();
    (0..sources.length())
        .filter_map(|index| sources.get(index))
        .map(|source| read_source(&source, frame, space))
        .collect()
}

fn read_source(source: &XrInputSource, frame: &XrFrame, space: &XrSpace) -> InputSource {
    let pose = |of: &XrSpace| frame.get_pose(of, space).map(|pose| Pose::from_xr(&pose));
    let gamepad = source.gamepad().map(|gamepad| Gamepad {
        buttons: gamepad
            .buttons()
            .iter()
            .map(|button| {
                let button: GamepadButton = button.unchecked_into();
                Button {
                    pressed: button.pressed(),
                    touched: button.touched(),
                    value: button.value(),
                }
            })
            .collect(),
        axes: gamepad.axes().iter().map(|axis| axis.as_f64()).collect(),
    });
    let hand = source.hand().map(|hand| {
        HAND_JOINTS
            .iter()
            .filter_map(|&name| {
                let joint = XrHandJoint::from_js_value(&JsValue::from_str(name))?;
                let joint_space = hand.get(joint);
                let joint_pose = frame.get_joint_pose(&joint_space, space)?;
                Some(Joint {
                    name,
                    pose: Pose::from_xr(&joint_pose),
                    radius: joint_pose.radius(),
                })
            })
            .collect()
    });
    InputSource {
        handedness: js_string(source, "handedness"),
        target_ray_mode: js_string(source, "targetRayMode"),
        profiles: source
            .profiles()
            .iter()
            .filter_map(|profile| profile.as_string())
            .collect(),
        target_ray: pose(&source.target_ray_space()),
        grip: source.grip_space().as_ref().and_then(pose),
        gamepad,
        hand,
    }
}

/// A string property of a JS object (WebXR enums arrive as strings).
pub fn js_string(object: &JsValue, key: &str) -> String {
    js_sys::Reflect::get(object, &JsValue::from_str(key))
        .ok()
        .and_then(|value| value.as_string())
        .unwrap_or_default()
}
