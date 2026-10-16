//! The assisted controls' marks and touch controls as Seraph Glass widgets
//! (`xem_render::glass`), for every host that draws them.
use crate::headset::{Pose, Quad, Screen, Seat};
use crate::overlay::{Kind, Mark, Place};
use crate::touch::Rect;
use xem_render::glam::{Mat4, Quat, Vec2, Vec3};
use xem_render::glass::{Icon, State, Widget};
use xem_render::ps1::{DioramaSpace, Table};

/// The widgets' texture size in pixels.
pub const WIDGET_PIXELS: u32 = 128;
/// The keys beside a headset's screen (their scale 1, [`Screen::keys`]):
/// size in metres and the gap.
pub const KEY_SIZE: (f32, f32) = (0.22, 0.11);
const KEY_GAP: f32 = 0.04;
/// The distance (metres) at which keys of scale 1 stand beside the default
/// screen: keys elsewhere take the scale that keeps their size as seen from
/// the seat ([`key_scale`]).
pub const KEY_DISTANCE: f32 = 2.5;

/// The scale of keys beside a quad `distance` metres from the seat that
/// look as large as the screen's own keys do from there.
pub fn key_scale(distance: f32) -> f32 {
    distance / KEY_DISTANCE
}

/// Where a headset's HUD (the frame counter, playback activity and state
/// notices: `hud_px` physical pixels at `scale_factor` per logical pixel)
/// stands on its screen: inside the top left corner of `anchor` (the
/// screen's pose, centred, and its size in metres), as large against it as
/// against the default screen, 2 mm in front. Its pose and size.
pub fn hud(anchor: Mat4, anchor_size: Vec2, hud_px: (u32, u32), scale_factor: f32) -> (Mat4, Vec2) {
    let unit = anchor_size.x / 640.0 / scale_factor;
    let size = Vec2::new(hud_px.0 as f32 * unit, hud_px.1 as f32 * unit);
    let margin = anchor_size.x / 80.0;
    let at = Vec3::new(-anchor_size.x / 2.0 + size.x / 2.0 + margin, anchor_size.y / 2.0 - size.y / 2.0 - margin, 0.002);
    (anchor * Mat4::from_translation(at), size)
}

/// The default screen (`xem_settings::XrScreen::default()`): its width and
/// its distance ahead of the seat (metres).
const DEFAULT_SCREEN: (f32, f32) = (3.0, 2.5);

/// Where a headset's HUD ([`hud`]) stands beside a diorama (`space`: its
/// pane, and the seat the table was placed from): inside the pane's top left
/// corner, as large seen from the seat as the HUD of the default screen
/// showing the same picture is from there, wherever the table puts the
/// pane; raised to stand on the pane's lower edge where it is taller than
/// the pane (that edge is above the table's volume: the HUD stays outside
/// it); 2 mm in front. Its pose and size.
pub fn pane_hud(space: &DioramaSpace, hud_px: (u32, u32), scale_factor: f32) -> (Mat4, Vec2) {
    let (pane, pane_size) = (space.pane, space.pane_size);
    let seat = space.seat.w_axis.truncate();
    // The default screen's HUD as seen from its seat: its size over its
    // distance.
    let screen = Vec2::new(DEFAULT_SCREEN.0, DEFAULT_SCREEN.0 * pane_size.y / pane_size.x.max(f32::EPSILON));
    let (on_screen, screen_size) = hud(Mat4::from_translation(Vec3::new(0.0, 0.0, -DEFAULT_SCREEN.1)), screen, hud_px, scale_factor);
    let seen = screen_size / on_screen.w_axis.truncate().length();
    let margin = pane_size.x / 80.0;
    let place = |size: Vec2| {
        let top = pane_size.y / 2.0 - size.y / 2.0 - margin;
        let y = top.max(size.y / 2.0 - pane_size.y / 2.0);
        pane * Mat4::from_translation(Vec3::new(-pane_size.x / 2.0 + size.x / 2.0 + margin, y, 0.002))
    };
    // Its size follows its distance, which its size moves a little: a few
    // steps settle it.
    let mut size = seen * pane.w_axis.truncate().distance(seat);
    for _ in 0..4 {
        size = seen * place(size).w_axis.truncate().distance(seat);
    }
    (place(size), size)
}

/// The settings panel's width (metres) below a headset's screen, how far
/// its middle stands from the seat, and how far (degrees, as seen from the
/// seat) its top edge's middle stands below the screen's lower edge (and
/// its edge beside the keys right of a diorama's pane).
pub const PANEL_WIDTH: f32 = 0.55;
const PANEL_DISTANCE: f32 = 1.33;
const PANEL_GAP_DEG: f32 = 4.0;

/// What a headset's settings panel stands beside: the screen (its size,
/// metres, as two-hand scaling has made it, and how far ahead of the seat
/// it stands) or the diorama's table.
#[derive(Clone, Copy, Debug)]
pub enum Beside<'a> {
    Screen { size: (f32, f32), distance: f32 },
    Table(&'a Table),
}

