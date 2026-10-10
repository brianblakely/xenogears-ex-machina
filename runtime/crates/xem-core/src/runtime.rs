//! The run loop: start, suspend, resume and restart the game, and advance the
//! virtual clock at its waits.

use crate::clock::{Clock, Tick};
use crate::devices::{self, Context, Device};
use crate::memory::GameMemory;
use crate::module::{Action, AsyncState, GameModule, Import, ImportHandler, Trap};
use crate::pad::Pads;

/// Where `xem_run` starts (port/include/xem/port.h).
pub const RESTART_BOOT: u32 = 0;
pub const RESTART_DISPATCH: u32 = 1;

/// The fibers (port/include/xem/port.h): the game, whose bottom frame is
/// `xem_run`, and the arena task, whose bottom frame is `xem_task_run`.
pub const FIBER_GAME: u32 = 0;
pub const FIBER_TASK: u32 = 1;

/// Interrupt sources (port/include/xem/port.h, the PS1's I_STAT bits).
pub const IRQ_VBLANK: u32 = 0;
pub const IRQ_DMA: u32 = 3;
pub const IRQ_RCNT0: u32 = 4;

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

/// What the import that unwound the game asked for.
#[derive(Debug, Clone, PartialEq, Eq)]
enum Pending {
    Stop(Stop),
    /// Suspend the running fiber and continue `fiber`, afresh on the shadow
    /// stack ending at `stack_top` when that is nonzero.
    Switch { fiber: u32, stack_top: u32 },
}

/// The services the imports reach: the console's devices and the clock.
#[derive(Default)]
pub struct Services {
    pending: Option<Pending>,
    pub clock: Clock,
    pub pads: Pads,
    pub presentation: crate::presentation::Presentation,
    pub gpu: devices::gpu::Gpu,
    pub cd: devices::cd::Cd,
    pub spu: devices::spu::Spu,
    pub card: devices::card::Card,
    pub missing: Vec<u32>,
    pub log: Vec<String>,
    pub trap_reason: Option<String>,
    /// Names of the trapping stubs, by number (build/game/stubs.txt).
    pub stub_names: Vec<String>,
    /// Interrupts raised by device services, delivered at the next wait.
    pub raised: Vec<(u32, u32)>,
}

