//! The game as the SDL3 hosts play it: a booted session paced at the
//! console's 60 frames a second by the wall clock (a stalled or paused host
//! drops time, never catches up) with the SPU's output on an SDL3 audio
//! stream.

use sdl3_sys::everything::*;
use std::time::{Duration, Instant};
use xem_core::control::Session;
use xem_game::NativeModule;

const FRAME: Duration = Duration::from_nanos(1_000_000_000 / 60);
const BATCH_BUDGET: Duration = Duration::from_millis(12);
/// Frames one update may catch up: a host presenting as slowly as 15 Hz (a
/// headset runtime's frame loop, a slow display) still runs every game
/// frame; a longer stall drops time instead of catching up.
const MAX_CATCH_UP: u32 = 4;
/// Fifty milliseconds of stereo S16 PCM before starting the device absorbs
/// the jitter between a busy game batch and the audio callback.
const AUDIO_PREROLL_BYTES: i32 = 44_100 / 20 * 2 * 2;

pub struct Game {
    pub session: Session<NativeModule>,
    audio: *mut SDL_AudioStream,
    audio_started: bool,
    paced: Instant,
    music_paced: Instant,
    samples: Vec<i16>,
    /// A session log drives the pads (`schedule`): the player's buttons are
    /// not applied.
    scripted: bool,
    /// Frames the game runs at most (a scripted session's end).
    frame_limit: Option<u64>,
    timeline_revision: u64,
    rewind_held: [bool;2],
    rewind_next: Instant,
    hud_forward: Option<Instant>,
    hud_rewind: Option<Instant>,
    applied_time_settings: Option<(xem_settings::FastForwardRate, bool, u32, u32)>,
    /// Fast-forward's pace: the rate the batches reach.
    pacer: xem_core::time::FastForwardPacer,
    last_update: Instant,
    /// The view the field's characters keep moving in (see
    /// `Session::set_actor_view`), set by the presentation.
    actor_view: u32,
    /// The battle's attack page stays shown through a sequence the player
    /// began by pointing at it (`Session::set_attack_page`).
    attack_page: bool,
    /// The listener's view while the presentation looks from elsewhere
    /// than the game's camera (`Session::set_listener_view`), for a
    /// presentation an update starts.
    listener_view: Option<xem_core::binaural::ListenerView>,
    /// The presentation shows first-person inspection
    /// (`Session::set_inspecting`).
    inspecting: bool,
}

impl Game {
    /// Plays `session` (booted with `audio_output`) from now; the audio
    /// stream opens on the default playback device, or the game runs silent.
    pub fn new(session: Session<NativeModule>) -> Self {
        let spec = SDL_AudioSpec {
            format: SDL_AUDIO_S16LE,
            channels: 2,
            freq: 44100,
        };
        // SAFETY: SDL is initialised with audio; a null callback queues data.
        let audio = unsafe {
            SDL_OpenAudioDeviceStream(
                SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                &spec,
                None,
                std::ptr::null_mut(),
            )
        };
        let mut game = Game {
            session,
            audio,
            audio_started: false,
            paced: Instant::now(),
            music_paced: Instant::now(),
            samples: Vec::new(),
            scripted: false,
            frame_limit: None,
            timeline_revision: 0,
            rewind_held: [false;2],
            rewind_next: Instant::now(),
            hud_forward: None,
            hud_rewind: None,
            applied_time_settings: None,
            pacer: Default::default(),
            last_update: Instant::now(),
            actor_view: 0,
            attack_page: false,
            listener_view: None,
            inspecting: false,
        };
        game.sync_time_settings(&xem_settings::Settings::default()).expect("valid interactive defaults");
        game
    }

    /// Plays a session log (`{"commands": [[frame, command], ...]}`, as
    /// tools/native_scenario.py --log writes it): its commands run before
    /// their frames and its pad states replace the player's.
    pub fn schedule(&mut self, log: &serde_json::Value) -> Result<(), String> {
        let reply = self.session.execute(&serde_json::json!({ "cmd": "schedule", "commands": log["commands"] }));
        if let Some(error) = reply.get("error") {
            return Err(format!("schedule: {error}"));
        }
        self.scripted = true;
        // Scripted playback has the same opt-in history policy as headless.
        self.configure_rewind(false,self.session.rewind.interval_seconds,self.session.rewind.history_seconds)?;
        Ok(())
    }

    /// Run no frame beyond `limit` (frames since boot).
    pub fn set_frame_limit(&mut self, limit: Option<u64>) {
        self.frame_limit = limit;
    }

    /// Frames the game ran since boot.
    pub fn frames_run(&self) -> u64 {
        self.session.runtime.frames()
    }

    /// The hash of RAM and the scratchpad (`memory_hash`).
    pub fn memory_hash(&mut self) -> String {
        format!("{:016x}", self.session.memory_hash().unwrap_or(0))
    }

