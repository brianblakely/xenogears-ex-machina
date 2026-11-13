//! Touch screens (Android, phone and tablet browsers): the game's controls
//! as a Seraph Glass deck laid out for two thumbs, and taps on the picture
//! resolved as the headset's pointer gestures are.
//!
//! - **A deck for what the game plays now**, not the pad's ([`keys`]): each
//!   context has the buttons the game reads there, named for what they do
//!   (walking: TALK, JUMP, the camera's turns, MENU; a dialogue: NEXT; a
//!   menu: OK, BACK, the page's or the character's arrows; a battle's attack
//!   page: the three attacks by their symbols and names, and END; the world
//!   map: ENTER, BOARD or the ship's LAND and THROTTLE; a Battling bout: A,
//!   B, JUMP, SPECIAL, DASH, CHARGE; a field's script-driven activity: the
//!   buttons its scripts test, by the symbols its prompts name).
//! - **Laid out for thumbs** ([`layout`]): the right thumb rests on the
//!   primary button (Confirm in every deck, always in the same place), the
//!   largest, with up to three round buttons on an arc about it, small keys
//!   in rows above the arc, and the app's Settings and the game's pause in
//!   the top corner. The left thumb has the stick. In the letterbox beside
//!   the picture when it has room, else over the picture's corners.
//! - **The stick** is relative: a finger touching the screen's left half
//!   (on no other control) centres it where it touched, and its travel from
//!   there holds the D-pad's eighth (a direction, or two together on a
//!   diagonal) beyond a small dead zone, the centre following a finger that
//!   goes past the knob's reach. It shows only under a finger holding it:
//!   where the game reads the directions as soon as the finger touches,
//!   elsewhere (a dialogue's page, a scene) once that finger steers it.
//! - **Holding.** A finger holds the control it is on and lets go when
//!   lifted or cancelled: explicit press and release. A bout's DASH and
//!   CHARGE and the ship's THROTTLE latch instead (a touch holds them, the
//!   next lets go; lit meanwhile), as the headset decks' held keys do: the
//!   thumb that holds them has the other buttons to press. RUN, AUTO and
//!   LOOK are the app's toggles (run by default, auto-advance dialogue,
//!   first-person inspection), lit while on, holding nothing of the game's.
//! - A tap on the picture (a short touch that does not travel, the stick's
//!   half too) is the pointer's tap: walking, on an actor it approaches and
//!   talks, on the floor it walks there; in a menu or a choice it selects
//!   the entry and confirms it; in a dialogue it advances. A finger dragged
//!   on the picture turns first-person inspection's view
//!   ([`Deck::take_drag`]); while that view shows there is no stick, so a
//!   finger on the left half turns it too.
use crate::overlay::QuantityKey;
use std::collections::{BTreeMap, BTreeSet};
use xem_core::action::GameAction;
use xem_core::context::Context;

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
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
    /// The game's Select: a field map's picture, the world map's view; held
    /// over a dialogue what hides its windows (the hosts').
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
    /// Run by default on or off (the app's setting, not the game's button).
    Run,
    L2,
    R2,
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
            Control::L2 => vec![GameAction::L2],
            Control::R2 => vec![GameAction::R2],
            Control::Settings | Control::Auto | Control::Look | Control::Run | Control::Quantity(_) => vec![],
        }
    }

    /// Its label in `scene`: a word (capitals), a symbol by name (the
    /// arrows, the pad's face shapes, `pause`, `gear`, `turn left`), or a
    /// symbol with a word under it (`triangle:WEAK`).
    pub fn label(self, scene: Scene) -> &'static str {
        let ui = scene.ui;
        let attack = matches!(ui, Context::Battle { attack: true, .. });
        let bout = matches!(ui, Context::Arena { state: 1 });
        // A field's scenes and dialogue name the buttons its scripts test by
        // their symbols, as its prompts do; the walking deck's own buttons
        // keep their names.
        let tested = scene.tested();
        let symbol = |action: GameAction| {
            let read = crate::arcade::activity_button(action).is_some_and(|bit| tested & bit != 0);
            (read && matches!(ui, Context::Event { .. } | Context::Dialogue { .. })).then(|| crate::arcade::activity_label(action)).flatten()
        };
        match self {
            Control::Up => "^",
            Control::Down => "v",
            Control::Left => "<",
            Control::Right => ">",
            Control::Confirm => match ui {
                _ if attack => "cross:FIERCE",
                _ if bout => "A",
                Context::Walk => "TALK",
                Context::WorldMap { scene, .. } if scene < 8 => "ENTER",
                // Where a field's scripts test it, by its symbol too, as
                // their prompts name it.
                _ if advances(ui) => if symbol(GameAction::Confirm).is_some() { "cross:NEXT" } else { "NEXT" },
                _ => if symbol(GameAction::Confirm).is_some() { "cross:OK" } else { "OK" },
            },
            Control::Cancel => match ui {
                _ if attack => "END",
                _ if bout => "DASH",
                Context::WorldMap { .. } => "LAND",
                // In a field, a button its scripts test and nothing else.
                Context::Walk | Context::Event { .. } | Context::Dialogue { .. } => "circle",
                _ => "BACK",
            },
            Control::Jump => match ui {
                _ if attack => "triangle:WEAK",
                Context::WorldMap { .. } => "THROTTLE",
                _ => symbol(GameAction::Jump).unwrap_or("JUMP"),
            },
            Control::Menu => match ui {
                _ if attack => "square:STRONG",
                _ if bout => "B",
                Context::Menu { .. } => "CLOSE",
                _ => symbol(GameAction::Menu).unwrap_or("MENU"),
            },
            Control::CameraLeft => match ui {
                _ if bout => "CHARGE",
                Context::Menu { .. } => "<",
                _ => symbol(GameAction::CameraLeft).unwrap_or("turn left"),
            },
            Control::CameraRight => match ui {
                _ if bout => "SPECIAL",
                Context::Menu { .. } => ">",
                _ => symbol(GameAction::CameraRight).unwrap_or("turn right"),
            },
            Control::Start => if matches!(ui, Context::Menu { .. }) { "DONE" } else { "pause" },
            Control::Select => match ui {
                Context::Walk if scene.picture => "MAP",
                Context::Walk => "SELECT",
                Context::WorldMap { .. } => "VIEW",
                _ => symbol(GameAction::Select).unwrap_or("HIDE"),
            },
            Control::Settings => "gear",
            Control::Board => "BOARD",
            Control::Auto => "AUTO",
            Control::Look => "LOOK",
            Control::Run => "RUN",
            Control::L2 => "L2",
            Control::R2 => "R2",
            Control::Quantity(k) => QuantityKey::ALL[k as usize].label(),
        }
    }

    /// Whether a touch latches the control in `ui` (held until the next
    /// touch of it) rather than holding it while the finger stays: the
    /// buttons the game wants held while the same thumb presses others (a
    /// bout's Dash and Charge) or for a long while (the ship's throttle), as
    /// the headset decks' held keys (`arcade::keys`).
    pub fn latches(self, ui: Context) -> bool {
        match ui {
            Context::Arena { state: 1 } => matches!(self, Control::Cancel | Control::CameraLeft),
            Context::WorldMap { .. } => self == Control::Jump,
            _ => false,
        }
    }
}

/// Whether Confirm does nothing in `ui` but go on to the next page: a
/// dialogue without a choice, a scene, a battle's script message, the world
/// map's scripted scenes.
fn advances(ui: Context) -> bool {
    match ui {
        Context::Dialogue { choice: None } | Context::Event { .. } => true,
        Context::Battle { message, attack, list, targeting, .. } => message && !attack && !list && !targeting,
        Context::WorldMap { scene, .. } => scene >= 8,
        _ => false,
    }
}

