//! The displayed frame: colour and auxiliary textures the effects and the
//! compositor read.
//!
//! Normally the frame is the display area of the scaled VRAM picture. Wider
//! framing and interpolation instead replay the primitives drawn into the
//! displayed buffer since its last clear (the renderer's frame log) into a
//! target of their own:
//! - **Wider framing** widens the target by `side` VRAM pixels on each side
//!   and lets scene primitives draw past the drawing area's left and right
//!   edges there; interface primitives keep the original area. The faces
//!   the game culled against its own screen (traced as revealed) fill the
//!   sides, unless viewport culling is selected.
//! - **Pulling the camera back** moves every projected vertex to where the
//!   game's camera would see it from further back along its own axis (the
//!   same angle): its screen point scales towards the projection centre by
//!   z / (z + d) and its depth grows by d. The interface keeps its place
//!   and size; the revealed faces show wherever they fall.
//! - **Interpolation** shows the newest frame's scene `alpha` of the way
//!   from the frame before, through a camera between the two the game drew
//!   them with (its rotation turned, its target moved): what holds still in
//!   the world stays where it is, and each model and sprite the port
//!   recorded the transform of in both frames moves from where its points
//!   were then ([`Motion`]); the interface moves as the game moves it. It
//!   presents one game frame late and never touches game state.
//! - **Looking around** from within the scene (first-person inspection)
//!   moves every projected vertex into a camera of its own ([`Look`],
//!   [`first_person`]); polygons reaching behind its near plane are clipped
//!   there as they are drawn, and depth is tested whatever the ordering.

use super::diorama::Layers;
use super::{
    AUX_FORMAT, Builder, COLOR_FORMAT, Capture, Copies, DisplayArea, DrawOptions, Near, PASS_RECIPROCAL_DEPTH, PassUniform, Ps1Renderer, Reveal, Storage,
    Stream, Target, VRAM_HEIGHT, VRAM_WIDTH, bind_group, effective_class, encode, overlap, pass_options, picture_usage, samples, scene_offset,
    texture,
};
use glam::{DMat3, DQuat, DVec3};
use std::borrow::Cow;
use std::collections::HashMap;
use std::sync::Arc;
use xem_gpu::{Camera, CameraMode, Class, Eye, Placement, Primitive, Shape, TraceOp, VramRect};

/// How the frame is framed.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Framing {
    /// VRAM pixels added on each side of the display area (wider framing).
    pub side: u32,
    /// Interpolate the newest frame from the frames shown before it at the
    /// host's time (seconds): the camera, models and sprites each move over
    /// the time between their own last two moves ([`interpolate`]).
    pub interpolate: Option<f64>,
    /// How far the camera is pulled back along its axis, in GTE depth
    /// units (0: where the game put it).
    pub pull_back: f32,
    /// Replay the frame even framed as the game framed it (for effects
    /// that need the interface drawn after them: [`FrameRenderer::draw_overlay`]).
    pub replay: bool,
    /// Look around from within the scene instead of the game's camera.
    pub look: Option<Look>,
    /// Whether the game's interface is drawn with its scene, left out, or
    /// drawn alone.
    pub interface: Interface,
    /// Frame the game frame shown last again, not the newest (a headset's
    /// eye through frames that draw no scene, such as a field's menu).
    pub hold: bool,
}

/// The game's interface in a frame: its interface primitives (the compass,
/// windows, portraits, fades), wherever the game drew them among its scene.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Interface {
    /// With the scene, as the game drew it.
    #[default]
    Drawn,
    /// Left out: a headset's eye looking from within the scene, which
    /// would otherwise carry the interface about with the head.
    Omitted,
    /// Alone, over nothing: the scene and the fills that clear it are left
    /// out, so the frame is the interface on a transparent picture (its
    /// coverage as alpha: [`FrameRenderer::pane`]), for a pane where the
    /// virtual screen was.
    Only,
}

/// A camera within the scene (first-person inspection): where it stands and
/// its right, down and forward axes (unit vectors), in the game camera's
/// view space (the space of [`xem_gpu::TraceVertex::view`]).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Look {
    pub eye: [f32; 3],
    pub axes: [[f32; 3]; 3],
    /// The world's up in that space: sprites stand along it.
    pub up: [f32; 3],
    /// The projection, per axis: a point (right, down, forward) of the
    /// camera's space lands at `centre + shift + h * (right, down) /
    /// forward`, `centre` being each primitive's projection centre (VRAM
    /// pixels). `h` is the projection distance across and down (the field
    /// of view; unequal where a headset's eye is fitted onto the frame),
    /// `shift` where the view's axis lies from that centre (an asymmetric
    /// field of view, such as each eye's of a headset).
    pub h: [f32; 2],
    pub shift: [f32; 2],
    /// The near plane's distance (view-space units).
    pub near: f32,
    /// The sprite whose parts are left out (the viewer's own; 0 none).
    pub hide: u32,
    /// What the sprites face: none, the view's plane (they turn as the
    /// view turns); a point (a headset's head, in the game camera's view
    /// space), each sprite turning about the world's up to face it, the
    /// same for both eyes whichever way the head looks.
    pub facing: Option<[f32; 3]>,
}

/// Holds the presented frame.
pub struct FrameRenderer {
    color: wgpu::Texture,
    aux: wgpu::Texture,
    depth: wgpu::Texture,
    color_view: wgpu::TextureView,
    size: (u32, u32),
    area: Option<DisplayArea>,
    scale: u32,
    side: u32,
    uniform: wgpu::Buffer,
    overlay_uniform: wgpu::Buffer,
    vertices: Stream,
    /// The interface the game drew after its scene, held back from the
    /// replay for [`FrameRenderer::draw_overlay`] with the frame's scene
    /// offset (it tells interface projected with another apart), and its
    /// vertices.
    overlay: Vec<TraceOp>,
    overlay_scene: Option<(i16, i16)>,
    overlay_vertices: Stream,
    overlay_staging: Stream,
    /// The newest displayed frame's log identity (buffer, clears) and its
    /// primitives, and the frame before.
    newest: Option<((u16, u16), u64)>,
    newest_ops: Vec<TraceOp>,
    /// Where the field's controlled character stood in the newest frame,
    /// the camera the game drew it with and its models' and sprites'
    /// placements, as the port recorded them ([`xem_gpu::TraceOp::Eye`],
    /// [`xem_gpu::TraceOp::Camera`], [`xem_gpu::TraceOp::Placement`]).
    eye: Option<Eye>,
    camera: Option<Camera>,
    placements: Arc<[Placement]>,
    /// How many of them change the picture (not the native pixels a copy,
    /// fill or drawing left, which only follow what was drawn).
    drawn: usize,
    /// What interpolation knows of the frames shown ([`Moved`]), and what
    /// it knew before the newest arrived with the placements of the frame
    /// before it; the host's time the newest arrived, and the last given.
    moved: Moved,
    prior: (Moved, Arc<[Placement]>),
    arrived: f64,
    time: f64,
    /// Primitives the last replay drew (for the effects), when it replayed.
    replayed: Vec<TraceOp>,
    replaying: bool,
    /// The last replay looked from within the scene: its depth is reversed
    /// ([`super::DepthMode::Nearer`]), for the interface drawn over it.
    reversed: bool,
    /// The native VRAM when the newest frame began (its log's start).
    snapshot: Option<Arc<Vec<u16>>>,
    /// The native VRAM a replay samples ([`ReplayVram`]) and its staged
    /// writes.
    vram: ReplayVram,
    feedback: wgpu::Texture,
    feedback_aux: wgpu::Texture,
    /// Extra rows the game draws below its viewport, which a framebuffer
    /// distortion may bring into view. Cropped only after that effect.
    work: Option<(wgpu::Texture, wgpu::Texture, wgpu::Texture)>,
    staging: Stream,
    /// The replay's bind group, made with the capture texture of a generation.
    group: Option<(u64, wgpu::BindGroup)>,
    overlay_group: Option<(u64, wgpu::BindGroup)>,
    /// The replay's and the overlay's builder buffers.
    storage: Storage,
    overlay_storage: Storage,
    /// The interface alone as a picture to lay over another
    /// ([`FrameRenderer::pane`]), made when first asked for.
    pane: Option<Pane>,
}

/// [`FrameRenderer::pane`]'s pass and picture.
struct Pane {
    pipeline: wgpu::RenderPipeline,
    layout: wgpu::BindGroupLayout,
    texture: wgpu::Texture,
    view: wgpu::TextureView,
}

impl Pane {
    fn new(device: &wgpu::Device) -> Self {
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("frame pane"),
            source: wgpu::ShaderSource::Wgsl(Cow::Borrowed(include_str!("pane.wgsl"))),
        });
        let entry = |binding| wgpu::BindGroupLayoutEntry {
            binding,
            visibility: wgpu::ShaderStages::FRAGMENT,
            ty: wgpu::BindingType::Texture {
                sample_type: wgpu::TextureSampleType::Float { filterable: false },
                view_dimension: wgpu::TextureViewDimension::D2,
                multisampled: false,
            },
            count: None,
        };
        let layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor { label: Some("frame pane"), entries: &[entry(0), entry(1)] });
        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("frame pane"),
            bind_group_layouts: &[Some(&layout)],
            immediate_size: 0,
        });
        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("frame pane"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState { module: &shader, entry_point: Some("vs_pane"), compilation_options: Default::default(), buffers: &[] },
            primitive: Default::default(),
            depth_stencil: None,
            multisample: Default::default(),
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: Some("fs_pane"),
                compilation_options: Default::default(),
                targets: &[Some(wgpu::ColorTargetState { format: COLOR_FORMAT, blend: None, write_mask: wgpu::ColorWrites::ALL })],
            }),
            multiview_mask: None,
            cache: None,
        });
        let texture = texture(device, "frame pane", (1, 1), COLOR_FORMAT, picture_usage());
        Self { pipeline, layout, view: texture.create_view(&Default::default()), texture }
    }
}