impl Services {
    fn devices(&mut self) -> [(&'static str, &mut dyn Device); 4] {
        [("gpu_", &mut self.gpu), ("cd_", &mut self.cd), ("spu_", &mut self.spu), ("card_", &mut self.card)]
    }

    /// The earliest scheduled device event.
    pub fn next_device_event(&mut self) -> Option<u64> {
        self.devices().into_iter().filter_map(|(_, d)| d.next_event()).min()
    }

    /// Call `f` on every device with a context.
    fn each_device(&mut self, memory: &mut dyn GameMemory, mut f: impl FnMut(&mut dyn Device, &mut Context<'_>)) {
        let now = self.clock.now;
        let mut raised = std::mem::take(&mut self.raised);
        let mut reason = self.trap_reason.take();
        for (_, device) in self.devices() {
            f(device, &mut Context { memory, now, raised: &mut raised, trap_reason: &mut reason });
        }
        self.raised = raised;
        self.trap_reason = reason;
    }

    /// Run the device events due now.
    fn run_device_events(&mut self, memory: &mut dyn GameMemory) {
        let now = self.clock.now;
        let mut raised = std::mem::take(&mut self.raised);
        let mut reason = self.trap_reason.take();
        for (_, device) in self.devices() {
            if device.next_event().is_some_and(|at| at <= now) {
                device.run_events(&mut Context { memory, now, raised: &mut raised, trap_reason: &mut reason });
            }
        }
        self.raised = raised;
        self.trap_reason = reason;
    }

    fn stub_name(&self, id: u32) -> String {
        if id >= 0x10000 {
            return format!("unmapped inline assembly #{}", id - 0x10000);
        }
        self.stub_names.get(id as usize).cloned().unwrap_or_else(|| format!("stub #{id}"))
    }
}

impl ImportHandler for Services {
    fn import(&mut self, import: Import<'_>, memory: &mut dyn GameMemory) -> Action {
        let arg = |i: usize| import.args.get(i).copied().unwrap_or(0);
        match import.name {
            "yield" => {
                self.pending = Some(Pending::Stop(Stop::Yield(YieldReason::from_raw(arg(0)))));
                Action::Unwind
            }
            "restart" => {
                self.pending = Some(Pending::Stop(Stop::Restart { kind: arg(0), arg: arg(1) }));
                Action::Unwind
            }
            "task_switch" => {
                self.pending = Some(Pending::Switch { fiber: arg(0), stack_top: arg(1) });
                Action::Unwind
            }
            "rcnt_set" => {
                self.clock.set_counter(arg(0) as usize % 3, arg(1), arg(2));
                Action::Return(0)
            }
            "rcnt_start" => {
                self.clock.start_counter(arg(0) as usize % 3);
                Action::Return(0)
            }
            "rcnt_stop" => {
                self.clock.stop_counter(arg(0) as usize % 3);
                Action::Return(0)
            }
            "rcnt_read" => Action::Return(self.clock.read_counter(arg(0) as usize % 3)),
            "pad_read" => match self.pads.write_receive_buffer(arg(0), arg(1), arg(2), memory) {
                Ok(()) => Action::Return(0),
                Err(error) => {
                    self.trap_reason = Some(error.to_string());
                    Action::Trap
                }
            },
            "present" => {
                if self.presentation.enabled {
                    let mut bytes = vec![0; arg(2).min(0x1000) as usize];
                    if memory.read(arg(1), &mut bytes).is_err() {
                        self.trap_reason = Some("presentation record outside memory".into());
                        return Action::Trap;
                    }
                    self.presentation.record(arg(0), bytes);
                }
                Action::Return(0)
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
                let now = self.clock.now;
                let mut raised = std::mem::take(&mut self.raised);
                let mut reason = self.trap_reason.take();
                let mut action = None;
                for (prefix, device) in self.devices() {
                    if let Some(op) = other.strip_prefix(prefix) {
                        let mut context = Context { memory, now, raised: &mut raised, trap_reason: &mut reason };
                        action = Some(device.import(op, import.args, &mut context));
                        break;
                    }
                }
                self.raised = raised;
                self.trap_reason = reason;
                action.unwrap_or_else(|| {
                    self.trap_reason = Some(format!("unknown import xem.{other}"));
                    Action::Trap
                })
            }
        }
    }
}

/// What a frame did.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct FrameReport {
    pub steps: u64,
    pub restarts: Vec<(u32, u32)>,
    pub interrupts: u64,
}

/// A fiber's state between steps: where it starts, or where it suspended.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Fiber {
    /// Not started: it starts on the shadow stack ending at `stack_top`.
    Fresh { stack_top: u32 },
    /// Unwound into the save area `area` with the shadow stack at `stack_pointer`.
    Suspended { stack_pointer: u32, area: u32 },
}

/// The game module with its services.
pub struct Runtime<M: GameModule> {
    module: M,
    services: *mut Services,
    entry: (u32, u32),
    stack_top: u32,
    /// The game fiber and the arena task fiber (`FIBER_GAME`, `FIBER_TASK`).
    fibers: [Option<Fiber>; 2],
    current: u32,
}

