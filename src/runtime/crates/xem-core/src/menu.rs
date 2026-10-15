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
//!   The D-pad steps rows and columns. The shops' lists the same way: one
//!   column of eight rows.
//! - A battle's lists (arts, items, the combo setup, a Gear's commands):
//!   their cells as their cursor glyph is placed (`battle_animate_cursor_glyph`).
//! - A battle's target choice (`battle_choose_target`): the candidates
//!   (`battle_target_candidates` 0x800C3E90, 0xFF unused; selectable ones in
//!   `battle_target_candidate_mask` 0x800C3D64; the selected slot
//!   `battle_target_cursor_slot` 0x800C3E2C), each at its sprite
//!   (`battle_area.sprites` 0x800CCB3C) projected as
//!   `battle_get_slot_screen_box` does (`battle_camera_view_matrix`
//!   0x800D30BC, screen 512, offset (160, 164)), as tall as the sprite's
//!   frame (+0x36); where boxes overlap the smallest takes the point. A
//!   direction moves to the nearest candidate that way on the floor
//!   (`battle_find_candidate_in_direction` on the slots' x and z,
//!   `battle_area_slots` 0x800C3EB4), so the way to a candidate is searched
//!   over those moves.
//! - A battle's command ring (`battle_turn_state` +0x2DD, the command page,
//!   while `battle_ui` +0xCB has the command panel drawn): its four slots
//!   (Right, Down, Left, Up; a direction highlights its slot, the
//!   highlighted one's own direction flips the ring's page), a slot the
//!   member cannot use (drawn dim: its shade from 0x2000 names a turn slot
//!   item, +0x1C, that is not 0) taking no pointing, and the page arrows
//!   above it (their glyph 0x54, a Gear's 0x58: up, left and right), each
//!   flipping the ring to the other page's slot its way: its direction
//!   pressed, twice when another slot is highlighted (the first highlights
//!   its own slot, or arms the game's second press past one it cannot use);
//!   an arrow reaches well beyond itself and takes the points it shares
//!   with the slots' labels. The panel is built from constant tables
//!   (`battle_command_panel_build_page`, 8008D598): each page's glyph sets
//!   (`battle_command_panel_pages` 0x800C3000, `battle_command_panel_glyph_sets`
//!   0x800C2F4C: id, x, y, shade; ids from 0x4000 a byte of the acting
//!   member's turn slot, 0xFF not drawn), each glyph's parts placed from its
//!   point by the battle's glyph table (`battle_glyph_table` 0x800D2F5C).
//! - The attack page (5, a Gear's 0x19; 0x64 and 0x65 while the panel
//!   fades out after the first attack): its four attacks, each its button's
//!   glyph (0xE Weak, 0xD Strong, 0xC Fierce, 0xB the end of the
//!   sequence) with the label and digits right of it on its row, which keep
//!   their places through the sequence; and, until the first attack fixes
//!   it (+0x2E9), the attack's target (+0x2E8) among the candidates, which
//!   a direction moves to as `battle_choose_target` does, unconfirmed: the
//!   attacks confirm it, and Confirm is one of them.
use crate::action::GameAction;
use crate::context::Context;
use crate::field::V3;
use crate::memory::GameMemory;
use serde::Serialize;

/// The least distance between the segments `p0`-`p1` and `q0`-`q1` (their
/// nearest points, each clamped to its segment).
fn segment_distance(p0: V3, p1: V3, q0: V3, q1: V3) -> f32 {
    let (d1, d2, r) = (p1.sub(p0), q1.sub(q0), p0.sub(q0));
    let (a, e, f) = (d1.dot(d1), d2.dot(d2), d2.dot(r));
    let (s, t) = if a <= f32::EPSILON && e <= f32::EPSILON {
        (0.0, 0.0)
    } else if a <= f32::EPSILON {
        (0.0, (f / e).clamp(0.0, 1.0))
    } else {
        let c = d1.dot(r);
        if e <= f32::EPSILON {
            ((-c / a).clamp(0.0, 1.0), 0.0)
        } else {
            let b = d1.dot(d2);
            let denominator = a * e - b * b;
            let s = if denominator > f32::EPSILON { ((b * f - c * e) / denominator).clamp(0.0, 1.0) } else { 0.0 };
            let t = (b * s + f) / e;
            if t < 0.0 {
                ((-c / a).clamp(0.0, 1.0), 0.0)
            } else if t > 1.0 {
                (((b - c) / a).clamp(0.0, 1.0), 1.0)
            } else {
                (s, t)
            }
        }
    };
    let gap = p0.add(d1.scale(s)).sub(q0.add(d2.scale(t)));
    gap.dot(gap).sqrt()
}

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
    /// Where its sprite stands in the battle's world (y down) and how tall
    /// it is (its frame's extent): for pointing at it along a ray through
    /// a diorama of the battle ([`Targets::near_ray`]).
    pub sprite: [i32; 3],
    pub height: u16,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize)]
pub struct Entry {
    pub rect: Rect,
    /// The action that selects it directly (`Steps::Direct`).
    pub select: Option<GameAction>,
    /// A button rather than a choice (the ring's page arrows, the attacks):
    /// pointing at it selects nothing, and choosing it presses this once,
    /// or (`twice`) twice.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub press: Option<GameAction>,
    #[serde(skip_serializing_if = "std::ops::Not::not")]
    pub twice: bool,
    /// Its place (`Steps::Compass`).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ground: Option<Ground>,
}

