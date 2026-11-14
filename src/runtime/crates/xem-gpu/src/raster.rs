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
//!
//! The fill loops give exactly what those rules give a pixel at a time
//! (tests/raster.rs draws random scenes both ways), with the work that is
//! the same for a whole primitive done once: each kind of primitive has its
//! own loop (what it samples, whether it modulates, shades and blends), and
//! an attribute is stepped along a row by one addition ([`Plane`]).

use crate::{Gpu, PrimitiveKind, VRAM_HEIGHT, VRAM_WIDTH};

/// psx-spx dither offsets, indexed by `[y & 3][x & 3]`.
const DITHER: [[i32; 4]; 4] = [
    [-4, 0, -3, 1],
    [2, -2, 3, -1],
    [-3, 1, -4, 0],
    [3, -1, 2, -2],
];

/// VRAM as the fill loops hold it: an index masked to its size needs no
/// bounds check.
type Vram = [u16; VRAM_WIDTH * VRAM_HEIGHT];
const INDEX_MASK: usize = VRAM_WIDTH * VRAM_HEIGHT - 1;

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
    /// GP0(E6h): bit 15 for every pixel written, and whether pixels whose
    /// bit 15 is set are kept.
    force_mask: u16,
    check_mask: bool,
}

impl Shader {
    /// Whether the texture window changes any coordinate.
    fn windowed(&self) -> bool {
        (self.keep, self.set) != ((0xFF, 0xFF), (0, 0))
    }
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

/// 8-bit channel (with its dither offset added, when dithered) to 5 bits.
#[inline(always)]
fn to5(value: i32) -> u32 {
    (value.clamp(0, 255) >> 3) as u32
}

/// Three 8-bit channels (each with `dither` added) as a 15-bit colour.
#[inline(always)]
fn pack([r, g, b]: [i32; 3], dither: i32) -> u32 {
    to5(r + dither) | to5(g + dither) << 5 | to5(b + dither) << 10
}

/// A texel's 5-bit channels times the colour's /128, with `dither` added.
#[inline(always)]
fn modulate(texel: u32, [r, g, b]: [i32; 3], dither: i32) -> u32 {
    let channel = |shift: u32, by: i32| to5(((((texel >> shift & 31) << 3) as i32 * by) >> 7) + dither) << shift;
    channel(0, r) | channel(5, g) | channel(10, b)
}

/// `back + front` per 5-bit channel, saturated: all three at once. With
/// each channel's sum less its lowest bit, a channel's carry lands on a bit
/// no other channel's sum has, so the three carries (bits 5, 10 and 15) are
/// the channels that overflowed: their sums wrap, and they take 1Fh.
#[inline(always)]
fn add_saturated(back: u32, front: u32) -> u32 {
    let sum = back + front;
    let carries = (sum - ((back ^ front) & 0x0421)) & 0x8420;
    (sum - carries) | (carries - (carries >> 5))
}

/// `front` over `back` (15-bit colours; `back`'s bit 15 is ignored) by
/// semi-transparency mode: B/2+F/2, B+F, B-F, B+F/4 per channel, saturated.
#[inline(always)]
fn blend(back: u32, front: u32, mode: u32) -> u32 {
    let back = back & 0x7FFF;
    match mode {
        // Half the sum is the bits both have plus half of those one has.
        0 => (back & front) + (((back ^ front) & 0x7BDE) >> 1),
        1 => add_saturated(back, front),
        2 => {
            // Each channel's difference plus 32: as for the sum, bits 5, 10
            // and 15 of it less its lowest bits are the channels that did
            // not borrow, which keep their difference; the others take 0.
            let difference = back + 0x8420 - front;
            let kept = (difference - ((back ^ front) & 0x0421)) & 0x8420;
            (difference - kept) & (kept - (kept >> 5))
        }
        _ => add_saturated(back, front >> 2 & 0x1CE7),
    }
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
        let carry = self.remainder >= denominator;
        self.remainder -= if carry { denominator } else { 0 };
        self.quotient += i64::from(carry);
    }
}

