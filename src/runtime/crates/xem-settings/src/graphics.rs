//! Graphics settings: the presentation profile, the original's limits each
//! selectable on its own, the HD-2D-inspired effects, quality tiers, framing,
//! filtering, culling and the headset screen. None of them reaches the game:
//! they choose how the recorded frame is drawn (docs/runtime.md, Presentation).

use serde::{Deserialize, Serialize};

/// The presentation profile. Modern is the default; choosing HD-2D or the
/// PS1 look is always explicit, and no tier or device replaces the choice.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum GraphicsProfile {
    /// High resolution, perspective-correct, sub-pixel and full colour,
    /// keeping the original drawing order. No effects.
    #[default]
    Modern,
    /// Modern plus the HD-2D-inspired effects at [`Graphics::hd2d`]'s levels.
    Hd2d,
    /// Every limit of the original GPU at its native resolution.
    Ps1,
    /// [`Graphics::custom_fidelity`] and [`Graphics::custom_effects`].
    Custom,
}

/// Each of the original GPU's limits, independently.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct Fidelity {
    pub affine_textures: bool,
    pub vertex_snapping: bool,
    pub color_15bit: bool,
    pub dithering: bool,
    pub native_resolution: bool,
    /// The ordering table's painter's order (off: depth-tested, which can
    /// break the original's intentional masks).
    pub painter_ordering: bool,
}

impl Fidelity {
    pub const MODERN: Fidelity = Fidelity {
        affine_textures: false,
        vertex_snapping: false,
        color_15bit: false,
        dithering: false,
        native_resolution: false,
        painter_ordering: true,
    };
    pub const PS1: Fidelity = Fidelity {
        affine_textures: true,
        vertex_snapping: true,
        color_15bit: true,
        dithering: true,
        native_resolution: true,
        painter_ordering: true,
    };
}

impl Default for Fidelity {
    fn default() -> Self {
        Fidelity::MODERN
    }
}

/// The HD-2D-inspired effects as percentages (0 off, 100 full), and the
/// focus shift (-100 near to 100 far).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct EffectLevels {
    pub lighting: u8,
    pub shadows: u8,
    pub ambient_occlusion: u8,
    pub fog: u8,
    pub light_shafts: u8,
    pub bloom: u8,
    pub color_grading: u8,
    pub depth_of_field: u8,
    pub focus: i8,
}

impl EffectLevels {
    pub const OFF: EffectLevels = EffectLevels {
        lighting: 0,
        shadows: 0,
        ambient_occlusion: 0,
        fog: 0,
        light_shafts: 0,
        bloom: 0,
        color_grading: 0,
        depth_of_field: 0,
        focus: 0,
    };
    pub const HD2D: EffectLevels = EffectLevels {
        lighting: 60,
        shadows: 80,
        ambient_occlusion: 60,
        fog: 50,
        light_shafts: 40,
        bloom: 50,
        color_grading: 70,
        depth_of_field: 60,
        focus: 0,
    };

    /// The levels by name, for interfaces and agents.
    pub const NAMES: [&'static str; 9] =
        ["lighting", "shadows", "ambient_occlusion", "fog", "light_shafts", "bloom", "color_grading", "depth_of_field", "focus"];

    pub fn get(&self, name: &str) -> Option<i32> {
        Some(match name {
            "lighting" => self.lighting.into(),
            "shadows" => self.shadows.into(),
            "ambient_occlusion" => self.ambient_occlusion.into(),
            "fog" => self.fog.into(),
            "light_shafts" => self.light_shafts.into(),
            "bloom" => self.bloom.into(),
            "color_grading" => self.color_grading.into(),
            "depth_of_field" => self.depth_of_field.into(),
            "focus" => self.focus.into(),
            _ => return None,
        })
    }

    /// Sets a level by name; out-of-range values are rejected.
    pub fn set(&mut self, name: &str, value: i32) -> Result<(), String> {
        if name == "focus" {
            self.focus = i8::try_from(value).ok().filter(|v| (-100..=100).contains(v)).ok_or("focus must be -100 to 100")?;
            return Ok(());
        }
        let level = u8::try_from(value).ok().filter(|v| *v <= 100).ok_or_else(|| format!("{name} must be 0 to 100"))?;
        match name {
            "lighting" => self.lighting = level,
            "shadows" => self.shadows = level,
            "ambient_occlusion" => self.ambient_occlusion = level,
            "fog" => self.fog = level,
            "light_shafts" => self.light_shafts = level,
            "bloom" => self.bloom = level,
            "color_grading" => self.color_grading = level,
            "depth_of_field" => self.depth_of_field = level,
            _ => return Err(format!("no effect named {name}")),
        }
        Ok(())
    }

    fn validate(&self) -> Result<(), String> {
        for name in Self::NAMES {
            let value = self.get(name).expect("named");
            let range = if name == "focus" { -100..=100 } else { 0..=100 };
            if !range.contains(&value) {
                return Err(format!("{name} is out of range"));
            }
        }
        Ok(())
    }
}

impl Default for EffectLevels {
    fn default() -> Self {
        EffectLevels::HD2D
    }
}

/// Explicit cost tiers for the effects; Auto picks the host's (desktop
/// High, mobile and browser Medium, headset Low). A tier changes sample
/// counts only.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum QualityTier {
    #[default]
    Auto,
    Low,
    Medium,
    High,
    Ultra,
}