impl Entry {
    fn at(rect: Rect) -> Entry {
        Entry { rect, select: None, press: None, twice: false, ground: None }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize)]
pub struct Targets {
    pub what: &'static str,
    pub entries: Vec<Entry>,
    pub selected: Option<usize>,
    pub steps: Steps,
    /// A chosen entry is confirmed (Confirm, once it is selected), and
    /// pointing at one selects it. Not on the attack page, whose attacks
    /// confirm its target (Confirm is one of them): there a target is
    /// selected only when chosen, and nothing confirms it.
    pub confirm: bool,
}

impl Targets {
    fn new(what: &'static str, entries: Vec<Entry>, selected: Option<usize>, steps: Steps) -> Targets {
        Targets { what, entries, selected, steps, confirm: true }
    }

    /// The entry at a screen point: the first of the interface's own there
    /// (a ring's page arrows before its slots, the attacks over the scene),
    /// else the smallest target (a battle's boxes overlap, a big enemy's
    /// holding the party's).
    pub fn at(&self, point: (f32, f32)) -> Option<usize> {
        let hits = || self.entries.iter().enumerate().filter(move |(_, e)| e.ground.is_none_or(|g| g.selectable) && e.rect.contains(point));
        hits().find(|(_, e)| e.ground.is_none())
            .or_else(|| hits().min_by(|(_, a), (_, b)| (a.rect.width * a.rect.height).total_cmp(&(b.rect.width * b.rect.height))))
            .map(|(i, _)| i)
    }

    /// The battle target a ray in the battle's world points at (a ray
    /// through a diorama of the battle, from `origin` to `within` lengths of
    /// `direction`): of those the game lets its cursor onto, the one whose
    /// upright axis, from its sprite's feet to its height, passes nearest
    /// the ray, within `reach` world units of it or, for a bigger target,
    /// within its frame's half width (as on the screen, where a target's box
    /// is 0.7 times as wide as it is tall).
    pub fn near_ray(&self, origin: V3, direction: V3, within: f32, reach: f32) -> Option<usize> {
        let end = origin.add(direction.scale(within));
        self.entries.iter().enumerate().filter_map(|(index, entry)| {
            let ground = entry.ground.filter(|g| g.selectable)?;
            let height = f32::from(ground.height.max(1));
            let feet = V3::new(ground.sprite[0] as f32, ground.sprite[1] as f32, ground.sprite[2] as f32);
            let d = segment_distance(origin, end, feet, V3::new(feet.x, feet.y - height, feet.z));
            (d <= reach.max(height * 0.35)).then_some((d, index))
        }).min_by(|a, b| a.0.total_cmp(&b.0)).map(|(_, index)| index)
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
fn u16_at(memory: &dyn GameMemory, address: u32) -> Option<u16> { bytes(memory, address, 2).map(|b| u16::from_le_bytes([b[0], b[1]])) }
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
        Some(Entry::at(highlight(hx, hy, width)))
    }).collect()
}

/// The entries `ui` shows that a pointer can select.
pub fn targets(memory: &dyn GameMemory, ui: Context) -> Option<Targets> {
    match ui {
        Context::Menu { screen, cursor, choice, choices, .. } => menu(memory, screen, cursor, choice, choices),
        Context::Dialogue { choice: Some(_) } => dialogue_choice(memory),
        Context::Battle { page, attack: true, .. } => battle_attack(memory, page),
        Context::Battle { targeting: true, .. } => battle_targets(memory),
        Context::Battle { list: true, .. } => battle_list(memory),
        Context::Battle { page, .. } => battle_ring(memory, page),
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
                Some(Entry { select: Some(if i == 2 { GameAction::Left } else { GameAction::Right }), ..Entry::at(Rect { x: x - 4.0, y: y - 6.0, width: 56.0, height: 24.0 }) })
            }).collect::<Option<Vec<_>>>()?;
            return Some(Targets::new("yes_no", entries, Some(if shown[0] != 0 { 0 } else { 1 }), Steps::Direct));
        }
    }
    if let Some(list) = list(memory, screen, state, flags) {
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
        return Some(Targets::new("commands", entries, Some(usize::from(cursor)), Steps::Ring { count: commands, next: GameAction::Up }));
    }
    if flag(0x0A) && (1..=4).contains(&choices) {
        let entries = table(memory, xs, ys, rows_from, usize::from(choices), width)?;
        return Some(Targets::new("choices", entries, Some(usize::from(choice)), Steps::Ring { count: usize::from(choices), next: GameAction::Up }));
    }
    None
}

/// The list cursor's sprite offset from the place `menu_list_cursor_place`
/// computes (measured on the item screen).
const LIST_CURSOR_OFFSET: (i32, i32) = (0, 0);

