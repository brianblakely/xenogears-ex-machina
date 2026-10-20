//! The game renderer: xem-gpu's draw trace drawn again with wgpu at any
//! scale, with each of the original's limits selectable.
//!
//! The software GPU keeps the authoritative VRAM; this renderer only
//! presents. It keeps a scaled picture of all of VRAM (colour, and an
//! auxiliary target holding each pixel's GTE depth and class for the
//! HD-2D effects), so double buffering, copies between screen regions and
//! the saved-screen slot work as on the console, and a copy of the native
//! 16-bit VRAM for texture sampling, which the trace keeps equal to the
//! device's at every command.

mod diorama;
mod effects;
mod frame;
mod movie;
mod view;

pub use diorama::{DioramaEye, DioramaPresented, DioramaSpace, DioramaStats, Table, Turns, XrView};
pub use effects::{DebugView, Effects, EffectsRenderer, Lighting, Quality, SceneAnalysis, SpriteGroup};
pub use frame::{FrameRenderer, Framing, Interface, Look, Welder, central_depth, first_person, interpolate, pull_back, weld};
pub use movie::{MovieRenderer, MovieTiles, Planes, fitted, planes};
pub use view::{GameView, HeadsetEye, HeadsetPresented, HostClass, MAX_WIDE_SIDE, Presented, ViewConfig, Viewpoint, headset_movie_output, scene_zoom, wide_side, xr_view};

use bytemuck::{Pod, Zeroable};
use std::collections::HashMap;
use xem_gpu::{Class, Primitive, Shape, Trace, TraceOp, VramRect};

pub const VRAM_WIDTH: u32 = 1024;
pub const VRAM_HEIGHT: u32 = 512;

/// The scaled VRAM picture's colour format (8-bit sRGB-encoded values, as
/// the console's DAC outputs them; alpha is the mask bit).
pub const COLOR_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8Unorm;
/// Per pixel: depth (r, g: 16 bits), class (b), flags (a: bit 0 baked
/// shading; 255 a semi-transparent interface pixel).
pub const AUX_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8Unorm;
const DEPTH_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Depth32Float;

/// Auxiliary classes (the b channel, 0-255).
pub const AUX_BACKGROUND: u32 = 0;
pub const AUX_INTERFACE: u32 = 1;
pub const AUX_WORLD: u32 = 2;
pub const AUX_MODEL: u32 = 3;
pub const AUX_SPRITE: u32 = 4;

const FLAG_TEXTURED: u32 = 1;
const FLAG_RAW: u32 = 2;
const FLAG_DITHER: u32 = 4;
const FLAG_SEMI: u32 = 8;
const FLAG_MASK_SET: u32 = 16;
const FLAG_LINEAR: u32 = 32;
const FLAG_RECTANGLE: u32 = 64;
const FLAG_BAKED: u32 = 128;
const BLEND_SHIFT: u32 = 8;
/// Texels are clamped to the polygon's own texture coordinates ([`Vertex::limits`]).
const FLAG_CLAMP: u32 = 2048;
const CLASS_SHIFT: u32 = 12;
/// Drawn as the original draws it whatever the pass's fidelity (the
/// interface: text, windows, portraits, menus).
const FLAG_ORIGINAL: u32 = 1 << 16;
const FLAG_LINE: u32 = 1 << 17;
/// Textured from a screen the game took ([`Capture`]): the picture presented
/// then, its sides included.
const FLAG_CAPTURE: u32 = 1 << 18;
const FLAG_FEEDBACK: u32 = 1 << 19;
/// A diorama vertex ([`diorama`]) in the table's space, not the game's
/// world (the backdrop's wall); one in clip space (a fade over a whole
/// target).
const FLAG_TABLE: u32 = 1 << 20;
const FLAG_CLIP: u32 = 1 << 21;
/// A diorama vertex of a sprite's billboard part, lifted by its `w` (world
/// units) along the billboards' normal towards the side the eye sees.
const FLAG_BILLBOARD: u32 = 1 << 22;

/// Which of the original GPU's limits a picture keeps. Each is independent.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Fidelity {
    /// Textures interpolate linearly in screen space (affine), not
    /// perspective-correct from the GTE depth.
    pub affine_textures: bool,
    /// Vertices at the GTE's integer screen points, not its sub-pixel ones.
    pub vertex_snapping: bool,
    /// Colours truncated to 5 bits per channel, as VRAM stores them.
    pub color_15bit: bool,
    /// The 4x4 ordered dither of the primitives the game dithers.
    pub dithering: bool,
    /// Rasterize at the native resolution (scale 1) whatever the output size.
    pub native_resolution: bool,
    /// The ordering table's painter's order. Off, projected primitives are
    /// also depth-tested by their GTE depth (it breaks intentional masks).
    pub painter_ordering: bool,
}

impl Fidelity {
    /// Every limit of the original.
    pub const ORIGINAL: Fidelity = Fidelity {
        affine_textures: true,
        vertex_snapping: true,
        color_15bit: true,
        dithering: true,
        native_resolution: true,
        painter_ordering: true,
    };
    /// None of the limits except the original's drawing order.
    pub const MODERN: Fidelity = Fidelity {
        affine_textures: false,
        vertex_snapping: false,
        color_15bit: false,
        dithering: false,
        native_resolution: false,
        painter_ordering: true,
    };
}

/// Texture filtering by what is drawn.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Filtering {
    pub world_linear: bool,
    pub sprites_linear: bool,
    pub interface_linear: bool,
}

/// Optional culling. Both kinds are off by default. Neither changes what
/// the game decides: its own culling (back faces by NCLIP, OTZ 0, the world
/// map's far terrain) happens before anything reaches the GPU, clipping to
/// the drawing area is reproduced with scissors (with the GPU's size limit:
/// triangles 1024 or more wide or 512 or more high are not drawn, counted
/// in [`DrawStats::rejected_size`]), and intentional hiding (actor flags,
/// fades, masks) and gameplay activation (on-screen tests against the
/// original camera and screen) are game state.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Culling {
    /// Viewport culling as the game does it: a view wider or farther out
    /// than the original's shows only what the game drew for its own 4:3
    /// screen. Off (the default), it also shows the faces the game culled
    /// against that screen ([`Primitive::revealed`]): beside the original
    /// screen with wider framing, everywhere when the camera is pulled back.
    pub viewport: bool,
    /// Drop projected triangles wound against the frame's majority, a
    /// quad's both as the quad winds (the game already culled its back
    /// faces; this also drops the double-sided faces it meant to show).
    pub back_faces: bool,
}

/// What the renderer did with the primitives of a draw (observation only).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct DrawStats {
    pub primitives: u32,
    pub triangles: u32,
    /// The original GPU's size limit (original behaviour, not culling).
    pub rejected_size: u32,
    pub culled_back_faces: u32,
    /// Faces the game culled that this draw showed.
    pub revealed: u32,
}

/// Where a draw shows the faces the game culled against its screen.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub(crate) enum Reveal {
    /// Nowhere (the VRAM picture, the original framing, viewport culling).
    #[default]
    Hidden,
    /// Beside the drawing area only, in the wider framing's sides.
    Sides,
    /// Wherever they fall (a pulled-back camera, or one looking around).
    Everywhere,
}

/// A camera within the scene (first-person inspection, [`frame::first_person`]):
/// the scene's points are in its space ([`xem_gpu::TraceVertex::view`]:
/// right, down, forward), polygons reaching nearer than `distance` are
/// clipped there, and `h` and `shift` project their points as
/// [`frame::Look`]'s do.
#[derive(Clone, Copy, Debug, PartialEq)]
pub(crate) struct Near {
    pub(crate) distance: f32,
    pub(crate) h: [f32; 2],
    pub(crate) shift: [f32; 2],
}

/// [`PassUniform::options`]: depth from its reciprocal, which interpolates
/// across the screen as the true depth does (a camera within the scene sees
/// polygons spanning much of its depth range; draw.wgsl), reversed: nearer
/// is larger, where a float keeps its precision at any distance
/// ([`DepthMode::Nearer`], whose fragments write their depth: GL's clip
/// space would keep a step of 6e-8 near the far plane; fills write the far
/// plane, 0).
pub(crate) const PASS_RECIPROCAL_DEPTH: u32 = 16;
/// [`PassUniform::options`]: an eye of the diorama ([`diorama`]: drawn only
/// inside the table's volume, coverage in alpha) and its interface pane
/// (premultiplied, coverage in alpha).
pub(crate) const PASS_WORLD: u32 = 32;
pub(crate) const PASS_PANE: u32 = 64;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DrawOptions {
    pub fidelity: Fidelity,
    pub filtering: Filtering,
    pub culling: Culling,
}

impl Default for DrawOptions {
    fn default() -> Self {
        Self { fidelity: Fidelity::MODERN, filtering: Filtering::default(), culling: Culling::default() }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Pod, Zeroable)]
pub(crate) struct Vertex {
    position: [f32; 2],
    depth: f32,
    w: f32,
    /// 0-1 per channel; a row of an interface triangle may extrapolate
    /// past them at its corners.
    color: [f32; 4],
    uv: [f32; 2],
    mode: [u32; 4],
    /// The texel range of the polygon (with [`FLAG_CLAMP`]): u and v
    /// minimum (bits 0-7, 8-15) and maximum (16-23, 24-31).
    limits: u32,
}

const VERTEX_ATTRIBUTES: [wgpu::VertexAttribute; 7] = wgpu::vertex_attr_array![
    0 => Float32x2, 1 => Float32, 2 => Float32, 3 => Float32x4, 4 => Float32x2, 5 => Uint32x4, 6 => Uint32
];

#[repr(C)]
#[derive(Clone, Copy, Pod, Zeroable)]
pub(crate) struct PassUniform {
    pub(crate) origin: [f32; 2],
    pub(crate) extent: [f32; 2],
    pub(crate) scale: f32,
    pub(crate) options: u32,
    /// The scale of the captured screen's picture ([`Capture`]) and its
    /// sides' width (VRAM pixels).
    pub(crate) capture_scale: f32,
    pub(crate) capture_side: u32,
    /// Where the captured screen lies in VRAM: x, y, width, height (width
    /// 0: none).
    pub(crate) capture: [u32; 4],
    /// The diorama's ([`diorama`]; zero elsewhere): an eye's clip space from
    /// the game's world and from the table's space, the world in the table's
    /// space, and the table's volume there.
    pub(crate) world_to_clip: [[f32; 4]; 4],
    pub(crate) table_to_clip: [[f32; 4]; 4],
    pub(crate) world_to_table: [[f32; 4]; 4],
    pub(crate) volume_min: [f32; 4],
    pub(crate) volume_max: [f32; 4],
    /// The eye's point in the game's world, and the sprites' billboards'
    /// normal there ([`FLAG_BILLBOARD`]).
    pub(crate) eye: [f32; 4],
    pub(crate) billboard: [f32; 4],
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) enum Fragment {
    Opaque,
    OpaqueTexels,
    Semi,
    /// Fills and uploads.
    Rect,
    /// Sprite shadows into the shadow mask (effects.rs).
    Shadow,
    /// An interface primitive's semi-transparent pixels marked as interface
    /// in the auxiliary target, the colour unchanged (after an additive or
    /// subtractive interface draw, whose blend cannot also set the class).
    Mark,
    /// A full-screen fade over a diorama's eye or pane (draw.wgsl `fs_tint`;
    /// blend 0-3 its mode, 4 opaque).
    Tint,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) enum DepthMode {
    /// Drawn over everything; depth unchanged.
    Over,
    /// Tested and written (depth-tested ordering).
    Test,
    /// Tested and written where depth is reversed, nearer larger (a camera
    /// within the scene: [`PASS_RECIPROCAL_DEPTH`]), the depth written by
    /// the fragment (draw.wgsl `NearerTargets`).
    Nearer,
    /// Tested, not written (a diorama's translucent primitives).
    Read,
    /// Writes the far plane (fills).
    Clear,
}

/// Where a draw goes: the VRAM picture or a replay of the frame (`Screen`),
/// a diorama's eyes (`World`: points in the game's world or the table's
/// space through each eye's matrices; `cull` drops the faces an eye sees
/// from behind), or its interface pane (`Pane`, drawn as the screen is).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) enum Space {
    Screen,
    World { cull: bool },
    Pane,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) struct PipelineKey {
    pub(crate) fragment: Fragment,
    pub(crate) blend: u32,
    pub(crate) depth: DepthMode,
    pub(crate) space: Space,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub(crate) struct Scissor {
    pub(crate) x: i32,
    pub(crate) y: i32,
    pub(crate) width: i32,
    pub(crate) height: i32,
}

#[derive(Clone, Debug)]
pub(crate) enum Command {
    Draw { key: PipelineKey, scissor: Option<Scissor>, vertices: std::ops::Range<u32> },
    /// Staged pixels to the native VRAM copy.
    Upload { offset: u64, rect: VramRect },
    Copy { source: (u16, u16), destination: (u16, u16), width: u16, height: u16, local: Option<u32> },
    /// The scene before an in-frame framebuffer effect reads it.
    Snapshot,
}

/// The pipelines and layouts every PS1 target shares (the VRAM picture,
/// framed replays): one per fragment kind, blend mode, auxiliary write and
/// depth mode, created when first used.
pub(crate) struct Pipelines {
    shader: wgpu::ShaderModule,
    pub(crate) layout: wgpu::BindGroupLayout,
    pipeline_layout: wgpu::PipelineLayout,
    pipelines: HashMap<PipelineKey, wgpu::RenderPipeline>,
}

impl Pipelines {
    fn new(device: &wgpu::Device) -> Self {
        let shader = device.create_shader_module(wgpu::include_wgsl!("draw.wgsl"));
        let layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("ps1 pass"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::VERTEX_FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: wgpu::BufferSize::new(size_of::<PassUniform>() as u64),
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Uint,
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 2,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 3,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 4,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 5,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
            ],
        });
        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("ps1"),
            bind_group_layouts: &[Some(&layout)],
            immediate_size: 0,
        });
        Self { shader, layout, pipeline_layout, pipelines: HashMap::new() }
    }

    pub(crate) fn get(&mut self, device: &wgpu::Device, key: PipelineKey) -> &wgpu::RenderPipeline {
        self.pipelines.entry(key).or_insert_with(|| {
            use wgpu::{BlendComponent as C, BlendFactor as F, BlendOperation as O};
            // draw.wgsl fs_semi: the weight is the source alpha, and both
            // targets keep their stored alpha.
            let keep = C { src_factor: F::Zero, dst_factor: F::One, operation: O::Add };
            let blend = match (key.fragment, key.space) {
                // Weight 0 for the colour, 1 for the auxiliary target, its
                // alpha included (flags 255).
                (Fragment::Mark, _) => {
                    let over = C { src_factor: F::SrcAlpha, dst_factor: F::OneMinusSrcAlpha, operation: O::Add };
                    Some(wgpu::BlendState { color: over, alpha: over })
                }
                // The pane is premultiplied (draw.wgsl fs_semi). B-F takes F
                // off as much of it as covers (the pane's alpha) and covers
                // what it does not by F's brightest channel.
                (Fragment::Semi, Space::Pane) if key.blend == 2 => Some(wgpu::BlendState {
                    color: C { src_factor: F::DstAlpha, dst_factor: F::One, operation: O::ReverseSubtract },
                    alpha: C { src_factor: F::OneMinusDstAlpha, dst_factor: F::One, operation: O::Add },
                }),
                (Fragment::Semi, Space::Pane) => {
                    let over = C { src_factor: F::One, dst_factor: F::OneMinusSrcAlpha, operation: O::Add };
                    Some(wgpu::BlendState { color: over, alpha: over })
                }
                (Fragment::Semi, _) => Some(wgpu::BlendState {
                    color: match key.blend {
                        0 => C { src_factor: F::SrcAlpha, dst_factor: F::OneMinusSrcAlpha, operation: O::Add },
                        2 => C { src_factor: F::SrcAlpha, dst_factor: F::One, operation: O::ReverseSubtract },
                        _ => C { src_factor: F::SrcAlpha, dst_factor: F::One, operation: O::Add },
                    },
                    alpha: keep,
                }),
                // Weighted by the target's coverage (draw.wgsl fs_tint).
                (Fragment::Tint, _) => Some(wgpu::BlendState {
                    color: match key.blend {
                        0 => C { src_factor: F::DstAlpha, dst_factor: F::OneMinusSrcAlpha, operation: O::Add },
                        2 => C { src_factor: F::DstAlpha, dst_factor: F::One, operation: O::ReverseSubtract },
                        4 => C { src_factor: F::DstAlpha, dst_factor: F::OneMinusDstAlpha, operation: O::Add },
                        _ => C { src_factor: F::DstAlpha, dst_factor: F::One, operation: O::Add },
                    },
                    alpha: keep,
                }),
                _ => None,
            };
            // A diorama's eyes take their points through the eye's matrices.
            // Reversed depth is written by the fragment (draw.wgsl
            // NearerTargets: in full precision on GL too).
            let vertex = if matches!(key.space, Space::World { .. }) { "vs_world" } else { "vs_main" };
            let nearer = key.depth == DepthMode::Nearer;
            let (vertex, fragment) = match key.fragment {
                Fragment::Opaque => (vertex, if nearer { "fs_opaque_nearer" } else { "fs_opaque" }),
                Fragment::OpaqueTexels => (vertex, if nearer { "fs_opaque_texels_nearer" } else { "fs_opaque_texels" }),
                Fragment::Semi => (vertex, if nearer { "fs_semi_nearer" } else { "fs_semi" }),
                Fragment::Rect => ("vs_rect", "fs_rect"),
                Fragment::Shadow => ("vs_main", "fs_shadow"),
                Fragment::Mark => (vertex, if nearer { "fs_mark_nearer" } else { "fs_mark" }),
                Fragment::Tint => ("vs_world", "fs_tint"),
            };
            let shadow = key.fragment == Fragment::Shadow;
            let picture_targets = [
                Some(wgpu::ColorTargetState { format: COLOR_FORMAT, blend, write_mask: wgpu::ColorWrites::ALL }),
                Some(wgpu::ColorTargetState { format: AUX_FORMAT, blend, write_mask: wgpu::ColorWrites::ALL }),
            ];
            let max = C { src_factor: F::One, dst_factor: F::One, operation: O::Max };
            let shadow_targets = [Some(wgpu::ColorTargetState {
                format: wgpu::TextureFormat::Rgba8Unorm,
                blend: Some(wgpu::BlendState { color: max, alpha: max }),
                write_mask: wgpu::ColorWrites::ALL,
            })];
            let (compare, write) = match key.depth {
                DepthMode::Over => (wgpu::CompareFunction::Always, false),
                DepthMode::Test => (wgpu::CompareFunction::LessEqual, true),
                DepthMode::Nearer => (wgpu::CompareFunction::GreaterEqual, true),
                DepthMode::Read => (wgpu::CompareFunction::LessEqual, false),
                DepthMode::Clear => (wgpu::CompareFunction::Always, true),
            };
            // A diorama emits the faces it may cull counter-clockwise seen
            // from their front ([`diorama`]).
            let cull = matches!(key.space, Space::World { cull: true });
            device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
                label: Some("ps1 primitives"),
                layout: Some(&self.pipeline_layout),
                vertex: wgpu::VertexState {
                    module: &self.shader,
                    entry_point: Some(vertex),
                    compilation_options: Default::default(),
                    buffers: &[Some(wgpu::VertexBufferLayout {
                        array_stride: size_of::<Vertex>() as u64,
                        step_mode: wgpu::VertexStepMode::Vertex,
                        attributes: &VERTEX_ATTRIBUTES,
                    })],
                },
                primitive: wgpu::PrimitiveState {
                    front_face: wgpu::FrontFace::Ccw,
                    cull_mode: cull.then_some(wgpu::Face::Back),
                    ..Default::default()
                },
                depth_stencil: (!shadow).then(|| wgpu::DepthStencilState {
                    format: DEPTH_FORMAT,
                    depth_write_enabled: Some(write),
                    depth_compare: Some(compare),
                    stencil: Default::default(),
                    bias: Default::default(),
                }),
                multisample: Default::default(),
                fragment: Some(wgpu::FragmentState {
                    module: &self.shader,
                    entry_point: Some(fragment),
                    compilation_options: Default::default(),
                    targets: if shadow { &shadow_targets } else { &picture_targets },
                }),
                multiview_mask: None,
                cache: None,
            })
        })
    }
}

