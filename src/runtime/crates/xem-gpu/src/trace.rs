//! The draw trace: what the GPU drew, for renderers that draw it again at
//! another resolution and with other rules (docs/runtime.md, Presentation).
//!
//! The trace observes the device. It is off until a host asks for it, it is
//! not part of [`Gpu::save`] and nothing in it feeds back into VRAM, GPUSTAT
//! or the game. While it is on, every drawing command is recorded with the
//! environment it was drawn with, and every change of VRAM a renderer cannot
//! derive from the drawing commands themselves (transfers, fills, copies, and
//! drawn pixels a later command samples as a texture) is recorded as the
//! software VRAM's resulting pixels, so a renderer's copy of VRAM for texture
//! sampling stays equal to the device's at every command.
//!
//! Vertices carry the GTE projection that produced their screen point when
//! there is one (`xem/gte.h` in the port keeps a ring of every RTPS/RTPT
//! vertex): its depth, sub-pixel position and what drew it (a model, a
//! sprite). A packet whose points did not come from the GTE (text, windows,
//! rectangles) has no depth. The port also records where the game stored
//! each screen point and which projection made it, so a packet's point is
//! matched by the address the DMA read it from, and only so: matching by
//! the point's value would tie interface points (window corners, glyphs)
//! to unrelated 3D points at the same pixel in busy scenes.
//!
//! The port records the faces its renderers culled at the edges of the
//! original screen as well; they are traced as [`Primitive::revealed`]
//! where the game would have linked them, for wider views, and never drawn.

use crate::{Gpu, VRAM_HEIGHT, VRAM_WIDTH};
use std::collections::HashMap;
use std::hash::{BuildHasherDefault, Hasher};

/// The trace's tables are keyed by 32-bit words (screen points, addresses,
/// owners) and touched tens of thousands of times a frame: a multiply and a
/// fold spread such keys well enough, for far less than the default hasher.
#[derive(Clone, Copy, Debug, Default)]
struct WordHasher(u64);

impl Hasher for WordHasher {
    fn finish(&self) -> u64 {
        self.0 ^ self.0 >> 29
    }

    fn write(&mut self, bytes: &[u8]) {
        for &byte in bytes {
            self.write_u32(u32::from(byte));
        }
    }

    fn write_u32(&mut self, word: u32) {
        self.0 = (self.0 ^ u64::from(word)).wrapping_mul(0x9E37_79B9_7F4A_7C15);
    }
}

type WordMap<V> = HashMap<u32, V, BuildHasherDefault<WordHasher>>;

/// A GTE projection as the port's ring holds it.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Projection {
    /// SXY2 as the GTE left it (the word a packet stores).
    pub sxy: u32,
    /// SZ3: the screen depth (0-65535).
    pub sz: u16,
    /// What drew it (the port's `XEM_PROJECTION_*`).
    pub class: u8,
    /// The screen point in 16.16 fixed point, before saturation.
    pub x: i32,
    pub y: i32,
    /// The GTE's screen offset (OFX, OFY) and projection distance (H).
    pub offset: (i16, i16),
    pub h: u16,
    /// The sprite or model whose drawing made it (its address), or 0.
    pub owner: u32,
    /// The point before the GTE's division: MAC1, MAC2 and the depth SZ3
    /// saturates from (also behind the camera).
    pub view: [i32; 3],
}

/// What a view wider or farther out than the original's asks of the port's
/// culled-face records ([`Gpu::set_reveals`]).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Reveals {
    /// How far beyond the screen (pixels, x and y) a culled face may lie:
    /// on the pulled-back camera's screen when there is one.
    pub reach: [u16; 2],
    /// How far behind the game's camera the view looks from (GTE depth
    /// units; 0 for the game's own camera).
    pub pull_back: u32,
    /// Whether the view looks from within or around the scene, as the
    /// port's `around` bits ([`Reveals::LOOK`], [`Reveals::ANYWHERE`]; 0
    /// from the game's camera): every face counts wherever it lies, the
    /// cameras the port records say so ([`Camera::around`]), and in a field
    /// it records where the controlled character stands ([`TraceOp::Eye`]).
    pub around: u8,
}

impl Reveals {
    /// [`Reveals::around`]: the view looks around from the field's
    /// controlled character (first-person inspection); the port also draws
    /// the field's sprites again facing its eyes.
    pub const LOOK: u8 = 1;
    /// [`Reveals::around`]: the view looks from anywhere around a field, a
    /// battle or the world map (a diorama); the port also records the
    /// battle's stage and the world map's ground before their cameras.
    pub const ANYWHERE: u8 = 2;
}

/// Where the field's controlled character stands, as the port records it
/// while the view looks from within or around the scene
/// ([`Reveals::around`]): its position and the world's x, y and z axes
/// (4.12 times the field's scale) through the field camera, in the camera's
/// view space (the space of [`TraceVertex::view`]), and its sprite (the
/// owner of its parts).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Eye {
    pub position: [f32; 3],
    pub axes: [[f32; 3]; 3],
    pub sprite: u32,
}

/// The camera the game draws a frame of a field, a battle or the world map
/// with, as the port records it: the world's x, y and z axes through its
/// rotation (its matrix's columns in 4.12, times the field's scale in a
/// field) and its translation, so that a world point's view-space point (the
/// space of [`TraceVertex::view`]) is `axes · point / 4096 + translation`;
/// the point it looks at, in the same world units (`target`), and that
/// point's depth through it (`focus`). A battle's carries its stage,
/// recorded just before it for a view from anywhere (`stage`); the world
/// map's the ground's height under its target (`ground`, y down: below a
/// target in flight), likewise.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Camera {
    pub axes: [[f32; 3]; 3],
    pub translation: [f32; 3],
    pub target: [f32; 3],
    pub focus: f32,
    pub mode: CameraMode,
    pub stage: Option<Stage>,
    pub ground: Option<f32>,
    /// Where its world lies in the game's: the world map places points
    /// relative to its camera's target (the target's x, 0 and z negated, in
    /// whole units), so a point of the world lies at its world point plus
    /// this in every frame; 0 elsewhere.
    pub origin: [f32; 3],
    /// [`Reveals::around`] as the port recorded it: a frame recorded looking
    /// from within or around the scene holds culled faces from anywhere.
    pub around: u8,
}

/// The transform the game draws a model or a sprite's parts with, as the
/// port records it before they project anything ([`TraceOp::Placement`]):
/// its local x, y and z axes through the rotation (the matrix's columns,
/// 4.12) and its translation, so that a local point's view-space point is
/// `axes · point / 4096 + translation`.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Placement {
    /// Its number in the trace (from 1), which its primitives hold
    /// ([`Primitive::placement`]).
    pub seq: u32,
    /// The model or sprite (game address): its primitives' owner.
    pub owner: u32,
    /// For a model, the packet buffer it is drawn into and the size of
    /// one: an instance's two buffers are one block
    /// (model_alloc_packet_buffers), so its draws alternate between buffers
    /// `size` apart. 0 for a sprite, the owner of its own parts.
    pub packets: u32,
    pub size: u32,
    pub axes: [[f32; 3]; 3],
    pub translation: [f32; 3],
}

/// A battle's stage, as the port records it before the battle's camera
/// (xem/gte.h `XEM_PROJECTION_STAGE`), in the battle's world (y down): the
/// combatants the battle shows (by slot: the party's 0-2, the enemies'
/// 3-10), as its camera frames them, the scene's start view, the camera
/// the battle begins with (its look-at and its eye), and whether its turns
/// run (`turns`: its opening is over, its combatants have entered and stand).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Stage {
    pub combatants: [Option<Combatant>; 11],
    pub look_at: [f32; 3],
    pub eye: [f32; 3],
    pub turns: bool,
}

/// A battle's combatant: where its sprite stands and how tall it is (its
/// frame's extent at its scale: a person's about 96 units, a Gear's about
/// 500, a big enemy's more).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Combatant {
    pub position: [f32; 3],
    pub height: f32,
}

