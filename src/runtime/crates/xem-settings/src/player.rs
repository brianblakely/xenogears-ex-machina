//! The player's input on pad 1, shared by every host: platform events
//! (keys by `KeyboardEvent.code`, the first gamepad, the mouse) come in as
//! held inputs, resolve through the game bindings ([`crate::bindings`]) to
//! typed actions, and leave as the pad's buttons. Hosts feed it; nothing
//! here knows SDL, the DOM or a headset runtime.
//!
//! Some inputs mean a short sequence of presses the game reads as it reads
//! any player's: the Menu action closes the field menu it opened (Cancel
//! until the menu has closed, from any of its screens), and the mouse wheel
//! steps a menu's or a choice's cursor. Such presses ([`Pulse`]) are held
//! and released for whole game frames (the menus act on a button's press),
//! counted as the host reports the frames run ([`PlayerInput::frames_ran`]).
use crate::bindings::{GameAction, GameBindings, Held, Mouse};
use std::collections::{BTreeSet, VecDeque};
use xem_core::context::Context as Ui;

/// What the game's state says of the player's buttons now.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Context {
    /// The battle's attack inputs take the buttons (Attack bindings first).
    pub attack: bool,
    /// The field's: first-person inspection's toggle and what it holds back.
    pub field: xem_core::FieldControls,
    /// The game's button assignment, which actions resolve through.
    pub assignment: xem_core::action::Assignment,
    /// The interface the input reaches (`xem_core::context`).
    pub ui: Ui,
}

impl Default for Context {
    fn default() -> Self {
        Self { attack: false, field: Default::default(), assignment: Default::default(), ui: Ui::None }
    }
}

/// Game frames a pulse holds its actions, then releases them.
const PULSE_HOLD: u32 = 2;
const PULSE_GAP: u32 = 2;
/// The most Cancel presses closing the field menu takes (its deepest screen
/// is a few levels down; each press backs out one).
const MENU_CLOSE_PRESSES: u32 = 8;
/// The most wheel steps queued at once.
const MAX_WHEEL_STEPS: usize = 3;

/// Actions pressed for [`PULSE_HOLD`] game frames, then released for
/// [`PULSE_GAP`].
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Pulse {
    pub actions: BTreeSet<GameAction>,
    /// Game frames run since it began.
    elapsed: u32,
}

impl Pulse {
    pub fn new(actions: impl IntoIterator<Item = GameAction>) -> Self {
        Self { actions: actions.into_iter().collect(), elapsed: 0 }
    }
    fn holding(&self) -> bool {
        self.elapsed < PULSE_HOLD
    }
    fn done(&self) -> bool {
        self.elapsed >= PULSE_HOLD + PULSE_GAP
    }
}

#[derive(Default)]
pub struct PlayerInput {
    held: Held,
    pub mouse: Mouse,
    /// Actions a touch screen's controls hold.
    pub touch: BTreeSet<GameAction>,
    pulses: VecDeque<Pulse>,
    /// Cancel presses left to close the field menu.
    closing: u32,
    /// Menu was held at the last resolution (the toggle acts on a press).
    menu_held: bool,
    /// Wheel notches not yet a whole step.
    wheel: f32,
    /// The pointer over the game's screen (320x224 pixels), whether it moved
    /// since the last steer, the entry it moved onto (steered to until
    /// selected), and a click waiting for it.
    hover: Option<(f32, f32)>,
    hover_moved: bool,
    goal: Option<usize>,
    click: u32,
    /// The interface the queued presses were meant for.
    epoch: Option<Epoch>,
}

/// What a press is meant for: a press queued in one interface never acts
/// on another (a dialogue's choice that opened, a list, a battle's next
/// step). Ring pages of one battle step and a menu's screens stay one.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Epoch {
    None,
    Walk,
    Event,
    Dialogue { choice: bool },
    Menu { screen: u8 },
    Battle { attack: bool, list: bool, targeting: bool },
    WorldMap,
    Arena { state: u32 },
}

