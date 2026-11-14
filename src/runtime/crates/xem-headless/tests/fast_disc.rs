//! Skip disc load times on the game (docs/runtime.md, Disc load times): the
//! `fast_disc` control makes the drive fast (`xem_core::devices::cd`, The
//! fast drive). Each test plays the same thing with the drive at its own
//! pace and fast, by what the game shows and never by frame numbers, which
//! the option changes, and compares where the two arrive: a field's
//! pictures in VRAM and the music's samples in the sound chip are the same
//! bytes, loaded in a fraction of the frames, the music's through a ring the
//! fast drive keeps full; the opening chapter from boot (the title's New
//! Game, its movie, the narration, Lahan and its battle) reaches Fei's room
//! through the same maps and battle sooner, its movie at the disc's pace;
//! timed scenes show the same messages and end where they ended; a movie
//! with its XA sound shows every frame as many vertical blanks apart and
//! plays as many sound sectors, only starting sooner; battles from the
//! kernel menu's test are fought to the same end with the same reads asked
//! of the disc; the world map streams its terrain while the party walks; a
//! recording made with the drive fast replays to its memory, and one made
//! at the disc's pace keeps that pace in a session whose drive was fast; a
//! state saved in the middle of a load goes on identically, and restores
//! under the other setting; a new game on disc 2 gets disc 1, and the
//! story's disc change still waits its five seconds. In every run the
//! game's own counts of disc errors stay at zero. Local only: it needs the
//! user's discs and a game module build.
//!
//!     XEM_DISC1=<disc 1> XEM_DISC2=<disc 2> cargo test --release -p xem-headless --test fast_disc

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::collections::BTreeSet;
use std::path::PathBuf;
use xem_core::action::GameAction;
use xem_core::context::Context;
use xem_core::control::{Session, button_bit};
use xem_core::debug::Request;
use xem_core::devices::cd::STORY_CHANGE_CYCLES;
use xem_core::dialogue::Prompt;
use xem_core::movie::{Kind, Movie};
use xem_game::{NativeModule, boot};

const MODE_FIELD_MAP_ID: u32 = 0x8004_F34C;
/// `field_event_variables[0]`, the story value, while the field runs.
const STORY: u32 = 0x800C_3A68;
/// `field_message_table` and the dialogue windows (text_and_movies.rs).
const MESSAGE_TABLE: u32 = 0x800A_DBF0;
const WINDOWS: u32 = 0x800C_2698;
const WINDOW_SIZE: u32 = 0x498;
/// `field_menu_request` (field_mode.h; not in the inspection schema).
const FIELD_MENU_REQUEST: u32 = 0x800A_DB64;
/// The title's map, the first map of a new game, and the title's New Game
/// among its commands.
const TITLE_MAP: u16 = 490;
const FIRST_MAP: u16 = 4;
const NEW_GAME: u8 = 2;
/// The scripts' Confirm (`31 0x0020`), as `Context::Event` reports the
/// buttons a map's scripts test.
const SCRIPT_CONFIRM: u16 = 0x20;

/// The game's own counts of what went wrong with the disc (resident cd.h):
/// a file's, a list's and a stream's sectors out of order, reads given up
/// after three errors, commands the drive refused and retries that failed.
const DISC_ERRORS: [&str; 6] = ["cd_file_out_of_order_count", "cd_list_out_of_order_count", "cd_stream_out_of_order_count",
    "cd_stat_error_limit_count", "cd_stat_command_fail_count", "cd_stat_retry_fail_count"];

fn disc(number: u8) -> Option<PathBuf> {
    let path = std::env::var_os(format!("XEM_DISC{number}")).map(PathBuf::from).unwrap_or_else(|| {
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join(format!("../../../../discs/Xenogears disc {number}.chd"))
    });
    path.exists().then_some(path)
}

fn run(session: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = session.execute(&command);
    assert!(reply.get("error").is_none(), "{command} -> {reply}");
    reply
}

fn word(session: &mut Session<NativeModule>, address: u32) -> u32 {
    let mut bytes = [0; 4];
    session.runtime.memory().read(address, &mut bytes).expect("game memory");
    u32::from_le_bytes(bytes)
}

fn half(session: &mut Session<NativeModule>, address: u32) -> u16 {
    word(session, address) as u16
}

/// A game global, through the inspection schema.
fn value(session: &mut Session<NativeModule>, path: &str) -> i64 {
    let reply = session.execute(&json!({ "cmd": "inspect", "path": path, "depth": 0 }));
    reply.as_i64().unwrap_or_else(|| panic!("{path}: {reply}"))
}

/// A session booted from `disc`, its drive fast or at the disc's pace.
fn session(disc: &PathBuf, fast: bool) -> Session<NativeModule> {
    let (mut session, _) = boot(disc, &Default::default()).expect("boot");
    session.set_fast_disc(fast);
    assert_eq!(run(&mut session, json!({"cmd": "status"}))["fast_disc"], json!(fast));
    session
}

fn step(session: &mut Session<NativeModule>) {
    assert!(session.frame().expect("the game runs"), "a native disc never blocks");
}

fn frames(session: &mut Session<NativeModule>, count: u64) {
    for _ in 0..count {
        step(session);
    }
}

fn hash(session: &mut Session<NativeModule>) -> String {
    format!("{:016x}", session.memory_hash().expect("game memory"))
}

/// The game counted no disc error.
fn no_disc_errors(session: &mut Session<NativeModule>, what: &str) {
    let counts: Vec<i64> = DISC_ERRORS.iter().map(|path| value(session, path)).collect();
    assert_eq!(counts, [0; 6], "{what}: the game's disc error counts {DISC_ERRORS:?}");
}

/// The two runs side by side, each in a thread of its own: at the disc's
/// pace, and with the drive fast.
fn both<T: Send>(play: impl Fn(bool) -> T + Sync) -> (T, T) {
    std::thread::scope(|scope| {
        let original = scope.spawn(|| play(false));
        let fast = scope.spawn(|| play(true));
        (original.join().expect("the run at the disc's pace"), fast.join().expect("the run with the drive fast"))
    })
}

/// Whether the field still loads: a music wave streams, a music load is
/// pending or the disc reads.
fn field_loading(session: &mut Session<NativeModule>) -> bool {
    value(session, "field_music_stream_running") != 0 || value(session, "mode_music_load_pending") != 0 || value(session, "cd_pending_read_count") != 0
}

