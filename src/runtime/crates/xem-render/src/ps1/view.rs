//! The game as every host presents it: the draw trace accumulated between
//! presentations, the PS1 renderer, the frame (replayed for wider framing
//! and interpolation) and the effects, configured from the graphics
//! settings. The host hands over traces and the display; nothing here
//! reaches the game.

use super::{DisplayArea, DrawOptions, Effects, EffectsRenderer, Fidelity, Filtering, FrameRenderer, Framing, Lighting, Look, Ps1Renderer, Quality, central_depth};
use xem_gpu::{Class, DisplayInfo, Reveals, Trace, TraceOp};
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
    /// Wider framing: VRAM pixels added on each side of a 224-line picture
    /// (the automatic scale's estimate), and the output's aspect ratio the
    /// sides widen the picture to (the display's own height decides them).
    pub side: u32,
    pub aspect: Option<f32>,
    pub interpolation: bool,
    /// How far the field camera is pulled back (1 the original, up to 5):
    /// the subject the camera looks at appears this many times smaller.
    /// The world map takes a tenth of the effect; other modes none.
    pub zoom: f32,
    /// Enhanced movies ([`super::MovieRenderer`]): the output the rebuilt
    /// picture is fitted into. `None` leaves movie frames to the device's
    /// own picture (the Original setting).
    pub movies: Option<(u32, u32)>,
}

/// The widest wider framing: VRAM pixels on each side.
pub const MAX_WIDE_SIDE: u32 = 160;

/// Wider framing's sides for an output of `width` x `height` pixels: the
/// original's 320x224 picture widened at the same height to the output's
/// aspect ratio (39 for 16:9, 101 for 21:9), at most [`MAX_WIDE_SIDE`].
pub fn wide_side((width, height): (u32, u32)) -> u32 {
    display_side((320, 224), width as f32 / height.max(1) as f32)
}

/// Wider framing's sides for a display of `width` x `height` VRAM pixels
/// widened at its own height to `aspect` (a 320x216 display: 32 for 16:9).
pub fn display_side((width, height): (u32, u32), aspect: f32) -> u32 {
    let wide = height as f32 * aspect;
    (((wide - width as f32) / 2.0).round().max(0.0) as u32).min(MAX_WIDE_SIDE)
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
        let aspect = match (graphics.widescreen, host) {
            (false, _) => None,
            (true, HostClass::Headset) => Some(16.0 / 9.0),
            (true, _) => Some(output.0 as f32 / output.1.max(1) as f32),
        };
        let side = aspect.map_or(0, |aspect| display_side((320, 224), aspect));
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
            aspect,
            interpolation: graphics.interpolation,
            zoom: f32::from(graphics.zoom_out.clamp(100, xem_settings::MAX_ZOOM_OUT)) / 100.0,
            movies: (graphics.filtering.movies == xem_settings::MovieFilter::Enhanced).then_some(output),
        }
    }
}

/// Ops kept between presentations at most (as the device's trace keeps
/// between takes); past it the trace restarts from the whole VRAM (a host
/// that stopped presenting): one replay's vertices stay tens of MB.
const PENDING_LIMIT: usize = 100_000;

/// First-person inspection's projection distance: a 70-degree field of
/// view across the original's 320 pixels (160 / tan 35°).
const LOOK_H: f32 = 228.5;
/// Its eye on the character's sprite, as a share of the sprite's height, and
/// that height in world units where the frame does not draw the sprite
/// (Fei's, measured in four fields: 70 to 78).
const EYE_LEVEL: f32 = 0.9;
const SPRITE_HEIGHT: f32 = 74.0;
/// How far it looks up or down at most (radians: 80 degrees).
const MAX_PITCH: f32 = 1.396;

/// First-person inspection (docs/runtime.md): the view's turn from the field
/// camera's heading when it began (`yaw` to the right, `pitch` up, radians),
/// that heading and the eye's height above the character's feet (from the
/// first frame showing where the character stood), and the field it began
/// in (leaving it ends the view).
#[derive(Clone, Debug, Default, PartialEq)]
struct FirstPerson {
    yaw: f32,
    pitch: f32,
    heading: Option<f32>,
    height: Option<f32>,
    field: Option<String>,
}

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
    /// Wider framing's sides at the last presentation (the display's).
    side: u32,
    /// The scene that drew each frame the renderer logs (by its identity).
    frame_scenes: Vec<(((u16, u16), u64), String)>,
    /// Welds each trace's scene points ([`super::weld`]), its tables kept.
    welder: super::Welder,
    /// What the last presentation showed: presented again with nothing new
    /// to draw (the display refreshing faster than the game draws), its
    /// picture stands.
    shown: Option<Shown>,
    /// Movie frames rebuilt from what the MDEC decoded, and whether the last
    /// presentation was one.
    movie: super::MovieRenderer,
    presented_movie: bool,
    /// First-person inspection, while it is on.
    first_person: Option<FirstPerson>,
}

