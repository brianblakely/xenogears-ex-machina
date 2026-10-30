//! The first gamepad on the open settings panel ([`SettingsUi::gamepad`]):
//! its buttons become the panel's keys, so it walks the panel's focus chain
//! as Tab does and works its controls as their keys do.
//!
//! - Up and down (the pad's, or the left stick's past half travel) move the
//!   focus to the previous or next control, or through an open dropdown's list.
//! - Left and right change the control that has the focus (a slider, a button
//!   group, a dropdown, a number box, the section tabs); where it takes
//!   neither, they move the focus as up and down do (along a row of buttons).
//! - South presses it (Return) for as long as it is held (Hold to rewind); on
//!   a gamepad binding's field it takes the next buttons pressed together as
//!   that binding.
//! - East closes a dropdown's list, the quit question or a binding's editor,
//!   else the panel.
//! - L1 and R1 go to the previous and next section, onto its first control.
//!
//! The first press after the panel shows only puts the focus on the
//! section's first control. Back begins the app's chords (Back+Start closes
//! the panel, Back+South saves a state), so the panel takes no press while
//! it is held. Directions repeat while held.

use crate::{PadFocus, SettingsUi};
use slint::platform::{Key, WindowEvent, WindowEventDispatchResult};
use slint::{ComponentHandle, SharedString};
use std::time::Duration;
use xem_settings::bindings::{GAMEPAD_INPUTS, Held};

/// A held direction's first repeat, and the repeats after it.
const REPEAT_DELAY: Duration = Duration::from_millis(400);
const REPEAT_EVERY: Duration = Duration::from_millis(80);

/// Bits in [`GAMEPAD_INPUTS`] order: the standard buttons, then the sticks'
/// directions.
const SOUTH: u32 = 1 << 0;
const EAST: u32 = 1 << 1;
const L1: u32 = 1 << 4;
const R1: u32 = 1 << 5;
const BACK: u32 = 1 << 8;
const UP: u32 = 1 << 12 | 1 << 16;
const DOWN: u32 = 1 << 13 | 1 << 17;
const LEFT: u32 = 1 << 14 | 1 << 18;
const RIGHT: u32 = 1 << 15 | 1 << 19;
/// The standard buttons: what an app shortcut's chord may hold.
const BUTTONS: u32 = 0xffff;

/// What the gamepad holds on the panel between polls.
#[derive(Default)]
pub(crate) struct Pad {
    /// The inputs held at the last poll; `None` until the first poll since
    /// the panel showed, as what was held then is no press.
    held: Option<u32>,
    /// The direction held and when it repeats.
    repeat: Option<(u32, Duration)>,
    /// South's Return is down, to be let go with South.
    pressing: bool,
    /// A press has put the focus on a control since the panel showed.
    engaged: bool,
    capture: Option<Capture>,
}

/// A gamepad binding's field taking the buttons pressed next.
struct Capture {
    /// `SettingsPanel::binding-field`: 1 a shortcut's chord, 2 a game action's inputs.
    field: i32,
    /// The field's text before, should it end with nothing pressed.
    before: SharedString,
    /// Everything was let go since South began it.
    armed: bool,
    /// The inputs pressed, in order.
    pressed: Vec<usize>,
}

impl SettingsUi {
    /// The first gamepad's standard buttons (1 = pressed) and axes (left X/Y,
    /// right X/Y), polled while the panel shows and the app has the input, at
    /// `now` on any steady clock. See the module's documentation.
    pub fn gamepad(&self, pressed: &[u8], axes: &[f32], now: Duration) {
        let mut pad = self.pad.take();
        self.poll(&mut pad, pressed, axes, now);
        self.pad.set(pad);
    }

    /// Whether a gamepad binding's field is taking the buttons pressed next:
    /// the host's app chords wait meanwhile, so binding Back+South does not
    /// also save a state.
    pub fn capturing_buttons(&self) -> bool {
        self.panel.get_capturing_buttons()
    }

    /// The panel hidden (or the app's input lost): the gamepad lets go of
    /// what it pressed, a capture ends as it began, and the next press after
    /// the panel shows again starts afresh.
    pub(crate) fn release_gamepad(&self) {
        let mut pad = self.pad.take();
        if pad.pressing {
            self.key_up(Key::Return);
        }
        if let Some(capture) = pad.capture.take() {
            self.end_capture(capture.field, capture.before);
        }
    }

