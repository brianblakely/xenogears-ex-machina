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
//! - **Interpolation** moves each scene primitive of the newest frame from
//!   where the same primitive (same shape, texture, CLUT and texture
//!   coordinates, or colours when untextured, nearest to it) was in the
//!   frame before, by `alpha`; the interface moves as the game moves it. It
//!   presents one game frame late and never touches game state.
//! - **Looking around** from within the scene (first-person inspection)
//!   moves every projected vertex into a camera of its own ([`Look`],
//!   [`first_person`]); polygons reaching behind its near plane are clipped
//!   there as they are drawn, and depth is tested whatever the ordering.

use super::{
    AUX_FORMAT, Builder, COLOR_FORMAT, Copies, DisplayArea, DrawOptions, Near, PASS_RECIPROCAL_DEPTH, PassUniform, Ps1Renderer, Reveal, Storage,
    Stream, Target, VRAM_HEIGHT, VRAM_WIDTH, bind_group, effective_class, encode, overlap, pass_options, picture_usage, samples, scene_offset,
    texture,
};
use std::collections::HashMap;
use std::sync::Arc;
use xem_gpu::{Class, Eye, Primitive, Shape, TraceOp, VramRect};

/// How the frame is framed.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Framing {
    /// VRAM pixels added on each side of the display area (wider framing).
    pub side: u32,
    /// Interpolate between the previous and the newest frame: 0 shows the
    /// previous, 1 the newest.
    pub interpolate: Option<f32>,
    /// How far the camera is pulled back along its axis, in GTE depth
    /// units (0: where the game put it).
    pub pull_back: f32,
    /// Replay the frame even framed as the game framed it (for effects
    /// that need the interface drawn after them: [`FrameRenderer::draw_overlay`]).
    pub replay: bool,
    /// Look around from within the scene instead of the game's camera.
    pub look: Option<Look>,
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
    /// The projection distance (VRAM pixels: the field of view) and the
    /// near plane's distance (view-space units).
    pub h: f32,
    pub near: f32,
    /// The sprite whose parts are left out (the viewer's own; 0 none).
    pub hide: u32,
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
    /// when the port recorded it ([`xem_gpu::TraceOp::Eye`]).
    eye: Option<Eye>,
    /// How many of them change the picture (not the native pixels a copy,
    /// fill or drawing left, which only follow what was drawn).
    drawn: usize,
    previous_ops: Vec<TraceOp>,
    /// Primitives the last replay drew (for the effects), when it replayed.
    replayed: Vec<TraceOp>,
    replaying: bool,
    /// The native VRAM when the newest frame began (its log's start).
    snapshot: Option<Arc<Vec<u16>>>,
    /// The native VRAM a replay samples: the snapshot, changed by the
    /// frame's writes in order as the replay goes (a texture slot the game
    /// fills again later in the frame shows each primitive its own texels),
    /// its staged writes, and a copy of what that texture holds (the rows of
    /// a new snapshot that differ from it are all a new frame uploads).
    vram: wgpu::Texture,
    staging: Stream,
    held: Vec<u16>,
    /// The replay's bind group, made with the capture texture of a generation.
    group: Option<(u64, wgpu::BindGroup)>,
    /// The replay's and the overlay's builder buffers.
    storage: Storage,
    overlay_storage: Storage,
}