/// A field entered as the kernel menu does: the frames until the player
/// walks (none within 600 on a map that opens with a scene), the frames
/// until its loads were over (nothing loading for the 120 frames after)
/// and, as those 120 frames end, its VRAM, the sound chip's memory, the
/// music's wave bank (-1: none) and the game's count of the seeks its reads
/// and retries made.
struct Entered {
    walk: Option<u64>,
    loads: u64,
    vram: Vec<u16>,
    sound: Vec<u8>,
    wave: i64,
    retries: i64,
}

fn enter(disc: &PathBuf, fast: bool, map: u16) -> Entered {
    let mut s = session(disc, fast);
    run(&mut s, json!({"cmd": "enter_field", "map": map}));
    let start = s.runtime.frames();
    let (mut walk, mut quiet, mut loaded) = (None, 0, None);
    while loaded.is_none() || s.runtime.frames() - start < 600 && walk.is_none() {
        assert!(s.runtime.frames() - start < 3000, "map {map}: the field's loads never end");
        step(&mut s);
        if walk.is_none() && s.runtime.context() == Context::Walk {
            walk = Some(s.runtime.frames() - start);
        }
        quiet = if field_loading(&mut s) { 0 } else { quiet + 1 };
        if quiet == 120 && loaded.is_none() {
            assert_eq!(half(&mut s, MODE_FIELD_MAP_ID) & 0xFFF, map, "the field shows the map");
            let (wave, retries) = (value(&mut s, "mode_music_loaded_wave"), value(&mut s, "cd_stat_retry_setloc_count"));
            loaded = Some((s.runtime.frames() - start - 120, s.runtime.services().gpu.vram().to_vec(), s.runtime.services().spu.chip().ram().to_vec(), wave, retries));
        }
    }
    no_disc_errors(&mut s, &format!("map {map}"));
    let (loads, vram, sound, wave, retries) = loaded.expect("loads over");
    Entered { walk, loads, vram, sound, wave, retries }
}

/// Lahan (map 1) and map 27: with the drive fast the player walks Lahan in
/// a fraction of the frames, and 120 frames after each run's loads are over
/// every pixel of VRAM (the map's textures, which the field streams into it
/// a strip a sector, the sprites, the display buffers) is the one the
/// disc's pace leaves at that point.
#[test]
fn a_field_loads_in_a_fraction_of_the_frames_to_the_same_pictures() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    for map in [1, 27] {
        let (original, fast) = both(|fast| enter(&disc, fast, map));
        eprintln!("map {map}: the player walks after {:?} frames at the disc's pace and {:?} with the drive fast; its loads are over after {} and {}",
            original.walk, fast.walk, original.loads, fast.loads);
        let differing = original.vram.iter().zip(&fast.vram).filter(|(a, b)| a != b).count();
        assert_eq!(differing, 0, "map {map}: VRAM pixels that differ");
        assert!(fast.loads * 3 <= original.loads, "map {map}: the loads are over in a third of the frames at most: {} against {}", fast.loads, original.loads);
        if map == 1 {
            let (original, fast) = (original.walk.expect("the player walks Lahan"), fast.walk.expect("the player walks Lahan"));
            assert!(original >= 100 && fast <= 10, "the player walks Lahan after {original} frames at the disc's pace and {fast} with the drive fast");
        }
    }
}

/// The music's wave bank streams through a ring of eight sectors that the
/// field empties five sectors a frame of its own: the fast drive fills it
/// at once, the reader's retry (status, pause, seek, read again) comes each
/// time it finds no free slot, and the samples reach the sound chip as they
/// are on the disc, where the disc's pace puts them.
#[test]
fn the_musics_samples_reach_the_sound_chip_whole_through_a_ring_the_fast_drive_keeps_full() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // Maps 14 (Lahan's music) and 131, each with a wave bank of its own.
    for map in [14, 131] {
        let (original, fast) = both(|fast| enter(&disc, fast, map));
        let wave = original.wave;
        assert!(wave >= 0 && fast.wave == wave, "map {map} plays music from wave bank {wave} ({} with the drive fast)", fast.wave);
        // The wave file (directory 0x1C, file 0x13 + 2 * wave), its samples
        // after the 0x2000 bytes the stream starts the bank with.
        let file = session(&disc, false).runtime.services().cd.read_file(0x1C, 0x13 + 2 * wave as u32).expect("the disc").expect("the wave file");
        let samples = &file[0x2000..];
        assert!(samples.len() >= 0x8000, "map {map}: {} bytes of samples", samples.len());
        let place = |sound: &[u8]| sound.windows(samples.len()).position(|window| window == samples);
        let (at, fast_at) = (place(&original.sound), place(&fast.sound));
        eprintln!("map {map}: wave {wave}, {} bytes of samples in the sound chip at {at:x?} and {fast_at:x?}; {} and {} seeks counted; loads over after {} and {} frames",
            samples.len(), original.retries, fast.retries, original.loads, fast.loads);
        assert!(at.is_some(), "map {map}: the samples are in the sound chip at the disc's pace");
        assert_eq!(fast_at, at, "map {map}: and with the drive fast, at the same place");
        assert!(fast.retries >= original.retries + 20, "map {map}: the fast drive finds the ring full again and again: {} seeks against {}", fast.retries, original.retries);
        assert!(fast.loads < original.loads, "map {map}: the loads are over sooner: {} against {}", fast.loads, original.loads);
    }
}

fn ram(pointer: u32) -> bool {
    (0x8000_0000..0x8020_0000).contains(&pointer)
}

/// Whether a field actor rests on a wait for Confirm outside a window (the
/// opening narration's; text_and_movies.rs).
fn confirm_loop(session: &mut Session<NativeModule>) -> bool {
    let (count, descriptors, bytecode) = (word(session, 0x800A_FB0C).min(256), word(session, 0x800A_FB10), word(session, 0x800A_DC00));
    if !ram(descriptors) || !ram(bytecode) {
        return false;
    }
    let looping = |session: &mut Session<NativeModule>, at: u32| {
        let branch = u32::from(half(session, bytecode + at + 3));
        word(session, bytecode + at) & 0xFF_FFFF == 0x00_2031 && word(session, bytecode + branch) & 0xFF_FFFF == 0x01 | at << 8
    };
    (0..count).any(|i| {
        let actor = word(session, descriptors + i * 0x5C + 0x4C);
        if !ram(actor) {
            return false;
        }
        let slot = word(session, actor + 0xCE) & 7;
        let pc = u32::from(half(session, actor + 0x8C + slot * 8));
        let jumped = word(session, bytecode + pc) & 0xFF == 0x01;
        let to = u32::from(half(session, bytecode + pc + 1));
        looping(session, pc) || (jumped && looping(session, to))
    })
}