/// What a presentation shows: the frame (its identity and the ops that
/// drew it so far), where and how.
#[derive(Clone, Copy, PartialEq)]
struct Shown {
    identity: Option<((u16, u16), u64)>,
    drawn: usize,
    area: DisplayArea,
    config: ViewConfig,
    framing: Framing,
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
            side: 0,
            frame_scenes: Vec::new(),
            welder: super::Welder::default(),
            shown: None,
            movie: super::MovieRenderer::new(adapter.is_some_and(|adapter| adapter.get_info().device_type == wgpu::DeviceType::Cpu)),
            presented_movie: false,
            first_person: None,
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

    /// Which of the faces the game culls against its screen the device's
    /// trace should record (`set_reveals`): those in the wider framing's
    /// sides, or, with the camera pulled back in the scene last presented,
    /// those the pulled-back camera sees (judged on its own screen once the
    /// depth it pulls back by is known); none with viewport culling.
    pub fn reveals(&self) -> Option<Reveals> {
        let config = self.config?;
        let zoom = scene_zoom(config.zoom, &self.scene);
        let side = config.side.max(self.side);
        if self.first_person.is_some() && self.scene.starts_with("field") {
            // Looking around from the controlled character: every face,
            // and where the character stands.
            Some(Reveals { reach: [side as u16 + 32, 32], pull_back: 0, around: Reveals::LOOK })
        } else if config.draw.culling.viewport {
            None
        } else if zoom > 1.0 {
            match self.focus {
                // The margin covers the camera moving between frames.
                Some(focus) => Some(Reveals { reach: [side as u16 + 32, 32], pull_back: ((zoom - 1.0) * focus).round() as u32, around: 0 }),
                None => {
                    // Pulled back k times, what lies at the camera's target
                    // depth within k frames of the middle shows, and nearer
                    // things from farther out still: twice that, enough down
                    // to half the depth.
                    let width = 320.0 + 2.0 * side as f32;
                    let x = (2.0 * (zoom - 1.0) * width + side as f32 + 16.0).min(2048.0);
                    let y = (2.0 * (zoom - 1.0) * 240.0 + 16.0).min(2048.0);
                    Some(Reveals { reach: [x as u16, y as u16], pull_back: 0, around: 0 })
                }
            }
        } else if side > 0 || (self.scene.starts_with("worldmap") && !config.draw.fidelity.vertex_snapping) {
            // The sides, and a little more for faces leaning into them.
            // Modern world terrain also needs sub-pixel slivers the
            // original's whole-pixel winding test discarded inside them.
            Some(Reveals { reach: [side as u16 + 16, 16], pull_back: 0, around: 0 })
        } else {
            None
        }
    }

    /// The view the game's field characters keep moving in (the session's
    /// `set_actor_view`): the camera's pull-back in percent plus 100 (bits
    /// 0-15) and the pixels shown beyond each side (bits 16-31); 0 while the
    /// view is the original's. Looking around, the widest: the view may
    /// turn anywhere.
    pub fn actor_view(&self) -> u32 {
        let Some(config) = self.config else { return 0 };
        if self.first_person.is_some() {
            return 1000 | 1024 << 16;
        }
        let zoom = (config.zoom * 100.0).round().clamp(100.0, 1000.0) as u32;
        let side = config.side.max(self.side).min(1024);
        if zoom == 100 && side == 0 { 0 } else { zoom | side << 16 }
    }

    /// Turns first-person inspection on (looking along the field camera's
    /// heading, level, from the controlled character's eyes, once a frame
    /// shows where it stands) or off. It shows only in a field and ends
    /// when the field (or its map) is left.
    pub fn set_first_person(&mut self, on: bool) {
        self.first_person = on.then(FirstPerson::default);
    }

    /// Whether first-person inspection is on.
    pub fn first_person(&self) -> bool {
        self.first_person.is_some()
    }

    /// The first-person view's axes (right, down, forward) in the game
    /// camera's view space, while it looks around: where a listener faces
    /// (the Binaural mode's first-person listener). They never include a
    /// headset's head, which the session turns the listener by once more
    /// (`Session::set_head`).
    pub fn listener_axes(&mut self) -> Option<[[f32; 3]; 3]> {
        self.look().map(|look| look.axes)
    }

