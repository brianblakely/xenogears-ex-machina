//! The headset's assisted controls in the OpenXR loop ([`crate::xr_play`]):
//! OpenXR's raw input (head, eye gaze, controller aims, hand joints) feeds
//! `xem_assist::headset`, whose resolved gestures act through the same
//! player input and command layer as every other control, and its marks
//! (`xem_assist::overlay`) are drawn as Seraph Glass quads on and beside
//! the virtual screen.
use crate::Game;
use xem_assist::draw::{Placed, WIDGET_PIXELS};
use xem_assist::gesture::{Gesture, JOINTS, Micro, Swipe};
use xem_assist::headset::{Carry, Headset, Input, PointerSource, Pose, Scaling, Screen, Seat};
use xem_assist::overlay;
use xem_render::glass::{self, Style, Widget};
use xem_render::ps1::GameView;
use xem_settings::player::{PlayerInput, Pulse};
use xem_settings::{AppAction, AppRequest, Settings, XrPointer};

fn pose(p: xem_xr::Pose) -> Pose {
    Pose { position: p.position, orientation: p.orientation }
}

/// The standard pinch's value at which a pinch begins, and below which it
/// has ended (`pinch_ext/value` runs 0..1, 1 the fingertips touching).
const PINCH_ON: f32 = 0.9;
const PINCH_OFF: f32 = 0.6;

/// One frame's input from OpenXR's raw state: the hands' joints (for the
/// plan's own gestures), the runtime's aim, pinch and microgestures (the
/// standard ones); `pinched` keeps each hand's pinch between frames.
pub fn input(state: &xem_xr::InputState, head: Option<xem_xr::Pose>, time: f64, focused: bool, pinched: &mut [bool; 2]) -> Input {
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
        pinch: [0, 1].map(|i| {
            let (value, ready) = state.hands[i].pinch?;
            pinched[i] = if pinched[i] { value >= PINCH_OFF } else { ready && value >= PINCH_ON };
            Some(pinched[i])
        }),
        micro: [0, 1].map(|i| state.hands[i].microgesture.map(|m| match m {
            xem_xr::Microgesture::Tap => Micro::Tap,
            xem_xr::Microgesture::Left => Micro::Swipe(Swipe::Left),
            xem_xr::Microgesture::Right => Micro::Swipe(Swipe::Right),
            xem_xr::Microgesture::Forward => Micro::Swipe(Swipe::Up),
            xem_xr::Microgesture::Backward => Micro::Swipe(Swipe::Down),
        })),
        micro_capable: [0, 1].map(|i| state.hands[i].microgestures),
        focused,
    }
}

/// The pointer source a setting names.
pub fn source(pointer: XrPointer) -> PointerSource {
    match pointer {
        XrPointer::Eye => PointerSource::Eye,
        XrPointer::Head => PointerSource::Head,
        XrPointer::LeftHand => PointerSource::LeftRay,
        XrPointer::RightHand => PointerSource::RightRay,
    }
}

/// The game and the player's input, as the headset's effects reach them.
struct Native<'a> {
    game: &'a mut Game,
    player: &'a mut PlayerInput,
    view: &'a mut GameView,
    inspectable: bool,
    requests: &'a mut Vec<AppRequest>,
}

impl Carry for Native<'_> {
    fn command(&mut self, command: serde_json::Value) {
        if let Err(error) = self.game.command(&command) {
            println!("assist command {command} failed: {error}");
        }
    }
    fn press(&mut self, actions: Vec<xem_core::action::GameAction>) {
        self.player.pulse(Pulse::new(actions));
    }
    fn select(&mut self, index: usize, confirm: bool) {
        self.player.select(index, confirm);
    }
    fn inspect(&mut self) {
        if self.inspectable {
            self.view.set_first_person(!self.view.first_person());
        }
    }
    fn release(&mut self) {
        self.player.clear();
    }
    fn toggle_auto_advance(&mut self) {
        self.requests.push(AppRequest::Action(AppAction::ToggleAutoAdvance));
    }
}

#[derive(Default)]
pub struct Assisted {
    pub headset: Headset,
    pub atlas: glass::Atlas,
    pub scaling: Scaling,
    /// Where everything stands in front of the player.
    pub seat: Seat,
    /// The last frame's gestures, for the frame log.
    pub gestures: Vec<Gesture>,
    /// App operations the gestures asked for (AUTO), for the host to carry
    /// out with its others.
    pub requests: Vec<AppRequest>,
}

impl Assisted {
    pub fn new() -> Self {
        Self::default()
    }

    /// Recognises and carries out this frame's gestures. Returns whether the
    /// settings panel is open after them. During first-person inspection
    /// the gestures leave the character where it stands, and nothing is
    /// pointed at through the field camera the view does not show.
    #[allow(clippy::too_many_arguments)]
    pub fn update(&mut self, settings: &Settings, input: &Input, screen: &Screen, game: &mut Game, player: &mut PlayerInput, view: &mut GameView, panel_visible: bool) -> bool {
        self.headset.source = source(settings.assisted.pointer);
        self.headset.inspecting = view.first_person();
        self.headset.auto_advance = settings.auto_advance.enabled;
        if self.headset.settings_open != panel_visible {
            self.headset.set_settings(panel_visible);
        }
        if !settings.assisted.gestures {
            self.gestures.clear();
            return panel_visible;
        }
        let context = game.pad_context();
        // The field as the screen last showed it (pulled back, say).
        let field = if self.headset.inspecting { None } else { game.field().map(|f| overlay::seen(f, view.viewpoint().as_ref())) };
        let targets = game.targets();
        let effects = self.headset.update(input, screen, context.ui, field.as_ref(), targets.as_ref());
        // What a control deck holds (its stick, its held keys).
        player.touch = effects.held.clone();
        // The underhand grab toggles inspection in a field but its menu,
        // where Ex Machina draws it.
        let inspectable = xem_settings::look::grab_toggles(context.field, settings.graphics.renderer);
        xem_assist::headset::carry(effects.outs, &mut Native { game, player, view, inspectable, requests: &mut self.requests }, &mut self.scaling);
        self.gestures = effects.gestures;
        self.headset.settings_open
    }

    /// The marks to draw this frame, as room quads, over the screen showing
    /// the scene from `viewpoint` ([`GameView::viewpoint`]).
    pub fn placed(&self, settings: &Settings, input: &Input, screen: &Screen, game: &mut Game, viewpoint: Option<xem_render::ps1::Viewpoint>) -> Vec<Placed> {
        if !settings.assisted.gestures {
            return vec![];
        }
        let ui = game.pad_context().ui;
        let inspecting = self.headset.inspecting;
        let field = if inspecting { None } else { game.field().map(|f| overlay::seen(f, viewpoint.as_ref())) };
        let aim = self.headset.aim(input, screen);
        let targets = game.targets();
        let destination = if inspecting { None } else { game.destination() };
        let marks = overlay::marks(ui, aim, field.as_ref(), destination, targets.as_ref(), screen.diorama.is_some());
        xem_assist::draw::room(&marks, screen, &self.headset.key_marks(ui, targets.as_ref()), &self.headset.pad_marks(ui))
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