/// The native VRAM a replay samples: the frame's snapshot, changed by its
/// writes in order as the replay goes (a texture slot the game fills again
/// later in the frame shows each primitive its own texels), and a copy of
/// what the texture holds once the replay's writes are done (the rows of a
/// snapshot that differ from it are all a replay uploads first).
pub(crate) struct ReplayVram {
    pub(crate) texture: wgpu::Texture,
    held: Vec<u16>,
}

impl ReplayVram {
    pub(crate) fn new(device: &wgpu::Device, label: &str) -> Self {
        let texture = texture(device, label, (VRAM_WIDTH, VRAM_HEIGHT), wgpu::TextureFormat::R16Uint,
            wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST);
        Self { texture, held: vec![0; (VRAM_WIDTH * VRAM_HEIGHT) as usize] }
    }

    /// Sets the texture to `start` before a replay of `ops` (the rows that
    /// differ from what it holds, in runs, through the queue), and notes what
    /// the writes among `ops` leave in it.
    pub(crate) fn start<'a>(&mut self, queue: &wgpu::Queue, start: &[u16], ops: impl Iterator<Item = &'a TraceOp>) {
        let width = VRAM_WIDTH as usize;
        let mut row = 0;
        while row < VRAM_HEIGHT as usize {
            let same = |row: usize| start[row * width..(row + 1) * width] == self.held[row * width..(row + 1) * width];
            if same(row) {
                row += 1;
                continue;
            }
            let first = row;
            while row < VRAM_HEIGHT as usize && !same(row) {
                row += 1;
            }
            let rows = first * width..row * width;
            queue.write_texture(
                wgpu::TexelCopyTextureInfo { texture: &self.texture, mip_level: 0, origin: wgpu::Origin3d { x: 0, y: first as u32, z: 0 }, aspect: wgpu::TextureAspect::All },
                bytemuck::cast_slice(&start[rows.clone()]),
                wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(VRAM_WIDTH * 2), rows_per_image: None },
                wgpu::Extent3d { width: VRAM_WIDTH, height: (row - first) as u32, depth_or_array_layers: 1 },
            );
            self.held[rows.clone()].copy_from_slice(&start[rows]);
        }
        for op in ops {
            if let TraceOp::Write { rect, pixels, .. } = op {
                let (x, w) = (usize::from(rect.x), usize::from(rect.width));
                for (k, line) in pixels.chunks(w.max(1)).enumerate() {
                    let at = (usize::from(rect.y) + k) * width + x;
                    if let Some(target) = self.held.get_mut(at..at + line.len()) {
                        target.copy_from_slice(line);
                    }
                }
            }
        }
    }

    /// The native VRAM as the replay's writes leave it.
    pub(crate) fn held(&self) -> &[u16] {
        &self.held
    }
}

/// A pass uniform of `size_of::<PassUniform>()` bytes.
pub(crate) fn uniform_buffer(device: &wgpu::Device) -> wgpu::Buffer {
    device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("frame replay"),
        size: size_of::<PassUniform>() as u64,
        usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        mapped_at_creation: false,
    })
}

/// Clears a frame's colour and auxiliary targets to transparent black.
fn clear(encoder: &mut wgpu::CommandEncoder, textures: [&wgpu::Texture; 2]) {
    for texture in textures {
        let view = texture.create_view(&Default::default());
        encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("frame clear"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: &view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations { load: wgpu::LoadOp::Clear(wgpu::Color::TRANSPARENT), store: wgpu::StoreOp::Store },
            })],
            ..Default::default()
        });
    }
}

impl FrameRenderer {
    pub fn new(device: &wgpu::Device) -> Self {
        let color = texture(device, "frame", (1, 1), COLOR_FORMAT, picture_usage());
        let aux = texture(device, "frame aux", (1, 1), AUX_FORMAT, picture_usage());
        let depth = texture(device, "frame depth", (1, 1), wgpu::TextureFormat::Depth32Float, wgpu::TextureUsages::RENDER_ATTACHMENT);
        Self {
            color_view: color.create_view(&Default::default()),
            color,
            aux,
            depth,
            size: (1, 1),
            area: None,
            scale: 1,
            side: 0,
            uniform: uniform_buffer(device),
            overlay_uniform: uniform_buffer(device),
            vertices: Stream::new(device, "frame replay vertices", wgpu::BufferUsages::VERTEX),
            overlay: Vec::new(),
            overlay_scene: None,
            overlay_vertices: Stream::new(device, "frame overlay vertices", wgpu::BufferUsages::VERTEX),
            overlay_staging: Stream::new(device, "frame overlay staging", wgpu::BufferUsages::COPY_SRC),
            newest: None,
            newest_ops: Vec::new(),
            eye: None,
            camera: None,
            placements: Arc::default(),
            drawn: 0,
            moved: Moved::default(),
            prior: (Moved::default(), Arc::default()),
            arrived: 0.0,
            time: 0.0,
            replayed: Vec::new(),
            replaying: false,
            reversed: false,
            snapshot: None,
            vram: ReplayVram::new(device, "frame replay vram"),
            feedback: texture(device, "frame feedback", (1, 1), COLOR_FORMAT, picture_usage()),
            feedback_aux: texture(device, "frame feedback aux", (1, 1), AUX_FORMAT, picture_usage()),
            work: None,
            staging: Stream::new(device, "frame replay staging", wgpu::BufferUsages::COPY_SRC),
            group: None,
            overlay_group: None,
            storage: Storage::default(),
            overlay_storage: Storage::default(),
            pane: None,
        }
    }

    fn resize(&mut self, device: &wgpu::Device, size: (u32, u32)) {
        if self.size != size {
            self.color = texture(device, "frame", size, COLOR_FORMAT, picture_usage());
            self.aux = texture(device, "frame aux", size, AUX_FORMAT, picture_usage());
            self.depth = texture(device, "frame depth", size, wgpu::TextureFormat::Depth32Float, wgpu::TextureUsages::RENDER_ATTACHMENT);
            self.color_view = self.color.create_view(&Default::default());
            self.group = None;
            self.overlay_group = None;
            self.size = size;
        }
    }

    /// Notes the displayed buffer's log at the host's time `now` (when
    /// known: the last time given otherwise); returns whether a new game
    /// frame is shown since the last call.
    pub fn track(&mut self, renderer: &Ps1Renderer, area: DisplayArea, now: Option<f64>) -> bool {
        self.time = now.unwrap_or(self.time);
        let Some(log) = renderer.frame_log(area) else {
            // No log of what the shown buffer holds (a trace started again,
            // a state loaded): nothing to replay until its next clear, rather
            // than an older frame's primitives over the new VRAM.
            self.newest = None;
            self.newest_ops.clear();
            self.snapshot = None;
            self.eye = None;
            self.camera = None;
            self.placements = Arc::default();
            self.moved = Moved::default();
            self.prior = (Moved::default(), Arc::default());
            self.drawn = 0;
            return false;
        };
        let identity = ((log.rect.x, log.rect.y), log.frames);
        if self.newest == Some(identity) {
            // The same frame, perhaps grown (a frame drawn over several
            // takes): a log only grows until its buffer is cleared, and
            // takes its records with its first primitive.
            if self.camera != log.camera || !Arc::ptr_eq(&self.placements, &log.placements) {
                (self.camera, self.placements) = (log.camera, log.placements.clone());
                self.moved = self.prior.0.next(&self.prior.1, self.camera.as_ref(), &self.placements, self.arrived);
            }
            self.eye = log.eye;
            let known = self.newest_ops.len();
            if log.ops.len() > known {
                self.newest_ops.extend_from_slice(&log.ops[known..]);
                self.drawn += drawn(&log.ops[known..], log.rect);
            } else if log.ops.len() < known {
                self.newest_ops.clone_from(&log.ops);
                self.drawn = drawn(&self.newest_ops, log.rect);
            }
            return false;
        }
        self.newest_ops.clone_from(&log.ops);
        self.drawn = drawn(&self.newest_ops, log.rect);
        self.newest = Some(identity);
        self.snapshot = Some(log.start.clone());
        self.eye = log.eye;
        self.camera = log.camera;
        self.arrived = self.time;
        self.prior = (std::mem::take(&mut self.moved), std::mem::replace(&mut self.placements, log.placements.clone()));
        self.moved = self.prior.0.next(&self.prior.1, self.camera.as_ref(), &self.placements, self.arrived);
        true
    }

    /// Whether interpolation still moves the newest frame's scene at the
    /// host's time `now`.
    pub(crate) fn moving(&self, now: f64) -> bool {
        self.moved.moving(now)
    }

    /// Where the field's controlled character stood in the newest frame
    /// shown, when the port recorded it (while a view looks around).
    pub fn eye(&self) -> Option<Eye> {
        self.eye
    }

    /// The camera the game drew the newest frame shown with, when the port
    /// recorded it (a field, a battle or the world map).
    pub fn camera(&self) -> Option<Camera> {
        self.camera
    }

    /// How many of the newest frame's ops change its picture.
    pub fn drawn(&self) -> usize {
        self.drawn
    }

    /// The native VRAM when the newest frame began (its log's start).
    pub(crate) fn snapshot(&self) -> Option<&Arc<Vec<u16>>> {
        self.snapshot.as_ref()
    }

