//! A headset's hands, gaze and head as the game's controls, shared by the
//! OpenXR and WebXR hosts: the chosen pointer source aims at the virtual
//! screen, gestures are recognised from the tracked joints and resolved in
//! the game's context ([`crate::dispatch`]), and the pause rules hold:
//!
//! - **Pause** (the overhand grab) toggles the player's pause in every
//!   context; head tracking, presentation and app input stay live.
//! - **Settings** (prayer hands, or the controller's menu button) add their
//!   own pause; closing them removes only that one.
//! - Losing the active hand's tracking, or the app's focus, releases held
//!   input, cancels what was pending and pauses; the player resumes with
//!   Pause, from a neutral hand (a hand must open before any grab fires).
//! - Inspection changes the view, never the pause. While it is on
//!   ([`Headset::inspecting`]) the character stays where it stands: walking,
//!   the pinch, the thumb's tap and its swipes up, left and right do
//!   nothing (the swipe down still opens the game's menu), the pointer's
//!   direction, JUMP and camera keys are gone, and two-hand scaling leaves
//!   the screen and the diorama as they were (the gesture still owns both
//!   hands).
//! - Two-hand scaling ([`Scaling`]) scales the diorama while it is the
//!   view, about its table's anchor, committed to the setting when the
//!   hands let go; otherwise the screen, for the session.
//!
//! The pointer source is the player's explicit choice (eye gaze where the
//! runtime grants it, the head's direction, or one hand's ray as the
//! runtime gives it); an unavailable source aims at nothing rather than
//! another source standing in, and the source in use when a gesture
//! begins stays until it ends.
//!
//! The pinch and the thumb's taps and swipes are the runtime's standard
//! gestures. Hands the runtime offers no microgestures for (WebXR has
//! none) play as a pointer: the pinch clicks where it aims (as a touch's
//! tap does) and a column of keys left of the screen presses what the
//! swipes would (directions, OK, BACK, MENU, JUMP, the camera's turns).
use crate::dispatch::{self, Aim, Out, Pointed, WorldRay};
use crate::gesture::{Frame, Gesture, Recognizer};
use glam::{Mat4, Quat, Vec3};
use serde::{Deserialize, Serialize};
use xem_core::context::Context;
use xem_core::field::{Field, V3};
use xem_core::menu::Targets;
use xem_render::ps1::DioramaSpace;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum PointerSource {
    /// Eye gaze, where the runtime offers it and the player permitted it.
    Eye,
    /// The head's forward direction.
    #[default]
    Head,
    LeftRay,
    RightRay,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Pose {
    pub position: Vec3,
    pub orientation: Quat,
}

impl Pose {
    pub fn forward(&self) -> Vec3 {
        self.orientation * Vec3::NEG_Z
    }
}

/// A flat quad in the room: its centre and orientation (the quad lies in
/// its local XY plane, facing +Z) and its size in metres.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Quad {
    pub pose: Pose,
    pub size: (f32, f32),
}

impl Quad {
    /// Where a ray meets the quad, as (u, v) across it from its top left,
    /// both 0..1, and the distance along the ray.
    pub fn hit(&self, origin: Vec3, direction: Vec3) -> Option<((f32, f32), f32)> {
        let inverse = self.pose.orientation.inverse();
        let o = inverse * (origin - self.pose.position);
        let d = inverse * direction;
        if d.z.abs() < 1e-6 {
            return None;
        }
        let t = -o.z / d.z;
        if t <= 0.0 {
            return None;
        }
        let p = o + d * t;
        let (u, v) = (p.x / self.size.0 + 0.5, 0.5 - p.y / self.size.1);
        ((0.0..1.0).contains(&u) && (0.0..1.0).contains(&v)).then_some(((u, v), t))
    }
}

/// The game's screen as a quad: the display pixels it spans (with the wider
/// framing's sides), and the scale of the keys beside it
/// ([`crate::draw::KEY_SIZE`] times it). While a diorama shows, the quad is
/// its interface's pane, and `diorama` where the table lies.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Screen {
    pub quad: Quad,
    pub display: (f32, f32),
    pub side: f32,
    pub keys: f32,
    pub diorama: Option<DioramaSpace>,
}

/// How near a ray through a diorama an actor counts as pointed at (metres
/// on the table, whatever its scale: a pointer's own steadiness).
pub const ACTOR_REACH_METRES: f32 = 0.04;

impl Screen {
    /// A flat screen (not a diorama's pane) with keys of scale 1 beside it.
    pub fn flat(quad: Quad, display: (f32, f32), side: f32) -> Screen {
        Screen { quad, display, side, keys: 1.0, diorama: None }
    }

    /// The pane of a diorama as `space` places it, the keys beside its
    /// edges as large as a screen's look from the seat (`seat`: the seat's
    /// place in the room).
    pub fn pane(space: DioramaSpace, seat: Vec3) -> Screen {
        let (_, orientation, position) = space.pane.to_scale_rotation_translation();
        let quad = Quad { pose: Pose { position, orientation }, size: (space.pane_size.x, space.pane_size.y) };
        let edge = position + orientation * Vec3::new(space.pane_size.x / 2.0, 0.0, 0.0);
        let keys = crate::draw::key_scale(edge.distance(seat));
        Screen { quad, display: (space.display.x, space.display.y), side: 0.0, keys, diorama: Some(space) }
    }

    /// The game screen pixel a ray meets.
    pub fn aim(&self, origin: Vec3, direction: Vec3) -> Option<(f32, f32)> {
        let ((u, v), _) = self.quad.hit(origin, direction)?;
        Some((u * (self.display.0 + 2.0 * self.side) - self.side, v * self.display.1))
    }

    /// What a ray in the room meets: the screen (or the diorama's pane),
    /// else the diorama's table, where the ray's part inside the table's
    /// volume becomes a ray in the game's world.
    pub fn pointed(&self, origin: Vec3, direction: Vec3) -> Option<Pointed> {
        if let Some(at) = self.aim(origin, direction) {
            return Some(if self.diorama.is_some() { Pointed::Pane(at) } else { Pointed::Screen(at) });
        }
        world_ray(&self.diorama?, origin, direction).map(Pointed::World)
    }

    /// The room point of a game screen pixel.
    pub fn point(&self, (x, y): (f32, f32)) -> Vec3 {
        let u = (x + self.side) / (self.display.0 + 2.0 * self.side) - 0.5;
        let v = 0.5 - y / self.display.1;
        self.quad.pose.position + self.quad.pose.orientation * Vec3::new(u * self.quad.size.0, v * self.quad.size.1, 0.0)
    }
}

/// A ray in the room through a diorama's table: its part inside the
/// table's volume (where the scene is drawn) as a ray in the game's world,
/// if it meets the volume ahead.
pub fn world_ray(space: &DioramaSpace, origin: Vec3, direction: Vec3) -> Option<WorldRay> {
    let table = space.table_from_room();
    let (o, d) = (table.transform_point3(origin), table.transform_vector3(direction));
    let (min, max) = space.volume;
    // The volume's slabs, axis by axis: where the ray is inside all three.
    let (mut near, mut far) = (0.0f32, f32::INFINITY);
    for axis in 0..3 {
        if d[axis].abs() < 1e-9 {
            if o[axis] < min[axis] || o[axis] > max[axis] {
                return None;
            }
            continue;
        }
        let (a, b) = ((min[axis] - o[axis]) / d[axis], (max[axis] - o[axis]) / d[axis]);
        near = near.max(a.min(b));
        far = far.min(a.max(b));
    }
    if !(far > near) {
        return None;
    }
    let world = space.world_from_room();
    let start = world.transform_point3(origin + direction * near);
    let end = world.transform_point3(origin + direction * far);
    let v = |p: Vec3| V3::new(p.x, p.y, p.z);
    Some(WorldRay { origin: v(start), direction: v(end - start), within: 1.0, reach: ACTOR_REACH_METRES / space.scale.max(f32::EPSILON) })
}

/// Where a seated player's view stands in the session's reference space:
/// the screen, the diorama's table, the settings panel and the keys stand in
/// front of it, and the head's turns are the head's from it (the Binaural
/// listener). The reference's own origin at first and whenever the runtime
/// moves the reference ([`reference_changed`]); the head's place, facing
/// the head's way about the vertical, when the player recentres.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Seat {
    pub pose: Mat4,
}

impl Default for Seat {
    fn default() -> Self {
        Seat { pose: Mat4::IDENTITY }
    }
}

impl Seat {
    /// The seat at the head: where it is, turned as it faces about the
    /// vertical (its pitch and roll left out).
    pub fn at(head: Pose) -> Seat {
        let forward = head.forward();
        let level = Vec3::new(forward.x, 0.0, forward.z);
        let yaw = if level.length_squared() > 1e-8 { (-level.x).atan2(-level.z) } else { 0.0 };
        Seat { pose: Mat4::from_rotation_translation(Quat::from_rotation_y(yaw), head.position) }
    }

    /// Its turn about the vertical.
    pub fn orientation(&self) -> Quat {
        self.pose.to_scale_rotation_translation().1
    }

    pub fn position(&self) -> Vec3 {
        self.pose.w_axis.truncate()
    }

    /// A pose given in front of the seat (its space), in the room.
    pub fn place(&self, local: Pose) -> Pose {
        Pose { position: self.pose.transform_point3(local.position), orientation: self.orientation() * local.orientation }
    }

    /// An orientation in the room as the seat sees it (a head's turn from it).
    pub fn relative(&self, orientation: Quat) -> Quat {
        self.orientation().inverse() * orientation
    }
}

