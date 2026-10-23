// Draws traced PS1 primitives (xem-gpu's draw trace) into a scaled picture
// of VRAM, sampling textures from a copy of the native 16-bit VRAM.
//
// The rules follow xem-gpu's software rasterizer (psx-spx): texel 0000h is
// transparent, bit 15 of a texel selects semi-transparency, modulation is
// texel * colour / 128, dithering adds the 4x4 offsets before truncating to
// 5 bits, and the texture window masks coordinates. Which of the original's
// limits apply (affine texturing, integer vertices, 15-bit colour,
// dithering) is chosen per pass.

struct Pass {
    // VRAM position of the target's top-left pixel and the VRAM extent it covers.
    origin: vec2<f32>,
    extent: vec2<f32>,
    // Target pixels per VRAM pixel.
    scale: f32,
    // Bit 0 perspective-correct texturing, 1 15-bit colour, 2 dithering,
    // 3 depth-tested ordering, 4 depth from its reciprocal (a camera within
    // the scene: ps1/mod.rs PASS_RECIPROCAL_DEPTH), 5 an eye of the diorama
    // (PASS_WORLD), 6 the diorama's interface pane (PASS_PANE).
    options: u32,
    // A screen the game took into VRAM (ps1/mod.rs Capture): its picture's
    // scale and sides (VRAM pixels on each side of the screen, the left one
    // first in the picture), and where it lies (x, y, width, height; width 0
    // for none).
    capture_scale: f32,
    capture_side: u32,
    capture: vec4<u32>,
    // The diorama (ps1/diorama.rs): an eye's clip space from the game's
    // world and from the table's space, the world in the table's space, and
    // the table's volume there (nothing outside it is drawn).
    world_to_clip: mat4x4<f32>,
    table_to_clip: mat4x4<f32>,
    world_to_table: mat4x4<f32>,
    volume_min: vec4<f32>,
    volume_max: vec4<f32>,
    // The eye's point in the game's world, and the sprites' billboards'
    // normal there (BILLBOARD).
    eye: vec4<f32>,
    billboard: vec4<f32>,
};

// Pass options (ps1/mod.rs PASS_WORLD, PASS_PANE).
const PASS_WORLD: u32 = 32u;
const PASS_PANE: u32 = 64u;

@group(0) @binding(0) var<uniform> pass_: Pass;
@group(0) @binding(1) var vram: texture_2d<u32>;
@group(0) @binding(2) var capture: texture_2d<f32>;
@group(0) @binding(3) var capture_sampler: sampler;
@group(0) @binding(4) var feedback: texture_2d<f32>;
@group(0) @binding(5) var feedback_aux: texture_2d<f32>;

// Primitive flags (ps1/mod.rs, FLAG_*).
const TEXTURED: u32 = 1u;
const RAW: u32 = 2u;
const DITHER: u32 = 4u;
const SEMI: u32 = 8u;
const MASK_SET: u32 = 16u;
const LINEAR: u32 = 32u;
const RECTANGLE: u32 = 64u;
const BAKED: u32 = 128u;
const BLEND_SHIFT: u32 = 8u;
const CLAMP: u32 = 2048u;
const CLASS_SHIFT: u32 = 12u;
const ORIGINAL: u32 = 65536u;
// The auxiliary class of a sprite's parts (ps1/mod.rs AUX_SPRITE).
const CLASS_SPRITE: u32 = 4u;
const LINE: u32 = 131072u;
const CAPTURE: u32 = 262144u;
const FEEDBACK: u32 = 524288u;
// A diorama vertex in the table's space, not the game's world; one in clip
// space (a full-screen fade over a whole target); one of a sprite's
// billboard part, lifted by its `w` (vs_world).
const TABLE: u32 = 1048576u;
const CLIP: u32 = 2097152u;
const BILLBOARD: u32 = 4194304u;
// A battle's sky, or a scene's past its depth (ps1/mod.rs FLAG_BACKDROP).
const BACKDROP: u32 = 8388608u;

