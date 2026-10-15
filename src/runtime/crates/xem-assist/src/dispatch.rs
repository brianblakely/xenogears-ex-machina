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
//! | Battle lists | | select the entry gazed at (or the selected one) | later | earlier | | back |
//! | Battle targets | | the target gazed at (or the selected one) | up | down | left | back |
use crate::gesture::{Gesture, Swipe};
use serde::Serialize;
use serde_json::{Value, json};
use xem_core::action::GameAction;
use xem_core::context::Context;
use xem_core::field::{Field, Reach, V3};
use xem_core::menu::{Targets, ring_slot};

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
    /// Choose entry `index` of the interface's targets: a button is
    /// pressed; anything else is selected, then (`confirm`) confirmed with
    /// Confirm once it is selected.
    Select { index: usize, confirm: bool },
    /// A command for the command layer (`navigate`, `interact`,
    /// `assist_stop`, `shop_quantity`).
    Command { command: Value },
    /// A key of a control deck beside the picture was tapped (or pinched).
    Key { key: u32 },
    /// AUTO was tapped: auto-advance dialogue goes on or off (an app
    /// setting, never the game's input).
    ToggleAutoAdvance,
}

/// Where the active pointer (eye gaze, head or a hand's ray) meets the
/// game, if it does.
pub type Aim = Option<Pointed>;

/// What a pointer meets.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Pointed {
    /// A pixel (320x224, with the wider framing's sides) of the game's
    /// picture: the floor and the actors are where the game's camera shows
    /// them there.
    Screen((f32, f32)),
    /// A pixel of the game's interface alone (a diorama's pane): its
    /// windows, lists and choices, with no scene under it.
    Pane((f32, f32)),
    /// A ray through a diorama's table, in the game's world.
    World(WorldRay),
}

impl Pointed {
    /// The pixel of the picture or the pane pointed at.
    pub fn screen(self) -> Option<(f32, f32)> {
        match self {
            Pointed::Screen(at) | Pointed::Pane(at) => Some(at),
            Pointed::World(_) => None,
        }
    }
}

/// A ray through a diorama in the game's world (world units, y down): its
/// part inside the table's volume, from `origin` to `within` lengths of
/// `direction` (the scene is drawn there only), and how near it an actor
/// counts as pointed at (`reach` world units: a few centimetres on the
/// table, whatever its scale).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct WorldRay {
    pub origin: V3,
    pub direction: V3,
    pub within: f32,
    pub reach: f32,
}

/// Screen pixels within which an actor counts as pointed at.
const ACTOR_REACH: f32 = 18.0;

/// The ring's directions in `ring_slot` order.
const RING_DIRECTIONS: [GameAction; 4] = [GameAction::Right, GameAction::Down, GameAction::Left, GameAction::Up];

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

/// A touch screen's tap (or a click that stands for one, or a headset's
/// pointer pinching) where `at` points: walking, an actor is approached and
/// talked to, the floor walked to (a tap is both the pointer's pinch and
/// its tap there); an entry of a menu, a choice or a battle is chosen
/// ([`choose`]); a dialogue advances. Elsewhere the deck's controls play.
pub fn tap_at(at: Pointed, ui: Context, field: Option<&Field>, targets: Option<&Targets>) -> Option<Out> {
    match ui {
        Context::Walk => {
            let field = field?;
            if let Some(actor) = actor_pointed(field, at) {
                return Some(Out::Command { command: json!({"cmd": "interact", "actor": actor}) });
            }
            pinch(ui, Some(at), Some(field))
        }
        Context::Dialogue { choice: None } => Some(press([GameAction::Confirm])),
        Context::Dialogue { choice: Some(_) } | Context::Menu { .. } | Context::Battle { .. } => {
            let targets = targets?;
            Some(Out::Select { index: targets.at(at.screen()?)?, confirm: targets.confirm })
        }
        _ => None,
    }
}

/// The floor point `at` picks: on the game's picture through its camera, or
/// along a ray through a diorama (a pane shows no floor).
fn floor_pointed(field: &Field, at: Pointed) -> Option<xem_core::field::Hit> {
    match at {
        Pointed::Screen(at) => field.pick(at),
        Pointed::World(ray) => field.pick_ray(ray.origin, ray.direction, ray.within),
        Pointed::Pane(_) => None,
    }
}

/// The actor `at` points at, as [`floor_pointed`] finds the floor.
fn actor_pointed(field: &Field, at: Pointed) -> Option<usize> {
    match at {
        Pointed::Screen(at) => field.actor_at(at, ACTOR_REACH),
        Pointed::World(ray) => field.actor_near_ray(ray.origin, ray.direction, ray.within, ray.reach),
        Pointed::Pane(_) => None,
    }
}