/// What a moved reference or a recentring anchors again besides the seat:
/// the view's diorama, whose table then takes the camera's point and
/// heading at once, without easing from where it stood.
pub trait Anchored {
    fn anchor_again(&mut self);
}

impl Anchored for xem_render::ps1::GameView {
    fn anchor_again(&mut self) {
        self.recenter_diorama();
    }
}

/// The runtime moved the session's reference space (OpenXR's
/// `ReferenceSpaceChangePending`, WebXR's `reset`), the same on every host:
/// the seat is the reference's origin again (the runtime recentred it on the
/// head already), gestures in progress are cancelled (every hand opens again
/// before the next), a control deck's captured stick is let go, two-hand
/// scaling ends, and the view's diorama is anchored again ([`Anchored`]).
pub fn reference_changed(headset: &mut Headset, scaling: &mut Scaling, seat: &mut Seat, view: &mut impl Anchored) {
    headset.recognizer.cancel();
    headset.arcade.release();
    scaling.end();
    *seat = Seat::default();
    view.anchor_again();
}

/// The player's Recenter (an app action, the panel's button): the seat
/// moves to the head (`head`, where the session tracks it), and the view's
/// diorama is anchored again. Refused without a tracked head.
pub fn recenter(seat: &mut Seat, head: Option<Pose>, view: &mut impl Anchored) -> Result<String, String> {
    let head = head.ok_or("The head is not tracked: nothing to recenter on.")?;
    *seat = Seat::at(head);
    view.anchor_again();
    Ok("The view is in front of you again.".into())
}

/// What holds the settings panel's pointer down in a headset, by hand (0
/// the left, 1 the right): its standard pinch (OpenXR's `pinch_ext`,
/// WebXR's select of a hand source) or a controller's select (its trigger).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Presser {
    Pinch(usize),
    Select(usize),
}

/// What the settings panel's pointer does in a frame, at (u, v) across the
/// panel from its top left (both 0..1).
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum PanelEvent {
    Moved((f32, f32)),
    Pressed((f32, f32)),
    Released((f32, f32)),
    /// The pointer left the panel.
    Exited,
}

/// The settings panel's pointer in a headset (OpenXR's hands, WebXR's
/// hands and controllers): it follows where the pointer meets the panel; a
/// pinch or a controller's select beginning there presses it, and that
/// pinch's or select's end lets it go where it pressed. Between the two it
/// moves nowhere (no pinch-and-drag scrolling: plan.md Phase 5), so a pinch
/// or a trigger held on Hold to rewind holds it as a held mouse button does.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PanelPointer {
    /// On the panel at the last update.
    inside: bool,
    /// Held down: by what, and where.
    pressed: Option<(Presser, (f32, f32))>,
}

impl PanelPointer {
    /// One frame: `at`, where the pointer meets the panel (none off it);
    /// `began`, what began to press this frame; `held`, whether a presser
    /// holds now (one that began and ended within the frame does not: a
    /// click). The panel's events, in order.
    pub fn update(&mut self, at: Option<(f32, f32)>, began: Option<Presser>, held: impl Fn(Presser) -> bool) -> Vec<PanelEvent> {
        let mut events = Vec::new();
        if let Some((presser, uv)) = self.pressed {
            if held(presser) {
                return events;
            }
            events.push(PanelEvent::Released(uv));
            self.pressed = None;
        }
        match at {
            Some(uv) => {
                self.inside = true;
                events.push(PanelEvent::Moved(uv));
                if let Some(presser) = began {
                    events.push(PanelEvent::Pressed(uv));
                    if held(presser) {
                        self.pressed = Some((presser, uv));
                    } else {
                        events.push(PanelEvent::Released(uv));
                    }
                }
            }
            None if self.inside => {
                self.inside = false;
                events.push(PanelEvent::Exited);
            }
            None => {}
        }
        events
    }

    /// Lets go of a press at once (another source took the pointer, or the
    /// panel hid and lets go itself): where it pressed, if it held one.
    pub fn release(&mut self) -> Option<(f32, f32)> {
        self.pressed.take().map(|(_, uv)| uv)
    }
}

/// One headset frame's input.
#[derive(Clone, Debug, Default)]
pub struct Input {
    pub time: f64,
    pub head: Option<Pose>,
    /// Eye gaze, when granted and tracked.
    pub gaze: Option<Pose>,
    /// Each hand's ray (a controller's aim, or the runtime's hand ray).
    pub rays: [Option<Pose>; 2],
    /// Hand joints in WebXR's order (room space).
    pub hands: [Option<[Vec3; crate::gesture::JOINTS]>; 2],
    /// Each hand's standard pinch (OpenXR's `pinch_ext`, WebXR's select of
    /// a hand input source), where the runtime gives one.
    pub pinch: [Option<bool>; 2],
    pub micro: [Option<crate::gesture::Micro>; 2],
    pub micro_capable: [bool; 2],
    /// The app has the runtime's focus (OpenXR FOCUSED, WebXR visible).
    pub focused: bool,
}

/// A ray for each pointer source.
fn ray(source: PointerSource, input: &Input) -> Option<(Vec3, Vec3)> {
    let pose = match source {
        PointerSource::Eye => input.gaze?,
        PointerSource::Head => input.head?,
        PointerSource::LeftRay => input.rays[0]?,
        PointerSource::RightRay => input.rays[1]?,
    };
    Some((pose.position, pose.forward()))
}

/// The app's pauses: the game runs only when none holds.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize)]
pub struct Pauses {
    /// The player's Pause toggle (also set by losing tracking or focus).
    pub player: bool,
    /// Settings' own pause, while they are open.
    pub settings: bool,
}

impl Pauses {
    pub fn paused(&self) -> bool {
        self.player || self.settings
    }
}

/// What a frame asks of the host.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Effects {
    /// The game screen pixel the pointer aims at.
    pub aim: Aim,
    /// What the gestures resolved to, in order.
    pub outs: Vec<Out>,
    /// Actions a control deck holds now (its stick and held keys).
    pub held: std::collections::BTreeSet<xem_core::action::GameAction>,
    /// The gestures recognised (for feedback and tests).
    pub gestures: Vec<Gesture>,
}

#[derive(Debug, Default)]
pub struct Headset {
    pub source: PointerSource,
    recognizer: Recognizer,
    /// The control decks of Battling and the world map.
    pub arcade: crate::arcade::Arcade,
    /// The deck key the pointer is on.
    pub hovered_key: Option<u32>,
    /// The pointer key (left of the screen) it is on.
    pub hovered_pad: Option<u32>,
    /// The hands play as a pointer (no microgestures): its keys show.
    pub pointer: bool,
    pub pauses: Pauses,
    pub settings_open: bool,
    /// The host shows first-person inspection (hosts set it before each
    /// [`Headset::update`]).
    pub inspecting: bool,
    /// Auto-advance dialogue is on (hosts set it from the setting before
    /// each [`Headset::update`]): AUTO beside the screen shows lit.
    pub auto_advance: bool,
    was_focused: bool,
    tracked: [bool; 2],
}

impl Headset {
    /// What the chosen source aims at, if available: a pixel of the game's
    /// screen (or of a diorama's pane), or a ray through the diorama.
    pub fn aim(&self, input: &Input, screen: &Screen) -> Aim {
        let (origin, direction) = ray(self.source, input)?;
        screen.pointed(origin, direction)
    }

    /// Any room quad the chosen source meets (the settings panel), with
    /// (u, v) and distance.
    pub fn hit(&self, input: &Input, quad: &Quad) -> Option<((f32, f32), f32)> {
        let (origin, direction) = ray(self.source, input)?;
        quad.hit(origin, direction)
    }

    /// The keys beside the screen this frame: a control deck's (held ones
    /// lit), else a shop list's quantity keys or a battle's target list.
    pub fn key_marks(&self, ui: Context, targets: Option<&Targets>) -> Vec<crate::draw::KeyMark> {
        use xem_render::glass::State;
        let state = |i: usize, active: bool| if active { State::Active } else if self.hovered_key == Some(i as u32) { State::Hover } else { State::Normal };
        match crate::arcade::deck(ui) {
            Some(deck) => crate::arcade::keys(deck).iter().enumerate().map(|(i, key)| crate::draw::KeyMark {
                label: key.label.to_string(),
                state: state(i, key.hold && key.actions.iter().all(|a| self.arcade.toggled.contains(a))),
            }).collect(),
            None => crate::overlay::side_keys(ui, targets, self.auto_advance).into_iter().enumerate()
                .map(|(i, key)| crate::draw::KeyMark { label: key.label, state: state(i, key.lit) }).collect(),
        }
    }

    /// The pointer's keys left of the screen this frame (hands without
    /// microgestures, outside a control deck).
    pub fn pad_marks(&self, ui: Context) -> Vec<crate::draw::KeyMark> {
        use xem_render::glass::State;
        if !self.pointer || crate::arcade::deck(ui).is_some() || self.settings_open {
            return vec![];
        }
        self.pointer_keys(ui).iter().enumerate().map(|(i, (label, _))| crate::draw::KeyMark {
            label: label.to_string(),
            state: if self.hovered_pad == Some(i as u32) { State::Hover } else { State::Normal },
        }).collect()
    }

    /// The pointer's keys in `ui`: inspecting while the player walks, only
    /// those that leave the character where it stands (OK, BACK, MENU).
    fn pointer_keys(&self, ui: Context) -> Vec<(&'static str, xem_core::action::GameAction)> {
        use xem_core::action::GameAction;
        let stationary = self.inspecting && ui == Context::Walk;
        crate::overlay::POINTER_KEYS.iter().copied()
            .filter(|(_, action)| !stationary || matches!(action, GameAction::Confirm | GameAction::Cancel | GameAction::Menu))
            .collect()
    }

