//! The fill loops against the rendering rules applied a pixel at a time.
//!
//! `Model` is the rules of `xem_gpu`'s rasterizer in their plainest form:
//! every pixel of a primitive's bounding box is tested against the edge
//! functions, its attributes come from one exact division each, and it is
//! coloured and written by the documented steps. Random scenes (state,
//! VRAM and primitives of every kind) are drawn by it and by the GPU, whose
//! loops step and specialize all of it; VRAM must agree to the last bit
//! after every command.

use xem_gpu::Gpu;

const WIDTH: usize = 1024;
const HEIGHT: usize = 512;

const DITHER: [[i32; 4]; 4] = [[-4, 0, -3, 1], [2, -2, 3, -1], [-3, 1, -4, 0], [3, -1, 2, -2]];

/// xorshift64*: the same scenes on every host.
struct Random(u64);

impl Random {
    fn next(&mut self) -> u64 {
        self.0 ^= self.0 >> 12;
        self.0 ^= self.0 << 25;
        self.0 ^= self.0 >> 27;
        self.0.wrapping_mul(0x2545_F491_4F6C_DD1D)
    }

    fn below(&mut self, limit: u32) -> u32 {
        ((self.next() >> 32) * u64::from(limit) >> 32) as u32
    }

    fn range(&mut self, low: i32, high: i32) -> i32 {
        low + self.below((high - low + 1) as u32) as i32
    }

    fn one_in(&mut self, n: u32) -> bool {
        self.below(n) == 0
    }
}

fn sign11(value: u32) -> i32 {
    ((value << 21) as i32) >> 21
}

#[derive(Clone, Copy, Default)]
struct Vertex {
    x: i64,
    y: i64,
    /// r, g, b, u, v.
    attributes: [i64; 5],
}

struct Shader {
    textured: bool,
    raw: bool,
    gouraud: bool,
    semi: Option<u32>,
    dither: bool,
    clut: u32,
}

/// The drawing state the rules read, and VRAM.
struct Model {
    vram: Vec<u16>,
    draw_mode: u32,
    texture_window: u32,
    area_top_left: u32,
    area_bottom_right: u32,
    draw_offset: u32,
    mask: u32,
    texture_disable_allowed: bool,
}

impl Model {
    fn new(vram: Vec<u16>) -> Model {
        Model { vram, draw_mode: 0, texture_window: 0, area_top_left: 0, area_bottom_right: 0, draw_offset: 0, mask: 0, texture_disable_allowed: false }
    }

    fn vertex(&self, word: u32) -> (i64, i64) {
        (i64::from(sign11(word) + sign11(self.draw_offset)), i64::from(sign11(word >> 16) + sign11(self.draw_offset >> 11)))
    }

    /// The drawing area, inclusive: left, top, right, bottom.
    fn area(&self) -> (i64, i64, i64, i64) {
        (
            i64::from(self.area_top_left & 0x3FF),
            i64::from(self.area_top_left >> 10 & 0x1FF),
            i64::from(self.area_bottom_right & 0x3FF),
            i64::from(self.area_bottom_right >> 10 & 0x1FF),
        )
    }

    fn at(&self, x: usize, y: usize) -> u16 {
        self.vram[y * WIDTH + (x & (WIDTH - 1))]
    }

    fn texel(&self, shader: &Shader, u: i64, v: i64) -> u16 {
        let window = self.texture_window;
        let (mask_x, mask_y) = (window & 0x1F, window >> 5 & 0x1F);
        let (offset_x, offset_y) = (window >> 10 & 0x1F, window >> 15 & 0x1F);
        let u = ((u as u32 & 0xFF & !(mask_x * 8)) | ((offset_x & mask_x) * 8)) as usize;
        let v = ((v as u32 & 0xFF & !(mask_y * 8)) | ((offset_y & mask_y) * 8)) as usize;
        let mode = self.draw_mode;
        let (page_x, page_y) = ((mode & 0xF) as usize * 64, (mode >> 4 & 1) as usize * 256);
        let (clut_x, clut_y) = ((shader.clut & 0x3F) as usize * 16, (shader.clut >> 6 & 0x1FF) as usize);
        let row = (page_y + v) & (HEIGHT - 1);
        match mode >> 7 & 3 {
            0 => {
                let index = self.at(page_x + (u >> 2), row) >> ((u & 3) * 4) & 0xF;
                self.at(clut_x + usize::from(index), clut_y)
            }
            1 => {
                let index = self.at(page_x + (u >> 1), row) >> ((u & 1) * 8) & 0xFF;
                self.at(clut_x + usize::from(index), clut_y)
            }
            _ => self.at(page_x + u, row),
        }
    }