struct VertexIn {
    @location(0) position: vec2<f32>,
    // The GTE's screen depth, 0 without a projection.
    @location(1) depth: f32,
    // Perspective weight (1 for affine).
    @location(2) w: f32,
    @location(3) color: vec4<f32>,
    @location(4) uv: vec2<f32>,
    // Draw mode (GP0(E1h) bits 0-13), CLUT word, texture window, flags.
    @location(5) mode: vec4<u32>,
    // The polygon's texel range (CLAMP): u, v minimum and maximum bytes.
    @location(6) limits: u32,
};

struct VertexOut {
    @builtin(position) clip: vec4<f32>,
    @location(0) color: vec4<f32>,
    @location(1) @interpolate(perspective) uv_perspective: vec2<f32>,
    // Screen-linear (affine) texture coordinates without `noperspective`,
    // which GLSL ES 3.0 lacks: (uv * w, w) interpolate perspective-correctly
    // and their quotient is the screen-linear uv.
    @location(2) uv_affine_w: vec3<f32>,
    @location(3) @interpolate(flat) mode: vec4<u32>,
    @location(4) depth: f32,
    @location(5) @interpolate(flat) limits: u32,
    // A diorama fragment's point in the table's space (vs_world).
    @location(6) table: vec3<f32>,
    // The depth the rasterizer takes (clip z over w) times w, and w:
    // interpolated perspective-correctly, their quotient is that depth
    // across the screen, which a camera within the scene writes itself
    // (fs_*_nearer).
    @location(7) depth_w: vec2<f32>,
};

@vertex
fn vs_main(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    // Vertices sit at native pixel centres (x + 0.5), where the original
    // samples. An interface polygon instead covers whole native pixels at
    // any scale (x + 0.5 / scale: the same at scale 1), as the original's
    // picture does scaled up; lines are drawn around their pixels' centres.
    var position = in.position;
    if (in.mode.w & ORIGINAL) != 0u && (in.mode.w & LINE) == 0u {
        position -= vec2<f32>(0.5 - 0.5 / pass_.scale);
    }
    let ndc = (position - pass_.origin) / pass_.extent * vec2<f32>(2.0, -2.0) + vec2<f32>(-1.0, 1.0);
    // Depth-tested ordering: projected primitives by their depth, the
    // rest drawn over (the pipeline's compare decides). A camera within the
    // scene sees polygons from near it to far away, across which only the
    // reciprocal of depth interpolates as the depth does; kept reversed
    // (nearer larger: ps1/mod.rs DepthMode::Nearer), where a float keeps
    // its precision at any distance, and written by the fragment there
    // (NearerTargets), as GL's clip space would not keep it.
    var z = 0.0;
    if in.depth > 0.0 {
        if (pass_.options & 16u) != 0u {
            z = clamp(1.0 / max(in.depth, 1.0), 0.0, 1.0);
        } else {
            z = clamp(in.depth / 65536.0, 0.0, 1.0);
        }
    }
    out.clip = vec4<f32>(ndc * in.w, z * in.w, in.w);
    out.depth_w = vec2<f32>(z * in.w, in.w);
    out.color = in.color;
    out.uv_perspective = in.uv;
    out.uv_affine_w = vec3<f32>(in.uv * in.w, in.w);
    out.mode = in.mode;
    out.depth = in.depth;
    out.limits = in.limits;
    out.table = vec3<f32>(0.0);
    return out;
}

// A diorama vertex (ps1/diorama.rs): its point in the game's world (x and y
// in `position`, z in `depth`), or in the table's space (TABLE: the
// backdrop's wall), seen through the eye's matrices; or a point of clip
// space (CLIP: a fade over the whole target). A sprite's billboard part
// (BILLBOARD) is lifted over the parts drawn before it by `w` (world units)
// along the billboards' normal, towards the side of its billboard the eye
// sees, so the game's order holds from either side. Textures interpolate
// perspective-correctly through the clip w.
@vertex
fn vs_world(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    var point = vec4<f32>(in.position, in.depth, 1.0);
    if (in.mode.w & BILLBOARD) != 0u {
        let normal = pass_.billboard.xyz;
        let side = select(-1.0, 1.0, dot(normal, pass_.eye.xyz - point.xyz) >= 0.0);
        point = vec4<f32>(point.xyz + normal * (in.w * side), 1.0);
    }
    if (in.mode.w & CLIP) != 0u {
        out.clip = vec4<f32>(in.position, 0.0, 1.0);
        out.table = vec3<f32>(0.0);
    } else if (in.mode.w & TABLE) != 0u {
        out.clip = pass_.table_to_clip * point;
        out.table = point.xyz;
    } else {
        out.clip = pass_.world_to_clip * point;
        out.table = (pass_.world_to_table * point).xyz;
    }
    out.color = in.color;
    out.uv_perspective = in.uv;
    out.uv_affine_w = vec3<f32>(in.uv, 1.0);
    out.mode = in.mode;
    out.depth = 0.0;
    out.limits = in.limits;
    out.depth_w = vec2<f32>(0.0, 1.0);
    return out;
}

