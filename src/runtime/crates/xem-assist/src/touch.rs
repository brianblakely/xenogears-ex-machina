//! Touch screens (Android, phone and tablet browsers): the game's controls
//! as a Seraph Glass deck beside the picture, and taps on the picture
//! resolved as the headset's pointer gestures are.
//!
//! - The deck: a D-pad on the left with Select above it, the four face
//!   buttons on the right (Jump, Cancel and Run, Confirm, Menu), the
//!   camera's turns at the top corners, Start and Settings at the top. A
//!   finger holds the control it is on, sliding from one to another (the
//!   D-pad's directions), and lets go when lifted or cancelled: explicit
//!   press and release. In Battling the face buttons show the bout's own
//!   names (A, B, Jump, Dash) and the turns its Charge and Special.
//!   Controls stand in the letterbox beside the picture when it leaves
//!   room, else over its corners.
//! - A tap on the picture (a short touch that does not travel) is the
//!   pointer's tap: walking, on an actor it approaches and talks, on the
//!   floor it walks there; in a menu or a choice it selects the entry and
//!   confirms it; in a dialogue it advances. A finger dragged on the
//!   picture turns first-person inspection's view ([`Deck::take_drag`]).
//! - A shop's quantity list adds the +, -, ALL and NONE keys; the world
//!   map adds Board (L2 and R2 together: a Gear boarded or left); a field
//!   (not its menu) adds LOOK (first-person inspection on or off, the app's
//!   Toggle first-person view, lit while on), and its scenes and dialogue
//!   AUTO beside it (auto-advance dialogue on or off, lit while on), apart
//!   from the face buttons that advance one page.
use crate::overlay::QuantityKey;
use std::collections::{BTreeMap, BTreeSet};
use xem_core::action::GameAction;
use xem_core::context::Context;

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum Control {
    Up,
    Down,
    Left,
    Right,
    Jump,
    Cancel,
    Confirm,
    Menu,
    CameraLeft,
    CameraRight,
    Start,
    /// The game's Select (a map's picture), and in a field first-person
    /// inspection's toggle where the game's Select shows nothing.
    Select,
    Settings,
    /// The world map's Board: L2 and R2 together (a Gear boarded or left).
    Board,
    /// Auto-advance dialogue on or off (the app's toggle, not the game's).
    Auto,
    /// First-person inspection on or off anywhere in a field but its menu
    /// (the app's Toggle first-person view, not the game's Select).
    Look,
    Quantity(u8),
}

impl Control {
    /// The game actions the control holds.
    pub fn actions(self) -> Vec<GameAction> {
        match self {
            Control::Up => vec![GameAction::Up],
            Control::Down => vec![GameAction::Down],
            Control::Left => vec![GameAction::Left],
            Control::Right => vec![GameAction::Right],
            Control::Jump => vec![GameAction::Jump],
            Control::Cancel => vec![GameAction::Cancel, GameAction::Run],
            Control::Confirm => vec![GameAction::Confirm],
            Control::Menu => vec![GameAction::Menu],
            Control::CameraLeft => vec![GameAction::CameraLeft],
            Control::CameraRight => vec![GameAction::CameraRight],
            Control::Start => vec![GameAction::Start],
            Control::Select => vec![GameAction::Select],
            Control::Board => vec![GameAction::L2, GameAction::R2],
            Control::Settings | Control::Auto | Control::Look | Control::Quantity(_) => vec![],
        }
    }

