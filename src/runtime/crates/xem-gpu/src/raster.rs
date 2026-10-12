//! Software rasterization of the GP0 drawing commands.
//!
//! Rules (psx-spx, GPU Rendering/Render commands):
//! - Vertex coordinates are signed 11-bit (-1024..1023) and the drawing offset
//!   (GP0(E5h), also signed 11-bit) is added to them.
//! - A triangle whose extent is 1024 or more horizontally or 512 or more
//!   vertically is not drawn; a quad is the two triangles 1-2-3 and 2-3-4,
//!   each tested separately. Lines have the same limits.
//! - Pixels are sampled at integer coordinates with a top-left fill rule, so
//!   a polygon's right column and bottom row are not drawn and triangles
//!   sharing an edge do not overlap.
//! - Everything is clipped to the drawing area (GP0(E3h)/GP0(E4h), inclusive);
//!   the fill command ignores the drawing area, offset and mask settings.
//! - Texel 0000h is transparent. Semi-transparency applies to every pixel of
//!   an untextured primitive and to texels with bit 15 set; modes are
//!   B/2+F/2, B+F, B-F and B+F/4 per 5-bit channel, saturated.
//! - Modulated textures multiply each texel channel by the vertex colour
//!   /128 (80h is neutral); raw textures use the texel unchanged.
//! - Dithering (GP0(E1h).9) applies to Gouraud-shaded primitives and to
//!   modulated textured polygons, never to flat untextured ones, raw
//!   textures, rectangles or fills.
//! - The written pixel's bit 15 is the texel's bit 15 (0 untextured), forced
//!   to 1 by GP0(E6h).0; with GP0(E6h).1 pixels whose bit 15 is set are kept.

use crate::{Gpu, PrimitiveKind, VRAM_HEIGHT, VRAM_WIDTH};

/// psx-spx dither offsets, indexed by `[y & 3][x & 3]`.
const DITHER: [[i32; 4]; 4] = [
    [-4, 0, -3, 1],
    [2, -2, 3, -1],
    [-3, 1, -4, 0],
    [3, -1, 2, -2],
];

fn sign11(value: u32) -> i32 {
    ((value << 21) as i32) >> 21
}

#[derive(Clone, Copy, Default)]
struct Vertex {
    x: i32,
    y: i32,
    /// r, g, b, u, v.
    attributes: [i32; 5],
}

/// How a primitive's pixels are coloured.
#[derive(Clone, Copy)]
struct Shader {
    textured: bool,
    raw: bool,
    gouraud: bool,
    semi: Option<u32>,
    dither: bool,
    /// Texture page base X (pixels), base Y, colour depth (0 4-bit, 1 8-bit, 2/3 15-bit).
    page_x: usize,
    page_y: usize,
    depth: u32,
    clut_x: usize,
    clut_y: usize,
    /// The texture window (GP0(E2h)) as masks: u and v are kept by `keep`
    /// and given `set`.
    keep: (u32, u32),
    set: (u32, u32),
}

/// floor(n / d) and n - d * floor(n / d) for d > 0 (div_euclid and
/// rem_euclid), from a float division set right by whole steps: exact for
/// the magnitudes here (far below 2^52), without a 64-bit integer division.
#[inline(always)]
fn floor_div(n: i64, d: i64) -> (i64, i64) {
    let mut q = (n as f64 / d as f64) as i64;
    let mut r = n - q * d;
    while r < 0 {
        q -= 1;
        r += d;
    }
    while r >= d {
        q += 1;
        r -= d;
    }
    (q, r)
}

fn color_attributes(color: u32) -> [i32; 3] {
    [
        (color & 0xFF) as i32,
        (color >> 8 & 0xFF) as i32,
        (color >> 16 & 0xFF) as i32,
    ]
}

/// 8-bit channel to 5 bits, with the dither offset when enabled.
#[inline(always)]
fn to5(value: i32, dither: Option<i32>) -> u16 {
    let value = value + dither.unwrap_or(0);
    (value.clamp(0, 255) >> 3) as u16
}

#[inline(always)]
fn blend(back: u16, front: u16, mode: u32) -> u16 {
    let mut out = 0;
    for shift in [0, 5, 10] {
        let b = i32::from(back >> shift & 31);
        let f = i32::from(front >> shift & 31);
        let c = match mode {
            0 => (b + f) >> 1,
            1 => b + f,
            2 => b - f,
            _ => b + (f >> 2),
        };
        out |= (c.clamp(0, 31) as u16) << shift;
    }
    out
}

