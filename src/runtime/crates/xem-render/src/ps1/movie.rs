//! Movie frames rebuilt from what the MDEC decoded, for the Enhanced movies
//! setting. The macroblocks the game loads into VRAM ([`TraceOp::Movie`]:
//! luminance and half-resolution chrominance as the IDCT left them, with
//! each block's quantization scale) are put together for the 24-bit display
//! that shows them. On the CPU ([`planes`]) the seams between 8x8 blocks are
//! smoothed where they are no larger than the compression's error at that
//! block's scale (real edges are larger and stay). On the GPU
//! (`movie.wgsl`) the chrominance is brought to full resolution following
//! the luminance's edges instead of repeated over 2x2 pixels, converted to
//! RGB with the MDEC's own coefficients, and the picture enlarged to the
//! output edge-adaptively and sharpened (AMD FidelityFX Super Resolution 1).
//! A movie that ends on the picture the game goes on from (the title movie:
//! [`TraceOp::MovieFramesLeft`]) eases into the MDEC's own picture over its
//! last second, its last frame the MDEC's.

use std::collections::HashMap;
use std::sync::Arc;
use xem_gpu::{DisplayInfo, MovieBlock, Primitive, TraceOp, VramRect};

/// The movie macroblocks in VRAM, by their position (VRAM pixels), each as
/// the game loaded it from the MDEC's output until VRAM there changes, and
/// the frames the movie has left after the game frame last followed.
#[derive(Default)]
pub struct MovieTiles {
    tiles: HashMap<(u16, u16), Arc<MovieBlock>>,
    left: Option<u16>,
}

/// The movie frames over which the rebuilt picture eases into the MDEC's own
/// before a movie's last frame: a second at the movies' 15 a second.
const EASE_FRAMES: u16 = 15;

/// A macroblock's VRAM pixels: 24 wide at 24 bits (16 pixels), else 16.
fn tile_rect((x, y): (u16, u16), block: &MovieBlock) -> VramRect {
    VramRect { x, y, width: if block.bits24 { 24 } else { 16 }, height: 16 }
}

fn overlaps(a: &VramRect, b: &VramRect) -> bool {
    let (ax, ay, bx, by) = (u32::from(a.x), u32::from(a.y), u32::from(b.x), u32::from(b.y));
    ax < bx + u32::from(b.width) && bx < ax + u32::from(a.width) && ay < by + u32::from(b.height) && by < ay + u32::from(a.height)
}

/// The VRAM a drawn primitive may change: its points' bounds within its
/// drawing area.
fn drawn_rect(p: &Primitive) -> VramRect {
    let v = &p.vertices[..p.vertex_count()];
    let [left, top, right, bottom] = p.environment.area.map(i32::from);
    let x0 = v.iter().map(|v| v.x).min().unwrap_or(0).max(left);
    let x1 = v.iter().map(|v| v.x).max().unwrap_or(0).min(right);
    let y0 = v.iter().map(|v| v.y).min().unwrap_or(0).max(top);
    let y1 = v.iter().map(|v| v.y).max().unwrap_or(0).min(bottom);
    if x1 < x0 || y1 < y0 {
        return VramRect { x: 0, y: 0, width: 0, height: 0 };
    }
    let clamp = |v: i32| v.clamp(0, 1023) as u16;
    VramRect { x: clamp(x0), y: clamp(y0), width: (x1 - x0 + 1).min(1024) as u16, height: (y1 - y0 + 1).min(512) as u16 }
}

impl MovieTiles {
    /// Follows a trace: macroblocks loaded, and VRAM changed under others.
    pub fn observe(&mut self, ops: &[TraceOp]) {
        for op in ops {
            match op {
                TraceOp::Movie { x, y, block } => {
                    self.tiles.insert((*x, *y), block.clone());
                }
                // A game frame's count follows its vertical blank.
                TraceOp::VBlank => self.left = None,
                TraceOp::MovieFramesLeft(frames) => self.left = Some(*frames),
                _ if self.tiles.is_empty() => {}
                TraceOp::Write { rect, .. } | TraceOp::Fill { rect, .. } => self.forget(*rect),
                TraceOp::Copy { destination, width, height, .. } => {
                    self.forget(VramRect { x: destination.0, y: destination.1, width: *width, height: *height });
                }
                TraceOp::Draw(p) if !p.revealed => self.forget(drawn_rect(p)),
                TraceOp::Draw(_) | TraceOp::Eye(_) | TraceOp::Camera(_) | TraceOp::Placement(_) => {}
            }
        }
    }