/// Which of the game's 3D modes a [`Camera`] drew a frame of, and so the
/// world its points lie in (each with y down).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CameraMode {
    /// A field: its world (`field_view.scaled_world`, looking at
    /// `field_view.target`).
    Field,
    /// A battle: its stage's world (`battle_camera.matrix`, looking at
    /// `battle_camera_view_target`).
    Battle,
    /// The world map: points relative to the camera's target, z negated, as
    /// the world map places them (`worldmap_camera_matrix`, looking at the
    /// target: (0, its height, 0); [`Camera::origin`]).
    Worldmap,
}

impl CameraMode {
    /// The mode a camera record's owner names (xem/gte.h `XEM_CAMERA_*`,
    /// below the host's `around` bits).
    fn from_owner(owner: u32) -> Option<CameraMode> {
        match owner & 0xFF {
            1 => Some(CameraMode::Field),
            2 => Some(CameraMode::Battle),
            3 => Some(CameraMode::Worldmap),
            _ => None,
        }
    }
}

impl Camera {
    /// The world point whose view-space point is `view` (the camera's
    /// transform inverted), or `None` when its axes span no space.
    pub fn world_point(&self, view: [f32; 3]) -> Option<[f32; 3]> {
        let column = |k: usize| self.axes[k].map(f64::from);
        let (a, b, c) = (column(0), column(1), column(2));
        let d: [f64; 3] = std::array::from_fn(|i| (f64::from(view[i]) - f64::from(self.translation[i])) * 4096.0);
        let cross = |u: [f64; 3], v: [f64; 3]| [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]];
        let dot = |u: [f64; 3], v: [f64; 3]| u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
        // Cramer's rule for the point p with a·p.x + b·p.y + c·p.z = d.
        let determinant = dot(a, cross(b, c));
        if determinant.abs() < 1e-6 {
            return None;
        }
        Some([dot(d, cross(b, c)), dot(a, cross(d, c)), dot(a, cross(b, d))].map(|x| (x / determinant) as f32))
    }

    /// The view-space point of the world point `point` through the camera.
    pub fn view_point(&self, point: [f32; 3]) -> [f32; 3] {
        std::array::from_fn(|i| (0..3).map(|k| self.axes[k][i] * point[k]).sum::<f32>() / 4096.0 + self.translation[i])
    }
}

/// A screen point the game stored (the port's store ring).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SxyStore {
    /// Where the word went (24-bit physical).
    pub address: u32,
    /// The SXY word.
    pub value: u32,
    /// The projection that made it, when the port knows it.
    pub source: Option<u32>,
}

/// A face a renderer of the game culled at the original screen's edges
/// (the port's reveal ring): the GP0 words it would have sent and the node
/// of the ordering table it would have been drawn just before (`entry`: the
/// packet it would have been linked in front of). Its points are the last
/// `vertices` projections made before `projections`, in order (none for a
/// flat one, such as a backdrop continued into the sides), or with `order`
/// point k the projection `order >> 8k & 0xFF` before that count (a sprite
/// part stores its corners out of order).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Reveal {
    pub entry: u32,
    pub vertices: u8,
    pub projections: u32,
    pub order: u32,
    pub words: FaceWords,
}

/// A face's GP0 words: at most 12 (a Gouraud-shaded textured quad), held
/// in place (thousands of culled faces a frame allocate nothing).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct FaceWords {
    words: [u32; 12],
    len: u8,
}

impl From<&[u32]> for FaceWords {
    /// The first 12 of `words`.
    fn from(words: &[u32]) -> Self {
        let mut face = FaceWords { words: [0; 12], len: words.len().min(12) as u8 };
        face.words[..usize::from(face.len)].copy_from_slice(&words[..usize::from(face.len)]);
        face
    }
}

impl std::ops::Deref for FaceWords {
    type Target = [u32];

    fn deref(&self) -> &[u32] {
        &self.words[..usize::from(self.len)]
    }
}

/// What a vertex belongs to.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub enum Class {
    /// Not projected by the GTE: 2D interface, text, screen effects.
    #[default]
    Flat,
    /// Projected outside a recorded seam (terrain, effects).
    World,
    /// A 3D model (`model_draw_sprite_model`).
    Model,
    /// A sprite's parts (`sprite_draw_parts`).
    Sprite,
}

/// Ring entries that are not drawn points (xem/gte.h): the floor under a
/// sprite, the depth of the field camera's target, the world's axes through
/// a camera and its translation (an axis owned by [`TRANSLATION`]), the
/// point that camera looks at, the controlled character's position through
/// the field camera, and a model's or sprite's axes and translation (its
/// `sz` the axis, [`TRANSLATION`] for the translation).
const GROUND: u8 = 3;
const FOCUS: u8 = 4;
const AXIS: u8 = 6;
const EYE: u8 = 7;
const CAMERA: u8 = 8;
const STAGE: u8 = 9;
const PLACEMENT: u8 = 11;
const TRANSLATION: u32 = 3;
/// A battle's stage entries past its slots: the start view's look-at and
/// eye; and the world map's ground under its camera's target and world
/// point there (xem/gte.h `XEM_STAGE_*`).
const STAGE_LOOK_AT: u32 = 16;
const STAGE_EYE: u32 = 17;
const STAGE_GROUND: u32 = 18;
const STAGE_ORIGIN: u32 = 19;
/// Points the interface projects (xem/gte.h: the world map's player marker),
/// traced as interface: flat, at the original's whole pixels.
const INTERFACE: u8 = 5;
/// Points of the backdrop a battle draws far behind its scene (xem/gte.h:
/// its sky), traced as the world ([`Primitive::backdrop`]).
const BACKDROP: u8 = 10;

/// What a traced point's projection tells of its primitive: what drew it,
/// the GTE's screen offset and projection distance, the owner, its depth,
/// whether the backdrop drew it, and the owner's placement then.
#[derive(Clone, Copy)]
struct Projected {
    class: Class,
    offset: (i16, i16),
    h: u16,
    owner: u32,
    sz: u16,
    backdrop: bool,
    placement: u32,
}

impl Class {
    fn from_projection(class: u8) -> Class {
        match class {
            1 => Class::Model,
            2 => Class::Sprite,
            _ => Class::World,
        }
    }

    /// Whether the primitive belongs to the 3D scene (not the interface).
    pub fn is_scene(self) -> bool {
        self != Class::Flat
    }
}

/// One vertex as the GPU drew it.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TraceVertex {
    /// VRAM position: the packet's signed 11-bit point plus the drawing offset.
    pub x: i32,
    pub y: i32,
    /// The same point at the GTE's sub-pixel precision (equal to `x`, `y`
    /// without a projection).
    pub fx: f32,
    pub fy: f32,
    /// The GTE's screen depth (SZ), 0 without a projection.
    pub z: f32,
    /// The projected point before the division (its projection's `view`),
    /// for a camera pulled back.
    pub view: Option<[f32; 3]>,
    /// Colour as the command gives it: 0xBBGGRR.
    pub color: u32,
    /// Texture coordinates. A rectangle's far corners hold the coordinate one
    /// texel past its last (it may pass 255, or go below 0 when flipped);
    /// texels wrap at 256.
    pub u: i16,
    pub v: i16,
}

