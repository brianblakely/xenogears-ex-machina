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
/// The mode the dispatcher enters next (`mode_next_mode`): cleared once that
/// mode's overlay has loaded, just before its entry runs, so an overlay's
/// memory is its mode's only while it is 0.
const MODE_NEXT_MODE: u32 = 0x8001_8088;
/// The field's pictures (`field_picture_table`, field_effect.c), shown by
/// Select where their item is held: eight words a picture, its map first
/// and its item fifth, ended by map 0xFFFF. Read no further than its ten
/// pictures, whatever its memory holds while the field loads.
const FIELD_PICTURE_TABLE: u32 = 0x800A_F47C;
const FIELD_PICTURES: u32 = 10;
/// The game data in use (`game_current_data`, a pointer): the inventory's
/// item counts at +0x1F90 and their ids at +0x2026, 150 of each.
const GAME_CURRENT_DATA: u32 = 0x8005_A39C;
const ITEM_COUNTS: u32 = 0x1F90;
const ITEM_IDS: u32 = 0x2026;
const ITEMS: usize = 150;
/// The sound driver's flags (`sound_driver_flags`): 0x40 while
/// `sound_silence_voices` holds the voices, as the pause screens do.
const SOUND_DRIVER_FLAGS: u32 = 0x8005_957C;
/// Battling's menu paused (`arena_menu_paused`, the arena overlay's byte).
const ARENA_MENU_PAUSED: u32 = 0x8009_2758;

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

/// What the field says of the player's buttons ([`field_controls`]):
/// whether it runs, whether the player walks there (the player control read
/// the pad in the last event pass, with no dialogue open, and no menu
/// runs), whether a menu runs, and whether Select shows the map's picture
/// there (else the hosts' first-person toggle).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct FieldControls {
    pub field: bool,
    pub walking: bool,
    pub menu: bool,
    pub picture: bool,
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

/// Whether `overlay`'s mode runs with its overlay loaded: the mode runs and
/// no next one waits (the host learns the next mode before the dispatcher
/// loads its overlay).
fn overlay_runs(memory: &dyn GameMemory, mode: Option<u32>, overlay: u32) -> bool {
    mode == Some(overlay) && u32_at(memory, MODE_NEXT_MODE) == Some(0)
}

/// Whether the game shows a pause screen: the sound driver holds the voices
/// for one, or Battling's menu is paused (read while Battling runs).
pub fn game_paused(memory: &dyn GameMemory, mode: Option<u32>) -> bool {
    u16_at(memory, SOUND_DRIVER_FLAGS).is_some_and(|flags| flags & 0x40 != 0)
        || (mode == Some(4) && u8_at(memory, ARENA_MENU_PAUSED).is_some_and(|paused| paused != 0))
}

/// Whether the battle's attack inputs take the player's buttons (the
/// keyboard's combo layout): its on-foot and Gear attack pages until the
/// combo closes or its events finish (`Context::Battle`'s `attack`), read
/// once the battle runs with its overlay loaded. Until the battle allocates
/// its turn state, the pointer holds what the overlay's memory held: it is
/// followed only into RAM.
pub fn combo_input(memory: &dyn GameMemory, mode: Option<u32>) -> bool {
    overlay_runs(memory, mode, 2) && matches!(battle(memory), Some(Context::Battle { attack: true, .. }))
}

/// What the field says of the player's buttons, read while the field runs
/// with its overlay loaded: a menu runs (the menu button's or a script's),
/// else the player walks where the player control read the pad in the last
/// event pass (which it does with no dialogue open), and Select shows the
/// map's picture where the map has one and its item is held.
pub fn field_controls(memory: &dyn GameMemory, mode: Option<u32>) -> FieldControls {
    let field = mode == Some(1);
    if !overlay_runs(memory, mode, 1) {
        return FieldControls { field, ..FieldControls::default() };
    }
    let menu = u32_at(memory, FIELD_MENU_REQUEST).is_some_and(|request| request != 0xFF);
    let walking = !menu && u32_at(memory, FIELD_PLAYER_CONTROL_POLLED) == Some(1);
    FieldControls { field, walking, menu, picture: picture_shown(memory) }
}

