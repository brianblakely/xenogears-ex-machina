//! The assisted controls' marks and touch controls as Seraph Glass widgets
//! (`xem_render::glass`), for every host that draws them.
use crate::headset::{Pose, Quad, Screen, Seat};
use crate::overlay::{Kind, Mark, Place};
use crate::touch::Rect;
use xem_render::glam::{Mat4, Quat, Vec2, Vec3};
use xem_render::glass::{Icon, State, Widget};
use xem_render::ps1::Table;

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
/// stands: inside the top left corner of `anchor` (the screen, or the
/// diorama's pane: its pose, centred, and size in metres), as large against
/// it as against the default screen, 2 mm in front. Its pose and size.
pub fn hud(anchor: Mat4, anchor_size: Vec2, hud_px: (u32, u32), scale_factor: f32) -> (Mat4, Vec2) {
    let unit = anchor_size.x / 640.0 / scale_factor;
    let size = Vec2::new(hud_px.0 as f32 * unit, hud_px.1 as f32 * unit);
    let margin = anchor_size.x / 80.0;
    let at = Vec3::new(-anchor_size.x / 2.0 + size.x / 2.0 + margin, anchor_size.y / 2.0 - size.y / 2.0 - margin, 0.002);
    (anchor * Mat4::from_translation(at), size)
}

/// The settings panel's width (metres) below a headset's screen, and where
/// it stands there from the seat: 1.1 m ahead and 0.75 m down, tilted 25
/// degrees towards the player.
pub const PANEL_WIDTH: f32 = 0.55;
const PANEL_AT: Vec3 = Vec3::new(0.0, -0.75, -1.1);
const PANEL_TILT_DEG: f32 = -25.0;

/// Where a headset's settings panel stands in front of `seat`, `aspect` its
/// height over its width: below the screen, tilted towards the player; with
/// the diorama as the view (`table`), where the diorama's pane floats, its
/// lower edge above the table's volume (over the pane while it shows: the
/// game is paused), as large as seen from the seat as below the screen.
pub fn panel(seat: &Seat, table: Option<&Table>, aspect: f32) -> Quad {
    match table {
        None => {
            let local = Pose { position: PANEL_AT, orientation: Quat::from_rotation_x(PANEL_TILT_DEG.to_radians()) };
            Quad { pose: seat.place(local), size: (PANEL_WIDTH, PANEL_WIDTH * aspect) }
        }
        Some(table) => {
            // As wide, seen from the seat at its middle, as below the screen
            // (its middle rises with its size: a few steps settle it).
            let above = table.above(seat.pose);
            let seen = PANEL_WIDTH / PANEL_AT.length();
            let centre = |width: f32| above * Mat4::from_translation(Vec3::new(0.0, width * aspect / 2.0, 0.0));
            let mut width = seen * above.w_axis.truncate().distance(seat.position());
            for _ in 0..4 {
                width = seen * centre(width).w_axis.truncate().distance(seat.position());
            }
            let (_, orientation, position) = centre(width).to_scale_rotation_translation();
            Quad { pose: Pose { position, orientation }, size: (width, width * aspect) }
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
                // Across the world's x and z, its top towards the far side,
                // a few millimetres over the floor.
                let room = screen.diorama?.room_from_world;
                let at = room.transform_point3(Vec3::new(point.x, point.y, point.z));
                let x = room.transform_vector3(Vec3::new(radius, 0.0, 0.0));
                let z = room.transform_vector3(Vec3::new(0.0, 0.0, -radius));
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
        let seats = [Mat4::IDENTITY, Mat4::from_rotation_translation(Quat::from_rotation_y(0.8), Vec3::new(0.3, 0.1, -0.2))];
        let mut sizes = Vec::new();
        let mut spread = (f32::MAX, 0.0f32);
        for (n, seat) in seats.into_iter().enumerate() {
            let at = seat.w_axis.truncate();
            for height in [-1.0, -0.45, 0.3] {
                for distance in [0.3, 0.6, 2.0] {
                    for tilt in [0.0f32, 20.0, 45.0] {
                        for scale in [0.25, 1.0, 4.0] {
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
                            // The HUD inside the pane's corner (the frame counter's
                            // 304x110 logical pixels at twice that).
                            let (hud_pose, hud_size) = hud(space.pane, space.pane_size, (608, 220), 2.0);
                            let (_, orientation, position) = hud_pose.to_scale_rotation_translation();
                            let hud = Quad { pose: Pose { position, orientation }, size: (hud_size.x, hud_size.y) };
                            assert!(outside(&hud, table_from_room), "{case}: the HUD");
                            // Everything beside the pane, laid out at this scale.
                            let beside: Vec<Quad> = side_keys(&pane, 10).into_iter().chain(pad_keys(&pane, 10)).map(|(_, key)| key).chain([hud]).collect();
                            sizes.push(((n, height, distance, tilt), scale, beside, pane.quad));
                            let panel = panel(&Seat { pose: seat }, Some(&table), 1.27);
                            assert!(outside(&panel, table_from_room), "{case}: the panel");
                            let below_screen = panel_width_seen(&super::panel(&Seat { pose: seat }, None, 1.27), at);
                            assert!((panel_width_seen(&panel, at) / below_screen - 1.0).abs() < 0.01, "{case}: the panel as large as below the screen");
                        }
                    }
                }
            }
        }
        eprintln!("keys beside the pane look {:.2}-{:.2} as large as beside the screen", spread.0, spread.1);
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
        // Without a diorama a floor mark has nowhere to lie.
        assert!(room(&marks, &Screen { diorama: None, ..pane }, &[], &[]).is_empty());
    }
}