fn movie(session: &mut Session<NativeModule>) -> Option<Movie> {
    let mode = session.runtime.services().mode;
    xem_core::movie::movie(session.runtime.memory(), mode)
}

/// How a run ended: on another map, with the player walking, or not within
/// its frames.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum End {
    Map(u16),
    Control,
    Unfinished,
}

/// The opening chapter as a player plays it from boot, by what the game
/// shows: the frames it took, the frame the title's commands took the pad
/// and the one the new game's first map showed, the vertical blanks between
/// the new game's movie showing its frames 20 and 100, the story value, the
/// maps in order and the battles fought.
#[derive(Debug)]
struct Opening {
    end: End,
    frames: u64,
    title: u64,
    new_game: u64,
    movie_pace: u64,
    story: u16,
    maps: Vec<u16>,
    battles: u32,
}

/// Start ends the boot's movie as soon as it reads the press; Confirm opens
/// the title's commands, Up reaches New Game and Confirm begins it; the new
/// game's movie plays to its frame 100, where Confirm ends it as the game
/// lets it; then Confirm for one vertical blank as each dialogue prompt
/// shows and at each wait for Confirm outside a window (the narration's),
/// and Cross 8 of every 64 frames in a battle, until the player has walked
/// 300 frames. With `toggled` the drive changes between fast and the
/// disc's pace every that many frames, whatever it is reading.
fn opening(disc: &PathBuf, fast: bool, toggled: Option<u64>) -> Opening {
    let mut session = session(disc, fast);
    let confirm = GameAction::Confirm.button();
    let (start, up, cross) = (button_bit("start").unwrap(), button_bit("up").unwrap(), button_bit("cross").unwrap());
    let (mut maps, mut battles, mut battling, mut walking) = (Vec::new(), 0, 0u64, 0);
    let (mut title, mut new_game) = (None, None);
    let (mut movie_20, mut movie_100) = (None, None);
    let mut pressed: Option<u64> = None;
    let mut end = End::Unfinished;
    while session.runtime.frames() < 60_000 {
        let now = session.runtime.frames();
        let mode = session.runtime.services().mode;
        let shown = movie(&mut session);
        let map = half(&mut session, MODE_FIELD_MAP_ID) & 0xFFF;
        let spaced = |gap: u64| pressed.is_none_or(|at| now - at >= gap);
        if let Some(every) = toggled {
            session.set_fast_disc((now / every) % 2 == 0);
        }
        let mut buttons = 0;
        match (mode, shown) {
            // The boot's movie, and whatever movie the title plays.
            (Some(6), Some(movie)) if movie.ends_on_press && spaced(30) => buttons = start,
            (Some(1), Some(movie)) if map == TITLE_MAP && movie.ends_on_press && spaced(30) => buttons = confirm,
            (Some(1), None) if map == TITLE_MAP => match session.runtime.context() {
                Context::Event { buttons: tested, .. } if tested & SCRIPT_CONFIRM != 0 && spaced(30) => buttons = confirm,
                Context::Menu { screen: 2, commands: true, cursor, .. } => {
                    title.get_or_insert(now);
                    if spaced(20) {
                        buttons = if cursor == NEW_GAME { confirm } else { up };
                    }
                }
                _ => {}
            },
            // The new game's movie: its pace, then the press it reads.
            (Some(1), Some(Movie { kind: Kind::Field, frame, ends_on_press })) => {
                if frame >= 20 {
                    movie_20.get_or_insert(now);
                }
                if frame >= 100 {
                    movie_100.get_or_insert(now);
                    if ends_on_press && spaced(30) {
                        buttons = confirm;
                    }
                }
            }
            (Some(2), _) => {
                battling += 1;
                if battling == 1 {
                    battles += 1;
                }
                if battling % 64 < 8 {
                    buttons = cross;
                }
            }
            (Some(1), None) => {
                battling = 0;
                match xem_core::dialogue::prompt(session.runtime.memory(), mode) {
                    Prompt::Choice => panic!("the opening chapter has no choice"),
                    Prompt::Waiting { .. } if spaced(16) => buttons = confirm,
                    Prompt::None if spaced(16) && confirm_loop(&mut session) => buttons = confirm,
                    _ => {}
                }
            }
            _ => {}
        }
        if buttons != 0 {
            pressed = Some(now);
        }
        session.set_pad(0, buttons);
        step(&mut session);
        if session.runtime.services().mode == Some(1) {
            let map = half(&mut session, MODE_FIELD_MAP_ID) & 0xFFF;
            if maps.last() != Some(&map) && map != 0xFFF {
                maps.push(map);
                if map == FIRST_MAP {
                    new_game.get_or_insert(session.runtime.frames());
                }
            }
        }
        walking = if session.runtime.context() == Context::Walk { walking + 1 } else { 0 };
        if walking >= 300 {
            end = End::Control;
            break;
        }
    }
    session.set_pad(0, 0);
    no_disc_errors(&mut session, "the opening chapter");
    Opening {
        end,
        frames: session.runtime.frames(),
        title: title.expect("the title's commands"),
        new_game: new_game.expect("the new game's first map"),
        movie_pace: movie_100.expect("the movie's frame 100") - movie_20.expect("the movie's frame 20"),
        story: half(&mut session, STORY),
        maps,
        battles,
    }
}