/// A growable buffer written through the queue.
pub(crate) struct Stream {
    buffer: wgpu::Buffer,
    usage: wgpu::BufferUsages,
    label: &'static str,
}

impl Stream {
    pub(crate) fn new(device: &wgpu::Device, label: &'static str, usage: wgpu::BufferUsages) -> Self {
        let usage = usage | wgpu::BufferUsages::COPY_DST;
        let buffer = device.create_buffer(&wgpu::BufferDescriptor { label: Some(label), size: 1 << 16, usage, mapped_at_creation: false });
        Self { buffer, usage, label }
    }

    pub(crate) fn write(&mut self, device: &wgpu::Device, queue: &wgpu::Queue, bytes: &[u8]) {
        if bytes.is_empty() {
            return;
        }
        let size = (bytes.len() as u64).next_multiple_of(wgpu::COPY_BUFFER_ALIGNMENT);
        if size > self.buffer.size() {
            self.buffer = device.create_buffer(&wgpu::BufferDescriptor {
                label: Some(self.label),
                size: size.next_power_of_two(),
                usage: self.usage,
                mapped_at_creation: false,
            });
        }
        if bytes.len() as u64 == size {
            queue.write_buffer(&self.buffer, 0, bytes);
        } else {
            let mut padded = bytes.to_vec();
            padded.resize(size as usize, 0);
            queue.write_buffer(&self.buffer, 0, &padded);
        }
    }

    pub(crate) fn buffer(&self) -> &wgpu::Buffer {
        &self.buffer
    }
}

/// Builds vertices and commands from trace operations.
pub(crate) struct Builder<'a> {
    pub(crate) options: &'a DrawOptions,
    pub(crate) vertices: Vec<Vertex>,
    commands: Vec<Command>,
    staging: Vec<u8>,
    /// Back-face culling: the winding most projected triangles of the trace have.
    front: i32,
    /// The scene's projection offset (see [`scene_offset`]).
    pub(crate) scene: Option<(i16, i16)>,
    pub(crate) stats: DrawStats,
    /// Horizontal VRAM range treated as inside the drawing area beyond it
    /// (wider framing): scene primitives may spill this far.
    pub(crate) spill: u32,
    /// Where the faces the game culled are shown.
    pub(crate) reveal: Reveal,
    /// A camera within the scene: its near plane clips the scene's
    /// polygons, which neither the GPU's size limit nor back-face culling
    /// drops, and depth is tested whatever the ordering.
    pub(crate) near: Option<Near>,
    /// The target's depth is reversed, nearer larger (a camera within the
    /// scene, and the interface drawn over its picture after it).
    pub(crate) reversed: bool,
    /// Moves every vertex (shadows, interpolated replays).
    pub(crate) remap: Option<Box<dyn Fn(&mut Vertex) + 'a>>,
    /// The area writes are shown in (a replay's display area; everywhere
    /// when none): elsewhere they only change the VRAM textures sample.
    pub(crate) shown: Option<VramRect>,
    /// Grow opaque world triangles by [`SEAL`] (the world map's terrain,
    /// whose blocks of different detail meet in T-junctions).
    pub(crate) seal: bool,
    /// The screen the game took ([`Capture`]): where it lies and the width
    /// of the sides its picture has (VRAM pixels).
    pub(crate) capture: Option<(VramRect, u32)>,
    pub(crate) capture_aliases: &'a [(VramRect, bool)],
    pub(crate) feedback: Option<VramRect>,
    feedback_copies: Vec<(VramRect, (u16, u16))>,
    feedback_started: bool,
    /// Row quads the interface may still take ([`push_rows`]); past them
    /// its polygons are drawn whole.
    rows_left: usize,
    /// The native VRAM as the frame's writes leave it, for the texels of
    /// faces the game paints in their buffer's clear colour
    /// ([`Builder::backdrop`]).
    pub(crate) vram: Option<&'a [u16]>,
    /// The fills that cleared screen buffers, newest last, in the colour
    /// VRAM stores.
    pub(crate) clears: Vec<(VramRect, u16)>,
    /// Where the draws of [`Builder::primitive`], fills and shown uploads go
    /// (a diorama's pane takes the interface drawn as the screen is).
    pub(crate) space: Space,
}

/// A builder's buffers kept between builds: a frame's vertices run to
/// megabytes, which allocated anew would fault their pages in each time.
#[derive(Default)]
pub(crate) struct Storage {
    vertices: Vec<Vertex>,
    commands: Vec<Command>,
    staging: Vec<u8>,
}

/// Row quads one build may draw the interface with (about 24 MB of vertices).
const ROW_BUDGET: usize = 65_536;

/// A primitive's vertex attributes ([`Builder::attributes`]): its vertices'
/// `mode` and `limits`, its class with off-centre projections as interface,
/// and what decides how its points are drawn.
pub(crate) struct Attributes {
    pub(crate) mode: [u32; 4],
    pub(crate) limits: u32,
    pub(crate) class: Class,
    projected: bool,
    original: bool,
    captured: bool,
    captured_from: Option<VramRect>,
    feedback: Option<[f32; 2]>,
    perspective: bool,
}

fn color(rgb: u32) -> [f32; 4] {
    [f32::from(rgb as u8) / 255.0, f32::from((rgb >> 8) as u8) / 255.0, f32::from((rgb >> 16) as u8) / 255.0, 1.0]
}

/// A colour (0xBBGGRR) as VRAM stores it: 5 bits a channel.
fn rgb15(rgb: u32) -> u16 {
    (((rgb >> 3) & 31) | ((rgb >> 11) & 31) << 5 | ((rgb >> 19) & 31) << 10) as u16
}

/// A fill's colour as VRAM keeps it, expanded as texels are (the full
/// range, or the console's 15-bit colours): texels painted to match a
/// fill, as a sky around a texture's clouds, then match it.
fn fill_color(rgb: u32, fifteen: bool) -> [f32; 4] {
    let channel = |shift: u32| {
        let c = (rgb >> shift >> 3) & 31;
        if fifteen { (c * 8 + c / 4) as f32 / 255.0 } else { c as f32 / 31.0 }
    };
    [channel(0), channel(8), channel(16), 1.0]
}

/// The texel at (u, v) of `p`'s texture in `vram`, as the GPU reads it
/// (the texture window applied; 4- and 8-bit texels through the CLUT).
fn texel(vram: &[u16], p: &Primitive, u: i16, v: i16) -> u16 {
    let window = p.environment.texture_window;
    let (mask_x, mask_y, offset_x, offset_y) = (window & 31, window >> 5 & 31, window >> 10 & 31, window >> 15 & 31);
    let u = (u as u32 & 255 & !(mask_x * 8)) | (offset_x & mask_x) * 8;
    let v = (v as u32 & 255 & !(mask_y * 8)) | (offset_y & mask_y) * 8;
    let mode = p.environment.draw_mode;
    let (page_x, page_y) = ((mode & 15) * 64, (mode >> 4 & 1) * 256);
    let at = |x: u32, y: u32| vram.get(((y & 511) * VRAM_WIDTH + (x & 1023)) as usize).copied().unwrap_or(0);
    let clut = |index: u32| at(u32::from(p.clut & 63) * 16 + index, u32::from(p.clut >> 6 & 511));
    match mode >> 7 & 3 {
        0 => clut(u32::from(at(page_x + u / 4, page_y + v)) >> (u % 4 * 4) & 15),
        1 => clut(u32::from(at(page_x + u / 2, page_y + v)) >> (u % 2 * 8) & 255),
        _ => at(page_x + u, page_y + v),
    }
}

/// The screen offset most of the scene is projected with. Projections
/// with another offset (the field compass, 3D interface models) are
/// interface, not scene.
pub fn scene_offset(ops: &[TraceOp]) -> Option<(i16, i16)> {
    // A frame has few offsets, mostly in runs: counted in a short list (the
    // first counted wins a tie).
    let mut counts: Vec<((i16, i16), usize)> = Vec::new();
    let mut last = 0;
    for op in ops {
        if let TraceOp::Draw(p) = op
            && p.class.is_scene()
            && let Some((offset, _)) = p.projection
        {
            if counts.get(last).is_none_or(|(known, _)| *known != offset) {
                last = counts.iter().position(|(known, _)| *known == offset).unwrap_or_else(|| {
                    counts.push((offset, 0));
                    counts.len() - 1
                });
            }
            counts[last].1 += 1;
        }
    }
    counts.iter().rev().max_by_key(|(_, n)| *n).map(|(offset, _)| *offset)
}

/// A primitive's class with off-centre projections as interface.
pub fn effective_class(p: &Primitive, scene: Option<(i16, i16)>) -> Class {
    match (p.class.is_scene(), p.projection, scene) {
        (true, Some((offset, _)), Some(scene)) if offset != scene => Class::Flat,
        _ => p.class,
    }
}

fn aux_class(class: Class) -> u32 {
    match class {
        Class::Flat => AUX_INTERFACE,
        Class::World => AUX_WORLD,
        Class::Model => AUX_MODEL,
        Class::Sprite => AUX_SPRITE,
    }
}

impl<'a> Builder<'a> {
    pub(crate) fn new(options: &'a DrawOptions, ops: &[TraceOp]) -> Self {
        let mut balance = 0i64;
        if options.culling.back_faces {
            for op in ops {
                if let TraceOp::Draw(p) = op
                    && p.class.is_scene()
                    && matches!(p.shape, Shape::Triangle | Shape::Quad)
                {
                    balance += i64::from(winding(p, [0, 1, 2]));
                }
            }
        }
        Self {
            options,
            vertices: Vec::new(),
            commands: Vec::new(),
            staging: Vec::new(),
            front: if balance < 0 { -1 } else { 1 },
            scene: scene_offset(ops),
            stats: DrawStats::default(),
            spill: 0,
            reveal: Reveal::Hidden,
            near: None,
            reversed: false,
            remap: None,
            rows_left: ROW_BUDGET,
            shown: None,
            seal: false,
            capture: None,
            capture_aliases: &[],
            feedback: None,
            feedback_copies: Vec::new(),
            feedback_started: false,
            vram: None,
            clears: Vec::new(),
            space: Space::Screen,
        }
    }

    /// [`Builder::new`] filling `storage`'s buffers ([`Builder::store`] gives
    /// them back).
    pub(crate) fn reusing(options: &'a DrawOptions, ops: &[TraceOp], storage: &mut Storage) -> Self {
        let mut builder = Self::new(options, ops);
        builder.vertices = std::mem::take(&mut storage.vertices);
        builder.commands = std::mem::take(&mut storage.commands);
        builder.staging = std::mem::take(&mut storage.staging);
        builder.vertices.clear();
        builder.commands.clear();
        builder.staging.clear();
        builder
    }

    /// Keeps the buffers for the next build.
    pub(crate) fn store(self, storage: &mut Storage) {
        storage.vertices = self.vertices;
        storage.commands = self.commands;
        storage.staging = self.staging;
    }

    fn push_draw(&mut self, key: PipelineKey, scissor: Option<Scissor>, start: u32) {
        self.push_range(key, scissor, start..self.vertices.len() as u32);
    }

    fn rect_vertices(&mut self, rect: VramRect, rgb: u32, upload: bool) {
        let (mut x0, y0) = (f32::from(rect.x), f32::from(rect.y));
        let (mut x1, y1) = (x0 + f32::from(rect.width), y0 + f32::from(rect.height));
        // Wider framing: a fill across the screen (the frame's clear) also
        // clears the sides.
        if !upload && rect.width >= 256 {
            x0 -= self.spill as f32;
            x1 += self.spill as f32;
        }
        let color = fill_color(rgb, self.options.fidelity.color_15bit);
        let vertex = |x, y| Vertex {
            position: [x, y],
            depth: 0.0,
            w: 1.0,
            color,
            uv: [0.0, 0.0],
            mode: [0, 0, 0, u32::from(upload)],
            limits: 0,
        };
        for (x, y) in [(x0, y0), (x1, y0), (x0, y1), (x1, y0), (x1, y1), (x0, y1)] {
            self.vertices.push(vertex(x, y));
        }
    }

    pub(crate) fn fill(&mut self, rect: VramRect, rgb: u32) {
        let start = self.vertices.len() as u32;
        self.rect_vertices(rect, rgb, false);
        let key = PipelineKey { fragment: Fragment::Rect, blend: 0, depth: DepthMode::Clear, space: self.space };
        self.push_draw(key, None, start);
        if rect.width >= 128 && rect.height >= 128 {
            self.clears.push((rect, rgb15(rgb)));
        }
    }

