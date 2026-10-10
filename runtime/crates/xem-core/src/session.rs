//! The command layer every host shares: load the executable as the BIOS does,
//! step the game, run it until a condition within a step bound, inspect it and
//! take and restore snapshots. Native and browser hosts, their automation
//! interfaces and their UIs call these same operations; nothing here depends on
//! a window, a clock or the host's scheduling.

use serde::{Deserialize, Serialize};

use crate::exe::{self, ExeHeader};
use crate::memory::{OutOfBounds, RAM_BASE, RAM_SIZE, SCRATCHPAD_BASE, SCRATCHPAD_SIZE};
use crate::module::GameModule;
use crate::runtime::{Runtime, RuntimeSnapshot, Stop};

/// How one step ended.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "stop", rename_all = "snake_case")]
pub enum Outcome {
    /// The game waits (`VSync`, `DrawSync`, `Poll` or a number).
    Yield { reason: String },
    /// The game abandoned its stack; the next step runs `xem_run(kind, arg)`.
    Restart { kind: u32, arg: u32 },
    /// `xem_run` returned, which the original never does. The game is halted.
    Returned,
    /// The module trapped or the host refused an import. The game is halted.
    Trap { reason: String },
}

impl Outcome {
    pub fn halts(&self) -> bool {
        matches!(self, Outcome::Returned | Outcome::Trap { .. })
    }
}

impl std::fmt::Display for Outcome {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Outcome::Yield { reason } => write!(f, "yield {reason}"),
            Outcome::Restart { kind, arg } => write!(f, "restart kind {kind} arg {arg:#x}"),
            Outcome::Returned => write!(f, "the game returned"),
            Outcome::Trap { reason } => write!(f, "{reason}"),
        }
    }
}

/// What [`Session::run_until`] waits for, checked after every step.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "until", rename_all = "snake_case")]
pub enum Condition {
    /// The game halts (a trap or a return).
    Halt,
    /// A yield; of this reason when given (`VSync`, `DrawSync`, `Poll`).
    Yield {
        #[serde(default)]
        reason: Option<String>,
    },
    Restart,
    /// The 32-bit little-endian word of game memory at `address` equals `value`.
    Word { address: u32, value: u32 },
}

/// The result of a stepping command.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Report {
    /// Steps this command ran.
    pub steps: u64,
    /// Whether the condition was met (`step_n`: whether every step ran).
    pub met: bool,
    /// How the last step ended.
    pub last: Option<Outcome>,
    pub halted: bool,
}

/// Introspection of a session.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Status {
    pub steps: u64,
    pub yields: u64,
    pub restarts: u64,
    pub suspended: bool,
    /// `xem_run(kind, arg)` of the next step when not suspended.
    pub entry: (u32, u32),
    /// Why the game halted, if it did.
    pub halted: Option<String>,
    pub last: Option<Outcome>,
    /// The port functions the game called that do not exist yet.
    pub missing: Vec<String>,
    /// The services' log (debug breaks).
    pub log: Vec<String>,
}

/// A session between steps: the runtime and the session's counters.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Snapshot {
    pub runtime: RuntimeSnapshot,
    pub steps: u64,
    pub yields: u64,
    pub restarts: u64,
    pub last: Option<Outcome>,
    pub halted: Option<String>,
}

const SNAPSHOT_MAGIC: &[u8; 8] = b"XEMSNAP1";

/// Everything but the region bytes, as JSON in the snapshot file.
#[derive(Serialize, Deserialize)]
struct SnapshotHeader {
    entry: (u32, u32),
    suspended: bool,
    saved_stack_pointer: u32,
    globals: Vec<u32>,
    regions: Vec<(u32, u32)>,
    missing: Vec<u32>,
    log: Vec<String>,
    steps: u64,
    yields: u64,
    restarts: u64,
    last: Option<Outcome>,
    halted: Option<String>,
}

