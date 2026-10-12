// A movie frame from the planes the MDEC decoded: its chrominance brought to
// full resolution following the luminance and converted to RGB as the MDEC
// converts (fs_compose); then enlarged to the output edge-adaptively and
// sharpened: AMD FidelityFX Super Resolution 1's EASU and RCAS (ffx_fsr1.h,
// MIT licence), in plain float with texel loads (no gathers), on the
// sRGB-encoded values the MDEC makes; on a GPU that rasterizes in software,
// enlarged bilinearly instead (fs_linear). The output mixes in the MDEC's own
// picture where a movie eases into it (`enhancement` below 1).

struct Params {
    // The source's size in pixels (xy) and the target's (zw).
    size: vec4<f32>,
    // Where the display starts in VRAM (x in 16-bit VRAM pixels, y).
    display: vec4<f32>,
    // RCAS's sharpness: 2^-stops (1 the sharpest).
    sharpness: f32,
    // How much of the output is the rebuilt picture, the rest the MDEC's.
    enhancement: f32,
};

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var source: texture_2d<f32>;
// fs_compose's: the chrominance plane (Cb, Cr, and the mean luminance of
// the four pixels each sample covers), half the luminance's size each way.
@group(0) @binding(2) var chroma: texture_2d<f32>;
// fs_rcas's: the native VRAM, which holds the MDEC's 24-bit pixels.
@group(0) @binding(3) var vram: texture_2d<u32>;

@vertex
fn vs_full(@builtin(vertex_index) index: u32) -> @builtin(position) vec4<f32> {
    let uv = vec2<f32>(f32((index << 1u) & 2u), f32(index & 2u));
    return vec4<f32>(uv * 2.0 - 1.0, 0.0, 1.0);
}

// How quickly a chrominance sample's weight falls with distance (samples)
// and with the difference between the luminance under it and the pixel's
// (levels of 255).
const SPATIAL_SIGMA: f32 = 0.75;
const RANGE_SIGMA: f32 = 12.0;

// The MDEC's YCbCr to RGB coefficients (12 fractional bits).
const R_CR: f32 = 5743.0 / 4096.0;
const G_CB: f32 = -1408.0 / 4096.0;
const G_CR: f32 = -2926.0 / 4096.0;
const B_CB: f32 = 7258.0 / 4096.0;

// Each pixel's chrominance from the 4x4 nearest samples, weighted by
// distance and by how close the luminance under each is to the pixel's
// (`source` holds the luminance here): colour follows the brightness's
// edges instead of spreading over the 2x2 pixels a sample covers. Then RGB
// as the MDEC makes it (unsigned output).
@fragment
fn fs_compose(@builtin(position) at: vec4<f32>) -> @location(0) vec4<f32> {
    let p = vec2<i32>(at.xy);
    let l = textureLoad(source, p, 0).r;
    let size = vec2<i32>(textureDimensions(chroma));
    // A pixel's centre lies a quarter of a sample past the sample at or
    // before it (even pixels: three quarters past the one before).
    let base = ((p - 1) >> vec2<u32>(1u)) - 1;
    let f = select(vec2<f32>(0.75), vec2<f32>(0.25), (p & vec2<i32>(1)) == vec2<i32>(1));
    let dx = vec4<f32>(f.x + 1.0, f.x, 1.0 - f.x, 2.0 - f.x);
    let dy = vec4<f32>(f.y + 1.0, f.y, 1.0 - f.y, 2.0 - f.y);
    let wx = exp(-dx * dx / (2.0 * SPATIAL_SIGMA * SPATIAL_SIGMA));
    let wy = exp(-dy * dy / (2.0 * SPATIAL_SIGMA * SPATIAL_SIGMA));
    var sum = vec3<f32>(0.0);
    for (var j = 0; j < 4; j++) {
        for (var i = 0; i < 4; i++) {
            let sample = textureLoad(chroma, clamp(base + vec2<i32>(i, j), vec2<i32>(0), size - 1), 0);
            let d = l - sample.b;
            let w = wx[i] * wy[j] * max(exp(-d * d / (2.0 * RANGE_SIGMA * RANGE_SIGMA)), 1e-6);
            sum += vec3<f32>(sample.rg * w, w);
        }
    }
    let cb = sum.x / sum.z;
    let cr = sum.y / sum.z;
    let rgb = vec3<f32>(l + R_CR * cr, l + G_CB * cb + G_CR * cr, l + B_CB * cb);
    return vec4<f32>((clamp(rgb, vec3<f32>(-128.0), vec3<f32>(127.0)) + 128.0) / 255.0, 1.0);
}