/// One edge of a triangle for [`TriangleRows`]: its function (inside where
/// >= 0) steps along x unless the edge is horizontal.
#[derive(Clone, Copy)]
struct Edge {
    /// The size of the function's step along x (1 for a horizontal edge,
    /// which has none), the function over that size, rounded down, at the
    /// current row, and that quotient's step per row.
    size: i64,
    bound: Stepper,
    step: Stepper,
    /// All ones where the function rises along x (the edge bounds a row's
    /// first pixel), and where it falls (its last).
    rises: i64,
    falls: i64,
}

/// The rows of a triangle as the GPU fills them: pixels at integer
/// coordinates, top-left rule, within `bounds` (left, top, right, bottom,
/// inclusive: the drawing area). Each row is its y and the first and last x
/// drawn (none when first > last).
pub struct TriangleRows {
    edges: [Edge; 3],
    /// The rows, counted from `y0`, inside a horizontal edge (its function
    /// is the same along a row): the first and the last.
    inside: (i64, i64),
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
        let (x0, x1) = (v[0].0.min(v[1].0).min(v[2].0).max(left), v[0].0.max(v[1].0).max(v[2].0).min(right));
        let (y0, y1) = (v[0].1.min(v[1].1).min(v[2].1).max(top), v[0].1.max(v[1].1).max(v[2].1).min(bottom));
        if x0 > x1 || y0 > y1 {
            return None;
        }
        // Edge a->b: w(p) = (b.x-a.x)(p.y-a.y) - (b.y-a.y)(p.x-a.x), positive
        // inside. A pixel on an edge is drawn only if the edge is a left edge
        // (going up) or a top edge (horizontal, going right).
        let mut inside = (0, i64::MAX);
        let mut edge = |a: (i32, i32), b: (i32, i32)| {
            let dx = i64::from(b.0 - a.0);
            let dy = i64::from(b.1 - a.1);
            let top_left = dy < 0 || (dy == 0 && dx > 0);
            // The function at (x0, y0) less the fill rule's bias; it steps
            // by -dy along x and by dx down a row.
            let origin = dx * i64::from(y0 - a.1) - dy * i64::from(x0 - a.0) - i64::from(!top_left);
            match dy.abs() {
                0 => {
                    // origin + dx * row >= 0 from a row on, or up to a row.
                    let rows = floor_div(origin, dx.abs()).0;
                    if dx > 0 {
                        inside.0 = inside.0.max(-rows);
                    } else {
                        inside.1 = inside.1.min(rows);
                    }
                    Edge { size: 1, bound: Stepper::default(), step: Stepper::default(), rises: 0, falls: 0 }
                }
                size => Edge { size, bound: Stepper::new(origin, size), step: Stepper::new(dx, size), rises: -i64::from(dy < 0), falls: -i64::from(dy > 0) },
            }
        };
        let edges = [edge(v[0], v[1]), edge(v[1], v[2]), edge(v[2], v[0])];
        Some(TriangleRows { edges, inside, x0, x1, y0, y1, y: y0 })
    }
}

impl Iterator for TriangleRows {
    type Item = (i32, i32, i32);

    #[inline]
    fn next(&mut self) -> Option<(i32, i32, i32)> {
        if self.y > self.y1 {
            return None;
        }
        let y = self.y;
        self.y += 1;
        // Every edge function is >= 0 from `first` to `last` (an edge
        // function is linear in x): w / |step x| rounded down, stepped from
        // row to row. A row outside a horizontal edge has no pixel.
        let row = i64::from(y - self.y0);
        let (x0, x1) = (i64::from(self.x0), i64::from(self.x1));
        let (mut first, mut last) = (x0, x1);
        for edge in &mut self.edges {
            first = first.max(x0 - (edge.bound.quotient & edge.rises));
            last = last.min(x1 + ((x0 + edge.bound.quotient - x1) & edge.falls));
            edge.bound.step(edge.step, edge.size);
        }
        let last = if row < self.inside.0 || row > self.inside.1 { first - 1 } else { last.max(first - 1) };
        Some((y, first as i32, last as i32))
    }
}

