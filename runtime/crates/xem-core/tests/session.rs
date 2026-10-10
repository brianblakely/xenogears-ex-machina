//! The command layer over a stand-in module that follows asyncify's protocol:
//! it waits for three vertical blanks, restarts into dispatch mode, polls and
//! then calls a missing port function (as runtime/web/tests/fixture.wat does).

use std::collections::HashMap;

use xem_core::memory::{GameMemory, OutOfBounds, RAM_BASE};
use xem_core::module::{Action, AsyncState, GameModule, Import, ImportHandler, Trap};
use xem_core::session::{Condition, FrameLog, Outcome, Session};

const PAGE: u32 = 4096;

/// Sparse game memory in 4 KiB pages.
#[derive(Default, Clone)]
struct Pages(HashMap<u32, Vec<u8>>);

impl Pages {
    fn spans(address: u32, len: usize) -> Result<Vec<(u32, usize, usize, usize)>, OutOfBounds> {
        let end = u64::from(address) + len as u64;
        if end > 1 << 32 {
            return Err(OutOfBounds { address, len });
        }
        let mut spans = Vec::new();
        let mut at = u64::from(address);
        while at < end {
            let page = (at / u64::from(PAGE)) as u32;
            let offset = (at % u64::from(PAGE)) as usize;
            let count = (PAGE as usize - offset).min((end - at) as usize);
            spans.push((page, offset, count, (at - u64::from(address)) as usize));
            at += count as u64;
        }
        Ok(spans)
    }
}

impl GameMemory for Pages {
    fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
        for (page, offset, count, at) in Self::spans(address, out.len())? {
            match self.0.get(&page) {
                Some(data) => out[at..at + count].copy_from_slice(&data[offset..offset + count]),
                None => out[at..at + count].fill(0),
            }
        }
        Ok(())
    }

    fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
        for (page, offset, count, at) in Self::spans(address, data.len())? {
            if data[at..at + count].iter().all(|&b| b == 0) && !self.0.contains_key(&page) {
                continue;
            }
            let bytes = self.0.entry(page).or_insert_with(|| vec![0; PAGE as usize]);
            bytes[offset..offset + count].copy_from_slice(&data[at..at + count]);
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
            if self.state != AsyncState::Rewinding {
                if kind == 0 && i == 3 {
                    self.memory.write_u32(COUNTER, 0).unwrap();
                    self.import("restart", &[1, 5])?;
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
    fn interrupt(&mut self, _: u32, _: u32) -> Result<(), Trap> {
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
    fn globals(&mut self) -> Vec<u32> {
        Vec::new()
    }
    fn set_globals(&mut self, _: &[u32]) {}
}

fn session() -> Session<Fake> {
    let fake = Fake { handler: None, state: AsyncState::Normal, memory: Pages::default(), sp: 0x800 };
    Session::new(fake, vec!["FakeStub".into()])
}

const STOP: &str = "host stopped the game: called FakeStub, which the port does not define";

#[test]
fn runs_frames_until_conditions_and_halts() {
    let mut session = session();
    let mut log = FrameLog::default();
    let first = session.run_frames(1, |index, outcome| {
        log.record(index, outcome);
    });
    assert_eq!(first.last, Some(Outcome::Frame { steps: 1, interrupts: 1, restarts: vec![] }));
    let word = session.run_until(&Condition::Word { address: RAM_BASE, value: 2 }, 10, |_, _| {});
    assert_eq!((word.frames, word.met), (2, true));
    let restart = session.run_until(&Condition::Restart, 10, |index, outcome| {
        log.record(index, outcome);
    });
    assert_eq!(restart.last, Some(Outcome::Frame { steps: 2, interrupts: 1, restarts: vec![(1, 5)] }));
    let bounded = session.run_until(&Condition::Halt, 2, |_, _| {});
    assert_eq!((bounded.frames, bounded.met, bounded.halted), (2, false, false));
    let halt = session.run_until(&Condition::Halt, 100, |index, outcome| {
        log.record(index, outcome);
    });
    assert!(halt.met && halt.halted);
    let status = session.status();
    // Three blanks, the restart's frame with the first poll, four more polls, the stop.
    assert_eq!(status.frames, 3 + 1 + 4 + 1);
    assert_eq!(status.vblanks, 8);
    assert_eq!(status.missing, ["FakeStub"]);
    assert_eq!(status.halted.as_deref(), Some(STOP));
    assert_eq!(session.frame(), None);
    assert_eq!(log.lines, ["frame 3: restart kind 1 arg 0x5".to_string(), format!("frame 8: {STOP}")]);
}

#[test]
fn snapshots_restore_the_session_after_a_halt() {
    let mut session = session();
    let mut exe = vec![0u8; 0x800 + 16];
    exe[..8].copy_from_slice(b"PS-X EXE");
    exe[0x18..0x1C].copy_from_slice(&(RAM_BASE + 0x1000).to_le_bytes());
    exe[0x1C..0x20].copy_from_slice(&16u32.to_le_bytes());
    exe[0x800..].fill(0xAB);
    session.load_executable(&exe).unwrap();
    session.run_frames(5, |_, _| {});
    let snapshot = session.snapshot().unwrap();
    assert!(session.restore(&snapshot[..snapshot.len() - 1]).is_err());

    session.run_until(&Condition::Halt, 100, |_, _| {});
    let (end, digest) = (session.status(), session.digest().unwrap());
    session.restore(&snapshot).unwrap();
    assert_eq!(session.status().frames, 5);
    assert_eq!(session.status().halted, None);
    session.run_until(&Condition::Halt, 100, |_, _| {});
    assert_eq!(session.status(), end);
    assert_eq!(session.digest().unwrap(), digest);
    let mut text = [0u8; 4];
    session.runtime().memory().read(RAM_BASE + 0x1000, &mut text).unwrap();
    assert_eq!(text, [0xAB; 4]);
}
