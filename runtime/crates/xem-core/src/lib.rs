//! The headless runtime around the game module (docs/runtime.md).
//!
//! The recovered C runs as the game module; this crate answers its imports
//! (the host services), drives it through asyncify's unwind/rewind protocol
//! and owns the virtual clock. It needs no window, GPU, audio or input device.

pub mod clock;
pub mod devices;
pub mod exe;
pub mod memory;
pub mod module;
pub mod pad;
pub mod runtime;

pub use memory::{GameMemory, SliceMemory};
pub use module::{Action, GameModule, Import, Trap};
pub use runtime::{FrameReport, Runtime, Services, Stop, YieldReason};