impl<M: GameModule> Runtime<M> {
    pub fn new(mut module: M) -> Self {
        let services = Box::into_raw(Box::<Services>::default());
        // SAFETY: the box lives until Drop; the runtime only touches it between
        // export calls.
        unsafe { module.set_import_handler(services as *mut dyn ImportHandler) };
        let stack_top = module.stack_pointer();
        Runtime {
            module,
            services,
            entry: (RESTART_BOOT, 0),
            stack_top,
            fibers: [Some(Fiber::Fresh { stack_top }), None],
            current: FIBER_GAME,
        }
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

    fn host_trap(&mut self, trap: Trap) -> Trap {
        match (trap, self.services().trap_reason.take()) {
            (Trap::Wasm(_), Some(reason)) => Trap::Host(reason),
            (trap, _) => trap,
        }
    }

    /// Run the game until it waits, restarts or traps. A switch between the
    /// game and the arena task fiber continues the other fiber at once.
    pub fn step(&mut self) -> Result<Stop, Trap> {
        loop {
            let fiber = self.current;
            match self.fibers[fiber as usize] {
                Some(Fiber::Suspended { stack_pointer, area }) => {
                    self.module.set_stack_pointer(stack_pointer);
                    self.module.start_rewind(area);
                }
                Some(Fiber::Fresh { stack_top }) => self.module.set_stack_pointer(stack_top),
                None => return Err(Trap::Host(format!("fiber {fiber} has no stack to run on"))),
            }
            let result = if fiber == FIBER_GAME {
                let (kind, arg) = self.entry;
                self.module.run(kind, arg)
            } else {
                self.module.run_task()
            };
            self.sync_devices();
            if let Err(trap) = result {
                return Err(self.host_trap(trap));
            }
            if self.module.async_state() != AsyncState::Unwinding {
                if fiber == FIBER_GAME {
                    self.fibers[0] = Some(Fiber::Fresh { stack_top: self.stack_top });
                    return Ok(Stop::Returned);
                }
                return Err(Trap::Host("the arena task returned from its entry, which the original re-enters".into()));
            }
            self.module.stop_unwind();
            let suspended = Fiber::Suspended { stack_pointer: self.module.stack_pointer(), area: self.module.unwind_area() };
            match self.services().pending.take() {
                Some(Pending::Stop(Stop::Yield(reason))) => {
                    self.fibers[fiber as usize] = Some(suspended);
                    self.module.set_resume_value(0);
                    return Ok(Stop::Yield(reason));
                }
                Some(Pending::Stop(Stop::Restart { kind, arg })) => {
                    self.fibers = [Some(Fiber::Fresh { stack_top: self.stack_top }), None];
                    self.current = FIBER_GAME;
                    self.entry = (kind, arg);
                    return Ok(Stop::Restart { kind, arg });
                }
                Some(Pending::Switch { fiber: target, stack_top }) => {
                    if target as usize >= self.fibers.len() || target == fiber {
                        return Err(Trap::Host(format!("fiber {fiber} switched to fiber {target}")));
                    }
                    self.fibers[fiber as usize] = Some(suspended);
                    if stack_top != 0 {
                        self.fibers[target as usize] = Some(Fiber::Fresh { stack_top });
                    } else if !matches!(self.fibers[target as usize], Some(Fiber::Suspended { .. })) {
                        return Err(Trap::Host(format!("fiber {fiber} switched to fiber {target}, which is not suspended")));
                    }
                    self.current = target;
                    self.module.set_resume_value(0);
                }
                _ => return Err(Trap::Host("the game unwound without a reason".into())),
            }
        }
    }

    /// Abandon the game stack and continue with `xem_run(kind, arg)` at the
    /// next step (as the game's own restarts do).
    pub fn restart(&mut self, kind: u32, arg: u32) {
        self.entry = (kind, arg);
        self.fibers = [Some(Fiber::Fresh { stack_top: self.stack_top }), None];
        self.current = 0;
    }

    /// Deliver an interrupt while the game is suspended, on the stack below
    /// the suspended frames.
    pub fn interrupt(&mut self, irq: u32, detail: u32) -> Result<(), Trap> {
        let sp = self.module.stack_pointer();
        let result = self.module.interrupt(irq, detail);
        self.module.set_stack_pointer(sp);
        self.sync_devices();
        result.map_err(|trap| self.host_trap(trap))
    }

    fn sync_devices(&mut self) {
        // SAFETY: no export runs here; services and module memory are disjoint.
        let services = unsafe { &mut *self.services };
        services.each_device(self.module.memory(), |device, context| device.sync(context));
    }

    /// Let the devices catch up with the clock (after it moved).
    fn advance_devices(&mut self) {
        // SAFETY: as in sync_devices.
        let services = unsafe { &mut *self.services };
        services.each_device(self.module.memory(), |device, context| device.advance(context));
    }

    /// Advance the clock to `limit` (or until the first interrupt when
    /// `first_only`), delivering interrupts in time order.
    fn advance(&mut self, limit: u64, first_only: bool, report: &mut FrameReport) -> Result<bool, Trap> {
        let mut delivered = false;
        loop {
            let raised = std::mem::take(&mut self.services().raised);
            for (irq, detail) in raised {
                self.interrupt(irq, detail)?;
                report.interrupts += 1;
                delivered = true;
            }
            if delivered && first_only {
                return Ok(true);
            }
            // A device event before the clock's next interrupt runs first.
            let device_at = self.services().next_device_event();
            if let Some(at) = device_at.filter(|&at| at <= limit) {
                let services = unsafe { &mut *self.services };
                if at < services.clock.peek().0 {
                    services.clock.now = services.clock.now.max(at);
                    self.advance_devices();
                    let services = unsafe { &mut *self.services };
                    services.run_device_events(self.module.memory());
                    continue;
                }
            }
            let Some(tick) = self.services().clock.next_tick(limit) else {
                return Ok(delivered);
            };
            self.advance_devices();
            match tick {
                Tick::VBlank => {
                    self.services().presentation.end_frame();
                    self.interrupt(IRQ_VBLANK, 0)?
                }
                Tick::RootCounter(n) => self.interrupt(IRQ_RCNT0 + n as u32, 0)?,
            }
            report.interrupts += 1;
            delivered = true;
            if first_only || tick == Tick::VBlank {
                return Ok(true);
            }
        }
    }

    /// Run the game until the clock passes the next vertical blank: each wait
    /// advances the clock (a frame wait to the blank, a poll to the next
    /// interrupt), delivering interrupts in time order.
    pub fn run_frame(&mut self) -> Result<FrameReport, Trap> {
        let frame_end = self.services().clock.next_vblank();
        let mut report = FrameReport::default();
        loop {
            report.steps += 1;
            match self.step()? {
                Stop::Yield(YieldReason::VSync) | Stop::Yield(YieldReason::Poll) | Stop::Yield(YieldReason::Other(_)) => {
                    self.advance(frame_end, true, &mut report)?;
                }
                Stop::Yield(YieldReason::DrawSync) => {
                    // Drawing ends at once; deliver what it raised.
                    let now = self.services().clock.now;
                    self.advance(now, false, &mut report)?;
                }
                Stop::Restart { kind, arg } => report.restarts.push((kind, arg)),
                Stop::Returned => return Err(Trap::Host("xem_run returned".into())),
            }
            if self.services().clock.now >= frame_end {
                return Ok(report);
            }
        }
    }
}

impl<M: GameModule> Runtime<M> {
    /// The whole runtime at a wait (between steps), as bytes.
    pub fn snapshot(&mut self) -> Result<Vec<u8>, crate::snapshot::SnapshotError> {
        use crate::snapshot::*;
        let mut w = Writer::default();
        write_header(&mut w);
        w.u32(self.entry.0);
        w.u32(self.entry.1);
        w.u32(self.current);
        for fiber in &self.fibers {
            match fiber {
                None => w.u32(0),
                Some(Fiber::Fresh { stack_top }) => {
                    w.u32(1);
                    w.u32(*stack_top);
                }
                Some(Fiber::Suspended { stack_pointer, area }) => {
                    w.u32(2);
                    w.u32(*stack_pointer);
                    w.u32(*area);
                }
            }
        }
        w.u32(self.module.stack_pointer());
        let globals = self.module.globals();
        w.u32(globals.len() as u32);
        for g in globals {
            w.u32(g);
        }
        write_memory(&mut w, self.module.memory())?;
        let services = unsafe { &mut *self.services };
        write_clock(&mut w, &services.clock);
        write_pads(&mut w, &services.pads);
        w.u32(services.raised.len() as u32);
        for &(irq, detail) in &services.raised {
            w.u32(irq);
            w.u32(detail);
        }
        for (_, device) in services.devices() {
            w.bytes(&device.save());
        }
        Ok(w.0)
    }