/// The field menu's or a shop's list under its cursor (cursor 0, the one
/// the D-pad moves), as cells around it.
fn list(memory: &dyn GameMemory, screen: u8, state: u32, flags: u32) -> Option<Targets> {
    let shown = u8_at(memory, flags + 0x50)?;
    if shown == 0 || !matches!(screen, 0 | 4 | 5) {
        return None;
    }
    let cursor = ram(s32_at(memory, state + 0x444)?)?;
    let poly = cursor + u32::from(u8_at(memory, cursor + 0x75)? & 1) * 0x28;
    let x = i32::from(s16_at(memory, poly + 8)?);
    let y = i32::from(s16_at(memory, poly + 0xA)?);
    // Kinds 0-2 (two columns from x 0x18-0x1c, rows from y 0x11-0x14) and
    // 3 (one column at x 0xa0, rows from y 0x14).
    // The shops' stock and sell lists (`item_shop_list_cursor_place`,
    // `gear_shop_list_cursor_place`): one column of eight rows from
    // (0x1c, 0x32), 13 high, wide enough for the quantity and price.
    let (columns, rows, column_width, row_height, left, top) = match (screen, shown) {
        (4 | 5, _) => (1, 8, 0xE8, 13, 0x1C, 0x32),
        (_, 2) => (1, 8, 0x88, 13, 0xA0, 0x14),
        _ => (2, 8, 0x88, 16, 0x18, 0x11),
    };
    let (cx, cy) = (x - LIST_CURSOR_OFFSET.0, y - LIST_CURSOR_OFFSET.1);
    let column = if columns == 2 { ((cx - left + column_width / 2) / column_width).clamp(0, 1) } else { 0 };
    let row = ((cy - top + row_height / 2) / row_height).clamp(0, rows - 1);
    let entries = (0..rows).flat_map(|r| (0..columns).map(move |c| (r, c))).map(|(r, c)| Entry::at(Rect {
        x: (x + (c - column) * column_width) as f32,
        y: (y + (r - row) * row_height) as f32,
        width: (column_width - 4) as f32,
        height: row_height as f32,
    })).collect();
    Some(Targets::new("list", entries, Some((row * columns + column) as usize), Steps::Grid { columns: columns as usize }))
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
        let entries = (0..count).map(|k| Entry::at(Rect { x: x - 16.0, y: y + line * (f32::from(first) + k as f32 - top), width: width + 16.0, height: line })).collect();
        return Some(Targets::new("dialogue_choice", entries, usize::try_from(index).ok(), Steps::Ring { count, next: GameAction::Down }));
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
        let entries = (0..rows).flat_map(|r| (0..columns).map(move |c| (r, c))).map(|(r, c)| Entry::at(Rect {
            x: (x + c * step + GLYPH_OFFSET.0) as f32,
            y: (y + r * height + GLYPH_OFFSET.1) as f32,
            width: (step - 4) as f32,
            height: height as f32,
        })).collect();
        return Some(Targets::new("battle_list", entries, Some((dy / height * columns + column) as usize), Steps::Grid { columns: columns as usize }));
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
    let mask = u16::from_le_bytes(bytes(memory, TARGET_CANDIDATE_MASK, 2)?.try_into().ok()?);
    let cursor = u8_at(memory, TARGET_CURSOR_SLOT)?;
    let (entries, selected) = target_entries(memory, cursor, |slot| mask & (1 << slot) != 0)?;
    (!entries.is_empty()).then_some(Targets::new("battle_targets", entries, selected, Steps::Compass))
}

/// The target candidates, each at its sprite (`selectable`: the game lets
/// its cursor onto that slot), and the entry of slot `cursor`.
fn target_entries(memory: &dyn GameMemory, cursor: u8, selectable: impl Fn(u8) -> bool) -> Option<(Vec<Entry>, Option<usize>)> {
    let candidates = bytes(memory, TARGET_CANDIDATES, 11)?;
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
        // A box as tall as the sprite's frame (`height` +0x36, its extent
        // at its scale: a person's 96 units, a Gear's about 500, a big
        // enemy's more) standing on the point, at least comfortable to
        // point at and at most the screen's height.
        let tall = match u16_at(memory, sprite + 0x36)? {
            0 => 100.0,
            height => f32::from(height),
        };
        let size = (512.0 * tall / depth).clamp(24.0, 224.0);
        if slot == cursor {
            selected = Some(entries.len());
        }
        let ground = Ground {
            slot,
            x: s16_at(memory, place + 0xA)?,
            z: s16_at(memory, place + 0xC)?,
            selectable: selectable(slot),
            sprite: at,
            height: tall as u16,
        };
        entries.push(Entry { ground: Some(ground), ..Entry::at(Rect { x: x - size * 0.35, y: y - size, width: size * 0.7, height: size }) });
    }
    Some((entries, selected))
}

const BATTLE_TURN_STATE: u32 = 0x800C_3EAC;
const COMMAND_PANEL_PAGES: u32 = 0x800C_3000;
const COMMAND_PANEL_GLYPH_SETS: u32 = 0x800C_2F4C;
const BATTLE_GLYPH_TABLE: u32 = 0x800D_2F5C;

/// The ring's slots: the command page of each direction on each of its two
/// pages (layers), on foot and in a Gear, in `battle_pressed_key` order
/// (Right, Down, Left, Up).
const RING_PAGES: [[[u8; 4]; 2]; 2] = [[[1, 2, 3, 4], [7, 8, 9, 0xA]], [[0x10, 0x11, 0x12, 0x13], [0x15, 0x16, 0x17, 0x18]]];

/// The ring slot a command page highlights: (its direction, Right, Down,
/// Left or Up, which of the ring's pages).
pub fn ring_slot(page: u8) -> Option<(usize, usize)> {
    RING_PAGES.iter().flat_map(|unit| unit.iter().enumerate()).find_map(|(layer, pages)| {
        pages.iter().position(|p| *p == page).map(|direction| (direction, layer))
    })
}

/// A command panel glyph as the page draws it: its id in the glyph set, its
/// parts on the screen (none when its turn slot byte is 0xFF) and whether
/// what it stands for can be used (not drawn dim for an item of the turn
/// slot that is not 0).
struct PanelGlyph {
    id: u16,
    parts: Vec<Rect>,
    available: bool,
}