    /// Whether `p` is sky the game painted: a textured face of the scene
    /// whose texture fades into the colour of the fill that cleared its
    /// buffer, at each of its corners and along its outline but for one
    /// texel in ten (Shevat's sea of clouds below its towers, the sky
    /// around each cloud; not a floor whose texture merely shares the
    /// clear's colour, as the desert's sand does). The effects leave it as
    /// they leave that fill (no depth: no light, shade or haze), which it
    /// meets without an edge, as on the console.
    fn backdrop(&self, p: &Primitive) -> bool {
        let Some(vram) = self.vram else { return false };
        if !p.textured || p.semi || p.shape == Shape::Rectangle || !matches!(p.class, Class::World | Class::Model) {
            return false;
        }
        let [x, y, ..] = p.environment.area;
        let Some(&(_, sky)) = self.clears.iter().rev().find(|(rect, _)| contains(rect, x, y)) else { return false };
        let corners = &p.vertices[..p.vertex_count()];
        if sky == 0 || !corners.iter().all(|v| texel(vram, p, v.u, v.v) == sky) {
            return false;
        }
        // Round its outline in the texture (a quad's corners go 0, 1, 3, 2).
        let outline: &[usize] = if corners.len() == 4 { &[0, 1, 3, 2] } else { &[0, 1, 2] };
        let (mut same, mut all) = (0, 0);
        for (k, &from) in outline.iter().enumerate() {
            let (a, b) = (&corners[from], &corners[outline[(k + 1) % outline.len()]]);
            let (du, dv) = (i32::from(b.u) - i32::from(a.u), i32::from(b.v) - i32::from(a.v));
            let steps = du.abs().max(dv.abs()).max(1);
            for t in 0..steps {
                let (u, v) = (i32::from(a.u) + du * t / steps, i32::from(a.v) + dv * t / steps);
                same += u32::from(texel(vram, p, u as i16, v as i16) == sky);
                all += 1;
            }
        }
        same * 10 >= all * 9
    }

    pub(crate) fn upload(&mut self, rect: VramRect, pixels: &[u16], show: bool) {
        let row = u32::from(rect.width) * 2;
        let padded = row.next_multiple_of(wgpu::COPY_BYTES_PER_ROW_ALIGNMENT);
        let offset = self.staging.len() as u64;
        for line in pixels.chunks(usize::from(rect.width)) {
            self.staging.extend_from_slice(bytemuck::cast_slice(line));
            self.staging.resize(self.staging.len() + (padded - row) as usize, 0);
        }
        self.commands.push(Command::Upload { offset, rect });
        if show {
            let start = self.vertices.len() as u32;
            self.rect_vertices(rect, 0, true);
            let key = PipelineKey { fragment: Fragment::Rect, blend: 0, depth: DepthMode::Clear, space: self.space };
            self.push_draw(key, None, start);
        }
    }

    pub(crate) fn copy(&mut self, source: (u16, u16), destination: (u16, u16), width: u16, height: u16) {
        let from = VramRect { x: source.0, y: source.1, width, height };
        let local = self.feedback.filter(|area| overlap(area, &from) == u32::from(width) * u32::from(height));
        if local.is_some() {
            self.start_feedback();
            self.feedback_copies.push((VramRect { x: destination.0, y: destination.1, width, height }, source));
        }
        self.commands.push(Command::Copy { source, destination, width, height,
            local: local.map(|area| if width == area.width { self.spill } else { 0 }) });
    }

    fn start_feedback(&mut self) {
        if !self.feedback_started {
            self.commands.push(Command::Snapshot);
            self.feedback_started = true;
        }
    }

    fn feedback_source(&self, p: &Primitive) -> Option<[f32; 2]> {
        let area = self.feedback?;
        for &(rect, source) in self.feedback_copies.iter().rev() {
            if samples(p, &rect) {
                return Some([f32::from(source.0) - f32::from(rect.x), f32::from(source.1) - f32::from(rect.y)]);
            }
        }
        samples(p, &area).then_some([0.0, 0.0])
    }

    /// One traced primitive as triangles under its pipelines.
    pub(crate) fn primitive(&mut self, p: &Primitive) {
        // Viewport culling is the game's against its own screen, which a
        // camera within the scene does not look through.
        if p.revealed && !self.restores_sliver(p) && (self.reveal == Reveal::Hidden || (self.options.culling.viewport && self.near.is_none())) {
            return;
        }
        self.stats.primitives += 1;
        self.stats.revealed += u32::from(p.revealed);
        if self.feedback_source(p).is_some() {
            self.start_feedback();
        }
        let start = self.triangles(p);
        self.commands_for(p, start);
    }

    /// Terrain triangles that collapse at whole-pixel precision were
    /// discarded by NCLIP. Their sub-pixel area still fills seams at 1x.
    fn restores_sliver(&self, p: &Primitive) -> bool {
        if !self.seal || self.options.fidelity.vertex_snapping || self.options.culling.viewport
            || p.class != Class::World || p.shape != Shape::Triangle || winding(p, [0, 1, 2]) != 0
        {
            return false;
        }
        let [a, b, c, _] = &p.vertices;
        (b.fx - a.fx) * (c.fy - a.fy) - (b.fy - a.fy) * (c.fx - a.fx) > 0.0
    }

    /// What a primitive's vertices carry besides their points: its drawing
    /// mode, CLUT, texture window and flags, and its texel range.
    pub(crate) fn attributes(&self, p: &Primitive) -> Attributes {
        let fidelity = &self.options.fidelity;
        let count = p.vertex_count();
        let class = effective_class(p, self.scene);
        let projected = p.vertices[..count].iter().all(|v| v.z > 0.0);
        let captured = self.captures(p);
        let captured_from = self.capture_source(p);
        let feedback = self.feedback_source(p);
        // The interface is drawn as the original draws it, except what is
        // drawn of a screen the game took and the interface's own models,
        // projected with an offset of their own (the field compass, the
        // world map's, menu models): those as the scene is.
        let model = class == Class::Flat && p.class.is_scene();
        let original = class == Class::Flat && !model && !captured && feedback.is_none();
        let perspective = projected && !fidelity.affine_textures && p.shape != Shape::Rectangle && (class != Class::Flat || model);
        let linear = match class {
            Class::Flat if original => self.options.filtering.interface_linear,
            Class::Sprite => self.options.filtering.sprites_linear,
            _ => self.options.filtering.world_linear,
        };
        let blend = p.environment.draw_mode >> 5 & 3;
        let baked = p.gouraud || (p.textured && !p.raw && p.vertices[..count].iter().any(|v| v.color & 0xFF_FFFF != 0x80_8080));
        let kind = if self.backdrop(p) { AUX_BACKGROUND } else { aux_class(class) };
        let mut flags = (blend << BLEND_SHIFT) | (kind << CLASS_SHIFT);
        for (on, flag) in [
            (p.textured, FLAG_TEXTURED),
            (p.raw, FLAG_RAW),
            (p.dither, FLAG_DITHER),
            (p.semi, FLAG_SEMI),
            (p.environment.mask & 1 != 0, FLAG_MASK_SET),
            (linear && p.textured, FLAG_LINEAR),
            (p.shape == Shape::Rectangle, FLAG_RECTANGLE),
            (baked, FLAG_BAKED),
            (original, FLAG_ORIGINAL),
            (p.shape == Shape::Line, FLAG_LINE),
            (captured, FLAG_CAPTURE),
            (feedback.is_some(), FLAG_FEEDBACK),
        ] {
            if on {
                flags |= flag;
            }
        }
        // A polygon samples only its own texels (no bleeding into neighbours
        // on a texture page at higher scales or with filtering).
        let mut limits = 0;
        if p.textured && matches!(p.shape, Shape::Triangle | Shape::Quad) {
            let vertices = &p.vertices[..count];
            let range = |values: &mut dyn Iterator<Item = i16>| {
                values.fold((255, 0), |(lo, hi), x| (lo.min(x.clamp(0, 255) as u32), hi.max(x.clamp(0, 255) as u32)))
            };
            let (u0, u1) = range(&mut vertices.iter().map(|v| v.u));
            let (v0, v1) = range(&mut vertices.iter().map(|v| v.v));
            limits = u0 | v0 << 8 | u1 << 16 | v1 << 24;
            flags |= FLAG_CLAMP;
        }
        let mode = [p.environment.draw_mode, u32::from(p.clut), p.environment.texture_window, flags];
        Attributes { mode, limits, class, projected, original, captured, captured_from, feedback, perspective }
    }

    /// A primitive's triangles; returns the index of its first vertex.
    pub(crate) fn triangles(&mut self, p: &Primitive) -> u32 {
        let fidelity = &self.options.fidelity;
        let count = p.vertex_count();
        let Attributes { mode, limits, class, projected, original, captured, captured_from, feedback, perspective } = self.attributes(p);
        let vertex = |i: usize| {
            let v = &p.vertices[i];
            // Each vertex at the GTE's sub-pixel point when it has one, so
            // a point shared by neighbouring faces is the same in each; the
            // interface at the original's whole pixels.
            let snap = fidelity.vertex_snapping || v.z <= 0.0 || original || captured;
            let (x, y) = if snap { (v.x as f32, v.y as f32) } else { (v.fx, v.fy) };
            let mut out = Vertex {
                position: [x + 0.5, y + 0.5],
                depth: if projected { v.z } else { 0.0 },
                w: if perspective { v.z / 1024.0 } else { 1.0 },
                color: color(v.color),
                uv: [f32::from(v.u), f32::from(v.v)],
                mode,
                limits,
            };
            if let Some(remap) = &self.remap {
                remap(&mut out);
            }
            if let (Some(from), Some((to, _))) = (captured_from, self.capture) {
                out.uv[0] += f32::from(to.x) - f32::from(from.x);
                out.uv[1] += f32::from(to.y) - f32::from(from.y);
            }
            if let Some(offset) = feedback {
                out.uv[0] += (p.environment.draw_mode & 15) as f32 * 64.0 + offset[0];
                out.uv[1] += ((p.environment.draw_mode >> 4) & 1) as f32 * 256.0 + offset[1];
            }
            out
        };
        let start = self.vertices.len() as u32;
        let widened = self.widens(p);
        // Each point once (a quad's triangles share two).
        let corners: [Vertex; 4] = std::array::from_fn(|i| if i < count { vertex(i) } else { Vertex::default() });
        // A camera within the scene: its scene, seen from anywhere, at any size.
        let near = self.near.filter(|_| class.is_scene());
        match p.shape {
            Shape::Triangle | Shape::Quad => {
                let rows = original && self.remap.is_none() && self.rows_left > 0 && needs_rows(p) && !widened;
                let [left, top, right, bottom] = p.environment.area.map(i32::from);
                // Reaching behind that camera's near plane, clipped there.
                let clipped = near.filter(|near| p.vertices[..count].iter().any(|v| v.view.is_some_and(|[_, _, z]| z < near.distance)));
                for triangle in [[0, 1, 2], [1, 2, 3]].iter().take(if p.shape == Shape::Quad { 2 } else { 1 }) {
                    if let Some(near) = clipped {
                        let ((ox, oy), _) = p.projection.unwrap_or_default();
                        let center = (f32::from(ox) + f32::from(p.environment.offset.0), f32::from(oy) + f32::from(p.environment.offset.1));
                        let points = triangle.map(|i| (p.vertices[i].view.unwrap_or_default(), corners[i]));
                        self.stats.triangles += 1;
                        push_clipped(&mut self.vertices, points, near, center, perspective, fidelity.vertex_snapping);
                        continue;
                    }
                    // A face the game culled never met the GPU's limit.
                    if !p.revealed && near.is_none() && !fits(p, *triangle) {
                        self.stats.rejected_size += 1;
                        continue;
                    }
                    // A quad's points run in Z order (0 1 / 2 3), so its
                    // second triangle 1-2-3 winds the other way round from
                    // its first: the face's own winding is that of 1-3-2.
                    let facing = if *triangle == [1, 2, 3] { [1, 3, 2] } else { *triangle };
                    if self.options.culling.back_faces && near.is_none() && class.is_scene() && winding(p, facing) * self.front < 0 {
                        self.stats.culled_back_faces += 1;
                        continue;
                    }
                    self.stats.triangles += 1;
                    if rows {
                        let points = triangle.map(|i| (p.vertices[i].x, p.vertices[i].y));
                        let drawn = push_rows(&mut self.vertices, triangle.map(|i| corners[i]), points, (left, top, right, bottom));
                        self.rows_left = self.rows_left.saturating_sub(drawn);
                    } else if self.seal && class == Class::World && !p.semi && projected {
                        self.vertices.extend(grow(triangle.map(|i| corners[i]), SEAL));
                    } else {
                        self.vertices.extend(triangle.map(|i| corners[i]));
                    }
                }
            }
            Shape::Rectangle => {
                self.vertices.extend([0, 1, 2, 1, 3, 2].map(|i| corners[i]));
            }
            Shape::Line if class == Class::Flat && self.remap.is_none() => {
                let (a, b) = (&p.vertices[0], &p.vertices[1]);
                if (b.x - a.x).abs() < 1024 && (b.y - a.y).abs() < 512 {
                    push_line(&mut self.vertices, [corners[0], corners[1]], [(a.x, a.y), (b.x, b.y)]);
                }
            }
            Shape::Line => {
                let (a, b) = (corners[0], corners[1]);
                let (dx, dy) = (b.position[0] - a.position[0], b.position[1] - a.position[1]);
                // One pixel wide, end points included.
                let (ex, ey, nx, ny) = if dx.abs() >= dy.abs() {
                    (0.5 * dx.signum(), 0.0, 0.0, 0.5)
                } else {
                    (0.0, 0.5 * dy.signum(), 0.5, 0.0)
                };
                let at = |v: &Vertex, sx: f32, sy: f32| Vertex { position: [v.position[0] + sx, v.position[1] + sy], ..*v };
                let corners = [
                    at(&a, -ex - nx, -ey - ny),
                    at(&a, -ex + nx, -ey + ny),
                    at(&b, ex - nx, ey - ny),
                    at(&b, ex + nx, ey + ny),
                ];
                if dx.abs() < 1024.0 && dy.abs() < 512.0 {
                    for i in [0, 1, 2, 1, 3, 2] {
                        self.vertices.push(corners[i]);
                    }
                }
            }
        }
        if let Some((rect, side)) = self.capture.filter(|&(_, side)| captured && side > 0)
            && matches!(p.shape, Shape::Triangle | Shape::Quad)
        {
            let page_x = (p.environment.draw_mode & 15) as f32 * 64.0;
            push_capture_sides(&mut self.vertices, &corners[..count], page_x, (rect, side));
        }
        if let Some(rect) = self.feedback.filter(|_| feedback.is_some() && self.spill > 0)
            && matches!(p.shape, Shape::Triangle | Shape::Quad)
        {
            push_capture_sides(&mut self.vertices, &corners[..count], 0.0, (rect, self.spill));
        }
        if widened {
            // Its points at or past the drawing area's sides move out by the
            // sides' width.
            let [left, _, right, _] = p.environment.area.map(|a| f32::from(a));
            let spill = self.spill as f32;
            for v in &mut self.vertices[start as usize..] {
                if v.position[0] <= left + 0.5 {
                    v.position[0] -= spill;
                } else if v.position[0] >= right + 1.0 {
                    v.position[0] += spill;
                }
            }
        }
        start
    }

    fn commands_for(&mut self, p: &Primitive, start: u32) {
        let fidelity = &self.options.fidelity;
        let projected = p.vertices[..p.vertex_count()].iter().all(|v| v.z > 0.0);
        let blend = p.environment.draw_mode >> 5 & 3;
        let [left, top, right, bottom] = p.environment.area.map(i32::from);
        // The scene, and what the game culled beside its screen (a backdrop's
        // continuation too), may spill into the sides.
        let spill = if p.revealed || effective_class(p, self.scene).is_scene() || self.widens(p) || self.captures(p) || self.feedback_source(p).is_some() { self.spill as i32 } else { 0 };
        let rect = |left: i32, right: i32| Some(Scissor { x: left, y: top, width: (right + 1 - left).max(0), height: (bottom + 1 - top).max(0) });
        // A culled face beside the original screen shows only in the sides.
        let scissors = if p.revealed && self.reveal == Reveal::Sides && !self.restores_sliver(p) {
            vec![rect(left - spill, left - 1), rect(right + 1, right + spill)]
        } else {
            vec![rect(left - spill, right + spill)]
        };
        let depth = match (!fidelity.painter_ordering || self.near.is_some()) && projected {
            true if self.reversed => DepthMode::Nearer,
            true => DepthMode::Test,
            false => DepthMode::Over,
        };
        let end = self.vertices.len() as u32;
        // Semi-transparent pixels leave the depth and class under them; those
        // of the interface (windows) are then marked as interface, so no
        // effect touches them (a full-screen one, a fade, is a screen effect).
        // (A diorama's pane needs no classes.)
        let mark = p.semi && effective_class(p, self.scene) == Class::Flat && !self.covers_screen(p) && self.space == Space::Screen;
        let space = self.space;
        for scissor in scissors {
            if !p.semi {
                self.push_range(PipelineKey { fragment: Fragment::Opaque, blend: 0, depth, space }, scissor, start..end);
            } else if p.textured {
                // Opaque texels, then semi-transparent ones, over the same vertices.
                self.push_range(PipelineKey { fragment: Fragment::OpaqueTexels, blend: 0, depth, space }, scissor, start..end);
                self.push_range(PipelineKey { fragment: Fragment::Semi, blend, depth, space }, scissor, start..end);
            } else {
                self.push_range(PipelineKey { fragment: Fragment::Semi, blend, depth, space }, scissor, start..end);
            }
            if mark {
                self.push_range(PipelineKey { fragment: Fragment::Mark, blend: 0, depth, space }, scissor, start..end);
            }
        }
    }