    /// Restore a snapshot taken by `snapshot` from the same game module.
    pub fn restore(&mut self, data: &[u8]) -> Result<(), crate::snapshot::SnapshotError> {
        use crate::snapshot::*;
        let mut r = Reader::new(data);
        read_header(&mut r)?;
        let entry = (r.u32()?, r.u32()?);
        let current = r.u32()?;
        let mut fibers = [None, None];
        for fiber in fibers.iter_mut() {
            *fiber = match r.u32()? {
                0 => None,
                1 => Some(Fiber::Fresh { stack_top: r.u32()? }),
                2 => Some(Fiber::Suspended { stack_pointer: r.u32()?, area: r.u32()? }),
                _ => return Err(SnapshotError::Format("fiber")),
            };
        }
        let stack_pointer = r.u32()?;
        let count = r.u32()? as usize;
        let mut globals = Vec::with_capacity(count);
        for _ in 0..count {
            globals.push(r.u32()?);
        }
        read_memory(&mut r, self.module.memory())?;
        let clock = read_clock(&mut r)?;
        let pads = read_pads(&mut r)?;
        let raised_count = r.u32()? as usize;
        let mut raised = Vec::with_capacity(raised_count);
        for _ in 0..raised_count {
            raised.push((r.u32()?, r.u32()?));
        }
        let services = unsafe { &mut *self.services };
        for (_, device) in services.devices() {
            device.load(r.bytes()?).map_err(SnapshotError::Device)?;
        }
        if !r.done() {
            return Err(SnapshotError::Format("trailing bytes"));
        }
        services.clock = clock;
        services.pads = pads;
        services.raised = raised;
        services.pending = None;
        self.entry = entry;
        self.current = current;
        self.fibers = fibers;
        self.module.set_stack_pointer(stack_pointer);
        self.module.set_globals(&globals);
        Ok(())
    }
}

impl<M: GameModule> Drop for Runtime<M> {
    fn drop(&mut self) {
        // SAFETY: created by Box::into_raw in `new`, dropped once.
        drop(unsafe { Box::from_raw(self.services) });
    }
}
