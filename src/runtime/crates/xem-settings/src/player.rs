//! The player's input on pad 1, shared by every host: platform events
//! (keys by `KeyboardEvent.code`, the first gamepad, the mouse) come in as
//! held inputs, resolve through the game bindings ([`crate::bindings`]) to
//! typed actions, and leave as the pad's buttons. Hosts feed it; nothing
//! here knows SDL, the DOM or a headset runtime.
use crate::bindings::{GameAction, GameBindings, Held, Mouse};
use std::collections::BTreeSet;

/// What the game's state says of the player's buttons now.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Context {
    /// The battle's attack inputs take the buttons (Attack bindings first).
    pub attack: bool,
    /// The field's: first-person inspection's toggle and what it holds back.
    pub field: xem_core::FieldControls,
    /// The game's button assignment, which actions resolve through.
    pub assignment: xem_core::action::Assignment,
}

#[derive(Default)]
pub struct PlayerInput {
    held: Held,
    pub mouse: Mouse,
}

impl PlayerInput {
    /// A key went down or up; false when no game binding names it.
    pub fn key(&mut self, bindings: &GameBindings, code: &str, down: bool) -> bool {
        if !bindings.keys().contains(code) {
            return false;
        }
        if down { self.held.keys.insert(code.to_owned()); } else { self.held.keys.remove(code); }
        true
    }

    /// Whether a key is held for the game.
    pub fn key_held(&self, code: &str) -> bool {
        self.held.keys.contains(code)
    }

    /// The first gamepad's standard buttons (1 = pressed, app chords already
    /// taken out) and axes (left X/Y, right X/Y).
    pub fn gamepad(&mut self, pressed: &[u8], axes: &[f32]) {
        self.held.set_gamepad(pressed, axes);
    }

    /// Release everything (focus lost, the panel took the input).
    pub fn clear(&mut self) {
        self.held = Held::default();
        self.mouse.clear();
    }

    /// The actions held at `now` (milliseconds).
    pub fn actions(&mut self, bindings: &GameBindings, now: f64, context: Context) -> BTreeSet<GameAction> {
        self.held.mouse = self.mouse.buttons();
        let mut actions = bindings.actions(&self.held, context.attack);
        actions.extend(self.mouse.turn(now));
        actions
    }

    /// The pad buttons held at `now` (milliseconds).
    pub fn buttons(&mut self, bindings: &GameBindings, now: f64, context: Context) -> u16 {
        xem_core::action::buttons_with(self.actions(bindings, now, context), &context.assignment)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use xem_core::pad::button;

    #[test]
    fn keys_the_gamepad_and_the_mouse_combine_through_the_bindings() {
        let bindings = GameBindings::default();
        let mut input = PlayerInput::default();
        assert!(input.key(&bindings, "KeyW", true));
        assert!(!input.key(&bindings, "KeyZ", true));
        let mut pad = [0; 16];
        pad[3] = 1;
        input.gamepad(&pad, &[0.0, 0.0, -0.9, 0.0]);
        input.mouse.button(0, true);
        let attack = Context { attack: true, ..Context::default() };
        assert_eq!(input.buttons(&bindings, 0.0, Context::default()), button::UP | button::TRIANGLE | button::L1 | button::CROSS);
        assert_eq!(input.buttons(&bindings, 0.0, attack), button::TRIANGLE | button::L1 | button::CROSS);
        input.clear();
        assert_eq!(input.buttons(&bindings, 0.0, Context::default()), 0);
    }
}
