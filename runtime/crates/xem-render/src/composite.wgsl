// Draws one textured quad with premultiplied alpha.

struct QuadUniform {
    // Maps the unit square (u right, v down) to clip space.
    transform: mat4x4<f32>,
    opacity: f32,
    // The texture holds sRGB-encoded values in a linear format.
    decode: u32,
    // The target holds encoded values in a linear format.
    encode: u32,
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

@fragment
fn fs_quad(in: Out) -> @location(0) vec4<f32> {
    let texel = textureSample(image, image_sampler, in.uv);
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
