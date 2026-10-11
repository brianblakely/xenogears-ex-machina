// HD-2D-inspired effects over the displayed frame: scene-aware lighting,
// ambient occlusion, sprite shadows, fog, light shafts, bloom, depth of
// field and colour grading. Interface pixels (auxiliary class 1: text,
// windows, portraits) keep their original colour through every pass.
//
// The auxiliary target gives each pixel the GTE depth (SZ, 16 bits in r, g)
// and class (b: 0 background, 1 interface, 2 world, 3 model, 4 sprite) of
// the primitive that drew it last; flags (a) bit 0 marks baked shading.

struct Params {
    size: vec2<f32>,
    inv_size: vec2<f32>,
    // Frame pixels per native pixel; projection distance H in native pixels.
    scale: f32,
    focal: f32,
    // Projection centre in frame pixels.
    center: vec2<f32>,
    // Key light direction (view space, towards the light) and colour * strength.
    light_dir: vec4<f32>,
    light_color: vec4<f32>,
    // Ambient colour; w: ambient occlusion strength.
    ambient: vec4<f32>,
    // Fog colour; w: fog strength.
    fog: vec4<f32>,
    // Fog start and end depth, the nearest and farthest depth kept sharp
    // (all SZ units).
    ranges: vec4<f32>,
    // Light-shaft source (0-1 frame coordinates), strength, threshold.
    shaft: vec4<f32>,
    // Bloom strength, threshold, knee, lighting strength.
    bloom: vec4<f32>,
    // Exposure, contrast, saturation, warmth.
    grade: vec4<f32>,
    // Vignette, grading strength, depth-of-field strength, shadow strength.
    grade2: vec4<f32>,
    // Samples: ambient occlusion, depth of field, light shafts; debug view
    // (0 off, 1 shadow received, 2 depth, 3 class).
    quality: vec4<u32>,
    // Blur pass: source texel size (xy); the scene average's easing (z).
    step: vec4<f32>,
};

@group(0) @binding(0) var<uniform> p: Params;
@group(0) @binding(1) var linear_sampler: sampler;
@group(1) @binding(0) var tex0: texture_2d<f32>;
@group(1) @binding(1) var tex1: texture_2d<f32>;
@group(1) @binding(2) var tex2: texture_2d<f32>;
@group(1) @binding(3) var tex3: texture_2d<f32>;
@group(1) @binding(4) var tex4: texture_2d<f32>;
@group(1) @binding(5) var tex5: texture_2d<f32>;

