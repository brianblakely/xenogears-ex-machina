//! The original kernel menu's operations, for Settings → Debug.
//!
//! Test launches start from a clean boot and use the original mode dispatcher
//! and selectors. The selectors still perform their own loading and game-state
//! setup; this adapter only supplies the values a person would select there.
use std::collections::VecDeque;

use serde::{Deserialize, Serialize};
use serde_json::{Value, json};

use crate::{GameModule, control::Session, pad::button};

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "tool", rename_all = "snake_case", deny_unknown_fields)]
pub enum Request {
    Field { map: u16, entry: u16 },
    Battle { formation: u8, file: u8, party: [u8; 3], gears: [u8; 3] },
    Worldmap { map: u16 },
    Battling,
    Menu { screen: u8, parameter: u8 },
    Movie { movie: u8, kind: u8 },
}

impl Request {
    pub fn validate(self) -> Result<(), String> {
        match self {
            Self::Field { map, .. } if map >= 0x400 => Err("Field map must be between 0 and 1023.".into()),
            Self::Battle { formation, party, gears, .. } if formation > 15 || party.iter().any(|p| *p > 11)
                || party.iter().all(|p| *p == 11) || gears.iter().any(|g| *g > 2) =>
                Err("Choose formation 0–15, at least one party member, and Off, Normal or Bar Gears.".into()),
            Self::Worldmap { map } if !(0x400..0x800).contains(&map) => Err("World map must be between 1024 and 2047.".into()),
            Self::Menu { screen, parameter } if screen > 6 || (screen < 4 && parameter > 30) || (screen == 6 && parameter > 1) =>
                Err("Choose a menu screen and a valid character, shop or disc number.".into()),
            Self::Movie { movie, kind } if movie > 63 || kind > 2 => Err("Choose movie 0–63 and Picture, Picture and audio, or Audio.".into()),
            _ => Ok(()),
        }
    }
}

enum Step {
    Equal(String, i64),
    Mode(u32),
    MenuInput(u8),
    InputReady,
    Write(String, i64),
    Pad(u16),
    Frames(u32),
}

#[derive(Default)]
pub struct Debug {
    steps: VecDeque<Step>,
    frames_left: u32,
    held: u16,
    pub message: String,
}

impl Debug {
    pub fn busy(&self) -> bool { !self.steps.is_empty() }

    fn write(&mut self, path: impl Into<String>, value: impl Into<i64>) {
        self.steps.push_back(Step::Write(path.into(), value.into()));
    }

    fn wait(&mut self, path: &str, value: i64) {
        self.steps.push_back(Step::Equal(path.into(), value));
    }

    fn press(&mut self, button: u16) {
        self.steps.extend([Step::Pad(button), Step::Frames(4), Step::Pad(0), Step::Frames(4)]);
    }

    fn launch(request: Request) -> Self {
        let mut job = Self { frames_left: 6000, message: "Starting debug test…".into(), ..Default::default() };
        if matches!(request, Request::Field { .. }) {
            job.wait("mode_kernel_menu_running", 1);
        } else {
            job.wait("cd_movie_request_kind", 1);
        }
        let mode = match request {
            Request::Field { map, entry } => {
                job.write("game_data.map", map);
                job.write("game_data.entry[2]", entry);
                1
            }
            Request::Battle { .. } => 2,
            Request::Worldmap { map } => { job.write("game_data.map", map); 3 }
            Request::Battling => 4,
            Request::Menu { .. } => 5,
            Request::Movie { movie, kind } => {
                job.write("cd_movie_request_kind", kind);
                job.write("cd_movie_request_index", movie);
                job.write("cd_movie_request_next_mode", 0);
                job.write("cd_movie_request_unskippable", 0);
                6
            }
        };
        if matches!(request, Request::Field { .. }) {
            job.write("mode_next_mode", mode);
            job.write("mode_kernel_menu_running", 0);
        }
        job.steps.push_back(Step::Mode(mode as u32));
        if matches!(request, Request::Battle { .. } | Request::Menu { .. }) {
            // The mode notification precedes overlay loading. Wait until the
            // new mode has actually polled input; old overlay values alone
            // cannot establish readiness after a second test launch.
            job.wait("mode_next_mode", 0);
            job.steps.extend([Step::Frames(1), Step::InputReady]);
        }
        match request {
            Request::Battle { formation, file, party, gears } => {
                job.wait("battle_scene_select_values[0][0]", 3);
                job.wait("battle_scene_select_shown_columns[1][0]", 1);
                job.wait("battle_scene_select_shown_columns[2][0]", 1);
                job.write("battle_scene_select_values[0][0]", formation);
                job.write("battle_scene_select_values[3][0]", file);
                for i in 0..3 {
                    job.write(format!("battle_scene_select_values[1][{i}]"), party[i]);
                    job.write(format!("battle_scene_select_values[2][{i}]"), gears[i]);
                }
                job.press(button::START);
            }
            Request::Menu { screen, parameter } => {
                // Supply the selector's decoded inputs at its frame boundary.
                // Its locals still wrap and select the original screen, whose
                // loader and setup run unchanged (mode_battle_and_menu.c).
                for _ in 0..screen { job.steps.extend([Step::MenuInput(0), Step::Frames(1)]); }
                for _ in 0..parameter { job.steps.extend([Step::MenuInput(3), Step::Frames(1)]); }
                job.steps.extend([Step::MenuInput(4), Step::Frames(1)]);
                job.wait("game_data.gold", 999999999);
            }
            _ => {}
        }
        job
    }
}

