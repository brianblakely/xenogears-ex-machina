//! The movie the game plays, as agents read it (`status`, docs/runtime.md,
//! Movies can be skipped): the field's (`field_movie_play`, 800A7C58,
//! requested and not stopped; a window movie is field movie mode 2, `fe
//! 67`) or the movie mode's (6), the frame it shows, and whether a press
//! ends it now by the original's own early exit. With the debug monitor
//! absent, as on the retail disc, a field movie ends on Confirm when its
//! request has fade bit 0x80 (`field_movie_fade_bits`); with the monitor
//! present every field movie ends on a press (Triangle a window movie's,
//! Confirm the others'). The movie mode's ends on Cross or Start unless its
//! request kept it (`movie_mode_unskippable`, read by
//! `movie_mode_read_playback_input`, 800769A4). Under the movies option
//! (src/port/movie_skip.c) a field movie gains the bit as its frames run and
//! the movie mode's keep flag clears as it reads the pad. Read from the
//! game's memory between steps; nothing is written.
use crate::memory::GameMemory;
use serde::Serialize;

/// The field's movie: stopped (-1 once it ends), requested, its mode (2 a
/// window movie), its request's fade bits, the frame it shows, and whether
/// the debug monitor is absent (`field_monitor_absent`).
const FIELD_MOVIE_STOPPED: u32 = 0x800A_DB6C;
const FIELD_MOVIE_REQUESTED: u32 = 0x800A_DB70;
const FIELD_MOVIE_MODE: u32 = 0x800A_DB74;
const FIELD_MOVIE_FADE_BITS: u32 = 0x800A_DB80;
const FIELD_MOVIE_FRAME: u32 = 0x800B_06A0;
const FIELD_MONITOR_ABSENT: u32 = 0x800C_268C;
const EARLY_EXIT: i32 = 0x80;
/// The movie mode's keep flag and the last frame its player loaded
/// (`movie_shown_frame`, the MDEC library's).
const MOVIE_MODE_UNSKIPPABLE: u32 = 0x8007_7028;
const MOVIE_SHOWN_FRAME: u32 = 0x801E_8988;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Kind {
    /// A field movie over the whole screen (the story's).
    Field,
    /// A field movie in a window, which the field's scripts end.
    Window,
    /// The movie mode's (the title's, the battle events').
    Mode,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
pub struct Movie {
    pub kind: Kind,
    pub frame: i32,
    /// A press would end it now: Confirm a field movie's, Cross or Start
    /// the movie mode's.
    pub ends_on_press: bool,
}

fn i32_at(memory: &dyn GameMemory, address: u32) -> Option<i32> {
    let mut bytes = [0; 4];
    memory.read(address, &mut bytes).ok().map(|()| i32::from_le_bytes(bytes))
}

/// The movie playing in `mode` (`Services::mode`) as `memory` holds it.
pub fn movie(memory: &dyn GameMemory, mode: Option<u32>) -> Option<Movie> {
    let at = |address| i32_at(memory, address);
    match mode {
        Some(1) => {
            if at(FIELD_MOVIE_REQUESTED)? == 0 || at(FIELD_MOVIE_STOPPED)? != 0 {
                return None;
            }
            let kind = if at(FIELD_MOVIE_MODE)? == 2 { Kind::Window } else { Kind::Field };
            let ends_on_press = at(FIELD_MONITOR_ABSENT)? == 0 || at(FIELD_MOVIE_FADE_BITS)? & EARLY_EXIT != 0;
            Some(Movie { kind, frame: at(FIELD_MOVIE_FRAME)?, ends_on_press })
        }
        Some(6) => Some(Movie { kind: Kind::Mode, frame: at(MOVIE_SHOWN_FRAME)?, ends_on_press: at(MOVIE_MODE_UNSKIPPABLE)? == 0 }),
        _ => None,
    }
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

    /// A field playing movie frame 21 in `mode` with `fade` bits, the
    /// monitor absent (invented memory, no original content).
    fn field(mode: i32, fade: i32) -> Ram {
        let mut memory = Ram(vec![0; 0x20_0000]);
        for (address, value) in [(FIELD_MOVIE_REQUESTED, 1), (FIELD_MOVIE_STOPPED, 0), (FIELD_MOVIE_MODE, mode),
            (FIELD_MOVIE_FADE_BITS, fade), (FIELD_MOVIE_FRAME, 21), (FIELD_MONITOR_ABSENT, 1)] {
            memory.write(address, &value.to_le_bytes()).unwrap();
        }
        memory
    }

    #[test]
    fn a_field_movie_ends_on_a_press_only_when_its_request_has_the_early_exit() {
        assert_eq!(movie(&field(0, 0x40), Some(1)), Some(Movie { kind: Kind::Field, frame: 21, ends_on_press: false }));
        assert_eq!(movie(&field(1, 0xC0), Some(1)), Some(Movie { kind: Kind::Field, frame: 21, ends_on_press: true }));
        assert_eq!(movie(&field(2, 0), Some(1)), Some(Movie { kind: Kind::Window, frame: 21, ends_on_press: false }));
        let mut monitor = field(2, 0);
        monitor.write(FIELD_MONITOR_ABSENT, &0i32.to_le_bytes()).unwrap();
        assert_eq!(movie(&monitor, Some(1)).map(|m| m.ends_on_press), Some(true), "the debug monitor lets a press end any");
    }

    #[test]
    fn no_movie_before_its_request_after_its_end_or_in_another_mode() {
        let mut ended = field(0, 0x80);
        ended.write(FIELD_MOVIE_STOPPED, &(-1i32).to_le_bytes()).unwrap();
        assert_eq!(movie(&ended, Some(1)), None, "stopped");
        let mut none = field(0, 0x80);
        none.write(FIELD_MOVIE_REQUESTED, &0i32.to_le_bytes()).unwrap();
        assert_eq!(movie(&none, Some(1)), None, "not requested");
        assert_eq!(movie(&field(0, 0x80), Some(2)), None, "a battle");
    }

    #[test]
    fn the_movie_modes_movie_ends_on_a_press_unless_its_request_kept_it() {
        let mut memory = Ram(vec![0; 0x20_0000]);
        memory.write(MOVIE_SHOWN_FRAME, &300i32.to_le_bytes()).unwrap();
        assert_eq!(movie(&memory, Some(6)), Some(Movie { kind: Kind::Mode, frame: 300, ends_on_press: true }));
        memory.write(MOVIE_MODE_UNSKIPPABLE, &1i32.to_le_bytes()).unwrap();
        assert_eq!(movie(&memory, Some(6)).map(|m| m.ends_on_press), Some(false));
    }
}
