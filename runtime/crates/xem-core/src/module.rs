//! The game module as the runtime sees it, natively (wasm2c, `xem-game`) or in
//! a browser (`xem-web`).

use crate::memory::GameMemory;

/// What an import tells the module to do next.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Action {
    /// Return this value to the game.
    Return(u32),
    /// Suspend: unwind the game stack (asyncify); the runtime resumes it.
    Unwind,
    /// Stop the game with an error.
    Trap,
}

/// A host import call: its name in the "xem" import module and its arguments.
#[derive(Debug, Clone, Copy)]
pub struct Import<'a> {
    pub name: &'a str,
    pub args: &'a [u32],
}

/// Why the module stopped abnormally.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Trap {
    /// A wasm trap (bounds, unreachable, division...), with the engine's description.
    Wasm(String),
    /// An import refused the call; the services hold the reason.
    Host(String),
}

impl std::fmt::Display for Trap {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Trap::Wasm(reason) => write!(f, "game module trap: {reason}"),
            Trap::Host(reason) => write!(f, "host stopped the game: {reason}"),
        }
    }
}

impl std::error::Error for Trap {}

/// Asyncify's state (binaryen): normal, unwinding or rewinding.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AsyncState {
    Normal,
    Unwinding,
    Rewinding,
}

impl AsyncState {
    pub fn from_raw(raw: u32) -> Self {
        match raw {
            1 => AsyncState::Unwinding,
            2 => AsyncState::Rewinding,
            _ => AsyncState::Normal,
        }
    }
}

/// Imports are answered by a handler the runtime installs; the module calls it
/// while one of its exports runs.
pub trait ImportHandler {
    fn import(&mut self, import: Import<'_>, memory: &mut dyn GameMemory) -> Action;
}

pub trait GameModule {
    /// Install the handler for imports. The runtime keeps it alive and does not
    /// touch it while an export runs.
    ///
    /// # Safety
    /// `handler` must stay valid, and unaliased during export calls, until replaced.
    unsafe fn set_import_handler(&mut self, handler: *mut dyn ImportHandler);
    /// The value an import returns when the game resumes from it (rewind).
    fn set_resume_value(&mut self, value: u32);
    /// The export `xem_run(kind, arg)`.
    fn run(&mut self, kind: u32, arg: u32) -> Result<(), Trap>;
    /// The export `xem_call(address)`: an interrupt callback.
    fn call(&mut self, address: u32) -> Result<(), Trap>;
    fn async_state(&mut self) -> AsyncState;
    fn stop_unwind(&mut self);
    /// Rewind with the save area the last unwind filled.
    fn start_rewind(&mut self);
    fn stack_pointer(&mut self) -> u32;
    fn set_stack_pointer(&mut self, value: u32);
    fn memory(&mut self) -> &mut dyn GameMemory;
    /// The end of the port's shadow stack and data (the module's `__heap_base`):
    /// memory below it is the module's own state, kept in snapshots.
    fn data_end(&mut self) -> u32;
    /// The module's mutable globals other than the stack pointer, for snapshots.
    fn globals(&mut self) -> Vec<u32>;
    fn set_globals(&mut self, values: &[u32]);
}