fn affine(in: VertexOut) -> vec2<f32> {
    return in.uv_affine_w.xy / in.uv_affine_w.z;
}

fn vram_at(x: i32, y: i32) -> u32 {
    return textureLoad(vram, vec2<i32>(x & 1023, y & 511), 0).r;
}

// The texel at integer coordinates (u, v) of the primitive's page: the raw
// 16-bit value (0 transparent, bit 15 semi-transparent).
fn texel(mode: vec4<u32>, uv: vec2<i32>) -> u32 {
    let window = mode.z;
    let mask = vec2<u32>(window & 31u, (window >> 5u) & 31u);
    let offset = vec2<u32>((window >> 10u) & 31u, (window >> 15u) & 31u);
    let u = ((u32(uv.x) & 255u) & ~(mask.x * 8u)) | ((offset.x & mask.x) * 8u);
    let v = ((u32(uv.y) & 255u) & ~(mask.y * 8u)) | ((offset.y & mask.y) * 8u);
    let page = mode.x;
    let page_x = i32((page & 15u) * 64u);
    let page_y = i32(((page >> 4u) & 1u) * 256u);
    let depth = (page >> 7u) & 3u;
    let clut_x = i32((mode.y & 63u) * 16u);
    let clut_y = i32((mode.y >> 6u) & 511u);
    let row = page_y + i32(v);
    if depth == 0u {
        let word = vram_at(page_x + i32(u >> 2u), row);
        let index = (word >> ((u & 3u) * 4u)) & 15u;
        return vram_at(clut_x + i32(index), clut_y);
    }
    if depth == 1u {
        let word = vram_at(page_x + i32(u >> 1u), row);
        let index = (word >> ((u & 1u) * 8u)) & 255u;
        return vram_at(clut_x + i32(index), clut_y);
    }
    return vram_at(page_x + i32(u), row);
}

fn rgb5(pixel: u32) -> vec3<f32> {
    return vec3<f32>(f32(pixel & 31u), f32((pixel >> 5u) & 31u), f32((pixel >> 10u) & 31u));
}

struct Sample {
    // Colour in 5-bit units (0-31), coverage (0 transparent), semi bit.
    color: vec3<f32>,
    coverage: f32,
    semi: bool,
};

// A texel coordinate kept inside the polygon's own texels (CLAMP): beyond
// the original resolution, or filtered, a sample near an edge would
// otherwise reach the neighbouring texels of the page.
fn inside(limits: u32, flags: u32, at: vec2<i32>) -> vec2<i32> {
    if (flags & CLAMP) == 0u {
        return at;
    }
    let lo = vec2<i32>(i32(limits & 255u), i32((limits >> 8u) & 255u));
    let hi = vec2<i32>(i32((limits >> 16u) & 255u), i32(limits >> 24u));
    return clamp(at, lo, hi);
}

fn sample_nearest(mode: vec4<u32>, limits: u32, uv: vec2<f32>) -> Sample {
    let t = texel(mode, inside(limits, mode.w, vec2<i32>(floor(uv + vec2<f32>(0.001)))));
    return Sample(rgb5(t), select(1.0, 0.0, t == 0u), (t & 0x8000u) != 0u);
}

