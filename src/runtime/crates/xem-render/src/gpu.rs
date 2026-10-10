use std::fmt;

/// The one wgpu instance, adapter, device and queue of a host. Hosts create it
/// once and share it with the scene renderer, the compositor and the Slint panel;
/// the wgpu handles are reference counted, so cloning them shares the device.
#[derive(Clone, Debug)]
pub struct Gpu {
    pub instance: wgpu::Instance,
    pub adapter: wgpu::Adapter,
    pub device: wgpu::Device,
    pub queue: wgpu::Queue,
}

#[derive(Debug)]
pub enum GpuError {
    Adapter(wgpu::RequestAdapterError),
    Device(wgpu::RequestDeviceError),
}

impl fmt::Display for GpuError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            GpuError::Adapter(error) => write!(f, "no suitable GPU adapter: {error}"),
            GpuError::Device(error) => write!(f, "GPU device request failed: {error}"),
        }
    }
}

impl std::error::Error for GpuError {}

impl Gpu {
    /// Picks an adapter of `instance` (able to present to `compatible_surface`
    /// when given) and opens its device with the adapter's limits.
    pub async fn new(
        instance: wgpu::Instance,
        compatible_surface: Option<&wgpu::Surface<'_>>,
    ) -> Result<Self, GpuError> {
        let adapter = instance
            .request_adapter(&wgpu::RequestAdapterOptions {
                power_preference: wgpu::PowerPreference::HighPerformance,
                compatible_surface,
                ..Default::default()
            })
            .await
            .map_err(GpuError::Adapter)?;
        let (device, queue) = adapter
            .request_device(&wgpu::DeviceDescriptor {
                label: Some("xem"),
                required_limits: adapter.limits(),
                ..Default::default()
            })
            .await
            .map_err(GpuError::Device)?;
        Ok(Self {
            instance,
            adapter,
            device,
            queue,
        })
    }

    /// A device without a surface, with backends and flags taken from the
    /// `WGPU_*` environment variables.
    pub async fn headless() -> Result<Self, GpuError> {
        let instance =
            wgpu::Instance::new(wgpu::InstanceDescriptor::new_without_display_handle_from_env());
        Self::new(instance, None).await
    }
}

/// Copies an 8-bit RGBA or BGRA `texture` (which needs `COPY_SRC`) into tightly
/// packed RGBA rows, blocking until the GPU has finished.
#[cfg(not(target_arch = "wasm32"))]
pub fn read_rgba8(device: &wgpu::Device, queue: &wgpu::Queue, texture: &wgpu::Texture) -> Vec<u8> {
    use wgpu::TextureFormat as F;
    let swap = match texture.format() {
        F::Rgba8Unorm | F::Rgba8UnormSrgb => false,
        F::Bgra8Unorm | F::Bgra8UnormSrgb => true,
        other => panic!("read_rgba8 does not read {other:?}"),
    };
    let wgpu::Extent3d { width, height, .. } = texture.size();
    let row = width * 4;
    let padded = row.next_multiple_of(wgpu::COPY_BYTES_PER_ROW_ALIGNMENT);
    let buffer = device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("readback"),
        size: u64::from(padded * height),
        usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ,
        mapped_at_creation: false,
    });
    let mut encoder = device.create_command_encoder(&Default::default());
    encoder.copy_texture_to_buffer(
        texture.as_image_copy(),
        wgpu::TexelCopyBufferInfo {
            buffer: &buffer,
            layout: wgpu::TexelCopyBufferLayout {
                offset: 0,
                bytes_per_row: Some(padded),
                rows_per_image: None,
            },
        },
        texture.size(),
    );
    queue.submit([encoder.finish()]);
    buffer.map_async(wgpu::MapMode::Read, .., |result| {
        result.expect("map readback buffer")
    });
    device
        .poll(wgpu::PollType::wait_indefinitely())
        .expect("wait for readback");
    let mapped = buffer.get_mapped_range(..).expect("readback range");
    let mut pixels = Vec::with_capacity((row * height) as usize);
    for line in mapped.chunks(padded as usize) {
        pixels.extend_from_slice(&line[..row as usize]);
    }
    if swap {
        pixels
            .chunks_exact_mut(4)
            .for_each(|pixel| pixel.swap(0, 2));
    }
    pixels
}
