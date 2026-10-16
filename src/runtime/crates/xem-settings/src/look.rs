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
//! In a headset the head looks around, and the right stick turns the view
//! in steps ([`SnapTurn`]). The app's Toggle first-person view action and
//! agents' `first_person` command ([`Command`]) toggle it anywhere in a
//! field but its menu, as a headset's underhand grab does.
use xem_core::FieldControls;
use xem_core::action::{Assignment, GameAction, INSPECTION_HELD, buttons_with};
use xem_core::pad::button;

/// The game's buttons held back while the view is on and the player walks,
/// under the game's button assignment: the directions, jump and the field
/// camera's turns (Triangle, L1 and R1 under the default assignment:
/// [`INSPECTION_HELD`]); in a dialogue or an event only the turns (choices
/// take the directions).
pub fn held_back(assignment: &Assignment) -> u16 {
    buttons_with(INSPECTION_HELD, assignment)
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
/// A headset's turn, radians (30 degrees), when its right stick is pushed
/// past [`SNAP_ON`] of its travel to a side; the next waits until the stick
/// comes back within [`SNAP_OFF`].
pub const SNAP_TURN: f32 = std::f32::consts::PI / 6.0;
const SNAP_ON: f32 = 0.7;
const SNAP_OFF: f32 = 0.3;

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

/// Whether a headset's underhand grab toggles the view: in a field but its
/// menu, as the right stick's click does, where Ex Machina draws it (the
/// Original renderer has nothing to look around in, and a toggle there
/// would still stop a route and hold back the walking buttons). The app's
/// Toggle first-person view action and agents' `first_person` command
/// toggle it so too.
pub fn grab_toggles(field: FieldControls, renderer: crate::Renderer) -> bool {
    refusal(field, renderer).is_none()
}

/// Why the view cannot be turned on or off now ([`grab_toggles`]).
fn refusal(field: FieldControls, renderer: crate::Renderer) -> Option<&'static str> {
    if renderer != crate::Renderer::ExMachina {
        Some("the Original renderer shows the console's picture: there is no first-person view to look around in")
    } else if !field.field {
        Some("first-person inspection looks around a field: there is none now")
    } else if field.menu {
        Some("the field's menu has the screen: close it first")
    } else {
        None
    }
}

/// An agent's `first_person` command (`{"cmd": "first_person", "on": bool,
/// "yaw": degrees, "pitch": degrees}`, each optional), which the app's
/// Toggle first-person view action is with none: the view on or off (`on`
/// absent: toggled, or on where a look is given) where [`grab_toggles`]
/// allows a change, then turned to look `yaw` degrees right of the heading
/// it began with and `pitch` up (held within 80 degrees of level) where it
/// turns ([`turns`]). The hosts give the session the view's state before
/// their next frames (`Session::set_inspecting`: a route under way stops).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Command {
    pub on: Option<bool>,
    pub yaw: Option<f32>,
    pub pitch: Option<f32>,
}

/// What a [`Command`] does to the view: whether it shows after it, then the
/// turn it takes (radians right and up) from where it looks then.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Change {
    pub on: bool,
    pub turn: (f32, f32),
}

impl Command {
    /// The app's Toggle first-person view.
    pub const TOGGLE: Command = Command { on: None, yaw: None, pitch: None };

    pub fn parse(command: &serde_json::Value) -> Result<Command, String> {
        let fields = command.as_object().ok_or("first_person is a JSON object")?;
        if let Some(other) = fields.keys().find(|key| !["cmd", "on", "yaw", "pitch"].contains(&key.as_str())) {
            return Err(format!("first_person takes on, yaw and pitch, not {other}"));
        }
        let on = fields.get("on").map(|on| on.as_bool().ok_or("on is true or false")).transpose()?;
        let degrees = |name: &str| {
            fields.get(name).map(|value| value.as_f64().filter(|v| v.is_finite()).map(|v| v as f32).ok_or_else(|| format!("{name} is a number of degrees"))).transpose()
        };
        Ok(Command { on, yaw: degrees("yaw")?, pitch: degrees("pitch")? })
    }