    /// How much of a frame is the rebuilt picture: all of it, easing to none
    /// (the MDEC's own picture) at a movie's last frame.
    pub fn enhancement(&self) -> f32 {
        self.left.map_or(1.0, |left| (f32::from(left) / f32::from(EASE_FRAMES)).min(1.0))
    }

    fn forget(&mut self, rect: VramRect) {
        self.tiles.retain(|&at, block| !overlaps(&tile_rect(at, block), &rect));
    }

    /// The macroblocks a 24-bit display shows, row by row (`columns` x `rows`),
    /// when every one of its area is a macroblock the game loaded.
    pub fn frame(&self, display: &DisplayInfo) -> Option<(Vec<Arc<MovieBlock>>, usize, usize)> {
        if !display.enabled || !display.color24 || display.width == 0 || display.height == 0 {
            return None;
        }
        let (columns, rows) = (display.width.div_ceil(16) as usize, display.height.div_ceil(16) as usize);
        let mut blocks = Vec::with_capacity(columns * rows);
        for row in 0..rows {
            for column in 0..columns {
                let at = (u32::from(display.x) + column as u32 * 24, u32::from(display.y) + row as u32 * 16);
                let at = (u16::try_from(at.0).ok()?, u16::try_from(at.1).ok()?);
                blocks.push(self.tiles.get(&at).filter(|block| block.bits24)?.clone());
            }
        }
        Some((blocks, columns, rows))
    }
}

/// How far a seam's step may be and still be the compression's (`ALPHA`)
/// and how flat each side must be (`BETA`), and the most a gentle filter
/// moves a sample (`CLIP`): a base and a part per unit of the blocks'
/// quantization scale, in luminance levels (0-255).
const ALPHA: (f32, f32) = (3.0, 2.5);
const BETA: (f32, f32) = (1.0, 0.5);
const CLIP: (f32, f32) = (1.0, 0.5);

/// One line of samples across a seam (`at` the first after it, `step` from
/// one to the next): when the step at the seam is the compression's and
/// both sides are flat, the seam is smoothed: over three samples a side
/// where they are very flat (luminance), else by a clipped correction of the
/// two nearest (H.264's in-loop filter's rules, in float).
fn smooth_seam(plane: &mut [f32], at: usize, step: usize, scale: f32, luma: bool) {
    if scale == 0.0 {
        // Scale 0 stores the coefficients as they are: no quantization error.
        return;
    }
    let s = |k: isize| plane[at.wrapping_add_signed(k * step as isize)];
    let (p3, p2, p1, p0, q0, q1, q2, q3) = (s(-4), s(-3), s(-2), s(-1), s(0), s(1), s(2), s(3));
    let alpha = ALPHA.0 + ALPHA.1 * scale;
    let beta = BETA.0 + BETA.1 * scale;
    let clip = CLIP.0 + CLIP.1 * scale;
    if (p0 - q0).abs() >= alpha || (p1 - p0).abs() >= beta || (q1 - q0).abs() >= beta {
        return;
    }
    let mut set = |k: isize, value: f32| plane[at.wrapping_add_signed(k * step as isize)] = value;
    let flat_p = (p2 - p0).abs() < beta;
    let flat_q = (q2 - q0).abs() < beta;
    if luma && flat_p && flat_q && (p0 - q0).abs() < alpha / 4.0 + 2.0 {
        set(-1, (p2 + 2.0 * p1 + 2.0 * p0 + 2.0 * q0 + q1) / 8.0);
        set(-2, (p2 + p1 + p0 + q0) / 4.0);
        set(-3, (2.0 * p3 + 3.0 * p2 + p1 + p0 + q0) / 8.0);
        set(0, (p1 + 2.0 * p0 + 2.0 * q0 + 2.0 * q1 + q2) / 8.0);
        set(1, (p0 + q0 + q1 + q2) / 4.0);
        set(2, (2.0 * q3 + 3.0 * q2 + q1 + q0 + p0) / 8.0);
        return;
    }
    let delta = ((4.0 * (q0 - p0) + (p1 - q1)) / 8.0).clamp(-clip, clip);
    set(-1, p0 + delta);
    set(0, q0 - delta);
    if luma && flat_p {
        set(-2, p1 + ((p2 + (p0 + q0) / 2.0 - 2.0 * p1) / 2.0).clamp(-clip, clip));
    }
    if luma && flat_q {
        set(1, q1 + ((q2 + (p0 + q0) / 2.0 - 2.0 * q1) / 2.0).clamp(-clip, clip));
    }
}