/// Ordinary progression: the opening chapter from boot reaches Fei's room,
/// story value 6 on map 14 with the player walking, through the same maps
/// and the same battle with the drive fast, sooner at every step, and its
/// movie shows its frames at the disc's pace.
#[test]
fn the_opening_chapter_reaches_feis_room_through_the_same_maps_and_battle_sooner() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (original, fast) = both(|fast| opening(&disc, fast, None));
    for (name, run) in [("at the disc's pace", &original), ("with the drive fast", &fast)] {
        eprintln!("the opening chapter {name}: {run:?}");
    }
    assert_eq!((original.end, original.story, original.maps.last(), original.battles), (End::Control, 6, Some(&14), 1), "the disc's pace reaches Fei's room past map 14's battle");
    assert_eq!((fast.end, fast.story, &fast.maps, fast.battles), (original.end, original.story, &original.maps, original.battles),
        "the fast drive reaches the same story value through the same maps and battle");
    assert!(fast.title < original.title && fast.new_game - fast.title < original.new_game - original.title && fast.frames < original.frames,
        "sooner at the title, from it to the new game's first map and at the end");
    assert!(fast.movie_pace.abs_diff(original.movie_pace) <= 2, "the movie's frames 20 to 100 take as long: {} and {} vertical blanks", fast.movie_pace, original.movie_pace);
}

/// The setting changes at any time: with the drive turned fast and back
/// every 7 frames and every 61, in the middle of whatever it reads (files,
/// lists, the rings' retries, the movie), the opening chapter reaches Fei's
/// room through the same maps and battle, the movie at the disc's pace, in
/// frames between the two settings'.
#[test]
fn the_setting_changed_every_few_frames_plays_the_opening_chapter_to_the_same_place() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (often, seldom) = std::thread::scope(|scope| {
        let often = scope.spawn(|| opening(&disc, true, Some(7)));
        let seldom = scope.spawn(|| opening(&disc, false, Some(61)));
        (often.join().expect("a run"), seldom.join().expect("a run"))
    });
    for (name, run) in [("every 7 frames", &often), ("every 61 frames", &seldom)] {
        eprintln!("the opening chapter with the drive changed {name}: {run:?}");
        assert_eq!((run.end, run.story, &run.maps[..], run.battles), (End::Control, 6, &[TITLE_MAP, FIRST_MAP, 2, 14][..], 1), "changed {name}: Fei's room through the same maps and battle");
        assert!((319..=323).contains(&run.movie_pace), "changed {name}: the movie's frames 20 to 100 take {} vertical blanks", run.movie_pace);
        assert!((13_000..17_000).contains(&run.frames), "changed {name}: {} frames", run.frames);
    }
}

/// The message dialogue window `i` shows, by its number in the map's table.
fn message(session: &mut Session<NativeModule>, i: u32) -> Option<u16> {
    let at = word(session, WINDOWS + i * WINDOW_SIZE + 0xA8);
    let table = word(session, MESSAGE_TABLE);
    if !ram(table) {
        return None;
    }
    let count = half(session, table).min(1024);
    (0..count).find(|n| table + u32::from(half(session, table + 4 + 2 * u32::from(*n))) == at)
}

#[derive(Debug)]
struct Played {
    end: End,
    frames: u64,
    story: u16,
    /// The messages the scene's windows showed, by number.
    shown: BTreeSet<u16>,
}

/// A map's opening scene played as a player who reads at once: Confirm for
/// one vertical blank as each prompt shows (again 16 frames on if it still
/// shows), until the map changes or the player has walked 300 frames.
fn scene(disc: &PathBuf, fast: bool, map: u16, budget: u64) -> Played {
    let mut session = session(disc, fast);
    run(&mut session, json!({"cmd": "enter_field", "map": map}));
    let start = session.runtime.frames();
    let confirm = GameAction::Confirm.button();
    let (mut shown, mut pressed, mut walking) = (BTreeSet::new(), None::<u64>, 0);
    let mut end = End::Unfinished;
    while session.runtime.frames() - start < budget {
        let now = session.runtime.frames();
        let mode = session.runtime.services().mode;
        if mode == Some(1) && half(&mut session, 0x800B_2174) != 0 {
            for i in 0..4 {
                if let Some(n) = message(&mut session, i).filter(|_| half(&mut session, WINDOWS + i * WINDOW_SIZE + 0x40E) == 0) {
                    shown.insert(n);
                }
            }
        }
        let mut buttons = 0;
        match xem_core::dialogue::prompt(session.runtime.memory(), mode) {
            Prompt::Choice => panic!("map {map}: a choice the scene was not to show"),
            Prompt::Waiting { .. } if pressed.is_none_or(|at| now - at >= 16) => {
                buttons = confirm;
                pressed = Some(now);
            }
            _ => {}
        }
        session.set_pad(0, buttons);
        step(&mut session);
        let at = half(&mut session, MODE_FIELD_MAP_ID) & 0xFFF;
        if at != map && session.runtime.services().mode == Some(1) {
            end = End::Map(at);
            break;
        }
        walking = if session.runtime.context() == Context::Walk { walking + 1 } else { 0 };
        if walking >= 300 {
            end = End::Control;
            break;
        }
    }
    no_disc_errors(&mut session, &format!("map {map}'s scene"));
    Played { end, frames: session.runtime.frames() - start, story: half(&mut session, STORY), shown }
}

/// Scenes that time their windows against other scripts (text_and_movies.rs
/// plays them for the text speed): map 27's, which goes on to map 28 with
/// no press, map 131's and map 482's, which end with the player walking.
/// Each shows the same messages and ends where it ended, sooner.
#[test]
fn timed_scenes_show_the_same_messages_and_end_where_they_ended() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    for (map, budget) in [(27, 3000), (131, 6000), (482, 4000)] {
        let (original, fast) = both(|fast| scene(&disc, fast, map, budget));
        eprintln!("map {map}: {:?} after {} frames at the disc's pace and {} with the drive fast, story {}, messages {:?}",
            original.end, original.frames, fast.frames, original.story, original.shown);
        assert_ne!(original.end, End::Unfinished, "map {map}: the scene ends within {budget} frames");
        assert_eq!((fast.end, fast.story, &fast.shown), (original.end, original.story, &original.shown), "map {map}: the same end, story value and messages");
        assert!(fast.frames < original.frames, "map {map}: sooner");
    }
}

/// A kernel menu test launched as Settings' Debug section launches it; the
/// frames it took.
fn launch(session: &mut Session<NativeModule>, request: Request) -> u64 {
    let start = session.runtime.frames();
    session.request_debug(request).expect("a debug test");
    for _ in 0..3000 {
        if !session.debug.busy() {
            return session.runtime.frames() - start;
        }
        session.tick_debug().expect("the debug test goes on");
    }
    panic!("the debug test did not start: {}", session.debug_status());
}

