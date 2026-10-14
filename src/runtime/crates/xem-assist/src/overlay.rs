//! What the assisted controls show over the game (as Seraph Glass widgets,
//! `xem_render::glass`), the same in a headset and on a touch screen: the
//! live cursor and the committed destination (related silhouettes, never
//! the same), the region a swipe scrolls, and the keys beside the picture
//! (a shop's quantity keys, a battle's target list). Positions are the
//! game screen's pixels (320x224, with the wider framing's sides); hosts
//! place them on their picture or virtual screen.
use crate::dispatch::Out;
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

/// A mark: flat on the screen at `centre`, or lying on the floor (its
/// half-axes `x` and `z` as they project).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Mark {
    pub kind: Kind,
    pub centre: (f32, f32),
    /// The mark's half-axes on the screen: across and up for flat marks
    /// (`size` pixels), the floor's x and z directions for floor marks.
    pub x: (f32, f32),
    pub y: (f32, f32),
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
    Mark { kind, centre, x: (half.0, 0.0), y: (0.0, half.1) }
}

/// A mark lying on the floor at `point`, as the field camera shows it.
fn floor(kind: Kind, field: &Field, point: V3, radius: f32) -> Option<Mark> {
    let centre = field.camera.project(point)?;
    let along = |dx: f32, dz: f32| field.camera.project(V3::new(point.x + dx, point.y, point.z + dz)).map(|p| (p.0 - centre.0, p.1 - centre.1));
    Some(Mark { kind, centre, x: along(radius, 0.0)?, y: along(0.0, -radius)? })
}

/// The marks for this frame: the cursor where `aim` points, the
/// destination of the assist under way (`destination`), the list a swipe
/// would scroll.
pub fn marks(ui: Context, aim: Option<(f32, f32)>, field: Option<&Field>, destination: Option<V3>, targets: Option<&Targets>) -> Vec<Mark> {
    let mut marks = Vec::new();
    if let Some((x0, y0, x1, y1)) = aim.zip(targets).and_then(|(aim, targets)| crate::dispatch::receiving_region(ui, targets, aim)) {
        let pad = 3.0;
        marks.push(flat(Kind::Region, ((x0 + x1) / 2.0, (y0 + y1) / 2.0), ((x1 - x0) / 2.0 + pad, (y1 - y0) / 2.0 + pad)));
    }
    if let (Some(field), Some(point)) = (field, destination) {
        marks.extend(floor(Kind::Destination, field, point, FLOOR_RADIUS * 1.2));
    }
    match ui {
        Context::Walk => {
            if let Some(aim) = aim {
                let on_floor = field.and_then(|f| f.pick(aim).and_then(|hit| floor(Kind::Floor(hit.reach), f, hit.point, FLOOR_RADIUS)));
                marks.push(on_floor.unwrap_or_else(|| flat(Kind::Cursor, aim, (CURSOR_RADIUS, CURSOR_RADIUS))));
            }
        }
        Context::None => {}
        _ => marks.extend(aim.map(|aim| flat(Kind::Cursor, aim, (CURSOR_RADIUS, CURSOR_RADIUS)))),
    }
    marks
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

/// A key beside the picture: its label, what aiming a tap at it does, and
/// whether it is lit (the target selected now).
#[derive(Clone, Debug, PartialEq)]
pub struct SideKey {
    pub label: String,
    pub out: Out,
    pub lit: bool,
}

/// The keys beside the picture outside a control deck: a shop list's
/// quantity keys, or a battle's targets as a list (slots 0-2 the party,
/// the rest the enemies), for targets that overlap or hide one another.
pub fn side_keys(ui: Context, targets: Option<&Targets>) -> Vec<SideKey> {
    if quantity_keys(ui) {
        return QuantityKey::ALL.iter().enumerate()
            .map(|(i, key)| SideKey { label: key.label().to_string(), out: Out::Key { key: i as u32 }, lit: false }).collect();
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
        let marks = marks(ring, Some((50.0, 60.0)), None, None, None);
        assert_eq!(marks.len(), 1, "the ring's own arrows show its pages: {marks:?}");
        assert_eq!(marks[0].kind, Kind::Cursor);
        assert_eq!(marks[0].centre, (50.0, 60.0));
        assert!(super::marks(ring, None, None, None, None).is_empty(), "nothing over the ring without a pointer");
        assert!(super::marks(Context::None, Some((1.0, 1.0)), None, None, None).is_empty());
        assert!(quantity_keys(Context::Menu { screen: 4, cursor: 2, choice: 0, choices: 0, switch: false, quantities: true }));
    }
}
