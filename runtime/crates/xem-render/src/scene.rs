use bytemuck::{Pod, Zeroable};
use glam::{Mat4, Quat, Vec3};
use std::collections::HashMap;
use wgpu::util::DeviceExt;

const DEPTH_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Depth32Float;

/// A rectangle of a render target in pixels.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Viewport {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

impl Viewport {
    /// The whole of a `width` x `height` target.
    pub fn full(width: u32, height: u32) -> Self {
        Self {
            x: 0.0,
            y: 0.0,
            width: width as f32,
            height: height as f32,
        }
    }
}

/// One view of the scene: an eye of a stereo pair, a spectator or a flat
/// window. `proj` maps to wgpu clip space (depth 0..1).
pub struct ViewTarget<'a> {
    pub view: Mat4,
    pub proj: Mat4,
    pub target: &'a wgpu::TextureView,
    pub viewport: Viewport,
}

/// A camera circling the scene, for hosts without head tracking.
pub fn demo_camera(aspect: f32, time: f32) -> (Mat4, Mat4) {
    let angle = time * 0.15;
    let eye = Vec3::new(angle.sin() * 9.0, 4.0, angle.cos() * 9.0);
    let view = Mat4::look_at_rh(eye, Vec3::new(0.0, 1.0, 0.0), Vec3::Y);
    let proj = Mat4::perspective_rh(60f32.to_radians(), aspect, 0.1, 200.0);
    (view, proj)
}

#[repr(C)]
#[derive(Clone, Copy, Pod, Zeroable)]
struct ViewUniform {
    view_proj: [[f32; 4]; 4],
    inv_view_proj: [[f32; 4]; 4],
    eye: [f32; 4],
}

#[repr(C)]
#[derive(Clone, Copy, Pod, Zeroable)]
struct Vertex {
    position: [f32; 3],
    normal: [f32; 3],
}

#[repr(C)]
#[derive(Clone, Copy, Pod, Zeroable)]
struct Instance {
    model: [[f32; 4]; 4],
    color: [f32; 4],
}

const CUBES: [([f32; 3], [f32; 3], f32); 5] = [
    ([0.0, 1.2, 0.0], [0.90, 0.30, 0.25], 1.2),
    ([3.5, 0.8, -1.0], [0.25, 0.70, 0.35], 0.8),
    ([-3.0, 0.9, 1.5], [0.25, 0.45, 0.90], 0.9),
    ([1.5, 0.6, 3.0], [0.95, 0.80, 0.25], 0.6),
    ([-1.8, 0.7, -3.2], [0.75, 0.35, 0.85], 0.7),
];

fn cube_mesh() -> (Vec<Vertex>, Vec<u16>) {
    let mut vertices = Vec::new();
    let mut indices = Vec::new();
    for axis in 0..3 {
        for sign in [-1.0f32, 1.0] {
            let mut normal = [0.0; 3];
            normal[axis] = sign;
            let (u, v) = ((axis + 1) % 3, (axis + 2) % 3);
            let base = vertices.len() as u16;
            for (a, b) in [(-1.0, -1.0), (1.0, -1.0), (1.0, 1.0), (-1.0, 1.0)] {
                let mut position = [0.0; 3];
                position[axis] = sign * 0.5;
                position[u] = a * 0.5;
                position[v] = b * 0.5;
                vertices.push(Vertex { position, normal });
            }
            // u x v points along +axis, so this order faces outwards for either sign.
            let quad = if sign > 0.0 {
                [0, 1, 2, 0, 2, 3]
            } else {
                [0, 2, 1, 0, 3, 2]
            };
            indices.extend(quad.map(|i| base + i));
        }
    }
    (vertices, indices)
}

/// Draws the test scene into any number of views.
pub struct SceneRenderer {
    device: wgpu::Device,
    queue: wgpu::Queue,
    format: wgpu::TextureFormat,
    layout: wgpu::BindGroupLayout,
    sky: wgpu::RenderPipeline,
    floor: wgpu::RenderPipeline,
    mesh: wgpu::RenderPipeline,
    vertices: wgpu::Buffer,
    indices: wgpu::Buffer,
    index_count: u32,
    instances: wgpu::Buffer,
    stride: u64,
    uniforms: wgpu::Buffer,
    bind_group: wgpu::BindGroup,
    capacity: usize,
    depth: HashMap<(u32, u32), wgpu::TextureView>,
}

