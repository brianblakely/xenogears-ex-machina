//! The movie the game plays, as agents read it (`status`, docs/runtime.md,
//! Movies can be skipped): the movie whose playback loop runs, the frame it
//! shows, and whether a press ends it now by the original's own early exit.
//!
//! A field movie (`field_movie_play`, 800A7C58; a window movie is field
//! movie mode 2, `fe 67`) shows from its first picture until its loop
//! closes the movie library's player (`movie_close`): before the picture
//! the player waits for its first frame in a loop that reads no press, and
//! after the loop the field reloads what the movie displaced. Its loop
//! reads the press from the game's pad queue (`field_pad_drain_queue`,
//! through the scripts' input mask): with the debug monitor absent, as on
//! the retail disc, Confirm when the request has fade bit 0x80
//! (`field_movie_fade_bits`); with the monitor present any movie
//! (Triangle a window movie, Confirm the others). The queue keeps 16
//! vertical blanks' pads and drops the rest, so a press counts only while
//! it has room: not on the first picture after a wait that filled it, until
//! the loop's first read. The loop reads none while the staff roll draws
//! over the movie, and once it has taken a press the movie fades for five
//! vertical blanks and closes.
//!
//! The movie mode's (6) shows from its player's start until it stops, its
//! loop reading the pad from the start (`movie_mode_read_playback_input`,
//! 800769A4): Cross or Start ends it unless its request kept it
//! (`movie_mode_unskippable`) or its end already counts down
//! (`movie_mode_stop_timer`).
//!
//! Whose movie the player plays is its frame callback: the movie library,
//! loaded with each movie, holds its player's state and callback, zeroed by
//! the load and set as the movie starts; before the load that memory holds
//! whatever was there. Under the movies option (src/port/movie_skip.c) a
//! field movie gains the bit as its frames run and the movie mode's keep
//! flag clears as it reads the pad. Read from the game's memory between
//! steps; nothing is written.
use crate::memory::GameMemory;
use serde::Serialize;

/// The field's movie: stopped (-1 once it ends), requested, its mode (2 a
/// window movie), its request's fade bits, the frame it shows, whether it
/// still waits for its first frame, whether the staff roll draws over it,
/// and whether the debug monitor is absent (`field_monitor_absent`).
const FIELD_MOVIE_STOPPED: u32 = 0x800A_DB6C;
const FIELD_MOVIE_REQUESTED: u32 = 0x800A_DB70;
const FIELD_MOVIE_MODE: u32 = 0x800A_DB74;
const FIELD_MOVIE_FADE_BITS: u32 = 0x800A_DB80;
const FIELD_MOVIE_FRAME: u32 = 0x800B_06A0;
const FIELD_MOVIE_AWAITING_FRAME: u32 = 0x800B_00E4;
const FIELD_STAFF_ROLL_DRAWING: u32 = 0x800A_FE74;
const FIELD_MONITOR_ABSENT: u32 = 0x800C_268C;
/// The buttons the field's pad read last took as pressed
/// (`field_pad_port0_repeated`) and the scripts' input mask it applies
/// (`field_work.input_mask`), both halfwords.
const FIELD_PAD_REPEATED: u32 = 0x800C_3900;
const FIELD_INPUT_MASK: u32 = 0x800B_217A;
/// The pads the game's queue holds (`pad_queue_count`, at most 16).
const PAD_QUEUE_COUNT: u32 = 0x8005_937C;
const PAD_QUEUE_LENGTH: i32 = 16;
const EARLY_EXIT: i32 = 0x80;
const CONFIRM: u16 = 0x20;
const TRIANGLE: u16 = 0x80;
/// The movie library's player (801D3000): its state (1 or 2 while it
/// streams, -1 once stopped; a byte), the last frame it loaded and the
/// callback its movie started it with: the field's
/// (`field_movie_frame_callback`) or the movie mode's
/// (`movie_mode_on_frame_loaded`).
const PLAYER_STATE: u32 = 0x801E_8964;
const PLAYER_SHOWN_FRAME: u32 = 0x801E_8988;
const PLAYER_CALLBACK: u32 = 0x801E_8990;
const FIELD_CALLBACK: u32 = 0x800A_7120;
const MOVIE_MODE_CALLBACK: u32 = 0x8007_68D8;
/// The movie mode's end countdown (0 while it plays on) and keep flag.
const MOVIE_MODE_STOP_TIMER: u32 = 0x8007_7014;
const MOVIE_MODE_UNSKIPPABLE: u32 = 0x8007_7028;

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
    /// The frame it shows (the movie mode's -1 until its first picture).
    pub frame: i32,
    /// A press now would end it: Confirm a field movie's, Cross or Start
    /// the movie mode's.
    pub ends_on_press: bool,
}