    /// Colours and writes the pixel at (x, y), inside the drawing area.
    fn shade(&mut self, x: i64, y: i64, attributes: &[i64; 5], shader: &Shader) {
        let dither = if shader.dither { DITHER[(y & 3) as usize][(x & 3) as usize] } else { 0 };
        let to5 = |value: i64| ((value + i64::from(dither)).clamp(0, 255) >> 3) as u16;
        let (color, mask_bit, semi) = if shader.textured {
            let texel = self.texel(shader, attributes[3], attributes[4]);
            if texel == 0 {
                return;
            }
            let translucent = texel & 0x8000 != 0;
            let color = if shader.raw {
                texel & 0x7FFF
            } else {
                (0..3).map(|channel| to5((i64::from(texel >> (channel * 5) & 31) << 3) * attributes[channel] >> 7) << (channel * 5)).sum()
            };
            (color, translucent, if translucent { shader.semi } else { None })
        } else {
            (to5(attributes[0]) | to5(attributes[1]) << 5 | to5(attributes[2]) << 10, false, shader.semi)
        };
        let index = (y as usize & (HEIGHT - 1)) * WIDTH + (x as usize & (WIDTH - 1));
        let back = self.vram[index];
        if self.mask & 2 != 0 && back & 0x8000 != 0 {
            return;
        }
        let color = match semi {
            Some(mode) => (0..3)
                .map(|channel| {
                    let b = i32::from(back >> (channel * 5) & 31);
                    let f = i32::from(color >> (channel * 5) & 31);
                    let mixed = match mode {
                        0 => (b + f) >> 1,
                        1 => b + f,
                        2 => b - f,
                        _ => b + (f >> 2),
                    };
                    (mixed.clamp(0, 31) as u16) << (channel * 5)
                })
                .sum(),
            None => color,
        };
        self.vram[index] = color | if mask_bit || self.mask & 1 != 0 { 0x8000 } else { 0 };
    }

    fn shader(&self, textured: bool, raw: bool, gouraud: bool, semi: bool, clut: u32) -> Shader {
        let mode = self.draw_mode;
        Shader { textured, raw, gouraud, semi: semi.then_some(mode >> 5 & 3), dither: mode & 0x200 != 0 && (gouraud || (textured && !raw)), clut }
    }

    fn texture_disabled(&self) -> bool {
        self.texture_disable_allowed && self.draw_mode & 0x800 != 0
    }

