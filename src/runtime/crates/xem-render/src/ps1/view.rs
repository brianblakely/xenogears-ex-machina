//! The game as every host presents it: the draw trace accumulated between
//! presentations, the PS1 renderer, the frame (replayed for wider framing
//! and interpolation) and the effects, configured from the graphics
//! settings. The host hands over traces and the display; nothing here
//! reaches the game.

use super::{DisplayArea, DrawOptions, Effects, EffectsRenderer, Fidelity, Filtering, FrameRenderer, Framing, Lighting, Ps1Renderer, Quality, central_depth};
use xem_gpu::{DisplayInfo, Trace, TraceOp};
use xem_settings::{Filter, Graphics, QualityTier};

/// What kind of host presents, for the settings' automatic choices.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HostClass {
    Desktop,
    Mobile,
    Browser,
    /// A headset (OpenXR, WebXR): low tier, no depth of field by default.
    Headset,
}

/// The resolved presentation configuration.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ViewConfig {
    pub draw: DrawOptions,
    /// Internal scale (multiples of the original resolution).
    pub scale: u32,
    pub effects: Effects,
    pub quality: Quality,
    /// Wider framing: VRAM pixels added on each side.
    pub side: u32,
    pub interpolation: bool,
    /// How far the field camera is pulled back (1 the original, up to 5):
    /// the subject the camera looks at appears this many times smaller.
    /// The world map takes a tenth of the effect; other modes none.
    pub zoom: f32,
}

/// The widest wider framing: VRAM pixels on each side.
pub const MAX_WIDE_SIDE: u32 = 160;

/// Wider framing's sides for an output of `width` x `height` pixels: the
/// original's 320x224 picture widened at the same height to the output's
/// aspect ratio (39 for 16:9, 101 for 21:9), at most [`MAX_WIDE_SIDE`].
pub fn wide_side((width, height): (u32, u32)) -> u32 {
    let wide = 224.0 * width as f32 / height.max(1) as f32;
    (((wide - 320.0) / 2.0).round().max(0.0) as u32).min(MAX_WIDE_SIDE)
}

/// The share of the field camera's pull-back a scene takes ("field:12",
/// "worldmap", ...): the field all of it, the world map a tenth.
pub fn scene_zoom(zoom: f32, scene: &str) -> f32 {
    match scene.split(':').next() {
        Some("field") => zoom,
        Some("worldmap") => 1.0 + (zoom - 1.0) / 10.0,
        _ => 1.0,
    }
}

impl ViewConfig {
    /// `graphics` for `host`, drawing for an output area of `output` pixels
    /// (the automatic scale follows the part of it the game picture fills),
    /// on a device allowing `max_scale`.
    pub fn resolve(graphics: &Graphics, host: HostClass, output: (u32, u32), max_scale: u32) -> ViewConfig {
        let f = graphics.fidelity();
        let fidelity = Fidelity {
            affine_textures: f.affine_textures,
            vertex_snapping: f.vertex_snapping,
            color_15bit: f.color_15bit,
            dithering: f.dithering,
            native_resolution: f.native_resolution,
            painter_ordering: f.painter_ordering,
        };
        let linear = |filter: Filter| filter == Filter::Linear;
        let draw = DrawOptions {
            fidelity,
            filtering: Filtering {
                world_linear: linear(graphics.filtering.world),
                sprites_linear: linear(graphics.filtering.sprites),
                interface_linear: linear(graphics.filtering.interface),
            },
            culling: super::Culling { viewport: graphics.culling.viewport, back_faces: graphics.culling.back_faces },
        };
        // A headset's virtual screen takes a 16:9 shape when widened.
        let side = match (graphics.widescreen, host) {
            (false, _) => 0,
            (true, HostClass::Headset) => wide_side((16, 9)),
            (true, _) => wide_side(output),
        };
        // The picture (about 320x224, wider with wider framing) fitted into
        // the output: its height there, in original lines.
        let fitted = output.1.min(output.0 * 224 / (320 + 2 * side));
        // Automatic scales stop where the host's budget does.
        let budget = match host {
            HostClass::Desktop => 8,
            HostClass::Mobile | HostClass::Browser | HostClass::Headset => 4,
        };
        let scale = if fidelity.native_resolution {
            1
        } else if graphics.internal_scale == 0 {
            fitted.div_ceil(224).clamp(1, budget.min(max_scale))
        } else {
            u32::from(graphics.internal_scale).clamp(1, max_scale)
        };
        let levels = graphics.effects();
        let level = |v: u8| f32::from(v) / 100.0;
        let mut effects = Effects {
            lighting: level(levels.lighting),
            shadows: level(levels.shadows),
            ambient_occlusion: level(levels.ambient_occlusion),
            fog: level(levels.fog),
            light_shafts: level(levels.light_shafts),
            bloom: level(levels.bloom),
            color_grading: level(levels.color_grading),
            depth_of_field: level(levels.depth_of_field),
            focus: f32::from(levels.focus) / 100.0,
        };
        if host == HostClass::Headset && !graphics.xr.depth_of_field {
            effects.depth_of_field = 0.0;
        }
        let quality = match (graphics.quality, host) {
            (QualityTier::Low, _) | (QualityTier::Auto, HostClass::Headset) => Quality::Low,
            (QualityTier::Medium, _) | (QualityTier::Auto, HostClass::Mobile | HostClass::Browser) => Quality::Medium,
            (QualityTier::High, _) | (QualityTier::Auto, HostClass::Desktop) => Quality::High,
            (QualityTier::Ultra, _) => Quality::Ultra,
        };
        ViewConfig {
            draw,
            scale,
            effects,
            quality,
            side,
            interpolation: graphics.interpolation,
            zoom: f32::from(graphics.zoom_out.clamp(100, xem_settings::MAX_ZOOM_OUT)) / 100.0,
        }
    }
}