/// What a deck is laid out for: the game's context, and what the hosts know
/// beside it.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Scene {
    pub ui: Context,
    /// First-person inspection shows: its view has no stick, and the
    /// buttons it holds back from the game (the jump, the camera's turns)
    /// have no place on the deck.
    pub inspecting: bool,
    /// Select shows the field map's picture here (`FieldControls::picture`).
    pub picture: bool,
    /// The pad buttons the field map's scripts test
    /// (`xem_core::context::field_script_buttons`): an activity's, which the
    /// field's decks all offer ([`Scene::tested`]).
    pub tested: u16,
}

impl Scene {
    /// The pad buttons the field's scripts may be testing now, as the game
    /// reads them (`arcade::activity_button`): the map's while the player
    /// walks, a scene plays or a dialogue shows (a script goes on testing
    /// the pad while another holds a window open, or beside the player's
    /// control); none outside a field.
    pub fn tested(&self) -> u16 {
        match self.ui {
            Context::Event { buttons, .. } => buttons | self.tested,
            Context::Walk | Context::Dialogue { .. } => self.tested,
            _ => 0,
        }
    }
}

impl Default for Scene {
    fn default() -> Self {
        Scene::from(Context::None)
    }
}

impl From<Context> for Scene {
    fn from(ui: Context) -> Self {
        Scene { ui, inspecting: false, picture: false, tested: 0 }
    }
}

/// Where a control stands on the deck ([`layout`]).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Slot {
    /// Under the right thumb at rest: the largest button.
    Primary,
    /// A round button on the arc about it: beside it, toward the screen's
    /// middle.
    Beside,
    /// On the arc between the two (a little larger: the next most used).
    Between,
    /// On the arc above it.
    Above,
    /// A key in the rows of two above the arc, from the lowest row's outer
    /// key (0), its inner one (1), the next row's outer one (2), ...
    Key(u8),
    /// A key above the stick, the lowest first.
    Left(u8),
    /// A small round button in the top right corner, the outermost first.
    Corner(u8),
}

/// The deck of `scene`: its controls and where each stands, and whether the
/// game reads the directions there ([`Stick::reads`]). The buttons
/// are those the game reads in that context (docs/runtime.md, Touch
/// screens, lists what each presses).
pub fn keys(scene: Scene) -> (Vec<(Control, Slot)>, bool) {
    use Control::*;
    use Slot::{Above, Beside, Between, Corner, Key, Primary};
    let Scene { ui, inspecting, picture, .. } = scene;
    // Confirm is every deck's primary button: the thumb finds it where it
    // was, whatever it does now.
    let mut keys = vec![(Settings, Corner(0)), (Confirm, Primary)];
    // The game's pause (Start) beside Settings, but in a menu, which reads
    // none.
    let mut pause = true;
    let mut stick = false;
    // A field's decks offer every button its map's scripts test (an
    // activity's), but those first-person inspection holds back.
    let tested = |action: GameAction| {
        crate::arcade::activity_button(action).is_some_and(|bit| scene.tested() & bit != 0) && !(inspecting && xem_core::action::INSPECTION_HELD.contains(&action))
    };
    match ui {
        // Booting, a movie, a mode change: Confirm (and Start) for what
        // takes one.
        Context::None => {}
        Context::Walk => {
            stick = true;
            // First-person inspection holds the walk, the jump and the
            // camera's turns back: its deck has LOOK to leave it, and the
            // menu.
            if !inspecting {
                keys.extend([(CameraLeft, Beside), (Jump, Between), (CameraRight, Above), (Run, Key(1))]);
            }
            keys.extend([(Menu, Key(0)), (Look, Key(2))]);
            // Select: the map's picture (MAP), or a button the scripts test.
            if picture || tested(GameAction::Select) {
                keys.push((Select, Key(3)));
            }
            // What else the scripts test: the held run button, L2 and R2
            // (the walking deck has the rest).
            for (control, action, slot) in [(Cancel, GameAction::Cancel, Key(4)), (R2, GameAction::R2, Key(5)), (L2, GameAction::L2, Slot::Left(0))] {
                if tested(action) {
                    keys.push((control, slot));
                }
            }
        }
        // A scene and a dialogue: the next page, AUTO, HIDE (Select held)
        // and LOOK; a choice takes the stick; an activity adds the buttons
        // its scripts test (the face buttons on the arc as they lie on the
        // pad about Cross, L1 and L2 over the stick, R1 and R2 on the right).
        Context::Event { .. } | Context::Dialogue { .. } => {
            stick = scene.tested() & 0xF000 != 0 || matches!(ui, Context::Dialogue { choice: Some(_) });
            keys.extend([(Auto, Key(0)), (Select, Key(1)), (Look, Key(2))]);
            for (control, action, slot) in [
                (Menu, GameAction::Menu, Beside), (Cancel, GameAction::Cancel, Between), (Jump, GameAction::Jump, Above),
                (CameraRight, GameAction::CameraRight, Key(3)), (R2, GameAction::R2, Key(4)),
                (CameraLeft, GameAction::CameraLeft, Slot::Left(0)), (L2, GameAction::L2, Slot::Left(1)),
            ] {
                if tested(action) {
                    keys.push((control, slot));
                }
            }
        }
        Context::Menu { screen, quantities, .. } => {
            stick = true;
            pause = false;
            // L1 and R1: the list's pages, the character shown.
            keys.extend([(Cancel, Beside), (CameraRight, Key(0)), (CameraLeft, Key(1))]);
            match screen {
                // The field's and the world map's menu: Menu closes it.
                0 => keys.push((Menu, Key(2))),
                // The name entry: Start ends it.
                3 => keys.push((Start, Key(2))),
                _ => {}
            }
            if quantities {
                // Plus over the outer keys, minus beside it; ALL and NONE.
                keys.extend((0..QuantityKey::ALL.len() as u8).map(|k| (Quantity(k), Key(2 + k))));
            }
        }
        // The attack page: Fierce under the thumb (every Deathblow ends
        // with it), Strong and Weak about it as on the pad, END apart.
        Context::Battle { attack: true, .. } => {
            stick = true;
            keys.extend([(Menu, Beside), (Jump, Above), (Cancel, Key(0))]);
        }
        Context::Battle { .. } if advances(ui) => keys.push((Select, Key(0))),
        Context::Battle { .. } => {
            stick = true;
            keys.push((Cancel, Beside));
        }
        Context::WorldMap { scene, .. } if scene >= 8 => {}
        Context::WorldMap { movement, .. } => {
            stick = true;
            keys.extend([(CameraLeft, Beside), (CameraRight, Above), (Menu, Key(0)), (Select, Key(2))]);
            if movement >= 4 {
                // A vehicle: Cancel takes off and lands, Jump is the
                // throttle (latched).
                keys.extend([(Cancel, Key(1)), (Jump, Key(3))]);
            } else {
                keys.push((Board, Key(1)));
            }
        }
        Context::Arena { state: 1 } => {
            stick = true;
            keys.extend([(Menu, Beside), (CameraRight, Between), (Jump, Above), (Cancel, Key(0)), (CameraLeft, Key(1))]);
        }
        Context::Arena { .. } => {
            stick = true;
            keys.push((Cancel, Beside));
        }
    }
    if pause {
        keys.push((Start, Corner(1)));
    }
    (keys, stick && !inspecting)
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

    /// Whether a control standing in it is a round button (a square) rather
    /// than a key.
    pub fn round(&self) -> bool {
        (self.width - self.height).abs() < 0.5
    }

    /// Whether a finger at `at` is on the control standing in it: a key's
    /// rectangle; a round button's circle and a little beyond (less than
    /// half the gap to the next one).
    pub fn touched(&self, at: (f32, f32)) -> bool {
        if !self.round() {
            return self.contains(at);
        }
        let (cx, cy) = self.centre();
        (at.0 - cx).hypot(at.1 - cy) <= self.width / 2.0 * ROUND_REACH
    }
}

