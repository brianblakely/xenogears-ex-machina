//! What the assisted controls show over the game (as Seraph Glass widgets,
//! `xem_render::glass`), the same in a headset and on a touch screen: the
//! live cursor and the committed destination (related silhouettes, never
//! the same), the region a swipe scrolls, and the keys beside the picture
//! (a shop's quantity keys, a battle's target list). Positions are the
//! game screen's pixels (320x224, with the wider framing's sides); hosts
//! place them on their picture or virtual screen. Over a diorama, marks on
//! the floor lie on its floor instead, at their points in the game's world.
use crate::dispatch::{Out, Pointed};
use xem_core::action::GameAction;
use xem_core::context::Context;
use xem_core::field::{Field, Reach, V3};
use xem_core::menu::Targets;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Kind {
    /// The 2D cursor (menus, dialogue, battle, anywhere off the floor).
    Cursor,
    /// The floor under the cursor, by how it is reached.
    Floor(Reach),
    /// The committed destination.
    Destination,
    /// The list a swipe up or down scrolls now.
    Region,
}

/// A mark and where it lies.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Mark {
    pub kind: Kind,
    pub place: Place,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Place {
    /// On the screen at `centre`: flat, or lying on the floor as it projects
    /// there. Its half-axes on the screen: across and up for flat marks
    /// (`size` pixels), the floor's x and z directions for floor marks.
    Screen { centre: (f32, f32), x: (f32, f32), y: (f32, f32) },
    /// Lying on a diorama's floor at `point` in the game's world, `radius`
    /// world units across.
    Floor { point: V3, radius: f32 },
}

/// A shop quantity key beside the picture.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum QuantityKey {
    Plus,
    Minus,
    All,
    None,
}

impl QuantityKey {
    pub const ALL: [QuantityKey; 4] = [QuantityKey::Plus, QuantityKey::Minus, QuantityKey::All, QuantityKey::None];
    pub fn op(self) -> &'static str {
        match self {
            QuantityKey::Plus => "plus",
            QuantityKey::Minus => "minus",
            QuantityKey::All => "all",
            QuantityKey::None => "none",
        }
    }
    pub fn label(self) -> &'static str {
        match self {
            QuantityKey::Plus => "+",
            QuantityKey::Minus => "-",
            QuantityKey::All => "ALL",
            QuantityKey::None => "NONE",
        }
    }
}

/// The floor cursor's radius (world units) and the screen cursor's (pixels).
const FLOOR_RADIUS: f32 = 14.0;
const CURSOR_RADIUS: f32 = 9.0;

fn flat(kind: Kind, centre: (f32, f32), half: (f32, f32)) -> Mark {
    Mark { kind, place: Place::Screen { centre, x: (half.0, 0.0), y: (0.0, half.1) } }
}

/// A mark lying on the floor at `point`: on a diorama's floor (`diorama`),
/// else as the field camera shows it.
fn floor(kind: Kind, field: &Field, point: V3, radius: f32, diorama: bool) -> Option<Mark> {
    if diorama {
        return Some(Mark { kind, place: Place::Floor { point, radius } });
    }
    let centre = field.camera.project(point)?;
    let along = |dx: f32, dz: f32| field.camera.project(V3::new(point.x + dx, point.y, point.z + dz)).map(|p| (p.0 - centre.0, p.1 - centre.1));
    Some(Mark { kind, place: Place::Screen { centre, x: along(radius, 0.0)?, y: along(0.0, -radius)? } })
}

