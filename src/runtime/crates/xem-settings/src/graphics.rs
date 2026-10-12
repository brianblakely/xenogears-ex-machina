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

/// How often frames are presented. The game always runs at its own 60
/// frames a second; presentation never changes simulation timing.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum PresentRate {
    /// Every display refresh (vsync).
    #[default]
    Display,
    /// Only when the game finished a frame.
    Game,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct TextureFiltering {
    pub world: crate::Filter,
    pub sprites: crate::Filter,
    pub interface: crate::Filter,
    /// Movie frames (24-bit pictures the renderer shows as the device holds
    /// them), enlarged to the window.
    pub movies: MovieFilter,
}

/// How Ex Machina's renderer enlarges movie frames.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum MovieFilter {
    /// Catmull-Rom bicubic: smooth and sharp many times enlarged.
    #[default]
    Bicubic,
    /// Each pixel a block, as the console's picture enlarged.
    Nearest,
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

/// The game screen in a headset: a flat virtual screen in front of a seated
/// player.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct XrScreen {
    /// Distance from the head at entry, centimetres.
    pub distance_cm: u16,
    /// Screen width, centimetres.
    pub width_cm: u16,
    /// Depth of field in a headset (off by default: it fights the eyes' own focus).
    pub depth_of_field: bool,
}

impl Default for XrScreen {
    fn default() -> Self {
        XrScreen { distance_cm: 250, width_cm: 300, depth_of_field: false }
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
    pub present_rate: PresentRate,
    /// Interpolate presented frames between the game's frames (one game
    /// frame of latency; matched primitives move smoothly).
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
            present_rate: PresentRate::Display,
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

    pub(crate) fn validate(&self) -> Result<(), String> {
        self.hd2d.validate()?;
        self.custom_effects.validate()?;
        if self.internal_scale > MAX_INTERNAL_SCALE {
            return Err(format!("internal scale must be 0 (auto) to {MAX_INTERNAL_SCALE}"));
        }
        if !(100..=MAX_ZOOM_OUT).contains(&self.zoom_out) {
            return Err(format!("zoom out must be 100 to {MAX_ZOOM_OUT} percent"));
        }
        if !(50..=2000).contains(&self.xr.distance_cm) || !(50..=2000).contains(&self.xr.width_cm) {
            return Err("headset screen distance and width must be 50-2000 cm".into());
        }
        Ok(())
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
    fn movies_are_bicubic_unless_nearest_is_chosen() {
        assert_eq!(Graphics::default().filtering.movies, MovieFilter::Bicubic);
        let older: Graphics = serde_json::from_str(r#"{"filtering":{"world":"Linear"}}"#).unwrap();
        assert_eq!((older.filtering.world, older.filtering.movies), (crate::Filter::Linear, MovieFilter::Bicubic));
        let nearest = Graphics { filtering: TextureFiltering { movies: MovieFilter::Nearest, ..Default::default() }, ..Graphics::default() };
        assert_eq!(serde_json::from_str::<Graphics>(&serde_json::to_string(&nearest).unwrap()).unwrap(), nearest);
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
