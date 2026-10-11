//! App operations and their keyboard/gamepad chords, shared by all hosts.
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub enum AppAction {
    SaveState, LoadState, Rewind, ToggleRewind, ToggleFastForward, ToggleTriggerInversion,
    IncreaseFastForwardRate, DecreaseFastForwardRate,
    IncreaseRewindInterval, DecreaseRewindInterval,
    IncreaseRewindHistory, DecreaseRewindHistory,
    ExportState, ImportState, ExportCard1, ImportCard1, ExportCard2, ImportCard2,
    ExportSettings, ImportSettings, RetryStorage, RequestPersistence,
    ToggleFps, TogglePauseOnFocusLoss,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum AppRequest {
    Action(AppAction),
    Rewind(bool),
    UiRewind(bool),
}
pub const APP_ACTIONS: [(AppAction, &str); 24] = [
    (AppAction::SaveState, "Save state"), (AppAction::LoadState, "Load state"),
    (AppAction::Rewind, "Rewind (hold)"), (AppAction::ToggleRewind, "Toggle rewind history"),
    (AppAction::ToggleFastForward, "Toggle fast-forward"),
    (AppAction::ToggleTriggerInversion, "Invert LT/RT chords"),
    (AppAction::IncreaseFastForwardRate, "Increase fast-forward rate"),
    (AppAction::DecreaseFastForwardRate, "Decrease fast-forward rate"),
    (AppAction::IncreaseRewindInterval, "Increase rewind interval"),
    (AppAction::DecreaseRewindInterval, "Decrease rewind interval"),
    (AppAction::IncreaseRewindHistory, "Increase rewind history length"),
    (AppAction::DecreaseRewindHistory, "Decrease rewind history length"),
    (AppAction::ExportState, "Export state backup"), (AppAction::ImportState, "Import state backup"),
    (AppAction::ExportCard1, "Export original card 1"), (AppAction::ImportCard1, "Import original card 1"),
    (AppAction::ExportCard2, "Export original card 2"), (AppAction::ImportCard2, "Import original card 2"),
    (AppAction::ExportSettings, "Export settings"), (AppAction::ImportSettings, "Import settings"),
    (AppAction::RetryStorage, "Retry pending card writes"), (AppAction::RequestPersistence, "Request browser persistence"),
    (AppAction::ToggleFps, "Toggle FPS display"),
    (AppAction::TogglePauseOnFocusLoss, "Toggle pause on focus loss"),
];
pub const PAD_NAMES: [&str; 16] = ["South", "East", "West", "North", "L1", "R1", "LT", "RT", "Back", "Start", "L3", "R3", "Up", "Down", "Left", "Right"];

#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AppBinding {
    pub keyboard: Vec<String>,
    pub gamepad: Vec<String>,
}
impl AppBinding {
    pub fn parse(keyboard: &str, gamepad: &str) -> Self {
        let split = |s: &str| s.split('+').map(str::trim).filter(|s| !s.is_empty()).map(str::to_owned).collect();
        Self { keyboard: split(keyboard), gamepad: split(gamepad) }
    }
    pub fn validate(&self) -> Result<(), String> {
        for chord in [&self.keyboard, &self.gamepad] {
            if chord.len() > 4 || chord.iter().collect::<BTreeSet<_>>().len() != chord.len() {
                return Err("binding chords have at most four distinct inputs".into());
            }
        }
        if self.keyboard.iter().any(|k| !keyboard_code(k)) {
            return Err("keyboard inputs use KeyboardEvent.code names, e.g. F5 or ControlLeft+KeyS".into());
        }
        if self.gamepad.iter().any(|k| !PAD_NAMES.contains(&k.as_str())) {
            return Err(format!("gamepad buttons: {}", PAD_NAMES.join(", ")));
        }
        Ok(())
    }
}
fn keyboard_code(code: &str) -> bool {
    if let Some(key) = code.strip_prefix("Key") { return key.len() == 1 && key.bytes().all(|b| b.is_ascii_uppercase()); }
    if let Some(key) = code.strip_prefix("Digit") { return key.len() == 1 && key.bytes().all(|b| b.is_ascii_digit()); }
    if let Some(key) = code.strip_prefix('F') { return key.parse::<u8>().is_ok_and(|n| (2..=24).contains(&n) && key == n.to_string()); }
    if let Some(key) = code.strip_prefix("Numpad") { return (key.len() == 1 && key.bytes().all(|b| b.is_ascii_digit())) || ["Decimal","Add","Subtract","Multiply","Divide","Enter"].contains(&key); }
    ["ControlLeft","ControlRight","ShiftLeft","ShiftRight","AltLeft","AltRight","MetaLeft","MetaRight",
     "ArrowUp","ArrowDown","ArrowLeft","ArrowRight","Enter","Space","Tab","Backspace","Delete","Insert","Home","End",
     "PageUp","PageDown","CapsLock","NumLock","ScrollLock","Pause","PrintScreen","BracketLeft","BracketRight","Backslash",
     "Semicolon","Quote","Backquote","Comma","Period","Slash","Minus","Equal"].contains(&code)
}
pub fn defaults() -> BTreeMap<AppAction, AppBinding> {
    use AppAction::*;
    [(SaveState,"F5","Back+South"), (LoadState,"F8","Back+North"),
     (Rewind,"KeyR","Back+LT"), (ToggleRewind,"F7","Back+East"),
     (ToggleFastForward,"Backquote","Back+RT")]
        .into_iter().map(|(a,k,g)| (a,AppBinding::parse(k,g))).collect()
}