    /// Takes `area` of the renderer's picture as the frame, replayed when
    /// `framing` asks for wider framing or interpolation (the newest frame
    /// the renderer logged, unless `framing` holds the one shown before:
    /// `area` is then that frame's).
    pub fn prepare(
        &mut self,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        encoder: &mut wgpu::CommandEncoder,
        renderer: &mut Ps1Renderer,
        area: DisplayArea,
        framing: Framing,
    ) {
        let scale = renderer.scale();
        if !framing.hold || self.newest_ops.is_empty() {
            self.track(renderer, area, framing.interpolate);
        }
        // The game may draw a few rows below the displayed viewport (the
        // field draws 224 and shows 216); its distortion copies those rows.
        let rect = renderer.frame_log(area).map(|log| log.rect)
            .unwrap_or(VramRect { x: area.x as u16, y: area.y as u16, width: area.width as u16, height: area.height as u16 });
        let feedback = renderer.options().fidelity != super::Fidelity::ORIGINAL && framebuffer_effect(&self.newest_ops, rect);
        let pulled = framing.pull_back > 0.0;
        // The interface alone has no scene to look at.
        let look = framing.look.filter(|_| framing.interface != Interface::Only);
        let replay = (framing.side > 0 || framing.interpolate.is_some() || pulled || look.is_some() || framing.replay || feedback
            || framing.interface != Interface::Drawn)
            && !self.newest_ops.is_empty();
        let side = if replay { framing.side } else { 0 };
        self.resize(device, ((area.width.max(1) + 2 * side) * scale, area.height.max(1) * scale));
        self.area = Some(area);
        self.scale = scale;
        self.side = side;
        self.replaying = replay;
        self.overlay.clear();
        self.reversed = false;
        if !replay {
            // The display as it is; or, without the interface or with it
            // alone, nothing while there is nothing to replay (no log of
            // what the shown buffer holds: a state loaded, a rewind's step),
            // since the display holds both as the game's camera saw them.
            match framing.interface {
                Interface::Drawn => renderer.copy_display(encoder, area, &self.color, &self.aux),
                Interface::Omitted | Interface::Only => clear(encoder, [&self.color, &self.aux]),
            }
            return;
        }
        let render_height = if feedback { u32::from(rect.height).max(area.height) } else { area.height };
        let render_size = (self.size.0, render_height * scale);
        if feedback && (self.feedback.size().width != render_size.0 || self.feedback.size().height != render_size.1) {
            self.feedback = texture(device, "frame feedback", render_size, COLOR_FORMAT, picture_usage());
            self.feedback_aux = texture(device, "frame feedback aux", render_size, AUX_FORMAT, picture_usage());
            self.group = None;
            self.overlay_group = None;
        }
        let extra = render_height > area.height;
        if extra && self.work.as_ref().is_none_or(|(color, _, _)| (color.width(), color.height()) != render_size) {
            self.work = Some((
                texture(device, "frame hidden rows", render_size, COLOR_FORMAT, picture_usage()),
                texture(device, "frame hidden rows aux", render_size, AUX_FORMAT, picture_usage()),
                texture(device, "frame hidden rows depth", render_size, super::DEPTH_FORMAT, wgpu::TextureUsages::RENDER_ATTACHMENT),
            ));
        }
        let (color, aux, depth) = match self.work.as_ref().filter(|_| extra) {
            Some((color, aux, depth)) => (color.clone(), aux.clone(), depth.clone()),
            None => (self.color.clone(), self.aux.clone(), self.depth.clone()),
        };
        // The last replay's buffer, refilled (moved between the frames when
        // interpolating).
        let mut ops = std::mem::take(&mut self.replayed);
        ops.clone_from(&self.newest_ops);
        let motion = framing.interpolate.and_then(|now| self.moved.motion(now));
        if let Some(motion) = &motion {
            motion.apply(&mut ops);
        }
        // A frame held from before the screen the game took last (a field's
        // menu takes it) drew nothing of it.
        let current = !framing.hold;
        let captures: Vec<_> = std::iter::once(&renderer.capture).chain(&renderer.earlier).filter(|capture| current && capture.scale == scale)
            .flat_map(Capture::areas).collect();
        settle_copies(&mut ops, &captures, feedback.then_some(rect));
        // What the game draws of a screen it took (its pieces, zoomed,
        // shattered) is that screen, as presented then: a screen effect,
        // where the game put it.
        let capture = renderer.capture.rect.filter(|_| current && renderer.capture.scale == scale).map(|rect| (rect, renderer.capture.side));
        if let Some((rect, _)) = capture {
            for op in &mut ops {
                if let TraceOp::Draw(p) = op
                    && (samples(p, &rect) || renderer.capture.aliases.iter().any(|(r, _)| samples(p, r)))
                {
                    p.class = Class::Flat;
                }
            }
        }
        if let Some(look) = &look {
            first_person_layered(&mut ops, look, self.newest.map(|frame| (&mut renderer.plane_layers, frame)));
        } else if pulled {
            pull_back(&mut ops, framing.pull_back);
        }
        self.overlay_scene = scene_offset(&ops);
        self.overlay = split_overlay(&mut ops, feedback.then_some(rect));
        // The interface left out wherever the game drew it, or kept alone
        // (with the writes and copies its textures follow).
        let scene = self.overlay_scene;
        match framing.interface {
            Interface::Drawn => {}
            Interface::Omitted => {
                ops.retain(|op| !matches!(op, TraceOp::Draw(p) if !effective_class(p, scene).is_scene()));
                self.overlay.clear();
            }
            Interface::Only => ops.retain(|op| match op {
                TraceOp::Draw(p) => !effective_class(p, scene).is_scene(),
                TraceOp::Fill { .. } => false,
                _ => true,
            }),
        }
        // The replay samples VRAM as it was when the frame began, then as the
        // frame's writes (the replay's and the overlay's) changed it, in order.
        if let Some(start) = &self.snapshot {
            self.vram.start(queue, start, ops.iter().chain(&self.overlay));
        }
        let options: DrawOptions = *renderer.options();
        let mut builder = Builder::reusing(&options, &ops, &mut self.storage);
        builder.shown = Some(VramRect { x: area.x as u16, y: area.y as u16, width: area.width as u16, height: area.height as u16 });
        builder.seal = renderer.seal;
        builder.spill = side;
        builder.capture = capture;
        builder.capture_aliases = if current { &renderer.capture.aliases } else { &[] };
        builder.feedback = feedback.then_some(rect);
        builder.vram = self.snapshot.is_some().then_some(self.vram.held());
        // A frame recorded looking from within or around the scene holds
        // faces from anywhere, not only beside the screen: seen from the
        // game's camera, none. Between two frames, those beside the screen
        // come into it as the camera turns.
        let around = self.eye.is_some() || self.camera.is_some_and(|camera| camera.around != 0);
        builder.reveal = if pulled || look.is_some() {
            Reveal::Everywhere
        } else if around {
            Reveal::Hidden
        } else if motion.is_some() {
            Reveal::Everywhere
        } else if side > 0 {
            Reveal::Sides
        } else {
            Reveal::Hidden
        };
        builder.near = look.map(|look| Near { distance: look.near, h: look.h, shift: look.shift });
        builder.reversed = look.is_some();
        self.reversed = look.is_some();
        for op in &ops {
            builder.op(op);
        }
        let origin = (area.x as f32 - side as f32, area.y as f32);
        let (capture_scale, capture_side, capture) = renderer.capture.uniform();
        let uniform = PassUniform {
            origin: [origin.0, origin.1],
            extent: [(area.width + 2 * side) as f32, render_height as f32],
            scale: scale as f32,
            options: pass_options(&options.fidelity) | if look.is_some() { PASS_RECIPROCAL_DEPTH } else { 0 },
            capture_scale,
            capture_side,
            capture,
            ..bytemuck::Zeroable::zeroed()
        };
        queue.write_buffer(&self.uniform, 0, bytemuck::bytes_of(&uniform));
        let overlay_uniform = PassUniform { extent: [(area.width + 2 * side) as f32, area.height as f32], ..uniform };
        queue.write_buffer(&self.overlay_uniform, 0, bytemuck::bytes_of(&overlay_uniform));
        self.vertices.write(device, queue, bytemuck::cast_slice(&builder.vertices));
        self.staging.write(device, queue, &builder.staging);
        // Start from black (the replayed clear covers what the game cleared),
        // at the far plane (0 where depth is reversed).
        clear(encoder, [&color, &aux]);
        let far = if look.is_some() { 0.0 } else { 1.0 };
        let depth_view = depth.create_view(&Default::default());
        let visible_depth_view = self.depth.create_view(&Default::default());
        for view in std::iter::once(&depth_view).chain(extra.then_some(&visible_depth_view)) {
            encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("frame depth clear"),
            color_attachments: &[],
            depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                view,
                depth_ops: Some(wgpu::Operations { load: wgpu::LoadOp::Clear(far), store: wgpu::StoreOp::Store }),
                stencil_ops: None,
            }),
            ..Default::default()
            });
        }
        if self.group.as_ref().is_some_and(|(generation, _)| *generation != renderer.capture.generation) {
            self.group = None;
        }
        let generation = renderer.capture.generation;
        if self.overlay_group.as_ref().is_some_and(|(known, _)| *known != generation) {
            self.overlay_group = None;
        }
        self.overlay_group.get_or_insert_with(|| (generation,
            bind_group(device, &renderer.pipelines.layout, &self.overlay_uniform, &self.vram.texture, &renderer.capture, Some((&self.feedback, &self.feedback_aux)))));
        let group = self
            .group
            .get_or_insert_with(|| (generation, bind_group(device, &renderer.pipelines.layout, &self.uniform, &self.vram.texture, &renderer.capture, Some((&self.feedback, &self.feedback_aux)))))
            .1
            .clone();
        let (picture_color, picture_aux) = (renderer.color.clone(), renderer.aux.clone());
        let target = Target { color: &color, aux: &aux, depth: &depth_view, origin, scale };
        encode(
            device,
            &mut renderer.pipelines,
            encoder,
            &group,
            self.vertices.buffer(),
            Some((self.staging.buffer(), &self.vram.texture)),
            Copies::From { color: &picture_color, aux: &picture_aux, scale, capture: Some(&renderer.capture), earlier: &renderer.earlier, scratch: &renderer.scratch, feedback: &self.feedback, feedback_aux: &self.feedback_aux },
            &target,
            &builder.commands,
        );
        builder.store(&mut self.storage);
        if extra {
            for (from, to) in [(&color, &self.color), (&aux, &self.aux)] {
                encoder.copy_texture_to_texture(from.as_image_copy(), to.as_image_copy(), to.size());
            }
        }
        self.replayed = ops;
    }

    /// Draws the interface the game drew after its scene (held back by the
    /// last [`FrameRenderer::prepare`]: the compass, windows, fades) over
    /// `color` (this frame's colour or what effects made of it; the same
    /// size), so effects keep off it and its translucent pixels show the
    /// finished scene, and culled faces drawn late stay beneath it.
    pub fn draw_overlay(
        &mut self,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        encoder: &mut wgpu::CommandEncoder,
        renderer: &mut Ps1Renderer,
        color: Option<&wgpu::Texture>,
    ) {
        let (Some(area), Some((_, group))) = (self.area, self.overlay_group.clone()) else { return };
        if self.overlay.is_empty() {
            return;
        }
        let options: DrawOptions = *renderer.options();
        let mut builder = Builder::reusing(&options, &self.overlay, &mut self.overlay_storage);
        builder.scene = self.overlay_scene;
        builder.reversed = self.reversed;
        builder.shown = Some(VramRect { x: area.x as u16, y: area.y as u16, width: area.width as u16, height: area.height as u16 });
        builder.spill = self.side;
        builder.capture = renderer.capture.rect.filter(|_| renderer.capture.scale == self.scale).map(|rect| (rect, renderer.capture.side));
        builder.capture_aliases = &renderer.capture.aliases;
        for op in &self.overlay {
            builder.op(op);
        }
        self.overlay_vertices.write(device, queue, bytemuck::cast_slice(&builder.vertices));
        self.overlay_staging.write(device, queue, &builder.staging);
        let color = color.unwrap_or(&self.color).clone();
        let depth_view = self.depth.create_view(&Default::default());
        let origin = (area.x as f32 - self.side as f32, area.y as f32);
        let target = Target { color: &color, aux: &self.aux, depth: &depth_view, origin, scale: self.scale };
        let (picture_color, picture_aux) = (renderer.color.clone(), renderer.aux.clone());
        let copies = Copies::From { color: &picture_color, aux: &picture_aux, scale: self.scale, capture: Some(&renderer.capture), earlier: &renderer.earlier, scratch: &renderer.scratch, feedback: &self.feedback, feedback_aux: &self.feedback_aux };
        let staging = Some((self.overlay_staging.buffer(), &self.vram.texture));
        encode(device, &mut renderer.pipelines, encoder, &group, self.overlay_vertices.buffer(), staging, copies, &target, &builder.commands);
        builder.store(&mut self.overlay_storage);
    }

    /// The frame as a picture to lay over another, its coverage as alpha
    /// (premultiplied; the colours sRGB-encoded as the frame's): for a frame
    /// drawn with [`Interface::Only`], its interface where the screen was.
    /// It covers what lies behind where the interface drew, by half where
    /// that was semi-transparent (the original's commonest blend lets half
    /// of what lies behind through; one over another interface pixel is
    /// taken the same way), adds the colour of a semi-transparent effect
    /// over the whole screen (which leaves no class, as an additive flash),
    /// and covers nothing elsewhere. The frame's alpha is the PS1's mask
    /// bit, not coverage: its auxiliary target's class and marks tell what
    /// was drawn (ps1/pane.wgsl).
    pub fn pane(&mut self, device: &wgpu::Device, encoder: &mut wgpu::CommandEncoder) -> &wgpu::TextureView {
        let size = self.size;
        let pane = self.pane.get_or_insert_with(|| Pane::new(device));
        if (pane.texture.width(), pane.texture.height()) != size {
            pane.texture = texture(device, "frame pane", size, COLOR_FORMAT, picture_usage());
            pane.view = pane.texture.create_view(&Default::default());
        }
        let group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("frame pane"),
            layout: &pane.layout,
            entries: &[
                wgpu::BindGroupEntry { binding: 0, resource: wgpu::BindingResource::TextureView(&self.color_view) },
                wgpu::BindGroupEntry { binding: 1, resource: wgpu::BindingResource::TextureView(&self.aux.create_view(&Default::default())) },
            ],
        });
        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("frame pane"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: &pane.view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations { load: wgpu::LoadOp::Clear(wgpu::Color::TRANSPARENT), store: wgpu::StoreOp::Store },
            })],
            ..Default::default()
        });
        pass.set_pipeline(&pane.pipeline);
        pass.set_bind_group(0, &group, &[]);
        pass.draw(0..3, 0..1);
        drop(pass);
        &pane.view
    }

    /// The picture the last [`FrameRenderer::pane`] made.
    pub fn pane_texture(&self) -> Option<&wgpu::Texture> {
        self.pane.as_ref().map(|pane| &pane.texture)
    }

    pub fn pane_view(&self) -> Option<&wgpu::TextureView> {
        self.pane.as_ref().map(|pane| &pane.view)
    }

    /// The identity of the newest frame shown (its buffer's position and
    /// clears), as [`Ps1Renderer::log_identities`] gives them.
    pub fn identity(&self) -> Option<((u16, u16), u64)> {
        self.newest
    }

    /// The frame's size in pixels.
    pub fn size(&self) -> (u32, u32) {
        self.size
    }

    /// The display area the frame shows (without wider framing's sides), its
    /// scale and the sides' width in VRAM pixels.
    pub fn area(&self) -> Option<(DisplayArea, u32)> {
        self.area.map(|area| (area, self.scale))
    }

    /// VRAM pixels of wider framing on each side of the display area.
    pub fn side(&self) -> u32 {
        self.side
    }

    /// The primitives drawn into the frame (for the effects).
    pub fn primitives(&self) -> &[TraceOp] {
        if self.replaying { &self.replayed } else { &self.newest_ops }
    }

    /// The primitives of the newest game frame shown, as the game drew them.
    pub fn newest(&self) -> &[TraceOp] {
        &self.newest_ops
    }

    pub fn color(&self) -> &wgpu::Texture {
        &self.color
    }

    pub fn color_view(&self) -> &wgpu::TextureView {
        &self.color_view
    }

    pub fn aux(&self) -> &wgpu::Texture {
        &self.aux
    }
}