    /// A shared control or native file command; an error is its plain text.
    pub fn command(&mut self, command: &serde_json::Value) -> Result<serde_json::Value,String> {
        let reply = xem_game::storage::command(&mut self.session,command);
        if let Some(error) = reply.get("error") {
            return Err(error.as_str().map_or_else(|| error.to_string(), str::to_owned));
        }
        Ok(reply)
    }
    pub fn snapshot_bytes(&mut self) -> Result<Vec<u8>,String> { self.session.snapshot_bytes() }
    pub fn debug_status(&self) -> (bool, &str) { (self.session.debug.busy(), &self.session.debug.message) }
    pub fn restore_bytes(&mut self, bytes: &[u8]) -> Result<(),String> { self.session.restore_bytes(bytes) }
    pub fn card_image(&mut self, slot: usize) -> Option<Vec<u8>> { self.session.runtime.services().card.image(slot).map(<[u8]>::to_vec) }
    /// Import a card or save file the player picked into card `slot`, by its
    /// contents (`file_name` names a raw single save), between frames;
    /// returns what was found ("Card 1: DexDrive card, 3 saves").
    pub fn import_card(&mut self, slot: usize, bytes: &[u8], file_name: &str) -> Result<String,String> {
        let services = self.session.runtime.services();
        let now = services.clock.now;
        services.card.import_file(slot,bytes,file_name,now)
    }
    pub fn retry_storage(&mut self) -> Result<(),String> { self.session.runtime.services().card.retry_writes() }
    /// Give the running game the Kanji font of a BIOS image the player
    /// imported (`Runtime::install_kanji_font`), between frames: the game
    /// draws its next save title from it.
    pub fn install_bios(&mut self, rom: &[u8]) -> Result<(),String> {
        self.session.runtime.install_kanji_font(Some(rom)).map(|_| ())
    }
    pub fn configure_fast_forward(&mut self, rate: xem_settings::FastForwardRate) -> Result<(),String> {
        self.session.configure_fast_forward(rate)
    }
    /// Apply changed preferences once. Agent commands keep authority until
    /// the player changes the corresponding preference again.
    pub fn sync_time_settings(&mut self, settings: &xem_settings::Settings) -> Result<(), String> {
        let next = (settings.fast_forward_rate,settings.rewind_enabled,settings.rewind_interval_seconds,settings.rewind_history_seconds);
        if self.applied_time_settings.is_none_or(|old| old.0 != next.0) { self.configure_fast_forward(next.0)?; }
        if self.applied_time_settings.is_none_or(|old| (old.1,old.2,old.3) != (next.1,next.2,next.3)) {
            self.configure_rewind(next.1,next.2,next.3)?;
        }
        self.applied_time_settings = Some(next);
        self.session.ledge_brake = settings.assisted.ledge_brake;
        Ok(())
    }
    pub fn configure_rewind(&mut self, enabled: bool, interval_seconds: u32, history_seconds: u32) -> Result<(),String> {
        let budget = if enabled { xem_core::rewind::MAX_REWIND_BYTES } else { 0 };
        if self.session.rewind.budget != budget || self.session.rewind.interval_seconds != interval_seconds || self.session.rewind.history_seconds != history_seconds {
            self.session.configure_rewind(budget,interval_seconds,history_seconds)?;
            if !enabled { self.set_rewind_held(false,false)?; self.set_rewind_held(false,true)?; }
        }
        Ok(())
    }
    pub fn set_rewind_held(&mut self, held: bool, from_panel: bool) -> Result<(),String> {
        if held && self.session.rewind.budget == 0 { return Err("Enable rewind history in Settings first.".into()); }
        if held && let Some(error) = &self.session.rewind.error { return Err(error.clone()); }
        let was_held = self.rewind_held.iter().any(|held| *held);
        self.rewind_held[usize::from(from_panel)] = held;
        let held = self.rewind_held.iter().any(|held| *held);
        if was_held != held {
            self.rewind_next = Instant::now();
            if held {
                self.pause();
            } else {
                // Play refills the pre-roll after the rewind's moments of sound.
                self.stop_audio();
                self.resume();
                self.timeline_revision = self.session.timeline_revision;
            }
        }
        Ok(())
    }
    /// Whether a rewind is held (a binding's or the panel's Hold to rewind).
    pub fn rewind_held(&self) -> bool {
        self.rewind_held.iter().any(|held| *held)
    }
    /// Recent completed work, rather than the time-control preference alone.
    pub fn time_activity(&self) -> (bool, bool) {
        let held = self.rewind_held.iter().any(|held| *held);
        let recent = |at: Option<Instant>, ms| at.is_some_and(|at| at.elapsed() < Duration::from_millis(ms));
        (self.session.fast_forward && !held && self.session.stopped.is_none() && recent(self.hud_forward, 150),
         held && recent(self.hud_rewind, xem_core::rewind::REWIND_STEP_MS + 100))
    }
    pub fn time_status(&mut self) -> (bool,String) {
        let rewind = &self.session.rewind;
        let time = format!("{:.0} of {} seconds available · {:.1} MiB used{}", rewind.available_seconds(), rewind.history_seconds, rewind.bytes as f64/1048576.0,
            rewind.error.as_ref().map(|e| format!(" — {e}")).unwrap_or_default());
        (self.session.fast_forward,time)
    }
    /// The storage line: card writes pending, the cards' storage errors and
    /// why the BIOS image the player imported is not used.
    pub fn storage_status(&mut self) -> String {
        let services = self.session.runtime.services();
        let problems: Vec<&str> = services.card.errors.iter().chain(&services.kanji_font_notice).map(String::as_str).collect();
        format!("{} card writes pending. {}",services.card.pending_writes(),problems.join("; "))
    }

    /// Give the drive's changer the game's other disc `number`.
    pub fn add_disc(&mut self, number: u8, disc: Box<dyn xem_disc::SectorSource>) {
        self.session.runtime.services().add_disc(number, disc);
    }

    /// The disc the game waits for that the drive's changer has not got.
    pub fn wanted_disc(&mut self) -> Option<u8> {
        self.session.runtime.services().cd.wanted()
    }

    /// Whether the SPU's output reaches an audio device.
    pub fn has_audio(&self) -> bool {
        !self.audio.is_null()
    }

    /// The game's frame count: vertical blanks since boot.
    pub fn vblanks(&mut self) -> u64 {
        self.session.runtime.services().clock.vblanks
    }

    /// What the game's state says of the player's buttons: whether the
    /// battle's attack inputs take them (the keyboard's combo layout
    /// applies), and in a field whether the player walks and Select shows
    /// the map's picture (first-person inspection's toggle and the buttons
    /// it holds back).
    pub fn pad_context(&mut self) -> crate::PadContext {
        crate::PadContext {
            attack: self.session.runtime.combo_input(),
            field: self.session.runtime.field_controls(),
            assignment: self.session.runtime.button_assignment(),
            ui: self.session.runtime.context(),
            paused: self.session.runtime.game_paused(),
        }
    }

