//! The runtime's fiber bookkeeping (the arena task coroutine, docs/runtime.md
//! "Suspension") against a scripted stand-in for the game module that keeps
//! asyncify's contract: an unwind fills the running fiber's save area (the
//! port's `xem_unwind_area`), a rewind must come with the same area and
//! shadow stack pointer and resumes after the import that unwound.

use std::collections::HashMap;

use xem_core::module::{Action, AsyncState, GameModule, Import, ImportHandler, Trap};
use xem_core::{GameMemory, Runtime, SliceMemory, Stop, YieldReason};

const STACK_TOP: u32 = 0x10_0000;
const TASK_STACK_TOP: u32 = 0x20_0000;
/// The port's save areas of the game and the task fiber.
const AREAS: [u32; 2] = [0x3000, 0x4000];

/// Game memory the devices synchronise with: nothing in these tests reads it.
struct VecMemory(Vec<u8>);

impl GameMemory for VecMemory {
    fn read(&self, address: u32, out: &mut [u8]) -> Result<(), xem_core::memory::OutOfBounds> {
        SliceMemory(&mut self.0.clone()).read(address, out)
    }
    fn write(&mut self, address: u32, data: &[u8]) -> Result<(), xem_core::memory::OutOfBounds> {
        SliceMemory(&mut self.0).write(address, data)
    }
}

#[derive(Debug, Clone, Copy)]
enum Op {
    Yield(u32),
    Switch(u32, u32),
    Restart(u32, u32),
    /// Return from the fiber's bottom frame.
    Return,
}

/// Where an unwound fiber resumes: its script position and shadow stack pointer.
#[derive(Debug, Clone, Copy)]
struct Saved {
    fiber: usize,
    pc: usize,
    stack_pointer: u32,
}

struct Fake {
    handler: Option<*mut dyn ImportHandler>,
    memory: VecMemory,
    scripts: [Vec<Op>; 2],
    /// The port's xem_fiber: the running fiber, or the one last unwound.
    port_fiber: usize,
    state: AsyncState,
    rewind_area: Option<u32>,
    last_area: u32,
    saved: HashMap<u32, Saved>,
    stack_pointer: u32,
    resume_value: u32,
    log: Vec<String>,
}

impl Fake {
    fn new(game: Vec<Op>, task: Vec<Op>) -> Self {
        Fake {
            handler: None,
            memory: VecMemory(vec![0; 16]),
            scripts: [game, task],
            port_fiber: 0,
            state: AsyncState::Normal,
            rewind_area: None,
            last_area: 0,
            saved: HashMap::new(),
            stack_pointer: STACK_TOP,
            resume_value: 0,
            log: Vec::new(),
        }
    }

    /// Run fiber `fiber` from its bottom frame, fresh or rewinding.
    fn enter(&mut self, fiber: usize) -> Result<(), Trap> {
        let mut pc = match self.rewind_area.take() {
            Some(area) => {
                let saved = self.saved.remove(&area).expect("rewind of an area no unwind filled");
                assert_eq!(saved.fiber, fiber, "fiber {fiber} rewound with fiber {}'s area", saved.fiber);
                assert_eq!(self.stack_pointer, saved.stack_pointer, "rewind on another shadow stack");
                assert_eq!(self.resume_value, 0);
                self.state = AsyncState::Normal;
                self.log.push(format!("rewind {fiber} sp={:#x}", self.stack_pointer));
                // The code after the import: a switch's caller marks its fiber running.
                if let Op::Switch(..) = self.scripts[fiber][saved.pc] {
                    self.port_fiber = fiber;
                }
                saved.pc + 1
            }
            None => {
                // xem_run resets the port's fibers; xem_task_run marks the task running.
                self.port_fiber = fiber;
                self.log.push(format!("start {fiber} sp={:#x}", self.stack_pointer));
                0
            }
        };
        loop {
            let op = self.scripts[fiber][pc];
            let (name, args) = match op {
                Op::Yield(reason) => ("yield", vec![reason]),
                Op::Switch(target, top) => ("task_switch", vec![target, top]),
                Op::Restart(kind, arg) => ("restart", vec![kind, arg]),
                Op::Return => {
                    self.log.push(format!("return {fiber}"));
                    return Ok(());
                }
            };
            // A deeper frame: the shadow stack pointer an unwind leaves.
            self.stack_pointer -= 0x10;
            let handler = self.handler.unwrap();
            let action = unsafe { (*handler).import(Import { name, args: &args }, &mut self.memory) };
            match action {
                Action::Unwind => {
                    self.last_area = AREAS[self.port_fiber];
                    self.saved.insert(self.last_area, Saved { fiber, pc, stack_pointer: self.stack_pointer });
                    self.state = AsyncState::Unwinding;
                    return Ok(());
                }
                Action::Return(_) => {
                    self.stack_pointer += 0x10;
                    pc += 1;
                }
                Action::Trap => return Err(Trap::Wasm("import trapped".into())),
            }
        }
    }
}

