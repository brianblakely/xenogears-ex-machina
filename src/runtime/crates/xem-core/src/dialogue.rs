//! Field dialogue as the game takes the player's Confirm, for auto-advance
//! (docs/runtime.md, Auto-advance dialogue). The field's draw pass
//! (`field_dialogue_draw_windows`, 8008004C) gives a press of Confirm to
//! every dialogue window that is in use (`busy` 0), done opening (`timer`
//! 0) and either selected (+0x412) or the newest (`age` 0): the press ends
//! the window's wait (`window_end_wait`) and stores the window's choice
//! line in its owner actor, so a press answers a choice the window shows
//! (`choice.status` 0). A choice also opens on the window once its text
//! has ended: `a9 start_choice` (8009BC98) waits for the end, and every a9
//! on both discs directly follows its message instruction, so while the
//! message shows, the window's owner actor (+0x416) rests on that a9; a
//! press at the end would answer, or skip, the choice before it shows.
//! Read from the game's memory between steps; nothing is written.
use crate::memory::GameMemory;
use serde::Serialize;

/// `field_menu_request` (0xFF while no menu runs) and
/// `field_work.open_windows` (a bit per open window).
const FIELD_MENU_REQUEST: u32 = 0x800A_DB64;
const FIELD_OPEN_WINDOWS: u32 = 0x800B_2174;
/// `field_dialogue_windows[4]` (field_dialogue.h): the text `Window` at
/// +0x18 (its flags +0x10, bit 8 while it waits, and what it waits for
/// +0x6B: 1 its text's end, 2 a page, 3 a pause), the choice's status
/// +0x37C, `timer` +0x408, `busy` +0x40E, `age` +0x410, the selection
/// +0x412 and the owner's descriptor index +0x416.
const WINDOWS: u32 = 0x800C_2698;
const WINDOW_SIZE: u32 = 0x498;
const TEXT_FLAGS: u32 = 0x18 + 0x10;
const TEXT_WAIT: u32 = 0x18 + 0x6B;
const CHOICE_STATUS: u32 = 0x37C;
const TIMER: u32 = 0x408;
const BUSY: u32 = 0x40E;
const AGE: u32 = 0x410;
const SELECTED: u32 = 0x412;
const OWNER: u32 = 0x416;
const WAITING: u16 = 8;
/// `field_view.components` (0x800AFB0C): the descriptor count (+0) and the
/// descriptors (+4), 0x5C bytes each with their actor (+0x4C); an actor's
/// script slots (+0x8C, eight bytes each, the resume PC first) and its
/// selected slot (+0xCE); `field_event_bytecode`, the PCs' base.
const COMPONENTS: u32 = 0x800A_FB0C;
const DESCRIPTOR_SIZE: u32 = 0x5C;
const DESCRIPTOR_ACTOR: u32 = 0x4C;
const ACTOR_SLOTS: u32 = 0x8C;
const ACTOR_SLOT: u32 = 0xCE;
const EVENT_BYTECODE: u32 = 0x800A_DC00;
const START_CHOICE: u8 = 0xA9;

/// What a press of Confirm would do to the field's dialogue now.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum Prompt {
    /// No window that takes the press waits for it.
    None,
    /// A window that takes the press waits for it (its prompt shows): for
    /// its text's end (1), a page (2) or a pause (3).
    Waiting { wait: u8 },
    /// A window that takes the press shows a choice, or its owner opens
    /// one when its text ends: the press would answer it.
    Choice,
}

fn read<const N: usize>(memory: &dyn GameMemory, address: u32) -> Option<[u8; N]> {
    let mut bytes = [0; N];
    memory.read(address, &mut bytes).ok().map(|()| bytes)
}
fn u8_at(memory: &dyn GameMemory, address: u32) -> Option<u8> { read::<1>(memory, address).map(|b| b[0]) }
fn i16_at(memory: &dyn GameMemory, address: u32) -> Option<i16> { read(memory, address).map(i16::from_le_bytes) }
fn u16_at(memory: &dyn GameMemory, address: u32) -> Option<u16> { read(memory, address).map(u16::from_le_bytes) }
fn u32_at(memory: &dyn GameMemory, address: u32) -> Option<u32> { read(memory, address).map(u32::from_le_bytes) }
fn ram(pointer: u32) -> Option<u32> {
    (0x8000_0000..0x8020_0000).contains(&pointer).then_some(pointer)
}

/// The instruction field actor `descriptor` runs next: its selected
/// slot's resume PC into the bytecode.
fn next_instruction(memory: &dyn GameMemory, descriptor: i16) -> Option<u8> {
    let count = u32_at(memory, COMPONENTS)?;
    let index = u32::try_from(descriptor).ok().filter(|index| *index < count.min(256))?;
    let descriptors = ram(u32_at(memory, COMPONENTS + 4)?)?;
    let actor = ram(u32_at(memory, descriptors + index * DESCRIPTOR_SIZE + DESCRIPTOR_ACTOR)?)?;
    let slot = u32::from(u8_at(memory, actor + ACTOR_SLOT)? & 7);
    let pc = u16_at(memory, actor + ACTOR_SLOTS + slot * 8)?;
    u8_at(memory, ram(u32_at(memory, EVENT_BYTECODE)?)? + u32::from(pc))
}

/// The field's dialogue in `mode` (`Services::mode`) as `memory` holds it.
pub fn prompt(memory: &dyn GameMemory, mode: Option<u32>) -> Prompt {
    read_prompt(memory, mode).unwrap_or(Prompt::None)
}