    pub fn mouse_camera_active(&mut self, settings: &xem_settings::Settings) -> bool {
        settings.captures_mouse(self.session.runtime.context(), self.session.runtime.game_paused())
    }

    /// The field as the assisted controls read it (in a field).
    pub fn field(&mut self) -> Option<xem_core::field::Field> {
        if self.session.runtime.services().mode != Some(1) {
            return None;
        }
        xem_core::field::Field::read(self.session.runtime.memory())
    }

    /// Assisted movement as the frame log shows it: its phase, else how it
    /// last ended.
    pub fn assist_state(&self) -> String {
        match (&self.session.assist, self.session.assist_outcome) {
            (Some(assist), _) => format!("{:?}", assist.phase).to_lowercase(),
            (None, Some(outcome)) => format!("{outcome:?}").to_lowercase(),
            (None, None) => "-".into(),
        }
    }

    /// Where assisted walking is headed.
    pub fn destination(&self) -> Option<xem_core::field::V3> {
        match self.session.assist.as_ref()?.intent {
            xem_core::assist::Intent::Walk { to, .. } => Some(to),
            xem_core::assist::Intent::Interact { .. } => None,
        }
    }

    /// The game interface's entries a pointer selects (`xem_core::menu`).
    pub fn targets(&mut self) -> Option<xem_core::menu::Targets> {
        self.session.runtime.targets()
    }

    /// Runs a bounded batch of frames due by the wall clock with pad 1 holding
    /// `buttons` and the game in the sound mode of `settings` (Stereo under
    /// Binaural, which presents the sound), then queues their sound at its
    /// master volume. Returns the frames run.
    pub fn update(&mut self, buttons: u16, settings: &xem_settings::Settings, mut focused: impl FnMut() -> bool) -> Result<u32, String> {
        self.sync_time_settings(settings)?;
        self.session.set_binaural(settings.binaural);
        // A presentation just started takes the listener's view too.
        self.session.set_listener_view(self.listener_view);
        if !self.scripted {
            self.session.set_inspecting(self.inspecting);
        }
        if self.rewind_held.iter().any(|held| *held) {
            let now = Instant::now();
            if now >= self.rewind_next {
                let stepped = self.session.rewind_step()?;
                self.hud_rewind = stepped.then_some(now);
                // The frames played on from the state sound as they played.
                if stepped { self.queue_audio(settings, true); }
                self.rewind_next = now + Duration::from_millis(xem_core::rewind::REWIND_STEP_MS);
            }
            self.paced = now;
            self.music_paced = now;
            return Ok(0);
        }
        if self.session.stopped.is_some() { return Ok(0); }
        if self.timeline_revision != self.session.timeline_revision {
            self.timeline_revision = self.session.timeline_revision;
            self.paced = Instant::now();
            self.music_paced = self.paced;
            self.pause();
        }
        let mut due = 0;
        while self.paced + FRAME <= Instant::now() && due < MAX_CATCH_UP {
            self.paced += FRAME;
            due += 1;
        }
        if self.paced + FRAME <= Instant::now() {
            self.paced = Instant::now();
        }
        let music_now = Instant::now();
        let music_cycles = (music_now.duration_since(self.music_paced).min(Duration::from_millis(100)).as_nanos()
            * u128::from(xem_core::clock::CPU_HZ) / 1_000_000_000) as u64;
        self.music_paced = music_now;
        let live_music = if self.scripted {
            self.session.frame_factor().is_none() && self.session.live_music()
        } else {
            self.session.begin_live_music()?
        };
        let factor = self.session.frame_factor();
        let requested = factor.map(|factor| due * factor);
        let start = Instant::now();
        let mut batch = 0;
        while requested.is_none_or(|frames| batch < frames)
            && self.frame_limit.is_none_or(|limit| self.session.runtime.frames() < limit)
            && (batch == 0 || start.elapsed() < BATCH_BUDGET)
        {
            if !self.scripted {
                if self.session.runtime.services().pads.0[0].buttons != buttons { self.session.set_pad(0,buttons); }
            }
            if !self.scripted {
                self.session.set_window_focus(focused(),settings.pause_on_focus_loss);
                self.session.set_actor_view(self.actor_view)?;
                self.session.set_attack_page(self.attack_page);
                self.session.set_sound_mode(settings.game_sound_mode())?;
                self.session.set_random_battles(settings.random_battles_off())?;
                self.session.set_half_encounters(settings.half_encounters());
                self.session.set_text_speed(settings.text_speed.glyphs())?;
                self.session.set_movie_skip(settings.movie_skip);
                self.session.set_auto_advance(settings.auto_advance_dwell())?;
            }
            if !self.session.frame().map_err(|trap| trap.to_string())? { break; }
            batch += 1;
        }
        if batch > 0 {
            self.hud_forward = (self.session.fast_forward && !self.session.runtime.game_paused()).then(Instant::now);
        }
        if live_music { self.session.advance_live_music(music_cycles)?; }
        // Adapt fixed fast-forward to the rate the batches actually reach.
        // Music has its own elapsed-time clock and keeps playing throughout.
        let now = Instant::now();
        let seconds = now.duration_since(self.last_update).as_secs_f64();
        self.last_update = now;
        match (self.session.fast_forward, self.session.fast_forward_rate.factor(), factor) {
            (true, Some(rate), Some(pace)) if !self.scripted && !self.session.replaying() => {
                let behind = requested.is_some_and(|frames| batch < frames);
                if let Some(next) = self.pacer.note(seconds, batch, behind, pace, rate) {
                    self.session.pace_fast_forward(Some(next))?;
                }
            }
            _ => self.pacer.reset(),
        }
        self.queue_audio(settings, false);
        Ok(batch)
    }