fn pinch(ui: Context, aim: Aim, field: Option<&Field>) -> Option<Out> {
    // Only walking: a pinch commits a destination, nothing else.
    if ui != Context::Walk {
        return None;
    }
    let hit = floor_pointed(field?, aim?)?;
    (hit.reach != Reach::Invalid).then(|| Out::Command { command: json!({"cmd": "navigate", "to": [hit.point.x, hit.point.z]}) })
}

fn tap(ui: Context, aim: Aim, field: Option<&Field>, targets: Option<&Targets>) -> Option<Out> {
    let at = aim.and_then(Pointed::screen);
    match ui {
        Context::Walk => {
            let field = field?;
            match aim.and_then(|aim| actor_pointed(field, aim)) {
                Some(actor) => Some(Out::Command { command: json!({"cmd": "interact", "actor": actor}) }),
                // Ground (or nothing): stop the route, choose nothing new.
                None => Some(Out::Command { command: json!({"cmd": "assist_stop"}) }),
            }
        }
        Context::Dialogue { choice: None } => Some(press([GameAction::Confirm])),
        // A choice answers only the one gazed at.
        Context::Dialogue { choice: Some(_) } => {
            let index = targets.zip(at).and_then(|(t, at)| t.at(at))?;
            Some(Out::Select { index, confirm: true })
        }
        Context::Menu { .. } | Context::Battle { targeting: true, .. } | Context::Battle { list: true, .. } => Some(match targets.zip(at).and_then(|(t, at)| Some((t, t.at(at)?))) {
            Some((targets, index)) => Out::Select { index, confirm: targets.confirm },
            None => press([GameAction::Confirm]),
        }),
        // Paging the ring: its highlighted direction again flips the layer.
        Context::Battle { page, attack: false, list: false, targeting: false, .. } => {
            let (direction, _) = ring_slot(page)?;
            Some(press([RING_DIRECTIONS[direction]]))
        }
        Context::Battle { attack: true, .. } => None,
        Context::WorldMap { .. } => Some(press([GameAction::Confirm])),
        Context::Event { .. } | Context::None | Context::Arena { .. } => None,
    }
}