    /// Its change of a view that shows (`on`) looking `look` (radians from
    /// the heading it began with, while it shows) in `field` under
    /// `renderer`; refused (nothing changes) where the view cannot change,
    /// or turn: off, or under the field's menu.
    pub fn change(&self, on: bool, look: Option<(f32, f32)>, field: FieldControls, renderer: crate::Renderer) -> Result<Change, String> {
        let looks = self.yaw.is_some() || self.pitch.is_some();
        let shows = self.on.unwrap_or(looks || !on);
        if shows != on && let Some(why) = refusal(field, renderer) {
            return Err(why.into());
        }
        if !looks {
            return Ok(Change { on: shows, turn: (0.0, 0.0) });
        }
        if !shows {
            return Err("the first-person view is off: turn it on to look around".into());
        }
        if !turns(shows, field) {
            return Err("the field's menu has the screen: the view turns once it closes".into());
        }
        // A view turned on begins along the heading, level.
        let (yaw, pitch) = if on { look.unwrap_or_default() } else { (0.0, 0.0) };
        let to = |target: Option<f32>, now: f32| target.map_or(0.0, |degrees| degrees.to_radians() - now);
        Ok(Change { on: shows, turn: (to(self.yaw, yaw), to(self.pitch, pitch)) })
    }
}

/// A `first_person` command's answer: whether the view shows, and where it
/// looks (degrees right of the heading it began with and up, to a
/// thousandth; null while it is off).
pub fn reply(on: bool, look: Option<(f32, f32)>) -> serde_json::Value {
    let degrees = |radians: f32| (f64::from(radians.to_degrees()) * 1000.0).round() / 1000.0;
    serde_json::json!({
        "first_person": on,
        "look": look.filter(|_| on).map(|(yaw, pitch)| serde_json::json!({ "yaw": degrees(yaw), "pitch": degrees(pitch) })),
    })
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

/// A headset's turning of the view: the head looks around, and the right
/// stick turns the view in [`SNAP_TURN`] steps, never smoothly (a smooth
/// turn the body does not feel is the commonest cause of discomfort);
/// pitching is the head's alone.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SnapTurn {
    /// A step was taken and the stick has not come back yet.
    held: bool,
}

impl SnapTurn {
    /// The step the right stick's `x` (-1 to 1, to the right) takes now:
    /// radians to the right, or 0.
    pub fn update(&mut self, x: f32) -> f32 {
        if self.held {
            self.held = x.abs() >= SNAP_OFF;
            return 0.0;
        }
        if x.abs() < SNAP_ON {
            return 0.0;
        }
        self.held = true;
        SNAP_TURN * x.signum()
    }

