//! The game as every host presents it: the draw trace accumulated between
//! presentations, the PS1 renderer, the frame (replayed for wider framing
//! and interpolation) and the effects, configured from the graphics
//! settings. The host hands over traces and the display; nothing here
//! reaches the game. A headset looking from within the scene (first-person
//! inspection) has each of its eyes drawn too, and the game's interface on
//! a pane of its own ([`GameView::present_headset`]).

use super::diorama::{Diorama, DioramaEye, DioramaPresented, Fallback, XrView};
use super::frame::projection_centre;
use super::{
    DisplayArea, DrawOptions, Effects, EffectsRenderer, Fidelity, Filtering, FrameRenderer, Framing, Interface, Lighting, Look, Ps1Renderer, Quality,
    central_depth,
};
use xem_gpu::{Class, DisplayInfo, Eye, Reveals, Trace, TraceOp};
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

/// The output a rebuilt movie frame is fitted into on a headset's virtual
/// screen ([`ViewConfig::movies`]): as many pixels across as the screen,
/// `width` metres wide and `distance` metres ahead, spans in an eye `eye`
/// pixels wide whose field of view's tangents are `fov` (left, right, up,
/// down), at most the eye's width (a larger screen shows only part of
/// itself), both ways: the picture keeps its shape within it.
pub fn headset_movie_output(width: f32, distance: f32, eye: u32, fov: [f32; 4]) -> (u32, u32) {
    let span = fov[1] - fov[0];
    let across = if span > 0.0 && distance > 0.0 { width / distance * eye as f32 / span } else { eye as f32 };
    let across = (across.round() as u32).clamp(1, eye.max(1));
    (across, across)
}

/// What a headset shows as the settings choose it (`Graphics::xr`): the
/// screen, or the diorama on its table as placed there. The Original
/// renderer draws no scene of its own: the screen, whatever the view.
pub fn xr_view(graphics: &Graphics) -> XrView {
    let xr = &graphics.xr;
    match xr.view {
        xem_settings::HeadsetView::Diorama if graphics.renderer == xem_settings::Renderer::ExMachina => XrView::Diorama(super::Table {
            scale: f32::from(xr.diorama_scale) / 100.0,
            height: f32::from(xr.diorama_height_cm) / 100.0,
            distance: f32::from(xr.diorama_distance_cm) / 100.0,
            tilt: f32::from(xr.diorama_tilt_deg).to_radians(),
            turns: match xr.diorama_turns {
                xem_settings::DioramaTurns::Smooth => super::Turns::Smooth,
                xem_settings::DioramaTurns::Snap => super::Turns::Snap,
                xem_settings::DioramaTurns::Fixed => super::Turns::Fixed,
            },
        }),
        _ => XrView::Screen,
    }
}

/// Whether a frame's ops draw a scene as the game drew it (its map, models
/// or sprites through the GTE), not only its interface and the faces the
/// port records for a wider view.
fn draws_scene(ops: &[TraceOp]) -> bool {
    ops.iter().any(|op| matches!(op, TraceOp::Draw(p) if p.class.is_scene() && !p.revealed))
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
/// A headset's eye height, metres: the head moving this far moves the
/// character's eye by the eye's own height above the character's feet (an
/// adult's standing eye height, assumed: Fei's eye at 66.6 units of a field
/// makes about 42 units a metre, 2.7 between the eyes of a 64 mm IPD).
pub const EYE_HEIGHT_METRES: f32 = 1.6;

/// First-person inspection (docs/runtime.md): the view's turn from the field
/// camera's heading when it began (`yaw` to the right, `pitch` up, radians),
/// that heading and the eye's height above the character's feet (from the
/// first frame showing where the character stood), the field it began in
/// (leaving it ends the view), the last frame's record of where the
/// character stood, and where the field's scene was projected about in the
/// display (VRAM pixels from its corner): a headset keeps both through
/// frames without them. Before it begins, `sceneless` counts the new frames
/// shown with the field camera's record but none of its scene.
#[derive(Clone, Debug, Default, PartialEq)]
struct FirstPerson {
    yaw: f32,
    pitch: f32,
    heading: Option<f32>,
    height: Option<f32>,
    field: Option<String>,
    held: Option<Eye>,
    centre: Option<(f32, f32)>,
    sceneless: u8,
}

/// A presented frame: an `Rgba8Unorm` texture of sRGB-encoded values.
pub struct Presented<'a> {
    pub view: &'a wgpu::TextureView,
    pub size: (u32, u32),
    /// A new game frame arrived with this presentation.
    pub new_frame: bool,
    /// The picture is a movie frame rebuilt from what the MDEC decoded (the
    /// Enhanced movies setting): the display alone, without wider framing's
    /// sides.
    pub movie: bool,
}

/// Which picture a flat presentation drew into: a movie frame, the screen
/// (the frame, or what the effects made of it), or beside a headset's eyes
/// the screen's interface alone, for its pane.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Picture {
    Movie,
    Screen,
    Pane,
}

/// A flat presentation: its picture, the picture's size and whether its
/// game frame is new.
#[derive(Clone, Copy, Debug)]
struct Flat {
    picture: Picture,
    size: (u32, u32),
    new_frame: bool,
}

/// One eye of a headset (OpenXR, WebXR) in its session's reference space,
/// whose -z is where the presentation faces (the virtual screen) and whose
/// y is up: its orientation (a unit quaternion x, y, z, w turning the eye's
/// own axes, x right, y up, looking along -z, into the space's), its
/// position (metres; the space's origin is where the character's eye
/// stands), and its field of view as the tangents of its half-angles
/// (left, right, up, down: left and down negative for a centred eye).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct HeadsetEye {
    pub orientation: [f32; 4],
    pub position: [f32; 3],
    pub fov: [f32; 4],
}