// Bilinear filtering of the decoded texels, transparent texels excluded.
fn sample_linear(mode: vec4<u32>, limits: u32, uv: vec2<f32>) -> Sample {
    let base = uv - vec2<f32>(0.5);
    let cell = vec2<i32>(floor(base));
    let f = base - floor(base);
    var color = vec3<f32>(0.0);
    var weight = 0.0;
    var total = 0.0;
    for (var k = 0; k < 4; k++) {
        let d = vec2<i32>(k & 1, k >> 1);
        let t = texel(mode, inside(limits, mode.w, cell + d));
        let w = select(1.0 - f.x, f.x, d.x == 1) * select(1.0 - f.y, f.y, d.y == 1);
        total += w;
        if t != 0u {
            color += rgb5(t) * w;
            weight += w;
        }
    }
    let nearest = texel(mode, inside(limits, mode.w, vec2<i32>(floor(uv + vec2<f32>(0.001)))));
    var out = Sample(vec3<f32>(0.0), 0.0, (nearest & 0x8000u) != 0u);
    if weight > 0.0 {
        out.color = color / weight;
        out.coverage = weight / total;
    }
    return out;
}

// A texel of the screen the game took (CAPTURE), from the picture presented
// then, its sides included (beyond the screen's left and right edges): every
// texel drawn (the picture's own pixels decide nothing about transparency),
// semi-transparent as the screen's nearest texel.
fn sample_capture(mode: vec4<u32>, uv: vec2<f32>) -> Sample {
    let page = vec2<f32>(f32((mode.x & 15u) * 64u), f32(((mode.x >> 4u) & 1u) * 256u));
    let at = page + uv - vec2<f32>(pass_.capture.xy);
    let edge = clamp(vec2<i32>(floor(at)), vec2<i32>(0), vec2<i32>(pass_.capture.zw) - vec2<i32>(1));
    let t = vram_at(i32(pass_.capture.x) + edge.x, i32(pass_.capture.y) + edge.y);
    let size = vec2<f32>(textureDimensions(capture));
    let point = (at + vec2<f32>(f32(pass_.capture_side), 0.0)) * pass_.capture_scale / size;
    return Sample(textureSampleLevel(capture, capture_sampler, point, 0.0).rgb * 31.0, 1.0, (t & 0x8000u) != 0u);
}

const DITHER_MATRIX = array<f32, 16>(
    -4.0, 0.0, -3.0, 1.0,
    2.0, -2.0, 3.0, -1.0,
    -3.0, 1.0, -4.0, 0.0,
    3.0, -1.0, 2.0, -2.0,
);

struct Shaded {
    color: vec4<f32>,
    semi: bool,
};