/// Where a headset's settings panel stands in front of `seat`, `aspect` its
/// height over its width: below the screen, facing the seat from
/// `PANEL_DISTANCE`, its top edge `PANEL_GAP_DEG` below the screen's
/// lower edge as seen from the seat, so that it stays off the screen
/// whatever the screen's shape, size and scale (the larger the screen, the
/// lower the panel, its lower edge at most straight down); with the
/// diorama as the view (`table`), beside the diorama's pane on its right:
/// its inner edge in the pane's plane beyond the column of keys beside the
/// pane ([`side_keys`]), `PANEL_GAP_DEG` further round as seen from the
/// seat, its lower edge on the pane's (above the table's volume, as the
/// pane: outside it whatever the table's height, distance and tilt),
/// upright and turned about that edge to face the seat, as large seen from
/// the seat as below the screen. Seen from the seat all of it lies further
/// round than the pane and its keys: the game's interface stays in view
/// while the panel shows.
pub fn panel(seat: &Seat, beside: Beside, aspect: f32) -> Quad {
    match beside {
        Beside::Screen { size, distance } => {
            let height = PANEL_WIDTH * aspect;
            let half = (height / 2.0 / PANEL_DISTANCE).atan();
            let edge = (size.1 / 2.0 / distance.max(0.01)).atan();
            let down = (edge + PANEL_GAP_DEG.to_radians() + half).min(std::f32::consts::FRAC_PI_2 - half);
            let local = Pose { position: Vec3::new(0.0, -down.sin(), -down.cos()) * PANEL_DISTANCE, orientation: Quat::from_rotation_x(-down) };
            Quad { pose: seat.place(local), size: (PANEL_WIDTH, height) }
        }
        Beside::Table(table) => {
            // In the seat's space: the pane's lower edge's middle (straight
            // ahead, the pane across the seat's way) and its half-width
            // (whatever the picture's shape).
            let lower = (seat.pose.inverse() * table.above(seat.pose)).w_axis.truncate();
            let half_pane = table.pane(seat.pose, 0.0).1.x / 2.0;
            let ahead = (-lower.z).max(0.01);
            // The keys right of the pane stand in its plane at the scale its
            // right edge's middle gives them ([`Screen::pane`]), at most the
            // scale of the farther of its ends for a picture no taller than
            // wide: their column's outer edge, and the panel's inner edge
            // beyond it in that plane, `PANEL_GAP_DEG` further round.
            let edge = |height: f32| Vec3::new(half_pane, lower.y + height / 2.0, lower.z).length();
            let column = half_pane + (KEY_GAP + KEY_SIZE.0) * key_scale(edge(0.0).max(edge(2.0 * half_pane)));
            let inner = Vec3::new(ahead * (column.atan2(ahead) + PANEL_GAP_DEG.to_radians()).tan(), lower.y, lower.z);
            // Turned about that edge to face the seat (its right `turn` from
            // the pane's: the seat on its normal through its middle), so that
            // all of it lies further round than the keys; as wide, seen from
            // the seat at its middle, as below the screen (its middle moves
            // with its size: a few steps settle it).
            let place = |width: f32| {
                let turn = inner.x.atan2(ahead) + (width / 2.0 / inner.x.hypot(ahead)).min(1.0).asin();
                (turn, inner + Vec3::new(turn.cos(), 0.0, turn.sin()) * (width / 2.0) + Vec3::Y * (width * aspect / 2.0))
            };
            let seen = PANEL_WIDTH / PANEL_DISTANCE;
            let mut width = seen * inner.length();
            for _ in 0..6 {
                width = seen * place(width).1.length();
            }
            let (turn, middle) = place(width);
            Quad { pose: seat.place(Pose { position: middle, orientation: Quat::from_rotation_y(-turn) }), size: (width, width * aspect) }
        }
    }
}

/// `count` keys' quads in a column beside the screen's right edge, the
/// first at the top, at the screen's key scale: centred on its middle, and
/// raised to stand on its lower edge where the column is taller than the
/// screen (a diorama's pane, whose lower edge is above the table's volume).
pub fn side_keys(screen: &Screen, count: usize) -> Vec<(u32, Quad)> {
    let right = screen.quad.pose.orientation * Vec3::X;
    let up = screen.quad.pose.orientation * Vec3::Y;
    let (size, gap) = ((KEY_SIZE.0 * screen.keys, KEY_SIZE.1 * screen.keys), KEY_GAP * screen.keys);
    let middle = (count as f32 - 1.0) / 2.0;
    let column = count as f32 * size.1 + (count as f32 - 1.0).max(0.0) * gap;
    let raised = (column - screen.quad.size.1).max(0.0) / 2.0;
    (0..count).map(|i| {
        let position = screen.quad.pose.position + right * (screen.quad.size.0 / 2.0 + gap + size.0 / 2.0)
            + up * ((middle - i as f32) * (size.1 + gap) + raised);
        (i as u32, Quad { pose: Pose { position, orientation: screen.quad.pose.orientation }, size })
    }).collect()
}