/// The viewpoint a picture showed the game's scene from when it was not the
/// game camera's own ([`GameView::viewpoint`]: the camera pulled back, or a
/// look from within the scene): a point `v` of the game camera's view space
/// (where [`xem_gpu::TraceVertex::view`] holds points) shows at `centre +
/// h · (q.x, q.y) / q.z`, `q = rows · v + offset`, in front of the view
/// (`q.z` above `near`). `centre` is in the picture's own pixels: the
/// display's, from its left edge and top (as hosts map pointers to the
/// game's screen). `rows` are orthonormal: the view's right, down and
/// forward.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Viewpoint {
    pub rows: [[f32; 3]; 3],
    pub offset: [f32; 3],
    pub h: f32,
    pub centre: (f32, f32),
    pub near: f32,
}

/// A headset's presentation ([`GameView::present_headset`]).
pub struct HeadsetPresented<'a> {
    /// Each eye's picture looking from within the scene (an `Rgba8Unorm`
    /// texture of sRGB-encoded values covering that eye's whole view), in
    /// the order the eyes came; none while the view is the game's screen.
    pub eyes: Vec<&'a wgpu::TextureView>,
    /// The screen's picture: the game's, or beside the eyes its interface
    /// alone, transparent elsewhere (premultiplied alpha), for a pane where
    /// the screen was.
    pub screen: Presented<'a>,
}

pub struct GameView {
    device: wgpu::Device,
    queue: wgpu::Queue,
    adapter: Option<wgpu::Adapter>,
    renderer: Ps1Renderer,
    /// The picture presented: the game's screen, or beside a headset's
    /// eyes its interface's pane.
    main: Target,
    /// A headset's eyes looking from within the scene, made when first
    /// needed, and whether the last presentation drew them.
    eyes: Vec<Target>,
    headset: bool,
    config: Option<ViewConfig>,
    pending: Vec<TraceOp>,
    /// The trace must restart from the whole VRAM.
    restart: bool,
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
    /// Movie frames rebuilt from what the MDEC decoded, and whether the last
    /// presentation was one.
    movie: super::MovieRenderer,
    presented_movie: bool,
    /// First-person inspection, while it is on.
    first_person: Option<FirstPerson>,
    /// What a headset shows, and the diorama's state.
    xr_view: XrView,
    diorama: Diorama,
    /// The room shows around a headset's frames ([`GameView::set_room`]).
    room: bool,
}

/// One picture a presentation draws: its frame, the effects over it, what
/// it showed last (presented again with nothing new to draw, the display
/// refreshing faster than the game draws, its picture stands) and whether
/// that had the effects.
struct Target {
    frame: FrameRenderer,
    effects: EffectsRenderer,
    shown: Option<Shown>,
    presented_effects: Option<bool>,
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

impl Target {
    fn new(device: &wgpu::Device, adapter: Option<&wgpu::Adapter>) -> Self {
        Self { frame: FrameRenderer::new(device), effects: EffectsRenderer::new(device, adapter), shown: None, presented_effects: None }
    }

    /// The picture last drawn: what the effects made of the frame, or the frame.
    fn texture(&self) -> Option<&wgpu::Texture> {
        match self.presented_effects? {
            true => self.effects.output(),
            false => Some(self.frame.color()),
        }
    }

    fn view(&self) -> Option<&wgpu::TextureView> {
        match self.presented_effects? {
            true => self.effects.output_view(),
            false => Some(self.frame.color_view()),
        }
    }

    /// Draws `area` of the renderer's picture framed by `framing`, with the
    /// effects `config` asks for (tuned for `scene`) but on the interface
    /// alone, unless this target shows it already.
    #[allow(clippy::too_many_arguments)]
    fn show(
        &mut self,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        encoder: &mut wgpu::CommandEncoder,
        renderer: &mut Ps1Renderer,
        area: DisplayArea,
        config: ViewConfig,
        framing: Framing,
        scene: &str,
        new_frame: bool,
    ) {
        // A frame held is replayed where it was drawn (the display may show
        // the game's other buffer now).
        let held = self.frame.area().map(|(area, _)| area).filter(|_| framing.hold && !self.frame.newest().is_empty());
        let area = held.unwrap_or(area);
        let new_frame = held.is_none() && (self.frame.track(renderer, area, framing.interpolate) || new_frame);
        let effects = config.effects.any() && framing.interface != Interface::Only;
        // Interpolation's time matters only while something moves.
        let settled = Framing { interpolate: framing.interpolate.filter(|&now| self.frame.moving(now)), ..framing };
        let shown = Shown { identity: self.frame.identity(), drawn: self.frame.drawn(), area, config, framing: settled };
        if !new_frame && self.shown == Some(shown) && self.presented_effects == Some(effects) {
            return;
        }
        self.shown = Some(shown);
        self.frame.prepare(device, queue, encoder, renderer, area, framing);
        self.presented_effects = Some(effects);
        if effects {
            let lighting = Lighting::for_scene(scene);
            self.effects.render(device, queue, encoder, renderer, &self.frame, self.frame.primitives(), &config.effects, config.quality, &lighting);
            // The interface the game drew after its scene, over the effects.
            let output = self.effects.output().cloned();
            self.frame.draw_overlay(device, queue, encoder, renderer, output.as_ref());
        } else {
            self.frame.draw_overlay(device, queue, encoder, renderer, None);
        }
        if framing.interface == Interface::Only {
            self.frame.pane(device, encoder);
        }
    }
}

/// Where first-person inspection looks from in the frame shown: the
/// character's eye, the world's axes (unit) and up in the game camera's
/// view space, the eye's height above the feet (view-space units), the
/// view's heading in the world and its pitch (radians), and the sprite left
/// out.
struct Base {
    eye: [f32; 3],
    world: [[f32; 3]; 3],
    up: [f32; 3],
    height: f32,
    heading: f32,
    pitch: f32,
    hide: u32,
}

impl Base {
    /// A direction of the world (y down) in the game camera's view space.
    fn to_view(&self, w: [f32; 3]) -> [f32; 3] {
        std::array::from_fn(|i| w[0] * self.world[0][i] + w[1] * self.world[1][i] + w[2] * self.world[2][i])
    }