    /// A draw of `vertices`, merged into the last when it continues it.
    fn push_range(&mut self, key: PipelineKey, scissor: Option<Scissor>, vertices: std::ops::Range<u32>) {
        if vertices.is_empty() {
            return;
        }
        if let Some(Command::Draw { key: last_key, scissor: last_scissor, vertices: last }) = self.commands.last_mut()
            && *last_key == key
            && *last_scissor == scissor
            && last.end == vertices.start
        {
            last.end = vertices.end;
            return;
        }
        self.commands.push(Command::Draw { key, scissor, vertices });
    }

    /// Whether `p` is textured from the screen the game took.
    fn captures(&self, p: &Primitive) -> bool {
        self.capture_source(p).is_some()
    }

    fn capture_source(&self, p: &Primitive) -> Option<VramRect> {
        let (rect, _) = self.capture?;
        std::iter::once(rect).chain(self.capture_aliases.iter().map(|(rect, _)| *rect)).find(|rect| samples(p, rect))
    }

    /// Wider framing: an untextured interface primitive over the whole
    /// screen (a fade, a flash) covers the sides too.
    fn widens(&self, p: &Primitive) -> bool {
        self.spill > 0 && !p.textured && p.shape != Shape::Line && effective_class(p, self.scene) == Class::Flat && self.covers_screen(p)
    }

    /// A full-screen translucent primitive (a fade) is a screen effect, not interface.
    fn covers_screen(&self, p: &Primitive) -> bool {
        let count = p.vertex_count();
        let [left, top, right, bottom] = p.environment.area.map(i32::from);
        let xs = p.vertices[..count].iter().map(|v| v.x);
        let ys = p.vertices[..count].iter().map(|v| v.y);
        let (min_x, max_x) = (xs.clone().min().unwrap_or(0), xs.max().unwrap_or(0));
        let (min_y, max_y) = (ys.clone().min().unwrap_or(0), ys.max().unwrap_or(0));
        let covered = (max_x.min(right + 1) - min_x.max(left)).max(0) * (max_y.min(bottom + 1) - min_y.max(top)).max(0);
        let area = (right - left + 1) * (bottom - top + 1);
        area > 0 && covered * 10 >= area * 9
    }

    pub(crate) fn op(&mut self, op: &TraceOp) {
        match op {
            TraceOp::Draw(p) => self.primitive(p),
            TraceOp::Fill { rect, color } => self.fill(*rect, *color),
            TraceOp::Copy { source, destination, width, height } => self.copy(*source, *destination, *width, *height),
            TraceOp::Write { rect, pixels, upload } => {
                let show = *upload && self.shown.is_none_or(|area| overlap(&area, rect) > 0);
                self.upload(*rect, pixels, show);
            }
            TraceOp::VBlank | TraceOp::Movie { .. } | TraceOp::MovieFramesLeft(_) | TraceOp::Eye(_) | TraceOp::Camera(_) | TraceOp::Placement(_) => {}
        }
    }
}

/// Whether `p` takes its texels from `rect` of VRAM as a 15-bit texture
/// (a screen the game took there: [`Capture`]).
pub(crate) fn samples(p: &Primitive, rect: &VramRect) -> bool {
    let mode = p.environment.draw_mode;
    if !p.textured || mode >> 7 & 3 != 2 || p.shape == Shape::Line {
        return false;
    }
    let page = (i32::from(mode as u16 & 15) * 64, i32::from(mode as u16 >> 4 & 1) * 256);
    let (x0, y0) = (i32::from(rect.x), i32::from(rect.y));
    let (x1, y1) = (x0 + i32::from(rect.width), y0 + i32::from(rect.height));
    if p.vertices[..p.vertex_count()].iter().all(|v| {
        let (x, y) = (page.0 + i32::from(v.u), page.1 + i32::from(v.v));
        x >= x0 && x <= x1 && y >= y0 && y <= y1
    }) {
        return true;
    }
    // The world map fades a saved 216-line screen with 239-line quads.
    // Their excess is outside the draw area and never samples VRAM. Test
    // the clipped polygon, so those quads still use the presented capture.
    let count = p.vertex_count();
    let order = if count == 4 { &[0, 1, 3, 2][..] } else { &[0, 1, 2][..] };
    let mut polygon: Vec<[f32; 4]> = order.iter().map(|&i| {
        let v = &p.vertices[i];
        [v.x as f32, v.y as f32, (page.0 + i32::from(v.u)) as f32, (page.1 + i32::from(v.v)) as f32]
    }).collect();
    let [left, top, right, bottom] = p.environment.area.map(f32::from);
    for (axis, edge, greater) in [(0, left, true), (0, right + 1.0, false), (1, top, true), (1, bottom + 1.0, false)] {
        let mut clipped = Vec::new();
        let Some(mut a) = polygon.last().copied() else { return false };
        let inside = |v: [f32; 4]| if greater { v[axis] >= edge } else { v[axis] <= edge };
        for &b in &polygon {
            if inside(a) != inside(b) {
                let t = (edge - a[axis]) / (b[axis] - a[axis]);
                clipped.push(std::array::from_fn(|k| a[k] + t * (b[k] - a[k])));
            }
            if inside(b) { clipped.push(b); }
            a = b;
        }
        polygon = clipped;
    }
    !polygon.is_empty() && polygon.iter().all(|p| {
        p[2] >= x0 as f32 - 0.001 && p[2] <= x1 as f32 + 0.001 && p[3] >= y0 as f32 - 0.001 && p[3] <= y1 as f32 + 0.001
    })
}

/// The sides of a captured screen's picture ([`Capture`]) beyond a polygon
/// textured from it, where an edge of the polygon lies on the screen's left
/// or right edge: a strip of `side` texels continuing its texture past that
/// edge, placed by the polygon's own mapping from texture to screen (the
/// screen's pieces carry the sides as they move, turn and grow).
fn push_capture_sides(vertices: &mut Vec<Vertex>, corners: &[Vertex], page_x: f32, (capture, side): (VramRect, u32)) {
    let [a, b, c] = [corners[0], corners[1], corners[2]];
    let (du1, dv1, du2, dv2) = (b.uv[0] - a.uv[0], b.uv[1] - a.uv[1], c.uv[0] - a.uv[0], c.uv[1] - a.uv[1]);
    let det = du1 * dv2 - du2 * dv1;
    if det.abs() < 1e-3 {
        return;
    }
    // The screen offset of one texel along u.
    let (dx1, dy1) = (b.position[0] - a.position[0], b.position[1] - a.position[1]);
    let (dx2, dy2) = (c.position[0] - a.position[0], c.position[1] - a.position[1]);
    let along = ((dx1 * dv2 - dx2 * dv1) / det, (dy1 * dv2 - dy2 * dv1) / det);
    let edges: &[(usize, usize)] = if corners.len() == 4 { &[(0, 1), (1, 3), (3, 2), (2, 0)] } else { &[(0, 1), (1, 2), (2, 0)] };
    let left = f32::from(capture.x) - page_x;
    for &(i, j) in edges {
        let (p, q) = (corners[i], corners[j]);
        for (edge, by) in [(left, -(side as f32)), (left + f32::from(capture.width), side as f32)] {
            if (p.uv[0] - edge).abs() < 0.01 && (q.uv[0] - edge).abs() < 0.01 {
                let out = |v: Vertex| Vertex { position: [v.position[0] + along.0 * by, v.position[1] + along.1 * by], uv: [v.uv[0] + by, v.uv[1]], ..v };
                vertices.extend([p, q, out(q), p, out(q), out(p)]);
            }
        }
    }
}

/// How far (native pixels) [`Builder::seal`] grows a triangle's edges.
const SEAL: f32 = 0.5;

/// `corners` moved out so each edge lies `by` native pixels further out
/// (mitred, at most four times that at a sharp corner), their attributes
/// from the triangle's own plane: depth linear in the screen, the rest
/// perspective-correct through `w`. Neighbours that do not quite meet
/// (blocks of terrain detail meeting in T-junctions) then overlap by a hair.
fn grow(corners: [Vertex; 3], by: f32) -> [Vertex; 3] {
    let p = corners.map(|v| glam::Vec2::from(v.position));
    let area = (p[1] - p[0]).perp_dot(p[2] - p[0]);
    if area.abs() < 1e-3 {
        return corners;
    }
    // Each edge's outward normal (away from the third corner).
    let normal = |a: usize, b: usize, c: usize| {
        let n = (p[b] - p[a]).perp().normalize_or_zero();
        if n.dot(p[c] - p[a]) > 0.0 { -n } else { n }
    };
    let normals = [normal(0, 1, 2), normal(1, 2, 0), normal(2, 0, 1)];
    let moved = std::array::from_fn::<_, 3, _>(|i| {
        let (before, after) = (normals[(i + 2) % 3], normals[i]);
        let miter = (before + after) * by / (1.0 + before.dot(after)).max(0.125);
        p[i] + miter.clamp_length_max(4.0 * by)
    });
    let inverse_w = corners.map(|v| 1.0 / v.w);
    moved.map(|q| {
        // Barycentric weights of q in the original triangle.
        let b1 = (q - p[0]).perp_dot(p[2] - p[0]) / (p[1] - p[0]).perp_dot(p[2] - p[0]);
        let b2 = (p[1] - p[0]).perp_dot(q - p[0]) / area;
        let b = [1.0 - b1 - b2, b1, b2];
        let per_w: f32 = (0..3).map(|i| b[i] * inverse_w[i]).sum();
        let w = 1.0 / per_w;
        let mix = |f: &dyn Fn(&Vertex) -> f32| (0..3).map(|i| b[i] * inverse_w[i] * f(&corners[i])).sum::<f32>() * w;
        Vertex {
            position: q.into(),
            depth: (0..3).map(|i| b[i] * corners[i].depth).sum(),
            w,
            color: std::array::from_fn(|k| mix(&|v| v.color[k])),
            uv: std::array::from_fn(|k| mix(&|v| v.uv[k])),
            ..corners[0]
        }
    })
}

/// Whether an interface polygon is drawn a row of native pixels at a time,
/// as the GPU fills it, to cover whole native pixels at any scale: one with a
/// slanted outer edge (a rectangle's halves meet where both planes agree).
fn needs_rows(p: &Primitive) -> bool {
    let v = &p.vertices;
    let slanted = |a: usize, b: usize| v[a].x != v[b].x && v[a].y != v[b].y;
    match p.shape {
        Shape::Triangle => slanted(0, 1) || slanted(1, 2) || slanted(2, 0),
        Shape::Quad => slanted(0, 1) || slanted(1, 3) || slanted(3, 2) || slanted(2, 0),
        _ => false,
    }
}

/// An interface triangle as the GPU fills it (`xem_gpu::TriangleRows`): a
/// quad over each row of native pixels it covers, with colour, texture
/// coordinates and depth from the triangle's plane through `corners`.
/// Returns the rows drawn.
fn push_rows(vertices: &mut Vec<Vertex>, corners: [Vertex; 3], points: [(i32, i32); 3], bounds: (i32, i32, i32, i32)) -> usize {
    let Some(rows) = xem_gpu::TriangleRows::new(points, bounds) else { return 0 };
    let [(ax, ay), (bx, by), (cx, cy)] = points.map(|(x, y)| (f64::from(x), f64::from(y)));
    let det = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    let [a, b, c] = corners;
    let at = |x: i32, y: i32| {
        let (px, py) = (f64::from(x), f64::from(y));
        let s = ((px - ax) * (cy - ay) - (cx - ax) * (py - ay)) / det;
        let t = ((bx - ax) * (py - ay) - (px - ax) * (by - ay)) / det;
        let mix = |pa: f32, pb: f32, pc: f32| (f64::from(pa) + (f64::from(pb) - f64::from(pa)) * s + (f64::from(pc) - f64::from(pa)) * t) as f32;
        Vertex {
            position: [x as f32 + 0.5, y as f32 + 0.5],
            depth: mix(a.depth, b.depth, c.depth),
            color: std::array::from_fn(|k| mix(a.color[k], b.color[k], c.color[k])),
            uv: std::array::from_fn(|k| mix(a.uv[k], b.uv[k], c.uv[k])),
            ..a
        }
    };
    let mut drawn = 0;
    for (y, first, last) in rows {
        if first <= last {
            let quad = [at(first, y), at(last + 1, y), at(first, y + 1), at(last + 1, y + 1)];
            vertices.extend([0, 1, 2, 1, 3, 2].map(|i| quad[i]));
            drawn += 1;
        }
    }
    drawn
}

/// An interface line as the GPU draws it (`xem_gpu`: a pixel per step
/// along the longer axis, the other coordinate rounded to the nearest pixel,
/// halves up): a quad over each run of pixels in a row (a column when steep),
/// covering whole native pixels like a polygon, its colour along the line.
fn push_line(vertices: &mut Vec<Vertex>, [a, b]: [Vertex; 2], [(x0, y0), (x1, y1)]: [(i32, i32); 2]) {
    let (dx, dy) = (x1 - x0, y1 - y0);
    let steps = dx.abs().max(dy.abs());
    let along = |from: i32, delta: i32, i: i32| if steps == 0 { from } else { from + (2 * delta * i + steps).div_euclid(2 * steps) };
    let steep = dy.abs() > dx.abs();
    // The colour at step i (the GPU's, before it rounds down).
    let color = |i: i32| -> [f32; 4] {
        let t = if steps == 0 { 0.0 } else { f64::from(i) / f64::from(steps) };
        std::array::from_fn(|k| (f64::from(a.color[k]) + (f64::from(b.color[k]) - f64::from(a.color[k])) * t) as f32)
    };
    let corner = |x: i32, y: i32, i: i32| Vertex {
        position: [x as f32 + 0.5, y as f32 + 0.5],
        color: color(i),
        // A polygon's whole pixels, not a line's centres (draw.wgsl).
        mode: [a.mode[0], a.mode[1], a.mode[2], a.mode[3] & !FLAG_LINE],
        ..a
    };
    let mut i = 0;
    while i <= steps {
        let (x, y) = (along(x0, dx, i), along(y0, dy, i));
        // The run: the next steps sharing this row (column).
        let mut j = i;
        while j < steps && if steep { along(x0, dx, j + 1) == x } else { along(y0, dy, j + 1) == y } {
            j += 1;
        }
        // Its pixels' span along the longer axis, and the steps at its ends
        // (one past the last: the colour keeps its slope to the far edge).
        let (from, to) = if steep { (y, along(y0, dy, j)) } else { (x, along(x0, dx, j)) };
        let (low, high) = (from.min(to), from.max(to) + 1);
        let sign = if steep { dy.signum() } else { dx.signum() };
        let start = if steep { y0 } else { x0 };
        let step_at = |at: i32| (at - start) * sign;
        let quad = if steep {
            [corner(x, low, step_at(low)), corner(x + 1, low, step_at(low)), corner(x, high, step_at(high)), corner(x + 1, high, step_at(high))]
        } else {
            [corner(low, y, step_at(low)), corner(high, y, step_at(high)), corner(low, y + 1, step_at(low)), corner(high, y + 1, step_at(high))]
        };
        vertices.extend([0, 1, 2, 1, 3, 2].map(|k| quad[k]));
        i = j + 1;
    }
}

/// A triangle of a camera within the scene ([`Near`]: its points in the
/// camera's space, each with the vertex it was built as) clipped to what
/// lies beyond the near plane, as triangles: a point on an edge takes the
/// colour and texture coordinates of its place along it, and each point is
/// projected about `center` as the near plane's camera projects (sub-pixel
/// unless `snap`).
fn push_clipped(vertices: &mut Vec<Vertex>, corners: [([f32; 3], Vertex); 3], near: Near, center: (f32, f32), perspective: bool, snap: bool) {
    let mut polygon = [([0.0f32; 3], Vertex::default()); 4];
    let mut n = 0;
    for i in 0..3 {
        let ((a, va), (b, vb)) = (corners[i], corners[(i + 1) % 3]);
        if a[2] >= near.distance {
            polygon[n] = (a, va);
            n += 1;
        }
        if (a[2] >= near.distance) != (b[2] >= near.distance) {
            let t = (near.distance - a[2]) / (b[2] - a[2]);
            let lerp = |x: f32, y: f32| x + (y - x) * t;
            let point = std::array::from_fn(|k| lerp(a[k], b[k]));
            let vertex = Vertex { color: std::array::from_fn(|k| lerp(va.color[k], vb.color[k])), uv: std::array::from_fn(|k| lerp(va.uv[k], vb.uv[k])), ..va };
            polygon[n] = (point, vertex);
            n += 1;
        }
    }
    let project = |(point, vertex): ([f32; 3], Vertex)| {
        let z = point[2].max(near.distance);
        let (x, y) = (center.0 + near.shift[0] + near.h[0] * point[0] / z, center.1 + near.shift[1] + near.h[1] * point[1] / z);
        let (x, y) = if snap { (x.floor(), y.floor()) } else { (x, y) };
        Vertex { position: [x + 0.5, y + 0.5], depth: z, w: if perspective { z / 1024.0 } else { 1.0 }, ..vertex }
    };
    for k in 1..n.saturating_sub(1) {
        vertices.extend([polygon[0], polygon[k], polygon[k + 1]].map(project));
    }
}

