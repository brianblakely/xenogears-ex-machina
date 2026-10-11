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
//! rectangles) has no depth.

use crate::{Gpu, VRAM_HEIGHT, VRAM_WIDTH};
use std::collections::HashMap;

/// A GTE projection as the port's ring holds it.
#[derive(Clone, Copy, Debug, PartialEq)]
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
    /// Where the command came from: the DMA list node's address (bits 8-31)
    /// and the primitive's index in it (bits 0-7), or 0 for words the CPU
    /// wrote.
    pub source: u32,
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
    Write { rect: VramRect, pixels: Vec<u16>, upload: bool },
    /// A vertical blank: the game's frame ended.
    VBlank,
}

/// The trace since the last [`Gpu::take_trace`].
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Trace {
    pub ops: Vec<TraceOp>,
}

#[derive(Clone, Debug, Default)]
pub(crate) struct Recorder {
    ops: Vec<TraceOp>,
    /// Projections of this frame and the one before, by SXY word (11-bit fields).
    current: HashMap<u32, Projection>,
    previous: HashMap<u32, Projection>,
    /// Drawn pixels not yet written to the trace.
    dirty: Option<VramRect>,
    /// The next take starts with all of VRAM (enabled, or a snapshot loaded).
    full_sync: bool,
    /// The DMA list node being sent (0 for words the CPU writes) and the
    /// primitives traced from it so far.
    source: u32,
    source_index: u32,
}

const SXY_MASK: u32 = 0x07FF_07FF;

fn sign11(value: u32) -> i32 {
    ((value << 21) as i32) >> 21
}

impl Gpu {
    /// Turns the draw trace on or off. Turning it on starts the next trace
    /// with the whole VRAM.
    pub fn set_trace(&mut self, enabled: bool) {
        self.trace = enabled.then(|| {
            Box::new(Recorder { full_sync: true, ..Default::default() })
        });
    }

    pub fn trace_enabled(&self) -> bool {
        self.trace.is_some()
    }

    /// Projections the GTE made since the last call, oldest first.
    pub fn trace_projections(&mut self, projections: impl IntoIterator<Item = Projection>) {
        if let Some(trace) = &mut self.trace {
            for projection in projections {
                trace.current.insert(projection.sxy & SXY_MASK, projection);
            }
        }
    }

    /// Everything traced since the last take, ending with the drawn pixels
    /// not yet written.
    pub fn take_trace(&mut self) -> Trace {
        let Some(trace) = self.trace.as_mut() else {
            return Trace::default();
        };
        let mut ops = std::mem::take(&mut trace.ops);
        if trace.full_sync {
            trace.full_sync = false;
            trace.dirty = None;
            ops.clear();
            ops.push(TraceOp::Write {
                rect: VramRect { x: 0, y: 0, width: VRAM_WIDTH as u16, height: VRAM_HEIGHT as u16 },
                pixels: self.vram.clone(),
                upload: true,
            });
            return Trace { ops };
        }
        if let Some(rect) = trace.dirty.take() {
            ops.push(TraceOp::Write { rect, pixels: read_rect(&self.vram, rect), upload: false });
        }
        Trace { ops }
    }

    /// The trace must start again from the whole VRAM (a snapshot replaced it).
    pub(crate) fn trace_resync(&mut self) {
        if let Some(trace) = &mut self.trace {
            trace.full_sync = true;
        }
    }

    pub(crate) fn trace_source(&mut self, node: u32) {
        if let Some(trace) = &mut self.trace {
            trace.source = node;
            trace.source_index = 0;
        }
    }