    /// The view's right, down and forward, on its heading and `pitch` up.
    fn axes(&self, pitch: f32) -> [[f32; 3]; 3] {
        let yaw = self.heading;
        let forward = self.to_view([yaw.sin() * pitch.cos(), -pitch.sin(), yaw.cos() * pitch.cos()]);
        let right = self.to_view([yaw.cos(), 0.0, -yaw.sin()]);
        let down = [
            forward[1] * right[2] - forward[2] * right[1],
            forward[2] * right[0] - forward[0] * right[2],
            forward[0] * right[1] - forward[1] * right[0],
        ];
        [right, down, forward]
    }

    /// The near plane's distance: as far in front of the eye as a 32nd of
    /// its height.
    fn near(&self) -> f32 {
        (self.height / 32.0).max(1.0)
    }
}

impl GameView {
    pub fn new(device: &wgpu::Device, queue: &wgpu::Queue, adapter: Option<&wgpu::Adapter>) -> Self {
        Self {
            device: device.clone(),
            queue: queue.clone(),
            adapter: adapter.cloned(),
            renderer: Ps1Renderer::new(device, queue, 1),
            main: Target::new(device, adapter),
            eyes: Vec::new(),
            headset: false,
            config: None,
            pending: Vec::new(),
            restart: true,
            scene: String::new(),
            recorded_focus: None,
            focus: None,
            side: 0,
            frame_scenes: Vec::new(),
            welder: super::Welder::default(),
            movie: super::MovieRenderer::new(adapter.is_some_and(|adapter| adapter.get_info().device_type == wgpu::DeviceType::Cpu)),
            presented_movie: false,
            first_person: None,
            xr_view: XrView::Screen,
            diorama: Diorama::default(),
            room: false,
        }
    }

    /// Whether the view shows (or may soon show) the diorama of a scene
    /// with a camera: a field, a battle or the world map, not looking
    /// around in first person.
    fn diorama_scene(&self) -> bool {
        matches!(self.xr_view, XrView::Diorama(_))
            && self.first_person.is_none()
            && ["field", "battle", "worldmap"].contains(&self.scene.split(':').next().unwrap_or_default())
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
        } else if self.diorama_scene() {
            // A view from anywhere: every face, and the camera each frame
            // is drawn with.
            Some(Reveals { reach: [side as u16 + 32, 32], pull_back: 0, around: Reveals::ANYWHERE })
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
        } else if side > 0 || config.interpolation || (self.scene.starts_with("worldmap") && !config.draw.fidelity.vertex_snapping) {
            // The sides, and a little more for faces leaning into them, or
            // what a camera between two frames sees beside the screen.
            // Modern world terrain also needs sub-pixel slivers the
            // original's whole-pixel winding test discarded inside them.
            let margin = if config.interpolation { 32 } else { 16 };
            Some(Reveals { reach: [side as u16 + margin, margin], pull_back: 0, around: 0 })
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
        // Looking around, or a diorama of a field: the view may show it
        // from anywhere.
        if self.first_person.is_some() || (self.diorama_scene() && self.scene.starts_with("field")) {
            return 1000 | 1024 << 16;
        }
        let zoom = (config.zoom * 100.0).round().clamp(100.0, 1000.0) as u32;
        let side = config.side.max(self.side).min(1024);
        if zoom == 100 && side == 0 { 0 } else { zoom | side << 16 }
    }

    /// Turns first-person inspection on (looking along the field camera's
    /// heading, level, from the controlled character's eyes, once a frame
    /// shows where it stands) or off. It shows only in a field, begins only
    /// on a frame drawing the field's scene and ends when the field (or its
    /// map) is left. A headset's eyes begin anew with it: what they hold
    /// through frames without the field is a picture of this inspection's,
    /// never an earlier one's.
    pub fn set_first_person(&mut self, on: bool) {
        if on && self.first_person.is_none() {
            self.eyes.clear();
        }
        self.first_person = on.then(FirstPerson::default);
    }

    /// Whether first-person inspection is on.
    pub fn first_person(&self) -> bool {
        self.first_person.is_some()
    }

    /// The Binaural listener's view (`xem_core::binaural::ListenerView`)
    /// while it looks around: the first-person view's axes (right, down,
    /// forward) in the game camera's view space as the frame shown was drawn
    /// (the character's record it looks from), where a listener faces, and
    /// that camera's rotation rows, which the presenter hears them through.
    /// They never include a headset's head, which the session turns the
    /// listener by once more (`Session::set_head`): in a headset they are
    /// level on the view's heading (the head does the pitching), the base
    /// its eyes turn from. While the diorama shows, the seat's axes through
    /// the table and the camera of the frame it shows
    /// ([`super::DioramaSpace::listener_axes`]): the head turns from the
    /// seat, whose forward the table's heading and tilt take elsewhere in
    /// the game's world.
    pub fn listener_axes(&mut self) -> Option<([[f32; 3]; 3], [[f32; 3]; 3])> {
        if self.first_person.is_none() && let Some(space) = self.diorama.space().filter(|_| self.diorama_scene()) {
            return space.listener_axes();
        }
        let headset = self.headset;
        let base = self.base(headset)?;
        // The character's record the view looks from (`base` holds it).
        let eye = self.first_person.as_ref()?.held?;
        Some((base.axes(if headset { 0.0 } else { base.pitch }), super::diorama::camera_rows(&eye.axes)?))
    }