/// The union of rectangles.
fn union(rects: impl IntoIterator<Item = Rect>) -> Option<Rect> {
    rects.into_iter().reduce(|a, b| {
        let (x, y) = (a.x.min(b.x), a.y.min(b.y));
        Rect { x, y, width: (a.x + a.width).max(b.x + b.width) - x, height: (a.y + a.height).max(b.y + b.height) - y }
    })
}

/// The glyph sets command panel page `page` draws, each glyph placed as
/// `battle_command_panel_build_page` places it, while the panel is drawn
/// (`battle_ui` +0xCB).
fn panel_glyph_sets(memory: &dyn GameMemory, page: u8) -> Option<Vec<Vec<PanelGlyph>>> {
    let ui = ram(s32_at(memory, BATTLE_UI)?)?;
    if u8_at(memory, ui + 0xCB)? == 0 || page >= 26 {
        return None;
    }
    let turn = ram(s32_at(memory, BATTLE_TURN_STATE)?)?;
    // The acting member's turn slot: its layout bytes, then its items +0x1C.
    let slot = turn + u32::from(u8_at(memory, turn + 0x2D3)?) * 0x40;
    let sheet = ram(s32_at(memory, BATTLE_GLYPH_TABLE)?)?;
    let lists = ram(s32_at(memory, COMMAND_PANEL_PAGES + u32::from(page) * 4)?)?;
    let mut sets = Vec::new();
    for i in 0..2 {
        if u8_at(memory, lists + i)? == 0xFF {
            break;
        }
        let set = ram(s32_at(memory, COMMAND_PANEL_GLYPH_SETS + u32::from(u8_at(memory, lists + 2 + i)?) * 4)?)?;
        let mut glyphs = Vec::new();
        for record in (set..).step_by(8).take(25) {
            let id = u16_at(memory, record)?;
            if id == 0xFFFF {
                break;
            }
            let glyph = if id >= 0x4000 { u16::from(u8_at(memory, slot + u32::from(id & 0xFF))?) } else { id };
            let shade = u16_at(memory, record + 6)?;
            let available = shade < 0x2000 || u16_at(memory, slot + 0x1C + u32::from(shade & 0xF) * 2)? == 0;
            if glyph == 0xFF {
                glyphs.push(PanelGlyph { id, parts: Vec::new(), available });
                continue;
            }
            let (x, y) = (i32::from(s16_at(memory, record + 2)?), i32::from(s16_at(memory, record + 4)?));
            // The sheet's entry: its part count, then 0x1C bytes a part
            // (size at +4, placement from the glyph's point at +8).
            let entry = sheet + u32::from(u16_at(memory, sheet + (u32::from(glyph) + 2) * 2)?);
            let parts = (0..u32::try_from(s16_at(memory, entry)?).unwrap_or(0)).map(|k| {
                let part = entry + 4 + k * 0x1C;
                Some(Rect {
                    x: (x + i32::from(s16_at(memory, part + 8)?)) as f32,
                    y: (y + i32::from(s16_at(memory, part + 0xA)?)) as f32,
                    width: f32::from(u16_at(memory, part + 4)?),
                    height: f32::from(u16_at(memory, part + 6)?),
                })
            }).collect::<Option<Vec<_>>>()?;
            glyphs.push(PanelGlyph { id, parts, available });
        }
        sets.push(glyphs);
    }
    Some(sets)
}

/// The page arrows' glyph above the ring (on foot, in a Gear).
const RING_ARROWS: [u16; 2] = [0x54, 0x58];
/// How far (screen pixels) pointing at a page arrow reaches beyond the
/// tiny arrow: upward, and away from the ring (the up arrow both ways),
/// over nothing else to point at. Toward the ring it reaches [`ARROW_EDGE`];
/// the slots' labels keep the points they cover.
const ARROW_REACH: f32 = 24.0;
const ARROW_EDGE: f32 = 2.0;

/// Whether two rectangles touch or overlap.
fn touching(a: &Rect, b: &Rect) -> bool {
    a.x <= b.x + b.width && b.x <= a.x + a.width && a.y <= b.y + b.height && b.y <= a.y + a.height
}

/// The command ring of page `page`: its page arrows, each flipping the
/// ring to the other page's slot its way, then its four slots (those the
/// member cannot use taking no pointing).
fn battle_ring(memory: &dyn GameMemory, page: u8) -> Option<Targets> {
    let (highlighted, _) = ring_slot(page)?;
    let ring = panel_glyph_sets(memory, page)?.into_iter().next()?;
    // The slots are the set's first four glyphs, in direction order; a slot
    // the member has no command for is not drawn and takes no pointing.
    if ring.len() < 4 {
        return None;
    }
    // The arrows: the glyph's parts joined where they touch (the one above
    // the ring is two halves), then each reaching away from the ring.
    let mut arrows: Vec<Rect> = Vec::new();
    for part in ring.iter().filter(|g| RING_ARROWS.contains(&g.id)).flat_map(|g| &g.parts) {
        match arrows.iter_mut().find(|arrow| touching(arrow, part)) {
            Some(arrow) => *arrow = union([*arrow, *part])?,
            None => arrows.push(*part),
        }
    }
    let top = arrows.iter().min_by(|a, b| a.y.total_cmp(&b.y)).copied();
    // The arrows come first, so their reach takes the points it shares
    // with the slots' labels.
    let mut entries: Vec<Entry> = arrows.iter().map(|arrow| {
        let middle = top.map_or(arrow.centre().0, |top| top.centre().0);
        let (direction, left, right) = if Some(*arrow) == top {
            (GameAction::Up, middle - ARROW_REACH * 0.75, middle + ARROW_REACH * 0.75)
        } else if arrow.centre().0 < middle {
            (GameAction::Left, arrow.x - ARROW_REACH, arrow.x + arrow.width + ARROW_EDGE)
        } else {
            (GameAction::Right, arrow.x - ARROW_EDGE, arrow.x + arrow.width + ARROW_REACH)
        };
        let rect = Rect { x: left, y: arrow.y - ARROW_REACH, width: right - left, height: arrow.height + ARROW_REACH + ARROW_EDGE };
        Entry { press: Some(direction), twice: direction != COMPASS[highlighted], ..Entry::at(rect) }
    }).collect();
    let nowhere = Rect { x: 0.0, y: 0.0, width: 0.0, height: 0.0 };
    entries.extend(ring.iter().take(4).zip(COMPASS).map(|(glyph, direction)| Entry {
        select: glyph.available.then_some(direction),
        ..Entry::at(union(glyph.parts.iter().copied()).unwrap_or(nowhere))
    }));
    Some(Targets::new("battle_ring", entries, Some(arrows.len() + highlighted), Steps::Direct))
}