impl SceneRenderer {
    /// A renderer for targets of `format` (an sRGB format shows the lighting as intended).
    pub fn new(device: &wgpu::Device, queue: &wgpu::Queue, format: wgpu::TextureFormat) -> Self {
        let shader = device.create_shader_module(wgpu::include_wgsl!("scene.wgsl"));
        let layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("scene view"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::VERTEX_FRAGMENT,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: true,
                    min_binding_size: wgpu::BufferSize::new(size_of::<ViewUniform>() as u64),
                },
                count: None,
            }],
        });
        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("scene"),
            bind_group_layouts: &[Some(&layout)],
            immediate_size: 0,
        });
        let pipeline = |name: &str,
                        buffers: &[Option<wgpu::VertexBufferLayout>],
                        cull_mode,
                        depth_write: bool,
                        depth_compare| {
            device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
                label: Some(name),
                layout: Some(&pipeline_layout),
                vertex: wgpu::VertexState {
                    module: &shader,
                    entry_point: Some(&format!("vs_{name}")),
                    compilation_options: Default::default(),
                    buffers,
                },
                primitive: wgpu::PrimitiveState {
                    cull_mode,
                    ..Default::default()
                },
                depth_stencil: Some(wgpu::DepthStencilState {
                    format: DEPTH_FORMAT,
                    depth_write_enabled: Some(depth_write),
                    depth_compare: Some(depth_compare),
                    stencil: Default::default(),
                    bias: Default::default(),
                }),
                multisample: Default::default(),
                fragment: Some(wgpu::FragmentState {
                    module: &shader,
                    entry_point: Some(&format!("fs_{name}")),
                    compilation_options: Default::default(),
                    targets: &[Some(format.into())],
                }),
                multiview_mask: None,
                cache: None,
            })
        };
        let mesh_buffers = [
            Some(wgpu::VertexBufferLayout {
                array_stride: size_of::<Vertex>() as u64,
                step_mode: wgpu::VertexStepMode::Vertex,
                attributes: &wgpu::vertex_attr_array![0 => Float32x3, 1 => Float32x3],
            }),
            Some(wgpu::VertexBufferLayout {
                array_stride: size_of::<Instance>() as u64,
                step_mode: wgpu::VertexStepMode::Instance,
                attributes: &wgpu::vertex_attr_array![
                    2 => Float32x4, 3 => Float32x4, 4 => Float32x4, 5 => Float32x4, 6 => Float32x4
                ],
            }),
        ];
        use wgpu::CompareFunction::{Always, Less};
        let sky = pipeline("sky", &[], None, false, Always);
        let floor = pipeline("floor", &[], None, true, Less);
        let mesh = pipeline("mesh", &mesh_buffers, Some(wgpu::Face::Back), true, Less);

        let (cube_vertices, cube_indices) = cube_mesh();
        let vertices = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("cube vertices"),
            contents: bytemuck::cast_slice(&cube_vertices),
            usage: wgpu::BufferUsages::VERTEX,
        });
        let indices = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("cube indices"),
            contents: bytemuck::cast_slice(&cube_indices),
            usage: wgpu::BufferUsages::INDEX,
        });
        let instances = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("cube instances"),
            size: (size_of::<Instance>() * CUBES.len()) as u64,
            usage: wgpu::BufferUsages::VERTEX | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        let stride = (size_of::<ViewUniform>() as u64).next_multiple_of(u64::from(
            device.limits().min_uniform_buffer_offset_alignment,
        ));
        let (uniforms, bind_group) = Self::view_uniforms(device, &layout, stride, 2);
        Self {
            device: device.clone(),
            queue: queue.clone(),
            format,
            layout,
            sky,
            floor,
            mesh,
            vertices,
            indices,
            index_count: cube_indices.len() as u32,
            instances,
            stride,
            uniforms,
            bind_group,
            capacity: 2,
            depth: HashMap::new(),
        }
    }

    pub fn format(&self) -> wgpu::TextureFormat {
        self.format
    }

    fn view_uniforms(
        device: &wgpu::Device,
        layout: &wgpu::BindGroupLayout,
        stride: u64,
        capacity: usize,
    ) -> (wgpu::Buffer, wgpu::BindGroup) {
        let buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("scene views"),
            size: stride * capacity as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        let bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("scene views"),
            layout,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: wgpu::BindingResource::Buffer(wgpu::BufferBinding {
                    buffer: &buffer,
                    offset: 0,
                    size: wgpu::BufferSize::new(size_of::<ViewUniform>() as u64),
                }),
            }],
        });
        (buffer, bind_group)
    }

    fn depth_view(&mut self, width: u32, height: u32) -> wgpu::TextureView {
        let device = &self.device;
        self.depth
            .entry((width, height))
            .or_insert_with(|| {
                device
                    .create_texture(&wgpu::TextureDescriptor {
                        label: Some("scene depth"),
                        size: wgpu::Extent3d {
                            width,
                            height,
                            depth_or_array_layers: 1,
                        },
                        mip_level_count: 1,
                        sample_count: 1,
                        dimension: wgpu::TextureDimension::D2,
                        format: DEPTH_FORMAT,
                        usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
                        view_formats: &[],
                    })
                    .create_view(&Default::default())
            })
            .clone()
    }

    /// Records one pass per view into `encoder`, all showing the scene at `time`
    /// seconds. Each view draws only inside its viewport, so several views may
    /// share a target. The view and instance data are written through the queue:
    /// call this at most once per queue submission.
    pub fn render_views(
        &mut self,
        encoder: &mut wgpu::CommandEncoder,
        views: &[ViewTarget],
        time: f32,
    ) {
        if views.len() > self.capacity {
            self.capacity = views.len();
            (self.uniforms, self.bind_group) =
                Self::view_uniforms(&self.device, &self.layout, self.stride, self.capacity);
        }
        for (index, view) in views.iter().enumerate() {
            let view_proj = view.proj * view.view;
            let uniform = ViewUniform {
                view_proj: view_proj.to_cols_array_2d(),
                inv_view_proj: view_proj.inverse().to_cols_array_2d(),
                eye: view.view.inverse().w_axis.to_array(),
            };
            self.queue.write_buffer(
                &self.uniforms,
                index as u64 * self.stride,
                bytemuck::bytes_of(&uniform),
            );
        }
        let instances = CUBES.map(|(position, color, size)| {
            let spin = time * (0.4 + size * 0.5);
            let rotation = Quat::from_rotation_y(spin) * Quat::from_rotation_x(spin * 0.7);
            let bob = (time * 1.3 + position[0]).sin() * 0.15;
            let model = Mat4::from_scale_rotation_translation(
                Vec3::splat(size),
                rotation,
                Vec3::from(position) + Vec3::Y * bob,
            );
            Instance {
                model: model.to_cols_array_2d(),
                color: [color[0], color[1], color[2], 1.0],
            }
        });
        self.queue
            .write_buffer(&self.instances, 0, bytemuck::cast_slice(&instances));

        for (index, view) in views.iter().enumerate() {
            let size = view.target.texture().size();
            let depth = self.depth_view(size.width, size.height);
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("scene"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: view.target,
                    depth_slice: None,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Load,
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view: &depth,
                    depth_ops: Some(wgpu::Operations {
                        load: wgpu::LoadOp::Clear(1.0),
                        store: wgpu::StoreOp::Discard,
                    }),
                    stencil_ops: None,
                }),
                ..Default::default()
            });
            let Viewport {
                x,
                y,
                width,
                height,
            } = view.viewport;
            pass.set_viewport(x, y, width, height, 0.0, 1.0);
            let left = (x.max(0.0) as u32).min(size.width);
            let top = (y.max(0.0) as u32).min(size.height);
            let right = ((x + width).ceil().max(0.0) as u32).min(size.width);
            let bottom = ((y + height).ceil().max(0.0) as u32).min(size.height);
            pass.set_scissor_rect(left, top, right - left, bottom - top);
            pass.set_bind_group(0, &self.bind_group, &[(index as u64 * self.stride) as u32]);
            pass.set_pipeline(&self.sky);
            pass.draw(0..3, 0..1);
            pass.set_pipeline(&self.floor);
            pass.draw(0..6, 0..1);
            pass.set_pipeline(&self.mesh);
            pass.set_vertex_buffer(0, self.vertices.slice(..));
            pass.set_vertex_buffer(1, self.instances.slice(..));
            pass.set_index_buffer(self.indices.slice(..), wgpu::IndexFormat::Uint16);
            pass.draw_indexed(0..self.index_count, 0, 0..CUBES.len() as u32);
        }
    }
}