    /// Opens or closes settings (prayer hands, a menu button): their pause
    /// comes and goes with them; the player's own Pause stays as it was.
    pub fn set_settings(&mut self, open: bool) {
        self.settings_open = open;
        self.pauses.settings = open;
    }

    /// One frame: gestures recognised and resolved in the game's context
    /// (`field` and `targets` as read this frame). Gestures act on the game
    /// only while settings are closed; app gestures act always. A tap or a
    /// pinch aimed at a key beside the picture (a control deck's, a shop's
    /// quantity keys, a battle's target list) is that key's, and nothing
    /// else's.
    pub fn update(&mut self, input: &Input, screen: &Screen, ui: Context, field: Option<&Field>, targets: Option<&Targets>) -> Effects {
        let mut effects = Effects { aim: self.aim(input, screen), ..Effects::default() };
        // Focus lost: everything pending is dropped (two-hand scaling ends
        // where it reached), and the game pauses.
        if !input.focused {
            if self.was_focused {
                if self.recognizer.scaling() {
                    effects.outs.push(Out::ScaleEnd);
                }
                self.recognizer.cancel();
                self.pauses.player = true;
                effects.outs.push(Out::TrackingLost);
            }
            self.was_focused = false;
            return effects;
        }
        self.was_focused = true;
        let frame = Frame {
            time: input.time,
            hands: input.hands,
            view_right: input.head.map_or(Vec3::X, |h| h.orientation * Vec3::X),
            pinch: input.pinch,
            micro: input.micro,
            micro_capable: input.micro_capable,
        };
        self.pointer = (0..2).any(|i| input.hands[i].is_some()) && !(0..2).any(|i| input.hands[i].is_some() && input.micro_capable[i]);
        let gestures = self.recognizer.update(&frame);
        for (i, hand) in input.hands.iter().enumerate() {
            self.tracked[i] = hand.is_some();
        }
        // The game's own gestures go nowhere while settings are open.
        let mut game_gestures: Vec<Gesture> = gestures.iter().copied().filter(|g| !self.settings_open || matches!(g,
            Gesture::Pause { .. } | Gesture::Settings | Gesture::TrackingLost { .. } | Gesture::ScaleBegin | Gesture::Scale { .. } | Gesture::ScaleEnd)).collect();
        // A control deck (Battling, the world map) takes the game's
        // gestures and its keys beside the screen.
        let deck = crate::arcade::deck(ui).filter(|_| !self.settings_open);
        let side_keys = if deck.is_some() { vec![] } else { crate::overlay::side_keys(ui, targets, self.auto_advance) };
        let keys = crate::draw::side_keys(screen, deck.map_or(side_keys.len(), |deck| crate::arcade::keys(deck).len()));
        // A tap or pinch aimed at a deck key is the key's.
        let nearest = |keys: &[(u32, Quad)]| ray(self.source, input).and_then(|(origin, direction)| {
            keys.iter().filter_map(|(key, quad)| quad.hit(origin, direction).map(|(_, t)| (t, *key))).min_by(|a, b| a.0.total_cmp(&b.0)).map(|(_, key)| key)
        });
        let aimed_key = nearest(&keys);
        self.hovered_key = aimed_key;
        // The pointer's keys, outside a deck.
        let pad = self.pointer && deck.is_none() && !self.settings_open;
        let pointer_keys = self.pointer_keys(ui);
        let pad_keys = if pad { crate::draw::pad_keys(screen, pointer_keys.len()) } else { vec![] };
        let aimed_pad = nearest(&pad_keys);
        self.hovered_pad = aimed_pad;
        if let Some(key) = aimed_pad {
            game_gestures.retain(|g| {
                let pressed = matches!(g, Gesture::Pinch { .. });
                if pressed {
                    effects.outs.push(Out::Press { actions: vec![pointer_keys[key as usize].1] });
                }
                !pressed
            });
        }
        if let Some(key) = aimed_key.filter(|_| !self.settings_open) {
            game_gestures.retain(|g| {
                let pressed = matches!(g, Gesture::Tap { .. } | Gesture::Pinch { .. });
                if pressed {
                    match deck {
                        Some(deck) => {
                            let played = self.arcade.key(deck, key as usize);
                            effects.outs.extend(played.presses.into_iter().map(|actions| Out::Press { actions }));
                        }
                        None => effects.outs.extend(side_keys.get(key as usize).map(|k| k.out.clone())),
                    }
                }
                !pressed
            });
        }
        // Inspecting, the screen and the diorama keep their size (the
        // scaling gesture still owns both hands: no grab fires; one under
        // way as inspection began ends where it reached) and, while the
        // player walks, nothing walks, approaches, jumps or turns the
        // game's camera.
        if self.inspecting {
            game_gestures.retain(|g| !matches!(g, Gesture::ScaleBegin | Gesture::Scale { .. }));
            if ui == Context::Walk {
                use crate::gesture::Swipe;
                game_gestures.retain(|g| !matches!(g, Gesture::Pinch { .. } | Gesture::Tap { .. }
                    | Gesture::Swipe { direction: Swipe::Up | Swipe::Left | Swipe::Right, .. }));
            }
        }
        let palms = [0, 1].map(|i| input.hands[i].map(|j| crate::gesture::Shape::of(&j, crate::gesture::Hand::BOTH[i]).palm));
        let facing = input.head.map_or(Vec3::NEG_Z, |h| h.forward());
        let forward = Vec3::new(facing.x, 0.0, facing.z).normalize_or(Vec3::NEG_Z);
        let right = Vec3::new(-forward.z, 0.0, forward.x);
        let arcade_gestures: Vec<Gesture> = if deck.is_some() {
            let (deck_gestures, rest): (Vec<Gesture>, Vec<Gesture>) = game_gestures.iter().copied().partition(|g| matches!(g,
                Gesture::Tap { .. } | Gesture::Pinch { .. } | Gesture::Swipe { .. } | Gesture::TrackingLost { .. }));
            game_gestures = rest.into_iter().chain(deck_gestures.iter().copied().filter(|g| matches!(g, Gesture::TrackingLost { .. }))).collect();
            deck_gestures
        } else {
            vec![]
        };
        if let Some(played) = self.arcade.update(ui, &arcade_gestures, palms, right, forward) {
            effects.outs.extend(played.presses.into_iter().map(|actions| Out::Press { actions }));
            effects.held = played.held;
        } else {
            effects.held.clear();
        }
        // As a pointer, a pinch on the picture clicks there.
        let pointer = self.pointer;
        let resolved: Vec<Out> = game_gestures.iter().flat_map(|g| match g {
            Gesture::Pinch { .. } if pointer && deck.is_none() => effects.aim.and_then(|at| dispatch::tap_at(at, ui, field, targets)).into_iter().collect(),
            other => dispatch::dispatch(std::slice::from_ref(other), ui, effects.aim, field, targets),
        }).collect();
        for out in resolved {
            match out {
                Out::TogglePause => self.pauses.player = !self.pauses.player,
                Out::ToggleSettings => {
                    let open = !self.settings_open;
                    self.set_settings(open);
                }
                Out::TrackingLost => {
                    // Losing a hand that is in use (the active one): pause.
                    self.pauses.player = true;
                }
                _ => {}
            }
            effects.outs.push(out);
        }
        effects.gestures = gestures;
        effects
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use crate::synthetic::{THUMB_REST, right_hand};

    fn screen() -> Screen {
        Screen::flat(Quad { pose: Pose { position: Vec3::new(0.0, 0.0, -2.5), orientation: Quat::IDENTITY }, size: (3.2, 2.24) }, (320.0, 224.0), 0.0)
    }

    fn head() -> Pose {
        Pose { position: Vec3::ZERO, orientation: Quat::IDENTITY }
    }

    #[test]
    fn the_head_aims_at_the_screens_centre_and_rays_invert() {
        let screen = screen();
        let input = Input { head: Some(head()), focused: true, ..Input::default() };
        let headset = Headset::default();
        let Some(Pointed::Screen(aim)) = headset.aim(&input, &screen) else { panic!("the screen's centre") };
        assert!((aim.0 - 160.0).abs() < 0.01 && (aim.1 - 112.0).abs() < 0.01, "{aim:?}");
        let point = screen.point((40.0, 200.0));
        let again = screen.aim(Vec3::ZERO, point.normalize()).unwrap();
        assert!((again.0 - 40.0).abs() < 0.01 && (again.1 - 200.0).abs() < 0.01, "{again:?}");
    }

    #[test]
    fn a_pinch_or_select_held_on_the_panel_presses_where_it_began_and_lets_go_there_moving_nowhere_between() {
        use PanelEvent::*;
        let mut pointer = PanelPointer::default();
        let pinch = Presser::Pinch(1);
        assert_eq!(pointer.update(Some((0.5, 0.4)), None, |_| false), [Moved((0.5, 0.4))], "hovering");
        assert_eq!(pointer.update(Some((0.5, 0.5)), Some(pinch), |p| p == pinch), [Moved((0.5, 0.5)), Pressed((0.5, 0.5))]);
        // Held, the pointer moving over the panel or leaving it sends
        // nothing (a drag would pan a narrow panel's form), and another
        // hand's pinch presses nothing.
        assert_eq!(pointer.update(Some((0.5, 0.8)), None, |p| p == pinch), []);
        assert_eq!(pointer.update(None, Some(Presser::Pinch(0)), |p| p == pinch || p == Presser::Pinch(0)), []);
        // Its end lets go where it pressed, then the pointer goes on.
        assert_eq!(pointer.update(None, None, |_| false), [Released((0.5, 0.5)), Exited]);
        // A select that begins and ends within a frame is a click.
        let select = Presser::Select(0);
        assert_eq!(pointer.update(Some((0.2, 0.3)), Some(select), |_| false), [Moved((0.2, 0.3)), Pressed((0.2, 0.3)), Released((0.2, 0.3))]);
        // Held, a select is the select's to end, not a pinch's.
        assert_eq!(pointer.update(Some((0.2, 0.3)), Some(select), |p| p == select), [Moved((0.2, 0.3)), Pressed((0.2, 0.3))]);
        assert_eq!(pointer.update(Some((0.7, 0.7)), None, |p| p == select), []);
        assert_eq!(pointer.release(), Some((0.2, 0.3)), "another source taking the pointer lets go where it pressed");
        assert_eq!(pointer.release(), None);
        assert_eq!(pointer.update(Some((0.7, 0.7)), None, |p| p == select), [Moved((0.7, 0.7))], "and the pointer follows again");
    }

    #[test]
    fn an_unavailable_source_aims_at_nothing_rather_than_another() {
        let input = Input { head: Some(head()), focused: true, ..Input::default() };
        let headset = Headset { source: PointerSource::Eye, ..Headset::default() };
        assert_eq!(headset.aim(&input, &screen()), None, "no gaze: no aim, not the head's");
        // A hand's ray is the runtime's only: joints alone give none.
        let hand = right_hand([0.2; 4], THUMB_REST).map(|j| j + Vec3::new(0.2, -0.2, -0.3));
        let input = Input { hands: [None, Some(hand)], ..input };
        let headset = Headset { source: PointerSource::RightRay, ..Headset::default() };
        assert_eq!(headset.aim(&input, &screen()), None);
        let ray = Pose { position: Vec3::new(0.2, -0.2, -0.3), orientation: Quat::IDENTITY };
        assert!(headset.aim(&Input { rays: [None, Some(ray)], ..input }, &screen()).is_some());
    }

    #[test]
    fn hands_without_microgestures_play_as_a_pointer_with_keys() {
        use crate::synthetic::{Runtime, Step, gesture, in_front};
        let mut headset = Headset::default();
        let play = |headset: &mut Headset, steps: &[Step], aim: (f32, f32), runtime: Runtime, ui: Context| -> Vec<Out> {
            let mut outs = Vec::new();
            for (i, step) in steps.iter().enumerate() {
                let target = screen().point(aim);
                let head = Pose { position: Vec3::ZERO, orientation: Quat::from_rotation_arc(Vec3::NEG_Z, target.normalize()) };
                let hands = in_front(step.hands, Vec3::ZERO, Quat::IDENTITY);
                let frame = step.frame(i as f64 / 72.0, hands, Vec3::X, runtime);
                let input = Input { time: frame.time, head: Some(head), hands, pinch: frame.pinch, micro: frame.micro, micro_capable: frame.micro_capable, focused: true, ..Input::default() };
                outs.extend(headset.update(&input, &screen(), ui, None, None).outs);
            }
            outs
        };
        let pinch = gesture("pinch").unwrap();
        // In dialogue a pinch clicks: it advances.
        let outs = play(&mut headset, &pinch, (160.0, 112.0), Runtime::WEBXR, Context::Dialogue { choice: None });
        assert!(headset.pointer);
        assert_eq!(outs, [Out::Press { actions: vec![xem_core::action::GameAction::Confirm] }]);
        // A pinch on the BACK key presses Cancel.
        let back = crate::overlay::POINTER_KEYS.iter().position(|k| k.0 == "BACK").unwrap();
        let quad = crate::draw::pad_keys(&screen(), crate::overlay::POINTER_KEYS.len())[back].1;
        let local = quad.pose.position - screen().quad.pose.position;
        let at = ((local.x / 3.2 + 0.5) * 320.0, (0.5 - local.y / 2.24) * 224.0);
        let outs = play(&mut headset, &pinch, at, Runtime::WEBXR, Context::Battle { page: 2, attack: false, ap: 6, list: true, targeting: false });
        assert_eq!(outs, [Out::Press { actions: vec![xem_core::action::GameAction::Cancel] }]);
        assert_eq!(headset.pad_marks(Context::Walk).len(), crate::overlay::POINTER_KEYS.len());
        // With microgestures the thumb plays and no keys show.
        let mut headset = Headset::default();
        let outs = play(&mut headset, &gesture("tap").unwrap(), (160.0, 112.0), Runtime::HORIZON, Context::Dialogue { choice: None });
        assert!(!headset.pointer && headset.pad_marks(Context::Walk).is_empty());
        assert_eq!(outs, [Out::Press { actions: vec![xem_core::action::GameAction::Confirm] }]);
    }

    #[test]
    fn auto_beside_the_screen_toggles_auto_advance_and_never_advances_the_page() {
        use crate::synthetic::{Runtime, gesture, in_front};
        let ui = Context::Dialogue { choice: None };
        let quad = crate::draw::side_keys(&screen(), 1)[0].1;
        let local = quad.pose.position - screen().quad.pose.position;
        let at = ((local.x / 3.2 + 0.5) * 320.0, (0.5 - local.y / 2.24) * 224.0);
        for runtime in [Runtime::HORIZON, Runtime::WEBXR] {
            let mut headset = Headset::default();
            let mut outs = Vec::new();
            let steps = gesture(if runtime.microgestures { "tap" } else { "pinch" }).unwrap();
            for (i, step) in steps.iter().enumerate() {
                let head = Pose { position: Vec3::ZERO, orientation: Quat::from_rotation_arc(Vec3::NEG_Z, screen().point(at).normalize()) };
                let hands = in_front(step.hands, Vec3::ZERO, Quat::IDENTITY);
                let frame = step.frame(i as f64 / 72.0, hands, Vec3::X, runtime);
                let input = Input { time: frame.time, head: Some(head), hands, pinch: frame.pinch, micro: frame.micro, micro_capable: frame.micro_capable, focused: true, ..Input::default() };
                outs.extend(headset.update(&input, &screen(), ui, None, None).outs);
            }
            assert_eq!(outs, [Out::ToggleAutoAdvance], "AUTO's, not the page's ({runtime:?})");
            let mark = |headset: &Headset| headset.key_marks(ui, None)[0].clone();
            assert_eq!(mark(&headset), crate::draw::KeyMark { label: "AUTO".into(), state: xem_render::glass::State::Hover });
            headset.auto_advance = true;
            assert_eq!(mark(&headset).state, xem_render::glass::State::Active, "lit while on");
        }
        assert!(Headset::default().key_marks(Context::Walk, None).is_empty(), "walking shows no AUTO");
    }

    #[test]
    fn settings_pause_apart_from_the_players_and_focus_loss_pauses() {
        let mut headset = Headset::default();
        let input = Input { head: Some(head()), focused: true, ..Input::default() };
        headset.update(&input, &screen(), Context::Walk, None, None);
        headset.pauses.player = true;
        headset.set_settings(true);
        assert!(headset.pauses.paused());
        headset.set_settings(false);
        assert!(headset.pauses.player, "closing settings keeps the player's Pause");
        headset.pauses.player = false;
        let lost = headset.update(&Input { focused: false, ..input.clone() }, &screen(), Context::Walk, None, None);
        assert_eq!(lost.outs, [Out::TrackingLost]);
        assert!(headset.pauses.player, "paused until the player resumes");
        assert!(headset.update(&input, &screen(), Context::Walk, None, None).outs.is_empty(), "regaining focus resumes nothing");
        assert!(headset.pauses.player);
    }

    #[test]
    fn after_focus_loss_one_hands_pause_resumes_whatever_the_other_hand_does() {
        use crate::synthetic::{Runtime, Step, fist, half, in_front, left, open, place};
        let mut headset = Headset::default();
        let mut time = 0.0;
        let mut step = |headset: &mut Headset, step: Step, focused: bool| {
            let hands = in_front(step.hands, Vec3::ZERO, Quat::IDENTITY);
            let signals = step.frame(time, hands, Vec3::X, Runtime::HORIZON);
            time += 1.0 / 72.0;
            let input = Input { time, head: Some(head()), hands, pinch: signals.pinch, micro: signals.micro, micro_capable: signals.micro_capable, focused, ..Input::default() };
            headset.update(&input, &screen(), Context::Walk, None, None).outs
        };
        let at = |x: f32, hand| place(hand, Quat::IDENTITY, Vec3::new(x, 0.0, 0.0));
        let both = |l, r| Step::of([Some(at(-0.12, left(l))), Some(at(0.12, r))]);
        step(&mut headset, both(open(), open()), true);
        assert_eq!(step(&mut headset, both(half(), open()), false), [Out::TrackingLost]);
        assert!(headset.pauses.player, "the focus lost pauses");
        // The focus back, the left hand stays half curled: the right hand's
        // overhand grab from open resumes.
        let outs: Vec<Out> = [open(), half(), fist()].into_iter().flat_map(|r| step(&mut headset, both(half(), r), true)).collect();
        assert_eq!(outs, [Out::TogglePause]);
        assert!(!headset.pauses.player, "resumed");
    }

    /// A field whose one other actor stands ahead of the player, at the
    /// screen's centre.
    fn field() -> Field {
        let actor = |index: usize, z: f32| xem_core::field::Actor {
            index, position: xem_core::field::V3::new(0.0, 0.0, z), layer: 0, triangle: [0; 4], flags: 0, layer_flags: 0,
            radius: 20, height: 0, heading: 0, heading_goal: 0,
        };
        Field {
            camera: xem_core::field::Camera { m: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]], t: [0.0; 3], h: 256.0, angle: 0 },
            layers: vec![], attributes: vec![], actors: vec![Some(actor(0, 10.0)), Some(actor(1, 500.0))], player: Some(0),
            alt_dpad: false, party_processing: false, dialogue_open: false,
        }
    }

    /// Carries nothing out (the outs are what is checked).
    struct Ignored;

    impl Carry for Ignored {
        fn command(&mut self, _: serde_json::Value) {}
        fn press(&mut self, _: Vec<xem_core::action::GameAction>) {}
        fn select(&mut self, _: usize, _: bool) {}
        fn inspect(&mut self) {}
        fn release(&mut self) {}
        fn toggle_auto_advance(&mut self) {}
    }

    #[test]
    fn inspecting_keeps_the_character_where_it_stands_and_the_screen_its_size() {
        use crate::synthetic::{Runtime, Step, fist, gesture, in_front, left, open, place, whole_pinch};
        use xem_core::action::GameAction;
        let field = field();
        let mut time = 0.0;
        let mut play = |headset: &mut Headset, steps: &[Step], aim: (f32, f32), runtime: Runtime, ui: Context| -> Vec<Out> {
            let mut outs = Vec::new();
            for step in steps {
                let head = Pose { position: Vec3::ZERO, orientation: Quat::from_rotation_arc(Vec3::NEG_Z, screen().point(aim).normalize()) };
                let hands = in_front(step.hands, Vec3::ZERO, Quat::IDENTITY);
                let frame = step.frame(time, hands, Vec3::X, runtime);
                time += 1.0 / 72.0;
                let input = Input { time: frame.time, head: Some(head), hands, pinch: frame.pinch, micro: frame.micro, micro_capable: frame.micro_capable, focused: true, ..Input::default() };
                outs.extend(headset.update(&input, &screen(), ui, Some(&field), None).outs);
            }
            outs
        };
        let g = |name: &str| gesture(name).unwrap();
        let npc = field.camera.project(field.actors[1].unwrap().position).unwrap();
        let press = |action| Out::Press { actions: vec![action] };
        // With the thumb's microgestures: a tap talks to the actor gazed at,
        // a swipe up jumps.
        let mut headset = Headset::default();
        assert_eq!(play(&mut headset, &g("tap"), npc, Runtime::HORIZON, Context::Walk), [Out::Command { command: serde_json::json!({"cmd": "interact", "actor": 1}) }]);
        assert_eq!(play(&mut headset, &g("swipe-up"), npc, Runtime::HORIZON, Context::Walk), [press(GameAction::Jump)]);
        // Inspecting, nothing walks, approaches, jumps or turns the camera;
        // the menu, Pause and inspection's own grab stay.
        headset.inspecting = true;
        for name in ["tap", "pinch", "swipe-up", "swipe-left", "swipe-right"] {
            let outs = play(&mut headset, &g(name), npc, Runtime::HORIZON, Context::Walk);
            assert!(outs.is_empty(), "{name} while inspecting: {outs:?}");
        }
        assert_eq!(play(&mut headset, &g("swipe-down"), npc, Runtime::HORIZON, Context::Walk),
            [Out::Command { command: serde_json::json!({"cmd": "assist_stop"}) }, press(GameAction::Menu)]);
        assert_eq!(play(&mut headset, &g("inspect"), npc, Runtime::HORIZON, Context::Walk), [Out::ToggleInspect]);
        assert_eq!(play(&mut headset, &g("pause"), npc, Runtime::HORIZON, Context::Walk), [Out::TogglePause]);
        headset.pauses.player = false;
        assert_eq!(play(&mut headset, &g("tap"), npc, Runtime::HORIZON, Context::Dialogue { choice: None }), [press(GameAction::Confirm)], "a dialogue still advances");
        // As a pointer: a pinch on the actor approaches nothing, and only the
        // keys that leave the character standing show and press.
        let mut pointer = Headset { inspecting: true, ..Headset::default() };
        assert!(play(&mut pointer, &g("pinch"), npc, Runtime::WEBXR, Context::Walk).is_empty());
        let labels: Vec<String> = pointer.pad_marks(Context::Walk).into_iter().map(|key| key.label).collect();
        assert_eq!(labels, ["OK", "BACK", "MENU"]);
        let back = crate::draw::pad_keys(&screen(), labels.len())[1].1.pose.position - screen().quad.pose.position;
        let on_back = ((back.x / 3.2 + 0.5) * 320.0, (0.5 - back.y / 2.24) * 224.0);
        assert_eq!(play(&mut pointer, &g("pinch"), on_back, Runtime::WEBXR, Context::Walk), [press(GameAction::Cancel)]);
        assert_eq!(pointer.pad_marks(Context::Dialogue { choice: None }).len(), crate::overlay::POINTER_KEYS.len(), "only walking loses keys");
        // Two whole-hand pinches spread apart: scaling owns both hands (no
        // grab fires when one closes) and, inspecting, leaves the screen.
        let at = |x: f32, hand| place(hand, Quat::IDENTITY, Vec3::new(x, 0.0, 0.0));
        let spread: Vec<Step> = [
            [Some(left(at(0.1, open()))), Some(at(0.1, open()))],
            [Some(at(-0.1, left(whole_pinch()))), Some(at(0.1, whole_pinch()))],
            [Some(at(-0.2, left(whole_pinch()))), Some(at(0.2, whole_pinch()))],
            [Some(at(-0.2, left(fist()))), Some(at(0.2, whole_pinch()))],
            [Some(at(-0.2, left(fist()))), Some(at(0.2, fist()))],
        ].map(Step::of).to_vec();
        for (inspecting, scaled) in [(false, true), (true, false)] {
            let mut headset = Headset { inspecting, ..Headset::default() };
            let mut scaling = Scaling::default();
            let outs = play(&mut headset, &spread, npc, Runtime::HORIZON, Context::Walk);
            assert!(!outs.contains(&Out::TogglePause), "{outs:?}");
            carry(outs, &mut Ignored, &mut scaling);
            assert_eq!(scaling.scale != 1.0, scaled, "inspecting={inspecting}: scale {}", scaling.scale);
        }
    }

    /// A level field camera at the world's origin looking along +z at the
    /// floor point (0, 0, 1200) (world y down).
    pub(crate) fn level_camera() -> xem_render::xem_gpu::Camera {
        xem_render::xem_gpu::Camera {
            axes: [[4096.0, 0.0, 0.0], [0.0, 4096.0, 0.0], [0.0, 0.0, 4096.0]],
            translation: [0.0; 3],
            target: [0.0, 0.0, 1200.0],
            focus: 1200.0,
            mode: xem_render::xem_gpu::CameraMode::Field,
            stage: None,
            ground: None,
            origin: [0.0; 3],
            around: xem_render::xem_gpu::Reveals::ANYWHERE,
        }
    }

    /// The diorama of [`level_camera`]'s frame on `table` from `seat`.
    pub(crate) fn diorama(table: &xem_render::ps1::Table, seat: Mat4) -> DioramaSpace {
        DioramaSpace::first(&level_camera(), 256.0, glam::Vec2::new(320.0, 224.0), table, seat).expect("a camera that spans space")
    }

    #[test]
    fn a_ray_through_the_table_becomes_the_worlds_and_one_at_the_pane_its_pixel() {
        let table = xem_render::ps1::Table::default();
        let space = diorama(&table, Mat4::IDENTITY);
        let screen = Screen::pane(space, Vec3::ZERO);
        let eye = Vec3::ZERO;
        // The floor point (100, 0, 1300) as the table shows it: a ray at it
        // reaches it in the world, within the ray's part inside the volume.
        let goal = Vec3::new(100.0, 0.0, 1300.0);
        let target = space.room_from_world.transform_point3(goal);
        let Some(Pointed::World(ray)) = screen.pointed(eye, (target - eye).normalize()) else { panic!("through the table") };
        let t = -ray.origin.y / ray.direction.y;
        let at = Vec3::new(ray.origin.x + ray.direction.x * t, 0.0, ray.origin.z + ray.direction.z * t);
        assert!(at.distance(goal) < 0.5 && (0.0..=ray.within).contains(&t), "{ray:?} meets the floor at {at} (t {t})");
        assert!((ray.reach - ACTOR_REACH_METRES / space.scale).abs() < 1e-3, "four centimetres on the table: {} world units", ray.reach);
        // The pane's centre is its picture's centre (a pane's pixel, which
        // shows no floor), and a ray above it all meets nothing.
        let middle = space.pane.transform_point3(Vec3::ZERO);
        let Some(Pointed::Pane(pixel)) = screen.pointed(eye, (middle - eye).normalize()) else { panic!("on the pane") };
        assert!((pixel.0 - 160.0).abs() < 0.01 && (pixel.1 - 112.0).abs() < 0.01, "{pixel:?}");
        assert_eq!(screen.pointed(eye, Vec3::Y), None);
        assert_eq!(Screen { diorama: None, ..screen }.pointed(eye, (target - eye).normalize()), None, "no table, no world");
    }

    #[test]
    fn through_the_diorama_the_listener_hears_the_table_where_it_lies_from_the_head() {
        // The Binaural presenter hears a direction of the game's world
        // through the camera the listener's axes are in (the one the frame
        // shown was drawn with), those axes, then the head's (each applied as
        // the axes' components, xem_core::binaural).
        let apply = |axes: [[f32; 3]; 3], v: Vec3| Vec3::new(v.dot(Vec3::from(axes[0])), v.dot(Vec3::from(axes[1])), v.dot(Vec3::from(axes[2])));
        let seat_pose = Pose { position: Vec3::new(0.1, 0.0, -0.05), orientation: Quat::from_rotation_y(0.4) };
        let seat = Seat::at(seat_pose);
        // A camera turned 63 degrees and pitched 23 (`DioramaSpace::first`
        // levels the table under it).
        let turned = glam::Mat3::from_rotation_x(0.4) * glam::Mat3::from_rotation_y(-1.1);
        let camera = xem_render::xem_gpu::Camera { axes: (turned * 4096.0).to_cols_array_2d(), ..level_camera() };
        for tilt in [0.0f32, 30.0] {
            let table = xem_render::ps1::Table { tilt: tilt.to_radians(), ..xem_render::ps1::Table::default() };
            let space = DioramaSpace::first(&camera, 256.0, glam::Vec2::new(320.0, 224.0), &table, seat.pose).unwrap();
            let (listener, rows) = space.listener_axes().unwrap();
            let head = Quat::from_rotation_y(0.9) * Quat::from_rotation_x(-0.3);
            let heard = xem_core::binaural::head_axes(seat.relative(head).to_array());
            for world in [Vec3::new(1.0, 0.0, 0.0), Vec3::new(0.0, 0.0, 1.0), Vec3::new(-0.6, -0.2, 0.5).normalize()] {
                // As heard: the camera's view of the direction, through the
                // listener's axes, then the head's.
                let got = apply(heard, apply(listener, apply(rows, world)));
                // Where it lies: the direction on the table in the room, in the
                // head's own axes (right, down, forward).
                let room = space.room_from_world.transform_vector3(world).normalize();
                let local = head.inverse() * room;
                let want = Vec3::new(local.x, -local.y, -local.z);
                assert!((got - want).length() < 1e-4, "tilt {tilt}, {world}: heard {got}, lies {want}");
            }
        }
    }

    #[test]
    fn the_seat_stands_at_the_head_facing_its_way_about_the_vertical_only() {
        let head = Pose { position: Vec3::new(0.3, 0.1, -0.2), orientation: Quat::from_rotation_y(0.7) * Quat::from_rotation_x(-0.4) * Quat::from_rotation_z(0.2) };
        let seat = Seat::at(head);
        assert!(seat.position().distance(head.position) < 1e-6);
        let forward = seat.orientation() * Vec3::NEG_Z;
        assert!(forward.y.abs() < 1e-6, "level: {forward}");
        assert!((seat.orientation().angle_between(Quat::from_rotation_y(0.7))) < 1e-3, "the head's yaw, its pitch and roll left out");
        // Things in front of it, and the head's turn from it.
        let ahead = seat.place(Pose { position: Vec3::new(0.0, 0.0, -2.5), orientation: Quat::IDENTITY });
        assert!(ahead.position.distance(head.position + Quat::from_rotation_y(0.7) * Vec3::new(0.0, 0.0, -2.5)) < 1e-5);
        assert!(seat.relative(Quat::from_rotation_y(0.7)).angle_between(Quat::IDENTITY) < 1e-3, "facing as it does: ahead");
        assert_eq!(Seat::default().pose, Mat4::IDENTITY);
    }

    /// Counts the times a view was anchored again.
    #[derive(Default)]
    struct View(u32);

    impl Anchored for View {
        fn anchor_again(&mut self) {
            self.0 += 1;
        }
    }

    #[test]
    fn a_moved_reference_cancels_gestures_lets_the_stick_go_ends_scaling_and_resets_the_seat() {
        use crate::synthetic::{Runtime, Step, fist, half, in_front, left, open, place, pinch, whole_pinch};
        let mut headset = Headset::default();
        let mut scaling = Scaling::default();
        let mut time = 0.0;
        let mut step = |headset: &mut Headset, step: Step, ui: Context| {
            let hands = in_front(step.hands, Vec3::ZERO, Quat::IDENTITY);
            let signals = step.frame(time, hands, Vec3::X, Runtime::HORIZON);
            time += 1.0 / 72.0;
            let input = Input { time, head: Some(head()), hands, pinch: signals.pinch, micro: signals.micro, micro_capable: signals.micro_capable, focused: true, ..Input::default() };
            headset.update(&input, &screen(), ui, None, None).outs
        };
        let at = |x: f32, hand| place(hand, Quat::IDENTITY, Vec3::new(x, 0.0, 0.0));
        let both = |l, r| Step::of([Some(at(-0.12, left(l))), Some(at(0.12, r))]);
        // A grab half closed, then the reference moves: the grab never fires.
        for shape in [open(), open(), half()] {
            step(&mut headset, both(open(), shape), Context::Walk);
        }
        let mut seat = Seat::at(Pose { position: Vec3::new(0.2, 0.0, 0.1), orientation: Quat::from_rotation_y(1.0) });
        let mut view = View::default();
        reference_changed(&mut headset, &mut scaling, &mut seat, &mut view);
        assert_eq!((seat, view.0), (Seat::default(), 1), "the seat is the reference's origin; the view anchored again");
        assert!(step(&mut headset, both(open(), fist()), Context::Walk).is_empty(), "the grab in progress is cancelled");
        // Two-hand scaling under way ends.
        for _ in 0..3 {
            step(&mut headset, both(open(), open()), Context::Walk);
        }
        let outs: Vec<Out> = [both(whole_pinch(), whole_pinch()), Step::of([Some(at(-0.2, left(whole_pinch()))), Some(at(0.2, whole_pinch()))])]
            .into_iter().flat_map(|hands| step(&mut headset, hands, Context::Walk)).collect();
        carry(outs, &mut Ignored, &mut scaling);
        assert!(scaling.scaling(), "scaling");
        reference_changed(&mut headset, &mut scaling, &mut seat, &mut view);
        assert!(!scaling.scaling() && scaling.scale > 1.0, "ended where it reached: {scaling:?}");
        // A bout's deck: a pinch captures its stick; the move lets it go.
        let bout = Context::Arena { state: 1 };
        for _ in 0..3 {
            step(&mut headset, both(open(), open()), bout);
        }
        for (shape, pinching) in [(pinch(), true), (pinch(), true), (open(), false)] {
            step(&mut headset, Step { pinch: [false, pinching], ..both(open(), shape) }, bout);
        }
        assert!(headset.arcade.steering(), "the pinch captured the stick");
        reference_changed(&mut headset, &mut scaling, &mut seat, &mut view);
        assert!(!headset.arcade.steering(), "the move let the stick go");
        // Recentring needs a tracked head.
        assert!(recenter(&mut seat, None, &mut view).is_err());
        assert_eq!(view.0, 3);
        recenter(&mut seat, Some(Pose { position: Vec3::new(0.0, 0.0, 0.5), orientation: Quat::from_rotation_y(-0.5) }), &mut view).unwrap();
        assert!(seat.position().distance(Vec3::new(0.0, 0.0, 0.5)) < 1e-6 && view.0 == 4);
    }

    /// [`field`] standing on a floor (a square 2000 units a side, 100 below
    /// the camera, ahead of it), so that a pinch below the screen's centre
    /// walks.
    fn floored() -> Field {
        use xem_core::field::{Layer, Triangle, V3};
        let mut field = field();
        let vertices = vec![V3::new(-1000.0, 100.0, 0.0), V3::new(1000.0, 100.0, 0.0), V3::new(-1000.0, 100.0, 2000.0), V3::new(1000.0, 100.0, 2000.0)];
        let triangles = vec![Triangle { v: [0, 1, 2], n: [-1, 1, -1], attribute: 0 }, Triangle { v: [1, 3, 2], n: [-1, -1, 0], attribute: 0 }];
        field.layers = vec![Layer { vertices, triangles }];
        for actor in field.actors.iter_mut().flatten() {
            actor.position.y = 100.0;
        }
        field
    }

    #[test]
    fn two_hand_scaling_owns_both_hands_stays_within_its_range_and_ends_where_it_reached() {
        use crate::synthetic::{Runtime, gesture, in_front};
        let field = floored();
        let floor = (160.0, 180.0);
        let mut time = 0.0;
        let mut play = |headset: &mut Headset, name: &str, ui: Context| -> Effects {
            let mut effects = Effects::default();
            for step in gesture(name).unwrap() {
                let head = Pose { position: Vec3::ZERO, orientation: Quat::from_rotation_arc(Vec3::NEG_Z, screen().point(floor).normalize()) };
                let hands = in_front(step.hands, Vec3::ZERO, Quat::IDENTITY);
                let frame = step.frame(time, hands, Vec3::X, Runtime::HORIZON);
                time += 1.0 / 72.0;
                let input = Input { time: frame.time, head: Some(head), hands, pinch: frame.pinch, micro: frame.micro, micro_capable: frame.micro_capable, focused: true, ..Input::default() };
                let more = headset.update(&input, &screen(), ui, Some(&field), None);
                effects.outs.extend(more.outs);
                effects.gestures.extend(more.gestures);
            }
            effects
        };
        let scaled = |outs: &[Out]| outs.iter().all(|o| matches!(o, Out::ScaleBegin | Out::Scale { .. } | Out::ScaleEnd));
        let walked = |outs: &[Out]| outs.iter().any(|o| matches!(o, Out::Command { command } if command["cmd"] == "navigate"));
        // The gaze on the floor, where a pinch walks: the runtime's pinch
        // held through the gesture walks nowhere, and nothing pauses.
        let mut headset = Headset::default();
        let mut scaling = Scaling::default();
        assert!(walked(&play(&mut headset, "pinch", Context::Walk).outs), "a pinch there walks");
        for (name, reached) in [("scale-pinch", 2.0), ("scale-in", 1.0), ("scale-out", 2.0), ("scale-out", 3.0), ("scale-in", 1.5)] {
            let effects = play(&mut headset, name, Context::Walk);
            assert!(scaled(&effects.outs) && effects.outs.contains(&Out::ScaleEnd), "{name}: {:?}", effects.outs);
            assert!(!headset.pauses.paused(), "{name}");
            carry(effects.outs, &mut Ignored, &mut scaling);
            assert!((scaling.scale - reached).abs() < 0.08, "{name}: scale {} (the range 0.4-3 holding it)", scaling.scale);
        }
        // A factor after the end changes nothing: it scales from no start.
        carry(vec![Out::Scale { factor: 2.0 }], &mut Ignored, &mut scaling);
        assert!((scaling.scale - 1.5).abs() < 0.08, "{}", scaling.scale);
        carry(vec![Out::ScaleBegin, Out::Scale { factor: 0.01 }, Out::ScaleEnd], &mut Ignored, &mut scaling);
        assert_eq!(scaling.scale, 0.4, "the smallest screen");
        // With settings open (their pause holding the game) scaling is the
        // app's: it still scales.
        headset.set_settings(true);
        let effects = play(&mut headset, "scale-out", Context::Walk);
        carry(effects.outs, &mut Ignored, &mut scaling);
        assert!((scaling.scale - 0.8).abs() < 0.05, "{}", scaling.scale);
    }

    /// Plays the synthetic gesture `name` in front of the head (one step a
    /// frame) and carries its outs out; returns them and the diorama's live
    /// scale after each frame.
    fn scale_by(headset: &mut Headset, scaling: &mut Scaling, name: &str, time: &mut f64) -> (Vec<Out>, Vec<Option<f32>>) {
        use crate::synthetic::{Runtime, gesture, in_front};
        let (mut outs, mut live) = (Vec::new(), Vec::new());
        for step in gesture(name).unwrap() {
            let hands = in_front(step.hands, Vec3::ZERO, Quat::IDENTITY);
            let frame = step.frame(*time, hands, Vec3::X, Runtime::HORIZON);
            *time += 1.0 / 72.0;
            let input = Input { time: frame.time, head: Some(head()), hands, pinch: frame.pinch, micro: frame.micro, micro_capable: frame.micro_capable, focused: true, ..Input::default() };
            let effects = headset.update(&input, &screen(), Context::Walk, None, None);
            outs.extend(effects.outs.iter().cloned());
            carry(effects.outs, &mut Ignored, scaling);
            live.push(scaling.diorama_scale());
        }
        (outs, live)
    }

    #[test]
    fn two_hand_scaling_scales_the_diorama_about_its_anchor_live_and_commits_once_when_it_ends() {
        use xem_render::ps1::{Table, XrView};
        let mut time = 0.0;
        let mut headset = Headset::default();
        let mut scaling = Scaling::default();
        let only_scaling = |outs: &[Out]| outs.iter().all(|o| matches!(o, Out::ScaleBegin | Out::Scale { .. } | Out::ScaleEnd));
        // The diorama is the view, at its default 100%: spreading the hands
        // enlarges it live, the screen keeps its scale, and the setting takes
        // it once, when they let go.
        scaling.view(Some(100));
        let (outs, live) = scale_by(&mut headset, &mut scaling, "spread", &mut time);
        assert!(only_scaling(&outs), "no movement, press, command or grab: {outs:?}");
        let reached = live.iter().flatten().fold(0.0f32, |a, b| a.max(*b));
        assert!((reached - 2.52).abs() < 0.01 && scaling.scale == 1.0, "live to {reached}; the screen {}", scaling.scale);
        assert!(live.windows(2).all(|w| w[1].unwrap_or(f32::MAX) >= w[0].unwrap_or(0.0) || w[1].is_none()), "it grows as the hands part: {live:?}");
        assert_eq!(live.last(), Some(&None), "the gesture ended: {live:?}");
        assert_eq!((scaling.reached(), scaling.reached()), (Some(252), None), "committed once");
        // Live, the view shows the hands' scale in place of the setting's.
        let table = Table::default();
        scaling.view(Some(252));
        scaling.begin();
        scaling.to(0.5);
        assert_eq!(scaling.shown(XrView::Diorama(table)), XrView::Diorama(Table { scale: 1.26, ..table }));
        assert_eq!(scaling.shown(XrView::Screen), XrView::Screen);
        scaling.end();
        assert_eq!(scaling.reached(), Some(126));
        // About the anchor: the point the table follows (the camera's target)
        // stays where it stands in the room within a millimetre at every
        // scale, and every other point moves away from it by the scale.
        let seat = Mat4::from_rotation_translation(Quat::from_rotation_y(0.4), Vec3::new(0.1, 0.0, -0.05));
        let camera = level_camera();
        let space = |scale: f32| DioramaSpace::first(&camera, 256.0, glam::Vec2::new(320.0, 224.0), &Table { scale, tilt: 0.3, ..table }, seat).unwrap();
        let (target, other) = (Vec3::from(camera.target), Vec3::new(150.0, -40.0, 1100.0));
        let at_one = space(1.0);
        let (anchor, apart) = (at_one.room_from_world.transform_point3(target), at_one.room_from_world.transform_point3(other));
        assert!(anchor.distance(at_one.room_from_table.transform_point3(Vec3::ZERO)) < 1e-4, "the target stands at the table's anchor");
        for scale in [0.25, 0.6, 1.26, 2.52, 4.0] {
            let s = space(scale);
            let moved = s.room_from_world.transform_point3(target).distance(anchor);
            assert!(moved < 0.001, "scale {scale}: the anchor moved {moved} m");
            let away = s.room_from_world.transform_point3(other).distance(anchor) / apart.distance(anchor);
            assert!((away - scale).abs() < 1e-3, "scale {scale}: another point {away} times as far from the anchor");
            assert_eq!((s.room_from_table, s.pane, s.pane_size, s.volume), (at_one.room_from_table, at_one.pane, at_one.pane_size, at_one.volume), "scale {scale}: the table, its volume and pane stay");
        }
        // Within the setting's range: from 400% a spread stays there, from
        // 25% a contraction does.
        for (from, name, to) in [(400, "spread", 400), (25, "contract", 25), (60, "contract", 36)] {
            scaling.view(Some(from));
            let (_, live) = scale_by(&mut headset, &mut scaling, name, &mut time);
            assert!(live.iter().flatten().all(|s| (0.25..=4.0).contains(s)), "{from}% {name}: {live:?}");
            assert_eq!(scaling.reached(), Some(to), "{from}% {name}");
        }
        // The screen as the view: the session's scale changes, nothing is
        // committed and the view shows no diorama scale.
        scaling.view(None);
        let (outs, live) = scale_by(&mut headset, &mut scaling, "spread", &mut time);
        assert!(only_scaling(&outs) && live.iter().all(Option::is_none));
        assert!((scaling.scale - 2.52).abs() < 0.01 && scaling.reached().is_none(), "the screen {}", scaling.scale);
        scale_by(&mut headset, &mut scaling, "contract", &mut time);
        assert!((scaling.scale - 2.52 * 0.598).abs() < 0.01, "the screen {}", scaling.scale);
    }

    #[test]
    fn a_diorama_scaling_cut_short_ends_where_it_reached_and_inspection_scales_nothing() {
        use crate::synthetic::{Runtime, gesture, in_front};
        let mut time = 0.0;
        let frame_input = |step: &crate::synthetic::Step, time: f64, focused: bool| {
            let hands = in_front(step.hands, Vec3::ZERO, Quat::IDENTITY);
            let frame = step.frame(time, hands, Vec3::X, Runtime::HORIZON);
            Input { time, head: Some(head()), hands, pinch: frame.pinch, micro: frame.micro, micro_capable: frame.micro_capable, focused, ..Input::default() }
        };
        let spread = gesture("spread").unwrap();
        // Cut short halfway: the app losing focus (the runtime's menu),
        // inspection beginning (the hands then let go), the screen chosen as
        // the view, or the runtime moving the reference space: the scaling
        // ends where it reached, which the setting takes.
        for cut in ["focus", "inspection", "view", "reference"] {
            let mut headset = Headset::default();
            let mut scaling = Scaling::default();
            scaling.view(Some(100));
            for (i, step) in spread.iter().enumerate() {
                let halfway = i == spread.len() / 2;
                if halfway && cut == "inspection" {
                    headset.inspecting = true;
                }
                if halfway && cut == "view" {
                    scaling.view(None);
                }
                if halfway && cut == "reference" {
                    reference_changed(&mut headset, &mut scaling, &mut Seat::default(), &mut View::default());
                }
                let effects = headset.update(&frame_input(step, time, !(halfway && cut == "focus")), &screen(), Context::Walk, None, None);
                time += 1.0 / 72.0;
                carry(effects.outs, &mut Ignored, &mut scaling);
                if halfway {
                    break;
                }
            }
            if cut == "inspection" {
                // The hands let go while inspecting: the end still comes.
                for step in &spread[spread.len() - 2..] {
                    let effects = headset.update(&frame_input(step, time, true), &screen(), Context::Walk, None, None);
                    time += 1.0 / 72.0;
                    carry(effects.outs, &mut Ignored, &mut scaling);
                }
            }
            let reached = scaling.reached();
            assert!(!scaling.scaling() && reached.is_some_and(|p| (110..252).contains(&p)), "{cut}: ended at {reached:?}");
            if cut == "focus" || cut == "reference" {
                // The hands still gathered (the focus back) begin nothing
                // until both have opened.
                let held = &spread[spread.len() / 2];
                for _ in 0..3 {
                    let effects = headset.update(&frame_input(held, time, true), &screen(), Context::Walk, None, None);
                    time += 1.0 / 72.0;
                    assert!(effects.outs.is_empty(), "{cut}: {:?}", effects.outs);
                }
            }
        }
        // Inspecting from the start, a spread scales neither the diorama nor
        // the screen (the gesture still owns the hands: no grab fires).
        let mut headset = Headset { inspecting: true, ..Headset::default() };
        let mut scaling = Scaling::default();
        scaling.view(Some(100));
        let (outs, live) = scale_by(&mut headset, &mut scaling, "spread", &mut time);
        assert!(live.iter().all(Option::is_none) && scaling.reached().is_none() && scaling.scale == 1.0, "{outs:?}");
    }

    #[test]
    fn a_reached_scale_is_committed_to_the_setting_through_the_settings_service() {
        use xem_settings::{MemoryStore, SettingChange, SettingsService};
        let mut settings = SettingsService::with_defaults(MemoryStore::default());
        let changes = std::rc::Rc::new(std::cell::RefCell::new(Vec::new()));
        settings.subscribe({
            let changes = changes.clone();
            move |ack| changes.borrow_mut().push(ack.change.clone())
        });
        let before = settings.settings().graphics;
        let ack = commit_scale(&mut settings, 252).unwrap();
        assert_eq!(ack.settings.graphics.xr.diorama_scale, 252);
        let mut want = before;
        want.xr.diorama_scale = 252;
        assert_eq!(settings.settings().graphics, want, "the rest of the block as it was");
        assert_eq!(changes.borrow().as_slice(), [SettingChange::Graphics(want)], "acknowledged once to every subscriber");
        // Reset diorama (the panel's) puts it back; out of range is refused.
        let mut reset = settings.settings().graphics;
        reset.xr = reset.xr.diorama_reset();
        settings.apply(SettingChange::Graphics(reset)).unwrap();
        assert_eq!(settings.settings().graphics.xr.diorama_scale, 100);
        assert!(commit_scale(&mut settings, 401).is_err() && settings.settings().graphics.xr.diorama_scale == 100);
    }
}