/// What draws the game.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum Renderer {
    /// The console's own picture (the software GPU's display) scaled to the
    /// window, as the original shows it; none of the renderer's settings
    /// apply and no draw trace is kept.
    Original,
    /// Ex Machina's renderer: the game's drawing at a higher resolution,
    /// with its profiles, wider framing, the camera pulled back and effects.
    #[default]
    ExMachina,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct TextureFiltering {
    pub world: crate::Filter,
    pub sprites: crate::Filter,
    pub interface: crate::Filter,
    /// Movie frames (the MDEC's 24-bit pictures).
    pub movies: MovieFilter,
}

/// How Ex Machina's renderer shows movie frames.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum MovieFilter {
    /// Rebuilt from what the MDEC decoded: its block seams smoothed where
    /// the compression made them, its colour (stored at half resolution)
    /// following the brightness's edges, and enlarged edge-adaptively and
    /// sharpened (FSR 1).
    #[default]
    Enhanced,
    /// The decoder's output as it is, each pixel a block.
    Original,
}

/// Optional culling; off by default. The game's own culling, clipping,
/// hiding and on-screen tests are game logic and are not settings
/// (xem_render::ps1::Culling).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct Culling {
    /// Viewport culling as the game does it: a wider or pulled-back view
    /// shows only what the game drew for its original screen. Off, it also
    /// shows the scenery the game culled against that screen.
    pub viewport: bool,
    /// Drop projected back faces (also the game's intended double-sided ones).
    pub back_faces: bool,
}

/// What a headset shows of the game (docs/runtime.md, Diorama).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum HeadsetView {
    /// A flat virtual screen in front of the seated player (the default).
    #[default]
    Screen,
    /// The scene as a miniature on a table in front of the seated player,
    /// each eye seeing it from where it is; the game's interface on a pane
    /// above the table's far edge. Movies, menus and other moments without
    /// a scene to stand on the table show the screen.
    Diorama,
}

/// How the diorama's table turns with the game's camera (fields and the
/// world map; a battle's table keeps the heading it began with).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum DioramaTurns {
    /// Eased towards the camera's heading.
    #[default]
    Smooth,
    /// In 30-degree steps, dimmed briefly at each.
    Snap,
    /// The heading the scene began with.
    Fixed,
}

/// The game in a headset: a flat virtual screen in front of a seated
/// player, or the seated diorama.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct XrScreen {
    /// Distance from the head at entry, centimetres.
    pub distance_cm: u16,
    /// Screen width, centimetres.
    pub width_cm: u16,
    /// Depth of field in a headset (off by default: it fights the eyes' own focus).
    pub depth_of_field: bool,
    /// The screen or the diorama.
    pub view: HeadsetView,
    /// The diorama's size, percent ([`DIORAMA_SCALE`]; 100: the original
    /// screen's width at the camera's target spans the table).
    pub diorama_scale: u16,
    /// The table top's height above the seated head, its distance ahead of
    /// it (centimetres) and its tilt (degrees, raising its far edge).
    pub diorama_height_cm: i16,
    pub diorama_distance_cm: u16,
    pub diorama_tilt_deg: u8,
    pub diorama_turns: DioramaTurns,
    /// The diorama over the room around the player, where the OpenXR runtime
    /// offers passthrough (the ALPHA_BLEND blend mode or XR_FB_passthrough;
    /// off by default). The browser takes no part in it: there the room
    /// shows in an immersive AR session (Enter AR).
    pub passthrough: bool,
}