    /// Its label: a symbol (the arrows, the pad's face shapes) or a word.
    pub fn label(self, ui: Context) -> &'static str {
        let arena = matches!(ui, Context::Arena { state: 1 });
        match self {
            Control::Up => "^",
            Control::Down => "v",
            Control::Left => "<",
            Control::Right => ">",
            Control::Jump => if arena { "JUMP" } else { "triangle" },
            Control::Cancel => if arena { "DASH" } else { "circle" },
            Control::Confirm => if arena { "A" } else { "cross" },
            Control::Menu => if arena { "B" } else { "square" },
            Control::CameraLeft => if arena { "CHARGE" } else { "L1" },
            Control::CameraRight => if arena { "SPECIAL" } else { "R1" },
            Control::Start => "START",
            Control::Select => "SELECT",
            Control::Settings => "SETTINGS",
            Control::Board => "BOARD",
            Control::Auto => "AUTO",
            Control::Look => "LOOK",
            Control::Quantity(k) => QuantityKey::ALL[k as usize].label(),
        }
    }
}

/// A rectangle in target pixels.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Rect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

impl Rect {
    pub fn contains(&self, (x, y): (f32, f32)) -> bool {
        x >= self.x && x < self.x + self.width && y >= self.y && y < self.y + self.height
    }
    pub fn centre(&self) -> (f32, f32) {
        (self.x + self.width / 2.0, self.y + self.height / 2.0)
    }
}

/// The deck's controls and where they stand.
pub fn layout(target: (f32, f32), picture: Rect, density: f32, ui: Context) -> Vec<(Control, Rect)> {
    let (w, h) = target;
    // Generous touch targets: about 12% of the short side, at least 52 dp.
    let mut size = (w.min(h) * 0.13).max(52.0 * density).min(w.min(h) * 0.2);
    // Beside the picture when the letterbox has room for a cluster (three
    // controls and their gaps), shrinking them no smaller than 40 dp.
    let side = (picture.x).min(w - picture.x - picture.width);
    let fits = side / 3.6;
    let beside = fits >= 40.0 * density;
    if beside {
        size = size.min(fits);
    }
    let gap = size * 0.12;
    let margin = size * 0.35;
    let cluster = |cx: f32, cy: f32| move |dx: f32, dy: f32| Rect { x: cx + dx * (size + gap) - size / 2.0, y: cy + dy * (size + gap) - size / 2.0, width: size, height: size };
    let low = h - margin - size * 1.6;
    let (left_x, right_x) = if beside {
        (picture.x / 2.0, picture.x + picture.width + (w - picture.x - picture.width) / 2.0)
    } else {
        (margin + size * 1.6, w - margin - size * 1.6)
    };
    let pad = cluster(left_x, low);
    let face = cluster(right_x, low);
    let mut controls = vec![
        (Control::Up, pad(0.0, -1.0)),
        (Control::Down, pad(0.0, 1.0)),
        (Control::Left, pad(-1.0, 0.0)),
        (Control::Right, pad(1.0, 0.0)),
        (Control::Jump, face(0.0, -1.0)),
        (Control::Cancel, face(1.0, 0.0)),
        (Control::Confirm, face(0.0, 1.0)),
        (Control::Menu, face(-1.0, 0.0)),
    ];
    let top = margin + size / 2.0;
    let corner = |x: f32| Rect { x: x - size * 0.8, y: top - size * 0.4, width: size * 1.6, height: size * 0.8 };
    controls.push((Control::CameraLeft, corner(left_x)));
    controls.push((Control::CameraRight, corner(right_x)));
    let small = |x: f32| Rect { x: x - size * 0.7, y: top - size * 0.35, width: size * 1.4, height: size * 0.7 };
    controls.push((Control::Settings, small(w / 2.0 - size * 0.85)));
    controls.push((Control::Start, small(w / 2.0 + size * 0.85)));
    // A wide key above a cluster: Select over the D-pad, the world map's
    // Board over the face buttons; in a field LOOK there, its scenes' AUTO
    // beside it, each of the two a little narrower.
    let above = |cx: f32| Rect { x: cx - size * 0.8, y: low - 2.3 * (size + gap) - size * 0.35, width: size * 1.6, height: size * 0.7 };
    let half = |cx: f32, right: bool| Rect { x: if right { cx + gap / 2.0 } else { cx - gap / 2.0 - size * 1.5 }, width: size * 1.5, ..above(cx) };
    controls.push((Control::Select, above(left_x)));
    if matches!(ui, Context::WorldMap { .. }) {
        controls.push((Control::Board, above(right_x)));
    }
    if crate::overlay::look_key(ui) {
        controls.push((Control::Look, half(right_x, true)));
    }
    if crate::overlay::auto_key(ui) {
        controls.push((Control::Auto, half(right_x, false)));
    }
    if matches!(ui, Context::Menu { quantities: true, .. }) {
        let x = right_x;
        for (i, _) in QuantityKey::ALL.iter().enumerate() {
            let y = top + size * 0.9 + i as f32 * size * 0.8;
            controls.push((Control::Quantity(i as u8), Rect { x: x - size * 0.8, y, width: size * 1.6, height: size * 0.7 }));
        }
    }
    controls
}