/// A plane's 8x8 block seams smoothed (columns' first, then rows'), each by
/// the larger quantization scale of the two blocks it divides (`scales`,
/// one per block, row by row).
fn deblock(plane: &mut [f32], width: usize, height: usize, scales: &[f32], luma: bool) {
    let blocks = width / 8;
    for x in (8..width).step_by(8) {
        for y in 0..height {
            let scale = scales[y / 8 * blocks + x / 8 - 1].max(scales[y / 8 * blocks + x / 8]);
            smooth_seam(plane, y * width + x, 1, scale, luma);
        }
    }
    for y in (8..height).step_by(8) {
        for x in 0..width {
            let scale = scales[(y / 8 - 1) * blocks + x / 8].max(scales[y / 8 * blocks + x / 8]);
            smooth_seam(plane, y * width + x, width, scale, luma);
        }
    }
}

/// A frame's planes, deblocked: the luminance (`width` x `height`, the
/// macroblocks' 16x16 each) and, per chrominance sample (half each way),
/// Cb, Cr and the mean luminance of the four pixels it covers.
pub struct Planes {
    pub width: usize,
    pub height: usize,
    pub luma: Vec<f32>,
    pub chroma: Vec<[f32; 4]>,
}

/// The planes `blocks` make (`columns` x `rows` macroblocks, row by row).
pub fn planes(blocks: &[Arc<MovieBlock>], columns: usize, rows: usize) -> Planes {
    let (w, h) = (columns * 16, rows * 16);
    let (cw, ch) = (columns * 8, rows * 8);
    let mut luma = vec![0f32; w * h];
    let mut cb = vec![0f32; cw * ch];
    let mut cr = vec![0f32; cw * ch];
    let mut luma_scales = vec![0f32; (w / 8) * (h / 8)];
    let mut chroma_scales = vec![0f32; columns * rows];
    for (index, block) in blocks.iter().enumerate().take(columns * rows) {
        let (mx, my) = (index % columns, index / columns);
        for y in 0..16 {
            for x in 0..16 {
                luma[(my * 16 + y) * w + mx * 16 + x] = f32::from(block.luma[y * 16 + x]);
            }
        }
        for y in 0..8 {
            for x in 0..8 {
                cb[(my * 8 + y) * cw + mx * 8 + x] = f32::from(block.cb[y * 8 + x]);
                cr[(my * 8 + y) * cw + mx * 8 + x] = f32::from(block.cr[y * 8 + x]);
            }
        }
        for quarter in 0..4 {
            luma_scales[(my * 2 + quarter / 2) * (w / 8) + mx * 2 + quarter % 2] = f32::from(block.quant[2 + quarter]);
        }
        chroma_scales[index] = f32::from(block.quant[0].max(block.quant[1]));
    }
    deblock(&mut luma, w, h, &luma_scales, true);
    deblock(&mut cb, cw, ch, &chroma_scales, false);
    deblock(&mut cr, cw, ch, &chroma_scales, false);
    let chroma = (0..cw * ch)
        .map(|i| {
            let (x, y) = (i % cw * 2, i / cw * 2);
            let under = 0.25 * (luma[y * w + x] + luma[y * w + x + 1] + luma[(y + 1) * w + x] + luma[(y + 1) * w + x + 1]);
            [cb[i], cr[i], under, 0.0]
        })
        .collect();
    Planes { width: w, height: h, luma, chroma }
}