/// `count` keys' quads in a column beside the screen's left edge, the
/// first at the top.
pub fn pad_keys(screen: &Screen, count: usize) -> Vec<(u32, Quad)> {
    let right = screen.quad.pose.orientation * Vec3::X;
    side_keys(screen, count).into_iter().map(|(i, mut quad)| {
        let offset = (quad.pose.position - screen.quad.pose.position).dot(right);
        quad.pose.position -= right * (2.0 * offset);
        (i, quad)
    }).collect()
}

/// A key to draw beside the screen: its label and state.
#[derive(Clone, Debug, PartialEq)]
pub struct KeyMark {
    pub label: String,
    pub state: State,
}

/// A widget at a room pose (`size` metres across its square image).
pub struct Placed {
    pub widget: Widget,
    pub pose: Mat4,
    pub size: Vec2,
}

/// How far a mark lying on a diorama's floor stands above it (metres).
const FLOOR_LIFT: f32 = 0.003;

/// Marks on a headset's screen (floor marks lie as the floor projects;
/// over a diorama, on its floor), the keys beside it on the right and the
/// pointer's keys on the left.
pub fn room(marks: &[Mark], screen: &Screen, keys: &[KeyMark], pad: &[KeyMark]) -> Vec<Placed> {
    let normal = screen.quad.pose.orientation * Vec3::Z;
    let mut placed: Vec<Placed> = marks.iter().filter_map(|mark| {
        let pose = match mark.place {
            Place::Screen { centre, x, y } => {
                let at = screen.point(centre);
                let x = screen.point((centre.0 + x.0, centre.1 + x.1)) - at;
                let y = screen.point((centre.0 + y.0, centre.1 + y.1)) - at;
                let lift = if matches!(mark.kind, Kind::Cursor) { 0.012 } else { 0.006 };
                Mat4::from_cols((x * 2.0).extend(0.0), (-y * 2.0).extend(0.0), normal.extend(0.0), (at + normal * lift).extend(1.0))
            }
            Place::Floor { point, radius } => {
                // Across the world's x and z as on the screen (its right
                // along the world's x, its top along its z), facing up, a
                // few millimetres over the floor.
                let room = screen.diorama?.room_from_world;
                let at = room.transform_point3(Vec3::new(point.x, point.y, point.z));
                let x = room.transform_vector3(Vec3::new(radius, 0.0, 0.0));
                let z = room.transform_vector3(Vec3::new(0.0, 0.0, radius));
                let up = room.transform_vector3(Vec3::NEG_Y).normalize_or_zero();
                Mat4::from_cols((x * 2.0).extend(0.0), (z * 2.0).extend(0.0), up.extend(0.0), (at + up * FLOOR_LIFT).extend(1.0))
            }
        };
        Some(Placed { widget: mark_widget(mark.kind), pose, size: Vec2::ONE })
    }).collect();
    let columns = side_keys(screen, keys.len()).into_iter().zip(keys).chain(pad_keys(screen, pad.len()).into_iter().zip(pad));
    for ((_, quad), key) in columns {
        let icon = icon_of(&key.label);
        placed.push(Placed {
            widget: Widget::Key(icon, key.state),
            pose: Mat4::from_rotation_translation(quad.pose.orientation, quad.pose.position + normal * 0.002),
            // The key image is square; the key fills its middle half.
            size: Vec2::new(quad.size.0, quad.size.0),
        });
    }
    placed
}

/// The glass widget of an assisted-controls mark.
pub fn mark_widget(kind: crate::overlay::Kind) -> xem_render::glass::Widget {
    use xem_render::glass::{Reach, Widget};
    match kind {
        crate::overlay::Kind::Cursor => Widget::Cursor,
        crate::overlay::Kind::Floor(reach) => Widget::Floor(match reach {
            xem_core::field::Reach::Walk => Reach::Walk,
            xem_core::field::Reach::Jump => Reach::Jump,
            xem_core::field::Reach::Invalid => Reach::Invalid,
        }),
        crate::overlay::Kind::Destination => Widget::Destination,
        crate::overlay::Kind::Region => Widget::Region,
    }
}

/// A control's symbol from its label: the arrows and the face buttons'
/// shapes by name, else the text.
fn icon_of(label: &str) -> Icon {
    use xem_render::glass::Dir;
    match label {
        "^" => Icon::Arrow(Dir::Up),
        "v" => Icon::Arrow(Dir::Down),
        "<" => Icon::Arrow(Dir::Left),
        ">" => Icon::Arrow(Dir::Right),
        "triangle" => Icon::Triangle,
        "circle" => Icon::Circle,
        "cross" => Icon::Cross,
        "square" => Icon::Square,
        "+" => Icon::Plus,
        "-" => Icon::Minus,
        text => Icon::Text(text.into()),
    }
}