/// What carries a headset's effects out on a host: the command layer and
/// the player's input.
pub trait Carry {
    /// A command for the command layer (`navigate`, `interact`, ...).
    fn command(&mut self, command: serde_json::Value);
    /// Actions pressed once.
    fn press(&mut self, actions: Vec<xem_core::action::GameAction>);
    /// An interface entry selected (and confirmed).
    fn select(&mut self, index: usize, confirm: bool);
    /// First-person inspection toggled (where the host has it).
    fn inspect(&mut self);
    /// Everything held let go (tracking or focus lost).
    fn release(&mut self);
    /// Auto-advance dialogue toggled (AUTO beside the screen): the app's
    /// `ToggleAutoAdvance`, through the settings.
    fn toggle_auto_advance(&mut self);
}

/// Two-hand scaling (two whole-hand pinches: spread to enlarge, contract to
/// shrink, either hand let go to finish). While the diorama is the view it
/// scales the diorama about its table's anchor (the point the table
/// follows stays where it stands in the room; the table's volume, its pane,
/// the keys and the panel keep their place and size): live while the hands
/// scale it ([`Scaling::shown`]), within the setting's range, and committed
/// once to the setting when the gesture ends ([`Scaling::reached`],
/// [`commit_scale`]). Otherwise it scales the headset's screen for the
/// session (in memory, 0.4-3 times the setting's width). Presentation
/// only: nothing reaches the game.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Scaling {
    /// The screen's scale (1: the setting's width).
    pub scale: f32,
    /// The diorama's scale setting while the diorama is the view
    /// ([`Scaling::view`]).
    diorama: Option<f32>,
    under: Option<Under>,
    /// The diorama's scale (percent) a gesture ended at, not yet committed.
    reached: Option<u16>,
}