/// A pass uniform of `size_of::<PassUniform>()` bytes.
fn uniform_buffer(device: &wgpu::Device) -> wgpu::Buffer {
    device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("frame replay"),
        size: size_of::<PassUniform>() as u64,
        usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        mapped_at_creation: false,
    })
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
            vertices: Stream::new(device, "frame replay vertices", wgpu::BufferUsages::VERTEX),
            overlay: Vec::new(),
            overlay_scene: None,
            overlay_vertices: Stream::new(device, "frame overlay vertices", wgpu::BufferUsages::VERTEX),
            overlay_staging: Stream::new(device, "frame overlay staging", wgpu::BufferUsages::COPY_SRC),
            newest: None,
            newest_ops: Vec::new(),
            eye: None,
            drawn: 0,
            previous_ops: Vec::new(),
            replayed: Vec::new(),
            replaying: false,
            snapshot: None,
            vram: texture(device, "frame replay vram", (VRAM_WIDTH, VRAM_HEIGHT), wgpu::TextureFormat::R16Uint,
                wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST),
            staging: Stream::new(device, "frame replay staging", wgpu::BufferUsages::COPY_SRC),
            held: vec![0; (VRAM_WIDTH * VRAM_HEIGHT) as usize],
            group: None,
            storage: Storage::default(),
            overlay_storage: Storage::default(),
        }
    }

    fn resize(&mut self, device: &wgpu::Device, size: (u32, u32)) {
        if self.size != size {
            self.color = texture(device, "frame", size, COLOR_FORMAT, picture_usage());
            self.aux = texture(device, "frame aux", size, AUX_FORMAT, picture_usage());
            self.depth = texture(device, "frame depth", size, wgpu::TextureFormat::Depth32Float, wgpu::TextureUsages::RENDER_ATTACHMENT);
            self.color_view = self.color.create_view(&Default::default());
            self.size = size;
        }
    }

    /// Notes the displayed buffer's log; returns whether a new game frame
    /// is shown since the last call.
    pub fn track(&mut self, renderer: &Ps1Renderer, area: DisplayArea) -> bool {
        let Some(log) = renderer.frame_log(area) else {
            // No log of what the shown buffer holds (a trace started again,
            // a state loaded): nothing to replay until its next clear, rather
            // than an older frame's primitives over the new VRAM.
            self.newest = None;
            self.newest_ops.clear();
            self.previous_ops.clear();
            self.snapshot = None;
            self.eye = None;
            self.drawn = 0;
            return false;
        };
        let identity = ((log.rect.x, log.rect.y), log.frames);
        if self.newest == Some(identity) {
            // The same frame, perhaps grown (a frame drawn over several
            // takes): a log only grows until its buffer is cleared.
            let known = self.newest_ops.len();
            if log.ops.len() > known {
                self.newest_ops.extend_from_slice(&log.ops[known..]);
                self.drawn += drawn(&log.ops[known..]);
            } else if log.ops.len() < known {
                self.newest_ops.clone_from(&log.ops);
                self.drawn = drawn(&self.newest_ops);
            }
            return false;
        }
        self.previous_ops = std::mem::replace(&mut self.newest_ops, log.ops.clone());
        self.drawn = drawn(&self.newest_ops);
        self.newest = Some(identity);
        self.snapshot = Some(log.start.clone());
        self.eye = log.eye;
        true
    }

    /// Where the field's controlled character stood in the newest frame
    /// shown, when the port recorded it (while a view looks around).
    pub fn eye(&self) -> Option<Eye> {
        self.eye
    }

    /// How many of the newest frame's ops change its picture.
    pub fn drawn(&self) -> usize {
        self.drawn
    }

    /// Takes `area` of the renderer's picture as the frame, replayed when
    /// `framing` asks for wider framing or interpolation.
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
        self.track(renderer, area);
        let pulled = framing.pull_back > 0.0;
        let replay = (framing.side > 0 || framing.interpolate.is_some() || pulled || framing.look.is_some() || framing.replay)
            && !self.newest_ops.is_empty();
        let side = if replay { framing.side } else { 0 };
        self.resize(device, ((area.width.max(1) + 2 * side) * scale, area.height.max(1) * scale));
        self.area = Some(area);
        self.scale = scale;
        self.side = side;
        self.replaying = replay;
        self.overlay.clear();
        if !replay {
            renderer.copy_display(encoder, area, &self.color, &self.aux);
            return;
        }
        let mut ops = match framing.interpolate {
            Some(alpha) if !self.previous_ops.is_empty() => interpolate(&self.newest_ops, &self.previous_ops, alpha.clamp(0.0, 1.0)),
            _ => {
                // The last replay's buffer, refilled.
                let mut ops = std::mem::take(&mut self.replayed);
                ops.clear();
                ops.extend_from_slice(&self.newest_ops);
                ops
            }
        };
        let screen_copy = renderer.capture.rect.filter(|_| renderer.capture.screen && renderer.capture.scale == scale);
        settle_copies(&mut ops, screen_copy);
        // What the game draws of a screen it took (its pieces, zoomed,
        // shattered) is that screen, as presented then: a screen effect,
        // where the game put it.
        let capture = renderer.capture.rect.filter(|_| renderer.capture.scale == scale).map(|rect| (rect, renderer.capture.side));
        if let Some((rect, _)) = capture {
            for op in &mut ops {
                if let TraceOp::Draw(p) = op
                    && samples(p, &rect)
                {
                    p.class = Class::Flat;
                }
            }
        }
        if let Some(look) = &framing.look {
            first_person(&mut ops, look);
        } else if pulled {
            pull_back(&mut ops, framing.pull_back);
        }
        self.overlay_scene = scene_offset(&ops);
        self.overlay = split_overlay(&mut ops);
        // The replay samples VRAM as it was when the frame began, then as the
        // frame's writes changed it, in order.
        if let Some(start) = &self.snapshot {
            // The rows that differ from what the texture holds, in runs.
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
                    wgpu::TexelCopyTextureInfo { texture: &self.vram, mip_level: 0, origin: wgpu::Origin3d { x: 0, y: first as u32, z: 0 }, aspect: wgpu::TextureAspect::All },
                    bytemuck::cast_slice(&start[rows.clone()]),
                    wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(VRAM_WIDTH * 2), rows_per_image: None },
                    wgpu::Extent3d { width: VRAM_WIDTH, height: (row - first) as u32, depth_or_array_layers: 1 },
                );
                self.held[rows.clone()].copy_from_slice(&start[rows]);
            }
            // The frame's writes (the replay's and the overlay's) as they will
            // leave it.
            for op in ops.iter().chain(&self.overlay) {
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
        let options: DrawOptions = *renderer.options();
        let mut builder = Builder::reusing(&options, &ops, &mut self.storage);
        builder.shown = Some(VramRect { x: area.x as u16, y: area.y as u16, width: area.width as u16, height: area.height as u16 });
        builder.seal = renderer.seal;
        builder.spill = side;
        builder.capture = capture;
        // A frame recorded looking around holds faces from anywhere, not
        // only beside the screen: seen from the game's camera, none.
        builder.reveal = if pulled || framing.look.is_some() {
            Reveal::Everywhere
        } else if side > 0 && self.eye.is_none() {
            Reveal::Sides
        } else {
            Reveal::Hidden
        };
        builder.near = framing.look.map(|look| Near { distance: look.near, h: look.h });
        for op in &ops {
            builder.op(op);
        }
        let origin = (area.x as f32 - side as f32, area.y as f32);
        let (capture_scale, capture_side, capture) = renderer.capture.uniform();
        let uniform = PassUniform {
            origin: [origin.0, origin.1],
            extent: [(area.width + 2 * side) as f32, area.height as f32],
            scale: scale as f32,
            options: pass_options(&options.fidelity) | if framing.look.is_some() { PASS_RECIPROCAL_DEPTH } else { 0 },
            capture_scale,
            capture_side,
            capture,
        };
        queue.write_buffer(&self.uniform, 0, bytemuck::bytes_of(&uniform));
        self.vertices.write(device, queue, bytemuck::cast_slice(&builder.vertices));
        self.staging.write(device, queue, &builder.staging);
        // Start from black (the replayed clear covers what the game cleared).
        for view in [self.color.create_view(&Default::default()), self.aux.create_view(&Default::default())] {
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
        let depth_view = self.depth.create_view(&Default::default());
        encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("frame depth clear"),
            color_attachments: &[],
            depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                view: &depth_view,
                depth_ops: Some(wgpu::Operations { load: wgpu::LoadOp::Clear(1.0), store: wgpu::StoreOp::Store }),
                stencil_ops: None,
            }),
            ..Default::default()
        });
        if self.group.as_ref().is_some_and(|(generation, _)| *generation != renderer.capture.generation) {
            self.group = None;
        }
        let generation = renderer.capture.generation;
        let group = self
            .group
            .get_or_insert_with(|| (generation, bind_group(device, &renderer.pipelines.layout, &self.uniform, &self.vram, &renderer.capture)))
            .1
            .clone();
        let (picture_color, picture_aux) = (renderer.color.clone(), renderer.aux.clone());
        let target = Target { color: &self.color, aux: &self.aux, depth: &depth_view, origin, scale };
        encode(
            device,
            &mut renderer.pipelines,
            encoder,
            &group,
            self.vertices.buffer(),
            Some((self.staging.buffer(), &self.vram)),
            Copies::From { color: &picture_color, aux: &picture_aux, scale, capture: Some(&renderer.capture) },
            &target,
            &builder.commands,
        );
        builder.store(&mut self.storage);
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
        let (Some(area), Some((_, group))) = (self.area, self.group.clone()) else { return };
        if self.overlay.is_empty() {
            return;
        }
        let options: DrawOptions = *renderer.options();
        let mut builder = Builder::reusing(&options, &self.overlay, &mut self.overlay_storage);
        builder.scene = self.overlay_scene;
        builder.shown = Some(VramRect { x: area.x as u16, y: area.y as u16, width: area.width as u16, height: area.height as u16 });
        builder.spill = self.side;
        builder.capture = renderer.capture.rect.filter(|_| renderer.capture.scale == self.scale).map(|rect| (rect, renderer.capture.side));
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
        let copies = Copies::From { color: &picture_color, aux: &picture_aux, scale: self.scale, capture: Some(&renderer.capture) };
        let staging = Some((self.overlay_staging.buffer(), &self.vram));
        encode(device, &mut renderer.pipelines, encoder, &group, self.overlay_vertices.buffer(), staging, copies, &target, &builder.commands);
        builder.store(&mut self.overlay_storage);
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
fn drawn(ops: &[TraceOp]) -> usize {
    ops.iter().filter(|op| !matches!(op, TraceOp::Write { upload: false, .. })).count()
}

