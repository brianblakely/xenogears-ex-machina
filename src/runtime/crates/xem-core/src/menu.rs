//! The selectable entries of the game's own interface as it shows them now,
//! for pointing at them (a mouse, a touch, gaze): each entry's rectangle on
//! the 320x224 screen, the entry the game has selected, and how the
//! D-pad moves the selection. Read from the game's memory at its original
//! addresses; the game moves its cursor only by the pad.
//!
//! - The field menu's commands (`menu_state_current` +0x336), drawn on an
//!   arc by `menu_highlight_x/y_table` (0x801E9A00/0x801E9A2C, 32-bit
//!   entries: the seven commands, then the four rows of a choice window);
//!   Up steps to the next, wrapping. While a command's window is open
//!   (`images.dim`) its choice window (+0x338, count +0x33A) is the same.
//! - The item shop's (ovl2601: 0x801D2194/0x801D21B0, three commands and
//!   four rows) and the Gear shop's (ovl2602: 0x801D6BFC/0x801D6C20, four
//!   and four) the same way.
//! - Yes and No (`MenuMarkers` +0x142, +0x143 shown with the messages):
//!   their markers' quads; Left selects Yes, Right No.
//! - A field dialogue's choice: its window's text lines (the text window at
//!   +0x18: x +0x1C, y +0x1E, width +0x20, line height +0x2C, first line
//!   +0x2E), Down to the next, wrapping.
use crate::action::GameAction;
use crate::context::Context;
use crate::memory::GameMemory;
use serde::Serialize;

#[derive(Clone, Copy, Debug, PartialEq, Serialize)]
pub struct Rect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

impl Rect {
    pub fn contains(&self, (x, y): (f32, f32)) -> bool {
        x >= self.x && x < self.x + self.width && y >= self.y && y < self.y + self.height
    }
    pub fn centre(&self) -> (f32, f32) {
        (self.x + self.width / 2.0, self.y + self.height / 2.0)
    }
}

/// How the D-pad moves the selection.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum Steps {
    /// `next` selects the following entry and its opposite the one before,
    /// wrapping around `count`.
    Ring { count: usize, next: GameAction },
    /// Each entry has its own action (Yes/No).
    Direct,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize)]
pub struct Entry {
    pub rect: Rect,
    /// The action that selects it directly (`Steps::Direct`).
    pub select: Option<GameAction>,
}

#[derive(Clone, Debug, PartialEq, Serialize)]
pub struct Targets {
    pub what: &'static str,
    pub entries: Vec<Entry>,
    pub selected: Option<usize>,
    pub steps: Steps,
}

impl Targets {
    /// The entry at a screen point.
    pub fn at(&self, point: (f32, f32)) -> Option<usize> {
        self.entries.iter().position(|e| e.rect.contains(point))
    }

    /// The next press toward entry `to`, if not selected already.
    pub fn step_toward(&self, to: usize) -> Option<GameAction> {
        if Some(to) == self.selected {
            return None;
        }
        match self.steps {
            Steps::Direct => self.entries.get(to)?.select,
            Steps::Ring { count, next } => {
                let from = self.selected?;
                let forward = (to + count - from) % count;
                Some(if forward <= count / 2 { next } else { opposite(next) })
            }
        }
    }
}

fn opposite(action: GameAction) -> GameAction {
    match action {
        GameAction::Up => GameAction::Down,
        GameAction::Down => GameAction::Up,
        GameAction::Left => GameAction::Right,
        GameAction::Right => GameAction::Left,
        other => other,
    }
}

const MENU_STATE_CURRENT: u32 = 0x8006_25A0;
const MENU_STATE_SCREEN: u32 = 0x8005_9460;
const DIALOGUE_WINDOWS: u32 = 0x800C_2698;

fn bytes(memory: &dyn GameMemory, address: u32, len: usize) -> Option<Vec<u8>> {
    let mut b = vec![0; len];
    memory.read(address, &mut b).ok().map(|()| b)
}
fn u8_at(memory: &dyn GameMemory, address: u32) -> Option<u8> { bytes(memory, address, 1).map(|b| b[0]) }
fn s16_at(memory: &dyn GameMemory, address: u32) -> Option<i16> { bytes(memory, address, 2).map(|b| i16::from_le_bytes([b[0], b[1]])) }
fn s32_at(memory: &dyn GameMemory, address: u32) -> Option<i32> { bytes(memory, address, 4).map(|b| i32::from_le_bytes([b[0], b[1], b[2], b[3]])) }
fn ram(pointer: i32) -> Option<u32> {
    let p = pointer as u32;
    (0x8000_0000..0x8020_0000).contains(&p).then_some(p)
}

/// The entry at table position (hx, hy): its bullet and label as the screen
/// shows them (measured on the field menu: the label's text starts at hx
/// and is centred about hy + 1; the bullet is left of it), `width` the
/// highlight's width.
fn highlight(hx: i32, hy: i32, width: i32) -> Rect {
    Rect { x: (hx - 14) as f32, y: (hy - 6) as f32, width: (14 + width - 4) as f32, height: 14.0 }
}

fn table(memory: &dyn GameMemory, xs: u32, ys: u32, first: usize, count: usize, width: i32) -> Option<Vec<Entry>> {
    (first..first + count).map(|i| {
        let hx = s32_at(memory, xs + i as u32 * 4)?;
        let hy = s32_at(memory, ys + i as u32 * 4)?;
        Some(Entry { rect: highlight(hx, hy, width), select: None })
    }).collect()
}