/// A finger's first touch: where and when, whether it travelled, and where
/// it is now.
#[derive(Clone, Copy, Debug)]
struct Finger {
    control: Option<Control>,
    start: (f32, f32),
    began: f64,
    travelled: bool,
    on_picture: bool,
    at: (f32, f32),
}

/// What a touch did.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Touched {
    Nothing,
    /// A tap on the picture, at target pixels.
    Tap((f32, f32)),
    /// Settings was pressed.
    Settings,
    /// AUTO was pressed: auto-advance dialogue goes on or off.
    AutoAdvance,
    /// LOOK was pressed: first-person inspection goes on or off.
    FirstPerson,
    /// A shop quantity key.
    Quantity(QuantityKey),
}

/// Seconds and pixels within which a touch is a tap.
const TAP_SECONDS: f64 = 0.35;
const TAP_TRAVEL: f32 = 14.0;

#[derive(Default)]
pub struct Deck {
    pub controls: Vec<(Control, Rect)>,
    pub picture: Option<Rect>,
    fingers: BTreeMap<u64, Finger>,
    /// How far a lone finger that began on the picture moved, not yet
    /// taken (target pixels).
    drag: (f32, f32),
}

impl Deck {
    /// Lays the deck out for this frame (the target and where the picture is).
    pub fn arrange(&mut self, target: (f32, f32), picture: Rect, density: f32, ui: Context) {
        self.controls = layout(target, picture, density, ui);
        self.picture = Some(picture);
    }

    fn control_at(&self, at: (f32, f32)) -> Option<Control> {
        self.controls.iter().find(|(_, r)| r.contains(at)).map(|(c, _)| *c)
    }

    /// A finger touched down.
    pub fn down(&mut self, finger: u64, at: (f32, f32), time: f64) -> Touched {
        let control = self.control_at(at);
        let on_picture = control.is_none() && self.picture.is_some_and(|p| p.contains(at));
        self.fingers.insert(finger, Finger { control, start: at, began: time, travelled: false, on_picture, at });
        match control {
            Some(Control::Settings) => Touched::Settings,
            Some(Control::Auto) => Touched::AutoAdvance,
            Some(Control::Look) => Touched::FirstPerson,
            Some(Control::Quantity(k)) => Touched::Quantity(QuantityKey::ALL[k as usize]),
            _ => Touched::Nothing,
        }
    }

    /// A finger moved: a finger on the D-pad slides between its directions;
    /// one alone on the picture drags it.
    pub fn moved(&mut self, finger: u64, at: (f32, f32)) {
        let control = self.control_at(at);
        let alone = self.fingers.values().filter(|f| f.on_picture).count() == 1;
        if let Some(f) = self.fingers.get_mut(&finger) {
            let dx = at.0 - f.start.0;
            let dy = at.1 - f.start.1;
            if (dx * dx + dy * dy).sqrt() > TAP_TRAVEL {
                f.travelled = true;
            }
            if f.on_picture && alone {
                self.drag.0 += at.0 - f.at.0;
                self.drag.1 += at.1 - f.at.1;
            }
            f.at = at;
            let dpad = |c: Option<Control>| matches!(c, Some(Control::Up | Control::Down | Control::Left | Control::Right));
            if dpad(f.control) && dpad(control) {
                f.control = control;
            }
        }
    }