    pub(crate) fn trace_vblank(&mut self) {
        if let Some(trace) = &mut self.trace {
            trace.previous = std::mem::take(&mut trace.current);
            trace.ops.push(TraceOp::VBlank);
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
        trace.ops.push(TraceOp::Write { rect: dirty, pixels, upload: false });
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
                    trace.ops.push(TraceOp::Write { rect, pixels, upload: false });
                }
            }
            0x20..=0x3F => self.trace_polygon(words),
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
            let pixels = read_rect(&self.vram, rect);
            self.trace.as_mut().expect("checked above").ops.push(TraceOp::Write { rect, pixels, upload });
        }
    }

    fn trace_vertex(&self, trace: &Recorder, word: u32, color: u32, uv: u32) -> (TraceVertex, Option<(Class, (i16, i16), u16, u32)>) {
        let (x, y) = self.vertex(word);
        let (dx, dy) = (x - sign11(word), y - sign11(word >> 16));
        let mut vertex = TraceVertex {
            x,
            y,
            fx: x as f32,
            fy: y as f32,
            z: 0.0,
            color: color & 0xFF_FFFF,
            u: (uv & 0xFF) as i16,
            v: (uv >> 8 & 0xFF) as i16,
        };
        let key = word & SXY_MASK;
        let projection = trace.current.get(&key).or_else(|| trace.previous.get(&key));
        let class = projection.map(|p| {
            let (px, py) = (p.x as f32 / 65536.0, p.y as f32 / 65536.0);
            // A saturated point keeps the integer position.
            if (px.floor() as i32 - sign11(word)).abs() <= 1 && (py.floor() as i32 - sign11(word >> 16)).abs() <= 1 {
                vertex.fx = px + dx as f32;
                vertex.fy = py + dy as f32;
            }
            vertex.z = f32::from(p.sz.max(1));
            (Class::from_projection(p.class), p.offset, p.h, p.owner)
        });
        (vertex, class)
    }

    fn trace_polygon(&mut self, words: &[u32]) {
        let trace = self.trace.as_ref().expect("checked by the caller");
        let op = words[0] >> 24;
        let gouraud = op & 0x10 != 0;
        let count = if op & 0x08 != 0 { 4 } else { 3 };
        let textured = op & 0x04 != 0 && !self.texture_disabled();
        let raw = op & 0x01 != 0;
        let mut vertices = [TraceVertex::default(); 4];
        let mut classes = Vec::new();
        let mut clut = 0;
        let mut next = 1;
        for (index, vertex) in vertices.iter_mut().take(count).enumerate() {
            let color = if index > 0 && gouraud {
                next += 1;
                words[next - 1]
            } else {
                words[0]
            };
            let position = words[next];
            next += 1;
            let mut uv = 0;
            if op & 0x04 != 0 {
                uv = words[next];
                next += 1;
                if index == 0 {
                    clut = (uv >> 16) as u16;
                }
            }
            let (traced, class) = self.trace_vertex(trace, position, color, uv);
            *vertex = traced;
            classes.extend(class);
        }
        let projection = classes.first().map(|&(_, offset, h, _)| (offset, h));
        let owner = classes.first().map_or(0, |c| c.3);
        let classes: Vec<Class> = classes.iter().map(|c| c.0).collect();
        let class = primitive_class(&classes, count);
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
            environment: self.trace_environment(),
            class,
            projection,
            owner,
            source: 0,
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
        let (corner, _) = self.trace_vertex(trace, words[1], words[0], uv);
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
            source: 0,
        };
        self.trace_drawn(primitive);
    }

    /// A line segment (also each segment of a polyline).
    pub(crate) fn trace_line(&mut self, command: u32, color0: u32, vertex0: u32, color1: u32, vertex1: u32) {
        let Some(trace) = self.trace.as_ref() else { return };
        let op = command >> 24;
        let gouraud = op & 0x10 != 0;
        let (a, class_a) = self.trace_vertex(trace, vertex0, color0, 0);
        let (b, class_b) = self.trace_vertex(trace, vertex1, if gouraud { color1 } else { color0 }, 0);
        let projection = class_a.or(class_b).map(|(_, offset, h, _)| (offset, h));
        let owner = class_a.or(class_b).map_or(0, |c| c.3);
        let classes: Vec<Class> = class_a.into_iter().chain(class_b).map(|c| c.0).collect();
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
            source: 0,
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
