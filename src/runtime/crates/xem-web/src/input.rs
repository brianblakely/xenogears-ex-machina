//! Pad 1 from the keyboard, the mouse and the first gamepad: keys and mouse
//! by the hosts' shared layout (`xem_settings::game_keys`), gamepad buttons
//! by the Gamepad API's standard mapping. The bits are the pad's data
//! halfword as the BIOS driver reports it (bit 0 Select .. bit 15 Square).

use xem_core::pad::button;
use xem_settings::game_keys::{self, Mouse};

/// The standard mapping's buttons 0-15 (South, East, West, North, LB, RB,
/// LT, RT, Back, Start, LS, RS, D-pad up, down, left, right).
const GAMEPAD: [u16; 16] = [
    button::CROSS,
    button::CIRCLE,
    button::SQUARE,
    button::TRIANGLE,
    button::L1,
    button::R1,
    button::L2,
    button::R2,
    button::SELECT,
    button::START,
    button::L3,
    button::R3,
    button::UP,
    button::DOWN,
    button::LEFT,
    button::RIGHT,
];

#[derive(Default)]
pub struct HostInput {
    /// Held game keys (`KeyboardEvent.code`).
    keys: Vec<String>,
    pub mouse: Mouse,
    gamepad: u16,
}

impl HostInput {
    /// A key went down or up; false when it is not a game key.
    pub fn key(&mut self, code: &str, down: bool) -> bool {
        if !game_keys::game_keys().any(|key| key == code) { return false; }
        self.keys.retain(|held| held != code);
        if down {
            self.keys.push(code.to_string());
        }
        true
    }

    /// The first gamepad's pressed buttons (standard mapping, 1 = pressed).
    pub fn gamepad(&mut self, pressed: &[u8]) {
        self.gamepad = GAMEPAD.iter().zip(pressed).filter(|(_, p)| **p != 0).fold(0, |bits, (bit, _)| bits | bit);
    }

    /// Release everything (the page lost focus).
    pub fn clear(&mut self) {
        self.keys.clear();
        self.mouse.clear();
        self.gamepad = 0;
    }

    /// The held buttons at `now` (milliseconds; `combo`: the battle's attack
    /// inputs have the buttons).
    pub fn buttons(&mut self, now: f64, combo: bool) -> u16 {
        let held = self.mouse.buttons(now) | self.gamepad;
        self.keys.iter().filter_map(|code| game_keys::key_button(code, combo)).fold(held, |bits, bit| bits | bit)
    }
}
