//! What the original game's state says the player's input reaches now: the
//! interface context hosts translate pointer, wheel, touch and headset
//! input for, and agents read (`context`). Read from the game's own memory
//! at its original addresses, between steps; nothing is written. An
//! overlay's memory is read only while its mode runs (it is reused
//! otherwise).
use crate::memory::GameMemory;
use serde::Serialize;

/// The field: `field_menu_request` (0xFF while no menu runs; the menu runs
/// inside the field mode, `field_run_menu`), `field_player_control_polled`
/// (1 when the player's control read the pad in the last event pass) and
/// `field_work.open_windows` (a bit per open dialogue window).
const FIELD_MENU_REQUEST: u32 = 0x800A_DB64;
const FIELD_PLAYER_CONTROL_POLLED: u32 = 0x800A_DB68;
const FIELD_OPEN_WINDOWS: u32 = 0x800B_2174;
/// `field_dialogue_windows[4]`: `busy` 0 while in use, `age` 0 for the
/// newest (the one that takes input), the choice (`status` 0 while open,
/// its first line, count and index).
const DIALOGUE_WINDOWS: u32 = 0x800C_2698;
const DIALOGUE_WINDOW_SIZE: u32 = 0x498;
const WINDOW_CHOICE: u32 = 0x37C;
const WINDOW_BUSY: u32 = 0x40E;
const WINDOW_AGE: u32 = 0x410;
/// The menus: the screen (`menu_state_screen`: 0 the field menu, 1 member
/// change, 2 load, 3 name entry, 4 shop, 5 Gear shop, 6 disc change) and
/// the state block (`menu_state_current`: the top cursor +0x336, a choice
/// window's cursor +0x338 and count +0x33A, bytes).
const MENU_STATE_SCREEN: u32 = 0x8005_9460;
const MENU_STATE_CURRENT: u32 = 0x8006_25A0;
/// The battle's turn (`battle_turn_state`, a pointer): `eventsDone` +0x2DB,
/// the command page +0x2DD, the closed combo or command +0x2E1, AP left
/// +0x2D4.
const BATTLE_TURN_STATE: u32 = 0x800C_3EAC;
/// The battle's interface (`battle_ui`, a pointer): the list cursor shown
/// (+0x9E) and target choosing (+0xC6).
const BATTLE_UI: u32 = 0x800D_2D28;
/// The world map's movement (`worldmap_movement_mode`: 1-3 on foot, 2
/// Gears, 4/5/7 vehicles) and scene (`worldmap_mode_index`, 8+ scripted).
const WORLDMAP_MOVEMENT_MODE: u32 = 0x8009_BE10;
const WORLDMAP_MODE_INDEX: u32 = 0x8009_C5A8;
/// Battling's state (`arena_mode_state`: 0 menu, 1 bout, 4 replay, 5 demo
/// or exit, 6 winner, 7 tutorial, 8 result).
const ARENA_MODE_STATE: u32 = 0x8009_2790;