/// A touch control's state: pressed while a finger holds it, AUTO lit
/// while auto-advance dialogue is on.
pub fn deck_state(control: crate::touch::Control, held: bool, auto_advance: bool) -> State {
    if held {
        State::Pressed
    } else if control == crate::touch::Control::Auto && auto_advance {
        State::Active
    } else {
        State::Normal
    }
}

/// A touch control's widget, and the square it is drawn in (wide keys
/// fill the middle of their square image).
pub fn deck_widget(control: crate::touch::Control, label: &str, state: xem_render::glass::State, rect: Rect) -> (xem_render::glass::Widget, Rect) {
    use crate::touch::Control;
    use xem_render::glass::Widget;
    let icon = icon_of(label);
    let round = matches!(control, Control::Up | Control::Down | Control::Left | Control::Right | Control::Jump | Control::Cancel | Control::Confirm | Control::Menu);
    if round {
        (Widget::Button(icon, state), Rect { x: rect.x, y: rect.y, width: rect.width, height: rect.height })
    } else {
        let side = rect.width.max(rect.height * 2.0);
        let (cx, cy) = rect.centre();
        (Widget::Key(icon, state), Rect { x: cx - side / 2.0, y: cy - side / 2.0, width: side, height: side })
    }
}


#[cfg(test)]
mod tests {
    use super::*;
    use crate::headset::tests::diorama;
    use xem_render::ps1::Turns;

    /// The corners of `quad` in the table's space.
    fn corners(quad: &Quad, table_from_room: Mat4) -> [Vec3; 4] {
        let (right, up) = (quad.pose.orientation * Vec3::X * quad.size.0 / 2.0, quad.pose.orientation * Vec3::Y * quad.size.1 / 2.0);
        [right + up, right - up, -right + up, -right - up].map(|d| table_from_room.transform_point3(quad.pose.position + d))
    }

    fn outside(quad: &Quad, table_from_room: Mat4) -> bool {
        let (min, max) = Table::VOLUME;
        corners(quad, table_from_room).iter().all(|p| p.cmplt(min).any() || p.cmpgt(max).any())
    }

    /// How tall `quad` looks from `seat` (radians).
    fn angular_height(quad: &Quad, seat: Vec3) -> f32 {
        quad.size.1 / quad.pose.position.distance(seat)
    }

