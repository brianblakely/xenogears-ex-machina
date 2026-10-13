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
//! - The field menu's lists (`menu_list_cursor_place`, 801DB0A8): the item
//!   and arts screens' two columns of eight rows 0x88 apart and 16 high,
//!   the equipment screen's one column of rows 13 high, placed around the
//!   list cursor where it is drawn (`menu_state_current` +0x444, its
//!   quad's corner; `MenuFlags.cursors_shown` +0x50: 1 two columns, 2 one).
//!   The D-pad steps rows and columns.
//! - A battle's lists (arts, items, the combo setup, a Gear's commands):
//!   their cells as their cursor glyph is placed (`battle_animate_cursor_glyph`).
//! - A battle's target choice (`battle_choose_target`): the candidates
//!   (`battle_target_candidates` 0x800C3E90, 0xFF unused; selectable ones in
//!   `battle_target_candidate_mask` 0x800C3D64; the selected slot
//!   `battle_target_cursor_slot` 0x800C3E2C), each at its sprite
//!   (`battle_area.sprites` 0x800CCB3C) projected as
//!   `battle_get_slot_screen_box` does (`battle_camera_view_matrix`
//!   0x800D30BC, screen 512, offset (160, 164)). A direction moves to the
//!   nearest candidate that way on the floor (`battle_find_candidate_in_direction`
//!   on the slots' x and z, `battle_area_slots` 0x800C3EB4), so the way to a
//!   candidate is searched over those moves.
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
    /// The directions move to the nearest entry that way on the battle's
    /// floor ([`Ground`]).
    Compass,
    /// Rows of `columns` entries (in reading order): Down and Up step rows,
    /// Right and Left columns, without wrapping.
    Grid { columns: usize },
}

/// A battle target's slot and its place on the floor.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
pub struct Ground {
    pub slot: u8,
    pub x: i16,
    pub z: i16,
    /// The game lets the cursor onto it.
    pub selectable: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize)]
pub struct Entry {
    pub rect: Rect,
    /// The action that selects it directly (`Steps::Direct`).
    pub select: Option<GameAction>,
    /// Its place (`Steps::Compass`).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ground: Option<Ground>,
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
        self.entries.iter().position(|e| e.ground.is_none_or(|g| g.selectable) && e.rect.contains(point))
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
            Steps::Grid { columns } => {
                let from = self.selected?;
                let columns = columns.max(1);
                let (row, column) = (from / columns, from % columns);
                let (to_row, to_column) = (to / columns, to % columns);
                Some(if to_row > row { GameAction::Down } else if to_row < row { GameAction::Up }
                    else if to_column > column { GameAction::Right } else { GameAction::Left })
            }
            Steps::Compass => {
                // The first move of the fewest that reach it.
                let from = self.selected?;
                let mut first: Vec<Option<GameAction>> = vec![None; self.entries.len()];
                let mut queue = std::collections::VecDeque::from([from]);
                let mut seen = vec![false; self.entries.len()];
                *seen.get_mut(from)? = true;
                while let Some(at) = queue.pop_front() {
                    for (direction, action) in COMPASS.into_iter().enumerate() {
                        let Some(next) = self.compass_move(at, direction) else { continue };
                        if !seen[next] {
                            seen[next] = true;
                            first[next] = first[at].or(Some(action));
                            if next == to {
                                return first[next];
                            }
                            queue.push_back(next);
                        }
                    }
                }
                None
            }
        }
    }

    /// Where direction `direction` (0 Right, 1 Down, 2 Left, 3 Up) moves the
    /// cursor from entry `from`, as `battle_find_candidate_in_direction`
    /// (80084890) finds it and `battle_choose_target` accepts it: the
    /// nearest other candidate inside that quarter, on the slots' 16-bit
    /// x and z.
    fn compass_move(&self, from: usize, direction: usize) -> Option<usize> {
        let origin = self.entries.get(from)?.ground?;
        let mut best = 0xFF_FFFF;
        let mut nearest = None;
        for (i, entry) in self.entries.iter().enumerate() {
            let Some(g) = entry.ground else { continue };
            if g.slot == origin.slot {
                continue;
            }
            let dz = i32::from(g.z as u16) - i32::from(origin.z as u16);
            let dx = i32::from(g.x as u16) - i32::from(origin.x as u16);
            let angle = ratan2(dz, dx);
            let inside = match direction {
                0 => ((angle + 0x200) as u16) < 0x400,
                1 => ((angle + 0x600) as u16) < 0x400,
                2 => ((angle + 0x800) as u16) < 0x200 || ((angle - 0x600) as u16) <= 0x200,
                _ => ((angle - 0x200) as u16) < 0x400,
            };
            let distance = dz.wrapping_mul(dz).wrapping_add(dx.wrapping_mul(dx));
            if inside && distance < best {
                best = distance;
                nearest = Some((i, g));
            }
        }
        let (i, g) = nearest?;
        g.selectable.then_some(i)
    }
}

