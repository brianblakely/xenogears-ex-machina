//! The seated diorama (plan.md Phase 8; docs/runtime.md, Diorama): the
//! game's 3D scene as a miniature on a table in front of a seated player,
//! each eye with its own view and projection.
//!
//! - **World points.** The frame shown is taken back into the game's world
//!   through the camera the port recorded it with ([`xem_gpu::Camera`]):
//!   every projected point of the scene (its point before the GTE's
//!   division, or its screen point taken back through its projection) goes
//!   through the camera's inverse once a game frame. The eyes see the world
//!   through their own matrices (draw.wgsl `vs_world`), so both eyes and
//!   every headset frame between two game frames draw one build.
//! - **The table.** The world stands on the table upright (the camera's
//!   pitch and roll removed): the table's space is the world shifted to the
//!   point the table follows (the camera's target, eased), turned about the
//!   world's up to the heading the table takes ([`Turns`]), flipped to y up
//!   (keeping the world's handedness, so nothing is mirrored and faces keep
//!   their winding) and scaled so the original screen's width at the
//!   camera's target depth spans the table, fixed when the scene begins,
//!   times the player's scale. A battle's stands where its combatants stand,
//!   facing the way its start view looks, scaled to show them all, for the
//!   whole battle ([`battle_table`]). Fragments outside the table's volume
//!   are not drawn.
//! - **Sprites** stand as upright billboards at their floor points, each
//!   part laid along the billboards' right and the world's up as the game's
//!   camera showed it, all facing the seat's way (built once a game frame,
//!   for both eyes); the sprites at one floor point stand together with
//!   their lowest part on the floor, or above it as far as the game drew
//!   them above it.
//! - **Depth.** The scene is depth-tested: opaque primitives write depth,
//!   translucent ones only test it, in the game's order. A face drawn over
//!   faces it overlaps in its plane (a shadow on a floor, a sprite's part
//!   over another) is lifted a tenth of a millimetre above them ([`Layers`]),
//!   as the PS1 draws it over them whatever their depths: a face towards
//!   the side the game's camera saw it from, a sprite's part towards the
//!   side of its billboard each eye sees (draw.wgsl `vs_world`). Faces the game
//!   shows from their front (its majority winding, judged in its camera's
//!   view space) and the faces it culled are drawn front-facing only, so an
//!   eye sees into rooms through walls it sees from behind (a cutaway);
//!   faces the game drew from behind (double-sided ones) and sprites are
//!   drawn either way.
//! - **What else the frame holds.** Flat primitives and fills the game drew
//!   before its scene (the clear, panoramas, sky bands) are the backdrop: a
//!   wall at the back of the table's volume, the original screen mapped onto
//!   it; where the room shows around the table (passthrough, immersive AR)
//!   the room stands in for the clear (the fills), which the wall then
//!   leaves out. Flat primitives drawn after (windows, text, portraits,
//!   menus, the interface's own models) and pictures uploaded to the screen
//!   go onto a transparent premultiplied pane, drawn as the screen draws
//!   them. A
//!   full-screen untextured one (a fade, a flash) tints each eye's diorama
//!   and the pane over what they held then, never the room around them.
//! - **The screen instead.** Movies, menus and other modes but fields,
//!   battles and the world map, frames that sample a screen the game took
//!   or the frame itself, and frames without a camera record or a scene to
//!   place show the flat screen, easing between the two over [`SWITCH`]
//!   seconds, the last diorama built fading out over it; two game frames in
//!   a row without a camera record or a scene keep the diorama of the frame
//!   before.

use super::frame::{FrameRenderer, ReplayVram, framebuffer_effect, sprite_feet, uniform_buffer, view_point};
use super::{
    AUX_FORMAT, Attributes, BLEND_SHIFT, Builder, COLOR_FORMAT, Capture, Command, DepthMode, DisplayArea, DrawOptions, FLAG_BILLBOARD, FLAG_CLIP,
    FLAG_LINE, FLAG_SEMI, FLAG_TABLE, Fidelity, Fragment, PASS_PANE, PASS_WORLD, PassUniform, PipelineKey, Pipelines, Ps1Renderer, Space, Storage,
    Stream, Vertex, bind_group, color, effective_class, fill_color, overlap, pass_options, picture_usage, samples, scene_offset, scissor_rect,
    texture, upload,
};
use glam::{DMat3, DVec3, Mat3, Mat4, Vec2, Vec3};
use std::collections::HashMap;
use std::sync::Arc;
use xem_gpu::{Camera, CameraMode, Class, Combatant, Environment, Primitive, Reveals, Shape, Stage, TraceOp, TraceVertex, VramRect};

/// What a headset shows of the game ([`super::GameView::set_xr_view`]): the
/// flat virtual screen, or the diorama on its table.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum XrView {
    #[default]
    Screen,
    Diorama(Table),
}

/// A headset eye the diorama is drawn for
/// ([`super::GameView::present_diorama`]): from the room through `view`
/// (the room to the eye) and `proj` (the eye to wgpu's clip space), into a
/// picture of `size` pixels.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DioramaEye {
    pub view: Mat4,
    pub proj: Mat4,
    pub size: (u32, u32),
}

/// How the table turns with the game's camera (fields and the world map;
/// a battle's table stands where its stage puts it for the whole battle).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Turns {
    /// Eased towards the camera's heading; at once at a cut, dimmed briefly.
    #[default]
    Smooth,
    /// At once, dimmed briefly, once the camera has turned [`SNAP_STEP`]
    /// from the table's heading (and at a cut).
    Snap,
    /// The heading the scene began with.
    Fixed,
}

/// The table the diorama stands on, placed from the seat.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Table {
    /// The player's scale (1: the original screen's width at the camera's
    /// target spans the table).
    pub scale: f32,
    /// The table top's height above the seat's origin (metres; the
    /// reference space's origin is the seated head's), its distance ahead,
    /// and its tilt (radians; positive raises its far edge).
    pub height: f32,
    pub distance: f32,
    pub tilt: f32,
    pub turns: Turns,
}

impl Default for Table {
    fn default() -> Self {
        Table { scale: 1.0, height: -0.45, distance: 0.6, tilt: 0.0, turns: Turns::Smooth }
    }
}

impl Table {
    /// The table's volume in its space (metres, its least and greatest
    /// corners): nothing of the diorama is drawn outside it, whatever the
    /// scale, and controls stand outside it.
    pub const VOLUME: (Vec3, Vec3) = (VOLUME_MIN, VOLUME_MAX);

    /// The room from the table's space (its top's centre, x right, y up, z
    /// towards the seat; metres) for a seat at `seat` (a pose in the room,
    /// turned about its up only).
    pub fn room_from_table(&self, seat: Mat4) -> Mat4 {
        seat * Mat4::from_translation(Vec3::new(0.0, self.height, -self.distance)) * Mat4::from_rotation_x(self.tilt)
    }

    /// Where things float above the table for a seat at `seat` (the pane, a
    /// host's settings panel): the middle of the volume's far top edge,
    /// raised a little, upright and facing the seat whatever the table's
    /// tilt (x right, y up, facing +z), in the room. What stands on it stays
    /// outside the volume: up from that edge is outside it at every tilt.
    pub fn above(&self, seat: Mat4) -> Mat4 {
        let (_, facing, _) = seat.to_scale_rotation_translation();
        let edge = self.room_from_table(seat).transform_point3(Vec3::new(0.0, VOLUME_MAX.y, VOLUME_MIN.z));
        Mat4::from_rotation_translation(facing, edge + facing * Vec3::new(0.0, PANE_LIFT, 0.0))
    }

    /// Where the interface's pane floats for a seat at `seat` and a picture
    /// `aspect` (its height over its width): standing on [`Table::above`],
    /// at a fixed size (the player's scale scales the world on the table,
    /// never the pane). Its pose in the room (its centre) and its size
    /// (metres).
    pub fn pane(&self, seat: Mat4, aspect: f32) -> (Mat4, Vec2) {
        let size = Vec2::new(PANE_WIDTH, PANE_WIDTH * aspect);
        (self.above(seat) * Mat4::from_translation(Vec3::new(0.0, size.y / 2.0, 0.0)), size)
    }
}

/// The table's volume in its space (metres): nothing outside it is drawn.
const VOLUME_MIN: Vec3 = Vec3::new(-0.45, -0.25, -0.45);
const VOLUME_MAX: Vec3 = Vec3::new(0.45, 0.55, 0.45);
/// Seconds the view takes to go between the diorama and the screen.
const SWITCH: f32 = 0.3;
/// The followed point and the heading ease with this time constant (s).
const EASE: f64 = 0.3;
/// A turn of the camera's heading beyond this from one game frame to the
/// next, or a jump of its target beyond a third of the table, is a cut.
const CUT_TURN: f32 = std::f32::consts::FRAC_PI_4;
/// [`Turns::Snap`]'s step.
const SNAP_STEP: f32 = std::f32::consts::FRAC_PI_6;
/// After a cut or a snap the diorama dims for this long (s), at first by
/// [`DIM_DEPTH`].
const DIM: f64 = 0.25;
const DIM_DEPTH: f32 = 0.7;
/// A face drawn over others in their plane is lifted this far above the
/// highest of them (metres on the table: a hundred times the depth
/// buffer's resolution a metre away, far below what an eye can tell), and
/// planes within [`COPLANAR`] of each other are one.
const LIFT: f32 = 0.000_1;
const COPLANAR: f32 = 0.000_2;
/// The cells (world units) [`Layers`] finds faces of a plane by.
const CELL: f32 = 256.0;
/// A game frame's build stands until the next game frame, and is made
/// again within one only once the table has turned this far since (a game
/// paused while the table turns towards its camera: the billboards face the
/// seat's way as the table stood at the build). While the game draws, a
/// smooth turn moves the table less than this from a game frame to the next
/// (at most 6.9 degrees at 20 frames a second, the camera just short of a
/// cut away).
const REBUILD_TURN: f32 = 10.0 * std::f32::consts::PI / 180.0;
/// Game frames in a row without a camera record or a scene to place that
/// keep the diorama of the frame before (the port records with the bits it
/// had, so the first frame after they change has none).
const MISSES: u32 = 2;
/// The interface pane's width (metres) and how far above the table's
/// volume, behind its far edge, it floats (its lower edge).
const PANE_WIDTH: f32 = 0.6;
const PANE_LIFT: f32 = 0.02;

/// The diorama of the frame shown, in the room (the reference space the
/// seat and the eyes are given in).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DioramaSpace {
    /// The room from the game's world (as the frame's camera record gives
    /// it; metres).
    pub room_from_world: Mat4,
    /// The room from the table's space (x right, y up, z towards the seat).
    pub room_from_table: Mat4,
    /// The table's turn from the game's world (its heading, the world's
    /// y-down made up): with `room_from_table`, `room_from_world`'s rotation
    /// without its scale.
    pub turn: Mat3,
    /// The table's tilt from the seat (radians; positive raises its far
    /// edge): with the seat, `room_from_table`'s rotation.
    pub tilt: f32,
    /// The table's volume in its space.
    pub volume: (Vec3, Vec3),
    /// The interface pane's pose in the room (its centre; x right, y up,
    /// facing +z) and its size (metres).
    pub pane: Mat4,
    pub pane_size: Vec2,
    /// The pane's picture: the display's pixels it spans.
    pub display: Vec2,
    /// The camera the frame shown was drawn with.
    pub camera: Camera,
    /// Metres per world unit.
    pub scale: f32,
    /// The seat the table was placed from (a pose in the room, turned about
    /// its up only).
    pub seat: Mat4,
}

impl DioramaSpace {
    /// The space a frame drawn with `camera` (its scene's projection
    /// distance `h`, its display `display` pixels) lies in on `table` from a
    /// seat at `seat`, as the first frame of its scene (the camera's target
    /// and heading taken at once, as after a recentring): for hosts' tests
    /// and agents placing things without a presentation.
    pub fn first(camera: &Camera, h: f32, display: Vec2, table: &Table, seat: Mat4) -> Option<DioramaSpace> {
        let placed = Placement::default().update(camera, h, "", false, 0.0, table)?;
        Some(DioramaSpace::of(&placed, camera, display, table, seat))
    }

    fn of(placed: &Placed, camera: &Camera, display: Vec2, table: &Table, seat: Mat4) -> DioramaSpace {
        let room_from_table = table.room_from_table(seat);
        let (pane, pane_size) = table.pane(seat, display.y / display.x.max(1.0));
        DioramaSpace {
            room_from_world: room_from_table * placed.table_from_world,
            room_from_table,
            turn: placed.turn,
            tilt: table.tilt,
            volume: Table::VOLUME,
            pane,
            pane_size,
            display,
            camera: *camera,
            scale: placed.scale,
            seat,
        }
    }

    /// The game's world from the room (world units, its y down).
    pub fn world_from_room(&self) -> Mat4 {
        self.room_from_world.inverse()
    }

    /// The table's space from the room.
    pub fn table_from_room(&self) -> Mat4 {
        self.room_from_table.inverse()
    }

    /// The listener's view for the Binaural mode while the diorama shows
    /// (`xem_core::binaural::ListenerView`): the seat's right, down and
    /// forward (the presentation's, which a headset's head turns from)
    /// through the table into the view space of the camera the frame shown
    /// was drawn with, and that camera's rotation rows (`camera_rows`),
    /// which the presenter hears them through: so what is heard is the
    /// table's world from the seat, whatever camera the game holds by the
    /// time it is heard. The head's turn, given in the seat's space, is
    /// composed with them once. The table stands where the seat puts it, so
    /// through the table is through its tilt from the seat and its turn
    /// from the world, both rotations: neither the player's scale nor where
    /// the seat stands in the room (a recentring) is in them, not even by
    /// rounding. None while the camera's matrix spans no space.
    pub fn listener_axes(&self) -> Option<([[f32; 3]; 3], [[f32; 3]; 3])> {
        let camera = camera_rows(&self.camera.axes)?;
        let seat_from_world = Mat3::from_rotation_x(self.tilt) * self.turn;
        let view_from_seat = Mat3::from_cols_array_2d(&camera).transpose() * seat_from_world.transpose();
        Some(([Vec3::X, Vec3::NEG_Y, Vec3::NEG_Z].map(|axis| (view_from_seat * axis).to_array()), camera))
    }
}

/// What a diorama presentation shows ([`super::GameView::present_diorama`]).
pub struct DioramaPresented<'a> {
    /// The flat screen's share of the view: 0 the diorama alone, 1 the
    /// screen alone, easing between them.
    pub screen: f32,
    /// The flat frame for the screen, while it shows (`screen > 0`): an
    /// `Rgba8Unorm` texture of sRGB-encoded values, as
    /// [`super::GameView::present`] gives it.
    pub frame: Option<super::Presented<'a>>,
    /// Each eye's picture of the diorama, while it shows (one per
    /// [`DioramaEye`], at its size: `Rgba8Unorm`, premultiplied, sRGB-encoded
    /// values, transparent around the scene): for the host to draw over its
    /// eye's whole view at `opacity`, over the screen and under the pane.
    pub eyes: Vec<&'a wgpu::TextureView>,
    /// The eyes' pictures' opacity: one minus `screen`, dimmed briefly after
    /// the table jumped (a cut or a snap).
    pub opacity: f32,
    /// The game's interface on a transparent pane (premultiplied,
    /// sRGB-encoded values; where it goes: `space`), while the diorama shows:
    /// for the host to draw at one minus `screen`.
    pub pane: Option<&'a wgpu::TextureView>,
    /// Where the diorama lies in the room, while it shows.
    pub space: Option<DioramaSpace>,
    /// A new game frame arrived with this presentation.
    pub new_frame: bool,
    /// The settings' HD-2D effects are not drawn in the diorama.
    pub effects_unavailable: bool,
}