/// The pad buttons the running field map's scripts test
/// ([`script_buttons`]), whatever the context shows there: an activity's
/// scripts go on testing the pad while one of them holds a window open, or
/// beside the player's own control (the `event` context carries them for a
/// scene alone). Read while the field runs with its overlay loaded; none
/// otherwise.
pub fn field_script_buttons(memory: &dyn GameMemory, mode: Option<u32>) -> u16 {
    if !overlay_runs(memory, mode, 1) {
        return 0;
    }
    u32_at(memory, FIELD_MAP_ID).map_or(0, |map| script_buttons((map & 0xFFF) as u16))
}

/// Whether Select shows the field map's picture, as `field_picture_show`
/// finds it: the map (`mode_field_map_id` below its flag bits) has one
/// and the party holds its item.
fn picture_shown(memory: &dyn GameMemory) -> bool {
    let Some(map) = u32_at(memory, FIELD_MAP_ID).map(|id| id & 0x3FFF) else { return false };
    for picture in 0..FIELD_PICTURES {
        let entry = FIELD_PICTURE_TABLE + picture * 32;
        match u32_at(memory, entry) {
            Some(0xFFFF) | None => return false,
            Some(shown) if shown == map => return u32_at(memory, entry + 16).is_some_and(|item| item_held(memory, item)),
            Some(_) => {}
        }
    }
    false
}