/// The marks for this frame: the cursor where `aim` points, the
/// destination of the assist under way (`destination`), the list a swipe
/// would scroll. With `diorama` (a diorama shows, the screen's pixels its
/// pane's), floor marks lie on its floor; a pointer on the pane shows a
/// flat cursor there, one through the table the floor it meets (and none
/// where it meets no floor).
pub fn marks(ui: Context, aim: Option<Pointed>, field: Option<&Field>, destination: Option<V3>, targets: Option<&Targets>, diorama: bool) -> Vec<Mark> {
    let mut marks = Vec::new();
    let at = aim.and_then(Pointed::screen);
    if let Some((x0, y0, x1, y1)) = at.zip(targets).and_then(|(at, targets)| crate::dispatch::receiving_region(ui, targets, at)) {
        let pad = 3.0;
        marks.push(flat(Kind::Region, ((x0 + x1) / 2.0, (y0 + y1) / 2.0), ((x1 - x0) / 2.0 + pad, (y1 - y0) / 2.0 + pad)));
    }
    if let (Some(field), Some(point)) = (field, destination) {
        marks.extend(floor(Kind::Destination, field, point, FLOOR_RADIUS * 1.2, diorama));
    }
    let cursor = |at: (f32, f32)| flat(Kind::Cursor, at, (CURSOR_RADIUS, CURSOR_RADIUS));
    match (ui, aim) {
        (_, None) | (Context::None, _) => {}
        (Context::Walk, Some(Pointed::Screen(at))) => {
            let on_floor = field.and_then(|f| f.pick(at).and_then(|hit| floor(Kind::Floor(hit.reach), f, hit.point, FLOOR_RADIUS, diorama)));
            marks.push(on_floor.unwrap_or_else(|| cursor(at)));
        }
        (Context::Walk, Some(Pointed::World(ray))) => {
            let hit = field.and_then(|f| f.pick_ray(ray.origin, ray.direction, ray.within).map(|hit| (f, hit)));
            marks.extend(hit.and_then(|(f, hit)| floor(Kind::Floor(hit.reach), f, hit.point, FLOOR_RADIUS, true)));
        }
        (_, Some(Pointed::World(_))) => {}
        (_, Some(Pointed::Screen(at) | Pointed::Pane(at))) => marks.push(cursor(at)),
    }
    marks
}

/// The field as a picture shows it: its camera seen from the picture's
/// viewpoint (`GameView::viewpoint`) when that is not the game camera's
/// own (the camera pulled back), so that a pinch, a tap or a click picks
/// the floor and the actors the picture shows where it points, and floor
/// marks lie where it shows that floor.
pub fn seen(mut field: Field, viewpoint: Option<&xem_render::ps1::Viewpoint>) -> Field {
    if let Some(v) = viewpoint {
        field.camera = field.camera.seen(v.rows, v.offset, v.h, v.centre);
    }
    field
}

/// The pointer's keys (left of the screen) for hands without the thumb's
/// microgestures: what the swipes and taps would press.
pub const POINTER_KEYS: [(&str, GameAction); 10] = [
    ("^", GameAction::Up),
    ("v", GameAction::Down),
    ("<", GameAction::Left),
    (">", GameAction::Right),
    ("OK", GameAction::Confirm),
    ("BACK", GameAction::Cancel),
    ("MENU", GameAction::Menu),
    ("JUMP", GameAction::Jump),
    ("CAM L", GameAction::CameraLeft),
    ("CAM R", GameAction::CameraRight),
];

/// Whether the shop's quantity keys show.
pub fn quantity_keys(ui: Context) -> bool {
    matches!(ui, Context::Menu { quantities: true, .. })
}

/// Whether AUTO shows (auto-advance dialogue on or off): in a field's
/// scenes and its dialogue, where pages wait.
pub fn auto_key(ui: Context) -> bool {
    matches!(ui, Context::Event { .. } | Context::Dialogue { .. })
}

/// A key beside the picture: its label, what aiming a tap at it does, and
/// whether it is lit (the target selected now).
#[derive(Clone, Debug, PartialEq)]
pub struct SideKey {
    pub label: String,
    pub out: Out,
    pub lit: bool,
}

