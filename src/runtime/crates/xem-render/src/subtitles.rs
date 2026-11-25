//! The anime movies' subtitles as every host shows them (docs/runtime.md,
//! Subtitles). `xem_core::subtitles` draws the line a movie shows in the
//! display's own pixels; a [`SubtitleLayer`] keeps it as a texture, drawn
//! again only when the line or its language changes, and the hosts place it
//! over the picture as they place the picture: centred on the display, its
//! bottom [`BOTTOM`] of the display's height above the display's, each of
//! its pixels as large as one of the display's ([`flat`] on a flat target,
//! [`on_quad`] on a headset's screen or pane). Drawn after the picture and
//! before the hosts' own widgets and panels, with nearest filtering, which
//! keeps the font's pixels.
use glam::{Mat4, Vec2, Vec3};

use crate::compositor::{Picture, Rect};

/// The subtitle's bottom above the display's, as a share of the display's
/// height: Perfect Works' own lines end 21 of a movie's 224 lines above it.
pub const BOTTOM: f32 = 20.0 / 224.0;

/// The subtitle shown, as a texture of sRGB-encoded, premultiplied RGBA
/// (`Quad::srgb_encoded`), and the line it shows (`K`).
pub struct SubtitleLayer<K> {
    key: Option<K>,
    texture: Option<(wgpu::Texture, wgpu::TextureView, (u32, u32))>,
}

impl<K> Default for SubtitleLayer<K> {
    fn default() -> Self {
        Self { key: None, texture: None }
    }
}

impl<K: Clone + PartialEq> SubtitleLayer<K> {
    /// Show `key`'s subtitle, or none: `draw` gives its `width` x `height`
    /// RGBA8 pixels (sRGB-encoded, each opaque or clear) when it is not the
    /// one shown; none when it cannot be drawn.
    pub fn update(&mut self, device: &wgpu::Device, queue: &wgpu::Queue, key: Option<K>, draw: impl FnOnce(&K) -> Option<(u32, u32, Vec<u8>)>) {
        if key == self.key {
            return;
        }
        self.texture = key.as_ref().and_then(draw).filter(|(w, h, rgba)| *w > 0 && *h > 0 && rgba.len() == (w * h * 4) as usize).map(|(width, height, rgba)| {
            let size = wgpu::Extent3d { width, height, depth_or_array_layers: 1 };
            let texture = device.create_texture(&wgpu::TextureDescriptor {
                label: Some("subtitle"),
                size,
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format: wgpu::TextureFormat::Rgba8Unorm,
                usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
                view_formats: &[],
            });
            queue.write_texture(
                texture.as_image_copy(),
                &rgba,
                wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(width * 4), rows_per_image: Some(height) },
                size,
            );
            let view = texture.create_view(&Default::default());
            (texture, view, (width, height))
        });
        self.key = key;
    }

    /// The subtitle's texture and its size in display pixels, while one shows.
    pub fn shown(&self) -> Option<(&wgpu::TextureView, (u32, u32))> {
        self.texture.as_ref().map(|(_, view, size)| (view, *size))
    }

    /// The line it shows.
    pub fn key(&self) -> Option<&K> {
        self.key.as_ref()
    }
}

/// Where a subtitle of `size` display pixels stands on the display: its
/// top left, centred across the display, its bottom [`BOTTOM`] up.
fn top_left(display: (f32, f32), size: (u32, u32)) -> (f32, f32) {
    ((display.0 - size.0 as f32) / 2.0, display.1 * (1.0 - BOTTOM) - size.1 as f32)
}

/// A subtitle of `size` display pixels over a flat target's `picture`, in
/// target pixels.
pub fn flat(picture: &Picture, size: (u32, u32)) -> Rect {
    let (x, y) = picture.from_screen(top_left(picture.display, size));
    let scale = (picture.placed.width / (picture.display.0 + 2.0 * picture.side), picture.placed.height / picture.display.1);
    Rect { x, y, width: size.0 as f32 * scale.0, height: size.1 as f32 * scale.1 }
}

/// A subtitle of `size` display pixels on a headset's screen or pane: a
/// quad (`pose` its centre, `quad` its width and height in metres) showing
/// the display, `side` pixels of wider framing each side of it, 2 mm in
/// front. Its pose and size, for `Quad::world`.
pub fn on_quad(pose: Mat4, quad: Vec2, display: (f32, f32), side: f32, size: (u32, u32)) -> (Mat4, Vec2) {
    let unit = Vec2::new(quad.x / (display.0 + 2.0 * side), quad.y / display.1);
    let extent = Vec2::new(size.0 as f32, size.1 as f32) * unit;
    let (_, top) = top_left(display, size);
    let centre = Vec3::new(0.0, quad.y / 2.0 - (top * unit.y + extent.y / 2.0), 0.002);
    (pose * Mat4::from_translation(centre), extent)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_subtitle_is_centred_on_the_display_near_its_bottom_at_its_pixels_size() {
        // A 320x224 movie shown three times as large, 40 pixels in: a 100x13
        // subtitle centred, its bottom 20 lines above the display's.
        let picture = Picture { placed: Rect { x: 40.0, y: 10.0, width: 960.0, height: 672.0 }, display: (320.0, 224.0), side: 0.0 };
        let rect = flat(&picture, (100, 13));
        assert_eq!((rect.x, rect.width, rect.height), (40.0 + 110.0 * 3.0, 300.0, 39.0));
        assert!((rect.y + rect.height - (10.0 + 204.0 * 3.0)).abs() < 1e-3);
        // Wider framing's sides leave it on the display: they widen the
        // picture, not the subtitle's place on the display.
        let wide = Picture { placed: Rect { x: 0.0, y: 0.0, width: 1280.0, height: 720.0 }, display: (320.0, 240.0), side: 53.0 };
        let rect = flat(&wide, (100, 13));
        let px = 1280.0 / 426.0;
        assert!((rect.x - (53.0 + 110.0) * px).abs() < 1e-3 && (rect.width - 100.0 * px).abs() < 1e-3);
    }

    #[test]
    fn on_a_headset_it_stands_on_the_screen_where_a_flat_target_shows_it() {
        // A 3 m screen 2.5 m ahead showing 320x224: a 100x13 subtitle 2 mm in
        // front, centred across, its bottom 20 lines above the screen's.
        let pose = Mat4::from_translation(Vec3::new(0.0, 0.0, -2.5));
        let quad = Vec2::new(3.0, 2.1);
        let (at, size) = on_quad(pose, quad, (320.0, 224.0), 0.0, (100, 13));
        let unit = 3.0 / 320.0;
        assert!((size - Vec2::new(100.0 * unit, 13.0 * 2.1 / 224.0)).length() < 1e-6);
        let centre = at.w_axis.truncate();
        assert!((centre.x).abs() < 1e-6 && (centre.z - (-2.5 + 0.002)).abs() < 1e-6);
        let bottom = centre.y - size.y / 2.0;
        assert!((bottom - (-2.1 / 2.0 + 20.0 * 2.1 / 224.0)).abs() < 1e-5, "{bottom}");
    }
}
