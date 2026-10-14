//! First-person inspection's controls, shared by the hosts (docs/runtime.md,
//! First-person inspection). In a field, while the player walks, Select
//! (its keys, a gamepad's Back released alone, a touch deck's SELECT)
//! toggles the view where the original's Select shows nothing (on a few
//! maps it shows the map's picture once its item is held), and the right
//! stick's click toggles it anywhere in one but a menu. While the view is
//! on, the arrow keys, the right stick, a finger dragged on the picture and
//! (under the mouse camera setting) the mouse turn it, and the game's
//! camera turns are held back, so the game's camera stays; while the player
//! walks so are walking and jumping, so the character stays where it
//! stands. A menu keeps every button, and the view does not turn under it.
use xem_core::FieldControls;
use xem_core::action::{Assignment, GameAction, buttons_with};
use xem_core::pad::button;

/// The game's buttons held back while the view is on and the player walks,
/// under the game's button assignment: the directions, jump and the field
/// camera's turns (Triangle, L1 and R1 under the default assignment); in a
/// dialogue or an event only the turns (choices take the directions).
pub fn held_back(assignment: &Assignment) -> u16 {
    button::UP | button::DOWN | button::LEFT | button::RIGHT | buttons_with([GameAction::Jump], assignment) | camera(assignment)
}

fn camera(assignment: &Assignment) -> u16 {
    buttons_with([GameAction::CameraLeft, GameAction::CameraRight], assignment)
}

/// Gamepad buttons, in `controls::PAD_NAMES` order: Back (Select) and the
/// right stick's click.
const BACK: usize = 8;
const RIGHT_STICK: usize = 11;

/// Turning: radians a second for the arrow keys (across, then up and down)
/// and the right stick at full tilt, radians a pixel of mouse motion, and
/// radians a logical pixel a finger drags the picture (the scene follows
/// the finger: about as far as the picture moves under it on a phone).
const KEY_TURN: [f32; 2] = [2.0, 1.5];
const STICK_TURN: [f32; 2] = [3.0, 2.0];
const MOUSE_TURN: f32 = 0.0025;
const TOUCH_TURN: f32 = 0.003;
/// The right stick's dead zone, a share of its travel.
const DEAD_ZONE: f32 = 0.2;

/// Whether Select toggles the view: while the player walks in a field
/// (only then the field reads Select; menus may), where it shows no map's
/// picture.
fn select_toggles(field: FieldControls) -> bool {
    field.field && field.walking && !field.picture
}

/// The toggle's inputs between frames: Select's keys (or a touch deck's
/// SELECT) and the right stick's click as last seen, and while Back is
/// held, whether alone (Back begins app chords such as Back+Start: only
/// Back released without another button toggles).
#[derive(Clone, Debug, Default)]
pub struct Toggle {
    select: bool,
    right_stick: bool,
    back: Option<bool>,
    /// Inputs held when they came back (focus regained, the panel closed)
    /// toggle nothing.
    rearm: bool,
}

impl Toggle {
    /// Whether the view toggles, `select` held (any of Select's bound keys,
    /// a touch deck's SELECT) and the gamepad's buttons `pad` as they are
    /// (before app chords take theirs).
    pub fn update(&mut self, select: bool, pad: &[u8; 16], field: FieldControls) -> bool {
        let pressed = |i: usize| pad[i] != 0;
        let others = (0..16).any(|i| i != BACK && pressed(i));
        let fresh = !std::mem::take(&mut self.rearm);
        let mut toggle = false;
        if select && !self.select && fresh && select_toggles(field) {
            toggle = true;
        }
        if pressed(RIGHT_STICK) && !self.right_stick && fresh && field.field && !field.menu {
            toggle = true;
        }
        self.select = select;
        self.right_stick = pressed(RIGHT_STICK);
        self.back = match (pressed(BACK), self.back) {
            (true, None) => Some(fresh && !others),
            (true, Some(alone)) => Some(alone && !others),
            (false, Some(true)) => {
                toggle |= select_toggles(field);
                None
            }
            (false, _) => None,
        };
        toggle
    }

    /// Forgets the inputs held (focus lost, the panel opened): those still
    /// held when they come back toggle nothing.
    pub fn clear(&mut self) {
        *self = Self { rearm: true, ..Self::default() };
    }
}

/// The game's buttons less what the view takes: Select where it toggles
/// the view, the right stick's click in a field but its menu, and while the
/// view is `on`, [`held_back`] under the game's live button `assignment`
/// (only the camera's turns in a dialogue or an event, nothing under a
/// menu).
pub fn game_buttons(buttons: u16, on: bool, field: FieldControls, assignment: &Assignment) -> u16 {
    let mut taken = 0;
    if select_toggles(field) {
        taken |= button::SELECT;
    }
    if field.field && !field.menu {
        taken |= button::R3;
    }
    if on && field.field && !field.menu {
        taken |= if field.walking { held_back(assignment) } else { camera(assignment) };
    }
    buttons & !taken
}

