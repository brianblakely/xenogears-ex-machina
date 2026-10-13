//! The game's bindings, shared by all hosts: the inputs (keyboard
//! `KeyboardEvent.code` names, gamepad buttons and stick directions in the
//! standard mapping, mouse buttons) that hold each typed action
//! ([`GameAction`], `xem_core::action`), in two contexts. **Play** is
//! everywhere; while the battle's attack inputs take the buttons
//! (`xem_game_combo_input`) **Attack** comes first, and an input it binds
//! means its attack action there (W/A/S the light, medium and heavy
//! attacks, D ending the sequence) instead of what Play binds it to.
//! Headset controllers and touch decks resolve through the same tables: a
//! headset's controllers are the gamepad (`xr-standard` layout), touch
//! buttons name their action. Agents name actions directly.
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
pub use xem_core::action::GameAction;

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub enum InputContext {
    Play,
    Attack,
}

/// Every action as the panel lists it: its context, the action and its title.
pub const GAME_ACTION_ROWS: [(InputContext, GameAction, &str); 19] = [
    (InputContext::Play, GameAction::Up, "Up"),
    (InputContext::Play, GameAction::Down, "Down"),
    (InputContext::Play, GameAction::Left, "Left"),
    (InputContext::Play, GameAction::Right, "Right"),
    (InputContext::Play, GameAction::Confirm, "Confirm, talk"),
    (InputContext::Play, GameAction::Cancel, "Cancel"),
    (InputContext::Play, GameAction::Run, "Run (held)"),
    (InputContext::Play, GameAction::Jump, "Jump"),
    (InputContext::Play, GameAction::Menu, "Menu"),
    (InputContext::Play, GameAction::CameraLeft, "Camera left"),
    (InputContext::Play, GameAction::CameraRight, "Camera right"),
    (InputContext::Play, GameAction::Start, "Start (pause)"),
    (InputContext::Play, GameAction::Select, "Select"),
    (InputContext::Play, GameAction::L2, "L2"),
    (InputContext::Play, GameAction::R2, "R2"),
    (InputContext::Attack, GameAction::AttackWeak, "Attack: Weak"),
    (InputContext::Attack, GameAction::AttackStrong, "Attack: Strong"),
    (InputContext::Attack, GameAction::AttackFierce, "Attack: Fierce"),
    (InputContext::Attack, GameAction::AttackEnd, "Attack: End the sequence"),
];

/// Gamepad inputs: the standard mapping's 16 buttons (`controls::PAD_NAMES`)
/// and each stick's four directions past half travel.
pub const GAMEPAD_INPUTS: [&str; 24] = [
    "South", "East", "West", "North", "L1", "R1", "LT", "RT", "Back", "Start", "L3", "R3", "Up", "Down", "Left", "Right",
    "LStickUp", "LStickDown", "LStickLeft", "LStickRight", "RStickUp", "RStickDown", "RStickLeft", "RStickRight",
];
pub const MOUSE_INPUTS: [&str; 3] = ["MouseLeft", "MouseRight", "MouseMiddle"];

/// The inputs that hold one action: any one of them does.
#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct GameBinding {
    pub keyboard: Vec<String>,
    pub gamepad: Vec<String>,
    pub mouse: Vec<String>,
}

impl GameBinding {
    /// From space- or comma-separated input names.
    pub fn parse(keyboard: &str, gamepad: &str, mouse: &str) -> Self {
        let split = |s: &str| s.split([' ', ',']).map(str::trim).filter(|s| !s.is_empty()).map(str::to_owned).collect();
        Self { keyboard: split(keyboard), gamepad: split(gamepad), mouse: split(mouse) }
    }

    pub fn validate(&self) -> Result<(), String> {
        for list in [&self.keyboard, &self.gamepad, &self.mouse] {
            if list.len() > 4 || list.iter().collect::<BTreeSet<_>>().len() != list.len() {
                return Err("an action has at most four distinct inputs of each kind".into());
            }
        }
        if let Some(key) = self.keyboard.iter().find(|k| !crate::controls::keyboard_code(k)) {
            return Err(format!("{key} is not a KeyboardEvent.code name such as KeyW or Space"));
        }
        if let Some(input) = self.gamepad.iter().find(|k| !GAMEPAD_INPUTS.contains(&k.as_str())) {
            return Err(format!("{input} is not a gamepad input ({})", GAMEPAD_INPUTS.join(", ")));
        }
        if let Some(input) = self.mouse.iter().find(|k| !MOUSE_INPUTS.contains(&k.as_str())) {
            return Err(format!("{input} is not a mouse button ({})", MOUSE_INPUTS.join(", ")));
        }
        Ok(())
    }