/// A movie with XA sound (the kernel menu's movie test, movie 1 with
/// picture and audio) to its end: the frame its first picture shows at, the
/// frame each tenth movie frame showed at, the XA sectors the drive played
/// and the frames the player counted as skipped.
fn movie_played(disc: &PathBuf, fast: bool) -> (u64, Vec<(i64, u64)>, u64, i64) {
    let mut session = session(disc, fast);
    launch(&mut session, Request::Movie { movie: 1, kind: 1 });
    let start = session.runtime.frames();
    let (mut first, mut shown, mut last) = (None, Vec::new(), 0);
    while session.runtime.services().mode == Some(6) {
        assert!(session.runtime.frames() - start < 6000, "the movie ends");
        step(&mut session);
        if session.runtime.services().mode != Some(6) {
            break;
        }
        let frame = value(&mut session, "movie_shown_frame");
        if frame > last {
            last = frame;
            first.get_or_insert(session.runtime.frames() - start);
            if frame % 10 == 0 {
                shown.push((frame, session.runtime.frames() - start));
            }
        }
    }
    no_disc_errors(&mut session, "the movie");
    let xa = session.runtime.services().cd.xa_sectors;
    (first.expect("a picture"), shown, xa, value(&mut session, "movie_skipped_frames"))
}

/// A movie is played, not loaded (the stream library's ReadS in Setmode
/// 0xC8: data sectors without their addresses, XA-ADPCM for the sound):
/// with the drive fast its first picture shows sooner (the seek to it is
/// the drive's), and from there every frame shows as many vertical blanks
/// after it as at the disc's pace, with as many XA sound sectors played and
/// as many frames skipped.
#[test]
fn a_movie_plays_at_the_discs_pace_and_starts_sooner() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let ((first, shown, xa, skipped), (fast_first, fast_shown, fast_xa, fast_skipped)) = both(|fast| movie_played(&disc, fast));
    eprintln!("movie 1: its first picture {first} frames after the launch at the disc's pace and {fast_first} with the drive fast; {} tenth frames to {:?}; \
        {xa} XA sectors, {skipped} frames skipped", shown.len(), shown.last());
    assert!(fast_first < first, "the first picture shows sooner");
    assert!(shown.len() >= 30, "the movie plays its hundreds of frames: {shown:?}");
    let pace = |shown: &[(i64, u64)], first: u64| -> Vec<(i64, u64)> { shown.iter().map(|(frame, at)| (*frame, at - first)).collect() };
    assert_eq!(pace(&fast_shown, fast_first), pace(&shown, first), "every tenth frame as long after the first picture");
    assert_eq!((fast_xa, fast_skipped), (xa, skipped), "as many XA sectors played and frames skipped");
    assert!(xa > 400, "the movie's sound played: {xa} XA sectors");
}

/// The kernel menu's other tests start alike: a Gear battle, Battling (the
/// arena) and the menu's screens (the field menu, member change, load, name
/// entry, the shops) reach the same mode and kind of context 300 frames
/// after their launch, sooner, with no disc error.
#[test]
fn the_kernel_menus_tests_start_alike_sooner() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut requests = vec![Request::Battle { formation: 15, file: 253, party: [0, 2, 1], gears: [1; 3] }, Request::Battling];
    requests.extend((0..=5).map(|screen| Request::Menu { screen, parameter: 0 }));
    for request in requests {
        let (original, fast) = both(|fast| {
            let mut session = session(&disc, fast);
            let launched = launch(&mut session, request);
            frames(&mut session, 300);
            no_disc_errors(&mut session, &format!("{request:?}"));
            let context = format!("{:?}", session.runtime.context());
            (launched, session.runtime.services().mode, context.split([' ', '{', '(']).next().unwrap_or_default().to_string())
        });
        eprintln!("{request:?}: launched in {} frames at the disc's pace and {} with the drive fast, then mode {:?} {}", original.0, fast.0, original.1, original.2);
        assert_eq!((fast.1, &fast.2), (original.1, &original.2), "{request:?}: the same mode and kind of context");
        assert!(fast.0 < original.0, "{request:?}: sooner");
    }
}

/// A battle from the kernel menu's test, fought with Cross 8 of every 64
/// frames to its end: the frames the test took to start, the battle's
/// frames, the context after it and the seeks the game counted.
fn battle(disc: &PathBuf, fast: bool, formation: u8, file: u8) -> (u64, u64, String, i64) {
    let mut session = session(disc, fast);
    let launched = launch(&mut session, Request::Battle { formation, file, party: [0, 1, 2], gears: [0; 3] });
    let cross = button_bit("cross").unwrap();
    let mut battling = 0u64;
    while session.runtime.services().mode == Some(2) || battling == 0 {
        assert!(battling < 40_000, "the battle ends");
        battling += 1;
        session.set_pad(0, if battling % 64 < 8 { cross } else { 0 });
        step(&mut session);
    }
    session.set_pad(0, 0);
    frames(&mut session, 300);
    no_disc_errors(&mut session, &format!("battle {file}"));
    let context = format!("{:?}", session.runtime.context());
    let kind = context.split([' ', '{']).next().unwrap_or_default().to_string();
    (launched, battling, kind, value(&mut session, "cd_stat_setloc_count"))
}

/// Battles load their stage, their enemies and, as each attack begins, its
/// pictures (a stream of strips into VRAM) and sounds. With the drive fast
/// each is fought to its end with the same reads asked of the disc, sooner.
#[test]
fn battles_are_fought_to_the_same_end_with_the_same_reads_sooner() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    for (formation, file) in [(3, 0), (0, 1)] {
        let ((launched, battled, after, reads), (fast_launched, fast_battled, fast_after, fast_reads)) = both(|fast| battle(&disc, fast, formation, file));
        eprintln!("battle {file} (formation {formation}): started after {launched} frames and fought in {battled} at the disc's pace, {fast_launched} and {fast_battled} with the drive fast; \
            {reads} reads; then {after}");
        assert_eq!((&fast_after, fast_reads), (&after, reads), "battle {file}: the same context after it and as many reads asked of the disc");
        assert!(fast_launched < launched && fast_battled < battled, "battle {file}: sooner");
    }
}