// The primitive's colour at this fragment in 8-bit units, before blending.
fn shade(in: VertexOut, frag: vec4<f32>) -> Shaded {
    let flags = in.mode.w;
    // A diorama's eye draws only inside the table's volume.
    if (pass_.options & PASS_WORLD) != 0u && (flags & CLIP) == 0u
        && (any(in.table < pass_.volume_min.xyz) || any(in.table > pass_.volume_max.xyz)) {
        discard;
    }
    // The interface as the original draws it (ORIGINAL), whatever the pass
    // draws the scene with: colour and texture coordinates taken at the
    // native pixel's sample point (so each native pixel is one colour, as
    // on the console), 15-bit colour and the original's dithering.
    let original = (flags & ORIGINAL) != 0u;
    let color_dx = dpdx(in.color);
    let color_dy = dpdy(in.color);
    let affine_uv = affine(in);
    let uv_dx = dpdx(affine_uv);
    let uv_dy = dpdy(affine_uv);
    var delta = vec2<f32>(0.0);
    if original {
        // The native pixel's sample point: where its first scaled pixel is
        // (a line's: its centre).
        let native = floor(pass_.origin + frag.xy / pass_.scale);
        var point = (native - pass_.origin) * pass_.scale + vec2<f32>(0.5);
        if (flags & LINE) != 0u {
            point = (native + vec2<f32>(0.5) - pass_.origin) * pass_.scale;
        }
        delta = point - frag.xy;
    }
    var color8 = (in.color.rgb + color_dx.rgb * delta.x + color_dy.rgb * delta.y) * 255.0;
    var semi = (flags & SEMI) != 0u;
    var mask = (flags & MASK_SET) != 0u;
    let fifteen = (pass_.options & 2u) != 0u || original;
    if (flags & TEXTURED) != 0u {
        var uv = affine_uv + uv_dx * delta.x + uv_dy * delta.y;
        if (pass_.options & 1u) != 0u && (flags & RECTANGLE) == 0u && !original {
            uv = in.uv_perspective;
        }
        // A mirrored sprite part (its coordinates falling across the
        // screen) samples one texel further on: the game's u - 1 meets the
        // GPU taking each texel at a native pixel's leading edge, so the
        // part keeps its first column at any scale, and at any size
        // (pulled back, a native pixel spans several of its texels).
        if ((flags >> CLASS_SHIFT) & 15u) == CLASS_SPRITE && !original {
            uv += select(vec2<f32>(0.0), vec2<f32>(1.0), vec2<f32>(uv_dx.x, uv_dy.y) < vec2<f32>(0.0));
        }
        var s: Sample;
        if (flags & FEEDBACK) != 0u {
            let point = (uv + vec2<f32>(0.5) - pass_.origin) * pass_.scale / vec2<f32>(textureDimensions(feedback));
            let pixel = textureSampleLevel(feedback, capture_sampler, point, 0.0);
            s = Sample(pixel.rgb * 31.0, 1.0, pixel.a > 0.0);
        } else if (flags & CAPTURE) != 0u && pass_.capture.z > 0u {
            s = sample_capture(in.mode, uv);
        } else if (flags & LINEAR) != 0u {
            s = sample_linear(in.mode, in.limits, uv);
        } else {
            s = sample_nearest(in.mode, in.limits, uv);
        }
        if s.coverage < 0.5 {
            discard;
        }
        semi = semi && s.semi;
        mask = mask || s.semi;
        // 5-bit texels: the original shifts them to 8 bits (x8); without the
        // 15-bit limit they expand to the full range.
        var texel8 = s.color * 8.0;
        if !fifteen {
            texel8 = s.color * (255.0 / 31.0);
        }
        if (flags & RAW) != 0u {
            color8 = texel8;
        } else {
            color8 = texel8 * color8 / 128.0;
        }
    }
    if (flags & DITHER) != 0u && ((pass_.options & 4u) != 0u || original) {
        let native = vec2<i32>(floor(pass_.origin + floor(frag.xy) / pass_.scale));
        color8 += DITHER_MATRIX[(native.y & 3) * 4 + (native.x & 3)];
    }
    color8 = clamp(color8, vec3<f32>(0.0), vec3<f32>(255.0));
    var color = color8 / 255.0;
    if fifteen {
        let c5 = floor(color8 / 8.0);
        color = (c5 * 8.0 + floor(c5 / 4.0)) / 255.0;
    }
    // A picture of VRAM keeps the mask bit in alpha; a diorama's eye and its
    // interface pane keep coverage there (they are drawn over other things).
    let coverage = (pass_.options & (PASS_WORLD | PASS_PANE)) != 0u;
    return Shaded(vec4<f32>(color, select(select(0.0, 1.0, mask), 1.0, coverage)), semi);
}

struct Targets {
    @location(0) color: vec4<f32>,
    // Depth (16 bits in r, g), class (b), flags (a): ps1/mod.rs AUX_*.
    @location(1) aux: vec4<f32>,
};

fn aux(in: VertexOut) -> vec4<f32> {
    if (in.mode.w & FEEDBACK) != 0u {
        let size = vec2<i32>(textureDimensions(feedback_aux));
        let point = clamp(vec2<i32>((affine(in) + vec2<f32>(0.5) - pass_.origin) * pass_.scale), vec2<i32>(0), size - vec2<i32>(1));
        return textureLoad(feedback_aux, point, 0);
    }
    let kind = (in.mode.w >> CLASS_SHIFT) & 15u;
    // The background (sky the game painted, ps1/mod.rs Builder::backdrop)
    // has no depth, as the fills it continues.
    let depth = select(u32(clamp(in.depth, 0.0, 65535.0)), 0u, kind == 0u);
    var flags = 0u;
    if (in.mode.w & BAKED) != 0u {
        flags |= 1u;
    }
    if (in.mode.w & BACKDROP) != 0u {
        flags |= 2u;
    }
    return vec4<f32>(f32(depth >> 8u), f32(depth & 255u), f32(kind), f32(flags)) / 255.0;
}

