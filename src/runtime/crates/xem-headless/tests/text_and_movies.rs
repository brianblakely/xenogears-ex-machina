//! Text speed, auto-advance dialogue and movie skipping on the game
//! (docs/runtime.md, Gameplay options). The timed scenes Perfect Works
//! Build had to re-script for its fast text (disc 1's maps 4, 27, 131, 288,
//! 326, 359 and 482; two of map 288's) reach the same end, story value and
//! map with Instant text as with the original's, showing the same
//! messages, timed ones among them (the opening narration needs a second
//! press a page); authored slow text (`0F 0E`, map 359's terminal
//! readouts) reveals in the same frames with Instant text as with the
//! original's; Fast and Instant reveal each page in fewer frames;
//! auto-advance takes a scene through its pages to the same end and leaves
//! a choice to the player however long it waits; the setting lets a story
//! movie end on Confirm, after which the field goes on as after the
//! movie's natural end; and the three controls are recorded, so a replay
//! reaches the recording's memory. Local only: it needs the user's disc 1
//! and a game module build.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test text_and_movies

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::collections::{BTreeMap, BTreeSet};
use std::path::PathBuf;
use xem_core::action::GameAction;
use xem_core::context::Context;
use xem_core::control::Session;
use xem_core::dialogue::Prompt;
use xem_game::{NativeModule, boot};

const MODE_FIELD_MAP_ID: u32 = 0x8004_F34C;
/// `field_event_variables[0]`, the story value, while the field runs.
const STORY: u32 = 0x800C_3A68;
/// `field_message_table` (count +0, entry offsets +4 + 2n) and each dialogue
/// window's message (+0xA8, 4 windows 0x498 apart), its owner's +0x81 (the
/// choice line taken, 0xFF until one is) through the choice's window.
const MESSAGE_TABLE: u32 = 0x800A_DBF0;
const WINDOWS: u32 = 0x800C_2698;
const WINDOW_SIZE: u32 = 0x498;
/// The field movie: stopped, requested and its frame.
const MOVIE_STOPPED: u32 = 0x800A_DB6C;
const MOVIE_REQUESTED: u32 = 0x800A_DB70;
const MOVIE_FRAME: u32 = 0x800B_06A0;

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
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

/// Where a scene starts: a field map entered as the development kernel menu
/// does (`enter_field`), then, for a scene the map plays only later in the
/// story, entered again with the game data's story value, entry parameter
/// (`game_data.entry[2]`, the map's v0002: the entrance a door's
/// `change_map_entry` names) and party set, and, for a talk, the actor
/// approached and talked to (`interact`).
#[derive(Clone, Copy)]
struct Scene {
    map: u16,
    story: Option<u16>,
    parameter: Option<u16>,
    /// The party's characters (0 Fei, 1 Elly, 2 Citan, 3 Bart, 4 Billy,
    /// 5 Rico), for a scene whose actors are the party's.
    party: Option<[u8; 3]>,
    talk: Option<u64>,
    /// The messages with timed controls (wait_done, slow) the scene shows,
    /// as `python3 -m tools.analysis.text_control --list field --item MAP`
    /// lists them.
    timed: &'static [u16],
    /// The scene also waits for Confirm with no window's prompt (a script
    /// resting on [`confirm_loop`]), which the player presses too.
    loops: bool,
}

impl Scene {
    const fn at(map: u16, timed: &'static [u16]) -> Self {
        Scene { map, story: None, parameter: None, party: None, talk: None, timed, loops: false }
    }
}

fn ram(pointer: u32) -> bool {
    (0x8000_0000..0x8020_0000).contains(&pointer)
}

