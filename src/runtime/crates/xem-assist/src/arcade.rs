//! Control decks for the game's bespoke inputs in a headset (plan.md:
//! battles and bespoke minigames): Battling's bouts (the arena's fighters,
//! mode 4) and the world map's walking, Gears and vehicles (mode 3). Each
//! is played one-handed:
//!
//! - **A captured stick.** A pinch captures it at the pinching hand; the
//!   hand's travel from there (across and ahead, as the player faces) holds
//!   the D-pad's eighth beyond a small dead zone, and the next pinch lets
//!   it go. The thumb stays free while steering.
//! - **Thumb actions** while steering or not: a tap and the four swipes
//!   are the deck's main actions (Battling: A, B, Jump, Special, Dash;
//!   the world map: Confirm, the menu, the camera's turns).
//! - **Keys** beside the screen for everything, gazed at and tapped: plain
//!   keys press once, held keys (Battling's Dash and Charge, flight's
//!   throttle) toggle a held button with a visible lit state, and the world
//!   map's Board key presses L2 and R2 together (boarding or leaving a Gear
//!   without a two-handed chord).
//!
//! Everything held lets go when the deck's context ends, on tracking or
//! focus loss, and when the stick is released; Pause stays available.
use crate::gesture::{Gesture, Hand, Swipe};
use glam::Vec3;
use std::collections::BTreeSet;
use xem_core::action::GameAction;
use xem_core::context::Context;

/// The steering dead zone and full travel, metres.
const DEAD_ZONE: f32 = 0.03;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Deck {
    /// A Battling bout.
    Bout,
    /// The world map, open (not a scripted scene).
    WorldMap,
}

/// The deck a context plays with.
pub fn deck(ui: Context) -> Option<Deck> {
    match ui {
        Context::Arena { state: 1 } => Some(Deck::Bout),
        Context::WorldMap { scene, .. } if scene < 8 => Some(Deck::WorldMap),
        _ => None,
    }
}

/// A key: its label, the actions it presses (together), and whether it
/// toggles them held.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Key {
    pub label: &'static str,
    pub actions: &'static [GameAction],
    pub hold: bool,
}

/// A deck's keys, top to bottom.
pub fn keys(deck: Deck) -> &'static [Key] {
    use GameAction::*;
    match deck {
        // The bout's names: Cross is A, Square B, Triangle jumps, R1 the
        // special, Circle held dashes, L1 held charges.
        Deck::Bout => &[
            Key { label: "A", actions: &[Confirm], hold: false },
            Key { label: "B", actions: &[Menu], hold: false },
            Key { label: "JUMP", actions: &[Jump], hold: false },
            Key { label: "SPECIAL", actions: &[CameraRight], hold: false },
            Key { label: "DASH", actions: &[Cancel], hold: true },
            Key { label: "CHARGE", actions: &[CameraLeft], hold: true },
            Key { label: "START", actions: &[Start], hold: false },
        ],
        // Cross enters a path or interacts, Square the menu, Circle takes
        // off and lands, Triangle held throttles in flight, L2 and R2
        // together board or leave a Gear.
        Deck::WorldMap => &[
            Key { label: "OK", actions: &[Confirm], hold: false },
            Key { label: "MENU", actions: &[Menu], hold: false },
            Key { label: "LAND", actions: &[Cancel], hold: false },
            Key { label: "THROTTLE", actions: &[Jump], hold: true },
            Key { label: "BOARD", actions: &[L2, R2], hold: false },
            Key { label: "START", actions: &[Start], hold: false },
        ],
    }
}

