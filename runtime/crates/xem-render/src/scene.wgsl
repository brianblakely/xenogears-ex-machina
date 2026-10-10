// The test scene: a sky gradient, a lit floor grid and lit rotating cubes.

struct View {
    view_proj: mat4x4<f32>,
    inv_view_proj: mat4x4<f32>,
    eye: vec4<f32>,
};

@group(0) @binding(0) var<uniform> view: View;

const LIGHT: vec3<f32> = vec3<f32>(0.42, 0.82, 0.38);
const HORIZON: vec3<f32> = vec3<f32>(0.55, 0.62, 0.72);

fn sky(dir: vec3<f32>) -> vec3<f32> {
    // Below the horizon the faded floor edge meets the horizon colour.
    let zenith = vec3<f32>(0.10, 0.22, 0.50);
    return mix(HORIZON, zenith, sqrt(clamp(dir.y, 0.0, 1.0)));
}

struct SkyOut {
    @builtin(position) clip: vec4<f32>,
    @location(0) ndc: vec2<f32>,
};

@vertex
fn vs_sky(@builtin(vertex_index) index: u32) -> SkyOut {
    let corner = vec2<f32>(f32((index << 1u) & 2u), f32(index & 2u)) * 2.0 - 1.0;
    var out: SkyOut;
    out.clip = vec4<f32>(corner, 1.0, 1.0);
    out.ndc = corner;
    return out;
}

@fragment
fn fs_sky(in: SkyOut) -> @location(0) vec4<f32> {
    let far = view.inv_view_proj * vec4<f32>(in.ndc, 1.0, 1.0);
    let near = view.inv_view_proj * vec4<f32>(in.ndc, 0.0, 1.0);
    let dir = normalize(far.xyz / far.w - near.xyz / near.w);
    return vec4<f32>(sky(dir), 1.0);
}

struct FloorOut {
    @builtin(position) clip: vec4<f32>,
    @location(0) world: vec3<f32>,
};

@vertex
fn vs_floor(@builtin(vertex_index) index: u32) -> FloorOut {
    var corners = array<vec2<f32>, 6>(
        vec2<f32>(-1.0, -1.0), vec2<f32>(1.0, 1.0), vec2<f32>(1.0, -1.0),
        vec2<f32>(-1.0, -1.0), vec2<f32>(-1.0, 1.0), vec2<f32>(1.0, 1.0),
    );
    let xz = corners[index] * 40.0;
    var out: FloorOut;
    out.world = vec3<f32>(xz.x, 0.0, xz.y);
    out.clip = view.view_proj * vec4<f32>(out.world, 1.0);
    return out;
}

@fragment
fn fs_floor(in: FloorOut) -> @location(0) vec4<f32> {
    let coord = in.world.xz;
    let width = max(fwidth(coord), vec2<f32>(1e-4));
    let cell = abs(fract(coord - 0.5) - 0.5) / width;
    let line = 1.0 - min(min(cell.x, cell.y), 1.0);
    let axis = 1.0 - min(min(abs(coord.x) / width.x, abs(coord.y) / width.y), 1.0);
    var color = mix(vec3<f32>(0.10, 0.11, 0.13), vec3<f32>(0.45, 0.50, 0.58), line);
    color = mix(color, vec3<f32>(0.85, 0.55, 0.25), axis);
    color = color * (0.35 + 0.65 * normalize(LIGHT).y);
    let fade = smoothstep(12.0, 38.0, length(coord - view.eye.xz));
    return vec4<f32>(mix(color, HORIZON, fade), 1.0);
}

struct MeshIn {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) model0: vec4<f32>,
    @location(3) model1: vec4<f32>,
    @location(4) model2: vec4<f32>,
    @location(5) model3: vec4<f32>,
    @location(6) color: vec4<f32>,
};

struct MeshOut {
    @builtin(position) clip: vec4<f32>,
    @location(0) world: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) color: vec3<f32>,
};

@vertex
fn vs_mesh(in: MeshIn) -> MeshOut {
    let model = mat4x4<f32>(in.model0, in.model1, in.model2, in.model3);
    let world = model * vec4<f32>(in.position, 1.0);
    var out: MeshOut;
    out.clip = view.view_proj * world;
    out.world = world.xyz;
    // Models are rotations, translations and uniform scales.
    out.normal = (model * vec4<f32>(in.normal, 0.0)).xyz;
    out.color = in.color.rgb;
    return out;
}

@fragment
fn fs_mesh(in: MeshOut) -> @location(0) vec4<f32> {
    let n = normalize(in.normal);
    let l = normalize(LIGHT);
    let v = normalize(view.eye.xyz - in.world);
    let diffuse = max(dot(n, l), 0.0);
    let specular = pow(max(dot(n, normalize(l + v)), 0.0), 48.0) * 0.4;
    let ambient = 0.18 + 0.12 * n.y;
    return vec4<f32>(in.color * (ambient + 0.8 * diffuse) + vec3<f32>(specular), 1.0);
}