/// How far a round button takes a finger, in its radii.
const ROUND_REACH: f32 = 1.08;

/// The stick (target pixels): where the left thumb rests (the deck keeps
/// its well there clear), where a finger touching no control takes it, and
/// its knob.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Stick {
    /// The left thumb's rest, where a D-pad would stand: its well's centre
    /// were it held there.
    pub rest: (f32, f32),
    /// The screen's left half.
    pub zone: Rect,
    /// How far the knob's centre travels from the stick's centre.
    pub reach: f32,
    /// The knob's size (a button's).
    pub knob: f32,
    /// Whether the game reads the directions here: the stick shows as soon
    /// as a finger takes it. Elsewhere a finger still steers it (what it
    /// holds is there when the game reads them again), and it shows under
    /// that finger once it travels. With no finger on it, it never shows.
    pub reads: bool,
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

/// How the stick shows under the finger holding it (target pixels): its
/// centre and its knob's, and the D-pad's directions it holds (up, down,
/// left, right).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct StickShown {
    pub centre: (f32, f32),
    pub knob: (f32, f32),
    pub directions: [bool; 4],
}

/// The deck's measures, in units (a round button's size: about 13% of the
/// screen's short side, at least 52 dp): the primary button, the arc's
/// buttons (the one between the others a little larger), the gap between
/// neighbours, a key and its rows' pitch, a corner button and its margin.
/// Three rows of keys stand under the corner's buttons on a screen 6.9
/// units high, and the stick's well beside the cluster on one 7 wide.
const PRIMARY: f32 = 1.45;
const ARC: f32 = 1.0;
const BETWEEN: f32 = 1.1;
const GAP: f32 = 0.12;
const KEY: (f32, f32) = (1.26, 0.66);
const KEY_PITCH: f32 = 0.76;
/// How far the keys' lowest row stands above the arc's top: apart from the
/// buttons the thumb plays with.
const KEY_LIFT: f32 = 0.2;
const CORNER: f32 = 0.8;
const CORNER_EDGE: f32 = 0.3;
/// The arc: how far its buttons' centres stand from the primary's (the
/// larger one a gap off it), and where, in degrees up from the right: just
/// below level beside it, between, and just right of straight above it (a
/// thumb's sweep about its rest; the neighbours a gap apart).
const ARC_RADIUS: f32 = PRIMARY / 2.0 + BETWEEN / 2.0 + GAP;
const ARC_DEGREES: [f32; 3] = [187.0, 137.0, 87.0];
/// The primary's centre from the screen's right edge where the picture is
/// under it (a thumb's rest), its least where the letterbox holds the deck,
/// and its height over the lower edge; the stick's rest likewise.
const PRIMARY_INSET: f32 = 1.7;
const PRIMARY_EDGE: f32 = 0.25;
const PRIMARY_HEIGHT: f32 = 1.35;
const STICK_INSET: f32 = 1.95;
/// The least a unit is beside the picture, in dp: narrower letterboxes
/// leave the deck over the picture's corners at its full size.
const BESIDE_UNIT: f32 = 44.0;

/// The deck's controls and where they stand, and the stick.
///
/// Sizes follow the screen (`density` its pixels per dp); the right hand's
/// cluster stands in the letterbox beside the picture when it has room for
/// it (the primary button, the arc beside it and a margin either side: just
/// under three units), the unit no smaller than 44 dp for it, and else over
/// the picture's lower right corner, the primary where a thumb rests.
pub fn layout(target: (f32, f32), picture: Rect, density: f32, scene: Scene) -> (Vec<(Control, Rect)>, Stick) {
    let (w, h) = target;
    let short = w.min(h);
    let arc = ARC_DEGREES.map(|degrees| (degrees.to_radians().cos() * ARC_RADIUS, -degrees.to_radians().sin() * ARC_RADIUS));
    // The cluster across: from the Beside button's far edge to the
    // primary's outer one, with a quarter unit to the screen's edge and a
    // tenth to the picture.
    let across = -arc[0].0 + ARC / 2.0;
    let cluster = across + 0.1 + PRIMARY / 2.0 + PRIMARY_EDGE;
    // The unit: generous targets, yet three rows of keys under the corner's
    // buttons and the stick's well beside the cluster on a small screen.
    let mut unit = (short * 0.13).max(52.0 * density).min(short * 0.2).min(h / 6.9).min(w / 7.0);
    let side = picture.x.min(w - picture.x - picture.width).max(0.0);
    let beside = side / cluster >= BESIDE_UNIT * density;
    if beside {
        unit = unit.min(side / cluster);
    }
    let u = unit;
    let square = |cx: f32, cy: f32, size: f32| Rect { x: cx - size * u / 2.0, y: cy - size * u / 2.0, width: size * u, height: size * u };
    let key = |cx: f32, cy: f32| Rect { x: cx - KEY.0 * u / 2.0, y: cy - KEY.1 * u / 2.0, width: KEY.0 * u, height: KEY.1 * u };
    // The primary's centre: a thumb's rest over the picture, as far in as
    // the letterbox allows beside it.
    let inset = if beside { (side - (across + 0.1) * u).clamp((PRIMARY / 2.0 + PRIMARY_EDGE) * u, PRIMARY_INSET * u) } else { PRIMARY_INSET * u };
    let (px, py) = (w - inset, h - PRIMARY_HEIGHT * u);
    // The stick's rest: the cluster's mirror, in the middle of a letterbox.
    let stick_x = if beside { (side / 2.0).clamp((STICK_REACH + 0.6) * u, STICK_INSET * u) } else { STICK_INSET * u };
    let stick_y = h - STICK_INSET * u;
    // The keys' rows: above the arc's top, their outer edge the primary's.
    let rows = py + (arc[2].1 - ARC / 2.0 - KEY_LIFT - KEY.1 / 2.0) * u;
    let outer = px + (PRIMARY / 2.0 - KEY.0 / 2.0) * u;
    let (deck, reads) = keys(scene);
    let controls = deck.into_iter().map(|(control, slot)| (control, match slot {
        Slot::Primary => square(px, py, PRIMARY),
        Slot::Beside => square(px + arc[0].0 * u, py + arc[0].1 * u, ARC),
        Slot::Between => square(px + arc[1].0 * u, py + arc[1].1 * u, BETWEEN),
        Slot::Above => square(px + arc[2].0 * u, py + arc[2].1 * u, ARC),
        Slot::Key(n) => key(outer - f32::from(n % 2) * (KEY.0 + GAP) * u, rows - f32::from(n / 2) * KEY_PITCH * u),
        Slot::Left(n) => key(stick_x, stick_y - (STICK_REACH + 0.5 + GAP + KEY.1 / 2.0 + f32::from(n) * KEY_PITCH) * u),
        Slot::Corner(n) => square(w - (CORNER_EDGE + CORNER / 2.0 + f32::from(n) * (CORNER + GAP)) * u, (CORNER_EDGE + CORNER / 2.0) * u, CORNER),
    })).collect();
    let zone = Rect { x: 0.0, y: 0.0, width: w / 2.0, height: h };
    (controls, Stick { rest: (stick_x, stick_y), zone, reach: u * STICK_REACH, knob: u, reads })
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
    /// RUN was pressed: run by default goes on or off.
    Run,
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
    /// What the deck was last laid out for.
    scene: Scene,
    fingers: BTreeMap<u64, Finger>,
    /// The latching keys held ([`Control::latches`]).
    latched: BTreeSet<Control>,
    /// How far a lone finger that began on the picture moved, not yet
    /// taken (target pixels).
    drag: (f32, f32),
}