/// Whether a field actor rests on a wait for Confirm outside a window (the
/// opening narration's): `31 0x0020` (branch_unless_buttons) whose
/// not-held branch is a jump straight back to it, the actor on either.
fn confirm_loop(session: &mut Session<NativeModule>) -> bool {
    let (count, descriptors, bytecode) = (word(session, 0x800A_FB0C).min(256), word(session, 0x800A_FB10), word(session, 0x800A_DC00));
    if !ram(descriptors) || !ram(bytecode) {
        return false;
    }
    // The test at `at`, its branch a jump back to it.
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

fn start(disc: &PathBuf, scene: Scene) -> Session<NativeModule> {
    let (mut session, _) = boot(disc, &Default::default()).expect("boot");
    run(&mut session, json!({"cmd": "enter_field", "map": scene.map}));
    if scene.story.is_some() || scene.parameter.is_some() || scene.party.is_some() {
        let mut writes = vec![json!({"path": "game_data.map", "value": scene.map})];
        if let Some(story) = scene.story {
            writes.push(json!({"path": "game_data.vars[0]", "value": story}));
        }
        if let Some(parameter) = scene.parameter {
            writes.push(json!({"path": "game_data.entry[2]", "value": parameter}));
        }
        if let Some(party) = scene.party {
            for (slot, character) in party.into_iter().enumerate() {
                writes.push(json!({"path": format!("game_data.party[{slot}]"), "value": character}));
            }
            // As field_main does when the field ends (for a battle or the
            // menu): the next entry loads the party's files afresh from the
            // game data, and its actors become that party.
            writes.push(json!({"path": "mode_party_file_kind", "value": 0}));
        }
        run(&mut session, json!({"cmd": "scenario", "writes": writes, "mode": 1}));
    }
    if let Some(actor) = scene.talk {
        // What the map's scripts say first, closed, until the player walks.
        for _ in 0..300 {
            run(&mut session, json!({"cmd": "frames", "count": 10}));
            match session.runtime.context() {
                Context::Walk => {
                    run(&mut session, json!({"cmd": "frames", "count": 30}));
                    if session.runtime.context() == Context::Walk {
                        break;
                    }
                }
                Context::Dialogue { choice: None } => {
                    run(&mut session, json!({"cmd": "act", "actions": ["Confirm"], "frames": 2}));
                }
                _ => {}
            }
        }
        run(&mut session, json!({"cmd": "interact", "actor": actor}));
    }
    session
}

/// How a run ended: on another map, with the player walking, or not
/// within its frames.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum End {
    Map(u16),
    Control,
    Unfinished,
}

#[derive(Debug)]
struct Played {
    end: End,
    frames: u64,
    story: u16,
    /// The messages the scene's windows showed, by number.
    shown: BTreeSet<u16>,
    /// Frames from each page's start (its window open, or the press that
    /// turned to it) to its prompt.
    reveals: Vec<u64>,
    /// Each slow message's first page (its window paced by authored slow
    /// text, the Window's frames between glyphs): frames from the window's
    /// opening to its first wait for a press, or to its closing.
    slow: BTreeMap<u16, u64>,
    presses: u32,
}

/// A message's window as it is shown, for [`Played::slow`].
struct Shown {
    message: u16,
    since: u64,
    slow: bool,
    waited: bool,
}

/// The message dialogue window `i` shows, by its number in the map's table.
fn message(session: &mut Session<NativeModule>, i: u32) -> Option<u16> {
    let at = word(session, WINDOWS + i * WINDOW_SIZE + 0xA8);
    let table = word(session, MESSAGE_TABLE);
    if !(0x8000_0000..0x8020_0000).contains(&table) {
        return None;
    }
    let count = half(session, table).min(1024);
    (0..count).find(|n| table + u32::from(half(session, table + 4 + 2 * u32::from(*n))) == at)
}

