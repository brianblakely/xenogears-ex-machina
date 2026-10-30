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
/// The field map number (`mode_field_map_id`, low 12 bits).
const FIELD_MAP_ID: u32 = 0x8004_F34C;
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
/// window's cursor +0x338 and count +0x33A, bytes; its `MenuCard` +0x32C,
/// `MenuFlags` +0x33C and `MenuImages` +0x350). A field's or the world
/// map's menu runs while `menu_state_debug_start` is 0 (`field_run_menu`
/// and the world map's loop clear it for `mode_run_menu`, which sets it
/// again on leaving): before, the block is the last menu's, after, it is
/// freed.
const MENU_STATE_SCREEN: u32 = 0x8005_9460;
const MENU_STATE_CURRENT: u32 = 0x8006_25A0;
const MENU_STATE_DEBUG_START: u32 = 0x8005_9178;
/// The battle's turn (`battle_turn_state`, a pointer): `eventsDone` +0x2DB,
/// the command page +0x2DD, the closed combo or command +0x2E1, AP left
/// +0x2D4.
const BATTLE_TURN_STATE: u32 = 0x800C_3EAC;
/// The battle's interface (`battle_ui`, a pointer): the list cursor shown
/// (+0x9E), its windows shown (+0xB0, a byte each), target choosing
/// (+0xC6), and the event script's portrait (+0xC8) and message text
/// window (+0xC9) drawn; the frame mode (`battle_frame_mode`: 2 while the
/// event script has the frame, its HUD drawing its messages, 1 the turns).
const BATTLE_UI: u32 = 0x800D_2D28;
const BATTLE_FRAME_MODE: u32 = 0x800C_3E4C;
/// The world map's movement (`worldmap_movement_mode`: 1-3 on foot, 2
/// Gears, 4/5/7 vehicles) and scene (`worldmap_mode_index`, 8+ scripted).
const WORLDMAP_MOVEMENT_MODE: u32 = 0x8009_BE10;
const WORLDMAP_MODE_INDEX: u32 = 0x8009_C5A8;
/// `worldmap_menu_requested`: set when the menu button is pressed, cleared
/// once the map is back after the menu (on foot it runs the field's menu
/// from its own loop, `menu_state_debug_start` 0 meanwhile).
const WORLDMAP_MENU_REQUESTED: u32 = 0x8009_D804;
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
    /// A field without the player's control (an event runs) on map `map`
    /// (`mode_field_map_id`); `buttons` the pad buttons its scripts test
    /// (the field's script-driven activities read the pad only so).
    Event { map: u16, buttons: u16 },
    /// A field dialogue the player advances, or answers.
    Dialogue { choice: Option<Choice> },
    /// A menu screen (`screen`, `menu_state_screen`) with its top cursor and
    /// a choice window's; `commands` while its commands take the input (the
    /// field menu's arc: its highlight shown, `MenuFlags.sprite_shown`, over
    /// no command's screen); `open` while a command's screen is open
    /// (`MenuImages.dim`; Cancel backs out of it); neither while the menu
    /// loads, opens, goes between them or closes; `switch` while L1/R1
    /// change the character shown (the arrow marks, `MenuFlags.marks_shown`);
    /// `quantities` while a shop's buy or sell list proposes quantities.
    Menu { screen: u8, cursor: u8, choice: u8, choices: u8, commands: bool, open: bool, switch: bool, quantities: bool },
    /// A battle: the command page, whether the attack inputs take the
    /// buttons, AP left, an item or art list open (`battle_ui.cursorShown`)
    /// and a target being chosen for one (the attack page's own target
    /// choice is part of its attack inputs); `message` while its event
    /// script shows a message (in the script's frame mode, the message's
    /// window, the speaker's portrait or the text: its battle window 0,
    /// `scriptPortraitShown`, `messageShown`).
    Battle { page: u8, attack: bool, ap: u8, list: bool, targeting: bool, message: bool },
    /// The world map: movement mode and scene (its menu is a `Menu`).
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
        Some(3) => world_map(memory),
        Some(4) => Some(Context::Arena { state: u32_at(memory, ARENA_MODE_STATE).unwrap_or(0) }),
        Some(5) => menu(memory),
        _ => None,
    };
    context.unwrap_or(Context::None)
}