impl Deck {
    /// Lays the deck out for this frame: the target, where the picture is,
    /// and what the game plays ([`Scene`]). A control the new deck lacks
    /// lets go (a finger on it holds nothing more, a latched key is held no
    /// longer); one it keeps stays held, whatever it does now, as a pad's
    /// button would. First-person inspection's view has no stick: a finger
    /// holding it lets it go and drags as any other.
    pub fn arrange(&mut self, target: (f32, f32), picture: Rect, density: f32, scene: Scene) {
        let (controls, stick) = layout(target, picture, density, scene);
        let stands = |control: &Control| controls.iter().any(|(c, _)| c == control);
        for finger in self.fingers.values_mut() {
            finger.control = finger.control.filter(stands);
        }
        self.latched.retain(|control| control.latches(scene.ui) && stands(control));
        self.controls = controls;
        self.picture = Some(picture);
        self.stick = (!scene.inspecting).then_some(stick);
        if scene.inspecting {
            self.fingers.values_mut().for_each(|f| f.steer = None);
        }
        self.scene = scene;
    }

    /// What the deck was last laid out for.
    pub fn scene(&self) -> Scene {
        self.scene
    }

    fn control_at(&self, at: (f32, f32)) -> Option<Control> {
        self.controls.iter().find(|(_, r)| r.touched(at)).map(|(c, _)| *c)
    }