/// A dialogue choice: its options and the highlighted one.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
pub struct Choice {
    pub count: u16,
    pub index: u16,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum Context {
    /// Booting, a movie, a mode change: nothing interprets the pad's
    /// directions as navigation.
    None,
    /// A field, the player walking.
    Walk,
    /// A field without the player's control (an event runs).
    Event,
    /// A field dialogue the player advances, or answers.
    Dialogue { choice: Option<Choice> },
    /// A menu screen (`screen`, `menu_state_screen`) with its top cursor and
    /// a choice window's; `switch` while L1/R1 change the character shown
    /// (the arrow marks, `MenuFlags.marks_shown`); `quantities` while a
    /// shop's buy or sell list proposes quantities.
    Menu { screen: u8, cursor: u8, choice: u8, choices: u8, switch: bool, quantities: bool },
    /// A battle: the command page, whether the attack inputs take the
    /// buttons, AP left, an item or art list open (`battle_ui.cursorShown`)
    /// and a target being chosen.
    Battle { page: u8, attack: bool, ap: u8, list: bool, targeting: bool },
    /// The world map: movement mode and scene.
    WorldMap { movement: u32, scene: u32 },
    /// Battling (the arena): its state.
    Arena { state: u32 },
}

impl Context {
    /// Whether directions move a cursor (menus, choices, the battle's
    /// pages) rather than a character.
    pub fn navigates(&self) -> bool {
        matches!(self, Context::Menu { .. } | Context::Dialogue { choice: Some(_) } | Context::Battle { attack: false, .. })
    }
}

fn read<const N: usize>(memory: &dyn GameMemory, address: u32) -> Option<[u8; N]> {
    let mut bytes = [0; N];
    memory.read(address, &mut bytes).ok().map(|()| bytes)
}
fn u8_at(memory: &dyn GameMemory, address: u32) -> Option<u8> { read::<1>(memory, address).map(|b| b[0]) }
fn i16_at(memory: &dyn GameMemory, address: u32) -> Option<i16> { read(memory, address).map(i16::from_le_bytes) }
fn u16_at(memory: &dyn GameMemory, address: u32) -> Option<u16> { read(memory, address).map(u16::from_le_bytes) }
fn u32_at(memory: &dyn GameMemory, address: u32) -> Option<u32> { read(memory, address).map(u32::from_le_bytes) }

/// A pointer into PS1 RAM, or none.
fn ram_pointer(value: u32) -> Option<u32> {
    (0x8000_0000..0x8020_0000).contains(&value).then_some(value)
}

/// The context of `mode` (`Services::mode`) as `memory` holds it.
pub fn read_context(memory: &dyn GameMemory, mode: Option<u32>) -> Context {
    let context = match mode {
        Some(1) => field(memory),
        Some(2) => battle(memory),
        Some(3) => Some(Context::WorldMap {
            movement: u32_at(memory, WORLDMAP_MOVEMENT_MODE).unwrap_or(0),
            scene: u32_at(memory, WORLDMAP_MODE_INDEX).unwrap_or(0),
        }),
        Some(4) => Some(Context::Arena { state: u32_at(memory, ARENA_MODE_STATE).unwrap_or(0) }),
        Some(5) => menu(memory),
        _ => None,
    };
    context.unwrap_or(Context::None)
}

fn menu(memory: &dyn GameMemory) -> Option<Context> {
    let screen = u8_at(memory, MENU_STATE_SCREEN)?;
    let state = ram_pointer(u32_at(memory, MENU_STATE_CURRENT)?)?;
    let flags = ram_pointer(u32_at(memory, state + 0x33C)?)?;
    let flag = |offset: u32| u8_at(memory, flags + offset).is_some_and(|f| f != 0);
    Some(Context::Menu {
        screen,
        cursor: u8_at(memory, state + 0x336)?,
        choice: u8_at(memory, state + 0x338)?,
        choices: u8_at(memory, state + 0x33A)?,
        switch: flag(0x53),
        quantities: matches!(screen, 4 | 5) && flag(0x5A) && flag(0x50),
    })
}

fn field(memory: &dyn GameMemory) -> Option<Context> {
    if u8_at(memory, FIELD_MENU_REQUEST)? != 0xFF {
        // The menu loads for some frames after the request; its state block
        // (left behind by the last menu until then) is read once it runs.
        return Some(menu(memory).unwrap_or(Context::Menu { screen: 0, cursor: 0, choice: 0, choices: 0, switch: false, quantities: false }));
    }
    if u16_at(memory, FIELD_OPEN_WINDOWS)? != 0 {
        let mut choice = None;
        for i in 0..4 {
            let window = DIALOGUE_WINDOWS + i * DIALOGUE_WINDOW_SIZE;
            if i16_at(memory, window + WINDOW_BUSY)? != 0 || i16_at(memory, window + WINDOW_AGE)? != 0 {
                continue;
            }
            if i16_at(memory, window + WINDOW_CHOICE)? == 0 {
                let count = u16_at(memory, window + WINDOW_CHOICE + 4)?;
                let index = u16_at(memory, window + WINDOW_CHOICE + 6)?;
                choice = Some(Choice { count, index });
            }
        }
        return Some(Context::Dialogue { choice });
    }
    Some(if u32_at(memory, FIELD_PLAYER_CONTROL_POLLED)? == 1 { Context::Walk } else { Context::Event })
}

fn battle(memory: &dyn GameMemory) -> Option<Context> {
    let turn = ram_pointer(u32_at(memory, BATTLE_TURN_STATE)?)?;
    let page = u8_at(memory, turn + 0x2DD)?;
    let closed = u8_at(memory, turn + 0x2E1)? != 0 || u8_at(memory, turn + 0x2DB)? != 0;
    let ui = u32_at(memory, BATTLE_UI).and_then(ram_pointer);
    let cursor = ui.and_then(|ui| u8_at(memory, ui + 0x9E)).is_some_and(|c| c != 0);
    let choosing = ui.and_then(|ui| u8_at(memory, ui + 0xC6)).is_some_and(|c| c == 1);
    Some(Context::Battle {
        page,
        attack: !closed && matches!(page, 5 | 0x64 | 0x19 | 0x65),
        ap: u8_at(memory, turn + 0x2D4)?,
        list: !closed && cursor && matches!(page, 2 | 8 | 4 | 0x11 | 0x13 | 0x16),
        targeting: !closed && choosing && !cursor,
    })
}