/// The size `source` takes fitted into `output`, its shape kept.
pub fn fitted(source: (u32, u32), output: (u32, u32)) -> (u32, u32) {
    let scale = (output.0 as f32 / source.0.max(1) as f32).min(output.1 as f32 / source.1.max(1) as f32);
    (((source.0 as f32 * scale).round() as u32).max(1), ((source.1 as f32 * scale).round() as u32).max(1))
}

/// RCAS's sharpness: 2^-0.25 (0 stops the sharpest).
const SHARPNESS: f32 = 0.840_896_4;

#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct Params {
    size: [f32; 4],
    display: [f32; 4],
    sharpness: f32,
    enhancement: f32,
    pad: [f32; 2],
}

/// The passes' textures, for a frame's planes and an output size.
struct Targets {
    planes: (u32, u32),
    source_size: (u32, u32),
    size: (u32, u32),
    luma: wgpu::Texture,
    chroma: wgpu::Texture,
    compose: wgpu::BindGroup,
    source_view: wgpu::TextureView,
    easu: wgpu::BindGroup,
    upscaled_view: wgpu::TextureView,
    output: wgpu::Texture,
    output_view: wgpu::TextureView,
}

struct Passes {
    layout: wgpu::BindGroupLayout,
    planes_layout: wgpu::BindGroupLayout,
    rcas_layout: wgpu::BindGroupLayout,
    compose: wgpu::RenderPipeline,
    easu: wgpu::RenderPipeline,
    rcas: wgpu::RenderPipeline,
    linear: wgpu::RenderPipeline,
    params: wgpu::Buffer,
    targets: Option<Targets>,
}

const FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8Unorm;

