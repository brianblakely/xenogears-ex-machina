//! The HD-2D-inspired profile's effects over the displayed frame
//! (effects.wgsl): they read the frame's colour, its per-pixel GTE depth and
//! class, and the frame's primitives (sprite groups cast shadows). They use
//! only the game's own pictures; no art is added or replaced. Interface
//! pixels stay as the game drew them.
//!
//! Scene tuning (light, fog, grading per mode and map) is presentation
//! metadata in `scenes.json` beside this file, not part of the game.

use super::{Builder, DepthMode, Fragment, FrameRenderer, PassUniform, PipelineKey, Ps1Renderer, Stream, bind_group, texture};
use bytemuck::{Pod, Zeroable};
use serde::Deserialize;
use std::collections::HashMap;
use xem_gpu::{Class, TraceOp};

/// Effect strengths, 0 (off) to 1 (full), and the focus shift.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Effects {
    pub lighting: f32,
    pub shadows: f32,
    pub ambient_occlusion: f32,
    pub fog: f32,
    pub light_shafts: f32,
    pub bloom: f32,
    pub color_grading: f32,
    pub depth_of_field: f32,
    /// Moves the focus nearer (negative) or farther (positive), -1 to 1.
    pub focus: f32,
}

impl Effects {
    pub const OFF: Effects = Effects {
        lighting: 0.0,
        shadows: 0.0,
        ambient_occlusion: 0.0,
        fog: 0.0,
        light_shafts: 0.0,
        bloom: 0.0,
        color_grading: 0.0,
        depth_of_field: 0.0,
        focus: 0.0,
    };

    /// The HD-2D-inspired profile's defaults.
    pub const HD2D: Effects = Effects {
        lighting: 0.6,
        shadows: 0.8,
        ambient_occlusion: 0.6,
        fog: 0.5,
        light_shafts: 0.4,
        bloom: 0.5,
        color_grading: 0.7,
        depth_of_field: 0.6,
        focus: 0.0,
    };

    pub fn any(&self) -> bool {
        [self.lighting, self.shadows, self.ambient_occlusion, self.fog, self.light_shafts, self.bloom, self.color_grading, self.depth_of_field]
            .iter()
            .any(|v| *v > 0.0)
    }
}

impl Default for Effects {
    fn default() -> Self {
        Effects::OFF
    }
}

/// Explicit cost tiers. A tier changes sample counts and resolutions only,
/// never which profile or effects are selected.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Quality {
    /// Headset and low-power budgets: no ambient occlusion, light shafts or
    /// depth of field samples beyond the minimum.
    Low,
    /// Mobile and browser budgets.
    Medium,
    #[default]
    High,
    Ultra,
}

impl Quality {
    fn samples(self) -> (u32, u32, u32, usize) {
        // Ambient occlusion, depth of field, light shafts, bloom levels.
        match self {
            Quality::Low => (0, 8, 16, 3),
            Quality::Medium => (6, 12, 24, 4),
            Quality::High => (10, 20, 40, 5),
            Quality::Ultra => (16, 32, 64, 6),
        }
    }
}

/// Scene tuning: presentation metadata, never game state.
#[derive(Clone, Copy, Debug, PartialEq, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct Lighting {
    /// Towards the key light, view space (x right, y up, z into the screen).
    pub direction: [f32; 3],
    pub color: [f32; 3],
    pub ambient: [f32; 3],
    pub fog_color: [f32; 3],
    /// Fog start and end as fractions of the scene's depth range (auto).
    pub fog_range: [f32; 2],
    /// Light-shaft source in frame coordinates (0-1).
    pub shaft_source: [f32; 2],
    pub shaft_threshold: f32,
    pub bloom_threshold: f32,
    pub exposure: f32,
    pub contrast: f32,
    pub saturation: f32,
    pub warmth: f32,
    pub vignette: f32,
    /// Cast shadows: horizontal lean and length per unit of sprite height
    /// (positive length lays them up the screen, behind the sprite;
    /// negative towards the camera).
    pub shadow_lean: f32,
    pub shadow_length: f32,
    /// Strength of a sprite's cast (directional) and contact shadows, 0-1.
    pub cast_shadow: f32,
    pub contact_shadow: f32,
}

impl Default for Lighting {
    fn default() -> Self {
        Lighting {
            direction: [-0.45, 0.75, -0.5],
            color: [0.5, 0.44, 0.34],
            ambient: [0.82, 0.85, 0.94],
            fog_color: [0.55, 0.6, 0.68],
            fog_range: [0.45, 1.25],
            shaft_source: [0.25, -0.15],
            shaft_threshold: 0.55,
            bloom_threshold: 0.56,
            exposure: 0.0,
            contrast: 1.08,
            saturation: 1.18,
            warmth: 0.7,
            vignette: 0.4,
            shadow_lean: 0.5,
            shadow_length: -0.28,
            cast_shadow: 0.4,
            contact_shadow: 0.9,
        }
    }
}