/// Whether the game's file screen shows (slot39 `menu_file_screen_run`):
/// the save, copy and delete screens of a field's and the world map's menu
/// and of the disc change, and the title's Load. They list the cards' saves
/// once and act on that listing by name (an Overwrite, a Delete, a Load).
/// It shows from when it begins to read the cards until its command has
/// returned: `MenuCard.mode` (+0x4FE6), nonzero while the card screen
/// polls the cards, is set where `menu_file_screen_enter_card_mode`
/// forgets the listing, before the frames that list the directories and
/// read each save's first block, and cleared by `menu_top_command_run`
/// once the screen has returned; the screen's card mode
/// (`MenuFlags.card_mode` +0x68), set once that first listing is made,
/// holds through the commands that clear `MenuCard.mode` meanwhile (a
/// question, a load). Read while a menu runs (`menu_state_debug_start` 0
/// from a field or the world map, or mode 5), as `read_context` reads the
/// menu.
pub fn file_screen(memory: &dyn GameMemory, mode: Option<u32>) -> bool {
    if mode != Some(5) && u8_at(memory, MENU_STATE_DEBUG_START) != Some(0) {
        return false;
    }
    let Some(state) = u32_at(memory, MENU_STATE_CURRENT).and_then(ram_pointer) else { return false };
    let set = |block: u32, offset: u32| {
        let at = u32_at(memory, state + block).and_then(ram_pointer);
        at.and_then(|at| u8_at(memory, at + offset)).is_some_and(|value| value != 0)
    };
    set(0x33C, 0x68) || set(0x32C, 0x4FE6)
}

fn menu(memory: &dyn GameMemory) -> Option<Context> {
    let screen = u8_at(memory, MENU_STATE_SCREEN)?;
    let state = ram_pointer(u32_at(memory, MENU_STATE_CURRENT)?)?;
    let flags = ram_pointer(u32_at(memory, state + 0x33C)?)?;
    let images = ram_pointer(u32_at(memory, state + 0x350)?)?;
    let flag = |offset: u32| u8_at(memory, flags + offset).is_some_and(|f| f != 0);
    let open = u8_at(memory, images + 0x1192)? != 0;
    Some(Context::Menu {
        screen,
        cursor: u8_at(memory, state + 0x336)?,
        choice: u8_at(memory, state + 0x338)?,
        choices: u8_at(memory, state + 0x33A)?,
        commands: !open && flag(0x04),
        open,
        switch: flag(0x53),
        quantities: matches!(screen, 4 | 5) && flag(0x5A) && flag(0x50),
    })
}

/// The menu a field or the world map runs from its own loop once requested:
/// it loads for some frames after the request, and the map fades back in
/// after it; its state block is read while it runs.
fn map_menu(memory: &dyn GameMemory) -> Option<Context> {
    let running = u8_at(memory, MENU_STATE_DEBUG_START)? == 0;
    Some(running.then(|| menu(memory)).flatten().unwrap_or(Context::Menu {
        screen: 0, cursor: 0, choice: 0, choices: 0, commands: false, open: false, switch: false, quantities: false,
    }))
}

/// The world map, or the menu its menu button opened on foot (in a vehicle
/// the button leaves the map instead).
fn world_map(memory: &dyn GameMemory) -> Option<Context> {
    let movement = u32_at(memory, WORLDMAP_MOVEMENT_MODE).unwrap_or(0);
    if (1..4).contains(&movement) && u32_at(memory, WORLDMAP_MENU_REQUESTED).is_some_and(|requested| requested != 0) {
        return map_menu(memory);
    }
    Some(Context::WorldMap { movement, scene: u32_at(memory, WORLDMAP_MODE_INDEX).unwrap_or(0) })
}

