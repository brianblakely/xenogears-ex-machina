//! Direct typed commands over the runtime, for humans, agents, tests and the
//! browser alike: one JSON command in, one JSON result out. Nothing here
//! simulates device input below the game's own interfaces; pads are set as the
//! BIOS reports them, and state is read and written through the schema.

use std::collections::HashMap;
use std::time::Duration;

use serde_json::{Value, json};
use serde::{Deserialize, Serialize};
use crate::time::FastForwardRate;

use crate::inspect::{InspectError, Schema, Type};
use crate::module::{GameModule, Trap};
use crate::pad::button;
use crate::runtime::{RESTART_DISPATCH, Runtime};

/// Pad states per vertical blank from where a recording started.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct InputLog {
    pub start_vblank: u64,
    /// (vblank, port, buttons) whenever a pad changed.
    pub changes: Vec<(u64, u32, u16)>,
    pub controls: Vec<(u64, Value)>,
    pub end_vblank: u64,
    /// The restores the recording went on across. The game's vblank count
    /// is restored with its state, so the entries after one belong to
    /// another timeline; a replay refuses the log.
    pub restores: Vec<Restore>,
}

/// A restore during a recording: what restored, the vblank it left and the
/// vblank of the state it restored.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Restore {
    pub kind: RestoreKind,
    pub from_vblank: u64,
    pub to_vblank: u64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum RestoreKind {
    /// A state loaded (`restore_bytes`: the `restore` command, a state file
    /// or a host's load).
    StateLoad,
    /// A rewind step (the `rewind` command or a held rewind).
    RewindStep,
}

/// Where a log list's vblanks first go back: (entry, its vblank, the one
/// before it), the start counting as the one before the first entry.
fn goes_back(start: u64, vblanks: impl IntoIterator<Item = u64>) -> Option<(usize, u64, u64)> {
    let mut previous = start;
    for (index, at) in vblanks.into_iter().enumerate() {
        if at < previous {
            return Some((index, at, previous));
        }
        previous = at;
    }
    None
}

/// A `random_battles` command's `off`: field map numbers 0-4095.
fn random_battle_maps(command: &Value) -> Option<Vec<u16>> {
    let off = command.get("off")?.as_array()?;
    off.iter().map(|map| map.as_u64().filter(|map| *map < 0x1000).map(|map| map as u16)).collect()
}

/// A `card_present` command's memory card slot (0, 1) and whether its card
/// is in.
fn card_presence(command: &Value) -> Option<(usize, bool)> {
    let slot = command.get("slot")?.as_u64().filter(|slot| *slot < 2)?;
    Some((slot as usize, command.get("present")?.as_bool()?))
}

pub struct Session<M: GameModule> {
    pub debug: crate::debug::Debug,
    pub runtime: Runtime<M>,
    pub schema: Option<Schema>,
    snapshots: HashMap<String, Vec<u8>>,
    /// Scheduled commands (`schedule`): each runs before the frame that
    /// starts with the runtime frame count at its number, in order.
    script: Vec<(u64, Value)>,
    recording: Option<InputLog>,
    /// Scheduled pad changes (vblank, port, buttons), applied before the frame
    /// that reaches the vertical blank.
    scheduled: Vec<(u64, u32, u16)>,
    replay_controls: Vec<(u64, Value)>,
    replay_end: Option<u64>,
    /// The last trap; the game stays stopped after one.
    pub stopped: Option<String>,
    pub rewind: crate::rewind::Rewind,
    /// An on/off switch, independent of the user's configured rate.
    pub fast_forward: bool,
    pub fast_forward_rate: FastForwardRate,
    /// The fixed rate fast-forward reaches, when the host cannot keep up with
    /// the configured one (`pace_fast_forward`); none until it falls behind.
    pub fast_forward_pace: Option<u32>,
    /// Hosts clear queued output after a restore (never future audio).
    pub timeline_revision: u64,
    rewind_clock: Box<dyn Fn() -> Duration>,
    rewind_game_paused: bool,
    /// A held rewind plays on from the state it went back to.
    rewinding: bool,
    /// Assisted movement (`navigate`, `interact`), and the pad 1 the host
    /// and scheduled input set beneath it.
    pub assist: Option<crate::assist::Assist>,
    pub assist_outcome: Option<crate::assist::Outcome>,
    base_pad: u16,
    /// Stop at a jump's takeoff instead of pressing on (an assistance option).
    pub ledge_brake: bool,
    /// The host shows first-person inspection ([`Session::set_inspecting`]).
    inspecting: bool,
    /// A session log's (`schedule`) or a replay's command runs: what it
    /// holds is the recording's, which inspection never refuses.
    recorded: bool,
    /// A shop quantity edit under way (`shop_quantity`).
    quantity: Option<Quantity>,
    /// Auto-advance ([`Session::set_auto_advance`]): the vertical blanks a
    /// dialogue's prompt shows before Confirm is pressed; 0 off.
    auto_advance: u32,
    /// The vertical blanks the prompt has shown so far, and whether the
    /// next frame presses Confirm.
    auto_waited: u32,
    auto_press: bool,
}

/// The longest auto-advance dwell: a minute of vertical blanks.
pub const MAX_AUTO_ADVANCE_DWELL: u32 = 3600;

/// A shop's proposed quantity edited by presses: `more` Right or Left,
/// `presses` left, the frame within the current press (one held, one
/// released), and the proposals' sum when it began (none in a sell list).
#[derive(Clone, Copy, Debug)]
struct Quantity {
    more: bool,
    presses: u32,
    frame: u32,
    sum: Option<u32>,
    /// Stop as soon as a press changes nothing (ALL, NONE), once one has.
    until_unchanged: bool,
    changed: bool,
}

const BUTTONS: [(&str, u16); 16] = [
    ("select", button::SELECT),
    ("l3", button::L3),
    ("r3", button::R3),
    ("start", button::START),
    ("up", button::UP),
    ("right", button::RIGHT),
    ("down", button::DOWN),
    ("left", button::LEFT),
    ("l2", button::L2),
    ("r2", button::R2),
    ("l1", button::L1),
    ("r1", button::R1),
    ("triangle", button::TRIANGLE),
    ("circle", button::CIRCLE),
    ("cross", button::CROSS),
    ("square", button::SQUARE),
];

/// Why agents' moves are refused during first-person inspection: `navigate`,
/// `interact` and a live `assist`, and while the player walks the commands
/// holding the directions, jumping or the camera's turns.
const INSPECTING: &str = "first-person inspection keeps the character where it stands: turn it off to walk, jump, turn the camera or interact";

/// A button's bit by its control name ("cross", "start", ...).
pub fn button_bit(name: &str) -> Option<u16> {
    BUTTONS.iter().find(|(n, _)| *n == name).map(|(_, b)| *b)
}

fn buttons_of(value: &Value) -> Result<u16, String> {
    let mut bits = 0;
    for name in value.as_array().ok_or("buttons must be a list of names")? {
        let name = name.as_str().ok_or("button names are strings")?;
        bits |= BUTTONS.iter().find(|(n, _)| *n == name).ok_or_else(|| format!("unknown button {name}"))?.1;
    }
    Ok(bits)
}

fn names_of(bits: u16) -> Vec<&'static str> {
    BUTTONS.iter().filter(|(_, b)| bits & b != 0).map(|(n, _)| *n).collect()
}

impl<M: GameModule> Session<M> {
    pub fn new(runtime: Runtime<M>, schema: Option<Schema>, rewind_clock: impl Fn() -> Duration + 'static) -> Self {
        let mut session = Session {
            debug: Default::default(),
            runtime,
            schema,
            snapshots: HashMap::new(),
            script: Vec::new(),
            recording: None,
            scheduled: Vec::new(),
            replay_controls: Vec::new(),
            replay_end: None,
            stopped: None,
            rewind: Default::default(),
            fast_forward: false,
            fast_forward_rate: FastForwardRate::default(),
            fast_forward_pace: None,
            timeline_revision: 0,
            rewind_clock: Box::new(rewind_clock),
            rewind_game_paused: false,
            rewinding: false,
            assist: None,
            assist_outcome: None,
            base_pad: 0,
            ledge_brake: false,
            inspecting: false,
            recorded: false,
            quantity: None,
            auto_advance: 0,
            auto_waited: 0,
            auto_press: false,
        };
        session.configure_rewind(0,crate::time::DEFAULT_REWIND_INTERVAL_SECONDS,crate::time::DEFAULT_REWIND_HISTORY_SECONDS).expect("valid default rewind settings");
        session
    }

    pub fn configure_rewind(&mut self, budget: usize, interval_seconds: u32, history_seconds: u32) -> Result<(),String> {
        self.rewind.configure(budget,interval_seconds,history_seconds,(self.rewind_clock)())?;
        self.checkpoint();
        Ok(())
    }

    /// Pausing or changing the timeline starts a fresh elapsed-play interval.
    pub fn reset_rewind_timer(&mut self) { self.rewind.reset_time((self.rewind_clock)()); }

    /// Sample focus before each game frame, including retries after disc waits.
    /// Refocusing cancels a pending request, but never resumes an actual pause.
    pub fn set_window_focus(&mut self, focused: bool, enabled: bool) {
        if self.replay_end.is_some() { return; }
        self.request_focus_pause(!focused && enabled);
    }

    fn request_focus_pause(&mut self, requested: bool) {
        if self.runtime.services().focus_pause.active() != requested {
            self.record_control(json!({"cmd":"focus_pause","requested":requested}));
            self.runtime.services().focus_pause.set_active(requested);
        }
        if requested && self.game_paused() { self.runtime.services().focus_pause.reached(); }
    }

    /// The game-frame multiplier (the configured rate, or the pace below it
    /// the host reaches), or no limit when uncapped is enabled.
    pub fn frame_factor(&self) -> Option<u32> {
        if self.fast_forward {
            self.fast_forward_rate.factor().map(|factor| self.fast_forward_pace.map_or(factor, |pace| pace.clamp(1, factor)))
        } else {
            Some(1)
        }
    }

    /// The fixed rate the host reaches: frames and the music they time run
    /// at it instead of the configured rate (whose music would starve), until
    /// fast-forward or its rate changes. Recorded, so replays match.
    pub fn pace_fast_forward(&mut self, pace: Option<u32>) -> Result<(), String> {
        if pace.is_some_and(|pace| !(1..=20).contains(&pace)) {
            return Err("fast-forward pace must be 1-20".into());
        }
        if pace != self.fast_forward_pace {
            self.record_control(json!({"cmd":"fast_forward_pace","pace":pace}));
            self.fast_forward_pace = pace;
            if self.fast_forward && let Some(factor) = self.frame_factor() {
                self.runtime.services().set_playback_speed(factor);
            }
        }
        Ok(())
    }