impl Snapshot {
    /// The snapshot as a file: magic, header length, JSON header, region bytes.
    pub fn to_bytes(&self) -> Vec<u8> {
        let r = &self.runtime;
        let header = SnapshotHeader {
            entry: r.entry,
            suspended: r.suspended,
            saved_stack_pointer: r.saved_stack_pointer,
            globals: r.globals.clone(),
            regions: r.regions.iter().map(|(address, bytes)| (*address, bytes.len() as u32)).collect(),
            missing: r.missing.clone(),
            log: r.log.clone(),
            steps: self.steps,
            yields: self.yields,
            restarts: self.restarts,
            last: self.last.clone(),
            halted: self.halted.clone(),
        };
        let json = serde_json::to_vec(&header).expect("snapshot header serializes");
        let mut out = Vec::with_capacity(12 + json.len() + r.regions.iter().map(|(_, b)| b.len()).sum::<usize>());
        out.extend_from_slice(SNAPSHOT_MAGIC);
        out.extend_from_slice(&(json.len() as u32).to_le_bytes());
        out.extend_from_slice(&json);
        for (_, bytes) in &r.regions {
            out.extend_from_slice(bytes);
        }
        out
    }

    pub fn from_bytes(bytes: &[u8]) -> Result<Self, String> {
        let rest = bytes.strip_prefix(SNAPSHOT_MAGIC).ok_or("not a snapshot")?;
        let (len, rest) = rest.split_at_checked(4).ok_or("truncated snapshot")?;
        let len = u32::from_le_bytes(len.try_into().unwrap()) as usize;
        let (json, mut data) = rest.split_at_checked(len).ok_or("truncated snapshot header")?;
        let header: SnapshotHeader = serde_json::from_slice(json).map_err(|e| format!("snapshot header: {e}"))?;
        let mut regions = Vec::new();
        for (address, size) in header.regions {
            let (bytes, rest) = data.split_at_checked(size as usize).ok_or("truncated snapshot region")?;
            regions.push((address, bytes.to_vec()));
            data = rest;
        }
        if !data.is_empty() {
            return Err("trailing bytes after the snapshot regions".into());
        }
        Ok(Snapshot {
            runtime: RuntimeSnapshot {
                entry: header.entry,
                suspended: header.suspended,
                saved_stack_pointer: header.saved_stack_pointer,
                globals: header.globals,
                regions,
                missing: header.missing,
                log: header.log,
            },
            steps: header.steps,
            yields: header.yields,
            restarts: header.restarts,
            last: header.last,
            halted: header.halted,
        })
    }
}

/// The trapping stubs' names by number, from build/game/stubs.txt.
pub fn parse_stub_names(text: &str) -> Vec<String> {
    text.lines().filter_map(|line| line.split_once(' ').map(|(_, name)| name.to_string())).collect()
}

/// FNV-1a (64-bit) over game RAM then the scratchpad: a cheap identity of the
/// game's state for comparing hosts.
pub fn fnv1a(hash: u64, bytes: &[u8]) -> u64 {
    bytes.iter().fold(hash, |h, &b| (h ^ u64::from(b)).wrapping_mul(0x100_0000_01B3))
}

pub const FNV_OFFSET: u64 = 0xCBF2_9CE4_8422_2325;

/// A session's notable steps as hosts print them: every restart and stop and
/// the first [`StepLog::YIELDS`] yields, as `step N: outcome`.
#[derive(Clone, Debug, Default)]
pub struct StepLog {
    pub lines: Vec<String>,
    yields: u64,
}

impl StepLog {
    pub const YIELDS: u64 = 5;

    /// Record a step; returns the line when it is logged.
    pub fn record(&mut self, index: u64, outcome: &Outcome) -> Option<&str> {
        if matches!(outcome, Outcome::Yield { .. }) {
            self.yields += 1;
            if self.yields > Self::YIELDS {
                return None;
            }
        }
        self.lines.push(format!("step {index}: {outcome}"));
        self.lines.last().map(String::as_str)
    }
}

/// The game module under the command layer.
pub struct Session<M: GameModule> {
    runtime: Runtime<M>,
    steps: u64,
    yields: u64,
    restarts: u64,
    last: Option<Outcome>,
    halted: Option<String>,
}

impl<M: GameModule> Session<M> {
    pub fn new(module: M, stub_names: Vec<String>) -> Self {
        let mut runtime = Runtime::new(module);
        runtime.services().stub_names = stub_names;
        Session { runtime, steps: 0, yields: 0, restarts: 0, last: None, halted: None }
    }

    /// Load a PS-X EXE as the BIOS does: copy its text to its address. The
    /// first step then starts the game at its entry.
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