fn bytes<const N: usize>(memory: &dyn GameMemory, address: u32) -> Option<[u8; N]> {
    let mut bytes = [0; N];
    memory.read(address, &mut bytes).ok().map(|()| bytes)
}

/// The movie whose playback loop runs in `mode` (`Services::mode`) as
/// `memory` holds it.
pub fn movie(memory: &dyn GameMemory, mode: Option<u32>) -> Option<Movie> {
    let at = |address| bytes(memory, address).map(i32::from_le_bytes);
    let half = |address| bytes(memory, address).map(u16::from_le_bytes);
    // The player streams (state 1, or 2 when it loops) the movie started
    // with `callback`.
    let streams = |callback: u32| -> Option<bool> {
        let [state] = bytes(memory, PLAYER_STATE)?;
        Some(matches!(state, 1 | 2) && at(PLAYER_CALLBACK)? as u32 == callback)
    };
    match mode {
        Some(1) => {
            let playing = at(FIELD_MOVIE_REQUESTED)? != 0 && at(FIELD_MOVIE_STOPPED)? == 0 && at(FIELD_MOVIE_AWAITING_FRAME)? == 0;
            if !playing || !streams(FIELD_CALLBACK)? {
                return None;
            }
            let window = at(FIELD_MOVIE_MODE)? == 2;
            let monitor = at(FIELD_MONITOR_ABSENT)? == 0;
            let press = if window && monitor { TRIANGLE } else { CONFIRM };
            // A press the queue keeps and the mask lets through, none once
            // the loop has taken one (for its first pass, the field's last
            // read before the movie).
            let reads = (monitor || at(FIELD_MOVIE_FADE_BITS)? & EARLY_EXIT != 0) && at(FIELD_STAFF_ROLL_DRAWING)? == 0;
            let kept = at(PAD_QUEUE_COUNT)? < PAD_QUEUE_LENGTH && half(FIELD_INPUT_MASK)? & press != 0;
            let ends_on_press = reads && kept && half(FIELD_PAD_REPEATED)? & press == 0;
            Some(Movie { kind: if window { Kind::Window } else { Kind::Field }, frame: at(FIELD_MOVIE_FRAME)?, ends_on_press })
        }
        Some(6) => {
            if !streams(MOVIE_MODE_CALLBACK)? {
                return None;
            }
            let ends_on_press = at(MOVIE_MODE_UNSKIPPABLE)? == 0 && at(MOVIE_MODE_STOP_TIMER)? == 0;
            Some(Movie { kind: Kind::Mode, frame: at(PLAYER_SHOWN_FRAME)?, ends_on_press })
        }
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

    impl Ram {
        fn set(&mut self, address: u32, value: i32) -> &mut Self {
            self.write(address, &value.to_le_bytes()).unwrap();
            self
        }
        fn set_half(&mut self, address: u32, value: u16) -> &mut Self {
            self.write(address, &value.to_le_bytes()).unwrap();
            self
        }
        /// The player streaming the movie `callback` started (a byte state).
        fn player(&mut self, state: i8, callback: u32) -> &mut Self {
            self.write(PLAYER_STATE, &state.to_le_bytes()).unwrap();
            self.set(PLAYER_CALLBACK, callback as i32)
        }
    }

    /// A field showing movie frame 21 in `mode` with `fade` bits, past its
    /// first picture, the monitor absent, every button let through and none
    /// taken (invented memory, no original content).
    fn field(mode: i32, fade: i32) -> Ram {
        let mut memory = Ram(vec![0; 0x20_0000]);
        memory.set(FIELD_MOVIE_REQUESTED, 1).set(FIELD_MOVIE_STOPPED, 0).set(FIELD_MOVIE_MODE, mode).set(FIELD_MOVIE_FADE_BITS, fade);
        memory.set(FIELD_MOVIE_FRAME, 21).set(FIELD_MONITOR_ABSENT, 1).set_half(FIELD_INPUT_MASK, 0xFFFF);
        memory.player(1, FIELD_CALLBACK);
        memory
    }

    #[test]
    fn a_field_movie_ends_on_a_press_only_when_its_request_has_the_early_exit() {
        assert_eq!(movie(&field(0, 0x40), Some(1)), Some(Movie { kind: Kind::Field, frame: 21, ends_on_press: false }));
        assert_eq!(movie(&field(1, 0xC0), Some(1)), Some(Movie { kind: Kind::Field, frame: 21, ends_on_press: true }));
        assert_eq!(movie(&field(2, 0), Some(1)), Some(Movie { kind: Kind::Window, frame: 21, ends_on_press: false }));
        let mut monitor = field(2, 0);
        monitor.set(FIELD_MONITOR_ABSENT, 0);
        assert_eq!(movie(&monitor, Some(1)).map(|m| m.ends_on_press), Some(true), "the debug monitor lets a press end any");
        monitor.set_half(FIELD_INPUT_MASK, !TRIANGLE);
        assert_eq!(movie(&monitor, Some(1)).map(|m| m.ends_on_press), Some(false), "a window movie's press is Triangle");
    }

    #[test]
    fn a_field_movie_shows_from_its_first_picture_until_its_player_closes() {
        let mut waiting = field(0, 0x80);
        waiting.set(FIELD_MOVIE_AWAITING_FRAME, 1);
        assert_eq!(movie(&waiting, Some(1)), None, "before its first picture the loop reads no press");
        let mut closed = field(0, 0x80);
        closed.player(-1, FIELD_CALLBACK);
        assert_eq!(movie(&closed, Some(1)), None, "the loop closed the player; the field reloads");
        // Before the movie library is loaded its memory holds anything.
        let mut stale = field(0, 0x80);
        stale.player(1, 0xD240_CE32);
        assert_eq!(movie(&stale, Some(1)), None, "another movie's or no player");
        let mut ended = field(0, 0x80);
        ended.set(FIELD_MOVIE_STOPPED, -1);
        assert_eq!(movie(&ended, Some(1)), None, "stopped");
        let mut none = field(0, 0x80);
        none.set(FIELD_MOVIE_REQUESTED, 0);
        assert_eq!(movie(&none, Some(1)), None, "not requested");
        assert_eq!(movie(&field(0, 0x80), Some(2)), None, "a battle");
        let mut looping = field(2, 0);
        looping.player(2, FIELD_CALLBACK);
        assert_eq!(movie(&looping, Some(1)).map(|m| m.kind), Some(Kind::Window), "a window movie the player loops");
    }

    #[test]
    fn a_field_movies_loop_takes_no_press_under_the_staff_roll_once_it_took_one_through_the_mask_or_a_full_queue() {
        let mut staff = field(0, 0x80);
        staff.set(FIELD_STAFF_ROLL_DRAWING, 1);
        assert_eq!(movie(&staff, Some(1)).map(|m| m.ends_on_press), Some(false), "the staff roll's loop reads no press");
        let mut taken = field(0, 0x80);
        taken.set_half(FIELD_PAD_REPEATED, CONFIRM);
        assert_eq!(movie(&taken, Some(1)).map(|m| m.ends_on_press), Some(false), "it fades out after the press it took");
        let mut masked = field(0, 0x80);
        masked.set_half(FIELD_INPUT_MASK, !CONFIRM);
        assert_eq!(movie(&masked, Some(1)).map(|m| m.ends_on_press), Some(false), "the scripts mask Confirm");
        let mut full = field(0, 0x80);
        full.set(PAD_QUEUE_COUNT, 15);
        assert_eq!(movie(&full, Some(1)).map(|m| m.ends_on_press), Some(true), "the queue keeps one more");
        full.set(PAD_QUEUE_COUNT, 16);
        assert_eq!(movie(&full, Some(1)).map(|m| m.ends_on_press), Some(false), "the queue drops the press");
    }

    #[test]
    fn the_movie_modes_movie_shows_from_its_players_start_and_ends_on_a_press_unless_kept_or_ending() {
        let mut memory = Ram(vec![0; 0x20_0000]);
        assert_eq!(movie(&memory, Some(6)), None, "the movie library still loads");
        memory.player(1, 0);
        assert_eq!(movie(&memory, Some(6)), None, "opened, the movie not started");
        memory.player(1, MOVIE_MODE_CALLBACK).set(PLAYER_SHOWN_FRAME, -1);
        assert_eq!(movie(&memory, Some(6)), Some(Movie { kind: Kind::Mode, frame: -1, ends_on_press: true }), "started, no picture yet");
        memory.set(PLAYER_SHOWN_FRAME, 300);
        assert_eq!(movie(&memory, Some(6)), Some(Movie { kind: Kind::Mode, frame: 300, ends_on_press: true }));
        memory.set(MOVIE_MODE_STOP_TIMER, 4);
        assert_eq!(movie(&memory, Some(6)).map(|m| m.ends_on_press), Some(false), "its end counts down");
        memory.set(MOVIE_MODE_STOP_TIMER, 0).set(MOVIE_MODE_UNSKIPPABLE, 1);
        assert_eq!(movie(&memory, Some(6)).map(|m| m.ends_on_press), Some(false), "kept");
        memory.player(-1, MOVIE_MODE_CALLBACK);
        assert_eq!(movie(&memory, Some(6)), None, "stopped");
        memory.player(1, FIELD_CALLBACK);
        assert_eq!(movie(&memory, Some(6)), None, "a field movie's player, left in memory");
    }
}