    fn inputs(&self) -> impl Iterator<Item = &str> {
        self.keyboard.iter().chain(&self.gamepad).chain(&self.mouse).map(String::as_str)
    }
}

/// Every context's bindings.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct GameBindings {
    pub play: BTreeMap<GameAction, GameBinding>,
    pub attack: BTreeMap<GameAction, GameBinding>,
}

impl Default for GameBindings {
    fn default() -> Self {
        use GameAction::*;
        let table = |rows: &[(GameAction, &str, &str, &str)]| {
            rows.iter().map(|(action, k, g, m)| (*action, GameBinding::parse(k, g, m))).collect()
        };
        Self {
            play: table(&[
                (Up, "KeyW", "Up LStickUp", ""),
                (Down, "KeyS", "Down LStickDown", ""),
                (Left, "KeyA", "Left LStickLeft", ""),
                (Right, "KeyD", "Right LStickRight", ""),
                (Confirm, "KeyE", "South", "MouseLeft"),
                (Cancel, "KeyF", "East", "MouseRight"),
                (Run, "ShiftLeft ShiftRight", "East", ""),
                (Jump, "Space", "North", ""),
                (Menu, "Tab", "West", ""),
                (CameraLeft, "ArrowLeft", "L1 RStickLeft", ""),
                (CameraRight, "ArrowRight", "R1 RStickRight", ""),
                (Start, "Enter", "Start", ""),
                (Select, "Backspace", "Back", ""),
                (L2, "Digit1", "LT", ""),
                (R2, "Digit2", "RT", ""),
            ]),
            attack: table(&[
                (AttackWeak, "KeyW", "North", ""),
                (AttackStrong, "KeyA", "West", ""),
                (AttackFierce, "KeyS", "South", ""),
                (AttackEnd, "KeyD", "East", ""),
            ]),
        }
    }
}

impl GameBindings {
    pub fn context(&self, context: InputContext) -> &BTreeMap<GameAction, GameBinding> {
        match context {
            InputContext::Play => &self.play,
            InputContext::Attack => &self.attack,
        }
    }

    pub fn context_mut(&mut self, context: InputContext) -> &mut BTreeMap<GameAction, GameBinding> {
        match context {
            InputContext::Play => &mut self.play,
            InputContext::Attack => &mut self.attack,
        }
    }

    /// Valid names, and no input in a context holding two different buttons.
    pub fn validate(&self) -> Result<(), String> {
        for context in [InputContext::Play, InputContext::Attack] {
            let mut seen: BTreeMap<&str, GameAction> = BTreeMap::new();
            for (action, binding) in self.context(context) {
                if action.attack() != (context == InputContext::Attack) {
                    return Err(format!("{} belongs to the {} context", action.name(),
                        if action.attack() { "Attack" } else { "Play" }));
                }
                binding.validate()?;
                for input in binding.inputs() {
                    if let Some(other) = seen.insert(input, *action) && other.button() != action.button() {
                        return Err(format!("{input} holds both {} and {}", other.name(), action.name()));
                    }
                }
            }
        }
        Ok(())
    }

    /// Every input either context names.
    pub fn inputs(&self) -> BTreeSet<&str> {
        self.play.values().chain(self.attack.values()).flat_map(GameBinding::inputs).collect()
    }

    /// Every key either context names.
    pub fn keys(&self) -> BTreeSet<&str> {
        self.play.values().chain(self.attack.values()).flat_map(|b| b.keyboard.iter().map(String::as_str)).collect()
    }

