//! Pad 1 from the keyboard and the first gamepad, with xem-host's default
//! mapping (`View::pad_buttons`): keys by `KeyboardEvent.code`, gamepad
//! buttons by the Gamepad API's standard mapping. The bits are the pad's
//! data halfword as the BIOS driver reports it (bit 0 Select .. bit 15 Square).

use xem_core::pad::button;

const KEYS: [(&str, u16); 16] = [
    ("ShiftRight", button::SELECT),
    ("Enter", button::START),
    ("ArrowUp", button::UP),
    ("ArrowRight", button::RIGHT),
    ("ArrowDown", button::DOWN),
    ("ArrowLeft", button::LEFT),
    ("Digit1", button::L2),
    ("Digit2", button::R2),
    ("KeyQ", button::L1),
    ("KeyW", button::R1),
    ("KeyS", button::TRIANGLE),
    ("KeyX", button::CIRCLE),
    ("KeyZ", button::CROSS),
    ("KeyA", button::SQUARE),
    ("Backspace", button::SELECT),
    ("Space", button::CROSS),
];

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

/// The pad bit a key (`KeyboardEvent.code`) holds, if it is a game key.
pub fn key_bit(code: &str) -> Option<u16> {
    KEYS.iter().find(|(name, _)| *name == code).map(|(_, bit)| *bit)
}

#[derive(Default)]
pub struct HostInput {
    /// Held game keys, by their bits (two keys may share one).
    keys: Vec<(String, u16)>,
    gamepad: u16,
}

impl HostInput {
    /// A key went down or up; false when it is not a game key.
    pub fn key(&mut self, code: &str, down: bool) -> bool {
        let Some(bit) = key_bit(code) else { return false };
        self.keys.retain(|(held, _)| held != code);
        if down {
            self.keys.push((code.to_string(), bit));
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
        self.gamepad = 0;
    }

    pub fn buttons(&self) -> u16 {
        self.keys.iter().fold(self.gamepad, |bits, (_, bit)| bits | bit)
    }
}