/// Floor division state of `numerator / denominator` stepped by a constant.
#[derive(Clone, Copy, Default)]
struct Stepper {
    quotient: i64,
    remainder: i64,
}

impl Stepper {
    #[inline(always)]
    fn new(numerator: i64, denominator: i64) -> Stepper {
        let (quotient, remainder) = floor_div(numerator, denominator);
        Stepper { quotient, remainder }
    }

    #[inline(always)]
    fn step(&mut self, by: Stepper, denominator: i64) {
        self.quotient += by.quotient;
        self.remainder += by.remainder;
        if self.remainder >= denominator {
            self.remainder -= denominator;
            self.quotient += 1;
        }
    }
}

/// The rows of a triangle as the GPU fills them: pixels at integer
/// coordinates, top-left rule, within `bounds` (left, top, right, bottom,
/// inclusive: the drawing area). Each row is its y and the first and last x
/// drawn (none when first > last).
pub struct TriangleRows {
    /// Per edge: its function at (x0, y0) less the fill rule's bias, and its
    /// steps along x and y (inside where >= 0).
    edges: [(i64, i64, i64); 3],
    /// Per edge with a step along x: its function over that step's size,
    /// rounded down, at the current row, and the step per row.
    bounds: [(Stepper, Stepper); 3],
    x0: i32,
    x1: i32,
    y0: i32,
    y1: i32,
    y: i32,
}

impl TriangleRows {
    /// None for a triangle without area or outside `bounds`. The size limits
    /// are the caller's.
    pub fn new(points: [(i32, i32); 3], bounds: (i32, i32, i32, i32)) -> Option<TriangleRows> {
        let mut v = points;
        let cross = |a: (i32, i32), b: (i32, i32), c: (i32, i32)| {
            i64::from(b.0 - a.0) * i64::from(c.1 - a.1) - i64::from(b.1 - a.1) * i64::from(c.0 - a.0)
        };
        let area = cross(v[0], v[1], v[2]);
        if area == 0 {
            return None;
        }
        if area < 0 {
            v.swap(1, 2);
        }
        let (left, top, right, bottom) = bounds;
        let (x0, x1) = (v.iter().map(|p| p.0).min()?.max(left), v.iter().map(|p| p.0).max()?.min(right));
        let (y0, y1) = (v.iter().map(|p| p.1).min()?.max(top), v.iter().map(|p| p.1).max()?.min(bottom));
        if x0 > x1 || y0 > y1 {
            return None;
        }
        // Edge a->b: w(p) = (b.x-a.x)(p.y-a.y) - (b.y-a.y)(p.x-a.x), positive
        // inside. A pixel on an edge is drawn only if the edge is a left edge
        // (going up) or a top edge (horizontal, going right).
        let mut edges = [(0i64, 0i64, 0i64); 3];
        for (edge, (a, b)) in edges.iter_mut().zip([(0, 1), (1, 2), (2, 0)]) {
            let (a, b) = (v[a], v[b]);
            let dx = i64::from(b.0 - a.0);
            let dy = i64::from(b.1 - a.1);
            let top_left = dy < 0 || (dy == 0 && dx > 0);
            let at_origin = dx * i64::from(y0 - a.1) - dy * i64::from(x0 - a.0);
            *edge = (at_origin - i64::from(!top_left), -dy, dx);
        }
        let bounds = edges.map(|(origin, step_x, step_y)| match step_x.abs() {
            0 => (Stepper::default(), Stepper::default()),
            size => (Stepper::new(origin, size), Stepper::new(step_y, size)),
        });
        Some(TriangleRows { edges, bounds, x0, x1, y0, y1, y: y0 })
    }
}

impl Iterator for TriangleRows {
    type Item = (i32, i32, i32);