    /// Queues the sound of the frames run since, at the master volume. The
    /// device starts once the pre-roll is queued, or at once (`at_once`: a
    /// rewind step's moment of sound, which never fills one).
    fn queue_audio(&mut self, settings: &xem_settings::Settings, at_once: bool) {
        let spu = &mut self.session.runtime.services().spu;
        self.samples.resize(spu.audio_queued(), 0);
        spu.take_audio(&mut self.samples);
        if self.audio.is_null() || self.samples.is_empty() {
            return;
        }
        let gain = f32::from(settings.master_volume.min(xem_settings::MAX_VOLUME))
            / f32::from(xem_settings::MAX_VOLUME);
        for sample in &mut self.samples {
            *sample = (f32::from(*sample) * gain) as i16;
        }
        // SAFETY: an open stream; SDL copies the bytes.
        unsafe {
            SDL_PutAudioStreamData(
                self.audio,
                self.samples.as_ptr().cast(),
                (self.samples.len() * 2) as i32,
            );
            if !self.audio_started && (at_once || SDL_GetAudioStreamQueued(self.audio) >= AUDIO_PREROLL_BYTES) {
                self.audio_started = SDL_ResumeAudioStreamDevice(self.audio);
            }
        }
    }

    /// The setting follows the sound mode the game's own Sound screen chose
    /// (the game follows the setting through `update`).
    pub fn follow_sound_mode(&mut self, settings: &std::cell::RefCell<xem_settings::SettingsService>) {
        if let Some(output) = self.session.take_sound_mode_choice().and_then(xem_settings::AudioOutput::from_game_mode) {
            let _ = settings.borrow_mut().apply(xem_settings::SettingChange::AudioOutput(output));
        }
    }

    /// Stops the game clock and the sound (the host went to the background):
    /// nothing runs until [`resume`](Self::resume), and queued sound is
    /// dropped.
    pub fn pause(&mut self) {
        self.hud_forward = None;
        self.hud_rewind = None;
        self.stop_audio();
    }

    /// Stops the audio device and drops the sound queued for it; the next
    /// sound refills the pre-roll first.
    fn stop_audio(&mut self) {
        self.audio_started = false;
        if !self.audio.is_null() {
            // SAFETY: an open stream.
            unsafe {
                SDL_PauseAudioStreamDevice(self.audio);
                SDL_ClearAudioStream(self.audio);
            }
        }
    }

    /// Continues from now: the paused span is not run.
    pub fn resume(&mut self) {
        self.paced = Instant::now();
        self.music_paced = self.paced;
        self.session.reset_rewind_timer();
        // The next updates refill the short pre-roll before playback resumes.
    }

    /// What the game drew since the last call (its GPU's draw trace, which
    /// this turns on; `restart` starts it again from the whole VRAM, and
    /// `reveals` says which of the faces the game culls to record), the
    /// display and the scene, for the renderer. Presentation only.
    pub fn take_frame(&mut self, restart: bool, reveals: Option<xem_render::xem_gpu::Reveals>) -> (xem_render::xem_gpu::Trace, xem_render::xem_gpu::DisplayInfo, String) {
        let scene = self.session.scene();
        let device = self.session.runtime.services().gpu.device_mut();
        if restart || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(reveals);
        (device.take_trace(), device.display(), scene)
    }

    /// The view the field's characters should keep moving in from the next
    /// frame (`GameView::actor_view`; 0 the original's).
    pub fn want_actor_view(&mut self, view: u32) {
        self.actor_view = view;
    }

    /// Whether the battle's attack page stays shown through the attack
    /// sequence from the next frame (`PlayerInput::attack_pointed`).
    pub fn want_attack_page(&mut self, kept: bool) {
        self.attack_page = kept;
    }

    /// The listener's view while the presentation looks from elsewhere than
    /// the game's camera (`GameView::listener_axes`: first-person
    /// inspection, a headset's diorama; its axes and the camera of the
    /// frame shown they are in), for the Binaural mode: taken at once,
    /// whether the game runs or not, as the head is (presentation only), and
    /// by a presentation the next update starts.
    pub fn set_listener_view(&mut self, view: Option<xem_core::binaural::ListenerView>) {
        self.listener_view = view;
        self.session.set_listener_view(view);
    }

    /// Whether the presentation shows first-person inspection from the next
    /// update (`View::first_person`): turning it on stops a route under way,
    /// and the session refuses new ones while it is on.
    pub fn want_inspection(&mut self, on: bool) {
        self.inspecting = on;
    }

    /// The headset's head in the presentation's reference space, for the
    /// Binaural mode (none: no head, out of a headset; a host that loses
    /// tracking keeps the last by not calling).
    pub fn set_head(&mut self, axes: Option<[[f32; 3]; 3]>) {
        self.session.set_head(axes);
    }

    /// Ends the draw trace (the Original renderer shows the display as it
    /// is): nothing is recorded for a renderer, nor faces the game culls.
    pub fn stop_trace(&mut self) {
        let device = self.session.runtime.services().gpu.device_mut();
        if device.trace_enabled() {
            device.set_trace(false);
        }
    }

    /// The display's mode and position.
    pub fn display_info(&mut self) -> xem_render::xem_gpu::DisplayInfo {
        self.session.runtime.services().gpu.device().display()
    }

    /// The display as RGBA8: (width, height, pixels).
    pub fn display(&mut self) -> (u32, u32, Vec<u8>) {
        self.session.runtime.services().gpu.display_rgba8()
    }
}