/// Plays the scene on as a player who reads at once: Confirm for one
/// vertical blank as each prompt shows (`press`; none when auto-advance
/// presses), again only if the prompt still shows 16 frames later (a press
/// a camera cut swallowed), and with `loops` likewise while a script waits
/// for Confirm outside a window, until the map changes, the player has
/// walked for 300 frames or `budget` frames have run. A choice is never
/// answered.
fn play(session: &mut Session<NativeModule>, budget: u64, press: bool, loops: bool) -> Played {
    let map = |s: &mut Session<NativeModule>| half(s, MODE_FIELD_MAP_ID) & 0xFFF;
    let start = session.runtime.frames();
    let first = map(session);
    let confirm = GameAction::Confirm.button();
    let (mut shown, mut reveals, mut presses) = (BTreeSet::new(), Vec::new(), 0);
    let (mut slow, mut open): (BTreeMap<u16, u64>, [Option<Shown>; 4]) = (BTreeMap::new(), Default::default());
    let mut page_since = None;
    let mut pressed_at: Option<u64> = None;
    let mut seen: Option<u64> = None;
    let mut last_texts = Vec::new();
    let mut looped_at: Option<u64> = None;
    let mut walking = 0;
    let mut end = None;
    while session.runtime.frames() - start < budget {
        let mode = session.runtime.services().mode;
        let prompt = xem_core::dialogue::prompt(session.runtime.memory(), mode);
        let windows = mode == Some(1) && half(session, 0x800B_2174) != 0;
        if windows {
            for i in 0..4 {
                if let Some(n) = message(session, i).filter(|_| half(session, WINDOWS + i * WINDOW_SIZE + 0x40E) == 0) {
                    shown.insert(n);
                }
            }
        }
        let now = session.runtime.frames();
        // A slow message's first page lasts until its window first waits
        // for a press (text flag 8), or closes (a timed end); its pace is
        // the one it ends with (a window opening on a message keeps the
        // last one's for a few frames).
        for (i, at) in open.iter_mut().enumerate() {
            let window = WINDOWS + i as u32 * WINDOW_SIZE;
            let message = if windows && half(session, window + 0x40E) == 0 { message(session, i as u32) } else { None };
            if at.as_ref().is_some_and(|s| Some(s.message) != message) {
                let ended = at.take().expect("a window shown");
                if ended.slow && !ended.waited {
                    slow.entry(ended.message).or_insert(now - ended.since);
                }
            }
            if let Some(n) = message {
                let s = at.get_or_insert(Shown { message: n, since: now, slow: false, waited: false });
                s.slow = half(session, window + 0x18 + 0x88) != 0;
                if !s.waited && half(session, window + 0x18 + 0x10) & 8 != 0 {
                    s.waited = true;
                    if s.slow {
                        slow.entry(n).or_insert(now - s.since);
                    }
                }
            }
        }
        let mut buttons = 0;
        // A page that ends where the one before it waited (a press turned
        // it, and it showed whole at once) waits with its window's text
        // further on: the waiting windows (text flag 8) and their text.
        let texts: Vec<(u32, u32)> = (0..4).filter_map(|i| {
            let window = WINDOWS + i * WINDOW_SIZE;
            (half(session, window + 0x40E) == 0 && half(session, window + 0x18 + 0x10) & 8 != 0).then(|| (i, word(session, window + 0x18 + 0x1C)))
        }).collect();
        if texts != last_texts {
            (seen, pressed_at) = (None, None);
            last_texts = texts;
        }
        // Each page's reveal counts from the press that turned to it, or
        // from its window's opening, to its prompt.
        match prompt {
            Prompt::Waiting { .. } => {
                if seen.is_none() {
                    seen = Some(now);
                    if let Some(since) = page_since.take() {
                        reveals.push(now - since);
                    }
                }
                if press && pressed_at.is_none_or(|at| now - at >= 16) {
                    buttons = confirm;
                    pressed_at = Some(now);
                    page_since = Some(now);
                    presses += 1;
                }
            }
            Prompt::Choice => panic!("map {first}: a choice the scene was not to show"),
            Prompt::None => {
                (seen, pressed_at) = (None, None);
                if windows && page_since.is_none() {
                    page_since = Some(now);
                }
                if loops && mode == Some(1) && confirm_loop(session) {
                    if looped_at.is_none_or(|at| now - at >= 16) {
                        buttons = confirm;
                        looped_at = Some(now);
                        presses += 1;
                    }
                } else {
                    looped_at = None;
                }
            }
        }
        if !windows && prompt == Prompt::None {
            page_since = None;
        }
        if press {
            session.set_pad(0, buttons);
        }
        assert!(session.frame().expect("the game runs"), "a disc read blocked");
        if map(session) != first && session.runtime.services().mode == Some(1) {
            end = Some(End::Map(map(session)));
            break;
        }
        walking = if session.runtime.context() == Context::Walk { walking + 1 } else { 0 };
        if walking >= 300 {
            end = Some(End::Control);
            break;
        }
    }
    session.set_pad(0, 0);
    Played { end: end.unwrap_or(End::Unfinished), frames: session.runtime.frames() - start, story: half(session, STORY), shown, reveals, slow, presses }
}