impl Passes {
    fn new(device: &wgpu::Device) -> Self {
        let shader = device.create_shader_module(wgpu::include_wgsl!("movie.wgsl"));
        let entry = |binding: u32| match binding {
            0 => wgpu::BindGroupLayoutEntry {
                binding,
                visibility: wgpu::ShaderStages::FRAGMENT,
                ty: wgpu::BindingType::Buffer { ty: wgpu::BufferBindingType::Uniform, has_dynamic_offset: false, min_binding_size: None },
                count: None,
            },
            _ => wgpu::BindGroupLayoutEntry {
                binding,
                visibility: wgpu::ShaderStages::FRAGMENT,
                ty: wgpu::BindingType::Texture {
                    sample_type: wgpu::TextureSampleType::Float { filterable: false },
                    view_dimension: wgpu::TextureViewDimension::D2,
                    multisampled: false,
                },
                count: None,
            },
        };
        let layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor { label: Some("movie"), entries: &[entry(0), entry(1)] });
        let planes_layout =
            device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor { label: Some("movie planes"), entries: &[entry(0), entry(1), entry(2)] });
        // RCAS's also takes the native VRAM, the MDEC's own pixels.
        let vram = wgpu::BindGroupLayoutEntry {
            binding: 3,
            visibility: wgpu::ShaderStages::FRAGMENT,
            ty: wgpu::BindingType::Texture {
                sample_type: wgpu::TextureSampleType::Uint,
                view_dimension: wgpu::TextureViewDimension::D2,
                multisampled: false,
            },
            count: None,
        };
        let rcas_layout =
            device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor { label: Some("movie output"), entries: &[entry(0), entry(1), vram] });
        let pipeline = |entry: &str, layout: &wgpu::BindGroupLayout| {
            let layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
                label: Some(entry),
                bind_group_layouts: &[Some(layout)],
                immediate_size: 0,
            });
            device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
                label: Some(entry),
                layout: Some(&layout),
                vertex: wgpu::VertexState { module: &shader, entry_point: Some("vs_full"), compilation_options: Default::default(), buffers: &[] },
                primitive: Default::default(),
                depth_stencil: None,
                multisample: Default::default(),
                fragment: Some(wgpu::FragmentState {
                    module: &shader,
                    entry_point: Some(entry),
                    compilation_options: Default::default(),
                    targets: &[Some(wgpu::ColorTargetState { format: FORMAT, blend: None, write_mask: wgpu::ColorWrites::ALL })],
                }),
                multiview_mask: None,
                cache: None,
            })
        };
        let params = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("movie params"),
            size: size_of::<Params>() as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        Self {
            compose: pipeline("fs_compose", &planes_layout),
            easu: pipeline("fs_easu", &layout),
            rcas: pipeline("fs_rcas", &rcas_layout),
            linear: pipeline("fs_linear", &rcas_layout),
            layout,
            planes_layout,
            rcas_layout,
            params,
            targets: None,
        }
    }

    /// The passes' textures for planes of `planes`, a picture of
    /// `source_size` and an output of `size`.
    fn ensure_targets(&mut self, device: &wgpu::Device, planes: (u32, u32), source_size: (u32, u32), size: (u32, u32)) {
        if self.targets.as_ref().is_some_and(|t| t.planes == planes && t.source_size == source_size && t.size == size) {
            return;
        }
        let texture = |label: &str, (width, height): (u32, u32), format: wgpu::TextureFormat, usage: wgpu::TextureUsages| {
            device.create_texture(&wgpu::TextureDescriptor {
                label: Some(label),
                size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format,
                usage,
                view_formats: &[],
            })
        };
        let upload = wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST;
        let pass = wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING;
        let luma = texture("movie luminance", planes, wgpu::TextureFormat::R32Float, upload);
        let chroma = texture("movie chrominance", (planes.0 / 2, planes.1 / 2), wgpu::TextureFormat::Rgba32Float, upload);
        let source = texture("movie frame", source_size, FORMAT, pass);
        let upscaled = texture("movie enlarged", size, FORMAT, pass);
        let output = texture("movie", size, FORMAT, pass | wgpu::TextureUsages::COPY_SRC);
        let group = |layout: &wgpu::BindGroupLayout, views: &[&wgpu::TextureView]| {
            let mut entries = vec![wgpu::BindGroupEntry { binding: 0, resource: self.params.as_entire_binding() }];
            entries.extend(views.iter().enumerate().map(|(i, view)| wgpu::BindGroupEntry {
                binding: i as u32 + 1,
                resource: wgpu::BindingResource::TextureView(view),
            }));
            device.create_bind_group(&wgpu::BindGroupDescriptor { label: Some("movie"), layout, entries: &entries })
        };
        let source_view = source.create_view(&Default::default());
        let upscaled_view = upscaled.create_view(&Default::default());
        self.targets = Some(Targets {
            planes,
            source_size,
            size,
            compose: group(&self.planes_layout, &[&luma.create_view(&Default::default()), &chroma.create_view(&Default::default())]),
            easu: group(&self.layout, &[&source_view]),
            luma,
            chroma,
            source_view,
            upscaled_view,
            output_view: output.create_view(&Default::default()),
            output,
        });
    }

    fn pass(encoder: &mut wgpu::CommandEncoder, pipeline: &wgpu::RenderPipeline, group: &wgpu::BindGroup, target: &wgpu::TextureView) {
        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("movie"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: target,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations { load: wgpu::LoadOp::Clear(wgpu::Color::BLACK), store: wgpu::StoreOp::Store },
            })],
            ..Default::default()
        });
        pass.set_pipeline(pipeline);
        pass.set_bind_group(0, group, &[]);
        pass.draw(0..3, 0..1);
    }
}

/// The Enhanced movie picture: [`MovieTiles`] followed, the frame a display
/// shows rebuilt and enlarged when it changes.
#[derive(Default)]
pub struct MovieRenderer {
    tiles: MovieTiles,
    /// Whether the GPU rasterizes in software (`wgpu::DeviceType::Cpu`:
    /// lavapipe, SwiftShader): FSR 1's passes would cost it some 50 ms a
    /// frame at 1080 lines, so the rebuilt frame is enlarged bilinearly.
    software: bool,
    passes: Option<Passes>,
    /// What the output holds: its blocks (by identity), area, size and
    /// enhancement (bits).
    shown: Option<(u64, (u16, u16, u32, u32), (u32, u32), u32)>,
}

impl MovieRenderer {
    /// A renderer for a GPU that rasterizes in software or not.
    pub fn new(software: bool) -> Self {
        Self { software, ..Self::default() }
    }