fn swipe(direction: Swipe, ui: Context, aim: Aim, targets: Option<&Targets>) -> Vec<Out> {
    let aim = aim.and_then(Pointed::screen);
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
        // Targets: the directions move to the nearest that way.
        Context::Battle { targeting: true, .. } => match direction {
            Swipe::Right => vec![press([GameAction::Cancel])],
            Swipe::Up => vec![press([GameAction::Up])],
            Swipe::Down => vec![press([GameAction::Down])],
            Swipe::Left => vec![press([GameAction::Left])],
        },
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

/// The entries' bounds: left, top, right, bottom.
fn bounds(targets: &Targets) -> (f32, f32, f32, f32) {
    let (mut x0, mut y0, mut x1, mut y1) = (f32::MAX, f32::MAX, f32::MIN, f32::MIN);
    for e in &targets.entries {
        x0 = x0.min(e.rect.x);
        y0 = y0.min(e.rect.y);
        x1 = x1.max(e.rect.x + e.rect.width);
        y1 = y1.max(e.rect.y + e.rect.height);
    }
    (x0, y0, x1, y1)
}

fn bounds_contain(targets: &Targets, aim: (f32, f32)) -> bool {
    let (x0, y0, x1, y1) = bounds(targets);
    aim.0 >= x0 && aim.0 < x1 && aim.1 >= y0 && aim.1 < y1
}

/// The list a thumb swipe up or down would scroll with the gaze on `aim`
/// (its bounds), as [`dispatch`] decides it: a menu's list (on a
/// character screen only under the gaze; beside it the swipe changes the
/// character) or a dialogue's choice.
pub fn receiving_region(ui: Context, targets: &Targets, aim: (f32, f32)) -> Option<(f32, f32, f32, f32)> {
    let scrolls = match ui {
        Context::Menu { switch, .. } => targets.what != "yes_no" && (!switch || targets.at(aim).is_some() || bounds_contain(targets, aim)),
        Context::Dialogue { choice: Some(_) } | Context::Battle { list: true, .. } => true,
        _ => false,
    };
    (scrolls && !targets.entries.is_empty()).then(|| bounds(targets))
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
    }

    #[test]
    fn a_touch_chooses_battle_entries_as_a_click_does() {
        let entry = |x: f32, press: Option<GameAction>, ground: Option<xem_core::menu::Ground>| Entry { rect: Rect { x, y: 0.0, width: 10.0, height: 10.0 }, select: None, press, twice: false, ground };
        let target = xem_core::menu::Ground { slot: 3, x: 0, z: 0, selectable: true };
        let attack = Targets {
            what: "battle_attack",
            entries: vec![entry(0.0, Some(GameAction::AttackWeak), None), entry(20.0, None, Some(target))],
            selected: Some(1),
            steps: Steps::Compass,
            confirm: false,
        };
        assert_eq!(tap_at(Pointed::Screen((5.0, 5.0)), battle(5, true), None, Some(&attack)), Some(Out::Select { index: 0, confirm: false }), "an attack is chosen (pressed)");
        assert_eq!(tap_at(Pointed::Screen((25.0, 5.0)), battle(5, true), None, Some(&attack)), Some(Out::Select { index: 1, confirm: false }), "its target only selected");
        assert_eq!(tap_at(Pointed::Screen((50.0, 5.0)), battle(5, true), None, Some(&attack)), None, "nothing there");
        let ring = Targets { what: "battle_ring", entries: vec![Entry { select: Some(GameAction::Down), ..entry(0.0, None, None) }], selected: None, steps: Steps::Direct, confirm: true };
        assert_eq!(tap_at(Pointed::Screen((5.0, 5.0)), battle(1, false), None, Some(&ring)), Some(Out::Select { index: 0, confirm: true }), "a slot is activated");
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
        let entry = |y| Entry { rect: rect(y), select: None, press: None, twice: false, ground: None };
        Targets { what: "dialogue_choice", entries: vec![entry(100.0), entry(114.0)], selected: Some(0), steps: Steps::Ring { count: 2, next: GameAction::Down }, confirm: true }
    }

    #[test]
    fn the_receiving_region_is_the_list_a_swipe_scrolls() {
        let menu = |switch| Context::Menu { screen: 0, cursor: 0, choice: 0, choices: 2, commands: false, open: true, switch, quantities: false };
        let bounds = Some((100.0, 100.0, 200.0, 128.0));
        assert_eq!(receiving_region(menu(false), &list(), (10.0, 10.0)), bounds, "the only list, wherever the gaze is");
        assert_eq!(receiving_region(menu(true), &list(), (150.0, 110.0)), bounds, "on a character screen, under the gaze");
        assert_eq!(receiving_region(menu(true), &list(), (10.0, 10.0)), None, "beside it a swipe changes the character");
        let yes_no = Targets { what: "yes_no", steps: Steps::Direct, ..list() };
        assert_eq!(receiving_region(menu(false), &yes_no, (150.0, 110.0)), None);
        assert_eq!(receiving_region(Context::Dialogue { choice: Some(xem_core::context::Choice { count: 2, index: 0 }) }, &list(), (0.0, 0.0)), bounds);
    }

    #[test]
    fn dialogue_advances_on_a_tap_and_a_choice_takes_the_one_gazed_at() {
        let tap = [Gesture::Tap { hand: HAND }];
        assert_eq!(dispatch(&tap, Context::Dialogue { choice: None }, None, None, None), [press([GameAction::Confirm])]);
        let choice = Context::Dialogue { choice: Some(xem_core::context::Choice { count: 2, index: 0 }) };
        assert_eq!(dispatch(&tap, choice, Some(Pointed::Screen((150.0, 120.0))), None, Some(&list())), [Out::Select { index: 1, confirm: true }]);
        assert!(dispatch(&tap, choice, Some(Pointed::Screen((10.0, 10.0))), None, Some(&list())).is_empty(), "no gaze, no answer");
    }

    #[test]
    fn menus_scroll_under_the_gaze_and_switch_characters_beside_it() {
        let menu = Context::Menu { screen: 0, cursor: 0, choice: 0, choices: 0, commands: false, open: true, switch: true, quantities: false };
        let swipe = |d, aim| dispatch(&[Gesture::Swipe { hand: HAND, direction: d }], menu, aim, None, Some(&list()));
        assert_eq!(swipe(Swipe::Up, Some(Pointed::Screen((150.0, 105.0)))), [press([GameAction::Down])], "up: later entries");
        assert_eq!(swipe(Swipe::Down, Some(Pointed::Screen((150.0, 105.0)))), [press([GameAction::Up])]);
        assert_eq!(swipe(Swipe::Up, Some(Pointed::Screen((10.0, 200.0)))), [press([GameAction::CameraRight])], "next character");
        assert_eq!(swipe(Swipe::Down, Some(Pointed::Screen((10.0, 200.0)))), [press([GameAction::CameraLeft])]);
        assert_eq!(swipe(Swipe::Right, None), [press([GameAction::Cancel])], "back");
        // Without character switching the gaze outside the list still scrolls.
        let plain = Context::Menu { screen: 0, cursor: 0, choice: 0, choices: 0, commands: false, open: true, switch: false, quantities: false };
        assert_eq!(dispatch(&[Gesture::Swipe { hand: HAND, direction: Swipe::Up }], plain, Some(Pointed::Screen((10.0, 200.0))), None, Some(&list())), [press([GameAction::Down])]);
    }

    /// A floor 400 units across, the player near its front, a villager at
    /// (0, 0, 300) (world y down).
    fn floor() -> Field {
        use xem_core::field::{Actor, Camera, Layer, Triangle};
        let layer = Layer {
            vertices: vec![V3::new(-200.0, 0.0, 0.0), V3::new(200.0, 0.0, 0.0), V3::new(-200.0, 0.0, 400.0), V3::new(200.0, 0.0, 400.0)],
            triangles: vec![Triangle { v: [0, 2, 1], n: [-1, 1, -1], attribute: 0 }, Triangle { v: [2, 3, 1], n: [-1, -1, 0], attribute: 0 }],
        };
        let actor = |index: usize, z: f32| Actor { index, position: V3::new(0.0, 0.0, z), layer: 0, triangle: [0; 4], flags: 0, layer_flags: 0, radius: 16, height: 60, heading: 0, heading_goal: 0 };
        Field {
            camera: Camera { m: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]], t: [0.0, -100.0, 0.0], h: 256.0, angle: 0 },
            layers: vec![layer], attributes: vec![0], actors: vec![Some(actor(0, 20.0)), Some(actor(1, 300.0))], player: Some(0),
            alt_dpad: false, party_processing: false, dialogue_open: false,
        }
    }

    #[test]
    fn through_a_diorama_a_pinch_walks_to_the_floor_under_the_ray_and_a_tap_talks_to_the_actor_by_it() {
        let field = floor();
        let down = |x: f32, z: f32| Some(Pointed::World(WorldRay { origin: V3::new(x, -300.0, z), direction: V3::new(0.0, 400.0, 0.0), within: 1.0, reach: 25.0 }));
        let pinch = [Gesture::Pinch { hand: HAND }];
        let tap = [Gesture::Tap { hand: HAND }];
        let walked = dispatch(&pinch, Context::Walk, down(-80.0, 150.0), Some(&field), None);
        assert_eq!(walked, [Out::Command { command: json!({"cmd": "navigate", "to": [-80.0, 150.0]}) }]);
        // Beside the villager: a tap talks to them; away from anyone it stops.
        assert_eq!(dispatch(&tap, Context::Walk, down(10.0, 310.0), Some(&field), None), [Out::Command { command: json!({"cmd": "interact", "actor": 1}) }]);
        assert_eq!(dispatch(&tap, Context::Walk, down(-150.0, 100.0), Some(&field), None), [Out::Command { command: json!({"cmd": "assist_stop"}) }]);
        // The pointer's pinch (no microgestures) is a tap there: the villager.
        assert_eq!(tap_at(down(10.0, 310.0).unwrap(), Context::Walk, Some(&field), None), Some(Out::Command { command: json!({"cmd": "interact", "actor": 1}) }));
        // A ray whose part on the table stops short of the floor picks nothing.
        let short = Some(Pointed::World(WorldRay { origin: V3::new(-80.0, -300.0, 150.0), direction: V3::new(0.0, 100.0, 0.0), within: 1.0, reach: 25.0 }));
        assert!(dispatch(&pinch, Context::Walk, short, Some(&field), None).is_empty());
        // The pane shows no floor: nothing walks there; its choices answer.
        assert!(dispatch(&pinch, Context::Walk, Some(Pointed::Pane((160.0, 112.0))), Some(&field), None).is_empty());
        assert_eq!(dispatch(&tap, Context::Walk, Some(Pointed::Pane((160.0, 112.0))), Some(&field), None), [Out::Command { command: json!({"cmd": "assist_stop"}) }]);
        let choice = Context::Dialogue { choice: Some(xem_core::context::Choice { count: 2, index: 0 }) };
        assert_eq!(dispatch(&tap, choice, Some(Pointed::Pane((150.0, 120.0))), None, Some(&list())), [Out::Select { index: 1, confirm: true }]);
        assert!(dispatch(&tap, choice, down(150.0, 120.0), None, Some(&list())).is_empty(), "a ray through the table answers no choice");
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
        assert!(dispatch(&[Gesture::Pinch { hand: HAND }], Context::Dialogue { choice: None }, Some(Pointed::Screen((1.0, 1.0))), None, None).is_empty());
    }
}