/// The scene played with the original's text and with Instant text: the
/// same end, story value and messages, its timed ones among them. The two
/// runs, or none without the disc.
fn same_end(scene: Scene, budget: u64) -> Option<Vec<Played>> {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return None;
    };
    let runs: Vec<Played> = [0, 255].into_iter().map(|glyphs| {
        let mut session = start(&disc, scene);
        session.set_text_speed(glyphs).expect("a text speed");
        play(&mut session, budget, true, scene.loops)
    }).collect();
    let (original, instant) = (&runs[0], &runs[1]);
    eprintln!("map {}: original {} frames, instant {} frames, ends {:?} and {:?}, story {} and {}, {} presses, timed messages {:?} of {:?}",
        scene.map, original.frames, instant.frames, original.end, instant.end, original.story, instant.story,
        original.presses, scene.timed, original.shown);
    assert_ne!(original.end, End::Unfinished, "map {}: the scene ends within {budget} frames", scene.map);
    assert_eq!((instant.end, instant.story), (original.end, original.story), "map {}: the same end and story value", scene.map);
    assert_eq!(instant.shown, original.shown, "map {}: the same messages", scene.map);
    assert!(scene.timed.iter().all(|n| original.shown.contains(n)), "map {}: its timed messages show", scene.map);
    assert!(instant.frames <= original.frames, "map {}: no longer with Instant text", scene.map);
    Some(runs)
}

/// The opening narration (map 4): each of its pages shows in a window
/// while its script counts out the page's time, then waits for Confirm
/// outside the window. The original's reveal lasts past that count, so the
/// press that ends the page's text also goes on; with Instant text the
/// text ends first, its press closes the window, and the narration waits
/// for a second press (Perfect Works Build re-scripted this map). With the
/// second press the narration ends as the original's does.
#[test]
fn instant_text_takes_the_opening_narration_to_lahan_with_a_second_press_on_each_page() {
    same_end(Scene { loops: true, ..Scene::at(4, &[0, 1]) }, 14_000);
    let Some(disc) = disc() else { return };
    let runs: Vec<Played> = [0, 255].into_iter().map(|glyphs| {
        let mut session = start(&disc, Scene::at(4, &[]));
        session.set_text_speed(glyphs).expect("a text speed");
        play(&mut session, 14_000, true, false)
    }).collect();
    eprintln!("map 4, pressing only at prompts: the original's {:?} after {} frames, Instant's {:?} after {}",
        runs[0].end, runs[0].frames, runs[1].end, runs[1].frames);
    assert_eq!((runs[0].end, runs[0].story), (End::Map(2), 3), "the original's narration needs one press a page");
    assert_eq!(runs[1].end, End::Unfinished, "Instant text's needs the second");
}

#[test]
fn instant_text_takes_map_27s_timed_scene_to_the_same_end() {
    same_end(Scene::at(27, &[0, 1, 2, 3, 4, 5, 6, 7]), 4_000);
}

#[test]
fn instant_text_takes_map_131s_scene_to_the_same_end() {
    same_end(Scene::at(131, &[3]), 6_000);
}

#[test]
fn instant_text_takes_map_288s_scene_from_entrance_2_to_the_same_end() {
    // Map 96's door at story 75: actor 22's scene (its event 0 keeps it at
    // entrance 2), to map 96.
    same_end(Scene { story: Some(75), parameter: Some(2), ..Scene::at(288, &[0x4F]) }, 8_000);
}

#[test]
fn instant_text_takes_map_288s_scene_from_entrance_8_to_the_same_end() {
    // Map 290's door, the story at 168 and Fei, Bart and Billy the party:
    // actor 27's scene (kept at entrance 8), to map 273.
    same_end(Scene { story: Some(168), parameter: Some(8), party: Some([0, 3, 4]), ..Scene::at(288, &[0x8D]) }, 12_000);
}

/// Map 359's data bank: Billy's party (here Citan and Bart, Elly and Rico
/// the scene's own actors) reads the Ethos's terminals, whose readouts
/// (messages 0x8C-0x8E) are authored slow text, a glyph every two field
/// frames. Instant text leaves their pace: each readout's first page takes
/// the same frames as the original's.
#[test]
fn instant_text_takes_map_359s_scene_to_the_same_end_and_its_slow_text_keeps_its_pace() {
    let scene = Scene { story: Some(158), party: Some([4, 2, 3]), ..Scene::at(359, &[0x8C, 0x8D, 0x8E, 0x8F]) };
    let Some(runs) = same_end(scene, 26_000) else { return };
    let (original, instant) = (&runs[0], &runs[1]);
    eprintln!("map 359's slow pages (message: frames to the first wait or the close): the original's {:?}, Instant's {:?}", original.slow, instant.slow);
    for readout in [0x8C, 0x8D, 0x8E] {
        let frames = original.slow.get(&readout).copied();
        assert!(frames.is_some_and(|f| f > 100), "message {readout:#x} shows as slow text: {:?}", original.slow);
        assert_eq!(instant.slow.get(&readout).copied(), frames, "message {readout:#x} keeps its pace");
    }
}