impl Lighting {
    /// The tuning for `scene` ("field:22", "battle", "worldmap", ...): the
    /// most specific entry of scenes.json, else its mode's, else the default.
    pub fn for_scene(scene: &str) -> Lighting {
        static TABLE: std::sync::OnceLock<std::collections::HashMap<String, Lighting>> = std::sync::OnceLock::new();
        let table = TABLE.get_or_init(|| serde_json::from_str(include_str!("scenes.json")).expect("scenes.json"));
        let mode = scene.split(':').next().unwrap_or("");
        table.get(scene).or_else(|| table.get(mode)).or_else(|| table.get("default")).copied().unwrap_or_default()
    }
}

#[repr(C)]
#[derive(Clone, Copy, Pod, Zeroable, Default)]
struct Params {
    size: [f32; 2],
    inv_size: [f32; 2],
    scale: f32,
    focal: f32,
    center: [f32; 2],
    light_dir: [f32; 4],
    light_color: [f32; 4],
    ambient: [f32; 4],
    fog: [f32; 4],
    ranges: [f32; 4],
    shaft: [f32; 4],
    bloom: [f32; 4],
    grade: [f32; 4],
    grade2: [f32; 4],
    quality: [u32; 4],
    step: [f32; 4],
    ramps: [f32; 4],
}

/// What the frame's primitives tell the effects.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SceneAnalysis {
    /// Sprite groups (characters): bounds in VRAM pixels, feet depth and
    /// the floor point under them.
    pub sprites: Vec<SpriteGroup>,
    /// Depth percentiles of the scene primitives: 10th, 50th, 90th.
    pub depths: Option<[f32; 3]>,
    /// The depths kept sharp, nearest and farthest: the subjects (sprites,
    /// and models of a character's size) in the playfield around the one
    /// nearest the middle of the screen, else the scene's middle depths
    /// there.
    pub focus: Option<[f32; 2]>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct SpriteGroup {
    /// The sprite's game address.
    pub owner: u32,
    pub bounds: [f32; 4],
    pub depth: f32,
    /// The point on the floor under the sprite (VRAM x, y, depth), where
    /// the game keeps one.
    pub ground: Option<[f32; 3]>,
    /// Indices of the group's primitives in the analysed operations.
    pub primitives: Vec<usize>,
}

/// A sprite or model the game drew: bounds and depth range.
struct Subject {
    bounds: [f32; 4],
    near: f32,
    far: f32,
}