/// What a diorama build drew (observation only).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct DioramaStats {
    /// Scene primitives on the table, sprite parts among them, faces drawn
    /// front-facing only, and faces the game culled.
    pub scene: u32,
    pub sprites: u32,
    pub one_sided: u32,
    pub revealed: u32,
    /// Backdrop primitives and fills on the wall, interface primitives and
    /// pictures on the pane, full-screen fades.
    pub wall: u32,
    pub pane: u32,
    pub tints: u32,
    /// Scene primitives that could not be placed (no point before the
    /// division nor a projection).
    pub unplaced: u32,
    /// Faces and sprite parts lifted over others they were drawn over in
    /// their plane, and the most a sprite's part was lifted off its
    /// billboard's plane (micrometres on the table: how thick the thickest
    /// stack of a sprite's parts stands).
    pub lifted: u32,
    pub thickest: u32,
    /// Groups of draws between VRAM uploads (each a pass per eye).
    pub runs: u32,
    /// The builds made since the diorama view began, this one included (one
    /// a game frame; [`REBUILD_TURN`]).
    pub builds: u32,
}

/// Why a frame shows the flat screen instead of the diorama: for good (a
/// movie, a menu, another mode, a screen the game took) or for a frame or
/// two (no camera record yet, or no scene to place).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Fallback {
    Screen,
    Moment,
}

/// The diorama's state in a [`super::GameView`]: presentation only.
pub(crate) struct Diorama {
    /// The flat screen's share of the view, and when it was last eased.
    screen: f32,
    eased: Option<f64>,
    /// Game frames in a row shown without a camera record.
    misses: u32,
    placement: Placement,
    gpu: Option<Targets>,
    space: Option<DioramaSpace>,
    stats: DioramaStats,
}

impl Default for Diorama {
    fn default() -> Self {
        Diorama { screen: 1.0, eased: None, misses: 0, placement: Placement::default(), gpu: None, space: None, stats: DioramaStats::default() }
    }
}

/// Where the table follows the scene, eased between presentations.
#[derive(Clone, Debug, Default)]
struct Placement {
    /// The scene it belongs to (a new scene begins it again), the scale it
    /// began with (metres per world unit), the point the table follows and
    /// its heading (radians about the world's up).
    scene: String,
    base: Option<f32>,
    target: Option<Vec3>,
    heading: Option<f32>,
    /// The camera's target and heading at the last game frame (cuts).
    last: Option<(Vec3, f32)>,
    /// When the table last jumped (a cut or a snap), and the host time of
    /// the last update.
    jumped: Option<f64>,
    at: Option<f64>,
    /// A battle's table, which its stage places for the whole battle.
    battle: Option<BattleTable>,
    /// How far the world map's table stands below its camera's target
    /// (world units), settling towards the ground under it.
    flight: Option<f32>,
}

/// Where a battle's stage put the table ([`battle_table`]): for the start
/// view it was placed for (a new one is a new battle), from its combatants
/// once any stood.
#[derive(Clone, Copy, Debug, PartialEq)]
struct BattleTable {
    start: ([f32; 3], [f32; 3]),
    formed: bool,
    point: Vec3,
    heading: f32,
    base: f32,
}

/// The original screen's width at two projection distances, the nearest the
/// battle's camera frames its combatants from (`battle_camera_frame_slots`):
/// the least of a battle's world a table spans (world units).
const BATTLE_SPAN: f32 = 2.0 * 320.0;
/// A battle's tallest combatant stands within this share of the table's
/// volume above its top.
const BATTLE_HEADROOM: f32 = 0.9;
/// The ground under the world map's camera target, in flight above it, lies
/// within this share of the table's volume below its top, and the target
/// within this share of the volume above it.
const GROUND_DEPTH: f32 = 0.8;
const FLIGHT_HEADROOM: f32 = 0.8;
/// How slowly the world map's table settles on the ground under a target in
/// flight (time constant, seconds): the map rises and falls with the
/// target, not with every hill it passes over.
const FLIGHT_EASE: f64 = 3.0;

/// Where a battle's table stands for the whole battle (docs/runtime.md,
/// Diorama), from its `stage` (`flip` the world's y and z to the table's,
/// `h` the projection distance): the start view's heading (the way the
/// camera the battle begins with looks); once its turns run (its opening is
/// over: its combatants have entered and stand), the middle of their extent
/// across the table, as its camera frames them all, on the ground the
/// lowest of them stands on, and the span of the battle's world the table's
/// width shows: their extent with the tallest one's height to spare, at
/// least [`BATTLE_SPAN`], and room for the tallest within
/// [`BATTLE_HEADROOM`] of the volume's height. During its opening, the start
/// view's look-at and the original screen's width at its depth.
fn battle_table(stage: &Stage, flip: Vec3, h: f32) -> BattleTable {
    let (look, eye) = (Vec3::from(stage.look_at), Vec3::from(stage.eye));
    let forward = flip * (look - eye);
    let heading = if forward.x.hypot(forward.z) > 1e-3 { std::f32::consts::PI - forward.x.atan2(forward.z) } else { 0.0 };
    let turn = Mat3::from_rotation_y(heading) * Mat3::from_diagonal(flip);
    let combatants: Vec<Combatant> = stage.combatants.iter().flatten().copied().collect();
    let start = (stage.look_at, stage.eye);
    let width = VOLUME_MAX.x - VOLUME_MIN.x;
    if !stands(stage) {
        let span = 320.0 * (look - eye).length() / h.max(1.0);
        return BattleTable { start, formed: false, point: look, heading, base: width / span.max(1.0) };
    }
    // Their bounds across the table (its x and z).
    let across: Vec<Vec3> = combatants.iter().map(|c| turn * Vec3::from(c.position)).collect();
    let (lo, hi) = across.iter().fold((Vec3::MAX, Vec3::MIN), |(lo, hi), p| (lo.min(*p), hi.max(*p)));
    let middle = turn.transpose() * ((lo + hi) / 2.0);
    let ground = combatants.iter().map(|c| c.position[1]).fold(f32::MIN, f32::max);
    let extent = (hi.x - lo.x).max(hi.z - lo.z);
    let tallest = combatants.iter().map(|c| c.height).fold(0.0, f32::max);
    let span = BATTLE_SPAN.max(extent + tallest).max(tallest * width / (BATTLE_HEADROOM * VOLUME_MAX.y));
    BattleTable { start, formed: true, point: Vec3::new(middle.x, ground, middle.z), heading, base: width / span }
}

/// Whether a battle's combatants stand: its turns run and some combatant
/// shows.
fn stands(stage: &Stage) -> bool {
    stage.turns && stage.combatants.iter().any(Option::is_some)
}

/// Where the frame shown stands on the table.
#[derive(Clone, Copy, Debug, PartialEq)]
struct Placed {
    table_from_world: Mat4,
    /// Its turn alone (the heading, the world's y-down made up).
    turn: Mat3,
    /// The billboards' right, up and towards-the-seat axes in the world.
    right: Vec3,
    up: Vec3,
    toward: Vec3,
    /// Metres per world unit and the heading.
    scale: f32,
    heading: f32,
}

/// `angle` within (-π, π].
fn wrap(angle: f32) -> f32 {
    let tau = std::f32::consts::TAU;
    let a = angle.rem_euclid(tau);
    if a > std::f32::consts::PI { a - tau } else { a }
}

/// A camera record's rotation (and the field's scale): view = M · world /
/// 4096 + translation, M's columns its axes.
fn rotation(camera: &Camera) -> Mat3 {
    Mat3::from_cols(Vec3::from(camera.axes[0]), Vec3::from(camera.axes[1]), Vec3::from(camera.axes[2])) / 4096.0
}

/// A recorded camera's rotation (`Camera::axes`, `Eye::axes`: its matrix's
/// columns) as its rows, each made unit length as the Binaural presenter
/// makes the game's own matrices' (`xem_core::binaural`), so that the same
/// matrix gives the same rows to the last bit; none when a row is shorter
/// than one (a matrix spanning no space).
pub(crate) fn camera_rows(axes: &[[f32; 3]; 3]) -> Option<[[f32; 3]; 3]> {
    let mut rows = [[0f32; 3]; 3];
    for (r, row) in rows.iter_mut().enumerate() {
        *row = [axes[0][r], axes[1][r], axes[2][r]];
        let length = row.iter().map(|c| c * c).sum::<f32>().sqrt();
        if length < 1.0 {
            return None;
        }
        *row = row.map(|c| c / length);
    }
    Some(rows)
}

/// The world from a camera's view space: a point's world point is
/// `linear · view + offset` (none when the camera's axes span no space).
#[derive(Clone, Copy, Debug)]
struct WorldFrame {
    linear: Mat3,
    offset: Vec3,
    /// View-space units per world unit (the field's scale).
    units: f32,
}

impl WorldFrame {
    fn new(camera: &Camera) -> Option<WorldFrame> {
        let m = DMat3::from_cols(DVec3::from(camera.axes[0].map(f64::from)), DVec3::from(camera.axes[1].map(f64::from)), DVec3::from(camera.axes[2].map(f64::from))) / 4096.0;
        if m.determinant().abs() < 1e-9 {
            return None;
        }
        let inverse = m.inverse();
        let offset = -(inverse * DVec3::from(camera.translation.map(f64::from)));
        Some(WorldFrame { linear: inverse.as_mat3(), offset: offset.as_vec3(), units: m.x_axis.length() as f32 })
    }

    fn point(&self, view: [f32; 3]) -> Vec3 {
        self.linear * Vec3::from(view) + self.offset
    }
}

impl Placement {
    /// The table for the frame shown, drawn with `camera` (its scene's
    /// projection distance `h`), at `now` (host seconds); `new_frame` when
    /// the game drew a new one since.
    fn update(&mut self, camera: &Camera, h: f32, scene: &str, new_frame: bool, now: f64, table: &Table) -> Option<Placed> {
        if self.scene != scene {
            *self = Placement { scene: scene.to_string(), ..Placement::default() };
        }
        let dt = self.at.map_or(0.0, |at| (now - at).clamp(0.0, 0.25));
        self.at = Some(now);
        let m = rotation(camera);
        let units = m.x_axis.length();
        if units <= 0.0 || m.determinant().abs() < 1e-9 {
            return None;
        }
        // y down to y up, keeping the world's handedness (its z flipped too
        // unless the camera's matrix mirrors its world, which none measured
        // does: a field's, the debug battle's, the world map's).
        let flip = Vec3::new(1.0, -1.0, if m.determinant() < 0.0 { 1.0 } else { -1.0 });
        // The camera's axis in the world (its third row), level, as the
        // table's -z (away from the seat).
        let forward = flip * m.row(2);
        let yaw = std::f32::consts::PI - forward.x.atan2(forward.z);
        let target = Vec3::from(camera.target);
        let (followed, heading, base) = match (camera.mode, &camera.stage) {
            // A battle's stage places its table for the whole battle,
            // whatever its camera does (it orbits, cuts and shakes): again
            // only for a new battle's start view or as its combatants come to
            // stand after its opening, a jump.
            (CameraMode::Battle, Some(stage)) => {
                let placed = match self.battle {
                    Some(battle) if battle.start == (stage.look_at, stage.eye) && (battle.formed || !stands(stage)) => battle,
                    before => {
                        if before.is_some() {
                            self.jumped = Some(now);
                        }
                        battle_table(stage, flip, h)
                    }
                };
                self.battle = Some(placed);
                (placed.point, placed.heading, placed.base)
            }
            _ => self.follow(camera, target, yaw, units, h, new_frame, now, dt, table),
        };
        let scale = base * table.scale;
        let turn = Mat3::from_rotation_y(heading) * Mat3::from_diagonal(flip);
        let table_from_world = Mat4::from_scale(Vec3::splat(scale)) * Mat4::from_mat3(turn) * Mat4::from_translation(-followed);
        // The table's axes back in the world (the turn's rows).
        let back = turn.transpose();
        Some(Placed { table_from_world, turn, right: back * Vec3::X, up: back * Vec3::Y, toward: back * Vec3::Z, scale, heading })
    }

    /// The point the table follows, its heading and the scene's scale (metres
    /// per world unit) for a field's or the world map's frame drawn with
    /// `camera` (looking at `target`, `yaw` its heading, `units` view-space
    /// units per world unit): the original screen's width at the target's
    /// depth spans the table, at the scene's beginning; the point eases
    /// towards the target (the world map's below it as far as the ground
    /// under a target in flight, settling there over [`FLIGHT_EASE`], at
    /// once as far as keeps that ground within [`GROUND_DEPTH`] of the
    /// volume below the table's top, and never so far that the target
    /// stands beyond [`FLIGHT_HEADROOM`] of the volume above it) and the
    /// heading turns as `table` says, both at once at a cut.
    #[allow(clippy::too_many_arguments)]
    fn follow(&mut self, camera: &Camera, target: Vec3, yaw: f32, units: f32, h: f32, new_frame: bool, now: f64, dt: f64, table: &Table) -> (Vec3, f32, f32) {
        let focus = if camera.focus > 1.0 { camera.focus } else { 4000.0 };
        let width = 320.0 * focus / h.max(1.0) / units;
        let base = *self.base.get_or_insert((VOLUME_MAX.x - VOLUME_MIN.x) / width.max(1.0));
        // y down: the ground lies `clearance` below a target in flight.
        self.flight = camera.ground.map(|ground| {
            let scale = (base * table.scale).max(1e-9);
            let headroom = FLIGHT_HEADROOM * VOLUME_MAX.y / scale;
            let clearance = ground - target.y;
            let settled = clearance.clamp(0.0, headroom);
            let eased = self.flight.map_or(settled, |below| below + (settled - below) * (1.0 - (-dt / FLIGHT_EASE).exp()) as f32);
            eased.max(clearance + GROUND_DEPTH * VOLUME_MIN.y / scale).max(0.0).min(headroom)
        });
        let target = target + Vec3::Y * self.flight.unwrap_or(0.0);
        let cut = new_frame
            && self.last.is_some_and(|(at, heading)| (target - at).length() * base > (VOLUME_MAX.x - VOLUME_MIN.x) / 3.0 || wrap(yaw - heading).abs() > CUT_TURN);
        if new_frame {
            self.last = Some((target, yaw));
        }
        let ease = 1.0 - (-dt / EASE).exp() as f32;
        let followed = match self.target {
            Some(point) if !cut => point + (target - point) * ease,
            _ => target,
        };
        self.target = Some(followed);
        let mut jumped = cut;
        let heading = match (self.heading, table.turns) {
            (None, _) => yaw,
            (Some(heading), Turns::Fixed) => heading,
            (Some(_), _) if cut => yaw,
            (Some(heading), Turns::Smooth) => heading + wrap(yaw - heading) * ease,
            (Some(heading), Turns::Snap) if wrap(yaw - heading).abs() >= SNAP_STEP => {
                jumped = true;
                yaw
            }
            (Some(heading), Turns::Snap) => heading,
        };
        self.heading = Some(heading);
        if jumped {
            self.jumped = Some(now);
        }
        (followed, heading, base)
    }

