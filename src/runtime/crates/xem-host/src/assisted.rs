//! The headset's assisted controls in the OpenXR loop ([`crate::xr_play`]):
//! OpenXR's raw input (head, eye gaze, controller aims, hand joints) feeds
//! `xem_assist::headset`, whose resolved gestures act through the same
//! player input and command layer as every other control, and its marks
//! (`xem_assist::overlay`) are drawn as Seraph Glass quads on and beside
//! the virtual screen.
use crate::Game;
use xem_assist::dispatch::Out;
use xem_assist::gesture::{Gesture, JOINTS};
use xem_assist::headset::{Headset, Input, PointerSource, Pose, Quad, Screen};
use xem_assist::overlay::{self, Kind, QuantityKey};
use xem_render::glam::{Mat4, Vec2, Vec3};
use xem_render::glass::{self, Icon, State, Style, Widget};
use xem_render::ps1::GameView;
use xem_settings::player::{PlayerInput, Pulse};
use xem_settings::{Settings, XrPointer};

/// The widgets' texture size in pixels.
const WIDGET_PIXELS: u32 = 128;
/// The quantity keys beside the screen: size in metres and the gap.
const KEY_SIZE: (f32, f32) = (0.22, 0.11);
const KEY_GAP: f32 = 0.04;

fn pose(p: xem_xr::Pose) -> Pose {
    Pose { position: p.position, orientation: p.orientation }
}

/// One frame's input from OpenXR's raw state.
pub fn input(state: &xem_xr::InputState, head: Option<xem_xr::Pose>, time: f64, focused: bool) -> Input {
    let hands = [0, 1].map(|i| {
        // XR_EXT_hand_tracking's 26 joints begin with the palm; WebXR's 25
        // (the recognizers') leave it out.
        state.hand_joints[i].as_ref().filter(|j| j.len() == JOINTS + 1).map(|joints| {
            std::array::from_fn(|k| joints[k + 1].pose.position)
        })
    });
    Input {
        time,
        head: head.map(pose),
        gaze: state.gaze.map(pose),
        rays: [0, 1].map(|i| state.hands[i].aim.filter(|_| state.hands[i].active).map(pose)),
        hands,
        micro: [None; 2],
        micro_capable: [false; 2],
        focused,
    }
}

/// The quantity keys' quads, beside the screen's right edge.
pub fn quantity_keys(screen: &Screen) -> Vec<(u32, Quad)> {
    let right = screen.quad.pose.orientation * Vec3::X;
    let up = screen.quad.pose.orientation * Vec3::Y;
    QuantityKey::ALL.iter().enumerate().map(|(i, _)| {
        let position = screen.quad.pose.position + right * (screen.quad.size.0 / 2.0 + KEY_GAP + KEY_SIZE.0 / 2.0)
            + up * (1.5 - i as f32) * (KEY_SIZE.1 + KEY_GAP);
        (i as u32, Quad { pose: Pose { position, orientation: screen.quad.pose.orientation }, size: KEY_SIZE })
    }).collect()
}

/// A drawable widget at a room pose.
pub struct Placed {
    pub widget: Widget,
    pub pose: Mat4,
    pub size: Vec2,
}

#[derive(Default)]
pub struct Assisted {
    pub headset: Headset,
    pub atlas: glass::Atlas,
    /// The virtual screen's scale from two-hand scaling.
    pub scale: f32,
    scale_start: Option<f32>,
    /// The last frame's gestures, for the frame log.
    pub gestures: Vec<Gesture>,
    /// The key the pointer is on.
    hovered_key: Option<u32>,
}

impl Assisted {
    pub fn new() -> Self {
        Self { scale: 1.0, ..Self::default() }
    }