    #[test]
    fn keys_the_hud_and_the_panel_stand_outside_the_diorama_as_large_as_beside_the_screen_at_every_scale_and_place() {
        // Keys of scale 1 as far as the default screen, as seen from the seat
        // (the screen's own, off its middle, look a little smaller).
        let beside_screen = KEY_SIZE.1 / KEY_DISTANCE;
        // The HUD (the frame counter's 304x110 logical pixels at twice that)
        // on the default screen (3 m wide, 2.5 m ahead) showing the picture
        // the pane shows (320x224), as seen from the seat.
        let (screen_hud, screen_hud_size) = hud(Mat4::from_translation(Vec3::new(0.0, 0.0, -2.5)), Vec2::new(3.0, 3.0 * 224.0 / 320.0), (608, 220), 2.0);
        let hud_on_screen = screen_hud_size.y / screen_hud.w_axis.truncate().length();
        let mut hud_spread = (f32::MAX, 0.0f32);
        let seats = [Mat4::IDENTITY, Mat4::from_rotation_translation(Quat::from_rotation_y(0.8), Vec3::new(0.3, 0.1, -0.2))];
        let mut sizes = Vec::new();
        let mut spread = (f32::MAX, 0.0f32);
        for (n, seat) in seats.into_iter().enumerate() {
            let at = seat.w_axis.truncate();
            for height in [-1.0, -0.45, 0.3] {
                for distance in [0.3, 0.6, 2.0] {
                    for tilt in [0.0f32, 20.0, 45.0] {
                        // Every scale of the setting's range (25-400%) in steps
                        // of 25%, as two-hand scaling passes through them.
                        for scale in (1..=16).map(|step| step as f32 * 0.25) {
                            let table = Table { scale, height, distance, tilt: tilt.to_radians(), turns: Turns::Smooth };
                            let space = diorama(&table, seat);
                            let pane = Screen::pane(space, at);
                            let table_from_room = space.table_from_room();
                            let case = format!("height {height}, distance {distance}, tilt {tilt}, scale {scale}");
                            assert!(outside(&pane.quad, table_from_room), "{case}: the pane");
                            for count in 1..=10 {
                                for (_, key) in side_keys(&pane, count).iter().chain(pad_keys(&pane, count).iter()) {
                                    assert!(outside(key, table_from_room), "{case}: {count} keys, {key:?}");
                                    // About as large as beside the screen (whose own
                                    // keys, off its middle, look 0.81-0.83 as large):
                                    // the column spreads about the pane's edge, most
                                    // where ten keys rise over a table above the eye.
                                    let size = angular_height(key, at) / beside_screen;
                                    spread = (spread.0.min(size), spread.1.max(size));
                                    assert!((0.7..1.3).contains(&size), "{case}, {count} keys: {size} as large as beside the screen");
                                }
                            }
                            // The HUD in the pane's corner, as large seen from the
                            // seat as on the default screen.
                            let (hud_pose, hud_size) = pane_hud(&space, (608, 220), 2.0);
                            let (_, orientation, position) = hud_pose.to_scale_rotation_translation();
                            let hud = Quad { pose: Pose { position, orientation }, size: (hud_size.x, hud_size.y) };
                            assert!(outside(&hud, table_from_room), "{case}: the HUD");
                            let size = angular_height(&hud, at) / hud_on_screen;
                            hud_spread = (hud_spread.0.min(size), hud_spread.1.max(size));
                            assert!((size - 1.0).abs() < 0.01, "{case}: the HUD {size} as large as on the default screen");
                            let along = |axis: Vec3| (hud.pose.orientation * axis).dot(pane.quad.pose.orientation * axis);
                            assert!(along(Vec3::X) > 0.9999 && along(Vec3::Z) > 0.9999, "{case}: the HUD lies on the pane");
                            // Everything beside the pane, laid out at this scale.
                            let beside: Vec<Quad> = side_keys(&pane, 10).into_iter().chain(pad_keys(&pane, 10)).map(|(_, key)| key).chain([hud]).collect();
                            sizes.push(((n, height, distance, tilt), scale, beside, pane.quad));
                            let panel = panel(&Seat { pose: seat }, Beside::Table(&table), 1.27);
                            assert!(outside(&panel, table_from_room), "{case}: the panel");
                            let below_screen = panel_width_seen(&super::panel(&Seat { pose: seat }, Beside::Screen { size: (3.0, 2.1), distance: 2.5 }, 1.27), at);
                            assert!((panel_width_seen(&panel, at) / below_screen - 1.0).abs() < 0.01, "{case}: the panel as large as below the screen");
                        }
                    }
                }
            }
        }
        eprintln!("keys beside the pane look {:.2}-{:.2} as large as beside the screen, the HUD {:.4}-{:.4} as on the default screen", spread.0, spread.1, hud_spread.0, hud_spread.1);
        // The player's scale changes nothing beside the table: the pane, its
        // keys and the HUD stand where and as large as they do at 100%.
        for (place, scale, beside, pane) in &sizes {
            let (_, _, at_one, pane_at_one) = sizes.iter().find(|(p, s, _, _)| p == place && *s == 1.0).unwrap();
            assert_eq!((beside, pane), (at_one, pane_at_one), "{place:?} at scale {scale}");
        }
    }

    /// How wide `quad` looks from `seat` at its middle (radians).
    fn panel_width_seen(quad: &Quad, seat: Vec3) -> f32 {
        quad.size.0 / quad.pose.position.distance(seat)
    }

    #[test]
    fn marks_on_a_diorama_floor_lie_on_it_at_their_world_points() {
        let table = Table::default();
        let space = diorama(&table, Mat4::IDENTITY);
        let pane = Screen::pane(space, Vec3::ZERO);
        let point = xem_core::field::V3::new(100.0, 0.0, 1300.0);
        let marks = [Mark { kind: Kind::Destination, place: Place::Floor { point, radius: 20.0 } }];
        let placed = room(&marks, &pane, &[], &[]);
        assert_eq!(placed.len(), 1);
        let pose = placed[0].pose;
        let floor = space.room_from_world.transform_point3(Vec3::new(100.0, 0.0, 1300.0));
        let up = space.room_from_table.transform_vector3(Vec3::Y).normalize();
        assert!((pose.w_axis.truncate() - floor - up * FLOOR_LIFT).length() < 1e-5, "{pose:?}");
        assert!((pose.z_axis.truncate() - up).length() < 1e-5, "facing up from the floor");
        let across = pose.x_axis.truncate().length();
        assert!((across - 40.0 * space.scale).abs() < 1e-6, "twice the radius across, as the table scales the world: {across}");
        // Not mirrored: right-handed about the floor's up, as on the screen,
        // its top along the world's z, which the level camera looks along
        // (away from the seat: a Jump mark's chevron points where it does on
        // the screen).
        let (right, top) = (pose.x_axis.truncate(), pose.y_axis.truncate());
        assert!(right.cross(top).normalize().dot(up) > 0.999, "facing up, not mirrored: {pose:?}");
        assert!(top.normalize().dot(Vec3::NEG_Z) > 0.999, "its top away from the seat: {top}");
        // Without a diorama a floor mark has nowhere to lie.
        assert!(room(&marks, &Screen { diorama: None, ..pane }, &[], &[]).is_empty());
    }