/// The ops that change a frame's picture: all but the native pixels a copy,
/// fill or drawing left.
/// How many of `ops` (the log of buffer `rect`) change its picture: all but
/// the mirrors of what was drawn and transfers elsewhere in VRAM (textures,
/// a saved screen's place given back while the frame is still shown).
fn drawn(ops: &[TraceOp], rect: VramRect) -> usize {
    ops.iter().filter(|op| match op {
        TraceOp::Write { upload: false, .. } => false,
        TraceOp::Write { rect: written, .. } => overlap(written, &rect) > 0,
        _ => true,
    }).count()
}

fn center(p: &Primitive) -> (f32, f32) {
    let count = p.vertex_count() as f32;
    let vertices = &p.vertices[..p.vertex_count()];
    (vertices.iter().map(|v| v.fx).sum::<f32>() / count, vertices.iter().map(|v| v.fy).sum::<f32>() / count)
}

/// A map of view-space points ([`xem_gpu::TraceVertex::view`]): `m · p + t`.
#[derive(Clone, Copy, Debug)]
struct Affine {
    m: DMat3,
    t: DVec3,
}

impl Affine {
    /// A recorded camera's or placement's transform: `axes · p / 4096 +
    /// translation`.
    fn of(axes: &[[f32; 3]; 3], translation: [f32; 3]) -> Affine {
        let column = |k: usize| DVec3::from(axes[k].map(f64::from)) / 4096.0;
        Affine { m: DMat3::from_cols(column(0), column(1), column(2)), t: DVec3::from(translation.map(f64::from)) }
    }

    /// This map, then `after`.
    fn then(self, after: Affine) -> Affine {
        Affine { m: after.m * self.m, t: after.m * self.t + after.t }
    }

    fn inverse(self) -> Option<Affine> {
        (self.m.determinant().abs() > 1e-12).then(|| {
            let m = self.m.inverse();
            Affine { m, t: -(m * self.t) }
        })
    }

    /// `alpha` of the way from this map to `to`, point by point.
    fn lerp(self, to: Affine, alpha: f64) -> Affine {
        Affine { m: self.m * (1.0 - alpha) + to.m * alpha, t: self.t.lerp(to.t, alpha) }
    }

    fn apply(self, p: DVec3) -> DVec3 {
        self.m * p + self.t
    }

    /// Its rows, each with its translation, for the vertices.
    fn rows(self) -> [[f32; 4]; 3] {
        std::array::from_fn(|i| {
            let row = self.m.row(i);
            [row.x as f32, row.y as f32, row.z as f32, self.t[i] as f32]
        })
    }
}

/// A view-space point through [`Affine::rows`].
fn mapped(rows: &[[f32; 4]; 3], p: [f32; 3]) -> [f32; 3] {
    rows.map(|r| r[0] * p[0] + r[1] * p[1] + r[2] * p[2] + r[3])
}

/// The most a camera turns in a game frame (radians): more is a cut.
const MAX_TURN: f64 = std::f64::consts::FRAC_PI_4;

/// A recorded camera as interpolation takes it: its map of the game's world
/// to its view (the world map's world placed at its origin), that map's
/// rotation (of the mirrored world, `flip`, where it mirrors), scale and
/// what is left of it besides them (its rounding), and the points of the
/// world it looks at and stands at.
#[derive(Clone, Copy, Debug)]
struct View {
    mode: CameraMode,
    affine: Affine,
    rotation: DQuat,
    scale: f64,
    flip: bool,
    rest: DMat3,
    target: DVec3,
    eye: DVec3,
}

