//! Clicking (or tapping) to walk, on the game's own walkmesh (issue #104;
//! docs/runtime.md, Assisted movement): routes keep to what the leader can
//! walk and jump as the game moves it, the assist jumps where a route needs
//! it and lands where it aimed, a click past the floor walks on into the
//! exit there, and a walk to another actor's feet arrives beside it.
//! Local only: it needs the user's disc 1 and a game module build.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test click_to_move

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::path::PathBuf;
use xem_core::assist::Outcome;
use xem_core::control::Session;
use xem_core::field::{Field, V3, jump_arc};
use xem_game::{NativeModule, boot};

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn run(session: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = session.execute(&command);
    assert!(reply.get("error").is_none(), "{command} -> {reply}");
    reply
}

/// Boots and enters `map`, the player walking.
fn walking(map: u64) -> Option<Session<NativeModule>> {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return None;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": map}));
    for _ in 0..60 {
        run(&mut session, json!({"cmd": "frames", "count": 10}));
        if run(&mut session, json!({"cmd": "context"}))["kind"] == "walk" {
            run(&mut session, json!({"cmd": "frames", "count": 30}));
            return Some(session);
        }
    }
    panic!("map {map}: the player never walked");
}

fn field(session: &mut Session<NativeModule>) -> Field {
    Field::read(session.runtime.memory()).expect("a field")
}

/// Runs the assist under way to its end (at most `frames`), noting whether
/// the leader left the floor: its outcome, and that.
fn until_done(session: &mut Session<NativeModule>, frames: u32) -> (Option<Outcome>, bool) {
    let mut jumped = false;
    for _ in 0..frames {
        if session.assist.is_none() {
            break;
        }
        run(session, json!({"cmd": "frames", "count": 1}));
        jumped |= Field::read(session.runtime.memory()).and_then(|f| f.player().copied()).is_some_and(|p| p.airborne());
    }
    (session.assist_outcome, jumped)
}

/// Every route the planner makes on the mountain path toward Citan's house
/// (map 15, its slopes walled by slides) and in Lahan (map 1) keeps to the
/// game's moves: none climbs higher than a jump rises, each jump lands on
/// a floor its arc carries it to, and none runs at a cliff (a takeoff it
/// walks to stands on its own side of the jump).
#[test]
fn routes_climb_no_higher_than_a_jump_rises_and_land_where_its_arc_carries() {
    for map in [15, 1] {
        let Some(mut session) = walking(map) else { return };
        let field = field(&mut session);
        let player = *field.player().expect("a player");
        let mesh = &field.layers[player.layer];
        let arc = jump_arc(&player);
        let reachable = field.reachable(&player);
        let (mut walks, mut jumps, mut none) = (0, 0, 0);
        for t in 0..mesh.triangles.len() {
            let goal = mesh.centroid(t);
            match field.route(&player, t, goal) {
                None => none += 1,
                Some(route) => match route.jump {
                    None => walks += 1,
                    Some(jump) => {
                        jumps += 1;
                        let rise = jump.takeoff.y - jump.toward.y;
                        let distance = jump.takeoff.planar(jump.toward);
                        assert!(arc.carries(distance, rise), "map {map}: a jump to triangle {t} rises {rise} over {distance}: {jump:?}");
                        assert!(rise < arc.rise(), "map {map}: {jump:?}");
                    }
                },
            }
            if reachable[t] {
                assert!(field.route(&player, t, goal).is_some(), "map {map}: triangle {t} is reachable yet has no route");
            }
        }
        eprintln!("map {map}: {walks} walks, {jumps} jumps, {none} unreachable");
        assert!(walks > 100 && none > 0, "map {map}");
    }
}