impl GameModule for Fake {
    unsafe fn set_import_handler(&mut self, handler: *mut dyn ImportHandler) {
        self.handler = Some(handler);
    }
    fn set_resume_value(&mut self, value: u32) {
        self.resume_value = value;
    }
    fn run(&mut self, _kind: u32, _arg: u32) -> Result<(), Trap> {
        self.enter(0)
    }
    fn run_task(&mut self) -> Result<(), Trap> {
        self.enter(1)
    }
    fn call(&mut self, _address: u32) -> Result<(), Trap> {
        Ok(())
    }
    fn interrupt(&mut self, _irq: u32, _detail: u32) -> Result<(), Trap> {
        Ok(())
    }
    fn async_state(&mut self) -> AsyncState {
        self.state
    }
    fn stop_unwind(&mut self) {
        assert_eq!(self.state, AsyncState::Unwinding);
        self.state = AsyncState::Normal;
    }
    fn unwind_area(&mut self) -> u32 {
        self.last_area
    }
    fn start_rewind(&mut self, area: u32) {
        self.state = AsyncState::Rewinding;
        self.rewind_area = Some(area);
    }
    fn stack_pointer(&mut self) -> u32 {
        self.stack_pointer
    }
    fn set_stack_pointer(&mut self, value: u32) {
        self.stack_pointer = value;
    }
    fn memory(&mut self) -> &mut dyn GameMemory {
        &mut self.memory
    }
    fn globals(&mut self) -> Vec<u32> {
        Vec::new()
    }
    fn set_globals(&mut self, _values: &[u32]) {}
}

const VSYNC: u32 = 1;
const POLL: u32 = 3;

#[test]
fn resume_and_yield_switch_fibers_inside_one_step() {
    let game = vec![
        Op::Yield(VSYNC),
        Op::Switch(1, TASK_STACK_TOP), // arena_task_create's first resume
        Op::Yield(VSYNC),
        Op::Switch(1, 0), // the frame loop's resume
        Op::Restart(1, 7),
    ];
    let task = vec![
        Op::Yield(POLL), // a disc poll inside the task
        Op::Switch(0, 0),
        Op::Yield(VSYNC),
        Op::Return,
    ];
    let mut runtime = Runtime::new(Fake::new(game, task));
    assert_eq!(runtime.step(), Ok(Stop::Yield(YieldReason::VSync)));
    // The game resumes the new task, which polls: the poll is the step's stop.
    assert_eq!(runtime.step(), Ok(Stop::Yield(YieldReason::Poll)));
    // The task yields; the game goes on to its frame wait.
    assert_eq!(runtime.step(), Ok(Stop::Yield(YieldReason::VSync)));
    // The game resumes the suspended task, which waits for a frame itself.
    assert_eq!(runtime.step(), Ok(Stop::Yield(YieldReason::VSync)));
    let log = runtime.module().log.clone();
    assert_eq!(
        log,
        [
            "start 0 sp=0x100000",
            "rewind 0 sp=0xffff0",
            "start 1 sp=0x200000",
            "rewind 1 sp=0x1ffff0",
            "rewind 0 sp=0xfffe0",
            "rewind 0 sp=0xfffd0",
            "rewind 1 sp=0x1fffe0",
        ]
    );
}

#[test]
fn restart_from_the_task_abandons_both_fibers() {
    let game = vec![Op::Switch(1, TASK_STACK_TOP), Op::Return];
    let task = vec![Op::Restart(1, 5)];
    let mut runtime = Runtime::new(Fake::new(game, task));
    assert_eq!(runtime.step(), Ok(Stop::Restart { kind: 1, arg: 5 }));
    // The game starts again on its empty stack, not where it suspended.
    assert_eq!(runtime.step(), Ok(Stop::Restart { kind: 1, arg: 5 }));
    let log = runtime.module().log.clone();
    assert_eq!(log, ["start 0 sp=0x100000", "start 1 sp=0x200000", "start 0 sp=0x100000", "start 1 sp=0x200000"]);
}

#[test]
fn a_returning_task_stops_the_game() {
    let game = vec![Op::Switch(1, TASK_STACK_TOP)];
    let task = vec![Op::Return];
    let mut runtime = Runtime::new(Fake::new(game, task));
    let error = runtime.step().unwrap_err();
    assert!(matches!(error, Trap::Host(ref reason) if reason.contains("arena task returned")), "{error}");
}

#[test]
fn a_yield_outside_the_task_stops_the_game() {
    let game = vec![Op::Switch(0, 0)];
    let mut runtime = Runtime::new(Fake::new(game, vec![]));
    let error = runtime.step().unwrap_err();
    assert!(matches!(error, Trap::Host(ref reason) if reason.contains("switched to fiber 0")), "{error}");
    // Resuming a task fiber that never started is refused too.
    let game = vec![Op::Switch(1, 0)];
    let mut runtime = Runtime::new(Fake::new(game, vec![]));
    let error = runtime.step().unwrap_err();
    assert!(matches!(error, Trap::Host(ref reason) if reason.contains("not suspended")), "{error}");
}