pub struct AppInput {
    keys: BTreeSet<String>,
    pad: u16,
    active: BTreeSet<AppAction>,
    focused: bool,
}
impl Default for AppInput {
    fn default() -> Self {
        Self { keys: BTreeSet::new(), pad: 0, active: BTreeSet::new(), focused: true }
    }
}
impl AppInput {
    pub fn focused(&self) -> bool { self.focused }
    pub fn set_focused(&mut self, focused: bool) -> Vec<AppRequest> {
        let released = if focused { vec![] } else { self.clear() };
        self.focused = focused;
        released
    }
    pub fn key(&mut self, code: &str, down: bool) {
        if !self.focused { return; }
        if down { self.keys.insert(code.to_owned()); } else { self.keys.remove(code); }
    }
    pub fn pad(&mut self, pressed: &[u8]) {
        if !self.focused { return; }
        self.pad = pressed.iter().take(16).enumerate().filter(|(_, p)| **p != 0).fold(0, |n,(i,_)| n | (1 << i));
    }
    pub fn release_keyboard(&mut self) { self.keys.clear(); }
    pub fn clear(&mut self) -> Vec<AppRequest> {
        let rewinding = self.active.contains(&AppAction::Rewind);
        *self = Self::default();
        if rewinding { vec![AppRequest::Rewind(false)] } else { vec![] }
    }
    fn pad_matches(&self, chord: &[String], invert_triggers: bool) -> bool {
        if chord.is_empty() { return false; }
        let invert = invert_triggers && chord.iter().any(|k| k == "LT" || k == "RT");
        if invert && (self.pad & (1 << 8) != 0) == chord.iter().any(|k| k == "Back") { return false; }
        chord.iter().filter(|k| !invert || *k != "Back").all(|k|
            PAD_NAMES.iter().position(|n| *n == k).is_some_and(|i| self.pad & (1 << i) != 0))
    }
    pub fn requests(&mut self, bindings: &BTreeMap<AppAction, AppBinding>, invert_triggers: bool) -> Vec<AppRequest> {
        let mut matches = Vec::new();
        for (&action, binding) in bindings {
            let key = !binding.keyboard.is_empty() && binding.keyboard.iter().all(|k| self.keys.contains(k));
            let pad = self.pad_matches(&binding.gamepad, invert_triggers);
            if key || pad { matches.push((action, if key { binding.keyboard.len() } else { 0 }, if pad { binding.gamepad.len() } else { 0 })); }
        }
        // A held chord on one device must not suppress a new chord on the other.
        let longest_key = matches.iter().map(|(_, key, _)| *key).max().unwrap_or(0);
        let longest_pad = matches.iter().map(|(_, _, pad)| *pad).max().unwrap_or(0);
        let active: BTreeSet<_> = matches.into_iter().filter(|(_,key,pad)|
            (*key > 0 && *key == longest_key) || (*pad > 0 && *pad == longest_pad))
            .map(|(a,_,_)| a).collect();
        let mut requests: Vec<_> = active.difference(&self.active).filter(|a| **a != AppAction::Rewind)
            .map(|a| AppRequest::Action(*a)).collect();
        let held = active.contains(&AppAction::Rewind);
        if held != self.active.contains(&AppAction::Rewind) { requests.push(AppRequest::Rewind(held)); }
        self.active = active;
        requests
    }
    pub fn captures_key(&self, code: &str, bindings: &BTreeMap<AppAction, AppBinding>) -> bool {
        bindings.values().any(|b| b.keyboard.iter().any(|k| k == code) && b.keyboard.iter().all(|k| self.keys.contains(k)))
    }
    /// Remove app chords from the game pad. Inverted Back+LT/RT send only
    /// the trigger to the game, without leaking the Back modifier as Select.
    pub fn gamepad_for_game(&self, bindings: &BTreeMap<AppAction, AppBinding>, invert_triggers: bool) -> [u8;16] {
        let mut pad = self.pad;
        for binding in bindings.values().filter(|b| self.pad_matches(&b.gamepad,invert_triggers)) {
            for key in &binding.gamepad {
                if let Some(i) = PAD_NAMES.iter().position(|n| *n == key) { pad &= !(1 << i); }
            }
        }
        if invert_triggers && self.pad & (1 << 8) != 0 && self.pad & ((1 << 6) | (1 << 7)) != 0 {
            pad &= !(1 << 8);
        }
        std::array::from_fn(|i| u8::from(pad & (1 << i) != 0))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn focus_loss_releases_rewind_and_prevents_background_polling_from_reasserting_it() {
        let bindings = defaults();
        let mut input = AppInput::default();
        let mut pad = [0;16]; pad[6]=1; pad[8]=1;
        input.pad(&pad);
        assert_eq!(input.requests(&bindings,false),vec![AppRequest::Rewind(true)]);
        assert_eq!(input.set_focused(false),vec![AppRequest::Rewind(false)]);
        input.pad(&pad); input.key("KeyR",true); input.key("Backquote",true);
        assert!(input.requests(&bindings,false).is_empty());
        assert_eq!(input.gamepad_for_game(&bindings,false),[0;16]);
        assert!(input.set_focused(true).is_empty());
        input.pad(&pad);
        assert_eq!(input.requests(&bindings,false),vec![AppRequest::Rewind(true)]);
        input.pad(&[0;16]);
        assert_eq!(input.requests(&bindings,false),vec![AppRequest::Rewind(false)]);
    }
    #[test]
    fn keyboard_and_gamepad_chords_do_not_suppress_each_other() {
        let bindings = BTreeMap::from([
            (AppAction::ToggleFastForward, AppBinding::parse("Backquote", "Back+R1")),
            (AppAction::ImportState, AppBinding::parse("F11", "")),
        ]);
        let mut input = AppInput::default();
        let mut pad = [0;16]; pad[8] = 1; pad[5] = 1;
        input.pad(&pad);
        assert_eq!(input.requests(&bindings,false),vec![AppRequest::Action(AppAction::ToggleFastForward)]);
        input.key("F11",true);
        assert_eq!(input.requests(&bindings,false),vec![AppRequest::Action(AppAction::ImportState)]);
        assert!(input.requests(&bindings,false).is_empty());
    }
    #[test]
    fn every_operation_can_use_either_device_without_repeating_held_chords() {
        for (action,_) in APP_ACTIONS {
            let binding = AppBinding::parse("ControlLeft+KeyS", "Back+South");
            binding.validate().unwrap();
            let bindings = BTreeMap::from([(action,binding)]);
            let mut input = AppInput::default();
            input.key("ControlLeft",true); input.key("KeyS",true);
            let request = if action == AppAction::Rewind { AppRequest::Rewind(true) } else { AppRequest::Action(action) };
            assert_eq!(input.requests(&bindings,false), vec![request]);
            assert!(input.requests(&bindings,false).is_empty());
            let released = input.clear();
            assert_eq!(released,if action == AppAction::Rewind { vec![AppRequest::Rewind(false)] } else { vec![] });
            let mut pad = [0;16]; pad[0]=1; pad[8]=1;
            input.pad(&pad); assert_eq!(input.requests(&bindings,false),vec![request]);
            assert_eq!(input.gamepad_for_game(&bindings,false),[0;16]);
        }
    }
    #[test]
    fn rewind_stays_held_until_both_devices_release_and_inverted_triggers_send_game_buttons() {
        let bindings = defaults();
        assert_eq!(bindings[&AppAction::Rewind],AppBinding::parse("KeyR","Back+LT"));
        assert_eq!(bindings[&AppAction::ToggleFastForward],AppBinding::parse("Backquote","Back+RT"));
        let mut input = AppInput::default();
        input.key("KeyR",true);
        assert_eq!(input.requests(&bindings,false),vec![AppRequest::Rewind(true)]);
        let mut pad = [0;16]; pad[6]=1; pad[8]=1;
        input.pad(&pad); assert!(input.requests(&bindings,false).is_empty());
        input.key("KeyR",false); assert!(input.requests(&bindings,false).is_empty());
        input.pad(&[0;16]);
        assert_eq!(input.requests(&bindings,false),vec![AppRequest::Rewind(false)]);
        for (trigger,request) in [(6,AppRequest::Rewind(true)),(7,AppRequest::Action(AppAction::ToggleFastForward))] {
            input.clear(); pad=[0;16]; pad[trigger]=1;
            input.pad(&pad);
            assert!(input.requests(&bindings,false).is_empty());
            assert_eq!(input.gamepad_for_game(&bindings,false),pad);
            assert_eq!(input.requests(&bindings,true),vec![request]);
            assert_eq!(input.gamepad_for_game(&bindings,true),[0;16]);
            pad[8]=1; input.pad(&pad);
            assert_eq!(input.requests(&bindings,true),if trigger == 6 { vec![AppRequest::Rewind(false)] } else { vec![] });
            let mut game = pad; game[8]=0;
            assert_eq!(input.gamepad_for_game(&bindings,true),game);
            assert_eq!(input.requests(&bindings,false),vec![request]);
            assert_eq!(input.gamepad_for_game(&bindings,false),[0;16]);
        }
    }
}