    fn triangle(&mut self, mut v: [Vertex; 3], shader: &Shader) {
        let (min_x, max_x) = (v.iter().map(|p| p.x).min().unwrap(), v.iter().map(|p| p.x).max().unwrap());
        let (min_y, max_y) = (v.iter().map(|p| p.y).min().unwrap(), v.iter().map(|p| p.y).max().unwrap());
        if max_x - min_x >= 1024 || max_y - min_y >= 512 {
            return;
        }
        let cross = |a: &Vertex, b: &Vertex, x: i64, y: i64| (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
        let mut area = cross(&v[0], &v[1], v[2].x, v[2].y);
        if area == 0 {
            return;
        }
        if area < 0 {
            v.swap(1, 2);
            area = -area;
        }
        let (left, top, right, bottom) = self.area();
        for y in min_y.max(top)..=max_y.min(bottom) {
            for x in min_x.max(left)..=max_x.min(right) {
                // Inside every edge; on an edge only if it is a left edge
                // (going up) or a top edge (horizontal, going right).
                let inside = [(0, 1), (1, 2), (2, 0)].iter().all(|&(i, j)| {
                    let (a, b) = (&v[i], &v[j]);
                    let w = cross(a, b, x, y);
                    w > 0 || (w == 0 && (b.y - a.y < 0 || (b.y == a.y && b.x - a.x > 0)))
                });
                if !inside {
                    continue;
                }
                // Each attribute on the plane through the three vertices,
                // rounded down.
                let mut attributes = v[0].attributes;
                for (k, attribute) in attributes.iter_mut().enumerate() {
                    let varies = if k < 3 { shader.gouraud } else { shader.textured };
                    if varies {
                        let da1 = v[1].attributes[k] - v[0].attributes[k];
                        let da2 = v[2].attributes[k] - v[0].attributes[k];
                        let nx = da1 * (v[2].y - v[0].y) - da2 * (v[1].y - v[0].y);
                        let ny = da2 * (v[1].x - v[0].x) - da1 * (v[2].x - v[0].x);
                        *attribute = (v[0].attributes[k] * area + nx * (x - v[0].x) + ny * (y - v[0].y)).div_euclid(area);
                    }
                }
                self.shade(x, y, &attributes, shader);
            }
        }
    }

    fn polygon(&mut self, words: &[u32]) {
        let op = words[0] >> 24;
        let gouraud = op & 0x10 != 0;
        let count = if op & 0x08 != 0 { 4 } else { 3 };
        let mut textured = op & 0x04 != 0;
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
            let (mut u, mut v) = (0, 0);
            if textured {
                let word = words[next];
                next += 1;
                match index {
                    0 => clut = word >> 16,
                    1 => page = word >> 16,
                    _ => {}
                }
                u = i64::from(word & 0xFF);
                v = i64::from(word >> 8 & 0xFF);
            }
            *vertex = Vertex { x, y, attributes: [i64::from(color & 0xFF), i64::from(color >> 8 & 0xFF), i64::from(color >> 16 & 0xFF), u, v] };
        }
        if textured {
            self.draw_mode = (self.draw_mode & !0x9FF) | (page & 0x9FF);
        }
        textured &= !self.texture_disabled();
        let shader = self.shader(textured, op & 0x01 != 0, gouraud, op & 0x02 != 0, clut);
        self.triangle([vertices[0], vertices[1], vertices[2]], &shader);
        if count == 4 {
            self.triangle([vertices[1], vertices[2], vertices[3]], &shader);
        }
    }

    fn rectangle(&mut self, words: &[u32]) {
        let op = words[0] >> 24;
        let mut textured = op & 0x04 != 0;
        let (x, y) = self.vertex(words[1]);
        let mut next = 2;
        let (mut u0, mut v0, mut clut) = (0, 0, 0);
        if textured {
            u0 = i64::from(words[next] & 0xFF);
            v0 = i64::from(words[next] >> 8 & 0xFF);
            clut = words[next] >> 16;
            next += 1;
        }
        let (width, height) = match op >> 3 & 3 {
            0 => (i64::from(words[next] & 0x3FF), i64::from(words[next] >> 16 & 0x1FF)),
            1 => (1, 1),
            2 => (8, 8),
            _ => (16, 16),
        };
        textured &= !self.texture_disabled();
        let mut shader = self.shader(textured, op & 0x01 != 0, false, op & 0x02 != 0, clut);
        shader.dither = false;
        let (flip_x, flip_y) = (self.draw_mode & 0x1000 != 0, self.draw_mode & 0x2000 != 0);
        let (left, top, right, bottom) = self.area();
        let color = words[0];
        for py in y.max(top)..=(y + height - 1).min(bottom) {
            let v = if flip_y { v0 - (py - y) } else { v0 + (py - y) };
            for px in x.max(left)..=(x + width - 1).min(right) {
                let u = if flip_x { u0 - (px - x) } else { u0 + (px - x) };
                self.shade(px, py, &[i64::from(color & 0xFF), i64::from(color >> 8 & 0xFF), i64::from(color >> 16 & 0xFF), u & 0xFF, v & 0xFF], &shader);
            }
        }
    }