/// The attack buttons' glyphs and what each presses.
const ATTACK_BUTTONS: [(u16, GameAction); 4] = [
    (0xE, GameAction::AttackWeak),
    (0xD, GameAction::AttackStrong),
    (0xC, GameAction::AttackFierce),
    (0xB, GameAction::AttackEnd),
];

/// The attack page (`page` 5 or 0x64, a Gear's 0x19 or 0x65): its attacks,
/// then the attack's target choice until the first attack fixes it.
fn battle_attack(memory: &dyn GameMemory, page: u8) -> Option<Targets> {
    let panel = match page {
        0x64 => 5,
        0x65 => 0x19,
        page => page,
    };
    let sets = panel_glyph_sets(memory, panel)?;
    let set = sets.iter().find(|set| set.iter().any(|g| ATTACK_BUTTONS.iter().any(|(id, _)| g.id == *id)))?;
    let buttons: Vec<(Rect, GameAction)> = set.iter()
        .filter_map(|g| Some((union(g.parts.iter().copied())?, ATTACK_BUTTONS.iter().find(|(id, _)| g.id == *id)?.1)))
        .collect();
    // Each label and digit joins the nearest button left of it on its row.
    let mut attacks: Vec<(Rect, GameAction)> = buttons.clone();
    for glyph in set.iter().filter(|g| !ATTACK_BUTTONS.iter().any(|(id, _)| g.id == *id)) {
        let Some(rect) = union(glyph.parts.iter().copied()) else { continue };
        let (_, middle) = rect.centre();
        let owner = buttons.iter().enumerate()
            .filter(|(_, (button, _))| button.x <= rect.x && middle >= button.y && middle < button.y + button.height)
            .max_by(|a, b| a.1.0.x.total_cmp(&b.1.0.x))
            .map(|(i, _)| i);
        if let Some(i) = owner {
            attacks[i].0 = union([attacks[i].0, rect])?;
        }
    }
    let mut entries: Vec<Entry> = attacks.into_iter().map(|(rect, action)| Entry { press: Some(action), ..Entry::at(rect) }).collect();
    let turn = ram(s32_at(memory, BATTLE_TURN_STATE)?)?;
    let mut selected = None;
    if u8_at(memory, turn + 0x2E9)? == 0 {
        let (targets, cursor) = target_entries(memory, u8_at(memory, turn + 0x2E8)?, |_| true)?;
        selected = cursor.map(|i| entries.len() + i);
        entries.extend(targets);
    }
    Some(Targets { confirm: false, ..Targets::new("battle_attack", entries, selected, Steps::Compass) })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory::OutOfBounds;

    #[test]
    fn rings_step_the_short_way_and_direct_entries_their_own() {
        let entry = Entry::at(Rect { x: 0.0, y: 0.0, width: 10.0, height: 10.0 });
        let ring = Targets::new("commands", vec![entry; 7], Some(1), Steps::Ring { count: 7, next: GameAction::Up });
        assert_eq!(ring.step_toward(3), Some(GameAction::Up));
        assert_eq!(ring.step_toward(6), Some(GameAction::Down));
        assert_eq!(ring.step_toward(1), None);
        let yes = Entry { select: Some(GameAction::Left), ..entry };
        let no = Entry { select: Some(GameAction::Right), ..Entry::at(Rect { x: 20.0, ..entry.rect }) };
        let pair = Targets::new("yes_no", vec![yes, no], Some(1), Steps::Direct);
        assert_eq!(pair.step_toward(0), Some(GameAction::Left));
        assert_eq!(pair.at((25.0, 5.0)), Some(1));
    }

    #[test]
    fn grids_step_rows_then_columns() {
        let entry = Entry::at(Rect { x: 0.0, y: 0.0, width: 1.0, height: 1.0 });
        let grid = Targets::new("list", vec![entry; 16], Some(1), Steps::Grid { columns: 2 });
        assert_eq!(grid.step_toward(5), Some(GameAction::Down));
        assert_eq!(grid.step_toward(0), Some(GameAction::Left));
        let below = Targets { selected: Some(4), ..grid.clone() };
        assert_eq!(below.step_toward(1), Some(GameAction::Up));
        assert_eq!(below.step_toward(5), Some(GameAction::Right));
    }

    #[test]
    fn battle_targets_step_the_way_the_game_moves_its_cursor() {
        // The debug battle's party (Fei, Elly, Citan) and its enemy, on the floor.
        let place = |slot, x: i16, z: i16| Entry { ground: Some(Ground { slot, x, z, selectable: true, sprite: [x.into(), 0, z.into()], height: 96 }), ..Entry::at(Rect { x: 0.0, y: 0.0, width: 1.0, height: 1.0 }) };
        let entries = vec![place(0, 4360, 4160), place(1, 4420, 3840), place(2, 4460, 4000), place(3, 3820, 4040)];
        let from = |selected| Targets::new("battle_targets", entries.clone(), Some(selected), Steps::Compass);
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

    #[test]
    fn battle_targets_are_pointed_at_along_a_ray_by_their_upright_axes() {
        // The debug battle at rest: Fei, Elly and Citan, 90 to 96 tall, and
        // its boss, 1523 tall (world y down).
        let target = |slot, sprite: [i32; 3], height| Entry {
            ground: Some(Ground { slot, x: sprite[0] as i16, z: sprite[2] as i16, selectable: true, sprite, height }),
            ..Entry::at(Rect { x: 0.0, y: 0.0, width: 1.0, height: 1.0 })
        };
        let entries = vec![target(0, [4360, 0, 4160], 92), target(1, [4420, 0, 3840], 96), target(2, [4460, 0, 4000], 96), target(3, [3820, 0, 4040], 1523)];
        let mut targets = Targets::new("battle_targets", entries, Some(0), Steps::Compass);
        // A ray from above the seat's side down towards a point (its reach
        // 20 units: a few centimetres on a table).
        let towards = |targets: &Targets, x: f32, y: f32, z: f32| {
            let origin = V3::new(x, y - 1500.0, z - 1500.0);
            targets.near_ray(origin, V3::new(x, y, z).sub(origin).scale(1.5), 1.0, 20.0)
        };
        assert_eq!(towards(&targets, 4360.0, -50.0, 4160.0), Some(0), "at Fei's chest");
        assert_eq!(towards(&targets, 4362.0, -5.0, 4158.0), Some(0), "at his feet");
        assert_eq!(towards(&targets, 4460.0, -60.0, 4000.0), Some(2), "Citan");
        assert_eq!(towards(&targets, 3820.0, -1200.0, 4040.0), Some(3), "high on the boss");
        assert_eq!(towards(&targets, 3820.0 + 400.0, -700.0, 4040.0), Some(3), "within the boss's half width (533 at 1523 tall)");
        assert_eq!(towards(&targets, 4360.0, -50.0, 4160.0 + 120.0), None, "beside Fei, beyond his half width");
        assert_eq!(towards(&targets, 5000.0, 0.0, 5000.0), None, "the floor away from them all");
        // One the game keeps its cursor from is not pointed at.
        targets.entries[0].ground.as_mut().unwrap().selectable = false;
        assert_eq!(towards(&targets, 4360.0, -50.0, 4160.0), None);
    }

    #[test]
    fn overlapping_targets_give_the_point_to_the_smallest() {
        let target = |slot, rect| Entry { ground: Some(Ground { slot, x: 0, z: 0, selectable: true, sprite: [0; 3], height: 96 }), ..Entry::at(rect) };
        let enemy = target(3, Rect { x: 0.0, y: 0.0, width: 200.0, height: 200.0 });
        let fei = target(0, Rect { x: 50.0, y: 50.0, width: 30.0, height: 45.0 });
        let targets = Targets::new("battle_targets", vec![enemy, fei], Some(0), Steps::Compass);
        assert_eq!(targets.at((60.0, 60.0)), Some(1), "Fei, inside the enemy's box");
        assert_eq!(targets.at((150.0, 150.0)), Some(0));
        let mut only_enemy = targets.clone();
        only_enemy.entries[1].ground.as_mut().unwrap().selectable = false;
        assert_eq!(only_enemy.at((60.0, 60.0)), Some(0), "where Fei cannot be chosen, the enemy behind him");
        // The interface's own entries keep their order: a page arrow's
        // reach takes the points it shares with a slot's label.
        let arrow = Entry { press: Some(GameAction::Up), ..Entry::at(Rect { x: 25.0, y: 51.0, width: 9.0, height: 11.0 }) };
        let label = Entry { select: Some(GameAction::Up), ..Entry::at(Rect { x: 28.0, y: 46.0, width: 60.0, height: 15.0 }) };
        let ring = Targets::new("battle_ring", vec![arrow, label], Some(1), Steps::Direct);
        assert_eq!(ring.at((30.0, 55.0)), Some(0));
        assert_eq!(ring.at((40.0, 55.0)), Some(1));
    }

    #[test]
    fn ring_pages_name_their_slot_and_page() {
        assert_eq!(ring_slot(1), Some((0, 0)), "Attack: Right, the first page");
        assert_eq!(ring_slot(9), Some((2, 1)));
        assert_eq!(ring_slot(0x17), Some((2, 1)), "a Gear's Escape");
        assert_eq!(ring_slot(5), None, "the attack page is no slot");
    }

    /// Main RAM for the battle's tables.
    struct Ram(Vec<u8>);

    impl Ram {
        fn put(&mut self, address: u32, data: &[u8]) {
            let at = (address - 0x8000_0000) as usize;
            self.0[at..at + data.len()].copy_from_slice(data);
        }
        fn word(&mut self, address: u32, value: u32) {
            self.put(address, &value.to_le_bytes());
        }
        /// A glyph set's records (id, x, y; shade 0), ended by 0xFFFF.
        fn set(&mut self, address: u32, glyphs: &[(u16, i16, i16)]) {
            for (i, (id, x, y)) in glyphs.iter().enumerate() {
                let record = address + i as u32 * 8;
                self.put(record, &id.to_le_bytes());
                self.put(record + 2, &x.to_le_bytes());
                self.put(record + 4, &y.to_le_bytes());
            }
            self.put(address + glyphs.len() as u32 * 8, &0xFFFFu16.to_le_bytes());
        }
    }

    impl GameMemory for Ram {
        fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
            let at = address.wrapping_sub(0x8000_0000) as usize;
            let bytes = self.0.get(at..at + out.len()).ok_or(OutOfBounds { address, len: out.len() })?;
            out.copy_from_slice(bytes);
            Ok(())
        }
        fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
            self.put(address, data);
            Ok(())
        }
    }

    /// The debug battle's on-foot command panel as the game holds it: Fei's
    /// slots, the ring's set (0) and the attack page's (4), each glyph's
    /// parts as the battle's glyph table places them (measured from the
    /// game's own quads), the enemy (slot 3) straight ahead of the camera.
    fn battle_memory(page: u8, fixed: bool) -> Ram {
        let mut ram = Ram(vec![0; 0x20_0000]);
        let (ui, turn, sheet) = (0x8010_0000, 0x8011_0000, 0x8013_0000);
        ram.word(BATTLE_UI, ui);
        ram.put(ui + 0xCB, &[1]);
        ram.word(BATTLE_TURN_STATE, turn);
        ram.put(turn, &[0x1E, 0x1C, 0x22, 0x25]);
        ram.put(turn + 0x2DD, &[page]);
        ram.put(turn + 0x2E8, &[3, u8::from(fixed)]);
        // Pages 1 (the ring) and 5 (its lists: the ring, then the attacks).
        ram.word(COMMAND_PANEL_PAGES + 4, 0x8012_0000);
        ram.put(0x8012_0000, &[0, 0xFF, 0, 0xFF]);
        ram.word(COMMAND_PANEL_PAGES + 5 * 4, 0x8012_0010);
        ram.put(0x8012_0010, &[1, 0, 0, 4]);
        ram.word(COMMAND_PANEL_GLYPH_SETS, 0x8012_1000);
        ram.set(0x8012_1000, &[(0x4000, 128, 72), (0x4001, 96, 96), (0x4002, 64, 72), (0x4003, 96, 48), (0x56, 96, 64), (0x54, 104, 72), (0x57, 96, 80)]);
        ram.word(COMMAND_PANEL_GLYPH_SETS + 4 * 4, 0x8012_2000);
        ram.set(0x8012_2000, &[(3, 214, 110), (0, 176, 144), (1, 140, 112), (2, 176, 72), (0xB, 192, 112), (0xC, 176, 128), (0xD, 160, 112), (0xE, 176, 96)]);
        // Each glyph's parts (x, y, w, h from its point).
        let glyphs: [(u16, &[(i16, i16, u16, u16)]); 15] = [
            (0x1E, &[(-68, -9, 45, 14)]), (0x1C, &[(-55, -12, 31, 12)]), (0x22, &[(-64, -9, 52, 18)]), (0x25, &[(-63, 0, 46, 12)]),
            (0x56, &[(-76, 1, 72, 14)]), (0x57, &[(-63, -24, 46, 32)]),
            (0x54, &[(-52, -31, 4, 5), (-48, -31, 4, 5), (-77, -19, 5, 7), (-24, -19, 5, 7)]),
            (3, &[(-45, -44, 45, 10)]), (0, &[(-32, -57, 31, 10)]), (1, &[(-21, -45, 29, 10)]), (2, &[(-30, -25, 24, 10)]),
            (0xB, &[(-36, -46, 12, 12)]), (0xC, &[(-44, -42, 12, 12)]), (0xD, &[(-52, -46, 12, 12)]), (0xE, &[(-44, -50, 12, 12)]),
        ];
        let mut entry = sheet + 0x400;
        for (id, parts) in glyphs {
            ram.put(sheet + (u32::from(id) + 2) * 2, &((entry - sheet) as u16).to_le_bytes());
            ram.put(entry, &(parts.len() as i16).to_le_bytes());
            for (k, (x, y, w, h)) in parts.iter().enumerate() {
                let part = entry + 4 + k as u32 * 0x1C;
                ram.put(part + 4, &w.to_le_bytes());
                ram.put(part + 6, &h.to_le_bytes());
                ram.put(part + 8, &x.to_le_bytes());
                ram.put(part + 0xA, &y.to_le_bytes());
            }
            entry += 4 + parts.len() as u32 * 0x1C;
        }
        ram.word(BATTLE_GLYPH_TABLE, sheet);
        // The enemy: the only candidate, at the camera's centre 1000 away.
        ram.put(TARGET_CANDIDATES, &[3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]);
        for (i, value) in [4096i16, 0, 0, 0, 4096, 0, 0, 0, 4096].into_iter().enumerate() {
            ram.put(CAMERA_VIEW_MATRIX + i as u32 * 2, &value.to_le_bytes());
        }
        ram.word(CAMERA_VIEW_MATRIX + 0x1C, 1000);
        ram.word(AREA_SPRITES + 3 * 4, 0x8014_0000);
        ram.put(0x8014_0036, &100u16.to_le_bytes());
        ram
    }

    #[test]
    fn the_ring_points_at_its_slots_and_its_arrows_flip_the_page() {
        let ram = battle_memory(1, false);
        let ring = targets(&ram, Context::Battle { page: 1, attack: false, ap: 7, list: false, targeting: false }).expect("the ring");
        // The arrows, then the slots: Attack (Right) is highlighted; Item
        // (Down) is a press of Down away.
        assert_eq!((ring.what, ring.selected, ring.steps, ring.confirm), ("battle_ring", Some(3), Steps::Direct, true));
        assert_eq!(ring.entries[3].rect, Rect { x: 60.0, y: 63.0, width: 45.0, height: 14.0 });
        assert_eq!(ring.at((70.0, 90.0)), Some(4), "Item");
        assert_eq!(ring.step_toward(4), Some(GameAction::Down));
        assert_eq!(ring.at((20.0, 70.0)), Some(5), "Defense");
        assert_eq!(ring.at((50.0, 55.0)), Some(6), "Arcane");
        // The arrows above it (the up arrow's halves one), each reaching
        // well beyond itself, away from the ring and upward, and each
        // flipping the ring to the other page's slot its way: Attack's own
        // direction once, the others twice (highlight, then flip).
        let arrows: Vec<_> = ring.entries[..3].iter().map(|e| (e.rect, e.press, e.twice)).collect();
        assert_eq!(arrows.len(), 3);
        let presses: Vec<_> = arrows.iter().map(|(_, press, twice)| (*press, *twice)).collect();
        assert_eq!(presses, [(Some(GameAction::Up), true), (Some(GameAction::Left), true), (Some(GameAction::Right), false)]);
        assert_eq!(arrows[0].0, Rect { x: 38.0, y: 17.0, width: 36.0, height: 31.0 }, "the up arrow");
        assert_eq!(arrows[1].0, Rect { x: 3.0, y: 29.0, width: 31.0, height: 33.0 }, "the left one");
        assert_eq!(arrows[2].0, Rect { x: 78.0, y: 29.0, width: 31.0, height: 33.0 }, "the right one");
        for (point, arrow) in [((54.0, 43.0), 0), ((56.0, 20.0), 0), ((29.0, 56.0), 1), ((8.0, 32.0), 1), ((84.0, 56.0), 2), ((105.0, 33.0), 2)] {
            assert_eq!(ring.at(point), Some(arrow), "{point:?}");
        }
        // Where an arrow's reach meets Arcane's label, the arrow takes it.
        assert_eq!((ring.at((33.5, 55.0)), ring.at((78.5, 55.0)), ring.at((40.0, 55.0))), (Some(1), Some(2), Some(6)));
        // A slot the member cannot use (drawn dim) is selected by nothing.
        let mut sealed = battle_memory(1, false);
        // Item and Arcane's shades name items 11 and 10 (set 0's records).
        sealed.put(0x8012_1000 + 8 + 6, &0x208Bu16.to_le_bytes());
        sealed.put(0x8012_1000 + 3 * 8 + 6, &0x208Au16.to_le_bytes());
        sealed.put(0x8011_0000 + 0x1C + 10 * 2, &1u16.to_le_bytes());
        let ring = targets(&sealed, Context::Battle { page: 1, attack: false, ap: 7, list: false, targeting: false }).unwrap();
        assert_eq!((ring.entries[6].select, ring.entries[4].select), (None, Some(GameAction::Down)), "Arcane sealed, Item not");
        // Without the panel there is nothing to point at.
        let mut hidden = battle_memory(1, false);
        hidden.put(0x8010_00CB, &[0]);
        assert!(targets(&hidden, Context::Battle { page: 1, attack: false, ap: 7, list: false, targeting: false }).is_none());
    }

    #[test]
    fn the_attack_page_presses_its_attacks_and_chooses_its_target_unconfirmed() {
        let ram = battle_memory(5, false);
        let attack = targets(&ram, Context::Battle { page: 5, attack: true, ap: 7, list: false, targeting: false }).expect("the attack page");
        assert_eq!((attack.what, attack.confirm), ("battle_attack", false));
        let presses: Vec<_> = attack.entries.iter().map(|e| e.press).collect();
        assert_eq!(presses, [Some(GameAction::AttackEnd), Some(GameAction::AttackFierce), Some(GameAction::AttackStrong), Some(GameAction::AttackWeak), None]);
        // Each button with its label right of it on its row.
        assert_eq!(attack.entries[3].rect, Rect { x: 132.0, y: 46.0, width: 38.0, height: 12.0 }, "Weak: its button and \"1 point\"");
        assert_eq!(attack.at((165.0, 52.0)), Some(3));
        assert_eq!(attack.at((210.0, 70.0)), Some(0), "the end of the sequence");
        assert_eq!(attack.at((125.0, 70.0)), Some(2), "Strong, left of the end");
        // The target, until the first attack fixes it.
        assert_eq!(attack.selected, Some(4));
        let target = attack.entries[4];
        assert_eq!(target.ground.map(|g| (g.slot, g.selectable)), Some((3, true)));
        assert!(target.rect.contains((160.0, 150.0)), "{:?}", target.rect);
        let fixed = targets(&battle_memory(0x64, true), Context::Battle { page: 0x64, attack: true, ap: 6, list: false, targeting: false }).unwrap();
        assert_eq!((fixed.entries.len(), fixed.selected), (4, None), "the attacks keep their places as the panel fades");
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