/// Ops kept between presentations at most (as the device's trace keeps
/// between takes); past it the trace restarts from the whole VRAM (a host
/// that stopped presenting): one replay's vertices stay tens of MB.
const PENDING_LIMIT: usize = 100_000;

/// A presented frame: an `Rgba8Unorm` texture of sRGB-encoded values.
pub struct Presented<'a> {
    pub view: &'a wgpu::TextureView,
    pub size: (u32, u32),
    /// A new game frame arrived with this presentation.
    pub new_frame: bool,
}

pub struct GameView {
    device: wgpu::Device,
    queue: wgpu::Queue,
    renderer: Ps1Renderer,
    frame: FrameRenderer,
    effects: EffectsRenderer,
    config: Option<ViewConfig>,
    pending: Vec<TraceOp>,
    /// The trace must restart from the whole VRAM.
    restart: bool,
    /// The last presentation drew the effects (else the plain frame).
    presented_effects: Option<bool>,
    /// When the newest game frame arrived and the interval before it
    /// (seconds, the host's clock), for interpolation.
    arrived: f64,
    interval: f64,
    /// The scene last presented, the depth the field camera looks at as
    /// the port recorded it, and the depth the camera is pulled back from.
    scene: String,
    recorded_focus: Option<f32>,
    focus: Option<f32>,
}

impl GameView {
    pub fn new(device: &wgpu::Device, queue: &wgpu::Queue, adapter: Option<&wgpu::Adapter>) -> Self {
        Self {
            device: device.clone(),
            queue: queue.clone(),
            renderer: Ps1Renderer::new(device, queue, 1),
            frame: FrameRenderer::new(device),
            effects: EffectsRenderer::new(device, adapter),
            config: None,
            pending: Vec::new(),
            restart: true,
            presented_effects: None,
            arrived: 0.0,
            interval: 1.0 / 30.0,
            scene: String::new(),
            recorded_focus: None,
            focus: None,
        }
    }

    pub fn max_scale(&self) -> u32 {
        Ps1Renderer::max_scale(&self.device)
    }

    pub fn config(&self) -> Option<&ViewConfig> {
        self.config.as_ref()
    }

    /// Applies a configuration (cheap when unchanged).
    pub fn configure(&mut self, config: ViewConfig) {
        if self.config == Some(config) {
            return;
        }
        self.renderer.set_options(config.draw, config.scale);
        if self.renderer.needs_full_vram() {
            self.restart = true;
        }
        self.config = Some(config);
    }

    /// Whether the host must restart the device's trace (`set_trace(true)`)
    /// before the next [`GameView::push`]: the picture needs all of VRAM.
    pub fn take_restart(&mut self) -> bool {
        std::mem::take(&mut self.restart)
    }

    /// How far beyond its screen the device's trace should record the faces
    /// the game culls against it (`set_reveals`; pixels, x and y): the
    /// wider framing's sides, or everywhere when the camera is pulled back
    /// in the scene last presented; none with viewport culling.
    pub fn reveal_reach(&self) -> Option<[u16; 2]> {
        let config = self.config?;
        let zoom = scene_zoom(config.zoom, &self.scene);
        if config.draw.culling.viewport {
            None
        } else if zoom > 1.0 {
            // Pulled back k times, what lies at the camera's target depth
            // within k frames of the middle shows, and nearer things from
            // farther out still: twice that, enough down to half the depth.
            let width = 320.0 + 2.0 * config.side as f32;
            let x = (2.0 * (zoom - 1.0) * width + config.side as f32 + 16.0).min(2048.0);
            let y = (2.0 * (zoom - 1.0) * 240.0 + 16.0).min(2048.0);
            Some([x as u16, y as u16])
        } else if config.side > 0 {
            // The sides, and a little more for faces leaning into them.
            Some([config.side as u16 + 16, 16])
        } else {
            None
        }
    }