    /// One line segment (the polyline's continuation is not modelled).
    fn line(&mut self, words: &[u32]) {
        let op = words[0] >> 24;
        let gouraud = op & 0x10 != 0;
        let (color1, vertex1) = if gouraud { (words[2], words[3]) } else { (words[0], words[2]) };
        let shader = self.shader(false, false, gouraud, op & 0x02 != 0, 0);
        let (x0, y0) = self.vertex(words[1]);
        let (x1, y1) = self.vertex(vertex1);
        let (dx, dy) = (x1 - x0, y1 - y0);
        if dx.abs() >= 1024 || dy.abs() >= 512 {
            return;
        }
        let channels = |color: u32| [i64::from(color & 0xFF), i64::from(color >> 8 & 0xFF), i64::from(color >> 16 & 0xFF)];
        let (start, end) = (channels(words[0]), channels(color1));
        let (left, top, right, bottom) = self.area();
        let steps = dx.abs().max(dy.abs());
        for i in 0..=steps {
            // The nearest pixel along the longer axis, halves rounded up.
            let along = |from: i64, delta: i64| if steps == 0 { from } else { from + (2 * delta * i + steps).div_euclid(2 * steps) };
            let (x, y) = (along(x0, dx), along(y0, dy));
            if x < left || x > right || y < top || y > bottom {
                continue;
            }
            let mut attributes = [0; 5];
            for k in 0..3 {
                attributes[k] = if steps == 0 { start[k] } else { start[k] + ((end[k] - start[k]) * i).div_euclid(steps) };
            }
            self.shade(x, y, &attributes, &shader);
        }
    }

    fn fill(&mut self, words: &[u32]) {
        let color = words[0];
        let pixel = ((color >> 3 & 0x1F) | (color >> 11 & 0x1F) << 5 | (color >> 19 & 0x1F) << 10) as u16;
        let (x, y) = ((words[1] & 0x3F0) as usize, (words[1] >> 16 & 0x1FF) as usize);
        let (width, height) = ((((words[2] & 0x3FF) + 0xF) & !0xF) as usize, (words[2] >> 16 & 0x1FF) as usize);
        for row in 0..height {
            for column in 0..width {
                self.vram[((y + row) & (HEIGHT - 1)) * WIDTH + ((x + column) & (WIDTH - 1))] = pixel;
            }
        }
    }

    /// A copy inside VRAM: each row is read whole before it is written.
    fn copy(&mut self, words: &[u32]) {
        let size = |word: u32, mask: u32| (((word.wrapping_sub(1)) & mask) + 1) as usize;
        let (sx, sy) = ((words[1] & 0x3FF) as usize, (words[1] >> 16 & 0x1FF) as usize);
        let (dx, dy) = ((words[2] & 0x3FF) as usize, (words[2] >> 16 & 0x1FF) as usize);
        let (width, height) = (size(words[3] & 0xFFFF, 0x3FF), size(words[3] >> 16, 0x1FF));
        for line in 0..height {
            let row = Vec::from_fn(width, |column| self.vram[((sy + line) & (HEIGHT - 1)) * WIDTH + ((sx + column) & (WIDTH - 1))]);
            for (column, &pixel) in row.iter().enumerate() {
                let index = ((dy + line) & (HEIGHT - 1)) * WIDTH + ((dx + column) & (WIDTH - 1));
                if self.mask & 2 != 0 && self.vram[index] & 0x8000 != 0 {
                    continue;
                }
                self.vram[index] = pixel | if self.mask & 1 != 0 { 0x8000 } else { 0 };
            }
        }
    }

    /// One whole GP0 command.
    fn gp0(&mut self, words: &[u32]) {
        let word = words[0];
        match word >> 24 {
            0x02 => self.fill(words),
            0x20..=0x3F => self.polygon(words),
            0x40..=0x5F => self.line(words),
            0x60..=0x7F => self.rectangle(words),
            0x80..=0x9F => self.copy(words),
            0xE1 => self.draw_mode = word & 0x3FFF,
            0xE2 => self.texture_window = word & 0xF_FFFF,
            0xE3 => self.area_top_left = word & 0x7_FFFF,
            0xE4 => self.area_bottom_right = word & 0x7_FFFF,
            0xE5 => self.draw_offset = word & 0x3F_FFFF,
            0xE6 => self.mask = word & 3,
            other => panic!("the model has no GP0({other:02X}h)"),
        }
    }
}

fn xy(x: i32, y: i32) -> u32 {
    (x as u32 & 0xFFFF) | (y as u32 & 0xFFFF) << 16
}

/// A colour: mostly any, often the neutral 80h (a texture as it is), now
/// and then an extreme.
fn color(random: &mut Random) -> u32 {
    match random.below(8) {
        0..=2 => 0x80_8080,
        3 => [0, 0xFF_FFFF, 0xFF, 0xFF00, 0xFF_0000, 0x7F_7F7F, 0x81_8181, 0x80_80FF][random.below(8) as usize],
        _ => random.below(0x100_0000),
    }
}

