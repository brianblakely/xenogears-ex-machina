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
//!   the screen as it was (the gesture still owns both hands).
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
use crate::dispatch::{self, Aim, Out};
use crate::gesture::{Frame, Gesture, Recognizer};
use glam::{Quat, Vec3};
use serde::{Deserialize, Serialize};
use xem_core::context::Context;
use xem_core::field::Field;
use xem_core::menu::Targets;

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
/// framing's sides).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Screen {
    pub quad: Quad,
    pub display: (f32, f32),
    pub side: f32,
}

impl Screen {
    /// The game screen pixel a ray meets.
    pub fn aim(&self, origin: Vec3, direction: Vec3) -> Option<(f32, f32)> {
        let ((u, v), _) = self.quad.hit(origin, direction)?;
        Some((u * (self.display.0 + 2.0 * self.side) - self.side, v * self.display.1))
    }

    /// The room point of a game screen pixel.
    pub fn point(&self, (x, y): (f32, f32)) -> Vec3 {
        let u = (x + self.side) / (self.display.0 + 2.0 * self.side) - 0.5;
        let v = 0.5 - y / self.display.1;
        self.quad.pose.position + self.quad.pose.orientation * Vec3::new(u * self.quad.size.0, v * self.quad.size.1, 0.0)
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
    was_focused: bool,
    tracked: [bool; 2],
}

impl Headset {
    /// The game screen pixel the chosen source aims at, if available.
    pub fn aim(&self, input: &Input, screen: &Screen) -> Aim {
        let (origin, direction) = ray(self.source, input)?;
        screen.aim(origin, direction)
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
            None => crate::overlay::side_keys(ui, targets).into_iter().enumerate()
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
        // Focus lost: everything pending is dropped, and the game pauses.
        if !input.focused {
            if self.was_focused {
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
        let side_keys = if deck.is_some() { vec![] } else { crate::overlay::side_keys(ui, targets) };
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
        // Inspecting, the screen keeps its size (the scaling gesture still
        // owns both hands: no grab fires) and, while the player walks,
        // nothing walks, approaches, jumps or turns the game's camera.
        if self.inspecting {
            game_gestures.retain(|g| !matches!(g, Gesture::ScaleBegin | Gesture::Scale { .. } | Gesture::ScaleEnd));
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
mod tests {
    use super::*;
    use crate::synthetic::{THUMB_REST, right_hand};

    fn screen() -> Screen {
        Screen { quad: Quad { pose: Pose { position: Vec3::new(0.0, 0.0, -2.5), orientation: Quat::IDENTITY }, size: (3.2, 2.24) }, display: (320.0, 224.0), side: 0.0 }
    }

    fn head() -> Pose {
        Pose { position: Vec3::ZERO, orientation: Quat::IDENTITY }
    }

    #[test]
    fn the_head_aims_at_the_screens_centre_and_rays_invert() {
        let screen = screen();
        let input = Input { head: Some(head()), focused: true, ..Input::default() };
        let headset = Headset::default();
        let aim = headset.aim(&input, &screen).unwrap();
        assert!((aim.0 - 160.0).abs() < 0.01 && (aim.1 - 112.0).abs() < 0.01, "{aim:?}");
        let point = screen.point((40.0, 200.0));
        let again = screen.aim(Vec3::ZERO, point.normalize()).unwrap();
        assert!((again.0 - 40.0).abs() < 0.01 && (again.1 - 200.0).abs() < 0.01, "{again:?}");
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
}

/// Two-hand scaling of the headset's screen.
#[derive(Clone, Copy, Debug)]
pub struct Scaling {
    pub scale: f32,
    start: Option<f32>,
}

impl Default for Scaling {
    fn default() -> Self {
        Self { scale: 1.0, start: None }
    }
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
            Out::ScaleBegin => scaling.start = Some(scaling.scale),
            Out::Scale { factor } => {
                if let Some(start) = scaling.start {
                    scaling.scale = (start * factor).clamp(0.4, 3.0);
                }
            }
            Out::ScaleEnd => scaling.start = None,
            Out::TrackingLost => {
                host.release();
                host.command(serde_json::json!({"cmd": "assist_stop"}));
            }
            Out::TogglePause | Out::ToggleSettings => {}
        }
    }
}
