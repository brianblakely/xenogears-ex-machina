//! The game's typed actions: what every human binding resolves to and what
//! agents name directly (`hold`, `act`). Each is one of the pad's buttons
//! as the BIOS driver reports them, so an action changes nothing but the
//! pad the game reads. The game swaps a digital pad's face buttons
//! (`pad_start_controllers`: Circle and Cross, Triangle and Square), so on
//! the pad Cross confirms and talks, Circle cancels and runs while held,
//! Triangle jumps, Square opens the menu and L1/R1 turn the field camera.
//! While the battle's attack inputs take the buttons, Triangle, Square and
//! Cross are the light, medium and heavy attacks and Circle ends the
//! sequence: the attack actions name those buttons.

use crate::pad::button;
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
pub enum GameAction {
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Cancel,
    /// Held while moving.
    Run,
    Jump,
    Menu,
    CameraLeft,
    CameraRight,
    Start,
    Select,
    L2,
    R2,
    /// The attack inputs' light (Weak), medium (Strong) and heavy (Fierce)
    /// attacks, and ending the sequence.
    AttackWeak,
    AttackStrong,
    AttackFierce,
    AttackEnd,
}

impl GameAction {
    pub const ALL: [GameAction; 19] = [
        GameAction::Up, GameAction::Down, GameAction::Left, GameAction::Right,
        GameAction::Confirm, GameAction::Cancel, GameAction::Run, GameAction::Jump, GameAction::Menu,
        GameAction::CameraLeft, GameAction::CameraRight, GameAction::Start, GameAction::Select,
        GameAction::L2, GameAction::R2,
        GameAction::AttackWeak, GameAction::AttackStrong, GameAction::AttackFierce, GameAction::AttackEnd,
    ];

    /// The game's own button slot the action means (`pad_button_bits`
    /// order: 0x20, 0x40, 0x10, 0x80, L1, L2, R1, R2 as the game reads its
    /// pad), for the face and shoulder buttons the game reassigns.
    fn slot(self) -> Option<usize> {
        match self {
            GameAction::Confirm | GameAction::AttackFierce => Some(0),
            GameAction::Cancel | GameAction::Run | GameAction::AttackEnd => Some(1),
            GameAction::Menu | GameAction::AttackStrong => Some(2),
            GameAction::Jump | GameAction::AttackWeak => Some(3),
            GameAction::CameraLeft => Some(4),
            GameAction::L2 => Some(5),
            GameAction::CameraRight => Some(6),
            GameAction::R2 => Some(7),
            _ => None,
        }
    }

    /// The pad button the action holds under the game's default button
    /// assignment.
    pub fn button(self) -> u16 {
        self.button_with(&Assignment::default())
    }

    /// The pad button the action holds under `assignment`.
    pub fn button_with(self, assignment: &Assignment) -> u16 {
        match self.slot() {
            Some(slot) => assignment.0.iter().position(|s| usize::from(*s) == slot)
                .or_else(|| Assignment::default().0.iter().position(|s| usize::from(*s) == slot))
                .map_or(0, |physical| SLOT_BUTTONS[physical]),
            None => match self {
                GameAction::Up => button::UP,
                GameAction::Down => button::DOWN,
                GameAction::Left => button::LEFT,
                GameAction::Right => button::RIGHT,
                GameAction::Start => button::START,
                _ => button::SELECT,
            },
        }
    }

    /// The action's name in commands and settings (`Confirm`).
    pub fn name(self) -> &'static str {
        match self {
            GameAction::Up => "Up",
            GameAction::Down => "Down",
            GameAction::Left => "Left",
            GameAction::Right => "Right",
            GameAction::Confirm => "Confirm",
            GameAction::Cancel => "Cancel",
            GameAction::Run => "Run",
            GameAction::Jump => "Jump",
            GameAction::Menu => "Menu",
            GameAction::CameraLeft => "CameraLeft",
            GameAction::CameraRight => "CameraRight",
            GameAction::Start => "Start",
            GameAction::Select => "Select",
            GameAction::L2 => "L2",
            GameAction::R2 => "R2",
            GameAction::AttackWeak => "AttackWeak",
            GameAction::AttackStrong => "AttackStrong",
            GameAction::AttackFierce => "AttackFierce",
            GameAction::AttackEnd => "AttackEnd",
        }
    }

    pub fn from_name(name: &str) -> Option<GameAction> {
        GameAction::ALL.into_iter().find(|action| action.name() == name)
    }

    /// Whether the action names one of the attack inputs' buttons.
    pub fn attack(self) -> bool {
        matches!(self, GameAction::AttackWeak | GameAction::AttackStrong | GameAction::AttackFierce | GameAction::AttackEnd)
    }
}