/// The battle's directions in `battle_pressed_key` order.
const COMPASS: [GameAction; 4] = [GameAction::Right, GameAction::Down, GameAction::Left, GameAction::Up];

/// `ratan2(y, x)`: the angle in 4096ths of a turn, -2048..=2048.
fn ratan2(y: i32, x: i32) -> i32 {
    (f64::from(y).atan2(f64::from(x)) / std::f64::consts::TAU * 4096.0).round() as i32
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
        Some(Entry { rect: highlight(hx, hy, width), select: None, ground: None })
    }).collect()
}

/// The entries `ui` shows that a pointer can select.
pub fn targets(memory: &dyn GameMemory, ui: Context) -> Option<Targets> {
    match ui {
        Context::Menu { screen, cursor, choice, choices, .. } => menu(memory, screen, cursor, choice, choices),
        Context::Dialogue { choice: Some(_) } => dialogue_choice(memory),
        Context::Battle { targeting: true, .. } => battle_targets(memory),
        Context::Battle { list: true, .. } => battle_list(memory),
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
                Some(Entry { rect: Rect { x: x - 4.0, y: y - 6.0, width: 56.0, height: 24.0 }, select: Some(if i == 2 { GameAction::Left } else { GameAction::Right }), ground: None })
            }).collect::<Option<Vec<_>>>()?;
            return Some(Targets { what: "yes_no", entries, selected: Some(if shown[0] != 0 { 0 } else { 1 }), steps: Steps::Direct });
        }
    }
    if screen == 0 && let Some(list) = list(memory, state, flags) {
        return Some(list);
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

/// The list cursor's sprite offset from the place `menu_list_cursor_place`
/// computes (measured on the item screen).
const LIST_CURSOR_OFFSET: (i32, i32) = (0, 0);

/// The field menu's list under its cursor (cursor 0, the one the D-pad
/// moves), as cells around it.
fn list(memory: &dyn GameMemory, state: u32, flags: u32) -> Option<Targets> {
    let shown = u8_at(memory, flags + 0x50)?;
    if shown == 0 {
        return None;
    }
    let cursor = ram(s32_at(memory, state + 0x444)?)?;
    let poly = cursor + u32::from(u8_at(memory, cursor + 0x75)? & 1) * 0x28;
    let x = i32::from(s16_at(memory, poly + 8)?);
    let y = i32::from(s16_at(memory, poly + 0xA)?);
    // Kinds 0-2 (two columns from x 0x18-0x1c, rows from y 0x11-0x14) and
    // 3 (one column at x 0xa0, rows from y 0x14).
    let (columns, rows, column_width, row_height, left, top) = if shown == 2 { (1, 8, 0x88, 13, 0xA0, 0x14) } else { (2, 8, 0x88, 16, 0x18, 0x11) };
    let (cx, cy) = (x - LIST_CURSOR_OFFSET.0, y - LIST_CURSOR_OFFSET.1);
    let column = if columns == 2 { ((cx - left + column_width / 2) / column_width).clamp(0, 1) } else { 0 };
    let row = ((cy - top + row_height / 2) / row_height).clamp(0, rows - 1);
    let entries = (0..rows).flat_map(|r| (0..columns).map(move |c| (r, c))).map(|(r, c)| Entry {
        rect: Rect {
            x: (x + (c - column) * column_width) as f32,
            y: (y + (r - row) * row_height) as f32,
            width: (column_width - 4) as f32,
            height: row_height as f32,
        },
        select: None,
        ground: None,
    }).collect();
    Some(Targets { what: "list", entries, selected: Some((row * columns + column) as usize), steps: Steps::Grid { columns: columns as usize } })
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
            ground: None,
        }).collect();
        return Some(Targets { what: "dialogue_choice", entries, selected: usize::try_from(index).ok(), steps: Steps::Ring { count, next: GameAction::Down } });
    }
    None
}