// The targets and the fragment's depth, for a camera within the scene
// (ps1/mod.rs DepthMode::Nearer): the depth its vertices interpolate to, in
// a float's full precision. wgpu's GL backend (WebGL2, GLES) would take it
// through GL's clip space (2z - 1), where a float's step near the far plane
// (0) is 6e-8: coarser beyond a few hundred units than the lift that keeps
// the game's order in a plane (ps1/frame.rs LIFT).
struct NearerTargets {
    @location(0) color: vec4<f32>,
    @location(1) aux: vec4<f32>,
    @builtin(frag_depth) depth: f32,
};

fn nearer(targets: Targets, in: VertexOut) -> NearerTargets {
    return NearerTargets(targets.color, targets.aux, in.depth_w.x / in.depth_w.y);
}

// Every fragment of an opaque primitive (semi-transparent texels included,
// drawn opaque as the GPU does without the semi flag).
fn opaque_targets(in: VertexOut) -> Targets {
    let s = shade(in, in.clip);
    return Targets(s.color, aux(in));
}

@fragment
fn fs_opaque(in: VertexOut) -> Targets {
    return opaque_targets(in);
}

@fragment
fn fs_opaque_nearer(in: VertexOut) -> NearerTargets {
    return nearer(opaque_targets(in), in);
}

// The opaque texels of a semi-transparent textured primitive.
fn opaque_texel_targets(in: VertexOut) -> Targets {
    let s = shade(in, in.clip);
    if s.semi {
        discard;
    }
    return Targets(s.color, aux(in));
}

@fragment
fn fs_opaque_texels(in: VertexOut) -> Targets {
    return opaque_texel_targets(in);
}

@fragment
fn fs_opaque_texels_nearer(in: VertexOut) -> NearerTargets {
    return nearer(opaque_texel_targets(in), in);
}

// The semi-transparent fragments. One blend state serves both targets
// (GLES and WebGL2 have no per-target blending): the source alpha is the
// blend weight, colour factors (SrcAlpha, OneMinusSrcAlpha) for mode 0 and
// (SrcAlpha, One) for the others, alpha (Zero, One). So the colour gets
// B/2+F/2, B+F, B-F or B+F/4 from weights 0.5, 1, 1 and 0.25, the
// auxiliary target stays as it was under weight 0 (scene effects keep the
// depth and class beneath; fs_mark marks an interface window's pixels), and
// both keep their stored alpha.
fn semi_targets(in: VertexOut) -> Targets {
    let s = shade(in, in.clip);
    if !s.semi {
        discard;
    }
    let mode = (in.mode.w >> BLEND_SHIFT) & 3u;
    var weight = 1.0;
    if mode == 0u {
        weight = 0.5;
    } else if mode == 3u {
        weight = 0.25;
    }
    // The diorama's interface pane is premultiplied and starts transparent,
    // blended (One, OneMinusSrcAlpha) for colour and alpha alike: B/2+F/2 is
    // F/2 at half coverage, B+F and B+F/4 add F and F/4 without coverage, so
    // the pane shows over the scene as the original blends them. B-F is
    // blended apart (ps1/mod.rs): F taken off as much of the pane as covers
    // (exactly B-F where the pane is opaque, as over the screen's interface),
    // and where the scene or the room shows through, what shows darkened by
    // F's brightest channel (no premultiplied form takes F off it).
    if (pass_.options & PASS_PANE) != 0u {
        if mode == 0u {
            return Targets(vec4<f32>(s.color.rgb * 0.5, 0.5), vec4<f32>(0.0));
        }
        if mode == 2u {
            return Targets(vec4<f32>(s.color.rgb, max(s.color.r, max(s.color.g, s.color.b))), vec4<f32>(0.0));
        }
        return Targets(vec4<f32>(s.color.rgb * weight, 0.0), vec4<f32>(0.0));
    }
    return Targets(vec4<f32>(s.color.rgb, weight), vec4<f32>(0.0));
}

@fragment
fn fs_semi(in: VertexOut) -> Targets {
    return semi_targets(in);
}

@fragment
fn fs_semi_nearer(in: VertexOut) -> NearerTargets {
    return nearer(semi_targets(in), in);
}