    /// The corners of `quad` in the room, in order round it.
    fn round(quad: &Quad) -> [Vec3; 4] {
        let (right, up) = (quad.pose.orientation * Vec3::X * quad.size.0 / 2.0, quad.pose.orientation * Vec3::Y * quad.size.1 / 2.0);
        [right + up, right - up, -right - up, -right + up].map(|d| quad.pose.position + d)
    }

    /// How far apart (radians) two quads look from `eye`: the widest gap a
    /// plane through the eye and an edge of either leaves between them (a
    /// gap between two convex shapes seen from a point is one such plane's),
    /// negative where they overlap.
    fn apart(eye: Vec3, a: &Quad, b: &Quad) -> f32 {
        let towards = |quad: &Quad| round(quad).map(|p| (p - eye).normalize());
        let (a, b) = (towards(a), towards(b));
        let mut widest = f32::NEG_INFINITY;
        for (own, other) in [(a, b), (b, a)] {
            for i in 0..4 {
                let normal = own[i].cross(own[(i + 1) % 4]).normalize();
                // The side of the edge's plane the quad itself lies on.
                let side = own[(i + 2) % 4].dot(normal).signum();
                let gap = other.iter().map(|d| -d.dot(normal) * side).fold(f32::INFINITY, f32::min);
                widest = widest.max(gap.clamp(-1.0, 1.0).asin());
            }
        }
        widest
    }

    #[test]
    fn the_settings_panel_and_the_keys_stay_off_the_screen_at_every_scale_and_shape() {
        // The screen as both hosts place it in front of the seat (the
        // default 3 m wide 2.5 m ahead; 2 m wide; 4 m wide 3 m ahead), two-hand
        // scaling from 0.4 to 3 times its width, its height from the
        // picture's shape (320x224, wider framing's 398x224, 320x240 and
        // 320x216), from two seats; OpenXR's panel (880x1120) and the
        // browser's (640x560); columns of 1 to 10 keys beside each edge.
        let seats = [Seat::default(), Seat { pose: Mat4::from_rotation_translation(Quat::from_rotation_y(-0.7), Vec3::new(0.2, 0.1, 0.3)) }];
        let degree = 1f32.to_radians();
        let (mut panel_gap, mut key_gap, mut lowest) = (f32::MAX, f32::MAX, 0.0f32);
        for seat in &seats {
            let eye = seat.position();
            let forward = seat.orientation() * Vec3::NEG_Z;
            for (width, distance) in [(3.0, 2.5), (2.0, 2.5), (4.0, 3.0)] {
                for tenths in 4..=30 {
                    let scale = tenths as f32 / 10.0;
                    for (w, h) in [(320.0, 224.0), (398.0, 224.0), (320.0, 240.0), (320.0, 216.0)] {
                        let size = (width * scale, width * scale * h / w);
                        let at = seat.place(Pose { position: Vec3::new(0.0, 0.0, -distance), orientation: Quat::IDENTITY });
                        let screen = Screen::flat(Quad { pose: at, size }, (w, h), 0.0);
                        let case = format!("a {width} m screen {distance} m ahead at {scale}, {w}x{h}");
                        for aspect in [1120.0 / 880.0, 560.0 / 640.0] {
                            let panel = panel(seat, Beside::Screen { size, distance }, aspect);
                            // In front of the seat, facing it, as large as ever.
                            let towards = panel.pose.position - eye;
                            assert!(towards.dot(forward) > 0.0 && (towards.length() - PANEL_DISTANCE).abs() < 1e-4, "{case}: {panel:?}");
                            assert!((panel.pose.orientation * Vec3::Z).dot(-towards.normalize()) > 0.9999, "{case}: the panel faces the seat");
                            let gap = apart(eye, &panel, &screen.quad);
                            assert!(gap > 2.0 * degree, "{case}: the panel {:.2} degrees off the screen", gap.to_degrees());
                            panel_gap = panel_gap.min(gap);
                            lowest = lowest.max(-(towards.normalize().y.asin()));
                            for count in 1..=10 {
                                for (_, key) in side_keys(&screen, count).iter().chain(pad_keys(&screen, count).iter()) {
                                    assert_eq!(key.size, KEY_SIZE, "{case}: the keys keep their size");
                                    // Beside the screen in its plane (its 4 cm gap
                                    // narrows with distance), and well off the panel.
                                    let (off_screen, off_panel) = (apart(eye, key, &screen.quad), apart(eye, key, &panel));
                                    assert!(off_screen > 0.0 && off_panel > degree, "{case}: {count} keys, one {:.2} degrees off the screen and {:.2} off the panel",
                                        off_screen.to_degrees(), off_panel.to_degrees());
                                    key_gap = key_gap.min(off_screen);
                                }
                            }
                        }
                    }
                }
            }
        }
        eprintln!("the panel at least {:.1} degrees below the screen, its middle at most {:.1} below level; the keys at least {:.2} degrees beside the screen",
            panel_gap.to_degrees(), lowest.to_degrees(), key_gap.to_degrees());
        // The old place (0.75 m down, 1.1 m ahead, tilted 25 degrees) covered
        // the default screen's lower edge, where the dialogue windows stand.
        let old = Quad {
            pose: Pose { position: Vec3::new(0.0, -0.75, -1.1), orientation: Quat::from_rotation_x(-25f32.to_radians()) },
            size: (PANEL_WIDTH, PANEL_WIDTH * 1120.0 / 880.0),
        };
        let screen = Quad { pose: Pose { position: Vec3::new(0.0, 0.0, -2.5), orientation: Quat::IDENTITY }, size: (3.0, 2.1) };
        assert!(apart(Vec3::ZERO, &old, &screen) < 0.0, "the old place overlapped the screen");
    }

