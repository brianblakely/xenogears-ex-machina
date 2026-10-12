//! Pad 1 from the keyboard and mouse, shared by all hosts. Keys are
//! `KeyboardEvent.code` names. The game remaps a digital pad's face buttons
//! (`pad_start_controllers`: Circle and Cross, Triangle and Square swap), so
//! on the physical pad Cross confirms and talks, Circle cancels and runs
//! while held, Triangle jumps, Square opens the menu and L1/R1 turn the
//! field camera. While the battle's attack inputs take the buttons
//! (`xem_game_combo_input`), W/A/S are the light, medium and heavy attacks
//! (Triangle, Square, Cross) and D or F cancels (Circle).
use xem_core::pad::button;

const ANYWHERE: [(&str, u16); 12] = [
    ("Tab", button::SQUARE),
    ("ShiftLeft", button::CIRCLE),
    ("ShiftRight", button::CIRCLE),
    ("Space", button::TRIANGLE),
    ("KeyE", button::CROSS),
    ("KeyF", button::CIRCLE),
    ("ArrowLeft", button::L1),
    ("ArrowRight", button::R1),
    ("Enter", button::START),
    ("Backspace", button::SELECT),
    ("Digit1", button::L2),
    ("Digit2", button::R2),
];
const MOVEMENT: [(&str, u16); 4] = [
    ("KeyW", button::UP),
    ("KeyA", button::LEFT),
    ("KeyS", button::DOWN),
    ("KeyD", button::RIGHT),
];
const COMBO: [(&str, u16); 4] = [
    ("KeyW", button::TRIANGLE),
    ("KeyA", button::SQUARE),
    ("KeyS", button::CROSS),
    ("KeyD", button::CIRCLE),
];

/// The pad bit a key holds (`combo`: the attack inputs have the buttons),
/// if it is a game key.
pub fn key_button(code: &str, combo: bool) -> Option<u16> {
    let layout: &[(&str, u16)] = if combo { &COMBO } else { &MOVEMENT };
    ANYWHERE.iter().chain(layout).find(|(name, _)| *name == code).map(|(_, bit)| *bit)
}

/// Every key either layout uses.
pub fn game_keys() -> impl Iterator<Item = &'static str> {
    ANYWHERE.iter().chain(&MOVEMENT).map(|(name, _)| *name)
}

/// Mouse milliseconds of camera turn per pixel moved, and the most it banks.
const TURN_MS_PER_PIXEL: f64 = 4.0;
const MAX_TURN_MS: f64 = 400.0;

/// Mouse buttons and sideways motion: left confirms (Cross), right cancels
/// (Circle), and
/// moving turns the field camera, holding L1 or R1 for as long as the
/// motion lasts.
#[derive(Default)]
pub struct Mouse {
    left: bool,
    right: bool,
    /// Turn still owed in milliseconds (negative turns left).
    turn: f64,
    last: Option<f64>,
}

impl Mouse {
    /// A button went down or up (0 left, 2 right, as `MouseEvent.button`).
    pub fn button(&mut self, button: i32, down: bool) -> bool {
        match button {
            0 => self.left = down,
            2 => self.right = down,
            _ => return false,
        }
        true
    }

    /// Sideways motion in pixels (positive to the right).
    pub fn moved(&mut self, dx: f32) {
        self.turn = (self.turn + f64::from(dx) * TURN_MS_PER_PIXEL).clamp(-MAX_TURN_MS, MAX_TURN_MS);
    }

    pub fn clear(&mut self) {
        *self = Self::default();
    }

    /// The buttons held at `now` (milliseconds), spending the owed turn.
    pub fn buttons(&mut self, now: f64) -> u16 {
        let elapsed = self.last.map_or(0.0, |last| (now - last).max(0.0));
        self.last = Some(now);
        let mut bits = 0;
        if self.left { bits |= button::CROSS; }
        if self.right { bits |= button::CIRCLE; }
        if self.turn > 0.0 { bits |= button::R1; }
        if self.turn < 0.0 { bits |= button::L1; }
        self.turn = self.turn.signum() * (self.turn.abs() - elapsed).max(0.0);
        bits
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn wasd_moves_outside_the_attack_inputs_and_attacks_in_them() {
        assert_eq!(key_button("KeyW", false), Some(button::UP));
        assert_eq!(key_button("KeyW", true), Some(button::TRIANGLE));
        assert_eq!(key_button("KeyA", true), Some(button::SQUARE));
        assert_eq!(key_button("KeyS", true), Some(button::CROSS));
        assert_eq!(key_button("KeyD", true), Some(button::CIRCLE));
        assert_eq!(key_button("KeyF", false), Some(button::CIRCLE));
        assert_eq!(key_button("KeyE", true), Some(button::CROSS));
        assert_eq!(key_button("KeyZ", false), None);
        assert!(game_keys().all(|code| key_button(code, true).is_some()));
    }
    #[test]
    fn mouse_motion_holds_a_shoulder_button_while_the_turn_lasts() {
        let mut mouse = Mouse::default();
        mouse.moved(5.0);
        assert_eq!(mouse.buttons(0.0), button::R1);
        assert_eq!(mouse.buttons(16.0), button::R1);
        assert_eq!(mouse.buttons(40.0), button::R1);
        assert_eq!(mouse.buttons(41.0), 0);
        mouse.moved(-1.0);
        mouse.button(0, true);
        mouse.button(2, true);
        assert_eq!(mouse.buttons(42.0), button::L1 | button::CROSS | button::CIRCLE);
    }
}
