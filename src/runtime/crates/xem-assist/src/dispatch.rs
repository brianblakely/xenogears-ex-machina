//! Gestures resolved in the game's interface context (plan.md, the shared
//! headset hand/gaze contract): each recognised gesture becomes, at once and
//! once, either app operations (Pause, inspection, settings, scaling) or
//! the game's legal commands: presses of typed actions, a selection of an
//! entry the gaze points at, or an assisted-movement intent for the
//! command layer. Hosts carry the results out; agents send the same
//! commands directly.
//!
//! | Context | Pinch | Thumb tap | Swipe up | down | left | right |
//! | --- | --- | --- | --- | --- | --- | --- |
//! | Field, walking | walk to the floor gazed at | approach and talk to the actor gazed at; on ground, stop | Jump | game menu, stop | camera left | camera right |
//! | Dialogue | | advance; a choice: the one gazed at | | | | |
//! | Menus | | select the entry gazed at (or the selected one) | later entries; next character outside a list | earlier entries; previous character | | back |
//! | Battle commands | | next page of the ring | the ring's upper slot | lower | left | right |
//! | Battle attacks | | | Weak | Fierce | Strong | end the sequence |
//! | Battle lists, targets | | select / confirm | later | earlier | | back |
use crate::gesture::{Gesture, Swipe};
use serde::Serialize;
use serde_json::{Value, json};
use xem_core::action::GameAction;
use xem_core::context::Context;
use xem_core::field::{Field, Reach};
use xem_core::menu::Targets;

/// What a gesture does.
#[derive(Clone, Debug, PartialEq, Serialize)]
#[serde(tag = "do", rename_all = "snake_case")]
pub enum Out {
    TogglePause,
    ToggleInspect,
    ToggleSettings,
    ScaleBegin,
    Scale { factor: f32 },
    ScaleEnd,
    /// Hand tracking was lost: release held input and pause (the player
    /// resumes explicitly).
    TrackingLost,
    /// Press these actions together, once.
    Press { actions: Vec<GameAction> },
    /// Select entry `index` of the interface's targets, then (`confirm`)
    /// press Confirm once it is selected.
    Select { index: usize, confirm: bool },
    /// A command for the command layer (`navigate`, `interact`,
    /// `assist_stop`, `shop_quantity`).
    Command { command: Value },
}

/// The screen pixels (320x224) the active pointer (eye gaze, head or a
/// hand's ray) meets on the game's picture, if it does.
pub type Aim = Option<(f32, f32)>;

/// Screen pixels within which an actor counts as pointed at.
const ACTOR_REACH: f32 = 18.0;

/// The ring's slots: the command page of each direction on each layer, on
/// foot and in a Gear (Right, Down, Left, Up; then the second layer).
const RING_PAGES: [[[u8; 4]; 2]; 2] = [[[1, 2, 3, 4], [7, 8, 9, 0xA]], [[0x10, 0x11, 0x12, 0x13], [0x15, 0x16, 0x17, 0x18]]];
const RING_DIRECTIONS: [GameAction; 4] = [GameAction::Right, GameAction::Down, GameAction::Left, GameAction::Up];

/// The ring slot a command page shows: (direction index, which layer).
pub fn ring_slot(page: u8) -> Option<(usize, usize)> {
    for unit in RING_PAGES {
        for (layer, pages) in unit.iter().enumerate() {
            if let Some(direction) = pages.iter().position(|p| *p == page) {
                return Some((direction, layer));
            }
        }
    }
    None
}

fn press(actions: impl IntoIterator<Item = GameAction>) -> Out {
    Out::Press { actions: actions.into_iter().collect() }
}