    /// How far the first-person view has turned from the heading it began
    /// with: radians to the right, and up (none while it is off).
    pub fn look_angles(&self) -> Option<(f32, f32)> {
        self.first_person.as_ref().map(|view| (view.yaw, view.pitch))
    }

    /// Turns the first-person view by `yaw` radians to the right and
    /// `pitch` up (within 80 degrees of level).
    pub fn look_around(&mut self, yaw: f32, pitch: f32) {
        if let Some(view) = &mut self.first_person {
            view.yaw = (view.yaw + yaw) % std::f32::consts::TAU;
            view.pitch = (view.pitch + pitch).clamp(-MAX_PITCH, MAX_PITCH);
        }
    }

    /// The first-person view of the frame shown, when it shows where the
    /// controlled character stood: from its eyes (by its sprite's height
    /// above its feet), turned from the heading it began with.
    fn look(&mut self) -> Option<Look> {
        let eye = self.frame.eye()?;
        let view = self.first_person.as_mut()?;
        let unit = |v: [f32; 3]| {
            let length = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
            (length > 0.0).then(|| v.map(|c| c / length))
        };
        let axes = [unit(eye.axes[0])?, unit(eye.axes[1])?, unit(eye.axes[2])?];
        // A direction of the world (y down) in the game camera's view space.
        let world = |w: [f32; 3]| -> [f32; 3] { std::array::from_fn(|i| w[0] * axes[0][i] + w[1] * axes[1][i] + w[2] * axes[2][i]) };
        let up = world([0.0, -1.0, 0.0]);
        // The heading the game's camera looked along: its axis in the world.
        let heading = *view.heading.get_or_insert_with(|| axes[0][2].atan2(axes[2][2]));
        let height = match view.height {
            Some(height) => height,
            None => {
                // A sprite the frame does not draw stands as tall as the
                // field's characters do (the axes carry the field's scale).
                let scale = eye.axes[0].iter().map(|c| c * c).sum::<f32>().sqrt() / 4096.0;
                let top = sprite_height(self.frame.newest(), eye.sprite, eye.position, up).unwrap_or(SPRITE_HEIGHT * scale);
                *view.height.insert(top * EYE_LEVEL)
            }
        };
        let (yaw, pitch) = (heading + view.yaw, view.pitch);
        let forward = world([yaw.sin() * pitch.cos(), -pitch.sin(), yaw.cos() * pitch.cos()]);
        let right = world([yaw.cos(), 0.0, -yaw.sin()]);
        let down = [
            forward[1] * right[2] - forward[2] * right[1],
            forward[2] * right[0] - forward[0] * right[2],
            forward[0] * right[1] - forward[1] * right[0],
        ];
        Some(Look {
            eye: std::array::from_fn(|i| eye.position[i] + up[i] * height),
            axes: [right, down, forward],
            up,
            h: LOOK_H,
            near: (height / 32.0).max(1.0),
            hide: eye.sprite,
        })
    }