    /// A finger touched down: on a control it holds it (a latching key is
    /// held from this touch to its next); on none in the stick's half (and
    /// no other finger holding it), it takes the stick, centred there.
    pub fn down(&mut self, finger: u64, at: (f32, f32), time: f64) -> Touched {
        let control = self.control_at(at);
        let on_picture = control.is_none() && self.picture.is_some_and(|p| p.contains(at));
        let free = !self.fingers.values().any(|f| f.steer.is_some());
        let steer = (control.is_none() && free && self.stick.is_some_and(|s| s.zone.contains(at))).then_some(Steer { centre: at, sector: None });
        self.fingers.insert(finger, Finger { control, start: at, began: time, travelled: false, on_picture, at, steer });
        match control {
            Some(key) if key.latches(self.scene.ui) => {
                if !self.latched.remove(&key) {
                    self.latched.insert(key);
                }
                Touched::Nothing
            }
            Some(Control::Settings) => Touched::Settings,
            Some(Control::Auto) => Touched::AutoAdvance,
            Some(Control::Look) => Touched::FirstPerson,
            Some(Control::Run) => Touched::Run,
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

    /// Lets everything go, latched keys too (focus lost, the panel took the
    /// screen).
    pub fn release(&mut self) {
        self.fingers.clear();
        self.latched.clear();
        self.drag = (0.0, 0.0);
    }

    /// How far a finger alone on the picture (one that began there) moved
    /// since the last call, in target pixels across and down: first-person
    /// inspection's look (two fingers there drag nothing).
    pub fn take_drag(&mut self) -> (f32, f32) {
        std::mem::take(&mut self.drag)
    }

    /// The controls fingers are on (drawn pressed).
    pub fn pressed(&self) -> BTreeSet<Control> {
        self.fingers.values().filter_map(|f| f.control).collect()
    }

    /// The latching keys held (drawn lit).
    pub fn latched(&self) -> &BTreeSet<Control> {
        &self.latched
    }

    /// The controls held: those fingers are on (a latching key is held by
    /// its latch, not its finger), the latched keys, and the stick's
    /// directions.
    pub fn held(&self) -> BTreeSet<Control> {
        let ui = self.scene.ui;
        let mut held: BTreeSet<Control> = self.fingers.values().filter_map(|f| f.control).filter(|c| !c.latches(ui)).collect();
        held.extend(self.latched.iter().copied());
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

    /// How the stick shows: under the finger holding it, as soon as it
    /// touches where the game reads the directions ([`Stick::reads`]),
    /// elsewhere once it steers (not under a tap on the picture's left
    /// half); with no finger holding it, not at all.
    pub fn stick_shown(&self) -> Option<StickShown> {
        let stick = self.stick?;
        let (steer, at) = self.fingers.values().find_map(|f| f.steer.map(|s| (s, f.at)))?;
        (stick.reads || steer.sector.is_some()).then(|| {
            let (dx, dy) = (at.0 - steer.centre.0, at.1 - steer.centre.1);
            let k = stick.reach / (dx * dx + dy * dy).sqrt().max(stick.reach);
            StickShown { centre: steer.centre, knob: (steer.centre.0 + dx * k, steer.centre.1 + dy * k), directions: steer.directions() }
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use xem_core::context::Choice;

    /// A 20:9 phone in landscape with a 4:3 picture in the middle.
    const PHONE: (f32, f32) = (2400.0, 1080.0);
    const LETTERBOXED: Rect = Rect { x: 480.0, y: 0.0, width: 1440.0, height: 1080.0 };

    /// The screens the deck is laid out on (the target, the picture's place
    /// and the pixels a dp): that phone (the deck beside the picture); the
    /// same with the picture filling it (wider framing); a 16:9 phone,
    /// whose letterbox has no room for the deck; a 4:3 tablet; and a phone
    /// browser's canvas in portrait.
    const SCREENS: [((f32, f32), Rect, f32); 5] = [
        (PHONE, LETTERBOXED, 3.0),
        (PHONE, Rect { x: 0.0, y: 0.0, width: 2400.0, height: 1080.0 }, 3.0),
        ((1920.0, 1080.0), Rect { x: 240.0, y: 0.0, width: 1440.0, height: 1080.0 }, 3.0),
        ((2048.0, 1536.0), Rect { x: 0.0, y: 0.0, width: 2048.0, height: 1536.0 }, 2.0),
        ((1080.0, 2160.0), Rect { x: 0.0, y: 675.0, width: 1080.0, height: 810.0 }, 3.0),
    ];

    fn menu(screen: u8, quantities: bool) -> Context {
        Context::Menu { screen, cursor: 0, choice: 0, choices: 0, commands: true, open: false, switch: false, quantities }
    }

    fn battle(attack: bool, list: bool, message: bool) -> Context {
        Context::Battle { page: if attack { 5 } else { 1 }, attack, ap: 6, list, targeting: false, message }
    }

    const CHOICE: Context = Context::Dialogue { choice: Some(Choice { count: 2, index: 0 }) };
    const SCENE: Context = Context::Event { map: 4, buttons: 0 };
    const ON_FOOT: Context = Context::WorldMap { movement: 1, scene: 0 };
    const SHIP: Context = Context::WorldMap { movement: 7, scene: 0 };
    const BOUT: Context = Context::Arena { state: 1 };

    /// Every deck, by what the game plays.
    fn scenes() -> Vec<(&'static str, Scene)> {
        vec![
            ("booting", Context::None.into()),
            ("walking", Context::Walk.into()),
            ("walking where Select shows a picture", Scene { picture: true, ..Context::Walk.into() }),
            ("walking, inspecting", Scene { inspecting: true, ..Context::Walk.into() }),
            ("a scene", SCENE.into()),
            ("an activity of every button", Context::Event { map: 723, buttons: 0xf0ff }.into()),
            ("the card game", Context::Event { map: 321, buttons: 0xf1dc }.into()),
            ("a window open over the card game", Scene { tested: 0xf1dc, ..Context::Dialogue { choice: None }.into() }),
            ("walking where the scripts test every button", Scene { tested: 0xf1ff, ..Context::Walk.into() }),
            ("inspecting where the scripts test every button", Scene { tested: 0xf1ff, inspecting: true, ..Context::Walk.into() }),
            ("a dialogue", Context::Dialogue { choice: None }.into()),
            ("a choice", CHOICE.into()),
            ("the menu", menu(0, false).into()),
            ("the name entry", menu(3, false).into()),
            ("a shop's quantities", menu(4, true).into()),
            ("a battle's ring", battle(false, false, false).into()),
            ("a battle's attack page", battle(true, false, false).into()),
            ("a battle's list", battle(false, true, false).into()),
            ("a battle's message", battle(false, false, true).into()),
            ("the world map on foot", ON_FOOT.into()),
            ("the world map in a ship", SHIP.into()),
            ("the world map's scene", Context::WorldMap { movement: 1, scene: 9 }.into()),
            ("a bout", BOUT.into()),
            ("Battling's menu", Context::Arena { state: 0 }.into()),
        ]
    }

    fn deck_of(scene: Scene) -> Deck {
        let mut deck = Deck::default();
        deck.arrange(PHONE, LETTERBOXED, 3.0, scene);
        deck
    }

    /// The walking deck on the phone.
    fn deck() -> Deck {
        deck_of(Context::Walk.into())
    }

    fn at(deck: &Deck, control: Control) -> (f32, f32) {
        deck.controls.iter().find(|(c, _)| *c == control).unwrap_or_else(|| panic!("no {control:?} on the deck")).1.centre()
    }

    /// A deck's controls with their labels, in the deck's order.
    fn labels(scene: Scene) -> Vec<(Control, &'static str)> {
        keys(scene).0.into_iter().map(|(control, _)| (control, control.label(scene))).collect()
    }

    /// Whether no finger can be on both of two controls: two round buttons'
    /// reaches apart, else their rectangles (a round button's about its
    /// reach).
    fn apart(a: Rect, b: Rect) -> bool {
        if a.round() && b.round() {
            let ((ax, ay), (bx, by)) = (a.centre(), b.centre());
            return (ax - bx).hypot(ay - by) > (a.width + b.width) / 2.0 * ROUND_REACH;
        }
        let grown = |r: Rect| if r.round() {
            let more = r.width * (ROUND_REACH - 1.0) / 2.0;
            Rect { x: r.x - more, y: r.y - more, width: r.width + 2.0 * more, height: r.height + 2.0 * more }
        } else { r };
        let (a, b) = (grown(a), grown(b));
        a.x >= b.x + b.width || b.x >= a.x + a.width || a.y >= b.y + b.height || b.y >= a.y + a.height
    }

    /// The square the stick's well would fill under the left thumb's rest.
    fn well(stick: Stick) -> Rect {
        let r = stick.radius();
        Rect { x: stick.rest.0 - r, y: stick.rest.1 - r, width: 2.0 * r, height: 2.0 * r }
    }

    #[test]
    fn every_deck_has_the_buttons_its_context_reads_named_for_what_they_do() {
        use Control::*;
        let (settings, pause) = ((Settings, "gear"), (Start, "pause"));
        let turns = [(CameraLeft, "turn left"), (CameraRight, "turn right")];
        let expect = |what: &str, mut wanted: Vec<(Control, &'static str)>, stick: bool| {
            let scene = scenes().into_iter().find(|(name, _)| *name == what).unwrap_or_else(|| panic!("no scene {what}")).1;
            let mut deck = labels(scene);
            deck.sort();
            wanted.sort();
            assert_eq!(deck, wanted, "{what}");
            assert_eq!(keys(scene).1, stick, "{what}: whether the game reads the directions");
        };
        expect("booting", vec![settings, pause, (Confirm, "OK")], false);
        expect("walking", vec![settings, pause, (Confirm, "TALK"), (Jump, "JUMP"), turns[0], turns[1], (Menu, "MENU"), (Run, "RUN"), (Look, "LOOK")], true);
        expect("walking where Select shows a picture", vec![settings, pause, (Confirm, "TALK"), (Jump, "JUMP"), turns[0], turns[1], (Menu, "MENU"), (Run, "RUN"), (Look, "LOOK"), (Select, "MAP")], true);
        // Inspection holds the walk, the jump and the camera back.
        expect("walking, inspecting", vec![settings, pause, (Confirm, "TALK"), (Menu, "MENU"), (Look, "LOOK")], false);
        let scene = vec![settings, pause, (Confirm, "NEXT"), (Auto, "AUTO"), (Select, "HIDE"), (Look, "LOOK")];
        expect("a scene", scene.clone(), false);
        expect("a dialogue", scene.clone(), false);
        expect("a choice", vec![settings, pause, (Confirm, "OK"), (Auto, "AUTO"), (Select, "HIDE"), (Look, "LOOK")], true);
        // An activity: the buttons its scripts test, by their symbols
        // (Confirm, every deck's primary, with what it does under its own).
        expect("an activity of every button", vec![
            settings, pause, (Confirm, "cross:NEXT"), (Cancel, "circle"), (Menu, "square"), (Jump, "triangle"),
            (CameraLeft, "L1"), (CameraRight, "R1"), (L2, "L2"), (R2, "R2"), (Auto, "AUTO"), (Select, "HIDE"), (Look, "LOOK"),
        ], true);
        // Map 321's cards: Triangle or R1 plays to the right, Square or L1
        // to the left, Circle and Select; Cross is not read (NEXT stays for
        // the dialogue's pages).
        expect("the card game", vec![
            settings, pause, (Confirm, "NEXT"), (Cancel, "circle"), (Menu, "square"), (Jump, "triangle"),
            (CameraLeft, "L1"), (CameraRight, "R1"), (Auto, "AUTO"), (Select, "SELECT"), (Look, "LOOK"),
        ], true);
        // Its scripts go on testing the pad while one of them holds a
        // window open: a dialogue's deck there has the activity's buttons.
        expect("a window open over the card game", vec![
            settings, pause, (Confirm, "NEXT"), (Cancel, "circle"), (Menu, "square"), (Jump, "triangle"),
            (CameraLeft, "L1"), (CameraRight, "R1"), (Auto, "AUTO"), (Select, "SELECT"), (Look, "LOOK"),
        ], true);
        // And beside the player's own control: the walking deck keeps its
        // names and adds what it lacks (the run button held, Select, L2, R2).
        expect("walking where the scripts test every button", vec![
            settings, pause, (Confirm, "TALK"), (Jump, "JUMP"), turns[0], turns[1], (Menu, "MENU"), (Run, "RUN"), (Look, "LOOK"),
            (Select, "SELECT"), (Cancel, "circle"), (L2, "L2"), (R2, "R2"),
        ], true);
        expect("inspecting where the scripts test every button", vec![
            settings, pause, (Confirm, "TALK"), (Menu, "MENU"), (Look, "LOOK"), (Select, "SELECT"), (Cancel, "circle"), (L2, "L2"), (R2, "R2"),
        ], false);
        // A choice on such a map: Cross is its OK.
        assert_eq!(Control::Confirm.label(Scene { tested: 0x0020, ..CHOICE.into() }), "cross:OK");
        // Outside a field no script tests the pad.
        assert_eq!(Scene { tested: 0xffff, ..SHIP.into() }.tested(), 0);
        assert_eq!(labels(Scene { tested: 0xffff, ..SHIP.into() }), labels(SHIP.into()));
        // A menu reads no Start; its own closes with Menu, the name entry
        // ends with Start.
        let pages = [(CameraLeft, "<"), (CameraRight, ">")];
        expect("the menu", vec![settings, (Confirm, "OK"), (Cancel, "BACK"), pages[0], pages[1], (Menu, "CLOSE")], true);
        expect("the name entry", vec![settings, (Confirm, "OK"), (Cancel, "BACK"), pages[0], pages[1], (Start, "DONE")], true);
        expect("a shop's quantities", vec![settings, (Confirm, "OK"), (Cancel, "BACK"), pages[0], pages[1],
            (Quantity(0), "+"), (Quantity(1), "-"), (Quantity(2), "ALL"), (Quantity(3), "NONE")], true);
        let choosing = vec![settings, pause, (Confirm, "OK"), (Cancel, "BACK")];
        expect("a battle's ring", choosing.clone(), true);
        expect("a battle's list", choosing.clone(), true);
        expect("a battle's attack page", vec![settings, pause, (Confirm, "cross:FIERCE"), (Menu, "square:STRONG"), (Jump, "triangle:WEAK"), (Cancel, "END")], true);
        expect("a battle's message", vec![settings, pause, (Confirm, "NEXT"), (Select, "HIDE")], false);
        expect("the world map on foot", vec![settings, pause, (Confirm, "ENTER"), turns[0], turns[1], (Menu, "MENU"), (Select, "VIEW"), (Board, "BOARD")], true);
        expect("the world map in a ship", vec![settings, pause, (Confirm, "ENTER"), turns[0], turns[1], (Menu, "MENU"), (Select, "VIEW"), (Cancel, "LAND"), (Jump, "THROTTLE")], true);
        expect("the world map's scene", vec![settings, pause, (Confirm, "NEXT")], false);
        expect("a bout", vec![settings, pause, (Confirm, "A"), (Menu, "B"), (Jump, "JUMP"), (CameraRight, "SPECIAL"), (Cancel, "DASH"), (CameraLeft, "CHARGE")], true);
        expect("Battling's menu", choosing, true);
        // The attack page's buttons are the attacks' own.
        let buttons = |control: Control| xem_core::action::buttons(control.actions());
        assert_eq!(buttons(Jump), GameAction::AttackWeak.button());
        assert_eq!(buttons(Menu), GameAction::AttackStrong.button());
        assert_eq!(buttons(Confirm), GameAction::AttackFierce.button());
        assert_eq!(buttons(Cancel), GameAction::AttackEnd.button());
        // RUN, AUTO, LOOK, Settings and the quantities are the app's: they
        // hold none of the game's buttons.
        for control in [Run, Auto, Look, Settings, Quantity(0)] {
            assert!(control.actions().is_empty(), "{control:?}");
        }
        assert_eq!(Board.actions(), [GameAction::L2, GameAction::R2]);
    }

    #[test]
    fn every_deck_fits_every_screen_with_generous_targets_no_two_under_one_finger() {
        for (target, picture, density) in SCREENS {
            for (what, scene) in scenes() {
                let case = format!("{what} on {target:?}");
                let (controls, stick) = layout(target, picture, density, scene);
                let slots = keys(scene).0;
                assert_eq!(controls.len(), slots.len());
                for (&(control, rect), &(_, slot)) in controls.iter().zip(&slots) {
                    assert!(rect.x >= 0.0 && rect.y >= 0.0 && rect.x + rect.width <= target.0 && rect.y + rect.height <= target.1, "{case}: {control:?} {rect:?} within the screen");
                    // In dp: a unit is 44 at least (52 but beside a
                    // picture, or on a screen under 364 dp high or wide).
                    let (wide, high) = (rect.width / density, rect.height / density);
                    let least = match slot {
                        Slot::Primary => (63.0, 63.0),
                        Slot::Beside | Slot::Between | Slot::Above => (44.0, 44.0),
                        Slot::Key(_) | Slot::Left(_) => (55.0, 29.0),
                        Slot::Corner(_) => (35.0, 35.0),
                    };
                    assert!(wide >= least.0 && high >= least.1, "{case}: {control:?} is {wide} by {high} dp");
                    assert_eq!(rect.round(), !matches!(slot, Slot::Key(_) | Slot::Left(_)), "{case}: {control:?} round but a key");
                }
                for (i, &(a, ra)) in controls.iter().enumerate() {
                    for &(b, rb) in &controls[i + 1..] {
                        assert!(apart(ra, rb), "{case}: {a:?} {ra:?} and {b:?} {rb:?} under one finger");
                    }
                }
                // The stick's well at rest: within the screen's left half,
                // clear of every control.
                let well = well(stick);
                assert!(well.x >= 0.0 && well.x + well.width <= target.0 / 2.0 && well.y >= 0.0 && well.y + well.height <= target.1, "{case}: the stick's well {well:?}");
                for &(control, rect) in &controls {
                    assert!(apart(rect, well), "{case}: {control:?} {rect:?} over the stick's well {well:?}");
                }
                assert!(stick.dead_zone() > 0.0 && stick.dead_zone() < stick.reach);
            }
        }
    }

    #[test]
    fn the_deck_stands_beside_the_picture_when_the_letterbox_has_room_else_over_its_corner() {
        // The 20:9 phone: everything in the letterbox, at the full size.
        for (what, scene) in scenes() {
            let (controls, stick) = layout(PHONE, LETTERBOXED, 3.0, scene);
            for (control, rect) in controls {
                assert!(rect.x + rect.width <= LETTERBOXED.x || rect.x >= LETTERBOXED.x + LETTERBOXED.width, "{what}: {control:?} {rect:?} beside the picture");
            }
            let well = well(stick);
            assert!(well.x >= 0.0 && well.x + well.width <= LETTERBOXED.x, "{what}: the stick rests beside the picture: {well:?}");
            assert!((stick.knob - 52.0 * 3.0).abs() < 0.5, "a unit of 52 dp: {}", stick.knob);
        }
        // A 16:9 phone's letterbox is too narrow: over the picture's
        // corner, the primary where a thumb rests, the unit kept.
        let (target, picture, density) = SCREENS[2];
        let (controls, stick) = layout(target, picture, density, Context::Walk.into());
        let primary = controls.iter().find(|(c, _)| *c == Control::Confirm).unwrap().1;
        assert!(primary.x < picture.x + picture.width && (stick.knob - 156.0).abs() < 0.5, "{primary:?}");
        let from_edge = (target.0 - primary.centre().0, target.1 - primary.centre().1);
        assert!((from_edge.0 / stick.knob - PRIMARY_INSET).abs() < 0.01 && (from_edge.1 / stick.knob - PRIMARY_HEIGHT).abs() < 0.01, "{from_edge:?}");
        // A narrow window with a letterbox a deck of 44 dp units fits:
        // smaller units, still beside.
        let picture = Rect { x: 160.0, y: 0.0, width: 960.0, height: 720.0 };
        let (controls, stick) = layout((1280.0, 720.0), picture, 1.0, Context::Walk.into());
        assert!(stick.knob >= 44.0 && stick.knob < 60.0, "{}", stick.knob);
        assert!(controls.iter().all(|(_, r)| r.x + r.width <= picture.x || r.x >= picture.x + picture.width));
    }

    #[test]
    fn the_thumb_finds_confirm_and_the_keys_it_knows_where_they_were() {
        for (target, picture, density) in SCREENS {
            let place = |scene: Scene, control: Control| layout(target, picture, density, scene).0.into_iter().find(|(c, _)| *c == control).map(|(_, r)| r);
            let walking = Scene::from(Context::Walk);
            let confirm = place(walking, Control::Confirm).expect("TALK");
            let settings = place(walking, Control::Settings).expect("Settings");
            let pause = place(walking, Control::Start).expect("pause");
            for (what, scene) in scenes() {
                // Confirm is the primary, the largest, in every deck.
                assert_eq!(place(scene, Control::Confirm), Some(confirm), "{what}: Confirm where it was");
                assert_eq!(place(scene, Control::Settings), Some(settings), "{what}: Settings where it was");
                for (control, rect) in layout(target, picture, density, scene).0 {
                    assert!(control == Control::Confirm || rect.width < confirm.width || !rect.round(), "{what}: {control:?} as large as the primary");
                    if control == Control::Start && rect.round() {
                        assert_eq!(rect, pause, "{what}: the pause where it was");
                    }
                }
            }
            // Walking, talking and watching a scene: LOOK stays put, and
            // the camera's turns stand where they do on the world map.
            let look = place(walking, Control::Look);
            for ui in [SCENE, Context::Dialogue { choice: None }, CHOICE] {
                assert_eq!(place(ui.into(), Control::Look), look, "{ui:?}");
            }
            for control in [Control::CameraLeft, Control::CameraRight, Control::Menu] {
                assert_eq!(place(ON_FOOT.into(), control), place(walking, control), "{control:?} on the world map");
            }
            // The arc lies about the primary within a thumb's sweep: beside
            // it and above it, neither further than a unit and a half off.
            let (jump, left, right) = (place(walking, Control::Jump).unwrap(), place(walking, Control::CameraLeft).unwrap(), place(walking, Control::CameraRight).unwrap());
            let unit = right.width;
            for (button, rect) in [("JUMP", jump), ("the turn left", left), ("the turn right", right)] {
                let off = ((rect.centre().0 - confirm.centre().0).hypot(rect.centre().1 - confirm.centre().1)) / unit;
                assert!((off - ARC_RADIUS).abs() < 0.01, "{button} {off} units off the primary");
            }
            assert!(left.centre().0 < jump.centre().0 && jump.centre().0 < right.centre().0, "the turns either side of the jump");
            assert!(right.centre().1 < jump.centre().1 && jump.centre().1 < left.centre().1 && left.centre().1 > confirm.centre().1 - unit);
        }
    }

    #[test]
    fn a_round_button_takes_a_finger_on_its_circle_and_a_key_on_its_rectangle() {
        let deck = deck();
        let talk = deck.controls.iter().find(|(c, _)| *c == Control::Confirm).unwrap().1;
        let (cx, cy) = talk.centre();
        let r = talk.width / 2.0;
        assert_eq!(deck.control_at((cx + r * 0.7, cy - r * 0.7)), Some(Control::Confirm), "on its rim");
        assert_eq!(deck.control_at((cx + r * 1.05, cy)), Some(Control::Confirm), "a little beyond it");
        assert_eq!(deck.control_at((talk.x + talk.width - 2.0, talk.y + talk.height - 2.0)), None, "its square's corner is no button");
        assert_eq!(deck.control_at((cx - r * 1.12, cy)), None, "past its reach");
        let menu = deck.controls.iter().find(|(c, _)| *c == Control::Menu).unwrap().1;
        assert!(!menu.round());
        assert_eq!(deck.control_at((menu.x + 1.0, menu.y + 1.0)), Some(Control::Menu), "a key's corner");
        assert_eq!(deck.control_at((menu.x - 1.0, menu.y + 1.0)), None);
    }

    #[test]
    fn a_finger_on_the_left_half_steers_the_stick_from_where_it_touched_and_lets_go() {
        let mut deck = deck();
        let stick = deck.stick.unwrap();
        let (reach, dead) = (stick.reach, stick.dead_zone());
        assert!(dead > 0.0 && dead < reach);
        // Nothing shows before a finger touches. On the picture's left half,
        // nowhere near the left thumb's rest.
        assert!(deck.stick_shown().is_none(), "no stick with no finger on it");
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
        assert!(shown.directions == [false, false, true, false], "{shown:?}");
        deck.moved(1, (x - 2.0 * reach + dead * 1.5, y));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Right]));
        // Another finger holds a button meanwhile; a third on the left half
        // takes no second stick.
        let confirm = at(&deck, Control::Confirm);
        deck.down(2, confirm, 0.1);
        deck.down(3, (600.0, 700.0), 0.1);
        deck.moved(3, (600.0 + reach, 700.0));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Right, GameAction::Confirm]));
        assert_eq!(deck.held(), BTreeSet::from([Control::Right, Control::Confirm]));
        assert_eq!(deck.pressed(), BTreeSet::from([Control::Confirm]), "the button is drawn pressed");
        deck.up(2, confirm, 0.2, false);
        deck.up(3, (600.0 + reach, 700.0), 0.2, false);
        deck.up(1, (x, y), 0.3, true);
        assert!(deck.actions().is_empty(), "lifted and cancelled");
        assert_eq!(deck.stick_shown(), None, "gone once let go");
        // The right half takes no stick.
        deck.down(4, (1700.0, 300.0), 1.0);
        deck.moved(4, (1700.0 + reach, 300.0));
        assert!(deck.actions().is_empty() && deck.stick_shown().is_none());
    }