/// A screen point around the 320x240 picture the scenes draw into, now and
/// then far from it.
fn point(random: &mut Random, near: Option<(i32, i32)>) -> (i32, i32) {
    match (near, random.below(16)) {
        (Some((x, y)), 0..=11) => {
            let reach = [2, 6, 16, 48, 160][random.below(5) as usize];
            (x + random.range(-reach, reach), y + random.range(-reach, reach))
        }
        (_, 15) => (random.range(-1024, 1023), random.range(-1024, 1023)),
        _ => (random.range(-40, 360), random.range(-40, 280)),
    }
}

/// The drawing state: now and then any, mostly a game's (a picture's
/// drawing area with its offset, a texture page, a blend mode).
fn state(random: &mut Random) -> Vec<u32> {
    let mut words = Vec::new();
    let page = random.below(0x20) | random.below(4) << 5 | [0, 0, 1, 2, 3][random.below(5) as usize] << 7;
    let flips = if random.one_in(4) { random.below(4) << 12 } else { 0 };
    words.push(0xE100_0000 | page | u32::from(random.one_in(2)) << 9 | u32::from(random.one_in(8)) << 11 | flips);
    words.push(0xE200_0000 | if random.one_in(3) { random.below(0x10_0000) } else { 0 });
    let (left, top) = if random.one_in(4) { (random.below(1024), random.below(512)) } else { (random.below(3) * 320, random.below(2) * 240) };
    let (right, bottom) = match random.below(8) {
        0 => (random.below(1024), random.below(512)),
        1 => (1023, 511),
        _ => ((left + 319).min(1023), (top + 239).min(511)),
    };
    words.push(0xE300_0000 | left | top << 10);
    words.push(0xE400_0000 | right | bottom << 10);
    let offset = if random.one_in(6) { (random.below(0x800), random.below(0x800)) } else { (left, top) };
    words.push(0xE500_0000 | offset.0 | offset.1 << 11);
    words.push(0xE600_0000 | if random.one_in(4) { random.below(4) } else { 0 });
    words
}

fn polygon(random: &mut Random) -> Vec<u32> {
    let op = 0x20 + random.below(0x20);
    let (gouraud, quad, textured) = (op & 0x10 != 0, op & 0x08 != 0, op & 0x04 != 0);
    let mut words = vec![op << 24 | color(random)];
    let first = point(random, None);
    let clut = random.below(0x8000);
    let page = random.below(0x20) | random.below(4) << 5 | [0, 0, 1, 2, 3][random.below(5) as usize] << 7 | u32::from(random.one_in(16)) << 11;
    // Texture coordinates over a small part of the page, or anywhere.
    let (u0, v0, span) = (random.below(256), random.below(256), [4, 16, 64, 255][random.below(4) as usize]);
    for index in 0..if quad { 4 } else { 3 } {
        if index > 0 && gouraud {
            words.push(if random.one_in(3) { words[0] & 0xFF_FFFF } else { color(random) });
        }
        let (x, y) = if index == 0 { first } else { point(random, Some(first)) };
        words.push(xy(x, y));
        if textured {
            let (u, v) = ((u0 + random.below(span + 1)) & 0xFF, (v0 + random.below(span + 1)) & 0xFF);
            words.push(u | v << 8 | [clut, page, random.below(0x1_0000), random.below(0x1_0000)][index] << 16);
        }
    }
    words
}

fn rectangle(random: &mut Random) -> Vec<u32> {
    let op = 0x60 + random.below(0x20);
    let mut words = vec![op << 24 | color(random)];
    let (x, y) = point(random, None);
    words.push(xy(x, y));
    if op & 0x04 != 0 {
        words.push(random.below(256) | random.below(256) << 8 | random.below(0x8000) << 16);
    }
    if op & 0x18 == 0 {
        let (width, height) = match random.below(8) {
            0 => (random.below(0x400), random.below(0x200)),
            1 => (random.below(2), random.below(2)),
            _ => (random.below(64), random.below(48)),
        };
        words.push(width | height << 16);
    }
    words
}