/// Whether a triangle is inside the GPU's size limits (1023x511).
fn fits(p: &Primitive, triangle: [usize; 3]) -> bool {
    let xs = triangle.map(|i| p.vertices[i].x);
    let ys = triangle.map(|i| p.vertices[i].y);
    let span = |v: [i32; 3]| v.iter().max().unwrap() - v.iter().min().unwrap();
    span(xs) < 1024 && span(ys) < 512
}

/// The sign of a triangle's screen-space area.
fn winding(p: &Primitive, [a, b, c]: [usize; 3]) -> i32 {
    let (a, b, c) = (&p.vertices[a], &p.vertices[b], &p.vertices[c]);
    let cross = i64::from(b.x - a.x) * i64::from(c.y - a.y) - i64::from(b.y - a.y) * i64::from(c.x - a.x);
    cross.signum() as i32
}

/// A target the PS1 pipelines draw into: colour, auxiliary and depth views
/// covering `extent` VRAM pixels from `origin` at `scale`.
pub(crate) struct Target<'a> {
    pub color: &'a wgpu::Texture,
    pub aux: &'a wgpu::Texture,
    pub depth: &'a wgpu::TextureView,
    pub origin: (f32, f32),
    pub scale: u32,
}

/// Records `commands` into `encoder` over `target`.
#[allow(clippy::too_many_arguments)]
pub(crate) fn encode(
    device: &wgpu::Device,
    pipelines: &mut Pipelines,
    encoder: &mut wgpu::CommandEncoder,
    group: &wgpu::BindGroup,
    vertices: &wgpu::Buffer,
    staging: Option<(&wgpu::Buffer, &wgpu::Texture)>,
    copies: Copies,
    target: &Target,
    commands: &[Command],
) {
    let color_view = target.color.create_view(&Default::default());
    let aux_view = target.aux.create_view(&Default::default());
    let size = target.color.size();
    let mut index = 0;
    while index < commands.len() {
        match &commands[index] {
            Command::Upload { offset, rect } => {
                if let Some((staging, vram)) = staging {
                    upload(encoder, staging, vram, *offset, *rect);
                }
                index += 1;
            }
            Command::Snapshot => {
                if let Copies::From { feedback, feedback_aux, .. } = copies {
                    encoder.copy_texture_to_texture(target.color.as_image_copy(), feedback.as_image_copy(), target.color.size());
                    encoder.copy_texture_to_texture(target.aux.as_image_copy(), feedback_aux.as_image_copy(), target.aux.size());
                }
                index += 1;
            }
            Command::Copy { source, destination, width, height, local } => {
                match copies {
                    Copies::InPlace { scratch, capture } => {
                        copy_region(encoder, target, scratch, *source, *destination, (*width, *height), 0);
                        if capture.areas().any(|(_, screen)| screen) {
                            copy_capture(encoder, target, capture, *source, *destination, (*width, *height));
                        }
                    }
                    Copies::From { color, aux, scale, capture, earlier, scratch, .. } => {
                        if let Some(side) = local {
                            copy_region(encoder, target, scratch, *source, *destination, (*width, *height), *side);
                        } else {
                            copy_from(encoder, target, (color, aux, scale), *source, *destination, (*width, *height));
                        }
                        if let Some(capture) = capture {
                            let _ = std::iter::once(capture).chain(earlier)
                                .any(|capture| copy_capture(encoder, target, capture, *source, *destination, (*width, *height)));
                        }
                    }
                }
                index += 1;
            }
            Command::Draw { .. } => {
                let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                    label: Some("ps1 draw"),
                    color_attachments: &[
                        Some(wgpu::RenderPassColorAttachment {
                            view: &color_view,
                            depth_slice: None,
                            resolve_target: None,
                            ops: wgpu::Operations { load: wgpu::LoadOp::Load, store: wgpu::StoreOp::Store },
                        }),
                        Some(wgpu::RenderPassColorAttachment {
                            view: &aux_view,
                            depth_slice: None,
                            resolve_target: None,
                            ops: wgpu::Operations { load: wgpu::LoadOp::Load, store: wgpu::StoreOp::Store },
                        }),
                    ],
                    depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                        view: target.depth,
                        depth_ops: Some(wgpu::Operations { load: wgpu::LoadOp::Load, store: wgpu::StoreOp::Store }),
                        stencil_ops: None,
                    }),
                    ..Default::default()
                });
                pass.set_bind_group(0, group, &[]);
                pass.set_vertex_buffer(0, vertices.slice(..));
                while let Some(Command::Draw { key, scissor, vertices }) = commands.get(index) {
                    let pipeline = pipelines.get(device, *key);
                    pass.set_pipeline(pipeline);
                    let (x, y, w, h) = scissor_rect(scissor.as_ref(), target.origin, target.scale, size);
                    if w > 0 && h > 0 {
                        pass.set_scissor_rect(x, y, w, h);
                        pass.draw(vertices.clone(), 0..1);
                    }
                    index += 1;
                }
            }
        }
    }
}

/// A draw's scissor in the pixels of a target covering VRAM from `origin` at
/// `scale` (x, y, width, height; all of it without one).
pub(crate) fn scissor_rect(scissor: Option<&Scissor>, origin: (f32, f32), scale: u32, size: wgpu::Extent3d) -> (u32, u32, u32, u32) {
    match scissor {
        Some(s) => {
            let x0 = ((s.x as f32 - origin.0) * scale as f32).max(0.0) as u32;
            let y0 = ((s.y as f32 - origin.1) * scale as f32).max(0.0) as u32;
            let x1 = (((s.x + s.width) as f32 - origin.0) * scale as f32).max(0.0) as u32;
            let y1 = (((s.y + s.height) as f32 - origin.1) * scale as f32).max(0.0) as u32;
            let x0 = x0.min(size.width);
            let y0 = y0.min(size.height);
            (x0, y0, x1.min(size.width).saturating_sub(x0), y1.min(size.height).saturating_sub(y0))
        }
        None => (0, 0, size.width, size.height),
    }
}

/// A [`Command::Upload`]: staged pixels at `offset` into the native VRAM
/// copy at `rect`.
pub(crate) fn upload(encoder: &mut wgpu::CommandEncoder, staging: &wgpu::Buffer, vram: &wgpu::Texture, offset: u64, rect: VramRect) {
    let row = (u32::from(rect.width) * 2).next_multiple_of(wgpu::COPY_BYTES_PER_ROW_ALIGNMENT);
    encoder.copy_buffer_to_texture(
        wgpu::TexelCopyBufferInfo {
            buffer: staging,
            layout: wgpu::TexelCopyBufferLayout { offset, bytes_per_row: Some(row), rows_per_image: None },
        },
        wgpu::TexelCopyTextureInfo {
            texture: vram,
            mip_level: 0,
            origin: wgpu::Origin3d { x: rect.x.into(), y: rect.y.into(), z: 0 },
            aspect: wgpu::TextureAspect::All,
        },
        wgpu::Extent3d { width: rect.width.into(), height: rect.height.into(), depth_or_array_layers: 1 },
    );
}

/// How [`encode`] copies VRAM rectangles.
#[derive(Clone, Copy)]
pub(crate) enum Copies<'a> {
    /// Inside the target, through a scratch texture.
    InPlace { scratch: &'a wgpu::Texture, capture: &'a Capture },
    /// From the scaled VRAM picture (colour, auxiliary, scale) into the
    /// target; a copy of a screen the game took (the capture, or one taken
    /// before it), from its picture.
    From { color: &'a wgpu::Texture, aux: &'a wgpu::Texture, scale: u32, capture: Option<&'a Capture>, earlier: &'a [Capture], scratch: &'a wgpu::Texture, feedback: &'a wgpu::Texture, feedback_aux: &'a wgpu::Texture },
}

/// A copy of all of a captured screen ([`Capture`]) into a replay's target:
/// its picture's colour, the sides too (where the target has room). False
/// when the copy is not of that screen.
fn copy_capture(
    encoder: &mut wgpu::CommandEncoder,
    target: &Target,
    capture: &Capture,
    source: (u16, u16),
    destination: (u16, u16),
    (width, height): (u16, u16),
) -> bool {
    let s = target.scale;
    let Some((rect, _)) = capture.areas().find(|(r, screen)| {
        (r.width, r.height) == (width, height)
            && ((r.x, r.y) == source || (*screen && (r.x, r.y) == destination))
    }) else { return false };
    if capture.scale != s {
        return false;
    }
    let size = target.color.size();
    let held = capture.texture.size();
    // Where the picture's left edge (its left side's) lands in the target.
    let left = (f32::from(destination.0) - target.origin.0 - capture.side as f32) * s as f32;
    let dy = (f32::from(destination.1) - target.origin.1) * s as f32;
    if dy < 0.0 {
        return true;
    }
    let skip = (-left).max(0.0) as u32;
    let dx = left.max(0.0) as u32;
    let w = ((u32::from(rect.width) + 2 * capture.side) * s).min(held.width).saturating_sub(skip).min(size.width.saturating_sub(dx));
    let h = (u32::from(rect.height) * s).min(held.height).min(size.height.saturating_sub(dy as u32));
    if w == 0 || h == 0 {
        return true;
    }
    encoder.copy_texture_to_texture(
        wgpu::TexelCopyTextureInfo { texture: &capture.texture, mip_level: 0, origin: wgpu::Origin3d { x: skip, y: 0, z: 0 }, aspect: wgpu::TextureAspect::All },
        wgpu::TexelCopyTextureInfo { texture: target.color, mip_level: 0, origin: wgpu::Origin3d { x: dx, y: dy as u32, z: 0 }, aspect: wgpu::TextureAspect::All },
        wgpu::Extent3d { width: w, height: h, depth_or_array_layers: 1 },
    );
    true
}

/// A VRAM copy from the scaled picture into another target (a replay).
fn copy_from(
    encoder: &mut wgpu::CommandEncoder,
    target: &Target,
    (color, aux, scale): (&wgpu::Texture, &wgpu::Texture, u32),
    source: (u16, u16),
    destination: (u16, u16),
    (width, height): (u16, u16),
) {
    let s = target.scale;
    let size = target.color.size();
    let dx = (f32::from(destination.0) - target.origin.0) * s as f32;
    let dy = (f32::from(destination.1) - target.origin.1) * s as f32;
    if dx < 0.0 || dy < 0.0 || scale != s {
        return;
    }
    let (sx, sy, dx, dy) = (u32::from(source.0) * s, u32::from(source.1) * s, dx as u32, dy as u32);
    let picture = color.size();
    let w = (u32::from(width) * s).min(size.width.saturating_sub(dx)).min(picture.width.saturating_sub(sx));
    let h = (u32::from(height) * s).min(size.height.saturating_sub(dy)).min(picture.height.saturating_sub(sy));
    if w == 0 || h == 0 {
        return;
    }
    for (from, to) in [(color, target.color), (aux, target.aux)] {
        encoder.copy_texture_to_texture(
            wgpu::TexelCopyTextureInfo { texture: from, mip_level: 0, origin: wgpu::Origin3d { x: sx, y: sy, z: 0 }, aspect: wgpu::TextureAspect::All },
            wgpu::TexelCopyTextureInfo { texture: to, mip_level: 0, origin: wgpu::Origin3d { x: dx, y: dy, z: 0 }, aspect: wgpu::TextureAspect::All },
            wgpu::Extent3d { width: w, height: h, depth_or_array_layers: 1 },
        );
    }
}

/// A VRAM copy inside the scaled picture (colour and auxiliary), through
/// `scratch` since a texture cannot copy onto itself.
fn copy_region(
    encoder: &mut wgpu::CommandEncoder,
    target: &Target,
    scratch: &wgpu::Texture,
    source: (u16, u16),
    destination: (u16, u16),
    (width, height): (u16, u16),
    side: u32,
) {
    let s = target.scale;
    let size = target.color.size();
    let clip = |x: u16, y: u16| {
        let x = (f32::from(x) - target.origin.0 - side as f32) * s as f32;
        let y = (f32::from(y) - target.origin.1) * s as f32;
        (x, y)
    };
    let (sx, sy) = clip(source.0, source.1);
    let (dx, dy) = clip(destination.0, destination.1);
    if sx < 0.0 || sy < 0.0 || dx < 0.0 || dy < 0.0 {
        return;
    }
    let (sx, sy, dx, dy) = (sx as u32, sy as u32, dx as u32, dy as u32);
    let w = ((u32::from(width) + 2 * side) * s).min(size.width.saturating_sub(sx)).min(size.width.saturating_sub(dx));
    let h = (u32::from(height) * s).min(size.height.saturating_sub(sy)).min(size.height.saturating_sub(dy));
    let scratch_size = scratch.size();
    let (w, h) = (w.min(scratch_size.width), h.min(scratch_size.height / 2));
    if w == 0 || h == 0 {
        return;
    }
    // Scratch rows 0..h hold colour, h..2h the auxiliary picture.
    for (texture, row) in [(target.color, 0), (target.aux, h)] {
        let extent = wgpu::Extent3d { width: w, height: h, depth_or_array_layers: 1 };
        encoder.copy_texture_to_texture(
            wgpu::TexelCopyTextureInfo { texture, mip_level: 0, origin: wgpu::Origin3d { x: sx, y: sy, z: 0 }, aspect: wgpu::TextureAspect::All },
            wgpu::TexelCopyTextureInfo { texture: scratch, mip_level: 0, origin: wgpu::Origin3d { x: 0, y: row, z: 0 }, aspect: wgpu::TextureAspect::All },
            extent,
        );
        encoder.copy_texture_to_texture(
            wgpu::TexelCopyTextureInfo { texture: scratch, mip_level: 0, origin: wgpu::Origin3d { x: 0, y: row, z: 0 }, aspect: wgpu::TextureAspect::All },
            wgpu::TexelCopyTextureInfo { texture, mip_level: 0, origin: wgpu::Origin3d { x: dx, y: dy, z: 0 }, aspect: wgpu::TextureAspect::All },
            extent,
        );
    }
}

fn texture(device: &wgpu::Device, label: &str, size: (u32, u32), format: wgpu::TextureFormat, usage: wgpu::TextureUsages) -> wgpu::Texture {
    device.create_texture(&wgpu::TextureDescriptor {
        label: Some(label),
        size: wgpu::Extent3d { width: size.0.max(1), height: size.1.max(1), depth_or_array_layers: 1 },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format,
        usage,
        view_formats: &[],
    })
}

pub(crate) fn picture_usage() -> wgpu::TextureUsages {
    wgpu::TextureUsages::RENDER_ATTACHMENT
        | wgpu::TextureUsages::TEXTURE_BINDING
        | wgpu::TextureUsages::COPY_SRC
        | wgpu::TextureUsages::COPY_DST
}

/// The display area to present: VRAM position and size in VRAM pixels.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DisplayArea {
    pub x: u32,
    pub y: u32,
    pub width: u32,
    pub height: u32,
}

/// The scaled VRAM picture and the native VRAM copy.
pub struct Ps1Renderer {
    device: wgpu::Device,
    queue: wgpu::Queue,
    pub(crate) pipelines: Pipelines,
    scale: u32,
    pub(crate) vram: wgpu::Texture,
    pub(crate) color: wgpu::Texture,
    pub(crate) aux: wgpu::Texture,
    depth: wgpu::Texture,
    scratch: wgpu::Texture,
    uniform: wgpu::Buffer,
    group: wgpu::BindGroup,
    vertices: Stream,
    staging: Stream,
    options: DrawOptions,
    /// The scale applied to `options` (native resolution forces 1).
    requested_scale: u32,
    /// Whether the picture holds anything yet (a trace must start with all of VRAM).
    pub(crate) synced: bool,
    /// What was drawn into each screen buffer since it was last cleared.
    logs: Vec<FrameLog>,
    /// Where the field's controlled character stood and the camera the game
    /// drew with, as the port recorded them last ([`TraceOp::Eye`],
    /// [`TraceOp::Camera`]), each with the vertical blanks since: the next
    /// frame a buffer's log begins drawing is the one they were recorded
    /// for.
    eye: Option<(xem_gpu::Eye, u32)>,
    camera: Option<(xem_gpu::Camera, u32)>,
    /// The placements the port recorded building the frame drawn next
    /// ([`TraceOp::Placement`]: after the frame before was drawn), and, from
    /// that frame's first primitive on, the same shared with its log.
    placements: Vec<xem_gpu::Placement>,
    placed: std::sync::Arc<[xem_gpu::Placement]>,
    placing: bool,
    /// The lifts a look from within gave the frame it last looked at, for
    /// its next looks ([`frame::PlaneLayers`]).
    pub(crate) plane_layers: frame::PlaneLayers,
    /// The display's size in VRAM pixels (a screen buffer's), when known,
    /// and the last few areas displayed (the screen buffers).
    display: Option<(u16, u16)>,
    shown: Vec<VramRect>,
    /// The native VRAM as the trace's writes left it (the start of each
    /// buffer's log copies it).
    native: Vec<u16>,
    /// Seal the world's T-junction cracks ([`Builder::seal`]).
    pub(crate) seal: bool,
    stats: DrawStats,
    /// Frames logged so far ([`FrameLog::frames`]).
    logged: u64,
    /// The screen the game last took ([`Capture`]), the picture presented
    /// before the trace being drawn, and the draws so far.
    pub(crate) capture: Capture,
    /// Screens it took before, elsewhere in VRAM, while VRAM still holds
    /// them (newest first): a copy of one still brings its picture.
    pub(crate) earlier: Vec<Capture>,
    presented: Option<PresentedPicture>,
    draws: u64,
    storage: Storage,
}

