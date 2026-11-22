//! Assisted movement in a field (plan.md, field traversal and interaction):
//! a committed destination or an actor to approach and talk to, played as
//! the pad input a player would give. Each frame the route is planned again
//! from the game's own state ([`crate::field`]); the D-pad points along it
//! by the camera's angle, Run is held while much of it remains (with a
//! margin, so it does not flicker). A route that needs a jump (onto a slide
//! no higher than a jump rises, across a gap no wider than it carries)
//! walks to the takeoff and presses Jump there toward the landing, holds
//! the direction through the jump and lets go of it over the landing (the
//! leader drops where it is), then plans again; with the ledge brake on it
//! waits at the takeoff for the player's own jump instead. An approach
//! presses Confirm exactly once, only while the player walks within the
//! original's talk reach and facing. A walk to the floor before a door or a
//! wall the player pointed at walks on into it (an exit's zone).
//!
//! The assist ends when it arrives (or is held beside the goal, as near as
//! it gets), when the player presses a direction (the player's own input
//! replaces it), when the interface changes (a dialogue, a menu, a battle,
//! another map) or when the player stops making progress.
use crate::action::{Assignment, GameAction};
use crate::context::Context;
use crate::field::{self, Field, V3};
use serde::{Deserialize, Serialize};

/// Run beyond this much route; walk only within that much, extremely near
/// (world units; the player walks about 3 a frame).
const RUN_ON: f32 = 96.0;
const RUN_OFF: f32 = 40.0;
/// Arrived within this of the destination.
const ARRIVE: f32 = 10.0;
/// A corner passed within this.
const CORNER: f32 = 6.0;
/// Takeoff reached within this: the jump starts at the floor's edge, where
/// its flight was planned from (a walking step is 12).
const TAKEOFF: f32 = 6.0;
/// Under the ledge brake, the takeoff is waited at from this far: the
/// player's own jump starts there, on the floor. Held short of it within
/// this as long as `HELD` frames (the game stops a walk the step short of a
/// slide it cannot walk onto), the jump starts where the leader stands.
const BRAKE: f32 = 20.0;
const HELD: u32 = 2;
/// Frames without getting nearer before stepping aside, and before giving
/// up; how much nearer counts.
const SIDESTEP_FRAMES: u32 = 15;
const STUCK_FRAMES: u32 = 120;
const PROGRESS: f32 = 4.0;
/// The longest approach to an actor (frames).
const APPROACH_FRAMES: u32 = 1800;
/// Frames a Confirm is held, and then waited on for the game to answer.
const CONFIRM_FRAMES: u32 = 2;
const ANSWER_FRAMES: u32 = 30;
/// Keep the D-pad's eighth unless the route turns this far from it.
const SECTOR_HYSTERESIS: i32 = 0x140;
/// Over the landing within this, or within half a frame's flight of it,
/// the direction is let go: the leader stops where it is and drops.
const LAND: f32 = 3.0;
/// How far the leader goes in the air in an update with the D-pad held
/// (the jumps' arcs: FOOT_ARC, GEAR_ARC).
const AIR_STEP: f32 = 12.0;
/// Frames Jump is held at a takeoff, and waited on for the leader to leave
/// the floor before walking up to it again; the jumps tried before giving up.
const LEAP_FRAMES: u32 = 4;
const LEAP_WAIT: u32 = 20;
const JUMPS: u32 = 3;
/// Walking on into a door or a wall: at most this many frames, ending
/// after this many without getting nearer.
const PRESS_FRAMES: u32 = 90;
const PRESS_STILL: u32 = 20;
/// Held this near the goal (another actor's body, a wall), the walk has
/// arrived as near as it gets.
const NEAR_ENOUGH: f32 = 48.0;

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum Intent {
    /// Walk to a floor point on the player's layer; then, if the player
    /// pointed past that floor at a door or a wall (`beyond`), on into it.
    Walk {
        to: V3,
        triangle: usize,
        layer: usize,
        #[serde(default, skip_serializing_if = "Option::is_none")]
        beyond: Option<V3>,
    },
    /// Approach an event actor and Confirm once in reach.
    Interact { actor: usize },
}