    pub fn configure_fast_forward(&mut self, rate: FastForwardRate) -> Result<(), String> {
        rate.validate()?;
        if rate != self.fast_forward_rate {
            self.record_control(json!({"cmd":"fast_forward_configure","rate":rate}));
            self.fast_forward_rate = rate;
            self.fast_forward_pace = None;
            if self.fast_forward {
                let factor = self.frame_factor();
                self.runtime.services().set_playback_speed(factor.unwrap_or(1));
                self.runtime.set_external_music(factor.is_none());
            }
        }
        Ok(())
    }

    fn set_fast_forward(&mut self, enabled: bool) {
        if self.fast_forward != enabled {
            self.record_control(json!({"cmd":"fast_forward","enabled":enabled}));
            self.fast_forward = enabled;
            self.fast_forward_pace = None;
            let factor = self.frame_factor();
            self.runtime.services().set_playback_speed(factor.unwrap_or(1));
            self.runtime.set_external_music(factor.is_none());
        }
    }

    fn vblank(&mut self) -> u64 {
        self.runtime.services().clock.vblanks
    }

    pub(crate) fn record_control(&mut self, command: Value) {
        let vblank = self.vblank();
        if let Some(log) = &mut self.recording { log.controls.push((vblank, command)); }
    }

    /// The host's view for the game's actors (src/port/activity.c): a field
    /// actor this view still shows keeps moving when the original screen
    /// would stop it. `view` is the field camera's pull-back in percent plus
    /// 100 (bits 0-15) and the pixels shown beyond each side (bits 16-31);
    /// 0 is the original's. It changes the game, so it is recorded with the
    /// inputs, and a replay keeps the recorded one.
    pub fn set_actor_view(&mut self, view: u32) -> Result<(), String> {
        if self.replay_end.is_some() {
            return Ok(());
        }
        self.apply_actor_view(view)
    }

    fn apply_actor_view(&mut self, view: u32) -> Result<(), String> {
        if view != 0 && (!(100..=1000).contains(&(view & 0xFFFF)) || view >> 16 > 1024) {
            return Err("actor_view is 0, or a pull-back of 100-1000 percent with sides of at most 1024 pixels".into());
        }
        if self.runtime.services().actor_view != view {
            self.record_control(json!({"cmd":"actor_view","view":view}));
            self.runtime.services().actor_view = view;
        }
        Ok(())
    }

    /// Whether the battle's attack page stays shown through the attack
    /// sequence (src/port/attack_page.c) rather than fading after its first
    /// attack, as when the player began the sequence by pointing at its
    /// attacks. It changes what the game draws, so it is recorded with the
    /// inputs, and a replay keeps the recorded one.
    pub fn set_attack_page(&mut self, kept: bool) {
        if self.replay_end.is_some() {
            return;
        }
        self.apply_attack_page(kept);
    }

    fn apply_attack_page(&mut self, kept: bool) {
        if self.runtime.services().attack_page != kept {
            self.record_control(json!({"cmd":"attack_page","kept":kept}));
            self.runtime.services().attack_page = kept;
        }
    }

    /// The field maps whose random battles the host turns off
    /// (src/port/encounters.c): player control's random draw does not run
    /// there, so no random battle starts, while scripted battles, map
    /// changes and the world map's encounters stay the game's. `off` holds
    /// field map numbers (the low 12 bits of `mode_field_map_id`) in any
    /// order; none is the original's. It changes the game, so it is
    /// recorded with the inputs, and a replay keeps the recorded maps.
    pub fn set_random_battles(&mut self, off: &[u16]) -> Result<(), String> {
        if self.replay_end.is_some() {
            return Ok(());
        }
        self.apply_random_battles(off)
    }

    fn apply_random_battles(&mut self, off: &[u16]) -> Result<(), String> {
        if self.runtime.services().random_battles_off == off {
            return Ok(());
        }
        if let Some(map) = off.iter().find(|map| **map >= 0x1000) {
            return Err(format!("random_battles turns off field maps 0-4095, not {map}"));
        }
        let mut maps = off.to_vec();
        maps.sort_unstable();
        maps.dedup();
        if self.runtime.services().random_battles_off != maps {
            self.record_control(json!({"cmd":"random_battles","off":maps}));
            self.runtime.services().random_battles_off = maps;
        }
        Ok(())
    }

    /// Whether the host halves the field's random battles
    /// (src/port/encounters.c): on the maps whose random battles it leaves
    /// on, the draw runs every other field frame, so the same walk brings
    /// half its battles; scripted battles and the world map's stay the
    /// game's. Off is the original. It changes the game, so it is recorded
    /// with the inputs, and a replay keeps the recorded one.
    pub fn set_half_encounters(&mut self, enabled: bool) {
        if self.replay_end.is_some() {
            return;
        }
        self.apply_half_encounters(enabled);
    }

    fn apply_half_encounters(&mut self, enabled: bool) {
        if self.runtime.services().half_encounters != enabled {
            self.record_control(json!({"cmd":"half_encounters","enabled":enabled}));
            self.runtime.services().half_encounters = enabled;
        }
    }

    /// Put memory card `slot` in or take it out (`Card::set_present`; both
    /// are in by default, as on a console whose cards stay in): the game's
    /// checks of a card taken out time out and its file calls fail, as the
    /// BIOS answers without a card, and its file screens list a card put
    /// back anew. It changes the game, so it is recorded with the inputs,
    /// and a replay keeps the recorded cards. Agents and tests change it
    /// (`card_present`); the hosts leave both cards in.
    fn apply_card_present(&mut self, slot: usize, present: bool) {
        if self.runtime.services().card.present(slot) != present {
            self.record_control(json!({"cmd":"card_present","slot":slot,"present":present}));
            self.runtime.services().card.set_present(slot, present);
        }
    }

    /// The host's text speed (src/port/text_speed.c): the glyphs a message
    /// window's step reveals at least, 1-255 (255: a line at a time); the
    /// windows still stop at their pages, pauses and timed waits, authored
    /// slow text keeps its pace, and a page a field script counts out before
    /// its wait for Confirm (the opening narration's) reveals at the
    /// original's pace until the count ends ([`crate::dialogue::page_counted_out`]).
    /// 0 is the window's own speed, the original's. It changes the game, so
    /// it is recorded with the inputs, and a replay keeps the recorded one.
    pub fn set_text_speed(&mut self, glyphs: u32) -> Result<(), String> {
        if self.replay_end.is_some() {
            return Ok(());
        }
        self.apply_text_speed(glyphs)
    }

    fn apply_text_speed(&mut self, glyphs: u32) -> Result<(), String> {
        if glyphs > 255 {
            return Err("text_speed glyphs is 0 (the original's speed) or 1-255".into());
        }
        if self.runtime.services().text_speed != glyphs {
            self.record_control(json!({"cmd":"text_speed","glyphs":glyphs}));
            self.runtime.services().text_speed = glyphs;
        }
        Ok(())
    }

    /// Whether the player's press ends any movie (src/port/movie_skip.c):
    /// the story's movies take the original's own early exit (Confirm in
    /// the field, Cross or Start in the movie mode) that only some movies
    /// allow; window movies stay the scripts'. Off is the original. It
    /// changes the game, so it is recorded with the inputs, and a replay
    /// keeps the recorded one.
    pub fn set_movie_skip(&mut self, enabled: bool) {
        if self.replay_end.is_some() {
            return;
        }
        self.apply_movie_skip(enabled);
    }

    fn apply_movie_skip(&mut self, enabled: bool) {
        if self.runtime.services().movie_skip != enabled {
            self.record_control(json!({"cmd":"movie_skip","enabled":enabled}));
            self.runtime.services().movie_skip = enabled;
        }
    }

    /// Auto-advance dialogue (`crate::dialogue`): once a field dialogue's
    /// prompt has shown for `dwell` vertical blanks, and no choice shows or
    /// is about to open there, the session presses Confirm on pad 1 for one
    /// vertical blank, as the player would; the player's own presses work
    /// as before. 0 turns it off, the original. The presses follow from the
    /// game's memory and the dwell alone, counted from when the dwell
    /// changes, a recording or replay starts or a state is restored, so the
    /// dwell is recorded with the inputs and a replay presses where the
    /// recording did.
    pub fn set_auto_advance(&mut self, dwell: u32) -> Result<(), String> {
        if self.replay_end.is_some() {
            return Ok(());
        }
        self.apply_auto_advance(dwell)
    }

    fn apply_auto_advance(&mut self, dwell: u32) -> Result<(), String> {
        if dwell > MAX_AUTO_ADVANCE_DWELL {
            return Err(format!("auto_advance dwell is 0 (off) or 1-{MAX_AUTO_ADVANCE_DWELL} vertical blanks"));
        }
        if self.auto_advance != dwell {
            self.record_control(json!({"cmd":"auto_advance","dwell":dwell}));
            self.auto_advance = dwell;
            self.restart_auto_advance();
        }
        Ok(())
    }

    /// The dwell counts from now, with no press due.
    fn restart_auto_advance(&mut self) {
        if std::mem::take(&mut self.auto_press) {
            self.restore_base_pad();
        }
        self.auto_waited = 0;
    }

    /// After a frame the game finished: the press it held ends, else the
    /// prompt's dwell counts on (the game paused counts nothing) and, once
    /// it is reached, the next frame presses.
    fn follow_auto_advance(&mut self) {
        if self.auto_advance == 0 {
            return;
        }
        if std::mem::take(&mut self.auto_press) {
            self.auto_waited = 0;
            return self.restore_base_pad();
        }
        let mode = self.runtime.services().mode;
        let waiting = matches!(crate::dialogue::prompt(self.runtime.memory(), mode), crate::dialogue::Prompt::Waiting { .. });
        if !waiting || self.game_paused() {
            self.auto_waited = 0;
            return;
        }
        self.auto_waited += 1;
        self.auto_press = self.auto_waited >= self.auto_advance;
    }

    /// The game's sound mode the host chooses (src/port/sound.c): 0 Mono,
    /// 1 Stereo, 2 Wide, as the game's Sound screen numbers them. The game
    /// takes it when its sound driver starts, whenever it changes and after
    /// a state is restored. It changes the game, so it is recorded with the
    /// inputs, and a replay keeps the recorded one. A choice on the game's
    /// Sound screen the host has not taken yet
    /// ([`take_sound_mode_choice`](Self::take_sound_mode_choice)) stands
    /// over the host's mode.
    pub fn set_sound_mode(&mut self, mode: u32) -> Result<(), String> {
        if self.replay_end.is_some() || self.runtime.services().sound_mode_chosen.is_some() {
            return Ok(());
        }
        self.apply_sound_mode(mode, false)
    }

    /// The mode the game's Sound screen chose since the last call, for the
    /// host's setting to follow.
    pub fn take_sound_mode_choice(&mut self) -> Option<u32> {
        self.runtime.services().sound_mode_chosen.take()
    }

