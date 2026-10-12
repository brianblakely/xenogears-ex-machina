// Draws one textured quad with premultiplied alpha.

struct QuadUniform {
    // Maps the unit square (u right, v down) to clip space.
    transform: mat4x4<f32>,
    opacity: f32,
    // The texture holds sRGB-encoded values in a linear format.
    decode: u32,
    // The target holds encoded values in a linear format.
    encode: u32,
    // Bicubic (Catmull-Rom) scaling through the linear sampler.
    bicubic: u32,
};

@group(0) @binding(0) var<uniform> quad: QuadUniform;
@group(1) @binding(0) var image: texture_2d<f32>;
@group(1) @binding(1) var image_sampler: sampler;

struct Out {
    @builtin(position) clip: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_quad(@builtin(vertex_index) index: u32) -> Out {
    var corners = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 0.0), vec2<f32>(0.0, 1.0), vec2<f32>(1.0, 0.0),
        vec2<f32>(1.0, 0.0), vec2<f32>(0.0, 1.0), vec2<f32>(1.0, 1.0),
    );
    var out: Out;
    out.uv = corners[index];
    out.clip = quad.transform * vec4<f32>(out.uv, 0.0, 1.0);
    return out;
}

fn to_linear(c: vec3<f32>) -> vec3<f32> {
    return select(pow((c + 0.055) / 1.055, vec3<f32>(2.4)), c / 12.92, c <= vec3<f32>(0.04045));
}

fn to_srgb(c: vec3<f32>) -> vec3<f32> {
    return select(1.055 * pow(c, vec3<f32>(1.0 / 2.4)) - 0.055, c * 12.92, c <= vec3<f32>(0.0031308));
}

// Catmull-Rom from nine bilinear taps (the outer weights' pairs merged):
// a small picture enlarged many times, smooth without the bilinear blur.
fn sample_bicubic(uv: vec2<f32>) -> vec4<f32> {
    let size = vec2<f32>(textureDimensions(image));
    let at = uv * size;
    let centre = floor(at - 0.5) + 0.5;
    let f = at - centre;
    let w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    let w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    let w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    let w3 = f * f * (-0.5 + 0.5 * f);
    let w12 = w1 + w2;
    let p0 = (centre - 1.0) / size;
    let p3 = (centre + 2.0) / size;
    let p12 = (centre + w2 / w12) / size;
    var sum = vec4<f32>(0.0);
    sum += textureSample(image, image_sampler, vec2<f32>(p0.x, p0.y)) * w0.x * w0.y;
    sum += textureSample(image, image_sampler, vec2<f32>(p12.x, p0.y)) * w12.x * w0.y;
    sum += textureSample(image, image_sampler, vec2<f32>(p3.x, p0.y)) * w3.x * w0.y;
    sum += textureSample(image, image_sampler, vec2<f32>(p0.x, p12.y)) * w0.x * w12.y;
    sum += textureSample(image, image_sampler, vec2<f32>(p12.x, p12.y)) * w12.x * w12.y;
    sum += textureSample(image, image_sampler, vec2<f32>(p3.x, p12.y)) * w3.x * w12.y;
    sum += textureSample(image, image_sampler, vec2<f32>(p0.x, p3.y)) * w0.x * w3.y;
    sum += textureSample(image, image_sampler, vec2<f32>(p12.x, p3.y)) * w12.x * w3.y;
    sum += textureSample(image, image_sampler, vec2<f32>(p3.x, p3.y)) * w3.x * w3.y;
    return clamp(sum, vec4<f32>(0.0), vec4<f32>(1.0));
}

@fragment
fn fs_quad(in: Out) -> @location(0) vec4<f32> {
    var texel = textureSample(image, image_sampler, in.uv);
    if quad.bicubic != 0u {
        texel = sample_bicubic(in.uv);
    }
    var color = texel.rgb;
    if texel.a > 0.0 {
        color = color / texel.a;
        if quad.decode != 0u {
            color = to_linear(color);
        }
        if quad.encode != 0u {
            color = to_srgb(color);
        }
        color = color * texel.a;
    }
    return vec4<f32>(color, texel.a) * quad.opacity;
}