impl Intent {
    /// Where it heads in `field`: a walk's floor point, or the feet of the
    /// actor approached (for the destination's mark).
    pub fn heading_to(&self, field: &Field) -> Option<V3> {
        match *self {
            Intent::Walk { to, .. } => Some(to),
            Intent::Interact { actor } => field.actors.get(actor).copied().flatten().map(|a| a.position),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Phase {
    Moving,
    /// At the takeoff with the ledge brake on; the jump is the player's.
    AwaitingJump,
    /// At the takeoff, pressing Jump toward the landing.
    Leaping,
    /// In the air: on toward the landing, letting go over it.
    Jumping,
    /// Arrived before a door or a wall the player pointed at: walking on into it.
    Pressing,
    /// Confirm pressed; waiting for the game to answer.
    Confirming,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Outcome {
    Arrived,
    Confirmed,
    /// The player's own direction replaced it.
    Replaced,
    /// The interface changed (dialogue, menu, battle, map).
    Interrupted,
    Unreachable,
    Stuck,
    TargetLost,
    Stopped,
}

/// The game's map id (`mode_field_map_id`).
const FIELD_MAP_ID: u32 = 0x8004_F34C;

#[derive(Clone, Debug, Serialize)]
pub struct Assist {
    pub intent: Intent,
    pub phase: Phase,
    run: bool,
    sector: Option<i32>,
    map: u16,
    /// The least route left so far, and frames since it last shrank.
    best: f32,
    still: u32,
    frames: u32,
    confirm: u32,
    /// Stop at a takeoff for the player's own jump rather than jump there.
    pub ledge_brake: bool,
    /// The jump under way: where it lands, the frames Jump has been
    /// pressed, and the jumps tried.
    landing: Option<V3>,
    leap: u32,
    jumps: u32,
    /// Where the leader was the frame before, in the air, and how high
    /// above the takeoff.
    #[serde(skip)]
    flown: Option<V3>,
    #[serde(skip)]
    risen: Option<f32>,
    /// Walking on into a door or a wall: the frames so far, the least
    /// distance left and the frames since it last shrank.
    pressed: u32,
    press_best: f32,
    press_still: u32,
    /// The jump of the route planned whole last: until the leader lands
    /// from it, each frame plans the walk to its takeoff alone (its jumps,
    /// ways and paths kept per thread for the walkmesh: Field::route).
    #[serde(skip)]
    planned: Option<field::Jump>,
}

/// What one frame of the assist does.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Step {
    pub actions: Vec<GameAction>,
    pub done: Option<Outcome>,
}

impl Assist {
    pub fn new(intent: Intent, memory: &dyn crate::memory::GameMemory, ledge_brake: bool) -> Assist {
        Assist {
            intent, phase: Phase::Moving, run: false, sector: None, map: map_id(memory), best: f32::INFINITY, still: 0, frames: 0, confirm: 0, ledge_brake,
            landing: None, leap: 0, jumps: 0, flown: None, risen: None, pressed: 0, press_best: f32::INFINITY, press_still: 0, planned: None,
        }
    }

    /// One frame: the actions to hold, or the outcome.
    pub fn step(&mut self, memory: &dyn crate::memory::GameMemory, ui: Context) -> Step {
        let end = |outcome| Step { actions: vec![], done: Some(outcome) };
        if self.phase == Phase::Confirming {
            self.confirm += 1;
            if self.confirm <= CONFIRM_FRAMES {
                return Step { actions: vec![GameAction::Confirm], done: None };
            }
            return if ui != Context::Walk || self.confirm > CONFIRM_FRAMES + ANSWER_FRAMES { end(Outcome::Confirmed) } else { Step::default() };
        }
        if map_id(memory) != self.map {
            return end(Outcome::Interrupted);
        }
        let Some(field) = Field::read(memory) else { return end(Outcome::Interrupted) };
        let Some(player) = field.player().copied() else { return end(Outcome::Interrupted) };
        let airborne = player.airborne();
        if ui != Context::Walk && !airborne {
            return end(Outcome::Interrupted);
        }
        self.frames += 1;
        // The goal and its layer (a route goes up stairs and down through
        // pits to another layer's floor).
        let (goal, goal_layer, goal_triangle, reach) = match self.intent {
            Intent::Walk { to, triangle, layer, .. } => (to, layer, triangle, ARRIVE),
            Intent::Interact { actor } => {
                let Some(target) = field.actors.get(actor).copied().flatten() else { return end(Outcome::TargetLost) };
                if self.frames > APPROACH_FRAMES {
                    return end(Outcome::TargetLost);
                }
                let Some(mesh) = field.layers.get(target.layer) else { return end(Outcome::TargetLost) };
                let talk = f32::from(player.radius) + 32.0 + f32::from(target.radius);
                // Its floor point, or the nearest one within talk reach.
                let Some((triangle, floor)) = mesh.nearest(target.position.x, target.position.z) else { return end(Outcome::TargetLost) };
                if floor.planar(target.position) > talk * 0.9 {
                    return end(Outcome::Unreachable);
                }
                if player.position.planar(target.position) < talk * 0.92 && !airborne {
                    // In reach: face it, then Confirm once.
                    let toward = field::heading(target.position.x - player.position.x, target.position.z - player.position.z);
                    let facing = (i32::from(player.heading_goal) - toward).rem_euclid(0x1000);
                    if (0x2BC..0xD45).contains(&facing) {
                        return Step { actions: dpad(screen_sector(toward, &field, &mut None)), done: None };
                    }
                    self.phase = Phase::Confirming;
                    self.confirm = 1;
                    return Step { actions: vec![GameAction::Confirm], done: None };
                }
                (floor, target.layer, triangle, ARRIVE)
            }
        };
        if self.phase == Phase::Pressing
            && let Intent::Walk { beyond: Some(beyond), .. } = self.intent
        {
            return self.press(&field, &player, beyond);
        }
        if airborne {
            // Through the jump: on toward the landing, letting go over it so
            // the leader drops there (in the air the D-pad steers it).
            self.phase = Phase::Jumping;
            let landing = self.landing.unwrap_or(goal);
            // Let go at the landing itself (a narrow floor beyond a slide
            // leaves little room short of it), or once past it along the
            // flight: the D-pad's eight directions need not bring the
            // leader within LAND of it.
            let flown = self.flown.map_or(0.0, |last| player.position.planar(last));
            self.flown = Some(player.position);
            let past = self.planned.is_some_and(|jump| {
                let (along, beyond) = (landing.sub_planar(jump.takeoff), player.position.sub_planar(landing));
                along.x * beyond.x + along.z * beyond.z >= 0.0
            });
            if player.position.planar(landing) < LAND.max(flown / 2.0) || past {
                return Step::default();
            }
            // On only while the next stretch's slides lie below by the
            // flight's clearance, else rising where it is (as the route's
            // flight was planned: Field::flight).
            let height = self.planned.map(|jump| jump.takeoff.y - player.position.y);
            let sinking = self.risen.zip(height).map_or(0.0, |(before, now)| (before - now).max(0.0));
            self.risen = height;
            if let (Some(jump), Some(height)) = (self.planned, height) {
                let line = landing.sub_planar(jump.takeoff);
                let length = (line.x * line.x + line.z * line.z).sqrt().max(1e-3);
                let along = player.position.sub_planar(jump.takeoff);
                let s = (along.x * line.x + along.z * line.z) / length;
                if field.rise_ahead(&player, &jump, s, s + AIR_STEP) > height - sinking {
                    return Step::default();
                }
            }
            let toward = field::heading(landing.x - player.position.x, landing.z - player.position.z);
            return Step { actions: dpad(screen_sector(toward, &field, &mut self.sector)), done: None };
        }
        if self.phase == Phase::Jumping {
            // Landed: plan again from here, the progress counted anew.
            self.phase = Phase::Moving;
            self.landing = None;
            self.flown = None;
            self.risen = None;
            self.planned = None;
            self.best = f32::INFINITY;
            self.still = 0;
        }
        // A walk's goal inside another actor's keep-out (a barrel, a
        // villager, a door's trigger box): into it, as the D-pad walks (a
        // touch it runs on contact runs), and held still against it, there.
        let into = if matches!(self.intent, Intent::Walk { .. }) && goal_layer == player.layer {
            field.obstacles(&player).into_iter().find(|(centre, keep)| goal.planar(*centre) < *keep)
        } else {
            None
        };
        if player.position.planar(goal) < reach && player.layer == goal_layer {
            if let Intent::Walk { beyond: Some(beyond), .. } = self.intent {
                self.phase = Phase::Pressing;
                return self.press(&field, &player, beyond);
            }
            // An approach that arrives out of talk reach found no way in.
            return end(if matches!(self.intent, Intent::Interact { .. }) { Outcome::Unreachable } else { Outcome::Arrived });
        }
        // The walk to the takeoff of the jump planned (the rest as planned),
        // else the route planned whole.
        let walk = self.planned.filter(|jump| jump.layer == player.layer).and_then(|jump| {
            let approach = field.approach(&player, &jump);
            let mut route = field.route(&player, jump.layer, jump.from, approach.unwrap_or(jump.takeoff)).filter(|r| r.jump.is_none())?;
            route.corners.extend(approach.map(|_| jump.takeoff));
            route.corners.extend([jump.toward, goal]);
            route.length = field::path_length(player.position, &route.corners);
            (route.jump, route.jumps) = (Some(jump), true);
            Some(route)
        });
        let Some(route) = walk.or_else(|| field.route(&player, goal_layer, goal_triangle, goal)) else { return end(Outcome::Unreachable) };
        self.planned = route.jump;
        let mut target = route.corners.iter().copied().find(|c| c.planar(player.position) > CORNER).unwrap_or(goal);
        let mut remaining = route.length;
        if let Some(jump) = route.jump {
            let to_takeoff = player.position.planar(jump.takeoff);
            if to_takeoff < TAKEOFF || (to_takeoff < BRAKE && (self.ledge_brake || self.still >= HELD)) {
                self.landing = Some(jump.toward);
                if self.ledge_brake {
                    self.phase = Phase::AwaitingJump;
                    return Step::default();
                }
                // Jump toward the landing: the direction held, Jump pressed a
                // few frames; a leader that never leaves the floor walks up to
                // the takeoff again and tries anew, a few times.
                if self.phase != Phase::Leaping {
                    self.phase = Phase::Leaping;
                    self.leap = 0;
                    self.jumps += 1;
                    if self.jumps > JUMPS {
                        return end(Outcome::Stuck);
                    }
                }
                self.leap += 1;
                if self.leap > LEAP_WAIT {
                    self.phase = Phase::Moving;
                }
                let toward = field::heading(jump.toward.x - player.position.x, jump.toward.z - player.position.z);
                let mut actions = dpad(screen_sector(toward, &field, &mut self.sector));
                if self.leap <= LEAP_FRAMES {
                    actions.push(GameAction::Jump);
                }
                return Step { actions, done: None };
            }
            self.phase = Phase::Moving;
            let to_takeoff = route.corners.iter().position(|c| c.planar(jump.takeoff) < 0.5).map_or(route.corners.len(), |i| i + 1);
            target = route.corners[..to_takeoff].iter().copied().find(|c| c.planar(player.position) > CORNER).unwrap_or(jump.takeoff);
            remaining = field::path_length(player.position, &route.corners[..to_takeoff]);
        } else {
            self.phase = Phase::Moving;
        }
        // Progress: the route left must shrink now and then (a player
        // waiting at a takeoff makes none and needs none).
        if self.phase == Phase::Moving {
            // On a slide the game moves the leader (it has no direction
            // there): the progress counted anew once off it.
            if field.sliding(&player) {
                (self.best, self.still) = (f32::INFINITY, 0);
            } else if route.length < self.best - PROGRESS {
                self.best = route.length;
                self.still = 0;
            } else {
                self.still += 1;
                if self.still > STUCK_FRAMES {
                    return end(Outcome::Stuck);
                }
                // Held beside a walk's goal (another actor's body, a wall):
                // as near as it gets.
                if self.still > SIDESTEP_FRAMES && route.jump.is_none() && player.layer == goal_layer
                    && player.position.planar(goal) < NEAR_ENOUGH.max(into.map_or(0.0, |(_, keep)| keep + 8.0))
                    && matches!(self.intent, Intent::Walk { .. })
                {
                    return end(Outcome::Arrived);
                }
            }
        }
        if self.run && remaining < RUN_OFF {
            self.run = false;
        } else if !self.run && remaining > RUN_ON {
            self.run = true;
        }
        // Around an actor in the way: aim round it, on the floor's side
        // nearer the target (not the one the walk goes into).
        let round = detour(&field, &player, target, into.map(|(centre, _)| centre));
        let target = round.map_or(target, |(point, ..)| point);
        // No progress for a while: step aside, then plan again.
        let target = if self.still > SIDESTEP_FRAMES {
            let toward = target.sub_planar(player.position);
            let side = if (self.still / 30) % 2 == 0 { 1.0 } else { -1.0 };
            V3::new(player.position.x - toward.z * side, player.position.y, player.position.z + toward.x * side)
        } else {
            target
        };
        let toward = field::heading(target.x - player.position.x, target.z - player.position.z);
        // Against the actor gone round: of the D-pad's two eighths either
        // side of the way, the one turning away from it (a step into it the
        // game refuses).
        let sector = match round.filter(|(_, centre, keep)| centre.planar(player.position) < keep + 12.0) {
            Some((_, centre, _)) => {
                let dir = field::screen_direction(toward, field.camera.angle, field.alt_dpad);
                let lo = dir.div_euclid(0x200);
                let away = |sector: i32| {
                    let h = (sector * 0x200 - (dir - toward)) as f32 / 4096.0 * std::f32::consts::TAU;
                    h.cos() * (player.position.x - centre.x) - h.sin() * (player.position.z - centre.z)
                };
                let sector = (if away(lo) >= away(lo + 1) { lo } else { lo + 1 }).rem_euclid(8);
                self.sector = Some(sector);
                sector
            }
            None => screen_sector(toward, &field, &mut self.sector),
        };
        let mut actions = dpad(sector);
        if self.run && self.phase == Phase::Moving {
            actions.push(GameAction::Run);
        }
        Step { actions, done: None }
    }
}

impl Assist {
    /// Walking on into a door or a wall beyond the floor arrived at, at a
    /// walk: until there, held still a while (a wall), or long enough; a
    /// door's map change interrupts it first.
    fn press(&mut self, field: &Field, player: &field::Actor, beyond: V3) -> Step {
        self.pressed += 1;
        let left = player.position.planar(beyond);
        if left < self.press_best - 1.0 {
            self.press_best = left;
            self.press_still = 0;
        } else {
            self.press_still += 1;
        }
        if left < ARRIVE || self.press_still > PRESS_STILL || self.pressed > PRESS_FRAMES {
            return Step { actions: vec![], done: Some(Outcome::Arrived) };
        }
        let toward = field::heading(beyond.x - player.position.x, beyond.z - player.position.z);
        Step { actions: dpad(screen_sector(toward, field, &mut self.sector)), done: None }
    }
}

/// A point round the first actor that stands within its keep-out distance
/// of the straight line to `target`, on the walkable side nearer `target`
/// (but the one standing at `into`), and that actor's centre and keep-out.
fn detour(field: &Field, player: &field::Actor, target: V3, into: Option<V3>) -> Option<(V3, V3, f32)> {
    let from = player.position;
    let along = target.sub_planar(from);
    let length = (along.x * along.x + along.z * along.z).sqrt();
    if length < 1.0 {
        return None;
    }
    let (ux, uz) = (along.x / length, along.z / length);
    let mesh = field.layers.get(player.layer)?;
    let mut nearest: Option<(f32, V3, f32)> = None;
    // (A follower is walked through: it keeps to the leader's path.)
    for (centre, keep) in field.in_the_way(player).into_iter().filter(|(centre, _)| into != Some(*centre)) {
        let (cx, cz) = (centre.x - from.x, centre.z - from.z);
        let t = cx * ux + cz * uz;
        let off = (cx * uz - cz * ux).abs();
        if t > 0.0 && t < length + keep && off < keep && nearest.is_none_or(|(d, _, _)| t < d) {
            nearest = Some((t, centre, keep));
        }
    }
    let (_, centre, keep) = nearest?;
    // On round it: a little past where the line from the leader touches the
    // circle kept from it, on either side (from a leader already against it,
    // an eighth of a turn on from where it stands: the game refuses a move
    // into it), each frame anew.
    let around = keep + 6.0;
    let (rx, rz) = (from.x - centre.x, from.z - centre.z);
    let out = (rx * rx + rz * rz).sqrt().max(1e-3);
    let turn = (around / out).min(1.0).acos() + std::f32::consts::FRAC_PI_4;
    let (cx, cz) = (rx / out, rz / out);
    let sides = [1.0f32, -1.0].map(|side| {
        let (c, s) = (turn.cos(), side * turn.sin());
        V3::new(centre.x + (cx * c - cz * s) * around, centre.y, centre.z + (cx * s + cz * c) * around)
    });
    sides.into_iter()
        .filter(|p| mesh.locate(p.x, p.z).is_some())
        .min_by(|a, b| a.planar(target).total_cmp(&b.planar(target)))
        .map(|point| (point, centre, keep))
}

fn map_id(memory: &dyn crate::memory::GameMemory) -> u16 {
    let mut b = [0; 2];
    memory.read(FIELD_MAP_ID, &mut b).map_or(0xFFFF, |()| u16::from_le_bytes(b) & 0x3FFF)
}

/// The D-pad eighth (0 Right .. 6 Up) moving along world heading `h`,
/// keeping `previous` unless the heading has turned well away from it.
fn screen_sector(h: i32, field: &Field, previous: &mut Option<i32>) -> i32 {
    let dir = field::screen_direction(h, field.camera.angle, field.alt_dpad);
    let nearest = ((dir + 0x100) / 0x200).rem_euclid(8);
    let sector = match *previous {
        Some(p) if (((dir - p * 0x200 + 0x800).rem_euclid(0x1000)) - 0x800).abs() < SECTOR_HYSTERESIS => p,
        _ => nearest,
    };
    *previous = Some(sector);
    sector
}

fn dpad(sector: i32) -> Vec<GameAction> {
    let [up, down, left, right] = field::dpad_of_sector(sector);
    [(up, GameAction::Up), (down, GameAction::Down), (left, GameAction::Left), (right, GameAction::Right)]
        .into_iter().filter_map(|(on, a)| on.then_some(a)).collect()
}

/// The pad buttons the assist owns while it runs: the D-pad and Run (the
/// player's other buttons pass).
pub fn owned_buttons(assignment: &Assignment) -> u16 {
    crate::action::buttons_with([GameAction::Up, GameAction::Down, GameAction::Left, GameAction::Right, GameAction::Run], assignment)
}

/// The D-pad's buttons (a player's own direction replaces the assist).
pub fn direction_buttons() -> u16 {
    crate::action::buttons([GameAction::Up, GameAction::Down, GameAction::Left, GameAction::Right])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sectors_keep_their_direction_near_a_boundary() {
        let field = Field {
            camera: field::Camera { m: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]], t: [0.0; 3], h: 256.0, angle: 0 },
            layers: vec![], attributes: vec![], actors: vec![], player: None, alt_dpad: false, party_processing: false, dialogue_open: false,
        };
        let mut previous = None;
        assert_eq!(screen_sector(0x110, &field, &mut previous), 1, "0x110 is nearest the second eighth");
        assert_eq!(screen_sector(0x0E0, &field, &mut previous), 1, "kept across the boundary");
        assert_eq!(screen_sector(0x020, &field, &mut previous), 0, "turned well away");
        assert_eq!(dpad(1), [GameAction::Down, GameAction::Right]);
    }
}
