//! The headset's assisted controls in the OpenXR loop ([`crate::xr_play`]):
//! OpenXR's raw input (head, eye gaze, controller aims, hand joints) feeds
//! `xem_assist::headset`, whose resolved gestures act through the same
//! player input and command layer as every other control, and its marks
//! (`xem_assist::overlay`) are drawn as Seraph Glass quads on and beside
//! the virtual screen.
use crate::Game;
use xem_assist::draw::{Placed, WIDGET_PIXELS};
use xem_assist::gesture::{Gesture, JOINTS};
use xem_assist::headset::{Carry, Headset, Input, PointerSource, Pose, Scaling, Screen};
use xem_assist::overlay;
use xem_render::glass::{self, Style, Widget};
use xem_render::ps1::GameView;
use xem_settings::player::{PlayerInput, Pulse};
use xem_settings::{Settings, XrPointer};

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
}

#[derive(Default)]
pub struct Assisted {
    pub headset: Headset,
    pub atlas: glass::Atlas,
    pub scaling: Scaling,
    /// The last frame's gestures, for the frame log.
    pub gestures: Vec<Gesture>,
}

impl Assisted {
    pub fn new() -> Self {
        Self::default()
    }

    /// Recognises and carries out this frame's gestures. Returns whether the
    /// settings panel is open after them.
    #[allow(clippy::too_many_arguments)]
    pub fn update(&mut self, settings: &Settings, input: &Input, screen: &Screen, game: &mut Game, player: &mut PlayerInput, view: &mut GameView, panel_visible: bool) -> bool {
        self.headset.source = source(settings.assisted.pointer);
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
        let effects = self.headset.update(input, screen, context.ui, field.as_ref(), targets.as_ref());
        // What a control deck holds (its stick, its held keys).
        player.touch = effects.held.clone();
        let inspectable = context.field.field && !context.field.menu;
        xem_assist::headset::carry(effects.outs, &mut Native { game, player, view, inspectable }, &mut self.scaling);
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
        let targets = game.targets();
        let marks = overlay::marks(ui, aim, field.as_ref(), game.destination(), targets.as_ref());
        xem_assist::draw::room(&marks, screen, &self.headset.key_marks(ui, targets.as_ref()))
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
