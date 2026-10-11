//! Swapchain readback for headset captures (from xem-xr's demo).

use std::path::{Path, PathBuf};
use xem_render::wgpu;

/// A copy of a swapchain image's layers into a mappable buffer, recorded
/// before the image is released.
pub struct Readback {
    buffer: wgpu::Buffer,
    size: (u32, u32),
    layers: u32,
    padded: u32,
    bgra: bool,
}

impl Readback {
    pub fn record(
        device: &wgpu::Device,
        encoder: &mut wgpu::CommandEncoder,
        texture: &wgpu::Texture,
    ) -> Self {
        let extent = texture.size();
        let padded = (extent.width * 4).next_multiple_of(wgpu::COPY_BYTES_PER_ROW_ALIGNMENT);
        let buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("xr readback"),
            size: u64::from(padded * extent.height * extent.depth_or_array_layers),
            usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ,
            mapped_at_creation: false,
        });
        encoder.copy_texture_to_buffer(
            texture.as_image_copy(),
            wgpu::TexelCopyBufferInfo {
                buffer: &buffer,
                layout: wgpu::TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(padded),
                    rows_per_image: Some(extent.height),
                },
            },
            extent,
        );
        Self {
            buffer,
            size: (extent.width, extent.height),
            layers: extent.depth_or_array_layers,
            padded,
            bgra: matches!(
                texture.format(),
                wgpu::TextureFormat::Bgra8Unorm | wgpu::TextureFormat::Bgra8UnormSrgb
            ),
        }
    }

    /// Waits for the copy and writes one PNG per layer (the bytes as stored:
    /// sRGB-encoded for the sRGB swapchains).
    pub fn save(self, device: &wgpu::Device, paths: &[PathBuf]) -> Result<(), String> {
        assert_eq!(paths.len(), self.layers as usize);
        self.buffer.map_async(wgpu::MapMode::Read, .., |result| {
            result.expect("map readback")
        });
        device
            .poll(wgpu::PollType::wait_indefinitely())
            .map_err(|e| e.to_string())?;
        let mapped = self
            .buffer
            .get_mapped_range(..)
            .map_err(|e| e.to_string())?;
        let (width, height) = self.size;
        let layer_bytes = (self.padded * height) as usize;
        for (layer, path) in paths.iter().enumerate() {
            let mut pixels = Vec::with_capacity((width * height * 4) as usize);
            let data = &mapped[layer * layer_bytes..(layer + 1) * layer_bytes];
            for row in data.chunks(self.padded as usize) {
                pixels.extend_from_slice(&row[..(width * 4) as usize]);
            }
            if self.bgra {
                pixels.chunks_exact_mut(4).for_each(|p| p.swap(0, 2));
            }
            write_png(path, width, height, &pixels)?;
        }
        Ok(())
    }
}

fn write_png(path: &Path, width: u32, height: u32, rgba: &[u8]) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    }
    let file = std::fs::File::create(path).map_err(|e| format!("{}: {e}", path.display()))?;
    let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), width, height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    encoder
        .write_header()
        .and_then(|mut writer| writer.write_image_data(rgba))
        .map_err(|e| format!("{}: {e}", path.display()))
}
