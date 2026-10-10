//! The command layer every host shares: load the executable as the BIOS does,
//! advance the game by frames of the virtual clock, run it until a condition
//! within a frame bound, inspect it and take and restore snapshots. Native and
//! browser hosts, their automation interfaces and their UIs call these same
//! operations; nothing here depends on a window, wall time or the host's
//! scheduling.

use serde::{Deserialize, Serialize};

use crate::exe::{self, ExeHeader};
use crate::memory::{OutOfBounds, RAM_BASE, RAM_SIZE, SCRATCHPAD_BASE, SCRATCHPAD_SIZE};
use crate::module::GameModule;
use crate::runtime::Runtime;
use crate::snapshot::SnapshotError;

/// How one frame (`Runtime::run_frame`) ended.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "stop", rename_all = "snake_case")]
pub enum Outcome {
    /// The clock passed the next vertical blank.
    Frame { steps: u64, interrupts: u64, restarts: Vec<(u32, u32)> },
    /// The module trapped or the host refused an import. The game is halted.
    Trap { reason: String },
}

impl Outcome {
    pub fn halts(&self) -> bool {
        matches!(self, Outcome::Trap { .. })
    }
}

/// What [`Session::run_until`] waits for, checked after every frame.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "until", rename_all = "snake_case")]
pub enum Condition {
    /// The game halts.
    Halt,
    /// The game abandons its stack (mode dispatch, soft reset).
    Restart,
    /// The 32-bit little-endian word of game memory at `address` equals `value`.
    Word { address: u32, value: u32 },
}

/// The result of a stepping command.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Report {
    /// Frames this command ran.
    pub frames: u64,
    /// Whether the condition was met (`run_frames`: whether every frame ran).
    pub met: bool,
    /// How the last frame ended.
    pub last: Option<Outcome>,
    pub halted: bool,
}

/// Introspection of a session.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Status {
    pub frames: u64,
    /// Waits the game reached (module runs), interrupts delivered, restarts.
    pub steps: u64,
    pub interrupts: u64,
    pub restarts: u64,
    /// The virtual clock: CPU cycles and vertical blanks since boot.
    pub cycles: u64,
    pub vblanks: u64,
    pub suspended: bool,
    /// `xem_run(kind, arg)` of the next run when not suspended.
    pub entry: (u32, u32),
    /// Why the game halted, if it did.
    pub halted: Option<String>,
    pub last: Option<Outcome>,
    /// The port functions the game called that do not exist yet.
    pub missing: Vec<String>,
    /// The services' log (debug breaks).
    pub log: Vec<String>,
}

/// A session's notable frames as hosts print them: each restart and the stop.
#[derive(Clone, Debug, Default)]
pub struct FrameLog {
    pub lines: Vec<String>,
}

impl FrameLog {
    /// Record frame `index`; returns the lines it added.
    pub fn record(&mut self, index: u64, outcome: &Outcome) -> &[String] {
        let start = self.lines.len();
        match outcome {
            Outcome::Frame { restarts, .. } => {
                for (kind, arg) in restarts {
                    self.lines.push(format!("frame {index}: restart kind {kind} arg {arg:#x}"));
                }
            }
            Outcome::Trap { reason } => self.lines.push(format!("frame {index}: {reason}")),
        }
        &self.lines[start..]
    }
}

/// The trapping stubs' names by number, from build/game/stubs.txt.
pub fn parse_stub_names(text: &str) -> Vec<String> {
    text.lines().filter_map(|line| line.split_once(' ').map(|(_, name)| name.to_string())).collect()
}

pub const FNV_OFFSET: u64 = 0xCBF2_9CE4_8422_2325;

/// FNV-1a (64-bit), continuing from `hash`.
pub fn fnv1a(hash: u64, bytes: &[u8]) -> u64 {
    bytes.iter().fold(hash, |h, &b| (h ^ u64::from(b)).wrapping_mul(0x100_0000_01B3))
}

const SNAPSHOT_MAGIC: &[u8; 8] = b"XEMSESS\0";

/// The session's own state in a snapshot, ahead of the runtime's.
#[derive(Serialize, Deserialize)]
struct Counters {
    frames: u64,
    steps: u64,
    interrupts: u64,
    restarts: u64,
    halted: Option<String>,
    last: Option<Outcome>,
    missing: Vec<u32>,
    log: Vec<String>,
}

/// The game module under the command layer.
pub struct Session<M: GameModule> {
    runtime: Runtime<M>,
    frames: u64,
    steps: u64,
    interrupts: u64,
    restarts: u64,
    last: Option<Outcome>,
    halted: Option<String>,
}

impl<M: GameModule> Session<M> {
    pub fn new(module: M, stub_names: Vec<String>) -> Self {
        let mut runtime = Runtime::new(module);
        runtime.services().stub_names = stub_names;
        Session { runtime, frames: 0, steps: 0, interrupts: 0, restarts: 0, last: None, halted: None }
    }

    /// Load a PS-X EXE as the BIOS does: copy its text to its address. The
    /// first frame then starts the game at its entry.
    pub fn load_executable(&mut self, executable: &[u8]) -> Result<ExeHeader, String> {
        let header = exe::parse(executable)?;
        self.runtime
            .memory()
            .write(header.text_address, exe::text(executable, &header))
            .map_err(|e| e.to_string())?;
        Ok(header)
    }

    pub fn runtime(&mut self) -> &mut Runtime<M> {
        &mut self.runtime
    }

    pub fn is_halted(&self) -> bool {
        self.halted.is_some()
    }