impl SceneAnalysis {
    /// Groups the sprite parts of `ops` and measures the scene's depths.
    pub fn of(ops: &[TraceOp], area: super::DisplayArea) -> SceneAnalysis {
        let mut groups: Vec<SpriteGroup> = Vec::new();
        let mut subjects: Vec<Subject> = Vec::new();
        // Where each owner's group and subject are in the lists.
        let mut group_of: HashMap<u32, usize> = HashMap::new();
        let mut subject_of: HashMap<(bool, u32), usize> = HashMap::new();
        let mut depths = Vec::new();
        let mut central = Vec::new();
        let scene = super::scene_offset(ops);
        let (cx, cy) = (area.x as f32 + area.width as f32 / 2.0, area.y as f32 + area.height as f32 / 2.0);
        for (index, op) in ops.iter().enumerate() {
            let TraceOp::Draw(p) = op else { continue };
            let class = super::effective_class(p, scene);
            let depth = p.depth();
            if depth <= 0.0 || !class.is_scene() {
                continue;
            }
            depths.push(depth);
            let vertices = &p.vertices[..p.vertex_count()];
            let bounds = [
                vertices.iter().map(|v| v.fx).fold(f32::MAX, f32::min),
                vertices.iter().map(|v| v.fy).fold(f32::MAX, f32::min),
                vertices.iter().map(|v| v.fx).fold(f32::MIN, f32::max),
                vertices.iter().map(|v| v.fy).fold(f32::MIN, f32::max),
            ];
            let middle = ((bounds[0] + bounds[2]) / 2.0, (bounds[1] + bounds[3]) / 2.0);
            if (middle.0 - cx).abs() < area.width as f32 / 6.0 && (middle.1 - cy).abs() < area.height as f32 / 5.0 {
                central.push(depth);
            }
            if p.owner != 0 && matches!(class, Class::Sprite | Class::Model) && !p.revealed {
                match subject_of.get(&(class == Class::Sprite, p.owner)) {
                    Some(&at) => {
                        let s = &mut subjects[at];
                        s.bounds = [s.bounds[0].min(bounds[0]), s.bounds[1].min(bounds[1]), s.bounds[2].max(bounds[2]), s.bounds[3].max(bounds[3])];
                        s.near = s.near.min(depth);
                        s.far = s.far.max(depth);
                    }
                    None => {
                        subject_of.insert((class == Class::Sprite, p.owner), subjects.len());
                        subjects.push(Subject { bounds, near: depth, far: depth });
                    }
                }
            }
            if class != Class::Sprite {
                continue;
            }
            // One group per sprite the game drew (its parts share the owner).
            match group_of.get(&p.owner) {
                Some(&at) => {
                    let g = &mut groups[at];
                    g.bounds = [g.bounds[0].min(bounds[0]), g.bounds[1].min(bounds[1]), g.bounds[2].max(bounds[2]), g.bounds[3].max(bounds[3])];
                    g.depth = g.depth.max(depth);
                    g.ground = g.ground.or(p.ground);
                    g.primitives.push(index);
                }
                None => {
                    group_of.insert(p.owner, groups.len());
                    groups.push(SpriteGroup { owner: p.owner, bounds, depth, ground: p.ground, primitives: vec![index] });
                }
            }
        }
        groups.retain(|g| g.bounds[2] - g.bounds[0] >= 4.0 && g.bounds[3] - g.bounds[1] >= 8.0);
        depths.sort_by(f32::total_cmp);
        let percentile = |values: &[f32], q: f32| values[((values.len() - 1) as f32 * q) as usize];
        let depths_at = (!depths.is_empty()).then(|| [percentile(&depths, 0.1), percentile(&depths, 0.5), percentile(&depths, 0.9)]);
        // Subjects: of a character's size, on the screen's middle part.
        let (width, height) = (area.width as f32, area.height as f32);
        subjects.retain(|s| {
            let (w, h) = (s.bounds[2] - s.bounds[0], s.bounds[3] - s.bounds[1]);
            let x = (s.bounds[0] + s.bounds[2]) / 2.0;
            w >= 3.0 && h >= 6.0 && w <= width * 0.35 && h <= height * 0.6 && (x - cx).abs() < width * 0.45
        });
        // The playfield: the subjects near in depth to the one nearest the
        // middle of the screen (a little below it, where the party stands),
        // nearness measured against the depths the scene spans (a camera
        // pulled back keeps its foreground and background soft).
        let span = depths_at.map_or(1.0, |d| (d[2] - d[0]).max(1.0));
        let anchor = (cx, area.y as f32 + height * 0.6);
        let distance = |s: &Subject| {
            let (x, y) = ((s.bounds[0] + s.bounds[2]) / 2.0, (s.bounds[1] + s.bounds[3]) / 2.0);
            (x - anchor.0).powi(2) + (y - anchor.1).powi(2)
        };
        let primary = subjects.iter().min_by(|a, b| distance(a).total_cmp(&distance(b))).map(|s| (s.near + s.far) / 2.0);
        let focus = match primary {
            Some(at) => {
                // Also every subject in the middle of the screen, whatever its
                // depth (all who take part in a conversation).
                let central_subject = |s: &Subject| {
                    let (x, y) = ((s.bounds[0] + s.bounds[2]) / 2.0, (s.bounds[1] + s.bounds[3]) / 2.0);
                    (x - cx).abs() < width * 0.3 && (y - cy).abs() < height * 0.3
                };
                let (near, far) = subjects
                    .iter()
                    .filter(|s| central_subject(s) || (s.far > at - span * 0.3 && s.near < at + span * 0.3))
                    .fold((at, at), |(n, f), s| (n.min(s.near), f.max(s.far)));
                Some([near, far])
            }
            None if !central.is_empty() => {
                central.sort_by(f32::total_cmp);
                Some([percentile(&central, 0.35), percentile(&central, 0.55)])
            }
            None => depths_at.map(|d| [d[1] - span * 0.05, d[1] + span * 0.05]),
        };
        SceneAnalysis { sprites: groups, depths: depths_at, focus }
    }
}

/// How an effects pass writes its target.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
enum Blend {
    /// Cleared and written.
    Replace,
    /// Added to what is there (the bloom chain's way up).
    Add,
    /// Eased towards by the output's alpha (the scene average, so exposure
    /// adapts over a few frames instead of flickering).
    Ease,
}

const STRIDE: u64 = 256;
const HDR_PREFERRED: wgpu::TextureFormat = wgpu::TextureFormat::Rgba16Float;