fn texel(at: vec2<i32>) -> vec3<f32> {
    let size = vec2<i32>(textureDimensions(source));
    return textureLoad(source, clamp(at, vec2<i32>(0), size - 1), 0).rgb;
}

fn luma(c: vec3<f32>) -> f32 {
    return c.b * 0.5 + (c.r * 0.5 + c.g);
}

fn reciprocal(x: f32) -> f32 {
    return select(1.0 / x, 0.0, x == 0.0);
}

// The gradient's direction and length at one of the four texels nearest the
// sample point (weight `w`, its bilinear one), from the '+' around it:
//    a
//  b c d
//    e
// Length turns a gradient's reversal into 0, smoothly to 1 without one.
fn easu_set(dir: ptr<function, vec2<f32>>, len: ptr<function, f32>, w: f32, a: f32, b: f32, c: f32, d: f32, e: f32) {
    let dir_x = d - b;
    let len_x = clamp(abs(dir_x) * reciprocal(max(abs(d - c), abs(c - b))), 0.0, 1.0);
    let dir_y = e - a;
    let len_y = clamp(abs(dir_y) * reciprocal(max(abs(e - c), abs(c - a))), 0.0, 1.0);
    *dir += vec2<f32>(dir_x, dir_y) * w;
    *len += (len_x * len_x + len_y * len_y) * w;
}

// One tap: its offset from the sample point turned to the gradient's
// direction and stretched (`stretch`), weighted by an approximation of
// Lanczos-2 whose window `lobe` follows the edge, clipped at `clip`:
//  (25/16 * (2/5 * x^2 - 1)^2 - (25/16 - 1)) * (lobe * x^2 - 1)^2
fn easu_tap(sum: ptr<function, vec4<f32>>, offset: vec2<f32>, dir: vec2<f32>, stretch: vec2<f32>, lobe: f32, clip: f32, color: vec3<f32>) {
    let v = vec2<f32>(offset.x * dir.x + offset.y * dir.y, offset.y * dir.x - offset.x * dir.y) * stretch;
    let d2 = min(dot(v, v), clip);
    var base = 2.0 / 5.0 * d2 - 1.0;
    var window = lobe * d2 - 1.0;
    base = 25.0 / 16.0 * base * base - (25.0 / 16.0 - 1.0);
    window = window * window;
    let w = base * window;
    *sum += vec4<f32>(color * w, w);
}