    /// How far the diorama is dimmed at `now` (0-1) after the table's last
    /// jump (a cut or a snap).
    fn dim(&self, now: f64) -> f32 {
        self.jumped.map_or(0.0, |at| (1.0 - (now - at) / DIM).clamp(0.0, 1.0) as f32)
    }
}

/// The projection distance most of the scene is drawn with.
fn projection_distance(ops: &[TraceOp], scene: Option<(i16, i16)>) -> Option<f32> {
    ops.iter().find_map(|op| match op {
        TraceOp::Draw(p) if p.class.is_scene() => p.projection.filter(|(offset, _)| Some(*offset) == scene).map(|(_, h)| f32::from(h)),
        _ => None,
    })
}

/// How far below each floor point (by its view-space point's bits) the
/// lowest part of the sprites standing there reaches on the game's screen
/// (view-space units down; negative when they are all above it). The game
/// draws a sprite facing its camera with its feet a little below its floor
/// point (in its own 3D, through the floor: its drawing order shows them),
/// and a character in several sprites at one floor point.
fn sprite_bottoms(ops: &[TraceOp], feet: &HashMap<u32, [f32; 3]>) -> HashMap<[u32; 3], f32> {
    let mut lowest: HashMap<[u32; 3], f32> = HashMap::new();
    for op in ops {
        let TraceOp::Draw(p) = op else { continue };
        let Some(anchor) = feet.get(&p.owner).filter(|_| p.class == Class::Sprite) else { continue };
        for q in (0..p.vertex_count()).filter_map(|i| view_point(p, i)) {
            let bottom = lowest.entry(anchor.map(f32::to_bits)).or_insert(f32::MIN);
            *bottom = bottom.max(q[1] - anchor[1]);
        }
    }
    lowest
}

/// Whether every point of `p` has a place in the camera's view space.
fn placeable(p: &Primitive) -> bool {
    p.projection.is_some() && (0..p.vertex_count()).all(|i| view_point(p, i).is_some())
}

/// Where the scene the game drew begins in `ops`: its first primitive of
/// the scene that has a place (the culled faces the port drew for the
/// record and the backdrop it projects aside). `None` for a frame without a
/// scene to place.
fn first_scene(ops: &[TraceOp]) -> Option<usize> {
    let scene = scene_offset(ops);
    ops.iter().position(|op| matches!(op, TraceOp::Draw(p) if !p.revealed && !p.backdrop && effective_class(p, scene).is_scene() && placeable(p)))
}

/// A line as a quad, its two ends doubled (a quad's in Z order: 0 0 / 1 1),
/// for a ribbon drawn where it lies.
fn ribbon(p: &Primitive) -> Primitive {
    let mut ribbon = p.clone();
    ribbon.shape = Shape::Quad;
    ribbon.vertices = [p.vertices[0], p.vertices[0], p.vertices[1], p.vertices[1]];
    ribbon
}

/// Which way round the game's camera sees `p` (the sign of its first three
/// view-space points' triple product: as its screen winding, also behind
/// the camera), 0 edge-on or without places.
fn facing(p: &Primitive) -> i32 {
    let point = |i| view_point(p, i).map(Vec3::from);
    let (Some(a), Some(b), Some(c)) = (point(0), point(1), point(2)) else { return 0 };
    let det = a.dot(b.cross(c));
    if det > 0.0 { 1 } else if det < 0.0 { -1 } else { 0 }
}

/// The backdrop's wall: the original screen mapped onto the back of the
/// table's volume, its width across the volume.
#[derive(Clone, Copy, Debug)]
struct Wall {
    origin: (f32, f32),
    metres: f32,
    top: f32,
}

impl Wall {
    fn new(area: DisplayArea) -> Wall {
        let metres = (VOLUME_MAX.x - VOLUME_MIN.x) / area.width.max(1) as f32;
        Wall { origin: (area.x as f32, area.y as f32), metres, top: VOLUME_MIN.y + area.height as f32 * metres }
    }

    /// The table-space point of VRAM point (x, y).
    fn point(&self, x: f32, y: f32) -> Vec3 {
        Vec3::new(VOLUME_MIN.x + (x - self.origin.0) * self.metres, self.top - (y - self.origin.1) * self.metres, VOLUME_MIN.z + 0.002)
    }

    /// Draws flat primitive `p` on the wall, seen from the seat's side only
    /// (a line as a ribbon a native pixel wide).
    fn draw(&self, builder: &mut Builder, p: &Primitive) {
        let mut points: Vec<Vec3> = p.vertices[..p.vertex_count()].iter().map(|v| self.point(v.fx, v.fy)).collect();
        let line = (p.shape == Shape::Line).then(|| ribbon(p));
        if line.is_some() {
            let side = (points[1] - points[0]).cross(Vec3::Z).normalize_or_zero() * (0.5 * self.metres);
            points = vec![points[0] - side, points[0] + side, points[1] - side, points[1] + side];
        }
        let towards_seat = (points[1] - points[0]).cross(points[2] - points[0]).z;
        builder.world_primitive(line.as_ref().unwrap_or(p), &points, true, true, towards_seat < 0.0, None);
    }
}

/// The faces of one kind (the scene's, or the sprites' billboard parts) in
/// their planes, as a build draws them: the PS1 draws a face over another
/// whatever their depths, and depth-tested a face in the same plane as one
/// it overlaps would show through it pixel by pixel (a shadow on a floor, a
/// sprite's part over another, split sprites at one floor point). Each is
/// lifted [`LIFT`] above the highest face it overlaps in its plane, drawn
/// before it (a look from within the scene lifts its faces likewise:
/// [`super::frame::first_person`]).
#[derive(Default)]
pub(crate) struct Layers {
    faces: Vec<Layered>,
    /// The faces by their plane's normal and distance (in bands of the
    /// tolerance) and by the cells their outline covers in it.
    cells: HashMap<([i32; 3], i64, i32, i32), Vec<u32>, std::hash::BuildHasherDefault<CellHasher>>,
    /// The last placement that looked at each face.
    seen: Vec<u32>,
    placements: u32,
}

/// A cheap hash for [`Layers`]' cell keys (a multiply and a rotation a
/// word; thousands of lookups a frame).
#[derive(Default)]
struct CellHasher(u64);

impl std::hash::Hasher for CellHasher {
    fn write(&mut self, bytes: &[u8]) {
        for chunk in bytes.chunks(8) {
            let mut word = [0u8; 8];
            word[..chunk.len()].copy_from_slice(chunk);
            self.write_u64(u64::from_le_bytes(word));
        }
    }

    fn write_u64(&mut self, word: u64) {
        self.0 = (self.0 ^ word).wrapping_mul(0x9E37_79B9_7F4A_7C15).rotate_left(29);
    }

    fn write_u32(&mut self, word: u32) {
        self.write_u64(u64::from(word));
    }

    fn write_i32(&mut self, word: i32) {
        self.write_u64(u64::from(word as u32));
    }

    fn write_i64(&mut self, word: i64) {
        self.write_u64(word as u64);
    }

    fn finish(&self) -> u64 {
        self.0
    }
}

/// A face [`Layers`] holds: its plane's distance before it was lifted and
/// after, and its outline in the plane.
struct Layered {
    base: f32,
    level: f32,
    outline: Vec<Vec2>,
}

/// Two axes across a plane with unit normal `n`.
fn plane_axes(n: Vec3) -> (Vec3, Vec3) {
    let u = n.cross(if n.x.abs() < 0.9 { Vec3::X } else { Vec3::Y }).normalize();
    (u, n.cross(u))
}

/// Whether convex outlines `a` and `b` overlap by more than `margin` (faces
/// that only share an edge do not).
fn overlaps(a: &[Vec2], b: &[Vec2], margin: f32) -> bool {
    for outline in [a, b] {
        for (i, &from) in outline.iter().enumerate() {
            let edge = outline[(i + 1) % outline.len()] - from;
            let axis = Vec2::new(-edge.y, edge.x).normalize_or_zero();
            if axis == Vec2::ZERO {
                continue;
            }
            let span = |points: &[Vec2]| points.iter().fold((f32::MAX, f32::MIN), |(lo, hi), p| (lo.min(p.dot(axis)), hi.max(p.dot(axis))));
            let ((a0, a1), (b0, b1)) = (span(a), span(b));
            if a1 <= b0 + margin || b1 <= a0 + margin {
                return false;
            }
        }
    }
    true
}

impl Layers {
    /// Places a face at `points` (a quad's in Z order) with unit normal
    /// `normal` (towards the side it is lifted to) and returns how far to
    /// lift it (in the points' units): planes within `tolerance` are one,
    /// `lift` the step, `cell` the size of the cells faces are found by. A
    /// face of fewer than three points is not one.
    pub(crate) fn place(&mut self, normal: Vec3, points: &[Vec3], tolerance: f32, lift: f32, cell: f32) -> f32 {
        if points.len() < 3 {
            return 0.0;
        }
        let base = normal.dot(points[0]);
        let (u, v) = plane_axes(normal);
        let order: &[usize] = if points.len() == 4 { &[0, 1, 3, 2] } else { &[0, 1, 2] };
        let outline: Vec<Vec2> = order.iter().map(|&i| Vec2::new(u.dot(points[i]), v.dot(points[i]))).collect();
        let (lo, hi) = outline.iter().fold((Vec2::MAX, Vec2::MIN), |(lo, hi), p| (lo.min(*p), hi.max(*p)));
        let direction = (normal * 512.0).round().as_ivec3().to_array();
        let band = (base / tolerance).floor() as i64;
        let cells = |lo: Vec2, hi: Vec2| {
            let (x0, x1) = ((lo.x / cell).floor() as i32, (hi.x / cell).floor() as i32);
            let (y0, y1) = ((lo.y / cell).floor() as i32, (hi.y / cell).floor() as i32);
            (x0..=x1).flat_map(move |x| (y0..=y1).map(move |y| (x, y)))
        };
        self.placements += 1;
        let mut level = base;
        for nearby in band - 1..=band + 1 {
            for (x, y) in cells(lo, hi) {
                let Some(list) = self.cells.get(&(direction, nearby, x, y)) else { continue };
                for &index in list {
                    let seen = &mut self.seen[index as usize];
                    if *seen == self.placements {
                        continue;
                    }
                    *seen = self.placements;
                    let face = &self.faces[index as usize];
                    if (face.base - base).abs() <= tolerance && overlaps(&face.outline, &outline, tolerance) {
                        level = level.max(face.level + lift);
                    }
                }
            }
        }
        let index = self.faces.len() as u32;
        for (x, y) in cells(lo, hi) {
            self.cells.entry((direction, band, x, y)).or_default().push(index);
        }
        self.faces.push(Layered { base, level, outline });
        self.seen.push(0);
        level - base
    }
}

/// A fill as an untextured flat quad (its colour as VRAM keeps it).
fn fill_primitive(rect: VramRect, rgb: u32, fifteen: bool) -> Primitive {
    let [r, g, b, _] = fill_color(rgb, fifteen).map(|c| (c * 255.0).round() as u32);
    let corner = |x: u16, y: u16| TraceVertex { x: i32::from(x), y: i32::from(y), fx: f32::from(x), fy: f32::from(y), color: r | g << 8 | b << 16, ..TraceVertex::default() };
    let (x1, y1) = (rect.x + rect.width, rect.y + rect.height);
    Primitive {
        shape: Shape::Quad,
        vertices: [corner(rect.x, rect.y), corner(x1, rect.y), corner(rect.x, y1), corner(x1, y1)],
        textured: false,
        raw: false,
        gouraud: false,
        semi: false,
        dither: false,
        clut: 0,
        environment: Environment { area: [rect.x, rect.y, x1.saturating_sub(1), y1.saturating_sub(1)], ..Environment::default() },
        class: Class::Flat,
        projection: None,
        owner: 0,
        placement: 0,
        source: 0,
        revealed: false,
        ground: None,
        facing: None,
        backdrop: false,
    }
}

impl Builder<'_> {
    /// `p` drawn into a diorama's eyes at `points` (in the game's world, or
    /// the table's space with `table`): its triangles as the face winds (a
    /// quad's second as 1-3-2), reversed with `flip`; front-facing only with
    /// `cull`; opaque pixels depth-tested and written, translucent ones
    /// tested only. In the table's space (the backdrop's wall, all of it in
    /// one plane) every pixel is tested only: the wall's pictures paint over
    /// one another in the game's order, as the PS1 draws them (written, the
    /// depth of the game's clear made the pictures drawn over it fight with
    /// it), and the scene stands in front of them all. A sprite's part
    /// (`billboard`) is lifted that far (world units) along the billboards'
    /// normal towards the side each eye sees (draw.wgsl `vs_world`). Fewer
    /// than three points draw nothing.
    fn world_primitive(&mut self, p: &Primitive, points: &[Vec3], table: bool, cull: bool, flip: bool, billboard: Option<f32>) {
        if points.len() < 3 {
            return;
        }
        let Attributes { mut mode, limits, .. } = self.attributes(p);
        mode[3] &= !FLAG_LINE;
        if table {
            mode[3] |= FLAG_TABLE;
        }
        if billboard.is_some() {
            mode[3] |= FLAG_BILLBOARD;
        }
        // `vs_world` takes no perspective weight: `w` carries the lift.
        let w = billboard.unwrap_or(0.0);
        let vertex = |i: usize| {
            let (point, v) = (points[i], &p.vertices[i]);
            Vertex { position: [point.x, point.y], depth: point.z, w, color: color(v.color), uv: [f32::from(v.u), f32::from(v.v)], mode, limits }
        };
        let triangles: &[[usize; 3]] = if points.len() == 4 { &[[0, 1, 2], [1, 3, 2]] } else { &[[0, 1, 2]] };
        let start = self.vertices.len() as u32;
        for &[a, b, c] in triangles {
            let order = if flip { [a, c, b] } else { [a, b, c] };
            self.vertices.extend(order.map(vertex));
        }
        self.stats.primitives += 1;
        self.stats.triangles += triangles.len() as u32;
        self.stats.revealed += u32::from(p.revealed);
        let end = self.vertices.len() as u32;
        let space = Space::World { cull };
        let blend = p.environment.draw_mode >> 5 & 3;
        let opaque = if table { DepthMode::Read } else { DepthMode::Test };
        if !p.semi {
            self.push_range(PipelineKey { fragment: Fragment::Opaque, blend: 0, depth: opaque, space }, None, start..end);
        } else {
            if p.textured {
                self.push_range(PipelineKey { fragment: Fragment::OpaqueTexels, blend: 0, depth: opaque, space }, None, start..end);
            }
            self.push_range(PipelineKey { fragment: Fragment::Semi, blend, depth: DepthMode::Read, space }, None, start..end);
        }
    }

    /// A full-screen fade over the eyes and the pane: `colors` at the
    /// target's corners (top left, top right, bottom left, bottom right),
    /// in `blend` mode when `semi`, else opaque.
    fn tint(&mut self, colors: [[f32; 4]; 4], semi: bool, blend: u32) {
        let flags = FLAG_CLIP | if semi { FLAG_SEMI | blend << BLEND_SHIFT } else { 0 };
        let corners = [(-1.0, 1.0), (1.0, 1.0), (-1.0, -1.0), (1.0, -1.0)];
        let vertex = |i: usize| Vertex { position: [corners[i].0, corners[i].1], depth: 0.0, w: 1.0, color: colors[i], uv: [0.0; 2], mode: [0, 0, 0, flags], limits: 0 };
        let start = self.vertices.len() as u32;
        self.vertices.extend([0, 1, 2, 1, 3, 2].map(vertex));
        let end = self.vertices.len() as u32;
        let blend = if semi { blend } else { 4 };
        for space in [Space::World { cull: false }, Space::Pane] {
            self.push_range(PipelineKey { fragment: Fragment::Tint, blend, depth: DepthMode::Over, space }, None, start..end);
        }
    }
}