impl Drop for Game {
    fn drop(&mut self) {
        if !self.audio.is_null() {
            // SAFETY: opened by SDL_OpenAudioDeviceStream, destroyed once.
            unsafe { SDL_DestroyAudioStream(self.audio) };
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[cfg(not(debug_assertions))]
    use xem_settings::FastForwardRate;

    #[test]
    fn a_refused_state_load_reports_the_runtimes_message_as_it_is() {
        use serde_json::json;
        use xem_core::snapshot::{Writer, checked, seal};
        use xem_settings::{AppAction, AppRequest, MemoryStore, SettingsService};
        // The game module with nothing booted, and its state as another
        // build's: the identity record after the magic and the version.
        let mut game = Game::new(Session::new(xem_core::Runtime::new(NativeModule::new()), None, Duration::default));
        let module = game.session.runtime.identity.module.clone();
        let content = checked(&game.snapshot_bytes().unwrap()).unwrap().to_vec();
        let length = u32::from_le_bytes(content[12..16].try_into().unwrap()) as usize;
        let mut foreign = Writer(content[..12].to_vec());
        foreign.bytes(br#"{"module":"another build","disc":"","packages":[]}"#);
        foreign.0.extend_from_slice(&content[16 + length..]);
        let directory = std::env::temp_dir().join(format!("xem-host-refused-state-{}", std::process::id()));
        std::fs::create_dir_all(directory.join("saves")).unwrap();
        let path = directory.join("saves/state.xemstate");
        std::fs::write(&path, seal(foreign.0)).unwrap();
        let expected = format!(r#"incompatible state: the state's game module is "another build", this session's is "{module}""#);
        assert_eq!(game.command(&json!({"cmd":"state_load","path":path})), Err(expected.clone()));
        // The panel's Load state, whose result the HUD shows, has it so too.
        let mut operations = crate::operations::Operations::default();
        operations.directory = Some(directory.clone());
        let mut settings = SettingsService::with_defaults(MemoryStore::default());
        let loaded = operations.execute(AppRequest::Action(AppAction::LoadState), None, Some(&mut game), &mut settings);
        std::fs::remove_dir_all(&directory).unwrap();
        assert_eq!(loaded, Err(expected));
    }

    #[test]
    fn an_agents_app_requests_run_as_the_panels_and_the_bindings_do_but_open_no_picker() {
        use serde_json::json;
        use xem_settings::{MemoryStore, SettingsService};
        // `--control`'s `app {request}`, as the window and a headset carry
        // it out: through the panel's and the bindings' operations.
        let mut game = Game::new(Session::new(xem_core::Runtime::new(NativeModule::new()), None, Duration::default));
        let mut operations = crate::operations::Operations::default();
        let mut settings = SettingsService::with_defaults(MemoryStore::default());
        let mut app = |game: &mut Game, request: serde_json::Value| {
            crate::agent_request(&json!({"cmd": "app", "request": request})).and_then(|request| operations.execute(request, None, Some(game), &mut settings))
        };
        // A held rewind starts and ends as a held binding's (the host's
        // frames then step it, `Game::update`).
        assert_eq!(app(&mut game, json!({"Rewind": true})), Ok("Rewinding while held.".into()));
        assert!(game.rewind_held());
        assert_eq!(app(&mut game, json!({"Rewind": false})), Ok("Rewind released.".into()));
        assert!(!game.rewind_held());
        assert_eq!(app(&mut game, json!({"Action": "RetryStorage"})), Ok("Pending card writes completed.".into()));
        assert_eq!(app(&mut game, json!({"Action": "ToggleFastForward"})), Ok("Fast-forward on. Music keeps its normal tempo and pitch.".into()));
        assert!(game.session.fast_forward);
        // A file picker is the player's to answer; only a headset recentres.
        let refused = app(&mut game, json!({"Action": "ImportCard1"})).unwrap_err();
        assert!(refused.starts_with("ImportCard1 opens a file picker") && refused.contains("card_import"), "{refused}");
        assert_eq!(app(&mut game, json!({"Action": "Recenter"})), Err("Recentering places a headset's view; play in a headset to use it.".into()));
        assert!(app(&mut game, json!({"Action": "ToggleFirstPerson"})).unwrap_err().contains("first_person {on, yaw, pitch}"));
        assert!(app(&mut game, json!({"Action": "Fly"})).unwrap_err().starts_with("not an app operation"));
        assert!(crate::agent_request(&json!({"cmd": "app"})).unwrap_err().starts_with("app needs request"));
    }

    #[test]
    fn frames_give_the_session_the_settings_random_battle_maps_and_a_session_log_none() {
        use serde_json::json;
        use xem_settings::{RandomBattles, Settings};
        let session = || Session::new(xem_core::Runtime::new(NativeModule::new()), None, Duration::default);
        let off = |game: &mut Game| game.session.execute(&json!({"cmd":"status"}))["random_battles_off"].clone();
        let update = |game: &mut Game, settings: &Settings| {
            game.paced = Instant::now() - FRAME * 2;
            game.update(0, settings, || true).unwrap()
        };
        let mut game = Game::new(session());
        let mut settings = Settings::default();
        assert!(update(&mut game, &settings) > 0);
        assert_eq!(off(&mut game), json!(settings.random_battles_off()), "the default policy's maps");
        settings.random_battles = RandomBattles::Original;
        update(&mut game, &settings);
        assert_eq!(off(&mut game), json!([]), "the original's: none");
        let half = |game: &mut Game| game.session.execute(&json!({"cmd":"status"}))["half_encounters"].clone();
        assert_eq!(half(&mut game), json!(false), "the original's rate");
        settings.encounter_rate = xem_settings::EncounterRate::Half;
        update(&mut game, &settings);
        assert_eq!(half(&mut game), json!(true));
        // A session log plays the original whatever the settings say.
        let mut scripted = Game::new(session());
        scripted.schedule(&json!({"commands": []})).unwrap();
        assert!(update(&mut scripted, &Settings { encounter_rate: xem_settings::EncounterRate::Half, ..Settings::default() }) > 0);
        assert_eq!((off(&mut scripted), half(&mut scripted)), (json!([]), json!(false)));
    }

    #[test]
    fn frames_give_the_session_the_settings_text_speed_auto_advance_and_movie_skipping_and_a_session_log_the_original() {
        use serde_json::json;
        use xem_settings::{AutoAdvance, Settings, TextSpeed};
        let session = || Session::new(xem_core::Runtime::new(NativeModule::new()), None, Duration::default);
        let controls = |game: &mut Game| {
            let status = game.session.execute(&json!({"cmd":"status"}));
            (status["text_speed"].clone(), status["auto_advance"].clone(), status["movie_skip"].clone())
        };
        let update = |game: &mut Game, settings: &Settings| {
            game.paced = Instant::now() - FRAME * 2;
            game.update(0, settings, || true).unwrap()
        };
        let mut game = Game::new(session());
        assert!(update(&mut game, &Settings::default()) > 0);
        assert_eq!(controls(&mut game), (json!(0), json!(0), json!(false)), "the defaults are the original's");
        let settings = Settings { text_speed: TextSpeed::Fast, auto_advance: AutoAdvance { enabled: true, dwell: 90 }, movie_skip: true, ..Settings::default() };
        update(&mut game, &settings);
        assert_eq!(controls(&mut game), (json!(4), json!(90), json!(true)));
        let mut scripted = Game::new(session());
        scripted.schedule(&json!({"commands": []})).unwrap();
        assert!(update(&mut scripted, &settings) > 0);
        assert_eq!(controls(&mut scripted), (json!(0), json!(0), json!(false)), "a session log plays the original");
    }

    #[test]
    fn an_agents_gameplay_settings_reach_the_session_on_the_next_frame() {
        use serde_json::json;
        use xem_settings::{MemoryStore, SettingsService};
        // `xem-desktop --control`: the settings commands go to the
        // settings service, and its frames give the session the result.
        let mut settings = SettingsService::with_defaults(MemoryStore::default());
        let mut game = Game::new(Session::new(xem_core::Runtime::new(NativeModule::new()), None, Duration::default));
        let update = |game: &mut Game, settings: &SettingsService| {
            game.paced = Instant::now() - FRAME * 2;
            game.update(0, settings.settings(), || true).unwrap()
        };
        let gameplay = |game: &mut Game| {
            let status = game.session.execute(&json!({"cmd":"status"}));
            ["random_battles_off", "half_encounters", "text_speed", "auto_advance", "movie_skip"].map(|key| status[key].clone())
        };
        assert!(update(&mut game, &settings) > 0);
        let maps = json!(xem_core::encounters::platforming_maps());
        assert_eq!(gameplay(&mut game), [maps.clone(), json!(false), json!(0), json!(0), json!(false)], "a fresh host: the defaults");
        for change in [json!({"RandomBattles":"Original"}), json!({"EncounterRate":"Half"}), json!({"TextSpeed":"Instant"}),
            json!({"AutoAdvance":{"enabled":true,"dwell":90}}), json!({"MovieSkip":true})] {
            let ack = settings.command(&json!({"cmd":"settings_apply","change":change})).unwrap();
            assert!(ack.get("revision").is_some(), "{change} -> {ack}");
        }
        update(&mut game, &settings);
        assert_eq!(gameplay(&mut game), [json!([]), json!(true), json!(255), json!(90), json!(true)], "the next frame's status shows each");
        let refused = settings.command(&json!({"cmd":"settings_apply","change":{"AutoAdvance":{"enabled":true,"dwell":601}}})).unwrap();
        assert!(refused["error"].is_string(), "{refused}");
        settings.command(&json!({"cmd":"settings_apply","change":{"RandomBattles":"NotWhilePlatforming"}})).unwrap();
        update(&mut game, &settings);
        assert_eq!(gameplay(&mut game), [maps, json!(true), json!(255), json!(90), json!(true)], "the refused dwell changed nothing");
    }

    /// The user's disc 1 (local only: XEM_DISC1, else discs/ beside the
    /// repository), for the original game's tests (release builds only:
    /// an unoptimised game module runs far below the console's rate).
    #[cfg(not(debug_assertions))]
    fn disc() -> Option<std::path::PathBuf> {
        let path = std::env::var_os("XEM_DISC1").map(std::path::PathBuf::from)
            .unwrap_or_else(|| std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
        path.exists().then_some(path)
    }

    #[cfg(not(debug_assertions))]
    #[test]
    fn original_game_rewind_frame_latency_and_scheduled_setup() {
        use serde_json::json;
        let Some(disc) = disc() else { eprintln!("skipping: no disc 1 (set XEM_DISC1)"); return };
        let (mut session,_) = xem_game::boot(&disc,&Default::default()).unwrap();
        let log = json!({"cmd":"schedule","commands":[
            [0,{"cmd":"connect_pad","port":1,"connected":false}],
            [0,{"cmd":"enter_field","map":22}],
            [0,{"cmd":"press","buttons":["start"],"frames":4}]
        ]});
        assert!(session.execute(&log).get("error").is_none());
        session.frame().unwrap();
        session.execute(&json!({"cmd":"frames","count":300}));
        assert_eq!(session.execute(&json!({"cmd":"inspect","path":"mode_field_map_id"})),22);
        assert!(!session.runtime.services().pads.0[1].connected);
        assert!(session.execute(&json!({"cmd":"status"})).get("error").is_none());
        let clock = std::rc::Rc::new(std::cell::Cell::new(Duration::ZERO));
        let source = clock.clone();
        let mut session = Session::new(session.runtime,session.schema,move || source.get());
        session.configure_rewind(xem_core::rewind::MAX_REWIND_BYTES,1,30).unwrap();
        let mut samples = Vec::new();
        let mut checkpoint_samples = Vec::new();
        // Put a one-second checkpoint every ten sampled frames, so the p99
        // includes serialization rather than hiding its cost in a small tail.
        for frame in 0..300 {
            clock.set(Duration::from_millis((frame / 10 + 1) * 1000));
            let before = session.rewind.status()["newest_frame"].clone();
            let start = Instant::now();
            assert!(session.frame().unwrap());
            let elapsed = start.elapsed().as_secs_f64() * 1000.0;
            samples.push(elapsed);
            if session.rewind.status()["newest_frame"] != before { checkpoint_samples.push(elapsed); }
        }
        assert!(checkpoint_samples.len() >= 29,"the timing run must actually create checkpoints");
        samples.sort_by(f64::total_cmp);
        checkpoint_samples.sort_by(f64::total_cmp);
        let p99 = samples[296];
        eprintln!("rewind frame latency: p99={p99:.3} ms; checkpoint median={:.3} ms, max={:.3} ms",
            checkpoint_samples[checkpoint_samples.len()/2],checkpoint_samples.last().unwrap());
        assert!(p99 < 16.0,"rewind frame p99 was {p99:.3} ms");
    }

    #[cfg(not(debug_assertions))]
    #[test]
    fn original_game_focus_loss_uses_original_pause_and_waits_for_legal_play() {
        use serde_json::json;
        let Some(disc) = disc() else { eprintln!("skipping: no disc 1 (set XEM_DISC1)"); return };
        let (mut session,_) = xem_game::boot(&disc,&Default::default()).unwrap();
        assert!(session.execute(&json!({"cmd":"enter_field","map":22})).get("error").is_none());
        assert!(session.execute(&json!({"cmd":"frames","count":300})).get("error").is_none());
        let clock = std::rc::Rc::new(std::cell::Cell::new(Duration::ZERO));
        let source = clock.clone();
        let session = Session::new(session.runtime,session.schema,move || source.get());
        let mut game = Game::new(session);
        let settings = xem_settings::Settings { mouse_camera: true, fast_forward_rate: FastForwardRate::Fixed(20), ..Default::default() };
        let paused = |game: &mut Game| game.session.execute(&json!({"cmd":"inspect","path":"sound_driver_flags"})).as_i64().unwrap() & 0x40 != 0;
        let update = |game: &mut Game, focused: bool, settings: &xem_settings::Settings, buttons| {
            game.paced = Instant::now()-FRAME*2;
            game.update(buttons,settings,|| focused).unwrap()
        };
        assert!(!paused(&mut game));
        assert!(game.mouse_camera_active(&settings));
        update(&mut game,true,&settings,0);
        clock.set(Duration::from_secs(1));
        update(&mut game,true,&settings,0);
        game.command(&json!({"cmd":"fast_forward","enabled":true})).unwrap();
        game.command(&json!({"cmd":"write","path":"field_work.unk2358","value":1})).unwrap();
        let before = game.frames_run();
        for _ in 0..4 { update(&mut game,false,&settings,0); }
        assert!(game.frames_run()>before,"unpausable play continues until an original pause check permits it");
        assert!(!paused(&mut game),"the game's script pause lock must be respected");
        assert_eq!(game.time_activity(),(true,false),"running FF appears in the HUD");
        // Refocus before unlocking: a cancelled focus request must not pause later.
        update(&mut game,true,&settings,0);
        game.command(&json!({"cmd":"write","path":"field_work.unk2358","value":0})).unwrap();
        for _ in 0..4 { update(&mut game,true,&settings,0); }
        assert!(!paused(&mut game));
        let input_mask = game.command(&json!({"cmd":"inspect","path":"field_work.input_mask"})).unwrap().as_i64().unwrap();
        game.command(&json!({"cmd":"write","path":"field_work.input_mask","value":input_mask & !0x800})).unwrap();
        for _ in 0..4 { update(&mut game,false,&settings,0); }
        assert!(!paused(&mut game),"a script's Start input mask must also defer focus pausing");
        game.command(&json!({"cmd":"write","path":"field_work.input_mask","value":input_mask})).unwrap();
        for _ in 0..8 {
            update(&mut game,false,&settings,0);
            if paused(&mut game) { break; }
        }
        assert!(paused(&mut game),"focus loss must enter the original pause screen even during FF");
        assert!(!game.mouse_camera_active(&settings),"the original pause screen releases the mouse");
        assert_eq!(game.time_activity(),(false,false),"the original pause hides FF despite its enabled setting");
        let retained = game.session.rewind.status();
        let paused_at = game.frames_run();
        clock.set(Duration::from_secs(60));
        for _ in 0..4 { update(&mut game,false,&settings,0); }
        for _ in 0..4 { update(&mut game,true,&settings,0); }
        assert!(game.frames_run()>paused_at,"the original pause screen still runs frames and reads resume input");
        assert!(paused(&mut game),"neither holding focus loss nor refocusing may resume the game");
        assert_eq!(game.session.rewind.status(),retained,"in-game pause freezes the rewind queue");
        game.command(&json!({"cmd":"fast_forward","enabled":false})).unwrap();
        for _ in 0..2 { update(&mut game,true,&settings,xem_core::pad::button::START); }
        for _ in 0..4 { update(&mut game,true,&settings,0); }
        assert!(!paused(&mut game),"normal Start input resumes the game");
        assert!(game.mouse_camera_active(&settings),"resuming play permits camera capture again");
        let disabled = xem_settings::Settings { pause_on_focus_loss:false, ..settings };
        for _ in 0..4 { update(&mut game,false,&disabled,0); }
        assert!(!paused(&mut game),"disabling the setting keeps unfocused play running");

        // Record an automatic pause, refocus and an ordinary Start resume;
        // replay must produce the same game state without window events.
        game.session.set_window_focus(true,true);
        let initial = game.session.snapshot_bytes().unwrap();
        game.session.execute(&json!({"cmd":"record"}));
        game.session.set_window_focus(false,true);
        assert!(game.session.execute(&json!({"cmd":"frames","count":8})).get("error").is_none());
        assert!(paused(&mut game));
        game.session.set_window_focus(true,true);
        assert!(game.session.execute(&json!({"cmd":"frames","count":4})).get("error").is_none());
        assert!(paused(&mut game));
        game.session.execute(&json!({"cmd":"press","buttons":["start"],"frames":4}));
        assert!(game.session.execute(&json!({"cmd":"frames","count":12})).get("error").is_none());
        assert!(!paused(&mut game));
        let mut log = game.session.execute(&json!({"cmd":"stop_recording"}));
        let final_hash = game.session.execute(&json!({"cmd":"memory_hash"}));
        game.session.restore_bytes(&initial).unwrap();
        log["cmd"] = json!("replay");
        assert!(game.session.execute(&log).get("error").is_none());
        assert!(game.session.execute(&json!({"cmd":"frames","count":24})).get("error").is_none());
        assert_eq!(game.session.execute(&json!({"cmd":"memory_hash"})),final_hash,"focus pause recordings replay exactly");
    }

    #[cfg(not(debug_assertions))]
    #[test]
    fn original_game_held_rewind_stops_at_history_start_and_resumes_on_release() {
        let Some(disc) = disc() else { eprintln!("skipping: no disc 1 (set XEM_DISC1)"); return };
        let (session,_) = xem_game::boot(&disc,&Default::default()).unwrap();
        let clock = std::rc::Rc::new(std::cell::Cell::new(Duration::ZERO));
        let source = clock.clone();
        let session = Session::new(session.runtime,session.schema,move || source.get());
        // An SDL host without an audio device runs the same game silently.
        let mut game = Game::new(session);
        let settings = xem_settings::Settings { fast_forward_rate: FastForwardRate::Fixed(20), ..Default::default() };
        game.configure_rewind(true,settings.rewind_interval_seconds,settings.rewind_history_seconds).unwrap();
        game.command(&serde_json::json!({"cmd":"fast_forward","enabled":true})).unwrap();
        for frame in 1..=150 {
            clock.set(Duration::from_millis(frame*1000/30));
            assert!(game.session.frame().unwrap());
        }
        game.command(&serde_json::json!({"cmd":"fast_forward","enabled":false})).unwrap();
        game.configure_fast_forward(FastForwardRate::Fixed(20)).unwrap();
        game.set_rewind_held(true,false).unwrap();
        game.set_rewind_held(true,true).unwrap();
        game.set_rewind_held(false,true).unwrap();
        assert!(game.rewind_held[0],"releasing a panel hold does not release the bound input");
        // States a second apart (frames 0, 30, ... 150): each step takes the
        // newest before the game's frame and plays on from it, at most
        // REWIND_PLAY_BLANKS (`Session::rewind_step`).
        let landed = |game: &Game, state: u64, was: u64| {
            let frame = game.frames_run();
            assert!(frame > state && frame <= state + xem_core::rewind::REWIND_PLAY_BLANKS && frame < was,
                "from frame {was}, the state at {state} played on to {frame}");
        };
        assert_eq!(game.update(0,&settings,|| true).unwrap(),0);
        landed(&game,120,150);
        assert_eq!(game.time_activity(),(false,true),"only completed rewind steps show the rewind icon");
        assert!(!game.session.fast_forward,"rewind must keep the current FF switch off");
        assert_eq!(game.session.fast_forward_rate,FastForwardRate::Fixed(20));
        for state in [90,60,30,0] {
            let was = game.frames_run();
            game.rewind_next = Instant::now();
            assert_eq!(game.update(0,&settings,|| true).unwrap(),0);
            landed(&game,state,was);
            assert!(!game.session.fast_forward);
            assert_eq!(game.session.runtime.services().clock.audio_speed,1);
            assert_eq!(game.session.runtime.services().spu.speed(),1);
        }
        let first = game.snapshot_bytes().unwrap();
        game.rewind_next = Instant::now();
        assert_eq!(game.update(0,&settings,|| true).unwrap(),0);
        assert_eq!(game.snapshot_bytes().unwrap(),first);
        assert_eq!(game.time_activity(),(false,false),"holding at history start does not claim to rewind");
        game.set_rewind_held(false,false).unwrap();
        game.paced = Instant::now() - FRAME*2;
        assert!(game.update(0,&settings,|| true).unwrap()>0);
        assert!(game.frames_run()>0);
        let saved = game.snapshot_bytes().unwrap();
        game.command(&serde_json::json!({"cmd":"fast_forward_configure","rate":{"Fixed":5}})).unwrap();
        game.command(&serde_json::json!({"cmd":"rewind_configure","max_bytes":16*1024*1024,"interval_seconds":3,"history_seconds":12})).unwrap();
        let oldest = game.session.rewind.status()["oldest_frame"].clone();
        let mut changed = settings.clone(); changed.master_volume = 55;
        game.update(0,&changed,|| true).unwrap();
        assert_eq!(game.session.fast_forward_rate,FastForwardRate::Fixed(5));
        assert_eq!(game.session.rewind.budget,16*1024*1024);
        assert_eq!(game.session.rewind.interval_seconds,3);
        assert_eq!(game.session.rewind.status()["oldest_frame"],oldest);
        game.pause();
        clock.set(Duration::from_secs(3600));
        game.resume();
        game.paced = Instant::now() - FRAME;
        game.update(0,&changed,|| true).unwrap();
        assert_eq!(game.session.rewind.status()["oldest_frame"],oldest,"minimized time must not expire retained states");
        game.session.stopped = Some("test trap".into());
        assert_eq!(game.update(0,&changed,|| true).unwrap(),0,"a stopped game stays available for recovery");
        game.restore_bytes(&saved).unwrap();
        assert!(game.session.stopped.is_none());
    }
}
