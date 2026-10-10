//! The game module natively: the wasm2c translation of build/game/game.wasm,
//! compiled into the host (docs/runtime.md). Ahead-of-time compiled C with
//! explicit bounds checks; nothing interprets or emulates a CPU.

#[cfg(has_game_module)]
mod native;

#[cfg(has_game_module)]
pub use native::NativeModule;

/// Whether this build carries the game module (it needs the user's matched
/// PS1 builds to produce).
pub const AVAILABLE: bool = cfg!(has_game_module);