    /// `taken`: the game has taken the mode already (a recording's start).
    fn apply_sound_mode(&mut self, mode: u32, taken: bool) -> Result<(), String> {
        if mode > 2 {
            return Err("sound_mode is 0 (Mono), 1 (Stereo) or 2 (Wide)".into());
        }
        let services = self.runtime.services();
        if taken {
            services.sound_mode = Some(mode);
            services.sound_mode_changed = false;
        } else if services.sound_mode != Some(mode) {
            services.sound_mode = Some(mode);
            services.sound_mode_changed = true;
            self.record_control(json!({"cmd":"sound_mode","mode":mode}));
        }
        Ok(())
    }

    /// The Binaural sound mode's settings (`crate::binaural`): presentation
    /// only, never recorded; it works where the SPU's output is kept for a
    /// host. Hosts run the game in Stereo under it (`set_sound_mode(1)`).
    pub fn set_binaural(&mut self, config: crate::binaural::Config) {
        let starting = self.runtime.services().spu.binaural().is_none();
        self.runtime.services().spu.configure_binaural(config);
        // A presentation starting reads the scene at once: the voices
        // sounding are identified at the next register sync, within the
        // frame, by where their emitters are.
        if starting {
            self.runtime.update_binaural();
        }
    }

    /// The listener's view while the presentation looks from elsewhere
    /// (first-person inspection, a headset's diorama): its axes (right,
    /// down, forward) in the view space of the camera the frame shown was
    /// drawn with, and that camera (`crate::binaural::ListenerView`); none:
    /// the camera's own.
    pub fn set_listener_view(&mut self, view: Option<crate::binaural::ListenerView>) {
        if let Some(presenter) = self.runtime.services().spu.binaural_mut() {
            presenter.set_view(view);
        }
    }

    /// The head's axes in the presentation's reference space (headsets), as
    /// the game camera's view space is oriented; none: no head (out of a
    /// headset; a host that loses tracking keeps the last by not calling).
    /// Presentation only: it never reaches the game.
    pub fn set_head(&mut self, axes: Option<[[f32; 3]; 3]>) {
        if let Some(presenter) = self.runtime.services().spu.binaural_mut() {
            presenter.set_head(axes);
        }
    }

    /// Whether a recording is replaying (its controls drive fast-forward).
    pub fn replaying(&self) -> bool {
        self.replay_end.is_some()
    }

    /// Whether live elapsed time drives music. Fixed-rate headless stepping
    /// keeps its deterministic divided game clock until a host attaches it.
    pub fn live_music(&mut self) -> bool {
        self.fast_forward && self.replay_end.is_none() && self.stopped.is_none()
            && self.runtime.services().clock.external_audio
    }

    /// Attach the interactive host's music clock before its first accelerated
    /// batch. A bounded batch may run fewer frames than requested, so dividing
    /// those frames by the requested speed would starve the audio device.
    /// Record the attachment before any frames, so replay does not run their
    /// music twice before it reaches the first elapsed-time input.
    pub fn begin_live_music(&mut self) -> Result<bool, String> {
        if !self.fast_forward || self.replay_end.is_some() || self.stopped.is_some() { return Ok(false); }
        if !self.runtime.services().clock.external_audio { self.advance_music(0)?; }
        Ok(true)
    }

    pub fn advance_live_music(&mut self, cycles: u64) -> Result<(), String> {
        if self.live_music() { self.advance_music(cycles)?; }
        Ok(())
    }

    fn advance_music(&mut self, cycles: u64) -> Result<(), String> {
        if cycles > crate::clock::CPU_HZ/10 { return Err("music advance is bounded to 100 ms".into()); }
        self.runtime.set_external_music(true);
        if let Err(error) = self.runtime.advance_music(cycles) {
            let reason = error.to_string();
            self.stopped = Some(reason.clone());
            return Err(reason);
        }
        self.record_control(json!({"cmd":"music_advance","cycles":cycles}));
        Ok(())
    }

    fn apply_replay_controls(&mut self) -> Result<(), Trap> {
        let now = self.vblank();
        while self.replay_controls.first().is_some_and(|(at, _)| *at <= now) {
            let (_, command) = self.replay_controls.remove(0);
            let was = std::mem::replace(&mut self.recorded, true);
            let done = self.try_execute(&command);
            self.recorded = was;
            done.map_err(Trap::Host)?;
        }
        if self.replay_end.is_some_and(|end| now >= end) { self.replay_end = None; }
        Ok(())
    }

    /// Set a pad's buttons now, as `pad` does (a recording keeps the change).
    pub fn set_pad(&mut self, port: u32, buttons: u16) {
        let vblank = self.vblank();
        if port == 0 {
            self.base_pad = buttons;
        }
        let pads = &mut self.runtime.services().pads.0;
        if let Some(pad) = pads.get_mut(port as usize) {
            pad.buttons = buttons;
        }
        if let Some(log) = &mut self.recording {
            log.changes.push((vblank, port, buttons));
        }
    }

    /// Run one frame (to the next vertical blank), applying scheduled input.
    /// `Ok(false)`: the frame stopped while the disc source fetches a sector
    /// (browser hosts); fetch it and call again, the frame continues.
    pub fn frame(&mut self) -> Result<bool, Trap> {
        if let Some(reason) = &self.stopped {
            return Err(Trap::Host(reason.clone()));
        }
        self.apply_replay_controls()?;
        while self.script.first().is_some_and(|(at, _)| *at <= self.runtime.frames()) {
            let (at, command) = self.script.remove(0);
            let was = std::mem::replace(&mut self.recorded, true);
            let reply = self.execute(&command);
            self.recorded = was;
            if let Some(error) = reply.get("error") {
                let reason = format!("scheduled command at frame {at} failed: {error}");
                self.stopped = Some(reason.clone());
                return Err(Trap::Host(reason));
            }
        }
        let vblank = self.vblank();
        let due: Vec<_> = self.scheduled.iter().filter(|(at, _, _)| *at <= vblank).copied().collect();
        self.scheduled.retain(|(at, _, _)| *at > vblank);
        for (_, port, buttons) in due {
            self.set_pad(port, buttons);
        }
        self.apply_assist();
        self.advance_debug().map_err(Trap::Host)?;
        if self.auto_press {
            let confirm = crate::action::GameAction::Confirm.button_with(&self.runtime.button_assignment());
            if let Some(pad) = self.runtime.services().pads.0.get_mut(0) {
                pad.buttons |= confirm;
            }
        }
        match self.runtime.run_frame() {
            Ok(report) => {
                if !report.blocked {
                    self.debug_frame_finished();
                    // Before the controls due next: a recording's change of
                    // the dwell lands after the frame it followed.
                    self.follow_auto_advance();
                    self.apply_replay_controls()?;
                    self.checkpoint();
                    self.trace_title_movie();
                    self.runtime.update_binaural();
                }
                Ok(!report.blocked)
            },
            Err(trap) => {
                if self.debug.busy() { let _ = self.fail_debug(&trap.to_string()); }
                self.stopped = Some(trap.to_string());
                Err(trap)
            }
        }
    }

    /// Starts assisted movement (replacing any under way).
    pub fn start_assist(&mut self, intent: crate::assist::Intent) {
        self.assist_outcome = None;
        let assist = crate::assist::Assist::new(intent, self.runtime.memory(), self.ledge_brake);
        self.record_control(json!({"cmd": "assist", "intent": assist.intent}));
        self.assist = Some(assist);
    }

    /// Ends assisted movement, releasing what it held.
    pub fn stop_assist(&mut self, outcome: crate::assist::Outcome) {
        if self.assist.take().is_some() {
            self.assist_outcome = Some(outcome);
            if outcome == crate::assist::Outcome::Stopped {
                self.record_control(json!({"cmd": "assist_stop"}));
            }
        }
        let base = self.base_pad;
        if let Some(pad) = self.runtime.services().pads.0.get_mut(0) {
            pad.buttons = base;
        }
    }

    /// Whether the host shows first-person inspection, which keeps the
    /// character where it stands: turning it on stops a route under way
    /// (recorded as `assist_stop`, as the player's own stop is), and while
    /// it is on `navigate`, `interact` and a live `assist` are refused, and
    /// while the player walks so are `pad`, `press`, `hold` and `act` holding
    /// pad 1's directions, jump or the camera's turns under the game's button
    /// assignment ([`crate::action::INSPECTION_HELD`]). A refused command
    /// changes and records nothing. A session log's and a replay's commands
    /// still run (they are the recording's), and so does the host's own
    /// `set_pad`. Hosts set it before every batch of frames; a replay keeps
    /// its own record and ignores it.
    pub fn set_inspecting(&mut self, on: bool) {
        if self.replay_end.is_some() {
            return;
        }
        if on && !self.inspecting {
            self.stop_assist(crate::assist::Outcome::Stopped);
        }
        self.inspecting = on;
    }

    /// Whether a command holding `buttons` on pad `port` may run now: not an
    /// agent's live command holding what inspection keeps from the field
    /// while the player walks ([`Session::set_inspecting`]).
    fn inspection_allows(&mut self, port: u32, buttons: u16) -> Result<(), String> {
        if !self.inspecting || self.recorded || port != 0 || self.runtime.context() != crate::context::Context::Walk {
            return Ok(());
        }
        let held = crate::action::buttons_with(crate::action::INSPECTION_HELD, &self.runtime.button_assignment());
        if buttons & held != 0 {
            return Err(INSPECTING.into());
        }
        Ok(())
    }

    /// Pad 1 for the next frame: the base input, and the assist's D-pad and
    /// Run over it while it runs. A direction of the player's own ends it.
    fn apply_assist(&mut self) {
        self.apply_quantity();
        let Some(mut assist) = self.assist.take() else { return };
        if self.base_pad & crate::assist::direction_buttons() != 0 {
            self.assist = Some(assist);
            return self.stop_assist(crate::assist::Outcome::Replaced);
        }
        let ui = self.runtime.context();
        let assignment = self.runtime.button_assignment();
        let step = assist.step(self.runtime.memory(), ui);
        if let Some(outcome) = step.done {
            self.assist = Some(assist);
            return self.stop_assist(outcome);
        }
        let held = crate::action::buttons_with(step.actions, &assignment);
        let base = self.base_pad;
        if let Some(pad) = self.runtime.services().pads.0.get_mut(0) {
            pad.buttons = (base & !crate::assist::owned_buttons(&assignment)) | held;
        }
        self.assist = Some(assist);
    }