    fn next(&mut self) -> Option<(i32, i32, i32)> {
        if self.y > self.y1 {
            return None;
        }
        let y = self.y;
        self.y += 1;
        // Every edge function is >= 0 from `first` to `last` (an edge
        // function is linear in x): w / |step x| rounded down, stepped from
        // row to row.
        let row = i64::from(y - self.y0);
        let (mut first, mut last) = (i64::from(self.x0), i64::from(self.x1));
        for (&(origin, step_x, step_y), (bound, step)) in self.edges.iter().zip(&mut self.bounds) {
            match step_x.signum() {
                1 => first = first.max(i64::from(self.x0) - bound.quotient),
                -1 => last = last.min(i64::from(self.x0) + bound.quotient),
                _ if origin + step_y * row < 0 => last = first - 1,
                _ => {}
            }
            if step_x != 0 {
                bound.step(*step, step_x.abs());
            }
        }
        Some((y, first as i32, last.max(first - 1) as i32))
    }
}
impl Gpu {
    pub(crate) fn vertex(&self, word: u32) -> (i32, i32) {
        (
            sign11(word) + sign11(self.draw_offset),
            sign11(word >> 16) + sign11(self.draw_offset >> 11),
        )
    }

    /// Drawing area as inclusive (left, top, right, bottom).
    pub(crate) fn drawing_area(&self) -> (i32, i32, i32, i32) {
        (
            (self.area_top_left & 0x3FF) as i32,
            (self.area_top_left >> 10 & 0x1FF) as i32,
            (self.area_bottom_right & 0x3FF) as i32,
            (self.area_bottom_right >> 10 & 0x1FF) as i32,
        )
    }

    pub(crate) fn texture_disabled(&self) -> bool {
        self.texture_disable_allowed && self.draw_mode & 0x800 != 0
    }

    fn shader(&self, textured: bool, raw: bool, gouraud: bool, semi: bool, clut: u32) -> Shader {
        let mode = self.draw_mode;
        let dither = mode & 0x200 != 0 && (gouraud || (textured && !raw));
        let window = self.texture_window;
        let (mask_x, mask_y) = (window & 0x1F, window >> 5 & 0x1F);
        let (offset_x, offset_y) = (window >> 10 & 0x1F, window >> 15 & 0x1F);
        Shader {
            textured,
            raw,
            gouraud,
            semi: semi.then_some(mode >> 5 & 3),
            dither,
            page_x: (mode & 0xF) as usize * 64,
            page_y: (mode >> 4 & 1) as usize * 256,
            depth: mode >> 7 & 3,
            clut_x: (clut & 0x3F) as usize * 16,
            clut_y: (clut >> 6 & 0x1FF) as usize,
            keep: (0xFF & !(mask_x * 8), 0xFF & !(mask_y * 8)),
            set: ((offset_x & mask_x) * 8, (offset_y & mask_y) * 8),
        }
    }

    #[inline(always)]
    fn texel(&self, shader: &Shader, u: i32, v: i32) -> u16 {
        let u = (u as u32 & shader.keep.0) | shader.set.0;
        let v = (v as u32 & shader.keep.1) | shader.set.1;
        let row = (shader.page_y + v as usize) & (VRAM_HEIGHT - 1);
        let at = |x: usize, y: usize| self.vram[y * VRAM_WIDTH + (x & (VRAM_WIDTH - 1))];
        match shader.depth {
            0 => {
                let word = at(shader.page_x + (u as usize >> 2), row);
                let index = (word >> ((u & 3) * 4)) & 0xF;
                at(shader.clut_x + index as usize, shader.clut_y)
            }
            1 => {
                let word = at(shader.page_x + (u as usize >> 1), row);
                let index = (word >> ((u & 1) * 8)) & 0xFF;
                at(shader.clut_x + index as usize, shader.clut_y)
            }
            _ => at(shader.page_x + u as usize, row),
        }
    }