impl View {
    fn of(camera: &Camera) -> Option<View> {
        let relative = Affine::of(&camera.axes, camera.translation);
        let origin = DVec3::from(camera.origin.map(f64::from));
        let affine = Affine { m: relative.m, t: relative.t - relative.m * origin };
        let determinant = affine.m.determinant();
        let scale = determinant.abs().cbrt();
        if !(scale > 1e-6) {
            return None;
        }
        let flip = determinant < 0.0;
        let mut turn = affine.m * (1.0 / scale);
        if flip {
            turn.z_axis = -turn.z_axis;
        }
        let rotation = DQuat::from_mat3(&turn).normalize();
        let rest = affine.m - turned(rotation, scale, flip);
        let target = DVec3::from(camera.target.map(f64::from)) + origin;
        Some(View { mode: camera.mode, affine, rotation, scale, flip, rest, target, eye: affine.inverse()?.apply(DVec3::ZERO) })
    }

    /// Whether the game cut from this camera to `to` rather than moved it:
    /// to another mode or a mirrored world, or by more than a game frame's
    /// turn ([`MAX_TURN`]), a quarter of its scale, or the distance from
    /// where it stands to what it looks at, which it or that point moved.
    fn cut_to(&self, to: &View) -> bool {
        let reach = to.eye.distance(to.target).max(1.0);
        self.mode != to.mode
            || self.flip != to.flip
            || self.rotation.angle_between(to.rotation) > MAX_TURN
            || !(0.8..1.25).contains(&(to.scale / self.scale))
            || self.eye.distance(to.eye) > reach
            || self.target.distance(to.target) > reach
    }

    /// The camera `alpha` of the way to `to` (each itself at its end): its
    /// rotation turned and its scale changed that far, and what it looks at
    /// moved that far in the world and in its view, so that it turns about
    /// that point as the game's camera does.
    fn between(&self, to: &View, alpha: f64) -> Affine {
        let scale = self.scale + (to.scale - self.scale) * alpha;
        let m = turned(self.rotation.slerp(to.rotation, alpha), scale, to.flip) + self.rest * (1.0 - alpha) + to.rest * alpha;
        let target = self.target.lerp(to.target, alpha);
        let seen = self.affine.apply(self.target).lerp(to.affine.apply(to.target), alpha);
        Affine { m, t: seen - m * target }
    }
}

/// A rotation scaled, of the mirrored world when `flip`.
fn turned(rotation: DQuat, scale: f64, flip: bool) -> DMat3 {
    let mut m = DMat3::from_quat(rotation) * scale;
    if flip {
        m.z_axis = -m.z_axis;
    }
    m
}

/// Whether two placements are the same transform of the same thing.
fn same(a: &Placement, b: &Placement) -> bool {
    (a.owner, a.packets, a.axes, a.translation) == (b.owner, b.packets, b.axes, b.translation)
}

/// The placement of the frame before that each of `newest` follows: the
/// same sprite's, or the same model instance's, whose draws alternate
/// between two packet buffers a buffer's size apart (the same one where its
/// subsystem did not flip them) and agree, for each model, on which way
/// they stepped; none for one not drawn then, or drawn twice differently.
fn partners<'a>(newest: &[Placement], previous: &'a [Placement]) -> Vec<Option<&'a Placement>> {
    let mut sprites: HashMap<u32, Option<&Placement>> = HashMap::new();
    let mut buffers: HashMap<u32, Option<&Placement>> = HashMap::new();
    for q in previous {
        let (table, key) = if q.packets == 0 { (&mut sprites, q.owner) } else { (&mut buffers, q.packets) };
        table.entry(key).and_modify(|known| {
            if known.is_some_and(|k| !same(k, q)) {
                *known = None;
            }
        }).or_insert(Some(q));
    }
    let buffer = |p: &Placement, at: u32| buffers.get(&at).copied().flatten().filter(|q| q.owner == p.owner);
    // Which way each model's buffers stepped, from the draws only one way
    // finds one for; for a model none tells, which way most did.
    let mut steps: HashMap<u32, [u32; 2]> = HashMap::new();
    let mut all = [0u32; 2];
    for p in newest.iter().filter(|p| p.packets != 0 && buffer(p, p.packets).is_none()) {
        let (back, on) = (buffer(p, p.packets.wrapping_sub(p.size)).is_some(), buffer(p, p.packets.wrapping_add(p.size)).is_some());
        if back != on {
            steps.entry(p.owner).or_default()[usize::from(on)] += 1;
            all[usize::from(on)] += 1;
        }
    }
    let way = |counts: [u32; 2]| (counts[0] != counts[1]).then_some(counts[1] > counts[0]);
    newest
        .iter()
        .map(|p| match p.packets {
            0 => sprites.get(&p.owner).copied().flatten(),
            packets => buffer(p, packets).or_else(|| {
                let on = steps.get(&p.owner).copied().and_then(way).or_else(|| way(all))?;
                buffer(p, if on { packets.wrapping_add(p.size) } else { packets.wrapping_sub(p.size) })
            }),
        })
        .collect()
}

/// When a moving thing of the frames shown moved last: the host's time the
/// frame it moved in was first shown, and the time between its last two
/// moves. Each thing moves at a pace of its own: a battle draws every
/// display frame while its camera and Gears move every other one and its
/// effects every one.
#[derive(Clone, Copy, Debug)]
struct Clock {
    since: f64,
    interval: f64,
}

impl Clock {
    /// A thing first shown at `now`.
    fn new(now: f64) -> Clock {
        Clock { since: now, interval: 1.0 / 30.0 }
    }

    /// Its move at `now`: over the time since its move before, 1 to 4
    /// vertical blanks; after a longer rest, over the time its moves took
    /// before it.
    fn moved(self, now: f64) -> Clock {
        let gap = now - self.since;
        Clock { since: now, interval: if gap <= 4.0 / 60.0 { gap.max(1.0 / 60.0) } else { self.interval } }
    }

    /// How far its last move has gone at `now`, 0 to 1.
    fn alpha(self, now: f64) -> f64 {
        ((now - self.since) / self.interval).clamp(0.0, 1.0)
    }
}

/// A moving thing as interpolation follows it: where it is in the newest
/// frame shown, where it was before its last move (none for one shown
/// first, after a cut, or one moved further than the camera stands from
/// what it looks at: another thing drawn into its memory), and when that
/// move was ([`Clock`]).
#[derive(Clone, Copy, Debug)]
struct Track<T> {
    now: T,
    from: Option<T>,
    clock: Clock,
}

/// A placement of the newest frame shown: its view-space points to its own
/// space, and its own space's place in the world.
#[derive(Clone, Copy, Debug)]
struct Placed {
    local: Affine,
    track: Track<Affine>,
}

/// What interpolation knows of the frames shown, carried from each to the
/// next ([`Moved::next`]): the camera, the newest frame's view-space points
/// to the world, and each of its placements by its number from `first`.
#[derive(Clone, Debug, Default)]
struct Moved {
    camera: Option<Track<View>>,
    world: Option<Affine>,
    first: u32,
    placed: Vec<Option<Placed>>,
}

impl Moved {
    /// The frame drawn with `camera` and `placements`, first shown at `now`,
    /// after this one, whose frame placed `before`: what did not move keeps
    /// its last move and when it was, what moved moves from where it was.
    fn next(&self, before: &[Placement], camera: Option<&Camera>, placements: &[Placement], now: f64) -> Moved {
        let Some(view) = camera.and_then(View::of) else { return Moved::default() };
        let held = self.camera.is_some_and(|track| track.now.affine.m == view.affine.m && track.now.affine.t == view.affine.t);
        let camera = match self.camera {
            Some(track) if held => Track { now: view, ..track },
            Some(track) => Track { now: view, from: (!track.now.cut_to(&view)).then_some(track.now), clock: track.clock.moved(now) },
            None => Track { now: view, from: None, clock: Clock::new(now) },
        };
        let Some(world) = view.affine.inverse() else { return Moved::default() };
        let reach = view.eye.distance(view.target).max(1.0);
        let first = placements.first().map_or(0, |p| p.seq);
        // Placements are numbered as recorded: a frame's run from its first.
        let span = placements.last().map_or(0, |p| p.seq.wrapping_sub(first) as usize + 1);
        let mut placed = vec![None; if span <= 2 * placements.len() + 64 { span } else { 0 }];
        for (p, q) in placements.iter().zip(partners(placements, before)) {
            let transform = Affine::of(&p.axes, p.translation);
            let (Some(local), Some(slot)) = (transform.inverse(), placed.get_mut(p.seq.wrapping_sub(first) as usize)) else { continue };
            let here = transform.then(world);
            let was = q.and_then(|q| self.placed.get(q.seq.wrapping_sub(self.first) as usize).copied().flatten().map(|placed| (q, placed.track)));
            let track = match was {
                Some((q, track)) if held && (q.axes, q.translation) == (p.axes, p.translation) => Track { now: here, ..track },
                Some((_, track)) => {
                    let near = here.apply(DVec3::ZERO).distance(track.now.apply(DVec3::ZERO)) <= reach;
                    Track { now: here, from: near.then_some(track.now), clock: track.clock.moved(now) }
                }
                None => Track { now: here, from: None, clock: Clock::new(now) },
            };
            *slot = Some(Placed { local, track });
        }
        Moved { camera: Some(camera), world: Some(world), first, placed }
    }

    /// Whether anything still moves at `now`.
    fn moving(&self, now: f64) -> bool {
        let moving = |from: bool, clock: Clock| from && clock.alpha(now) < 1.0;
        self.camera.is_some_and(|camera| moving(camera.from.is_some(), camera.clock))
            || self.placed.iter().flatten().any(|placed| moving(placed.track.from.is_some(), placed.track.clock))
    }

    /// How the newest frame's scene moves at `now`: none where nothing
    /// still moves.
    fn motion(&self, now: f64) -> Option<Motion> {
        if !self.moving(now) {
            return None;
        }
        let (camera, world) = (self.camera.as_ref()?, self.world?);
        let turning = camera.from.map(|from| (from, camera.clock.alpha(now))).filter(|&(_, alpha)| alpha < 1.0);
        let moving = |placed: &Placed| placed.track.from.map(|from| (from, placed.track.clock.alpha(now))).filter(|&(_, alpha)| alpha < 1.0);
        let between = turning.map_or(camera.now.affine, |(from, alpha)| from.between(&camera.now, alpha));
        Some(Motion {
            still: world.then(between).rows(),
            first: self.first,
            placed: self.placed.iter().map(|placed| {
                let placed = placed.as_ref()?;
                let (from, alpha) = moving(placed)?;
                Some(placed.local.then(from).lerp(world, alpha).then(between).rows())
            }).collect(),
        })
    }
}