/// Whether the party holds some of `item`, as
/// `field_inventory_is_item_not_held` reads the game data's inventory.
fn item_held(memory: &dyn GameMemory, item: u32) -> bool {
    let Some(data) = u32_at(memory, GAME_CURRENT_DATA).and_then(ram_pointer) else { return false };
    let (Some(ids), Some(counts)) = (read::<ITEMS>(memory, data + ITEM_IDS), read::<ITEMS>(memory, data + ITEM_COUNTS)) else {
        return false;
    };
    ids.iter().zip(counts).any(|(&id, count)| u32::from(id) == item && count != 0)
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

    /// Where a test keeps the game data (`game_current_data`).
    const DATA: u32 = 0x8012_0000;

    /// A field that runs, its player walking on map 7 (with a flag bit above
    /// its number), its picture table two invented pictures (map 7 with item
    /// 0x40, map 9 with 0x41) and nothing held.
    fn field_ram() -> Ram {
        let mut ram = Ram(vec![0; 0x20_0000]);
        let word = |ram: &mut Ram, at: u32, value: u32| ram.write(at, &value.to_le_bytes()).unwrap();
        for (n, (map, item)) in [(7, 0x40), (9, 0x41)].into_iter().enumerate() {
            word(&mut ram, FIELD_PICTURE_TABLE + n as u32 * 32, map);
            word(&mut ram, FIELD_PICTURE_TABLE + n as u32 * 32 + 16, item);
        }
        word(&mut ram, FIELD_PICTURE_TABLE + 2 * 32, 0xFFFF);
        word(&mut ram, GAME_CURRENT_DATA, DATA);
        word(&mut ram, FIELD_MENU_REQUEST, 0xFF);
        word(&mut ram, FIELD_PLAYER_CONTROL_POLLED, 1);
        word(&mut ram, FIELD_MAP_ID, 0x4007);
        ram
    }

    /// `count` of `item` in the inventory's last slot.
    fn hold(ram: &mut Ram, item: u8, count: u8) {
        ram.write(DATA + ITEM_IDS + 149, &[item]).unwrap();
        ram.write(DATA + ITEM_COUNTS + 149, &[count]).unwrap();
    }

    const WALKING: FieldControls = FieldControls { field: true, walking: true, menu: false, picture: false };

    #[test]
    fn select_shows_the_maps_picture_only_with_its_item_held() {
        let mut ram = field_ram();
        assert_eq!(field_controls(&ram, Some(1)), WALKING, "the picture's item is not held");
        hold(&mut ram, 0x41, 1);
        assert_eq!(field_controls(&ram, Some(1)), WALKING, "another map's item");
        hold(&mut ram, 0x40, 0);
        assert_eq!(field_controls(&ram, Some(1)), WALKING, "none of it left");
        hold(&mut ram, 0x40, 1);
        assert_eq!(field_controls(&ram, Some(1)), FieldControls { picture: true, ..WALKING }, "the map's id below its flag bits");
        ram.write(FIELD_MAP_ID, &8u32.to_le_bytes()).unwrap();
        assert_eq!(field_controls(&ram, Some(1)), WALKING, "a map without a picture");
    }

    #[test]
    fn the_player_walks_while_its_control_reads_the_pad_and_no_menu_runs() {
        let mut ram = field_ram();
        for request in [0x80u32, 0] {
            ram.write(FIELD_MENU_REQUEST, &request.to_le_bytes()).unwrap();
            let menu = FieldControls { field: true, menu: true, ..FieldControls::default() };
            assert_eq!(field_controls(&ram, Some(1)), menu, "the menu runs ({request:#x}: the button's, a script's)");
        }
        ram.write(FIELD_MENU_REQUEST, &0xFFu32.to_le_bytes()).unwrap();
        ram.write(FIELD_PLAYER_CONTROL_POLLED, &0u32.to_le_bytes()).unwrap();
        assert_eq!(field_controls(&ram, Some(1)), FieldControls { field: true, ..FieldControls::default() }, "a dialogue, an event");
    }

    #[test]
    fn a_fields_script_buttons_are_its_maps_whatever_its_context_and_none_outside_it() {
        // Map 565's console (its scripts test the directions, Confirm and
        // Cancel), the player walking: the context alone carries no buttons.
        let mut ram = field_ram();
        ram.write(FIELD_MAP_ID, &(0x4000u32 | 565).to_le_bytes()).unwrap();
        assert_eq!(read_context(&ram, Some(1)), Context::Walk);
        assert_eq!(field_script_buttons(&ram, Some(1)), 0xf060);
        // A window open: a dialogue's context, the same buttons.
        ram.write(FIELD_OPEN_WINDOWS, &1u16.to_le_bytes()).unwrap();
        assert!(matches!(read_context(&ram, Some(1)), Context::Dialogue { .. }));
        assert_eq!(field_script_buttons(&ram, Some(1)), 0xf060);
        // A scene's context carries them itself.
        ram.write(FIELD_OPEN_WINDOWS, &0u16.to_le_bytes()).unwrap();
        ram.write(FIELD_PLAYER_CONTROL_POLLED, &0u32.to_le_bytes()).unwrap();
        assert_eq!(read_context(&ram, Some(1)), Context::Event { map: 565, buttons: 0xf060 });
        // A map whose scripts test none; another mode; the field's overlay
        // not loaded yet.
        ram.write(FIELD_MAP_ID, &30u32.to_le_bytes()).unwrap();
        assert_eq!(field_script_buttons(&ram, Some(1)), 0);
        ram.write(FIELD_MAP_ID, &565u32.to_le_bytes()).unwrap();
        for mode in [None, Some(2), Some(3), Some(5)] {
            assert_eq!(field_script_buttons(&ram, mode), 0, "{mode:?}");
        }
        ram.write(MODE_NEXT_MODE, &1u32.to_le_bytes()).unwrap();
        assert_eq!(field_script_buttons(&ram, Some(1)), 0, "the overlay still loads");
    }

    #[test]
    fn a_picture_table_without_its_end_is_read_no_further_than_its_pictures() {
        // Memory the loading field has not filled yet: no end within ten
        // pictures, and an eleventh that would match.
        let mut ram = field_ram();
        hold(&mut ram, 0x40, 1);
        for picture in 0..11u32 {
            ram.write(FIELD_PICTURE_TABLE + picture * 32, &(0x100 + picture).to_le_bytes()).unwrap();
            ram.write(FIELD_PICTURE_TABLE + picture * 32 + 16, &0x40u32.to_le_bytes()).unwrap();
        }
        ram.write(FIELD_MAP_ID, &0x109u32.to_le_bytes()).unwrap();
        assert!(field_controls(&ram, Some(1)).picture, "the tenth is read");
        ram.write(FIELD_MAP_ID, &0x10Au32.to_le_bytes()).unwrap();
        assert!(!field_controls(&ram, Some(1)).picture, "the eleventh is not");
    }

    #[test]
    fn an_overlays_memory_is_read_only_while_its_mode_runs_with_it_loaded() {
        let mut ram = field_ram();
        hold(&mut ram, 0x40, 1);
        for mode in [None, Some(0), Some(2), Some(3), Some(4), Some(5), Some(6)] {
            assert_eq!(field_controls(&ram, mode), FieldControls::default(), "mode {mode:?} has no field overlay");
        }
        // The next mode waits for its overlay: the battle pointer's address
        // still holds a texture word of the world map's.
        ram.write(BATTLE_TURN_STATE, &0x7FFF_7FFFu32.to_le_bytes()).unwrap();
        ram.write(MODE_NEXT_MODE, &2u32.to_le_bytes()).unwrap();
        assert!(!combo_input(&ram, Some(2)));
        ram.write(MODE_NEXT_MODE, &1u32.to_le_bytes()).unwrap();
        assert_eq!(field_controls(&ram, Some(1)), FieldControls { field: true, ..FieldControls::default() });
    }

    /// The battle's turn state at `at`: its page, closed combo and finished
    /// events.
    fn turn(ram: &mut Ram, at: u32, page: u8, closed: u8, done: u8) {
        ram.write(BATTLE_TURN_STATE, &at.to_le_bytes()).unwrap();
        ram.write(at + 0x2DD, &[page]).unwrap();
        ram.write(at + 0x2E1, &[closed]).unwrap();
        ram.write(at + 0x2DB, &[done]).unwrap();
    }

    #[test]
    fn a_starting_battle_follows_no_turn_pointer_outside_ram() {
        // The battle runs but has not allocated its turn state yet: the
        // pointer holds the overlay memory's word, past RAM's end or below it.
        let mut ram = Ram(vec![0; 0x20_0000]);
        for word in [0x7FFF_7FFFu32, 0x8020_0000 - 4, 0x8F00_0000, 0xFFFF_FFF0, 0x1F80_0000] {
            ram.write(BATTLE_TURN_STATE, &word.to_le_bytes()).unwrap();
            assert!(!combo_input(&ram, Some(2)), "{word:#x}");
        }
        turn(&mut ram, 0x8010_0000, 5, 0, 0);
        assert!(combo_input(&ram, Some(2)), "the allocated state, in RAM");
    }

    #[test]
    fn a_battle_takes_the_buttons_only_on_its_open_attack_pages() {
        let mut ram = Ram(vec![0; 0x20_0000]);
        for page in [5, 0x19, 0x64, 0x65] {
            turn(&mut ram, 0x8010_0000, page, 0, 0);
            assert!(combo_input(&ram, Some(2)), "page {page:#x}");
            assert!(!combo_input(&ram, Some(3)), "page {page:#x} outside a battle");
            turn(&mut ram, 0x8010_0000, page, 1, 0);
            assert!(!combo_input(&ram, Some(2)), "page {page:#x}: the combo closed");
            turn(&mut ram, 0x8010_0000, page, 0, 1);
            assert!(!combo_input(&ram, Some(2)), "page {page:#x}: its events finished");
        }
        turn(&mut ram, 0x8010_0000, 0, 0, 0);
        assert!(!combo_input(&ram, Some(2)), "the command page");
    }

    #[test]
    fn a_pause_is_the_sound_drivers_hold_or_battlings_menu_while_battling_runs() {
        let mut ram = Ram(vec![0; 0x20_0000]);
        assert!(!game_paused(&ram, Some(1)));
        ram.write(ARENA_MENU_PAUSED, &[1]).unwrap();
        assert!(!game_paused(&ram, Some(1)), "the arena overlay's byte outside Battling");
        assert!(game_paused(&ram, Some(4)));
        ram.write(ARENA_MENU_PAUSED, &[0]).unwrap();
        ram.write(SOUND_DRIVER_FLAGS, &0x40u16.to_le_bytes()).unwrap();
        for mode in [Some(1), Some(2), Some(3), Some(4), None] {
            assert!(game_paused(&ram, mode), "{mode:?}");
        }
    }
}
