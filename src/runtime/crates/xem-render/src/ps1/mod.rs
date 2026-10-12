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

mod effects;
mod frame;
mod view;

pub use effects::{DebugView, Effects, EffectsRenderer, Lighting, Quality, SceneAnalysis, SpriteGroup};
pub use frame::{FrameRenderer, Framing, central_depth, interpolate, pull_back};
pub use view::{GameView, HostClass, MAX_WIDE_SIDE, Presented, ViewConfig, scene_zoom, wide_side};

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
    /// Drop projected triangles wound against the frame's majority (the
    /// game already culled its back faces; this also drops the double-sided
    /// faces it meant to show).
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
    /// Wherever they fall (a pulled-back camera).
    Everywhere,
}

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
    pub(crate) _pad: [u32; 2],
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
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) enum DepthMode {
    /// Drawn over everything; depth unchanged.
    Over,
    /// Tested and written (depth-tested ordering).
    Test,
    /// Writes the far plane (fills).
    Clear,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) struct PipelineKey {
    pub(crate) fragment: Fragment,
    pub(crate) blend: u32,
    pub(crate) depth: DepthMode,
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
    Copy { source: (u16, u16), destination: (u16, u16), width: u16, height: u16 },
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
            let blend = match key.fragment {
                // Weight 0 for the colour, 1 for the auxiliary target, its
                // alpha included (flags 255).
                Fragment::Mark => {
                    let over = C { src_factor: F::SrcAlpha, dst_factor: F::OneMinusSrcAlpha, operation: O::Add };
                    Some(wgpu::BlendState { color: over, alpha: over })
                }
                Fragment::Semi => Some(wgpu::BlendState {
                    color: match key.blend {
                        0 => C { src_factor: F::SrcAlpha, dst_factor: F::OneMinusSrcAlpha, operation: O::Add },
                        2 => C { src_factor: F::SrcAlpha, dst_factor: F::One, operation: O::ReverseSubtract },
                        _ => C { src_factor: F::SrcAlpha, dst_factor: F::One, operation: O::Add },
                    },
                    alpha: keep,
                }),
                _ => None,
            };
            let (vertex, fragment) = match key.fragment {
                Fragment::Opaque => ("vs_main", "fs_opaque"),
                Fragment::OpaqueTexels => ("vs_main", "fs_opaque_texels"),
                Fragment::Semi => ("vs_main", "fs_semi"),
                Fragment::Rect => ("vs_rect", "fs_rect"),
                Fragment::Shadow => ("vs_main", "fs_shadow"),
                Fragment::Mark => ("vs_main", "fs_mark"),
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
                DepthMode::Clear => (wgpu::CompareFunction::Always, true),
            };
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
                primitive: wgpu::PrimitiveState::default(),
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
    scene: Option<(i16, i16)>,
    pub(crate) stats: DrawStats,
    /// Horizontal VRAM range treated as inside the drawing area beyond it
    /// (wider framing): scene primitives may spill this far.
    pub(crate) spill: u32,
    /// Where the faces the game culled are shown.
    pub(crate) reveal: Reveal,
    /// Moves every vertex (shadows, interpolated replays).
    pub(crate) remap: Option<Box<dyn Fn(&mut Vertex) + 'a>>,
    /// The area writes are shown in (a replay's display area; everywhere
    /// when none): elsewhere they only change the VRAM textures sample.
    pub(crate) shown: Option<VramRect>,
    /// Grow opaque world triangles by [`SEAL`] (the world map's terrain,
    /// whose blocks of different detail meet in T-junctions).
    pub(crate) seal: bool,
    /// Row quads the interface may still take ([`push_rows`]); past them
    /// its polygons are drawn whole.
    rows_left: usize,
}

/// Row quads one build may draw the interface with (about 24 MB of vertices).
const ROW_BUDGET: usize = 65_536;