/// What identifies a primitive between frames: its shape, page, CLUT,
/// texture coordinates and, untextured, its colours.
fn identity(p: &Primitive) -> (u8, bool, u32, u16, [i16; 8], [u32; 4]) {
    let count = p.vertex_count();
    let mut uv = [0i16; 8];
    let mut colors = [0u32; 4];
    for (i, v) in p.vertices[..count].iter().enumerate() {
        if p.textured {
            uv[i * 2] = v.u;
            uv[i * 2 + 1] = v.v;
        } else {
            colors[i] = v.color;
        }
    }
    let shape = match p.shape {
        Shape::Triangle => 0,
        Shape::Quad => 1,
        Shape::Rectangle => 2,
        Shape::Line => 3,
    };
    (shape, p.textured, p.environment.draw_mode & 0x1FF, p.clut, uv, colors)
}

fn center(p: &Primitive) -> (f32, f32) {
    let count = p.vertex_count() as f32;
    let vertices = &p.vertices[..p.vertex_count()];
    (vertices.iter().map(|v| v.fx).sum::<f32>() / count, vertices.iter().map(|v| v.fy).sum::<f32>() / count)
}

/// Farther than this (VRAM pixels) between frames is a cut, not motion.
const MAX_MOTION: f32 = 48.0;

