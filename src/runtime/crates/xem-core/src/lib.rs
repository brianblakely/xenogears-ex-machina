//! The headless runtime around the game module (docs/runtime.md).
//!
//! The recovered C runs as the game module; this crate answers its imports
//! (the host services), drives it through asyncify's unwind/rewind protocol
//! and owns the virtual clock. It needs no window, GPU, audio or input device.

pub mod action;
pub mod assist;
pub mod binaural;
pub mod bios;
pub mod clock;
pub mod context;
pub mod control;
pub mod dialogue;
pub mod debug;
mod focus;
pub mod time;
pub mod devices;
pub mod encounters;
pub mod field;
pub mod exe;
pub mod inspect;
pub mod memory;
pub mod menu;
pub mod module;
pub mod movie;
pub mod pad;
pub mod presentation;
pub mod runtime;
pub mod rewind;
pub mod snapshot;

pub use memory::{GameMemory, SliceMemory};
pub use module::{Action, GameModule, Import, Trap};
pub use runtime::{FieldControls, FrameReport, Runtime, Services, Stop, YieldReason};