    /// A stick still pushed when input comes back (focus regained, the
    /// panel closed) steps nothing until it has come back.
    pub fn clear(&mut self) {
        self.held = true;
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
    fn the_underhand_grab_toggles_in_a_field_but_its_menu_where_ex_machina_draws_it() {
        use crate::Renderer::{ExMachina, Original};
        for field in [FIELD, PICTURE, DIALOGUE] {
            assert!(grab_toggles(field, ExMachina), "{field:?}");
            assert!(!grab_toggles(field, Original), "the Original renderer has no view to show: {field:?}");
        }
        assert!(!grab_toggles(MENU, ExMachina), "a menu has the screen");
        assert!(!grab_toggles(ELSEWHERE, ExMachina), "only in a field");
    }

    #[test]
    fn the_command_and_the_action_toggle_where_the_grab_does_and_turn_the_view_where_it_turns() {
        use crate::Renderer::{ExMachina, Original};
        use serde_json::json;
        let parse = |value: serde_json::Value| Command::parse(&value);
        assert_eq!(parse(json!({"cmd": "first_person"})), Ok(Command::TOGGLE), "the action is the bare command");
        let look = parse(json!({"cmd": "first_person", "on": true, "yaw": 30, "pitch": -10.5})).unwrap();
        assert_eq!(look, Command { on: Some(true), yaw: Some(30.0), pitch: Some(-10.5) });
        for bad in [json!({"cmd": "first_person", "on": 1}), json!({"cmd": "first_person", "yaw": "right"}), json!({"cmd": "first_person", "enabled": true})] {
            assert!(parse(bad.clone()).is_err(), "{bad}");
        }
        // Toggled in any field but its menu (where the player walks, a
        // dialogue's, a map whose Select shows its picture), under Ex Machina.
        for field in [FIELD, DIALOGUE, PICTURE] {
            assert_eq!(Command::TOGGLE.change(false, None, field, ExMachina), Ok(Change { on: true, turn: (0.0, 0.0) }), "{field:?}");
            assert_eq!(Command::TOGGLE.change(true, Some((0.5, 0.1)), field, ExMachina), Ok(Change { on: false, turn: (0.0, 0.0) }), "{field:?}");
        }
        for (field, renderer, why) in [(MENU, ExMachina, "menu"), (ELSEWHERE, ExMachina, "field"), (FIELD, Original, "Original")] {
            let refused = Command::TOGGLE.change(false, None, field, renderer).unwrap_err();
            assert!(refused.contains(why), "{field:?} {renderer:?}: {refused}");
            assert!(!grab_toggles(field, renderer));
        }
        let off = Command { on: Some(false), ..Command::TOGGLE };
        assert_eq!(off.change(false, None, ELSEWHERE, Original), Ok(Change { on: false, turn: (0.0, 0.0) }), "off where it is off anywhere");
        assert!(off.change(true, Some((0.0, 0.0)), MENU, ExMachina).is_err(), "no change under the menu");
        // A look: from the heading when it comes on, else from where it looks.
        let (yaw, pitch) = (30f32.to_radians(), (-10.5f32).to_radians());
        let near = |change: Change, on: bool, turn: (f32, f32)| change.on == on && (change.turn.0 - turn.0).abs() < 1e-6 && (change.turn.1 - turn.1).abs() < 1e-6;
        assert!(near(look.change(false, None, FIELD, ExMachina).unwrap(), true, (yaw, pitch)));
        assert!(near(look.change(true, Some((0.5, 0.1)), DIALOGUE, ExMachina).unwrap(), true, (yaw - 0.5, pitch - 0.1)));
        let across = Command { yaw: Some(-90.0), ..Command::TOGGLE };
        assert!(near(across.change(true, Some((0.5, 0.1)), FIELD, ExMachina).unwrap(), true, ((-90f32).to_radians() - 0.5, 0.0)), "a look alone keeps it on");
        assert!(near(across.change(false, None, FIELD, ExMachina).unwrap(), true, ((-90f32).to_radians(), 0.0)), "and turns it on");
        assert!(Command { on: Some(false), ..across }.change(true, Some((0.0, 0.0)), FIELD, ExMachina).unwrap_err().contains("off"));
        assert!(across.change(true, Some((0.0, 0.0)), MENU, ExMachina).unwrap_err().contains("menu"), "the view turns but under a menu");
        // The answer: degrees to a thousandth, no look while off.
        assert_eq!(reply(true, Some((yaw, pitch))), json!({"first_person": true, "look": {"yaw": 30.0, "pitch": -10.5}}));
        assert_eq!(reply(false, None), json!({"first_person": false, "look": null}));
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

    #[test]
    fn a_headsets_right_stick_turns_the_view_thirty_degrees_a_push() {
        let mut snap = SnapTurn::default();
        assert_eq!(snap.update(0.5), 0.0, "short of the push");
        assert_eq!(snap.update(0.8), SNAP_TURN, "pushed right, a step right");
        assert!((SNAP_TURN - 30f32.to_radians()).abs() < 1e-6);
        assert_eq!(snap.update(1.0), 0.0, "held, once");
        assert_eq!(snap.update(0.4), 0.0, "not yet back");
        assert_eq!(snap.update(0.1), 0.0, "back");
        assert_eq!(snap.update(-0.9), -SNAP_TURN, "pushed left, a step left");
        assert_eq!(snap.update(0.0), 0.0);
        // Input coming back with the stick still pushed steps nothing.
        snap.clear();
        assert_eq!(snap.update(0.9), 0.0, "still pushed when input came back");
        assert_eq!(snap.update(0.0), 0.0);
        assert_eq!(snap.update(0.9), SNAP_TURN, "pushed again");
    }
}
