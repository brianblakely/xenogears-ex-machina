// A frame drawn with its interface alone (ps1/frame.rs Interface::Only)
// as a picture to lay over another, its coverage as alpha (premultiplied):
// FrameRenderer::pane. The colour target's alpha is the PS1's mask bit, so
// the auxiliary target says what was drawn: its class (b; 0 the
// background) and its marks (a; 255 a semi-transparent interface pixel,
// whose colour the blend already weighed over nothing).
// - drawn by the interface (any class but the background): opaque;
// - a semi-transparent interface pixel (a window): half covering, as the
//   original's commonest blend lets half of what lies behind through;
// - colour without a class (a semi-transparent effect over the whole
//   screen, which the builder leaves unmarked): added to what lies behind,
//   as an additive flash is (alpha 0);
// - nothing drawn: transparent.

@group(0) @binding(0) var color: texture_2d<f32>;
@group(0) @binding(1) var aux: texture_2d<f32>;

@vertex
fn vs_pane(@builtin(vertex_index) index: u32) -> @builtin(position) vec4<f32> {
    // One triangle over the whole target.
    let uv = vec2<f32>(f32((index << 1u) & 2u), f32(index & 2u));
    return vec4<f32>(uv * vec2<f32>(2.0, -2.0) + vec2<f32>(-1.0, 1.0), 0.0, 1.0);
}

@fragment
fn fs_pane(@builtin(position) at: vec4<f32>) -> @location(0) vec4<f32> {
    let point = vec2<i32>(floor(at.xy));
    let c = textureLoad(color, point, 0).rgb;
    let info = textureLoad(aux, point, 0);
    var alpha = 0.0;
    if info.a > 0.999 {
        alpha = 0.5;
    } else if info.b > 0.0 {
        alpha = 1.0;
    }
    return vec4<f32>(c, alpha);
}