    #[test]
    fn the_stick_shows_only_under_a_finger_and_steers_everywhere() {
        // A dialogue's page reads none: a tap on the left half (which
        // advances the page) shows no stick.
        let dialogue = Scene::from(Context::Dialogue { choice: None });
        let mut deck = deck_of(dialogue);
        let stick = deck.stick.expect("a finger may still steer");
        assert!(!stick.reads && deck.stick_shown().is_none());
        deck.down(1, (700.0, 500.0), 0.0);
        assert!(deck.stick_shown().is_none(), "no stick under a tap");
        assert_eq!(deck.up(1, (701.0, 500.0), 0.1, false), Touched::Tap((701.0, 500.0)));
        // A finger that steers holds the direction, and the stick shows
        // under it: held through the dialogue, it walks at once after it.
        deck.down(2, (700.0, 500.0), 1.0);
        deck.moved(2, (700.0 + stick.reach, 500.0));
        let shown = deck.stick_shown().expect("the stick under the steering finger");
        assert!(shown.centre == (700.0, 500.0) && shown.directions == [false, false, false, true]);
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Right]));
        deck.arrange(PHONE, LETTERBOXED, 3.0, Context::Walk.into());
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Right]), "still held as the player walks again");
        assert!(deck.stick.unwrap().reads);
        deck.up(2, (700.0 + stick.reach, 500.0), 2.0, false);
        assert_eq!(deck.stick_shown(), None, "not at rest where the player walks");
        // Where the game reads them (walking, a choice, a menu, a battle) a
        // finger shows it as it touches; with none on it, no deck shows it.
        for (what, scene) in scenes() {
            let mut deck = deck_of(scene);
            assert!(deck.stick_shown().is_none(), "{what}: not at rest");
            assert_eq!(deck.stick.is_some(), !scene.inspecting, "{what}: no stick but under inspection");
            if deck.stick.is_some() {
                deck.down(1, (700.0, 500.0), 0.0);
                assert_eq!(deck.stick_shown().is_some(), keys(scene).1, "{what}: under a finger as it touches where the game reads the directions");
            }
        }
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
        let settings = at(&deck, Control::Settings);
        assert_eq!(deck.down(8, settings, 4.0), Touched::Settings);
        assert_eq!(deck.up(8, settings, 4.1, false), Touched::Nothing, "a key is no tap on the picture");
        // On the stick's half too: a still touch holds nothing and taps.
        deck.down(9, (700.0, 500.0), 5.0);
        assert!(deck.stick_shown().is_some() && deck.actions().is_empty());
        assert_eq!(deck.up(9, (703.0, 502.0), 5.2, false), Touched::Tap((703.0, 502.0)));
        // Over the picture's corner (no room beside it) a button is no tap.
        let (target, picture, density) = SCREENS[1];
        let mut deck = Deck::default();
        deck.arrange(target, picture, density, Context::Walk.into());
        let talk = at(&deck, Control::Confirm);
        assert!(picture.contains(talk));
        deck.down(1, talk, 0.0);
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Confirm]));
        assert_eq!(deck.up(1, talk, 0.1, false), Touched::Nothing);
    }

    #[test]
    fn a_bouts_dash_and_charge_and_the_ships_throttle_latch_and_let_go_with_their_deck() {
        use crate::draw::{Lit, deck_state};
        use xem_render::glass::State;
        let mut deck = deck_of(BOUT.into());
        let (dash, charge, a) = (at(&deck, Control::Cancel), at(&deck, Control::CameraLeft), at(&deck, Control::Confirm));
        assert!(Control::Cancel.latches(BOUT) && Control::CameraLeft.latches(BOUT) && !Control::Confirm.latches(BOUT) && !Control::Jump.latches(BOUT));
        // A touch holds DASH; lifted, it stays held, lit.
        assert_eq!(deck.down(1, dash, 0.0), Touched::Nothing);
        deck.up(1, dash, 0.1, false);
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Cancel, GameAction::Run]), "held with no finger on it");
        assert_eq!(deck.latched(), &BTreeSet::from([Control::Cancel]));
        assert_eq!(deck_state(Control::Cancel, false, true, Lit::default()), State::Active, "lit while latched");
        // The same thumb presses A meanwhile.
        deck.down(2, a, 0.2);
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Cancel, GameAction::Run, GameAction::Confirm]));
        deck.up(2, a, 0.3, false);
        // The next touch lets go at once, the finger still on it.
        deck.down(3, dash, 0.4);
        assert!(deck.actions().is_empty() && deck.pressed() == BTreeSet::from([Control::Cancel]), "let go as it is touched");
        deck.up(3, dash, 0.5, false);
        assert!(deck.actions().is_empty());
        // CHARGE latched as the bout ends: its deck gone, it lets go.
        deck.down(4, charge, 1.0);
        deck.up(4, charge, 1.1, false);
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::CameraLeft]));
        deck.arrange(PHONE, LETTERBOXED, 3.0, Context::Arena { state: 8 }.into());
        assert!(deck.actions().is_empty() && deck.latched().is_empty(), "the bout's keys let go with it");
        // The ship's THROTTLE latches; the panel taking the screen lets go.
        let mut deck = deck_of(SHIP.into());
        let throttle = at(&deck, Control::Jump);
        deck.down(1, throttle, 0.0);
        deck.up(1, throttle, 0.1, false);
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Jump]));
        deck.release();
        assert!(deck.actions().is_empty() && deck.latched().is_empty());
        // Walking, JUMP is held only while a finger is on it.
        let mut deck = deck_of(Context::Walk.into());
        let jump = at(&deck, Control::Jump);
        assert!(!Control::Jump.latches(Context::Walk));
        deck.down(1, jump, 0.0);
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Jump]));
        deck.up(1, jump, 0.1, false);
        assert!(deck.actions().is_empty() && deck.latched().is_empty());
    }

    #[test]
    fn a_control_the_next_deck_lacks_lets_go_and_one_it_keeps_stays_held() {
        let mut deck = deck();
        let (jump, talk) = (at(&deck, Control::Jump), at(&deck, Control::Confirm));
        deck.down(1, jump, 0.0);
        deck.down(2, talk, 0.0);
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Jump, GameAction::Confirm]));
        // A dialogue opens: its deck has no JUMP; TALK is its NEXT, held on
        // as a pad's button would be.
        deck.arrange(PHONE, LETTERBOXED, 3.0, Context::Dialogue { choice: None }.into());
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Confirm]));
        assert_eq!(deck.pressed(), BTreeSet::from([Control::Confirm]));
        // Walking again, the finger still where JUMP stands holds nothing
        // until it touches again.
        deck.arrange(PHONE, LETTERBOXED, 3.0, Context::Walk.into());
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Confirm]));
        deck.up(1, jump, 1.0, false);
        deck.up(2, talk, 1.0, false);
        assert!(deck.actions().is_empty());
    }

    #[test]
    fn run_auto_and_look_are_the_apps_toggles_lit_while_on() {
        use crate::draw::{Lit, deck_state};
        use xem_render::glass::State;
        let mut deck = deck();
        // RUN: run by default on or off, where the player walks.
        assert_eq!(deck.down(1, at(&deck, Control::Run), 0.0), Touched::Run);
        assert!(deck.actions().is_empty(), "the app's toggle presses nothing of the game's");
        deck.release();
        assert_eq!(deck.down(2, at(&deck, Control::Look), 0.0), Touched::FirstPerson);
        assert!(deck.actions().is_empty(), "not the game's Select");
        // AUTO: in a field's scenes and dialogue, where pages wait.
        for ui in [SCENE, Context::Dialogue { choice: None }, CHOICE] {
            let mut deck = deck_of(ui.into());
            assert_eq!(deck.down(1, at(&deck, Control::Auto), 0.0), Touched::AutoAdvance, "{ui:?}");
            assert!(deck.actions().is_empty());
            assert_eq!(deck.down(2, at(&deck, Control::Look), 0.0), Touched::FirstPerson, "{ui:?}");
        }
        for (what, scene) in scenes() {
            let has = |control: Control| keys(scene).0.iter().any(|(c, _)| *c == control);
            assert_eq!(has(Control::Auto), crate::overlay::auto_key(scene.ui), "{what}: AUTO");
            assert_eq!(has(Control::Look), crate::overlay::look_key(scene.ui), "{what}: LOOK");
            assert_eq!(has(Control::Run), scene.ui == Context::Walk && !scene.inspecting, "{what}: RUN");
        }
        let lit = Lit { auto_advance: true, first_person: false, run: true };
        assert_eq!(deck_state(Control::Auto, false, false, lit), State::Active);
        assert_eq!(deck_state(Control::Run, false, false, lit), State::Active);
        assert_eq!(deck_state(Control::Look, false, false, lit), State::Normal);
        assert_eq!(deck_state(Control::Look, false, false, Lit { first_person: true, ..lit }), State::Active, "lit while the view shows");
        assert_eq!(deck_state(Control::Look, true, false, Lit { first_person: true, ..lit }), State::Pressed);
        assert_eq!(deck_state(Control::Confirm, false, false, lit), State::Normal, "the toggles light their own keys only");
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
        let inspecting = Scene { inspecting: true, ..Context::Walk.into() };
        // A finger holding the stick as the view comes lets it go.
        deck.down(1, (700.0, 500.0), 0.0);
        deck.moved(1, (800.0, 500.0));
        assert_eq!(deck.actions(), BTreeSet::from([GameAction::Right]));
        deck.take_drag();
        deck.arrange(PHONE, LETTERBOXED, 3.0, inspecting);
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
        deck.arrange(PHONE, LETTERBOXED, 3.0, Context::Walk.into());
        deck.down(3, (600.0, 600.0), 2.0);
        deck.moved(3, (640.0, 600.0));
        assert_eq!((deck.take_drag(), deck.actions()), ((0.0, 0.0), BTreeSet::from([GameAction::Right])));
    }
}