/// The world map on foot outside Lahan, as a player leaves the village
/// (states.rs' route, by what the game shows): the frames to it, then 400
/// frames walking south: the frames its terrain stream read in, the errors
/// it counted while the map ran.
fn world_map(disc: &PathBuf, fast: bool) -> (u64, u64, [i64; 2]) {
    let mut session = session(disc, fast);
    run(&mut session, json!({"cmd": "enter_field", "map": 1}));
    let start = session.runtime.frames();
    let exit = json!({"cmd": "assist", "intent": {"kind": "walk", "layer": 0, "to": {"x": -300.0, "y": 112.5, "z": -1800.0}, "triangle": 295}});
    let held = |names: &[&str]| names.iter().map(|name| button_bit(name).unwrap()).fold(0, |all, bit| all | bit);
    // Walk to the gate, push through it, walk on and out.
    let (mut phase, mut pushed) = (0, 0);
    while !matches!(session.runtime.context(), Context::WorldMap { movement: 1, .. }) {
        assert!(session.runtime.frames() - start < 8000, "the party leaves Lahan (phase {phase})");
        let walking = session.runtime.context() == Context::Walk;
        let assisting = !run(&mut session, json!({"cmd": "status"}))["assist"].is_null();
        match phase {
            0 | 3 if walking => {
                run(&mut session, exit.clone());
                phase += 1;
            }
            1 | 4 if !assisting => phase += 1,
            2 => {
                pushed += 1;
                session.set_pad(0, if pushed <= 100 { held(&["right", "down", "cross"]) } else { 0 });
                if pushed > 100 {
                    phase = 3;
                }
            }
            5 => session.set_pad(0, held(&["down", "cross"])),
            _ => {}
        }
        step(&mut session);
    }
    let reached = session.runtime.frames() - start;
    let mut streaming = 0;
    let mut errors = [0; 2];
    session.set_pad(0, held(&["down"]));
    for _ in 0..400 {
        step(&mut session);
        if !matches!(session.runtime.context(), Context::WorldMap { .. }) {
            break;
        }
        streaming += u64::from(value(&mut session, "worldmap_stream_state") != 0);
        errors = [value(&mut session, "worldmap_stream_read_error_count"), value(&mut session, "worldmap_stream_status_error_count")];
    }
    no_disc_errors(&mut session, "the world map");
    (reached, streaming, errors)
}

/// The world map reads its terrain with a reader of its own (ReadS, whole
/// sectors, each one's address checked), as the party moves: with the drive
/// fast it reads in fewer frames and counts no error.
#[test]
fn the_world_map_streams_its_terrain_while_the_party_walks() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let ((reached, streaming, errors), (fast_reached, fast_streaming, fast_errors)) = both(|fast| world_map(&disc, fast));
    eprintln!("the world map from Lahan: reached after {reached} frames at the disc's pace and {fast_reached} with the drive fast; \
        its terrain read in {streaming} and {fast_streaming} of the first 400 frames walking");
    assert_eq!((errors, fast_errors), ([0; 2], [0; 2]), "the terrain reader counts no error");
    assert!(streaming >= 30, "the terrain streams while the party walks: {streaming} frames");
    assert!(fast_streaming * 3 <= streaming && fast_reached < reached, "in a third of the frames at most, the map reached sooner");
}

/// The control is recorded: a recording made with the drive fast, across
/// map 27's scene and its change to map 28, replays from its first state to
/// its memory; and a recording made at the disc's pace replays at that pace
/// in a session whose drive was fast when the replay began.
#[test]
fn the_control_is_recorded_and_a_replay_keeps_the_recordings_drive() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    for recorded_fast in [true, false] {
        let mut session = session(&disc, false);
        run(&mut session, json!({"cmd": "enter_field", "map": 27}));
        let initial = session.snapshot_bytes().expect("a state");
        run(&mut session, json!({"cmd": "record"}));
        session.set_fast_disc(recorded_fast);
        frames(&mut session, 2000);
        assert_eq!(half(&mut session, MODE_FIELD_MAP_ID) & 0xFFF, 28, "the scene went on to map 28");
        let end = hash(&mut session);
        let mut log = run(&mut session, json!({"cmd": "stop_recording"}));
        let controls: Vec<Value> = log["controls"].as_array().unwrap().iter().filter(|c| c[1]["cmd"] == "fast_disc").map(|c| c[1].clone()).collect();
        assert_eq!(controls, if recorded_fast { vec![json!({"cmd": "fast_disc", "enabled": true})] } else { vec![] }, "the change, once");
        log["cmd"] = json!("replay");
        // A session whose drive is the other way when the replay begins.
        let mut other = self::session(&disc, !recorded_fast);
        other.restore_bytes(&initial).expect("the recording's start");
        run(&mut other, log);
        assert_eq!(run(&mut other, json!({"cmd": "status"}))["fast_disc"], json!(recorded_fast), "the replay has the recording's drive");
        other.set_fast_disc(!recorded_fast);
        frames(&mut other, 2000);
        assert_eq!(hash(&mut other), end, "the replay reaches the recording's memory (recorded with the drive fast: {recorded_fast})");
    }
}

