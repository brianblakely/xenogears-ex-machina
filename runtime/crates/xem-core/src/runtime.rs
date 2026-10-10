//! The run loop: start, suspend, resume and restart the game.

use crate::memory::GameMemory;
use crate::module::{Action, AsyncState, GameModule, Import, ImportHandler, Trap};

/// Where `xem_run` starts (port/include/xem/port.h).
pub const RESTART_BOOT: u32 = 0;
pub const RESTART_DISPATCH: u32 = 1;

/// Yield reasons (port/include/xem/port.h).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum YieldReason {
    VSync,
    DrawSync,
    Poll,
    Other(u32),
}

impl YieldReason {
    fn from_raw(raw: u32) -> Self {
        match raw {
            1 => YieldReason::VSync,
            2 => YieldReason::DrawSync,
            3 => YieldReason::Poll,
            other => YieldReason::Other(other),
        }
    }
}

/// Why `Runtime::step` returned.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Stop {
    /// The game waits; the host advances the clock and resumes it.
    Yield(YieldReason),
    /// The game abandoned its stack; the next step starts `xem_run(kind, arg)`.
    Restart { kind: u32, arg: u32 },
    /// `xem_run` returned, which the original program never does.
    Returned,
}

/// The services the imports reach.
#[derive(Default)]
pub struct Services {
    pending: Option<Stop>,
    pub missing: Vec<u32>,
    pub log: Vec<String>,
    pub trap_reason: Option<String>,
    /// Names of the trapping stubs, by number (build/game/stubs.txt).
    pub stub_names: Vec<String>,
}

impl Services {
    fn stub_name(&self, id: u32) -> String {
        if id >= 0x10000 {
            return format!("unmapped inline assembly #{}", id - 0x10000);
        }
        self.stub_names.get(id as usize).cloned().unwrap_or_else(|| format!("stub #{id}"))
    }
}

impl ImportHandler for Services {
    fn import(&mut self, import: Import<'_>, _memory: &mut dyn GameMemory) -> Action {
        let arg = |i: usize| import.args.get(i).copied().unwrap_or(0);
        match import.name {
            "yield" => {
                self.pending = Some(Stop::Yield(YieldReason::from_raw(arg(0))));
                Action::Unwind
            }
            "restart" => {
                self.pending = Some(Stop::Restart { kind: arg(0), arg: arg(1) });
                Action::Unwind
            }
            "debug_break" => {
                self.log.push(format!("break {}", arg(0)));
                Action::Return(0)
            }
            "missing" => {
                self.missing.push(arg(0));
                self.trap_reason = Some(format!("called {}, which the port does not define", self.stub_name(arg(0))));
                Action::Trap
            }
            "bad_call" => {
                self.trap_reason = Some(format!(
                    "indirect call to {:#010x} (signature #{}) found no loaded function",
                    arg(0),
                    arg(1)
                ));
                Action::Trap
            }
            other => {
                self.trap_reason = Some(format!("unknown import xem.{other}"));
                Action::Trap
            }
        }
    }
}

/// The game module with its services.
pub struct Runtime<M: GameModule> {
    module: M,
    services: *mut Services,
    entry: (u32, u32),
    suspended: bool,
    stack_top: u32,
    saved_stack_pointer: u32,
}

impl<M: GameModule> Runtime<M> {
    pub fn new(mut module: M) -> Self {
        let services = Box::into_raw(Box::<Services>::default());
        // SAFETY: the box lives until Drop; the runtime only touches it between
        // export calls.
        unsafe { module.set_import_handler(services as *mut dyn ImportHandler) };
        let stack_top = module.stack_pointer();
        Runtime { module, services, entry: (RESTART_BOOT, 0), suspended: false, stack_top, saved_stack_pointer: 0 }
    }

    pub fn services(&mut self) -> &mut Services {
        // SAFETY: no export runs while `self` is borrowed mutably here.
        unsafe { &mut *self.services }
    }

    pub fn module(&mut self) -> &mut M {
        &mut self.module
    }

    pub fn memory(&mut self) -> &mut dyn GameMemory {
        self.module.memory()
    }

    /// Run the game until it yields, restarts or traps.
    pub fn step(&mut self) -> Result<Stop, Trap> {
        let (kind, arg) = self.entry;
        if self.suspended {
            self.module.set_stack_pointer(self.saved_stack_pointer);
            self.module.start_rewind();
        } else {
            self.module.set_stack_pointer(self.stack_top);
        }
        let result = self.module.run(kind, arg);
        if let Err(trap) = result {
            let reason = self.services().trap_reason.take();
            return Err(match (trap, reason) {
                (Trap::Wasm(_), Some(reason)) => Trap::Host(reason),
                (trap, _) => trap,
            });
        }
        if self.module.async_state() != AsyncState::Unwinding {
            self.suspended = false;
            return Ok(Stop::Returned);
        }
        self.module.stop_unwind();
        match self.services().pending.take() {
            Some(Stop::Yield(reason)) => {
                self.saved_stack_pointer = self.module.stack_pointer();
                self.suspended = true;
                self.module.set_resume_value(0);
                Ok(Stop::Yield(reason))
            }
            Some(Stop::Restart { kind, arg }) => {
                self.suspended = false;
                self.entry = (kind, arg);
                Ok(Stop::Restart { kind, arg })
            }
            _ => Err(Trap::Host("the game unwound without a reason".into())),
        }
    }

    /// Deliver an interrupt callback while the game is suspended.
    pub fn call(&mut self, address: u32) -> Result<(), Trap> {
        let sp = self.module.stack_pointer();
        let result = self.module.call(address);
        self.module.set_stack_pointer(sp);
        result
    }
}

impl<M: GameModule> Drop for Runtime<M> {
    fn drop(&mut self) {
        // SAFETY: created by Box::into_raw in `new`, dropped once.
        drop(unsafe { Box::from_raw(self.services) });
    }
}