/// The primitives of one screen buffer: since its last clear (a fill or a
/// copy over all of it) and the clear before.
#[derive(Clone, Debug, Default)]
pub struct FrameLog {
    pub rect: VramRect,
    /// Its primitives, and every VRAM write since (anywhere: textures the
    /// frame samples), with their pixels.
    pub ops: Vec<TraceOp>,
    pub previous: Vec<TraceOp>,
    /// The frame it holds, numbered across every buffer and every start of
    /// the trace (a state loaded, a rewind's step): which frame it is.
    pub frames: u64,
    /// The native VRAM when the buffer was cleared: a replay samples VRAM
    /// as it was then, changed by the writes in `ops` as it goes.
    pub start: std::sync::Arc<Vec<u16>>,
    /// Where the field's controlled character stood in this frame, when
    /// the port recorded it (a view looking around: [`TraceOp::Eye`]).
    pub eye: Option<xem_gpu::Eye>,
    /// The camera the game drew this frame with, when the port recorded it
    /// (a field, a battle or the world map: [`TraceOp::Camera`]).
    pub camera: Option<xem_gpu::Camera>,
    /// The transforms the game drew this frame's models and sprites with
    /// ([`TraceOp::Placement`]), in the order it drew them.
    pub placements: std::sync::Arc<[xem_gpu::Placement]>,
    /// Whether a primitive was drawn into it since it began: the port
    /// records a frame's place, camera and placements as it builds it,
    /// before its first primitive, so the records current then are its own
    /// (also for a buffer the game draws over instead of clearing it, whose
    /// log begins with the frame).
    drawn: bool,
}

/// Where the field's controlled character stood, the camera the game drew
/// with and its models' and sprites' placements, as the port recorded them
/// for a frame.
#[derive(Clone, Default)]
struct Records {
    eye: Option<xem_gpu::Eye>,
    camera: Option<xem_gpu::Camera>,
    placements: std::sync::Arc<[xem_gpu::Placement]>,
}

/// A log grows without a clear in modes that never clear nor draw over all
/// of their buffer; it is dropped past this length (the effects then see
/// no primitives).
const LOG_LIMIT: usize = 200_000;

/// The last of a trace's game frames (where each begins in `ops`) from
/// which `ops` leave none of `rect`'s pixels as they were, hiding what was
/// drawn there before: each covered by an opaque fill, copy, upload or
/// polygon (untextured, or textured scenery, whose textures have no holes)
/// drawn without the mask test. A semi-transparent one shows what lay
/// beneath.
fn covered_from(ops: &[TraceOp], frames: &[usize], rect: VramRect) -> Option<usize> {
    // What a frame covers is all the same in any order: the frames from
    // the last one back, until they cover it all.
    let mut coverage = Coverage::new(rect);
    let mut end = ops.len();
    for &start in frames.iter().rev() {
        if ops[start..end].iter().any(|op| coverage.add(op)) {
            return Some(start);
        }
        end = start;
    }
    None
}

/// The pixels of a rectangle opaque drawing covered ([`covered_from`]).
struct Coverage {
    rect: VramRect,
    stride: usize,
    bits: Vec<u64>,
    /// Pixels not yet covered: done as soon as there are none.
    left: usize,
}

impl Coverage {
    fn new(rect: VramRect) -> Self {
        let (width, height) = (usize::from(rect.width), usize::from(rect.height));
        let stride = width.div_ceil(64);
        Coverage { rect, stride, bits: vec![0; stride * height], left: width * height }
    }

    fn span(&mut self, y: i32, x0: i32, x1: i32) {
        let (width, height) = (i32::from(self.rect.width), i32::from(self.rect.height));
        let y = y - i32::from(self.rect.y);
        let (x0, x1) = ((x0 - i32::from(self.rect.x)).max(0) as usize, (x1 - i32::from(self.rect.x)).clamp(0, width) as usize);
        if y < 0 || y >= height || x0 >= x1 {
            return;
        }
        let row = &mut self.bits[y as usize * self.stride..(y as usize + 1) * self.stride];
        for (i, word) in row.iter_mut().enumerate().take(x1.div_ceil(64)).skip(x0 / 64) {
            let (lo, hi) = (i * 64, i * 64 + 64);
            let a = x0.max(lo) - lo;
            let b = x1.min(hi) - lo;
            let set = if b - a == 64 { u64::MAX } else { ((1u64 << (b - a)) - 1) << a };
            self.left -= (set & !*word).count_ones() as usize;
            *word |= set;
        }
    }

    /// Adds what `op` covers; returns whether all of the rectangle is
    /// covered (never an empty one).
    fn add(&mut self, op: &TraceOp) -> bool {
        if self.bits.is_empty() {
            return false;
        }
        let fill = match op {
            TraceOp::Fill { rect, .. } | TraceOp::Write { rect, upload: true, .. } => Some(*rect),
            TraceOp::Copy { destination, width, height, .. } => Some(VramRect { x: destination.0, y: destination.1, width: *width, height: *height }),
            _ => None,
        };
        if let Some(r) = fill {
            for y in i32::from(r.y)..i32::from(r.y) + i32::from(r.height) {
                self.span(y, i32::from(r.x), i32::from(r.x) + i32::from(r.width));
            }
            return self.left == 0;
        }
        let TraceOp::Draw(p) = op else { return false };
        let solid = !p.textured || matches!(p.class, Class::World | Class::Model);
        if p.semi || p.revealed || !solid || p.environment.mask & 2 != 0 || !matches!(p.shape, Shape::Triangle | Shape::Quad | Shape::Rectangle) {
            return false;
        }
        let rect = self.rect;
        let [area_left, top, right, bottom] = p.environment.area.map(i32::from);
        // Drawn elsewhere (the other buffer): nothing here.
        if right < i32::from(rect.x) || area_left >= i32::from(rect.x) + i32::from(rect.width) || bottom < i32::from(rect.y) || top >= i32::from(rect.y) + i32::from(rect.height) {
            return false;
        }
        let triangles: &[[usize; 3]] = if p.shape == Shape::Triangle { &[[0, 1, 2]] } else { &[[0, 1, 2], [1, 2, 3]] };
        for t in triangles {
            let v = t.map(|i| (p.vertices[i].x as f32, p.vertices[i].y as f32));
            let (y0, y1) = (v.iter().map(|v| v.1).fold(f32::MAX, f32::min), v.iter().map(|v| v.1).fold(f32::MIN, f32::max));
            for y in (y0.floor() as i32).max(top)..=(y1.ceil() as i32).min(bottom) {
                // The row's pixel centres inside the triangle.
                let c = y as f32 + 0.5;
                let (mut lo, mut hi) = (f32::MAX, f32::MIN);
                for (a, b) in [(v[0], v[1]), (v[1], v[2]), (v[2], v[0])] {
                    if (a.1 <= c && b.1 > c) || (b.1 <= c && a.1 > c) {
                        let x = a.0 + (c - a.1) * (b.0 - a.0) / (b.1 - a.1);
                        lo = lo.min(x);
                        hi = hi.max(x);
                    }
                }
                if lo <= hi {
                    let x0 = ((lo - 0.5).ceil() as i32).max(area_left);
                    let x1 = ((hi - 0.5).ceil() as i32).min(right + 1);
                    self.span(y, x0, x1);
                }
            }
        }
        self.left == 0
    }
}

/// Whether `op` clears most of `rect`: a fill or a copy over nine tenths
/// of it.
fn clears(op: &TraceOp, rect: VramRect) -> bool {
    let over = |r: VramRect| overlap(&r, &rect) * 10 >= u32::from(rect.width) * u32::from(rect.height) * 9;
    match op {
        TraceOp::Fill { rect: r, .. } => over(*r),
        TraceOp::Copy { destination, width, height, .. } => over(VramRect { x: destination.0, y: destination.1, width: *width, height: *height }),
        _ => false,
    }
}

fn contains(rect: &VramRect, x: u16, y: u16) -> bool {
    x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height
}

pub(crate) fn overlap(a: &VramRect, b: &VramRect) -> u32 {
    let w = (a.x + a.width).min(b.x + b.width).saturating_sub(a.x.max(b.x));
    let h = (a.y + a.height).min(b.y + b.height).saturating_sub(a.y.max(b.y));
    u32::from(w) * u32::from(h)
}

impl Ps1Renderer {
    /// The largest scale this device's textures allow.
    pub fn max_scale(device: &wgpu::Device) -> u32 {
        (device.limits().max_texture_dimension_2d / VRAM_WIDTH).clamp(1, 16)
    }

    pub fn new(device: &wgpu::Device, queue: &wgpu::Queue, scale: u32) -> Self {
        let pipelines = Pipelines::new(device);
        let vram = texture(device, "native vram", (VRAM_WIDTH, VRAM_HEIGHT), wgpu::TextureFormat::R16Uint,
            wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST);
        let uniform = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("ps1 pass"),
            size: size_of::<PassUniform>() as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        let capture = Capture::new(device, (1, 1));
        let group = bind_group(device, &pipelines.layout, &uniform, &vram, &capture, None);
        let scale = scale.clamp(1, Self::max_scale(device));
        let (color, aux, depth, scratch) = Self::pictures(device, scale);
        Self {
            device: device.clone(),
            queue: queue.clone(),
            pipelines,
            scale,
            vram,
            color,
            aux,
            depth,
            scratch,
            uniform,
            group,
            vertices: Stream::new(device, "ps1 vertices", wgpu::BufferUsages::VERTEX),
            staging: Stream::new(device, "ps1 uploads", wgpu::BufferUsages::COPY_SRC),
            options: DrawOptions::default(),
            requested_scale: scale,
            synced: false,
            logs: Vec::new(),
            eye: None,
            camera: None,
            placements: Vec::new(),
            placed: std::sync::Arc::default(),
            placing: false,
            plane_layers: frame::PlaneLayers::default(),
            display: None,
            shown: Vec::new(),
            native: vec![0; (VRAM_WIDTH * VRAM_HEIGHT) as usize],
            seal: false,
            stats: DrawStats::default(),
            logged: 0,
            capture,
            earlier: Vec::new(),
            presented: None,
            draws: 0,
            storage: Storage::default(),
        }
    }

    /// The picture last presented of the displayed buffer, for a screen
    /// the next trace takes ([`Capture`]).
    pub fn set_presented(&mut self, picture: Option<PresentedPicture>) {
        self.presented = picture;
    }

    /// Whether a VRAM rect covers (most of) an area the display showed.
    fn is_screen(&self, rect: &VramRect) -> bool {
        self.shown.iter().any(|area| overlap(area, rect) * 10 >= u32::from(area.width) * u32::from(area.height) * 9)
    }

    /// Notes the screens `ops` take into VRAM (the presented picture is kept
    /// for each) and what overwrites one; returns whether a kept capture's
    /// VRAM was written (its picture goes back into the scaled VRAM after).
    fn watch_captures(&mut self, encoder: &mut wgpu::CommandEncoder, ops: &[TraceOp]) -> bool {
        let mut written = false;
        for op in ops {
            // All of VRAM again (a state loaded, a rewind's step, a trace
            // started again): no screen taken before is kept.
            if let TraceOp::Write { rect, upload: true, .. } = op
                && u32::from(rect.width) == VRAM_WIDTH
                && u32::from(rect.height) == VRAM_HEIGHT
            {
                self.capture.rect = None;
                self.capture.aliases.clear();
                self.capture.source = None;
                self.earlier.clear();
                continue;
            }
            // Reusing a source is only an alias while that source remains
            // unchanged; mirrored writes also cover draws and copies.
            if let TraceOp::Write { rect, .. } | TraceOp::Fill { rect, .. } = op
                && self.capture.source.is_some_and(|source| overlap(&source, rect) > 0)
            {
                self.capture.source = None;
            }
            match op {
                TraceOp::Copy { source, destination, width, height } => {
                    let from = VramRect { x: source.0, y: source.1, width: *width, height: *height };
                    let to = VramRect { x: destination.0, y: destination.1, width: *width, height: *height };
                    if *width >= 256 && *height >= 192 && source != destination && self.is_screen(&from) {
                        if self.capture.rect.is_some() && self.capture.source == Some(from) {
                            let screen = self.is_screen(&to);
                            if !self.capture.areas().any(|(r, _)| r == to) {
                                self.capture.aliases.push((to, screen));
                            }
                            written = true;
                        } else {
                            written |= self.take_capture(encoder, to);
                            self.capture.source = Some(from);
                        }
                    } else {
                        self.invalidate_captures(to, false);
                    }
                }
                // A non-upload only mirrors the native pixels of a copy,
                // fill or drawing just traced.
                TraceOp::Write { rect, pixels, upload: true } => {
                    let marked = !pixels.is_empty() && pixels.iter().all(|p| p & 0x8000 != 0);
                    let inside = self.capture.areas().any(|(c, _)| overlap(&c, rect) == u32::from(rect.width) * u32::from(rect.height));
                    if marked && inside && self.draws - self.capture.taken <= 16 {
                        written = true;
                    } else if marked && rect.width >= 256 && rect.height >= 192 && !self.is_screen(rect) {
                        written |= self.take_capture(encoder, *rect);
                    } else {
                        self.invalidate_captures(*rect, true);
                    }
                }
                TraceOp::Fill { rect, .. } => self.invalidate_captures(*rect, false),
                _ => {}
            }
        }
        written
    }

    /// What overwrites VRAM lets go of the screens kept there, but for
    /// replays of a frame logged copying one (the field's saved screen,
    /// given back to textures while a buffer copied from it is shown).
    fn invalidate_captures(&mut self, rect: VramRect, partial_upload: bool) {
        let held = self.capture.rect;
        self.capture.invalidate(rect, partial_upload);
        if let Some(held) = held.filter(|held| self.capture.rect.is_none() && copied_from(&self.logs, *held)) {
            let generation = self.capture.generation + 1;
            let mut capture = std::mem::replace(&mut self.capture, Capture::new(&self.device, (1, 1)));
            self.capture.generation = generation;
            self.group = bind_group(&self.device, &self.pipelines.layout, &self.uniform, &self.vram, &self.capture, None);
            capture.rect = Some(held);
            capture.aliases.clear();
            capture.replayed = true;
            self.earlier.insert(0, capture);
        }
        for capture in &mut self.earlier {
            let held = capture.rect;
            capture.invalidate(rect, partial_upload);
            if let Some(held) = held.filter(|held| capture.rect.is_none() && copied_from(&self.logs, *held)) {
                capture.rect = Some(held);
                capture.aliases.clear();
                capture.replayed = true;
            }
        }
        self.earlier.retain(|capture| capture.rect.is_some());
    }

    /// Keeps the presented picture (its sides included) as the screen the
    /// game took into `rect`. The screen taken before stays while VRAM
    /// holds it elsewhere (the field's saved one, which its menu copies
    /// under it after taking the dimmed screen too).
    fn take_capture(&mut self, encoder: &mut wgpu::CommandEncoder, rect: VramRect) -> bool {
        self.earlier.retain(|kept| kept.rect.is_some_and(|kept| overlap(&kept, &rect) == 0));
        if self.capture.rect.is_some_and(|kept| overlap(&kept, &rect) == 0) {
            let generation = self.capture.generation + 1;
            let earlier = std::mem::replace(&mut self.capture, Capture::new(&self.device, (1, 1)));
            self.capture.generation = generation;
            self.group = bind_group(&self.device, &self.pipelines.layout, &self.uniform, &self.vram, &self.capture, None);
            self.earlier.insert(0, earlier);
            self.earlier.truncate(EARLIER_CAPTURES);
        }
        self.capture.rect = None;
        self.capture.aliases.clear();
        self.capture.source = None;
        let Some(picture) = self.presented.clone().filter(|p| p.scale == self.scale) else { return false };
        let size = picture.texture.size();
        let width = ((u32::from(rect.width) + 2 * picture.side) * picture.scale).min(size.width);
        let height = (u32::from(rect.height) * picture.scale).min(size.height);
        if width <= 2 * picture.side * picture.scale || height == 0 {
            return false;
        }
        let held = self.capture.texture.size();
        if held.width < width || held.height < height {
            let generation = self.capture.generation + 1;
            self.capture = Capture::new(&self.device, (width.max(held.width), height.max(held.height)));
            self.capture.generation = generation;
            self.group = bind_group(&self.device, &self.pipelines.layout, &self.uniform, &self.vram, &self.capture, None);
        }
        encoder.copy_texture_to_texture(
            picture.texture.as_image_copy(),
            self.capture.texture.as_image_copy(),
            wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
        );
        self.capture.rect = Some(rect);
        self.capture.screen = self.is_screen(&rect);
        self.capture.scale = picture.scale;
        self.capture.side = picture.side;
        self.capture.taken = self.draws;
        true
    }

