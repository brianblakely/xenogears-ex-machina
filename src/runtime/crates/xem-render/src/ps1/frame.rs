//! The displayed frame: the display area of the scaled VRAM picture, as
//! colour and auxiliary textures the effects and the compositor read.

use super::{AUX_FORMAT, COLOR_FORMAT, DisplayArea, Ps1Renderer, picture_usage, texture};

/// How the frame is framed. (Wider framing and interpolation replay the
/// frame's primitives; see [`FrameRenderer`].)
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Framing {
    /// Extra VRAM pixels shown on each side of the display area, filled by
    /// scene primitives that reach past the original screen.
    pub side: u32,
}

/// Holds the presented frame.
pub struct FrameRenderer {
    color: wgpu::Texture,
    aux: wgpu::Texture,
    color_view: wgpu::TextureView,
    size: (u32, u32),
    area: Option<DisplayArea>,
    scale: u32,
}

impl FrameRenderer {
    pub fn new(device: &wgpu::Device) -> Self {
        let color = texture(device, "frame", (1, 1), COLOR_FORMAT, picture_usage());
        let aux = texture(device, "frame aux", (1, 1), AUX_FORMAT, picture_usage());
        Self { color_view: color.create_view(&Default::default()), color, aux, size: (1, 1), area: None, scale: 1 }
    }

    fn resize(&mut self, device: &wgpu::Device, size: (u32, u32)) {
        if self.size != size {
            self.color = texture(device, "frame", size, COLOR_FORMAT, picture_usage());
            self.aux = texture(device, "frame aux", size, AUX_FORMAT, picture_usage());
            self.color_view = self.color.create_view(&Default::default());
            self.size = size;
        }
    }

    /// Takes `area` of the renderer's picture as the frame.
    pub fn prepare(&mut self, device: &wgpu::Device, encoder: &mut wgpu::CommandEncoder, renderer: &Ps1Renderer, area: DisplayArea) {
        let scale = renderer.scale();
        self.resize(device, (area.width.max(1) * scale, area.height.max(1) * scale));
        renderer.copy_display(encoder, area, &self.color, &self.aux);
        self.area = Some(area);
        self.scale = scale;
    }

    /// The frame's size in pixels.
    pub fn size(&self) -> (u32, u32) {
        self.size
    }

    /// The display area in VRAM pixels the frame shows, and its scale.
    pub fn area(&self) -> Option<(DisplayArea, u32)> {
        self.area.map(|area| (area, self.scale))
    }

    pub fn color(&self) -> &wgpu::Texture {
        &self.color
    }

    pub fn color_view(&self) -> &wgpu::TextureView {
        &self.color_view
    }

    pub fn aux(&self) -> &wgpu::Texture {
        &self.aux
    }
}