/// The diorama's scale, height, distance and tilt within their bounds.
pub const DIORAMA_SCALE: std::ops::RangeInclusive<u16> = 25..=400;
pub const DIORAMA_HEIGHT_CM: std::ops::RangeInclusive<i16> = -100..=30;
pub const DIORAMA_DISTANCE_CM: std::ops::RangeInclusive<u16> = 30..=200;
pub const DIORAMA_TILT_DEG: std::ops::RangeInclusive<u8> = 0..=45;

impl Default for XrScreen {
    fn default() -> Self {
        XrScreen {
            distance_cm: 250,
            width_cm: 300,
            depth_of_field: false,
            view: HeadsetView::Screen,
            diorama_scale: 100,
            diorama_height_cm: -45,
            diorama_distance_cm: 60,
            diorama_tilt_deg: 0,
            diorama_turns: DioramaTurns::Smooth,
            passthrough: false,
        }
    }
}

impl XrScreen {
    /// The diorama's size, placement and turns as they are by default (the
    /// panel's Reset diorama); the view, the screen and passthrough stay.
    pub fn diorama_reset(&self) -> XrScreen {
        let defaults = XrScreen::default();
        XrScreen {
            diorama_scale: defaults.diorama_scale,
            diorama_height_cm: defaults.diorama_height_cm,
            diorama_distance_cm: defaults.diorama_distance_cm,
            diorama_tilt_deg: defaults.diorama_tilt_deg,
            diorama_turns: defaults.diorama_turns,
            ..*self
        }
    }

    fn validate(&self) -> Result<(), String> {
        if !(50..=2000).contains(&self.distance_cm) || !(50..=2000).contains(&self.width_cm) {
            return Err("headset screen distance and width must be 50-2000 cm".into());
        }
        if !DIORAMA_SCALE.contains(&self.diorama_scale) {
            return Err(format!("diorama scale must be {}-{} percent", DIORAMA_SCALE.start(), DIORAMA_SCALE.end()));
        }
        if !DIORAMA_HEIGHT_CM.contains(&self.diorama_height_cm) {
            return Err(format!("diorama height must be {} to {} cm", DIORAMA_HEIGHT_CM.start(), DIORAMA_HEIGHT_CM.end()));
        }
        if !DIORAMA_DISTANCE_CM.contains(&self.diorama_distance_cm) {
            return Err(format!("diorama distance must be {}-{} cm", DIORAMA_DISTANCE_CM.start(), DIORAMA_DISTANCE_CM.end()));
        }
        if !DIORAMA_TILT_DEG.contains(&self.diorama_tilt_deg) {
            return Err(format!("diorama tilt must be {}-{} degrees", DIORAMA_TILT_DEG.start(), DIORAMA_TILT_DEG.end()));
        }
        Ok(())
    }
}