// Edge-adaptive spatial upsampling: twelve source texels around the output
// pixel's point,
//     b c
//   e f g h
//   i j k l
//     n o
// weighted along the local edge, the result kept within the four nearest.
@fragment
fn fs_easu(@builtin(position) at: vec4<f32>) -> @location(0) vec4<f32> {
    let scale = params.size.xy / params.size.zw;
    var pp = floor(at.xy) * scale + (0.5 * scale - 0.5);
    let fp = floor(pp);
    pp -= fp;
    let o = vec2<i32>(fp);
    let b = texel(o + vec2<i32>(0, -1));
    let c = texel(o + vec2<i32>(1, -1));
    let e = texel(o + vec2<i32>(-1, 0));
    let f = texel(o);
    let g = texel(o + vec2<i32>(1, 0));
    let h = texel(o + vec2<i32>(2, 0));
    let i = texel(o + vec2<i32>(-1, 1));
    let j = texel(o + vec2<i32>(0, 1));
    let k = texel(o + vec2<i32>(1, 1));
    let l = texel(o + vec2<i32>(2, 1));
    let n = texel(o + vec2<i32>(0, 2));
    let p = texel(o + vec2<i32>(1, 2));
    let bl = luma(b);
    let cl = luma(c);
    let el = luma(e);
    let fl = luma(f);
    let gl = luma(g);
    let hl = luma(h);
    let il = luma(i);
    let jl = luma(j);
    let kl = luma(k);
    let ll = luma(l);
    let nl = luma(n);
    let pl = luma(p);
    var dir = vec2<f32>(0.0);
    var len = 0.0;
    easu_set(&dir, &len, (1.0 - pp.x) * (1.0 - pp.y), bl, el, fl, gl, jl);
    easu_set(&dir, &len, pp.x * (1.0 - pp.y), cl, fl, gl, hl, kl);
    easu_set(&dir, &len, (1.0 - pp.x) * pp.y, fl, il, jl, kl, nl);
    easu_set(&dir, &len, pp.x * pp.y, gl, jl, kl, ll, pl);
    // The direction normalized (along x where there is none).
    let r2 = dot(dir, dir);
    if r2 < 1.0 / 32768.0 {
        dir = vec2<f32>(1.0, dir.y);
    } else {
        dir *= inverseSqrt(r2);
    }
    // Length from {0 to 2} to {0 to 1}, shaped by its square.
    len = len * 0.5;
    len *= len;
    // Stretched from 1 along an axis to sqrt(2) on a diagonal; along the
    // edge to that, across it to half as wide.
    let along = dot(dir, dir) * reciprocal(max(abs(dir.x), abs(dir.y)));
    let stretch = vec2<f32>(1.0 + (along - 1.0) * len, 1.0 - 0.5 * len);
    // The window, from +/-sqrt(2) to a little beyond 2 with the edge.
    let lobe = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
    let clip = 1.0 / lobe;
    var sum = vec4<f32>(0.0);
    easu_tap(&sum, vec2<f32>(0.0, -1.0) - pp, dir, stretch, lobe, clip, b);
    easu_tap(&sum, vec2<f32>(1.0, -1.0) - pp, dir, stretch, lobe, clip, c);
    easu_tap(&sum, vec2<f32>(-1.0, 1.0) - pp, dir, stretch, lobe, clip, i);
    easu_tap(&sum, vec2<f32>(0.0, 1.0) - pp, dir, stretch, lobe, clip, j);
    easu_tap(&sum, vec2<f32>(0.0, 0.0) - pp, dir, stretch, lobe, clip, f);
    easu_tap(&sum, vec2<f32>(-1.0, 0.0) - pp, dir, stretch, lobe, clip, e);
    easu_tap(&sum, vec2<f32>(1.0, 1.0) - pp, dir, stretch, lobe, clip, k);
    easu_tap(&sum, vec2<f32>(2.0, 1.0) - pp, dir, stretch, lobe, clip, l);
    easu_tap(&sum, vec2<f32>(2.0, 0.0) - pp, dir, stretch, lobe, clip, h);
    easu_tap(&sum, vec2<f32>(1.0, 0.0) - pp, dir, stretch, lobe, clip, g);
    easu_tap(&sum, vec2<f32>(1.0, 2.0) - pp, dir, stretch, lobe, clip, p);
    easu_tap(&sum, vec2<f32>(0.0, 2.0) - pp, dir, stretch, lobe, clip, n);
    // Normalized, and kept within the four nearest texels (no ringing).
    let low = min(min(f, g), min(j, k));
    let high = max(max(f, g), max(j, k));
    return vec4<f32>(clamp(sum.rgb / max(sum.w, 1e-5), low, high), 1.0);
}