    /// Adds what the game drew since the last push.
    pub fn push(&mut self, trace: Trace) {
        if let Some(TraceOp::Write { rect, upload: true, .. }) = trace.ops.first()
            && u32::from(rect.width) == super::VRAM_WIDTH
        {
            self.pending.clear();
        }
        if let Some(focus) = trace.focus {
            self.recorded_focus = Some(f32::from(focus));
        }
        self.pending.extend(trace.ops);
        if self.pending.len() > PENDING_LIMIT {
            self.pending.clear();
            self.restart = true;
        }
    }

    /// Draws the pending trace and the frame of `display`, with the effects
    /// tuned for `scene` ([`Lighting::for_scene`]), at `now` seconds of the
    /// host's clock (interpolation follows the game's frame arrivals).
    /// `None` while there is nothing to show (a 24-bit display shows the
    /// device's own picture instead; see [`GameView::shows`]).
    pub fn present(&mut self, encoder: &mut wgpu::CommandEncoder, display: &DisplayInfo, scene: &str, now: f64) -> Option<Presented<'_>> {
        let config = self.config?;
        let trace = Trace { ops: std::mem::take(&mut self.pending), focus: None };
        self.renderer.draw(encoder, &trace);
        if !Self::shows(display) || self.renderer.needs_full_vram() {
            return None;
        }
        let area = DisplayArea { x: display.x.into(), y: display.y.into(), width: display.width, height: display.height };
        let new_frame = self.frame.track(&self.renderer, area);
        if scene != self.scene {
            self.scene = scene.to_string();
            self.recorded_focus = None;
            self.focus = None;
        }
        if new_frame {
            // Game frames come every 1-4 vertical blanks; a long gap (a
            // pause, a load) is not an interval to interpolate over.
            self.interval = (now - self.arrived).clamp(1.0 / 60.0, 4.0 / 60.0);
            self.arrived = now;
            self.update_focus(area);
        }
        let alpha = ((now - self.arrived) / self.interval).clamp(0.0, 1.0) as f32;
        let zoom = scene_zoom(config.zoom, scene);
        let pull_back = if zoom > 1.0 { self.focus.map_or(0.0, |focus| (zoom - 1.0) * focus) } else { 0.0 };
        let framing = Framing { side: config.side, interpolate: config.interpolation.then_some(alpha), pull_back };
        self.frame.prepare(&self.device, &self.queue, encoder, &mut self.renderer, area, framing);
        self.presented_effects = Some(config.effects.any());
        if config.effects.any() {
            let lighting = Lighting::for_scene(scene);
            let view = self.effects.render(
                &self.device,
                &self.queue,
                encoder,
                &mut self.renderer,
                &self.frame,
                self.frame.primitives(),
                &config.effects,
                config.quality,
                &lighting,
            );
            return Some(Presented { view, size: self.frame.size(), new_frame });
        }
        Some(Presented { view: self.frame.color_view(), size: self.frame.size(), new_frame })
    }

    /// The depth a pulled-back camera keeps its zoom at: the field camera's
    /// target where the port records it, else the depth in the middle of
    /// the screen, eased between frames (and taken at once after a cut).
    fn update_focus(&mut self, area: DisplayArea) {
        if let Some(recorded) = self.recorded_focus.filter(|f| *f > 0.0) {
            self.focus = Some(recorded);
            return;
        }
        let Some(measured) = central_depth(self.frame.newest(), area) else { return };
        self.focus = Some(match self.focus {
            Some(focus) if (measured / focus - 1.0).abs() < 0.5 => focus + (measured - focus) * 0.1,
            _ => measured,
        });
    }

    /// Whether this view draws `display` (15-bit and on); 24-bit movie
    /// frames are shown from the device's VRAM as they are.
    pub fn shows(display: &DisplayInfo) -> bool {
        display.enabled && !display.color24 && display.width > 0 && display.height > 0
    }

    /// The frame the last [`GameView::present`] drew (its colour and
    /// auxiliary textures), before the effects.
    pub fn frame(&self) -> &FrameRenderer {
        &self.frame
    }

    /// The texture the last [`GameView::present`] returned a view of.
    pub fn presented_texture(&self) -> Option<&wgpu::Texture> {
        match self.presented_effects? {
            true => self.effects.output(),
            false => Some(self.frame.color()),
        }
    }

    /// What the renderer drew and dropped at the last presentation.
    pub fn stats(&self) -> super::DrawStats {
        self.renderer.stats()
    }

    pub fn renderer(&self) -> &Ps1Renderer {
        &self.renderer
    }

    pub fn effects_mut(&mut self) -> &mut EffectsRenderer {
        &mut self.effects
    }
}