    /// The diorama's table takes the camera's point and heading at once at
    /// the next presentation, without easing there (a headset recentred, or
    /// its runtime moved the reference space).
    pub fn recenter_diorama(&mut self) {
        self.diorama.recenter();
    }

    /// The viewpoint the screen last presented showed the game's scene from,
    /// when it was not the game camera's own: the camera pulled back (wider
    /// views' zoom out in a field or the world map), or first-person
    /// inspection's look from the character's eye. Pointing at the picture
    /// goes through it, so that a pinch or a tap picks what the picture
    /// shows there and marks lie where it shows them (hosts see the field's
    /// camera through it: `xem_core::field::Camera::seen`). None for the
    /// game camera's own picture, a headset's pane (no scene), a frame
    /// whose world the port did not project, and while the host shows
    /// something else ([`GameView::away`]: the Original renderer's picture
    /// is the game camera's).
    pub fn viewpoint(&self) -> Option<Viewpoint> {
        let shown = self.main.shown?;
        let framing = shown.framing;
        if framing.interface == Interface::Only || (framing.pull_back <= 0.0 && framing.look.is_none()) {
            return None;
        }
        let ((x, y), h) = super::frame::projection(self.main.frame.newest())?;
        let centre = (x - shown.area.x as f32, y - shown.area.y as f32);
        Some(match framing.look {
            Some(look) => {
                let rows = look.axes;
                let offset = std::array::from_fn(|r| -(0..3).map(|k| rows[r][k] * look.eye[k]).sum::<f32>());
                Viewpoint { rows, offset, h: look.h[0], centre: (centre.0 + look.shift[0], centre.1 + look.shift[1]), near: look.near }
            }
            None => Viewpoint {
                rows: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                offset: [0.0, 0.0, framing.pull_back],
                h,
                centre,
                near: h / 2.0,
            },
        })
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

    /// Where first-person inspection looks from in the frame shown, when it
    /// shows where the controlled character stood (with `hold`, as the last
    /// frame that did when it does not): from its eyes (by its sprite's
    /// height above its feet), turned from the heading it began with.
    fn base(&mut self, hold: bool) -> Option<Base> {
        let shown = self.main.frame.eye();
        let view = self.first_person.as_mut()?;
        let eye = match (shown, view.held) {
            (Some(eye), _) => *view.held.insert(eye),
            (None, Some(held)) if hold => held,
            _ => return None,
        };
        let unit = |v: [f32; 3]| {
            let length = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
            (length > 0.0).then(|| v.map(|c| c / length))
        };
        let world = [unit(eye.axes[0])?, unit(eye.axes[1])?, unit(eye.axes[2])?];
        // The world's up in the game camera's view space (the world's y is down).
        let up: [f32; 3] = std::array::from_fn(|i| -world[1][i]);
        // The heading the game's camera looked along: its axis in the world.
        let heading = *view.heading.get_or_insert_with(|| world[0][2].atan2(world[2][2]));
        let height = match view.height {
            Some(height) => height,
            None => {
                // A sprite the frame does not draw stands as tall as the
                // field's characters do (the axes carry the field's scale).
                let scale = eye.axes[0].iter().map(|c| c * c).sum::<f32>().sqrt() / 4096.0;
                let top = sprite_height(self.main.frame.newest(), eye.sprite, eye.position, up).unwrap_or(SPRITE_HEIGHT * scale);
                *view.height.insert(top * EYE_LEVEL)
            }
        };
        Some(Base {
            eye: std::array::from_fn(|i| eye.position[i] + up[i] * height),
            world,
            up,
            height,
            heading: heading + view.yaw,
            pitch: view.pitch,
            hide: eye.sprite,
        })
    }

    /// The first-person view of the frame shown, when it shows where the
    /// controlled character stood.
    fn look(&mut self) -> Option<Look> {
        let base = self.base(false)?;
        Some(Look {
            eye: base.eye,
            axes: base.axes(base.pitch),
            up: base.up,
            h: [LOOK_H; 2],
            shift: [0.0; 2],
            near: base.near(),
            hide: base.hide,
            facing: None,
        })
    }

    /// Each headset eye's view from within the scene, fitted onto the frame
    /// of `area` (its wider framing's sides included): none unless
    /// first-person inspection is on and a frame has shown where the
    /// character stands (the last such frame's stands through frames that
    /// do not, rather than a cut to the game's camera). The session's
    /// reference space is laid on the view: its -z along the view's level
    /// heading, its y the world's up, its origin at the character's eye,
    /// [`EYE_HEIGHT_METRES`] to the eye's height above the feet; the
    /// sprites face the head between the eyes.
    fn eye_looks(&mut self, area: DisplayArea, eyes: &[HeadsetEye]) -> Vec<Look> {
        if eyes.is_empty() {
            return Vec::new();
        }
        let Some(base) = self.base(true) else { return Vec::new() };
        // Where the field's scene is projected about, from the display's
        // corner (each buffer the game draws in has its own corner): this
        // frame's, else the last frame's that drew it, else the middle.
        let corner = (area.x as f32, area.y as f32);
        let measured = projection_centre(self.main.frame.newest()).map(|(x, y)| (x - corner.0, y - corner.1));
        let Some(view) = self.first_person.as_mut() else { return Vec::new() };
        if let Some(measured) = measured.filter(|_| self.main.frame.eye().is_some()) {
            view.centre = Some(measured);
        }
        let (width, height) = ((area.width + 2 * self.side) as f32, area.height as f32);
        let centre = view.centre.unwrap_or((area.width as f32 / 2.0, height / 2.0));
        let [right, _, forward] = base.axes(0.0);
        let space = |v: [f32; 3]| -> [f32; 3] { std::array::from_fn(|i| v[0] * right[i] + v[1] * base.up[i] - v[2] * forward[i]) };
        let metre = base.height / EYE_HEIGHT_METRES;
        let at = |p: [f32; 3]| -> [f32; 3] {
            let d = space(p);
            std::array::from_fn(|i| base.eye[i] + d[i] * metre)
        };
        let n = eyes.len() as f32;
        let head = at(std::array::from_fn(|i| eyes.iter().map(|eye| eye.position[i]).sum::<f32>() / n));
        eyes.iter()
            .map(|eye| {
                let [l, r, u, d] = eye.fov;
                if r - l <= 1e-3 || u - d <= 1e-3 {
                    return None;
                }
                let q = glam::Quat::from_array(eye.orientation).normalize();
                let axis = |v: glam::Vec3| space((q * v).to_array());
                let (hx, hy) = (width / (r - l), height / (u - d));
                Some(Look {
                    eye: at(eye.position),
                    axes: [axis(glam::Vec3::X), axis(glam::Vec3::NEG_Y), axis(glam::Vec3::NEG_Z)],
                    up: base.up,
                    // The frustum's edges on the frame's: tangent l at its
                    // left edge (the sides' outer edge), r at its right, u
                    // at the top, d the bottom, whichever buffer it is in.
                    h: [hx, hy],
                    shift: [-(self.side as f32) - l * hx - centre.0, u * hy - centre.1],
                    near: base.near(),
                    hide: base.hide,
                    facing: Some(head),
                })
            })
            .collect::<Option<Vec<_>>>()
            .unwrap_or_default()
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
        self.draw_pending(encoder, display, scene, config);
        let flat = if let Some(output) = config.movies.filter(|_| self.movie.shows(display)) {
            self.present_movie(encoder, display, output)
        } else {
            self.presented_movie = false;
            let area = self.area(display)?;
            let new_frame = self.track_shown(area, scene, now);
            self.headset = false;
            self.present_frame(encoder, area, now, new_frame)
        };
        self.flat_view(flat?)
    }

    /// [`GameView::present`] for a headset whose `eyes` are as given: while
    /// first-person inspection looks from within the scene (once a frame
    /// has shown where the character stands), each eye's picture from the
    /// character's eye turned and moved as the eye is (`HeadsetEye`; its
    /// sprites face the head, the same for both eyes), and the game's
    /// interface alone as the screen's, for a pane where the screen was;
    /// otherwise the screen's picture alone, as `present` draws it. Called
    /// every headset frame (paused or not), so the eyes follow the head.
    pub fn present_headset(
        &mut self,
        encoder: &mut wgpu::CommandEncoder,
        display: &DisplayInfo,
        scene: &str,
        now: f64,
        eyes: &[HeadsetEye],
    ) -> Option<HeadsetPresented<'_>> {
        let config = self.config?;
        self.draw_pending(encoder, display, scene, config);
        // No diorama drawn this time: once drawn again (inspection over, the
        // view the diorama), its table eases on from where it stood.
        self.diorama.undrawn();
        if let Some(output) = config.movies.filter(|_| self.movie.shows(display)) {
            self.headset = false;
            let flat = self.present_movie(encoder, display, output)?;
            return Some(HeadsetPresented { eyes: Vec::new(), screen: self.flat_view(flat)? });
        }
        self.presented_movie = false;
        let area = self.area(display)?;
        let new_frame = self.track_shown(area, scene, now);
        let looks = self.eye_looks(area, eyes);
        self.headset = !looks.is_empty();
        if looks.is_empty() {
            let flat = self.present_frame(encoder, area, now, new_frame)?;
            return Some(HeadsetPresented { eyes: Vec::new(), screen: self.flat_view(flat)? });
        }
        // The interface on its pane, then the eyes without it. A frame that
        // does not draw the field (a field's menu has the screen: the port
        // records the character's place as the field draws its models, and
        // a frame may keep a record of the one before it) leaves the eyes
        // on the last that did, seen as the head now is (one they drew in
        // this inspection: they are made anew as it begins).
        self.show_screen(encoder, area, now, new_frame, None, Interface::Only);
        let hold = self.main.frame.eye().is_none() || projection_centre(self.main.frame.newest()).is_none();
        while self.eyes.len() < looks.len() {
            self.eyes.push(Target::new(&self.device, self.adapter.as_ref()));
        }
        for (target, look) in self.eyes.iter_mut().zip(&looks) {
            let framing = Framing {
                side: self.side,
                interpolate: config.interpolation.then_some(now).filter(|_| !hold),
                pull_back: 0.0,
                replay: config.effects.any(),
                look: Some(*look),
                interface: Interface::Omitted,
                hold,
            };
            target.show(&self.device, &self.queue, encoder, &mut self.renderer, area, config, framing, &self.scene, new_frame);
        }
        let eyes = self.eyes[..looks.len()].iter().map(Target::view).collect::<Option<Vec<_>>>()?;
        let screen = self.flat_view(Flat { picture: Picture::Pane, size: self.main.frame.size(), new_frame })?;
        Some(HeadsetPresented { eyes, screen })
    }

