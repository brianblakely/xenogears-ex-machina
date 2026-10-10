//! The command layer over a stand-in module that follows asyncify's protocol:
//! it yields VSync three times, restarts into dispatch mode, polls and then
//! calls a missing port function (as runtime/web/tests/fixture.wat does).

use std::collections::HashMap;

use xem_core::memory::{GameMemory, OutOfBounds, RAM_BASE};
use xem_core::module::{Action, AsyncState, GameModule, Import, ImportHandler, Trap};
use xem_core::session::{Condition, Outcome, Session, Snapshot, StepLog};

/// Sparse game memory in 4 KiB pages.
#[derive(Default, Clone)]
struct Pages(HashMap<u32, Vec<u8>>);

impl GameMemory for Pages {
    fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
        let len = out.len();
        for (i, byte) in out.iter_mut().enumerate() {
            let at = address.checked_add(i as u32).ok_or(OutOfBounds { address, len })?;
            *byte = self.0.get(&(at >> 12)).map_or(0, |page| page[(at & 0xFFF) as usize]);
        }
        Ok(())
    }

    fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
        for (i, byte) in data.iter().enumerate() {
            let at = address.checked_add(i as u32).ok_or(OutOfBounds { address, len: data.len() })?;
            self.0.entry(at >> 12).or_insert_with(|| vec![0; 4096])[(at & 0xFFF) as usize] = *byte;
        }
        Ok(())
    }
}

/// The module's progress lives in its memory (word 0x100: the loop counter),
/// as a real module's stack does, so snapshots capture it.
struct Fake {
    handler: Option<*mut dyn ImportHandler>,
    state: AsyncState,
    memory: Pages,
    sp: u32,
}

const COUNTER: u32 = 0x100;

impl Fake {
    fn import(&mut self, name: &str, args: &[u32]) -> Result<u32, Trap> {
        if self.state == AsyncState::Rewinding {
            self.state = AsyncState::Normal;
            return Ok(0);
        }
        let handler = self.handler.unwrap();
        // SAFETY: the runtime keeps the handler alive while an export runs.
        match unsafe { (*handler).import(Import { name, args }, &mut self.memory) } {
            Action::Return(value) => Ok(value),
            Action::Unwind => {
                self.state = AsyncState::Unwinding;
                Ok(0)
            }
            Action::Trap => Err(Trap::Wasm("unreachable".into())),
        }
    }
}

impl GameModule for Fake {
    unsafe fn set_import_handler(&mut self, handler: *mut dyn ImportHandler) {
        self.handler = Some(handler);
    }
    fn set_resume_value(&mut self, _: u32) {}
    fn run(&mut self, kind: u32, arg: u32) -> Result<(), Trap> {
        loop {
            let i = self.memory.read_u32(COUNTER).unwrap();
            let rewinding = self.state == AsyncState::Rewinding;
            if !rewinding {
                if kind == 0 && i == 3 {
                    self.memory.write_u32(COUNTER, 0).unwrap();
                    self.import("restart", &[1, arg + 5])?;
                    return Ok(());
                }
                if kind == 1 && i == arg {
                    self.import("missing", &[0])?;
                    return Ok(());
                }
                self.memory.write_u32(RAM_BASE + 4 * kind, i).unwrap();
            }
            self.import("yield", &[if kind == 0 { 1 } else { 3 }])?;
            if self.state == AsyncState::Unwinding {
                return Ok(());
            }
            self.memory.write_u32(COUNTER, i + 1).unwrap();
        }
    }
    fn call(&mut self, _: u32) -> Result<(), Trap> {
        Ok(())
    }
    fn async_state(&mut self) -> AsyncState {
        self.state
    }
    fn stop_unwind(&mut self) {
        self.state = AsyncState::Normal;
    }
    fn start_rewind(&mut self) {
        self.state = AsyncState::Rewinding;
    }
    fn stack_pointer(&mut self) -> u32 {
        self.sp
    }
    fn set_stack_pointer(&mut self, value: u32) {
        self.sp = value;
    }
    fn memory(&mut self) -> &mut dyn GameMemory {
        &mut self.memory
    }
    fn data_end(&mut self) -> u32 {
        0x1000
    }
    fn globals(&mut self) -> Vec<u32> {
        Vec::new()
    }
    fn set_globals(&mut self, _: &[u32]) {}
}

fn session() -> Session<Fake> {
    let fake = Fake { handler: None, state: AsyncState::Normal, memory: Pages::default(), sp: 0x800 };
    Session::new(fake, vec!["FakeStub".into()])
}

#[test]
fn steps_until_conditions_and_halts() {
    let mut session = session();
    let mut log = StepLog::default();
    let first = session.step_n(1, |index, outcome| {
        log.record(index, outcome);
    });
    assert_eq!(first.last, Some(Outcome::Yield { reason: "VSync".into() }));
    let word = session.run_until(&Condition::Word { address: RAM_BASE, value: 2 }, 10, |_, _| {});
    assert_eq!((word.steps, word.met), (2, true));
    let restart = session.run_until(&Condition::Restart, 10, |_, _| {});
    assert_eq!(restart.last, Some(Outcome::Restart { kind: 1, arg: 5 }));
    let bounded = session.run_until(&Condition::Halt, 2, |_, _| {});
    assert_eq!((bounded.steps, bounded.met, bounded.halted), (2, false, false));
    let halt = session.run_until(&Condition::Halt, 100, |index, outcome| {
        log.record(index, outcome);
    });
    assert!(halt.met && halt.halted);
    let status = session.status();
    assert_eq!(status.steps, 4 + 5 + 1);
    assert_eq!(status.missing, ["FakeStub"]);
    assert_eq!(
        status.halted.as_deref(),
        Some("host stopped the game: called FakeStub, which the port does not define")
    );
    assert_eq!(session.step(), None);
    assert_eq!(log.lines[0], "step 0: yield VSync");
    assert_eq!(log.lines.last().unwrap(), "step 9: host stopped the game: called FakeStub, which the port does not define");
}

#[test]
fn snapshots_round_trip_through_bytes_and_restore_after_a_halt() {
    let mut session = session();
    session.load_executable(&{
        let mut exe = vec![0u8; 0x800 + 16];
        exe[..8].copy_from_slice(b"PS-X EXE");
        exe[0x18..0x1C].copy_from_slice(&(RAM_BASE + 0x1000).to_le_bytes());
        exe[0x1C..0x20].copy_from_slice(&16u32.to_le_bytes());
        exe[0x800..].fill(0xAB);
        exe
    })
    .unwrap();
    session.step_n(5, |_, _| {});
    let snapshot = session.snapshot().unwrap();
    let bytes = snapshot.to_bytes();
    assert_eq!(Snapshot::from_bytes(&bytes).unwrap(), snapshot);
    assert!(Snapshot::from_bytes(&bytes[..bytes.len() - 1]).is_err());

    session.run_until(&Condition::Halt, 100, |_, _| {});
    let (end, digest) = (session.status(), session.digest().unwrap());
    session.restore(&Snapshot::from_bytes(&bytes).unwrap()).unwrap();
    assert_eq!(session.status().steps, 5);
    assert_eq!(session.status().halted, None);
    session.run_until(&Condition::Halt, 100, |_, _| {});
    assert_eq!(session.status(), end);
    assert_eq!(session.digest().unwrap(), digest);
    let mut text = [0u8; 4];
    session.runtime().memory().read(RAM_BASE + 0x1000, &mut text).unwrap();
    assert_eq!(text, [0xAB; 4]);
}