struct Out {
    @builtin(position) clip: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_full(@builtin(vertex_index) index: u32) -> Out {
    let corner = vec2<f32>(f32((index << 1u) & 2u), f32(index & 2u));
    var out: Out;
    out.clip = vec4<f32>(corner * vec2<f32>(2.0, -2.0) + vec2<f32>(-1.0, 1.0), 0.0, 1.0);
    out.uv = corner;
    return out;
}

fn to_linear(c: vec3<f32>) -> vec3<f32> {
    return select(pow((c + 0.055) / 1.055, vec3<f32>(2.4)), c / 12.92, c <= vec3<f32>(0.04045));
}

fn to_srgb(c: vec3<f32>) -> vec3<f32> {
    let c2 = max(c, vec3<f32>(0.0));
    return select(1.055 * pow(c2, vec3<f32>(1.0 / 2.4)) - 0.055, c2 * 12.92, c2 <= vec3<f32>(0.0031308));
}

fn luma(c: vec3<f32>) -> f32 {
    return dot(c, vec3<f32>(0.2126, 0.7152, 0.0722));
}

struct Aux {
    depth: f32,
    kind: u32,
    baked: bool,
};

fn aux_at(pixel: vec2<i32>) -> Aux {
    let dims = vec2<i32>(textureDimensions(tex1));
    let a = textureLoad(tex1, clamp(pixel, vec2<i32>(0), dims - 1), 0);
    let bytes = vec4<u32>(round(a * 255.0));
    return Aux(f32(bytes.r * 256u + bytes.g), bytes.b, (bytes.a & 1u) != 0u);
}

fn is_scene(kind: u32) -> bool {
    return kind != 1u;
}

// View-space position of a frame pixel at depth z (SZ units).
fn view_position(pixel: vec2<f32>, z: f32) -> vec3<f32> {
    let native = (pixel - p.center) / p.scale;
    return vec3<f32>(native.x * z / p.focal, -native.y * z / p.focal, z);
}

fn hash(v: vec2<f32>) -> f32 {
    return fract(sin(dot(v, vec2<f32>(12.9898, 78.233))) * 43758.5453);
}

// Pass 1: lighting, ambient occlusion, sprite shadows and fog, in linear light.
// tex0 frame colour, tex1 auxiliary, tex2 shadow mask.
@fragment
fn fs_light(in: Out) -> @location(0) vec4<f32> {
    let pixel = vec2<i32>(in.clip.xy);
    let base = textureLoad(tex0, pixel, 0);
    var color = to_linear(base.rgb);
    let a = aux_at(pixel);
    if !is_scene(a.kind) {
        return vec4<f32>(color, 0.0);
    }
    let lighting = p.bloom.w;
    if p.quality.w == 2u {
        return vec4<f32>(vec3<f32>(a.depth / 8192.0), 1.0);
    }
    if p.quality.w == 3u {
        return vec4<f32>(f32(a.kind == 2u), f32(a.kind == 3u), f32(a.kind == 4u), 1.0);
    }
    if a.depth > 0.0 {
        let center = view_position(in.clip.xy, a.depth);
        // Normal from the depths a native pixel away (the depth is stored in
        // whole GTE units, which neighbouring scaled pixels share and then
        // step: nearer ones would light in bands), the nearest difference
        // of the two sides to keep edges.
        let span = max(round(p.scale), 1.0);
        let reach = i32(span);
        let r = aux_at(pixel + vec2<i32>(reach, 0));
        let l = aux_at(pixel - vec2<i32>(reach, 0));
        let d = aux_at(pixel + vec2<i32>(0, reach));
        let u = aux_at(pixel - vec2<i32>(0, reach));
        var dx = view_position(in.clip.xy + vec2<f32>(span, 0.0), r.depth) - center;
        let dx2 = center - view_position(in.clip.xy - vec2<f32>(span, 0.0), l.depth);
        if r.depth <= 0.0 || (l.depth > 0.0 && abs(dx2.z) < abs(dx.z)) {
            dx = dx2;
        }
        var dy = view_position(in.clip.xy + vec2<f32>(0.0, span), d.depth) - center;
        let dy2 = center - view_position(in.clip.xy - vec2<f32>(0.0, span), u.depth);
        if d.depth <= 0.0 || (u.depth > 0.0 && abs(dy2.z) < abs(dy.z)) {
            dy = dy2;
        }
        var normal = normalize(cross(dy, dx));
        if a.kind == 4u || dot(normal, normal) < 0.5 {
            // Sprites face the camera.
            normal = vec3<f32>(0.0, 0.0, -1.0);
        }
        if normal.z > 0.0 {
            normal = -normal;
        }
        // Lighting relights gently over the original (baked) shading.
        let diffuse = max(dot(normal, normalize(p.light_dir.xyz)), 0.0);
        let light = p.ambient.rgb + p.light_color.rgb * diffuse;
        var strength = lighting;
        if a.baked {
            strength *= 0.6;
        }
        color = color * mix(vec3<f32>(1.0), light, strength);

        // Ambient occlusion: nearer samples in a disc around the pixel.
        let samples = p.quality.x;
        if samples > 0u && p.ambient.w > 0.0 && a.kind != 4u {
            let radius = clamp(60.0 * p.focal / a.depth, 2.0, 24.0) * p.scale;
            var occlusion = 0.0;
            let rotation = hash(in.clip.xy) * 6.2831853;
            for (var i = 0u; i < samples; i++) {
                let t = (f32(i) + 0.5) / f32(samples);
                let angle = rotation + f32(i) * 2.399963;
                let offset = vec2<f32>(cos(angle), sin(angle)) * radius * sqrt(t);
                let s = aux_at(pixel + vec2<i32>(offset));
                if s.depth > 0.0 && is_scene(s.kind) {
                    let delta = a.depth - s.depth;
                    let range = 0.08 * a.depth;
                    if delta > 0.01 * a.depth && delta < range * 3.0 {
                        occlusion += 1.0 - smoothstep(range, range * 3.0, delta);
                    }
                }
            }
            color *= 1.0 - p.ambient.w * 0.8 * occlusion / f32(samples);
        }

        // Sprite shadows on what lies around or behind the caster's floor
        // point. No hard depth test: a surface takes the shadow in full from
        // a little in front of the floor point backwards, fading out nearer
        // than that, so a surface at about the caster's depth never flickers
        // between the two. Floors take it in full, upright surfaces less.
        if p.grade2.w > 0.0 && a.kind != 4u {
            var shadow = 0.0;
            let spacing = i32(max(p.scale, 1.0)) * 2;
            let weights = array<f32, 3>(0.25, 0.5, 0.25);
            for (var y = -1; y <= 1; y++) {
                for (var x = -1; x <= 1; x++) {
                    let m = textureLoad(tex2, clamp(pixel + vec2<i32>(x, y) * spacing, vec2<i32>(0), vec2<i32>(p.size) - 1), 0);
                    if m.r > 0.0 {
                        let caster = m.g * 255.0 * 256.0 + m.b * 255.0;
                        let receive = smoothstep(caster * 0.76, caster * 0.9, a.depth);
                        shadow += m.r * receive * weights[x + 1] * weights[y + 1];
                    }
                }
            }
            let floor = mix(0.55, 1.0, smoothstep(0.15, 0.6, normal.y));
            color *= 1.0 - p.grade2.w * 0.85 * shadow * floor;
            if p.quality.w == 1u {
                return vec4<f32>(shadow, textureLoad(tex2, pixel, 0).r, 0.0, 1.0);
            }
        }

        // Aerial perspective: distance lightens and desaturates the scene
        // towards the haze tint at the scene's own brightness (tex3: the
        // frame's average scene colour), so dark places stay dark.
        let average = textureLoad(tex3, vec2<i32>(0, 0), 0).rgb;
        let haze = mix(p.fog.rgb * clamp(luma(average) * 2.6, 0.03, 0.85), average * 1.8, 0.35);
        let fog = smoothstep(p.ranges.x, p.ranges.y, a.depth) * p.fog.w;
        let faded = mix(vec3<f32>(luma(color)), color, 1.0 - 0.6 * fog);
        color = mix(faded, haze, fog);
    }
    return vec4<f32>(color, 1.0);
}

// Pass 0: the average colour of the frame's scene pixels (one texel),
// eased over frames (step.z).
// tex0 frame colour, tex1 auxiliary.
@fragment
fn fs_average(in: Out) -> @location(0) vec4<f32> {
    let dims = vec2<f32>(textureDimensions(tex0));
    var sum = vec3<f32>(0.0);
    var count = 0.0;
    for (var y = 0; y < 16; y++) {
        for (var x = 0; x < 16; x++) {
            let pixel = vec2<i32>((vec2<f32>(f32(x), f32(y)) + 0.5) / 16.0 * dims);
            if is_scene(aux_at(pixel).kind) {
                sum += to_linear(textureLoad(tex0, pixel, 0).rgb);
                count += 1.0;
            }
        }
    }
    return vec4<f32>(sum / max(count, 1.0), p.step.z);
}

// Pass 2: bright parts for bloom (rgb) and light shafts (a: far bright
// parts), at half resolution. tex0 lit, tex1 auxiliary.
@fragment
fn fs_bright(in: Out) -> @location(0) vec4<f32> {
    let lit = textureSampleLevel(tex0, linear_sampler, in.uv, 0.0);
    let pixel = vec2<i32>(in.uv * vec2<f32>(textureDimensions(tex1)));
    let a = aux_at(pixel);
    if lit.a < 0.5 {
        return vec4<f32>(0.0);
    }
    let l = luma(lit.rgb);
    let threshold = p.bloom.y;
    let knee = max(p.bloom.z, 1e-4);
    let soft = clamp(l - threshold + knee, 0.0, 2.0 * knee);
    let contribution = max(soft * soft / (4.0 * knee), l - threshold) / max(l, 1e-4);
    // Coloured lights (lamps, fire, magic) glow more than white walls, and
    // characters' bright pixels (hair, cloth) barely at all.
    let saturation = max(max(lit.r, lit.g), lit.b) - min(min(lit.r, lit.g), lit.b);
    var glow = 1.0 + 0.6 * saturation;
    if a.kind == 4u {
        glow *= 0.25;
    }
    let bloom = lit.rgb * max(contribution, 0.0) * glow;
    // Shaft sources: bright pixels without depth (sky, light) or beyond
    // the fog's end.
    var shaft = 0.0;
    if a.depth <= 0.0 || a.depth > p.ranges.y {
        shaft = smoothstep(p.shaft.w, p.shaft.w + 0.25, l);
    }
    return vec4<f32>(bloom, shaft);
}

// Bloom chain: 13-tap downsample (tex0 the level above).
@fragment
fn fs_down(in: Out) -> @location(0) vec4<f32> {
    let t = p.step.xy;
    var c = textureSampleLevel(tex0, linear_sampler, in.uv, 0.0).rgb * 0.125;
    c += (textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(-1.0, -1.0), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(1.0, -1.0), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(-1.0, 1.0), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(1.0, 1.0), 0.0).rgb) * 0.125;
    c += (textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(-2.0, 0.0), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(2.0, 0.0), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(0.0, -2.0), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(0.0, 2.0), 0.0).rgb) * 0.0625;
    c += (textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(-2.0, -2.0), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(2.0, -2.0), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(-2.0, 2.0), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + t * vec2<f32>(2.0, 2.0), 0.0).rgb) * 0.03125;
    return vec4<f32>(c, 1.0);
}