/// An attribute's plane over a triangle, stepped along and down its rows.
///
/// With `n(x, y) = a0 * area + nx * (x - x0) + ny * (y - y0)` the attribute
/// at a pixel is `n / area` rounded down (`a0` the first vertex's, `area`
/// twice the triangle's). Inside the triangle the attribute lies between the
/// vertices' (0..255), so `0 <= n <= 255 * area`, and `area` is below 2^19
/// (a triangle's extent is below 1024 by 512): there `n / area` rounded down
/// is `n * m >> 46` with `m = 2^46 / area + 1`. (The product is below 2^55,
/// and exceeds `n / area * 2^46` by at most `n`, less than `2^46 / area`: too
/// little to reach the next whole number.) And `n * m` is linear: a pixel's
/// is the one before plus `nx * m`, a row's the one above plus `ny * m`, so
/// a pixel costs one addition and one shift. The sums wrap on the way (a
/// row's is taken at the first vertex's column, where it may lie outside
/// the triangle); at a pixel inside they are what the rule gives.
#[derive(Clone, Copy, Default)]
struct Plane {
    /// `n * m` at the first vertex's column of the current row.
    row: u64,
    /// `nx * m` and `ny * m`.
    along: u64,
    down: u64,
}

const PLANE_SHIFT: u32 = 46;

/// `m` for a triangle of twice the area `area` (below 2^19). The float
/// quotient rounded down is the whole one: its 53 bits hold it, and what
/// separates it from the next whole number (at least `1 / area`) is far
/// more than its rounding.
#[inline(always)]
fn plane_reciprocal(area: i64) -> u64 {
    ((1u64 << PLANE_SHIFT) as f64 / area as f64) as u64 + 1
}

impl Plane {
    /// Attribute `k`'s plane, at row `y`.
    #[inline(always)]
    fn new(v: &[Vertex; 3], k: usize, area: i64, reciprocal: u64, y: i32) -> Plane {
        let (dx1, dy1) = (i64::from(v[1].x - v[0].x), i64::from(v[1].y - v[0].y));
        let (dx2, dy2) = (i64::from(v[2].x - v[0].x), i64::from(v[2].y - v[0].y));
        let da1 = i64::from(v[1].attributes[k] - v[0].attributes[k]);
        let da2 = i64::from(v[2].attributes[k] - v[0].attributes[k]);
        let scaled = |n: i64| (n as u64).wrapping_mul(reciprocal);
        let down = scaled(da2 * dx1 - da1 * dx2);
        Plane {
            row: scaled(i64::from(v[0].attributes[k]) * area).wrapping_add(down.wrapping_mul(i64::from(y - v[0].y) as u64)),
            along: scaled(da1 * dy2 - da2 * dy1),
            down,
        }
    }

    /// `n * m` at the pixel `dx` right of the first vertex in the current
    /// row, which becomes the next.
    #[inline(always)]
    fn start_row(&mut self, dx: i32) -> u64 {
        let value = self.row.wrapping_add(self.along.wrapping_mul(i64::from(dx) as u64));
        self.row = self.row.wrapping_add(self.down);
        value
    }
}

/// What a fill loop samples: nothing, or a texture of 4, 8 or 15 bits.
const UNTEXTURED: u8 = 0;
const CLUT4: u8 = 1;
const CLUT8: u8 = 2;
const DIRECT: u8 = 3;

/// The texel at (u, v), each 0..255, of the shader's page, through the
/// texture window when it is `WINDOWED` (one that keeps every bit and sets
/// none is not).
#[inline(always)]
fn texel<const TEXTURE: u8, const WINDOWED: bool>(vram: &Vram, shader: &Shader, u: u32, v: u32) -> u32 {
    let (u, v) = if WINDOWED { ((u & shader.keep.0) | shader.set.0, (v & shader.keep.1) | shader.set.1) } else { (u, v) };
    let (u, v) = (u as usize, v as usize);
    // A page's 256 rows end within VRAM's 512.
    let row = (shader.page_y + v) * VRAM_WIDTH;
    let at = |x: usize, row: usize| u32::from(vram[(row | (x & (VRAM_WIDTH - 1))) & INDEX_MASK]);
    match TEXTURE {
        CLUT4 => {
            let index = at(shader.page_x + (u >> 2), row) >> ((u & 3) * 4) & 0xF;
            at(shader.clut_x + index as usize, shader.clut_y * VRAM_WIDTH)
        }
        CLUT8 => {
            let index = at(shader.page_x + (u >> 1), row) >> ((u & 1) * 8) & 0xFF;
            at(shader.clut_x + index as usize, shader.clut_y * VRAM_WIDTH)
        }
        _ => at(shader.page_x + u, row),
    }
}

