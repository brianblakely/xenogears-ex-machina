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
}

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
    aim_spaces: [xr::Space; 2],
    grip_spaces: [xr::Space; 2],
    gaze: Option<(xr::Action<xr::Posef>, xr::Space)>,
    trackers: Option<[xr::HandTracker; 2]>,
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
            ],
        )?;

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
            aim_spaces,
            grip_spaces,
            gaze,
            trackers,
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
            };
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
}