/// The drawing environment of a primitive.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Environment {
    /// GP0(E1h) bits 0-13 as the primitive used them (a polygon's own page applied).
    pub draw_mode: u32,
    /// GP0(E2h).
    pub texture_window: u32,
    /// Drawing area, inclusive: left, top, right, bottom.
    pub area: [u16; 4],
    /// GP0(E6h) bits 0-1.
    pub mask: u32,
    /// GP0(E5h): the drawing offset added to every vertex.
    pub offset: (i16, i16),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Shape {
    Triangle,
    /// Vertices 0-1-2 and 1-2-3.
    Quad,
    /// A rectangle or sprite as a quad (vertex order as [`Shape::Quad`]):
    /// its texture coordinates step one texel per pixel and are never
    /// interpolated across a diagonal.
    Rectangle,
    Line,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Primitive {
    pub shape: Shape,
    pub vertices: [TraceVertex; 4],
    pub textured: bool,
    /// Texels unmodulated by the vertex colour.
    pub raw: bool,
    pub gouraud: bool,
    pub semi: bool,
    /// Dithered (GP0(E1h).9 for a Gouraud or modulated textured polygon).
    pub dither: bool,
    /// The CLUT word (bits 16-31 of the first texture coordinate word).
    pub clut: u16,
    pub environment: Environment,
    pub class: Class,
    /// The screen offset and projection distance of the GTE projection of
    /// its first projected vertex.
    pub projection: Option<((i16, i16), u16)>,
    /// The sprite or model (game address) that projected it, 0 if none.
    pub owner: u32,
    /// The transform the owner projected it with: the [`Placement::seq`]
    /// of the port's record of it (0 for none).
    pub placement: u32,
    /// Where the command came from: the DMA list node's address (bits 8-31)
    /// and the primitive's index in it (bits 0-7), or 0 for words the CPU
    /// wrote.
    pub source: u32,
    /// A face the game culled at its screen's edges ([`Reveal`]), traced
    /// where it would have been drawn. The GPU never drew it: a renderer
    /// draws it only beyond the original screen, for a wider view.
    pub revealed: bool,
    /// For a sprite part: the point on the floor under its sprite (VRAM x,
    /// y and GTE depth), where the port records one.
    pub ground: Option<[f32; 3]>,
    /// The original animation assembled from the first-person viewing angle.
    /// Presentation only; the GPU's commands and VRAM remain original.
    pub facing: Option<std::sync::Arc<SpriteFacing>>,
    /// Every point projected by the backdrop the game draws far behind its
    /// scene (a battle's sky; xem/gte.h `XEM_PROJECTION_BACKDROP`): part of
    /// the world in the game's view, the backdrop to a view from elsewhere.
    pub backdrop: bool,
}

#[derive(Clone, Debug, PartialEq)]
pub struct SpriteFacingPart {
    pub words: [u32; 9],
    pub points: [Projection; 4],
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct SpriteFacing {
    pub parts: Vec<SpriteFacingPart>,
    /// Alternate cells, in the original upload queue's execution order.
    pub textures: Vec<(VramRect, std::sync::Arc<[u16]>)>,
    /// Original pixels to put back after this sprite's alternative is drawn.
    pub restore: Vec<(VramRect, std::sync::Arc<[u16]>)>,
}

impl Primitive {
    pub fn vertex_count(&self) -> usize {
        match self.shape {
            Shape::Triangle => 3,
            Shape::Line => 2,
            Shape::Quad | Shape::Rectangle => 4,
        }
    }

    /// Mean screen depth of the projected vertices, 0 without any.
    pub fn depth(&self) -> f32 {
        let vertices = &self.vertices[..self.vertex_count()];
        let projected: Vec<f32> = vertices.iter().map(|v| v.z).filter(|z| *z > 0.0).collect();
        if projected.is_empty() {
            0.0
        } else {
            projected.iter().sum::<f32>() / projected.len() as f32
        }
    }
}

/// A rectangle of VRAM pixels.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct VramRect {
    pub x: u16,
    pub y: u16,
    pub width: u16,
    pub height: u16,
}

impl VramRect {
    fn intersect(&self, other: &VramRect) -> Option<VramRect> {
        let x0 = self.x.max(other.x);
        let y0 = self.y.max(other.y);
        let x1 = (self.x + self.width).min(other.x + other.width);
        let y1 = (self.y + self.height).min(other.y + other.height);
        (x0 < x1 && y0 < y1).then(|| VramRect { x: x0, y: y0, width: x1 - x0, height: y1 - y0 })
    }

    fn union(&self, other: &VramRect) -> VramRect {
        let x0 = self.x.min(other.x);
        let y0 = self.y.min(other.y);
        let x1 = (self.x + self.width).max(other.x + other.width);
        let y1 = (self.y + self.height).max(other.y + other.height);
        VramRect { x: x0, y: y0, width: x1 - x0, height: y1 - y0 }
    }

    /// The pieces of a rectangle that may run past the VRAM edges.
    fn wrapped(x: usize, y: usize, width: usize, height: usize) -> Vec<VramRect> {
        let mut out = Vec::new();
        let (x, y) = (x & (VRAM_WIDTH - 1), y & (VRAM_HEIGHT - 1));
        let xs = [(x, (VRAM_WIDTH - x).min(width)), (0, width.saturating_sub(VRAM_WIDTH - x))];
        let ys = [(y, (VRAM_HEIGHT - y).min(height)), (0, height.saturating_sub(VRAM_HEIGHT - y))];
        for &(ry, rh) in &ys {
            for &(rx, rw) in &xs {
                if rw > 0 && rh > 0 {
                    out.push(VramRect { x: rx as u16, y: ry as u16, width: rw as u16, height: rh as u16 });
                }
            }
        }
        out
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum TraceOp {
    Draw(Primitive),
    /// GP0(02h): `color` 0xBBGGRR over the rectangle (already wrapped).
    Fill { rect: VramRect, color: u32 },
    /// GP0(80h): a copy inside VRAM. The destination's resulting pixels
    /// follow as a [`TraceOp::Write`].
    Copy { source: (u16, u16), destination: (u16, u16), width: u16, height: u16 },
    /// VRAM pixels as the device now holds them. `upload` is true for a
    /// CPU-to-VRAM transfer (or the whole VRAM when a trace starts): pixels
    /// no drawing command produced. Otherwise the pixels result from the
    /// fill, copy or drawing commands before it, which a renderer drew
    /// itself: it updates only its copy for texture sampling.
    /// The pixels are shared: the trace, a renderer's logs and replays hold
    /// the same ones.
    Write { rect: VramRect, pixels: std::sync::Arc<[u16]>, upload: bool },
    /// A vertical blank: the game's frame ended.
    VBlank,
    /// The VRAM at (`x`, `y`) holds a movie macroblock the MDEC decoded, as
    /// it output it: 16 rows of 24 VRAM pixels at 24 bits (16 pixels), or of
    /// 16 at 15 bits. Follows the [`TraceOp::Write`] of the transfer that
    /// loaded it ([`Gpu::trace_macroblocks`]).
    Movie { x: u16, y: u16, block: std::sync::Arc<MovieBlock> },
    /// The frames a movie has after the one its player loaded last, for a
    /// movie that ends on the picture the game goes on from (the title
    /// movie, which the session notes after each of its game frames:
    /// [`Gpu::trace_movie_frames_left`]). Presentation only.
    MovieFramesLeft(u16),
    /// Where the field's controlled character stands in the frame the game
    /// draws next (while the view looks from within or around the scene:
    /// [`Reveals::around`]), in order with the commands: the port records
    /// it as it builds the frame.
    Eye(Eye),
    /// The camera the game draws its next frame of a field, a battle or the
    /// world map with, in order with the commands: the port records it as
    /// it builds the frame. Boxed: with a battle's stage it is larger than a
    /// primitive, and every op of a trace, of a renderer's logs and of its
    /// replays is as large as the largest kind of op.
    Camera(Box<Camera>),
    /// The transform a model or a sprite's parts of the frame the game
    /// builds next are drawn with, in order with the commands.
    Placement(Placement),
}

/// A movie macroblock as the MDEC decoded it, before its conversion to RGB:
/// for a renderer that converts and filters the movie itself.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MovieBlock {
    /// Luminance, 16x16 row by row, and the chrominance blocks, 8x8 each, as
    /// the IDCT left them (signed, before the output's clamp and offset).
    pub luma: [i16; 256],
    pub cb: [i16; 64],
    pub cr: [i16; 64],
    /// Each block's quantization scale: Cr, Cb, then the four luminance blocks.
    pub quant: [u8; 6],
    /// 24-bit output (else 15-bit).
    pub bits24: bool,
}

/// Movie macroblocks kept for the transfers that load them.
const MACROBLOCKS: usize = 4096;

/// FNV-1a, 64-bit (the key the MDEC gives a macroblock's output bytes).
fn fnv1a(bytes: impl IntoIterator<Item = u8>) -> u64 {
    bytes.into_iter().fold(0xCBF2_9CE4_8422_2325, |hash, byte| (hash ^ u64::from(byte)).wrapping_mul(0x100_0000_01B3))
}

/// The trace since the last [`Gpu::take_trace`].
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Trace {
    pub ops: Vec<TraceOp>,
    /// The GTE depth of the field camera's target, when the port recorded
    /// one since the last take (it does while culled faces are wanted).
    pub focus: Option<u16>,
}

#[derive(Clone, Debug, Default)]
pub(crate) struct Recorder {
    ops: Vec<TraceOp>,
    /// The newest [`HISTORY`] projections by their index in the port's
    /// ring (its count when made); `history_end` is one past the newest.
    history: Vec<Projection>,
    history_end: u32,
    history_len: u32,
    /// Screen points the game stored, by word of RAM: the value and the
    /// projection that made it plus 1 (0 none). The port records stores to
    /// RAM only.
    stored: Vec<(u32, u32)>,
    /// Culled faces by the ordering-table entry they would have been linked
    /// at, until the DMA walk reaches it, and emptied lists kept for new
    /// entries (thousands a frame when zoomed out).
    reveals: WordMap<Vec<Revealed>>,
    facings: Vec<(u32, u32, std::sync::Arc<SpriteFacing>)>,
    spare: Vec<Vec<Revealed>>,
    /// The address each word of the GPU's partly received command was read
    /// from (0 for words the CPU wrote), and the word being written's.
    fifo_addresses: Vec<u32>,
    word_address: u32,
    /// While a revealed face is traced: its vertices' projections.
    revealing: Option<[Option<u32>; 4]>,
    /// What the host wants of culled faces (none when `None`). The port
    /// reads it.
    wanted: Option<Reveals>,
    /// Points traced: matched to their projection, and not (2D points).
    matches: std::cell::Cell<[u64; 2]>,
    /// The newest floor point of each sprite (by owner), the camera focus
    /// depth since the last take, and the world's axes and the translation
    /// the port recorded last (before the camera's target and the
    /// controlled character's position), and the battle's stage and the
    /// world map's ground recorded before its camera (taken by it).
    grounds: WordMap<u32>,
    focus: Option<u16>,
    axes: [[f32; 3]; 3],
    translation: [f32; 3],
    stage: Option<Stage>,
    ground: Option<f32>,
    origin: Option<[f32; 3]>,
    /// Each history projection's placement ([`Primitive::placement`], 0
    /// none), the placements recorded so far, the axes of the one being
    /// recorded, the newest of each sprite or model (by owner), and the
    /// owner the last projection took one for, with it (runs of projections
    /// share their owner).
    placed: Vec<u32>,
    placements: u32,
    placement_axes: [[f32; 3]; 3],
    placed_by: WordMap<u32>,
    last_placed: (u32, u32),
    /// The age of culled faces: vertical blanks after which the port had
    /// recorded something new. A game that records nothing (paused) holds
    /// its last list and its faces with it.
    frames: u32,
    recorded: bool,
    /// Movie macroblocks the MDEC decoded, by their output's key, with the
    /// count of the decoding that last gave each; `macroblock_keys` holds
    /// keys and counts, oldest first (a key decoded again is as new as that).
    macroblocks: HashMap<u64, (std::sync::Arc<MovieBlock>, u64)>,
    macroblock_keys: std::collections::VecDeque<(u64, u64)>,
    decoded_macroblocks: u64,
    /// Drawn pixels not yet written to the trace.
    dirty: Option<VramRect>,
    /// The next take starts with all of VRAM (enabled, or a snapshot loaded).
    full_sync: bool,
    /// The DMA list node being sent (0 for words the CPU writes) and the
    /// primitives traced from it so far.
    source: u32,
    source_index: u32,
}

#[derive(Clone, Copy, Debug)]
struct Revealed {
    words: FaceWords,
    projections: [Option<u32>; 4],
    frame: u32,
}

/// Projections kept for matching: as many as the port's ring holds.
const HISTORY: usize = 65536;
/// DMA addresses: 24-bit, word aligned.
const ADDRESS_MASK: u32 = 0xFF_FFFC;
/// Words of RAM ([`Recorder::stored`]).
const RAM_WORDS: usize = 0x20_0000 / 4;
/// Recording frames ([`Recorder::frames`]) a culled face waits for its
/// ordering table's walk.
const REVEAL_FRAMES: u32 = 3;
/// Ops a trace holds at most between takes (tens of frames; a battle frame
/// is a few thousand): past it, at a vertical blank, it starts again from
/// the whole VRAM, so a host that runs frames without presenting them
/// keeps no backlog.
const TRACE_LIMIT: usize = 100_000;

/// The indices of a polygon command's vertex words.
fn polygon_points(op: u32) -> impl Iterator<Item = usize> {
    let stride = 1 + usize::from(op & 0x04 != 0) + usize::from(op & 0x10 != 0);
    let count = if op & 0x08 != 0 { 4 } else { 3 };
    (0..count).map(move |k| 1 + k * stride)
}

impl Recorder {
    /// Drop what was traced: the next take starts with all of VRAM as it is
    /// then.
    pub(crate) fn restart(&mut self) {
        self.ops.clear();
        self.full_sync = true;
    }

    /// Projection `index` while the history holds it.
    fn projection(&self, index: u32) -> Option<&Projection> {
        let age = self.history_end.wrapping_sub(index);
        (age >= 1 && age <= self.history_len).then(|| &self.history[index as usize % HISTORY])
    }

    /// The movie macroblocks a transfer to `rect` carries, found by their
    /// output's bytes: a column of them, 16 rows each, as the MDEC output
    /// them (the movie player loads each slice it decoded as it is).
    fn movie_blocks(&mut self, rect: VramRect, pixels: &[u16]) {
        let width = usize::from(rect.width);
        if self.macroblocks.is_empty() || !(width == 16 || width == 24) || rect.height % 16 != 0 {
            return;
        }
        for (k, chunk) in pixels.chunks_exact(width * 16).enumerate() {
            let key = fnv1a(chunk.iter().flat_map(|pixel| pixel.to_le_bytes()));
            if let Some((block, _)) = self.macroblocks.get(&key).filter(|(block, _)| block.bits24 == (width == 24)) {
                self.ops.push(TraceOp::Movie { x: rect.x, y: rect.y + 16 * k as u16, block: block.clone() });
            }
        }
    }

    fn ingest(&mut self, first: u32, projections: &[Projection], stores: &[SxyStore], reveals: &[Reveal]) {
        self.recorded |= !projections.is_empty() || !reveals.is_empty();
        if self.history.is_empty() {
            self.history = vec![Projection::default(); HISTORY];
            self.placed = vec![0; HISTORY];
            self.stored = vec![(0, 0); RAM_WORDS];
        }
        if first != self.history_end {
            // The trace just started or the port's ring overflowed: the
            // projections before these are unknown.
            self.history_end = first;
            self.history_len = 0;
        }
        for projection in projections {
            match projection.class {
                GROUND => {
                    self.grounds.insert(projection.owner, self.history_end);
                }
                FOCUS => self.focus = Some(projection.sz),
                AXIS => {
                    let view = projection.view.map(|c| c as f32);
                    match self.axes.get_mut(projection.owner as usize) {
                        Some(axis) => *axis = view,
                        None if projection.owner == TRANSLATION => self.translation = view,
                        None => {}
                    }
                }
                CAMERA => {
                    // A battle's stage, or the world map's ground and world
                    // point, comes just before its camera.
                    let (stage, ground, origin) = (self.stage.take(), self.ground.take(), self.origin.take());
                    if let Some(mode) = CameraMode::from_owner(projection.owner) {
                        let target = projection.view.map(|c| c as f32);
                        let stage = stage.filter(|_| mode == CameraMode::Battle);
                        let ground = ground.filter(|_| mode == CameraMode::Worldmap);
                        let origin = origin.filter(|_| mode == CameraMode::Worldmap).unwrap_or_default();
                        let around = (projection.owner >> 8) as u8;
                        let camera = Camera { axes: self.axes, translation: self.translation, target, focus: f32::from(projection.sz), mode, stage, ground, origin, around };
                        self.ops.push(TraceOp::Camera(Box::new(camera)));
                    }
                }
                PLACEMENT => {
                    let view = projection.view.map(|c| c as f32);
                    match self.placement_axes.get_mut(usize::from(projection.sz)) {
                        Some(axis) => *axis = view,
                        None if u32::from(projection.sz) == TRANSLATION => {
                            self.placements = self.placements.wrapping_add(1).max(1);
                            let seq = self.placements;
                            self.placed_by.insert(projection.owner, seq);
                            self.last_placed = (projection.owner, seq);
                            self.ops.push(TraceOp::Placement(Placement {
                                seq,
                                owner: projection.owner,
                                packets: projection.x as u32,
                                size: projection.y as u32,
                                axes: self.placement_axes,
                                translation: view,
                            }));
                        }
                        None => {}
                    }
                }
                STAGE => {
                    let point = projection.view.map(|c| c as f32);
                    if projection.owner == STAGE_GROUND {
                        self.ground = Some(point[1]);
                    } else if projection.owner == STAGE_ORIGIN {
                        self.origin = Some(point);
                    } else {
                        let stage = self.stage.get_or_insert_with(Stage::default);
                        match projection.owner {
                            STAGE_LOOK_AT => (stage.look_at, stage.turns) = (point, projection.sz != 0),
                            STAGE_EYE => stage.eye = point,
                            slot => {
                                if let Some(combatant) = stage.combatants.get_mut(slot as usize) {
                                    *combatant = Some(Combatant { position: point, height: f32::from(projection.sz) });
                                }
                            }
                        }
                    }
                }
                EYE => {
                    let eye = Eye { position: projection.view.map(|c| c as f32), axes: self.axes, sprite: projection.owner };
                    self.ops.push(TraceOp::Eye(eye));
                }
                _ => {}
            }
            // A model's or sprite's points take its newest placement.
            let placed = match Class::from_projection(projection.class) {
                Class::Model | Class::Sprite if projection.owner != 0 => {
                    if self.last_placed.0 != projection.owner {
                        self.last_placed = (projection.owner, self.placed_by.get(&projection.owner).copied().unwrap_or(0));
                    }
                    self.last_placed.1
                }
                _ => 0,
            };
            self.history[self.history_end as usize % HISTORY] = *projection;
            self.placed[self.history_end as usize % HISTORY] = placed;
            self.history_end = self.history_end.wrapping_add(1);
        }
        self.history_len = (self.history_len as usize + projections.len()).min(HISTORY) as u32;
        for store in stores {
            let Some(slot) = self.stored.get_mut((store.address & ADDRESS_MASK) as usize / 4) else { continue };
            let source = store.source.filter(|&index| {
                let age = self.history_end.wrapping_sub(index);
                age >= 1 && age <= self.history_len && self.history[index as usize % HISTORY].sxy == store.value
            });
            *slot = (store.value, source.map_or(0, |index| index.wrapping_add(1)));
        }
        for reveal in reveals {
            let Some(&command) = reveal.words.first() else { continue };
            let vertices = usize::from(reveal.vertices);
            if !(0x20..=0x3F).contains(&(command >> 24)) || vertices > 4 || reveal.words.len() != crate::command_words(command) {
                continue;
            }
            let mut points = [None; 4];
            for (k, at) in polygon_points(command >> 24).enumerate().take(vertices) {
                let index = match reveal.order {
                    0 => reveal.projections.wrapping_sub(vertices as u32).wrapping_add(k as u32),
                    order => reveal.projections.wrapping_sub(order >> (8 * k) & 0xFF),
                };
                points[k] = self.projection(index).is_some_and(|p| p.sxy == reveal.words[at]).then_some(index);
            }
            let spare = &mut self.spare;
            self.reveals.entry(reveal.entry & ADDRESS_MASK).or_insert_with(|| spare.pop().unwrap_or_default()).push(Revealed {
                words: reveal.words,
                projections: points,
                frame: self.frames,
            });
        }
        if self.grounds.len() > 1024 {
            let (end, len) = (self.history_end, self.history_len);
            self.grounds.retain(|_, index| {
                let age = end.wrapping_sub(*index);
                age >= 1 && age <= len
            });
        }
        // Owners of other scenes' placements, long gone.
        if self.placed_by.len() > 4096 {
            let newest = self.placements;
            self.placed_by.retain(|_, seq| newest.wrapping_sub(*seq) < 4096);
        }
    }

    /// The index of the projection that made the point `word`, the `at`th
    /// word of the command (its `vertex`th point): the one stored at the
    /// word's address while the word still holds it, or a culled face's own.
    fn point_index(&self, word: u32, at: usize, vertex: usize) -> Option<u32> {
        if let Some(revealed) = &self.revealing {
            return revealed[vertex];
        }
        let address = *self.fifo_addresses.get(at).filter(|&&a| a != 0)?;
        let &(value, source) = self.stored.get(address as usize / 4)?;
        if source == 0 || value != word {
            return None;
        }
        Some(source.wrapping_sub(1))
    }
}

fn sign11(value: u32) -> i32 {
    ((value << 21) as i32) >> 21
}

impl Gpu {
    /// The port assembled an alternate facing for these original projections.
    pub fn trace_sprite_facing(&mut self, first: u32, last: u32, facing: SpriteFacing) {
        if let Some(trace) = &mut self.trace {
            trace.facings.push((first, last, std::sync::Arc::new(facing)));
            // A DMA may consume the previous frame; retain its records too.
            let end = last;
            trace.facings.retain(|(start, _, _)| end.wrapping_sub(*start) < HISTORY as u32);
        }
    }
    /// Turns the draw trace on or off. Turning it on starts the next trace
    /// with the whole VRAM.
    pub fn set_trace(&mut self, enabled: bool) {
        let pending = self.fifo.len();
        self.trace = enabled.then(|| {
            Box::new(Recorder { full_sync: true, fifo_addresses: vec![0; pending], ..Default::default() })
        });
    }

    pub fn trace_enabled(&self) -> bool {
        self.trace.is_some()
    }

    /// Which faces the game culls against its screen the port should
    /// record, for a view wider or farther out than the original's; `None`
    /// records none. Kept while the trace is on; none without a trace.
    pub fn set_reveals(&mut self, reveals: Option<Reveals>) {
        if let Some(trace) = &mut self.trace {
            trace.wanted = reveals.filter(|r| r.reach != [0, 0]);
        }
    }

    /// Takes over the presentation of `live`, the device a snapshot loaded
    /// into this one replaces: its counters, and its draw trace, which goes
    /// on asking for the same culled faces but starts again from all of this
    /// VRAM (what is drawn next follows it), without what the abandoned
    /// timeline recorded (the port's projections, the faces it culled, the
    /// screen points it stored in RAM).
    pub fn keep_presentation(&mut self, live: &mut Gpu) {
        self.stats = std::mem::take(&mut live.stats);
        if let Some(trace) = live.trace.take() {
            self.set_trace(true);
            self.set_reveals(trace.wanted);
            self.trace_resync();
        }
    }

    /// [`Gpu::set_reveals`]'s reach as the port's word: x in bits 0-15, y
    /// in bits 16-31, 0 for none.
    pub fn reveal_reach(&self) -> u32 {
        self.trace.as_ref().and_then(|t| t.wanted).map_or(0, |r| u32::from(r.reach[0]) | u32::from(r.reach[1]) << 16)
    }

    /// [`Gpu::set_reveals`]'s pull-back as the port's word (0 for none).
    pub fn reveal_pull_back(&self) -> u32 {
        self.trace.as_ref().and_then(|t| t.wanted).map_or(0, |r| r.pull_back)
    }

    /// [`Gpu::set_reveals`]'s `around` bits as the port's word (0 for none).
    pub fn reveal_around(&self) -> u32 {
        self.trace.as_ref().and_then(|t| t.wanted).map_or(0, |r| u32::from(r.around))
    }

    /// Points traced since the trace started: matched to their projection
    /// (by address or a culled face's record), and not (2D points). For
    /// validation.
    pub fn trace_matches(&self) -> [u64; 2] {
        self.trace.as_ref().map_or([0; 2], |t| t.matches.get())
    }

    /// What the port recorded since the last call, oldest first: the GTE's
    /// projections (the first one's index in the port's ring is `first`),
    /// the screen points stored and the faces culled at the screen's edges.
    pub fn trace_projections(&mut self, first: u32, projections: &[Projection], stores: &[SxyStore], reveals: &[Reveal]) {
        if let Some(trace) = &mut self.trace {
            trace.ingest(first, projections, stores, reveals);
        }
    }

    /// Everything traced since the last take, ending with the drawn pixels
    /// not yet written.
    pub fn take_trace(&mut self) -> Trace {
        let Some(trace) = self.trace.as_mut() else {
            return Trace::default();
        };
        let mut ops = std::mem::take(&mut trace.ops);
        let focus = trace.focus.take();
        if trace.full_sync {
            trace.full_sync = false;
            trace.dirty = None;
            ops.clear();
            ops.push(TraceOp::Write {
                rect: VramRect { x: 0, y: 0, width: VRAM_WIDTH as u16, height: VRAM_HEIGHT as u16 },
                pixels: self.vram.as_slice().into(),
                upload: true,
            });
            return Trace { ops, focus };
        }
        if let Some(rect) = trace.dirty.take() {
            ops.push(TraceOp::Write { rect, pixels: read_rect(&self.vram, rect).into(), upload: false });
        }
        Trace { ops, focus }
    }

    /// The trace starts again from the whole VRAM as it is now (a snapshot
    /// replaced it): what was traced before is dropped, and what the GPU
    /// does next follows it (a rewind's frames played on from the state).
    pub(crate) fn trace_resync(&mut self) {
        if self.trace.is_none() {
            return;
        }
        let pixels = self.vram.as_slice().into();
        let trace = self.trace.as_mut().expect("checked above");
        trace.full_sync = false;
        trace.dirty = None;
        trace.ops.clear();
        trace.ops.push(TraceOp::Write {
            rect: VramRect { x: 0, y: 0, width: VRAM_WIDTH as u16, height: VRAM_HEIGHT as u16 },
            pixels,
            upload: true,
        });
    }

    /// The DMA walk reached `node` (0: it left a node). The faces culled
    /// at an ordering-table entry are traced when the walk reaches it.
    pub(crate) fn trace_source(&mut self, node: u32) {
        let Some(trace) = &mut self.trace else { return };
        trace.source = node;
        trace.source_index = 0;
        let Some(mut faces) = trace.reveals.remove(&node) else { return };
        // Linked in front of the same packet, the newest goes first, as the
        // GPU walks an entry's packets.
        for face in faces.iter().rev() {
            self.trace.as_mut().expect("checked above").revealing = Some(face.projections);
            self.trace_before_command(&face.words);
            self.trace_polygon(&face.words, true);
            self.trace.as_mut().expect("checked above").revealing = None;
        }
        faces.clear();
        self.trace.as_mut().expect("checked above").spare.push(faces);
    }

    /// The address of the next GP0 word (0 for the CPU's).
    pub(crate) fn trace_word_address(&mut self, address: u32) {
        if let Some(trace) = &mut self.trace {
            trace.word_address = address;
        }
    }

    /// A word joined the command buffer.
    pub(crate) fn trace_fifo_word(&mut self) {
        if let Some(trace) = &mut self.trace {
            trace.fifo_addresses.push(trace.word_address);
        }
    }

    /// The command buffer emptied.
    pub(crate) fn trace_fifo_clear(&mut self) {
        if let Some(trace) = &mut self.trace {
            trace.fifo_addresses.clear();
        }
    }

    pub(crate) fn trace_vblank(&mut self) {
        if let Some(trace) = &mut self.trace {
            if std::mem::take(&mut trace.recorded) {
                trace.frames = trace.frames.wrapping_add(1);
            }
            let now = trace.frames;
            trace.reveals.retain(|_, faces| {
                faces.retain(|face| now.wrapping_sub(face.frame) < REVEAL_FRAMES);
                !faces.is_empty()
            });
            trace.ops.push(TraceOp::VBlank);
            if trace.ops.len() > TRACE_LIMIT {
                trace.restart();
            }
        }
    }

    fn trace_area(&self) -> [u16; 4] {
        let (left, top, right, bottom) = self.drawing_area();
        [left as u16, top as u16, right as u16, bottom as u16]
    }

    fn trace_environment(&self) -> Environment {
        Environment {
            draw_mode: self.draw_mode,
            texture_window: self.texture_window,
            area: self.trace_area(),
            mask: self.mask,
            offset: (sign11(self.draw_offset) as i16, sign11(self.draw_offset >> 11) as i16),
        }
    }

    /// Before a textured command: drawn pixels it may sample go to the trace first.
    pub(crate) fn trace_before_command(&mut self, words: &[u32]) {
        let Some(trace) = &self.trace else { return };
        let Some(dirty) = trace.dirty else { return };
        let op = words[0] >> 24;
        let (page, clut) = match op {
            0x20..=0x3F if op & 0x04 != 0 => {
                let stride = if op & 0x10 != 0 { 3 } else { 2 };
                (words[2 + stride] >> 16, words[2] >> 16)
            }
            0x60..=0x7F if op & 0x04 != 0 => (self.draw_mode, words[2] >> 16),
            _ => return,
        };
        let depth = page >> 7 & 3;
        let page_rect = VramRect {
            x: (page & 0xF) as u16 * 64,
            y: (page >> 4 & 1) as u16 * 256,
            width: match depth { 0 => 64, 1 => 128, _ => 256 },
            height: 256,
        };
        let mut sampled = vec![page_rect];
        if depth < 2 {
            sampled.push(VramRect {
                x: (clut & 0x3F) as u16 * 16,
                y: (clut >> 6 & 0x1FF) as u16,
                width: if depth == 0 { 16 } else { 256 },
                height: 1,
            });
        }
        let mut writes = Vec::new();
        for rect in sampled {
            let rect = VramRect { width: rect.width.min(VRAM_WIDTH as u16 - rect.x), ..rect };
            if let Some(overlap) = rect.intersect(&dirty) {
                writes.push(overlap);
            }
        }
        if writes.is_empty() {
            return;
        }
        // Write the whole dirty rectangle once: later samples need it too.
        let pixels = read_rect(&self.vram, dirty);
        let trace = self.trace.as_mut().expect("checked above");
        trace.ops.push(TraceOp::Write { rect: dirty, pixels: pixels.into(), upload: false });
        trace.dirty = None;
    }

    /// After a command executed: what it drew or changed.
    pub(crate) fn trace_command(&mut self, words: &[u32]) {
        if self.trace.is_none() {
            return;
        }
        let op = words[0] >> 24;
        match op {
            0x02 => {
                let color = words[0] & 0xFF_FFFF;
                let x = (words[1] & 0x3F0) as usize;
                let y = (words[1] >> 16 & 0x1FF) as usize;
                let width = (((words[2] & 0x3FF) + 0xF) & !0xF) as usize;
                let height = (words[2] >> 16 & 0x1FF) as usize;
                for rect in VramRect::wrapped(x, y, width, height) {
                    let pixels = read_rect(&self.vram, rect);
                    let trace = self.trace.as_mut().expect("checked above");
                    trace.ops.push(TraceOp::Fill { rect, color });
                    trace.ops.push(TraceOp::Write { rect, pixels: pixels.into(), upload: false });
                }
            }
            0x20..=0x3F => self.trace_polygon(words, false),
            0x60..=0x7F => self.trace_rectangle(words),
            0x80..=0x9F => {
                let source = crate::Image::from_words(words[1], words[3]);
                let destination = crate::Image::from_words(words[2], words[3]);
                let trace = self.trace.as_mut().expect("checked above");
                trace.ops.push(TraceOp::Copy {
                    source: (source.x, source.y),
                    destination: (destination.x, destination.y),
                    width: source.width,
                    height: source.height,
                });
                self.trace_write(destination.x, destination.y, destination.width, destination.height, false);
            }
            _ => {}
        }
    }

    /// VRAM a transfer (`upload`) or copy changed.
    pub(crate) fn trace_write(&mut self, x: u16, y: u16, width: u16, height: u16, upload: bool) {
        if self.trace.is_none() {
            return;
        }
        for rect in VramRect::wrapped(x.into(), y.into(), width.into(), height.into()) {
            let pixels: std::sync::Arc<[u16]> = read_rect(&self.vram, rect).into();
            let trace = self.trace.as_mut().expect("checked above");
            trace.ops.push(TraceOp::Write { rect, pixels: pixels.clone(), upload });
            if upload {
                trace.movie_blocks(rect, &pixels);
            }
        }
    }

    /// The colour macroblocks the MDEC decoded, by the key of their output's
    /// bytes (xem_media's `Macroblock::key`), for the transfers that load
    /// that output into VRAM ([`TraceOp::Movie`]). Presentation only.
    pub fn trace_macroblocks(&mut self, blocks: impl IntoIterator<Item = (u64, MovieBlock)>) {
        let Some(trace) = &mut self.trace else { return };
        for (key, block) in blocks {
            // A block the MDEC decodes again (a still part of the picture)
            // stays as long as it keeps coming: its earlier place lapses.
            trace.decoded_macroblocks += 1;
            trace.macroblocks.insert(key, (std::sync::Arc::new(block), trace.decoded_macroblocks));
            trace.macroblock_keys.push_back((key, trace.decoded_macroblocks));
            while trace.macroblock_keys.len() > MACROBLOCKS
                && let Some((oldest, count)) = trace.macroblock_keys.pop_front()
            {
                if trace.macroblocks.get(&oldest).is_some_and(|(_, latest)| *latest == count) {
                    trace.macroblocks.remove(&oldest);
                }
            }
        }
    }

    /// The frames a movie that ends on the picture the game goes on from has
    /// after the one its player loaded last ([`TraceOp::MovieFramesLeft`]).
    /// Presentation only.
    pub fn trace_movie_frames_left(&mut self, frames: u16) {
        if let Some(trace) = &mut self.trace {
            trace.ops.push(TraceOp::MovieFramesLeft(frames));
        }
    }

    /// The vertex `word`, the `at`th word of the command (usize::MAX when
    /// it is not in the command buffer) and its `index`th point: with its
    /// projection's class, offset, projection distance, owner, depth,
    /// whether the backdrop made it, and its placement.
    fn trace_vertex(&self, trace: &Recorder, word: u32, at: usize, index: usize, color: u32, uv: u32) -> (TraceVertex, Option<Projected>) {
        let (x, y) = self.vertex(word);
        let (dx, dy) = (x - sign11(word), y - sign11(word >> 16));
        let mut vertex = TraceVertex {
            x,
            y,
            fx: x as f32,
            fy: y as f32,
            z: 0.0,
            view: None,
            color: color & 0xFF_FFFF,
            u: (uv & 0xFF) as i16,
            v: (uv >> 8 & 0xFF) as i16,
        };
        let found = trace.point_index(word, at, index).and_then(|i| Some((trace.projection(i)?, trace.placed[i as usize % HISTORY])));
        let mut counts = trace.matches.get();
        counts[usize::from(found.is_none())] += 1;
        trace.matches.set(counts);
        let class = found.filter(|(p, _)| p.class != INTERFACE).map(|(p, placement)| {
            // The sub-pixel point, also where the GTE saturated the word.
            vertex.fx = p.x as f32 / 65536.0 + dx as f32;
            vertex.fy = p.y as f32 / 65536.0 + dy as f32;
            vertex.z = f32::from(p.sz.max(1));
            vertex.view = Some(p.view.map(|c| c as f32));
            Projected {
                class: Class::from_projection(p.class),
                offset: p.offset,
                h: p.h,
                owner: p.owner,
                sz: p.sz,
                backdrop: p.class == BACKDROP,
                placement,
            }
        });
        (vertex, class)
    }

    /// The floor point under sprite `owner` in VRAM coordinates, with its depth.
    fn ground(&self, trace: &Recorder, owner: u32) -> Option<[f32; 3]> {
        let p = trace.projection(*trace.grounds.get(&owner)?)?;
        let offset = (sign11(self.draw_offset), sign11(self.draw_offset >> 11));
        (p.sz > 0).then(|| [p.x as f32 / 65536.0 + offset.0 as f32, p.y as f32 / 65536.0 + offset.1 as f32, f32::from(p.sz)])
    }

    /// A polygon command after it executed, or a culled face (`revealed`)
    /// that never reaches the GPU.
    fn trace_polygon(&mut self, words: &[u32], revealed: bool) {
        let trace = self.trace.as_ref().expect("checked by the caller");
        let op = words[0] >> 24;
        let gouraud = op & 0x10 != 0;
        let count = if op & 0x08 != 0 { 4 } else { 3 };
        let textured = op & 0x04 != 0 && !self.texture_disabled();
        let raw = op & 0x01 != 0;
        let mut vertices = [TraceVertex::default(); 4];
        let mut classes = Vec::new();
        let (mut clut, mut page) = (0, 0);
        let mut next = 1;
        for (index, vertex) in vertices.iter_mut().take(count).enumerate() {
            let color = if index > 0 && gouraud {
                next += 1;
                words[next - 1]
            } else {
                words[0]
            };
            let at = next;
            next += 1;
            let mut uv = 0;
            if op & 0x04 != 0 {
                uv = words[next];
                next += 1;
                match index {
                    0 => clut = (uv >> 16) as u16,
                    1 => page = uv >> 16,
                    _ => {}
                }
            }
            let (traced, class) = self.trace_vertex(trace, words[at], at, index, color, uv);
            *vertex = traced;
            classes.extend(class);
        }
        // Every point projected at the projection distance (H) is drawn at
        // its screen size: interface the GTE places (the field menu's
        // counter backing, the world map's marker), traced as interface.
        // So is a polygon the game draws with every point the GTE put at
        // depth 0, the divide overflowing (the field's screen pieces: the
        // GTE used to zoom a picture of the screen about its centre); a
        // culled face there lies behind the game's camera, in the view of
        // a camera pulled back.
        let at_h = classes.iter().zip(&vertices).all(|(c, v)| v.z == f32::from(c.h));
        let at_zero = !revealed && classes.iter().all(|c| c.sz == 0);
        if classes.len() == count && (at_h || at_zero) {
            for v in &mut vertices[..count] {
                (v.fx, v.fy, v.z, v.view) = (v.x as f32, v.y as f32, 0.0, None);
            }
            classes.clear();
        }
        let mut environment = self.trace_environment();
        if revealed && op & 0x04 != 0 {
            // Executed, the polygon's page would have set GP0(E1h) bits 0-8 and 11.
            environment.draw_mode = (environment.draw_mode & !0x9FF) | (page & 0x9FF);
        }
        let projection = classes.first().map(|c| (c.offset, c.h));
        let (owner, placement) = classes.first().map_or((0, 0), |c| (c.owner, c.placement));
        let backdrop = classes.len() == count && classes.iter().all(|c| c.backdrop);
        let classes: Vec<Class> = classes.iter().map(|c| c.class).collect();
        let class = primitive_class(&classes, count);
        let ground = if class == Class::Sprite { self.ground(trace, owner) } else { None };
        let facing_index = trace.point_index(words[1], 1, 0).and_then(|point| {
            trace.facings.iter().rposition(|(first, last, _)| point.wrapping_sub(*first) < last.wrapping_sub(*first))
        });
        let facing = facing_index.map(|index| {
            let facing = &mut self.trace.as_mut().expect("tracing").facings[index].2;
            if facing.restore.is_empty() && !facing.textures.is_empty() {
                let facing = std::sync::Arc::make_mut(facing);
                facing.restore = facing.textures.iter().map(|(rect, _)| (*rect, read_rect(&self.vram, *rect).into())).collect();
            }
            facing.clone()
        });
        let dither = self.draw_mode & 0x200 != 0 && (gouraud || (textured && !raw));
        let primitive = Primitive {
            shape: if count == 4 { Shape::Quad } else { Shape::Triangle },
            vertices,
            textured,
            raw,
            gouraud,
            semi: op & 0x02 != 0,
            dither,
            clut,
            environment,
            class,
            projection,
            owner,
            placement,
            source: 0,
            revealed,
            ground,
            facing,
            backdrop,
        };
        self.trace_drawn(primitive);
    }

    fn trace_rectangle(&mut self, words: &[u32]) {
        let trace = self.trace.as_ref().expect("checked by the caller");
        let op = words[0] >> 24;
        let textured = op & 0x04 != 0 && !self.texture_disabled();
        let mut next = 2;
        let mut uv = 0;
        if op & 0x04 != 0 {
            uv = words[next];
            next += 1;
        }
        let (width, height) = match op >> 3 & 3 {
            0 => ((words[next] & 0x3FF) as i32, (words[next] >> 16 & 0x1FF) as i32),
            1 => (1, 1),
            2 => (8, 8),
            _ => (16, 16),
        };
        let (corner, _) = self.trace_vertex(trace, words[1], 1, 0, words[0], uv);
        let flip_x = self.draw_mode & 0x1000 != 0;
        let flip_y = self.draw_mode & 0x2000 != 0;
        let (u0, v0) = (i32::from(corner.u), i32::from(corner.v));
        // Texture coordinates of the far edges: one texel per pixel, the
        // last pixel samples u0 + width - 1 (or u0 - width + 1 flipped).
        let u1 = if flip_x { u0 - width } else { u0 + width };
        let v1 = if flip_y { v0 - height } else { v0 + height };
        let at = |dx: i32, dy: i32, u: i32, v: i32| TraceVertex {
            x: corner.x + dx,
            y: corner.y + dy,
            fx: (corner.x + dx) as f32,
            fy: (corner.y + dy) as f32,
            z: 0.0,
            view: None,
            color: corner.color,
            u: u as i16,
            v: v as i16,
        };
        let vertices = [
            at(0, 0, u0, v0),
            at(width, 0, u1, v0),
            at(0, height, u0, v1),
            at(width, height, u1, v1),
        ];
        let primitive = Primitive {
            shape: Shape::Rectangle,
            vertices,
            textured,
            raw: op & 0x01 != 0,
            gouraud: false,
            semi: op & 0x02 != 0,
            dither: false,
            clut: (uv >> 16) as u16,
            environment: self.trace_environment(),
            class: Class::Flat,
            projection: None,
            owner: 0,
            placement: 0,
            source: 0,
            revealed: false,
            ground: None,
            facing: None,
            backdrop: false,
        };
        self.trace_drawn(primitive);
    }

    /// A line segment (also each segment of a polyline).
    pub(crate) fn trace_line(&mut self, command: u32, color0: u32, vertex0: u32, color1: u32, vertex1: u32) {
        let Some(trace) = self.trace.as_ref() else { return };
        let op = command >> 24;
        let gouraud = op & 0x10 != 0;
        let (a, class_a) = self.trace_vertex(trace, vertex0, usize::MAX, 0, color0, 0);
        let (b, class_b) = self.trace_vertex(trace, vertex1, usize::MAX, 1, if gouraud { color1 } else { color0 }, 0);
        let projection = class_a.or(class_b).map(|c| (c.offset, c.h));
        let (owner, placement) = class_a.or(class_b).map_or((0, 0), |c| (c.owner, c.placement));
        let backdrop = class_a.zip(class_b).is_some_and(|(a, b)| a.backdrop && b.backdrop);
        let classes: Vec<Class> = class_a.into_iter().chain(class_b).map(|c| c.class).collect();
        let primitive = Primitive {
            shape: Shape::Line,
            vertices: [a, b, TraceVertex::default(), TraceVertex::default()],
            textured: false,
            raw: false,
            gouraud,
            semi: op & 0x02 != 0,
            dither: self.draw_mode & 0x200 != 0 && gouraud,
            clut: 0,
            environment: self.trace_environment(),
            class: primitive_class(&classes, 2),
            projection,
            owner,
            placement,
            source: 0,
            revealed: false,
            ground: None,
            facing: None,
            backdrop,
        };
        self.trace_drawn(primitive);
    }

    fn trace_drawn(&mut self, primitive: Primitive) {
        let [left, top, right, bottom] = primitive.environment.area;
        let vertices = &primitive.vertices[..primitive.vertex_count()];
        let min_x = vertices.iter().map(|v| v.x).min().unwrap_or(0).max(i32::from(left));
        let max_x = vertices.iter().map(|v| v.x).max().unwrap_or(0).min(i32::from(right));
        let min_y = vertices.iter().map(|v| v.y).min().unwrap_or(0).max(i32::from(top));
        let max_y = vertices.iter().map(|v| v.y).max().unwrap_or(0).min(i32::from(bottom));
        let trace = self.trace.as_mut().expect("checked by the caller");
        let mut primitive = primitive;
        if primitive.revealed {
            // Never drawn: VRAM is unchanged.
            trace.ops.push(TraceOp::Draw(primitive));
            return;
        }
        if trace.source != 0 {
            primitive.source = trace.source << 8 | (trace.source_index & 0xFF);
            trace.source_index += 1;
        }
        if min_x <= max_x && min_y <= max_y && max_x >= 0 && max_y >= 0 {
            let rect = VramRect {
                x: min_x.max(0) as u16,
                y: min_y.max(0) as u16,
                width: (max_x.min(VRAM_WIDTH as i32 - 1) - min_x.max(0) + 1) as u16,
                height: (max_y.min(VRAM_HEIGHT as i32 - 1) - min_y.max(0) + 1) as u16,
            };
            trace.dirty = Some(trace.dirty.map_or(rect, |dirty| dirty.union(&rect)));
        }
        trace.ops.push(TraceOp::Draw(primitive));
    }
}

/// A primitive's class: Flat unless every vertex was projected; a sprite
/// part if any vertex belongs to one.
fn primitive_class(classes: &[Class], count: usize) -> Class {
    if classes.len() < count {
        return Class::Flat;
    }
    if classes.contains(&Class::Sprite) {
        Class::Sprite
    } else if classes.contains(&Class::Model) {
        Class::Model
    } else {
        Class::World
    }
}

fn read_rect(vram: &[u16], rect: VramRect) -> Vec<u16> {
    let mut pixels = Vec::with_capacity(usize::from(rect.width) * usize::from(rect.height));
    for row in 0..usize::from(rect.height) {
        let start = (usize::from(rect.y) + row) * VRAM_WIDTH + usize::from(rect.x);
        pixels.extend_from_slice(&vram[start..start + usize::from(rect.width)]);
    }
    pixels
}