struct Targets {
    size: (u32, u32),
    lit: wgpu::Texture,
    shadow: wgpu::Texture,
    /// Bloom levels; level 0 is the half-resolution bright pass (a: shaft sources).
    bloom: Vec<wgpu::Texture>,
    shafts: wgpu::Texture,
    dof: wgpu::Texture,
    average: wgpu::Texture,
    output: wgpu::Texture,
    output_view: wgpu::TextureView,
}

/// Renders the effects for one frame at a time.
pub struct EffectsRenderer {
    hdr: wgpu::TextureFormat,
    shader: wgpu::ShaderModule,
    params_layout: wgpu::BindGroupLayout,
    textures_layout: wgpu::BindGroupLayout,
    layout: wgpu::PipelineLayout,
    pipelines: std::collections::HashMap<(&'static str, wgpu::TextureFormat, Blend), wgpu::RenderPipeline>,
    params: wgpu::Buffer,
    params_group: wgpu::BindGroup,
    dummy: wgpu::TextureView,
    shadow_uniform: wgpu::Buffer,
    shadow_vertices: Stream,
    targets: Option<Targets>,
    debug: u32,
    /// Bind groups and views of the passes, by their textures (they live
    /// as long as the textures; dropped when the targets are made again).
    groups: HashMap<(&'static str, Vec<wgpu::Texture>), wgpu::BindGroup>,
    views: HashMap<wgpu::Texture, wgpu::TextureView>,
    shadow_group: Option<(wgpu::Texture, u64, wgpu::BindGroup)>,
    /// The targets were just made: the scene average starts afresh.
    fresh: bool,
    /// Fog start and end, nearest and farthest sharp depth and the blur's
    /// ramps towards and away from the camera, eased between frames (a cut
    /// takes the new ones at once).
    ranges: Option<[f32; 6]>,
}

/// What [`EffectsRenderer::set_debug_view`] shows instead of the effects.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum DebugView {
    #[default]
    Off,
    /// Red: sprite shadow received; green: the shadow mask.
    Shadows,
    /// The GTE depth of each pixel.
    Depth,
    /// Red world, green model, blue sprite; black interface and background.
    Class,
}

impl EffectsRenderer {
    pub fn new(device: &wgpu::Device, adapter: Option<&wgpu::Adapter>) -> Self {
        let hdr = match adapter {
            Some(adapter)
                if adapter
                    .get_texture_format_features(HDR_PREFERRED)
                    .allowed_usages
                    .contains(wgpu::TextureUsages::RENDER_ATTACHMENT) =>
            {
                HDR_PREFERRED
            }
            Some(_) => wgpu::TextureFormat::Rgba8Unorm,
            None => HDR_PREFERRED,
        };
        let shader = device.create_shader_module(wgpu::include_wgsl!("effects.wgsl"));
        let params_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("effects params"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::VERTEX_FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Uniform,
                        has_dynamic_offset: true,
                        min_binding_size: wgpu::BufferSize::new(size_of::<Params>() as u64),
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                    count: None,
                },
            ],
        });
        let textures_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("effects textures"),
            entries: &(0..6)
                .map(|binding| wgpu::BindGroupLayoutEntry {
                    binding,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                })
                .collect::<Vec<_>>(),
        });
        let layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("effects"),
            bind_group_layouts: &[Some(&params_layout), Some(&textures_layout)],
            immediate_size: 0,
        });
        let params = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("effects params"),
            size: STRIDE * 16,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        let sampler = device.create_sampler(&wgpu::SamplerDescriptor {
            mag_filter: wgpu::FilterMode::Linear,
            min_filter: wgpu::FilterMode::Linear,
            address_mode_u: wgpu::AddressMode::ClampToEdge,
            address_mode_v: wgpu::AddressMode::ClampToEdge,
            ..Default::default()
        });
        let params_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("effects params"),
            layout: &params_layout,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: wgpu::BindingResource::Buffer(wgpu::BufferBinding {
                        buffer: &params,
                        offset: 0,
                        size: wgpu::BufferSize::new(size_of::<Params>() as u64),
                    }),
                },
                wgpu::BindGroupEntry { binding: 1, resource: wgpu::BindingResource::Sampler(&sampler) },
            ],
        });
        let dummy = texture(device, "effects dummy", (1, 1), wgpu::TextureFormat::Rgba8Unorm, wgpu::TextureUsages::TEXTURE_BINDING)
            .create_view(&Default::default());
        let shadow_uniform = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("shadow pass"),
            size: size_of::<PassUniform>() as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        Self {
            hdr,
            shader,
            params_layout,
            textures_layout,
            layout,
            pipelines: Default::default(),
            params,
            params_group,
            dummy,
            shadow_uniform,
            shadow_vertices: Stream::new(device, "shadow vertices", wgpu::BufferUsages::VERTEX),
            targets: None,
            debug: 0,
            groups: HashMap::new(),
            views: HashMap::new(),
            shadow_group: None,
            fresh: true,
            ranges: None,
        }
    }

    pub fn set_debug_view(&mut self, view: DebugView) {
        self.debug = view as u32;
    }

    /// The intermediate format (half-float where the device renders it).
    pub fn hdr_format(&self) -> wgpu::TextureFormat {
        self.hdr
    }

    fn targets(&mut self, device: &wgpu::Device, size: (u32, u32), levels: usize) -> &Targets {
        let stale = self.targets.as_ref().is_none_or(|t| t.size != size || t.bloom.len() != levels);
        if stale {
            self.fresh = true;
            self.groups.clear();
            self.views.clear();
            let usage = wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_SRC;
            let half = ((size.0 / 2).max(1), (size.1 / 2).max(1));
            let bloom = (0..levels)
                .map(|level| texture(device, "bloom", ((half.0 >> level).max(1), (half.1 >> level).max(1)), self.hdr, usage))
                .collect();
            // The frame's interface is drawn over it afterwards, copies too.
            let output = texture(device, "effects output", size, wgpu::TextureFormat::Rgba8Unorm,
                usage | wgpu::TextureUsages::COPY_SRC | wgpu::TextureUsages::COPY_DST);
            self.targets = Some(Targets {
                size,
                lit: texture(device, "lit", size, self.hdr, usage),
                shadow: texture(device, "shadow mask", size, wgpu::TextureFormat::Rgba8Unorm, usage),
                bloom,
                shafts: texture(device, "shafts", half, self.hdr, usage),
                dof: texture(device, "depth of field", half, self.hdr, usage),
                average: texture(device, "scene average", (1, 1), self.hdr, usage),
                output_view: output.create_view(&Default::default()),
                output,
            });
        }
        self.targets.as_ref().expect("created above")
    }

    fn pipeline(&mut self, device: &wgpu::Device, entry: &'static str, format: wgpu::TextureFormat, mode: Blend) -> wgpu::RenderPipeline {
        self.pipelines
            .entry((entry, format, mode))
            .or_insert_with(|| {
                use wgpu::{BlendComponent as C, BlendFactor as F, BlendOperation as O};
                let blend = match mode {
                    Blend::Replace => None,
                    Blend::Add => Some(wgpu::BlendState { color: C { src_factor: F::One, dst_factor: F::One, operation: O::Add }, alpha: C::REPLACE }),
                    Blend::Ease => Some(wgpu::BlendState {
                        color: C { src_factor: F::SrcAlpha, dst_factor: F::OneMinusSrcAlpha, operation: O::Add },
                        alpha: C::REPLACE,
                    }),
                };
                device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
                    label: Some(entry),
                    layout: Some(&self.layout),
                    vertex: wgpu::VertexState { module: &self.shader, entry_point: Some("vs_full"), compilation_options: Default::default(), buffers: &[] },
                    primitive: Default::default(),
                    depth_stencil: None,
                    multisample: Default::default(),
                    fragment: Some(wgpu::FragmentState {
                        module: &self.shader,
                        entry_point: Some(entry),
                        compilation_options: Default::default(),
                        targets: &[Some(wgpu::ColorTargetState {
                            format,
                            blend,
                            write_mask: if mode == Blend::Add { wgpu::ColorWrites::COLOR } else { wgpu::ColorWrites::ALL },
                        })],
                    }),
                    multiview_mask: None,
                    cache: None,
                })
            })
            .clone()
    }

    #[allow(clippy::too_many_arguments)]
    fn pass(
        &mut self,
        device: &wgpu::Device,
        encoder: &mut wgpu::CommandEncoder,
        entry: &'static str,
        target: &wgpu::Texture,
        mode: Blend,
        slot: u32,
        inputs: &[&wgpu::Texture],
    ) {
        let pipeline = self.pipeline(device, entry, target.format(), mode);
        // The frame's own textures change with its size: a cache that only
        // grows past a few sizes' worth starts again.
        if self.groups.len() > 64 || self.views.len() > 64 {
            self.groups.clear();
            self.views.clear();
        }
        let key = (entry, inputs.iter().map(|t| (*t).clone()).collect::<Vec<_>>());
        if !self.groups.contains_key(&key) {
            let views: Vec<wgpu::TextureView> = inputs.iter().map(|t| t.create_view(&Default::default())).collect();
            let entries: Vec<wgpu::BindGroupEntry> = (0..6)
                .map(|i| wgpu::BindGroupEntry {
                    binding: i as u32,
                    resource: wgpu::BindingResource::TextureView(views.get(i).unwrap_or(&self.dummy)),
                })
                .collect();
            let group = device.create_bind_group(&wgpu::BindGroupDescriptor { label: Some(entry), layout: &self.textures_layout, entries: &entries });
            self.groups.insert(key.clone(), group);
        }
        let group = self.groups[&key].clone();
        let view = self.views.entry(target.clone()).or_insert_with(|| target.create_view(&Default::default())).clone();
        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some(entry),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: &view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations {
                    load: if mode == Blend::Replace { wgpu::LoadOp::Clear(wgpu::Color::TRANSPARENT) } else { wgpu::LoadOp::Load },
                    store: wgpu::StoreOp::Store,
                },
            })],
            ..Default::default()
        });
        pass.set_pipeline(&pipeline);
        pass.set_bind_group(0, &self.params_group, &[(u64::from(slot) * STRIDE) as u32]);
        pass.set_bind_group(1, &group, &[]);
        pass.draw(0..3, 0..1);
    }

    /// Draws the effects of `frame` (whose primitives are `ops`) and returns
    /// the result: sRGB-encoded values in an `Rgba8Unorm` texture of the
    /// frame's size. Call at most once per queue submission.
    #[allow(clippy::too_many_arguments)]
    pub fn render(
        &mut self,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        encoder: &mut wgpu::CommandEncoder,
        ps1: &mut Ps1Renderer,
        frame: &FrameRenderer,
        ops: &[TraceOp],
        effects: &Effects,
        quality: Quality,
        lighting: &Lighting,
    ) -> &wgpu::TextureView {
        let size = frame.size();
        let Some((area, scale)) = frame.area() else {
            return &self.targets(device, size, 1).output_view;
        };
        let (ao, dof, shafts, levels) = quality.samples();
        self.targets(device, size, levels);
        let analysis = SceneAnalysis::of(ops, area);

        // Shadow mask: sprite silhouettes laid on the floor and contact
        // ellipses, both at the floor point under the sprite (the game's
        // own, where it keeps one): a sprite that jumps leaves its shadow
        // on the floor, smaller and fainter the higher it is.
        let options = *ps1.options();
        let mut builder = Builder::new(&options, &[]);
        if effects.shadows > 0.0 {
            for group in &analysis.sprites {
                let (width, height) = (group.bounds[2] - group.bounds[0], (group.bounds[3] - group.bounds[1]).max(1.0));
                let feet = ((group.bounds[0] + group.bounds[2]) / 2.0, group.bounds[3]);
                let (ground, depth) = match group.ground {
                    Some([x, y, z]) if y >= feet.1 - height * 0.15 && y - feet.1 < height * 2.5 && (z / group.depth - 1.0).abs() < 0.35 => ((x, y), z),
                    _ => (feet, group.depth),
                };
                let lift = ((ground.1 - feet.1) / height).clamp(0.0, 1.0);
                let (lean, length) = (lighting.shadow_lean, lighting.shadow_length);
                let cast = lighting.cast_shadow * (1.0 - 0.7 * lift);
                builder.remap = Some(Box::new(move |v| {
                    let dx = v.position[0] - feet.0;
                    // Height above the floor: above the feet, plus the jump.
                    let up = (feet.1 - v.position[1]).max(0.0) + (ground.1 - feet.1).max(0.0);
                    v.position = [ground.0 + dx + up * lean, ground.1 - up * length];
                    v.depth = depth;
                    v.w = 1.0;
                    v.color = [cast, 0.0, 0.0, 1.0];
                }));
                for &index in &group.primitives {
                    if let TraceOp::Draw(p) = &ops[index] {
                        builder.triangles(p);
                    }
                }
                builder.remap = None;
                let shrink = 1.0 - 0.45 * lift;
                let strength = lighting.contact_shadow * (1.0 - 0.6 * lift);
                contact(&mut builder.vertices, ground, (width * 0.4 * shrink, (width * 0.13).max(2.0) * shrink), depth, strength);
            }
        }
        let shadow_vertices = std::mem::take(&mut builder.vertices);
        let side = frame.side() as f32;
        let uniform = PassUniform {
            origin: [area.x as f32 - side, area.y as f32],
            extent: [area.width as f32 + 2.0 * side, area.height as f32],
            scale: scale as f32,
            options: super::pass_options(&options.fidelity),
            capture_scale: 0.0,
            capture_side: 0,
            capture: [0; 4],
            ..Zeroable::zeroed()
        };
        queue.write_buffer(&self.shadow_uniform, 0, bytemuck::bytes_of(&uniform));
        self.shadow_vertices.write(device, queue, bytemuck::cast_slice(&shadow_vertices));
        // Passes whose effect is off are not drawn (the light and composite
        // passes do not read their textures then).
        if effects.shadows > 0.0 {
            let targets = self.targets.as_ref().expect("created above");
            let view = self.views.entry(targets.shadow.clone()).or_insert_with(|| targets.shadow.create_view(&Default::default())).clone();
            let group = match &self.shadow_group {
                Some((vram, generation, group)) if *vram == ps1.vram && *generation == ps1.capture.generation => group.clone(),
                _ => {
                    let group = bind_group(device, &ps1.pipelines.layout, &self.shadow_uniform, &ps1.vram, &ps1.capture, None);
                    self.shadow_group = Some((ps1.vram.clone(), ps1.capture.generation, group.clone()));
                    group
                }
            };
            let pipeline = ps1.pipelines.get(device, PipelineKey { fragment: Fragment::Shadow, blend: 0, depth: DepthMode::Over, space: super::Space::Screen }).clone();
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("sprite shadows"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &view,
                    depth_slice: None,
                    resolve_target: None,
                    ops: wgpu::Operations { load: wgpu::LoadOp::Clear(wgpu::Color::TRANSPARENT), store: wgpu::StoreOp::Store },
                })],
                ..Default::default()
            });
            if !shadow_vertices.is_empty() {
                pass.set_pipeline(&pipeline);
                pass.set_bind_group(0, &group, &[]);
                pass.set_vertex_buffer(0, self.shadow_vertices.buffer().slice(..));
                pass.draw(0..shadow_vertices.len() as u32, 0..1);
            }
        }

        // Parameters.
        let depths = analysis.depths.unwrap_or([1000.0, 4000.0, 8000.0]);
        let span = (depths[2] - depths[0]).max(500.0);
        // The playfield stays sharp, with a margin; the focus setting moves
        // it; the blur grows over parts of the scene's depth span beyond it,
        // faster towards the camera.
        let [near, far] = analysis.focus.unwrap_or([depths[1] - span * 0.05, depths[1] + span * 0.05]);
        let shift = effects.focus * span * 0.5;
        let near = (near - span * 0.06 + shift).max(1.0);
        let far = (far + span * 0.08 + shift).max(near + 1.0);
        let fog_start = depths[0] + span * lighting.fog_range[0];
        let fog_end = depths[0] + span * lighting.fog_range[1];
        let wanted = [fog_start, fog_end, near, far, span * 0.35, span * 0.6];
        let eased = match self.ranges {
            Some(old) if old.iter().zip(&wanted).all(|(o, w)| (w / o.max(1.0) - 1.0).abs() < 0.6) => {
                std::array::from_fn(|i| old[i] + (wanted[i] - old[i]) * 0.15)
            }
            _ => wanted,
        };
        self.ranges = Some(eased);
        let [fog_start, fog_end, near, far, near_ramp, far_ramp] = eased;
        let ease = if std::mem::take(&mut self.fresh) { 1.0 } else { 0.12 };
        let light = glam::Vec3::from(lighting.direction).normalize_or_zero();
        let s = |c: [f32; 3], k: f32| [c[0] * k, c[1] * k, c[2] * k, 0.0];
        let base = Params {
            size: [size.0 as f32, size.1 as f32],
            inv_size: [1.0 / size.0 as f32, 1.0 / size.1 as f32],
            scale: scale as f32,
            focal: 300.0,
            center: [size.0 as f32 / 2.0, size.1 as f32 / 2.0],
            light_dir: [light.x, light.y, light.z, 0.0],
            light_color: s(lighting.color, 1.0),
            ambient: [lighting.ambient[0], lighting.ambient[1], lighting.ambient[2], effects.ambient_occlusion],
            fog: [lighting.fog_color[0], lighting.fog_color[1], lighting.fog_color[2], effects.fog * 0.7],
            ranges: [fog_start, fog_end, near, far],
            ramps: [near_ramp, far_ramp, 0.0, 0.0],
            shaft: [lighting.shaft_source[0], lighting.shaft_source[1], effects.light_shafts, lighting.shaft_threshold],
            bloom: [effects.bloom * 0.7, lighting.bloom_threshold, 0.3, effects.lighting],
            grade: [lighting.exposure, lighting.contrast, lighting.saturation, lighting.warmth],
            grade2: [lighting.vignette, effects.color_grading, effects.depth_of_field, effects.shadows],
            quality: [
                if effects.ambient_occlusion > 0.0 { ao } else { 0 },
                if effects.depth_of_field > 0.0 { dof } else { 0 },
                if effects.light_shafts > 0.0 { shafts } else { 0 },
                self.debug,
            ],
            step: [0.0, 0.0, ease, 0.0],
        };
        let targets = self.targets.as_ref().expect("created above");
        let bloom_sizes: Vec<(u32, u32)> = targets.bloom.iter().map(|t| (t.width(), t.height())).collect();
        let mut slots = vec![base];
        // Slots 1..: bloom down (source texel size), then up.
        for &(w, h) in &bloom_sizes {
            slots.push(Params { step: [1.0 / w as f32, 1.0 / h as f32, 0.0, 0.0], ..base });
        }
        let mut bytes = vec![0u8; slots.len() * STRIDE as usize];
        for (i, slot) in slots.iter().enumerate() {
            bytes[i * STRIDE as usize..i * STRIDE as usize + size_of::<Params>()].copy_from_slice(bytemuck::bytes_of(slot));
        }
        if bytes.len() as u64 > self.params.size() {
            self.params = device.create_buffer(&wgpu::BufferDescriptor {
                label: Some("effects params"),
                size: (bytes.len() as u64).next_power_of_two(),
                usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
                mapped_at_creation: false,
            });
            self.params_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
                label: Some("effects params"),
                layout: &self.params_layout,
                entries: &[
                    wgpu::BindGroupEntry {
                        binding: 0,
                        resource: wgpu::BindingResource::Buffer(wgpu::BufferBinding {
                            buffer: &self.params,
                            offset: 0,
                            size: wgpu::BufferSize::new(size_of::<Params>() as u64),
                        }),
                    },
                    wgpu::BindGroupEntry {
                        binding: 1,
                        resource: wgpu::BindingResource::Sampler(&device.create_sampler(&wgpu::SamplerDescriptor {
                            mag_filter: wgpu::FilterMode::Linear,
                            min_filter: wgpu::FilterMode::Linear,
                            ..Default::default()
                        })),
                    },
                ],
            });
        }
        queue.write_buffer(&self.params, 0, &bytes);

        let t = self.targets.take().expect("created above");
        self.pass(device, encoder, "fs_average", &t.average, Blend::Ease, 0, &[frame.color(), frame.aux()]);
        self.pass(device, encoder, "fs_light", &t.lit, Blend::Replace, 0, &[frame.color(), frame.aux(), &t.shadow, &t.average]);
        if effects.bloom > 0.0 || effects.light_shafts > 0.0 {
            self.pass(device, encoder, "fs_bright", &t.bloom[0], Blend::Replace, 0, &[&t.lit, frame.aux()]);
        }
        if effects.bloom > 0.0 {
            for level in 1..t.bloom.len() {
                self.pass(device, encoder, "fs_down", &t.bloom[level], Blend::Replace, level as u32, &[&t.bloom[level - 1]]);
            }
            for level in (0..t.bloom.len() - 1).rev() {
                self.pass(device, encoder, "fs_up", &t.bloom[level], Blend::Add, level as u32 + 2, &[&t.bloom[level + 1]]);
            }
        }
        if effects.light_shafts > 0.0 {
            self.pass(device, encoder, "fs_shafts", &t.shafts, Blend::Replace, 0, &[&t.bloom[0]]);
        }
        if base.quality[1] > 0 {
            self.pass(device, encoder, "fs_dof", &t.dof, Blend::Replace, 0, &[&t.lit, frame.aux()]);
        }
        self.pass(device, encoder, "fs_composite", &t.output, Blend::Replace, 0, &[&t.lit, frame.aux(), &t.bloom[0], &t.shafts, &t.dof, &t.average]);
        self.targets = Some(t);
        &self.targets.as_ref().expect("restored").output_view
    }

    /// The sprite shadow mask of the last frame (r intensity, g and b the
    /// caster depth), for inspection.
    pub fn shadow_mask(&self) -> Option<&wgpu::Texture> {
        self.targets.as_ref().map(|t| &t.shadow)
    }

    /// The last [`EffectsRenderer::render`] result.
    pub fn output(&self) -> Option<&wgpu::Texture> {
        self.targets.as_ref().map(|t| &t.output)
    }

    /// A view of [`EffectsRenderer::output`].
    pub fn output_view(&self) -> Option<&wgpu::TextureView> {
        self.targets.as_ref().map(|t| &t.output_view)
    }
}

/// A soft ellipse on the floor under a sprite (untextured; uv spans -1..1).
fn contact(vertices: &mut Vec<super::Vertex>, (x, y): (f32, f32), (rx, ry): (f32, f32), depth: f32, strength: f32) {
    let corner = |sx: f32, sy: f32| super::Vertex {
        position: [x + sx * rx, y - 1.0 + sy * ry],
        depth,
        w: 1.0,
        color: [strength, 0.0, 0.0, 1.0],
        uv: [sx, sy],
        mode: [0, 0, 0, 0],
        limits: 0,
    };
    for (sx, sy) in [(-1.0, -1.0), (1.0, -1.0), (-1.0, 1.0), (1.0, -1.0), (1.0, 1.0), (-1.0, 1.0)] {
        vertices.push(corner(sx, sy));
    }
}