    /// Colours and writes one pixel already known to be inside the drawing
    /// area. `attributes` are r, g, b, u, v.
    #[inline(always)]
    fn shade(&mut self, x: i32, y: i32, attributes: &[i32; 5], shader: &Shader) {
        let dither = shader
            .dither
            .then(|| DITHER[(y & 3) as usize][(x & 3) as usize]);
        let (color, mask_bit, semi) = if shader.textured {
            let texel = self.texel(shader, attributes[3], attributes[4]);
            if texel == 0 {
                return;
            }
            let opaque = texel & 0x8000 == 0;
            let color = if shader.raw {
                texel & 0x7FFF
            } else {
                let mut color = 0;
                for (channel, shift) in [0, 5, 10].into_iter().enumerate() {
                    let t = i32::from(texel >> shift & 31) << 3;
                    color |= to5((t * attributes[channel]) >> 7, dither) << shift;
                }
                color
            };
            (color, !opaque, if opaque { None } else { shader.semi })
        } else {
            let color = to5(attributes[0], dither)
                | to5(attributes[1], dither) << 5
                | to5(attributes[2], dither) << 10;
            (color, false, shader.semi)
        };
        let index = (y as usize & (VRAM_HEIGHT - 1)) * VRAM_WIDTH + (x as usize & (VRAM_WIDTH - 1));
        let back = self.vram[index];
        if self.mask & 2 != 0 && back & 0x8000 != 0 {
            return;
        }
        let color = match semi {
            Some(mode) => blend(back, color, mode),
            None => color,
        };
        self.vram[index] = color
            | if mask_bit || self.mask & 1 != 0 {
                0x8000
            } else {
                0
            };
    }

    /// GP0(02h): fills a rectangle with a colour, ignoring the drawing area,
    /// offset and mask settings. X and width are in 16-pixel units (width
    /// rounded up); the rectangle wraps at the VRAM edges.
    pub(crate) fn fill_rectangle(&mut self, words: &[u32]) {
        let color = words[0];
        let pixel =
            ((color >> 3 & 0x1F) | (color >> 11 & 0x1F) << 5 | (color >> 19 & 0x1F) << 10) as u16;
        let x = (words[1] & 0x3F0) as usize;
        let y = (words[1] >> 16 & 0x1FF) as usize;
        let width = (((words[2] & 0x3FF) + 0xF) & !0xF) as usize;
        let height = (words[2] >> 16 & 0x1FF) as usize;
        for row in 0..height {
            let base = ((y + row) & (VRAM_HEIGHT - 1)) * VRAM_WIDTH;
            for column in 0..width {
                self.vram[base + ((x + column) & (VRAM_WIDTH - 1))] = pixel;
            }
        }
    }

    /// GP0(80h): copies a VRAM rectangle, wrapping at the edges, with the
    /// mask settings applied to the destination.
    pub(crate) fn copy_rectangle(&mut self, words: &[u32]) {
        let source = crate::Image::from_words(words[1], words[3]);
        let destination = crate::Image::from_words(words[2], words[3]);
        let width = u32::from(source.width);
        let mut row = vec![0u16; width as usize];
        for line in 0..u32::from(source.height) {
            for (column, pixel) in row.iter_mut().enumerate() {
                *pixel = self.vram[source.vram_index(line * width + column as u32)];
            }
            for (column, &pixel) in row.iter().enumerate() {
                self.write_masked(destination.vram_index(line * width + column as u32), pixel);
            }
        }
    }

    /// GP0(20h)-GP0(3Fh).
    pub(crate) fn draw_polygon(&mut self, words: &[u32]) {
        let op = words[0] >> 24;
        let gouraud = op & 0x10 != 0;
        let count = if op & 0x08 != 0 { 4 } else { 3 };
        let mut textured = op & 0x04 != 0;
        let semi = op & 0x02 != 0;
        let raw = op & 0x01 != 0;
        let mut vertices = [Vertex::default(); 4];
        let (mut clut, mut page) = (0, 0);
        let mut next = 1;
        for (index, vertex) in vertices.iter_mut().take(count).enumerate() {
            let color = if index > 0 && gouraud {
                next += 1;
                words[next - 1]
            } else {
                words[0]
            };
            let (x, y) = self.vertex(words[next]);
            next += 1;
            let [r, g, b] = color_attributes(color);
            let (mut u, mut v) = (0, 0);
            if textured {
                let word = words[next];
                next += 1;
                match index {
                    0 => clut = word >> 16,
                    1 => page = word >> 16,
                    _ => {}
                }
                u = (word & 0xFF) as i32;
                v = (word >> 8 & 0xFF) as i32;
            }
            *vertex = Vertex {
                x,
                y,
                attributes: [r, g, b, u, v],
            };
        }
        if textured {
            // psx-spx: the polygon's texpage sets GP0(E1h) bits 0-8 and 11.
            self.draw_mode = (self.draw_mode & !0x9FF) | (page & 0x9FF);
        }
        textured &= !self.texture_disabled();
        self.stats.add(match (gouraud, textured) {
            (false, false) => PrimitiveKind::PolygonFlat,
            (true, false) => PrimitiveKind::PolygonGouraud,
            (false, true) => PrimitiveKind::PolygonTextured,
            (true, true) => PrimitiveKind::PolygonGouraudTextured,
        });
        let shader = self.shader(textured, raw, gouraud, semi, clut);
        self.draw_triangle([vertices[0], vertices[1], vertices[2]], &shader);
        if count == 4 {
            self.draw_triangle([vertices[1], vertices[2], vertices[3]], &shader);
        }
    }