// Bloom chain: tent upsample of the level below, added onto this level.
@fragment
fn fs_up(in: Out) -> @location(0) vec4<f32> {
    let t = p.step.xy;
    var c = textureSampleLevel(tex0, linear_sampler, in.uv, 0.0).rgb * 4.0;
    c += (textureSampleLevel(tex0, linear_sampler, in.uv + vec2<f32>(t.x, 0.0), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv - vec2<f32>(t.x, 0.0), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + vec2<f32>(0.0, t.y), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv - vec2<f32>(0.0, t.y), 0.0).rgb) * 2.0;
    c += textureSampleLevel(tex0, linear_sampler, in.uv + t, 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv - t, 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + vec2<f32>(t.x, -t.y), 0.0).rgb
        + textureSampleLevel(tex0, linear_sampler, in.uv + vec2<f32>(-t.x, t.y), 0.0).rgb;
    return vec4<f32>(c / 16.0, 1.0);
}

// Light shafts: the bright pass's shaft sources blurred towards the light
// (tex0 bright, half resolution).
@fragment
fn fs_shafts(in: Out) -> @location(0) vec4<f32> {
    let samples = p.quality.z;
    if samples == 0u {
        return vec4<f32>(0.0);
    }
    let source = p.shaft.xy;
    let delta = (in.uv - source) / f32(samples) * 0.85;
    var uv = in.uv;
    var decay = 1.0;
    var sum = 0.0;
    let jitter = hash(in.clip.xy);
    uv -= delta * jitter;
    for (var i = 0u; i < samples; i++) {
        uv -= delta;
        sum += textureSampleLevel(tex0, linear_sampler, uv, 0.0).a * decay;
        decay *= 0.965;
    }
    return vec4<f32>(sum / f32(samples), 0.0, 0.0, 1.0);
}