fn color(rgb: u32) -> [f32; 4] {
    [f32::from(rgb as u8) / 255.0, f32::from((rgb >> 8) as u8) / 255.0, f32::from((rgb >> 16) as u8) / 255.0, 1.0]
}

/// The screen offset most of the scene is projected with. Projections
/// with another offset (the field compass, 3D interface models) are
/// interface, not scene.
pub fn scene_offset(ops: &[TraceOp]) -> Option<(i16, i16)> {
    let mut counts: HashMap<(i16, i16), usize> = HashMap::new();
    for op in ops {
        if let TraceOp::Draw(p) = op
            && p.class.is_scene()
            && let Some((offset, _)) = p.projection
        {
            *counts.entry(offset).or_default() += 1;
        }
    }
    counts.into_iter().max_by_key(|(_, n)| *n).map(|(offset, _)| offset)
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
            remap: None,
            rows_left: ROW_BUDGET,
            shown: None,
            seal: false,
        }
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
        let vertex = |x, y| Vertex {
            position: [x, y],
            depth: 0.0,
            w: 1.0,
            color: color(rgb),
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
        let key = PipelineKey { fragment: Fragment::Rect, blend: 0, depth: DepthMode::Clear };
        self.push_draw(key, None, start);
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
            let key = PipelineKey { fragment: Fragment::Rect, blend: 0, depth: DepthMode::Clear };
            self.push_draw(key, None, start);
        }
    }

    pub(crate) fn copy(&mut self, source: (u16, u16), destination: (u16, u16), width: u16, height: u16) {
        self.commands.push(Command::Copy { source, destination, width, height });
    }

    /// One traced primitive as triangles under its pipelines.
    pub(crate) fn primitive(&mut self, p: &Primitive) {
        if p.revealed && (self.reveal == Reveal::Hidden || self.options.culling.viewport) {
            return;
        }
        self.stats.primitives += 1;
        self.stats.revealed += u32::from(p.revealed);
        let start = self.triangles(p);
        self.commands_for(p, start);
    }

    /// A primitive's triangles; returns the index of its first vertex.
    pub(crate) fn triangles(&mut self, p: &Primitive) -> u32 {
        let fidelity = &self.options.fidelity;
        let count = p.vertex_count();
        let class = effective_class(p, self.scene);
        let projected = p.vertices[..count].iter().all(|v| v.z > 0.0);
        // The interface's attributes are screen-linear, as the original's
        // (a projected one too: the field compass, the world map's).
        let perspective = projected && !fidelity.affine_textures && p.shape != Shape::Rectangle && class != Class::Flat;
        let linear = match class {
            Class::Flat => self.options.filtering.interface_linear,
            Class::Sprite => self.options.filtering.sprites_linear,
            _ => self.options.filtering.world_linear,
        };
        let blend = p.environment.draw_mode >> 5 & 3;
        let baked = p.gouraud || (p.textured && !p.raw && p.vertices[..count].iter().any(|v| v.color & 0xFF_FFFF != 0x80_8080));
        let mut flags = (blend << BLEND_SHIFT) | (aux_class(class) << CLASS_SHIFT);
        for (on, flag) in [
            (p.textured, FLAG_TEXTURED),
            (p.raw, FLAG_RAW),
            (p.dither, FLAG_DITHER),
            (p.semi, FLAG_SEMI),
            (p.environment.mask & 1 != 0, FLAG_MASK_SET),
            (linear && p.textured, FLAG_LINEAR),
            (p.shape == Shape::Rectangle, FLAG_RECTANGLE),
            (baked, FLAG_BAKED),
            (class == Class::Flat, FLAG_ORIGINAL),
            (p.shape == Shape::Line, FLAG_LINE),
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
        let vertex = |i: usize| {
            let v = &p.vertices[i];
            // Each vertex at the GTE's sub-pixel point when it has one, so
            // a point shared by neighbouring faces is the same in each; the
            // interface at the original's whole pixels.
            let snap = fidelity.vertex_snapping || v.z <= 0.0 || class == Class::Flat;
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
            out
        };
        let start = self.vertices.len() as u32;
        match p.shape {
            Shape::Triangle | Shape::Quad => {
                let rows = class == Class::Flat && self.remap.is_none() && self.rows_left > 0 && needs_rows(p);
                let [left, top, right, bottom] = p.environment.area.map(i32::from);
                for triangle in [[0, 1, 2], [1, 2, 3]].iter().take(if p.shape == Shape::Quad { 2 } else { 1 }) {
                    // A face the game culled never met the GPU's limit.
                    if !p.revealed && !fits(p, *triangle) {
                        self.stats.rejected_size += 1;
                        continue;
                    }
                    if self.options.culling.back_faces && class.is_scene() && winding(p, *triangle) * self.front < 0 {
                        self.stats.culled_back_faces += 1;
                        continue;
                    }
                    self.stats.triangles += 1;
                    if rows {
                        let points = triangle.map(|i| (p.vertices[i].x, p.vertices[i].y));
                        let drawn = push_rows(&mut self.vertices, triangle.map(&vertex), points, (left, top, right, bottom));
                        self.rows_left = self.rows_left.saturating_sub(drawn);
                    } else if self.seal && class == Class::World && !p.semi && projected {
                        self.vertices.extend(grow(triangle.map(&vertex), SEAL));
                    } else {
                        for &i in triangle {
                            self.vertices.push(vertex(i));
                        }
                    }
                }
            }
            Shape::Rectangle => {
                for i in [0, 1, 2, 1, 3, 2] {
                    self.vertices.push(vertex(i));
                }
            }
            Shape::Line if class == Class::Flat && self.remap.is_none() => {
                let (a, b) = (&p.vertices[0], &p.vertices[1]);
                if (b.x - a.x).abs() < 1024 && (b.y - a.y).abs() < 512 {
                    push_line(&mut self.vertices, [vertex(0), vertex(1)], [(a.x, a.y), (b.x, b.y)]);
                }
            }
            Shape::Line => {
                let (a, b) = (vertex(0), vertex(1));
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
        start
    }

    fn commands_for(&mut self, p: &Primitive, start: u32) {
        let fidelity = &self.options.fidelity;
        let projected = p.vertices[..p.vertex_count()].iter().all(|v| v.z > 0.0);
        let blend = p.environment.draw_mode >> 5 & 3;
        let [left, top, right, bottom] = p.environment.area.map(i32::from);
        // The scene, and what the game culled beside its screen (a backdrop's
        // continuation too), may spill into the sides.
        let spill = if p.revealed || effective_class(p, self.scene).is_scene() { self.spill as i32 } else { 0 };
        let rect = |left: i32, right: i32| Some(Scissor { x: left, y: top, width: (right + 1 - left).max(0), height: (bottom + 1 - top).max(0) });
        // A culled face beside the original screen shows only in the sides.
        let scissors = if p.revealed && self.reveal == Reveal::Sides {
            vec![rect(left - spill, left - 1), rect(right + 1, right + spill)]
        } else {
            vec![rect(left - spill, right + spill)]
        };
        let depth = if !fidelity.painter_ordering && projected { DepthMode::Test } else { DepthMode::Over };
        let end = self.vertices.len() as u32;
        // Semi-transparent pixels leave the depth and class under them; those
        // of the interface (windows) are then marked as interface, so no
        // effect touches them (a full-screen one, a fade, is a screen effect).
        let mark = p.semi && effective_class(p, self.scene) == Class::Flat && !self.covers_screen(p);
        for scissor in scissors {
            if !p.semi {
                self.push_range(PipelineKey { fragment: Fragment::Opaque, blend: 0, depth }, scissor, start..end);
            } else if p.textured {
                // Opaque texels, then semi-transparent ones, over the same vertices.
                self.push_range(PipelineKey { fragment: Fragment::OpaqueTexels, blend: 0, depth }, scissor, start..end);
                self.push_range(PipelineKey { fragment: Fragment::Semi, blend, depth }, scissor, start..end);
            } else {
                self.push_range(PipelineKey { fragment: Fragment::Semi, blend, depth }, scissor, start..end);
            }
            if mark {
                self.push_range(PipelineKey { fragment: Fragment::Mark, blend: 0, depth }, scissor, start..end);
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
            TraceOp::VBlank => {}
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
                    let row = (u32::from(rect.width) * 2).next_multiple_of(wgpu::COPY_BYTES_PER_ROW_ALIGNMENT);
                    encoder.copy_buffer_to_texture(
                        wgpu::TexelCopyBufferInfo {
                            buffer: staging,
                            layout: wgpu::TexelCopyBufferLayout { offset: *offset, bytes_per_row: Some(row), rows_per_image: None },
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
                index += 1;
            }
            Command::Copy { source, destination, width, height } => {
                match copies {
                    Copies::InPlace(scratch) => copy_region(encoder, target, scratch, *source, *destination, (*width, *height)),
                    Copies::From { color, aux, scale } => {
                        copy_from(encoder, target, (color, aux, scale), *source, *destination, (*width, *height))
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
                    let (x, y, w, h) = match scissor {
                        Some(s) => {
                            let x0 = ((s.x as f32 - target.origin.0) * target.scale as f32).max(0.0) as u32;
                            let y0 = ((s.y as f32 - target.origin.1) * target.scale as f32).max(0.0) as u32;
                            let x1 = (((s.x + s.width) as f32 - target.origin.0) * target.scale as f32).max(0.0) as u32;
                            let y1 = (((s.y + s.height) as f32 - target.origin.1) * target.scale as f32).max(0.0) as u32;
                            let x0 = x0.min(size.width);
                            let y0 = y0.min(size.height);
                            (x0, y0, x1.min(size.width).saturating_sub(x0), y1.min(size.height).saturating_sub(y0))
                        }
                        None => (0, 0, size.width, size.height),
                    };
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

/// How [`encode`] copies VRAM rectangles.
#[derive(Clone, Copy)]
pub(crate) enum Copies<'a> {
    /// Inside the target, through a scratch texture.
    InPlace(&'a wgpu::Texture),
    /// From the scaled VRAM picture (colour, auxiliary, scale) into the target.
    From { color: &'a wgpu::Texture, aux: &'a wgpu::Texture, scale: u32 },
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
) {
    let s = target.scale;
    let size = target.color.size();
    let clip = |x: u16, y: u16| {
        let x = (f32::from(x) - target.origin.0) * s as f32;
        let y = (f32::from(y) - target.origin.1) * s as f32;
        (x, y)
    };
    let (sx, sy) = clip(source.0, source.1);
    let (dx, dy) = clip(destination.0, destination.1);
    if sx < 0.0 || sy < 0.0 || dx < 0.0 || dy < 0.0 {
        return;
    }
    let (sx, sy, dx, dy) = (sx as u32, sy as u32, dx as u32, dy as u32);
    let w = (u32::from(width) * s).min(size.width.saturating_sub(sx)).min(size.width.saturating_sub(dx));
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
    /// The display's size in VRAM pixels (a screen buffer's), when known.
    display: Option<(u16, u16)>,
    /// The native VRAM as the trace's writes left it (the start of each
    /// buffer's log copies it).
    native: Vec<u16>,
    /// Seal the world's T-junction cracks ([`Builder::seal`]).
    pub(crate) seal: bool,
    stats: DrawStats,
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
    /// Clears so far (one per frame drawn into the buffer).
    pub frames: u64,
    /// The native VRAM when the buffer was cleared: a replay samples VRAM
    /// as it was then, changed by the writes in `ops` as it goes.
    pub start: std::sync::Arc<Vec<u16>>,
}

/// A log grows without a clear in modes that never clear; it is dropped
/// past this length (the effects then see no primitives).
const LOG_LIMIT: usize = 200_000;

fn contains(rect: &VramRect, x: u16, y: u16) -> bool {
    x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height
}

fn overlap(a: &VramRect, b: &VramRect) -> u32 {
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
        let group = bind_group(device, &pipelines.layout, &uniform, &vram);
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
            display: None,
            native: vec![0; (VRAM_WIDTH * VRAM_HEIGHT) as usize],
            seal: false,
            stats: DrawStats::default(),
        }
    }

    /// The size of the display (a screen buffer) the frames are drawn for:
    /// a fill or copy of about that size clears a buffer.
    pub fn set_display(&mut self, width: u16, height: u16) {
        self.display = Some((width, height));
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
        self.log(&trace.ops);
        let options = self.options;
        let mut builder = Builder::new(&options, &trace.ops);
        builder.seal = self.seal;
        for op in &trace.ops {
            builder.op(op);
        }
        let uniform = PassUniform {
            origin: [0.0, 0.0],
            extent: [VRAM_WIDTH as f32, VRAM_HEIGHT as f32],
            scale: self.scale as f32,
            options: pass_options(&options.fidelity),
            _pad: [0; 2],
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
            Copies::InPlace(&self.scratch),
            &target,
            &builder.commands,
        );
    }

    fn log(&mut self, ops: &[TraceOp]) {
        for op in ops {
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
                TraceOp::Copy { destination, width, height, .. } => {
                    Some(VramRect { x: destination.0, y: destination.1, width: *width, height: *height })
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
                if let Some(log) = self.logs.iter_mut().find(|l| {
                    let size = u32::from(l.rect.width) * u32::from(l.rect.height);
                    overlap(&l.rect, &rect) * 10 >= size.max(area) * 9
                }) {
                    log.previous = std::mem::replace(&mut log.ops, vec![op.clone()]);
                    log.rect = rect;
                    log.frames += 1;
                    log.start = std::sync::Arc::new(self.native.clone());
                    continue;
                }
                if buffer || self.logs.iter().all(|l| overlap(&l.rect, &rect) == 0) {
                    let previous = self.logs.iter().find(|l| overlap(&l.rect, &rect) > 0).map_or(0, |l| l.frames);
                    self.logs.retain(|l| overlap(&l.rect, &rect) == 0);
                    let start = std::sync::Arc::new(self.native.clone());
                    self.logs.push(FrameLog { rect, ops: vec![op.clone()], previous: Vec::new(), frames: previous + 1, start });
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
            if let Some((x, y)) = at
                && let Some(log) = self.logs.iter_mut().find(|l| contains(&l.rect, x, y))
            {
                if log.ops.len() >= LOG_LIMIT {
                    log.ops.clear();
                    log.previous.clear();
                }
                log.ops.push(op.clone());
            }
        }
    }

    /// The log of the screen buffer `area` shows (most of it inside the
    /// buffer's last clear).
    pub fn frame_log(&self, area: DisplayArea) -> Option<&FrameLog> {
        let rect = VramRect { x: area.x as u16, y: area.y as u16, width: area.width as u16, height: area.height as u16 };
        let size = area.width * area.height;
        self.logs.iter().find(|l| overlap(&l.rect, &rect) * 2 >= size)
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

pub(crate) fn pass_options(fidelity: &Fidelity) -> u32 {
    u32::from(!fidelity.affine_textures)
        | u32::from(fidelity.color_15bit) << 1
        | u32::from(fidelity.dithering) << 2
        | u32::from(!fidelity.painter_ordering) << 3
}

pub(crate) fn bind_group(device: &wgpu::Device, layout: &wgpu::BindGroupLayout, uniform: &wgpu::Buffer, vram: &wgpu::Texture) -> wgpu::BindGroup {
    device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some("ps1 pass"),
        layout,
        entries: &[
            wgpu::BindGroupEntry { binding: 0, resource: uniform.as_entire_binding() },
            wgpu::BindGroupEntry { binding: 1, resource: wgpu::BindingResource::TextureView(&vram.create_view(&Default::default())) },
        ],
    })
}