    /// The actions the held inputs hold (`attack`: the attack inputs have
    /// the buttons, and Attack's bindings come first).
    pub fn actions(&self, held: &Held, attack: bool) -> BTreeSet<GameAction> {
        let mut actions = BTreeSet::new();
        let held_input = |input: &str| held.keys.contains(input)
            || GAMEPAD_INPUTS.iter().position(|n| *n == input).is_some_and(|i| held.gamepad & (1 << i) != 0)
            || MOUSE_INPUTS.iter().position(|n| *n == input).is_some_and(|i| held.mouse & (1 << i) != 0);
        let attack_inputs: BTreeSet<&str> = if attack { self.attack.values().flat_map(GameBinding::inputs).collect() } else { BTreeSet::new() };
        if attack {
            for (action, binding) in &self.attack {
                if binding.inputs().any(held_input) { actions.insert(*action); }
            }
        }
        for (action, binding) in &self.play {
            if binding.inputs().any(|input| !attack_inputs.contains(input) && held_input(input)) {
                actions.insert(*action);
            }
        }
        actions
    }

    /// The pad buttons the held inputs hold.
    pub fn buttons(&self, held: &Held, attack: bool) -> u16 {
        xem_core::action::buttons(self.actions(held, attack))
    }

    /// The first inputs of each kind bound to `action` in Play (or Attack
    /// for the attack actions), for showing.
    pub fn binding(&self, action: GameAction) -> Option<&GameBinding> {
        self.context(if action.attack() { InputContext::Attack } else { InputContext::Play }).get(&action)
    }
}

/// The inputs held now.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Held {
    pub keys: BTreeSet<String>,
    /// Bits in [`GAMEPAD_INPUTS`] order.
    pub gamepad: u32,
    /// Bits in [`MOUSE_INPUTS`] order.
    pub mouse: u8,
}

impl Held {
    /// The gamepad's standard buttons (1 = pressed) and its axes (left X/Y,
    /// right X/Y, negative Y up); each stick direction holds past half travel.
    pub fn set_gamepad(&mut self, pressed: &[u8], axes: &[f32]) {
        let axis = |i| axes.get(i).copied().unwrap_or(0.0);
        let mut bits = pressed.iter().take(16).enumerate().filter(|(_, p)| **p != 0).fold(0u32, |n, (i, _)| n | (1 << i));
        for (i, on) in [axis(1) < -0.5, axis(1) > 0.5, axis(0) < -0.5, axis(0) > 0.5,
            axis(3) < -0.5, axis(3) > 0.5, axis(2) < -0.5, axis(2) > 0.5].into_iter().enumerate() {
            if on { bits |= 1 << (16 + i); }
        }
        self.gamepad = bits;
    }
}

/// Mouse milliseconds of camera turn per pixel moved, and the most it banks.
const TURN_MS_PER_PIXEL: f64 = 4.0;
const MAX_TURN_MS: f64 = 400.0;

/// The mouse: its buttons, and (under the mouse camera setting) sideways
/// motion turning the field camera, holding CameraLeft or CameraRight for
/// as long as the motion lasts.
#[derive(Default)]
pub struct Mouse {
    buttons: u8,
    /// Turn still owed in milliseconds (negative turns left).
    turn: f64,
    last: Option<f64>,
}

impl Mouse {
    /// A button went down or up (0 left, 1 middle, 2 right, as
    /// `MouseEvent.button`); false for other buttons.
    pub fn button(&mut self, button: i32, down: bool) -> bool {
        let bit = match button {
            0 => 1,
            2 => 2,
            1 => 4,
            _ => return false,
        };
        if down { self.buttons |= bit } else { self.buttons &= !bit }
        true
    }

    /// Sideways motion in pixels (positive to the right).
    pub fn moved(&mut self, dx: f32) {
        self.turn = (self.turn + f64::from(dx) * TURN_MS_PER_PIXEL).clamp(-MAX_TURN_MS, MAX_TURN_MS);
    }

    pub fn clear(&mut self) {
        *self = Self::default();
    }

    /// The buttons held ([`MOUSE_INPUTS`] bits).
    pub fn buttons(&self) -> u8 {
        self.buttons
    }