/// The colours a primitive covering the screen gives a tint's corners: its
/// vertices' nearest each corner of its bounds.
fn tint_colors(p: &Primitive) -> [[f32; 4]; 4] {
    let vertices = &p.vertices[..p.vertex_count()];
    let (x0, x1) = vertices.iter().fold((f32::MAX, f32::MIN), |(a, b), v| (a.min(v.fx), b.max(v.fx)));
    let (y0, y1) = vertices.iter().fold((f32::MAX, f32::MIN), |(a, b), v| (a.min(v.fy), b.max(v.fy)));
    [(x0, y0), (x1, y0), (x0, y1), (x1, y1)].map(|(x, y)| {
        let nearest = vertices.iter().min_by(|a, b| ((a.fx - x).hypot(a.fy - y)).total_cmp(&(b.fx - x).hypot(b.fy - y))).expect("a vertex");
        color(nearest.color)
    })
}

/// A target a diorama draws into (an eye, or the pane): colour, auxiliary
/// and depth, and its pass uniform and bind group.
struct Target {
    size: (u32, u32),
    view: wgpu::TextureView,
    aux: wgpu::TextureView,
    depth: wgpu::TextureView,
    uniform: wgpu::Buffer,
    group: wgpu::BindGroup,
}

impl Target {
    fn new(device: &wgpu::Device, layout: &wgpu::BindGroupLayout, vram: &wgpu::Texture, capture: &Capture, size: (u32, u32)) -> Target {
        let color = texture(device, "diorama", size, COLOR_FORMAT, picture_usage());
        let aux = texture(device, "diorama aux", size, AUX_FORMAT, picture_usage());
        let depth = texture(device, "diorama depth", size, super::DEPTH_FORMAT, wgpu::TextureUsages::RENDER_ATTACHMENT);
        let uniform = uniform_buffer(device);
        let group = bind_group(device, layout, &uniform, vram, capture, None);
        Target {
            size,
            view: color.create_view(&Default::default()),
            aux: aux.create_view(&Default::default()),
            depth: depth.create_view(&Default::default()),
            uniform,
            group,
        }
    }

    /// Clears it: transparent, depth at the far plane.
    fn clear(&self, encoder: &mut wgpu::CommandEncoder) {
        let clear = |view| Some(wgpu::RenderPassColorAttachment {
            view,
            depth_slice: None,
            resolve_target: None,
            ops: wgpu::Operations { load: wgpu::LoadOp::Clear(wgpu::Color::TRANSPARENT), store: wgpu::StoreOp::Store },
        });
        encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("diorama clear"),
            color_attachments: &[clear(&self.view), clear(&self.aux)],
            depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                view: &self.depth,
                depth_ops: Some(wgpu::Operations { load: wgpu::LoadOp::Clear(1.0), store: wgpu::StoreOp::Store }),
                stencil_ops: None,
            }),
            ..Default::default()
        });
    }

    fn pass<'e>(&self, encoder: &'e mut wgpu::CommandEncoder) -> wgpu::RenderPass<'e> {
        let load = |view| Some(wgpu::RenderPassColorAttachment {
            view,
            depth_slice: None,
            resolve_target: None,
            ops: wgpu::Operations { load: wgpu::LoadOp::Load, store: wgpu::StoreOp::Store },
        });
        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("diorama"),
            color_attachments: &[load(&self.view), load(&self.aux)],
            depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                view: &self.depth,
                depth_ops: Some(wgpu::Operations { load: wgpu::LoadOp::Load, store: wgpu::StoreOp::Store }),
                stencil_ops: None,
            }),
            ..Default::default()
        });
        pass.set_bind_group(0, &self.group, &[]);
        pass
    }
}

/// A build of the frame shown, drawn again for each presentation's eyes.
struct Built {
    identity: (Option<((u16, u16), u64)>, usize),
    /// The table's heading and scale it was built for, and its billboards'
    /// normal in the world (towards the seat then).
    heading: f32,
    scale: f32,
    toward: Vec3,
    /// The VRAM the frame began with and its writes (each presentation's
    /// replay starts there again), the camera it was drawn with, its
    /// placement and display area.
    start: Arc<Vec<u16>>,
    writes: Vec<TraceOp>,
    camera: Camera,
    placed: Placed,
    area: DisplayArea,
    /// Built for the room around the table (its wall without the game's
    /// clear).
    room: bool,
    commands: Vec<Command>,
}

/// The diorama's GPU state, made when it first presents.
struct Targets {
    vram: ReplayVram,
    /// A stand-in for the bindings no diorama draw samples (screens the
    /// game took fall back to the screen).
    capture: Capture,
    vertices: Stream,
    staging: Stream,
    storage: Storage,
    built: Option<Built>,
    eyes: Vec<Target>,
    pane: Option<Target>,
}

impl Targets {
    fn new(device: &wgpu::Device) -> Targets {
        Targets {
            vram: ReplayVram::new(device, "diorama vram"),
            capture: Capture::new(device, (1, 1)),
            vertices: Stream::new(device, "diorama vertices", wgpu::BufferUsages::VERTEX),
            staging: Stream::new(device, "diorama staging", wgpu::BufferUsages::COPY_SRC),
            storage: Storage::default(),
            built: None,
            eyes: Vec::new(),
            pane: None,
        }
    }

    /// Builds the frame's diorama ([`build`]) into the vertex and staging
    /// buffers (the last build's no longer); returns its commands.
    #[allow(clippy::too_many_arguments)]
    fn build(&mut self, device: &wgpu::Device, queue: &wgpu::Queue, options: &DrawOptions, ops: &[TraceOp], camera: &Camera, placed: &Placed, area: DisplayArea, room: bool) -> Option<(Vec<Command>, DioramaStats)> {
        let mut builder = Builder::reusing(options, ops, &mut self.storage);
        let stats = build(&mut builder, ops, camera, placed, area, room);
        self.vertices.write(device, queue, bytemuck::cast_slice(&builder.vertices));
        self.staging.write(device, queue, &builder.staging);
        let commands = std::mem::take(&mut builder.commands);
        builder.store(&mut self.storage);
        Some((commands, stats?))
    }
}

/// Builds the diorama of a frame into `builder` (made for `ops`): `ops`
/// drawn with `camera` and placed as `placed`, the screen at `area`; with
/// `room` (the room shows around the table) the wall leaves out the game's
/// clear (its fills before the scene), which the room stands in for. `None`
/// when the camera's axes span no space.
fn build(builder: &mut Builder, ops: &[TraceOp], camera: &Camera, placed: &Placed, area: DisplayArea, room: bool) -> Option<DioramaStats> {
    let options = builder.options;
    let world = WorldFrame::new(camera)?;
    let scene = scene_offset(ops);
    let shown = VramRect { x: area.x as u16, y: area.y as u16, width: area.width as u16, height: area.height as u16 };
    // (A frame without a scene to place is not built: `Diorama::fallback`.)
    let first = first_scene(ops).unwrap_or(ops.len());
    // The way round the game's camera sees most of the faces it drew:
    // their front.
    let balance: i64 = ops.iter().map(|op| match op {
        TraceOp::Draw(p) if !p.revealed && matches!(p.class, Class::World | Class::Model) && matches!(p.shape, Shape::Triangle | Shape::Quad) => i64::from(facing(p)),
        _ => 0,
    }).sum();
    let front = if balance < 0 { -1 } else { 1 };
    // Emitted counter-clockwise seen from their front (docs/runtime.md,
    // Diorama: the transform keeps the world's handedness).
    let flip = front > 0;
    let feet = sprite_feet(ops);
    let lowest = sprite_bottoms(ops, &feet);
    // The game's order among faces in one plane (docs/runtime.md,
    // Diorama): the scene's faces, and the sprites' billboard parts.
    let (lift, tolerance) = (LIFT / placed.scale, COPLANAR / placed.scale);
    let (mut faces, mut parts) = (Layers::default(), Layers::default());
    let camera_at = world.point([0.0; 3]);
    let wall = Wall::new(area);
    let mut stats = DioramaStats::default();
    builder.shown = Some(shown);
    builder.space = Space::Pane;
    for (index, op) in ops.iter().enumerate() {
        match op {
            // Pictures uploaded to the screen go onto the pane; every
            // upload changes the VRAM the replay samples.
            TraceOp::Write { rect, pixels, upload } => {
                let show = *upload && overlap(&shown, rect) > 0;
                stats.pane += u32::from(show);
                builder.upload(*rect, pixels, show);
            }
            TraceOp::Fill { rect, color } => {
                let fill = fill_primitive(*rect, *color, options.fidelity.color_15bit);
                if index < first {
                    // The game's clear, on the wall unless the room around
                    // the table stands in for it.
                    if !room {
                        wall.draw(builder, &fill);
                        stats.wall += 1;
                    }
                } else if overlap(&shown, rect) * 10 >= u32::from(shown.width) * u32::from(shown.height) * 9 {
                    // Over (nine tenths of) the screen shown: a fade.
                    builder.tint(tint_colors(&fill), false, 0);
                    stats.tints += 1;
                } else {
                    builder.fill(*rect, *color);
                    stats.pane += 1;
                }
            }
            TraceOp::Draw(p) if p.backdrop => {
                // The backdrop the game projects far behind its scene (a
                // battle's sky): on the wall where its screen shows it, in
                // its order (but its culled faces).
                if !p.revealed {
                    wall.draw(builder, p);
                    stats.wall += 1;
                }
            }
            TraceOp::Draw(p) => {
                if effective_class(p, scene).is_scene() {
                    if !placeable(p) {
                        stats.unplaced += 1;
                        continue;
                    }
                    // Viewport culling keeps what the game drew.
                    if p.revealed && options.culling.viewport {
                        continue;
                    }
                    let count = p.vertex_count();
                    stats.scene += 1;
                    stats.revealed += u32::from(p.revealed);
                    if p.class == Class::Sprite {
                        let anchor = feet.get(&p.owner).copied().unwrap_or_default();
                        let base = world.point(anchor);
                        // Standing on the floor (its lowest part there), or
                        // above it as high as the game drew it (a jump).
                        let floor = lowest.get(&anchor.map(f32::to_bits)).copied().unwrap_or(0.0).max(0.0);
                        let points: Vec<Vec3> = (0..count).filter_map(|i| view_point(p, i)).map(|q| {
                            let (right, down) = (q[0] - anchor[0], q[1] - anchor[1]);
                            base + placed.right * (right / world.units) + placed.up * ((floor - down) / world.units)
                        }).collect();
                        // Lifted over the parts drawn before it in its
                        // billboard's plane, towards whichever side of it an
                        // eye sees (the game's order holds from behind too).
                        let raised = parts.place(placed.toward, &points, tolerance, lift, CELL);
                        stats.lifted += u32::from(raised > 0.0);
                        stats.thickest = stats.thickest.max((raised * placed.scale * 1e6).round() as u32);
                        builder.world_primitive(p, &points, false, false, false, Some(raised));
                        stats.sprites += 1;
                        continue;
                    }
                    let mut points: Vec<Vec3> = (0..count).filter_map(|i| view_point(p, i)).map(|q| world.point(q)).collect();
                    if p.shape == Shape::Line {
                        // A line as a ribbon a native pixel wide where the
                        // game's camera saw it, across its view.
                        let (Some(a), Some(b)) = (view_point(p, 0).map(Vec3::from), view_point(p, 1).map(Vec3::from)) else { continue };
                        let h = p.projection.map_or(256.0, |(_, h)| f32::from(h).max(1.0));
                        let half = 0.5 * ((a.z + b.z) / 2.0).abs().max(1.0) / h;
                        let side = world.linear * ((b - a).cross((a + b) / 2.0).normalize_or_zero() * half);
                        builder.world_primitive(&ribbon(p), &[points[0] - side, points[0] + side, points[1] - side, points[1] + side], false, false, false, None);
                        continue;
                    }
                    // Lifted over the faces it was drawn over in its plane,
                    // towards the side the game's camera saw it from.
                    let normal = (points[1] - points[0]).cross(points[2] - points[0]).normalize_or_zero();
                    if normal != Vec3::ZERO {
                        let normal = if normal.dot(camera_at - points[0]) < 0.0 { -normal } else { normal };
                        let raised = faces.place(normal, &points, tolerance, lift, CELL);
                        points.iter_mut().for_each(|point| *point += normal * raised);
                        stats.lifted += u32::from(raised > 0.0);
                    }
                    let cull = matches!(p.class, Class::World | Class::Model)
                        && matches!(p.shape, Shape::Triangle | Shape::Quad)
                        && (p.revealed || facing(p) == front);
                    stats.one_sided += u32::from(cull);
                    builder.world_primitive(p, &points, false, cull, flip && cull, None);
                } else if p.revealed {
                    // The backdrop's continuations beside the screen.
                    continue;
                } else if index < first {
                    wall.draw(builder, p);
                    stats.wall += 1;
                } else if !p.textured && p.shape != Shape::Line && builder.covers_screen(p) {
                    builder.tint(tint_colors(p), p.semi, p.environment.draw_mode >> 5 & 3);
                    stats.tints += 1;
                } else {
                    builder.primitive(p);
                    stats.pane += 1;
                }
            }
            _ => {}
        }
    }
    stats.runs = runs(&builder.commands);
    Some(stats)
}

/// Groups of draws between uploads in `commands`.
fn runs(commands: &[Command]) -> u32 {
    let mut runs = 0;
    let mut drawing = false;
    for command in commands {
        let draw = matches!(command, Command::Draw { .. });
        runs += u32::from(draw && !drawing);
        drawing = draw;
    }
    runs
}