pub const MAX_INTERNAL_SCALE: u8 = 8;
/// The farthest the field camera can be pulled back, in percent (5x).
pub const MAX_ZOOM_OUT: u16 = 500;
/// A mouse wheel notch moves the zoom by this much (percent).
pub const ZOOM_STEP: u16 = 25;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct Graphics {
    /// What draws the game; the rest applies to Ex Machina's renderer.
    pub renderer: Renderer,
    pub profile: GraphicsProfile,
    /// The HD-2D profile's effect levels.
    pub hd2d: EffectLevels,
    pub custom_fidelity: Fidelity,
    pub custom_effects: EffectLevels,
    pub quality: QualityTier,
    /// Internal resolution as a multiple of the original's; 0 follows the
    /// output size. The PS1 profile and native resolution draw at 1.
    pub internal_scale: u8,
    /// Interpolate presented frames between the game's frames (one game
    /// frame of latency; matched primitives move smoothly), presenting
    /// every display refresh; off, only the game's frames are presented
    /// ([`Graphics::game_paced`]).
    pub interpolation: bool,
    /// Wider framing than the original 4:3 screen, to the output's aspect
    /// ratio: the sides show the scene beyond the original screen.
    pub widescreen: bool,
    /// The field camera pulled back along its own axis, without turning:
    /// 100 (the original, default) to [`MAX_ZOOM_OUT`] percent, the subject
    /// appearing that many times smaller. The world map takes a tenth of
    /// it. Independent of the aspect ratio.
    pub zoom_out: u16,
    /// Field characters keep moving wherever the zoomed-out or wider view
    /// shows them; the original stops one just beyond its own screen. This
    /// changes the game (it is recorded with the inputs); it does nothing
    /// while the view is the original's.
    pub actors_in_view: bool,
    pub filtering: TextureFiltering,
    pub culling: Culling,
    pub xr: XrScreen,
}

impl Default for Graphics {
    fn default() -> Self {
        Graphics {
            renderer: Renderer::ExMachina,
            profile: GraphicsProfile::Modern,
            hd2d: EffectLevels::HD2D,
            custom_fidelity: Fidelity::MODERN,
            custom_effects: EffectLevels::OFF,
            quality: QualityTier::Auto,
            internal_scale: 0,
            interpolation: false,
            widescreen: false,
            zoom_out: 100,
            actors_in_view: true,
            filtering: TextureFiltering::default(),
            culling: Culling::default(),
            xr: XrScreen::default(),
        }
    }
}

impl Graphics {
    /// The settings `notches` wheel notches further out (negative: back in),
    /// within 100 to [`MAX_ZOOM_OUT`] percent.
    pub fn zoomed(&self, notches: i32) -> Graphics {
        let zoom = i32::from(self.zoom_out) + notches * i32::from(ZOOM_STEP);
        Graphics { zoom_out: zoom.clamp(100, i32::from(MAX_ZOOM_OUT)) as u16, ..*self }
    }

    /// Whether a frame is presented only when the game drew one: Ex
    /// Machina's renderer without interpolation, where a display refresh in
    /// between would show the same picture again. The game always runs at
    /// its own 60 frames a second; presentation never changes its timing.
    pub fn game_paced(&self) -> bool {
        self.renderer == Renderer::ExMachina && !self.interpolation
    }

    /// The limits the selected profile draws with.
    pub fn fidelity(&self) -> Fidelity {
        match self.profile {
            GraphicsProfile::Modern | GraphicsProfile::Hd2d => Fidelity::MODERN,
            GraphicsProfile::Ps1 => Fidelity::PS1,
            GraphicsProfile::Custom => self.custom_fidelity,
        }
    }

    /// The effects the selected profile draws with.
    pub fn effects(&self) -> EffectLevels {
        match self.profile {
            GraphicsProfile::Modern | GraphicsProfile::Ps1 => EffectLevels::OFF,
            GraphicsProfile::Hd2d => self.hd2d,
            GraphicsProfile::Custom => self.custom_effects,
        }
    }

    /// Whether a headset draws any of the profile's effects on its screen
    /// (depth of field only where [`XrScreen::depth_of_field`] asks for it):
    /// the diorama shows them as unavailable.
    pub fn headset_effects(&self) -> bool {
        let e = self.effects();
        [e.lighting, e.shadows, e.ambient_occlusion, e.fog, e.light_shafts, e.bloom, e.color_grading].iter().any(|level| *level > 0)
            || (self.xr.depth_of_field && e.depth_of_field > 0)
    }

