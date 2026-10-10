use bytemuck::{Pod, Zeroable};
use glam::{Mat4, Vec2, Vec4};
use xem_settings::Scale;

/// A rectangle in target pixels, y down.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Rect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

impl Rect {
    pub fn contains(&self, x: f32, y: f32) -> bool {
        x >= self.x && y >= self.y && x < self.x + self.width && y < self.y + self.height
    }
}

/// Where an `image`-sized game picture goes inside `area` under `scale`,
/// centred. Sizes are in pixels.
pub fn present_rect(image: (u32, u32), area: (u32, u32), scale: Scale) -> Rect {
    let (iw, ih) = (image.0 as f32, image.1 as f32);
    let (aw, ah) = (area.0 as f32, area.1 as f32);
    let (width, height) = match scale {
        Scale::Stretch => (aw, ah),
        Scale::Fit => {
            let factor = (aw / iw).min(ah / ih);
            (iw * factor, ih * factor)
        }
        Scale::Integer => {
            let factor = (aw / iw).min(ah / ih).floor().max(1.0);
            (iw * factor, ih * factor)
        }
    };
    Rect {
        x: ((aw - width) / 2.0).round(),
        y: ((ah - height) / 2.0).round(),
        width,
        height,
    }
}

/// A texture drawn as a quad.
pub struct Quad<'a> {
    pub texture: &'a wgpu::TextureView,
    /// Maps the unit square (u right, v down, texture coordinates) to clip space.
    pub transform: Mat4,
    pub filter: wgpu::FilterMode,
    /// Multiplies the (premultiplied) texture colour and alpha.
    pub opacity: f32,
    /// The texture has a linear format but holds sRGB-encoded values, as Slint's
    /// panel texture does.
    pub srgb_encoded: bool,
}

impl<'a> Quad<'a> {
    /// A quad covering `rect` of a target of `target` pixels.
    pub fn screen(texture: &'a wgpu::TextureView, rect: Rect, target: (u32, u32)) -> Self {
        let (tw, th) = (target.0 as f32, target.1 as f32);
        let transform = Mat4::from_cols(
            Vec4::new(2.0 * rect.width / tw, 0.0, 0.0, 0.0),
            Vec4::new(0.0, -2.0 * rect.height / th, 0.0, 0.0),
            Vec4::Z,
            Vec4::new(2.0 * rect.x / tw - 1.0, 1.0 - 2.0 * rect.y / th, 0.0, 1.0),
        );
        Self::new(texture, transform)
    }

    /// A `size`-metre panel centred on the origin of `pose` (x right, y up,
    /// facing +z), seen through `view_proj`.
    pub fn world(texture: &'a wgpu::TextureView, view_proj: Mat4, pose: Mat4, size: Vec2) -> Self {
        let unit = Mat4::from_cols(
            Vec4::new(size.x, 0.0, 0.0, 0.0),
            Vec4::new(0.0, -size.y, 0.0, 0.0),
            Vec4::Z,
            Vec4::new(-size.x / 2.0, size.y / 2.0, 0.0, 1.0),
        );
        Self::new(texture, view_proj * pose * unit)
    }