/// `newest`'s primitives moved back towards their matches in `previous`:
/// alpha 0 is where they were, 1 where they are.
pub fn interpolate(newest: &[TraceOp], previous: &[TraceOp], alpha: f32) -> Vec<TraceOp> {
    // The interface moves as the game moves it, frame by frame.
    let scene = scene_offset(newest);
    let mut candidates: HashMap<_, Vec<&Primitive>> = HashMap::new();
    for op in previous {
        if let TraceOp::Draw(p) = op {
            candidates.entry(identity(p)).or_default().push(p);
        }
    }
    let mut used: HashMap<*const Primitive, ()> = HashMap::new();
    newest
        .iter()
        .map(|op| {
            let TraceOp::Draw(p) = op else { return op.clone() };
            if !effective_class(p, scene).is_scene() {
                return op.clone();
            }
            let (cx, cy) = center(p);
            let Some(list) = candidates.get(&identity(p)) else { return op.clone() };
            let best = list
                .iter()
                .filter(|q| !used.contains_key(&(**q as *const Primitive)))
                .map(|q| {
                    let (qx, qy) = center(q);
                    (q, (qx - cx).hypot(qy - cy))
                })
                .min_by(|a, b| a.1.total_cmp(&b.1));
            let Some((q, distance)) = best else { return op.clone() };
            if distance > MAX_MOTION {
                return op.clone();
            }
            used.insert(*q as *const Primitive, ());
            let mut moved = p.clone();
            for (v, w) in moved.vertices.iter_mut().zip(q.vertices.iter()).take(p.vertex_count()) {
                let lerp = |a: f32, b: f32| b + (a - b) * alpha;
                v.fx = lerp(v.fx, w.fx);
                v.fy = lerp(v.fy, w.fy);
                v.x = v.fx.round() as i32;
                v.y = v.fy.round() as i32;
                if v.z > 0.0 && w.z > 0.0 {
                    v.z = lerp(v.z, w.z);
                }
                if let (Some(a), Some(b)) = (v.view, w.view) {
                    v.view = Some(std::array::from_fn(|i| lerp(a[i], b[i])));
                }
            }
            TraceOp::Draw(moved)
        })
        .collect()
}