fn read_prompt(memory: &dyn GameMemory, mode: Option<u32>) -> Option<Prompt> {
    if mode != Some(1) || u8_at(memory, FIELD_MENU_REQUEST)? != 0xFF || u16_at(memory, FIELD_OPEN_WINDOWS)? == 0 {
        return Some(Prompt::None);
    }
    let mut prompt = Prompt::None;
    for i in 0..4 {
        let window = WINDOWS + i * WINDOW_SIZE;
        let takes_press = i16_at(memory, window + BUSY)? == 0 && i16_at(memory, window + TIMER)? == 0
            && (i16_at(memory, window + SELECTED)? != 0 || u16_at(memory, window + AGE)? == 0);
        if !takes_press {
            continue;
        }
        if i16_at(memory, window + CHOICE_STATUS)? == 0 {
            return Some(Prompt::Choice);
        }
        let wait = u8_at(memory, window + TEXT_WAIT)?;
        if u16_at(memory, window + TEXT_FLAGS)? & WAITING == 0 || !(1..=3).contains(&wait) {
            continue;
        }
        if wait == 1 && next_instruction(memory, i16_at(memory, window + OWNER)?) == Some(START_CHOICE) {
            return Some(Prompt::Choice);
        }
        prompt = Prompt::Waiting { wait };
    }
    Some(prompt)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory::OutOfBounds;

    /// PS1 RAM at its KSEG0 addresses.
    struct Ram(Vec<u8>);

    impl GameMemory for Ram {
        fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
            let at = address.wrapping_sub(0x8000_0000) as usize;
            out.copy_from_slice(self.0.get(at..at + out.len()).ok_or(OutOfBounds { address, len: out.len() })?);
            Ok(())
        }
        fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
            let at = address.wrapping_sub(0x8000_0000) as usize;
            self.0.get_mut(at..at + data.len()).ok_or(OutOfBounds { address, len: data.len() })?.copy_from_slice(data);
            Ok(())
        }
    }

    /// A field with one window that takes presses, whose owner (descriptor
    /// 2) rests on `next` (invented memory, no original content).
    fn field(flags: u16, wait: u8, choice: i16, next: u8) -> Ram {
        let mut memory = Ram(vec![0; 0x20_0000]);
        let mut put = |address: u32, bytes: &[u8]| memory.write(address, bytes).unwrap();
        put(FIELD_MENU_REQUEST, &[0xFF]);
        put(FIELD_OPEN_WINDOWS, &1u16.to_le_bytes());
        for i in 0..4 {
            let window = WINDOWS + i * WINDOW_SIZE;
            put(window + BUSY, &(if i == 1 { 0i16 } else { -1 }).to_le_bytes());
            put(window + AGE, &(if i == 1 { 0u16 } else { 0xFFFF }).to_le_bytes());
            put(window + CHOICE_STATUS, &(-1i16).to_le_bytes());
        }
        let window = WINDOWS + WINDOW_SIZE;
        put(window + TEXT_FLAGS, &flags.to_le_bytes());
        put(window + TEXT_WAIT, &[wait]);
        put(window + CHOICE_STATUS, &choice.to_le_bytes());
        put(window + OWNER, &2i16.to_le_bytes());
        let (descriptors, actor, bytecode) = (0x8010_0000u32, 0x8011_0000u32, 0x8012_0000u32);
        put(COMPONENTS, &3u32.to_le_bytes());
        put(COMPONENTS + 4, &descriptors.to_le_bytes());
        put(descriptors + 2 * DESCRIPTOR_SIZE + DESCRIPTOR_ACTOR, &actor.to_le_bytes());
        put(actor + ACTOR_SLOT, &[3]);
        put(actor + ACTOR_SLOTS + 3 * 8, &0x40u16.to_le_bytes());
        put(EVENT_BYTECODE, &bytecode.to_le_bytes());
        put(bytecode + 0x40, &[next]);
        memory
    }

    #[test]
    fn a_window_waiting_for_its_page_or_end_takes_a_press() {
        assert_eq!(prompt(&field(8, 2, -1, START_CHOICE), Some(1)), Prompt::Waiting { wait: 2 });
        assert_eq!(prompt(&field(8, 1, -1, 0x9C), Some(1)), Prompt::Waiting { wait: 1 });
        assert_eq!(prompt(&field(0x48, 3, -1, 0x9C), Some(1)), Prompt::Waiting { wait: 3 });
    }

    #[test]
    fn a_shown_choice_or_one_about_to_open_is_never_a_wait() {
        assert_eq!(prompt(&field(8, 1, -1, START_CHOICE), Some(1)), Prompt::Choice, "the owner opens a choice at the end");
        assert_eq!(prompt(&field(0, 0, 0, 0x9C), Some(1)), Prompt::Choice, "a choice shown");
        assert_eq!(prompt(&field(8, 2, 0, 0x9C), Some(1)), Prompt::Choice, "a choice shown on a waiting window");
    }

    #[test]
    fn revealing_text_a_menu_or_another_mode_is_no_wait() {
        assert_eq!(prompt(&field(0, 1, -1, 0x9C), Some(1)), Prompt::None, "still revealing");
        assert_eq!(prompt(&field(8, 0, -1, 0x9C), Some(1)), Prompt::None);
        assert_eq!(prompt(&field(8, 1, -1, 0x9C), Some(2)), Prompt::None, "a battle");
        let mut menu = field(8, 1, -1, 0x9C);
        menu.write(FIELD_MENU_REQUEST, &[0]).unwrap();
        assert_eq!(prompt(&menu, Some(1)), Prompt::None, "a menu runs");
        let mut opening = field(8, 1, -1, 0x9C);
        opening.write(WINDOWS + WINDOW_SIZE + TIMER, &3i16.to_le_bytes()).unwrap();
        assert_eq!(prompt(&opening, Some(1)), Prompt::None, "the window still opens");
    }
}