    /// The kept capture over its VRAM in the scaled picture (after the
    /// trace's own copy or write of the native screen there), without its
    /// sides.
    fn place_capture(&self, encoder: &mut wgpu::CommandEncoder) {
        let Some(rect) = self.capture.rect.filter(|_| self.capture.scale == self.scale) else { return };
        let held = self.capture.texture.size();
        let left = self.capture.side * self.scale;
        let width = (u32::from(rect.width) * self.scale).min(held.width.saturating_sub(left));
        let height = (u32::from(rect.height) * self.scale).min(held.height);
        let origin = wgpu::Origin3d { x: u32::from(rect.x) * self.scale, y: u32::from(rect.y) * self.scale, z: 0 };
        let limit = self.color.size();
        if origin.x + width > limit.width || origin.y + height > limit.height {
            return;
        }
        encoder.copy_texture_to_texture(
            wgpu::TexelCopyTextureInfo { texture: &self.capture.texture, mip_level: 0, origin: wgpu::Origin3d { x: left, y: 0, z: 0 }, aspect: wgpu::TextureAspect::All },
            wgpu::TexelCopyTextureInfo { texture: &self.color, mip_level: 0, origin, aspect: wgpu::TextureAspect::All },
            wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
        );
    }

    /// The area the display shows (a screen buffer): a fill or copy of
    /// about its size clears a buffer, and a copy of one elsewhere takes
    /// the screen ([`Capture`]).
    pub fn set_display(&mut self, area: VramRect) {
        self.display = Some((area.width, area.height));
        // A destination first copied before the mode change may only now
        // become a display buffer (and must restore the picture's sides).
        let displayed = |rect: VramRect| overlap(&area, &rect) * 10 >= u32::from(area.width) * u32::from(area.height) * 9;
        for capture in std::iter::once(&mut self.capture).chain(&mut self.earlier) {
            capture.screen |= capture.rect.is_some_and(displayed);
            for (rect, screen) in &mut capture.aliases {
                *screen |= displayed(*rect);
            }
        }
        if self.shown.last() != Some(&area) {
            self.shown.retain(|a| *a != area);
            self.shown.push(area);
            if self.shown.len() > 4 {
                self.shown.remove(0);
            }
        }
    }

    /// Grow opaque world triangles so terrain blocks meeting in T-junctions
    /// show no cracks at higher scales (the world map).
    pub fn set_seal(&mut self, seal: bool) {
        self.seal = seal;
    }

    fn pictures(device: &wgpu::Device, scale: u32) -> (wgpu::Texture, wgpu::Texture, wgpu::Texture, wgpu::Texture) {
        let size = (VRAM_WIDTH * scale, VRAM_HEIGHT * scale);
        (
            texture(device, "vram picture", size, COLOR_FORMAT, picture_usage()),
            texture(device, "vram picture aux", size, AUX_FORMAT, picture_usage()),
            texture(device, "vram picture depth", size, DEPTH_FORMAT, wgpu::TextureUsages::RENDER_ATTACHMENT),
            texture(device, "vram copy scratch", size, COLOR_FORMAT, wgpu::TextureUsages::COPY_SRC | wgpu::TextureUsages::COPY_DST),
        )
    }

    pub fn scale(&self) -> u32 {
        self.scale
    }

    pub fn options(&self) -> &DrawOptions {
        &self.options
    }

    /// Changes the options; a new scale needs the trace's next full VRAM
    /// (call [`Ps1Renderer::needs_full_vram`] and restart the trace).
    pub fn set_options(&mut self, options: DrawOptions, scale: u32) {
        self.options = options;
        self.requested_scale = scale;
        let scale = if options.fidelity.native_resolution { 1 } else { scale.clamp(1, Self::max_scale(&self.device)) };
        if scale != self.scale {
            self.scale = scale;
            (self.color, self.aux, self.depth, self.scratch) = Self::pictures(&self.device, scale);
            self.synced = false;
        }
    }

    /// Whether the picture needs the whole VRAM again (start, or a new
    /// scale): the host restarts the device's trace.
    pub fn needs_full_vram(&self) -> bool {
        !self.synced
    }

    /// Draws `trace` into the picture. Data goes through the queue: call
    /// at most once per queue submission.
    pub fn draw(&mut self, encoder: &mut wgpu::CommandEncoder, trace: &Trace) {
        if let Some(TraceOp::Write { rect, upload: true, .. }) = trace.ops.first()
            && rect.width as u32 == VRAM_WIDTH
            && rect.height as u32 == VRAM_HEIGHT
        {
            self.synced = true;
        }
        if !self.synced {
            return;
        }
        self.draws += 1;
        let captured = self.watch_captures(encoder, &trace.ops);
        self.log(&trace.ops);
        // A screen kept only for replays goes once no frame logged copies it.
        if self.earlier.iter().any(|capture| capture.replayed) {
            let logs = &self.logs;
            self.earlier.retain(|capture| !capture.replayed || capture.rect.is_some_and(|rect| copied_from(logs, rect)));
        }
        let options = self.options;
        let mut builder = Builder::reusing(&options, &trace.ops, &mut self.storage);
        builder.seal = self.seal;
        // (Its sides have no place in VRAM.)
        builder.capture = self.capture.rect.map(|rect| (rect, 0));
        builder.capture_aliases = &self.capture.aliases;
        builder.vram = Some(&self.native);
        builder.clears = self.logs.iter().filter_map(|log| match log.ops.first() {
            Some(TraceOp::Fill { rect, color }) => Some((*rect, rgb15(*color))),
            _ => None,
        }).collect();
        for op in &trace.ops {
            builder.op(op);
        }
        let (capture_scale, capture_side, capture) = self.capture.uniform();
        let uniform = PassUniform {
            origin: [0.0, 0.0],
            extent: [VRAM_WIDTH as f32, VRAM_HEIGHT as f32],
            scale: self.scale as f32,
            options: pass_options(&options.fidelity),
            capture_scale,
            capture_side,
            capture,
            ..Zeroable::zeroed()
        };
        self.queue.write_buffer(&self.uniform, 0, bytemuck::bytes_of(&uniform));
        self.stats = builder.stats;
        self.vertices.write(&self.device, &self.queue, bytemuck::cast_slice(&builder.vertices));
        self.staging.write(&self.device, &self.queue, &builder.staging);
        let depth_view = self.depth.create_view(&Default::default());
        let target = Target { color: &self.color, aux: &self.aux, depth: &depth_view, origin: (0.0, 0.0), scale: self.scale };
        encode(
            &self.device,
            &mut self.pipelines,
            encoder,
            &self.group,
            self.vertices.buffer(),
            Some((self.staging.buffer(), &self.vram)),
            Copies::InPlace { scratch: &self.scratch, capture: &self.capture },
            &target,
            &builder.commands,
        );
        builder.store(&mut self.storage);
        if captured && !self.capture.screen {
            self.place_capture(encoder);
        }
    }

    fn log(&mut self, ops: &[TraceOp]) {
        // All of VRAM again (a trace started again: a new scale, the Original
        // renderer shown meanwhile) holding what the logs left: nothing was
        // drawn meanwhile (a paused game draws nothing new), and the frames
        // they replay are still the ones shown. Anything else (a state
        // loaded, a rewind's step) leaves no frame to replay: what follows
        // starts the logs again.
        let ops = match ops.first() {
            Some(TraceOp::Write { rect, pixels, upload: true })
                if u32::from(rect.width) == VRAM_WIDTH && u32::from(rect.height) == VRAM_HEIGHT =>
            {
                if pixels[..] != self.native[..] {
                    self.native.copy_from_slice(pixels);
                    self.logs.clear();
                }
                &ops[1..]
            }
            _ => ops,
        };
        // The game frames the trace holds begin at its start and after each
        // vertical blank: several when the host ran more than one before
        // presenting (fast-forward, catching up).
        let frame_starts: Vec<usize> = std::iter::once(0)
            .chain(ops.iter().enumerate().filter(|(_, op)| matches!(op, TraceOp::VBlank)).map(|(n, _)| n + 1))
            .collect();
        // A buffer the game draws over completely instead of clearing it
        // (a battle's stage, every frame) starts its log again with the last
        // of them from which the trace draws all over it, which hides
        // everything before (each buffer's, found when first needed).
        let (width, height) = self.display.unwrap_or((256, 192));
        let mut drawn_over: Vec<(VramRect, Option<usize>)> = Vec::new();
        // A shown buffer without a log (the trace started again, a state
        // loaded or a rewind's step) that the game never clears starts one
        // with the last frame from which the trace draws all over it, once
        // its clears are done below: with the VRAM as that frame began and
        // the records current at its first primitive there.
        let mut unlogged: Vec<(VramRect, usize, Option<std::sync::Arc<Vec<u16>>>, Option<Records>)> = self.shown.iter().copied()
            .filter(|area| !self.half_logged(area))
            .filter_map(|area| Some((area, covered_from(ops, &frame_starts, area)?, None, None)))
            .collect();
        for (n, op) in ops.iter().enumerate() {
            if n == 0 || matches!(ops[n - 1], TraceOp::VBlank) {
                let mut start = None;
                for index in 0..self.logs.len() {
                    let rect = self.logs[index].rect;
                    if self.logs[index].ops.len() <= 64 {
                        continue;
                    }
                    let from = match drawn_over.iter().find(|(r, _)| *r == rect) {
                        Some((_, from)) => *from,
                        None => {
                            // From where the display shows it now: a log
                            // begun for a mode's buffer before shows the next
                            // mode's a few rows lower (the kernel menu's at
                            // y 240, the arena's at 256), whose game never
                            // draws the rows between.
                            let (x, y) = self.shown.iter().find(|area| (area.width, area.height) == (width, height) && shown_by(&self.logs[index], area))
                                .map_or((rect.x, rect.y), |area| (area.x, area.y));
                            let shown = VramRect { x, y, width: rect.width.min(width), height: rect.height.min(height) };
                            // Unless a clear there starts it.
                            let from = covered_from(ops, &frame_starts, shown).filter(|&from| !ops[from..].iter().any(|op| clears(op, rect)));
                            drawn_over.push((rect, from));
                            from
                        }
                    };
                    if from == Some(n) {
                        let start = start.get_or_insert_with(|| std::sync::Arc::new(self.native.clone())).clone();
                        self.logged += 1;
                        let log = &mut self.logs[index];
                        log.previous = std::mem::take(&mut log.ops);
                        log.frames = self.logged;
                        log.start = start;
                        (log.eye, log.camera, log.placements, log.drawn) = (None, None, Default::default(), false);
                    }
                }
                for (_, from, taken, _) in &mut unlogged {
                    if *from == n {
                        *taken = Some(start.get_or_insert_with(|| std::sync::Arc::new(self.native.clone())).clone());
                    }
                }
            }
            match op {
                TraceOp::Eye(eye) => {
                    self.eye = Some((*eye, 0));
                    continue;
                }
                TraceOp::Camera(camera) => {
                    self.camera = Some((**camera, 0));
                    continue;
                }
                TraceOp::Placement(placement) => {
                    // The first after a primitive begins the next frame's.
                    if !self.placing {
                        self.placements.clear();
                        self.placing = true;
                    }
                    self.placements.push(*placement);
                    continue;
                }
                // Recorded each frame the game draws (every other blank in a
                // field); older, they are not this frame's.
                TraceOp::VBlank => {
                    self.eye = self.eye.and_then(|(eye, blanks)| (blanks < 4).then_some((eye, blanks + 1)));
                    self.camera = self.camera.and_then(|(camera, blanks)| (blanks < 4).then_some((camera, blanks + 1)));
                    continue;
                }
                _ => {}
            }
            if let TraceOp::Write { rect, pixels, upload } = op {
                // The native VRAM, as replays start from it.
                for (row, line) in pixels.chunks(usize::from(rect.width).max(1)).enumerate() {
                    let at = (usize::from(rect.y) + row) * VRAM_WIDTH as usize + usize::from(rect.x);
                    if let Some(native) = self.native.get_mut(at..at + line.len()) {
                        native.copy_from_slice(line);
                    }
                }
                if *upload && u32::from(rect.width) == VRAM_WIDTH && u32::from(rect.height) == VRAM_HEIGHT {
                    self.logs.clear();
                    continue;
                }
                // A replay samples VRAM as it was at each primitive: every
                // buffer's log keeps every write, in order.
                for log in &mut self.logs {
                    log.ops.push(op.clone());
                }
                continue;
            }
            let cleared = match op {
                TraceOp::Fill { rect, .. } => Some(*rect),
                TraceOp::Copy { source, destination, width, height } => {
                    let from = VramRect { x: source.0, y: source.1, width: *width, height: *height };
                    let to = VramRect { x: destination.0, y: destination.1, width: *width, height: *height };
                    // A screen taken elsewhere is not a buffer cleared.
                    let taken = self.is_screen(&from) && !self.is_screen(&to);
                    // Moving rows within a buffer is part of its current
                    // frame, not a new frame (the field's distortion shift).
                    let pixels = u32::from(*width) * u32::from(*height);
                    let internal = overlap(&from, &to) > 0
                        && self.logs.iter().any(|l| overlap(&l.rect, &from) == pixels && overlap(&l.rect, &to) == pixels);
                    (!taken && !internal).then_some(to)
                }
                _ => None,
            };
            if let Some(rect) = cleared.filter(|r| r.width >= 128 && r.height >= 128) {
                let area = u32::from(rect.width) * u32::from(rect.height);
                // A clear of a whole screen buffer (the display's size, or
                // about 256x192 before the display is known) starts that
                // buffer's log even inside an older, larger one (both buffers
                // cleared at once before, a mode change), which it replaces.
                let (width, height) = self.display.unwrap_or((256, 192));
                let buffer = u32::from(rect.width) * 10 >= u32::from(width) * 9 && u32::from(rect.height) * 10 >= u32::from(height) * 8;
                self.logged += 1;
                let frames = self.logged;
                if let Some(log) = self.logs.iter_mut().find(|l| {
                    let size = u32::from(l.rect.width) * u32::from(l.rect.height);
                    overlap(&l.rect, &rect) * 10 >= size.max(area) * 9
                }) {
                    log.previous = std::mem::replace(&mut log.ops, vec![op.clone()]);
                    log.rect = rect;
                    log.frames = frames;
                    log.start = std::sync::Arc::new(self.native.clone());
                    (log.eye, log.camera, log.placements, log.drawn) = (None, None, Default::default(), false);
                    continue;
                }
                if buffer || self.logs.iter().all(|l| overlap(&l.rect, &rect) == 0) {
                    let replaced = |l: &FrameLog| overlap(&l.rect, &rect) * 2 >= area.min(u32::from(l.rect.width) * u32::from(l.rect.height));
                    // A mode change moves the second buffer from y=256 to
                    // y=216. Its clear must not retire the still displayed
                    // first buffer merely because their last eight rows meet.
                    self.logs.retain(|l| !replaced(l));
                    let start = std::sync::Arc::new(self.native.clone());
                    self.logs.push(FrameLog { rect, ops: vec![op.clone()], frames, start, ..Default::default() });
                    if self.logs.len() > 6 {
                        self.logs.remove(0);
                    }
                    continue;
                }
            }
            let at = match op {
                TraceOp::Draw(p) => Some((p.environment.area[0], p.environment.area[1])),
                TraceOp::Fill { rect, .. } => Some((rect.x, rect.y)),
                TraceOp::Copy { destination, .. } => Some(*destination),
                _ => None,
            };
            // Saved framebuffer strips are used later in this frame even
            // though their destinations lie outside its drawing area.
            if let TraceOp::Copy { source, destination, .. } = op {
                for log in &mut self.logs {
                    if contains(&log.rect, source.0, source.1) && !contains(&log.rect, destination.0, destination.1) {
                        log.ops.push(op.clone());
                    }
                }
            }
            if let TraceOp::Draw(p) = op {
                // The frame's first primitive: its placements are all recorded.
                if self.placing {
                    self.placed = std::sync::Arc::from(self.placements.as_slice());
                    self.placing = false;
                }
                for (area, from, _, taken) in &mut unlogged {
                    if taken.is_none() && n >= *from && contains(area, p.environment.area[0], p.environment.area[1]) {
                        *taken = Some(self.frame_records());
                    }
                }
            }
            if let Some((x, y)) = at
                && let Some(index) = self.logs.iter().position(|l| (l.rect.x, l.rect.y) == (x, y))
                    .or_else(|| self.logs.iter().position(|l| contains(&l.rect, x, y)))
            {
                // Its frame's first primitive: the records current now are
                // the ones the port made building that frame.
                let records = (!self.logs[index].drawn && matches!(op, TraceOp::Draw(_))).then(|| self.frame_records());
                let log = &mut self.logs[index];
                if log.ops.len() >= LOG_LIMIT {
                    log.ops.clear();
                    log.previous.clear();
                }
                if let Some(records) = records {
                    (log.eye, log.camera, log.placements, log.drawn) = (records.eye, records.camera, records.placements, true);
                }
                log.ops.push(op.clone());
            }
        }
        for (area, from, start, records) in unlogged {
            let Some(start) = start.filter(|_| !self.half_logged(&area)) else { continue };
            // Its draws, fills and copies, and every write (as a log keeps
            // them), from its frame on.
            let ops = ops[from..].iter().filter(|op| match op {
                TraceOp::Write { .. } => true,
                TraceOp::Draw(p) => contains(&area, p.environment.area[0], p.environment.area[1]),
                TraceOp::Fill { rect, .. } => contains(&area, rect.x, rect.y),
                TraceOp::Copy { destination, .. } => contains(&area, destination.0, destination.1),
                _ => false,
            }).cloned().collect();
            self.logged += 1;
            let drawn = records.is_some();
            let Records { eye, camera, placements } = records.unwrap_or_default();
            self.logs.push(FrameLog { rect: area, ops, frames: self.logged, start, eye, camera, placements, drawn, ..Default::default() });
            if self.logs.len() > 6 {
                self.logs.remove(0);
            }
        }
    }

