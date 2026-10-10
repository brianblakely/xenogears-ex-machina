//! The headless runtime around the game module (docs/runtime.md).
//!
//! The recovered C runs as the game module; this crate answers its imports
//! (the host services), drives it through asyncify's unwind/rewind protocol
//! and owns the virtual clock. It needs no window, GPU, audio or input device.

pub mod exe;
pub mod memory;
pub mod module;
pub mod runtime;
pub mod session;

pub use memory::{GameMemory, SliceMemory};
pub use module::{Action, GameModule, Import, Trap};
pub use runtime::{Runtime, RuntimeSnapshot, Services, Stop, YieldReason};
pub use session::{Condition, Outcome, Report, Session, Snapshot, Status, StepLog};