    pub(crate) fn validate(&self) -> Result<(), String> {
        self.hd2d.validate()?;
        self.custom_effects.validate()?;
        if self.internal_scale > MAX_INTERNAL_SCALE {
            return Err(format!("internal scale must be 0 (auto) to {MAX_INTERNAL_SCALE}"));
        }
        if !(100..=MAX_ZOOM_OUT).contains(&self.zoom_out) {
            return Err(format!("zoom out must be 100 to {MAX_ZOOM_OUT} percent"));
        }
        self.xr.validate()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn profiles_resolve_without_changing_the_default() {
        let graphics = Graphics::default();
        assert_eq!(graphics.profile, GraphicsProfile::Modern);
        assert_eq!(graphics.effects(), EffectLevels::OFF);
        assert_eq!(graphics.fidelity(), Fidelity::MODERN);
        let hd2d = Graphics { profile: GraphicsProfile::Hd2d, ..graphics };
        assert_eq!(hd2d.effects(), EffectLevels::HD2D);
        assert_eq!(hd2d.fidelity(), Fidelity::MODERN);
        let ps1 = Graphics { profile: GraphicsProfile::Ps1, ..graphics };
        assert_eq!(ps1.fidelity(), Fidelity::PS1);
        assert_eq!(ps1.effects(), EffectLevels::OFF);
        let custom = Graphics {
            profile: GraphicsProfile::Custom,
            custom_fidelity: Fidelity { dithering: true, ..Fidelity::MODERN },
            custom_effects: EffectLevels { bloom: 30, ..EffectLevels::OFF },
            ..graphics
        };
        assert!(custom.fidelity().dithering && !custom.fidelity().affine_textures);
        assert_eq!(custom.effects().bloom, 30);
    }

    #[test]
    fn the_renderer_is_ex_machina_unless_the_original_is_chosen() {
        assert_eq!(Graphics::default().renderer, Renderer::ExMachina);
        // Settings saved before the choice existed keep Ex Machina's renderer.
        let older: Graphics = serde_json::from_str(r#"{"profile":"Hd2d","zoom_out":300}"#).unwrap();
        assert_eq!((older.renderer, older.profile, older.zoom_out), (Renderer::ExMachina, GraphicsProfile::Hd2d, 300));
        let original = Graphics { renderer: Renderer::Original, ..Graphics::default() };
        let text = serde_json::to_string(&original).unwrap();
        assert!(text.contains(r#""renderer":"Original""#), "{text}");
        assert_eq!(serde_json::from_str::<Graphics>(&text).unwrap(), original);
    }

    #[test]
    fn movies_are_enhanced_unless_original_is_chosen() {
        assert_eq!(Graphics::default().filtering.movies, MovieFilter::Enhanced);
        let older: Graphics = serde_json::from_str(r#"{"filtering":{"world":"Linear"}}"#).unwrap();
        assert_eq!((older.filtering.world, older.filtering.movies), (crate::Filter::Linear, MovieFilter::Enhanced));
        let original = Graphics { filtering: TextureFiltering { movies: MovieFilter::Original, ..Default::default() }, ..Graphics::default() };
        assert_eq!(serde_json::from_str::<Graphics>(&serde_json::to_string(&original).unwrap()).unwrap(), original);
    }

    #[test]
    fn characters_follow_the_view_unless_turned_off() {
        assert!(Graphics::default().actors_in_view);
        let older: Graphics = serde_json::from_str(r#"{"zoom_out":300}"#).unwrap();
        assert!(older.actors_in_view, "settings saved before the choice existed follow the view");
        let off = Graphics { actors_in_view: false, ..Graphics::default() };
        assert_eq!(serde_json::from_str::<Graphics>(&serde_json::to_string(&off).unwrap()).unwrap(), off);
    }

    #[test]
    fn zoom_out_is_bounded_and_off_by_default() {
        let graphics = Graphics::default();
        assert_eq!(graphics.zoom_out, 100);
        assert!(graphics.validate().is_ok());
        assert!(Graphics { zoom_out: MAX_ZOOM_OUT, ..graphics }.validate().is_ok());
        assert!(Graphics { zoom_out: 99, ..graphics }.validate().is_err());
        assert!(Graphics { zoom_out: MAX_ZOOM_OUT + 1, ..graphics }.validate().is_err());
        assert!(!graphics.culling.viewport && !graphics.culling.back_faces);
        assert_eq!(graphics.zoomed(2).zoom_out, 150);
        assert_eq!(graphics.zoomed(-1).zoom_out, 100, "1x is the nearest");
        assert_eq!(graphics.zoomed(100).zoom_out, MAX_ZOOM_OUT);
    }

    #[test]
    fn a_headset_shows_the_screen_unless_the_diorama_is_chosen_within_its_bounds() {
        let xr = XrScreen::default();
        assert_eq!(xr.view, HeadsetView::Screen, "the screen by default");
        assert_eq!((xr.diorama_scale, xr.diorama_height_cm, xr.diorama_distance_cm, xr.diorama_tilt_deg), (100, -45, 60, 0));
        assert_eq!((xr.diorama_turns, xr.passthrough), (DioramaTurns::Smooth, false));
        // Settings saved before the diorama existed keep their screen.
        let older: Graphics = serde_json::from_str(r#"{"xr":{"distance_cm":300,"width_cm":400,"depth_of_field":true}}"#).unwrap();
        assert_eq!(older.xr, XrScreen { distance_cm: 300, width_cm: 400, depth_of_field: true, ..XrScreen::default() });
        let diorama = Graphics {
            xr: XrScreen { view: HeadsetView::Diorama, diorama_scale: 250, diorama_turns: DioramaTurns::Snap, passthrough: true, ..xr },
            ..Graphics::default()
        };
        let text = serde_json::to_string(&diorama).unwrap();
        assert!(text.contains(r#""view":"Diorama""#) && text.contains(r#""diorama_turns":"Snap""#), "{text}");
        assert_eq!(serde_json::from_str::<Graphics>(&text).unwrap(), diorama);
        assert!(diorama.validate().is_ok());
        let bounded = |edit: fn(&mut XrScreen)| {
            let mut xr = XrScreen::default();
            edit(&mut xr);
            Graphics { xr, ..Graphics::default() }.validate()
        };
        for (edit, ok) in [
            ((|x: &mut XrScreen| x.diorama_scale = 25) as fn(&mut XrScreen), true),
            (|x| x.diorama_scale = 400, true),
            (|x| x.diorama_scale = 24, false),
            (|x| x.diorama_scale = 401, false),
            (|x| x.diorama_height_cm = -100, true),
            (|x| x.diorama_height_cm = 31, false),
            (|x| x.diorama_height_cm = -101, false),
            (|x| x.diorama_distance_cm = 29, false),
            (|x| x.diorama_distance_cm = 201, false),
            (|x| x.diorama_tilt_deg = 45, true),
            (|x| x.diorama_tilt_deg = 46, false),
        ] {
            assert_eq!(bounded(edit).is_ok(), ok, "{:?}", bounded(edit));
        }
        assert_eq!(bounded(|x| x.diorama_tilt_deg = 46).unwrap_err(), "diorama tilt must be 0-45 degrees");
        // Reset diorama: its size, place and turns, not the view, the screen
        // or passthrough.
        let moved = XrScreen { diorama_scale: 300, diorama_height_cm: -20, diorama_distance_cm: 90, diorama_tilt_deg: 10, diorama_turns: DioramaTurns::Fixed, ..diorama.xr };
        assert_eq!(moved.diorama_reset(), XrScreen { view: HeadsetView::Diorama, passthrough: true, ..XrScreen::default() });
    }

    #[test]
    fn a_headset_draws_effects_its_profile_has_but_depth_of_field_only_when_asked() {
        assert!(!Graphics::default().headset_effects(), "Modern draws none");
        let hd2d = Graphics { profile: GraphicsProfile::Hd2d, ..Graphics::default() };
        assert!(hd2d.headset_effects());
        let only_dof = Graphics { profile: GraphicsProfile::Custom, custom_effects: EffectLevels { depth_of_field: 50, focus: 20, ..EffectLevels::OFF }, ..Graphics::default() };
        assert!(!only_dof.headset_effects(), "a headset leaves depth of field out by default");
        assert!(Graphics { xr: XrScreen { depth_of_field: true, ..XrScreen::default() }, ..only_dof }.headset_effects());
    }

    #[test]
    fn levels_are_named_and_bounded() {
        let mut levels = EffectLevels::OFF;
        levels.set("bloom", 40).unwrap();
        levels.set("focus", -20).unwrap();
        assert_eq!(levels.get("bloom"), Some(40));
        assert_eq!(levels.get("focus"), Some(-20));
        assert!(levels.set("bloom", 101).is_err());
        assert!(levels.set("focus", 120).is_err());
        assert!(levels.set("sparkles", 1).is_err());
    }
}