    /// Presses for a shop quantity edit, one frame held and one released,
    /// until its presses run out, a press changes nothing (ALL, NONE), or
    /// the quantity list closes.
    fn apply_quantity(&mut self) {
        let Some(mut q) = self.quantity.take() else { return };
        if !matches!(self.runtime.context(), crate::context::Context::Menu { quantities: true, .. }) {
            return;
        }
        let assignment = self.runtime.button_assignment();
        let button = crate::action::GameAction::button_with(if q.more { crate::action::GameAction::Right } else { crate::action::GameAction::Left }, &assignment);
        if q.frame == 0 {
            if q.presses == 0 {
                return self.restore_base_pad();
            }
            if q.until_unchanged {
                // The buy list's proposals stop changing at the limit; a
                // sell list's are not in memory, and it presses on (the
                // game ignores presses past the limit).
                let sum = crate::menu::shop_amounts(self.runtime.memory());
                if q.changed && sum == q.sum {
                    return self.restore_base_pad();
                }
                q.changed |= q.sum.is_some() && sum != q.sum && q.presses < 99;
                q.sum = sum;
            }
            q.presses = q.presses.saturating_sub(1);
        }
        let held = if q.frame == 0 { button } else { 0 };
        let base = self.base_pad;
        if let Some(pad) = self.runtime.services().pads.0.get_mut(0) {
            pad.buttons = (base & !crate::assist::direction_buttons()) | held;
        }
        q.frame = (q.frame + 1) % 2;
        self.quantity = Some(q);
    }

    fn restore_base_pad(&mut self) {
        let base = self.base_pad;
        if let Some(pad) = self.runtime.services().pads.0.get_mut(0) {
            pad.buttons = base;
        }
    }

    /// For a draw trace showing the title movie, the frames its player has
    /// left after the one it loaded last, by the player's own count: the
    /// renderer eases its movie filter out over the last of them, as the
    /// movie's last frame is the title screen's picture. The title movie is
    /// the movie mode's (6) playing the boot's request (kind 1; the battle
    /// events' requests set bit 7). Read only.
    fn trace_title_movie(&mut self) {
        let services = self.runtime.services();
        let device = services.gpu.device();
        if services.mode != Some(6) || !device.trace_enabled() || !device.display().color24 {
            return;
        }
        let mut read = |path: &str| self.inspect(path, 0).ok().and_then(|value| value.as_i64());
        // Playing to its end (1), or stopped there (-1), its last frame shown.
        let (Some(1), Some(1 | -1), Some(shown), Some(end)) =
            (read("cd_movie_request_kind"), read("movie_player_state"), read("movie_shown_frame"), read("movie_end_frame"))
        else {
            return;
        };
        if let Some(left) = (0..=end).contains(&shown).then(|| u16::try_from(end - shown).ok()).flatten() {
            self.runtime.services().gpu.device_mut().trace_movie_frames_left(left);
        }
    }

    fn schema(&self) -> Result<&Schema, String> {
        self.schema.as_ref().ok_or_else(|| "no schema (build/game/schema.json) loaded".to_string())
    }

    pub(crate) fn inspect(&mut self, path: &str, depth: u32) -> Result<Value, String> {
        let schema = self.schema.as_ref().ok_or("no schema loaded")?;
        let memory = self.runtime.memory();
        let place = schema.locate(path, memory).map_err(|e: InspectError| e.to_string())?;
        schema.decode(place, memory, depth).map_err(|e| e.to_string())
    }

    /// Write a scalar at a path (integers, enums by name, pointers as numbers).
    pub(crate) fn write_path(&mut self, path: &str, value: &Value) -> Result<(), String> {
        let schema = self.schema.as_ref().ok_or("no schema loaded")?;
        let memory = self.runtime.memory();
        let place = schema.locate(path, memory).map_err(|e| e.to_string())?;
        let size = schema.size_of(place.ty) as usize;
        let mut ty = place.ty;
        while let Some(Type::Typedef { ty: inner, .. }) = ty.map(|t| &schema.types[t]) {
            ty = *inner;
        }
        let number = match (ty.map(|t| &schema.types[t]), value) {
            (Some(Type::Enum { values, .. }), Value::String(name)) => {
                *values.get(name).ok_or_else(|| format!("no enumerator {name}"))?
            }
            (Some(Type::Struct { .. }) | Some(Type::Union { .. }) | Some(Type::Array { .. }), _) => {
                return Err("write scalars, member by member".into());
            }
            (_, v) => v.as_i64().or_else(|| v.as_u64().map(|u| u as i64)).ok_or("value must be an integer")?,
        };
        let bytes = number.to_le_bytes();
        memory.write(place.address, &bytes[..size.min(8)]).map_err(|e| e.to_string())
    }

    fn condition_met(&mut self, until: &Value) -> Result<bool, String> {
        if let Some(n) = until.get("vblanks").and_then(Value::as_u64) {
            return Ok(self.vblank() >= n);
        }
        if let Some(n) = until.get("frames").and_then(Value::as_u64) {
            return Ok(self.runtime.frames() >= n);
        }
        if let Some(path) = until.get("path").and_then(Value::as_str) {
            let value = self.inspect(path, 0)?;
            if let Some(expected) = until.get("equals") {
                return Ok(&value == expected);
            }
            if let Some(expected) = until.get("not_equals") {
                return Ok(&value != expected);
            }
            return Err("a path condition needs equals or not_equals".into());
        }
        Err("until needs vblanks or path".into())
    }

    /// Execute one command; errors come back as {"error": ...}.
    pub fn execute(&mut self, command: &Value) -> Value {
        match self.try_execute(command) {
            Ok(value) => value,
            Err(error) => json!({ "error": error }),
        }
    }

    fn status(&mut self) -> Value {
        let stopped = self.stopped.clone();
        let actor_view = self.runtime.services().actor_view;
        let attack_page = self.runtime.services().attack_page;
        let sound_mode = self.runtime.services().sound_mode;
        let random_battles_off = self.runtime.services().random_battles_off.clone();
        let half_encounters = self.runtime.services().half_encounters;
        let text_speed = self.runtime.services().text_speed;
        let movie_skip = self.runtime.services().movie_skip;
        // What the dialogue options act on, as the game shows it: the
        // field dialogue's prompt (auto-advance counts toward its dwell
        // while it waits) and the movie whose loop reads the press.
        let mode = self.runtime.services().mode;
        let dialogue = crate::dialogue::prompt(self.runtime.memory(), mode);
        let movie = crate::movie::movie(self.runtime.memory(), mode);
        let binaural = self.runtime.services().spu.binaural().map(|p| p.config());
        let services = self.runtime.services();
        let card_present = [services.card.present(0), services.card.present(1)];
        let kanji_font = services.kanji_font;
        // What the storage line shows: card writes not yet stored, the
        // cards' storage errors, and why an imported BIOS image is unused.
        let storage = json!({
            "pending_writes": services.card.pending_writes(),
            "errors": services.card.errors,
            "kanji_font_notice": services.kanji_font_notice,
        });
        json!({
            "vblanks": services.clock.vblanks,
            "cycles": services.clock.now,
            "pads": services.pads.0.iter().map(|p| json!({"connected": p.connected, "buttons": names_of(p.buttons)})).collect::<Vec<_>>(),
            "stopped": stopped,
            "query_trap": self.runtime.query_trap(),
            "recording": self.recording.is_some(),
            "snapshots": self.snapshots.keys().cloned().collect::<Vec<_>>(),
            "frames": self.runtime.frames(),
            "fast_forward": self.fast_forward,
            "fast_forward_rate": self.fast_forward_rate,
            "fast_forward_pace": self.fast_forward_pace,
            "actor_view": actor_view,
            "attack_page": attack_page,
            "sound_mode": sound_mode,
            "random_battles_off": random_battles_off,
            "half_encounters": half_encounters,
            "card_present": card_present,
            "kanji_font": kanji_font,
            "storage": storage,
            "text_speed": text_speed,
            "movie_skip": movie_skip,
            "auto_advance": self.auto_advance,
            "auto_advance_waited": self.auto_waited,
            "dialogue": dialogue,
            "movie": movie,
            "binaural": binaural,
            "rewind": self.rewind.status(),
            "assist": self.assist.as_ref().map(|a| json!({"intent": a.intent, "phase": a.phase})),
            "assist_outcome": self.assist_outcome,
            "inspecting": self.inspecting,
        })
    }