/// What a two-hand scaling under way scales, from what.
#[derive(Clone, Copy, Debug, PartialEq)]
enum Under {
    /// The screen's scale as it began.
    Screen { from: f32 },
    /// The diorama's scale as it began, and now.
    Diorama { from: f32, now: f32 },
}

/// The screen's scale's bounds (times the setting's width).
const SCREEN_SCALE: (f32, f32) = (0.4, 3.0);

impl Default for Scaling {
    fn default() -> Self {
        Self { scale: 1.0, diorama: None, under: None, reached: None }
    }
}

/// A diorama scale as the setting holds it (percent, within its range).
fn percent(scale: f32) -> u16 {
    let range = xem_settings::graphics::DIORAMA_SCALE;
    ((scale * 100.0).round() as i64).clamp(i64::from(*range.start()), i64::from(*range.end())) as u16
}

impl Scaling {
    /// What two-hand scaling scales from now on (hosts say it every frame):
    /// the diorama, at its setting's scale (`diorama`, percent), while it is
    /// the view, else the screen. A scaling under way of the other ends at
    /// the scale it reached.
    pub fn view(&mut self, diorama: Option<u16>) {
        let diorama = diorama.map(|percent| f32::from(percent) / 100.0);
        if matches!((self.under, diorama), (Some(Under::Screen { .. }), Some(_)) | (Some(Under::Diorama { .. }), None)) {
            self.end();
        }
        self.diorama = diorama;
    }