/// The pad's buttons in the game's slot order (`pad_button_bits`: the
/// physical Circle, Cross, Triangle, Square, L1, L2, R1, R2).
const SLOT_BUTTONS: [u16; 8] = [button::CIRCLE, button::CROSS, button::TRIANGLE, button::SQUARE, button::L1, button::L2, button::R1, button::R2];

/// The game's button assignment (`pad_button_assignment`, 0x80050238): the
/// game slot each physical slot's button reads as (`pad_remap_buttons`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Assignment(pub [u8; 8]);

/// Where the game keeps it.
pub const ASSIGNMENT_ADDRESS: u32 = 0x8005_0238;

impl Default for Assignment {
    /// What `pad_init` sets: Circle and Cross, Triangle and Square swapped.
    fn default() -> Self {
        Assignment([1, 0, 3, 2, 4, 5, 6, 7])
    }
}

impl Assignment {
    /// The table as read from the game; one that is not a permutation of
    /// the eight slots (before `pad_init` ran, or memory reused) is the
    /// default.
    pub fn from_bytes(bytes: [u8; 8]) -> Self {
        let mut seen = [false; 8];
        for b in bytes {
            match seen.get_mut(usize::from(b)) {
                Some(s) if !*s => *s = true,
                _ => return Assignment::default(),
            }
        }
        // The resident's initial table is the identity until pad_init runs.
        if bytes == [0, 1, 2, 3, 4, 5, 6, 7] { return Assignment::default(); }
        Assignment(bytes)
    }
}

/// The pad buttons a set of actions holds under the default assignment.
pub fn buttons(actions: impl IntoIterator<Item = GameAction>) -> u16 {
    buttons_with(actions, &Assignment::default())
}

/// The pad buttons a set of actions holds under `assignment`.
pub fn buttons_with(actions: impl IntoIterator<Item = GameAction>, assignment: &Assignment) -> u16 {
    actions.into_iter().fold(0, |bits, action| bits | action.button_with(assignment))
}

/// Actions named in a command's JSON list.
pub fn actions_of(value: &serde_json::Value) -> Result<Vec<GameAction>, String> {
    value.as_array().ok_or("actions must be a list of names")?.iter().map(|name| {
        let name = name.as_str().ok_or("action names are strings")?;
        GameAction::from_name(name).ok_or_else(|| {
            let names: Vec<_> = GameAction::ALL.iter().map(|a| a.name()).collect();
            format!("unknown action {name} (actions: {})", names.join(", "))
        })
    }).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn names_round_trip_and_serialize_as_names() {
        for action in GameAction::ALL {
            assert_eq!(GameAction::from_name(action.name()), Some(action));
            assert_eq!(serde_json::to_value(action).unwrap(), serde_json::json!(action.name()));
        }
    }

    #[test]
    fn actions_hold_the_game_buttons_they_mean() {
        assert_eq!(buttons([GameAction::Confirm, GameAction::Up]), button::CROSS | button::UP);
        assert_eq!(GameAction::Run.button(), GameAction::Cancel.button());
        assert_eq!(GameAction::AttackWeak.button(), GameAction::Jump.button());
        assert_eq!(GameAction::AttackStrong.button(), GameAction::Menu.button());
        assert_eq!(GameAction::AttackFierce.button(), GameAction::Confirm.button());
        assert_eq!(GameAction::AttackEnd.button(), GameAction::Cancel.button());
        assert!(actions_of(&serde_json::json!(["Jump", "Nope"])).unwrap_err().contains("unknown action Nope"));
        assert_eq!(buttons([GameAction::Jump, GameAction::Menu, GameAction::Cancel]), button::TRIANGLE | button::SQUARE | button::CIRCLE);
        assert_eq!(buttons([GameAction::CameraLeft, GameAction::CameraRight, GameAction::L2, GameAction::R2]),
            button::L1 | button::R1 | button::L2 | button::R2);
    }

    #[test]
    fn actions_follow_the_games_button_assignment() {
        // A configuration that swaps nothing: the physical Circle confirms.
        let identity = Assignment([0, 1, 2, 3, 4, 5, 6, 7]);
        assert_eq!(GameAction::Confirm.button_with(&identity), button::CIRCLE);
        assert_eq!(GameAction::Jump.button_with(&identity), button::SQUARE);
        // A table read from memory before pad_init, or not a permutation.
        assert_eq!(Assignment::from_bytes([0, 1, 2, 3, 4, 5, 6, 7]), Assignment::default());
        assert_eq!(Assignment::from_bytes([1, 1, 3, 2, 4, 5, 6, 7]), Assignment::default());
        let shoulders = Assignment::from_bytes([1, 0, 3, 2, 6, 5, 4, 7]);
        assert_eq!(GameAction::CameraLeft.button_with(&shoulders), button::R1);
        assert_eq!(GameAction::Up.button_with(&shoulders), button::UP);
    }
}