    /// Adds what the game drew since the last push.
    pub fn push(&mut self, mut trace: Trace) {
        self.movie.observe(&trace.ops);
        self.welder.weld(&mut trace.ops);
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
        if Self::shows(display) {
            self.renderer.set_display(xem_gpu::VramRect {
                x: display.x,
                y: display.y,
                width: display.width.min(1024) as u16,
                height: display.height.min(512) as u16,
            });
        }
        // A screen the game takes in this trace is the picture last presented
        // (a frame's; after a movie frame, the native pixels it took).
        let frame = self.presented_texture().filter(|_| !self.presented_movie).cloned();
        let presented = frame.zip(self.frame.area()).map(|(texture, (_, scale))| {
            super::PresentedPicture { texture, side: self.frame.side(), scale }
        });
        self.renderer.set_presented(presented);
        // The world map's terrain blocks meet in T-junctions that open at
        // higher scales; the original's whole pixels hide them.
        self.renderer.set_seal(scene.starts_with("worldmap") && !config.draw.fidelity.vertex_snapping);
        self.renderer.draw(encoder, &trace);
        // The frames this trace began are the current scene's.
        let identities: Vec<_> = self.renderer.log_identities().collect();
        self.frame_scenes.retain(|(identity, _)| identities.contains(identity));
        for identity in identities {
            if !self.frame_scenes.iter().any(|(known, _)| *known == identity) {
                self.frame_scenes.push((identity, scene.to_string()));
            }
        }
        // A movie frame the MDEC decoded, rebuilt (the Enhanced setting).
        if let Some(output) = config.movies
            && self.movie.shows(display)
        {
            self.presented_movie = true;
            self.shown = None;
            let (view, size, new_frame) = self.movie.present(&self.device, &self.queue, encoder, display, output, &self.renderer.vram)?;
            return Some(Presented { view, size, new_frame });
        }
        self.presented_movie = false;
        if !Self::shows(display) || self.renderer.needs_full_vram() {
            return None;
        }
        let area = DisplayArea { x: display.x.into(), y: display.y.into(), width: display.width, height: display.height };
        let new_frame = self.frame.track(&self.renderer, area);
        // The scene that drew the frame shown frames it: the field's last
        // frame stays pulled back while a battle loads behind it (and its
        // shatter takes it so), a battle's last one stays as the battle drew
        // it while the field loads.
        let shown = self.frame.identity().and_then(|identity| self.frame_scenes.iter().find(|(known, _)| *known == identity));
        let shown = shown.map_or(scene, |(_, drawn)| drawn.as_str());
        if shown != self.scene {
            self.scene = shown.to_string();
            self.recorded_focus = None;
            self.focus = None;
        }
        // First-person inspection is the field's it began in.
        if let Some(view) = &mut self.first_person {
            match &view.field {
                _ if !self.scene.starts_with("field") => self.first_person = None,
                Some(field) if *field != self.scene => self.first_person = None,
                Some(_) => {}
                None => view.field = Some(self.scene.clone()),
            }
        }
        if new_frame {
            // Game frames come every 1-4 vertical blanks; a long gap (a
            // pause, a load) is not an interval to interpolate over.
            self.interval = (now - self.arrived).clamp(1.0 / 60.0, 4.0 / 60.0);
            self.arrived = now;
            self.update_focus(area);
        }
        let alpha = ((now - self.arrived) / self.interval).clamp(0.0, 1.0) as f32;
        let zoom = scene_zoom(config.zoom, &self.scene);
        let look = self.look();
        let pull_back = if zoom > 1.0 && look.is_none() { self.focus.map_or(0.0, |focus| (zoom - 1.0) * focus) } else { 0.0 };
        // The sides widen this display at its own height to the output's shape.
        self.side = config.aspect.map_or(0, |aspect| display_side((area.width, area.height), aspect));
        let framing = Framing { side: self.side, interpolate: config.interpolation.then_some(alpha), pull_back, replay: config.effects.any(), look };
        let shown = Shown { identity: self.frame.identity(), drawn: self.frame.drawn(), area, config, framing };
        if !new_frame && self.shown == Some(shown) && self.presented_effects == Some(config.effects.any()) {
            let view = if config.effects.any() { self.effects.output_view()? } else { self.frame.color_view() };
            return Some(Presented { view, size: self.frame.size(), new_frame });
        }
        self.shown = Some(shown);
        self.frame.prepare(&self.device, &self.queue, encoder, &mut self.renderer, area, framing);
        self.presented_effects = Some(config.effects.any());
        if config.effects.any() {
            let lighting = Lighting::for_scene(&self.scene);
            self.effects.render(
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
            // The interface the game drew after its scene, over the effects.
            let output = self.effects.output().cloned();
            self.frame.draw_overlay(&self.device, &self.queue, encoder, &mut self.renderer, output.as_ref());
            let view = self.effects.output_view()?;
            return Some(Presented { view, size: self.frame.size(), new_frame });
        }
        self.frame.draw_overlay(&self.device, &self.queue, encoder, &mut self.renderer, None);
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

    /// Whether this view draws a movie frame `display` shows: rebuilt from
    /// what the MDEC decoded (the Enhanced setting), when every macroblock
    /// of it is one the game loaded.
    pub fn shows_movie(&self, display: &DisplayInfo) -> bool {
        self.config.is_some_and(|config| config.movies.is_some()) && self.movie.shows(display)
    }

    /// Whether this view draws `display` (15-bit and on); 24-bit movie
    /// frames are the device's own picture unless [`GameView::shows_movie`].
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
        if self.presented_movie {
            return self.movie.output();
        }
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

/// How high the parts of sprite `owner` in `ops` reach above `feet` along
/// `up` (view-space units), if it has any.
fn sprite_height(ops: &[TraceOp], owner: u32, feet: [f32; 3], up: [f32; 3]) -> Option<f32> {
    ops.iter()
        .filter_map(|op| match op {
            TraceOp::Draw(p) if p.class == Class::Sprite && p.owner == owner => Some(p),
            _ => None,
        })
        .flat_map(|p| p.vertices[..p.vertex_count()].iter().filter_map(|v| v.view))
        .map(|q| (0..3).map(|i| (q[i] - feet[i]) * up[i]).sum::<f32>())
        .reduce(f32::max)
        .filter(|height| *height > 0.0)
}
