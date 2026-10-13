//! Raw XR input: controller actions, and hand joints and eye gaze where the
//! runtime offers them. Nothing here interprets gestures; it reports states and
//! poses for an input adapter to map to commands.

use crate::math::Pose;
use crate::{Capabilities, XrError};
use openxr as xr;

/// One hand's controller state at the frame's predicted display time.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct HandInput {
    /// Whether any action of this hand is bound to an active input source.
    pub active: bool,
    pub aim: Option<Pose>,
    pub grip: Option<Pose>,
    pub trigger: f32,
    pub select: bool,
    pub menu: bool,
    /// Thumbstick x (right) and y (up), -1..1.
    pub thumbstick: [f32; 2],
    /// A or X (the lower face button), B or Y (the upper).
    pub primary: bool,
    pub secondary: bool,
    pub squeeze: bool,
    pub thumbstick_click: bool,
    /// The hand's standard pinch (XR_EXT_hand_interaction's `pinch_ext`):
    /// its value 0..1 and whether the hand is posed for it; `None` while
    /// nothing is bound to it.
    pub pinch: Option<(f32, bool)>,
    /// The runtime's thumb microgesture this frame
    /// (XR_META_hand_tracking_microgestures).
    pub microgesture: Option<Microgesture>,
    /// Microgestures are bound for this hand.
    pub microgestures: bool,
}

/// A thumb microgesture on the index finger.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Microgesture {
    Tap,
    Left,
    Right,
    /// Toward the fingertip.
    Forward,
    /// Toward the knuckle.
    Backward,
}

/// XR_META_hand_tracking_microgestures' components, in [`Microgesture`]
/// order.
const MICROGESTURES: [(&str, Microgesture); 5] = [
    ("tap_thumb_meta/click", Microgesture::Tap),
    ("swipe_left_meta/click", Microgesture::Left),
    ("swipe_right_meta/click", Microgesture::Right),
    ("swipe_forward_meta/click", Microgesture::Forward),
    ("swipe_backward_meta/click", Microgesture::Backward),
];

/// A tracked hand joint (XR_EXT_hand_tracking joint order).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Joint {
    pub pose: Pose,
    pub radius: f32,
}

/// Everything sampled for one frame. Hand joints and gaze are `None` when the
/// runtime does not offer them, and also when it does but nothing is tracked.
#[derive(Clone, Debug, Default)]
pub struct InputState {
    /// Left, right.
    pub hands: [HandInput; 2],
    pub hand_joints: [Option<Vec<Joint>>; 2],
    pub gaze: Option<Pose>,
    /// The interaction profile currently bound to each hand, if any.
    pub profiles: [Option<String>; 2],
}

pub(crate) struct Input {
    set: xr::ActionSet,
    hands: [xr::Path; 2],
    aim: xr::Action<xr::Posef>,
    grip: xr::Action<xr::Posef>,
    trigger: xr::Action<f32>,
    select: xr::Action<bool>,
    menu: xr::Action<bool>,
    thumbstick: xr::Action<xr::Vector2f>,
    primary: xr::Action<bool>,
    secondary: xr::Action<bool>,
    squeeze: xr::Action<f32>,
    thumbstick_click: xr::Action<bool>,
    aim_spaces: [xr::Space; 2],
    grip_spaces: [xr::Space; 2],
    gaze: Option<(xr::Action<xr::Posef>, xr::Space)>,
    trackers: Option<[xr::HandTracker; 2]>,
    /// XR_EXT_hand_interaction's pinch value and readiness.
    pinch: Option<(xr::Action<f32>, xr::Action<bool>)>,
    /// XR_META_hand_tracking_microgestures' clicks.
    microgestures: Vec<(xr::Action<bool>, Microgesture)>,
}

const HANDS: [&str; 2] = ["/user/hand/left", "/user/hand/right"];

