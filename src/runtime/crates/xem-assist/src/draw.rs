//! The assisted controls' marks and touch controls as Seraph Glass widgets
//! (`xem_render::glass`), for every host that draws them.
use crate::headset::{Pose, Quad, Screen};
use crate::overlay::{Kind, Mark};
use crate::touch::Rect;
use xem_render::glam::{Mat4, Vec2, Vec3};
use xem_render::glass::{Icon, State, Widget};

/// The widgets' texture size in pixels.
pub const WIDGET_PIXELS: u32 = 128;
/// The keys beside a headset's screen: size in metres and the gap.
const KEY_SIZE: (f32, f32) = (0.22, 0.11);
const KEY_GAP: f32 = 0.04;

/// `count` keys' quads in a column beside the screen's right edge, the
/// first at the top.
pub fn side_keys(screen: &Screen, count: usize) -> Vec<(u32, Quad)> {
    let right = screen.quad.pose.orientation * Vec3::X;
    let up = screen.quad.pose.orientation * Vec3::Y;
    let middle = (count as f32 - 1.0) / 2.0;
    (0..count).map(|i| {
        let position = screen.quad.pose.position + right * (screen.quad.size.0 / 2.0 + KEY_GAP + KEY_SIZE.0 / 2.0)
            + up * (middle - i as f32) * (KEY_SIZE.1 + KEY_GAP);
        (i as u32, Quad { pose: Pose { position, orientation: screen.quad.pose.orientation }, size: KEY_SIZE })
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

/// Marks on a headset's screen (floor marks lie as the floor projects),
/// the keys beside it on the right and the pointer's keys on the left.
pub fn room(marks: &[Mark], screen: &Screen, keys: &[KeyMark], pad: &[KeyMark]) -> Vec<Placed> {
    let normal = screen.quad.pose.orientation * Vec3::Z;
    let mut placed: Vec<Placed> = marks.iter().map(|mark| {
        let centre = screen.point(mark.centre);
        let x = screen.point((mark.centre.0 + mark.x.0, mark.centre.1 + mark.x.1)) - centre;
        let y = screen.point((mark.centre.0 + mark.y.0, mark.centre.1 + mark.y.1)) - centre;
        let lift = if matches!(mark.kind, Kind::Cursor) { 0.012 } else { 0.006 };
        Placed {
            widget: mark_widget(mark.kind),
            pose: Mat4::from_cols((x * 2.0).extend(0.0), (-y * 2.0).extend(0.0), normal.extend(0.0), (centre + normal * lift).extend(1.0)),
            size: Vec2::ONE,
        }
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