/// Writes `color` (15 bits) at `index` with `mask_bit` (a texel's bit 15),
/// over what is there by the semi-transparency mode when `translucent`
/// (`SEMI` primitives only), unless the pixel there is kept.
#[inline(always)]
fn put<const SEMI: bool>(vram: &mut Vram, shader: &Shader, index: usize, color: u32, mask_bit: u32, translucent: bool) {
    let index = index & INDEX_MASK;
    if shader.check_mask && vram[index] & 0x8000 != 0 {
        return;
    }
    let color = match shader.semi {
        Some(mode) if SEMI && translucent => blend(u32::from(vram[index]), color, mode),
        _ => color,
    };
    vram[index] = (color | mask_bit) as u16 | shader.force_mask;
}

/// Samples, colours and writes one textured pixel: `MODULATE` multiplies
/// the texel by `rgb` (with `dither` added), else it is taken as it is.
#[inline(always)]
#[allow(clippy::too_many_arguments)]
fn put_texel<const TEXTURE: u8, const WINDOWED: bool, const MODULATE: bool, const SEMI: bool>(vram: &mut Vram, shader: &Shader, index: usize, u: u32, v: u32, rgb: [i32; 3], dither: i32) {
    let texel = texel::<TEXTURE, WINDOWED>(vram, shader, u, v);
    if texel == 0 {
        return;
    }
    let color = if MODULATE { modulate(texel, rgb, dither) } else { texel & 0x7FFF };
    put::<SEMI>(vram, shader, index, color, texel & 0x8000, texel & 0x8000 != 0);
}

/// A triangle's rows. `GOURAUD` interpolates the colour, a texture its
/// coordinates; what does not vary stays the first vertex's.
fn fill_triangle<const TEXTURE: u8, const WINDOWED: bool, const MODULATE: bool, const GOURAUD: bool, const SEMI: bool>(
    vram: &mut Vram,
    rows: TriangleRows,
    v: &[Vertex; 3],
    area: i64,
    shader: &Shader,
) {
    let reciprocal = plane_reciprocal(area);
    let plane = |k: usize, used: bool| if used { Plane::new(v, k, area, reciprocal, rows.y0) } else { Plane::default() };
    let mut colors = [plane(0, GOURAUD), plane(1, GOURAUD), plane(2, GOURAUD)];
    let mut coordinates = [plane(3, TEXTURE != UNTEXTURED), plane(4, TEXTURE != UNTEXTURED)];
    let mut rgb = [v[0].attributes[0], v[0].attributes[1], v[0].attributes[2]];
    // One colour, never dithered.
    let flat = pack(rgb, 0);
    for (y, first, last) in rows {
        let dx = first - v[0].x;
        let mut color = [colors[0].start_row(dx), colors[1].start_row(dx), colors[2].start_row(dx)];
        let mut coordinate = [coordinates[0].start_row(dx), coordinates[1].start_row(dx)];
        if first > last {
            continue;
        }
        let row = y as usize * VRAM_WIDTH;
        let pixels = row + first as usize..row + last as usize + 1;
        if TEXTURE == UNTEXTURED && !GOURAUD {
            if !SEMI && !shader.check_mask {
                vram[pixels].fill(flat as u16 | shader.force_mask);
            } else {
                for index in pixels {
                    put::<SEMI>(vram, shader, index, flat, 0, true);
                }
            }
            continue;
        }
        let dither = if shader.dither { DITHER[(y & 3) as usize] } else { [0; 4] };
        for index in pixels {
            // A row starts at a multiple of 4: the index's low bits are x's.
            let offset = dither[index & 3];
            if GOURAUD {
                rgb = [(color[0] >> PLANE_SHIFT) as i32, (color[1] >> PLANE_SHIFT) as i32, (color[2] >> PLANE_SHIFT) as i32];
                color = [color[0].wrapping_add(colors[0].along), color[1].wrapping_add(colors[1].along), color[2].wrapping_add(colors[2].along)];
            }
            if TEXTURE == UNTEXTURED {
                put::<SEMI>(vram, shader, index, pack(rgb, offset), 0, true);
            } else {
                let (u, v) = ((coordinate[0] >> PLANE_SHIFT) as u32, (coordinate[1] >> PLANE_SHIFT) as u32);
                coordinate = [coordinate[0].wrapping_add(coordinates[0].along), coordinate[1].wrapping_add(coordinates[1].along)];
                put_texel::<TEXTURE, WINDOWED, MODULATE, SEMI>(vram, shader, index, u, v, rgb, offset);
            }
        }
    }
}