    /// Run one frame; `None` once the game has halted.
    pub fn frame(&mut self) -> Option<Outcome> {
        if self.halted.is_some() {
            return None;
        }
        let outcome = match self.runtime.run_frame() {
            Ok(report) => {
                self.steps += report.steps;
                self.interrupts += report.interrupts;
                self.restarts += report.restarts.len() as u64;
                Outcome::Frame { steps: report.steps, interrupts: report.interrupts, restarts: report.restarts }
            }
            Err(trap) => Outcome::Trap { reason: trap.to_string() },
        };
        self.frames += 1;
        if let Outcome::Trap { reason } = &outcome {
            self.halted = Some(reason.clone());
        }
        self.last = Some(outcome.clone());
        Some(outcome)
    }

    /// Run up to `count` frames, stopping early if the game halts. `observe`
    /// sees each frame's index and outcome.
    pub fn run_frames(&mut self, count: u64, observe: impl FnMut(u64, &Outcome)) -> Report {
        let report = self.drive(count, observe, |_, _| false);
        Report { met: report.frames == count, ..report }
    }

    /// Run frames until `condition` holds after one, at most `max_frames`
    /// frames or until the game halts. `observe` sees each frame.
    pub fn run_until(&mut self, condition: &Condition, max_frames: u64, observe: impl FnMut(u64, &Outcome)) -> Report {
        self.drive(max_frames, observe, |session, outcome| match condition {
            Condition::Halt => outcome.halts(),
            Condition::Restart => matches!(outcome, Outcome::Frame { restarts, .. } if !restarts.is_empty()),
            Condition::Word { address, value } => {
                session.runtime.memory().read_u32(*address).is_ok_and(|word| word == *value)
            }
        })
    }

    fn drive(
        &mut self,
        max_frames: u64,
        mut observe: impl FnMut(u64, &Outcome),
        mut met: impl FnMut(&mut Self, &Outcome) -> bool,
    ) -> Report {
        let mut ran = 0;
        let mut done = false;
        while ran < max_frames {
            let index = self.frames;
            let Some(outcome) = self.frame() else { break };
            ran += 1;
            observe(index, &outcome);
            done = met(self, &outcome);
            if done || outcome.halts() {
                break;
            }
        }
        Report { frames: ran, met: done, last: self.last.clone(), halted: self.is_halted() }
    }

    pub fn status(&mut self) -> Status {
        let (suspended, entry) = (self.runtime.is_suspended(), self.runtime.entry());
        let services = self.runtime.services();
        let missing = services
            .missing
            .iter()
            .map(|&id| services.stub_names.get(id as usize).cloned().unwrap_or_else(|| format!("stub #{id}")))
            .collect();
        Status {
            frames: self.frames,
            steps: self.steps,
            interrupts: self.interrupts,
            restarts: self.restarts,
            cycles: services.clock.now,
            vblanks: services.clock.vblanks,
            suspended,
            entry,
            halted: self.halted.clone(),
            last: self.last.clone(),
            missing,
            log: services.log.clone(),
        }
    }

    /// [`fnv1a`] over game RAM and the scratchpad: a cheap identity of the
    /// game's state for comparing hosts.
    pub fn digest(&mut self) -> Result<u64, OutOfBounds> {
        let mut hash = FNV_OFFSET;
        for (address, len) in [(RAM_BASE, RAM_SIZE), (SCRATCHPAD_BASE, SCRATCHPAD_SIZE)] {
            let mut bytes = vec![0; len as usize];
            self.runtime.memory().read(address, &mut bytes)?;
            hash = fnv1a(hash, &bytes);
        }
        Ok(hash)
    }

    /// The session between frames, as bytes: its counters, then the runtime's
    /// snapshot (`Runtime::snapshot`).
    pub fn snapshot(&mut self) -> Result<Vec<u8>, SnapshotError> {
        let services = self.runtime.services();
        let counters = Counters {
            frames: self.frames,
            steps: self.steps,
            interrupts: self.interrupts,
            restarts: self.restarts,
            halted: self.halted.clone(),
            last: self.last.clone(),
            missing: services.missing.clone(),
            log: services.log.clone(),
        };
        let json = serde_json::to_vec(&counters).expect("session counters serialize");
        let mut out = SNAPSHOT_MAGIC.to_vec();
        out.extend_from_slice(&(json.len() as u32).to_le_bytes());
        out.extend_from_slice(&json);
        out.extend_from_slice(&self.runtime.snapshot()?);
        Ok(out)
    }

    /// Return to a snapshot of this game module's session, also after a halt.
    pub fn restore(&mut self, bytes: &[u8]) -> Result<(), SnapshotError> {
        let rest = bytes.strip_prefix(SNAPSHOT_MAGIC).ok_or(SnapshotError::Format("not a session snapshot"))?;
        let (len, rest) = rest.split_at_checked(4).ok_or(SnapshotError::Format("truncated"))?;
        let len = u32::from_le_bytes(len.try_into().unwrap()) as usize;
        let (json, runtime) = rest.split_at_checked(len).ok_or(SnapshotError::Format("truncated"))?;
        let counters: Counters =
            serde_json::from_slice(json).map_err(|_| SnapshotError::Format("session counters"))?;
        self.runtime.restore(runtime)?;
        let services = self.runtime.services();
        services.missing = counters.missing;
        services.log = counters.log;
        services.trap_reason = None;
        self.frames = counters.frames;
        self.steps = counters.steps;
        self.interrupts = counters.interrupts;
        self.restarts = counters.restarts;
        self.halted = counters.halted;
        self.last = counters.last;
        Ok(())
    }
}