    /// Recognises and carries out this frame's gestures. Returns whether the
    /// settings panel is open after them.
    #[allow(clippy::too_many_arguments)]
    pub fn update(&mut self, settings: &Settings, input: &Input, screen: &Screen, game: &mut Game, player: &mut PlayerInput, view: &mut GameView, panel_visible: bool) -> bool {
        self.headset.source = match settings.assisted.pointer {
            XrPointer::Eye => PointerSource::Eye,
            XrPointer::Head => PointerSource::Head,
            XrPointer::LeftHand => PointerSource::LeftRay,
            XrPointer::RightHand => PointerSource::RightRay,
        };
        if self.headset.settings_open != panel_visible {
            self.headset.set_settings(panel_visible);
        }
        if !settings.assisted.gestures {
            self.gestures.clear();
            return panel_visible;
        }
        let context = game.pad_context();
        let field = game.field();
        let targets = game.targets();
        let keys = if overlay::quantity_keys(context.ui) { quantity_keys(screen) } else { vec![] };
        self.hovered_key = keys.iter().find(|(_, q)| self.headset.hit(input, q).is_some()).map(|(k, _)| *k);
        let effects = self.headset.update_with_keys(input, screen, context.ui, field.as_ref(), targets.as_ref(), &keys);
        for out in effects.outs {
            match out {
                Out::Press { actions } => player.pulse(Pulse::new(actions)),
                Out::Select { index, confirm } => player.select(index, confirm),
                Out::Command { command } => {
                    if let Err(error) = game.command(&command) {
                        println!("assist command {command} failed: {error}");
                    }
                }
                Out::Key { key } => {
                    if let Some(key) = QuantityKey::ALL.get(key as usize) {
                        let _ = game.command(&serde_json::json!({"cmd": "shop_quantity", "op": key.op()}));
                    }
                }
                Out::ToggleInspect => {
                    if context.field.field && !context.field.menu {
                        view.set_first_person(!view.first_person());
                    }
                }
                Out::ScaleBegin => self.scale_start = Some(self.scale),
                Out::Scale { factor } => {
                    if let Some(start) = self.scale_start {
                        self.scale = (start * factor).clamp(0.4, 3.0);
                    }
                }
                Out::ScaleEnd => self.scale_start = None,
                Out::TrackingLost => {
                    player.clear();
                    let _ = game.command(&serde_json::json!({"cmd": "assist_stop"}));
                }
                Out::TogglePause | Out::ToggleSettings => {}
            }
        }
        self.gestures = effects.gestures;
        self.headset.settings_open
    }

    /// The marks to draw this frame, as room quads.
    pub fn placed(&self, settings: &Settings, input: &Input, screen: &Screen, game: &mut Game) -> Vec<Placed> {
        if !settings.assisted.gestures {
            return vec![];
        }
        let ui = game.pad_context().ui;
        let field = game.field();
        let aim = self.headset.aim(input, screen);
        let normal = screen.quad.pose.orientation * Vec3::Z;
        let mut placed: Vec<Placed> = overlay::marks(ui, aim, field.as_ref(), game.destination()).into_iter().map(|mark| {
            let centre = screen.point(mark.centre);
            let x = screen.point((mark.centre.0 + mark.x.0, mark.centre.1 + mark.x.1)) - centre;
            let y = screen.point((mark.centre.0 + mark.y.0, mark.centre.1 + mark.y.1)) - centre;
            let widget = match mark.kind {
                Kind::Cursor => Widget::Cursor,
                Kind::Floor(reach) => Widget::Floor(match reach {
                    xem_core::field::Reach::Walk => glass::Reach::Walk,
                    xem_core::field::Reach::Jump => glass::Reach::Jump,
                    xem_core::field::Reach::Invalid => glass::Reach::Invalid,
                }),
                Kind::Destination => Widget::Destination,
                Kind::Pages { count, current } => Widget::Pages { count, current },
            };
            let lift = if matches!(mark.kind, Kind::Cursor) { 0.012 } else { 0.006 };
            Placed {
                widget,
                pose: Mat4::from_cols((x * 2.0).extend(0.0), (-y * 2.0).extend(0.0), normal.extend(0.0), (centre + normal * lift).extend(1.0)),
                size: Vec2::ONE,
            }
        }).collect();
        if overlay::quantity_keys(ui) {
            for (key, quad) in quantity_keys(screen) {
                let state = if self.hovered_key == Some(key) { State::Hover } else { State::Normal };
                let label = QuantityKey::ALL[key as usize].label();
                let icon = match label { "+" => Icon::Plus, "-" => Icon::Minus, text => Icon::Text(text.into()) };
                placed.push(Placed {
                    widget: Widget::Key(icon, state),
                    pose: Mat4::from_rotation_translation(quad.pose.orientation, quad.pose.position + normal * 0.002),
                    // The key image is square; the key fills its middle half.
                    size: Vec2::new(quad.size.0, quad.size.0),
                });
            }
        }
        placed
    }

    /// The widgets' style from the settings.
    pub fn style(settings: &Settings) -> Style {
        Style { reduced_transparency: settings.assisted.reduced_transparency }
    }

    /// The texture of a widget.
    pub fn texture(&mut self, gpu: &xem_render::Gpu, widget: &Widget, style: Style) -> wgpu::TextureView {
        self.atlas.view(&gpu.device, &gpu.queue, widget, WIDGET_PIXELS, style).clone()
    }
}