fn field(memory: &dyn GameMemory) -> Option<Context> {
    if u8_at(memory, FIELD_MENU_REQUEST)? != 0xFF {
        return map_menu(memory);
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
    if u32_at(memory, FIELD_PLAYER_CONTROL_POLLED)? == 1 {
        return Some(Context::Walk);
    }
    let map = (u32_at(memory, FIELD_MAP_ID)? & 0xFFF) as u16;
    Some(Context::Event { map, buttons: script_buttons(map) })
}

/// The pad buttons field map `map`'s scripts test (`branch_unless_buttons`
/// 31, 32, fe e2, fe e3), as `python3 -m tools.analysis.events --buttons`
/// finds them on both discs' scripts (maps of the same number together).
pub fn script_buttons(map: u16) -> u16 {
    SCRIPT_BUTTONS.binary_search_by_key(&map, |(m, _)| *m).map_or(0, |i| SCRIPT_BUTTONS[i].1)
}

/// Every map whose scripts test the pad, with the buttons they test.
pub fn script_button_maps() -> &'static [(u16, u16)] {
    &SCRIPT_BUTTONS
}

const SCRIPT_BUTTONS: [(u16, u16); 84] = [
    (0, 0x004c), (4, 0x00e0), (14, 0x0020), (15, 0xf080), (94, 0x0020), (95, 0x0040),
    (98, 0x0020), (100, 0x0040), (118, 0x0020), (123, 0x0020), (125, 0x0022), (129, 0x0020),
    (130, 0xf040), (131, 0xf070), (132, 0xf070), (133, 0xf070), (135, 0xf070), (144, 0xf070),
    (147, 0xf040), (153, 0xf070), (154, 0xf070), (194, 0x0020), (197, 0xf000), (198, 0x0020),
    (228, 0xa000), (293, 0x0020), (296, 0x0020), (305, 0x5040), (306, 0x5040), (307, 0x5040),
    (312, 0xf080), (318, 0xf080), (319, 0x0020), (321, 0xf1dc), (326, 0x0020), (336, 0x0040),
    (406, 0xb080), (423, 0x0020), (424, 0x0020), (426, 0x0020), (428, 0x0020), (430, 0x0020),
    (431, 0x0020), (432, 0xf0f0), (435, 0x0020), (436, 0x0020), (444, 0x0020), (445, 0x0020),
    (447, 0x0020), (448, 0x0020), (449, 0x0020), (450, 0x0020), (475, 0xf040), (476, 0xf040),
    (477, 0xf040), (478, 0xf040), (479, 0xf040), (480, 0xf040), (481, 0xf040), (482, 0xf040),
    (483, 0x0020), (484, 0xf040), (485, 0xf040), (486, 0xf040), (487, 0xf040), (488, 0xf0ff),
    (490, 0x0020), (491, 0xf040), (492, 0xf040), (516, 0x0010), (565, 0xf060), (596, 0x0010),
    (603, 0x0010), (628, 0x0010), (641, 0x0010), (642, 0x0010), (685, 0x0020), (695, 0x0020),
    (699, 0xf0f0), (709, 0x0010), (717, 0x0010), (720, 0xf060), (722, 0x5000), (723, 0xf0ff),
];