    fn try_execute(&mut self, command: &Value) -> Result<Value, String> {
        let cmd = command.get("cmd").and_then(Value::as_str).ok_or("missing cmd")?;
        let arg_u64 = |name: &str| command.get(name).and_then(Value::as_u64);
        let arg_str = |name: &str| command.get(name).and_then(Value::as_str);
        match cmd {
            "debug" => {
                let request = serde_json::from_value(command.get("request").cloned().ok_or("debug needs request")?)
                    .map_err(|e| format!("Invalid debug request: {e}"))?;
                self.request_debug(request)?;
                Ok(self.debug_status())
            }
            "debug_status" => Ok(self.debug_status()),
            "debug_tick" => { self.tick_debug()?; Ok(self.debug_status()) }
            "status" => Ok(self.status()),
            "reset" => {
                self.record_control(json!({"cmd":"reset"}));
                self.reset_game();
                Ok(self.status())
            }
            "fast_forward_configure" => {
                let rate: FastForwardRate = serde_json::from_value(command.get("rate").ok_or("fast_forward_configure needs rate")?.clone())
                    .map_err(|e| e.to_string())?;
                self.configure_fast_forward(rate)?;
                Ok(self.status())
            }
            "fast_forward" => {
                let enabled = command.get("enabled").map(|v| v.as_bool().ok_or("fast_forward enabled must be boolean"))
                    .transpose()?.unwrap_or(!self.fast_forward);
                self.set_fast_forward(enabled);
                Ok(self.status())
            }
            "fast_forward_pace" => {
                let pace = command.get("pace").map(|v| if v.is_null() { Ok(None) } else {
                    v.as_u64().and_then(|p| u32::try_from(p).ok()).map(Some).ok_or("fast_forward_pace pace must be a number or null")
                }).transpose()?.flatten();
                self.pace_fast_forward(pace)?;
                Ok(self.status())
            }
            "music_advance" => {
                let cycles = arg_u64("cycles").ok_or("music_advance needs cycles (at most 100 ms)")?;
                self.advance_music(cycles)?;
                Ok(self.status())
            }
            "attack_page" => {
                let kept = command.get("kept").and_then(Value::as_bool).ok_or("attack_page needs kept: true or false")?;
                self.apply_attack_page(kept);
                Ok(self.status())
            }
            "random_battles" => {
                let off = random_battle_maps(command).ok_or("random_battles needs off: a list of field maps 0-4095")?;
                self.apply_random_battles(&off)?;
                Ok(self.status())
            }
            "half_encounters" => {
                let enabled = command.get("enabled").and_then(Value::as_bool).ok_or("half_encounters needs enabled: true or false")?;
                self.apply_half_encounters(enabled);
                Ok(self.status())
            }
            "card_present" => {
                let (slot, present) = card_presence(command).ok_or("card_present needs slot (0 or 1) and present (true or false)")?;
                self.apply_card_present(slot, present);
                Ok(self.status())
            }
            "text_speed" => {
                let glyphs = arg_u64("glyphs").and_then(|g| u32::try_from(g).ok()).ok_or("text_speed needs glyphs: 0 (the original's) or 1-255")?;
                self.apply_text_speed(glyphs)?;
                Ok(self.status())
            }
            "movie_skip" => {
                let enabled = command.get("enabled").and_then(Value::as_bool).ok_or("movie_skip needs enabled: true or false")?;
                self.apply_movie_skip(enabled);
                Ok(self.status())
            }
            "auto_advance" => {
                let dwell = arg_u64("dwell").and_then(|d| u32::try_from(d).ok())
                    .ok_or_else(|| format!("auto_advance needs dwell: 0 (off) or 1-{MAX_AUTO_ADVANCE_DWELL} vertical blanks"))?;
                self.apply_auto_advance(dwell)?;
                Ok(self.status())
            }
            "actor_view" => {
                let view = arg_u64("view").and_then(|v| u32::try_from(v).ok()).ok_or("actor_view needs view")?;
                self.apply_actor_view(view)?;
                Ok(self.status())
            }
            "sound_mode" => {
                let mode = arg_u64("mode").and_then(|v| u32::try_from(v).ok()).ok_or("sound_mode needs mode")?;
                let taken = command.get("taken").map(|v| v.as_bool().ok_or("sound_mode taken must be boolean")).transpose()?.unwrap_or(false);
                self.apply_sound_mode(mode, taken)?;
                self.runtime.services().sound_mode_chosen = None;
                Ok(self.status())
            }
            "binaural" => {
                // The Binaural presentation: the given settings change (the
                // rest stay), and what it does now comes back.
                let current = self.runtime.services().spu.binaural().map(|p| p.config()).unwrap_or_default();
                let mut value = serde_json::to_value(current).map_err(|e| e.to_string())?;
                let mut changed = false;
                for (key, setting) in command.as_object().into_iter().flatten().filter(|(key, _)| *key != "cmd") {
                    if value.get(key).is_none() {
                        return Err(format!("binaural has no setting {key}"));
                    }
                    value[key.as_str()] = setting.clone();
                    changed = true;
                }
                let config: crate::binaural::Config = serde_json::from_value(value).map_err(|e| format!("binaural: {e}"))?;
                config.validate()?;
                if changed {
                    self.set_binaural(config);
                }
                let spu = &self.runtime.services().spu;
                Ok(match spu.binaural() {
                    Some(presenter) => presenter.status(),
                    None => json!({
                        "config": config,
                        "presenting": false,
                        "why": if spu.output_kept() { "off" } else { "the SPU's output is not kept for a host (headless without --wav): nothing to present" },
                    }),
                })
            }
            "audio_capture" => {
                // The output as presented (Binaural or the original mix),
                // kept from "start" to "stop": its length, levels, an FNV-1a
                // hash of its samples and, with "samples": true, the
                // samples as little-endian hex. "tap": true at the start
                // also keeps what the Binaural presentation mixed them from
                // (presentation only), and the stop reports which voices
                // each source had in each block.
                let spu = &mut self.runtime.services().spu;
                match arg_str("action").ok_or("audio_capture needs action: start or stop")? {
                    "start" => {
                        let tap = command.get("tap").map(|v| v.as_bool().ok_or("audio_capture tap must be boolean")).transpose()?.unwrap_or(false);
                        if tap && spu.binaural().is_none() {
                            return Err("audio_capture tap: true keeps what the Binaural presentation mixes, and nothing presents".into());
                        }
                        if !spu.start_capture() {
                            return Err("the SPU's output is not kept for a host (headless without --wav): nothing to capture".into());
                        }
                        if tap {
                            spu.start_tap_capture();
                        } else {
                            spu.take_tap_capture();
                        }
                        Ok(json!({"capturing": true, "tap": tap}))
                    }
                    "stop" => {
                        let samples = spu.take_capture().ok_or("no audio capture under way")?;
                        let tap = spu.take_tap_capture();
                        let side = |s: usize| samples.iter().skip(s).step_by(2);
                        let rms = |s: usize| {
                            let n = side(s).count().max(1) as f64;
                            let mean = side(s).map(|x| f64::from(*x).powi(2)).sum::<f64>() / n;
                            if mean > 0.0 { Some(10.0 * (mean / (32768.0 * 32768.0)).log10()) } else { None }
                        };
                        let bytes: Vec<u8> = samples.iter().flat_map(|x| x.to_le_bytes()).collect();
                        let mut hash: u64 = 0xCBF2_9CE4_8422_2325;
                        for byte in &bytes {
                            hash = (hash ^ u64::from(*byte)).wrapping_mul(0x0000_0100_0000_01B3);
                        }
                        let mut reply = json!({
                            "frames": samples.len() / 2,
                            "hash": format!("{hash:016x}"),
                            "peak": [side(0).map(|x| x.unsigned_abs()).max(), side(1).map(|x| x.unsigned_abs()).max()],
                            "rms_db": [rms(0), rms(1)],
                        });
                        if command.get("samples").and_then(Value::as_bool) == Some(true) {
                            reply["samples"] = json!(hex(&bytes));
                        }
                        if let Some(journal) = tap {
                            reply["tap"] = tap_summary(&journal);
                        }
                        Ok(reply)
                    }
                    other => Err(format!("audio_capture action {other}: start or stop")),
                }
            }
            "focus_pause" => {
                let requested = command.get("requested").and_then(Value::as_bool).ok_or("focus_pause needs requested boolean")?;
                self.request_focus_pause(requested);
                Ok(self.status())
            }
            "rewind_configure" => {
                if command.get("interval_frames").is_some() { return Err("rewind intervals use interval_seconds".into()); }
                let budget = usize::try_from(arg_u64("max_bytes").ok_or("rewind_configure needs max_bytes (0 disables)")?)
                    .map_err(|_| "rewind budget too large")?;
                let seconds = u32::try_from(arg_u64("interval_seconds").unwrap_or(u64::from(crate::time::DEFAULT_REWIND_INTERVAL_SECONDS)))
                    .map_err(|_| "rewind interval too large")?;
                let history = u32::try_from(arg_u64("history_seconds").unwrap_or(u64::from(crate::time::DEFAULT_REWIND_HISTORY_SECONDS)))
                    .map_err(|_| "rewind history too large")?;
                self.configure_rewind(budget,seconds,history)?;
                Ok(self.status())
            }
            "rewind" => {
                if command.get("frames").is_some() { return Err("rewind steps through saved states; use steps".into()); }
                let steps = usize::try_from(arg_u64("steps").unwrap_or(1)).map_err(|_| "rewind step count too large")?;
                let (at,data) = self.rewind.previous(self.runtime.frames(),steps).ok_or("requested state is outside retained rewind history")?;
                self.restore_rewind_state(&data)?;
                self.rewind.truncate(at);
                Ok(self.status())
            }
            "rewind_step" => {
                // A held rewind, as the hosts hold one (they take a step
                // every REWIND_STEP_MS while it is held): `steps` of them,
                // each one older state played on from (`rewind_step`); fewer
                // taken than asked for, the history's start was reached.
                let steps = arg_u64("steps").unwrap_or(1);
                let mut taken = 0;
                while taken < steps && self.rewind_step()? {
                    taken += 1;
                }
                Ok(json!({ "steps": taken, "status": self.status() }))
            }
            "frames" => {
                let n = arg_u64("count").unwrap_or(1);
                for done in 0..n {
                    if !self.frame().map_err(|t| t.to_string())? {
                        return Ok(json!({ "blocked": true, "frames": done, "status": self.status() }));
                    }
                }
                Ok(self.status())
            }
            "run_until" => {
                let max = arg_u64("max_frames").ok_or("run_until needs max_frames (it is bounded)")?;
                let until = command.get("until").ok_or("run_until needs until")?;
                for frame in 0..max {
                    if self.condition_met(until)? {
                        return Ok(json!({ "met": true, "frames": frame, "status": self.status() }));
                    }
                    if !self.frame().map_err(|t| t.to_string())? {
                        return Ok(json!({ "met": false, "blocked": true, "frames": frame, "status": self.status() }));
                    }
                }
                let met = self.condition_met(until)?;
                Ok(json!({ "met": met, "frames": max, "status": self.status() }))
            }
            "pad" => {
                let port = arg_u64("port").unwrap_or(0) as u32;
                let buttons = buttons_of(command.get("buttons").ok_or("pad needs buttons")?)?;
                self.inspection_allows(port, buttons)?;
                self.set_pad(port, buttons);
                Ok(self.status())
            }
            "press" => {
                // Hold the buttons for `frames` frames from now, then release.
                let port = arg_u64("port").unwrap_or(0) as u32;
                let buttons = buttons_of(command.get("buttons").ok_or("press needs buttons")?)?;
                self.inspection_allows(port, buttons)?;
                let frames = arg_u64("frames").unwrap_or(1);
                let now = self.vblank();
                self.set_pad(port, buttons);
                self.scheduled.push((now + frames, port, 0));
                Ok(self.status())
            }
            "context" => Ok(serde_json::to_value(self.runtime.context()).expect("context")),
            "targets" => {
                // The interface's selectable entries on the screen.
                let ui = self.runtime.context();
                Ok(serde_json::to_value(crate::menu::targets(self.runtime.memory(), ui)).expect("targets"))
            }
            "field" => {
                // The field as the assisted controls see it: the player, the
                // actors and where they show on the screen.
                let field = crate::field::Field::read(self.runtime.memory()).ok_or("no field")?;
                let actors: Vec<_> = field.actors.iter().flatten().map(|a| json!({
                    "index": a.index, "position": a.position, "screen": field.camera.project(a.position), "radius": a.radius,
                })).collect();
                // The random draw, and whether the host turns it off here.
                let draw = crate::field::RandomDraw::read(self.runtime.memory()).ok_or("no field")?;
                let off_here = self.runtime.services().random_battles_off.binary_search(&draw.map).is_ok();
                let mut random_draw = serde_json::to_value(draw).expect("random draw");
                random_draw["off_here"] = json!(off_here);
                Ok(json!({ "player": field.player, "camera_angle": field.camera.angle, "actors": actors, "random_draw": random_draw }))
            }
            "pick" => {
                // The floor a screen point (field screen pixels) picks.
                let screen = command.get("screen").and_then(Value::as_array).ok_or("pick needs screen [x, y]")?;
                let point = (screen.first().and_then(Value::as_f64).ok_or("screen x")? as f32, screen.get(1).and_then(Value::as_f64).ok_or("screen y")? as f32);
                let field = crate::field::Field::read(self.runtime.memory()).ok_or("no field")?;
                Ok(serde_json::to_value(field.pick(point)).expect("hit"))
            }
            "navigate" => {
                // Walk to a floor point: `to` [x, z] in world units, or the
                // floor a `screen` point picks.
                if self.inspecting {
                    return Err(INSPECTING.into());
                }
                if self.runtime.context() != crate::context::Context::Walk {
                    return Err("navigate needs the player walking in a field".into());
                }
                let field = crate::field::Field::read(self.runtime.memory()).ok_or("no field")?;
                let player = *field.player().ok_or("no player")?;
                let hit = if let Some(to) = command.get("to").and_then(Value::as_array) {
                    let (x, z) = (to.first().and_then(Value::as_f64).ok_or("to x")? as f32, to.get(1).and_then(Value::as_f64).ok_or("to z")? as f32);
                    let mesh = &field.layers[player.layer];
                    let triangle = mesh.locate(x, z).ok_or("not on the player's floor")?;
                    let point = crate::field::V3::new(x, mesh.height(triangle, x, z), z);
                    crate::field::Hit { point, triangle, layer: player.layer, reach: field.reach(&player, triangle, point) }
                } else {
                    let screen = command.get("screen").and_then(Value::as_array).ok_or("navigate needs to [x, z] or screen [x, y]")?;
                    let point = (screen.first().and_then(Value::as_f64).ok_or("screen x")? as f32, screen.get(1).and_then(Value::as_f64).ok_or("screen y")? as f32);
                    field.pick(point).ok_or("no floor there")?
                };
                if hit.reach == crate::field::Reach::Invalid {
                    return Err("the player cannot reach that point".into());
                }
                self.start_assist(crate::assist::Intent::Walk { to: hit.point, triangle: hit.triangle, layer: hit.layer });
                Ok(json!({ "hit": hit, "status": self.status() }))
            }
            "interact" => {
                if self.inspecting {
                    return Err(INSPECTING.into());
                }
                if self.runtime.context() != crate::context::Context::Walk {
                    return Err("interact needs the player walking in a field".into());
                }
                let actor = arg_u64("actor").ok_or("interact needs actor")? as usize;
                let field = crate::field::Field::read(self.runtime.memory()).ok_or("no field")?;
                if field.player == Some(actor) || field.actors.get(actor).is_none_or(Option::is_none) {
                    return Err(format!("no other actor {actor}"));
                }
                self.start_assist(crate::assist::Intent::Interact { actor });
                Ok(self.status())
            }
            "assist" => {
                // A recorded intent, as resolved when it was given: an
                // agent's own, sent live, is a route inspection refuses.
                let intent: crate::assist::Intent = serde_json::from_value(command.get("intent").ok_or("assist needs intent")?.clone())
                    .map_err(|e| e.to_string())?;
                if self.inspecting && !self.recorded {
                    return Err(INSPECTING.into());
                }
                self.start_assist(intent);
                Ok(self.status())
            }
            "shop_quantity" => {
                // The selected entry's proposed quantity: plus or minus one,
                // ALL (as many as the shop allows: funds, stock, inventory)
                // or NONE, by the list's own Right and Left.
                if !matches!(self.runtime.context(), crate::context::Context::Menu { quantities: true, .. }) {
                    return Err("shop_quantity needs a shop's buy or sell list".into());
                }
                let (more, presses, until_unchanged) = match arg_str("op").ok_or("shop_quantity needs op: plus, minus, all or none")? {
                    "plus" => (true, 1, false),
                    "minus" => (false, 1, false),
                    "all" => (true, 99, true),
                    "none" => (false, 99, true),
                    other => return Err(format!("unknown op {other}")),
                };
                self.record_control(command.clone());
                self.quantity = Some(Quantity { more, presses, frame: 0, sum: None, until_unchanged, changed: false });
                Ok(self.status())
            }
            "assist_stop" => {
                self.stop_assist(crate::assist::Outcome::Stopped);
                Ok(self.status())
            }
            "hold" => {
                // Hold the named game actions on pad 1 (the buttons they mean)
                // until the next hold; an empty list releases them.
                let actions = crate::action::actions_of(command.get("actions").ok_or("hold needs actions")?)?;
                let assignment = self.runtime.button_assignment();
                let buttons = crate::action::buttons_with(actions, &assignment);
                self.inspection_allows(0, buttons)?;
                self.set_pad(0, buttons);
                Ok(self.status())
            }
            "act" => {
                // Hold the named actions for `frames` frames from now, then release.
                let actions = crate::action::actions_of(command.get("actions").ok_or("act needs actions")?)?;
                let frames = arg_u64("frames").unwrap_or(1);
                let now = self.vblank();
                let assignment = self.runtime.button_assignment();
                let buttons = crate::action::buttons_with(actions, &assignment);
                self.inspection_allows(0, buttons)?;
                self.set_pad(0, buttons);
                self.scheduled.push((now + frames, 0, 0));
                Ok(self.status())
            }
            "connect_pad" => {
                let port = arg_u64("port").ok_or("connect_pad needs port")? as usize;
                let connected = command.get("connected").and_then(Value::as_bool).unwrap_or(true);
                let pads = &mut self.runtime.services().pads.0;
                pads.get_mut(port).ok_or("port is 0 or 1")?.connected = connected;
                Ok(self.status())
            }
            "inspect" => {
                let path = arg_str("path").ok_or("inspect needs path")?;
                let depth = arg_u64("depth").unwrap_or(2) as u32;
                self.inspect(path, depth)
            }
            "write" => {
                let path = arg_str("path").ok_or("write needs path")?;
                self.write_path(path, command.get("value").ok_or("write needs value")?)?;
                self.inspect(path, 1)
            }
            "globals" => {
                let prefix = arg_str("prefix").unwrap_or("");
                let schema = self.schema()?;
                let mut names: Vec<_> = schema.globals.keys().filter(|n| n.starts_with(prefix)).cloned().collect();
                names.sort();
                Ok(json!(names))
            }
            "read_memory" => {
                let address = arg_u64("address").ok_or("read_memory needs address")? as u32;
                let length = arg_u64("length").unwrap_or(16).min(0x10000) as usize;
                let mut data = vec![0; length];
                self.runtime.memory().read(address, &mut data).map_err(|e| e.to_string())?;
                Ok(json!(hex(&data)))
            }
            "write_memory" => {
                let address = arg_u64("address").ok_or("write_memory needs address")? as u32;
                let data = unhex(arg_str("hex").ok_or("write_memory needs hex")?)?;
                self.runtime.memory().write(address, &data).map_err(|e| e.to_string())?;
                Ok(json!({ "written": data.len() }))
            }
            "snapshot" => {
                let name = arg_str("name").unwrap_or("default").to_string();
                let data = self.snapshot_bytes()?;
                let size = data.len();
                self.snapshots.insert(name.clone(), data);
                Ok(json!({ "name": name, "bytes": size }))
            }
            "restore" => {
                let name = arg_str("name").unwrap_or("default");
                let data = self.snapshots.get(name).ok_or_else(|| format!("no snapshot {name}"))?.clone();
                self.restore_bytes(&data)?;
                Ok(self.status())
            }
            "presentation" => {
                // Turn capture on (it is off until asked for) and report the last
                // complete frame's records: kind counts and the first few.
                let presentation = &mut self.runtime.services().presentation;
                presentation.enabled = command.get("enable").and_then(Value::as_bool).unwrap_or(true);
                let records = &presentation.last_frame;
                let count = |kind| records.iter().filter(|r| r.kind == kind).count();
                let limit = arg_u64("limit").unwrap_or(8) as usize;
                Ok(json!({
                    "models": count(crate::presentation::MODEL),
                    "sprite_matrices": count(crate::presentation::SPRITE_MATRIX),
                    "sprite_parts": count(crate::presentation::SPRITE_PARTS),
                    "records": records.iter().take(limit).map(|r| json!({
                        "kind": r.kind,
                        "object": format!("{:#010x}", r.word(0)),
                        "words": (0..r.bytes.len() / 4).map(|i| r.word(i)).collect::<Vec<_>>(),
                    })).collect::<Vec<_>>(),
                }))
            }
            "readiness" => {
                // What the game is doing now: the mode the dispatcher entered,
                // what it waits for, whether it consumed pad input last frame.
                let stopped = self.stopped.clone();
                let services = self.runtime.services();
                const MODES: [&str; 7] = ["kernel menu", "field", "battle", "world map", "battling", "menu", "movie"];
                Ok(json!({
                    "mode": services.mode,
                    "mode_name": services.mode.and_then(|m| MODES.get(m as usize)).copied(),
                    "dispatches": services.dispatches,
                    "waiting_for": services.last_wait.map(|w| format!("{w:?}")),
                    // Reads of the game's pad queue (pad_dequeue_state); the
                    // field also reads the held buttons directly, so 0 does not
                    // mean the game ignores the pad.
                    "pad_queue_reads_last_frame": services.input_reads_last_frame,
                    "stopped": stopped,
                    "valid_commands": ["pad", "press", "hold", "act", "context", "targets", "field", "pick", "navigate", "interact", "assist", "assist_stop", "shop_quantity", "frames", "run_until", "inspect", "write", "snapshot",
                                        "restore", "rewind_configure", "rewind", "rewind_step", "fast_forward_configure", "fast_forward", "fast_forward_pace", "actor_view", "attack_page", "sound_mode", "random_battles",
                                        "half_encounters", "text_speed", "movie_skip", "auto_advance", "binaural", "audio_capture", "focus_pause", "music_advance", "scenario", "boot_mode", "enter_field", "debug", "debug_status", "debug_tick", "reset", "schedule", "record", "replay", "card_list", "card_present"],
                }))
            }
            "memory_hash" => Ok(json!(format!("{:016x}", self.memory_hash()?))),
            "card_list" => {
                // What a memory card holds (`card_file::list`), read only:
                // changing it is the hosts' explicit file operations'.
                let slot = arg_u64("slot").ok_or("card_list needs slot (0 or 1)")?;
                if slot > 1 {
                    return Err("card_list: memory cards are 0 and 1".into());
                }
                let services = self.runtime.services();
                Ok(serde_json::to_value(services.card.status(slot as usize, services.clock.now)).expect("card status"))
            }
            "record" => {
                // Auto-advance's dwell counts from the recording's start, as
                // a replay's does.
                self.restart_auto_advance();
                let vblank = self.vblank();
                let changes = self.runtime.services().pads.0.iter().enumerate().map(|(port,pad)| (vblank,port as u32,pad.buttons)).collect();
                self.recording = Some(InputLog { start_vblank: vblank, changes, end_vblank: vblank, restores: Vec::new(),
                    controls: vec![(vblank,json!({"cmd":"fast_forward_configure","rate":self.fast_forward_rate})),
                        (vblank,json!({"cmd":"fast_forward","enabled":self.fast_forward})),
                        (vblank,json!({"cmd":"fast_forward_pace","pace":self.fast_forward_pace}))] });
                // Recording may begin after an interactive host attached its
                // music clock. Save that attachment before the first batch;
                // game snapshots deliberately omit the host's clock source.
                if self.runtime.services().clock.external_audio {
                    self.record_control(json!({"cmd":"music_advance","cycles":0}));
                }
                if self.runtime.services().focus_pause.active() {
                    let requested = self.runtime.services().focus_pause.requested();
                    self.record_control(json!({"cmd":"focus_pause","requested":requested}));
                }
                let view = self.runtime.services().actor_view;
                if view != 0 {
                    self.record_control(json!({"cmd":"actor_view","view":view}));
                }
                if self.runtime.services().attack_page {
                    self.record_control(json!({"cmd":"attack_page","kept":true}));
                }
                let off = self.runtime.services().random_battles_off.clone();
                if !off.is_empty() {
                    self.record_control(json!({"cmd":"random_battles","off":off}));
                }
                if self.runtime.services().half_encounters {
                    self.record_control(json!({"cmd":"half_encounters","enabled":true}));
                }
                for slot in 0..2 {
                    if !self.runtime.services().card.present(slot) {
                        self.record_control(json!({"cmd":"card_present","slot":slot,"present":false}));
                    }
                }
                let glyphs = self.runtime.services().text_speed;
                if glyphs != 0 {
                    self.record_control(json!({"cmd":"text_speed","glyphs":glyphs}));
                }
                if self.runtime.services().movie_skip {
                    self.record_control(json!({"cmd":"movie_skip","enabled":true}));
                }
                if self.auto_advance != 0 {
                    self.record_control(json!({"cmd":"auto_advance","dwell":self.auto_advance}));
                }
                if let Some(mode) = self.runtime.services().sound_mode {
                    let taken = !self.runtime.services().sound_mode_changed;
                    self.record_control(json!({"cmd":"sound_mode","mode":mode,"taken":taken}));
                }
                Ok(self.status())
            }
            "stop_recording" => {
                let mut log = self.recording.take().ok_or("not recording")?;
                log.end_vblank = self.vblank();
                Ok(serde_json::to_value(log).expect("input log"))
            }
            "replay" => {
                // Schedule a recorded input log (from stop_recording) relative to now.
                let changes = command.get("changes").and_then(Value::as_array).ok_or("replay needs changes")?;
                let start = command.get("start_vblank").and_then(Value::as_u64).unwrap_or(0);
                let now = self.vblank();
                let mut scheduled = Vec::new();
                let mut vblanks = (Vec::new(), Vec::new());
                for change in changes {
                    let c = change.as_array().ok_or("changes are [vblank, port, buttons]")?;
                    let at = c.first().and_then(Value::as_u64).ok_or("vblank")?;
                    let port = c.get(1).and_then(Value::as_u64).ok_or("port")? as u32;
                    let buttons = c.get(2).and_then(Value::as_u64).ok_or("buttons")? as u16;
                    if port > 1 { return Err("port is 0 or 1".into()); }
                    scheduled.push((now + at.saturating_sub(start), port, buttons));
                    vblanks.0.push(at);
                }
                let mut controls = Vec::new();
                if let Some(entries) = command.get("controls") {
                    for entry in entries.as_array().ok_or("controls are [vblank, command]")? {
                        let pair = entry.as_array().filter(|p| p.len() == 2).ok_or("controls are [vblank, command]")?;
                        let at = pair[0].as_u64().ok_or("control vblank")?;
                        vblanks.1.push(at);
                        let control = &pair[1];
                        match control.get("cmd").and_then(Value::as_str) {
                            Some("fast_forward") if control.get("enabled").is_some_and(Value::is_boolean) => {},
                            Some("focus_pause") if control.get("requested").is_some_and(Value::is_boolean) => {},
                            Some("fast_forward_configure") => {
                                let rate: FastForwardRate = serde_json::from_value(control.get("rate").ok_or("control rate")?.clone()).map_err(|e| e.to_string())?;
                                rate.validate()?;
                            },
                            Some("music_advance") if control.get("cycles").and_then(Value::as_u64).is_some_and(|c| c <= crate::clock::CPU_HZ/10) => {},
                            Some("fast_forward_pace") if control.get("pace").is_some_and(|p| p.is_null() || p.as_u64().is_some_and(|p| (1..=20).contains(&p))) => {},
                            Some("actor_view") if control.get("view").and_then(Value::as_u64).is_some_and(|v| v <= u64::from(u32::MAX)) => {},
                            Some("attack_page") if control.get("kept").is_some_and(Value::is_boolean) => {},
                            Some("random_battles") if random_battle_maps(control).is_some() => {},
                            Some("half_encounters") if control.get("enabled").is_some_and(Value::is_boolean) => {},
                            Some("card_present") if card_presence(control).is_some() => {},
                            Some("text_speed") if control.get("glyphs").and_then(Value::as_u64).is_some_and(|g| g <= 255) => {},
                            Some("movie_skip") if control.get("enabled").is_some_and(Value::is_boolean) => {},
                            Some("auto_advance") if control.get("dwell").and_then(Value::as_u64).is_some_and(|d| d <= u64::from(MAX_AUTO_ADVANCE_DWELL)) => {},
                            Some("sound_mode") if control.get("mode").and_then(Value::as_u64).is_some_and(|m| m <= 2)
                                && control.get("taken").is_none_or(Value::is_boolean) => {},
                            Some("assist") if control.get("intent").is_some() => {},
                            Some("assist_stop" | "reset") => {},
                            Some("debug") => {
                                let request: crate::debug::Request = serde_json::from_value(control.get("request").cloned().ok_or("debug needs request")?)
                                    .map_err(|e| e.to_string())?;
                                request.validate()?;
                            },
                            Some("shop_quantity") if control.get("op").is_some_and(Value::is_string) => {},
                            _ => return Err("recorded controls contain only explicit reset, fast-forward, focus-pause, actor-view, attack-page, sound-mode, random-battles, \
                                half-encounters, card-presence, text-speed, movie-skip, auto-advance, assist, debug and music-clock inputs".into()),
                        }
                        controls.push((now + at.saturating_sub(start),control.clone()));
                    }
                }
                // The game's vblank count is restored with its state: a log
                // whose vblanks go back spans a restore, and the entries after
                // it belong to another timeline, never to be interleaved.
                let end = command.get("end_vblank").and_then(Value::as_u64);
                for (list, list_vblanks) in [("changes", &vblanks.0), ("controls", &vblanks.1)] {
                    if let Some((index, at, previous)) = goes_back(start, list_vblanks.iter().copied().chain(end)) {
                        let entry = if index < list_vblanks.len() { format!("{list} entry {index}") } else { format!("end after its {list}") };
                        return Err(format!("the log goes back in time at its {entry}: vblank {at} after vblank {previous}. \
                            It spans a restore (a state load or a rewind step), which a replay cannot reproduce"));
                    }
                }
                if let Some(restores) = command.get("restores").and_then(Value::as_array).filter(|r| !r.is_empty()) {
                    let first: Restore = serde_json::from_value(restores[0].clone()).map_err(|e| format!("restores: {e}"))?;
                    let kind = match first.kind { RestoreKind::StateLoad => "a state load", RestoreKind::RewindStep => "a rewind step" };
                    return Err(format!("the log spans {} restore(s), the first {kind} at vblank {} to vblank {}, \
                        which a replay cannot reproduce", restores.len(), first.from_vblank, first.to_vblank));
                }
                scheduled.sort_by_key(|(at,_,_)| *at);
                controls.sort_by_key(|(at,_)| *at);
                self.scheduled = scheduled;
                self.runtime.services().focus_pause = Default::default();
                self.runtime.services().actor_view = 0;
                self.runtime.services().attack_page = false;
                self.runtime.services().random_battles_off.clear();
                self.runtime.services().half_encounters = false;
                self.runtime.services().card.set_present(0, true);
                self.runtime.services().card.set_present(1, true);
                self.runtime.services().text_speed = 0;
                self.runtime.services().movie_skip = false;
                self.auto_advance = 0;
                self.restart_auto_advance();
                self.runtime.services().sound_mode = None;
                self.runtime.services().sound_mode_changed = false;
                self.replay_end = command.get("end_vblank").and_then(Value::as_u64).map(|end| now + end.saturating_sub(start));
                self.replay_controls = controls;
                self.apply_replay_controls().map_err(|e| e.to_string())?;
                Ok(self.status())
            }
            "boot_mode" => {
                // Leave boot into `mode` instead of the opening movie: the
                // value main's mode_select_next_mode(6) chose is replaced when
                // main's mode_dispatch restarts into the dispatcher, before the
                // dispatcher reads it (as tools/reference/scenario.py's patch of
                // that call's argument does in the original program).
                if self.runtime.services().dispatches != 0 {
                    return Err("boot_mode is only valid before the first dispatch".into());
                }
                let mode = arg_u64("mode").ok_or("boot_mode needs mode")? as u32;
                let address = {
                    let schema = self.schema.as_ref().ok_or("no schema loaded")?;
                    schema.locate("mode_next_mode", self.runtime.memory()).map_err(|e| e.to_string())?.address
                };
                self.runtime.write_before_next_dispatch(address, mode);
                Ok(self.status())
            }
            "schedule" => {
                // Commands to run before given frames (counted from boot by
                // `frames` in the status): a recorded session replays on any
                // host that speaks these commands.
                let list = command.get("commands").and_then(Value::as_array).ok_or("schedule needs commands")?;
                let mut commands = Vec::new();
                for item in list {
                    let pair = item.as_array().filter(|p| p.len() == 2).ok_or("commands are [frame, command]")?;
                    let cmd = pair[1].get("cmd").and_then(Value::as_str).ok_or("scheduled command needs cmd")?;
                    if !matches!(cmd, "pad" | "press" | "hold" | "act" | "assist" | "assist_stop" | "shop_quantity" | "connect_pad" | "enter_field" | "debug" | "reset" | "write" | "write_memory" | "boot_mode" | "scenario" | "fast_forward" | "fast_forward_configure" | "fast_forward_pace" | "actor_view" | "attack_page" | "sound_mode" | "random_battles"
                        | "half_encounters" | "card_present" | "text_speed" | "movie_skip" | "auto_advance" | "music_advance") {
                        return Err(format!("{cmd} cannot be scheduled; session logs contain only game input, setup and pacing"));
                    }
                    let at = pair[0].as_u64().ok_or("a command's frame is a number")?;
                    if at < self.runtime.frames() {
                        return Err(format!("frame {at} has passed"));
                    }
                    commands.push((at, pair[1].clone()));
                }
                self.script.extend(commands);
                self.script.sort_by_key(|(at, _)| *at);
                Ok(json!({ "scheduled": self.script.len() }))
            }
            "enter_field" => {
                // Go anywhere in the field the way the original's development
                // kernel menu does (tools/reference/scenario.py used the same
                // recipe in the emulator): after boot, enter mode 0, put the
                // map in the game data, select mode 1 and let the kernel menu
                // finish into the original dispatcher. `entry` is the
                // entrance (`game_data.entry[2]`, which the field reads as
                // its v0002, as a door's map change leaves it; `entry[0]` is
                // the heading).
                // A field already running leaves through its own end, as its
                // scripts' "end field mode" event asks for it (encounters held
                // off, then the field loop frees its party sprite blocks, heap
                // and sound bank and starts mode 0): a restart in the middle
                // of it left them all to the next field, which loaded over
                // them (field 30 after field 12's dialogue made its party
                // sprite from garbage and trapped). The field's menu runs a
                // loop of its own, so the field leaves only once it closes.
                let map = arg_u64("map").ok_or("enter_field needs map")?;
                let budget = arg_u64("max_frames").unwrap_or(6000);
                if self.runtime.services().mode == Some(1) && self.inspect("mode_kernel_menu_running", 0)? == 0 {
                    if self.inspect("field_menu_request", 0)? != 0xFF {
                        return Err("enter_field: the field's menu is open; close it first (the field leaves only from its own loop)".into());
                    }
                    self.write_path("field_work.encounter_inhibition", &json!(-1))?;
                    self.write_path("field_exit_game_mode", &json!(0))?;
                    self.write_path("field_mode_exit_not_requested", &json!(0))?;
                } else {
                    self.run_until_value("cd_movie_request_kind", 1, budget)?;
                    self.write_path("mode_next_mode", &json!(0))?;
                    self.runtime.restart(RESTART_DISPATCH, 0);
                }
                self.run_until_value("mode_kernel_menu_running", 1, budget)?;
                self.write_path("game_data.map", &json!(map))?;
                if let Some(entry) = command.get("entry") {
                    self.write_path("game_data.entry[2]", entry)?;
                }
                self.write_path("mode_next_mode", &json!(1))?;
                self.write_path("mode_kernel_menu_running", &json!(0))?;
                self.write_path("mode_field_entered_once", &json!(0))?;
                self.run_until_value("mode_field_entered_once", 1, budget)?;
                Ok(json!({ "map": self.inspect("mode_field_map_id", 0)?, "status": self.status() }))
            }
            "scenario" => {
                // Typed go-anywhere setup: write values through the schema, then
                // enter a mode through the original dispatcher on an empty stack.
                for write in command.get("writes").and_then(Value::as_array).into_iter().flatten() {
                    let path = write.get("path").and_then(Value::as_str).ok_or("writes need path")?;
                    self.write_path(path, write.get("value").ok_or("writes need value")?)?;
                }
                if let Some(mode) = arg_u64("mode") {
                    self.write_path("mode_next_mode", &json!(mode))?;
                    self.runtime.restart(RESTART_DISPATCH, 0);
                    self.stopped = None;
                }
                Ok(self.status())
            }
            other => Err(format!("unknown command {other}")),
        }
    }