/// Records `commands` into `encoder`: uploads into the replay's VRAM in
/// order, each group of draws between them into every eye (their space's
/// draws) and into the pane (the pane's), so each draw samples VRAM as the
/// game left it for that draw, in both eyes.
#[allow(clippy::too_many_arguments)]
fn encode(
    device: &wgpu::Device,
    pipelines: &mut Pipelines,
    encoder: &mut wgpu::CommandEncoder,
    vertices: &wgpu::Buffer,
    staging: &wgpu::Buffer,
    vram: &wgpu::Texture,
    eyes: &[Target],
    pane: &Target,
    (origin, scale): ((f32, f32), u32),
    commands: &[Command],
) {
    let world = |command: &Command| matches!(command, Command::Draw { key, .. } if matches!(key.space, Space::World { .. }));
    let paned = |command: &Command| matches!(command, Command::Draw { key, .. } if key.space == Space::Pane);
    let mut index = 0;
    while index < commands.len() {
        match &commands[index] {
            Command::Upload { offset, rect } => {
                upload(encoder, staging, vram, *offset, *rect);
                index += 1;
            }
            Command::Draw { .. } => {
                let end = commands[index..].iter().position(|c| !matches!(c, Command::Draw { .. })).map_or(commands.len(), |n| index + n);
                let run = &commands[index..end];
                if run.iter().any(world) {
                    for eye in eyes {
                        let mut pass = eye.pass(encoder);
                        pass.set_vertex_buffer(0, vertices.slice(..));
                        for command in run.iter().filter(|c| world(c)) {
                            let Command::Draw { key, vertices, .. } = command else { continue };
                            pass.set_pipeline(pipelines.get(device, *key));
                            pass.draw(vertices.clone(), 0..1);
                        }
                    }
                }
                if run.iter().any(paned) {
                    let size = wgpu::Extent3d { width: pane.size.0, height: pane.size.1, depth_or_array_layers: 1 };
                    let mut pass = pane.pass(encoder);
                    pass.set_vertex_buffer(0, vertices.slice(..));
                    for command in run.iter().filter(|c| paned(c)) {
                        let Command::Draw { key, scissor, vertices } = command else { continue };
                        let (x, y, w, h) = scissor_rect(scissor.as_ref(), origin, scale, size);
                        if w > 0 && h > 0 {
                            pass.set_pipeline(pipelines.get(device, *key));
                            pass.set_scissor_rect(x, y, w, h);
                            pass.draw(vertices.clone(), 0..1);
                        }
                    }
                }
                index = end;
            }
            _ => index += 1,
        }
    }
}

impl Diorama {
    /// Why the frame shown cannot be a diorama, if it cannot: `scene` is the
    /// session's scene that drew it.
    pub(crate) fn fallback(renderer: &Ps1Renderer, frame: &FrameRenderer, scene: &str, area: DisplayArea) -> Option<Fallback> {
        if !["field", "battle", "worldmap"].contains(&scene.split(':').next().unwrap_or_default()) {
            return Some(Fallback::Screen);
        }
        let ops = frame.newest();
        // What the game draws of a screen it took, or of the frame itself
        // (its own picture as a texture: the field's heat haze).
        let captures: Vec<VramRect> = std::iter::once(&renderer.capture).chain(&renderer.earlier).flat_map(Capture::areas).map(|(rect, _)| rect).collect();
        if ops.iter().any(|op| matches!(op, TraceOp::Draw(p) if captures.iter().any(|rect| samples(p, rect)))) {
            return Some(Fallback::Screen);
        }
        if let Some(log) = renderer.frame_log(area)
            && framebuffer_effect(ops, log.rect)
        {
            return Some(Fallback::Screen);
        }
        // No camera record made for a view from anywhere (or one whose axes
        // span no space), or no scene to place: a frame recorded before the
        // diorama asked (its faces culled as the game's camera sees them),
        // or one the game draws over the picture before without drawing its
        // scene (the field menu's opening fade over the field, its camera
        // the one recorded within the four vertical blanks before), holds no
        // diorama of its own.
        let camera = frame.camera().filter(|camera| camera.around & Reveals::ANYWHERE != 0 && WorldFrame::new(camera).is_some());
        if camera.is_none() || frame.snapshot().is_none() || first_scene(ops).is_none() {
            return Some(Fallback::Moment);
        }
        None
    }

    /// The flat screen's share of the view at `now`, eased towards the
    /// screen when the frame shown cannot be a diorama (`fallback`), else
    /// towards the diorama; and whether to build the frame shown. While a
    /// frame cannot be one, the last build stands: held (a frame without a
    /// camera record or a scene, `Fallback::Moment`, keeps it for [`MISSES`]
    /// game frames in a row) or fading out as the screen eases in.
    pub(crate) fn ease(&mut self, fallback: Option<Fallback>, new_frame: bool, now: f64) -> (f32, bool) {
        if new_frame {
            self.misses = if fallback == Some(Fallback::Moment) { self.misses + 1 } else { 0 };
        }
        let hold = fallback == Some(Fallback::Moment) && self.misses <= MISSES && self.gpu.as_ref().is_some_and(|gpu| gpu.built.is_some());
        let screen = fallback.is_some() && !hold;
        let dt = self.eased.map_or(0.0, |at| (now - at).clamp(0.0, 1.0)) as f32;
        self.eased = Some(now);
        self.screen = if screen { (self.screen + dt / SWITCH).min(1.0) } else { (self.screen - dt / SWITCH).max(0.0) };
        (self.screen, fallback.is_none())
    }

    /// The diorama is off: it begins again from the screen next time.
    pub(crate) fn reset(&mut self) {
        let gpu = self.gpu.take();
        *self = Diorama { gpu, ..Diorama::default() };
        if let Some(gpu) = &mut self.gpu {
            gpu.built = None;
        }
    }

    /// Draws a diorama for `eyes` (each from the room through its view and
    /// projection) into a picture of each eye's own (cleared to nothing
    /// first: [`Diorama::eyes`]), and the pane; `seat` is the seat's pose in
    /// the room. With `shown` (the frame shown, its display area, the scene
    /// that drew it and whether it is a new game frame), the frame shown's
    /// (built once a game frame); without, the last build (held, or fading
    /// out); with `room` (the room shows around the table) built without the
    /// game's clear on its wall. Returns the opacity to draw the eyes'
    /// pictures at (`opacity`, dimmed briefly after a jump); `None` when
    /// there is nothing to draw.
    #[allow(clippy::too_many_arguments)]
    pub(crate) fn present(
        &mut self,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        encoder: &mut wgpu::CommandEncoder,
        renderer: &mut Ps1Renderer,
        shown: Option<(&FrameRenderer, DisplayArea, &str, bool)>,
        now: f64,
        eyes: &[DioramaEye],
        seat: Mat4,
        table: &Table,
        opacity: f32,
        room: bool,
    ) -> Option<f32> {
        let gpu = self.gpu.get_or_insert_with(|| Targets::new(device));
        if let Some((frame, area, scene, new_frame)) = shown {
            let (Some(camera), Some(start)) = (frame.camera(), frame.snapshot().cloned()) else { return None };
            let ops = frame.newest();
            let h = projection_distance(ops, scene_offset(ops)).unwrap_or(256.0);
            let placed = self.placement.update(&camera, h, scene, new_frame, now, table)?;
            let identity = (frame.identity(), frame.drawn());
            let rebuild = gpu.built.as_ref().is_none_or(|built| {
                built.identity != identity || built.area != area || built.room != room || wrap(built.heading - placed.heading).abs() > REBUILD_TURN || (built.scale / placed.scale - 1.0).abs() > 0.25
            });
            if rebuild {
                let base = *renderer.options();
                // The screen's limits have no place in a view from anywhere.
                let fidelity = Fidelity { affine_textures: false, vertex_snapping: false, dithering: false, native_resolution: false, painter_ordering: true, ..base.fidelity };
                let options = DrawOptions { fidelity, ..base };
                let Some((commands, stats)) = gpu.build(device, queue, &options, ops, &camera, &placed, area, room) else {
                    // (Its buffers hold the last build no longer.)
                    gpu.built = None;
                    return None;
                };
                self.stats = DioramaStats { builds: self.stats.builds + 1, ..stats };
                let writes = ops.iter().filter(|op| matches!(op, TraceOp::Write { .. })).cloned().collect();
                gpu.built = Some(Built { identity, heading: placed.heading, scale: placed.scale, toward: placed.toward, start, writes, camera, placed, area, room, commands });
            } else if let Some(built) = &mut gpu.built {
                built.placed = placed;
            }
        }
        let built = gpu.built.as_ref()?;
        let placed = built.placed;
        let room_from_table = table.room_from_table(seat);
        let world_from_room = (room_from_table * placed.table_from_world).inverse();
        // The targets: one per eye at its size, and the pane at the
        // picture's scale.
        let layout = &renderer.pipelines.layout;
        let sizes: Vec<(u32, u32)> = eyes.iter().map(|eye| (eye.size.0.max(1), eye.size.1.max(1))).collect();
        if gpu.eyes.len() != sizes.len() || gpu.eyes.iter().zip(&sizes).any(|(eye, size)| eye.size != *size) {
            gpu.eyes = sizes.iter().map(|&size| Target::new(device, layout, &gpu.vram.texture, &gpu.capture, size)).collect();
        }
        let scale = renderer.scale();
        let pane_size = (built.area.width * scale, built.area.height * scale);
        if gpu.pane.as_ref().is_none_or(|pane| pane.size != pane_size) {
            gpu.pane = Some(Target::new(device, layout, &gpu.vram.texture, &gpu.capture, pane_size));
        }
        let fifteen = u32::from(renderer.options().fidelity.color_15bit) << 1;
        for (eye, target) in eyes.iter().zip(&gpu.eyes) {
            let room_to_clip = eye.proj * eye.view * room_from_table;
            // Where the eye is in the game's world (which side of each
            // billboard it sees).
            let eye_point = world_from_room.transform_point3(eye.view.inverse().transform_point3(Vec3::ZERO));
            let uniform = PassUniform {
                extent: [1.0, 1.0],
                scale: 1.0,
                // Perspective-correct textures, and the 15-bit limit.
                options: PASS_WORLD | 1 | fifteen,
                world_to_clip: (room_to_clip * placed.table_from_world).to_cols_array_2d(),
                table_to_clip: room_to_clip.to_cols_array_2d(),
                world_to_table: placed.table_from_world.to_cols_array_2d(),
                volume_min: VOLUME_MIN.extend(0.0).to_array(),
                volume_max: VOLUME_MAX.extend(0.0).to_array(),
                eye: eye_point.extend(1.0).to_array(),
                billboard: built.toward.extend(0.0).to_array(),
                ..bytemuck::Zeroable::zeroed()
            };
            queue.write_buffer(&target.uniform, 0, bytemuck::bytes_of(&uniform));
        }
        let pane = gpu.pane.as_ref().expect("made above");
        let origin = (built.area.x as f32, built.area.y as f32);
        let uniform = PassUniform {
            origin: [origin.0, origin.1],
            extent: [built.area.width as f32, built.area.height as f32],
            scale: scale as f32,
            options: pass_options(&renderer.options().fidelity) | PASS_PANE,
            ..bytemuck::Zeroable::zeroed()
        };
        queue.write_buffer(&pane.uniform, 0, bytemuck::bytes_of(&uniform));
        // Each presentation replays the frame from the VRAM it began with.
        gpu.vram.start(queue, &built.start, built.writes.iter());
        for target in gpu.eyes.iter().chain(std::iter::once(pane)) {
            target.clear(encoder);
        }
        encode(device, &mut renderer.pipelines, encoder, gpu.vertices.buffer(), gpu.staging.buffer(), &gpu.vram.texture, &gpu.eyes, pane, (origin, scale), &built.commands);
        // The pane floats above the table's far edge, facing the seat.
        let display = Vec2::new(built.area.width as f32, built.area.height as f32);
        self.space = Some(DioramaSpace::of(&placed, &built.camera, display, table, seat));
        // The eyes' pictures are drawn faded, dimmed briefly after a jump.
        Some(opacity * (1.0 - DIM_DEPTH * self.placement.dim(now)))
    }

    /// Each eye's picture the last [`Diorama::present`] drew.
    pub(crate) fn eyes(&self) -> Vec<&wgpu::TextureView> {
        self.gpu.as_ref().map_or_else(Vec::new, |gpu| gpu.eyes.iter().map(|eye| &eye.view).collect())
    }

    /// Eye `eye`'s auxiliary picture the last [`Diorama::present`] drew:
    /// each pixel's class ([`super::AUX_FORMAT`]'s blue, `AUX_*`; the
    /// background where nothing of the scene is drawn).
    pub(crate) fn eye_aux(&self, eye: usize) -> Option<&wgpu::TextureView> {
        Some(&self.gpu.as_ref()?.eyes.get(eye)?.aux)
    }

    /// The table takes the camera's point and heading at once at the next
    /// presentation, as a new scene's does (a recentring, a reference space
    /// the runtime moved): no easing from where it stood. A battle's table
    /// stays where its stage put it.
    pub(crate) fn recenter(&mut self) {
        let placement = &mut self.placement;
        (placement.target, placement.heading, placement.last) = (None, None, None);
    }

    /// The last presentation showed no diorama: the screen alone, from which
    /// the diorama eases in again.
    pub(crate) fn hide(&mut self) {
        self.undrawn();
        self.space = None;
        self.screen = 1.0;
    }

    /// Nothing drew the diorama this time (first-person inspection's eyes or
    /// the screen alone): drawn again, the table eases on from where it
    /// stood, its first step none, not one for the time it was not drawn.
    pub(crate) fn undrawn(&mut self) {
        self.placement.at = None;
    }

    /// Nothing presented the diorama this time (the host showed something
    /// else): the screen alone, from which the diorama eases in once
    /// presented again (not by the time it was away).
    pub(crate) fn away(&mut self) {
        self.hide();
        self.eased = None;
    }

    /// The pane the last presentation drew.
    pub(crate) fn pane(&self) -> Option<&wgpu::TextureView> {
        self.gpu.as_ref()?.pane.as_ref().map(|pane| &pane.view)
    }

    pub(crate) fn space(&self) -> Option<DioramaSpace> {
        self.space
    }