/// How interpolation moves the newest frame's scene ([`Moved::motion`]):
/// maps of its view-space points to the view of the camera in between, for
/// what holds still in the world and for each model and sprite still moving
/// (by its placement in the newest, from `first`).
struct Motion {
    still: [[f32; 4]; 3],
    first: u32,
    placed: Vec<Option<[[f32; 4]; 3]>>,
}

impl Motion {
    fn map(&self, placement: u32) -> &[[f32; 4]; 3] {
        placement.checked_sub(self.first).and_then(|i| self.placed.get(i as usize)).and_then(Option::as_ref).unwrap_or(&self.still)
    }

    /// `ops`' scene moved: each projected point (and a sprite's floor point)
    /// taken through its map and its sub-pixel point and depth moved by as
    /// much as its projection about the projection centre; a primitive with
    /// a point without depth, or one at or behind the camera's plane, stays
    /// as drawn, and so does the interface.
    fn apply(&self, ops: &mut [TraceOp]) {
        let scene = scene_offset(ops);
        for op in ops {
            let TraceOp::Draw(p) = op else { continue };
            let Some((_, h)) = p.projection else { continue };
            if !effective_class(p, scene).is_scene() {
                continue;
            }
            let (map, h, count) = (self.map(p.placement), f32::from(h), p.vertex_count());
            let mut points = [([0.0f32; 3], [0.0f32; 3]); 4];
            let movable = (0..count).all(|i| {
                let Some(q) = view_point(p, i) else { return false };
                let m = mapped(map, q);
                points[i] = (q, m);
                q[2] >= 1.0 && m[2] >= 1.0
            });
            if !movable {
                continue;
            }
            for (v, (q, m)) in p.vertices.iter_mut().zip(points).take(count) {
                v.fx += h * (m[0] / m[2] - q[0] / q[2]);
                v.fy += h * (m[1] / m[2] - q[1] / q[2]);
                v.x = whole(v.fx);
                v.y = whole(v.fy);
                if v.z > 0.0 {
                    v.z = (v.z + m[2] - q[2]).max(1.0);
                }
                if v.view.is_some() {
                    v.view = Some(m);
                }
            }
            if let Some([x, y, z]) = p.ground.filter(|ground| ground[2] >= 1.0) {
                let q = unproject(p, (x, y, z));
                let m = mapped(map, q);
                if m[2] >= 1.0 {
                    p.ground = Some([x + h * (m[0] / m[2] - q[0] / q[2]), y + h * (m[1] / m[2] - q[1] / q[2]), (z + m[2] - q[2]).max(1.0)]);
                }
            }
        }
    }
}

/// `ops`, the newest of `frames`' (each first shown at its time, drawn with
/// its camera and placements), moved as interpolation shows them at the
/// host's time `now` ([`Motion`]); false, leaving them as drawn, where
/// nothing moves then.
pub fn interpolate(ops: &mut [TraceOp], frames: &[(f64, &Camera, &[Placement])], now: f64) -> bool {
    let mut moved = Moved::default();
    let mut before: &[Placement] = &[];
    for &(time, camera, placements) in frames {
        moved = moved.next(before, Some(camera), placements, time);
        before = placements;
    }
    let Some(motion) = moved.motion(now) else { return false };
    motion.apply(ops);
    true
}

/// Points of the scene that the GTE placed within half a native pixel of a
/// point of another primitive of the same game frame, at nearly the same
/// depth, become that point (the first drawn; its sub-pixel position and
/// point before the division): neighbouring pieces of scenery, each placed
/// by its own matrix, meet without the hairline cracks their rounding opens
/// at higher scales (the original's whole pixels close them). Sprites and
/// the interface keep their points.
pub fn weld(ops: &mut [TraceOp]) {
    Welder::default().weld(ops);
}

/// [`weld`] with its tables kept from one trace to the next.
#[derive(Default)]
pub struct Welder {
    /// One-pixel cells, open-addressed: a cell's key and its newest anchor
    /// (its index + 1; 0 an empty slot).
    cells: Vec<(u64, u32)>,
    anchors: Vec<Anchor>,
}

/// A point other points weld to, and the anchor before it in its cell.
struct Anchor {
    fx: f32,
    fy: f32,
    z: f32,
    view: Option<[f32; 3]>,
    primitive: usize,
    next: u32,
}

/// The whole pixel at or below `x`, as `x.floor() as i32` (without libm's
/// floor, which a baseline x86-64 build calls).
fn whole(x: f32) -> i32 {
    let t = x as i32;
    if t as f32 > x && t != i32::MIN { t - 1 } else { t }
}

impl Welder {
    pub fn weld(&mut self, ops: &mut [TraceOp]) {
        // Each game frame (they end at a vertical blank) apart: a trace
        // holding several (the host ran them before presenting) would weld
        // a frame's points to the ones an earlier frame drew there, with
        // another camera.
        for frame in ops.split_mut(|op| matches!(op, TraceOp::VBlank)) {
            self.weld_frame(frame);
        }
    }

    fn weld_frame(&mut self, ops: &mut [TraceOp]) {
        let welded = |p: &Primitive| matches!(p.class, Class::World | Class::Model) && matches!(p.shape, Shape::Triangle | Shape::Quad);
        let points: usize = ops.iter().map(|op| match op {
            TraceOp::Draw(p) if welded(p) => p.vertex_count(),
            _ => 0,
        }).sum();
        if points == 0 {
            return;
        }
        let size = (points * 2).next_power_of_two();
        self.cells.clear();
        self.cells.resize(size, (0, 0));
        self.anchors.clear();
        let mask = size - 1;
        let key = |x: i32, y: i32| (u64::from(x as u32) << 32) | u64::from(y as u32);
        let slot = |cells: &[(u64, u32)], key: u64| {
            let mut i = ((key ^ key >> 29).wrapping_mul(0x9E37_79B9_7F4A_7C15) >> 32) as usize & mask;
            while cells[i].1 != 0 && cells[i].0 != key {
                i = (i + 1) & mask;
            }
            i
        };
        for (primitive, op) in ops.iter_mut().enumerate() {
            let TraceOp::Draw(p) = op else { continue };
            if !welded(p) {
                continue;
            }
            let count = p.vertex_count();
            for v in p.vertices.iter_mut().take(count).filter(|v| v.z > 0.0) {
                // The cells within half a pixel: at most two each way.
                let (x0, x1) = (whole(v.fx - 0.5), whole(v.fx + 0.5));
                let (y0, y1) = (whole(v.fy - 0.5), whole(v.fy + 0.5));
                let mut found = None;
                'search: for cx in x0..=x1 {
                    for cy in y0..=y1 {
                        let mut at = self.cells[slot(&self.cells, key(cx, cy))].1;
                        while at != 0 {
                            let a = &self.anchors[at as usize - 1];
                            if a.primitive != primitive && (a.fx - v.fx).abs() <= 0.5 && (a.fy - v.fy).abs() <= 0.5 && (a.z - v.z).abs() <= (v.z * 0.01).max(4.0) {
                                found = Some(at as usize - 1);
                                break 'search;
                            }
                            at = a.next;
                        }
                    }
                }
                match found {
                    Some(i) => {
                        let a = &self.anchors[i];
                        (v.fx, v.fy) = (a.fx, a.fy);
                        if a.view.is_some() && v.view.is_some() {
                            v.view = a.view;
                        }
                    }
                    None => {
                        let i = slot(&self.cells, key(whole(v.fx), whole(v.fy)));
                        let next = self.cells[i].1;
                        self.cells[i] = (key(whole(v.fx), whole(v.fy)), self.anchors.len() as u32 + 1);
                        self.anchors.push(Anchor { fx: v.fx, fy: v.fy, z: v.z, view: v.view, primitive, next });
                    }
                }
            }
        }
    }
}

/// Takes out of `ops` what the game did after the last primitive of its
/// scene (the interface over it, in order with the VRAM writes and copies
/// its textures follow), culled faces aside (a replay draws some late):
/// what lies over the finished scene.
fn split_overlay(ops: &mut Vec<TraceOp>, feedback: Option<VramRect>) -> Vec<TraceOp> {
    let scene = scene_offset(ops);
    let last = ops.iter().rposition(|op| matches!(op, TraceOp::Draw(p) if !p.revealed && effective_class(p, scene).is_scene()))
        .max(feedback.and_then(|area| framebuffer_end(ops, area)));
    let tail = ops.split_off(last.map_or(0, |i| i + 1));
    let mut overlay = Vec::with_capacity(tail.len());
    for op in tail {
        match &op {
            TraceOp::Draw(p) if p.revealed => ops.push(op),
            _ => overlay.push(op),
        }
    }
    overlay
}

/// The scene read as a texture during the same frame, including strips saved
/// elsewhere first (the field's heat-wave distortion). Keep its copies and
/// quads with the scene, ahead of the interface and postprocessing.
fn framebuffer_end(ops: &[TraceOp], area: VramRect) -> Option<usize> {
    let mut regions = vec![area];
    let mut scene = false;
    let mut last = None;
    for (i, op) in ops.iter().enumerate() {
        match op {
            TraceOp::Draw(p) => {
                if scene && regions.iter().any(|rect| samples(p, rect)) {
                    last = Some(i);
                }
                scene |= p.class.is_scene();
            }
            &TraceOp::Copy { source, destination, width, height } if scene => {
                let from = VramRect { x: source.0, y: source.1, width, height };
                if overlap(&area, &from) == u32::from(width) * u32::from(height) {
                    regions.push(VramRect { x: destination.0, y: destination.1, width, height });
                }
            }
            _ => {}
        }
    }
    last
}