// The circle of confusion, 0 (sharp) to 1: the playfield between the
// nearest and farthest depths kept sharp is; beyond it the blur grows with
// the relative distance, faster towards the camera (a miniature's shallow
// focus: foreground and far background soft, the subjects crisp).
fn coc(depth: f32, kind: u32) -> f32 {
    if !is_scene(kind) {
        return 0.0;
    }
    let near = p.ranges.z;
    let far = p.ranges.w;
    // Background without depth is far.
    var z = depth;
    if z <= 0.0 {
        z = far * 4.0;
    }
    if z > far {
        return clamp((z - far) / far, 0.0, 1.0);
    }
    if z < near {
        return clamp((near - z) / (near * 0.45), 0.0, 1.0);
    }
    return 0.0;
}

// Depth of field: a disc gather at half resolution weighted by each
// sample's own circle of confusion, so sharp subjects do not bleed.
// tex0 lit, tex1 auxiliary. Output a: this pixel's circle.
@fragment
fn fs_dof(in: Out) -> @location(0) vec4<f32> {
    let full = vec2<f32>(textureDimensions(tex1));
    let pixel = vec2<i32>(in.uv * full);
    let a = aux_at(pixel);
    let own = coc(a.depth, a.kind);
    let samples = p.quality.y;
    let radius = 6.0 * p.scale * p.grade2.z;
    var sum = textureSampleLevel(tex0, linear_sampler, in.uv, 0.0).rgb;
    var weight = 1.0;
    let rotation = hash(in.clip.xy) * 6.2831853;
    for (var i = 0u; i < samples; i++) {
        let t = (f32(i) + 0.5) / f32(samples);
        let angle = rotation + f32(i) * 2.399963;
        let offset = vec2<f32>(cos(angle), sin(angle)) * radius * sqrt(t) * max(own, 0.15);
        let s_uv = in.uv + offset / full;
        let s = aux_at(vec2<i32>(s_uv * full));
        let c = coc(s.depth, s.kind);
        let lit = textureSampleLevel(tex0, linear_sampler, s_uv, 0.0);
        // Interface pixels never blur into the scene; bright samples weigh
        // more, so out-of-focus lights open into discs.
        let bokeh = 1.0 + 2.0 * max(luma(lit.rgb) - 0.6, 0.0);
        let w = select(0.0, (max(c, own * 0.5) + 0.02) * bokeh, lit.a > 0.5);
        sum += lit.rgb * w;
        weight += w;
    }
    return vec4<f32>(sum / weight, own);
}