    /// A finger lifted (or the touch was cancelled: `cancelled`).
    pub fn up(&mut self, finger: u64, at: (f32, f32), time: f64, cancelled: bool) -> Touched {
        let Some(f) = self.fingers.remove(&finger) else { return Touched::Nothing };
        if cancelled || !f.on_picture || f.travelled || time - f.began > TAP_SECONDS {
            return Touched::Nothing;
        }
        if self.picture.is_some_and(|p| p.contains(at)) { Touched::Tap(at) } else { Touched::Nothing }
    }

    /// Lets everything go (focus lost, the panel took the screen).
    pub fn release(&mut self) {
        self.fingers.clear();
        self.drag = (0.0, 0.0);
    }

    /// How far a finger alone on the picture (one that began there) moved
    /// since the last call, in target pixels across and down: first-person
    /// inspection's look (two fingers there drag nothing).
    pub fn take_drag(&mut self) -> (f32, f32) {
        std::mem::take(&mut self.drag)
    }

    /// The controls fingers hold.
    pub fn held(&self) -> BTreeSet<Control> {
        self.fingers.values().filter_map(|f| f.control).collect()
    }

    /// The game actions the held controls hold.
    pub fn actions(&self) -> BTreeSet<GameAction> {
        self.held().into_iter().flat_map(Control::actions).collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn deck() -> Deck {
        let mut deck = Deck::default();
        // A 20:9 phone in landscape with a 4:3 picture in the middle.
        let picture = Rect { x: 480.0, y: 0.0, width: 1440.0, height: 1080.0 };
        deck.arrange((2400.0, 1080.0), picture, 3.0, Context::Walk);
        deck
    }

    #[test]
    fn controls_stand_beside_the_picture_with_generous_targets() {
        let deck = deck();
        let picture = deck.picture.unwrap();
        for (control, rect) in &deck.controls {
            if !matches!(control, Control::Settings | Control::Start) {
                assert!(rect.x + rect.width <= picture.x || rect.x >= picture.x + picture.width, "{control:?} {rect:?}");
            }
            assert!(rect.height >= 40.0 * 3.0 * 0.69, "{control:?} is too small: {rect:?}");
        }
    }

    #[test]
    fn fingers_hold_controls_slide_across_the_dpad_and_release() {
        let mut deck = deck();
        let at = |c: Control| deck.controls.iter().find(|(k, _)| *k == c).unwrap().1.centre();
        let (up, right, confirm) = (at(Control::Up), at(Control::Right), at(Control::Confirm));
        deck.down(1, up, 0.0);
        deck.down(2, confirm, 0.1);
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Up, GameAction::Confirm]));
        deck.moved(1, right);
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Right, GameAction::Confirm]));
        deck.up(2, confirm, 0.2, false);
        deck.up(1, right, 0.3, true);
        assert!(deck.actions().is_empty());
    }

    #[test]
    fn a_short_still_touch_on_the_picture_is_a_tap() {
        let mut deck = deck();
        deck.down(5, (1200.0, 500.0), 1.0);
        assert_eq!(deck.up(5, (1203.0, 502.0), 1.2, false), Touched::Tap((1203.0, 502.0)));
        deck.down(6, (1200.0, 500.0), 2.0);
        deck.moved(6, (1260.0, 500.0));
        assert_eq!(deck.up(6, (1260.0, 500.0), 2.1, false), Touched::Nothing, "a drag");
        deck.down(7, (1200.0, 500.0), 3.0);
        assert_eq!(deck.up(7, (1200.0, 500.0), 3.6, false), Touched::Nothing, "a hold");
        let settings = deck.controls.iter().find(|(k, _)| *k == Control::Settings).unwrap().1.centre();
        assert_eq!(deck.down(8, settings, 4.0), Touched::Settings);
    }

    #[test]
    fn select_stands_above_the_dpad_clear_of_every_other_control() {
        for (target, picture) in [
            // The phone's letterbox, and a 4:3 tablet's picture filling it.
            ((2400.0, 1080.0), Rect { x: 480.0, y: 0.0, width: 1440.0, height: 1080.0 }),
            ((2048.0, 1536.0), Rect { x: 0.0, y: 0.0, width: 2048.0, height: 1536.0 }),
        ] {
            for ui in [Context::Walk, Context::WorldMap { movement: 0, scene: 0 }] {
                let mut deck = Deck::default();
                deck.arrange(target, picture, 2.0, ui);
                let rect = |c: Control| deck.controls.iter().find(|(k, _)| *k == c).unwrap().1;
                let (select, up) = (rect(Control::Select), rect(Control::Up));
                assert!(select.y + select.height <= up.y && (select.centre().0 - up.centre().0).abs() < 1.0, "{select:?} over {up:?}");
                for (other, r) in deck.controls.iter().filter(|(k, _)| *k != Control::Select) {
                    let apart = r.x >= select.x + select.width || select.x >= r.x + r.width || r.y >= select.y + select.height || select.y >= r.y + r.height;
                    assert!(apart, "{other:?} {r:?} overlaps Select {select:?} ({ui:?})");
                }
                let mut deck = deck;
                deck.down(1, select.centre(), 0.0);
                assert_eq!(deck.actions(), BTreeSet::from([GameAction::Select]));
                assert_eq!(Control::Select.label(ui), "SELECT");
            }
        }
    }

    #[test]
    fn auto_shows_in_field_scenes_and_dialogue_clear_of_every_other_control() {
        let picture = Rect { x: 480.0, y: 0.0, width: 1440.0, height: 1080.0 };
        for ui in [Context::Event { map: 4, buttons: 0 }, Context::Dialogue { choice: None }] {
            let mut deck = Deck::default();
            deck.arrange((2400.0, 1080.0), picture, 3.0, ui);
            let auto = deck.controls.iter().find(|(k, _)| *k == Control::Auto).expect("AUTO in a scene").1;
            for (other, r) in deck.controls.iter().filter(|(k, _)| *k != Control::Auto) {
                let apart = r.x >= auto.x + auto.width || auto.x >= r.x + r.width || r.y >= auto.y + auto.height || auto.y >= r.y + r.height;
                assert!(apart, "{other:?} {r:?} overlaps AUTO {auto:?} ({ui:?})");
            }
            assert_eq!(deck.down(1, auto.centre(), 0.0), Touched::AutoAdvance);
            assert!(deck.actions().is_empty(), "the app's toggle presses nothing of the game's");
            assert_eq!(Control::Auto.label(ui), "AUTO");
        }
        for ui in [Context::Walk, Context::WorldMap { movement: 1, scene: 0 }, Context::Battle { page: 0, attack: false, ap: 0, list: false, targeting: false }] {
            let mut deck = Deck::default();
            deck.arrange((2400.0, 1080.0), picture, 3.0, ui);
            assert!(deck.controls.iter().all(|(k, _)| *k != Control::Auto), "no AUTO in {ui:?}");
        }
    }

    #[test]
    fn look_shows_in_a_field_beside_auto_clear_of_every_other_control_and_presses_nothing() {
        for (target, picture) in [
            // The phone's letterbox, a 16:9 phone without room beside the
            // picture, and a 4:3 tablet's picture filling it.
            ((2400.0, 1080.0), Rect { x: 480.0, y: 0.0, width: 1440.0, height: 1080.0 }),
            ((1920.0, 1080.0), Rect { x: 240.0, y: 0.0, width: 1440.0, height: 1080.0 }),
            ((2048.0, 1536.0), Rect { x: 0.0, y: 0.0, width: 2048.0, height: 1536.0 }),
        ] {
            for ui in [Context::Walk, Context::Event { map: 4, buttons: 0 }, Context::Dialogue { choice: None }] {
                let mut deck = Deck::default();
                deck.arrange(target, picture, 3.0, ui);
                let look = deck.controls.iter().find(|(k, _)| *k == Control::Look).expect("LOOK in a field").1;
                for (other, r) in deck.controls.iter().filter(|(k, _)| *k != Control::Look) {
                    let apart = r.x >= look.x + look.width || look.x >= r.x + r.width || r.y >= look.y + look.height || look.y >= r.y + r.height;
                    assert!(apart, "{other:?} {r:?} overlaps LOOK {look:?} ({ui:?})");
                }
                assert!(look.x >= 0.0 && look.x + look.width <= target.0, "LOOK within the screen: {look:?}");
                assert_eq!(deck.down(1, look.centre(), 0.0), Touched::FirstPerson);
                assert!(deck.actions().is_empty(), "the app's toggle presses nothing of the game's (not Select)");
                assert_eq!(Control::Look.label(ui), "LOOK");
            }
        }
        let picture = Rect { x: 480.0, y: 0.0, width: 1440.0, height: 1080.0 };
        let menu = Context::Menu { screen: 0, cursor: 0, choice: 0, choices: 0, commands: true, open: false, switch: false, quantities: false };
        for ui in [menu, Context::WorldMap { movement: 1, scene: 0 }, Context::Battle { page: 0, attack: false, ap: 0, list: false, targeting: false }, Context::None] {
            let mut deck = Deck::default();
            deck.arrange((2400.0, 1080.0), picture, 3.0, ui);
            assert!(deck.controls.iter().all(|(k, _)| *k != Control::Look), "no LOOK in {ui:?}");
        }
        use crate::draw::deck_state;
        use xem_render::glass::State;
        assert_eq!(deck_state(Control::Look, false, true, true), State::Active, "lit while the view shows");
        assert_eq!(deck_state(Control::Look, false, true, false), State::Normal);
        assert_eq!(deck_state(Control::Auto, false, true, false), State::Active, "AUTO keeps its own light");
        assert_eq!(deck_state(Control::Look, true, false, true), State::Pressed);
    }

    #[test]
    fn a_lone_finger_dragged_on_the_picture_is_taken_as_a_drag() {
        let mut deck = deck();
        let dpad = deck.controls.iter().find(|(k, _)| *k == Control::Up).unwrap().1.centre();
        deck.down(1, (1200.0, 500.0), 0.0);
        deck.moved(1, (1230.0, 490.0));
        deck.moved(1, (1250.0, 520.0));
        assert_eq!(deck.take_drag(), (50.0, 20.0), "what it moved since it touched");
        assert_eq!(deck.take_drag(), (0.0, 0.0), "taken once");
        // A finger on the deck drags nothing; a second on the picture stops
        // the drag (two fingers are no look).
        deck.down(2, dpad, 0.1);
        deck.moved(2, (dpad.0 + 40.0, dpad.1));
        deck.moved(1, (1260.0, 520.0));
        assert_eq!(deck.take_drag(), (10.0, 0.0));
        deck.down(3, (1000.0, 600.0), 0.2);
        deck.moved(1, (1300.0, 520.0));
        deck.moved(3, (1050.0, 600.0));
        assert_eq!(deck.take_drag(), (0.0, 0.0));
        assert_eq!(deck.up(1, (1300.0, 520.0), 0.3, false), Touched::Nothing, "a drag is no tap");
        deck.moved(3, (1080.0, 600.0));
        deck.release();
        assert_eq!(deck.take_drag(), (0.0, 0.0), "letting go forgets it");
    }
}