/// The keys beside the picture outside a control deck: a shop list's
/// quantity keys, AUTO in a field's scenes and dialogue (lit while
/// `auto_advance` is on), or a battle's targets as a list (slots 0-2 the
/// party, the rest the enemies), for targets that overlap or hide one
/// another.
pub fn side_keys(ui: Context, targets: Option<&Targets>, auto_advance: bool) -> Vec<SideKey> {
    if quantity_keys(ui) {
        return QuantityKey::ALL.iter().enumerate()
            .map(|(i, key)| SideKey { label: key.label().to_string(), out: Out::Key { key: i as u32 }, lit: false }).collect();
    }
    if auto_key(ui) {
        return vec![SideKey { label: "AUTO".into(), out: Out::ToggleAutoAdvance, lit: auto_advance }];
    }
    let Some(targets) = targets.filter(|t| t.what == "battle_targets") else { return vec![] };
    let (mut ally, mut foe) = (0, 0);
    targets.entries.iter().enumerate().filter_map(|(index, entry)| {
        let ground = entry.ground?;
        let label = if ground.slot < 3 { ally += 1; format!("ALLY {ally}") } else { foe += 1; format!("FOE {foe}") };
        ground.selectable.then(|| SideKey { label, out: Out::Select { index, confirm: true }, lit: targets.selected == Some(index) })
    }).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_ring_and_menus_show_only_a_flat_cursor() {
        let ring = Context::Battle { page: 9, attack: false, ap: 6, list: false, targeting: false };
        let marks = marks(ring, Some(Pointed::Screen((50.0, 60.0))), None, None, None, false);
        assert_eq!(marks.len(), 1, "the ring's own arrows show its pages: {marks:?}");
        assert_eq!(marks[0].kind, Kind::Cursor);
        assert!(matches!(marks[0].place, Place::Screen { centre: (50.0, 60.0), .. }));
        assert!(super::marks(ring, None, None, None, None, false).is_empty(), "nothing over the ring without a pointer");
        assert!(super::marks(Context::None, Some(Pointed::Screen((1.0, 1.0))), None, None, None, false).is_empty());
        assert!(quantity_keys(Context::Menu { screen: 4, cursor: 2, choice: 0, choices: 0, commands: false, open: true, switch: false, quantities: true }));
    }

    /// A flat floor 400 units across under a camera looking along +z.
    fn floor_field() -> Field {
        let square = xem_core::field::Layer {
            vertices: vec![V3::new(-200.0, 0.0, 0.0), V3::new(200.0, 0.0, 0.0), V3::new(-200.0, 0.0, 400.0), V3::new(200.0, 0.0, 400.0)],
            triangles: vec![
                xem_core::field::Triangle { v: [0, 2, 1], n: [-1, 1, -1], attribute: 0 },
                xem_core::field::Triangle { v: [2, 3, 1], n: [-1, -1, 0], attribute: 0 },
            ],
        };
        let actor = |index: usize, z: f32| xem_core::field::Actor {
            index, position: V3::new(0.0, 0.0, z), layer: 0, triangle: [0; 4], flags: 0, layer_flags: 0, radius: 16, height: 60, heading: 0, heading_goal: 0,
        };
        Field {
            camera: xem_core::field::Camera { m: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]], t: [0.0, -100.0, 0.0], h: 256.0, angle: 0 },
            layers: vec![square], attributes: vec![0], actors: vec![Some(actor(0, 20.0)), Some(actor(1, 300.0))], player: Some(0),
            alt_dpad: false, party_processing: false, dialogue_open: false,
        }
    }

    #[test]
    fn over_a_diorama_floor_marks_lie_at_their_world_points_and_the_pane_shows_a_cursor() {
        let field = floor_field();
        let destination = V3::new(50.0, 0.0, 380.0);
        // Down through the table onto the floor at (-40, 0, 150).
        let ray = crate::dispatch::WorldRay { origin: V3::new(-40.0, -300.0, 150.0), direction: V3::new(0.0, 100.0, 0.0), within: 5.0, reach: 10.0 };
        let marks = marks(Context::Walk, Some(Pointed::World(ray)), Some(&field), Some(destination), None, true);
        assert_eq!(marks.len(), 2, "{marks:?}");
        assert_eq!(marks[0], Mark { kind: Kind::Destination, place: Place::Floor { point: destination, radius: FLOOR_RADIUS * 1.2 } });
        assert!(matches!(marks[1], Mark { kind: Kind::Floor(Reach::Walk), place: Place::Floor { point, .. } } if point.planar(V3::new(-40.0, 0.0, 150.0)) < 0.5), "{marks:?}");
        // A ray meeting no floor shows nothing; one on the pane, its cursor.
        let short = crate::dispatch::WorldRay { within: 2.0, ..ray };
        assert_eq!(super::marks(Context::Walk, Some(Pointed::World(short)), Some(&field), None, None, true), []);
        let pane = super::marks(Context::Walk, Some(Pointed::Pane((100.0, 40.0))), Some(&field), None, None, true);
        assert!(matches!(pane[..], [Mark { kind: Kind::Cursor, place: Place::Screen { centre: (100.0, 40.0), .. } }]), "{pane:?}");
        // On the screen, floor marks lie as the camera shows them.
        let screen = super::marks(Context::Walk, None, Some(&field), Some(destination), None, false);
        assert!(matches!(screen[..], [Mark { kind: Kind::Destination, place: Place::Screen { .. } }]), "{screen:?}");
    }
}