fn epoch(ui: Ui) -> Epoch {
    match ui {
        Ui::None => Epoch::None,
        Ui::Walk => Epoch::Walk,
        Ui::Event => Epoch::Event,
        Ui::Dialogue { choice } => Epoch::Dialogue { choice: choice.is_some() },
        Ui::Menu { screen, .. } => Epoch::Menu { screen },
        Ui::Battle { attack, list, targeting, .. } => Epoch::Battle { attack, list, targeting },
        Ui::WorldMap { .. } => Epoch::WorldMap,
        Ui::Arena { state } => Epoch::Arena { state },
    }
}

/// Frames a click waits for the cursor to reach its entry.
const CLICK_FRAMES: u32 = 90;

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

    /// Release everything (focus lost, the panel took the input), queued
    /// presses too: nothing pending acts when the input comes back.
    pub fn clear(&mut self) {
        self.held = Held::default();
        self.mouse.clear();
        self.touch.clear();
        self.pulses.clear();
        self.closing = 0;
        self.menu_held = false;
        self.wheel = 0.0;
        self.goal = None;
        self.click = 0;
    }

    /// The pointer moved over the game's screen (its 320x224 pixels), or
    /// off it.
    pub fn hover(&mut self, point: Option<(f32, f32)>) {
        if point != self.hover {
            self.hover = point;
            self.hover_moved = true;
        }
    }

    /// A click at the pointer: true when it lands on an entry of `targets`
    /// (it selects that entry, then confirms it); otherwise the click is
    /// the bindings' (the mouse's Confirm).
    pub fn click(&mut self, targets: Option<&xem_core::menu::Targets>) -> bool {
        let Some(entry) = targets.zip(self.hover).and_then(|(t, p)| t.at(p)) else { return false };
        self.goal = Some(entry);
        self.click = CLICK_FRAMES;
        true
    }

    /// Selects entry `index` of the interface's targets with the D-pad, then
    /// (`confirm`) presses Confirm once it is selected (gaze and a tap).
    pub fn select(&mut self, index: usize, confirm: bool) {
        self.goal = Some(index);
        self.click = if confirm { CLICK_FRAMES } else { 0 };
    }

    /// Steers the game's cursor to the entry the pointer moved onto, and
    /// confirms a clicked one once selected. Call once a host frame with
    /// the interface's entries.
    pub fn browse(&mut self, targets: Option<&xem_core::menu::Targets>) {
        let Some(targets) = targets else {
            self.goal = None;
            self.click = 0;
            return;
        };
        if std::mem::take(&mut self.hover_moved)
            && let Some(entry) = self.hover.and_then(|p| targets.at(p))
        {
            self.goal = Some(entry);
        }
        if self.pulsing() {
            return;
        }
        let Some(goal) = self.goal else { return };
        match targets.step_toward(goal) {
            Some(action) => self.pulse(Pulse::new([action])),
            None if targets.selected == Some(goal) => {
                self.goal = None;
                if self.click > 0 {
                    self.click = 0;
                    self.pulse(Pulse::new([GameAction::Confirm]));
                }
            }
            None => self.goal = None,
        }
        self.click = self.click.saturating_sub(1);
    }

    /// Queue a pulse (presses an agent or a gesture resolved to).
    pub fn pulse(&mut self, pulse: Pulse) {
        self.pulses.push_back(pulse);
    }

    /// Whether presses are queued or under way.
    pub fn pulsing(&self) -> bool {
        !self.pulses.is_empty()
    }

    /// The host ran `frames` game frames with the last buttons.
    pub fn frames_ran(&mut self, frames: u32) {
        if let Some(pulse) = self.pulses.front_mut() {
            pulse.elapsed += frames;
            if pulse.done() {
                self.pulses.pop_front();
            }
        }
    }

    /// Mouse wheel notches (positive toward the user, down a list) over the
    /// game: in a menu or a choice each whole notch steps the cursor (Down
    /// or Up). False when the interface has no cursor (the host zooms).
    pub fn wheel(&mut self, notches: f32, ui: Ui) -> bool {
        if !ui.navigates() || matches!(ui, Ui::Battle { .. }) {
            self.wheel = 0.0;
            return false;
        }
        self.wheel += notches;
        while self.wheel.abs() >= 1.0 {
            let step = self.wheel.signum();
            self.wheel -= step;
            if self.pulses.len() < MAX_WHEEL_STEPS {
                self.pulse(Pulse::new([if step > 0.0 { GameAction::Down } else { GameAction::Up }]));
            }
        }
        true
    }

    /// The actions held at `now` (milliseconds).
    pub fn actions(&mut self, bindings: &GameBindings, now: f64, context: Context) -> BTreeSet<GameAction> {
        let epoch = epoch(context.ui);
        if self.epoch.is_some_and(|e| e != epoch) {
            // A press under way finishes its release; nothing queued after
            // it reaches the new interface.
            let current = self.pulses.pop_front().filter(|p| !p.holding());
            self.pulses.clear();
            self.pulses.extend(current);
            self.goal = None;
            self.click = 0;
        }
        self.epoch = Some(epoch);
        self.held.mouse = self.mouse.buttons();
        let mut actions = bindings.actions(&self.held, context.attack);
        actions.extend(self.mouse.turn(now));
        actions.extend(self.touch.iter().copied());
        // Menu toggles the field menu: pressed while it runs, it backs out
        // with Cancel until the menu has closed.
        let field_menu = matches!(context.ui, Ui::Menu { screen: 0, .. }) && context.field.field;
        let menu = actions.contains(&GameAction::Menu);
        if field_menu && menu && !self.menu_held {
            self.closing = MENU_CLOSE_PRESSES;
        }
        self.menu_held = menu;
        if !field_menu {
            self.closing = 0;
        } else {
            actions.remove(&GameAction::Menu);
            if self.closing > 0 && self.pulses.is_empty() {
                self.closing -= 1;
                self.pulse(Pulse::new([GameAction::Cancel]));
            }
        }
        if let Some(pulse) = self.pulses.front() {
            if pulse.holding() {
                actions.extend(pulse.actions.iter().copied());
            } else {
                // A clean release: the menus act on presses.
                let buttons = xem_core::action::buttons_with(pulse.actions.iter().copied(), &context.assignment);
                actions.retain(|a| a.button_with(&context.assignment) & buttons == 0);
            }
        }
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

    #[test]
    fn presses_queued_for_one_interface_never_reach_the_next() {
        let bindings = GameBindings::default();
        let mut input = PlayerInput::default();
        let dialogue = field(Ui::Dialogue { choice: None });
        input.pulse(Pulse::new([GameAction::Confirm]));
        input.pulse(Pulse::new([GameAction::Confirm]));
        assert_eq!(run(&mut input, &bindings, dialogue, 2), [button::CROSS, button::CROSS]);
        // The first press revealed a choice: the second is dropped.
        let choice = field(Ui::Dialogue { choice: Some(xem_core::context::Choice { count: 2, index: 0 }) });
        assert_eq!(run(&mut input, &bindings, choice, 4), [0, 0, 0, 0]);
        assert!(!input.pulsing());
    }

    #[test]
    fn pointing_selects_a_menu_entry_and_clicking_confirms_it() {
        use xem_core::menu::{Entry, Rect, Steps, Targets};
        let bindings = GameBindings::default();
        let mut input = PlayerInput::default();
        let rect = |y: f32| Rect { x: 0.0, y, width: 50.0, height: 10.0 };
        let mut targets = Targets {
            what: "commands",
            entries: (0..7).map(|i| Entry { rect: rect(100.0 - 10.0 * i as f32), select: None }).collect(),
            selected: Some(0),
            steps: Steps::Ring { count: 7, next: GameAction::Up },
        };
        let menu = field(Ui::Menu { screen: 0, cursor: 0, choice: 0, choices: 0, switch: false, quantities: false });
        input.hover(Some((10.0, 85.0)));
        assert!(input.click(Some(&targets)), "on entry 2");
        let mut pressed = Vec::new();
        for _ in 0..16 {
            input.browse(Some(&targets));
            let buttons = input.buttons(&bindings, 0.0, menu);
            // The game steps its cursor on each Up press.
            if buttons & button::UP != 0 && !pressed.last().is_some_and(|b: &u16| b & button::UP != 0) {
                targets.selected = targets.selected.map(|s| (s + 1) % 7);
            }
            pressed.push(buttons);
            input.frames_ran(1);
        }
        assert_eq!(targets.selected, Some(2));
        assert_eq!(pressed.iter().filter(|b| **b & button::CROSS != 0).count(), 2, "one Confirm press, held two frames: {pressed:?}");
        // Off the entries a click is the mouse's own.
        input.hover(Some((200.0, 5.0)));
        assert!(!input.click(Some(&targets)));
    }

    fn field(ui: Ui) -> Context {
        let field = xem_core::FieldControls { field: true, ..Default::default() };
        Context { field, ui, ..Context::default() }
    }

    /// Runs `frames` game frames of `context` with the player's keys,
    /// returning each frame's buttons.
    fn run(input: &mut PlayerInput, bindings: &GameBindings, context: Context, frames: usize) -> Vec<u16> {
        (0..frames).map(|_| {
            let buttons = input.buttons(bindings, 0.0, context);
            input.frames_ran(1);
            buttons
        }).collect()
    }

    #[test]
    fn menu_opens_the_field_menu_and_closes_it_with_cancel_presses() {
        let bindings = GameBindings::default();
        let mut input = PlayerInput::default();
        let walk = field(Ui::Walk);
        let menu = field(Ui::Menu { screen: 0, cursor: 0, choice: 0, choices: 0, switch: false, quantities: false });
        input.key(&bindings, "Tab", true);
        assert_eq!(run(&mut input, &bindings, walk, 1), [button::SQUARE], "Tab opens the menu");
        // Held into the menu, it does nothing there.
        assert_eq!(run(&mut input, &bindings, menu, 3), [0, 0, 0]);
        input.key(&bindings, "Tab", false);
        run(&mut input, &bindings, menu, 1);
        input.key(&bindings, "Tab", true);
        // Pressed again: Cancel for two frames, released for two, again
        // while the menu runs.
        assert_eq!(run(&mut input, &bindings, menu, 8),
            [button::CIRCLE, button::CIRCLE, 0, 0, button::CIRCLE, button::CIRCLE, 0, 0]);
        // Closed: the presses stop.
        input.key(&bindings, "Tab", false);
        assert_eq!(run(&mut input, &bindings, walk, 4), [0, 0, 0, 0]);
        // A shop (another screen) keeps the Menu button.
        input.key(&bindings, "Tab", true);
        let shop = field(Ui::Menu { screen: 4, cursor: 0, choice: 0, choices: 0, switch: false, quantities: false });
        assert_eq!(run(&mut input, &bindings, shop, 1), [button::SQUARE]);
    }

    #[test]
    fn the_wheel_steps_a_menus_cursor_and_is_the_hosts_elsewhere() {
        let bindings = GameBindings::default();
        let mut input = PlayerInput::default();
        assert!(!input.wheel(1.0, Ui::Walk));
        let menu = Ui::Menu { screen: 0, cursor: 0, choice: 0, choices: 0, switch: false, quantities: false };
        assert!(input.wheel(0.5, menu));
        assert!(!input.pulsing(), "half a notch");
        assert!(input.wheel(1.5, menu));
        assert!(input.wheel(-1.0, menu));
        assert_eq!(run(&mut input, &bindings, field(menu), 12), [
            button::DOWN, button::DOWN, 0, 0, button::DOWN, button::DOWN, 0, 0, button::UP, button::UP, 0, 0]);
        // A held key of the same button is released between pulses.
        input.key(&bindings, "KeyS", true);
        input.wheel(1.0, menu);
        assert_eq!(run(&mut input, &bindings, field(menu), 5), [button::DOWN, button::DOWN, 0, 0, button::DOWN]);
        // Focus loss drops what is queued.
        input.wheel(3.0, menu);
        input.clear();
        assert!(!input.pulsing());
    }
}