    fn poll(&self, pad: &mut Pad, pressed: &[u8], axes: &[f32], now: Duration) {
        let mut inputs = Held::default();
        inputs.set_gamepad(pressed, axes);
        let held = inputs.gamepad;
        let Some(before) = pad.held.replace(held) else { return };
        let pressed = held & !before;
        if let Some(capture) = &mut pad.capture {
            if self.panel.get_binding_field() != capture.field {
                // The pointer took the focus elsewhere.
                let capture = pad.capture.take().unwrap();
                self.end_capture(capture.field, capture.before);
            } else if !capture.armed {
                capture.armed = held == 0;
            } else {
                for input in (0..GAMEPAD_INPUTS.len()).filter(|i| pressed & (1 << i) != 0) {
                    capture.pressed.push(input);
                }
                if held == 0 && !capture.pressed.is_empty() {
                    let capture = pad.capture.take().unwrap();
                    self.finish_capture(capture);
                }
            }
            return;
        }
        if pad.pressing && held & SOUTH == 0 {
            pad.pressing = false;
            self.key_up(Key::Return);
        }
        if held & BACK != 0 {
            pad.repeat = None;
            return;
        }
        let direction = [UP, DOWN, LEFT, RIGHT].into_iter().find(|d| pressed & d != 0);
        let step = match (direction, pad.repeat) {
            (Some(direction), _) => {
                pad.repeat = Some((direction, now + REPEAT_DELAY));
                Some(direction)
            }
            (None, Some((direction, _))) if held & direction == 0 => {
                pad.repeat = None;
                None
            }
            (None, Some((direction, at))) if now >= at => {
                pad.repeat = Some((direction, now + REPEAT_EVERY));
                Some(direction)
            }
            _ => None,
        };
        if pressed & EAST != 0 {
            self.back();
        }
        // An open list keeps the gamepad until it closes.
        let open = self.panel.global::<PadFocus>().get_list_open();
        for (button, step) in [(L1, -1), (R1, 1)] {
            if pressed & button != 0 && !open {
                let count = self.panel.get_section_count();
                self.select_section((self.panel.get_selected_section() + step).rem_euclid(count));
                self.enter(pad);
            }
        }
        if !pad.engaged && (step.is_some() || pressed & SOUTH != 0) {
            self.enter(pad);
            return;
        }
        if pressed & SOUTH != 0 {
            match self.panel.get_binding_field() {
                0 => {
                    self.key_down(Key::Return);
                    pad.pressing = true;
                }
                field => pad.capture = Some(self.begin_capture(field)),
            }
        }
        if let Some(direction) = step {
            self.step(direction);
        }
    }

    /// Another section: the controls of the last go, the focus with them if
    /// it was on one (their focus handlers never see it leave), so what it
    /// said of them goes too.
    pub(crate) fn select_section(&self, index: i32) {
        let focus = self.panel.global::<PadFocus>();
        focus.set_number_focused(false);
        focus.set_list_open(false);
        self.panel.set_selected_section(index);
    }

    /// The focus onto the section's first control.
    fn enter(&self, pad: &mut Pad) {
        self.panel.invoke_focus_section();
        self.key(Key::Tab);
        pad.engaged = true;
    }

    fn step(&self, direction: u32) {
        let focus = self.panel.global::<PadFocus>();
        let open = focus.get_list_open();
        let key = match direction {
            UP if open => Key::UpArrow,
            DOWN if open => Key::DownArrow,
            UP => Key::Backtab,
            DOWN => Key::Tab,
            // The list keeps the focus until it closes.
            _ if open => return,
            LEFT if focus.get_number_focused() => Key::DownArrow,
            RIGHT if focus.get_number_focused() => Key::UpArrow,
            LEFT => Key::LeftArrow,
            _ => Key::RightArrow,
        };
        if !self.key(key) && matches!(key, Key::LeftArrow | Key::RightArrow) {
            self.key(if key == Key::LeftArrow { Key::Backtab } else { Key::Tab });
        }
    }

    /// East: what is open closes, else the panel.
    fn back(&self) {
        let focus = self.panel.global::<PadFocus>();
        if focus.get_list_open() {
            self.key(Key::Escape);
        } else if self.cancel_dialog() {
            self.panel.invoke_focus_section();
        } else {
            focus.set_backed(false);
            self.key(Key::Escape);
            if !focus.get_backed() {
                self.panel.invoke_close_requested();
            }
        }
    }

    fn begin_capture(&self, field: i32) -> Capture {
        let before = if field == 1 { self.panel.get_gamepad_binding() } else { self.panel.get_game_buttons() };
        self.set_field(field, SharedString::new());
        self.panel.set_capturing_buttons(true);
        Capture { field, before, armed: false, pressed: Vec::new() }
    }

    /// The buttons pressed, as the field writes them: a shortcut's chord
    /// joined by `+` (its standard buttons only), a game action's inputs
    /// (any of which plays it) by spaces; four at most.
    fn finish_capture(&self, capture: Capture) {
        let names: Vec<&str> = capture.pressed.iter()
            .filter(|&&input| capture.field != 1 || BUTTONS & (1 << input) != 0)
            .map(|&input| GAMEPAD_INPUTS[input])
            .take(4)
            .collect();
        let text = names.join(if capture.field == 1 { "+" } else { " " });
        self.end_capture(capture.field, if text.is_empty() { capture.before } else { text.into() });
    }

    fn end_capture(&self, field: i32, text: SharedString) {
        self.set_field(field, text);
        self.panel.set_capturing_buttons(false);
    }

    fn set_field(&self, field: i32, text: SharedString) {
        if field == 1 { self.panel.set_gamepad_binding(text) } else { self.panel.set_game_buttons(text) }
    }

    /// A key pressed and let go; whether a control took it.
    fn key(&self, key: Key) -> bool {
        let taken = self.key_down(key);
        self.key_up(key);
        taken
    }

    fn key_down(&self, key: Key) -> bool {
        self.panel.global::<PadFocus>().set_shown(true);
        let text = SharedString::from(key);
        let taken = self.window.window.dispatch_event_with_result(WindowEvent::KeyPressed { text })
            .is_ok_and(|result| result == WindowEventDispatchResult::Accepted);
        slint::platform::update_timers_and_animations();
        taken
    }

    fn key_up(&self, key: Key) {
        self.dispatch(WindowEvent::KeyReleased { text: key.into() });
    }
}