#[test]
fn instant_text_takes_map_326s_scene_to_the_same_end() {
    // Entered with parameter 5: actor 31's scene, as its event 0 lets it run.
    same_end(Scene { story: Some(153), parameter: Some(5), ..Scene::at(326, &[0, 0x23, 0x2E, 0x35, 0x38]) }, 6_000);
}

#[test]
fn instant_text_takes_map_482s_scene_to_the_same_end() {
    same_end(Scene::at(482, &[]), 4_000);
}

#[test]
fn fast_and_instant_text_reveal_each_page_in_fewer_frames() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let runs: Vec<(u32, Played)> = [0, 4, 255].into_iter().map(|glyphs| {
        let mut session = start(&disc, Scene::at(131, &[]));
        session.set_text_speed(glyphs).expect("a text speed");
        (glyphs, play(&mut session, 6_000, true, false))
    }).collect();
    for (glyphs, played) in &runs {
        let total: u64 = played.reveals.iter().sum();
        eprintln!("map 131 at {glyphs} glyphs a step: {} pages revealed in {total} frames (each {:?}), the scene {} frames",
            played.reveals.len(), played.reveals, played.frames);
    }
    let (original, fast, instant) = (&runs[0].1, &runs[1].1, &runs[2].1);
    assert_eq!((fast.reveals.len(), instant.reveals.len()), (original.reveals.len(), original.reveals.len()), "the same pages");
    for page in 0..original.reveals.len() {
        assert!(instant.reveals[page] <= fast.reveals[page] && fast.reveals[page] <= original.reveals[page], "page {page} reveals no slower");
    }
    let sum = |played: &Played| played.reveals.iter().sum::<u64>();
    assert!(sum(instant) < sum(fast) && sum(fast) < sum(original), "each speed reveals the pages in fewer frames");
}

#[test]
fn auto_advance_takes_a_scene_to_its_end_pressing_only_at_its_prompts() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut pressed = start(&disc, Scene::at(482, &[]));
    let player = play(&mut pressed, 4_000, true, false);
    let mut auto = start(&disc, Scene::at(482, &[]));
    auto.set_auto_advance(30).expect("a dwell");
    let advanced = play(&mut auto, 6_000, false, false);
    eprintln!("map 482: {} pages; the player's run {} frames, auto-advance's (30 vertical blanks a page) {} frames", player.presses, player.frames, advanced.frames);
    assert_eq!((advanced.end, advanced.story, &advanced.shown), (player.end, player.story, &player.shown), "the same end, story value and messages");
    assert!(advanced.frames > player.frames, "each page waited its dwell");
}

#[test]
fn auto_advance_leaves_a_choice_to_the_player_however_long_it_waits() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // Map 172's Repair Bot: pages, then four choices (the third its battle).
    let mut session = start(&disc, Scene { talk: Some(5), ..Scene::at(172, &[]) });
    session.set_auto_advance(20).expect("a dwell");
    let mut shown = None;
    for _ in 0..3000 {
        run(&mut session, json!({"cmd": "frames", "count": 1}));
        if let Context::Dialogue { choice: Some(choice) } = session.runtime.context() {
            shown = Some(choice);
            break;
        }
    }
    let choice = shown.expect("auto-advance turns the bot's pages to its choice");
    assert_eq!((choice.count, choice.index), (4, 0));
    run(&mut session, json!({"cmd": "frames", "count": 1200}));
    assert_eq!(session.runtime.context(), Context::Dialogue { choice: Some(choice) }, "the choice still waits, unanswered");
    assert_eq!(session.runtime.services().mode, Some(1), "no battle: nothing was chosen");
    // The player's own answer still works.
    for _ in 0..2 {
        run(&mut session, json!({"cmd": "act", "actions": ["Down"], "frames": 2}));
        run(&mut session, json!({"cmd": "frames", "count": 6}));
    }
    run(&mut session, json!({"cmd": "act", "actions": ["Confirm"], "frames": 2}));
    let battle = (0..60).any(|_| {
        run(&mut session, json!({"cmd": "frames", "count": 10}));
        session.runtime.services().mode == Some(2)
    });
    assert!(battle, "the player's third choice starts the bot's battle");
}