    fn draw_triangle(&mut self, mut v: [Vertex; 3], shader: &Shader) {
        let min_x = v.iter().map(|p| p.x).min().unwrap_or(0);
        let max_x = v.iter().map(|p| p.x).max().unwrap_or(0);
        let min_y = v.iter().map(|p| p.y).min().unwrap_or(0);
        let max_y = v.iter().map(|p| p.y).max().unwrap_or(0);
        if max_x - min_x >= 1024 || max_y - min_y >= 512 {
            self.stats.rejected += 1;
            return;
        }
        let cross = |a: &Vertex, b: &Vertex, c: &Vertex| {
            i64::from(b.x - a.x) * i64::from(c.y - a.y)
                - i64::from(b.y - a.y) * i64::from(c.x - a.x)
        };
        let mut area = cross(&v[0], &v[1], &v[2]);
        if area == 0 {
            return;
        }
        if area < 0 {
            v.swap(1, 2);
            area = -area;
        }
        let Some(rows) = TriangleRows::new(v.map(|p| (p.x, p.y)), self.drawing_area()) else { return };

        // Attribute a at p: (a0*area + nx*(p.x-x0') + ny*(p.y-y0')) / area,
        // rounded down, with x0', y0' the first vertex.
        let interpolate = shader.gouraud || shader.textured;
        let mut gradients = [(0i64, 0i64); 5];
        if interpolate {
            let (dx1, dy1) = (i64::from(v[1].x - v[0].x), i64::from(v[1].y - v[0].y));
            let (dx2, dy2) = (i64::from(v[2].x - v[0].x), i64::from(v[2].y - v[0].y));
            for (k, gradient) in gradients.iter_mut().enumerate() {
                let da1 = i64::from(v[1].attributes[k] - v[0].attributes[k]);
                let da2 = i64::from(v[2].attributes[k] - v[0].attributes[k]);
                *gradient = (da1 * dy2 - da2 * dy1, da2 * dx1 - da1 * dx2);
            }
        }
        let steps = gradients.map(|(nx, _)| Stepper::new(nx, area));
        // Only what varies is stepped: a flat-shaded polygon's colour and an
        // untextured one's coordinates stay their first vertex's.
        match (shader.gouraud, shader.textured) {
            (false, false) => self.fill_rows::<false, false>(rows, &v, area, &gradients, &steps, shader),
            (false, true) => self.fill_rows::<false, true>(rows, &v, area, &gradients, &steps, shader),
            (true, false) => self.fill_rows::<true, false>(rows, &v, area, &gradients, &steps, shader),
            (true, true) => self.fill_rows::<true, true>(rows, &v, area, &gradients, &steps, shader),
        }
    }

    /// A triangle's rows, its colour interpolated with `GOURAUD` and its
    /// texture coordinates with `TEXTURED` (attribute a at p: (a0*area +
    /// nx*(p.x-x0) + ny*(p.y-y0)) / area, rounded down, with x0, y0 the first
    /// vertex).
    #[allow(clippy::too_many_arguments)]
    fn fill_rows<const GOURAUD: bool, const TEXTURED: bool>(
        &mut self,
        rows: TriangleRows,
        v: &[Vertex; 3],
        area: i64,
        gradients: &[(i64, i64); 5],
        steps: &[Stepper; 5],
        shader: &Shader,
    ) {
        let varying = match (GOURAUD, TEXTURED) {
            (true, true) => 0..5,
            (true, false) => 0..3,
            (false, true) => 3..5,
            (false, false) => 0..0,
        };
        let mut attributes = v[0].attributes;
        for (y, first, last) in rows {
            if first > last {
                continue;
            }
            let mut values = [Stepper::default(); 5];
            for k in varying.clone() {
                let (nx, ny) = gradients[k];
                let numerator = i64::from(v[0].attributes[k]) * area + nx * (i64::from(first) - i64::from(v[0].x)) + ny * i64::from(y - v[0].y);
                values[k] = Stepper::new(numerator, area);
            }
            for x in first..=last {
                for k in varying.clone() {
                    attributes[k] = values[k].quotient as i32;
                    values[k].step(steps[k], area);
                }
                if GOURAUD {
                    for channel in &mut attributes[..3] {
                        *channel = (*channel).clamp(0, 255);
                    }
                }
                self.shade(x, y, &attributes, shader);
            }
        }
    }