/// A state names nothing of the setting: one saved on each of the first
/// frames of Lahan's load with the drive fast (the reads under way) goes on
/// in a fresh fast session to the same memory, frame for frame, and in a
/// session at the disc's pace to the same field, later; one saved in the
/// middle of the load at the disc's pace goes on with the drive fast, sooner.
#[test]
fn a_state_saved_in_the_middle_of_a_load_goes_on_identically_and_under_the_other_setting() {
    let Some(disc) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // The reference: Lahan entered with the drive fast, a state each frame.
    let mut reference = session(&disc, true);
    run(&mut reference, json!({"cmd": "enter_field", "map": 1}));
    let mut states = Vec::new();
    for _ in 0..6 {
        states.push((reference.snapshot_bytes().expect("a state"), reference.runtime.services().next_device_event().is_some()));
        step(&mut reference);
    }
    assert!(states.iter().any(|(_, reading)| *reading), "the drive has events scheduled in some of the states");
    frames(&mut reference, 294);
    assert_eq!(reference.runtime.context(), Context::Walk, "the player walks Lahan");
    let (end, at) = (hash(&mut reference), reference.runtime.frames());
    let walks = |session: &mut Session<NativeModule>| -> u64 {
        let start = session.runtime.frames();
        while session.runtime.context() != Context::Walk {
            assert!(session.runtime.frames() - start < 600, "the player walks Lahan");
            step(session);
        }
        session.runtime.frames() - start
    };
    for (n, (state, _)) in states.iter().enumerate() {
        let mut fast = session(&disc, true);
        fast.restore_bytes(state).expect("the state restores");
        while fast.runtime.frames() < at {
            step(&mut fast);
        }
        assert_eq!(hash(&mut fast), end, "the state of frame {n} goes on to the same memory");
        // At the disc's pace the load goes on from where it was.
        let mut slow = session(&disc, false);
        slow.restore_bytes(state).expect("the state restores at the disc's pace");
        assert_eq!(run(&mut slow, json!({"cmd": "status"}))["fast_disc"], json!(false), "a state does not make the drive fast");
        let later = walks(&mut slow);
        no_disc_errors(&mut slow, "the fast load gone on at the disc's pace");
        assert_eq!(half(&mut slow, MODE_FIELD_MAP_ID) & 0xFFF, 1);
        if n == 0 {
            assert!(later > 60, "from its first frame the load takes the disc's time: {later} frames");
        }
    }
    // The other way round: saved 30 frames into the load at the disc's pace.
    let mut slow = session(&disc, false);
    run(&mut slow, json!({"cmd": "enter_field", "map": 1}));
    frames(&mut slow, 30);
    let state = slow.snapshot_bytes().expect("a state");
    let left = walks(&mut slow);
    let mut fast = session(&disc, true);
    fast.restore_bytes(&state).expect("the state restores with the drive fast");
    let sooner = walks(&mut fast);
    no_disc_errors(&mut fast, "the load gone on with the drive fast");
    assert!(sooner * 3 <= left, "the rest of the load takes {sooner} frames with the drive fast, {left} at the disc's pace");
}

/// Both disc changes with the drive fast. Booted from disc 2, the title's
/// new game asks for disc 1 and the changer brings it (discs.rs): the game
/// reads its label and index and begins on map 4. At the story's change
/// (the menu the last event of disc 1 requests) the changer still waits
/// its five seconds of game time with the notice shown, then the game
/// loads disc 2's index.
#[test]
fn both_disc_changes_go_as_at_the_discs_pace() {
    let (Some(disc1), Some(disc2)) = (disc(1), disc(2)) else {
        eprintln!("skipping: needs both discs (XEM_DISC1, XEM_DISC2), side by side");
        return;
    };
    let (original, fast) = both(|fast| {
        let mut session = session(&disc2, fast);
        let (confirm, start, up) = (GameAction::Confirm.button(), button_bit("start").unwrap(), button_bit("up").unwrap());
        let mut pressed: Option<u64> = None;
        while half(&mut session, MODE_FIELD_MAP_ID) & 0xFFF != FIRST_MAP || session.runtime.services().mode != Some(1) {
            let now = session.runtime.frames();
            assert!(now < 6000, "the new game begins");
            let spaced = |gap: u64| pressed.is_none_or(|at| now - at >= gap);
            let mode = session.runtime.services().mode;
            let buttons = match (mode, movie(&mut session), session.runtime.context()) {
                (Some(6), Some(movie), _) if movie.ends_on_press && spaced(30) => start,
                (Some(1), Some(movie), _) if movie.ends_on_press && spaced(30) => confirm,
                (Some(1), None, Context::Event { map: TITLE_MAP, buttons: tested }) if tested & SCRIPT_CONFIRM != 0 && spaced(30) => confirm,
                (Some(1), None, Context::Menu { screen: 2, commands: true, cursor, .. }) if spaced(20) => if cursor == NEW_GAME { confirm } else { up },
                _ => 0,
            };
            if buttons != 0 {
                pressed = Some(now);
            }
            session.set_pad(0, buttons);
            step(&mut session);
        }
        no_disc_errors(&mut session, "the new game on disc 2");
        (session.runtime.frames(), value(&mut session, "cd_directory_table[60]"), session.runtime.services().cd.inserted(), session.runtime.services().cd.wanted())
    });
    eprintln!("a new game on disc 2: on map 4 from disc 1 after {} frames at the disc's pace and {} with the drive fast", original.0, fast.0);
    assert_eq!((original.1, original.2, original.3), (1, 1, None), "disc 1 went in and its index loaded at the disc's pace");
    assert_eq!((fast.1, fast.2, fast.3), (1, 1, None), "and with the drive fast");
    assert!(fast.0 < original.0, "sooner");

    // The story's change, with the drive fast.
    let mut session = session(&disc1, true);
    run(&mut session, json!({"cmd": "enter_field", "map": 23}));
    frames(&mut session, 600);
    session.runtime.memory().write(FIELD_MENU_REQUEST, &6i32.to_le_bytes()).expect("the menu request");
    // Through its save questions (Cross, then Circle: there is no card).
    let mut due = None;
    for frame in 0..3000u64 {
        let button = match frame % 60 {
            0..6 => button_bit("cross"),
            30..36 => button_bit("circle"),
            _ => None,
        };
        session.set_pad(0, button.unwrap_or(0));
        step(&mut session);
        due = session.runtime.services().cd.change_due();
        if due.is_some() {
            break;
        }
    }
    let (wanted, at) = due.expect("the disc change menu asks for a disc");
    let asked = session.runtime.services().clock.now;
    assert_eq!(wanted, 2);
    assert!(at - asked <= STORY_CHANGE_CYCLES && at - asked > STORY_CHANGE_CYCLES - 2 * xem_core::clock::VBLANK_CYCLES, "five seconds of game time");
    session.set_pad(0, 0);
    while session.runtime.services().cd.change_due().is_some() {
        assert_eq!(session.runtime.services().cd.inserted(), 1, "disc 1 stays in for the five seconds");
        step(&mut session);
    }
    assert_eq!(session.runtime.services().cd.inserted(), 2);
    frames(&mut session, 300);
    assert_eq!(value(&mut session, "cd_directory_table[60]"), 2, "the game loaded disc 2's index");
    no_disc_errors(&mut session, "the story's disc change");
}

/// What a field map does from a fresh boot's entry, left to itself for 600
/// frames (longer until its loads have once been over for 60 frames): its
/// events in order (each map shown, each message a window shows for the
/// first time, the mode it leaves the field for, where the run ends: a
/// scripted battle, or the kernel menu at once for most of disc 2's maps
/// entered from its boot), the frames until its loads were first over and
/// the game's disc error counts; or why it did not get there (a map the
/// kernel menu's entry cannot play traps or never settles at either pace).
type Shown = (Vec<String>, u64, Vec<i64>);