/// A route that needs a jump (Lahan's first: up over a slide or across a
/// gap) is walked to its takeoff, jumped by the assist itself and arrives;
/// with the ledge brake on the assist waits at the takeoff instead.
#[test]
fn the_assist_jumps_where_its_route_needs_it_and_waits_under_the_ledge_brake() {
    let Some(mut session) = walking(1) else { return };
    run(&mut session, json!({"cmd": "snapshot", "name": "start"}));
    let field = field(&mut session);
    let player = *field.player().expect("a player");
    let mesh = &field.layers[player.layer];
    // The nearest floor a route reaches by a jump (a floor: no slide,
    // attribute 0x400000, which nothing stands on).
    let (goal, triangle, length) = (0..mesh.triangles.len())
        .filter(|t| field.attributes.get(mesh.triangles[*t].attribute as usize).is_some_and(|a| a & 0x40_0000 == 0))
        .filter_map(|t| {
            let goal = mesh.centroid(t);
            let route = field.route(&player, t, goal)?;
            route.jump.is_some().then_some((goal, t, route.length))
        })
        .min_by(|a, b| a.2.total_cmp(&b.2))
        .expect("a route with a jump");
    eprintln!("to triangle {triangle} at {goal:?}, a route of {length:.0}");
    run(&mut session, json!({"cmd": "navigate", "to": [goal.x, goal.z], "triangle": triangle}));
    let (outcome, jumped) = until_done(&mut session, 1200);
    let at = Field::read(session.runtime.memory()).unwrap().player().unwrap().position;
    eprintln!("outcome {outcome:?}, jumped {jumped}, at {at:?} ({:.0} from the goal)", at.planar(goal));
    assert!(jumped, "the assist jumped");
    assert_eq!(outcome, Some(Outcome::Arrived), "and arrived");
    // Arrived within 10, the leader coasting a step once let go.
    assert!(at.planar(goal) < 24.0 && (at.y - goal.y).abs() < 24.0, "on the floor it aimed for: {at:?}");
    // With the ledge brake the assist stops at the takeoff for the
    // player's own jump.
    run(&mut session, json!({"cmd": "restore", "name": "start"}));
    session.ledge_brake = true;
    run(&mut session, json!({"cmd": "navigate", "to": [goal.x, goal.z], "triangle": triangle}));
    let mut waited = false;
    for _ in 0..1200 {
        run(&mut session, json!({"cmd": "frames", "count": 1}));
        let status = run(&mut session, json!({"cmd": "status"}));
        if status["assist"]["phase"] == "awaiting_jump" {
            waited = true;
            break;
        }
        assert!(!Field::read(session.runtime.memory()).unwrap().player().unwrap().airborne(), "no jump under the ledge brake");
    }
    assert!(waited, "it waits at the takeoff");
}

/// Lahan's south exit: a click past the end of the path there walks to the
/// path's end and on into the exit, which leaves for the world map.
#[test]
fn a_click_past_the_paths_end_walks_on_into_lahans_exit() {
    let Some(mut session) = walking(1) else { return };
    // The floor nearest the exit's zone, and a point past the mesh's edge.
    let field = field(&mut session);
    let player = *field.player().expect("a player");
    let mesh = &field.layers[player.layer];
    let reachable = field.reachable(&player);
    let toward = V3::new(-300.0, 0.0, -2400.0);
    let (t, floor) = (0..mesh.triangles.len())
        .filter(|t| reachable[*t])
        .map(|t| (t, mesh.centroid(t)))
        .min_by(|a, b| a.1.planar(toward).total_cmp(&b.1.planar(toward)))
        .expect("a floor");
    eprintln!("the floor nearest the exit: triangle {t} at {floor:?}");
    run(&mut session, json!({"cmd": "navigate", "to": [floor.x, floor.z], "triangle": t, "beyond": [toward.x, toward.z]}));
    let (outcome, _) = until_done(&mut session, 2400);
    eprintln!("outcome {outcome:?}, context {}", session.execute(&json!({"cmd": "context"})));
    assert_eq!(outcome, Some(Outcome::Interrupted), "the exit's map change ends it");
    for _ in 0..120 {
        run(&mut session, json!({"cmd": "frames", "count": 10}));
        if session.execute(&json!({"cmd": "context"}))["kind"] == "world_map" {
            return;
        }
    }
    panic!("never reached the world map: {}", session.execute(&json!({"cmd": "context"})));
}

/// A walk to another actor's feet (a villager, a barrel) arrives beside it
/// rather than pushing into it until it gives up.
#[test]
fn a_walk_to_another_actors_feet_arrives_beside_it() {
    let Some(mut session) = walking(1) else { return };
    let field = field(&mut session);
    let player = *field.player().expect("a player");
    let mesh = &field.layers[player.layer];
    let reachable = field.reachable(&player);
    // The nearest still actor standing on floor the player reaches, a body
    // (one that does not pass through others: flags bit 7).
    let (actor, triangle) = field.actors.iter().flatten()
        .filter(|a| a.index != player.index && a.flags & 0x81 == 0 && a.layer == player.layer && a.position.planar(player.position) > 2.0)
        .filter_map(|a| Some((a.position, mesh.locate(a.position.x, a.position.z)?)))
        .filter(|(_, t)| reachable[*t])
        .min_by(|a, b| a.0.planar(player.position).total_cmp(&b.0.planar(player.position)))
        .expect("an actor on the player's floor");
    eprintln!("to the feet of the actor at {actor:?}");
    run(&mut session, json!({"cmd": "navigate", "to": [actor.x, actor.z], "triangle": triangle}));
    let (outcome, _) = until_done(&mut session, 1200);
    let at = Field::read(session.runtime.memory()).unwrap().player().unwrap().position;
    eprintln!("outcome {outcome:?} at {at:?}, {:.0} from it", at.planar(actor));
    assert_eq!(outcome, Some(Outcome::Arrived));
    assert!(at.planar(actor) < 60.0, "beside it");
}