/// A rectangle's pixels, already clipped: columns `xs` of rows `ys`, the
/// texture coordinates of its corner at `corner` in `uv`, stepping by `step`
/// (1 or -1 each way).
#[allow(clippy::too_many_arguments)]
fn fill_sprite<const TEXTURE: u8, const WINDOWED: bool, const MODULATE: bool, const SEMI: bool>(
    vram: &mut Vram,
    shader: &Shader,
    xs: (i32, i32),
    ys: (i32, i32),
    corner: (i32, i32),
    uv: (u32, u32),
    step: (u32, u32),
    rgb: [i32; 3],
) {
    let flat = pack(rgb, 0);
    for y in ys.0..=ys.1 {
        let row = y as usize * VRAM_WIDTH;
        let pixels = row + xs.0 as usize..row + xs.1 as usize + 1;
        if TEXTURE == UNTEXTURED {
            if !SEMI && !shader.check_mask {
                vram[pixels].fill(flat as u16 | shader.force_mask);
            } else {
                for index in pixels {
                    put::<SEMI>(vram, shader, index, flat, 0, true);
                }
            }
            continue;
        }
        let v = uv.1.wrapping_add(step.1.wrapping_mul((y - corner.1) as u32)) & 0xFF;
        let mut u = uv.0.wrapping_add(step.0.wrapping_mul((xs.0 - corner.0) as u32));
        for index in pixels {
            put_texel::<TEXTURE, WINDOWED, MODULATE, SEMI>(vram, shader, index, u & 0xFF, v, rgb, 0);
            u = u.wrapping_add(step.0);
        }
    }
}

/// Calls `$fill::<TEXTURE, WINDOWED, MODULATE, ...>` for what `$shader`
/// samples, through a texture window or not, and whether it modulates by a
/// colour other than the neutral one (`$modulate`); the other constant
/// parameters (each an expression) follow in `[...]`.
macro_rules! by_texture {
    ($shader:expr, $modulate:expr, $fill:ident, [$($flag:expr),*], $arguments:tt) => {
        match if $shader.textured { CLUT4 + $shader.depth.min(2) as u8 } else { UNTEXTURED } {
            CLUT4 => by_flags!($fill, [CLUT4], [$shader.windowed(), $modulate $(, $flag)*], $arguments),
            CLUT8 => by_flags!($fill, [CLUT8], [$shader.windowed(), $modulate $(, $flag)*], $arguments),
            DIRECT => by_flags!($fill, [DIRECT], [$shader.windowed(), $modulate $(, $flag)*], $arguments),
            _ => by_flags!($fill, [UNTEXTURED, false, false], [$($flag),*], $arguments),
        }
    };
}