fn entered(disc: &PathBuf, fast: bool, map: u16) -> Result<Shown, String> {
    let mut session = session(disc, fast);
    let reply = session.execute(&json!({"cmd": "enter_field", "map": map}));
    if let Some(error) = reply.get("error") {
        return Err(format!("enter_field: {error}"));
    }
    let start = session.runtime.frames();
    // The map entered is the first event, shown for a frame or not (a map
    // its disc cannot play leaves the field at once).
    let (mut events, mut quiet, mut loaded) = (vec![format!("map {map}")], 0, None);
    let mut at = Some(map);
    loop {
        let frame = session.runtime.frames() - start;
        if frame >= 600 && loaded.is_some() {
            break;
        }
        if frame >= 1500 {
            return Err("its loads never end".into());
        }
        match session.frame() {
            Ok(true) => {}
            Ok(false) => return Err("a disc read blocked".into()),
            Err(trap) => return Err(trap.to_string()),
        }
        let mode = session.runtime.services().mode;
        if mode != Some(1) {
            // The scene left the field (a battle, a movie, the world map).
            events.push(format!("mode {}", mode.map_or(-1, i64::from)));
            loaded.get_or_insert(frame);
            break;
        }
        let shown = half(&mut session, MODE_FIELD_MAP_ID) & 0xFFF;
        if at != Some(shown) {
            at = Some(shown);
            events.push(format!("map {shown}"));
        }
        if half(&mut session, 0x800B_2174) != 0 {
            for i in 0..4 {
                if let Some(n) = message(&mut session, i).filter(|_| half(&mut session, WINDOWS + i * WINDOW_SIZE + 0x40E) == 0) {
                    let event = format!("map {shown} message {n}");
                    if !events.contains(&event) {
                        events.push(event);
                    }
                }
            }
        }
        quiet = if field_loading(&mut session) { 0 } else { quiet + 1 };
        if quiet == 60 {
            loaded.get_or_insert(frame + 1 - 60);
        }
    }
    let errors = DISC_ERRORS.iter().map(|path| value(&mut session, path)).collect();
    Ok((events, loaded.expect("loads over"), errors))
}

/// Every field map the kernel menu's entry plays is entered with the drive
/// fast as at the disc's pace: left to itself its scene does the same
/// things in the same order (the maps it shows, the messages its windows
/// show, the mode it leaves the field for; the fast drive's run, ahead in
/// its scene, may do more of them), its loads are first over no later, and
/// the game counts no disc error; a map that entry cannot play (it traps,
/// or never settles) fails alike at both. A sample of disc 1's 730 maps by
/// default (every 29th); `XEM_FAST_DISC_SWEEP=1` enters every map of each
/// disc present (N: every Nth), a quarter of an hour on a desktop's cores.
#[test]
fn field_maps_enter_with_the_drive_fast_as_at_the_discs_pace() {
    let Some(disc1) = disc(1) else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let sweep = std::env::var("XEM_FAST_DISC_SWEEP").ok().and_then(|step| step.parse::<usize>().ok()).filter(|step| *step > 0);
    let discs: Vec<(u8, PathBuf)> = match sweep {
        Some(_) => [Some((1, disc1)), disc(2).map(|path| (2, path))].into_iter().flatten().collect(),
        None => vec![(1, disc1)],
    };
    let step = sweep.unwrap_or(29);
    let threads = std::thread::available_parallelism().map_or(4, |n| n.get()).min(16);
    let mut failures = Vec::new();
    for (number, disc) in &discs {
        let maps: Vec<u16> = (0..730).step_by(step).collect();
        let results: Vec<(u16, Result<Shown, String>, Result<Shown, String>)> = std::thread::scope(|scope| {
            let workers: Vec<_> = (0..threads).map(|worker| {
                let maps = &maps;
                scope.spawn(move || {
                    maps.iter().skip(worker).step_by(threads).map(|&map| (map, entered(disc, false, map), entered(disc, true, map))).collect::<Vec<_>>()
                })
            }).collect();
            workers.into_iter().flat_map(|worker| worker.join().expect("a worker")).collect()
        });
        let (mut played, mut unplayable, mut frames, mut fast_frames) = (0, Vec::new(), 0, 0);
        let mut differing = Vec::new();
        for (map, original, fast) in &results {
            match (original, fast) {
                (Ok((events, loads, _)), Ok((fast_events, fast_loads, errors))) => {
                    played += 1;
                    frames += loads;
                    fast_frames += fast_loads;
                    if !fast_events.starts_with(events) || fast_loads > loads || errors.iter().any(|count| *count != 0) {
                        differing.push(format!("map {map}: {events:?}, loads over after {loads} frames at the disc's pace; {fast_events:?}, after {fast_loads} with the drive fast, disc errors {errors:?}"));
                    }
                }
                (Err(why), Err(_)) => unplayable.push(format!("{map} ({})", why.chars().take(60).collect::<String>())),
                (original, fast) => differing.push(format!("map {map}: {original:?} at the disc's pace, {fast:?} with the drive fast")),
            }
        }
        unplayable.sort();
        differing.sort();
        let named: Vec<&str> = unplayable.iter().map(|map| map.split(' ').next().unwrap_or_default()).collect();
        eprintln!("disc {number}: {played} of {} maps entered, {} of them differing, their loads first over after {frames} frames in all at the disc's pace and {fast_frames} with the drive fast; \
            {} the entry cannot play at either: {}", maps.len(), differing.len(), unplayable.len(), if unplayable.len() <= 12 { format!("{unplayable:?}") } else { named.join(" ") });
        if !differing.is_empty() {
            failures.push(format!("disc {number}: {} maps differ:\n{}", differing.len(), differing.join("\n")));
        }
        if *number == 1 {
            assert!(played * 10 > maps.len() * 9, "disc 1: nearly every map plays: {played} of {}", maps.len());
        }
        assert!(played > 0 && fast_frames * 3 <= frames, "disc {number}: the loads are over in a third of the frames at most: {fast_frames} against {frames}");
    }
    assert!(failures.is_empty(), "{}", failures.join("\n"));
}