    /// The display area this view draws `display` as, if it draws it (15-bit
    /// and on, with all of VRAM).
    fn area(&self, display: &DisplayInfo) -> Option<DisplayArea> {
        (Self::shows(display) && !self.renderer.needs_full_vram())
            .then(|| DisplayArea { x: display.x.into(), y: display.y.into(), width: display.width, height: display.height })
    }

    /// The view of a flat presentation's picture.
    fn flat_view(&self, flat: Flat) -> Option<Presented<'_>> {
        let view = match flat.picture {
            Picture::Movie => self.movie.output_view()?,
            Picture::Screen => self.main.view()?,
            Picture::Pane => self.main.frame.pane_view()?,
        };
        Some(Presented { view, size: flat.size, new_frame: flat.new_frame, movie: flat.picture == Picture::Movie })
    }

    /// A movie frame the MDEC decoded, rebuilt (the Enhanced setting) to fit
    /// `output`.
    fn present_movie(&mut self, encoder: &mut wgpu::CommandEncoder, display: &DisplayInfo, output: (u32, u32)) -> Option<Flat> {
        self.presented_movie = true;
        self.main.shown = None;
        let (_, size, new_frame) = self.movie.present(&self.device, &self.queue, encoder, display, output, &self.renderer.vram)?;
        Some(Flat { picture: Picture::Movie, size, new_frame })
    }