    /// Enlarges as for a GPU that rasterizes in software (bilinearly) or not
    /// (FSR 1) from the next frame on, whatever the GPU is: tests run FSR on
    /// a software rasterizer this way.
    pub fn set_software(&mut self, software: bool) {
        if software != self.software {
            self.software = software;
            self.shown = None;
        }
    }

    pub fn observe(&mut self, ops: &[TraceOp]) {
        self.tiles.observe(ops);
    }

    /// Whether `display` shows a movie frame it can rebuild (not one the
    /// MDEC's own picture shows whole: [`MovieTiles::enhancement`]).
    pub fn shows(&self, display: &DisplayInfo) -> bool {
        self.tiles.enhancement() > 0.0 && self.tiles.frame(display).is_some()
    }

    /// The movie frame `display` shows, rebuilt and enlarged to fit `output`
    /// (a view of an `Rgba8Unorm` texture of sRGB-encoded values, its size,
    /// and whether it is new), or `None` when the display shows no such
    /// frame. Where it eases into the MDEC's own picture, that comes from
    /// `vram`, the native VRAM (`R16Uint`).
    pub fn present(
        &mut self,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        encoder: &mut wgpu::CommandEncoder,
        display: &DisplayInfo,
        output: (u32, u32),
        vram: &wgpu::Texture,
    ) -> Option<(&wgpu::TextureView, (u32, u32), bool)> {
        let enhancement = self.tiles.enhancement();
        if enhancement == 0.0 {
            return None;
        }
        let (blocks, columns, rows) = self.tiles.frame(display)?;
        let source = (display.width, display.height);
        let size = fitted(source, output);
        let identity = blocks.iter().fold(0xCBF2_9CE4_8422_2325u64, |hash, block| {
            (hash ^ Arc::as_ptr(block) as usize as u64).wrapping_mul(0x100_0000_01B3)
        });
        let key = (identity, (display.x, display.y, display.width, display.height), size, enhancement.to_bits());
        let new = self.shown != Some(key);
        let passes = self.passes.get_or_insert_with(|| Passes::new(device));
        if new {
            let planes = planes(&blocks, columns, rows);
            let plane_size = (planes.width as u32, planes.height as u32);
            let params = Params {
                size: [source.0 as f32, source.1 as f32, size.0 as f32, size.1 as f32],
                display: [f32::from(display.x), f32::from(display.y), 0.0, 0.0],
                sharpness: SHARPNESS,
                enhancement,
                pad: [0.0; 2],
            };
            queue.write_buffer(&passes.params, 0, bytemuck::bytes_of(&params));
            passes.ensure_targets(device, plane_size, source, size);
            let targets = passes.targets.as_ref().expect("made above");
            let write = |texture: &wgpu::Texture, bytes: &[u8], (width, height): (u32, u32), texel: u32| {
                queue.write_texture(
                    wgpu::TexelCopyTextureInfo { texture, mip_level: 0, origin: wgpu::Origin3d::ZERO, aspect: wgpu::TextureAspect::All },
                    bytes,
                    wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(width * texel), rows_per_image: Some(height) },
                    wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
                );
            };
            write(&targets.luma, bytemuck::cast_slice(&planes.luma), plane_size, 4);
            write(&targets.chroma, bytemuck::cast_slice(&planes.chroma), (plane_size.0 / 2, plane_size.1 / 2), 16);
            Passes::pass(encoder, &passes.compose, &targets.compose, &targets.source_view);
            // The last pass enlarges (EASU's picture sharpened by RCAS, or the
            // composed frame bilinearly) and eases into the MDEC's picture.
            let (last, enlarged) = if self.software {
                (&passes.linear, &targets.source_view)
            } else {
                Passes::pass(encoder, &passes.easu, &targets.easu, &targets.upscaled_view);
                (&passes.rcas, &targets.upscaled_view)
            };
            let output = device.create_bind_group(&wgpu::BindGroupDescriptor {
                label: Some("movie output"),
                layout: &passes.rcas_layout,
                entries: &[
                    wgpu::BindGroupEntry { binding: 0, resource: passes.params.as_entire_binding() },
                    wgpu::BindGroupEntry { binding: 1, resource: wgpu::BindingResource::TextureView(enlarged) },
                    wgpu::BindGroupEntry { binding: 3, resource: wgpu::BindingResource::TextureView(&vram.create_view(&Default::default())) },
                ],
            });
            Passes::pass(encoder, last, &output, &targets.output_view);
            self.shown = Some(key);
        }
        let targets = passes.targets.as_ref()?;
        Some((&targets.output_view, size, new))
    }

    /// The texture the last [`MovieRenderer::present`] returned a view of.
    pub fn output(&self) -> Option<&wgpu::Texture> {
        self.passes.as_ref()?.targets.as_ref().map(|targets| &targets.output)
    }

    /// The view the last [`MovieRenderer::present`] returned.
    pub fn output_view(&self) -> Option<&wgpu::TextureView> {
        self.passes.as_ref()?.targets.as_ref().map(|targets| &targets.output_view)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn block(luma: i16, cb: i16, cr: i16) -> Arc<MovieBlock> {
        Arc::new(MovieBlock { luma: [luma; 256], cb: [cb; 64], cr: [cr; 64], quant: [4; 6], bits24: true })
    }

    /// Flat blocks make flat planes, the chrominance samples carrying the
    /// luminance they cover.
    #[test]
    fn planes_hold_the_blocks() {
        let planes = planes(&vec![block(10, -20, 30); 4], 2, 2);
        assert_eq!((planes.width, planes.height), (32, 32));
        assert!(planes.luma.iter().all(|&l| l == 10.0));
        assert!(planes.chroma.iter().all(|&c| c == [-20.0, 30.0, 10.0, 0.0]));
    }

    /// A small step between blocks (the compression's) is smoothed; a large
    /// one (an edge in the picture) stays sharp.
    #[test]
    fn seams_are_smoothed_only_where_compression_made_them() {
        for (step, smoothed) in [(4i16, true), (60, false)] {
            let planes = planes(&[block(0, 0, 0), block(step, 0, 0)], 2, 1);
            let jump = planes.luma[8 * 32 + 16] - planes.luma[8 * 32 + 15];
            if smoothed {
                assert!(jump < f32::from(step), "a step of {step} stays {jump} at the seam");
            } else {
                assert_eq!(jump, f32::from(step), "an edge of {step} must stay sharp");
            }
        }
    }

    #[test]
    fn tiles_follow_loads_and_writes() {
        let display = DisplayInfo {
            enabled: true,
            x: 0,
            y: 0,
            width: 32,
            height: 16,
            mode_width: 320,
            color24: true,
            interlaced: false,
            pal: false,
        };
        let mut tiles = MovieTiles::default();
        let movie = |x, y| TraceOp::Movie { x, y, block: block(0, 0, 0) };
        tiles.observe(&[movie(0, 0)]);
        assert!(tiles.frame(&display).is_none(), "one of two macroblocks");
        tiles.observe(&[movie(24, 0)]);
        assert!(tiles.frame(&display).is_some_and(|(blocks, columns, rows)| blocks.len() == 2 && (columns, rows) == (2, 1)));
        tiles.observe(&[TraceOp::Fill { rect: VramRect { x: 30, y: 4, width: 2, height: 2 }, color: 0 }]);
        assert!(tiles.frame(&display).is_none(), "a fill over the second");
    }

    /// A counted movie's last second eases the rebuilt picture out, to none
    /// at its last frame; a frame without a count shows all of it.
    #[test]
    fn the_rebuilt_picture_eases_out_over_a_counted_movies_last_frames() {
        let mut tiles = MovieTiles::default();
        assert_eq!(tiles.enhancement(), 1.0);
        for (left, enhancement) in [(40, 1.0), (15, 1.0), (6, 0.4), (0, 0.0)] {
            tiles.observe(&[TraceOp::VBlank, TraceOp::MovieFramesLeft(left)]);
            assert_eq!(tiles.enhancement(), enhancement, "{left} frames left");
        }
        tiles.observe(&[TraceOp::VBlank]);
        assert_eq!(tiles.enhancement(), 1.0);
    }

    #[test]
    fn fitted_keeps_the_shape() {
        assert_eq!(fitted((320, 216), (1920, 1080)), (1600, 1080));
        assert_eq!(fitted((320, 216), (3840, 1600)), (2370, 1600));
    }
}