// Highlights ease into white instead of clipping (bloom and exposure add
// light); below the knee colours stay as they are.
fn shoulder(c: vec3<f32>) -> vec3<f32> {
    let k = 0.62;
    let over = max(c - vec3<f32>(k), vec3<f32>(0.0));
    return min(c, vec3<f32>(k)) + over / (vec3<f32>(1.0) + over / (1.0 - k));
}

// Composite: depth of field, bloom and shafts over the lit scene, graded,
// with interface pixels from the original frame. Output sRGB-encoded.
// tex0 lit, tex1 auxiliary, tex2 bloom, tex3 shafts, tex4 depth of field,
// tex5 the average scene colour.
@fragment
fn fs_composite(in: Out) -> @location(0) vec4<f32> {
    let pixel = vec2<i32>(in.clip.xy);
    let lit = textureLoad(tex0, pixel, 0);
    if lit.a < 0.5 || p.quality.w != 0u {
        return vec4<f32>(to_srgb(lit.rgb), 1.0);
    }
    var color = lit.rgb;
    let a = aux_at(pixel);
    if p.grade2.z > 0.0 && p.quality.y > 0u {
        let dof = textureSampleLevel(tex4, linear_sampler, in.uv, 0.0);
        let own = coc(a.depth, a.kind);
        color = mix(color, dof.rgb, smoothstep(0.0, 0.5, own) * min(p.grade2.z * 1.6, 1.0));
    }
    // Bloom: the glow of bright parts, and the light it throws on the
    // surfaces around them.
    let glow = textureSampleLevel(tex2, linear_sampler, in.uv, 0.0).rgb * p.bloom.x;
    color += glow + color * glow * 0.6;
    let shafts = textureSampleLevel(tex3, linear_sampler, in.uv, 0.0).r;
    color += (p.light_color.rgb + p.ambient.rgb) * shafts * p.shaft.z * 0.5;

    let g = p.grade2.y;
    // Exposure towards a bright, airy middle grey; dark places brighten
    // only so far.
    let average = luma(textureLoad(tex5, vec2<i32>(0, 0), 0).rgb);
    let automatic = clamp(0.18 / max(average, 0.01), 0.85, 1.5);
    var graded = color * mix(1.0, automatic, g) * exp2(p.grade.x * g);
    // Split toning: cool shadows, warm highlights.
    let tone = mix(vec3<f32>(0.9, 0.98, 1.1), vec3<f32>(1.08, 1.0, 0.88), smoothstep(0.05, 0.6, luma(graded)));
    graded *= mix(vec3<f32>(1.0), tone, g * clamp(p.grade.w, 0.0, 1.0));
    graded = mix(vec3<f32>(luma(graded)), graded, mix(1.0, p.grade.z, g));
    graded = mix(graded, shoulder(graded), g);
    // Contrast around mid grey in a perceptual space.
    var s = to_srgb(graded);
    s = clamp((s - 0.5) * mix(1.0, p.grade.y, g) + 0.5, vec3<f32>(0.0), vec3<f32>(1.0));
    let centered = in.uv * 2.0 - 1.0;
    let vignette = 1.0 - p.grade2.x * g * smoothstep(0.35, 1.6, dot(centered, centered));
    return vec4<f32>(s * vignette, 1.0);
}