const BATTLE_UI: u32 = 0x800D_2D28;
const BATTLE_GRAPHICS: u32 = 0x800C_3EA4;
/// The cursor glyph's corner from the place it is built at.
const GLYPH_OFFSET: (i32, i32) = (0, 0);
/// The battle's lists as `battle_animate_cursor_glyph`'s callers place
/// their cursor: columns, rows, column step, row height and the first
/// cell's place (the arts list, the item list, the combo setup, a Gear's
/// commands).
const BATTLE_LISTS: [(i32, i32, i32, i32, i32, i32); 4] = [
    (2, 6, 0x84, 16, 0x2A, 0x38),
    (2, 8, 0x82, 13, 0x2A, 0x63),
    (2, 4, 0x8C, 16, 0x1E, 0x64),
    (1, 4, 0x80, 16, 0x8C, 0x58),
];

/// A battle list (`battle_ui.cursorShown` +0x9E): its cells, found by where
/// the cursor glyph (`battle_graphics` +0x27C8, the buffer at
/// `battle_ui` +0xA7) is drawn.
fn battle_list(memory: &dyn GameMemory) -> Option<Targets> {
    let ui = ram(s32_at(memory, BATTLE_UI)?)?;
    if u8_at(memory, ui + 0x9E)? == 0 {
        return None;
    }
    let graphics = ram(s32_at(memory, BATTLE_GRAPHICS)?)?;
    let poly = graphics + 0x27C8 + u32::from(u8_at(memory, ui + 0xA7)? & 1) * 0x28;
    let px = i32::from(s16_at(memory, poly + 8)?) - GLYPH_OFFSET.0;
    let py = i32::from(s16_at(memory, poly + 0xA)?) - GLYPH_OFFSET.1;
    for (columns, rows, step, height, x, y) in BATTLE_LISTS {
        let column = if px == x { 0 } else if columns == 2 && px == x + step { 1 } else { continue };
        let dy = py - y;
        if dy < 0 || dy % height != 0 || dy / height >= rows {
            continue;
        }
        let entries = (0..rows).flat_map(|r| (0..columns).map(move |c| (r, c))).map(|(r, c)| Entry {
            rect: Rect { x: (x + c * step + GLYPH_OFFSET.0) as f32, y: (y + r * height + GLYPH_OFFSET.1) as f32, width: (step - 4) as f32, height: height as f32 },
            select: None,
            ground: None,
        }).collect();
        return Some(Targets { what: "battle_list", entries, selected: Some((dy / height * columns + column) as usize), steps: Steps::Grid { columns: columns as usize } });
    }
    None
}

const TARGET_CANDIDATES: u32 = 0x800C_3E90;
const TARGET_CANDIDATE_MASK: u32 = 0x800C_3D64;
const TARGET_CURSOR_SLOT: u32 = 0x800C_3E2C;
const AREA_SLOTS: u32 = 0x800C_3EB4;
const AREA_SPRITES: u32 = 0x800C_CB3C;
const CAMERA_VIEW_MATRIX: u32 = 0x800D_30BC;

/// `RotTransPers` with the battle's camera: a point's screen position and depth.
fn project(matrix: &[i32; 12], point: [i32; 3]) -> Option<(f32, f32, f32)> {
    let row = |r: usize| (0..3).map(|c| i64::from(matrix[r * 3 + c]) * i64::from(point[c])).sum::<i64>() / 4096 + i64::from(matrix[9 + r]);
    let (x, y, z) = (row(0), row(1), row(2));
    (z > 0).then(|| {
        let scale = 512.0 / z as f32;
        (160.0 + x as f32 * scale, 164.0 + y as f32 * scale, z as f32)
    })
}

