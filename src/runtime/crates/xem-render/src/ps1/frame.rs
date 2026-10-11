//! The displayed frame: colour and auxiliary textures the effects and the
//! compositor read.
//!
//! Normally the frame is the display area of the scaled VRAM picture. Wider
//! framing and interpolation instead replay the primitives drawn into the
//! displayed buffer since its last clear (the renderer's frame log) into a
//! target of their own:
//! - **Wider framing** widens the target by `side` VRAM pixels on each side
//!   and lets scene primitives draw past the drawing area's left and right
//!   edges there; interface primitives keep the original area. The game
//!   still culls against its own screen, so the sides show only what
//!   reaches past it (the gaps are the known content limit of the option).
//! - **Interpolation** moves each primitive of the newest frame from where
//!   the same primitive (same shape, texture, CLUT and texture coordinates,
//!   or colours when untextured, nearest to it) was in the frame before, by
//!   `alpha`. It presents one game frame late and never touches game state.

use super::{
    AUX_FORMAT, Builder, COLOR_FORMAT, Copies, DisplayArea, DrawOptions, PassUniform, Ps1Renderer, Stream, Target, bind_group, encode,
    pass_options, picture_usage, texture,
};
use std::collections::HashMap;
use xem_gpu::{Primitive, Shape, TraceOp};

/// How the frame is framed.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Framing {
    /// VRAM pixels added on each side of the display area (wider framing).
    pub side: u32,
    /// Interpolate between the previous and the newest frame: 0 shows the
    /// previous, 1 the newest.
    pub interpolate: Option<f32>,
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
    /// The newest displayed frame's log identity (buffer, clears) and its
    /// primitives, and the frame before.
    newest: Option<((u16, u16), u64)>,
    newest_ops: Vec<TraceOp>,
    previous_ops: Vec<TraceOp>,
    /// Primitives the last replay drew (for the effects).
    replayed: Vec<TraceOp>,
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
            newest: None,
            newest_ops: Vec::new(),
            previous_ops: Vec::new(),
            replayed: Vec::new(),
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
            return false;
        };
        let identity = ((log.rect.x, log.rect.y), log.frames);
        if self.newest == Some(identity) {
            // The same frame, perhaps grown (a frame drawn over several takes).
            self.newest_ops.clone_from(&log.ops);
            return false;
        }
        self.previous_ops = std::mem::replace(&mut self.newest_ops, log.ops.clone());
        self.newest = Some(identity);
        true
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
        let replay = (framing.side > 0 || framing.interpolate.is_some()) && !self.newest_ops.is_empty();
        let side = if replay { framing.side } else { 0 };
        self.resize(device, ((area.width.max(1) + 2 * side) * scale, area.height.max(1) * scale));
        self.area = Some(area);
        self.scale = scale;
        self.side = side;
        if !replay {
            renderer.copy_display(encoder, area, &self.color, &self.aux);
            self.replayed.clone_from(&self.newest_ops);
            return;
        }
        let ops = match framing.interpolate {
            Some(alpha) if !self.previous_ops.is_empty() => interpolate(&self.newest_ops, &self.previous_ops, alpha.clamp(0.0, 1.0)),
            _ => self.newest_ops.clone(),
        };
        let options: DrawOptions = *renderer.options();
        let mut builder = Builder::new(&options, &ops);
        builder.spill = side;
        for op in &ops {
            builder.op(op);
        }
        let origin = (area.x as f32 - side as f32, area.y as f32);
        let uniform = PassUniform {
            origin: [origin.0, origin.1],
            extent: [(area.width + 2 * side) as f32, area.height as f32],
            scale: scale as f32,
            options: pass_options(&options.fidelity),
            _pad: [0; 2],
        };
        queue.write_buffer(&self.uniform, 0, bytemuck::bytes_of(&uniform));
        self.vertices.write(device, queue, bytemuck::cast_slice(&builder.vertices));
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
        let group = bind_group(device, &renderer.pipelines.layout, &self.uniform, &renderer.vram);
        let (picture_color, picture_aux) = (renderer.color.clone(), renderer.aux.clone());
        let target = Target { color: &self.color, aux: &self.aux, depth: &depth_view, origin, scale };
        let commands = builder.commands;
        encode(
            device,
            &mut renderer.pipelines,
            encoder,
            &group,
            self.vertices.buffer(),
            None,
            Copies::From { color: &picture_color, aux: &picture_aux, scale },
            &target,
            &commands,
        );
        self.replayed = ops;
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
        &self.replayed
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
            }
            TraceOp::Draw(moved)
        })
        .collect()
}