    pub fn snapshot_bytes(&mut self) -> Result<Vec<u8>, String> {
        if self.debug.busy() { return Err("Wait for the debug operation to finish before saving a state.".into()); }
        self.runtime.snapshot().map_err(|e| e.to_string())
    }

    fn game_paused(&mut self) -> bool {
        self.runtime.game_paused()
    }

    fn checkpoint(&mut self) {
        if self.debug.busy() { return; }
        // A held rewind's frames go back in time: no history of their own.
        if self.rewind.budget == 0 || self.rewinding { return; }
        let paused = self.game_paused();
        if paused || self.rewind_game_paused {
            self.reset_rewind_timer();
            self.rewind_game_paused = paused;
            if paused { return; }
        }
        let frame = self.runtime.frames();
        let now = (self.rewind_clock)();
        if self.rewind.due(now) {
            match self.runtime.snapshot_content() {
                Ok(data) => self.rewind.push(frame,data,now),
                Err(error) => self.rewind.error = Some(error.to_string()),
            }
        }
    }

    /// One older checkpoint for a host's held rewind, taken off the queue
    /// and played on from until the display shows a frame the game drew
    /// since, so the picture and the sound are the game's of that moment
    /// ([`crate::rewind::REWIND_PLAY_BLANKS`]), never as far as where the
    /// game was. Reaching the beginning leaves the game there until the
    /// player releases the input.
    pub fn rewind_step(&mut self) -> Result<bool, String> {
        if let Some(error) = &self.rewind.error { return Err(error.clone()); }
        if self.rewind.budget == 0 { return Err("Enable rewind history in Settings first.".into()); }
        let was = self.runtime.frames();
        let Some((at, data)) = self.rewind.pop_before(was) else { return Ok(false) };
        self.restore_rewind_state(&data)?;
        self.rewinding = true;
        let played = self.play_rewound(crate::rewind::REWIND_PLAY_BLANKS.min(was.saturating_sub(at + 1)));
        self.rewinding = false;
        played?;
        Ok(true)
    }

