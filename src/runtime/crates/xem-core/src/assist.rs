//! Assisted movement in a field (plan.md, field traversal and interaction):
//! a committed destination or an actor to approach and talk to, played as
//! the pad input a player would give. Each frame the route is planned again
//! from the game's own state ([`crate::field`]); the D-pad points along it
//! by the camera's angle, Run is held while much of it remains (with a
//! margin, so it does not flicker), and nothing jumps for the player: a
//! route that needs a jump walks to the takeoff and waits there, then holds
//! its direction through the player's jump and plans again after landing.
//! An approach presses Confirm exactly once, only while the player walks
//! within the original's talk reach and facing.
//!
//! The assist ends when it arrives, when the player presses a direction
//! (the player's own input replaces it), when the interface changes (a
//! dialogue, a menu, a battle, another map) or when the player stops making
//! progress.
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
/// Takeoff reached within this.
const TAKEOFF: f32 = 20.0;
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

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum Intent {
    /// Walk to a floor point on the player's layer.
    Walk { to: V3, triangle: usize, layer: usize },
    /// Approach an event actor and Confirm once in reach.
    Interact { actor: usize },
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Phase {
    Moving,
    /// At the takeoff; the jump is the player's.
    AwaitingJump,
    /// In the air on the player's jump.
    Jumping,
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
    /// Stop at a takeoff rather than press on toward the edge.
    pub ledge_brake: bool,
}

/// What one frame of the assist does.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Step {
    pub actions: Vec<GameAction>,
    pub done: Option<Outcome>,
}

impl Assist {
    pub fn new(intent: Intent, memory: &dyn crate::memory::GameMemory, ledge_brake: bool) -> Assist {
        Assist { intent, phase: Phase::Moving, run: false, sector: None, map: map_id(memory), best: f32::INFINITY, still: 0, frames: 0, confirm: 0, ledge_brake }
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
        let (goal, goal_triangle, reach) = match self.intent {
            Intent::Walk { to, triangle, .. } => (to, triangle, ARRIVE),
            Intent::Interact { actor } => {
                let Some(target) = field.actors.get(actor).copied().flatten() else { return end(Outcome::TargetLost) };
                if self.frames > APPROACH_FRAMES {
                    return end(Outcome::TargetLost);
                }
                let mesh = &field.layers[player.layer];
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
                (floor, triangle, ARRIVE)
            }
        };
        if airborne {
            // Through the player's jump: keep heading for the destination.
            self.phase = Phase::Jumping;
            let toward = field::heading(goal.x - player.position.x, goal.z - player.position.z);
            return Step { actions: dpad(screen_sector(toward, &field, &mut self.sector)), done: None };
        }
        if self.phase == Phase::Jumping {
            self.phase = Phase::Moving;
        }
        if player.position.planar(goal) < reach {
            // An approach that arrives out of talk reach found no way in.
            return end(if matches!(self.intent, Intent::Interact { .. }) { Outcome::Unreachable } else { Outcome::Arrived });
        }
        let Some(route) = field.route(&player, goal_triangle, goal) else { return end(Outcome::Unreachable) };
        let mut target = route.corners.iter().copied().find(|c| c.planar(player.position) > CORNER).unwrap_or(goal);
        let mut remaining = route.length;
        if let Some(jump) = route.jump {
            let to_takeoff = player.position.planar(jump.takeoff);
            if to_takeoff < TAKEOFF {
                self.phase = Phase::AwaitingJump;
                if self.ledge_brake {
                    return Step::default();
                }
                target = jump.toward;
            } else {
                self.phase = Phase::Moving;
                target = route.corners.iter().copied().find(|c| c.planar(player.position) > CORNER).unwrap_or(jump.takeoff);
                remaining = field::path_length(player.position, &route.corners);
            }
        } else {
            self.phase = Phase::Moving;
        }
        // Progress: the route left must shrink now and then (a player
        // waiting at a takeoff makes none and needs none).
        if self.phase == Phase::Moving {
            if route.length < self.best - PROGRESS {
                self.best = route.length;
                self.still = 0;
            } else {
                self.still += 1;
                if self.still > STUCK_FRAMES {
                    return end(Outcome::Stuck);
                }
            }
        }
        if self.run && remaining < RUN_OFF {
            self.run = false;
        } else if !self.run && remaining > RUN_ON {
            self.run = true;
        }
        // Around an actor in the way: aim beside it, on the floor's side
        // nearer the target.
        let target = detour(&field, &player, target).unwrap_or(target);
        // No progress for a while: step aside, then plan again.
        let target = if self.still > SIDESTEP_FRAMES {
            let toward = target.sub_planar(player.position);
            let side = if (self.still / 30) % 2 == 0 { 1.0 } else { -1.0 };
            V3::new(player.position.x - toward.z * side, player.position.y, player.position.z + toward.x * side)
        } else {
            target
        };
        let toward = field::heading(target.x - player.position.x, target.z - player.position.z);
        let mut actions = dpad(screen_sector(toward, &field, &mut self.sector));
        if self.run && self.phase == Phase::Moving {
            actions.push(GameAction::Run);
        }
        Step { actions, done: None }
    }
}

/// A point beside the first actor that stands within its keep-out distance
/// of the straight line to `target`, on the walkable side nearer `target`.
fn detour(field: &Field, player: &field::Actor, target: V3) -> Option<V3> {
    let from = player.position;
    let along = target.sub_planar(from);
    let length = (along.x * along.x + along.z * along.z).sqrt();
    if length < 1.0 {
        return None;
    }
    let (ux, uz) = (along.x / length, along.z / length);
    let mesh = field.layers.get(player.layer)?;
    let mut nearest: Option<(f32, V3, f32)> = None;
    for (centre, keep) in field.obstacles(player) {
        let (cx, cz) = (centre.x - from.x, centre.z - from.z);
        let t = cx * ux + cz * uz;
        let off = (cx * uz - cz * ux).abs();
        if t > 0.0 && t < length + keep && off < keep && nearest.is_none_or(|(d, _, _)| t < d) {
            nearest = Some((t, centre, keep));
        }
    }
    let (_, centre, keep) = nearest?;
    let sides = [1.0f32, -1.0].map(|side| V3::new(centre.x + uz * side * (keep + 6.0), centre.y, centre.z - ux * side * (keep + 6.0)));
    sides.into_iter()
        .filter(|p| mesh.locate(p.x, p.z).is_some())
        .min_by(|a, b| a.planar(target).total_cmp(&b.planar(target)))
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