fn battle(memory: &dyn GameMemory) -> Option<Context> {
    let turn = ram_pointer(u32_at(memory, BATTLE_TURN_STATE)?)?;
    let page = u8_at(memory, turn + 0x2DD)?;
    let closed = u8_at(memory, turn + 0x2E1)? != 0 || u8_at(memory, turn + 0x2DB)? != 0;
    let ui = u32_at(memory, BATTLE_UI).and_then(ram_pointer);
    let cursor = ui.and_then(|ui| u8_at(memory, ui + 0x9E)).is_some_and(|c| c != 0);
    let choosing = ui.and_then(|ui| u8_at(memory, ui + 0xC6)).is_some_and(|c| c == 1);
    let attack = !closed && matches!(page, 5 | 0x64 | 0x19 | 0x65);
    let shown = |at: u32| ui.and_then(|ui| u8_at(memory, ui + at)).is_some_and(|c| c != 0);
    let script = u8_at(memory, BATTLE_FRAME_MODE)? == 2;
    Some(Context::Battle {
        page,
        attack,
        ap: u8_at(memory, turn + 0x2D4)?,
        list: !closed && cursor && matches!(page, 2 | 8 | 4 | 0x11 | 0x13 | 0x16),
        targeting: !closed && choosing && !cursor && !attack,
        message: script && (shown(0xB0) || shown(0xC8) || shown(0xC9)),
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory::OutOfBounds;

    /// PS1 RAM, all zero but what a test puts there.
    struct Ram(Vec<u8>);

    impl GameMemory for Ram {
        fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
            let at = address.wrapping_sub(0x8000_0000) as usize;
            out.copy_from_slice(self.0.get(at..at + out.len()).ok_or(OutOfBounds { address, len: out.len() })?);
            Ok(())
        }
        fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
            let at = address.wrapping_sub(0x8000_0000) as usize;
            self.0[at..at + data.len()].copy_from_slice(data);
            Ok(())
        }
    }

    #[test]
    fn a_battle_shows_its_event_scripts_message_in_the_scripts_frame_mode() {
        let (ui, turn) = (0x8010_0000u32, 0x8011_0000u32);
        let mut ram = Ram(vec![0; 0x20_0000]);
        ram.write(BATTLE_UI, &ui.to_le_bytes()).unwrap();
        ram.write(BATTLE_TURN_STATE, &turn.to_le_bytes()).unwrap();
        let message = |ram: &Ram| matches!(read_context(ram, Some(2)), Context::Battle { message: true, .. });
        ram.write(BATTLE_FRAME_MODE, &[2]).unwrap();
        assert!(!message(&ram), "the script's frame mode, nothing shown");
        // Its window (battle window 0), the speaker's portrait, the text.
        for at in [0xB0, 0xC8, 0xC9] {
            ram.write(ui + at, &[1]).unwrap();
            assert!(message(&ram), "+{at:#x} shown");
            ram.write(BATTLE_FRAME_MODE, &[1]).unwrap();
            assert!(!message(&ram), "+{at:#x}: the turns' frame mode draws no script message (window 0 is a command's)");
            ram.write(BATTLE_FRAME_MODE, &[2]).unwrap();
            ram.write(ui + at, &[0]).unwrap();
        }
    }

    /// On foot the world map's menu button runs the field's menu from the
    /// map's loop: the context is that menu from the press until the map is
    /// back; in a vehicle the button leaves the map instead.
    #[test]
    fn the_world_maps_menu_is_a_menu_from_its_request_until_the_map_is_back() {
        let (state, flags, images) = (0x8010_0000u32, 0x8011_0000u32, 0x8012_0000u32);
        let mut ram = Ram(vec![0; 0x20_0000]);
        let word = |ram: &mut Ram, address: u32, value: u32| ram.write(address, &value.to_le_bytes()).unwrap();
        ram.write(MENU_STATE_DEBUG_START, &[1]).unwrap();
        word(&mut ram, WORLDMAP_MOVEMENT_MODE, 1);
        word(&mut ram, WORLDMAP_MODE_INDEX, 1);
        let map = Context::WorldMap { movement: 1, scene: 1 };
        assert_eq!(read_context(&ram, Some(3)), map);
        word(&mut ram, WORLDMAP_MENU_REQUESTED, 1);
        let between = Context::Menu { screen: 0, cursor: 0, choice: 0, choices: 0, commands: false, open: false, switch: false, quantities: false };
        assert_eq!(read_context(&ram, Some(3)), between, "the map fades out and the menu loads");
        ram.write(MENU_STATE_DEBUG_START, &[0]).unwrap();
        word(&mut ram, MENU_STATE_CURRENT, state);
        word(&mut ram, state + 0x33C, flags);
        word(&mut ram, state + 0x350, images);
        ram.write(state + 0x336, &[3]).unwrap();
        ram.write(flags + 0x04, &[1]).unwrap();
        assert!(matches!(read_context(&ram, Some(3)), Context::Menu { screen: 0, cursor: 3, commands: true, .. }), "it runs");
        ram.write(MENU_STATE_DEBUG_START, &[1]).unwrap();
        assert_eq!(read_context(&ram, Some(3)), between, "it returned: the map fades back in");
        word(&mut ram, WORLDMAP_MENU_REQUESTED, 0);
        assert_eq!(read_context(&ram, Some(3)), map);
        word(&mut ram, WORLDMAP_MENU_REQUESTED, 1);
        word(&mut ram, WORLDMAP_MOVEMENT_MODE, 5);
        assert_eq!(read_context(&ram, Some(3)), Context::WorldMap { movement: 5, scene: 1 }, "a vehicle's press leaves the map");
    }
}