    /// Runs at most `limit` frames from a rewound state, until the display
    /// has changed buffers three times: the first shows a buffer drawn
    /// before the state, the second one drawn from what the game had built
    /// before it (whose projections the port recorded then, not now), the
    /// third one built and drawn after it. Under fast-forward the music, on
    /// the host's clock, plays those frames too.
    fn play_rewound(&mut self, limit: u64) -> Result<(), String> {
        let shown = |session: &mut Self| {
            let display = session.runtime.services().gpu.device().display();
            (display.x, display.y)
        };
        let mut last = shown(self);
        let (mut blanks, mut flips) = (0, 0);
        while blanks < limit && flips < 3 {
            // A disc read the browser still fetches: the frame goes on later.
            if !self.frame().map_err(|trap| trap.to_string())? { break; }
            if self.live_music() { self.advance_music(crate::clock::VBLANK_CYCLES)?; }
            blanks += 1;
            let now = shown(self);
            flips += u32::from(now != last);
            last = now;
        }
        Ok(())
    }

    /// Rewind game progress, keeping the app's current controls and commands.
    fn restore_game_state(&mut self, data: &[u8]) -> Result<(), String> {
        let from = self.vblank();
        self.runtime.restore(data).map_err(|e| e.to_string())?;
        self.note_restore(RestoreKind::StateLoad, from);
        self.game_state_restored();
        Ok(())
    }