/// The entries `ui` shows that a pointer can select.
pub fn targets(memory: &dyn GameMemory, ui: Context) -> Option<Targets> {
    match ui {
        Context::Menu { screen, cursor, choice, choices } => menu(memory, screen, cursor, choice, choices),
        Context::Dialogue { choice: Some(_) } => dialogue_choice(memory),
        _ => None,
    }
}

fn menu(memory: &dyn GameMemory, screen: u8, cursor: u8, choice: u8, choices: u8) -> Option<Targets> {
    let state = ram(s32_at(memory, MENU_STATE_CURRENT)?)?;
    if u8_at(memory, MENU_STATE_SCREEN)? != screen {
        return None;
    }
    let flags = ram(s32_at(memory, state + 0x33C)?)?;
    let markers = ram(s32_at(memory, state + 0x428)?);
    let images = ram(s32_at(memory, state + 0x350)?)?;
    let dim = u8_at(memory, images + 0x1192)? != 0;
    let flag = |offset: u32| u8_at(memory, flags + offset).is_some_and(|f| f != 0);
    // Yes and No, whichever screen asks.
    if flag(0x2E) && let Some(markers) = markers && flag(0x2F) {
        let shown = [u8_at(memory, markers + 0x142)?, u8_at(memory, markers + 0x143)?];
        if shown.iter().any(|s| *s != 0) {
            let entries = (2..4).map(|i| {
                let buffer = u32::from(u8_at(memory, markers + 0x148 + i)? & 1);
                let poly = markers + (i * 2 + buffer) * 0x28;
                let x = f32::from(s16_at(memory, poly + 8)?);
                let y = f32::from(s16_at(memory, poly + 0xA)?);
                // The marker and the word after it.
                Some(Entry { rect: Rect { x: x - 4.0, y: y - 6.0, width: 56.0, height: 24.0 }, select: Some(if i == 2 { GameAction::Left } else { GameAction::Right }) })
            }).collect::<Option<Vec<_>>>()?;
            return Some(Targets { what: "yes_no", entries, selected: Some(if shown[0] != 0 { 0 } else { 1 }), steps: Steps::Direct });
        }
    }
    let (xs, ys, commands, rows_from, width) = match screen {
        0 => (0x801E_9A00, 0x801E_9A2C, 7, 7, 0x40),
        4 => (0x801D_2194, 0x801D_21B0, 3, 3, 0x4C),
        5 => (0x801D_6BFC, 0x801D_6C20, 4, 4, 0x4C),
        _ => return None,
    };
    if !flag(0x04) {
        return None;
    }
    if !dim {
        let entries = table(memory, xs, ys, 0, commands, width)?;
        return Some(Targets { what: "commands", entries, selected: Some(usize::from(cursor)), steps: Steps::Ring { count: commands, next: GameAction::Up } });
    }
    if flag(0x0A) && (1..=4).contains(&choices) {
        let entries = table(memory, xs, ys, rows_from, usize::from(choices), width)?;
        return Some(Targets { what: "choices", entries, selected: Some(usize::from(choice)), steps: Steps::Ring { count: usize::from(choices), next: GameAction::Up } });
    }
    None
}

fn dialogue_choice(memory: &dyn GameMemory) -> Option<Targets> {
    for i in 0..4u32 {
        let window = DIALOGUE_WINDOWS + i * 0x498;
        let active = s16_at(memory, window + 0x40E)? == 0 && s16_at(memory, window + 0x410)? == 0
            && s16_at(memory, window + 0x37C)? == 0 && s16_at(memory, window + 0x408)? == 0;
        if !active {
            continue;
        }
        let first = s16_at(memory, window + 0x37E)?;
        let count = s16_at(memory, window + 0x380)?.clamp(0, 8) as usize;
        let index = s16_at(memory, window + 0x382)?;
        let text = window + 0x18;
        let x = f32::from(s16_at(memory, text + 4)?);
        let y = f32::from(s16_at(memory, text + 6)?);
        let width = f32::from(s16_at(memory, text + 8)?);
        let line = f32::from(s16_at(memory, text + 0x14)?.max(1));
        let top = f32::from(s16_at(memory, text + 0x16)?);
        let entries = (0..count).map(|k| Entry {
            rect: Rect { x: x - 16.0, y: y + line * (f32::from(first) + k as f32 - top), width: width + 16.0, height: line },
            select: None,
        }).collect();
        return Some(Targets { what: "dialogue_choice", entries, selected: usize::try_from(index).ok(), steps: Steps::Ring { count, next: GameAction::Down } });
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rings_step_the_short_way_and_direct_entries_their_own() {
        let entry = Entry { rect: Rect { x: 0.0, y: 0.0, width: 10.0, height: 10.0 }, select: None };
        let ring = Targets { what: "commands", entries: vec![entry; 7], selected: Some(1), steps: Steps::Ring { count: 7, next: GameAction::Up } };
        assert_eq!(ring.step_toward(3), Some(GameAction::Up));
        assert_eq!(ring.step_toward(6), Some(GameAction::Down));
        assert_eq!(ring.step_toward(1), None);
        let yes = Entry { select: Some(GameAction::Left), ..entry };
        let no = Entry { select: Some(GameAction::Right), rect: Rect { x: 20.0, ..entry.rect } };
        let pair = Targets { what: "yes_no", entries: vec![yes, no], selected: Some(1), steps: Steps::Direct };
        assert_eq!(pair.step_toward(0), Some(GameAction::Left));
        assert_eq!(pair.at((25.0, 5.0)), Some(1));
    }
}