/// Resolves gestures. `field` is the field as read this frame (in a field),
/// `targets` the interface's selectable entries.
pub fn dispatch(gestures: &[Gesture], ui: Context, aim: Aim, field: Option<&Field>, targets: Option<&Targets>) -> Vec<Out> {
    let mut out = Vec::new();
    for gesture in gestures {
        match *gesture {
            Gesture::Pause { .. } => out.push(Out::TogglePause),
            Gesture::Inspect { .. } => out.push(Out::ToggleInspect),
            Gesture::Settings => out.push(Out::ToggleSettings),
            Gesture::ScaleBegin => out.push(Out::ScaleBegin),
            Gesture::Scale { factor } => out.push(Out::Scale { factor }),
            Gesture::ScaleEnd => out.push(Out::ScaleEnd),
            Gesture::TrackingLost { .. } => out.push(Out::TrackingLost),
            Gesture::Pinch { .. } => out.extend(pinch(ui, aim, field)),
            Gesture::Tap { .. } => out.extend(tap(ui, aim, field, targets)),
            Gesture::Swipe { direction, .. } => out.extend(swipe(direction, ui, aim, targets)),
        }
    }
    out
}

fn pinch(ui: Context, aim: Aim, field: Option<&Field>) -> Option<Out> {
    // Only walking: a pinch commits a destination, nothing else.
    if ui != Context::Walk {
        return None;
    }
    let hit = field?.pick(aim?)?;
    (hit.reach != Reach::Invalid).then(|| Out::Command { command: json!({"cmd": "navigate", "to": [hit.point.x, hit.point.z]}) })
}

fn tap(ui: Context, aim: Aim, field: Option<&Field>, targets: Option<&Targets>) -> Option<Out> {
    match ui {
        Context::Walk => {
            let field = field?;
            match aim.and_then(|aim| field.actor_at(aim, ACTOR_REACH)) {
                Some(actor) => Some(Out::Command { command: json!({"cmd": "interact", "actor": actor}) }),
                // Ground (or nothing): stop the route, choose nothing new.
                None => Some(Out::Command { command: json!({"cmd": "assist_stop"}) }),
            }
        }
        Context::Dialogue { choice: None } => Some(press([GameAction::Confirm])),
        // A choice answers only the one gazed at.
        Context::Dialogue { choice: Some(_) } => {
            let index = targets.zip(aim).and_then(|(t, aim)| t.at(aim))?;
            Some(Out::Select { index, confirm: true })
        }
        Context::Menu { .. } => Some(match targets.zip(aim).and_then(|(t, aim)| t.at(aim)) {
            Some(index) => Out::Select { index, confirm: true },
            None => press([GameAction::Confirm]),
        }),
        // Paging the ring: its highlighted direction again flips the layer.
        Context::Battle { page, attack: false, list: false, targeting: false, .. } => {
            let (direction, _) = ring_slot(page)?;
            Some(press([RING_DIRECTIONS[direction]]))
        }
        Context::Battle { attack: true, .. } => None,
        Context::Battle { .. } => Some(press([GameAction::Confirm])),
        Context::WorldMap { .. } => Some(press([GameAction::Confirm])),
        Context::Event | Context::None | Context::Arena { .. } => None,
    }
}