    /// Draws the pending trace into the renderer's picture of VRAM, the
    /// frames it begins tagged with the scene drawing them.
    fn draw_pending(&mut self, encoder: &mut wgpu::CommandEncoder, display: &DisplayInfo, scene: &str, config: ViewConfig) {
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
        // (a frame's, or with a headset's eyes the first eye's, what the
        // player looked at; after a movie frame, the native pixels it took).
        let looked = match self.headset {
            true => self.eyes.first().and_then(Target::texture),
            false => self.presented_texture(),
        };
        let frame = looked.filter(|_| !self.presented_movie).cloned();
        let presented = frame.zip(self.main.frame.area()).map(|(texture, (_, scale))| {
            super::PresentedPicture { texture, side: self.main.frame.side(), scale }
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
    }

    /// Notes the frame `area` shows and the scene that drew it, at `now`
    /// (the view's field, the game's frame arrivals, the camera's focus and
    /// the wider framing's sides); returns whether it is a new game frame.
    fn track_shown(&mut self, area: DisplayArea, scene: &str, now: f64) -> bool {
        let new_frame = self.main.frame.track(&self.renderer, area, Some(now));
        // The scene that drew the frame shown frames it: the field's last
        // frame stays pulled back while a battle loads behind it (and its
        // shatter takes it so), a battle's last one stays as the battle drew
        // it while the field loads.
        let shown = self.main.frame.identity().and_then(|identity| self.frame_scenes.iter().find(|(known, _)| *known == identity));
        let shown = shown.map_or(scene, |(_, drawn)| drawn.as_str());
        if shown != self.scene {
            self.scene = shown.to_string();
            self.recorded_focus = None;
            self.focus = None;
        }
        // First-person inspection is the field's it began in, and begins
        // only where a frame shown with the field camera's record made
        // looking around (the port records it so from the frame after the
        // view asks, as the field draws its models) draws the field's
        // scene: a field whose camera shows
        // none of it (the title screen's: its logo over a map the game culls
        // whole) has nothing to look around in, which the second new frame
        // of the record alone tells. The first proves nothing: a trace
        // started again mid-frame (as the Original renderer ends) logs the
        // rest of a frame whose scene came before it, and a busy host shows
        // that frame long enough for a toggle to meet it.
        if let Some(view) = &mut self.first_person {
            match &view.field {
                _ if !self.scene.starts_with("field") => self.first_person = None,
                Some(field) if *field != self.scene => self.first_person = None,
                Some(_) => {}
                None if self.main.frame.camera().is_none_or(|camera| camera.around == 0) => {}
                None if draws_scene(self.main.frame.newest()) => view.field = Some(self.scene.clone()),
                None => {
                    view.sceneless += u8::from(new_frame);
                    if view.sceneless >= 2 {
                        self.first_person = None;
                    }
                }
            }
        }
        if new_frame {
            self.update_focus(area);
        }
        // The sides widen this display at its own height to the output's shape.
        self.side = self.config.and_then(|config| config.aspect).map_or(0, |aspect| display_side((area.width, area.height), aspect));
        new_frame
    }

    /// The frame `area` shows (tracked: [`GameView::track_shown`]) as the
    /// screen: from the game's camera, or from the character's eye while
    /// first-person inspection looks around, with the effects.
    fn present_frame(&mut self, encoder: &mut wgpu::CommandEncoder, area: DisplayArea, now: f64, new_frame: bool) -> Option<Flat> {
        let look = self.look();
        self.show_screen(encoder, area, now, new_frame, look, Interface::Drawn);
        Some(Flat { picture: Picture::Screen, size: self.main.frame.size(), new_frame })
    }

    /// Draws the screen's picture of `area`: from the game's camera (pulled
    /// back as the settings say) or from `look`, its interface as
    /// `interface` says (alone: the interface's pane).
    fn show_screen(&mut self, encoder: &mut wgpu::CommandEncoder, area: DisplayArea, now: f64, new_frame: bool, look: Option<Look>, interface: Interface) {
        let Some(config) = self.config else { return };
        let alone = interface == Interface::Only;
        let zoom = scene_zoom(config.zoom, &self.scene);
        let pull_back = if zoom > 1.0 && look.is_none() && !alone { self.focus.map_or(0.0, |focus| (zoom - 1.0) * focus) } else { 0.0 };
        // The interface moves as the game moves it: nothing to interpolate.
        let interpolate = (config.interpolation && !alone).then_some(now);
        let framing = Framing { side: self.side, interpolate, pull_back, replay: config.effects.any(), look, interface, hold: false };
        self.main.show(&self.device, &self.queue, encoder, &mut self.renderer, area, config, framing, &self.scene, new_frame);
    }

    /// Shows the game in a headset as `view`: the flat screen
    /// ([`GameView::present`]) or the diorama
    /// ([`GameView::present_diorama`]; the screen until it can show).
    pub fn set_xr_view(&mut self, view: XrView) {
        if view == XrView::Screen && self.xr_view != XrView::Screen {
            self.diorama.reset();
        }
        self.xr_view = view;
    }

    pub fn xr_view(&self) -> XrView {
        self.xr_view
    }

    /// Whether the room shows around a headset's frames (passthrough,
    /// immersive AR; hosts say it every frame): the diorama's wall then
    /// leaves out the game's clear, which the room stands in for
    /// (docs/runtime.md, The room).
    pub fn set_room(&mut self, room: bool) {
        self.room = room;
    }

    /// Presents for a headset (docs/runtime.md, Diorama): draws the pending
    /// trace and, while the view is the diorama and the frame shown can be
    /// one, the diorama for each of `eyes` (from the room through its view
    /// and projection) into a picture of its own, the table placed from
    /// `seat` (its pose in the room, turned about its up only). Otherwise,
    /// and easing between the two (the last diorama fading out as the
    /// screen eases in, and back), the flat frame `present` gives, for the
    /// host to show on its screen. `None` while there is nothing to show.
    pub fn present_diorama(
        &mut self,
        encoder: &mut wgpu::CommandEncoder,
        display: &DisplayInfo,
        scene: &str,
        now: f64,
        eyes: &[DioramaEye],
        seat: glam::Mat4,
    ) -> Option<DioramaPresented<'_>> {
        let config = self.config?;
        self.draw_pending(encoder, display, scene, config);
        // Not a headset's eyes looking from within the scene.
        self.headset = false;
        let movie = config.movies.filter(|_| self.movie.shows(display));
        let area = if movie.is_some() { None } else { self.area(display) };
        let new_frame = area.is_some_and(|area| self.track_shown(area, scene, now));
        let table = match self.xr_view {
            XrView::Diorama(table) => Some(table),
            XrView::Screen => None,
        };
        // A movie, a display this view does not draw, and looking around in
        // first person show the screen.
        let fallback = match area.filter(|_| self.first_person.is_none()) {
            Some(area) => Diorama::fallback(&self.renderer, &self.main.frame, &self.scene, area),
            None => Some(Fallback::Screen),
        };
        let (screen, build) = self.diorama.ease(fallback, new_frame, now);
        let opacity = match table {
            Some(table) if screen < 1.0 => {
                // The frame shown when it can be a diorama, else the last
                // build, held or fading out.
                let shown = area.filter(|_| build).map(|area| (&self.main.frame, area, self.scene.as_str(), new_frame));
                self.diorama.present(&self.device, &self.queue, encoder, &mut self.renderer, shown, now, eyes, seat, &table, 1.0 - screen, self.room)
            }
            _ => None,
        };
        let drew = opacity.is_some();
        if !drew {
            self.diorama.hide();
        }
        let screen = if drew { screen } else { 1.0 };
        let flat = if screen > 0.0 {
            match (movie, area) {
                (Some(output), _) => self.present_movie(encoder, display, output),
                (None, Some(area)) => {
                    self.presented_movie = false;
                    self.present_frame(encoder, area, now, new_frame)
                }
                (None, None) => None,
            }
        } else {
            // No flat picture this time: a screen the game takes next is
            // not the one presented before.
            (self.presented_movie, self.main.presented_effects, self.main.shown) = (false, None, None);
            None
        };
        if !drew && flat.is_none() {
            return None;
        }
        Some(DioramaPresented {
            screen,
            frame: flat.and_then(|flat| self.flat_view(flat)),
            eyes: if drew { self.diorama.eyes() } else { Vec::new() },
            opacity: opacity.unwrap_or(0.0),
            pane: if drew { self.diorama.pane() } else { None },
            space: self.diorama.space(),
            new_frame,
            effects_unavailable: drew && config.effects.any(),
        })
    }

    /// The host shows something other than this view's presentations (the
    /// device's own picture: the Original renderer, a 24-bit movie frame
    /// the view does not draw): nothing the view presented is on show until
    /// it presents again. Pointing goes through the game's camera (no
    /// [`GameView::viewpoint`]), a screen the game takes next is not a
    /// picture it presented, and the diorama gives way to the screen at
    /// once (and eases in again from it), lying nowhere (no space, no
    /// listener's axes).
    pub fn away(&mut self) {
        (self.presented_movie, self.headset, self.main.presented_effects, self.main.shown) = (false, false, None, None);
        self.diorama.away();
    }

    /// The diorama the last [`GameView::present_diorama`] drew: where the
    /// game's world and the table lie in the room.
    pub fn diorama_space(&self) -> Option<super::DioramaSpace> {
        self.diorama.space()
    }

    /// What the last diorama build drew.
    pub fn diorama_stats(&self) -> super::diorama::DioramaStats {
        self.diorama.stats()
    }

    /// The auxiliary picture of diorama eye `eye` the last
    /// [`GameView::present_diorama`] drew, while it drew the diorama: each
    /// pixel's class (blue: `AUX_WORLD`, `AUX_MODEL` and `AUX_SPRITE` where
    /// the scene is drawn; `AUX_BACKGROUND` where nothing is, the room around
    /// the table; `AUX_INTERFACE` or `AUX_BACKGROUND` on the backdrop's
    /// wall). For checks of what the eyes see (tests, captures).
    pub fn diorama_aux(&self, eye: usize) -> Option<&wgpu::Texture> {
        self.diorama.space().and(self.diorama.eye_aux(eye)).map(wgpu::TextureView::texture)
    }

    /// The depth a pulled-back camera keeps its zoom at: the field camera's
    /// target where the port records it, else the depth in the middle of
    /// the screen, eased between frames (and taken at once after a cut).
    fn update_focus(&mut self, area: DisplayArea) {
        if let Some(recorded) = self.recorded_focus.filter(|f| *f > 0.0) {
            self.focus = Some(recorded);
            return;
        }
        let Some(measured) = central_depth(self.main.frame.newest(), area) else { return };
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

    /// Whether the last presentation (`present`, `present_headset` or
    /// `present_diorama`) showed a movie frame rebuilt from what the MDEC
    /// decoded (the Enhanced setting): not when it drew one of the
    /// renderer's frames, or nothing (the host then shows the device's own
    /// picture). [`Presented::movie`] says it of each presentation.
    pub fn presented_movie(&self) -> bool {
        self.presented_movie
    }

    /// Enlarges rebuilt movie frames bilinearly, as on a GPU that rasterizes
    /// in software (the adapter's kind chooses by default), or by FSR 1
    /// ([`super::MovieRenderer::set_software`]).
    pub fn set_software_movies(&mut self, software: bool) {
        self.movie.set_software(software);
    }

    /// Whether this view draws `display` (15-bit and on); 24-bit movie
    /// frames are the device's own picture unless [`GameView::shows_movie`].
    pub fn shows(display: &DisplayInfo) -> bool {
        display.enabled && !display.color24 && display.width > 0 && display.height > 0
    }

    /// The frame the last [`GameView::present`] drew (its colour and
    /// auxiliary textures), before the effects.
    pub fn frame(&self) -> &FrameRenderer {
        &self.main.frame
    }

    /// The texture the last [`GameView::present`] (or the screen's of
    /// [`GameView::present_headset`]) returned a view of.
    pub fn presented_texture(&self) -> Option<&wgpu::Texture> {
        if self.presented_movie {
            return self.movie.output();
        }
        if self.headset {
            return self.main.frame.pane_texture();
        }
        self.main.texture()
    }

    /// The picture of headset eye `eye` the last
    /// [`GameView::present_headset`] drew, while it drew the eyes.
    pub fn eye_texture(&self, eye: usize) -> Option<&wgpu::Texture> {
        self.eyes.get(eye).filter(|_| self.headset)?.texture()
    }

    /// Where headset eye `eye` looked from within the scene at the last
    /// [`GameView::present_headset`] that drew the eyes (its [`Look`]), and
    /// its auxiliary picture: each pixel's class (blue: `AUX_WORLD`,
    /// `AUX_MODEL` and `AUX_SPRITE` where the scene is drawn,
    /// `AUX_BACKGROUND` where nothing is or the game's clear). For checks of
    /// what the eyes see (tests).
    pub fn eye_look(&self, eye: usize) -> Option<(Look, &wgpu::Texture)> {
        let target = self.eyes.get(eye).filter(|_| self.headset)?;
        Some((target.shown?.framing.look?, target.frame.aux()))
    }

    /// The look the screen's picture was last drawn with (first-person
    /// inspection on a flat host: [`GameView::present`]), none while it
    /// shows the game's camera or a headset's pane. For checks of what the
    /// picture shows (tests), with [`GameView::frame`]'s classes.
    pub fn screen_look(&self) -> Option<Look> {
        self.main.shown.filter(|_| !self.headset)?.framing.look
    }

    /// What the renderer drew and dropped at the last presentation.
    pub fn stats(&self) -> super::DrawStats {
        self.renderer.stats()
    }

    pub fn renderer(&self) -> &Ps1Renderer {
        &self.renderer
    }

    pub fn effects_mut(&mut self) -> &mut EffectsRenderer {
        &mut self.main.effects
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

#[cfg(test)]
mod tests {
    use super::*;
    use xem_settings::MovieFilter;

    /// Every kind of host rebuilds movie frames into the output it gives
    /// under Enhanced, and none under Original.
    #[test]
    fn movies_are_rebuilt_under_enhanced_alone_on_every_host() {
        for host in [HostClass::Desktop, HostClass::Mobile, HostClass::Browser, HostClass::Headset] {
            for (movies, rebuilt) in [(MovieFilter::Enhanced, Some((1280, 720))), (MovieFilter::Original, None)] {
                let mut graphics = Graphics::default();
                graphics.filtering.movies = movies;
                assert_eq!(ViewConfig::resolve(&graphics, host, (1280, 720), 8).movies, rebuilt, "{host:?}, {movies:?}");
            }
        }
    }

    /// A screen 3 m wide 2.5 m ahead spans 0.6 of an eye 90 degrees wide
    /// (its tangents -1 to 1); one three times as wide, more than the eye.
    #[test]
    fn a_headsets_movie_frames_are_made_as_wide_as_the_screen_spans_in_an_eye() {
        let fov = [-1.0, 1.0, 1.0, -1.0];
        assert_eq!(headset_movie_output(3.0, 2.5, 1000, fov), (600, 600));
        assert_eq!(headset_movie_output(9.0, 2.5, 1000, fov), (1000, 1000), "at most the eye's width");
        assert_eq!(headset_movie_output(3.0, 2.5, 1000, [-1.2, 0.8, 1.0, -1.0]), (600, 600), "an eye's own span");
    }
}