fn line(random: &mut Random) -> Vec<u32> {
    // A single line: the polyline bit stays off.
    let op = (0x40 + random.below(0x20)) & !0x08;
    let mut words = vec![op << 24 | color(random)];
    let first = point(random, None);
    words.push(xy(first.0, first.1));
    if op & 0x10 != 0 {
        words.push(color(random));
    }
    let (x, y) = point(random, Some(first));
    words.push(xy(x, y));
    words
}

fn fill(random: &mut Random) -> Vec<u32> {
    let (width, height) = if random.one_in(4) { (random.below(0x400), random.below(0x200)) } else { (random.below(400), random.below(260)) };
    vec![0x0200_0000 | color(random), random.below(1024) | random.below(512) << 16, width | height << 16]
}

fn copy(random: &mut Random) -> Vec<u32> {
    let place = |random: &mut Random| random.below(1024) | random.below(512) << 16;
    let (width, height) = if random.one_in(6) { (random.below(0x401), random.below(0x201)) } else { (random.below(96), random.below(64)) };
    vec![0x8000_0000, place(random), place(random), width | height << 16]
}

/// Draws `commands` random commands from `seed` with both, comparing VRAM
/// after each.
fn scene(seed: u64, commands: usize) {
    let mut random = Random(seed.wrapping_mul(0x9E37_79B9_7F4A_7C15) | 1);
    // VRAM: noise (textures, look-up tables and mask bits of every kind)
    // with transparent texels and plain areas among it.
    let mut vram = vec![0u16; WIDTH * HEIGHT];
    for block in 0..(WIDTH / 16) * (HEIGHT / 16) {
        let kind = random.below(8);
        let plain = random.next() as u16;
        for y in 0..16 {
            for x in 0..16 {
                let pixel = match kind {
                    0 => 0,
                    1 => plain,
                    2 => random.next() as u16 & 0x7FFF,
                    3 if random.one_in(3) => 0,
                    _ => random.next() as u16,
                };
                vram[(block / (WIDTH / 16) * 16 + y) * WIDTH + block % (WIDTH / 16) * 16 + x] = pixel;
            }
        }
    }
    let mut gpu = Gpu::new();
    gpu.vram_mut().copy_from_slice(&vram);
    let mut model = Model::new(vram);
    // GP1(09h): now and then GP0(E1h).11 may turn textures off.
    if random.one_in(4) {
        gpu.write_gp1(0x0900_0001);
        model.texture_disable_allowed = true;
    }
    let mut pending: Vec<Vec<u32>> = Vec::new();
    for number in 0..commands {
        if pending.is_empty() {
            pending = match random.below(32) {
                0..=2 => state(&mut random).into_iter().map(|word| vec![word]).collect(),
                3..=18 => vec![polygon(&mut random)],
                19..=27 => vec![rectangle(&mut random)],
                28 | 29 => vec![line(&mut random)],
                30 => vec![fill(&mut random)],
                _ => vec![copy(&mut random)],
            };
            if number == 0 {
                pending = state(&mut random).into_iter().map(|word| vec![word]).collect();
            }
            pending.reverse();
        }
        let words = pending.pop().expect("a command");
        gpu.write_gp0_words(&words);
        model.gp0(&words);
        if let Some(index) = gpu.vram().iter().zip(&model.vram).position(|(a, b)| a != b) {
            panic!(
                "seed {seed}, command {number} {words:08X?}: the pixel at ({}, {}) is {:04X}, by the rules {:04X} (mode {:04X}, window {:05X}, area {:05X}-{:05X}, offset {:06X}, mask {})",
                index % WIDTH,
                index / WIDTH,
                gpu.vram()[index],
                model.vram[index],
                model.draw_mode,
                model.texture_window,
                model.area_top_left,
                model.area_bottom_right,
                model.draw_offset,
                model.mask
            );
        }
    }
}

#[test]
fn every_primitive_draws_what_the_rules_give_a_pixel_at_a_time() {
    for seed in 0..24 {
        scene(seed, 160);
    }
}

/// Many more scenes (minutes unoptimized): `cargo test --release -p xem-gpu
/// --test raster -- --ignored`.
#[test]
#[ignore = "a long run for changes to the fill loops"]
fn many_more_scenes_draw_what_the_rules_give() {
    for seed in 1000..1300 {
        scene(seed, 400);
    }
}