    #[test]
    fn with_the_diorama_the_settings_panel_stands_beside_its_pane_and_keys_above_the_table_facing_the_seat() {
        // From two seats, the table at the ends and the middle of the
        // settings' height, distance and tilt ranges, the pane for each
        // picture's shape (the field's 320x224, 320x240, wider framing's
        // 398x224 and the world map's 320x216) with 1 to 10 keys beside its
        // right edge, OpenXR's panel (880x1120) and the browser's (640x560).
        let seats = [Mat4::IDENTITY, Mat4::from_rotation_translation(Quat::from_rotation_y(0.8), Vec3::new(0.3, 0.1, -0.2))];
        let degree = 1f32.to_radians();
        let (mut pane_gap, mut key_gap, mut turns) = (f32::MAX, f32::MAX, (f32::MAX, 0.0f32));
        for pose in seats {
            let seat = Seat { pose };
            let eye = seat.position();
            let forward = seat.orientation() * Vec3::NEG_Z;
            // How far round to the right of the seat's way a point is, about
            // the vertical through the seat (radians).
            let round_to = |p: Vec3| {
                let local = pose.inverse().transform_point3(p);
                local.x.atan2(-local.z)
            };
            let furthest = |quad: &Quad| round(quad).map(round_to).into_iter().fold(f32::MIN, f32::max);
            for height in [-1.0, -0.45, 0.3] {
                for distance in [0.3, 0.6, 2.0] {
                    for tilt in [0.0f32, 20.0, 45.0] {
                        let table = Table { height, distance, tilt: tilt.to_radians(), ..Table::default() };
                        // The volume's highest point in the room.
                        let (min, max) = Table::VOLUME;
                        let room_from_table = table.room_from_table(pose);
                        let top = (0..8).map(|i| {
                            let corner = Vec3::new([min.x, max.x][i & 1], [min.y, max.y][i >> 1 & 1], [min.z, max.z][i >> 2 & 1]);
                            room_from_table.transform_point3(corner).y
                        }).fold(f32::MIN, f32::max);
                        for aspect in [1120.0 / 880.0, 560.0 / 640.0] {
                            let panel = panel(&seat, Beside::Table(&table), aspect);
                            let case = format!("height {height}, distance {distance}, tilt {tilt}, a panel {aspect:.2} high");
                            let corners = round(&panel);
                            let lowest = corners.iter().map(|c| c.y).fold(f32::MAX, f32::min);
                            assert!(lowest > top, "{case}: the panel above the table's volume ({lowest} over {top})");
                            // Upright, turned to face the seat, on its right.
                            let towards = (eye - panel.pose.position) * Vec3::new(1.0, 0.0, 1.0);
                            assert!((panel.pose.orientation * Vec3::Z).dot(towards.normalize()) > 0.9999 && (panel.pose.orientation * Vec3::Y).y > 0.9999,
                                "{case}: the panel upright, facing the seat: {panel:?}");
                            let turn = forward.cross(-towards.normalize()).y.asin();
                            assert!(turn < 0.0, "{case}: right of the seat's way");
                            turns = (turns.0.min(-turn), turns.1.max(-turn));
                            let nearest = round(&panel).map(round_to).into_iter().fold(f32::MAX, f32::min);
                            for (w, h) in [(320.0, 224.0), (320.0, 240.0), (398.0, 224.0), (320.0, 216.0)] {
                                let space = DioramaSpace::first(&crate::headset::tests::level_camera(), 256.0, Vec2::new(w, h), &table, pose).expect("a space");
                                let pane = Screen::pane(space, eye);
                                // Standing on the pane's lower edge, all of it
                                // `PANEL_GAP_DEG` or more further round than the
                                // pane and its keys, so off them as seen from
                                // the seat (less than that gap where they stand
                                // high above it: a vertical plane's gap shrinks
                                // towards the zenith).
                                let pane_lowest = round(&pane.quad).iter().map(|c| c.y).fold(f32::MAX, f32::min);
                                assert!((lowest - pane_lowest).abs() < 1e-4, "{case}, {w}x{h}: the panel's lower edge at {lowest}, the pane's at {pane_lowest}");
                                let off = apart(eye, &panel, &pane.quad);
                                assert!(nearest - furthest(&pane.quad) > PANEL_GAP_DEG * degree && off > PANEL_GAP_DEG * degree,
                                    "{case}, {w}x{h}: the panel {:.2} degrees round from the pane, {:.2} off it", (nearest - furthest(&pane.quad)).to_degrees(), off.to_degrees());
                                pane_gap = pane_gap.min(off);
                                for count in 1..=10 {
                                    for (_, key) in side_keys(&pane, count) {
                                        let off = apart(eye, &panel, &key);
                                        assert!(nearest - furthest(&key) > PANEL_GAP_DEG * degree - 1e-5 && off > degree,
                                            "{case}, {w}x{h}, {count} keys: the panel {:.2} degrees round from a key, {:.2} off it", (nearest - furthest(&key)).to_degrees(), off.to_degrees());
                                        key_gap = key_gap.min(off);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        // The default table's, from the seat at the origin (recorded).
        let default = panel(&Seat::default(), Beside::Table(&Table::default()), 1120.0 / 880.0);
        let middle = default.pose.position;
        eprintln!("beside the diorama's pane the panel stands at least {:.2} degrees off it and {:.2} off its keys, its middle {:.1}-{:.1} degrees right of the seat's way; \
            with the default table at {middle:.3}, {:.1} degrees right and {:.1} up, {:.3} m wide",
            pane_gap.to_degrees(), key_gap.to_degrees(), turns.0.to_degrees(), turns.1.to_degrees(),
            middle.x.atan2(-middle.z).to_degrees(), middle.y.atan2(middle.x.hypot(middle.z)).to_degrees(), default.size.0);
    }

    /// Where the room below the screen runs out: the panel keeps its gap
    /// until the screen's lower edge stands about 56.5 degrees below the
    /// seat's level (65.5 for the browser's shorter panel), then stops
    /// with its lower edge straight down and covers the screen's lower edge
    /// from about 60.5 degrees (69.5), whatever makes the screen so tall:
    /// the settings' largest screens at their nearest, or two-hand scaling
    /// at its largest (3 times) of a 4 m screen at the default 2.5 m with a
    /// 4:3 picture or a 4.5 m one with the field's.
    #[test]
    fn the_panel_covers_the_screen_only_where_its_lower_edge_stands_over_sixty_degrees_down() {
        let seat = Seat::default();
        let eye = seat.position();
        let degree = 1f32.to_radians();
        let distance = 2.5;
        let gap = |size: (f32, f32), aspect: f32| {
            let screen = Quad { pose: seat.place(Pose { position: Vec3::new(0.0, 0.0, -distance), orientation: Quat::IDENTITY }), size };
            apart(eye, &panel(&seat, Beside::Screen { size, distance }, aspect), &screen)
        };
        let edges: Vec<f32> = (300..=800).map(|tenths| tenths as f32 / 10.0).collect();
        for (aspect, kept_from, covered_by, what) in [(1120.0 / 880.0, 56.0, 61.0, "OpenXR's panel"), (560.0 / 640.0, 65.0, 70.0, "the browser's panel")] {
            // The field's 320x224 picture, its lower edge `edge` degrees down.
            let at = |edge: f32| {
                let height = 2.0 * distance * edge.to_radians().tan();
                gap((height * 320.0 / 224.0, height), aspect)
            };
            let kept = edges.iter().copied().take_while(|&edge| at(edge) > 3.9 * degree).last().expect("room below a low screen");
            let covered = edges.iter().copied().find(|&edge| at(edge) < 0.0).expect("no room below a screen reaching straight down");
            eprintln!("{what}: its gap kept to a lower edge {kept:.1} degrees down, the screen covered from {covered:.1}");
            assert!((kept_from..kept_from + 1.0).contains(&kept) && (covered_by - 1.0..=covered_by).contains(&covered), "{what}: kept to {kept}, covered from {covered}");
            assert!(edges.iter().copied().filter(|&edge| edge < covered).all(|edge| at(edge) >= 0.0), "{what}: off the screen below {covered}");
        }
        // Two-hand scaling at its largest.
        let spread = |width: f32, (w, h): (f32, f32)| (3.0 * width, 3.0 * width * h / w);
        assert!(gap(spread(3.0, (320.0, 240.0)), 1120.0 / 880.0) > 3.9 * degree, "the default 3 m screen of a 4:3 picture spread to 3 times keeps its gap");
        assert!(gap(spread(4.0, (320.0, 240.0)), 1120.0 / 880.0) < 0.0, "a 4 m screen of a 4:3 picture spread to 3 times is covered");
        assert!(gap(spread(4.5, (320.0, 224.0)), 1120.0 / 880.0) < 0.0, "a 4.5 m screen of the field's picture spread to 3 times is covered");
    }
}