// A full-screen fade over a diorama's eye or its pane (ps1/diorama.rs): its
// colour over what the target holds, weighted by the target's own coverage
// (the blend's source factor is the destination's alpha), so what is
// transparent there (the room around the table) stays so. Opaque (blend key
// 4) it replaces; mode 0 halves, 1 and 3 add, 2 subtracts (ps1/mod.rs).
@fragment
fn fs_tint(in: VertexOut) -> Targets {
    let mode = (in.mode.w >> BLEND_SHIFT) & 3u;
    if (in.mode.w & SEMI) == 0u {
        return Targets(vec4<f32>(in.color.rgb, 1.0), vec4<f32>(0.0));
    }
    if mode == 0u {
        return Targets(vec4<f32>(in.color.rgb * 0.5, 0.5), vec4<f32>(0.0));
    }
    return Targets(vec4<f32>(in.color.rgb * select(1.0, 0.25, mode == 3u), 0.0), vec4<f32>(0.0));
}

// The semi-transparent pixels of an interface primitive, marked as
// interface in the auxiliary target (weight 1 there, its alpha included:
// flags 255) with the colour left as fs_semi made it (weight 0).
fn mark_targets(in: VertexOut) -> Targets {
    let s = shade(in, in.clip);
    if !s.semi {
        discard;
    }
    var info = aux(in);
    info.a = 1.0;
    return Targets(vec4<f32>(0.0), info);
}

@fragment
fn fs_mark(in: VertexOut) -> Targets {
    return mark_targets(in);
}

@fragment
fn fs_mark_nearer(in: VertexOut) -> NearerTargets {
    return nearer(mark_targets(in), in);
}

// GP0(02h) fills and uploads: a VRAM rectangle shown as itself.
struct BlitOut {
    @builtin(position) clip: vec4<f32>,
    @location(0) @interpolate(flat) color: vec4<f32>,
    @location(1) @interpolate(flat) upload: u32,
};

@vertex
fn vs_rect(in: VertexIn) -> BlitOut {
    var out: BlitOut;
    let ndc = (in.position - pass_.origin) / pass_.extent * vec2<f32>(2.0, -2.0) + vec2<f32>(-1.0, 1.0);
    // Fills clear depth to the far plane (0 where depth is reversed).
    out.clip = vec4<f32>(ndc, select(1.0, 0.0, (pass_.options & 16u) != 0u), 1.0);
    out.color = in.color;
    out.upload = in.mode.w;
    return out;
}

@fragment
fn fs_rect(in: BlitOut) -> Targets {
    // The diorama's pane keeps coverage in alpha (shade).
    let coverage = (pass_.options & PASS_PANE) != 0u;
    if in.upload == 0u {
        return Targets(vec4<f32>(in.color.rgb, select(0.0, 1.0, coverage)), vec4<f32>(0.0));
    }
    // An upload: the native pixel under this one.
    let native = vec2<i32>(floor(pass_.origin + floor(in.clip.xy) / pass_.scale));
    let pixel = vram_at(native.x, native.y);
    var c5 = rgb5(pixel);
    let color = (c5 * 8.0 + floor(c5 / 4.0)) / 255.0;
    // Uploaded pictures belong to the interface class (1): no world effects.
    let alpha = select(select(0.0, 1.0, (pixel & 0x8000u) != 0u), 1.0, coverage);
    return Targets(vec4<f32>(color, alpha), vec4<f32>(0.0, 0.0, 1.0 / 255.0, 0.0));
}

// Sprite shadows (ps1/effects.rs): a cast shadow is a sprite part's
// silhouette laid on the ground, a contact shadow a soft ellipse (untextured,
// uv -1..1) at the feet. r: intensity (the vertex colour's red), g, b: the
// caster's feet depth in 16 bits.
@fragment
fn fs_shadow(in: VertexOut) -> @location(0) vec4<f32> {
    var intensity = in.color.r;
    if (in.mode.w & TEXTURED) != 0u {
        let s = sample_nearest(in.mode, in.limits, affine(in));
        if s.coverage < 0.5 {
            discard;
        }
    } else {
        intensity *= 1.0 - smoothstep(0.35, 1.0, length(affine(in)));
    }
    if intensity <= 0.0 {
        discard;
    }
    let depth = u32(clamp(in.depth, 0.0, 65535.0));
    return vec4<f32>(intensity, f32(depth >> 8u) / 255.0, f32(depth & 255u) / 255.0, 1.0);
}
