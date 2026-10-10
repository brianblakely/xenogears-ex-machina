//! The console's devices as host services. Each answers the imports named
//! `<device>_*` (the port declares them `xem_host_<device>_*`), may schedule
//! events on the virtual clock and raises interrupts for the port's handler.

use crate::memory::GameMemory;
use crate::module::Action;

pub mod card;
pub mod cd;
pub mod gpu;
pub mod spu;

/// What a device needs while it answers an import or runs an event.
pub struct Context<'a> {
    pub memory: &'a mut dyn GameMemory,
    /// The clock's current cycle.
    pub now: u64,
    /// Interrupts to deliver (irq, detail) at the game's next wait.
    pub raised: &'a mut Vec<(u32, u32)>,
    /// Why an import trapped.
    pub trap_reason: &'a mut Option<String>,
}

pub trait Device {
    /// Answer `<device>_<op>`; `op` is the name without the prefix.
    fn import(&mut self, op: &str, args: &[u32], context: &mut Context<'_>) -> Action;
    /// The cycle of the device's next scheduled event.
    fn next_event(&self) -> Option<u64> {
        None
    }
    /// Run the events due at `context.now`.
    fn run_events(&mut self, _context: &mut Context<'_>) {}
    /// The clock moved to `context.now` (e.g. generate audio up to it).
    fn advance(&mut self, _context: &mut Context<'_>) {}
    /// Game code ran (a step or an interrupt): pick up what it wrote to
    /// memory-mapped registers.
    fn sync(&mut self, _context: &mut Context<'_>) {}
}

/// An import the device does not know.
pub fn unknown(device: &str, op: &str, context: &mut Context<'_>) -> Action {
    *context.trap_reason = Some(format!("unknown import xem.{device}_{op}"));
    Action::Trap
}