/// Points of the scene that the GTE placed within half a native pixel of a
/// point of another primitive, at nearly the same depth, become that point
/// (the first drawn; its sub-pixel position and point before the
/// division): neighbouring pieces of scenery, each placed by its own
/// matrix, meet without the hairline cracks their rounding opens at higher
/// scales (the original's whole pixels close them). Sprites and the
/// interface keep their points.
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
fn split_overlay(ops: &mut Vec<TraceOp>) -> Vec<TraceOp> {
    let scene = scene_offset(ops);
    let last = ops.iter().rposition(|op| matches!(op, TraceOp::Draw(p) if !p.revealed && effective_class(p, scene).is_scene()));
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

/// A copy whose source the game changed after it (a screen taken, then
/// overwritten while the copy is still shown) shows the pixels it copied:
/// the native ones its destination's write carries, not the scaled
/// picture's source as it is when the frame is replayed.
fn settle_copies(ops: &mut [TraceOp], screen_copy: Option<VramRect>) {
    let mut later: Vec<VramRect> = Vec::new();
    for i in (0..ops.len()).rev() {
        match &ops[i] {
            TraceOp::Write { rect, .. } => later.push(*rect),
            &TraceOp::Copy { source, destination, width, height } => {
                let from = VramRect { x: source.0, y: source.1, width, height };
                let to = VramRect { x: destination.0, y: destination.1, width, height };
                // A pause/transition copied the displayed picture to this
                // buffer. Its held presentation survives source overwrites.
                if screen_copy == Some(to) {
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
/// projected with `look.h` about its projection centre; a point without a
/// point before the division is taken back through the game's projection
/// first. A sprite's parts face the camera, standing along the world's up
/// at the sprite's floor point (as the game's camera showed them, sized by
/// their distance); the viewer's own sprite is left out. A primitive wholly
/// behind the near plane is dropped, and points behind it are placed at it
/// until the primitive is clipped there. Interface primitives stay as they
/// are.
pub fn first_person(ops: &mut Vec<TraceOp>, look: &Look) {
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
    // wide along the camera's right as the game's camera showed it.
    let up = look.axes.map(|axis| dot(look.up, axis));
    // A point on the screen and its depth, back in the game's view space.
    let unproject = |p: &Primitive, (x, y, z): (f32, f32, f32)| {
        let ((ox, oy), h) = p.projection.unwrap_or_default();
        let h = f32::from(h).max(1.0);
        let center = (f32::from(ox) + f32::from(p.environment.offset.0), f32::from(oy) + f32::from(p.environment.offset.1));
        [(x - center.0) * z / h, (y - center.1) * z / h, z]
    };
    let point = |p: &Primitive, i: usize| {
        let v = &p.vertices[i];
        v.view.or_else(|| (v.z > 0.0).then(|| unproject(p, (v.fx, v.fy, v.z))))
    };
    // Where each sprite stands: its floor point, else below the middle of
    // all its parts' points, at the lowest (y grows downwards).
    let mut feet: HashMap<u32, [f32; 3]> = HashMap::new();
    let mut parts: HashMap<u32, ([f32; 3], f32)> = HashMap::new();
    for op in ops.iter() {
        let TraceOp::Draw(p) = op else { continue };
        if p.class != Class::Sprite || p.projection.is_none() {
            continue;
        }
        if let Some([x, y, z]) = p.ground {
            feet.insert(p.owner, unproject(p, (x, y, z)));
        }
        for q in (0..p.vertex_count()).filter_map(|i| point(p, i)) {
            let (sums, n) = parts.entry(p.owner).or_insert(([0.0, f32::MIN, 0.0], 0.0));
            *sums = [sums[0] + q[0], sums[1].max(q[1]), sums[2] + q[2]];
            *n += 1.0;
        }
    }
    for (owner, (sums, n)) in parts {
        feet.entry(owner).or_insert([sums[0] / n, sums[1], sums[2] / n]);
    }
    ops.retain_mut(|op| {
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
        for (i, m) in moved.iter_mut().enumerate().take(count) {
            let Some(q) = point(p, i) else { return false };
            *m = match anchor {
                Some(anchor) => {
                    let base = camera(anchor);
                    let (right, down) = (q[0] - anchor[0], q[1] - anchor[1]);
                    [base[0] + right - down * up[0], base[1] - down * up[1], base[2] - down * up[2]]
                }
                None => camera(q),
            };
        }
        if moved[..count].iter().all(|m| m[2] < look.near) {
            return false;
        }
        let center = (f32::from(ox) + f32::from(p.environment.offset.0), f32::from(oy) + f32::from(p.environment.offset.1));
        let project = |m: [f32; 3]| {
            let z = m[2].max(look.near);
            (center.0 + look.h * m[0] / z, center.1 + look.h * m[1] / z, z)
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