pub(crate) fn framebuffer_effect(ops: &[TraceOp], area: VramRect) -> bool {
    framebuffer_end(ops, area).is_some()
}

/// A copy whose source the game changed after it (a screen taken, then
/// overwritten while the copy is still shown) shows the pixels it copied:
/// the native ones its destination's write carries, not the scaled
/// picture's source as it is when the frame is replayed.
fn settle_copies(ops: &mut [TraceOp], captures: &[(VramRect, bool)], feedback: Option<VramRect>) {
    let mut later: Vec<VramRect> = Vec::new();
    for i in (0..ops.len()).rev() {
        match &ops[i] {
            TraceOp::Write { rect, .. } => later.push(*rect),
            &TraceOp::Copy { source, destination, width, height } => {
                let from = VramRect { x: source.0, y: source.1, width, height };
                let to = VramRect { x: destination.0, y: destination.1, width, height };
                // A pause/transition copied the displayed picture to this
                // buffer, or a buffer took a kept screen: the held picture
                // survives source overwrites.
                if captures.iter().any(|&(area, screen)| (screen && area == to) || area == from)
                    || feedback.is_some_and(|area| overlap(&area, &from) == u32::from(width) * u32::from(height))
                {
                    continue;
                }
                // Its own destination's write follows it.
                let own = matches!(ops.get(i + 1), Some(TraceOp::Write { rect, .. }) if *rect == to);
                let changed = &later[..later.len() - usize::from(own && !later.is_empty())];
                if own && changed.iter().any(|rect| overlap(rect, &from) > 0) {
                    if let Some(TraceOp::Write { upload, .. }) = ops.get_mut(i + 1) {
                        *upload = true;
                    }
                    ops[i] = TraceOp::VBlank;
                }
            }
            _ => {}
        }
    }
}

/// `ops` seen from `distance` GTE depth units further back along the
/// camera's axis: each projected vertex of the scene is projected again
/// from its point before the division (also one behind the game's camera),
/// and its depth grows by `distance`; a point without one (and a sprite's
/// floor point) moves towards its projection centre by z / (z + distance).
/// A primitive with a point within the new camera's near plane (half the
/// projection distance, as the GTE allows) is dropped. Interface
/// primitives stay as they are.
pub fn pull_back(ops: &mut Vec<TraceOp>, distance: f32) {
    let scene = scene_offset(ops);
    let moved = |center: (f32, f32), x: &mut f32, y: &mut f32, z: &mut f32| {
        let k = *z / (*z + distance);
        *x = center.0 + (*x - center.0) * k;
        *y = center.1 + (*y - center.1) * k;
        *z += distance;
    };
    ops.retain_mut(|op| {
        let TraceOp::Draw(p) = op else { return true };
        let Some(((ox, oy), h)) = p.projection else { return true };
        if !effective_class(p, scene).is_scene() {
            return true;
        }
        let h = f32::from(h);
        let count = p.vertex_count();
        if p.vertices[..count].iter().any(|v| v.view.is_some_and(|[_, _, z]| (z + distance) * 2.0 <= h)) {
            return false;
        }
        // The projection centre in VRAM: the GTE's offset plus the drawing offset.
        let center = (f32::from(ox) + f32::from(p.environment.offset.0), f32::from(oy) + f32::from(p.environment.offset.1));
        for v in p.vertices.iter_mut().take(count) {
            match v.view {
                Some([x, y, z]) => {
                    let depth = z + distance;
                    (v.fx, v.fy, v.z) = (center.0 + h * x / depth, center.1 + h * y / depth, depth);
                }
                None if v.z > 0.0 => moved(center, &mut v.fx, &mut v.fy, &mut v.z),
                None => continue,
            }
            v.x = whole(v.fx);
            v.y = whole(v.fy);
        }
        if let Some([x, y, z]) = &mut p.ground {
            moved(center, x, y, z);
        }
        true
    });
}

/// `ops` seen from `look`, a camera within the scene: each projected vertex
/// of the scene moves into its space (its [`xem_gpu::TraceVertex::view`]
/// holds it there, for the near plane's clipping when drawn) and is
/// projected with `look.h` and `look.shift` about its projection centre; a
/// point without a point before the division is taken back through the
/// game's projection first. A sprite's parts stand along the world's up at
/// the sprite's floor point (as the game's camera showed them, sized by
/// their distance), spread across the view's right or, with `look.facing`,
/// turned to face that point; the viewer's own sprite is left out. A
/// primitive wholly behind the near plane is dropped, and points behind it
/// are placed at it until the primitive is clipped there. Interface
/// primitives stay as they are.
pub fn first_person(ops: &mut Vec<TraceOp>, look: &Look) {
    first_person_layered(ops, look, None);
}

/// The lifts a look from within the scene gives a frame's faces
/// ([`plane_layers`]), kept for the frame's next looks (both eyes of a
/// headset and every presentation of it): the frame's identity and its
/// operations' count, and each operation's faces below it.
#[derive(Default)]
pub(crate) struct PlaneLayers {
    frame: Option<((u16, u16), u64)>,
    count: usize,
    below: Vec<f32>,
}

/// [`first_person`], the lifts of frame `kept.1`'s faces taken from `kept.0`
/// when it holds them, else worked out and kept there.
pub(crate) fn first_person_layered(ops: &mut Vec<TraceOp>, look: &Look, kept: Option<(&mut PlaneLayers, ((u16, u16), u64))>) {
    // Each original part carries the same alternative for its drawing pass.
    // Replace that pass once, including the original builder's texture cells,
    // and put those cells back before another sprite/scenery samples VRAM.
    let mut replaced = std::collections::HashSet::new();
    let mut facing_ops = Vec::with_capacity(ops.len());
    for op in ops.drain(..) {
        let TraceOp::Draw(p) = &op else { facing_ops.push(op); continue };
        let Some(facing) = &p.facing else { facing_ops.push(op); continue };
        if !replaced.insert(std::sync::Arc::as_ptr(facing)) { continue; }
        for (rect, pixels) in &facing.textures {
            facing_ops.push(TraceOp::Write { rect: *rect, pixels: pixels.clone(), upload: true });
        }
        for part in &facing.parts {
            let mut p = p.clone();
            p.facing = None;
            p.shape = xem_gpu::Shape::Quad;
            p.raw = part.words[0] & 0x01000000 != 0;
            p.semi = part.words[0] & 0x02000000 != 0;
            p.clut = (part.words[2] >> 16) as u16;
            p.environment.draw_mode = (p.environment.draw_mode & !0x9FF) | (part.words[4] >> 16 & 0x9FF);
            p.projection = Some((part.points[0].offset, part.points[0].h));
            for (i, v) in p.vertices.iter_mut().enumerate() {
                let q = part.points[i];
                let xy = part.words[i * 2 + 1];
                let uv = part.words[i * 2 + 2];
                v.x = i32::from(xy as i16) + i32::from(p.environment.offset.0);
                v.y = i32::from((xy >> 16) as i16) + i32::from(p.environment.offset.1);
                v.fx = q.x as f32 / 65536.0 + f32::from(p.environment.offset.0);
                v.fy = q.y as f32 / 65536.0 + f32::from(p.environment.offset.1);
                v.z = f32::from(q.sz.max(1));
                v.view = Some(q.view.map(|value| value as f32));
                v.color = part.words[0] & 0xFFFFFF;
                v.u = (uv & 0xFF) as i16;
                v.v = (uv >> 8 & 0xFF) as i16;
            }
            facing_ops.push(TraceOp::Draw(p));
        }
        for (rect, pixels) in &facing.restore {
            facing_ops.push(TraceOp::Write { rect: *rect, pixels: pixels.clone(), upload: true });
        }
    }
    *ops = facing_ops;
    let scene = scene_offset(ops);
    let dot = |a: [f32; 3], b: [f32; 3]| a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    let camera = |q: [f32; 3]| {
        let d = [q[0] - look.eye[0], q[1] - look.eye[1], q[2] - look.eye[2]];
        look.axes.map(|axis| dot(d, axis))
    };
    // The world's up in the camera's space: a sprite stands along it, as
    // wide along the camera's right (or across the line to what it faces)
    // as the game's camera showed it.
    let up = look.axes.map(|axis| dot(look.up, axis));
    let across = |anchor: [f32; 3]| -> [f32; 3] {
        let Some(viewer) = look.facing else { return [1.0, 0.0, 0.0] };
        // The line from the viewer to the sprite, level: the sprite's
        // right is across it (the viewer's right, facing the sprite).
        let d = [anchor[0] - viewer[0], anchor[1] - viewer[1], anchor[2] - viewer[2]];
        let rise = dot(d, look.up);
        let level = [d[0] - rise * look.up[0], d[1] - rise * look.up[1], d[2] - rise * look.up[2]];
        let right = [
            level[1] * look.up[2] - level[2] * look.up[1],
            level[2] * look.up[0] - level[0] * look.up[2],
            level[0] * look.up[1] - level[1] * look.up[0],
        ];
        let length = dot(right, right).sqrt();
        if length < 1e-3 {
            return [1.0, 0.0, 0.0];
        }
        look.axes.map(|axis| dot(right, axis) / length)
    };
    let point = view_point;
    let feet = sprite_feet(ops);
    let raises = sprite_raises(ops, &feet);
    // The game's order among faces in one plane, which depth cannot tell
    // apart: each lifted over those it is drawn over there.
    let worked;
    let below: &[f32] = match kept {
        Some((kept, frame)) => {
            if kept.frame != Some(frame) || kept.count != ops.len() {
                *kept = PlaneLayers { frame: Some(frame), count: ops.len(), below: plane_layers(ops, scene) };
            }
            &kept.below
        }
        None => {
            worked = plane_layers(ops, scene);
            &worked
        }
    };
    let mut index = 0;
    ops.retain_mut(|op| {
        let lifts = below[index];
        index += 1;
        let TraceOp::Draw(p) = op else { return true };
        let Some(((ox, oy), _)) = p.projection else { return true };
        if !effective_class(p, scene).is_scene() {
            return true;
        }
        if look.hide != 0 && p.class == Class::Sprite && p.owner == look.hide {
            return false;
        }
        let count = p.vertex_count();
        let mut moved = [[0.0f32; 3]; 4];
        let anchor = feet.get(&p.owner).copied().filter(|_| p.class == Class::Sprite);
        let spread = anchor.map(&across);
        let raise = anchor.and_then(|anchor| raises.get(&anchor.map(f32::to_bits))).copied().unwrap_or(0.0);
        for (i, m) in moved.iter_mut().enumerate().take(count) {
            let Some(q) = point(p, i) else { return false };
            *m = match (anchor, spread) {
                (Some(anchor), Some(s)) => {
                    let base = camera(anchor);
                    let (right, down) = (q[0] - anchor[0], q[1] - anchor[1]);
                    std::array::from_fn(|k| base[k] + right * s[k] + (raise - down) * up[k])
                }
                _ => camera(q),
            };
        }
        if moved[..count].iter().all(|m| m[2] < look.near) {
            return false;
        }
        if lifts > 0.0 {
            // Towards the eye along each point's ray: where it showed, nearer.
            let nearer = 1.0 / (1.0 + lifts * LIFT);
            moved[..count].iter_mut().for_each(|m| *m = m.map(|c| c * nearer));
        }
        let center = (f32::from(ox) + f32::from(p.environment.offset.0), f32::from(oy) + f32::from(p.environment.offset.1));
        let project = |m: [f32; 3]| {
            let z = m[2].max(look.near);
            (center.0 + look.shift[0] + look.h[0] * m[0] / z, center.1 + look.shift[1] + look.h[1] * m[1] / z, z)
        };
        for (v, m) in p.vertices.iter_mut().zip(&moved[..count]) {
            (v.fx, v.fy, v.z) = project(*m);
            v.x = whole(v.fx);
            v.y = whole(v.fy);
            v.view = Some(*m);
        }
        if let Some(anchor) = anchor
            && let Some(ground) = &mut p.ground
        {
            let (x, y, z) = project(camera(anchor));
            *ground = [x, y, z];
        }
        true
    });
}

