//! Touch screens (Android, phone and tablet browsers): the game's controls
//! as a Seraph Glass deck beside the picture, and taps on the picture
//! resolved as the headset's pointer gestures are.
//!
//! - The deck: a stick on the left with Select above it, the four face
//!   buttons on the right (Jump, Cancel and Run, Confirm, Menu), the
//!   camera's turns at the top corners, Start and Settings at the top. The
//!   stick is relative: a finger touching the screen's left half (on no
//!   other control) centres it where it touched, and its travel from there
//!   holds the D-pad's eighth (a direction, or two together on a diagonal)
//!   beyond a small dead zone, the centre following a finger that goes
//!   past the knob's reach; untouched it rests where a D-pad would stand.
//!   A finger holds the control it is on, and lets go when lifted or
//!   cancelled: explicit press and release. In Battling the face buttons
//!   show the bout's own names (A, B, Jump, Dash) and the turns its Charge
//!   and Special. Controls stand in the letterbox beside the picture when
//!   it leaves room, else over its corners.
//! - A tap on the picture (a short touch that does not travel, the stick's
//!   half too) is the pointer's tap: walking, on an actor it approaches and
//!   talks, on the floor it walks there; in a menu or a choice it selects
//!   the entry and confirms it; in a dialogue it advances. A finger dragged
//!   on the picture turns first-person inspection's view
//!   ([`Deck::take_drag`]); while that view shows there is no stick, so a
//!   finger on the left half turns it too.
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
    /// The D-pad's directions, which the stick holds (no keys of their own).
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

/// The stick (target pixels): where it rests while no finger holds it,
/// where a finger touching no control takes it, and its knob.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Stick {
    /// Its centre at rest, where a D-pad would stand.
    pub rest: (f32, f32),
    /// The screen's left half.
    pub zone: Rect,
    /// How far the knob's centre travels from the stick's centre.
    pub reach: f32,
    /// The knob's size (a button's).
    pub knob: f32,
}

impl Stick {
    /// Its well's radius: the knob stays within it.
    pub fn radius(&self) -> f32 {
        self.reach + self.knob / 2.0
    }

    /// The travel within which it holds nothing.
    pub fn dead_zone(&self) -> f32 {
        self.reach * STICK_DEAD_ZONE
    }
}

/// The knob's reach, in buttons, and the dead zone, in reaches.
const STICK_REACH: f32 = 0.75;
const STICK_DEAD_ZONE: f32 = 0.25;

/// How the stick shows (target pixels): its centre and its knob's, whether
/// a finger holds it, and the D-pad's directions it holds (up, down, left,
/// right).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct StickShown {
    pub centre: (f32, f32),
    pub knob: (f32, f32),
    pub held: bool,
    pub directions: [bool; 4],
}