/// Whether the view turns now: on, but not under a menu (it has the screen).
pub fn turns(on: bool, field: FieldControls) -> bool {
    on && !field.menu
}

/// What turns the view in a host frame.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Turn {
    /// The arrow keys held: left, right, up, down.
    pub arrows: [bool; 4],
    /// The right stick, -1 to 1 (x to the right, y down).
    pub stick: [f32; 2],
    /// Mouse motion since the frame before, in pixels (y down).
    pub mouse: [f32; 2],
    /// A finger's drag on the picture since the frame before, in logical
    /// pixels (y down): the scene follows it.
    pub touch: [f32; 2],
}

impl Turn {
    /// The turn over `seconds`: radians to the right, and up.
    pub fn radians(&self, seconds: f32) -> (f32, f32) {
        let keys = |minus: bool, plus: bool| f32::from(u8::from(plus)) - f32::from(u8::from(minus));
        // Past the dead zone, finer near it.
        let stick = |v: f32| {
            let t = ((v.abs() - DEAD_ZONE) / (1.0 - DEAD_ZONE)).clamp(0.0, 1.0);
            t * t * v.signum()
        };
        let yaw = (keys(self.arrows[0], self.arrows[1]) * KEY_TURN[0] + stick(self.stick[0]) * STICK_TURN[0]) * seconds
            + self.mouse[0] * MOUSE_TURN - self.touch[0] * TOUCH_TURN;
        let pitch = (keys(self.arrows[3], self.arrows[2]) * KEY_TURN[1] - stick(self.stick[1]) * STICK_TURN[1]) * seconds
            - self.mouse[1] * MOUSE_TURN + self.touch[1] * TOUCH_TURN;
        (yaw, pitch)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const FIELD: FieldControls = FieldControls { field: true, walking: true, menu: false, picture: false };
    const PICTURE: FieldControls = FieldControls { picture: true, ..FIELD };
    /// A dialogue (its choices) or an event in a field, and its menu.
    const DIALOGUE: FieldControls = FieldControls { walking: false, ..FIELD };
    const MENU: FieldControls = FieldControls { walking: false, menu: true, ..FIELD };
    const ELSEWHERE: FieldControls = FieldControls { field: false, walking: false, menu: false, picture: false };

    fn pad(buttons: &[usize]) -> [u8; 16] {
        std::array::from_fn(|i| u8::from(buttons.contains(&i)))
    }

    /// The game's buttons under the default assignment (`pad_init`'s).
    fn game_buttons(buttons: u16, on: bool, field: FieldControls) -> u16 {
        super::game_buttons(buttons, on, field, &Assignment::default())
    }

    #[test]
    fn select_toggles_where_the_original_shows_nothing_and_the_stick_click_anywhere_in_a_field() {
        let mut toggle = Toggle::default();
        // Select's keys (Backspace) and a touch deck's SELECT alike.
        assert!(toggle.update(true, &pad(&[]), FIELD), "Select toggles on its press");
        assert!(!toggle.update(true, &pad(&[]), FIELD), "held, once");
        assert!(!toggle.update(false, &pad(&[]), FIELD));
        assert!(!toggle.update(true, &pad(&[]), PICTURE), "the map's picture keeps Select");
        assert!(!toggle.update(false, &pad(&[]), PICTURE));
        assert!(!toggle.update(true, &pad(&[]), ELSEWHERE), "only in a field");
        assert!(!toggle.update(false, &pad(&[RIGHT_STICK]), ELSEWHERE));
        assert!(!toggle.update(false, &pad(&[]), ELSEWHERE));
        assert!(!toggle.update(true, &pad(&[]), DIALOGUE), "only while the player walks");
        assert!(!toggle.update(true, &pad(&[]), MENU), "a menu may read Select");
        assert!(!toggle.update(false, &pad(&[]), MENU));
        assert!(toggle.update(false, &pad(&[RIGHT_STICK]), PICTURE), "the stick's click in any field");
        assert!(!toggle.update(false, &pad(&[]), DIALOGUE));
        assert!(toggle.update(false, &pad(&[RIGHT_STICK]), DIALOGUE), "a dialogue's too");
        assert!(!toggle.update(false, &pad(&[]), MENU));
        assert!(!toggle.update(false, &pad(&[RIGHT_STICK]), MENU), "but not a menu's");
        assert_eq!(game_buttons(button::SELECT | button::R3 | button::CROSS, false, FIELD), button::CROSS);
        assert_eq!(game_buttons(button::SELECT | button::R3, false, PICTURE), button::SELECT);
        assert_eq!(game_buttons(button::SELECT | button::R3, false, MENU), button::SELECT | button::R3);
        assert_eq!(game_buttons(button::SELECT | button::R3, false, ELSEWHERE), button::SELECT | button::R3);
    }

    #[test]
    fn back_toggles_released_alone_not_as_a_chord() {
        let mut toggle = Toggle::default();
        assert!(!toggle.update(false, &pad(&[BACK]), FIELD), "Back toggles when it is released");
        assert!(toggle.update(false, &pad(&[]), FIELD));
        // Back+Start (the settings panel) and Back+South (save state).
        for other in [9, 0] {
            assert!(!toggle.update(false, &pad(&[BACK]), FIELD));
            assert!(!toggle.update(false, &pad(&[BACK, other]), FIELD));
            assert!(!toggle.update(false, &pad(&[BACK]), FIELD));
            assert!(!toggle.update(false, &pad(&[]), FIELD), "a chord's Back toggles nothing");
        }
        assert!(!toggle.update(false, &pad(&[BACK]), PICTURE));
        assert!(!toggle.update(false, &pad(&[]), PICTURE), "the map's picture keeps Select");
    }

    #[test]
    fn inputs_held_across_a_focus_change_toggle_nothing() {
        let mut toggle = Toggle::default();
        toggle.update(false, &pad(&[BACK]), FIELD);
        toggle.clear();
        assert!(!toggle.update(true, &pad(&[BACK, RIGHT_STICK]), FIELD), "still held when focus returns");
        assert!(!toggle.update(false, &pad(&[]), FIELD), "nor Back released after it");
        assert!(toggle.update(true, &pad(&[]), FIELD), "pressed again, it toggles");
    }

    #[test]
    fn the_view_holds_back_walking_jumping_and_the_camera_while_the_player_walks() {
        let all = u16::MAX;
        let held = held_back(&Assignment::default());
        assert_eq!(held, button::UP | button::DOWN | button::LEFT | button::RIGHT | button::TRIANGLE | button::L1 | button::R1);
        let kept = game_buttons(all, true, PICTURE);
        assert_eq!(kept & held, 0);
        assert_eq!(kept & (button::CROSS | button::CIRCLE | button::SQUARE | button::START | button::SELECT), button::CROSS | button::CIRCLE | button::SQUARE | button::START | button::SELECT);
        assert_eq!(game_buttons(all, false, PICTURE) & held, held, "off, the game has them");
        assert_eq!(game_buttons(all, true, DIALOGUE) & held, held & !(button::L1 | button::R1), "a dialogue's choices keep the directions, the view the turns");
        assert_eq!(game_buttons(all, true, MENU), all, "a menu keeps every button");
        assert!(turns(true, DIALOGUE) && !turns(true, MENU) && !turns(false, FIELD), "the view turns but under a menu");
    }

    #[test]
    fn jumping_and_the_camera_are_held_back_where_the_games_assignment_puts_them() {
        // A configuration that swaps nothing: Square jumps, Triangle is
        // the menu's.
        let identity = Assignment([0, 1, 2, 3, 4, 5, 6, 7]);
        let kept = super::game_buttons(u16::MAX, true, FIELD, &identity);
        assert_eq!(kept & (button::SQUARE | button::L1 | button::R1), 0, "the assigned jump and turns are held back");
        assert_eq!(kept & (button::TRIANGLE | button::CROSS | button::CIRCLE), button::TRIANGLE | button::CROSS | button::CIRCLE);
        // The shoulders swapped: the turns are still both held back.
        let shoulders = Assignment([1, 0, 3, 2, 6, 5, 4, 7]);
        let kept = super::game_buttons(u16::MAX, true, DIALOGUE, &shoulders);
        assert_eq!(kept & (button::L1 | button::R1), 0);
        assert_eq!(kept & (button::TRIANGLE | button::UP), button::TRIANGLE | button::UP, "a dialogue keeps jump's button and the directions");
    }

    #[test]
    fn keys_sticks_the_mouse_and_a_finger_turn_the_view() {
        let turn = |turn: Turn| turn.radians(0.5);
        assert_eq!(turn(Turn { arrows: [false, true, false, false], ..Turn::default() }), (1.0, 0.0), "right turns right");
        assert_eq!(turn(Turn { arrows: [true, false, true, false], ..Turn::default() }), (-1.0, 0.75), "left and up");
        assert_eq!(turn(Turn { stick: [0.15, -0.19], ..Turn::default() }), (0.0, 0.0), "within the dead zone");
        let (yaw, pitch) = turn(Turn { stick: [1.0, -1.0], ..Turn::default() });
        assert!((yaw - 1.5).abs() < 1e-6 && (pitch - 1.0).abs() < 1e-6, "the stick at full tilt, up looks up");
        let (yaw, pitch) = Turn { mouse: [40.0, -20.0], ..Turn::default() }.radians(0.0);
        assert!((yaw - 0.1).abs() < 1e-6 && (pitch - 0.05).abs() < 1e-6, "the mouse turns by how far it moved");
        // The scene follows the finger: dragged right, the view turns left;
        // dragged down, it looks up.
        let (yaw, pitch) = Turn { touch: [100.0, 50.0], ..Turn::default() }.radians(0.0);
        assert!((yaw + 0.3).abs() < 1e-6 && (pitch - 0.15).abs() < 1e-6, "{yaw} {pitch}");
    }
}