impl Input {
    pub(crate) fn new(
        instance: &xr::Instance,
        session: &xr::Session<xr::Vulkan>,
        capabilities: &Capabilities,
    ) -> Result<Self, XrError> {
        let path = |name: &str| {
            instance
                .string_to_path(name)
                .map_err(XrError::xr("xrStringToPath"))
        };
        let hands = [path(HANDS[0])?, path(HANDS[1])?];
        let set = instance
            .create_action_set("xem", "Xenogears Ex Machina", 0)
            .map_err(XrError::xr("xrCreateActionSet"))?;
        let create = "xrCreateAction";
        let aim = set
            .create_action::<xr::Posef>("aim", "Aim", &hands)
            .map_err(XrError::xr(create))?;
        let grip = set
            .create_action::<xr::Posef>("grip", "Grip", &hands)
            .map_err(XrError::xr(create))?;
        let trigger = set
            .create_action::<f32>("trigger", "Trigger", &hands)
            .map_err(XrError::xr(create))?;
        let select = set
            .create_action::<bool>("select", "Select", &hands)
            .map_err(XrError::xr(create))?;
        let menu = set
            .create_action::<bool>("menu", "Menu", &hands)
            .map_err(XrError::xr(create))?;
        let thumbstick = set
            .create_action::<xr::Vector2f>("thumbstick", "Thumbstick", &hands)
            .map_err(XrError::xr(create))?;
        let primary = set
            .create_action::<bool>("primary", "Lower face button", &hands)
            .map_err(XrError::xr(create))?;
        let secondary = set
            .create_action::<bool>("secondary", "Upper face button", &hands)
            .map_err(XrError::xr(create))?;
        let squeeze = set
            .create_action::<f32>("squeeze", "Squeeze", &hands)
            .map_err(XrError::xr(create))?;
        let thumbstick_click = set
            .create_action::<bool>("thumbstick_click", "Thumbstick click", &hands)
            .map_err(XrError::xr(create))?;

        let suggest = |profile: &str, bindings: &[(&str, Binding)]| -> Result<(), XrError> {
            let mut list = Vec::new();
            for &(input, action) in bindings {
                for hand in HANDS {
                    if action.hand_only.is_some_and(|only| only != hand) {
                        continue;
                    }
                    let binding = path(&format!("{hand}/input/{input}"))?;
                    list.push(match action.kind {
                        Kind::Aim => xr::Binding::new(&aim, binding),
                        Kind::Grip => xr::Binding::new(&grip, binding),
                        Kind::Trigger => xr::Binding::new(&trigger, binding),
                        Kind::Select => xr::Binding::new(&select, binding),
                        Kind::Menu => xr::Binding::new(&menu, binding),
                        Kind::Thumbstick => xr::Binding::new(&thumbstick, binding),
                        Kind::Primary => xr::Binding::new(&primary, binding),
                        Kind::Secondary => xr::Binding::new(&secondary, binding),
                        Kind::Squeeze => xr::Binding::new(&squeeze, binding),
                        Kind::ThumbstickClick => xr::Binding::new(&thumbstick_click, binding),
                    });
                }
            }
            instance
                .suggest_interaction_profile_bindings(path(profile)?, &list)
                .map_err(XrError::xr("xrSuggestInteractionProfileBindings"))
        };
        suggest(
            "/interaction_profiles/khr/simple_controller",
            &[
                ("aim/pose", Binding::both(Kind::Aim)),
                ("grip/pose", Binding::both(Kind::Grip)),
                ("select/click", Binding::both(Kind::Select)),
                ("select/click", Binding::both(Kind::Trigger)),
                ("menu/click", Binding::both(Kind::Menu)),
            ],
        )?;
        suggest(
            "/interaction_profiles/oculus/touch_controller",
            &[
                ("aim/pose", Binding::both(Kind::Aim)),
                ("grip/pose", Binding::both(Kind::Grip)),
                ("trigger/value", Binding::both(Kind::Trigger)),
                ("trigger/value", Binding::both(Kind::Select)),
                // The right controller's menu button is reserved for the system.
                ("menu/click", Binding::left(Kind::Menu)),
                ("thumbstick", Binding::both(Kind::Thumbstick)),
                ("thumbstick/click", Binding::both(Kind::ThumbstickClick)),
                ("squeeze/value", Binding::both(Kind::Squeeze)),
                ("x/click", Binding::left(Kind::Primary)),
                ("y/click", Binding::left(Kind::Secondary)),
                ("a/click", Binding::right(Kind::Primary)),
                ("b/click", Binding::right(Kind::Secondary)),
            ],
        )?;

        // The hands' standard profile: their aim and grip, the pinch, and
        // (where offered) the thumb's microgestures, in one suggestion.
        let mut pinch = None;
        let mut microgestures = Vec::new();
        if capabilities.hand_interaction {
            let value = set
                .create_action::<f32>("pinch", "Pinch", &hands)
                .map_err(XrError::xr(create))?;
            let ready = set
                .create_action::<bool>("pinch_ready", "Pinch ready", &hands)
                .map_err(XrError::xr(create))?;
            if capabilities.microgestures {
                for (component, gesture) in MICROGESTURES {
                    let name = component.split('/').next().unwrap_or(component);
                    let action = set
                        .create_action::<bool>(name, name, &hands)
                        .map_err(XrError::xr(create))?;
                    microgestures.push((action, gesture));
                }
            }
            let mut list = Vec::new();
            for hand in HANDS {
                list.push(xr::Binding::new(&aim, path(&format!("{hand}/input/aim/pose"))?));
                list.push(xr::Binding::new(&grip, path(&format!("{hand}/input/grip/pose"))?));
                list.push(xr::Binding::new(&value, path(&format!("{hand}/input/pinch_ext/value"))?));
                list.push(xr::Binding::new(&ready, path(&format!("{hand}/input/pinch_ext/ready_ext"))?));
                for ((action, _), (component, _)) in microgestures.iter().zip(MICROGESTURES) {
                    list.push(xr::Binding::new(action, path(&format!("{hand}/input/{component}"))?));
                }
            }
            instance
                .suggest_interaction_profile_bindings(path("/interaction_profiles/ext/hand_interaction_ext")?, &list)
                .map_err(XrError::xr("xrSuggestInteractionProfileBindings"))?;
            pinch = Some((value, ready));
        }

        let gaze = if capabilities.eye_gaze {
            let action = set
                .create_action::<xr::Posef>("gaze", "Eye gaze", &[])
                .map_err(XrError::xr(create))?;
            instance
                .suggest_interaction_profile_bindings(
                    path("/interaction_profiles/ext/eye_gaze_interaction")?,
                    &[xr::Binding::new(
                        &action,
                        path("/user/eyes_ext/input/gaze_ext/pose")?,
                    )],
                )
                .map_err(XrError::xr("xrSuggestInteractionProfileBindings"))?;
            Some(action)
        } else {
            None
        };

        session
            .attach_action_sets(&[&set])
            .map_err(XrError::xr("xrAttachSessionActionSets"))?;
        let space = |action: &xr::Action<xr::Posef>, hand: xr::Path| {
            action
                .create_space(session, hand, xr::Posef::IDENTITY)
                .map_err(XrError::xr("xrCreateActionSpace"))
        };
        let aim_spaces = [space(&aim, hands[0])?, space(&aim, hands[1])?];
        let grip_spaces = [space(&grip, hands[0])?, space(&grip, hands[1])?];
        let gaze = match gaze {
            Some(action) => {
                let space = space(&action, xr::Path::NULL)?;
                Some((action, space))
            }
            None => None,
        };
        let trackers = if capabilities.hand_tracking {
            let tracker = |hand| {
                session
                    .create_hand_tracker(hand)
                    .map_err(XrError::xr("xrCreateHandTrackerEXT"))
            };
            Some([tracker(xr::Hand::LEFT)?, tracker(xr::Hand::RIGHT)?])
        } else {
            None
        };
        Ok(Self {
            set,
            hands,
            aim,
            grip,
            trigger,
            select,
            menu,
            thumbstick,
            primary,
            secondary,
            squeeze,
            thumbstick_click,
            aim_spaces,
            grip_spaces,
            gaze,
            trackers,
            pinch,
            microgestures,
        })
    }