    /// Whether a two-hand scaling is under way.
    pub fn scaling(&self) -> bool {
        self.under.is_some()
    }

    /// The diorama's scale while the hands scale it (1: the original
    /// screen's width at the camera's target spans the table).
    pub fn diorama_scale(&self) -> Option<f32> {
        match self.under {
            Some(Under::Diorama { now, .. }) => Some(now),
            _ => None,
        }
    }

    /// `view` (the settings') as the scaling under way shows it: the
    /// diorama at the hands' scale until the setting takes it.
    pub fn shown(&self, view: xem_render::ps1::XrView) -> xem_render::ps1::XrView {
        match (view, self.diorama_scale()) {
            (xem_render::ps1::XrView::Diorama(table), Some(scale)) => xem_render::ps1::XrView::Diorama(xem_render::ps1::Table { scale, ..table }),
            (view, _) => view,
        }
    }

    fn begin(&mut self) {
        self.under = Some(match self.diorama {
            Some(from) => Under::Diorama { from, now: from },
            None => Under::Screen { from: self.scale },
        });
    }

    /// The hands' distance is `factor` times what it was as they began.
    fn to(&mut self, factor: f32) {
        let range = xem_settings::graphics::DIORAMA_SCALE;
        let (least, most) = (f32::from(*range.start()) / 100.0, f32::from(*range.end()) / 100.0);
        match &mut self.under {
            Some(Under::Screen { from }) => self.scale = (*from * factor).clamp(SCREEN_SCALE.0, SCREEN_SCALE.1),
            Some(Under::Diorama { from, now }) => *now = (*from * factor).clamp(least, most),
            None => {}
        }
    }

