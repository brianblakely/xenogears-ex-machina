//! Touch screens (Android, phone and tablet browsers): the game's controls
//! as a Seraph Glass deck beside the picture, and taps on the picture
//! resolved as the headset's pointer gestures are.
//!
//! - The deck: a D-pad on the left, the four face buttons on the right
//!   (Jump, Cancel and Run, Confirm, Menu), the camera's turns at the top
//!   corners, Start and Settings at the top. A finger holds the control it
//!   is on, sliding from one to another (the D-pad's directions), and
//!   lets go when lifted or cancelled: explicit press and release. In
//!   Battling the face buttons show the bout's own names (A, B, Jump,
//!   Dash) and the turns its Charge and Special. Controls stand in the
//!   letterbox beside the picture when it leaves room, else over its
//!   corners.
//! - A tap on the picture (a short touch that does not travel) is the
//!   pointer's tap: walking, on an actor it approaches and talks, on the
//!   floor it walks there; in a menu or a choice it selects the entry and
//!   confirms it; in a dialogue it advances.
//! - A shop's quantity list adds the +, -, ALL and NONE keys.
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
    Settings,
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
            Control::Settings | Control::Quantity(_) => vec![],
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
            Control::Settings => "SETTINGS",
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
    if matches!(ui, Context::Menu { quantities: true, .. }) {
        let x = right_x;
        for (i, _) in QuantityKey::ALL.iter().enumerate() {
            let y = top + size * 0.9 + i as f32 * size * 0.8;
            controls.push((Control::Quantity(i as u8), Rect { x: x - size * 0.8, y, width: size * 1.6, height: size * 0.7 }));
        }
    }
    controls
}

/// A finger's first touch: where and when, and whether it travelled.
#[derive(Clone, Copy, Debug)]
struct Finger {
    control: Option<Control>,
    start: (f32, f32),
    began: f64,
    travelled: bool,
    on_picture: bool,
}

/// What a touch did.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Touched {
    Nothing,
    /// A tap on the picture, at target pixels.
    Tap((f32, f32)),
    /// Settings was pressed.
    Settings,
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
        self.fingers.insert(finger, Finger { control, start: at, began: time, travelled: false, on_picture });
        match control {
            Some(Control::Settings) => Touched::Settings,
            Some(Control::Quantity(k)) => Touched::Quantity(QuantityKey::ALL[k as usize]),
            _ => Touched::Nothing,
        }
    }

    /// A finger moved: a finger on the D-pad slides between its directions.
    pub fn moved(&mut self, finger: u64, at: (f32, f32)) {
        let control = self.control_at(at);
        if let Some(f) = self.fingers.get_mut(&finger) {
            let dx = at.0 - f.start.0;
            let dy = at.1 - f.start.1;
            if (dx * dx + dy * dy).sqrt() > TAP_TRAVEL {
                f.travelled = true;
            }
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
}