    /// Syncs the actions (they only update while the session is focused) and
    /// samples every source relative to `base` at `time`.
    pub(crate) fn sample(
        &self,
        session: &xr::Session<xr::Vulkan>,
        base: &xr::Space,
        time: xr::Time,
    ) -> Result<InputState, XrError> {
        session
            .sync_actions(&[xr::ActiveActionSet::new(&self.set)])
            .map_err(XrError::xr("xrSyncActions"))?;
        let locate = |space: &xr::Space| -> Result<Option<Pose>, XrError> {
            let location = space
                .locate(base, time)
                .map_err(XrError::xr("xrLocateSpace"))?;
            let valid =
                xr::SpaceLocationFlags::POSITION_VALID | xr::SpaceLocationFlags::ORIENTATION_VALID;
            Ok(location
                .location_flags
                .contains(valid)
                .then(|| Pose::from_xr(location.pose)))
        };
        let get = "xrGetActionState";
        let mut state = InputState::default();
        for (index, &hand) in self.hands.iter().enumerate() {
            let trigger = self
                .trigger
                .state(session, hand)
                .map_err(XrError::xr(get))?;
            let select = self.select.state(session, hand).map_err(XrError::xr(get))?;
            let menu = self.menu.state(session, hand).map_err(XrError::xr(get))?;
            let thumbstick = self.thumbstick.state(session, hand).map_err(XrError::xr(get))?;
            let primary = self.primary.state(session, hand).map_err(XrError::xr(get))?;
            let secondary = self.secondary.state(session, hand).map_err(XrError::xr(get))?;
            let squeeze = self.squeeze.state(session, hand).map_err(XrError::xr(get))?;
            let thumbstick_click = self.thumbstick_click.state(session, hand).map_err(XrError::xr(get))?;
            let aim_active = self
                .aim
                .is_active(session, hand)
                .map_err(XrError::xr(get))?;
            let grip_active = self
                .grip
                .is_active(session, hand)
                .map_err(XrError::xr(get))?;
            state.hands[index] = HandInput {
                active: aim_active
                    || grip_active
                    || trigger.is_active
                    || select.is_active
                    || menu.is_active,
                aim: if aim_active {
                    locate(&self.aim_spaces[index])?
                } else {
                    None
                },
                grip: if grip_active {
                    locate(&self.grip_spaces[index])?
                } else {
                    None
                },
                trigger: trigger.current_state,
                select: select.current_state,
                menu: menu.current_state,
                thumbstick: [thumbstick.current_state.x, thumbstick.current_state.y],
                primary: primary.current_state,
                secondary: secondary.current_state,
                squeeze: squeeze.current_state > 0.5,
                thumbstick_click: thumbstick_click.current_state,
                pinch: None,
                microgesture: None,
                microgestures: false,
            };
            if let Some((value, ready)) = &self.pinch {
                let value = value.state(session, hand).map_err(XrError::xr(get))?;
                let ready = ready.state(session, hand).map_err(XrError::xr(get))?;
                if value.is_active {
                    state.hands[index].pinch = Some((value.current_state, ready.current_state));
                    state.hands[index].active = true;
                }
            }
            for (action, gesture) in &self.microgestures {
                let click = action.state(session, hand).map_err(XrError::xr(get))?;
                if click.is_active {
                    state.hands[index].microgestures = true;
                    // A click's start is the gesture.
                    if click.current_state && click.changed_since_last_sync && state.hands[index].microgesture.is_none() {
                        state.hands[index].microgesture = Some(*gesture);
                    }
                }
            }
            state.profiles[index] = match session.current_interaction_profile(hand) {
                Ok(profile) if profile != xr::Path::NULL => {
                    session.instance().path_to_string(profile).ok()
                }
                _ => None,
            };
        }
        if let Some((action, space)) = &self.gaze
            && action
                .is_active(session, xr::Path::NULL)
                .map_err(XrError::xr(get))?
        {
            state.gaze = locate(space)?;
        }
        if let Some(trackers) = &self.trackers {
            for (index, tracker) in trackers.iter().enumerate() {
                state.hand_joints[index] = base
                    .locate_hand_joints(tracker, time)
                    .map_err(XrError::xr("xrLocateHandJointsEXT"))?
                    .map(|joints| {
                        joints
                            .iter()
                            .map(|joint| Joint {
                                pose: Pose::from_xr(joint.pose),
                                radius: joint.radius,
                            })
                            .collect()
                    });
            }
        }
        Ok(state)
    }
}

#[derive(Clone, Copy)]
enum Kind {
    Aim,
    Grip,
    Trigger,
    Select,
    Menu,
    Thumbstick,
    Primary,
    Secondary,
    Squeeze,
    ThumbstickClick,
}

#[derive(Clone, Copy)]
struct Binding {
    kind: Kind,
    hand_only: Option<&'static str>,
}

impl Binding {
    fn both(kind: Kind) -> Self {
        Self {
            kind,
            hand_only: None,
        }
    }

    fn left(kind: Kind) -> Self {
        Self {
            kind,
            hand_only: Some(HANDS[0]),
        }
    }

    fn right(kind: Kind) -> Self {
        Self {
            kind,
            hand_only: Some(HANDS[1]),
        }
    }
}