/// The deck's controls and where they stand, and the stick.
pub fn layout(target: (f32, f32), picture: Rect, density: f32, ui: Context) -> (Vec<(Control, Rect)>, Stick) {
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
    let face = cluster(right_x, low);
    let mut controls = vec![
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
    // A wide key above a cluster: Select over the stick, the world map's
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
    let zone = Rect { x: 0.0, y: 0.0, width: w / 2.0, height: h };
    (controls, Stick { rest: (left_x, low), zone, reach: size * STICK_REACH, knob: size })
}

/// A finger's first touch: where and when, whether it travelled, where it
/// is now, and the stick while it holds it.
#[derive(Clone, Copy, Debug)]
struct Finger {
    control: Option<Control>,
    start: (f32, f32),
    began: f64,
    travelled: bool,
    on_picture: bool,
    at: (f32, f32),
    steer: Option<Steer>,
}

/// A finger's hold of the stick: its centre (where the finger touched,
/// following a finger past the reach) and the eighth it holds.
#[derive(Clone, Copy, Debug)]
struct Steer {
    centre: (f32, f32),
    sector: Option<i32>,
}

impl Steer {
    /// The finger now at `at`.
    fn to(&mut self, at: (f32, f32), stick: &Stick) {
        let (mut dx, mut dy) = (at.0 - self.centre.0, at.1 - self.centre.1);
        let travel = (dx * dx + dy * dy).sqrt();
        if travel > stick.reach {
            (dx, dy) = (dx * stick.reach / travel, dy * stick.reach / travel);
            self.centre = (at.0 - dx, at.1 - dy);
        }
        self.sector = (travel > stick.dead_zone()).then(|| {
            // Screen directions: Right 0, Down 0x400, Left 0x800, Up 0xC00.
            let dir = (dy.atan2(dx) / std::f32::consts::TAU * 4096.0).round() as i32;
            crate::arcade::eighth(dir.rem_euclid(4096), self.sector)
        });
    }

    /// The D-pad's directions held: up, down, left, right.
    fn directions(&self) -> [bool; 4] {
        self.sector.map_or([false; 4], xem_core::field::dpad_of_sector)
    }
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
    /// The stick, but while first-person inspection shows.
    pub stick: Option<Stick>,
    fingers: BTreeMap<u64, Finger>,
    /// How far a lone finger that began on the picture moved, not yet
    /// taken (target pixels).
    drag: (f32, f32),
}

impl Deck {
    /// Lays the deck out for this frame (the target, where the picture is,
    /// and whether first-person inspection shows: its view has no stick, a
    /// finger holding it lets it go and drags as any other).
    pub fn arrange(&mut self, target: (f32, f32), picture: Rect, density: f32, ui: Context, inspecting: bool) {
        let (controls, stick) = layout(target, picture, density, ui);
        self.controls = controls;
        self.picture = Some(picture);
        self.stick = (!inspecting).then_some(stick);
        if inspecting {
            self.fingers.values_mut().for_each(|f| f.steer = None);
        }
    }

    fn control_at(&self, at: (f32, f32)) -> Option<Control> {
        self.controls.iter().find(|(_, r)| r.contains(at)).map(|(c, _)| *c)
    }

    /// A finger touched down: on no control in the stick's half (and no
    /// other finger holding it), it takes the stick, centred there.
    pub fn down(&mut self, finger: u64, at: (f32, f32), time: f64) -> Touched {
        let control = self.control_at(at);
        let on_picture = control.is_none() && self.picture.is_some_and(|p| p.contains(at));
        let free = !self.fingers.values().any(|f| f.steer.is_some());
        let steer = (control.is_none() && free && self.stick.is_some_and(|s| s.zone.contains(at))).then_some(Steer { centre: at, sector: None });
        self.fingers.insert(finger, Finger { control, start: at, began: time, travelled: false, on_picture, at, steer });
        match control {
            Some(Control::Settings) => Touched::Settings,
            Some(Control::Auto) => Touched::AutoAdvance,
            Some(Control::Look) => Touched::FirstPerson,
            Some(Control::Quantity(k)) => Touched::Quantity(QuantityKey::ALL[k as usize]),
            _ => Touched::Nothing,
        }
    }

    /// A finger moved: one holding the stick steers it; one alone on the
    /// picture (the stick's aside) drags it.
    pub fn moved(&mut self, finger: u64, at: (f32, f32)) {
        let alone = self.fingers.values().filter(|f| f.on_picture && f.steer.is_none()).count() == 1;
        let stick = self.stick;
        if let Some(f) = self.fingers.get_mut(&finger) {
            let dx = at.0 - f.start.0;
            let dy = at.1 - f.start.1;
            if (dx * dx + dy * dy).sqrt() > TAP_TRAVEL {
                f.travelled = true;
            }
            match (&mut f.steer, stick) {
                (Some(steer), Some(stick)) => steer.to(at, &stick),
                _ if f.on_picture && alone => {
                    self.drag.0 += at.0 - f.at.0;
                    self.drag.1 += at.1 - f.at.1;
                }
                _ => {}
            }
            f.at = at;
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

    /// The controls fingers hold, the stick's directions among them.
    pub fn held(&self) -> BTreeSet<Control> {
        let mut held: BTreeSet<Control> = self.fingers.values().filter_map(|f| f.control).collect();
        for steer in self.fingers.values().filter_map(|f| f.steer) {
            let directions = [Control::Up, Control::Down, Control::Left, Control::Right];
            held.extend(directions.into_iter().zip(steer.directions()).filter_map(|(c, on)| on.then_some(c)));
        }
        held
    }

    /// The game actions the held controls hold.
    pub fn actions(&self) -> BTreeSet<GameAction> {
        self.held().into_iter().flat_map(Control::actions).collect()
    }

    /// How the stick shows: where a finger holds it, else at rest.
    pub fn stick_shown(&self) -> Option<StickShown> {
        let stick = self.stick?;
        Some(match self.fingers.values().find_map(|f| f.steer.map(|s| (s, f.at))) {
            Some((steer, at)) => {
                let (dx, dy) = (at.0 - steer.centre.0, at.1 - steer.centre.1);
                let k = stick.reach / (dx * dx + dy * dy).sqrt().max(stick.reach);
                StickShown { centre: steer.centre, knob: (steer.centre.0 + dx * k, steer.centre.1 + dy * k), held: true, directions: steer.directions() }
            }
            None => StickShown { centre: stick.rest, knob: stick.rest, held: false, directions: [false; 4] },
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn deck() -> Deck {
        let mut deck = Deck::default();
        // A 20:9 phone in landscape with a 4:3 picture in the middle.
        let picture = Rect { x: 480.0, y: 0.0, width: 1440.0, height: 1080.0 };
        deck.arrange((2400.0, 1080.0), picture, 3.0, Context::Walk, false);
        deck
    }

    fn apart(a: Rect, b: Rect) -> bool {
        a.x >= b.x + b.width || b.x >= a.x + a.width || a.y >= b.y + b.height || b.y >= a.y + a.height
    }

    /// The square the stick's well fills at rest.
    fn well(stick: Stick) -> Rect {
        let r = stick.radius();
        Rect { x: stick.rest.0 - r, y: stick.rest.1 - r, width: 2.0 * r, height: 2.0 * r }
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
        let well = well(deck.stick.unwrap());
        assert!(well.x >= 0.0 && well.x + well.width <= picture.x, "the stick rests beside the picture: {well:?}");
        assert!(deck.controls.iter().all(|(_, r)| apart(*r, well)), "the stick's well is clear of the keys");
    }

    #[test]
    fn a_finger_on_the_left_half_steers_the_stick_from_where_it_touched_and_lets_go() {
        let mut deck = deck();
        let stick = deck.stick.unwrap();
        let (reach, dead) = (stick.reach, stick.dead_zone());
        assert!(dead > 0.0 && dead < reach);
        // On the picture's left half, nowhere near where the stick rests.
        let (x, y) = (700.0, 300.0);
        assert!(deck.control_at((x, y)).is_none() && stick.zone.contains((x, y)));
        assert_eq!(deck.down(1, (x, y), 0.0), Touched::Nothing);
        assert_eq!(deck.stick_shown().unwrap().centre, (x, y), "centred where it touched");
        assert!(deck.actions().is_empty(), "nothing held before it travels");
        deck.moved(1, (x + dead * 0.8, y));
        assert!(deck.actions().is_empty(), "within the dead zone");
        deck.moved(1, (x + reach * 0.6, y));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Right]));
        deck.moved(1, (x + reach * 0.5, y - reach * 0.5));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Up, GameAction::Right]), "a diagonal holds two");
        deck.moved(1, (x, y + reach * 0.7));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Down]));
        // Its eighth holds a few degrees past a boundary (22.5 degrees off
        // Down, toward Down and Left).
        let off = |degrees: f32| (x + reach * 0.7 * (90.0 + degrees).to_radians().cos(), y + reach * 0.7 * (90.0 + degrees).to_radians().sin());
        deck.moved(1, off(25.0));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Down]), "kept near the boundary");
        deck.moved(1, off(35.0));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Down, GameAction::Left]));
        // Past its reach the centre follows: a short way back turns it round.
        deck.moved(1, (x - 3.0 * reach, y));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Left]));
        let shown = deck.stick_shown().unwrap();
        let near = |a: (f32, f32), b: (f32, f32)| (a.0 - b.0).abs() < 0.01 && (a.1 - b.1).abs() < 0.01;
        assert!(near(shown.centre, (x - 2.0 * reach, y)) && near(shown.knob, (x - 3.0 * reach, y)), "{shown:?}");
        assert!(shown.held && shown.directions == [false, false, true, false], "{shown:?}");
        deck.moved(1, (x - 2.0 * reach + dead * 1.5, y));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Right]));
        // Another finger holds a button meanwhile; a third on the left half
        // takes no second stick.
        let confirm = deck.controls.iter().find(|(k, _)| *k == Control::Confirm).unwrap().1.centre();
        deck.down(2, confirm, 0.1);
        deck.down(3, (600.0, 700.0), 0.1);
        deck.moved(3, (600.0 + reach, 700.0));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Right, GameAction::Confirm]));
        assert_eq!(deck.held(), BTreeSet::from([Control::Right, Control::Confirm]));
        deck.up(2, confirm, 0.2, false);
        deck.up(3, (600.0 + reach, 700.0), 0.2, false);
        deck.up(1, (x, y), 0.3, true);
        assert!(deck.actions().is_empty(), "lifted and cancelled");
        assert_eq!(deck.stick_shown().unwrap(), StickShown { centre: stick.rest, knob: stick.rest, held: false, directions: [false; 4] }, "back at rest");
        // The right half takes no stick.
        deck.down(4, (1700.0, 300.0), 1.0);
        deck.moved(4, (1700.0 + reach, 300.0));
        assert!(deck.actions().is_empty() && !deck.stick_shown().unwrap().held);
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
        // On the stick's half too: a still touch holds nothing and taps.
        deck.down(9, (700.0, 500.0), 5.0);
        assert!(deck.stick_shown().unwrap().held && deck.actions().is_empty());
        assert_eq!(deck.up(9, (703.0, 502.0), 5.2, false), Touched::Tap((703.0, 502.0)));
    }

    #[test]
    fn select_stands_above_the_stick_clear_of_every_other_control() {
        for (target, picture) in [
            // The phone's letterbox, and a 4:3 tablet's picture filling it.
            ((2400.0, 1080.0), Rect { x: 480.0, y: 0.0, width: 1440.0, height: 1080.0 }),
            ((2048.0, 1536.0), Rect { x: 0.0, y: 0.0, width: 2048.0, height: 1536.0 }),
        ] {
            for ui in [Context::Walk, Context::WorldMap { movement: 0, scene: 0 }] {
                let mut deck = Deck::default();
                deck.arrange(target, picture, 2.0, ui, false);
                let select = deck.controls.iter().find(|(k, _)| *k == Control::Select).unwrap().1;
                let stick = deck.stick.unwrap();
                let well = well(stick);
                assert!(select.y + select.height <= well.y && (select.centre().0 - stick.rest.0).abs() < 1.0, "{select:?} over {well:?}");
                for (other, r) in deck.controls.iter().filter(|(k, _)| *k != Control::Select) {
                    assert!(apart(*r, select), "{other:?} {r:?} overlaps Select {select:?} ({ui:?})");
                    assert!(apart(*r, well), "{other:?} {r:?} overlaps the stick's well {well:?} ({ui:?})");
                }
                assert!(well.x >= 0.0 && well.y + well.height <= target.1, "the stick within the screen: {well:?}");
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
            deck.arrange((2400.0, 1080.0), picture, 3.0, ui, false);
            let auto = deck.controls.iter().find(|(k, _)| *k == Control::Auto).expect("AUTO in a scene").1;
            for (other, r) in deck.controls.iter().filter(|(k, _)| *k != Control::Auto) {
                let apart = r.x >= auto.x + auto.width || auto.x >= r.x + r.width || r.y >= auto.y + auto.height || auto.y >= r.y + r.height;
                assert!(apart, "{other:?} {r:?} overlaps AUTO {auto:?} ({ui:?})");
            }
            assert_eq!(deck.down(1, auto.centre(), 0.0), Touched::AutoAdvance);
            assert!(deck.actions().is_empty(), "the app's toggle presses nothing of the game's");
            assert_eq!(Control::Auto.label(ui), "AUTO");
        }
        for ui in [Context::Walk, Context::WorldMap { movement: 1, scene: 0 }, Context::Battle { page: 0, attack: false, ap: 0, list: false, targeting: false, message: false }] {
            let mut deck = Deck::default();
            deck.arrange((2400.0, 1080.0), picture, 3.0, ui, false);
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
                deck.arrange(target, picture, 3.0, ui, false);
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
        for ui in [menu, Context::WorldMap { movement: 1, scene: 0 }, Context::Battle { page: 0, attack: false, ap: 0, list: false, targeting: false, message: false }, Context::None] {
            let mut deck = Deck::default();
            deck.arrange((2400.0, 1080.0), picture, 3.0, ui, false);
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
        deck.down(1, (1500.0, 500.0), 0.0);
        deck.moved(1, (1530.0, 490.0));
        deck.moved(1, (1550.0, 520.0));
        assert_eq!(deck.take_drag(), (50.0, 20.0), "what it moved since it touched");
        assert_eq!(deck.take_drag(), (0.0, 0.0), "taken once");
        // A finger on the stick (on the picture's left half) drags nothing;
        // a second on the picture's right half stops the drag (two fingers
        // are no look).
        deck.down(2, (700.0, 500.0), 0.1);
        deck.moved(2, (740.0, 500.0));
        deck.moved(1, (1560.0, 520.0));
        assert_eq!(deck.take_drag(), (10.0, 0.0));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Right]), "the stick steers");
        deck.down(3, (1300.0, 600.0), 0.2);
        deck.moved(1, (1600.0, 520.0));
        deck.moved(3, (1350.0, 600.0));
        assert_eq!(deck.take_drag(), (0.0, 0.0));
        assert_eq!(deck.up(1, (1600.0, 520.0), 0.3, false), Touched::Nothing, "a drag is no tap");
        deck.moved(3, (1380.0, 600.0));
        deck.release();
        assert_eq!(deck.take_drag(), (0.0, 0.0), "letting go forgets it");
        assert!(deck.actions().is_empty());
    }

    #[test]
    fn first_person_inspection_has_no_stick_and_a_finger_on_the_left_half_turns_the_view() {
        let mut deck = deck();
        let picture = deck.picture.unwrap();
        // A finger holding the stick as the view comes lets it go.
        deck.down(1, (700.0, 500.0), 0.0);
        deck.moved(1, (800.0, 500.0));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Right]));
        deck.take_drag();
        deck.arrange((2400.0, 1080.0), picture, 3.0, Context::Walk, true);
        assert!(deck.stick.is_none() && deck.stick_shown().is_none() && deck.actions().is_empty());
        deck.moved(1, (820.0, 490.0));
        assert_eq!(deck.take_drag(), (20.0, -10.0), "it drags as any finger on the picture");
        deck.up(1, (820.0, 490.0), 0.5, false);
        deck.down(2, (600.0, 600.0), 1.0);
        deck.moved(2, (640.0, 600.0));
        assert_eq!(deck.take_drag(), (40.0, 0.0));
        assert!(deck.actions().is_empty());
        deck.up(2, (640.0, 600.0), 1.5, false);
        // The view gone, the stick is back.
        deck.arrange((2400.0, 1080.0), picture, 3.0, Context::Walk, false);
        deck.down(3, (600.0, 600.0), 2.0);
        deck.moved(3, (640.0, 600.0));
        assert_eq!((deck.take_drag(), deck.actions()), ((0.0, 0.0), BTreeSet::from([GameAction::Right])));
    }
}