    pub(crate) fn stats(&self) -> DioramaStats {
        self.stats
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A camera at the world's origin looking along +z (view = world), at a
    /// point on the floor 1200 ahead.
    const LEVEL: Camera = Camera {
        axes: [[4096.0, 0.0, 0.0], [0.0, 4096.0, 0.0], [0.0, 0.0, 4096.0]],
        translation: [0.0; 3],
        target: [0.0, 200.0, 1200.0],
        focus: 1200.0,
        mode: CameraMode::Field,
        stage: None,
        ground: None,
        origin: [0.0; 3],
        around: 0,
    };

    const AREA: DisplayArea = DisplayArea { x: 0, y: 0, width: 320, height: 240 };

    /// A primitive at view-space `points` (offset 160, 120; H 256; a quad's in
    /// Z order) of `class`, owned by `owner`; flat (no projection) when the
    /// points are its screen points, `flat`.
    fn primitive(points: &[[f32; 3]], class: Class, owner: u32, flat: bool) -> Primitive {
        let mut p = fill_primitive(VramRect { x: 0, y: 0, width: 1, height: 1 }, 0x80_8080, false);
        p.shape = if points.len() == 4 { Shape::Quad } else { Shape::Triangle };
        p.class = class;
        p.owner = owner;
        p.environment.area = [0, 0, 319, 239];
        p.projection = (!flat).then_some(((160, 120), 256));
        for (v, &[x, y, z]) in p.vertices.iter_mut().zip(points) {
            let (sx, sy) = if flat { (x, y) } else { (160.0 + 256.0 * x / z, 120.0 + 256.0 * y / z) };
            *v = TraceVertex { x: sx as i32, y: sy as i32, fx: sx, fy: sy, z: if flat { 0.0 } else { z }, view: (!flat).then_some([x, y, z]), color: 0x80_8080, u: 0, v: 0 };
        }
        p
    }

    fn placed() -> Placed {
        Placement::default().update(&LEVEL, 256.0, "field:1", true, 0.0, &Table::default()).expect("a placement")
    }

    /// The diorama `ops` build: its vertices and commands.
    fn built(ops: &[TraceOp]) -> (Vec<Vertex>, Vec<Command>, DioramaStats) {
        built_for(ops, false)
    }

    /// The diorama `ops` build with the room around the table or not.
    fn built_for(ops: &[TraceOp], room: bool) -> (Vec<Vertex>, Vec<Command>, DioramaStats) {
        let options = DrawOptions::default();
        let mut builder = Builder::new(&options, ops);
        let stats = build(&mut builder, ops, &LEVEL, &placed(), AREA, room).expect("a camera");
        (builder.vertices, builder.commands, stats)
    }

    /// The draws of `commands` over vertices `range`.
    fn draws(commands: &[Command], range: std::ops::Range<u32>) -> Vec<PipelineKey> {
        commands.iter().filter_map(|command| match command {
            Command::Draw { key, vertices, .. } if vertices.start < range.end && range.start < vertices.end => Some(*key),
            _ => None,
        }).collect()
    }

    const FLOOR: [[f32; 3]; 4] = [[-300.0, 200.0, 2000.0], [300.0, 200.0, 2000.0], [-300.0, 200.0, 400.0], [300.0, 200.0, 400.0]];

    #[test]
    fn backdrops_lie_on_one_wall_and_the_interface_goes_on_the_pane_only() {
        let fill = TraceOp::Fill { rect: VramRect { x: 0, y: 0, width: 320, height: 240 }, color: 0x40_2010 };
        // A panorama strip before the scene, the floor, then a window.
        let strip = primitive(&[[0.0, 0.0, 0.0], [320.0, 0.0, 0.0], [0.0, 60.0, 0.0], [320.0, 40.0, 0.0]], Class::Flat, 0, true);
        let window = primitive(&[[20.0, 160.0, 0.0], [300.0, 160.0, 0.0], [20.0, 230.0, 0.0], [300.0, 230.0, 0.0]], Class::Flat, 0, true);
        let ops = [fill, TraceOp::Draw(strip), TraceOp::Draw(primitive(&FLOOR, Class::Model, 1, false)), TraceOp::Draw(window)];
        let (vertices, commands, stats) = built(&ops);
        assert_eq!((stats.wall, stats.scene, stats.pane), (2, 1, 1));
        // The fill's and the strip's vertices: on the table's back wall.
        let wall: Vec<&Vertex> = vertices.iter().filter(|v| v.mode[3] & FLAG_TABLE != 0).collect();
        assert_eq!(wall.len(), 12, "two quads on the wall");
        assert!(wall.iter().all(|v| v.depth == VOLUME_MIN.z + 0.002), "every backdrop vertex on one plane");
        assert!(wall.iter().all(|v| v.position[0] >= VOLUME_MIN.x - 1e-6 && v.position[0] <= VOLUME_MAX.x + 1e-6), "across the volume");
        // The window: drawn on the pane, never into the eyes.
        let last = commands.iter().rev().find_map(|command| match command {
            Command::Draw { key, vertices, .. } if key.space == Space::Pane => Some(vertices.clone()),
            _ => None,
        }).expect("the window on the pane");
        assert!(draws(&commands, last).iter().all(|key| key.space == Space::Pane), "the interface never enters the eyes' pass");
        assert!(commands.iter().all(|command| !matches!(command, Command::Draw { key, .. } if matches!(key.space, Space::World { .. }) && key.fragment == Fragment::Rect)));
    }

    #[test]
    fn a_backdrop_projected_far_behind_the_scene_and_the_pictures_after_it_lie_on_the_wall() {
        // A battle's sky (the Mountain Path's): the clear, a band, the sky
        // projected 14 000 deep, then its panorama strip, then the scene.
        let fill = TraceOp::Fill { rect: VramRect { x: 0, y: 0, width: 320, height: 240 }, color: 0x5A_8CE7 };
        let band = primitive(&[[0.0, 0.0, 0.0], [320.0, 0.0, 0.0], [0.0, 29.0, 0.0], [320.0, 29.0, 0.0]], Class::Flat, 0, true);
        let mut sky = primitive(&[[-9000.0, -6000.0, 14000.0], [9000.0, -6000.0, 14000.0], [-9000.0, -4000.0, 14000.0], [9000.0, -4000.0, 14000.0]], Class::World, 0, false);
        sky.backdrop = true;
        let strip = primitive(&[[0.0, 0.0, 0.0], [320.0, 0.0, 0.0], [0.0, 120.0, 0.0], [320.0, 120.0, 0.0]], Class::Flat, 0, true);
        let ops = [fill, TraceOp::Draw(band), TraceOp::Draw(sky.clone()), TraceOp::Draw(strip), TraceOp::Draw(primitive(&FLOOR, Class::Model, 1, false))];
        assert_eq!(first_scene(&ops), Some(4), "the scene begins after the sky and its pictures");
        let (vertices, _, stats) = built(&ops);
        assert_eq!((stats.wall, stats.scene, stats.pane), (4, 1, 0), "the clear, the band, the sky and the strip on the wall; nothing on the pane");
        // The sky where its screen points show it on the wall, in its plane.
        let wall: Vec<&Vertex> = vertices.iter().filter(|v| v.mode[3] & FLAG_TABLE != 0).collect();
        assert_eq!(wall.len(), 24);
        assert!(wall.iter().all(|v| v.depth == VOLUME_MIN.z + 0.002));
        let at = Wall::new(AREA).point(sky.vertices[0].fx, sky.vertices[0].fy);
        assert!(wall[12..18].iter().any(|v| (Vec3::new(v.position[0], v.position[1], v.depth) - at).length() < 1e-6), "the sky's first corner at its screen point");
        // Its culled faces are left out.
        sky.revealed = true;
        let (_, _, stats) = built(&[TraceOp::Draw(sky), TraceOp::Draw(primitive(&FLOOR, Class::Model, 1, false))]);
        assert_eq!((stats.wall, stats.scene), (0, 1));
    }

    #[test]
    fn where_the_room_shows_the_wall_leaves_out_the_games_clear_and_keeps_its_pictures() {
        let screen = VramRect { x: 0, y: 0, width: 320, height: 240 };
        // The clear and a panorama strip before the scene, the floor, then a
        // fade over the screen.
        let strip = primitive(&[[0.0, 0.0, 0.0], [320.0, 0.0, 0.0], [0.0, 60.0, 0.0], [320.0, 40.0, 0.0]], Class::Flat, 0, true);
        let ops = [
            TraceOp::Fill { rect: screen, color: 0x40_2010 },
            TraceOp::Draw(strip),
            TraceOp::Draw(primitive(&FLOOR, Class::Model, 1, false)),
            TraceOp::Fill { rect: screen, color: 0x10_1010 },
        ];
        let on_wall = |vertices: &[Vertex]| vertices.iter().filter(|v| v.mode[3] & FLAG_TABLE != 0).map(|v| v.position[1]).collect::<Vec<f32>>();
        let (vertices, _, stats) = built_for(&ops, false);
        assert_eq!((stats.wall, stats.scene, stats.tints, on_wall(&vertices).len()), (2, 1, 1, 12), "in VR the clear and the strip");
        let (vertices, _, stats) = built_for(&ops, true);
        assert_eq!((stats.wall, stats.scene, stats.tints), (1, 1, 1), "the strip alone on the wall; the scene and the fade as they were");
        // The strip's quad, as high as it is on the screen (not the clear's,
        // down to the wall's foot).
        let wall = on_wall(&vertices);
        let foot = Wall::new(AREA).point(0.0, 60.0).y;
        assert!(wall.len() == 6 && wall.iter().all(|y| *y >= foot - 1e-6), "{wall:?} above {foot}");
    }

    #[test]
    fn a_sprites_parts_stand_upright_at_its_floor_point_facing_the_seat() {
        let sprite = 0x8010_0000;
        let mut first = primitive(&[[-30.0, 100.0, 1000.0], [30.0, 100.0, 1000.0], [-30.0, 200.0, 1000.0], [30.0, 200.0, 1000.0]], Class::Sprite, sprite, false);
        // Its floor point: (0, 200, 1000), as VRAM x, y and depth.
        first.ground = Some([160.0, 120.0 + 256.0 * 200.0 / 1000.0, 1000.0]);
        let mut second = primitive(&[[0.0, 120.0, 1000.0], [40.0, 120.0, 1000.0], [0.0, 180.0, 1000.0], [40.0, 180.0, 1000.0]], Class::Sprite, sprite, false);
        second.ground = first.ground;
        let (vertices, _, stats) = built(&[TraceOp::Draw(first), TraceOp::Draw(second)]);
        assert_eq!((stats.sprites, stats.lifted), (2, 1), "the second part, drawn over the first, is lifted");
        let placed = placed();
        let room = |v: &Vertex| placed.table_from_world.transform_point3(Vec3::new(v.position[0], v.position[1], v.depth));
        let corners: Vec<Vec3> = vertices.iter().map(room).collect();
        // Upright, facing the seat: the parts' corners share their z (the
        // table's towards-the-seat axis) and stand above the floor point.
        assert!(corners.iter().all(|c| (c.z - corners[0].z).abs() < 1e-6), "the parts lie in one upright plane facing the seat: {corners:?}");
        let floor = room(&Vertex { position: [0.0, 200.0], depth: 1000.0, ..Vertex::default() });
        let bottom = corners[..6].iter().map(|c| c.y).fold(f32::MAX, f32::min);
        assert!((bottom - floor.y).abs() < 1e-6, "the first part stands on its floor point");
        // The second is lifted off that plane by the lift (in world units,
        // towards whichever side an eye sees: draw.wgsl `vs_world`).
        assert!(vertices.iter().all(|v| v.mode[3] & FLAG_BILLBOARD != 0), "billboard parts");
        assert!(vertices[..6].iter().all(|v| v.w == 0.0), "the first part is not lifted");
        assert!(vertices[6..].iter().all(|v| (v.w * placed.scale - LIFT).abs() < 1e-6), "the second part is lifted by the lift: {}", vertices[6].w * placed.scale);
        // As tall as the game drew it: 100 view units (the field's scale 1).
        let top = corners[..6].iter().map(|c| c.y).fold(f32::MIN, f32::max);
        assert!(((top - bottom) / placed.scale - 100.0).abs() < 1e-2);
    }

    #[test]
    fn sprites_stand_on_their_floor_point_together_or_above_it_when_raised() {
        // A character in two sprites whose feet reach 20 below their floor
        // point at (0, 180, 1000), as the game draws them facing its camera,
        // and a third sprite jumping 60 above its floor point.
        let (a, b, jumper) = (0x8010_0000, 0x8010_0100, 0x8010_0200);
        let ground = |y: f32| Some([160.0, 120.0 + 256.0 * y / 1000.0, 1000.0]);
        let part = |x: f32, top: f32, bottom: f32, owner: u32, floor: f32| {
            let mut p = primitive(&[[x - 20.0, top, 1000.0], [x + 20.0, top, 1000.0], [x - 20.0, bottom, 1000.0], [x + 20.0, bottom, 1000.0]], Class::Sprite, owner, false);
            p.ground = ground(floor);
            TraceOp::Draw(p)
        };
        let ops = [part(0.0, 100.0, 190.0, a, 180.0), part(0.0, 150.0, 200.0, b, 180.0), part(300.0, 100.0, 200.0, jumper, 260.0)];
        let (vertices, _, _) = built(&ops);
        let placed = placed();
        let floor = |y: f32| placed.table_from_world.transform_point3(Vec3::new(0.0, y, 1000.0)).y;
        let bottom = |range: std::ops::Range<usize>| vertices[range].iter()
            .map(|v| placed.table_from_world.transform_point3(Vec3::new(v.position[0], v.position[1], v.depth)).y).fold(f32::MAX, f32::min);
        assert!((bottom(6..12) - floor(180.0)).abs() < 1e-6, "the character's lowest part stands on its floor point");
        assert!((bottom(0..6) - (floor(180.0) + 10.0 * placed.scale)).abs() < 1e-6, "its other sprite keeps its place on it, 10 higher");
        assert!((bottom(12..18) - (floor(260.0) + 60.0 * placed.scale)).abs() < 1e-6, "a jumping sprite stays 60 above its floor point");
    }

    #[test]
    fn faces_drawn_over_others_in_their_plane_are_lifted_towards_the_camera() {
        let floor = primitive(&FLOOR, Class::Model, 1, false);
        // A shadow inside the floor, and a floor tile beside it that only
        // shares an edge.
        let shadow = primitive(&[[-50.0, 200.0, 1100.0], [50.0, 200.0, 1100.0], [-50.0, 200.0, 900.0], [50.0, 200.0, 900.0]], Class::World, 0, false);
        let beside = primitive(&[[300.0, 200.0, 2000.0], [600.0, 200.0, 2000.0], [300.0, 200.0, 400.0], [600.0, 200.0, 400.0]], Class::Model, 1, false);
        let (vertices, _, stats) = built(&[TraceOp::Draw(floor), TraceOp::Draw(shadow), TraceOp::Draw(beside)]);
        assert_eq!(stats.lifted, 1, "only the shadow is lifted");
        let placed = placed();
        let height = |range: std::ops::Range<usize>| vertices[range].iter().map(|v| placed.table_from_world.transform_point3(Vec3::new(v.position[0], v.position[1], v.depth)).y).sum::<f32>() / 6.0;
        let (floor, shadow, beside) = (height(0..6), height(6..12), height(12..18));
        assert!((shadow - floor - LIFT).abs() < 1e-6, "the shadow lies the lift above the floor, towards the camera ({floor} {shadow})");
        assert!((beside - floor).abs() < 1e-6, "a face beside it keeps its place");
    }

    #[test]
    fn faces_the_game_shows_from_their_front_or_culled_are_drawn_front_facing_only() {
        // Three faces seen from their front (the majority), one wound the
        // other way that the game drew all the same (double-sided), one it
        // culled facing away, and a sprite.
        let front = primitive(&FLOOR, Class::Model, 1, false);
        let wall = [[-100.0, 0.0, 1500.0], [100.0, 0.0, 1500.0], [-100.0, 200.0, 1500.0], [100.0, 200.0, 1500.0]];
        let back = [wall[1], wall[0], wall[3], wall[2]];
        let mut culled = primitive(&back, Class::Model, 1, false);
        culled.revealed = true;
        let mut sprite = primitive(&[[-30.0, 100.0, 1000.0], [30.0, 100.0, 1000.0], [-30.0, 200.0, 1000.0], [30.0, 200.0, 1000.0]], Class::Sprite, 9, false);
        sprite.ground = Some([160.0, 171.2, 1000.0]);
        let ops = [front.clone(), primitive(&wall, Class::Model, 1, false), primitive(&wall.map(|[x, y, z]| [x, y, z + 100.0]), Class::World, 1, false),
            primitive(&back.map(|[x, y, z]| [x, y, z + 200.0]), Class::Model, 1, false), culled, sprite].map(TraceOp::Draw);
        let (_, commands, stats) = built(&ops);
        let culls = |face: u32| {
            let keys = draws(&commands, face * 6..face * 6 + 6);
            keys.iter().all(|key| key.space == Space::World { cull: true })
        };
        assert!(culls(0) && culls(1) && culls(2), "faces the game showed from their front");
        assert!(!culls(3), "a face the game drew from behind is drawn either way");
        assert!(culls(4), "a face the game culled");
        assert!(!culls(5), "a sprite");
        assert_eq!(stats.one_sided, 4);
    }

    #[test]
    fn a_fill_over_part_of_the_screen_after_the_scene_goes_on_the_pane_not_over_the_eyes() {
        // A letterbox band and a window's fill after the scene, then a fill
        // over the whole screen (a fade to black).
        let band = TraceOp::Fill { rect: VramRect { x: 0, y: 224, width: 320, height: 16 }, color: 0 };
        let window = TraceOp::Fill { rect: VramRect { x: 20, y: 160, width: 280, height: 60 }, color: 0x80_2010 };
        let fade = TraceOp::Fill { rect: VramRect { x: 0, y: 0, width: 320, height: 240 }, color: 0 };
        let (_, commands, stats) = built(&[TraceOp::Draw(primitive(&FLOOR, Class::Model, 1, false)), band, window, fade]);
        assert_eq!((stats.scene, stats.pane, stats.tints), (1, 2, 1), "the band and the window on the pane, the fade a tint");
        let fills: Vec<(PipelineKey, u32)> = commands.iter().filter_map(|command| match command {
            Command::Draw { key, vertices, .. } if key.fragment == Fragment::Rect => Some((*key, vertices.len() as u32)),
            _ => None,
        }).collect();
        assert_eq!(fills.iter().map(|(_, n)| n).sum::<u32>(), 12, "two fills' rectangles");
        assert!(fills.iter().all(|(key, _)| key.space == Space::Pane), "never in the eyes' pass");
    }

    #[test]
    fn lines_on_the_wall_are_ribbons_and_a_frame_without_a_scene_builds_without_one() {
        // A flat line (LINE_F2) before the scene: a ribbon a native pixel
        // wide on the backdrop's wall.
        let mut line = primitive(&[[40.0, 30.0, 0.0], [280.0, 30.0, 0.0]], Class::Flat, 0, true);
        line.shape = Shape::Line;
        let (vertices, _, stats) = built(&[TraceOp::Draw(line.clone()), TraceOp::Draw(primitive(&FLOOR, Class::Model, 1, false))]);
        assert_eq!((stats.wall, stats.scene), (1, 1));
        let wall: Vec<&Vertex> = vertices.iter().filter(|v| v.mode[3] & FLAG_TABLE != 0).collect();
        assert_eq!(wall.len(), 6, "a quad");
        let metres = (VOLUME_MAX.x - VOLUME_MIN.x) / 320.0;
        let ys = wall.iter().map(|v| v.position[1]);
        let height = ys.clone().fold(f32::MIN, f32::max) - ys.fold(f32::MAX, f32::min);
        assert!((height - metres).abs() < 1e-6, "a native pixel wide: {height} m");
        // No scene to place (the build is not asked for one, but makes
        // nothing of its lines).
        let (_, _, stats) = built(&[TraceOp::Draw(line)]);
        assert_eq!((stats.wall, stats.scene), (1, 0));
    }

    /// A field camera at the world's origin turned `yaw` degrees about the
    /// world's up from looking along +z, pitched `pitch` degrees down and
    /// rolled `roll`, looking at `target` (its depth 1200).
    fn camera(yaw: f32, pitch: f32, roll: f32, target: [f32; 3], mode: CameraMode) -> Camera {
        // view = R · world; R's rows are the camera's axes in the world.
        let r = Mat3::from_rotation_z(roll.to_radians()) * Mat3::from_rotation_x(pitch.to_radians()) * Mat3::from_rotation_y(-yaw.to_radians());
        let columns = (r * 4096.0).to_cols_array_2d();
        Camera { axes: columns, translation: [0.0; 3], target, focus: 1200.0, mode, stage: None, ground: None, origin: [0.0; 3], around: 0 }
    }

    /// The world's direction `d` on the table.
    fn on_table(placed: &Placed, d: Vec3) -> Vec3 {
        placed.table_from_world.transform_vector3(d).normalize()
    }

    #[test]
    fn the_table_stays_level_under_a_pitched_or_rolled_camera() {
        let table = Table::default();
        for (pitch, roll) in [(0.0, 0.0), (35.0, 0.0), (-20.0, 0.0), (30.0, 12.0)] {
            let placed = Placement::default().update(&camera(20.0, pitch, roll, [0.0, 200.0, 1200.0], CameraMode::Field), 256.0, "field:1", true, 0.0, &table).expect("a placement");
            // The world's up (y down) is the table's.
            let up = on_table(&placed, Vec3::new(0.0, -1.0, 0.0));
            assert!((up - Vec3::Y).length() < 1e-5, "pitch {pitch}, roll {roll}: the world's up on the table is {up}");
            assert!(wrap(placed.heading - 20f32.to_radians()).abs() < 1e-4, "pitch {pitch}, roll {roll}: heading {}", placed.heading.to_degrees());
            // The camera's level axis points away from the seat (-z).
            let axis = on_table(&placed, Vec3::new(20f32.to_radians().sin(), 0.0, 20f32.to_radians().cos()));
            assert!((axis - Vec3::NEG_Z).length() < 1e-4, "pitch {pitch}, roll {roll}: the camera looks along {axis} on the table");
        }
    }

    #[test]
    fn a_cut_jumps_at_once_and_dims_the_diorama_briefly() {
        let table = Table::default();
        let mut placement = Placement::default();
        let target = [0.0, 200.0, 1200.0];
        placement.update(&camera(0.0, 30.0, 0.0, target, CameraMode::Field), 256.0, "field:1", true, 0.0, &table).expect("a placement");
        // Turning 20 degrees in a game frame: eased, no dimming.
        let eased = placement.update(&camera(20.0, 30.0, 0.0, target, CameraMode::Field), 256.0, "field:1", true, 0.1, &table).expect("a placement");
        let turned = wrap(eased.heading).to_degrees();
        assert!(turned > 1.0 && turned < 19.0, "eased part of the way: {turned}");
        assert_eq!(placement.dim(0.1), 0.0);
        // Turning 60 more: a cut, the heading jumps there, dimmed by the full depth.
        let cut = placement.update(&camera(80.0, 30.0, 0.0, target, CameraMode::Field), 256.0, "field:1", true, 0.2, &table).expect("a placement");
        assert!(wrap(cut.heading - 80f32.to_radians()).abs() < 1e-4, "jumped to the camera's heading: {}", cut.heading.to_degrees());
        assert_eq!(placement.dim(0.2), 1.0);
        assert!(placement.dim(0.3) > 0.0 && placement.dim(0.3) < 1.0);
        assert_eq!(placement.dim(0.2 + DIM + 0.01), 0.0, "the dimming is gone after {DIM} s");
        // The target jumping beyond a third of the table: a cut, followed at once.
        let far = [3000.0, 200.0, 1200.0];
        let jumped = placement.update(&camera(80.0, 30.0, 0.0, far, CameraMode::Field), 256.0, "field:1", true, 1.0, &table).expect("a placement");
        let centre = jumped.table_from_world.transform_point3(Vec3::from(far));
        assert!(centre.length() < 1e-4, "the table's centre on the new target: {centre}");
        assert_eq!(placement.dim(1.0), 1.0);
    }

    #[test]
    fn snap_turns_the_table_in_steps_and_fixed_keeps_its_first_heading() {
        let target = [0.0, 200.0, 1200.0];
        let heading = |turns: Turns, yaws: &[f32]| {
            let table = Table { turns, ..Table::default() };
            let mut placement = Placement::default();
            let mut headings = Vec::new();
            for (n, &yaw) in yaws.iter().enumerate() {
                let placed = placement.update(&camera(yaw, 30.0, 0.0, target, CameraMode::Field), 256.0, "field:1", true, n as f64 * 0.5, &table).expect("a placement");
                headings.push(wrap(placed.heading).to_degrees());
            }
            (headings, placement)
        };
        // The camera turning 10 to 15 degrees a game frame (no cut).
        let yaws = [0.0, 10.0, 20.0, 35.0, 45.0];
        let (snapped, placement) = heading(Turns::Snap, &yaws);
        let close = |a: f32, b: f32| (a - b).abs() < 1e-3;
        assert!(close(snapped[0], 0.0) && close(snapped[1], 0.0) && close(snapped[2], 0.0), "within 30 degrees the table stays: {snapped:?}");
        assert!(close(snapped[3], 35.0) && close(snapped[4], 35.0), "past 30 it jumps to the camera's heading, and stays: {snapped:?}");
        assert_eq!(placement.dim(1.5), 1.0, "a snap dims the diorama");
        let (fixed, placement) = heading(Turns::Fixed, &yaws);
        assert!(fixed.iter().all(|&h| close(h, 0.0)), "the scene's first heading: {fixed:?}");
        assert_eq!(placement.dim(2.0), 0.0);
        let (smooth, _) = heading(Turns::Smooth, &yaws);
        assert!(smooth.windows(2).all(|w| w[1] > w[0]) && smooth[4] < 45.0, "eased behind the camera: {smooth:?}");
    }

    /// A battle's stage, its turns running: combatants at `combatants` ((x,
    /// y, z), height; y down) and the start view looking from `eye` at
    /// `look_at`.
    fn stage(combatants: &[([f32; 3], f32)], look_at: [f32; 3], eye: [f32; 3]) -> Stage {
        let mut stage = Stage { look_at, eye, turns: true, ..Stage::default() };
        for (slot, &(position, height)) in combatants.iter().enumerate() {
            stage.combatants[slot] = Some(Combatant { position, height });
        }
        stage
    }

    /// A battle camera turned `yaw` degrees and pitched 20 down, looking at
    /// `target`, shaken by `quake` (its translation's offset), with `stage`.
    fn battle_camera(yaw: f32, target: [f32; 3], quake: [f32; 3], stage: Stage) -> Camera {
        let mut camera = camera(yaw, 20.0, 0.0, target, CameraMode::Battle);
        camera.translation = quake;
        camera.stage = Some(stage);
        camera
    }

    #[test]
    fn a_battle_stands_where_its_combatants_stand_facing_its_start_view_whatever_its_camera_does() {
        let table = Table::default();
        // The party on the right, two enemies on the left (one 300 tall),
        // on ground 100 and 120 up (y down); the start view looks along the
        // world's +z, 30 degrees to its right.
        let (s, c) = 30f32.to_radians().sin_cos();
        let look_at = [900.0, 0.0, 900.0];
        let eye = [900.0 - 2000.0 * s, -1700.0, 900.0 - 2000.0 * c];
        let party = [([1000.0, -100.0, 1000.0], 96.0), ([1100.0, -100.0, 1300.0], 96.0)];
        let foes = [([600.0, -120.0, 1100.0], 300.0), ([500.0, -100.0, 1400.0], 96.0)];
        let formed = stage(&[party[0], party[1], foes[0], foes[1]], look_at, eye);
        let mut placement = Placement::default();
        let first = placement.update(&battle_camera(10.0, [0.0, 0.0, 1200.0], [0.0; 3], formed), 256.0, "battle", true, 0.0, &table).expect("a placement");
        // Its heading is the start view's: the way it looks lies away from
        // the seat on the table.
        assert!(wrap(first.heading - first_heading(look_at, eye)).abs() < 1e-5);
        let away = on_table(&first, Vec3::new(s, 0.0, c));
        assert!((away - Vec3::NEG_Z).length() < 1e-4, "the start view looks along {away} on the table");
        // Its centre: the middle of their extent across the table, on the
        // ground the lowest of them stands on (100 up).
        let across: Vec<Vec3> = [party[0], party[1], foes[0], foes[1]].iter().map(|(p, _)| first.table_from_world.transform_point3(Vec3::from(*p))).collect();
        let (lo, hi) = across.iter().fold((Vec3::MAX, Vec3::MIN), |(lo, hi), p| (lo.min(*p), hi.max(*p)));
        assert!(((lo.x + hi.x) / 2.0).abs() < 1e-5 && ((lo.z + hi.z) / 2.0).abs() < 1e-5, "centred across the table: {lo} {hi}");
        assert!(lo.y.abs() < 1e-5 && (hi.y - 20.0 * first.scale).abs() < 1e-5, "the lowest ground on the table's top, the other 20 above: {lo} {hi}");
        // Its scale: their extent across it with the tallest one's height to
        // spare spans the table.
        let extent = ((hi.x - lo.x).max(hi.z - lo.z)) / first.scale;
        assert!(((VOLUME_MAX.x - VOLUME_MIN.x) / first.scale - (extent + 300.0)).abs() < 0.5, "{extent} + 300 across the table");
        // Its camera orbits, cuts and shakes (the quake offsets its
        // translation): the table stays, never dims, and a recentring
        // leaves it.
        for (n, (yaw, target, quake)) in [(40.0, [100.0, 0.0, 1100.0], [0.0; 3]), (120.0, [-500.0, -300.0, 900.0], [12.0, -8.0, 0.0]), (250.0, [0.0, 0.0, 1300.0], [-6.0, 4.0, 3.0])].into_iter().enumerate() {
            let now = 1.0 + n as f64;
            let placed = placement.update(&battle_camera(yaw, target, quake, formed), 256.0, "battle", true, now, &table).expect("a placement");
            assert_eq!(placed.table_from_world, first.table_from_world, "the battle's table stands where its stage put it");
            assert_eq!(placement.dim(now), 0.0, "and never dims");
            (placement.target, placement.heading, placement.last) = (None, None, None);
        }
        // A quake moves the camera, not the world: a point the game draws
        // through the shaken camera is the same world point.
        let (still, shaken) = (battle_camera(40.0, look_at, [0.0; 3], formed), battle_camera(40.0, look_at, [12.0, -8.0, 5.0], formed));
        let point = [700.0, -80.0, 1250.0];
        let back = |camera: &Camera| WorldFrame::new(camera).expect("a frame").point(camera.view_point(point));
        assert!((back(&still) - back(&shaken)).length() < 1e-2 && (back(&still) - Vec3::from(point)).length() < 1e-2);
        // During the battle's opening (its turns not yet running: its
        // combatants entering), and without any combatant, the table stands
        // on the start view's look-at, spanning the screen's width at its
        // depth; once the turns run, its combatants place it again, a jump.
        let mut placement = Placement::default();
        let depth = (Vec3::from(look_at) - Vec3::from(eye)).length();
        for (n, opening) in [stage(&[], look_at, eye), Stage { turns: false, ..formed }].into_iter().enumerate() {
            let waiting = placement.update(&battle_camera(0.0, look_at, [0.0; 3], opening), 256.0, "battle", true, n as f64 * 0.1, &table).expect("a placement");
            assert!(waiting.table_from_world.transform_point3(Vec3::from(look_at)).length() < 1e-4, "the start view's look-at at the centre");
            assert!(((VOLUME_MAX.x - VOLUME_MIN.x) / waiting.scale - 320.0 * depth / 256.0).abs() < 0.5);
        }
        assert_eq!(placement.dim(0.2), 0.0, "no jump during the opening");
        let joined = placement.update(&battle_camera(0.0, look_at, [0.0; 3], formed), 256.0, "battle", true, 0.5, &table).expect("a placement");
        assert_eq!(joined.table_from_world, first.table_from_world, "placed by the combatants once they stand");
        assert_eq!(placement.dim(0.5), 1.0, "a jump");
        let stays = placement.update(&battle_camera(0.0, look_at, [0.0; 3], Stage { turns: false, ..formed }), 256.0, "battle", true, 0.6, &table).expect("a placement");
        assert_eq!(stays.table_from_world, first.table_from_world, "and stays should the turns pause");
        // Another battle (another start view) without leaving the mode: its
        // own table, a jump; a new scene begins its own too.
        let next = stage(&[party[0], foes[1]], [0.0, 0.0, 0.0], [0.0, -1000.0, -2000.0]);
        let other = placement.update(&battle_camera(0.0, look_at, [0.0; 3], next), 256.0, "battle", true, 2.0, &table).expect("a placement");
        assert_ne!(other.table_from_world, first.table_from_world);
        assert_eq!(placement.dim(2.0), 1.0);
        let mut far = camera(0.0, 0.0, 0.0, [0.0, 0.0, 2400.0], CameraMode::Field);
        far.focus = 2400.0;
        let field = placement.update(&far, 256.0, "field:2", true, 10.0, &table).expect("a placement");
        assert!(field.table_from_world.transform_point3(Vec3::new(0.0, 0.0, 2400.0)).length() < 1e-4, "a field's own target at the table's centre");
        // The screen's width at the target's depth spans a field's table:
        // half the scale at twice the depth.
        let near = placement.update(&camera(0.0, 0.0, 0.0, [0.0, 0.0, 1200.0], CameraMode::Field), 256.0, "field:3", true, 11.0, &table).expect("a placement");
        assert!((field.scale * 2.0 - near.scale).abs() < 1e-9, "{} against {}", field.scale, near.scale);
    }

    /// The table's heading for a start view looking from `eye` at `look_at`.
    fn first_heading(look_at: [f32; 3], eye: [f32; 3]) -> f32 {
        let forward = Vec3::new(1.0, -1.0, -1.0) * (Vec3::from(look_at) - Vec3::from(eye));
        std::f32::consts::PI - forward.x.atan2(forward.z)
    }

    #[test]
    fn a_battles_table_shows_a_tall_enemy_within_its_volume_and_a_small_formation_at_least_two_projections_wide() {
        let table = Table::default();
        let place = |combatants: &[([f32; 3], f32)]| {
            let stage = stage(combatants, [0.0, 0.0, 0.0], [0.0, -1000.0, -3000.0]);
            Placement::default().update(&battle_camera(0.0, [0.0; 3], [0.0; 3], stage), 256.0, "battle", true, 0.0, &table).expect("a placement")
        };
        // The debug battle's boss (1523 tall) beside three people: it stands
        // within nine tenths of the volume's height above the table.
        let boss = place(&[([3820.0, 0.0, 4040.0], 1523.0), ([4360.0, 0.0, 4160.0], 92.0), ([4420.0, 0.0, 3840.0], 96.0), ([4460.0, 0.0, 4000.0], 96.0)]);
        assert!((1523.0 * boss.scale - BATTLE_HEADROOM * VOLUME_MAX.y).abs() < 1e-4, "{} m tall", 1523.0 * boss.scale);
        // Two against two a step apart (the Mountain Path's: 241 across):
        // the table spans two projection distances' screen width.
        let small = place(&[([2288.0, -562.0, 1095.0], 60.0), ([2333.0, -514.0, 932.0], 96.0), ([2131.0, -560.0, 1095.0], 62.0), ([2092.0, -519.0, 963.0], 62.0)]);
        assert!(((VOLUME_MAX.x - VOLUME_MIN.x) / small.scale - BATTLE_SPAN).abs() < 0.5);
    }

    #[test]
    fn the_world_maps_table_settles_on_the_ground_under_a_target_in_flight() {
        // The world map's frame: its target at (0, its height, 0), y down,
        // and the ground under it as the port records it.
        let world = |height: f32, ground: Option<f32>| Camera { ground, ..camera(0.0, 30.0, 0.0, [0.0, height, 0.0], CameraMode::Worldmap) };
        // How high above the table's top a world point (0, y, 0) stands (m).
        let above = |placed: &Placed, y: f32| placed.table_from_world.transform_point3(Vec3::new(0.0, y, 0.0)).y;
        let (depth, headroom) = (GROUND_DEPTH * -VOLUME_MIN.y, FLIGHT_HEADROOM * VOLUME_MAX.y);
        for scale in [0.25, 1.0, 4.0] {
            let table = Table { scale, ..Table::default() };
            let place = |height: f32, ground: Option<f32>| Placement::default().update(&world(height, ground), 256.0, "worldmap", true, 0.0, &table).expect("a placement");
            // The target on the top on its ground, without a ground (none
            // recorded: no block loaded there) or with one above it (the
            // target within the terrain).
            for (height, ground) in [(-256.0, Some(-256.0)), (-256.0, Some(-300.0)), (-472.0, None)] {
                let at = place(height, ground);
                assert!(above(&at, height).abs() < 1e-5, "scale {scale}: the target {} m above the top for the ground {ground:?}", above(&at, height));
            }
            // Beginning in flight, 136 units up (the debug world map's
            // vehicle) and 600: the ground on the top and the target above
            // it, or, where that would put the target beyond the headroom,
            // the target at the headroom.
            for clearance in [136.0, 600.0] {
                let at = place(-256.0 - clearance, Some(-256.0));
                let (target, ground) = (above(&at, -256.0 - clearance), above(&at, -256.0));
                assert!((target - ground - clearance * at.scale).abs() < 1e-5, "scale {scale}: {target} and {ground} m");
                if clearance * at.scale <= headroom {
                    assert!(ground.abs() < 1e-5, "scale {scale}, {clearance} up: the ground {ground} m above the top");
                } else {
                    assert!((target - headroom).abs() < 1e-5, "scale {scale}, {clearance} up: the target {target} m above the top");
                }
            }
        }
        // At 100%, the vehicle on its ground climbing 100 units a second
        // for 5 s (the debug world map's climbs about 35): the table comes
        // down with it as far as keeps the ground at the depth (and the
        // followed point's ease behind), then, the vehicle holding its
        // height, settles on the ground; a hill 100 units high coming
        // under it in a tenth of a second barely moves it.
        let table = Table::default();
        let mut placement = Placement::default();
        let mut t = 0.0;
        let mut placed = placement.update(&world(-256.0, Some(-256.0)), 256.0, "worldmap", true, t, &table).expect("a placement");
        let mut deepest = 0.0f32;
        while t < 5.0 {
            t += 0.05;
            placed = placement.update(&world(-256.0 - 100.0 * t as f32, Some(-256.0)), 256.0, "worldmap", true, t, &table).expect("a placement");
            deepest = deepest.min(above(&placed, -256.0));
        }
        assert!(deepest > -depth - 0.03, "climbing: the ground {deepest} m above the top at its deepest");
        while t < 30.0 {
            t += 0.05;
            placed = placement.update(&world(-756.0, Some(-256.0)), 256.0, "worldmap", true, t, &table).expect("a placement");
        }
        assert!(above(&placed, -256.0).abs() < 1e-3, "settled: the ground {} m above the top", above(&placed, -256.0));
        let hill = placement.update(&world(-756.0, Some(-356.0)), 256.0, "worldmap", true, t + 0.1, &table).expect("a placement");
        let rise = above(&hill, -256.0) - above(&placed, -256.0);
        assert!(rise.abs() < 0.05 * 100.0 * hill.scale, "the hill moved the table {rise} m");
    }

    #[test]
    fn the_pane_stands_upright_facing_the_seat_above_the_far_edge_at_every_tilt_and_scale() {
        let seat = Mat4::from_rotation_translation(glam::Quat::from_rotation_y(0.6), Vec3::new(0.2, 0.0, -0.1));
        let mut panes = Vec::new();
        for tilt in [0.0f32, 20.0, 45.0] {
            for scale in [0.25, 1.0, 4.0] {
                let table = Table { scale, tilt: tilt.to_radians(), ..Table::default() };
                let space = DioramaSpace::first(&LEVEL, 256.0, Vec2::new(320.0, 224.0), &table, seat).expect("a space");
                let (_, facing, centre) = space.pane.to_scale_rotation_translation();
                assert!(facing.angle_between(glam::Quat::from_rotation_y(0.6)) < 1e-5, "tilt {tilt}: upright, facing the seat");
                // Its lower edge is above the volume's top and beyond its far side.
                let bottom = space.table_from_room().transform_point3(centre - facing * Vec3::Y * space.pane_size.y / 2.0);
                assert!(bottom.y > VOLUME_MAX.y && bottom.z <= VOLUME_MIN.z + 1e-5, "tilt {tilt}: {bottom}");
                assert_eq!((space.pane_size, space.display), (Vec2::new(PANE_WIDTH, PANE_WIDTH * 0.7), Vec2::new(320.0, 224.0)));
                panes.push((tilt, space.pane));
            }
        }
        for (tilt, pane) in &panes {
            assert!(panes.iter().filter(|(t, _)| t == tilt).all(|(_, p)| p == pane), "tilt {tilt}: the player's scale leaves the pane where it is");
        }
    }

    #[test]
    fn the_listener_faces_the_seats_way_through_the_table() {
        let seat = Mat4::from_rotation_translation(glam::Quat::from_rotation_y(0.6), Vec3::new(0.2, 0.0, -0.1));
        let close = |a: [f32; 3], b: [f32; 3]| (Vec3::from(a) - Vec3::from(b)).length() < 1e-4;
        // A table turned to the camera's heading: the seat's axes are the
        // camera's own, whatever its yaw and the seat's.
        for yaw in [0.0, 70.0, -130.0] {
            let space = DioramaSpace::first(&camera(yaw, 25.0, 0.0, [0.0, 0.0, 1200.0], CameraMode::Field), 256.0, Vec2::new(320.0, 224.0), &Table::default(), seat).unwrap();
            let ([right, down, forward], rows) = space.listener_axes().unwrap();
            assert_eq!(Some(rows), camera_rows(&space.camera.axes), "in the view space of the camera the frame was drawn with");
            // The camera pitched 25 degrees; the table is level: its forward
            // is the camera's level heading, 25 degrees up in its view.
            let (s, c) = 25f32.to_radians().sin_cos();
            assert!(close(right, [1.0, 0.0, 0.0]) && close(forward, [0.0, -s, c]) && close(down, [0.0, c, s]), "yaw {yaw}: {:?}", [right, down, forward]);
        }
        // Tilted, the table turns its world towards the seat: the listener
        // (the seat's level forward) looks that much down into it.
        let tilted = Table { tilt: 20f32.to_radians(), ..Table::default() };
        let space = DioramaSpace::first(&LEVEL, 256.0, Vec2::new(320.0, 224.0), &tilted, Mat4::IDENTITY).unwrap();
        let (s, c) = 20f32.to_radians().sin_cos();
        let ([_, _, forward], _) = space.listener_axes().unwrap();
        assert!(close(forward, [0.0, s, c]), "{forward:?}");
    }

    #[test]
    fn the_listener_is_the_same_at_every_scale_to_the_last_bit() {
        // The player's scale changes how large the table shows the world,
        // not where the seat faces in it: from 25% to 400%, at scales whose
        // rounding differs, the listener's axes are identical, and the room
        // from the world is the table's place and turn, scaled.
        let seat = Mat4::from_rotation_translation(glam::Quat::from_rotation_y(0.6), Vec3::new(0.2, 0.0, -0.1));
        let camera = camera(70.0, 25.0, 3.0, [100.0, 200.0, 1200.0], CameraMode::Field);
        for tilt in [0.0f32, 30.0] {
            let at = |scale: f32| {
                let table = Table { scale, tilt: tilt.to_radians(), ..Table::default() };
                DioramaSpace::first(&camera, 256.0, Vec2::new(320.0, 224.0), &table, seat).expect("a space")
            };
            let one = at(1.0).listener_axes().expect("axes");
            for scale in [0.25, 0.377, 1.37, 2.52, 4.0] {
                let space = at(scale);
                assert_eq!(space.listener_axes(), Some(one), "tilt {tilt}, scale {scale}");
                let rotation = Mat3::from_mat4(space.room_from_world) / space.scale;
                let placed = Mat3::from_mat4(space.room_from_table) * space.turn;
                let apart = rotation.to_cols_array().iter().zip(placed.to_cols_array()).map(|(a, b)| (a - b).abs()).fold(0.0, f32::max);
                assert!(apart < 1e-5, "tilt {tilt}, scale {scale}: the room from the world is the table's place and turn, scaled ({apart})");
            }
        }
    }

    #[test]
    fn the_listener_is_the_same_from_every_seat_to_the_last_bit() {
        // A recentring stands the seat at the head, turned its way about the
        // vertical, and the table with it: the seat faces the world on the
        // table as it did, so the listener's axes are identical from every
        // seat (the yaws Monado's turning head was recentred at among them),
        // the table tilted or not.
        let camera = camera(70.0, 25.0, 3.0, [100.0, 200.0, 1200.0], CameraMode::Field);
        for tilt in [0.0f32, 30.0] {
            let table = Table { tilt: tilt.to_radians(), ..Table::default() };
            let from = |seat: Mat4| DioramaSpace::first(&camera, 256.0, Vec2::new(320.0, 224.0), &table, seat).expect("a space").listener_axes().expect("axes");
            let origin = from(Mat4::IDENTITY);
            for yaw in [0.0814f32, 0.0570, 0.0341, 0.0185, -0.0868, 0.6, -2.3, std::f32::consts::PI] {
                let seat = Mat4::from_rotation_translation(glam::Quat::from_rotation_y(yaw), Vec3::new(0.2, -0.03, -0.1));
                assert_eq!(from(seat), origin, "tilt {tilt}, a seat turned {yaw} radians");
            }
        }
    }

    #[test]
    fn the_settings_choose_the_screen_or_the_diorama_as_placed() {
        use xem_settings::{DioramaTurns, Graphics, HeadsetView, Renderer, XrScreen};
        let with = |xr: XrScreen| super::super::xr_view(&Graphics { xr, ..Graphics::default() });
        assert_eq!(with(XrScreen::default()), XrView::Screen);
        let xr = XrScreen { view: HeadsetView::Diorama, diorama_scale: 250, diorama_height_cm: -30, diorama_distance_cm: 80, diorama_tilt_deg: 15, diorama_turns: DioramaTurns::Fixed, ..XrScreen::default() };
        let XrView::Diorama(table) = with(xr) else { panic!("a diorama") };
        assert_eq!((table.scale, table.height, table.distance, table.turns), (2.5, -0.3, 0.8, Turns::Fixed));
        assert!((table.tilt - 15f32.to_radians()).abs() < 1e-6);
        let XrView::Diorama(default) = with(XrScreen { view: HeadsetView::Diorama, ..XrScreen::default() }) else { panic!("a diorama") };
        assert_eq!(default, Table::default(), "the settings' defaults are the renderer's");
        // The Original renderer draws no scene of its own: the screen.
        let original = Graphics { renderer: Renderer::Original, xr, ..Graphics::default() };
        assert_eq!(super::super::xr_view(&original), XrView::Screen, "no diorama under the Original renderer");
    }
}