    /// Whether a log is of the buffer a display of `area` shows.
    fn half_logged(&self, area: &VramRect) -> bool {
        self.logs.iter().any(|l| shown_by(l, area))
    }

    /// The character's place, the camera and the placements a frame
    /// starting now was recorded with.
    fn frame_records(&self) -> Records {
        Records { eye: self.eye.map(|(eye, _)| eye), camera: self.camera.map(|(camera, _)| camera), placements: self.placed.clone() }
    }

    /// The logs' identities (where each buffer lies and how many frames it
    /// was cleared for), as [`FrameRenderer::identity`] gives the shown one's.
    pub fn log_identities(&self) -> impl Iterator<Item = ((u16, u16), u64)> + '_ {
        self.logs.iter().map(|log| ((log.rect.x, log.rect.y), log.frames))
    }

    /// The log of the screen buffer `area` shows (most of it inside the
    /// buffer's last clear).
    pub fn frame_log(&self, area: DisplayArea) -> Option<&FrameLog> {
        let rect = VramRect { x: area.x as u16, y: area.y as u16, width: area.width as u16, height: area.height as u16 };
        self.logs.iter().find(|l| shown_by(l, &rect))
    }

    /// What the last [`Ps1Renderer::draw`] drew and dropped.
    pub fn stats(&self) -> DrawStats {
        self.stats
    }

    /// The scaled picture of all of VRAM.
    pub fn picture(&self) -> (&wgpu::Texture, &wgpu::Texture) {
        (&self.color, &self.aux)
    }

    /// Copies `area` of the picture into `color` and `aux` (sized
    /// `area * scale`, formats [`COLOR_FORMAT`] and [`AUX_FORMAT`]).
    pub fn copy_display(&self, encoder: &mut wgpu::CommandEncoder, area: DisplayArea, color: &wgpu::Texture, aux: &wgpu::Texture) {
        let s = self.scale;
        let width = (area.width * s).min(VRAM_WIDTH * s - (area.x * s).min(VRAM_WIDTH * s)).min(color.width());
        let height = (area.height * s).min(VRAM_HEIGHT * s - (area.y * s).min(VRAM_HEIGHT * s)).min(color.height());
        if width == 0 || height == 0 {
            return;
        }
        for (source, destination) in [(&self.color, color), (&self.aux, aux)] {
            encoder.copy_texture_to_texture(
                wgpu::TexelCopyTextureInfo { texture: source, mip_level: 0, origin: wgpu::Origin3d { x: area.x * s, y: area.y * s, z: 0 }, aspect: wgpu::TextureAspect::All },
                destination.as_image_copy(),
                wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            );
        }
    }

}

/// A pass's option bits for `fidelity` (draw.wgsl's `Pass::options`; a
/// camera within the scene adds [`PASS_RECIPROCAL_DEPTH`]).
pub(crate) fn pass_options(fidelity: &Fidelity) -> u32 {
    u32::from(!fidelity.affine_textures)
        | u32::from(fidelity.color_15bit) << 1
        | u32::from(fidelity.dithering) << 2
        | u32::from(!fidelity.painter_ordering) << 3
}

pub(crate) fn bind_group(
    device: &wgpu::Device,
    layout: &wgpu::BindGroupLayout,
    uniform: &wgpu::Buffer,
    vram: &wgpu::Texture,
    capture: &Capture,
    feedback: Option<(&wgpu::Texture, &wgpu::Texture)>,
) -> wgpu::BindGroup {
    device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some("ps1 pass"),
        layout,
        entries: &[
            wgpu::BindGroupEntry { binding: 0, resource: uniform.as_entire_binding() },
            wgpu::BindGroupEntry { binding: 1, resource: wgpu::BindingResource::TextureView(&vram.create_view(&Default::default())) },
            wgpu::BindGroupEntry { binding: 2, resource: wgpu::BindingResource::TextureView(&capture.view) },
            wgpu::BindGroupEntry { binding: 3, resource: wgpu::BindingResource::Sampler(&capture.sampler) },
            wgpu::BindGroupEntry { binding: 4, resource: wgpu::BindingResource::TextureView(&feedback.map_or(&capture.texture, |f| f.0).create_view(&Default::default())) },
            wgpu::BindGroupEntry { binding: 5, resource: wgpu::BindingResource::TextureView(&feedback.map_or(&capture.texture, |f| f.1).create_view(&Default::default())) },
        ],
    })
}

/// Screens taken before the capture kept while VRAM holds them.
const EARLIER_CAPTURES: usize = 2;

/// Whether `log` is of the buffer a display of `area` shows: one holding
/// the display's corner and at least half of it (the field draws 224 rows
/// and shows 216), not another buffer it overlaps (a newly shown one whose
/// own log has not started yet would show that buffer's frame moved).
fn shown_by(log: &FrameLog, area: &VramRect) -> bool {
    contains(&log.rect, area.x, area.y) && overlap(&log.rect, area) * 2 >= u32::from(area.width) * u32::from(area.height)
}

/// Whether a frame logged copies all of `rect` (a screen kept there).
fn copied_from(logs: &[FrameLog], rect: VramRect) -> bool {
    logs.iter().any(|log| log.ops.iter().any(|op| matches!(op,
        TraceOp::Copy { source, width, height, .. } if (source.0, source.1, *width, *height) == (rect.x, rect.y, rect.width, rect.height))))
}

/// A screen the game took into VRAM (a copy of a screen buffer, or its
/// pixels written back with their semi-transparency bits set), kept as the
/// picture presented then (wider, pulled back, at the renderer's scale):
/// what samples that VRAM as a 15-bit texture shows it instead
/// (draw.wgsl), and a copy back to a screen buffer copies it.
pub(crate) struct Capture {
    pub(crate) texture: wgpu::Texture,
    view: wgpu::TextureView,
    sampler: wgpu::Sampler,
    /// Where it lies in VRAM, its picture's scale and the width of the
    /// picture's sides (VRAM pixels; it starts with the left one).
    pub(crate) rect: Option<VramRect>,
    pub(crate) aliases: Vec<(VramRect, bool)>,
    source: Option<VramRect>,
    pub(crate) scale: u32,
    pub(crate) side: u32,
    /// A buffer-to-buffer copy (pause/transition): its partial uploads are
    /// overlays, and a replay of the original copy uses this held picture.
    pub(crate) screen: bool,
    /// Its VRAM was overwritten: kept for replays of a frame logged copying
    /// it ([`Ps1Renderer::earlier`]).
    replayed: bool,
    /// The draw it was taken at.
    taken: u64,
    /// Counts new textures (bind groups made with an older one are stale).
    pub(crate) generation: u64,
}

impl Capture {
    fn new(device: &wgpu::Device, size: (u32, u32)) -> Self {
        let texture = texture(device, "screen capture", size, COLOR_FORMAT, wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST | wgpu::TextureUsages::COPY_SRC);
        let sampler = device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("screen capture"),
            mag_filter: wgpu::FilterMode::Linear,
            min_filter: wgpu::FilterMode::Linear,
            ..Default::default()
        });
        Capture { view: texture.create_view(&Default::default()), texture, sampler, rect: None, aliases: Vec::new(), source: None, scale: 1, side: 0, screen: false, replayed: false, taken: 0, generation: 0 }
    }

    pub(crate) fn areas(&self) -> impl Iterator<Item = (VramRect, bool)> + '_ {
        self.rect.map(|r| (r, self.screen)).into_iter().chain(self.aliases.iter().copied())
    }

    fn invalidate(&mut self, rect: VramRect, partial_upload: bool) {
        let overwritten = |r: VramRect, screen: bool| overlap(&r, &rect) > 0
            && (!partial_upload || !screen || overlap(&r, &rect) == u32::from(r.width) * u32::from(r.height));
        self.aliases.retain(|&(r, screen)| !overwritten(r, screen));
        if self.rect.is_some_and(|r| overwritten(r, self.screen)) {
            self.rect = self.aliases.pop().map(|(r, screen)| { self.screen = screen; r });
        }
    }

    /// The pass uniform's capture fields.
    pub(crate) fn uniform(&self) -> (f32, u32, [u32; 4]) {
        match self.rect {
            Some(r) => (self.scale as f32, self.side, [r.x.into(), r.y.into(), r.width.into(), r.height.into()]),
            None => (0.0, 0, [0; 4]),
        }
    }
}

/// The picture a host last presented of the displayed buffer: its texture,
/// the wider framing's sides (VRAM pixels) and its scale.
#[derive(Clone, Debug)]
pub struct PresentedPicture {
    pub texture: wgpu::Texture,
    pub side: u32,
    pub scale: u32,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_worldmap_fade_samples_only_the_visible_part_of_its_saved_screen() {
        let mut gpu = xem_gpu::Gpu::new();
        gpu.set_trace(true);
        let _ = gpu.take_trace();
        // The original fade uses 239-line quads over a 216-line capture.
        gpu.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 215 << 10 | 319, 0xE500_0000]);
        gpu.write_gp0_words(&[0x2D80_8080, 0, 0, 128, (0x11B << 16) | 128, 239 << 16, 239 << 8, (239 << 16) | 128, (239 << 8) | 128]);
        let trace = gpu.take_trace();
        let mut p = trace.ops.into_iter().find_map(|op| match op { TraceOp::Draw(p) => Some(p), _ => None }).unwrap();
        let capture = VramRect { x: 704, y: 256, width: 320, height: 216 };
        assert!(samples(&p, &capture), "the clipped excess does not sample uncaptured VRAM");
        p.environment.area[3] = 239;
        assert!(!samples(&p, &capture), "visible pixels beyond the capture must keep their original texture");
        p.environment.area[3] = 215;
        p.environment.draw_mode -= 1;
        assert!(!samples(&p, &capture), "neighbouring texture pages are not the captured screen");
    }

    #[test]
    fn a_face_painted_in_its_buffers_clear_colour_is_background() {
        // Shevat's sea of clouds: a 4-bit texture (page 640,0; CLUT 0,492)
        // whose corners hold the sky its buffer was cleared with.
        let sky = 0xF8_90_68;
        let width = VRAM_WIDTH as usize;
        let mut vram = vec![0u16; width * VRAM_HEIGHT as usize];
        vram[492 * width + 1] = rgb15(sky);
        vram[492 * width + 2] = 0x7FFF;
        for v in 0..16 {
            vram[v * width + 640..][..4].fill(0x1111);
        }
        vram[8 * width + 641] = 0x2222;
        let mut gpu = xem_gpu::Gpu::new();
        gpu.set_trace(true);
        let _ = gpu.take_trace();
        gpu.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 223 << 10 | 319, 0xE500_0000]);
        // A raw textured quad, its texels u 0-15, v 0-15.
        gpu.write_gp0_words(&[0x2D80_8080, 0, (492 << 6) << 16, 100, 10 << 16 | 15, 100 << 16, 15 << 8, 100 << 16 | 100, 15 << 8 | 15]);
        let trace = gpu.take_trace();
        let mut face = trace.ops.into_iter().find_map(|op| match op { TraceOp::Draw(p) => Some(p), _ => None }).unwrap();
        // (A model's face, as the port's projections make it.)
        face.class = Class::Model;
        let options = DrawOptions::default();
        let screen = VramRect { x: 0, y: 0, width: 320, height: 224 };
        let kind = |face: &Primitive, clear: u32, vram: &[u16]| {
            let mut builder = Builder::new(&options, &[]);
            builder.vram = Some(vram);
            builder.op(&TraceOp::Fill { rect: screen, color: clear });
            builder.primitive(face);
            builder.vertices.last().unwrap().mode[3] >> CLASS_SHIFT & 15
        };
        assert_eq!(kind(&face, sky, &vram), AUX_BACKGROUND, "the sky the game painted");
        assert_eq!(kind(&face, 0xF8_F8_F8, &vram), AUX_MODEL, "another clear colour");
        assert_eq!(kind(&face, 0, &vram), AUX_MODEL, "a black clear: transparent texels are not sky");
        let mut cloud = vram.clone();
        cloud[640] = 0x1112;
        assert_eq!(kind(&face, sky, &cloud), AUX_MODEL, "a cloud at a corner");
        // The clear's colour at the corners only, its top and bottom edges
        // between them of cloud: a texture that shares the colour.
        let mut edges = vram.clone();
        for v in [0, 15] {
            edges[v * width + 640..][..4].copy_from_slice(&[0x2221, 0x2222, 0x2222, 0x1222]);
        }
        assert_eq!(kind(&face, sky, &edges), AUX_MODEL, "an outline mostly of cloud");
        face.semi = true;
        assert_eq!(kind(&face, sky, &vram), AUX_MODEL, "translucent: the class beneath stays anyway");

        // The clear as VRAM keeps it, expanded as texels are (the full
        // range, or the console's 15-bit colours).
        for (fidelity, expected) in [(Fidelity::MODERN, [13.0 / 31.0, 18.0 / 31.0, 1.0, 1.0]), (Fidelity::ORIGINAL, [107.0 / 255.0, 148.0 / 255.0, 1.0, 1.0])] {
            let options = DrawOptions { fidelity, ..DrawOptions::default() };
            let mut builder = Builder::new(&options, &[]);
            builder.op(&TraceOp::Fill { rect: screen, color: sky });
            assert_eq!(builder.vertices[0].color, expected);
        }
    }

    #[test]
    fn a_triangle_reaching_behind_the_near_plane_is_cut_there() {
        let corner = |u: f32, red: f32| Vertex { uv: [u, 0.0], color: [red, 0.0, 0.0, 1.0], ..Vertex::default() };
        // One point 90 units behind the near plane (10), two 90 ahead of it.
        let corners = [([0.0, 100.0, -80.0], corner(0.0, 0.0)), ([-100.0, 100.0, 100.0], corner(10.0, 1.0)), ([100.0, 100.0, 100.0], corner(20.0, 1.0))];
        let mut vertices = Vec::new();
        push_clipped(&mut vertices, corners, Near { distance: 10.0, h: [100.0; 2], shift: [0.0; 2] }, (160.0, 120.0), true, false);
        assert_eq!(vertices.len(), 6, "a quad: two triangles");
        // The cuts lie on the near plane halfway along the edges from the
        // point behind it, with the colour and texture coordinates there.
        let cuts: Vec<&Vertex> = vertices.iter().filter(|v| v.depth == 10.0).collect();
        assert_eq!(cuts.len(), 3, "a fan from the first cut: in both triangles, the second in one");
        assert!(cuts.iter().all(|v| v.color[0] == 0.5 && v.w == 10.0 / 1024.0 && v.position[1] == 120.5 + 1000.0));
        let mut cut: Vec<(f32, f32)> = cuts.iter().map(|v| (v.position[0], v.uv[0])).collect();
        cut.sort_by(|a, b| a.0.total_cmp(&b.0));
        cut.dedup();
        assert_eq!(cut, [(160.5 - 500.0, 5.0), (160.5 + 500.0, 10.0)], "x ±50 on the near plane, projected");
        // Wholly behind it, nothing.
        let behind = corners.map(|(point, vertex)| ([point[0], point[1], 5.0], vertex));
        let mut none = Vec::new();
        push_clipped(&mut none, behind, Near { distance: 10.0, h: [100.0; 2], shift: [0.0; 2] }, (160.0, 120.0), true, false);
        assert!(none.is_empty());
    }
}