    /// Ends the scaling under way at the scale it reached: the screen's
    /// stays; the diorama's waits to be committed ([`Scaling::reached`]).
    pub fn end(&mut self) {
        if let Some(Under::Diorama { now, .. }) = self.under.take() {
            self.reached = Some(percent(now));
        }
    }

    /// The diorama's scale (percent) the last scaling of it ended at, once:
    /// for the host to commit ([`commit_scale`]).
    pub fn reached(&mut self) -> Option<u16> {
        self.reached.take()
    }
}

/// Commits the diorama's scale a two-hand scaling ended at (`percent`,
/// [`Scaling::reached`]) as the player's setting, through the settings
/// service as every other change (validated, saved, acknowledged to every
/// subscriber: the panel shows it). The rest of the graphics block stays as
/// it is.
pub fn commit_scale(settings: &mut xem_settings::SettingsService, percent: u16) -> Result<xem_settings::Ack, xem_settings::Rejected> {
    let mut graphics = settings.settings().graphics;
    graphics.xr.diorama_scale = percent;
    settings.apply(xem_settings::SettingChange::Graphics(graphics))
}

/// Carries out resolved effects (the pauses and settings were the
/// headset's own already).
pub fn carry(outs: Vec<Out>, host: &mut impl Carry, scaling: &mut Scaling) {
    for out in outs {
        match out {
            Out::Press { actions } => host.press(actions),
            Out::Select { index, confirm } => host.select(index, confirm),
            Out::Command { command } => host.command(command),
            Out::Key { key } => {
                if let Some(key) = crate::overlay::QuantityKey::ALL.get(key as usize) {
                    host.command(serde_json::json!({"cmd": "shop_quantity", "op": key.op()}));
                }
            }
            Out::ToggleInspect => host.inspect(),
            Out::ToggleAutoAdvance => host.toggle_auto_advance(),
            Out::ScaleBegin => scaling.begin(),
            Out::Scale { factor } => scaling.to(factor),
            Out::ScaleEnd => scaling.end(),
            Out::TrackingLost => {
                host.release();
                host.command(serde_json::json!({"cmd": "assist_stop"}));
            }
            Out::TogglePause | Out::ToggleSettings => {}
        }
    }
}