// The most RCAS sharpens.
const RCAS_LIMIT: f32 = 0.25 - 1.0 / 16.0;

// Robust contrast-adaptive sharpening: a negative lobe on the four
// neighbours as strong as the neighbourhood allows without clipping,
// weaker where the centre looks like noise.
//    b
//  d e f
//    h
@fragment
fn fs_rcas(@builtin(position) at: vec4<f32>) -> @location(0) vec4<f32> {
    let o = vec2<i32>(at.xy);
    let b = texel(o + vec2<i32>(0, -1));
    let d = texel(o + vec2<i32>(-1, 0));
    let e = texel(o);
    let f = texel(o + vec2<i32>(1, 0));
    let h = texel(o + vec2<i32>(0, 1));
    let bl = luma(b);
    let dl = luma(d);
    let el = luma(e);
    let fl = luma(f);
    let hl = luma(h);
    let range = max(max(max(bl, dl), max(el, fl)), hl) - min(min(min(bl, dl), min(el, fl)), hl);
    let noise = 1.0 - 0.5 * clamp(abs(0.25 * (bl + dl + fl + hl) - el) * reciprocal(range), 0.0, 1.0);
    let low = min(min(b, d), min(f, h));
    let high = max(max(b, d), max(f, h));
    let hit_low = min(low, e) / max(4.0 * high, vec3<f32>(1e-5));
    let hit_high = (1.0 - max(high, e)) / min(4.0 * low - 4.0, vec3<f32>(-1e-5));
    let lobes = max(-hit_low, hit_high);
    let lobe = max(-RCAS_LIMIT, min(max(lobes.r, max(lobes.g, lobes.b)), 0.0)) * params.sharpness * noise;
    return eased(at.xy, (lobe * (b + d + f + h) + e) / (4.0 * lobe + 1.0));
}

// The composed frame enlarged bilinearly, in place of EASU and RCAS on a GPU
// that rasterizes in software (where they cost some 50 ms a frame).
@fragment
fn fs_linear(@builtin(position) at: vec4<f32>) -> @location(0) vec4<f32> {
    let p = at.xy / params.size.zw * params.size.xy - 0.5;
    let o = vec2<i32>(floor(p));
    let f = p - floor(p);
    let top = mix(texel(o), texel(o + vec2<i32>(1, 0)), f.x);
    let bottom = mix(texel(o + vec2<i32>(0, 1)), texel(o + vec2<i32>(1, 1)), f.x);
    return eased(at.xy, mix(top, bottom, f.y));
}

// The output pixel: the rebuilt picture, eased into the MDEC's own where a
// movie ends on it (`enhancement` below 1).
fn eased(at: vec2<f32>, rebuilt: vec3<f32>) -> vec4<f32> {
    if params.enhancement >= 1.0 {
        return vec4<f32>(rebuilt, 1.0);
    }
    return vec4<f32>(mix(decoded(at), rebuilt, params.enhancement), 1.0);
}

// The MDEC's own pixel under an output point, as the console shows it: the
// display's 24-bit pixel there (three bytes across VRAM's 16-bit words).
fn decoded(at: vec2<f32>) -> vec3<f32> {
    let pixel = vec2<u32>(floor(at / params.size.zw * params.size.xy));
    let byte = 2u * u32(params.display.x) + 3u * pixel.x;
    let y = (u32(params.display.y) + pixel.y) & 511u;
    let word = byte / 2u;
    let low = textureLoad(vram, vec2<u32>(word & 1023u, y), 0).r;
    let high = textureLoad(vram, vec2<u32>((word + 1u) & 1023u, y), 0).r;
    let bytes = (low | (high << 16u)) >> ((byte & 1u) * 8u);
    return vec3<f32>(f32(bytes & 255u), f32((bytes >> 8u) & 255u), f32((bytes >> 16u) & 255u)) / 255.0;
}