/// The thumb's actions on a deck.
fn thumb(deck: Deck, gesture: &Gesture) -> Option<(GameAction, bool)> {
    use GameAction::*;
    Some(match (deck, gesture) {
        (Deck::Bout, Gesture::Tap { .. }) => (Confirm, false),
        (Deck::Bout, Gesture::Swipe { direction: Swipe::Left, .. }) => (Menu, false),
        (Deck::Bout, Gesture::Swipe { direction: Swipe::Up, .. }) => (Jump, false),
        (Deck::Bout, Gesture::Swipe { direction: Swipe::Right, .. }) => (CameraRight, false),
        (Deck::Bout, Gesture::Swipe { direction: Swipe::Down, .. }) => (Cancel, true),
        (Deck::WorldMap, Gesture::Tap { .. }) => (Confirm, false),
        (Deck::WorldMap, Gesture::Swipe { direction: Swipe::Down, .. }) => (Menu, false),
        (Deck::WorldMap, Gesture::Swipe { direction: Swipe::Left, .. }) => (CameraLeft, false),
        (Deck::WorldMap, Gesture::Swipe { direction: Swipe::Right, .. }) => (CameraRight, false),
        (Deck::WorldMap, Gesture::Swipe { direction: Swipe::Up, .. }) => (Jump, true),
        _ => return None,
    })
}

/// What a frame of the deck does: actions held now, and pressed once.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Played {
    pub held: BTreeSet<GameAction>,
    pub presses: Vec<Vec<GameAction>>,
}

#[derive(Clone, Debug, Default)]
pub struct Arcade {
    deck: Option<Deck>,
    /// The captured stick: the hand and where it was captured.
    stick: Option<(Hand, Vec3)>,
    /// Held toggles.
    pub toggled: BTreeSet<GameAction>,
    /// The stick's eighth last held (kept near a boundary).
    sector: Option<i32>,
}

impl Arcade {
    /// Whether the stick is captured.
    pub fn steering(&self) -> bool {
        self.stick.is_some()
    }

    /// Lets everything go.
    pub fn release(&mut self) {
        self.stick = None;
        self.toggled.clear();
        self.sector = None;
    }

    /// A deck key tapped: pressed once, or its hold toggled.
    pub fn key(&mut self, deck: Deck, index: usize) -> Played {
        let mut played = Played::default();
        if let Some(key) = keys(deck).get(index) {
            if key.hold {
                for action in key.actions {
                    if !self.toggled.remove(action) {
                        self.toggled.insert(*action);
                    }
                }
            } else {
                played.presses.push(key.actions.to_vec());
            }
        }
        played.held = self.toggled.clone();
        played
    }