fn swipe(direction: Swipe, ui: Context, aim: Aim, targets: Option<&Targets>) -> Vec<Out> {
    match ui {
        Context::Walk => match direction {
            Swipe::Up => vec![press([GameAction::Jump])],
            Swipe::Left => vec![press([GameAction::CameraLeft])],
            Swipe::Right => vec![press([GameAction::CameraRight])],
            // The game menu; navigation stops and stays stopped.
            Swipe::Down => vec![Out::Command { command: json!({"cmd": "assist_stop"}) }, press([GameAction::Menu])],
        },
        Context::Menu { switch, .. } => match direction {
            Swipe::Right => vec![press([GameAction::Cancel])],
            Swipe::Left => vec![],
            Swipe::Up | Swipe::Down => {
                let over_list = match (targets, aim) {
                    (Some(t), Some(aim)) => t.at(aim).is_some() || bounds_contain(t, aim),
                    // Without known entries the list is wherever the gaze is.
                    (None, _) => true,
                    (Some(_), None) => true,
                };
                if switch && !over_list {
                    // Next / previous character (R1 / L1).
                    vec![press([if direction == Swipe::Up { GameAction::CameraRight } else { GameAction::CameraLeft }])]
                } else {
                    // Up moves the content up: later entries.
                    let later = match targets.map(|t| t.steps) {
                        Some(xem_core::menu::Steps::Ring { next, .. }) => next,
                        _ => GameAction::Down,
                    };
                    let earlier = opposite(later);
                    vec![press([if direction == Swipe::Up { later } else { earlier }])]
                }
            }
        },
        Context::Dialogue { choice: Some(_) } => match direction {
            Swipe::Up => vec![press([GameAction::Down])],
            Swipe::Down => vec![press([GameAction::Up])],
            _ => vec![],
        },
        Context::Battle { attack: true, .. } => vec![press([match direction {
            Swipe::Up => GameAction::AttackWeak,
            Swipe::Left => GameAction::AttackStrong,
            Swipe::Down => GameAction::AttackFierce,
            Swipe::Right => GameAction::AttackEnd,
        }])],
        Context::Battle { page, list: false, targeting: false, .. } => {
            // A slot: selected by its direction (unless it is the one
            // highlighted, which the direction would flip), then activated.
            let wanted = match direction { Swipe::Right => 0, Swipe::Down => 1, Swipe::Left => 2, Swipe::Up => 3 };
            match ring_slot(page) {
                Some((current, _)) if current == wanted => vec![press([GameAction::Confirm])],
                Some(_) => vec![press([RING_DIRECTIONS[wanted]]), press([GameAction::Confirm])],
                None => vec![],
            }
        }
        Context::Battle { .. } => match direction {
            Swipe::Right => vec![press([GameAction::Cancel])],
            Swipe::Up => vec![press([GameAction::Down])],
            Swipe::Down => vec![press([GameAction::Up])],
            Swipe::Left => vec![],
        },
        Context::WorldMap { .. } => match direction {
            Swipe::Down => vec![press([GameAction::Menu])],
            Swipe::Left => vec![press([GameAction::CameraLeft])],
            Swipe::Right => vec![press([GameAction::CameraRight])],
            Swipe::Up => vec![],
        },
        _ => vec![],
    }
}

fn bounds_contain(targets: &Targets, aim: (f32, f32)) -> bool {
    let (mut x0, mut y0, mut x1, mut y1) = (f32::MAX, f32::MAX, f32::MIN, f32::MIN);
    for e in &targets.entries {
        x0 = x0.min(e.rect.x);
        y0 = y0.min(e.rect.y);
        x1 = x1.max(e.rect.x + e.rect.width);
        y1 = y1.max(e.rect.y + e.rect.height);
    }
    aim.0 >= x0 && aim.0 < x1 && aim.1 >= y0 && aim.1 < y1
}