    /// One line segment from `vertex0` to `vertex1`; `command` selects
    /// Gouraud shading and semi-transparency. Both end points are drawn.
    pub(crate) fn draw_line(
        &mut self,
        command: u32,
        color0: u32,
        vertex0: u32,
        color1: u32,
        vertex1: u32,
    ) {
        let op = command >> 24;
        let gouraud = op & 0x10 != 0;
        let shader = self.shader(false, false, gouraud, op & 0x02 != 0, 0);
        let (x0, y0) = self.vertex(vertex0);
        let (x1, y1) = self.vertex(vertex1);
        let (dx, dy) = (x1 - x0, y1 - y0);
        if dx.abs() >= 1024 || dy.abs() >= 512 {
            self.stats.rejected += 1;
            return;
        }
        self.trace_line(command, color0, vertex0, color1, vertex1);
        let start = color_attributes(color0);
        let end = if gouraud {
            color_attributes(color1)
        } else {
            start
        };
        let (left, top, right, bottom) = self.drawing_area();
        let steps = dx.abs().max(dy.abs());
        let along = |from: i32, delta: i32, i: i32| {
            if steps == 0 {
                from
            } else {
                // Nearest pixel, halves rounded up.
                from + (2 * delta * i + steps).div_euclid(2 * steps)
            }
        };
        for i in 0..=steps {
            let x = along(x0, dx, i);
            let y = along(y0, dy, i);
            if x < left || x > right || y < top || y > bottom {
                continue;
            }
            let mut attributes = [0; 5];
            for k in 0..3 {
                attributes[k] = if steps == 0 {
                    start[k]
                } else {
                    start[k] + ((end[k] - start[k]) * i).div_euclid(steps)
                };
            }
            self.shade(x, y, &attributes, &shader);
        }
    }

    /// GP0(60h)-GP0(7Fh): rectangles and sprites. They use the texture page
    /// and the flip bits of GP0(E1h) and are never dithered.
    pub(crate) fn draw_rectangle(&mut self, words: &[u32]) {
        let op = words[0] >> 24;
        let mut textured = op & 0x04 != 0;
        let (x, y) = self.vertex(words[1]);
        let mut next = 2;
        let (mut u0, mut v0, mut clut) = (0, 0, 0);
        if textured {
            let word = words[next];
            next += 1;
            u0 = (word & 0xFF) as i32;
            v0 = (word >> 8 & 0xFF) as i32;
            clut = word >> 16;
        }
        let (width, height) = match op >> 3 & 3 {
            0 => (
                (words[next] & 0x3FF) as i32,
                (words[next] >> 16 & 0x1FF) as i32,
            ),
            1 => (1, 1),
            2 => (8, 8),
            _ => (16, 16),
        };
        textured &= !self.texture_disabled();
        self.stats.add(if textured {
            PrimitiveKind::RectangleTextured
        } else {
            PrimitiveKind::Rectangle
        });
        let mut shader = self.shader(textured, op & 0x01 != 0, false, op & 0x02 != 0, clut);
        shader.dither = false;
        let flip_x = self.draw_mode & 0x1000 != 0;
        let flip_y = self.draw_mode & 0x2000 != 0;
        let (left, top, right, bottom) = self.drawing_area();
        let [r, g, b] = color_attributes(words[0]);
        for py in y.max(top)..=(y + height - 1).min(bottom) {
            let dv = py - y;
            let v = if flip_y { v0 - dv } else { v0 + dv };
            for px in x.max(left)..=(x + width - 1).min(right) {
                let du = px - x;
                let u = if flip_x { u0 - du } else { u0 + du };
                self.shade(px, py, &[r, g, b, u & 0xFF, v & 0xFF], &shader);
            }
        }
    }
}