fn battle_targets(memory: &dyn GameMemory) -> Option<Targets> {
    let candidates = bytes(memory, TARGET_CANDIDATES, 11)?;
    let mask = u16::from_le_bytes(bytes(memory, TARGET_CANDIDATE_MASK, 2)?.try_into().ok()?);
    let cursor = u8_at(memory, TARGET_CURSOR_SLOT)?;
    let m = bytes(memory, CAMERA_VIEW_MATRIX, 0x20)?;
    let half = |o: usize| i32::from(i16::from_le_bytes([m[o], m[o + 1]]));
    let word = |o: usize| i32::from_le_bytes([m[o], m[o + 1], m[o + 2], m[o + 3]]);
    let matrix = [half(0), half(2), half(4), half(6), half(8), half(10), half(12), half(14), half(16), word(0x14), word(0x18), word(0x1C)];
    let mut entries = Vec::new();
    let mut selected = None;
    for slot in candidates.into_iter().filter(|c| *c < 11) {
        let place = AREA_SLOTS + u32::from(slot) * 0x1C;
        let sprite = ram(s32_at(memory, AREA_SPRITES + u32::from(slot) * 4)?)?;
        let at = [s32_at(memory, sprite)? >> 16, s32_at(memory, sprite + 4)? >> 16, s32_at(memory, sprite + 8)? >> 16];
        let (x, y, depth) = project(&matrix, at)?;
        // A box the size of a person standing on the point (about 100
        // units tall: Fei measures 45 pixels at depth 1074), at least
        // comfortable to point at.
        let size = (512.0 * 100.0 / depth).clamp(24.0, 200.0);
        if slot == cursor {
            selected = Some(entries.len());
        }
        entries.push(Entry {
            rect: Rect { x: x - size * 0.35, y: y - size, width: size * 0.7, height: size },
            select: None,
            ground: Some(Ground { slot, x: s16_at(memory, place + 0xA)?, z: s16_at(memory, place + 0xC)?, selectable: mask & (1 << slot) != 0 }),
        });
    }
    (!entries.is_empty()).then_some(Targets { what: "battle_targets", entries, selected, steps: Steps::Compass })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rings_step_the_short_way_and_direct_entries_their_own() {
        let entry = Entry { rect: Rect { x: 0.0, y: 0.0, width: 10.0, height: 10.0 }, select: None, ground: None };
        let ring = Targets { what: "commands", entries: vec![entry; 7], selected: Some(1), steps: Steps::Ring { count: 7, next: GameAction::Up } };
        assert_eq!(ring.step_toward(3), Some(GameAction::Up));
        assert_eq!(ring.step_toward(6), Some(GameAction::Down));
        assert_eq!(ring.step_toward(1), None);
        let yes = Entry { select: Some(GameAction::Left), ..entry };
        let no = Entry { select: Some(GameAction::Right), rect: Rect { x: 20.0, ..entry.rect }, ground: None };
        let pair = Targets { what: "yes_no", entries: vec![yes, no], selected: Some(1), steps: Steps::Direct };
        assert_eq!(pair.step_toward(0), Some(GameAction::Left));
        assert_eq!(pair.at((25.0, 5.0)), Some(1));
    }

    #[test]
    fn grids_step_rows_then_columns() {
        let entry = Entry { rect: Rect { x: 0.0, y: 0.0, width: 1.0, height: 1.0 }, select: None, ground: None };
        let grid = Targets { what: "list", entries: vec![entry; 16], selected: Some(1), steps: Steps::Grid { columns: 2 } };
        assert_eq!(grid.step_toward(5), Some(GameAction::Down));
        assert_eq!(grid.step_toward(0), Some(GameAction::Left));
        let below = Targets { selected: Some(4), ..grid.clone() };
        assert_eq!(below.step_toward(1), Some(GameAction::Up));
        assert_eq!(below.step_toward(5), Some(GameAction::Right));
    }

    #[test]
    fn battle_targets_step_the_way_the_game_moves_its_cursor() {
        // The debug battle's party (Fei, Elly, Citan) and its enemy, on the floor.
        let place = |slot, x, z| Entry {
            rect: Rect { x: 0.0, y: 0.0, width: 1.0, height: 1.0 },
            select: None,
            ground: Some(Ground { slot, x, z, selectable: true }),
        };
        let entries = vec![place(0, 4360, 4160), place(1, 4420, 3840), place(2, 4460, 4000), place(3, 3820, 4040)];
        let from = |selected| Targets { what: "battle_targets", entries: entries.clone(), selected: Some(selected), steps: Steps::Compass };
        assert_eq!(from(0).step_toward(3), Some(GameAction::Left));
        assert_eq!(from(0).step_toward(2), Some(GameAction::Down), "Citan is nearer than Elly");
        // From the enemy Right reaches Fei first, then Down Citan.
        assert_eq!(from(3).step_toward(2), Some(GameAction::Right));
        assert_eq!(from(2).step_toward(1), Some(GameAction::Down));
        // An unselectable target is not a place to move to.
        let mut only = from(0);
        only.entries[3].ground.as_mut().unwrap().selectable = false;
        assert_eq!(only.step_toward(3), None);
    }
}

/// The quantities a shop's buy list proposes, summed (`ShopDetails.amounts`,
/// +0x4654 of the block at `menu_state_current` +0x450, a byte per stock
/// entry): what ALL and NONE watch to stop. The sell list keeps its counts
/// on the stack, so there is none.
pub fn shop_amounts(memory: &dyn GameMemory) -> Option<u32> {
    let state = ram(s32_at(memory, MENU_STATE_CURRENT)?)?;
    let details = ram(s32_at(memory, state + 0x450)?)?;
    bytes(memory, details + 0x4654, 0x30).map(|b| b.iter().map(|v| u32::from(*v)).sum())
}