    fn new(texture: &'a wgpu::TextureView, transform: Mat4) -> Self {
        Self {
            texture,
            transform,
            filter: wgpu::FilterMode::Linear,
            opacity: 1.0,
            srgb_encoded: false,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Pod, Zeroable)]
struct QuadUniform {
    transform: [[f32; 4]; 4],
    opacity: f32,
    decode: u32,
    encode: u32,
    _pad: u32,
}

/// Draws textures (the game image, application panels) onto targets of one format.
pub struct Compositor {
    device: wgpu::Device,
    queue: wgpu::Queue,
    format: wgpu::TextureFormat,
    pipeline: wgpu::RenderPipeline,
    uniform_layout: wgpu::BindGroupLayout,
    texture_layout: wgpu::BindGroupLayout,
    nearest: wgpu::Sampler,
    linear: wgpu::Sampler,
    stride: u64,
    capacity: usize,
    uniforms: wgpu::Buffer,
    uniform_group: wgpu::BindGroup,
}

impl Compositor {
    pub fn new(device: &wgpu::Device, queue: &wgpu::Queue, format: wgpu::TextureFormat) -> Self {
        let shader = device.create_shader_module(wgpu::include_wgsl!("composite.wgsl"));
        let uniform_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("quad"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::VERTEX_FRAGMENT,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: true,
                    min_binding_size: wgpu::BufferSize::new(size_of::<QuadUniform>() as u64),
                },
                count: None,
            }],
        });
        let texture_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("quad texture"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                    count: None,
                },
            ],
        });
        let layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("compositor"),
            bind_group_layouts: &[Some(&uniform_layout), Some(&texture_layout)],
            immediate_size: 0,
        });
        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("compositor"),
            layout: Some(&layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("vs_quad"),
                compilation_options: Default::default(),
                buffers: &[],
            },
            primitive: Default::default(),
            depth_stencil: None,
            multisample: Default::default(),
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: Some("fs_quad"),
                compilation_options: Default::default(),
                targets: &[Some(wgpu::ColorTargetState {
                    format,
                    blend: Some(wgpu::BlendState::PREMULTIPLIED_ALPHA_BLENDING),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview_mask: None,
            cache: None,
        });
        let sampler = |filter| {
            device.create_sampler(&wgpu::SamplerDescriptor {
                mag_filter: filter,
                min_filter: filter,
                ..Default::default()
            })
        };
        let stride = (size_of::<QuadUniform>() as u64).next_multiple_of(u64::from(
            device.limits().min_uniform_buffer_offset_alignment,
        ));
        let (uniforms, uniform_group) = Self::quad_uniforms(device, &uniform_layout, stride, 4);
        Self {
            device: device.clone(),
            queue: queue.clone(),
            format,
            pipeline,
            uniform_layout,
            texture_layout,
            nearest: sampler(wgpu::FilterMode::Nearest),
            linear: sampler(wgpu::FilterMode::Linear),
            stride,
            capacity: 4,
            uniforms,
            uniform_group,
        }
    }

    pub fn format(&self) -> wgpu::TextureFormat {
        self.format
    }

    fn quad_uniforms(
        device: &wgpu::Device,
        layout: &wgpu::BindGroupLayout,
        stride: u64,
        capacity: usize,
    ) -> (wgpu::Buffer, wgpu::BindGroup) {
        let buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("quads"),
            size: stride * capacity as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        let group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("quads"),
            layout,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: wgpu::BindingResource::Buffer(wgpu::BufferBinding {
                    buffer: &buffer,
                    offset: 0,
                    size: wgpu::BufferSize::new(size_of::<QuadUniform>() as u64),
                }),
            }],
        });
        (buffer, group)
    }

    /// Records one pass drawing `quads` in order onto `target`, cleared to
    /// `clear` first when given. Quad data is written through the queue: call
    /// this at most once per queue submission.
    pub fn draw(
        &mut self,
        encoder: &mut wgpu::CommandEncoder,
        target: &wgpu::TextureView,
        clear: Option<wgpu::Color>,
        quads: &[Quad],
    ) {
        if quads.len() > self.capacity {
            self.capacity = quads.len();
            (self.uniforms, self.uniform_group) = Self::quad_uniforms(
                &self.device,
                &self.uniform_layout,
                self.stride,
                self.capacity,
            );
        }
        let encode = !self.format.is_srgb();
        let groups: Vec<_> = quads
            .iter()
            .enumerate()
            .map(|(index, quad)| {
                let uniform = QuadUniform {
                    transform: quad.transform.to_cols_array_2d(),
                    opacity: quad.opacity,
                    decode: quad.srgb_encoded.into(),
                    encode: encode.into(),
                    _pad: 0,
                };
                self.queue.write_buffer(
                    &self.uniforms,
                    index as u64 * self.stride,
                    bytemuck::bytes_of(&uniform),
                );
                let sampler = match quad.filter {
                    wgpu::FilterMode::Nearest => &self.nearest,
                    wgpu::FilterMode::Linear => &self.linear,
                };
                self.device.create_bind_group(&wgpu::BindGroupDescriptor {
                    label: Some("quad texture"),
                    layout: &self.texture_layout,
                    entries: &[
                        wgpu::BindGroupEntry {
                            binding: 0,
                            resource: wgpu::BindingResource::TextureView(quad.texture),
                        },
                        wgpu::BindGroupEntry {
                            binding: 1,
                            resource: wgpu::BindingResource::Sampler(sampler),
                        },
                    ],
                })
            })
            .collect();
        let load = clear.map_or(wgpu::LoadOp::Load, wgpu::LoadOp::Clear);
        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("compositor"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: target,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations {
                    load,
                    store: wgpu::StoreOp::Store,
                },
            })],
            ..Default::default()
        });
        pass.set_pipeline(&self.pipeline);
        for (index, group) in groups.iter().enumerate() {
            pass.set_bind_group(
                0,
                &self.uniform_group,
                &[(index as u64 * self.stride) as u32],
            );
            pass.set_bind_group(1, group, &[]);
            pass.draw(0..6, 0..1);
        }
    }
}