fn opposite(action: GameAction) -> GameAction {
    match action {
        GameAction::Up => GameAction::Down,
        GameAction::Down => GameAction::Up,
        GameAction::Left => GameAction::Right,
        GameAction::Right => GameAction::Left,
        other => other,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::gesture::Hand;
    use xem_core::menu::{Entry, Rect, Steps};

    const HAND: Hand = Hand::Right;

    fn battle(page: u8, attack: bool) -> Context {
        Context::Battle { page, attack, ap: 6, list: false, targeting: false }
    }

    #[test]
    fn the_command_ring_selects_slots_directly_and_a_tap_pages_it() {
        // On foot, the Attack slot (Right, page 1) highlighted.
        let swipe = |d| dispatch(&[Gesture::Swipe { hand: HAND, direction: d }], battle(1, false), None, None, None);
        assert_eq!(swipe(Swipe::Right), [press([GameAction::Confirm])], "the highlighted slot activates");
        assert_eq!(swipe(Swipe::Up), [press([GameAction::Up]), press([GameAction::Confirm])]);
        assert_eq!(dispatch(&[Gesture::Tap { hand: HAND }], battle(1, false), None, None, None), [press([GameAction::Right])], "its direction again flips the page");
        assert_eq!(ring_slot(0x17), Some((2, 1)));
    }

    #[test]
    fn the_attack_ring_is_fixed_and_a_tap_does_nothing() {
        let swipe = |d| dispatch(&[Gesture::Swipe { hand: HAND, direction: d }], battle(5, true), None, None, None);
        assert_eq!(swipe(Swipe::Up), [press([GameAction::AttackWeak])]);
        assert_eq!(swipe(Swipe::Left), [press([GameAction::AttackStrong])]);
        assert_eq!(swipe(Swipe::Down), [press([GameAction::AttackFierce])]);
        assert_eq!(swipe(Swipe::Right), [press([GameAction::AttackEnd])]);
        assert!(dispatch(&[Gesture::Tap { hand: HAND }], battle(5, true), None, None, None).is_empty());
    }

    fn list() -> Targets {
        let rect = |y: f32| Rect { x: 100.0, y, width: 100.0, height: 14.0 };
        Targets { what: "dialogue_choice", entries: vec![Entry { rect: rect(100.0), select: None }, Entry { rect: rect(114.0), select: None }], selected: Some(0), steps: Steps::Ring { count: 2, next: GameAction::Down } }
    }

    #[test]
    fn dialogue_advances_on_a_tap_and_a_choice_takes_the_one_gazed_at() {
        let tap = [Gesture::Tap { hand: HAND }];
        assert_eq!(dispatch(&tap, Context::Dialogue { choice: None }, None, None, None), [press([GameAction::Confirm])]);
        let choice = Context::Dialogue { choice: Some(xem_core::context::Choice { count: 2, index: 0 }) };
        assert_eq!(dispatch(&tap, choice, Some((150.0, 120.0)), None, Some(&list())), [Out::Select { index: 1, confirm: true }]);
        assert!(dispatch(&tap, choice, Some((10.0, 10.0)), None, Some(&list())).is_empty(), "no gaze, no answer");
    }

    #[test]
    fn menus_scroll_under_the_gaze_and_switch_characters_beside_it() {
        let menu = Context::Menu { screen: 0, cursor: 0, choice: 0, choices: 0, switch: true, quantities: false };
        let swipe = |d, aim| dispatch(&[Gesture::Swipe { hand: HAND, direction: d }], menu, aim, None, Some(&list()));
        assert_eq!(swipe(Swipe::Up, Some((150.0, 105.0))), [press([GameAction::Down])], "up: later entries");
        assert_eq!(swipe(Swipe::Down, Some((150.0, 105.0))), [press([GameAction::Up])]);
        assert_eq!(swipe(Swipe::Up, Some((10.0, 200.0))), [press([GameAction::CameraRight])], "next character");
        assert_eq!(swipe(Swipe::Down, Some((10.0, 200.0))), [press([GameAction::CameraLeft])]);
        assert_eq!(swipe(Swipe::Right, None), [press([GameAction::Cancel])], "back");
        // Without character switching the gaze outside the list still scrolls.
        let plain = Context::Menu { screen: 0, cursor: 0, choice: 0, choices: 0, switch: false, quantities: false };
        assert_eq!(dispatch(&[Gesture::Swipe { hand: HAND, direction: Swipe::Up }], plain, Some((10.0, 200.0)), None, Some(&list())), [press([GameAction::Down])]);
    }

    #[test]
    fn walking_maps_swipes_and_global_gestures_act_everywhere() {
        let swipe = |d| dispatch(&[Gesture::Swipe { hand: HAND, direction: d }], Context::Walk, None, None, None);
        assert_eq!(swipe(Swipe::Up), [press([GameAction::Jump])]);
        assert_eq!(swipe(Swipe::Down), [Out::Command { command: json!({"cmd": "assist_stop"}) }, press([GameAction::Menu])]);
        for ui in [Context::Walk, battle(5, true), Context::None] {
            assert_eq!(dispatch(&[Gesture::Pause { hand: HAND }, Gesture::Settings], ui, None, None, None), [Out::TogglePause, Out::ToggleSettings]);
        }
        // A pinch outside walking commits nothing.
        assert!(dispatch(&[Gesture::Pinch { hand: HAND }], Context::Dialogue { choice: None }, Some((1.0, 1.0)), None, None).is_empty());
    }
}