    /// Run one step; `None` once the game has halted.
    pub fn step(&mut self) -> Option<Outcome> {
        if self.halted.is_some() {
            return None;
        }
        let outcome = match self.runtime.step() {
            Ok(Stop::Yield(reason)) => {
                self.yields += 1;
                Outcome::Yield { reason: format!("{reason:?}") }
            }
            Ok(Stop::Restart { kind, arg }) => {
                self.restarts += 1;
                Outcome::Restart { kind, arg }
            }
            Ok(Stop::Returned) => Outcome::Returned,
            Err(trap) => Outcome::Trap { reason: trap.to_string() },
        };
        self.steps += 1;
        if outcome.halts() {
            self.halted = Some(outcome.to_string());
        }
        self.last = Some(outcome.clone());
        Some(outcome)
    }

    /// Run up to `count` steps, stopping early if the game halts. `observe`
    /// sees each step's index and outcome.
    pub fn step_n(&mut self, count: u64, observe: impl FnMut(u64, &Outcome)) -> Report {
        let report = self.drive(count, observe, |_, _| false);
        Report { met: report.steps == count, ..report }
    }

    /// Step until `condition` holds after a step, at most `max_steps` steps or
    /// until the game halts. `observe` sees each step's index and outcome.
    pub fn run_until(&mut self, condition: &Condition, max_steps: u64, observe: impl FnMut(u64, &Outcome)) -> Report {
        self.drive(max_steps, observe, |session, outcome| match condition {
            Condition::Halt => outcome.halts(),
            Condition::Yield { reason: None } => matches!(outcome, Outcome::Yield { .. }),
            Condition::Yield { reason: Some(want) } => {
                matches!(outcome, Outcome::Yield { reason } if reason.eq_ignore_ascii_case(want))
            }
            Condition::Restart => matches!(outcome, Outcome::Restart { .. }),
            Condition::Word { address, value } => {
                session.runtime.memory().read_u32(*address).is_ok_and(|word| word == *value)
            }
        })
    }

    fn drive(
        &mut self,
        max_steps: u64,
        mut observe: impl FnMut(u64, &Outcome),
        mut met: impl FnMut(&mut Self, &Outcome) -> bool,
    ) -> Report {
        let mut ran = 0;
        let mut done = false;
        while ran < max_steps {
            let index = self.steps;
            let Some(outcome) = self.step() else { break };
            ran += 1;
            observe(index, &outcome);
            done = met(self, &outcome);
            if done || outcome.halts() {
                break;
            }
        }
        Report { steps: ran, met: done, last: self.last.clone(), halted: self.is_halted() }
    }

    pub fn status(&mut self) -> Status {
        let (suspended, entry) = (self.runtime.is_suspended(), self.runtime.entry());
        let services = self.runtime.services();
        let missing = services
            .missing
            .clone()
            .into_iter()
            .map(|id| services.stub_names.get(id as usize).cloned().unwrap_or_else(|| format!("stub #{id}")))
            .collect();
        Status {
            steps: self.steps,
            yields: self.yields,
            restarts: self.restarts,
            suspended,
            entry,
            halted: self.halted.clone(),
            last: self.last.clone(),
            missing,
            log: services.log.clone(),
        }
    }

    /// [`fnv1a`] over game RAM and the scratchpad.
    pub fn digest(&mut self) -> Result<u64, OutOfBounds> {
        let mut hash = FNV_OFFSET;
        for (address, len) in [(RAM_BASE, RAM_SIZE), (SCRATCHPAD_BASE, SCRATCHPAD_SIZE)] {
            let mut bytes = vec![0; len as usize];
            self.runtime.memory().read(address, &mut bytes)?;
            hash = fnv1a(hash, &bytes);
        }
        Ok(hash)
    }

    pub fn snapshot(&mut self) -> Result<Snapshot, OutOfBounds> {
        Ok(Snapshot {
            runtime: self.runtime.snapshot()?,
            steps: self.steps,
            yields: self.yields,
            restarts: self.restarts,
            last: self.last.clone(),
            halted: self.halted.clone(),
        })
    }

    pub fn restore(&mut self, snapshot: &Snapshot) -> Result<(), OutOfBounds> {
        self.runtime.restore(&snapshot.runtime)?;
        self.steps = snapshot.steps;
        self.yields = snapshot.yields;
        self.restarts = snapshot.restarts;
        self.last = snapshot.last.clone();
        self.halted = snapshot.halted.clone();
        Ok(())
    }
}