    fn restore_rewind_state(&mut self, data: &[u8]) -> Result<(), String> {
        let from = self.vblank();
        self.runtime.restore_content(data).map_err(|e| e.to_string())?;
        self.note_restore(RestoreKind::RewindStep, from);
        self.game_state_restored();
        Ok(())
    }

    /// A recording goes on across a restore and reports it (`restores`).
    fn note_restore(&mut self, kind: RestoreKind, from_vblank: u64) {
        let to_vblank = self.vblank();
        if let Some(log) = &mut self.recording {
            log.restores.push(Restore { kind, from_vblank, to_vblank });
        }
    }

    pub(crate) fn reset_game(&mut self) {
        self.runtime.reset();
        self.quantity = None;
        self.set_pad(0, 0);
        self.set_pad(1, 0);
        self.rewind.clear();
        self.rewind.error = None;
        self.rewind_game_paused = false;
        self.game_state_restored();
    }

    fn game_state_restored(&mut self) {
        self.debug = Default::default();
        self.stopped = None;
        // Assisted movement belonged to the timeline left behind.
        if self.assist.take().is_some() {
            self.assist_outcome = Some(crate::assist::Outcome::Interrupted);
        }
        // Auto-advance's dwell counts from the restored moment.
        self.restart_auto_advance();
        self.timeline_revision += 1;
        self.reset_rewind_timer();
        // The restored game takes the host's sound mode again.
        let services = self.runtime.services();
        services.sound_mode_changed = services.sound_mode.is_some();
        // The Binaural presentation forgot its sound with the state; it sees
        // the restored scene before the restored voices are identified.
        self.runtime.update_binaural();
    }

    /// Restore game progress, keeping current app controls and settings.
    pub fn restore_bytes(&mut self, data: &[u8]) -> Result<(), String> {
        self.restore_game_state(data)?;
        self.rewind.clear();
        self.checkpoint();
        Ok(())
    }

    /// Run frames until the global at `path` equals `value` (bounded).
    fn run_until_value(&mut self, path: &str, value: i64, max_frames: u64) -> Result<(), String> {
        for _ in 0..max_frames {
            if self.inspect(path, 0)? == json!(value) {
                return Ok(());
            }
            if !self.frame().map_err(|t| t.to_string())? {
                return Err("a disc read is still being fetched; retry".into());
            }
        }
        Err(format!("{path} did not become {value} within {max_frames} frames"))
    }

    /// What the game shows, for presentation tuning (scene metadata):
    /// "field:<map>", "battle", "worldmap", "arena", "menu", "movie",
    /// "kernel" or "boot". Read-only.
    pub fn scene(&mut self) -> String {
        match self.runtime.services().mode {
            Some(1) => match self.inspect("mode_field_map_id", 0) {
                Ok(map) => format!("field:{map}"),
                Err(_) => "field".into(),
            },
            Some(0) => "kernel".into(),
            Some(2) => "battle".into(),
            Some(3) => "worldmap".into(),
            Some(4) => "arena".into(),
            Some(5) => "menu".into(),
            Some(6) => "movie".into(),
            _ => "boot".into(),
        }
    }

    /// FNV-1a over the PS1 RAM, the scratchpad and the I/O pages: the
    /// authoritative game state (comparable across hosts and runs).
    pub fn memory_hash(&mut self) -> Result<u64, String> {
        let mut h: u64 = 0xCBF2_9CE4_8422_2325;
        for (start, len) in [(0x8000_0000u32, 0x20_0000u32), (0x1F80_0000, 0x400)] {
            let mut data = vec![0; len as usize];
            self.runtime.memory().read(start, &mut data).map_err(|e| e.to_string())?;
            for b in data {
                h = (h ^ b as u64).wrapping_mul(0x100_0000_01B3);
            }
        }
        Ok(h)
    }
}

fn hex(data: &[u8]) -> String {
    data.iter().map(|b| format!("{b:02x}")).collect()
}

/// A tap capture as `audio_capture` reports it: its samples, the output
/// samples before its first (`lead`), and its blocks in runs (first and
/// last block) in which each source had the same voices sounding.
fn tap_summary(journal: &xem_binaural::Journal) -> Value {
    let blocks = journal.block_routes();
    let voices = |mask: u32| (0..xem_spu::VOICES as u32).filter(|v| mask & (1 << v) != 0).collect::<Vec<u32>>();
    let mut runs = Vec::new();
    let mut first = 0;
    while first < blocks.len() {
        let end = (first..blocks.len()).find(|&k| blocks[k] != blocks[first]).unwrap_or(blocks.len());
        let sources: serde_json::Map<String, Value> = blocks[first].iter().map(|(id, mask)| (id.to_string(), json!(voices(*mask)))).collect();
        runs.push(json!({"blocks": [first, end - 1], "sources": sources}));
        first = end;
    }
    json!({
        "frames": journal.frames().len(),
        "lead": journal.lead(),
        "block": xem_binaural::BLOCK,
        "truncated": journal.truncated(),
        "routes": runs,
    })
}

fn unhex(text: &str) -> Result<Vec<u8>, String> {
    if text.len() % 2 != 0 {
        return Err("odd hex length".into());
    }
    (0..text.len()).step_by(2).map(|i| u8::from_str_radix(&text[i..i + 2], 16).map_err(|e| e.to_string())).collect()
}