    /// The camera turn the motion holds at `now` (milliseconds), spending
    /// what it owes.
    pub fn turn(&mut self, now: f64) -> Option<GameAction> {
        let elapsed = self.last.map_or(0.0, |last| (now - last).max(0.0));
        self.last = Some(now);
        let turn = (self.turn > 0.0).then_some(GameAction::CameraRight).or((self.turn < 0.0).then_some(GameAction::CameraLeft));
        self.turn = self.turn.signum() * (self.turn.abs() - elapsed).max(0.0);
        turn
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use xem_core::pad::button;

    fn keys(names: &[&str]) -> Held {
        Held { keys: names.iter().map(|k| k.to_string()).collect(), ..Held::default() }
    }

    #[test]
    fn defaults_validate_and_wasd_attacks_only_in_the_attack_inputs() {
        let bindings = GameBindings::default();
        bindings.validate().unwrap();
        assert_eq!(bindings.buttons(&keys(&["KeyW"]), false), button::UP);
        assert_eq!(bindings.buttons(&keys(&["KeyW"]), true), button::TRIANGLE);
        assert_eq!(bindings.buttons(&keys(&["KeyA"]), true), button::SQUARE);
        assert_eq!(bindings.buttons(&keys(&["KeyS"]), true), button::CROSS);
        assert_eq!(bindings.buttons(&keys(&["KeyD"]), true), button::CIRCLE);
        assert_eq!(bindings.buttons(&keys(&["KeyF"]), true), button::CIRCLE);
        assert_eq!(bindings.buttons(&keys(&["KeyE", "Space"]), false), button::CROSS | button::TRIANGLE);
        assert_eq!(bindings.buttons(&keys(&["KeyZ"]), false), 0);
        assert_eq!(bindings.actions(&keys(&["ShiftLeft"]), false), BTreeSet::from([GameAction::Run]));
    }

    #[test]
    fn sticks_and_mouse_buttons_hold_their_actions() {
        let bindings = GameBindings::default();
        let mut held = Held::default();
        let mut physical = [0; 16];
        physical[0] = 1;
        held.set_gamepad(&physical, &[-0.8, -0.8, 0.8, 0.0]);
        assert_eq!(bindings.buttons(&held, false), button::CROSS | button::UP | button::LEFT | button::R1);
        held.set_gamepad(&physical, &[0.5, -0.5, 0.25, 1.0]);
        assert_eq!(bindings.buttons(&held, false), button::CROSS);
        held.set_gamepad(&[], &[]);
        let mut mouse = Mouse::default();
        mouse.button(0, true);
        mouse.button(2, true);
        held.mouse = mouse.buttons();
        assert_eq!(bindings.buttons(&held, false), button::CROSS | button::CIRCLE);
        mouse.moved(5.0);
        assert_eq!(mouse.turn(0.0), Some(GameAction::CameraRight));
        assert_eq!(mouse.turn(40.0), Some(GameAction::CameraRight));
        assert_eq!(mouse.turn(41.0), None);
        mouse.moved(-1.0);
        assert_eq!(mouse.turn(42.0), Some(GameAction::CameraLeft));
    }

    #[test]
    fn rebinding_moves_an_action_and_conflicts_are_refused() {
        let mut bindings = GameBindings::default();
        bindings.play.insert(GameAction::Jump, GameBinding::parse("KeyJ", "North", ""));
        bindings.validate().unwrap();
        assert_eq!(bindings.buttons(&keys(&["KeyJ"]), false), button::TRIANGLE);
        assert_eq!(bindings.buttons(&keys(&["Space"]), false), 0);
        bindings.play.insert(GameAction::Jump, GameBinding::parse("KeyE", "", ""));
        assert!(bindings.validate().unwrap_err().contains("KeyE holds both"));
        // Inputs holding the same button may share (Cancel and Run on East).
        bindings.play.insert(GameAction::Jump, GameBinding::parse("Space", "North", ""));
        bindings.validate().unwrap();
        bindings.play.insert(GameAction::AttackWeak, GameBinding::default());
        assert!(bindings.validate().unwrap_err().contains("Attack context"));
        bindings.play.remove(&GameAction::AttackWeak);
        assert!(GameBinding::parse("KeyQ", "Nope", "").validate().is_err());
        assert!(GameBinding::parse("", "", "MouseBack").validate().is_err());
    }
}