/// Map 25's scene to its movie (#7, 1059 frames, requested without the
/// early exit), the dialogue turned at once.
fn to_the_movie(disc: &PathBuf, skip: bool) -> Session<NativeModule> {
    let mut session = start(disc, Scene::at(25, &[]));
    session.set_movie_skip(skip);
    session.set_auto_advance(1).expect("a dwell");
    for _ in 0..8000 {
        run(&mut session, json!({"cmd": "frames", "count": 1}));
        if word(&mut session, MOVIE_REQUESTED) != 0 && word(&mut session, MOVIE_STOPPED) == 0 {
            session.set_auto_advance(0).expect("off");
            return session;
        }
    }
    panic!("map 25's movie never started");
}

/// Presses Confirm (one vertical blank) `after` frames into the movie and
/// runs until the field shows map 23: the frames the movie played, its
/// last frame and the frames to map 23.
fn press_through(session: &mut Session<NativeModule>, after: u64) -> (u64, u32, u64) {
    let start = session.runtime.frames();
    let mut ended = None;
    for frame in 0..12_000u64 {
        if frame == after || frame == after + 2 {
            run(session, json!({"cmd": "act", "actions": ["Confirm"], "frames": 1}));
        }
        let movie_frame = word(session, MOVIE_FRAME);
        run(session, json!({"cmd": "frames", "count": 1}));
        if ended.is_none() && word(session, MOVIE_REQUESTED) == 0 {
            ended = Some((session.runtime.frames() - start, movie_frame));
        }
        if half(session, MODE_FIELD_MAP_ID) & 0xFFF == 23 {
            let (played, last) = ended.expect("the movie ended first");
            return (played, last, session.runtime.frames() - start);
        }
    }
    panic!("map 23 never came");
}

#[test]
fn a_story_movie_ends_on_confirm_where_the_setting_lets_it_and_the_field_goes_on() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut original = to_the_movie(&disc, false);
    let (played, last, arrived) = press_through(&mut original, 120);
    let mut skipped = to_the_movie(&disc, true);
    let (skipped_played, skipped_last, skipped_arrived) = press_through(&mut skipped, 120);
    eprintln!("map 25's movie: the original's played {played} frames to its frame {last}, map 23 after {arrived}; \
        skippable it ended {skipped_played} frames in at its frame {skipped_last}, map 23 after {skipped_arrived}");
    assert!(last >= 1050, "the original's movie ignores Confirm and plays to its end");
    assert!(skipped_last < 100 && skipped_played < 400, "skippable, Confirm ends it");
    // The field goes on as after the natural end: map 23's opening runs to
    // the same moment, story value and memory of the scene ahead.
    let story = |s: &mut Session<NativeModule>| half(s, STORY);
    assert_eq!(story(&mut skipped), story(&mut original));
    for session in [&mut original, &mut skipped] {
        session.set_movie_skip(false);
        run(session, json!({"cmd": "frames", "count": 600}));
        assert_eq!(session.runtime.services().mode, Some(1), "the field runs on");
        assert_eq!(half(session, MODE_FIELD_MAP_ID) & 0xFFF, 23);
    }
    assert_eq!(story(&mut skipped), story(&mut original), "the same story value after map 23's opening");
}

#[test]
fn the_options_are_recorded_and_a_replay_reaches_the_recordings_memory() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let mut session = start(&disc, Scene::at(131, &[]));
    let initial = session.snapshot_bytes().expect("a state");
    run(&mut session, json!({"cmd": "record"}));
    session.set_text_speed(255).expect("a text speed");
    session.set_movie_skip(true);
    session.set_auto_advance(30).expect("a dwell");
    run(&mut session, json!({"cmd": "frames", "count": 900}));
    session.set_auto_advance(60).expect("a dwell");
    run(&mut session, json!({"cmd": "frames", "count": 900}));
    let hash = run(&mut session, json!({"cmd": "memory_hash"}));
    let mut log = run(&mut session, json!({"cmd": "stop_recording"}));
    let controls: Vec<&str> = log["controls"].as_array().unwrap().iter().filter_map(|c| c[1]["cmd"].as_str()).collect();
    for name in ["text_speed", "movie_skip", "auto_advance"] {
        assert!(controls.contains(&name), "{name} recorded: {controls:?}");
    }
    log["cmd"] = json!("replay");
    let (mut other, _) = boot(&disc, &Default::default()).expect("boot");
    other.restore_bytes(&initial).expect("the recording's start");
    run(&mut other, log);
    run(&mut other, json!({"cmd": "frames", "count": 1800}));
    assert_eq!(run(&mut other, json!({"cmd": "memory_hash"})), hash, "the replay presses and reveals as the recording did");
}