    /// A frame: `palms` the hands' palm centres (tracked ones), `right` and
    /// `forward` the player's facing on the floor.
    pub fn update(&mut self, ui: Context, gestures: &[Gesture], palms: [Option<Vec3>; 2], right: Vec3, forward: Vec3) -> Option<Played> {
        let deck = deck(ui);
        if deck != self.deck {
            self.release();
            self.deck = deck;
        }
        let deck = deck?;
        let mut played = Played::default();
        for gesture in gestures {
            match gesture {
                Gesture::Pinch { hand } => {
                    self.stick = match self.stick {
                        Some(_) => None,
                        None => palms[hand.index()].map(|origin| (*hand, origin)),
                    };
                    self.sector = None;
                }
                Gesture::TrackingLost { .. } => self.release(),
                other => {
                    if let Some((action, hold)) = thumb(deck, other) {
                        if hold {
                            if !self.toggled.remove(&action) {
                                self.toggled.insert(action);
                            }
                        } else {
                            played.presses.push(vec![action]);
                        }
                    }
                }
            }
        }
        if let Some((hand, origin)) = self.stick {
            match palms[hand.index()] {
                None => self.release(),
                Some(palm) => {
                    let travel = palm - origin;
                    let (x, z) = (travel.dot(right), travel.dot(forward));
                    if (x * x + z * z).sqrt() > DEAD_ZONE {
                        // Screen directions: Right 0, Down 0x400 (toward the
                        // player), Left 0x800, Up 0xC00 (ahead).
                        let dir = (z.atan2(x) / std::f32::consts::TAU * 4096.0).round() as i32;
                        let dir = (-dir).rem_euclid(4096);
                        let nearest = ((dir + 0x100) / 0x200).rem_euclid(8);
                        let sector = match self.sector {
                            Some(p) if (((dir - p * 0x200 + 0x800).rem_euclid(0x1000)) - 0x800).abs() < 0x140 => p,
                            _ => nearest,
                        };
                        self.sector = Some(sector);
                        let [up, down, left, r] = xem_core::field::dpad_of_sector(sector);
                        for (on, action) in [(up, GameAction::Up), (down, GameAction::Down), (left, GameAction::Left), (r, GameAction::Right)] {
                            if on {
                                played.held.insert(action);
                            }
                        }
                    } else {
                        self.sector = None;
                    }
                }
            }
        }
        played.held.extend(self.toggled.iter().copied());
        Some(played)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const BOUT: Context = Context::Arena { state: 1 };

    fn palm(x: f32, z: f32) -> [Option<Vec3>; 2] {
        [None, Some(Vec3::new(x, 1.2, z))]
    }

    #[test]
    fn a_pinch_captures_the_stick_and_hand_travel_steers() {
        let mut arcade = Arcade::default();
        let (right, forward) = (Vec3::X, Vec3::NEG_Z);
        let pinch = [Gesture::Pinch { hand: Hand::Right }];
        assert!(arcade.update(BOUT, &pinch, palm(0.0, -0.3), right, forward).unwrap().held.is_empty());
        assert!(arcade.steering());
        let held = |arcade: &mut Arcade, x, z| arcade.update(BOUT, &[], palm(x, z), right, forward).unwrap().held;
        assert!(held(&mut arcade, 0.01, -0.3).is_empty(), "inside the dead zone");
        assert_eq!(held(&mut arcade, 0.08, -0.3), BTreeSet::from([GameAction::Right]));
        assert_eq!(held(&mut arcade, 0.0, -0.38), BTreeSet::from([GameAction::Up]), "ahead");
        assert_eq!(held(&mut arcade, -0.06, -0.24), BTreeSet::from([GameAction::Down, GameAction::Left]), "back and left");
        // The thumb acts while the hand steers.
        let tap = arcade.update(BOUT, &[Gesture::Tap { hand: Hand::Right }], palm(0.08, -0.3), right, forward).unwrap();
        assert_eq!(tap.presses, vec![vec![GameAction::Confirm]]);
        assert_eq!(tap.held, BTreeSet::from([GameAction::Right]));
        // Another pinch lets the stick go.
        assert!(arcade.update(BOUT, &pinch, palm(0.08, -0.3), right, forward).unwrap().held.is_empty());
        assert!(!arcade.steering());
    }

    #[test]
    fn held_keys_toggle_and_everything_lets_go_with_the_context() {
        let mut arcade = Arcade::default();
        arcade.update(BOUT, &[], palm(0.0, 0.0), Vec3::X, Vec3::NEG_Z);
        let dash = keys(Deck::Bout).iter().position(|k| k.label == "DASH").unwrap();
        assert_eq!(arcade.key(Deck::Bout, dash).held, BTreeSet::from([GameAction::Cancel]));
        let a = arcade.key(Deck::Bout, 0);
        assert_eq!(a.presses, vec![vec![GameAction::Confirm]]);
        assert_eq!(a.held, BTreeSet::from([GameAction::Cancel]), "the toggle stays held");
        // Leaving the bout lets go.
        assert!(arcade.update(Context::Walk, &[], palm(0.0, 0.0), Vec3::X, Vec3::NEG_Z).is_none());
        assert!(arcade.toggled.is_empty());
        // The world map's Board presses L2 and R2 together.
        let map = Context::WorldMap { movement: 2, scene: 0 };
        arcade.update(map, &[], palm(0.0, 0.0), Vec3::X, Vec3::NEG_Z);
        let board = keys(Deck::WorldMap).iter().position(|k| k.label == "BOARD").unwrap();
        assert_eq!(arcade.key(Deck::WorldMap, board).presses, vec![vec![GameAction::L2, GameAction::R2]]);
        assert!(deck(Context::WorldMap { movement: 2, scene: 9 }).is_none(), "scripted scenes keep the game's own controls");
    }
}