/// Faces of a look from within the scene in one plane: their planes within
/// this of each other (the game camera's view-space units, a field's about
/// three a world unit: a quarter of the space between a field's shadows and
/// their floor), found by cells this large of the plane.
const COPLANAR: f32 = 0.25;
const PLANE_CELL: f32 = 768.0;
/// A face drawn over others in their plane is drawn this share of its depth
/// nearer a face below it (with depth reversed and written by the fragment,
/// eighty times a float's precision on every backend: a tenth of a unit
/// 10 000 away), where it shows as it did.
const LIFT: f32 = 1e-5;

/// For each of `ops`, how many faces it is drawn over in its plane
/// ([`Layers`]; 0 for all but the scene's faces), found in the game
/// camera's view space: the same from any look, which moves the world's
/// faces as one and lays a sprite's parts on its billboard as the game's
/// screen shows them (they share the plane facing its camera there).
fn plane_layers(ops: &[TraceOp], scene: Option<(i16, i16)>) -> Vec<f32> {
    let mut layers = Layers::default();
    ops.iter().map(|op| {
        let TraceOp::Draw(p) = op else { return 0.0 };
        if p.projection.is_none() || p.shape == Shape::Line || !effective_class(p, scene).is_scene() {
            return 0.0;
        }
        let count = p.vertex_count();
        let mut corners = [glam::Vec3::ZERO; 4];
        for (i, corner) in corners.iter_mut().enumerate().take(count) {
            let Some(q) = view_point(p, i) else { return 0.0 };
            *corner = glam::Vec3::from(q);
        }
        let corners = &corners[..count];
        let normal = (corners[1] - corners[0]).cross(corners[2] - corners[0]).normalize_or_zero();
        if normal == glam::Vec3::ZERO {
            return 0.0;
        }
        // Towards the game's camera, the view space's origin.
        let normal = if normal.dot(-corners[0]) < 0.0 { -normal } else { normal };
        layers.place(normal, corners, COPLANAR, 1.0, PLANE_CELL)
    }).collect()
}

/// A point on the screen and its depth, back in the game camera's view space
/// through `p`'s projection.
pub(crate) fn unproject(p: &Primitive, (x, y, z): (f32, f32, f32)) -> [f32; 3] {
    let ((ox, oy), h) = p.projection.unwrap_or_default();
    let h = f32::from(h).max(1.0);
    let center = (f32::from(ox) + f32::from(p.environment.offset.0), f32::from(oy) + f32::from(p.environment.offset.1));
    [(x - center.0) * z / h, (y - center.1) * z / h, z]
}

/// Vertex `i` of `p` in the game camera's view space: its point before the
/// division, else its screen point taken back through the projection (none
/// for a point without a projection).
pub(crate) fn view_point(p: &Primitive, i: usize) -> Option<[f32; 3]> {
    let v = &p.vertices[i];
    v.view.or_else(|| (v.z > 0.0).then(|| unproject(p, (v.fx, v.fy, v.z))))
}

/// How far a look from within the scene raises the sprites at each floor
/// point of `ops` (`feet`, by the point's bits; view-space units along the
/// world's up): the game draws a sprite a little below its floor point,
/// over the floor whatever their depths, so they stand on the floor, their
/// lowest part there, or above it as far as the game drew them above it (a
/// jump: raised by nothing), as the diorama stands them.
fn sprite_raises(ops: &[TraceOp], feet: &HashMap<u32, [f32; 3]>) -> HashMap<[u32; 3], f32> {
    let scene = scene_offset(ops);
    let mut raises: HashMap<[u32; 3], f32> = HashMap::new();
    for op in ops {
        let TraceOp::Draw(p) = op else { continue };
        if p.class != Class::Sprite || p.projection.is_none() || !effective_class(p, scene).is_scene() {
            continue;
        }
        let Some(anchor) = feet.get(&p.owner) else { continue };
        for q in (0..p.vertex_count()).filter_map(|i| view_point(p, i)) {
            let raise = raises.entry(anchor.map(f32::to_bits)).or_insert(0.0);
            *raise = raise.max(q[1] - anchor[1]);
        }
    }
    // Less than a hundredth of a unit is none: the floor point taken back
    // through the projection lies that close to a part drawn at it.
    raises.retain(|_, raise| *raise >= 0.01);
    raises
}

/// Where each sprite of `ops` stands (by its owner), in the game camera's
/// view space: its floor point, else below the middle of all its parts'
/// points, at the lowest (y grows downwards).
pub(crate) fn sprite_feet(ops: &[TraceOp]) -> HashMap<u32, [f32; 3]> {
    let mut feet: HashMap<u32, [f32; 3]> = HashMap::new();
    let mut parts: HashMap<u32, ([f32; 3], f32)> = HashMap::new();
    for op in ops {
        let TraceOp::Draw(p) = op else { continue };
        if p.class != Class::Sprite || p.projection.is_none() {
            continue;
        }
        if let Some([x, y, z]) = p.ground {
            feet.insert(p.owner, unproject(p, (x, y, z)));
        }
        for q in (0..p.vertex_count()).filter_map(|i| view_point(p, i)) {
            let (sums, n) = parts.entry(p.owner).or_insert(([0.0, f32::MIN, 0.0], 0.0));
            *sums = [sums[0] + q[0], sums[1].max(q[1]), sums[2] + q[2]];
            *n += 1.0;
        }
    }
    for (owner, (sums, n)) in parts {
        feet.entry(owner).or_insert([sums[0] / n, sums[1], sums[2] / n]);
    }
    feet
}

/// Where the world of `ops` (its scenery, drawn through the GTE: a field's
/// map, which a menu's own models are not) is projected about: its
/// projection offset plus the drawing offset (VRAM pixels), as
/// [`first_person`] takes each scene primitive's centre; none without it.
pub(crate) fn projection_centre(ops: &[TraceOp]) -> Option<(f32, f32)> {
    projection(ops).map(|(centre, _)| centre)
}

/// [`projection_centre`] and the projection distance (the GTE's H) the
/// world of `ops` is projected with.
pub(crate) fn projection(ops: &[TraceOp]) -> Option<((f32, f32), f32)> {
    let scene = scene_offset(ops)?;
    ops.iter().find_map(|op| match op {
        TraceOp::Draw(p) if p.class == Class::World => match p.projection {
            Some((offset, h)) if offset == scene => Some((
                (f32::from(offset.0) + f32::from(p.environment.offset.0), f32::from(offset.1) + f32::from(p.environment.offset.1)),
                f32::from(h),
            )),
            _ => None,
        },
        _ => None,
    })
}

/// The depth the camera looks at, measured: the median depth of the scene
/// primitives the game drew in the middle of its screen (a third of its
/// width, two fifths of its height), or `None` without any.
pub fn central_depth(ops: &[TraceOp], area: DisplayArea) -> Option<f32> {
    let scene = scene_offset(ops);
    let (cx, cy) = (area.x as f32 + area.width as f32 / 2.0, area.y as f32 + area.height as f32 / 2.0);
    let (rx, ry) = (area.width as f32 / 6.0, area.height as f32 / 5.0);
    let mut depths: Vec<f32> = ops
        .iter()
        .filter_map(|op| match op {
            TraceOp::Draw(p) if !p.revealed && effective_class(p, scene).is_scene() => {
                let (x, y) = center(p);
                ((x - cx).abs() < rx && (y - cy).abs() < ry).then(|| p.depth()).filter(|z| *z > 0.0)
            }
            _ => None,
        })
        .collect();
    if depths.is_empty() {
        return None;
    }
    depths.sort_by(f32::total_cmp);
    Some(depths[depths.len() / 2])
}