/// Turns each remaining flag into a constant parameter, one at a time.
macro_rules! by_flags {
    ($fill:ident, [$($constant:tt),*], [], ($($argument:expr),*)) => {
        $fill::<$($constant),*>($($argument),*)
    };
    ($fill:ident, [$($constant:tt),*], [$flag:expr $(, $rest:expr)*], $arguments:tt) => {
        if $flag {
            by_flags!($fill, [$($constant,)* true], [$($rest),*], $arguments)
        } else {
            by_flags!($fill, [$($constant,)* false], [$($rest),*], $arguments)
        }
    };
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
            force_mask: if self.mask & 1 != 0 { 0x8000 } else { 0 },
            check_mask: self.mask & 2 != 0,
        }
    }

    /// VRAM for the fill loops.
    fn pixels(&mut self) -> &mut Vram {
        self.vram.as_mut_slice().try_into().expect("VRAM is 1024x512")
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
        // The columns up to the right edge, then those that wrap to the left.
        let ahead = width.min(VRAM_WIDTH - x);
        for row in 0..height {
            let base = ((y + row) & (VRAM_HEIGHT - 1)) * VRAM_WIDTH;
            self.vram[base + x..base + x + ahead].fill(pixel);
            self.vram[base..base + width - ahead].fill(pixel);
        }
    }

    /// GP0(80h): copies a VRAM rectangle, wrapping at the edges, with the
    /// mask settings applied to the destination.
    pub(crate) fn copy_rectangle(&mut self, words: &[u32]) {
        let source = crate::Image::from_words(words[1], words[3]);
        let destination = crate::Image::from_words(words[2], words[3]);
        let width = usize::from(source.width);
        let (force, check) = (if self.mask & 1 != 0 { 0x8000 } else { 0 }, self.mask & 2 != 0);
        let mut row = [0u16; VRAM_WIDTH];
        for line in 0..usize::from(source.height) {
            let from = ((usize::from(source.y) + line) & (VRAM_HEIGHT - 1)) * VRAM_WIDTH;
            let to = ((usize::from(destination.y) + line) & (VRAM_HEIGHT - 1)) * VRAM_WIDTH;
            for (column, pixel) in row[..width].iter_mut().enumerate() {
                *pixel = self.vram[from + ((usize::from(source.x) + column) & (VRAM_WIDTH - 1))];
            }
            for (column, &pixel) in row[..width].iter().enumerate() {
                let index = to + ((usize::from(destination.x) + column) & (VRAM_WIDTH - 1));
                if !(check && self.vram[index] & 0x8000 != 0) {
                    self.vram[index] = pixel | force;
                }
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
        let min_x = v[0].x.min(v[1].x).min(v[2].x);
        let max_x = v[0].x.max(v[1].x).max(v[2].x);
        let min_y = v[0].y.min(v[1].y).min(v[2].y);
        let max_y = v[0].y.max(v[1].y).max(v[2].y);
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
        // A texture modulated by the neutral colour without dithering is the
        // texture as it is, and a raw one takes no colour.
        let neutral = !shader.gouraud && !shader.dither && v[0].attributes[..3] == [0x80; 3];
        let modulate = shader.textured && !shader.raw && !neutral;
        let gouraud = shader.gouraud && (modulate || !shader.textured);
        let vram = self.pixels();
        by_texture!(shader, modulate, fill_triangle, [gouraud, shader.semi.is_some()], (vram, rows, &v, area, shader));
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
        let vram = self.pixels();
        for i in 0..=steps {
            let x = along(x0, dx, i);
            let y = along(y0, dy, i);
            if x < left || x > right || y < top || y > bottom {
                continue;
            }
            let rgb = [0, 1, 2].map(|k| if steps == 0 { start[k] } else { start[k] + ((end[k] - start[k]) * i).div_euclid(steps) });
            let dither = if shader.dither { DITHER[(y & 3) as usize][(x & 3) as usize] } else { 0 };
            put::<true>(vram, &shader, y as usize * VRAM_WIDTH + x as usize, pack(rgb, dither), 0, true);
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
            u0 = word & 0xFF;
            v0 = word >> 8 & 0xFF;
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
        // A flipped sprite's coordinates fall from its corner's.
        let step = |flipped: bool| if flipped { u32::MAX } else { 1 };
        let step = (step(self.draw_mode & 0x1000 != 0), step(self.draw_mode & 0x2000 != 0));
        let (left, top, right, bottom) = self.drawing_area();
        let xs = (x.max(left), (x + width - 1).min(right));
        let ys = (y.max(top), (y + height - 1).min(bottom));
        if xs.0 > xs.1 || ys.0 > ys.1 {
            return;
        }
        let rgb = color_attributes(words[0]);
        // A texture modulated by the neutral colour is the texture as it is.
        let modulate = textured && !shader.raw && rgb != [0x80; 3];
        let vram = self.pixels();
        by_texture!(shader, modulate, fill_sprite, [shader.semi.is_some()], (vram, &shader, xs, ys, (x, y), (u0, v0), step, rgb));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The modes a channel at a time, as psx-spx gives them.
    fn blend_by_channel(back: u32, front: u32, mode: u32) -> u32 {
        let mut out = 0;
        for shift in [0, 5, 10] {
            let b = (back >> shift & 31) as i32;
            let f = (front >> shift & 31) as i32;
            let c = match mode {
                0 => (b + f) >> 1,
                1 => b + f,
                2 => b - f,
                _ => b + (f >> 2),
            };
            out |= (c.clamp(0, 31) as u32) << shift;
        }
        out
    }

    #[test]
    fn blending_three_channels_at_once_is_blending_each() {
        // Every pair of values of one channel, whatever the others hold
        // (nothing carries from a channel into the next), with and without
        // the pixel's mask bit; then pairs of whole colours.
        const OTHERS: [u32; 8] = [0, 0x7FFF, 0x5555, 0x2AAA, 0x0421, 0x4210, 0x7BDE, 0x1CE7];
        for mode in 0..4 {
            for shift in [0, 5, 10] {
                let channel = 31 << shift;
                for (b, f) in (0..32).flat_map(|b| (0..32).map(move |f| (b, f))) {
                    for (behind, over) in OTHERS.into_iter().flat_map(|behind| OTHERS.map(|over| (behind, over))) {
                        for mask in [0, 0x8000] {
                            let (back, front) = (b << shift | behind & !channel | mask, f << shift | over & !channel);
                            assert_eq!(blend(back, front, mode), blend_by_channel(back, front, mode), "mode {mode}: {front:04X} over {back:04X}");
                        }
                    }
                }
            }
            let mut seed = 0x2545_F491u32;
            for _ in 0..200_000 {
                seed = seed.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
                let (back, front) = (seed >> 16, seed >> 1 & 0x7FFF);
                assert_eq!(blend(back, front, mode), blend_by_channel(back, front, mode), "mode {mode}: {front:04X} over {back:04X}");
            }
        }
    }

    #[test]
    fn a_planes_reciprocal_is_the_whole_quotient_plus_one() {
        for area in 1..1i64 << 19 {
            assert_eq!(plane_reciprocal(area), (1u64 << PLANE_SHIFT) / area as u64 + 1, "area {area}");
        }
    }

    #[test]
    fn a_planes_steps_are_the_division_at_every_pixel_inside() {
        // The largest and thinnest triangles the size limits allow, and
        // slivers between: the stepped attribute against the division.
        let triangles = [
            [(0, 0), (1023, 0), (0, 511)],
            [(0, 0), (1023, 511), (0, 511)],
            [(0, 0), (1023, 1), (1022, 0)],
            [(0, 0), (1, 511), (0, 510)],
            [(5, 3), (1027, 514), (4, 514)],
            [(0, 0), (1023, 255), (511, 511)],
            [(3, 7), (9, 8), (4, 12)],
        ];
        for points in triangles {
            for values in [[0, 255, 0], [255, 0, 255], [255, 255, 0], [1, 254, 127], [0, 0, 255], [200, 13, 77]] {
                let mut v = [Vertex::default(); 3];
                for (vertex, ((x, y), value)) in v.iter_mut().zip(points.into_iter().zip(values)) {
                    *vertex = Vertex { x, y, attributes: [value; 5] };
                }
                let cross = |a: &Vertex, b: &Vertex, c: &Vertex| i64::from(b.x - a.x) * i64::from(c.y - a.y) - i64::from(b.y - a.y) * i64::from(c.x - a.x);
                let mut area = cross(&v[0], &v[1], &v[2]);
                if area < 0 {
                    v.swap(1, 2);
                    area = -area;
                }
                // The rule's numerator at a pixel.
                let n = |x: i32, y: i32| {
                    let (da1, da2) = (i64::from(v[1].attributes[0] - v[0].attributes[0]), i64::from(v[2].attributes[0] - v[0].attributes[0]));
                    let nx = da1 * i64::from(v[2].y - v[0].y) - da2 * i64::from(v[1].y - v[0].y);
                    let ny = da2 * i64::from(v[1].x - v[0].x) - da1 * i64::from(v[2].x - v[0].x);
                    i64::from(v[0].attributes[0]) * area + nx * i64::from(x - v[0].x) + ny * i64::from(y - v[0].y)
                };
                let rows = TriangleRows::new(v.map(|p| (p.x, p.y)), (0, 0, 1023, 511)).expect("a triangle with area");
                let mut plane = Plane::new(&v, 0, area, plane_reciprocal(area), rows.y0);
                for (y, first, last) in rows {
                    let mut value = plane.start_row(first - v[0].x);
                    for x in first..=last {
                        assert_eq!((value >> PLANE_SHIFT) as i64, n(x, y).div_euclid(area), "{points:?} {values:?} at ({x}, {y})");
                        value = value.wrapping_add(plane.along);
                    }
                }
            }
        }
    }
}