impl<M: GameModule> Session<M> {
    /// Queue a bounded test launch. Normal host frames, or debug_tick while
    /// Settings is open, advance it. Async disc reads resume on the next tick.
    pub fn request_debug(&mut self, request: Request) -> Result<(), String> {
        request.validate()?;
        if self.debug.busy() { return Err("A debug operation is still running.".into()); }
        let address = self.schema.as_ref().ok_or("No game schema loaded.")?
            .locate("mode_next_mode", self.runtime.memory()).map_err(|e| e.to_string())?.address;
        // Validate the job's schema before a reset can discard progress.
        let job = Debug::launch(request);
        for step in &job.steps {
            let path = match step { Step::Equal(path, _) | Step::Write(path, _) => path.as_str(), Step::MenuInput(_) => "menu_state_current", _ => continue };
            self.schema.as_ref().unwrap().locate(path, self.runtime.memory()).map_err(|e| e.to_string())?;
        }
        self.record_control(json!({"cmd":"debug", "request":request}));
        self.reset_game();
        self.runtime.write_before_next_dispatch(address, match request {
            Request::Field { .. } => 0, Request::Battle { .. } => 2, Request::Worldmap { .. } => 3,
            Request::Battling => 4, Request::Menu { .. } => 5, Request::Movie { .. } => 6,
        });
        self.debug = job;
        Ok(())
    }

    pub fn debug_status(&self) -> Value {
        json!({"busy":self.debug.busy(), "message":self.debug.message})
    }

    /// A small batch, also used by hosts whose Settings pause ordinary play.
    pub fn tick_debug(&mut self) -> Result<(), String> {
        for _ in 0..4 {
            if !self.debug.busy() { break; }
            match self.frame() {
                Ok(true) => {},
                Ok(false) => break,
                Err(error) => return self.fail_debug(&error.to_string()),
            }
        }
        Ok(())
    }

    pub(crate) fn advance_debug(&mut self) -> Result<(), String> {
        if !self.debug.busy() { return Ok(()); }
        self.runtime.services().pads.0[0].buttons = self.debug.held;
        if self.debug.frames_left == 0 { return self.fail_debug("The debug operation did not finish within 6000 frames."); }
        loop {
            let Some(step) = self.debug.steps.pop_front() else {
                self.debug.message = "Debug test started.".into();
                return Ok(());
            };
            let result = match &step {
                Step::Equal(path, value) => self.inspect(path, 0).map(|actual| actual.as_i64() == Some(*value)),
                Step::Mode(mode) => Ok(self.runtime.services().mode == Some(*mode)),
                Step::MenuInput(input) => self.debug_menu_input(*input),
                Step::InputReady => Ok(self.runtime.services().input_reads_last_frame > 0),
                Step::Write(path, value) => self.write_path(path, &json!(value)).map(|()| true),
                Step::Pad(buttons) => { self.debug.held = *buttons; self.runtime.services().pads.0[0].buttons = *buttons; Ok(true) }
                Step::Frames(0) => Ok(true),
                Step::Frames(_) => Ok(false),
            };
            match result {
                Ok(true) => continue,
                Ok(false) => { self.debug.steps.push_front(step); return Ok(()); }
                Err(error) => return self.fail_debug(&error),
            }
        }
    }

    fn debug_menu_input(&mut self, input: u8) -> Result<bool, String> {
        // menu_state_current is declared void* in the schema. MenuState's
        // +0x325 input is shared by all seven screens (resident/menu.h).
        let address = self.schema.as_ref().ok_or("No game schema loaded.")?
            .locate("menu_state_current", self.runtime.memory()).map_err(|e| e.to_string())?.address;
        let memory = self.runtime.memory();
        let state = memory.read_u32(address).map_err(|e| e.to_string())?;
        if state == 0 { return Ok(false); }
        if !(0x80000000..=0x80200000 - 0x1e98).contains(&state) {
            return Err("The menu work area is invalid.".into());
        }
        memory.write(state + 0x325, &[input]).map_err(|e| e.to_string())?;
        Ok(true)
    }

    pub(crate) fn debug_frame_finished(&mut self) {
        if self.debug.busy() {
            self.debug.frames_left = self.debug.frames_left.saturating_sub(1);
            if let Some(Step::Frames(frames)) = self.debug.steps.front_mut() { *frames = frames.saturating_sub(1); }
        }
    }

    pub(crate) fn fail_debug(&mut self, error: &str) -> Result<(), String> {
        self.debug.steps.clear();
        self.debug.message = error.into();
        self.set_pad(0, 0);
        Err(error.into())
    }
}
