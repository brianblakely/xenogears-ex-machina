//! Public tests for restore failure atomicity, queued commands and time controls.
//! This stand-in exercises the real clock/devices; original-game tests live in
//! xem-headless/tests/slice.rs and route.rs. No local inputs are needed:
//!
//!     cargo test -p xem-core --test persistence
use serde_json::json;
use std::{cell::{Cell,RefCell},rc::Rc,time::Duration};
use xem_core::control::Session;
use xem_core::devices::card::{CardStorage, formatted};
use xem_core::memory::OutOfBounds;
use xem_core::module::{AsyncState, ImportHandler};
use xem_core::snapshot::{REGIONS, checked, seal};
use xem_core::{Action, GameMemory, GameModule, Import, Runtime, Trap};

/// The stand-in's save, as the game's (menu_framework.c's save command):
/// with `SAVE_STEP` nonzero each frame makes one step of BIOS file calls
/// through the port's card imports. Step 1 checks the card, lists it, deletes
/// the old save and the temporary file, creates that and opens it for
/// writing; steps 2-33 write its 32 sectors of 0x100 bytes from `SAVE_DATA`;
/// step 34 closes and renames it to the save's name.
const SAVE_STEP: u32 = 0x8000_0200;
const SAVE_FD: u32 = 0x8000_0204;
/// The writes that wrote their sector, those refused, the rename's result.
const SAVE_WRITTEN: u32 = 0x8000_0208;
const SAVE_REFUSED: u32 = 0x8000_020c;
const SAVE_RENAMED: u32 = 0x8000_0210;
const SAVE_DEVICE: u32 = 0x8000_0280;
const SAVE_TEMP: u32 = 0x8000_0300;
const SAVE_NAME: u32 = 0x8000_0320;
const SAVE_ENTRY: u32 = 0x8000_0340;
const SAVE_DATA: u32 = 0x8000_1000;
const SAVE_STEPS: u32 = 34;
/// The stand-in's fibers, as the port switches them: 1 makes the game's next
/// step restart it into the dispatcher (a fresh game fiber), 2 makes it start
/// the arena task, which then waits a frame at a time.
const FIBERS: u32 = 0x8000_0214;
const TASK_STACK_TOP: u32 = 0x100C_0000;
/// With `DRAW_MAP` nonzero each frame runs the stand-in's random draw on that
/// field map as the port's field_encounter_count_down does: it asks the host
/// (xem.random_battles with the map's low 12 bits) and counts the frames the
/// draw ran at `DRAW_RAN`.
const DRAW_MAP: u32 = 0x8000_0400;
const DRAW_RAN: u32 = 0x8000_0404;

/// Card images shared with the test, as a host's storage.
#[derive(Clone)]
struct Storage(Rc<RefCell<[Vec<u8>; 2]>>);
impl CardStorage for Storage {
    fn load(&mut self, slot: usize) -> std::io::Result<Option<Vec<u8>>> { Ok(Some(self.0.borrow()[slot].clone())) }
    fn store(&mut self, slot: usize, image: &[u8]) -> std::io::Result<()> { self.0.borrow_mut()[slot] = image.to_vec(); Ok(()) }
}

/// A small deterministic generator for damaged states.
struct Lcg(u64);
impl Lcg {
    fn next(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        (self.0 >> 33) as u32
    }
    fn below(&mut self, n: usize) -> usize { self.next() as usize % n }
}

struct Memory(Vec<Vec<u8>>);
impl Memory {
    fn locate(&self, address: u32, len: usize) -> Result<(usize, usize), OutOfBounds> {
        REGIONS.iter().enumerate().find_map(|(i, &(start, size))| {
            let offset = address.checked_sub(start)? as usize;
            (offset.checked_add(len)? <= size as usize).then_some((i, offset))
        }).ok_or(OutOfBounds { address, len })
    }
}
impl GameMemory for Memory {
    fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
        let (i, at) = self.locate(address, out.len())?;
        out.copy_from_slice(&self.0[i][at..at + out.len()]); Ok(())
    }
    fn write(&mut self, address: u32, bytes: &[u8]) -> Result<(), OutOfBounds> {
        let (i, at) = self.locate(address, bytes.len())?;
        self.0[i][at..at + bytes.len()].copy_from_slice(bytes); Ok(())
    }
}
struct Counter {
    memory: Memory,
    handler: Option<*mut dyn ImportHandler>,
    state: AsyncState,
    sp: u32,
    /// The module's own globals (none unless a test gives it some).
    globals: Vec<u32>,
}
impl Counter {
    fn new() -> Self {
        Self { memory: Memory(REGIONS.iter().map(|(_, size)| vec![0; *size as usize]).collect()),
            handler: None, state: AsyncState::Normal, sp: 0x1010_0000, globals: Vec::new() }
    }

    /// An import that returns, as the port's calls make it.
    fn import(&mut self, name: &str, args: &[u32]) -> u32 {
        match unsafe { (*self.handler.unwrap()).import(Import { name, args }, &mut self.memory) } {
            Action::Return(value) => value,
            other => panic!("{name}: {other:?}"),
        }
    }

    /// An import that unwinds the module: a wait, a restart, a fiber switch.
    fn unwind(&mut self, name: &str, args: &[u32]) -> Result<(), Trap> {
        let action = unsafe { (*self.handler.unwrap()).import(Import { name, args }, &mut self.memory) };
        assert_eq!(action, Action::Unwind);
        self.state = AsyncState::Unwinding;
        Ok(())
    }

    fn save_step(&mut self, step: u32) {
        let count = |memory: &mut Memory, at: u32| memory.write_u32(at, memory.read_u32(at).unwrap() + 1).unwrap();
        match step {
            1 => {
                self.import("card_start", &[]);
                self.import("card_info", &[0]);
                self.import("card_firstfile", &[SAVE_DEVICE, SAVE_ENTRY]);
                self.import("card_delete", &[SAVE_NAME]);
                self.import("card_delete", &[SAVE_TEMP]);
                let created = self.import("card_open", &[SAVE_TEMP, 1 << 16 | 0x200]);
                self.import("card_close", &[created]);
                let fd = self.import("card_open", &[SAVE_TEMP, 2]);
                for (at, value) in [(SAVE_FD, fd), (SAVE_WRITTEN, 0), (SAVE_REFUSED, 0), (SAVE_RENAMED, 0)] {
                    self.memory.write_u32(at, value).unwrap();
                }
            }
            2..SAVE_STEPS => {
                let fd = self.memory.read_u32(SAVE_FD).unwrap();
                let wrote = self.import("card_write", &[fd, SAVE_DATA + (step - 2) * 0x100, 0x100]);
                count(&mut self.memory, if wrote == 0x100 { SAVE_WRITTEN } else { SAVE_REFUSED });
            }
            _ => {
                let fd = self.memory.read_u32(SAVE_FD).unwrap();
                self.import("card_close", &[fd]);
                let renamed = self.import("card_rename", &[SAVE_TEMP, SAVE_NAME]);
                self.memory.write_u32(SAVE_RENAMED, renamed).unwrap();
            }
        }
        self.memory.write_u32(SAVE_STEP, if step == SAVE_STEPS { 0 } else { step + 1 }).unwrap();
    }
}
impl GameModule for Counter {
    fn game_paused(&mut self, mode: u32) -> bool {
        self.memory.read_u32(0x8000_0010).unwrap() & 0x40 != 0
            || (mode == 4 && self.memory.read_u32(0x8000_0010).unwrap() & 0x00ff_0000 != 0)
    }
    unsafe fn set_import_handler(&mut self, handler: *mut dyn ImportHandler) { self.handler = Some(handler); }
    fn set_resume_value(&mut self, _: u32) {}
    fn run(&mut self, _: u32, _: u32) -> Result<(), Trap> {
        self.state = AsyncState::Normal;
        let count = self.memory.read_u32(0x8000_0000).unwrap();
        self.memory.write_u32(0x8000_0000, count + 1).unwrap();
        let step = self.memory.read_u32(SAVE_STEP).unwrap();
        if step != 0 { self.save_step(step); }
        let map = self.memory.read_u32(DRAW_MAP).unwrap();
        if map != 0 && self.import("random_battles", &[map & 0xFFF]) != 0 {
            let ran = self.memory.read_u32(DRAW_RAN).unwrap();
            self.memory.write_u32(DRAW_RAN, ran + 1).unwrap();
        }
        let fibers = self.memory.read_u32(FIBERS).unwrap();
        self.memory.write_u32(FIBERS, 0).unwrap();
        match fibers {
            1 => self.unwind("restart", &[xem_core::runtime::RESTART_DISPATCH, 0]),
            2 => self.unwind("task_switch", &[1, TASK_STACK_TOP]),
            _ => self.unwind("yield", &[1]),
        }
    }
    fn run_task(&mut self) -> Result<(), Trap> {
        self.state = AsyncState::Normal;
        self.unwind("yield", &[1])
    }
    fn call(&mut self, _: u32) -> Result<(), Trap> { Ok(()) }
    fn interrupt(&mut self, irq: u32, _: u32) -> Result<(), Trap> {
        if irq == xem_core::runtime::IRQ_RCNT0+2 {
            let ticks = self.memory.read_u32(0x8000_0008).unwrap();
            self.memory.write_u32(0x8000_0008,ticks+1).unwrap();
        }
        Ok(())
    }
    fn async_state(&mut self) -> AsyncState { self.state }
    fn stop_unwind(&mut self) { self.state = AsyncState::Normal; }
    fn unwind_area(&mut self) -> u32 { 0x1000_0040 }
    fn start_rewind(&mut self, _: u32) { self.state = AsyncState::Rewinding; }
    fn stack_pointer(&mut self) -> u32 { self.sp }
    fn set_stack_pointer(&mut self, value: u32) { self.sp = value; }
    fn memory(&mut self) -> &mut dyn GameMemory { &mut self.memory }
    fn globals(&mut self) -> Vec<u32> { self.globals.clone() }
    fn set_globals(&mut self, values: &[u32]) { self.globals = values.to_vec(); }
}
fn session() -> Session<Counter> {
    let started = std::time::Instant::now();
    Session::new(Runtime::new(Counter::new()),None,move || started.elapsed())
}
fn timed_session() -> (Session<Counter>,Rc<Cell<Duration>>) {
    let clock = Rc::new(Cell::new(Duration::ZERO));
    let source = clock.clone();
    let mut session = Session::new(Runtime::new(Counter::new()),None,move || source.get());
    session.configure_rewind(xem_core::rewind::MAX_REWIND_BYTES,1,30).unwrap();
    (session,clock)
}
fn frame_at(s: &mut Session<Counter>, clock: &Cell<Duration>, seconds: u64) {
    clock.set(Duration::from_secs(seconds));
    assert!(s.frame().unwrap());
}

#[test]
fn pending_focus_pause_requests_stay_out_of_game_states_and_are_recorded() {
    let mut s = session();
    let saved = s.snapshot_bytes().unwrap();
    s.execute(&json!({"cmd":"record"}));
    s.set_window_focus(false,true);
    assert_eq!(s.snapshot_bytes().unwrap(),saved,"a pending window request is not game state");
    s.set_window_focus(true,true);
    s.set_window_focus(false,false);
    let log = s.execute(&json!({"cmd":"stop_recording"}));
    let focus: Vec<_> = log["controls"].as_array().unwrap().iter()
        .filter(|pair| pair[1]["cmd"] == "focus_pause").map(|pair| pair[1]["requested"].clone()).collect();
    assert_eq!(focus,vec![json!(true),json!(false)],"record both the request and its cancellation");
    let mut replay = log;
    replay["cmd"] = json!("replay");
    assert!(s.execute(&replay).get("error").is_none());
}

#[test]
fn paused_game_frames_neither_add_snapshots_nor_expire_play_history() {
    let (mut s, clock) = timed_session();
    assert!(s.schema.is_none(),"pause detection must not require an inspection schema");
    s.runtime.services().mode = Some(1);
    frame_at(&mut s, &clock, 1);
    let retained = s.rewind.status();
    s.runtime.memory().write(0x8000_0010,&0x40u16.to_le_bytes()).unwrap();
    for second in 2..=40 { frame_at(&mut s, &clock, second); }
    assert_eq!(s.rewind.status(),retained,"pause-screen frames must not consume or age out history");
    s.configure_rewind(xem_core::rewind::MAX_REWIND_BYTES,1,30).unwrap();
    assert_eq!(s.rewind.status(),retained,"configuring history while paused must not add a state");
    s.runtime.memory().write(0x8000_0010,&0u16.to_le_bytes()).unwrap();
    frame_at(&mut s, &clock, 4000);
    assert_eq!(s.rewind.status(),retained,"resume starts a fresh snapshot interval");
    frame_at(&mut s, &clock, 4001);
    assert_eq!(s.rewind.status()["states"],3);
    assert_eq!(s.rewind.status()["oldest_frame"],0);
    assert_eq!(s.rewind.available_seconds(),2.0);

    // Arena overlay memory is reused in other modes: its pause byte only
    // describes a pause while the arena is running.
    s.runtime.memory().write(0x8000_0012,&[1]).unwrap();
    frame_at(&mut s, &clock, 4002);
    assert_eq!(s.rewind.status()["states"],4);
    s.runtime.services().mode = Some(4);
    let retained = s.rewind.status();
    for second in 4003..=4040 { frame_at(&mut s, &clock, second); }
    assert_eq!(s.rewind.status(),retained);
    let paused_state = s.snapshot_bytes().unwrap();
    s.restore_bytes(&paused_state).unwrap();
    assert_eq!(s.rewind.status()["states"],0,"restoring a paused game must not enqueue it");
    s.runtime.memory().write(0x8000_0012,&[0]).unwrap();
    frame_at(&mut s, &clock, 4041);
    assert_eq!(s.rewind.status()["states"],1);
}

#[test]
fn loading_and_rewinding_do_not_roll_back_ordinary_card_saves() {
    use xem_core::devices::card::CARD_SIZE;
    let stored = Rc::new(RefCell::new([formatted(), formatted()]));
    let (mut s, clock) = timed_session();
    s.configure_rewind(0,1,30).unwrap();
    s.runtime.services().card.attach_storage(Box::new(Storage(stored.clone()))).unwrap();
    s.configure_rewind(xem_core::rewind::MAX_REWIND_BYTES, 1, 30).unwrap();
    let state = s.snapshot_bytes().unwrap();
    frame_at(&mut s, &clock, 1);
    let mut saved_card = formatted();
    saved_card[CARD_SIZE - 1] = 0x45;
    s.runtime.services().card.import_image(1, saved_card.clone()).unwrap();
    assert!(s.rewind_step().unwrap());
    assert_eq!(s.runtime.services().card.image(1).unwrap(), saved_card);
    assert_eq!(stored.borrow()[1], saved_card);
    s.restore_bytes(&state).unwrap();
    s.runtime.services().card.retry_writes().unwrap();
    assert_eq!(s.runtime.services().card.image(1).unwrap(), saved_card);
    assert_eq!(stored.borrow()[1], saved_card);
}

#[test]
fn scheduled_file_operations_are_rejected_before_any_command_is_queued() {
    let mut s = session();
    for operation in ["state_save", "state_load", "card_export", "card_import"] {
        let reply = s.execute(&json!({"cmd":"schedule", "commands":[[1,{"cmd":"fast_forward"}],[2,{"cmd":operation,"path":"/tmp/user-file"}]]}));
        assert!(reply.get("error").is_some(), "{reply}");
        assert!(s.frame().unwrap());
        assert!(s.frame().unwrap());
        assert!(!s.fast_forward, "a rejected log must not queue its valid prefix");
    }
}

#[test]
fn headless_sessions_have_no_automatic_wall_clock_history() {
    let mut s = session();
    assert_eq!(s.rewind.budget,0);
    assert!(s.execute(&json!({"cmd":"frames","count":120})).get("error").is_none());
    assert_eq!(s.rewind.status()["states"],0);
}

#[test]
fn fixed_fast_forward_and_its_recording_are_independent_of_host_batch_size() {
    fn start() -> Session<Counter> {
        let mut s = session();
        s.runtime.services().clock.set_counter(2,0x44e8,xem_core::clock::RCNT_MODE_INTERRUPT);
        s.runtime.services().clock.start_counter(2);
        s
    }
    fn run(s: &mut Session<Counter>, batch: u64) {
        while s.runtime.frames() < 120 {
            let frame = s.runtime.frames();
            if frame == 0 { s.execute(&json!({"cmd":"fast_forward","enabled":true})); }
            if frame == 40 { s.execute(&json!({"cmd":"fast_forward","enabled":false})); }
            if frame == 80 { s.execute(&json!({"cmd":"fast_forward_configure","rate":{"Fixed":5}})); s.execute(&json!({"cmd":"fast_forward","enabled":true})); }
            let count = batch.min(40 - frame%40);
            s.execute(&json!({"cmd":"frames","count":count}));
            s.advance_live_music(xem_core::clock::VBLANK_CYCLES).unwrap();
        }
    }
    let mut a = start();
    let initial = a.snapshot_bytes().unwrap();
    a.execute(&json!({"cmd":"record"}));
    run(&mut a,10);
    let log = a.execute(&json!({"cmd":"stop_recording"}));
    let mut b = start(); run(&mut b,4);
    assert_eq!(a.memory_hash().unwrap(),b.memory_hash().unwrap());
    assert!(a.snapshot_bytes().unwrap() == b.snapshot_bytes().unwrap());
    let mut replayed = start(); replayed.restore_bytes(&initial).unwrap();
    let mut replay = log.clone(); replay["cmd"] = json!("replay");
    assert!(replayed.execute(&replay).get("error").is_none());
    replayed.execute(&json!({"cmd":"frames","count":120}));
    assert!(a.snapshot_bytes().unwrap() == replayed.snapshot_bytes().unwrap());
    assert_eq!(replayed.fast_forward_rate,xem_core::time::FastForwardRate::Fixed(5));
    assert!(replayed.fast_forward);
}

#[test]
fn uncapped_recordings_replay_the_music_clock_at_the_same_game_frames() {
    let mut s = session();
    s.runtime.services().clock.set_counter(2,0x44e8,xem_core::clock::RCNT_MODE_INTERRUPT);
    s.runtime.services().clock.start_counter(2);
    let initial = s.snapshot_bytes().unwrap();
    s.execute(&json!({"cmd":"record"}));
    s.execute(&json!({"cmd":"fast_forward_configure","rate":"Uncapped"}));
    s.execute(&json!({"cmd":"fast_forward","enabled":true}));
    for count in [3,10,4,8] {
        s.execute(&json!({"cmd":"frames","count":count}));
        s.advance_live_music(xem_core::clock::VBLANK_CYCLES).unwrap();
    }
    let mut log = s.execute(&json!({"cmd":"stop_recording"})); log["cmd"] = json!("replay");
    let mut other = session(); other.restore_bytes(&initial).unwrap();
    assert!(other.execute(&log).get("error").is_none());
    assert!(!other.live_music(),"replay must use recorded clock inputs");
    other.execute(&json!({"cmd":"frames","count":25}));
    assert!(s.snapshot_bytes().unwrap() == other.snapshot_bytes().unwrap());
}

#[test]
fn fast_forward_toggles_without_changing_the_saved_rate_and_uncapped_uses_normal_music_time() {
    use xem_core::time::FastForwardRate;
    let mut s = session();
    let reply = s.execute(&json!({"cmd":"fast_forward_configure","rate":{"Fixed":20}}));
    assert_eq!(reply["fast_forward"],false,"{reply}");
    assert_eq!(s.frame_factor(),Some(1));
    assert_eq!(s.runtime.services().clock.audio_speed,1);
    assert_eq!(s.execute(&json!({"cmd":"fast_forward"}))["fast_forward"],true);
    assert_eq!(s.frame_factor(),Some(20));
    let revision = s.timeline_revision;
    assert_eq!(s.execute(&json!({"cmd":"fast_forward","enabled":true}))["fast_forward"],true);
    assert_eq!(s.timeline_revision,revision,"setting the switch on again does not reset audio");
    assert_eq!(s.execute(&json!({"cmd":"fast_forward"}))["fast_forward"],false);
    assert_eq!(s.fast_forward_rate,FastForwardRate::Fixed(20));
    assert_eq!(s.frame_factor(),Some(1));
    assert_eq!(s.runtime.services().clock.audio_speed,1);
    let before = s.snapshot_bytes().unwrap();
    assert!(s.execute(&json!({"cmd":"fast_forward_configure","rate":{"Fixed":17}})).get("error").is_some());
    assert_eq!(s.snapshot_bytes().unwrap(),before);
    assert!(s.execute(&json!({"cmd":"fast_forward_configure","rate":"Uncapped"})).get("error").is_none());
    assert_eq!(s.execute(&json!({"cmd":"fast_forward"}))["fast_forward"],true);
    assert_eq!(s.frame_factor(),None);
    s.runtime.services().clock.set_counter(2,0x44e8,xem_core::clock::RCNT_MODE_INTERRUPT);
    s.runtime.services().clock.start_counter(2);
    let reply = s.execute(&json!({"cmd":"frames","count":60}));
    assert!(reply.get("error").is_none(),"{reply}");
    assert_eq!(s.runtime.frames(),60);
    assert_eq!(s.runtime.services().spu.chip().clock(),0,"game frames do not speed up music");
    for _ in 0..60 { s.execute(&json!({"cmd":"music_advance","cycles":xem_core::clock::VBLANK_CYCLES})); }
    assert_eq!(s.runtime.services().spu.chip().clock(),44100);
    assert_eq!(s.runtime.memory().read_u32(0x8000_0008).unwrap(),240);
    let saved = s.snapshot_bytes().unwrap();
    let mut other = session(); other.restore_bytes(&saved).unwrap();
    assert!(!other.fast_forward);
    assert_eq!(other.fast_forward_rate,FastForwardRate::default());
    assert_eq!(other.snapshot_bytes().unwrap(),saved);
    // Old app/session envelopes are explicitly incompatible with game saves.
    let mut writer = xem_core::snapshot::Writer(b"XEMSTATE\x02".to_vec());
    writer.bytes(br#"{"script":[],"scheduled":[],"recording":null,"stopped":null,"fast_forward":true,"fast_forward_rate":"Uncapped"}"#);
    writer.bytes(&saved);
    assert!(other.restore_bytes(&seal(writer.0)).is_err());
    assert_eq!(other.snapshot_bytes().unwrap(),saved);
}

/// A state's content with its identity record (after the magic and the
/// version) replaced by `identity`.
fn with_identity(content: &[u8], identity: &str) -> Vec<u8> {
    let length = u32::from_le_bytes(content[12..16].try_into().unwrap()) as usize;
    let mut w = xem_core::snapshot::Writer(content[..12].to_vec());
    w.bytes(identity.as_bytes());
    w.0.extend_from_slice(&content[16 + length..]);
    w.0
}

/// A small disc for the stand-in's drive.
fn disc() -> Box<dyn xem_disc::SectorSource> {
    Box::new(xem_disc::MemorySource::from_sectors(std::iter::repeat_n([0; xem_disc::sector::SECTOR_SIZE], 16)))
}

#[test]
fn every_damaged_or_foreign_state_is_refused_by_name_and_changes_nothing() {
    use xem_core::snapshot::{MAX_STATE, VERSION};
    let mut s = session();
    s.execute(&json!({"cmd":"frames", "count":2}));
    let original = s.runtime.snapshot().unwrap();
    let content = checked(&original).unwrap().to_vec();
    s.execute(&json!({"cmd":"frames", "count":3}));
    let current = s.runtime.snapshot().unwrap();
    let changed = |at: usize, bytes: &[u8]| {
        let mut c = content.clone();
        c[at..at + bytes.len()].copy_from_slice(bytes);
        seal(c)
    };
    let mut rng = Lcg(7);
    let garbage: Vec<u8> = (0..4096).map(|_| rng.next() as u8).collect();
    let module = std::any::type_name::<Counter>();
    let identity = |json: String| seal(with_identity(&content, &json));
    let cases: Vec<(&str, Vec<u8>, String)> = vec![
        ("an empty file", vec![], "bad snapshot: state size 0 bytes, outside 32 bytes to 64 MiB".into()),
        ("31 bytes", vec![0; 31], "bad snapshot: state size 31 bytes".into()),
        ("over the size cap", vec![0; MAX_STATE + 1], format!("bad snapshot: state size {} bytes", MAX_STATE + 1)),
        ("a flipped byte", { let mut b = original.clone(); b[80] ^= 1; b }, "bad snapshot: checksum".into()),
        ("a truncated file", original[..original.len() - 1].to_vec(), "bad snapshot: checksum".into()),
        ("garbage", garbage.clone(), "bad snapshot: checksum".into()),
        ("sealed garbage", seal(garbage), "bad snapshot: magic".into()),
        ("another magic", changed(0, b"XEMSNAQ"), "bad snapshot: magic".into()),
        ("version 8", changed(8, &8u32.to_le_bytes()), format!("bad snapshot: format version 8, expected {VERSION}")),
        ("version 10", changed(8, &10u32.to_le_bytes()), format!("bad snapshot: format version 10, expected {VERSION}")),
        ("another game module", identity(r#"{"module":"another build","disc":"","packages":[]}"#.into()),
            format!(r#"incompatible state: the state's game module is "another build", this session's is "{module}""#)),
        ("another disc", identity(format!(r#"{{"module":"{module}","disc":"SLUS-00664+SLUS-00669","packages":[]}}"#)),
            r#"incompatible state: the state's disc is "SLUS-00664+SLUS-00669", this session's is """#.into()),
        ("other packages", identity(format!(r#"{{"module":"{module}","disc":"","packages":["a mod"]}}"#)),
            r#"incompatible state: the state's packages are ["a mod"], this session's are []"#.into()),
        ("an extra identity field", identity(format!(r#"{{"module":"{module}","disc":"","packages":[],"extra":1}}"#)),
            "bad snapshot: identity: unknown field `extra`".into()),
        ("a trailing byte", { let mut c = content.clone(); c.push(7); seal(c) }, "bad snapshot: trailing bytes".into()),
        ("a truncated last device", { let mut c = content.clone(); c.pop(); seal(c) }, "bad snapshot: truncated".into()),
    ];
    // Through the runtime and the session; a refusal cannot undo another's
    // change, so one snapshot after both shows that neither changed the game.
    for (what, bytes, expected) in &cases {
        let error = s.runtime.restore(bytes).unwrap_err().to_string();
        assert!(error.contains(expected.as_str()), "{what}: {error}");
        let (revision, rewind) = (s.timeline_revision, s.rewind.status());
        let error = s.restore_bytes(bytes).unwrap_err();
        assert!(error.contains(expected.as_str()), "{what}: {error}");
        assert_eq!((s.timeline_revision, s.rewind.status()), (revision, rewind), "{what}: the session's timeline is unchanged");
        assert!(s.snapshot_bytes().unwrap() == current, "{what}: the game is unchanged");
    }

    // A state with disc 2 in the drive, in a session that has not got it.
    let mut two = session();
    two.runtime.services().insert_disc(2, disc());
    two.execute(&json!({"cmd":"frames", "count":2}));
    let with_disc_2 = two.snapshot_bytes().unwrap();
    let expected = "snapshot device state: the state has disc 2 in the drive, which is not here";
    let error = s.runtime.restore(&with_disc_2).unwrap_err().to_string();
    assert!(error.contains(expected), "{error}");
    assert!(s.runtime.snapshot().unwrap() == current);
    assert!(s.restore_bytes(&with_disc_2).unwrap_err().contains(expected));
    assert!(s.snapshot_bytes().unwrap() == current);
    // Given disc 2, it restores and goes on as where it was saved.
    s.runtime.services().add_disc(2, disc());
    s.restore_bytes(&with_disc_2).unwrap();
    for session in [&mut s, &mut two] { session.execute(&json!({"cmd":"frames", "count":3})); }
    assert!(s.snapshot_bytes().unwrap() == two.snapshot_bytes().unwrap());
}

#[test]
fn random_damage_never_panics_or_half_restores() {
    // Each restore and each snapshot that checks it go through the regions'
    // 26 MiB and every device: an unoptimised build (CI's dev profile, about
    // 0.3 s a mutation here) runs the first tenth of the mutations.
    let mutations = if cfg!(debug_assertions) { 20 } else { 200 };
    let mut s = session();
    s.execute(&json!({"cmd":"frames", "count":3}));
    let content = checked(&s.runtime.snapshot().unwrap()).unwrap().to_vec();
    s.execute(&json!({"cmd":"frames", "count":2}));
    let mut current = s.runtime.snapshot().unwrap();
    let mut rng = Lcg(0x5eed);
    let (mut refused, mut restored) = (0, 0);
    for _ in 0..mutations {
        let mut damaged = content.clone();
        // Anywhere, or where the structure is dense: the header, identity,
        // fibers and clock first, the card and the SPU's registers last.
        let at = match rng.below(3) {
            0 => rng.below(damaged.len() - 4),
            1 => rng.below(1024),
            _ => damaged.len() - 4 - rng.below(4096),
        };
        match rng.below(4) {
            0 => damaged[at] ^= 1 << rng.below(8),
            1 => damaged[at..at + 4].copy_from_slice(&rng.next().to_le_bytes()),
            2 => { damaged.splice(at..at, rng.next().to_le_bytes()); }
            _ => { damaged.drain(at..at + 4); }
        }
        let damaged = seal(damaged);
        match s.runtime.restore(&damaged) {
            Err(_) => {
                refused += 1;
                assert!(s.runtime.snapshot().unwrap() == current, "a refused state changes nothing");
            }
            Ok(()) => {
                restored += 1;
                // What it accepted is whole: it saves as it was given, or,
                // given in another encoding, restores as itself again.
                let accepted = s.runtime.snapshot().unwrap();
                if accepted != damaged {
                    s.runtime.restore(&accepted).unwrap();
                    assert!(s.runtime.snapshot().unwrap() == accepted, "an accepted state restores as itself");
                }
                current = accepted;
            }
        }
    }
    assert!(refused * 4 > mutations && restored * 10 > mutations, "both outcomes: {refused} refused, {restored} restored");
}

/// A disc of XA audio sectors (stereo, 37800 Hz), as a movie's or a scene's,
/// its left channel unlike its right.
fn xa_disc() -> Box<dyn xem_disc::SectorSource> {
    use xem_disc::sector::{Subheader, build_mode2_sector, submode};
    let audio = Subheader { file: 1, channel: 1, submode: submode::AUDIO | submode::REAL_TIME | submode::FORM2, coding: 0x01 };
    let mut group = [0u8; 128];
    group[..4].copy_from_slice(&[0x0C; 4]);
    group[16..].fill(0x37);
    let adpcm: Vec<u8> = group.iter().copied().cycle().take(0x900).collect();
    Box::new(xem_disc::MemorySource::from_sectors((0..16).map(|lba| build_mode2_sector(lba, audio, &adpcm))))
}

/// The MDEC's input for one 15-bit macroblock of four greys: its command
/// word, then six blocks (Cr and Cb flat, each luma block another DC; each
/// ends at once) padded to 32 words.
fn macroblock() -> Vec<u8> {
    use xem_media::mdec::{Depth, decode_command};
    let mut halfwords: Vec<u16> = [0x0400, 0x0400, 0x0410, 0x0420, 0x0430, 0x0440].iter().flat_map(|&dc| [dc, 0xFE00]).collect();
    halfwords.resize(64, 0xFE00);
    let mut bytes = decode_command(Depth::Bits15, false, false, 32).to_le_bytes().to_vec();
    bytes.extend(halfwords.iter().flat_map(|h| h.to_le_bytes()));
    bytes
}

/// The content of `s`'s state, which restores as itself.
fn whole_state(s: &mut Session<Counter>) -> Vec<u8> {
    let state = s.runtime.snapshot().unwrap();
    s.runtime.restore(&state).unwrap();
    assert!(s.runtime.snapshot().unwrap() == state, "the state restores as itself");
    checked(&state).unwrap().to_vec()
}

#[test]
fn format_fingerprint_matches_version() {
    use xem_core::devices::cd::{com, intr};
    use xem_core::snapshot::{FORMAT_FINGERPRINT, VERSION, digest};
    const MOVIE: u32 = 0x8000_3000;
    // Three deterministic stand-in states that between them hold each part
    // of the format and each variant it writes, each restoring as itself.
    let counter = Counter { globals: vec![0x1234_5678, 7], ..Counter::new() };
    let mut s = Session::new(Runtime::new(counter), None, || Duration::ZERO);
    let mut states = Vec::new();

    // A save half written (an open file, a listing and a check), a read
    // under way and the story's disc change waiting, a macroblock's output
    // on its way, a sound transfer and CD audio queued for the SPU, a
    // partial GPU command, game memory in each region and the module's
    // globals, a sound timer, pads, raised interrupts, a dispatch write, the
    // game's fiber fresh after a restart.
    s.runtime.services().card.attach_storage(Box::new(Storage(Rc::new(RefCell::new([formatted(), formatted()]))))).unwrap();
    s.runtime.services().insert_disc(1, disc());
    s.runtime.services().add_disc(2, xa_disc());
    s.runtime.services().clock.set_counter(2, 0x44e8, xem_core::clock::RCNT_MODE_INTERRUPT);
    s.runtime.services().clock.start_counter(2);
    for (at, value) in [(0x0000_0400, 1), (0x1000_0100, 2), (0x1F80_0010, 3), (0x1F80_1D80, 0x3FFF_3FFF), (0x8000_0040, 0x1234_5678)] {
        s.runtime.memory().write_u32(at, value).unwrap();
    }
    s.set_pad(0, xem_core::pad::button::CROSS | xem_core::pad::button::START);
    let gpu = s.runtime.services().gpu.device_mut();
    gpu.write_gp1(0x0800_0001);
    gpu.write_gp0_words(&[0xE100_0208, 0x0212_3456, 0x0010_0020, 0x0008_0010, 0x2800_FF00, 0x0004_0004]);
    s.execute(&json!({"cmd":"frames", "count":3}));
    begin_save(&mut s, 0x3C);
    s.runtime.memory().write_u32(FIBERS, 1).unwrap();
    assert_eq!(s.runtime.step(), Ok(xem_core::Stop::Restart { kind: xem_core::runtime::RESTART_DISPATCH, arg: 0 }));
    let now = s.runtime.services().clock.now;
    s.runtime.services().cd.command(com::READN, &[], now);
    let counter = s.runtime.module();
    counter.import("cd_wanted", &[2, 1]);
    counter.memory.write(MOVIE, &macroblock()).unwrap();
    counter.import("mdec_control", &[0x6000_0000]);
    counter.import("mdec_in", &[MOVIE, 32]);
    counter.import("mdec_out", &[MOVIE + 0x400, 128]);
    counter.import("spu_transfer", &[0x1010, SAVE_DATA, 0x400, 0]);
    s.runtime.services().spu.push_cd_audio(&[0x100, -0x100, 0x200, -0x200]);
    s.runtime.services().raised.push((5, 0x20));
    s.runtime.write_before_next_dispatch(0x8000_0400, 7);
    assert!(s.runtime.services().cd.change_due().is_some(), "the story's change waits");
    states.push(whole_state(&mut s));

    // The arena task's fiber beside the game's, sectors waiting for the
    // game and one in its buffer, the changer's lid open on disc 2 with the
    // game's poll seen, a CPU-to-VRAM image half sent and a VRAM-to-CPU one
    // asked for, the dispatcher's mode.
    s.runtime.memory().write_u32(FIBERS, 2).unwrap();
    s.execute(&json!({"cmd":"frames", "count":80}));
    let cd = &mut s.runtime.services().cd;
    assert!(cd.take_interrupt().is_some_and(|(kind, _)| kind == intr::ACKNOWLEDGE), "the read's first response");
    assert!(cd.take_interrupt().is_some_and(|(kind, _)| kind == intr::DATA_READY), "its first sector, into the buffer");
    cd.read_buffer(0x10);
    s.runtime.module().import("cd_wanted", &[2, 0]);
    let now = s.runtime.services().clock.now;
    s.runtime.services().cd.command(com::GETSTAT, &[], now);
    s.runtime.module().import("mode", &[1]);
    let gpu = s.runtime.services().gpu.device_mut();
    gpu.write_gp0_words(&[0x0014_0004, 0x0014_0014, 0x0004_0014]);
    gpu.write_gp0_words(&[0xC000_0000, 0x0000_0000, 0x0001_0002, 0xA000_0000, 0x0000_0010, 0x0002_0002, 0x1234_5678]);
    assert_eq!(s.runtime.services().cd.inserted(), 2);
    states.push(whole_state(&mut s));

    // A seek under way on the disc the changer closed the lid on, the next
    // target set and the XA audio it read decoded, the arena task waiting,
    // a gouraud polyline half drawn.
    s.execute(&json!({"cmd":"frames", "count":2}));
    let now = s.runtime.services().clock.now;
    let cd = &mut s.runtime.services().cd;
    cd.command(com::SETMODE, &[0x40], now);
    cd.command(com::READS, &[], now);
    let played = cd.xa_sectors;
    while s.runtime.services().cd.xa_sectors == played {
        assert_eq!(s.runtime.module().import("poll_now", &[]), 1, "the drive's next event");
    }
    let now = s.runtime.services().clock.now;
    let cd = &mut s.runtime.services().cd;
    cd.command(com::SEEKL, &[], now);
    cd.command(com::SETLOC, &[0x00, 0x02, 0x10], now);
    let gpu = s.runtime.services().gpu.device_mut();
    gpu.write_gp0_words(&[0x9ABC_DEF0, 0x5800_00FF, 0x0000_0000, 0x0000_FF00, 0x0010_0010, 0x00FF_0000]);
    states.push(whole_state(&mut s));

    let fingerprint: String = digest(&states.concat()).iter().map(|b| format!("{b:02x}")).collect();
    assert_eq!(fingerprint, FORMAT_FINGERPRINT,
        "the state format changed: bump snapshot::VERSION (now {VERSION}) and set FORMAT_FINGERPRINT to {fingerprint}");
}

/// The stand-in's save under way from the next frame: its names, and
/// `fill` mixed with each sector's number as its data.
fn begin_save(s: &mut Session<Counter>, fill: u8) {
    let memory = s.runtime.memory();
    for (at, text) in [(SAVE_DEVICE, "bu00:"), (SAVE_TEMP, "bu00:__tmp_file"), (SAVE_NAME, "bu00:BASLUS-00664")] {
        memory.write(at, format!("{text}\0").as_bytes()).unwrap();
    }
    let data: Vec<u8> = (0..0x2000u32).map(|i| fill ^ (i / 0x100) as u8).collect();
    memory.write(SAVE_DATA, &data).unwrap();
    memory.write_u32(SAVE_STEP, 1).unwrap();
}

/// Frames until the save under way has committed or failed.
fn finish_save(s: &mut Session<Counter>) {
    while word(s, SAVE_STEP) != 0 { assert!(s.frame().unwrap()); }
}

fn word(s: &mut Session<Counter>, at: u32) -> u32 { s.runtime.memory().read_u32(at).unwrap() }

fn card_image(s: &mut Session<Counter>) -> Vec<u8> { s.runtime.services().card.image(0).unwrap().to_vec() }

#[test]
fn rewinding_into_a_save_never_changes_another_file_and_the_same_card_goes_on_identically() {
    let (mut s, clock) = timed_session();
    let stored = Rc::new(RefCell::new([formatted(), formatted()]));
    s.runtime.services().card.attach_storage(Box::new(Storage(stored.clone()))).unwrap();
    begin_save(&mut s, 0xA0);
    for _ in 0..10 { assert!(s.frame().unwrap()); }
    // A rewind state inside the save's writes.
    frame_at(&mut s, &clock, 1);
    let mid_frame = s.runtime.frames();
    assert_eq!(s.rewind.status()["newest_frame"], mid_frame);
    let mid = s.snapshot_bytes().unwrap();
    let card_then = card_image(&mut s);
    let written = word(&mut s, SAVE_WRITTEN);
    assert!((1..32).contains(&written), "{written} sectors written");
    finish_save(&mut s);
    assert_eq!((word(&mut s, SAVE_WRITTEN), word(&mut s, SAVE_RENAMED)), (32, 1), "saved");
    let finished = s.snapshot_bytes().unwrap();
    let first_save = card_image(&mut s);
    // A newer save of the same file takes the same block.
    begin_save(&mut s, 0x5B);
    finish_save(&mut s);
    let newer = card_image(&mut s);
    assert!(newer != first_save && word(&mut s, SAVE_RENAMED) == 1);

    // Rewound into the first save's writes, its calls on the old file are
    // refused as on a removed card, and its own failure path follows (the
    // rename finds no temporary file): the newer save stays as it is.
    let reply = s.execute(&json!({"cmd":"rewind","steps":1}));
    assert!(reply.get("error").is_none(), "{reply}");
    assert_eq!(s.runtime.frames(), mid_frame);
    finish_save(&mut s);
    assert_eq!((word(&mut s, SAVE_REFUSED), word(&mut s, SAVE_RENAMED)), (32 - written, 0));
    assert!(card_image(&mut s) == newer, "the newer save is intact");
    assert!(stored.borrow()[0] == newer, "and so is the stored card");

    // Over the card as the state left it, the save goes on identically.
    s.runtime.services().card.import_image(0, card_then).unwrap();
    s.restore_bytes(&mid).unwrap();
    finish_save(&mut s);
    assert!(card_image(&mut s) == first_save, "the same save");
    assert!(s.snapshot_bytes().unwrap() == finished, "the same state as the first time");
}

fn vblank(s: &mut Session<Counter>) -> u64 { s.execute(&json!({"cmd":"status"}))["vblanks"].as_u64().unwrap() }

#[test]
fn a_recording_reports_the_restores_it_spans_and_a_replay_refuses_them() {
    let cross = xem_core::pad::button::CROSS;
    let (mut s, clock) = timed_session();
    let initial = s.snapshot_bytes().unwrap();
    s.execute(&json!({"cmd":"record"}));
    s.execute(&json!({"cmd":"frames","count":5}));
    frame_at(&mut s, &clock, 1);
    let rewound_to = vblank(&mut s);
    s.execute(&json!({"cmd":"frames","count":4}));
    s.set_pad(0, cross);
    let pressed_at = vblank(&mut s);
    s.execute(&json!({"cmd":"frames","count":2}));
    let rewound_from = vblank(&mut s);
    assert!(s.execute(&json!({"cmd":"rewind","steps":1})).get("error").is_none());
    assert_eq!(vblank(&mut s), rewound_to);
    s.set_pad(0, 0);
    s.execute(&json!({"cmd":"frames","count":3}));
    let loaded_from = vblank(&mut s);
    s.restore_bytes(&initial).unwrap();
    assert_eq!(s.execute(&json!({"cmd":"status"}))["recording"], true, "the recording goes on");
    s.execute(&json!({"cmd":"frames","count":2}));
    let log = s.execute(&json!({"cmd":"stop_recording"}));
    assert_eq!(log["restores"], json!([
        {"kind":"rewind_step","from_vblank":rewound_from,"to_vblank":rewound_to},
        {"kind":"state_load","from_vblank":loaded_from,"to_vblank":0},
    ]));
    // Its pad changes after the rewind go back in time: the replay refuses
    // the log, naming where, and schedules nothing.
    let mut replay = log.clone();
    replay["cmd"] = json!("replay");
    let mut other = session();
    other.restore_bytes(&initial).unwrap();
    let error = other.execute(&replay)["error"].as_str().unwrap().to_string();
    assert!(error.contains(&format!("goes back in time at its changes entry 3: vblank {rewound_to} after vblank {pressed_at}")), "{error}");
    other.execute(&json!({"cmd":"frames","count":20}));
    assert_eq!(other.execute(&json!({"cmd":"status"}))["pads"][0]["buttons"], json!([]), "nothing replayed");

    // A restore forward in time keeps the vblanks in order; the log names it
    // and a replay refuses it all the same.
    let mut later = session();
    later.execute(&json!({"cmd":"frames","count":30}));
    let ahead = later.snapshot_bytes().unwrap();
    let mut s = session();
    s.execute(&json!({"cmd":"record"}));
    s.execute(&json!({"cmd":"frames","count":2}));
    let from = vblank(&mut s);
    s.restore_bytes(&ahead).unwrap();
    let to = vblank(&mut s);
    s.execute(&json!({"cmd":"frames","count":2}));
    let mut log = s.execute(&json!({"cmd":"stop_recording"}));
    assert_eq!(log["restores"], json!([{"kind":"state_load","from_vblank":from,"to_vblank":to}]));
    log["cmd"] = json!("replay");
    let error = session().execute(&log)["error"].as_str().unwrap().to_string();
    assert!(error.contains(&format!("the log spans 1 restore(s), the first a state load at vblank {from} to vblank {to}")), "{error}");
}

#[test]
fn save_states_omit_app_controls_and_loading_keeps_the_current_app_state() {
    use xem_core::time::FastForwardRate;
    let mut s = session();
    s.execute(&json!({"cmd":"record"}));
    s.execute(&json!({"cmd":"fast_forward","enabled":true}));
    s.execute(&json!({"cmd":"frames", "count":2}));
    let saved = s.snapshot_bytes().unwrap();
    s.execute(&json!({"cmd":"stop_recording"}));
    s.execute(&json!({"cmd":"fast_forward","enabled":false}));
    s.configure_fast_forward(FastForwardRate::Fixed(5)).unwrap();
    s.execute(&json!({"cmd":"schedule", "commands":[[3,{"cmd":"write_memory", "address":2147483652u64,"hex":"aabbccdd"}]]}));
    s.runtime.services().presentation.enabled = true;
    assert!(s.snapshot_bytes().unwrap() == saved,"changing app state must not change a game save");
    s.execute(&json!({"cmd":"frames", "count":2}));
    s.restore_bytes(&saved).unwrap();
    assert_eq!(s.runtime.frames(),2);
    assert!(!s.fast_forward);
    assert_eq!(s.fast_forward_rate,FastForwardRate::Fixed(5));
    assert_eq!(s.execute(&json!({"cmd":"status"}))["recording"],false);
    assert!(s.runtime.services().presentation.enabled);
    s.execute(&json!({"cmd":"schedule", "commands":[[3,{"cmd":"write_memory", "address":2147483652u64,"hex":"aabbccdd"}]]}));
    s.execute(&json!({"cmd":"frames", "count":2}));
    assert_eq!(s.runtime.memory().read_u32(0x8000_0004).unwrap(),0xddcc_bbaa);
}

#[test]
fn rewind_is_bounded_discards_the_future_and_restores_the_clock() {
    let (mut s,clock) = timed_session();
    let size = s.snapshot_bytes().unwrap().len();
    let reply = s.execute(&json!({"cmd":"rewind_configure", "max_bytes":size*3+1024, "interval_seconds":1}));
    assert!(reply.get("error").is_none(), "{reply}");
    for at in 1..=8 { frame_at(&mut s,&clock,at); }
    assert!(s.rewind.bytes <= s.rewind.budget);
    let reference = s.runtime.snapshot().unwrap();
    let reply = s.execute(&json!({"cmd":"rewind", "steps":1}));
    assert_eq!(reply["frames"], 7, "{reply}");
    assert_eq!(reply["rewind"]["newest_frame"], 7);
    s.frame().unwrap();
    assert_eq!(s.runtime.snapshot().unwrap(), reference);
    let before = s.snapshot_bytes().unwrap();
    assert!(s.execute(&json!({"cmd":"rewind", "steps":100})).get("error").is_some());
    assert_eq!(s.snapshot_bytes().unwrap(), before);
    let reply = s.execute(&json!({"cmd":"rewind_configure", "max_bytes":1}));
    assert!(reply["rewind"]["error"].as_str().unwrap().contains("one state"));
    assert!(s.frame().unwrap(), "a small rewind budget must not stop gameplay");
}

#[test]
fn rewind_records_game_saves_by_elapsed_seconds_and_steps_through_the_queue() {
    let (mut s,clock) = timed_session();
    assert_eq!(s.rewind.status()["interval_seconds"],1);
    s.execute(&json!({"cmd":"fast_forward","enabled":true}));
    s.execute(&json!({"cmd":"frames","count":100}));
    assert_eq!(s.rewind.status()["states"],1,"a burst of FF game frames does not advance real elapsed time");
    clock.set(Duration::from_millis(999));
    s.frame().unwrap();
    assert_eq!(s.rewind.status()["states"],1);
    clock.set(Duration::from_secs(1));
    s.frame().unwrap();
    let checkpoint_frame = s.runtime.frames();
    assert_eq!(s.rewind.status()["states"],2);
    let (_,saved) = s.rewind.latest().unwrap();
    assert!(saved == s.snapshot_bytes().unwrap(),"rewind stores the identical game save format");
    s.configure_rewind(xem_core::rewind::MAX_REWIND_BYTES,2,30).unwrap();
    assert_eq!(s.rewind.status()["states"],2,"changing the interval keeps existing states");
    clock.set(Duration::from_millis(2999)); s.frame().unwrap();
    assert_eq!(s.rewind.status()["states"],2);
    clock.set(Duration::from_secs(3)); s.frame().unwrap();
    assert_eq!(s.rewind.status()["states"],3);
    s.frame().unwrap();
    assert!(s.rewind_step().unwrap());
    assert!(s.rewind_step().unwrap());
    // The state it took, played on a frame (not as far as where it began).
    assert_eq!(s.runtime.frames(),checkpoint_frame+1);
    let mut reference = session();
    reference.execute(&json!({"cmd":"fast_forward","enabled":true}));
    reference.restore_bytes(&saved).unwrap();
    reference.frame().unwrap();
    assert!(s.snapshot_bytes().unwrap() == reference.snapshot_bytes().unwrap());
    assert!(s.rewind_step().unwrap());
    assert_eq!(s.runtime.frames(),xem_core::rewind::REWIND_PLAY_BLANKS);
    assert!(!s.rewind_step().unwrap());
}

#[test]
fn pause_and_restore_reset_the_elapsed_play_interval_without_saving_the_timer() {
    let (mut s,clock) = timed_session();
    frame_at(&mut s,&clock,1);
    let save = s.snapshot_bytes().unwrap();
    clock.set(Duration::from_secs(100));
    s.reset_rewind_timer();
    s.frame().unwrap();
    assert_eq!(s.rewind.status()["states"],2,"paused time does not create another state on resume");
    clock.set(Duration::from_secs(101)); s.frame().unwrap();
    assert_eq!(s.rewind.status()["states"],3);
    s.restore_bytes(&save).unwrap();
    assert_eq!(s.rewind.status()["states"],1);
    assert!(s.snapshot_bytes().unwrap() == save);
    clock.set(Duration::from_millis(101999)); s.frame().unwrap();
    assert_eq!(s.rewind.status()["states"],1);
    clock.set(Duration::from_secs(102)); s.frame().unwrap();
    assert_eq!(s.rewind.status()["states"],2);
}

#[test]
fn snapshot_and_rewind_preserve_a_stopped_sound_timer_without_rescheduling_it() {
    let (mut s,clock) = timed_session();
    s.runtime.services().clock.set_counter(2,0x44e8,xem_core::clock::RCNT_MODE_INTERRUPT);
    s.runtime.services().clock.start_counter(2);
    s.runtime.services().clock.stop_counter(2);
    for at in 1..=3 { frame_at(&mut s,&clock,at); }
    let counter = s.runtime.services().clock.counters[2];
    assert!(counter.due < s.runtime.services().clock.now);
    let saved = s.runtime.snapshot().unwrap();
    let mut restored = Runtime::new(Counter::new());
    restored.restore(&saved).unwrap();
    assert_eq!(restored.services().clock.counters[2],counter);
    frame_at(&mut s,&clock,4);
    assert!(s.rewind_step().unwrap());
    assert_eq!(s.runtime.services().clock.counters[2],counter);
}

#[test]
fn rewind_keeps_current_app_controls_and_restores_only_game_progress() {
    use xem_core::time::FastForwardRate;
    let (mut s,clock) = timed_session();
    s.runtime.services().clock.set_counter(2,0x44e8,xem_core::clock::RCNT_MODE_INTERRUPT);
    s.runtime.services().clock.start_counter(2);
    s.execute(&json!({"cmd":"record"}));
    s.execute(&json!({"cmd":"fast_forward","enabled":true}));
    for at in 1..=3 { frame_at(&mut s,&clock,at); }
    let music_phase = s.runtime.services().clock.read_counter(2);
    let music_samples = s.runtime.services().spu.chip().clock();
    let game_progress = s.runtime.memory().read_u32(0x8000_0000).unwrap();
    let (_,checkpoint) = s.rewind.latest().unwrap();
    let mut game_checkpoint = Runtime::new(Counter::new());
    game_checkpoint.restore(&checkpoint).unwrap();
    assert_eq!(game_checkpoint.services().clock.audio_speed,1,"a checkpoint does not retain the app's old pacing");
    assert_eq!(game_checkpoint.services().spu.speed(),1);
    assert_eq!(game_checkpoint.services().clock.read_counter(2),music_phase);
    assert_eq!(s.runtime.services().clock.audio_speed,10,"taking a checkpoint does not change live pacing");
    frame_at(&mut s,&clock,4);
    s.execute(&json!({"cmd":"stop_recording"}));
    s.execute(&json!({"cmd":"fast_forward","enabled":false}));
    s.configure_fast_forward(FastForwardRate::Fixed(5)).unwrap();
    s.runtime.services().presentation.enabled = true;
    let reply = s.execute(&json!({"cmd":"rewind","steps":1}));
    assert_eq!(reply["frames"],3,"{reply}");
    assert_eq!(reply["fast_forward"],false,"rewind must not re-enable FF");
    assert_eq!(reply["fast_forward_rate"],json!({"Fixed":5}));
    assert_eq!(reply["recording"],false,"rewind must not re-enable an app recording");
    assert!(s.runtime.services().presentation.enabled);
    assert_eq!(s.runtime.services().clock.audio_speed,1);
    assert_eq!(s.runtime.services().spu.speed(),1);
    assert_eq!(s.runtime.services().clock.read_counter(2),music_phase);
    assert_eq!(s.runtime.services().spu.chip().clock(),music_samples);
    assert_eq!(s.runtime.memory().read_u32(0x8000_0000).unwrap(),game_progress);
    s.execute(&json!({"cmd":"fast_forward","enabled":true}));
    assert!(s.rewind_step().unwrap());
    assert_eq!(s.runtime.frames(),2);
    assert!(s.fast_forward);
    assert_eq!(s.fast_forward_rate,FastForwardRate::Fixed(5));
    assert_eq!(s.runtime.services().clock.audio_speed,5);
    assert_eq!(s.runtime.services().spu.speed(),5);
    s.configure_fast_forward(FastForwardRate::Uncapped).unwrap();
    s.execute(&json!({"cmd":"fast_forward","enabled":true}));
    s.execute(&json!({"cmd":"record"}));
    s.execute(&json!({"cmd":"schedule","commands":[[3,{"cmd":"write_memory","address":2147483660u64,"hex":"aabbccdd"}]]}));
    for frame in [1,0] {
        assert!(s.rewind_step().unwrap());
        assert_eq!(s.runtime.frames(),frame);
        assert!(s.fast_forward,"rewind must not disable FF either");
        assert_eq!(s.fast_forward_rate,FastForwardRate::Uncapped);
        assert_eq!(s.execute(&json!({"cmd":"status"}))["recording"],true);
        assert_eq!(s.runtime.services().clock.audio_speed,1);
        assert_eq!(s.runtime.services().spu.speed(),1);
    }
    s.execute(&json!({"cmd":"frames","count":4}));
    assert_eq!(s.runtime.memory().read_u32(0x8000_000c).unwrap(),0xddcc_bbaa,"current app commands are not replaced by historical ones");
}

#[test]
fn long_runs_and_output_modes_produce_the_same_restored_state() {
    let mut s = session();
    s.execute(&json!({"cmd":"frames", "count":1000}));
    let saved = s.snapshot_bytes().unwrap();
    assert!(s.execute(&json!({"cmd":"fast_forward_configure", "rate":{"Fixed":20}})).get("error").is_none());
    assert_eq!(s.execute(&json!({"cmd":"fast_forward"}))["fast_forward"],true);
    s.execute(&json!({"cmd":"frames", "count":1000}));
    let reference = s.runtime.snapshot().unwrap();
    let mut other = session();
    other.runtime.services().presentation.enabled = true;
    other.runtime.services().spu.set_output(true);
    other.restore_bytes(&saved).unwrap();
    assert!(other.runtime.services().presentation.enabled);
    assert!(other.execute(&json!({"cmd":"fast_forward_configure", "rate":{"Fixed":20}})).get("error").is_none());
    assert_eq!(other.execute(&json!({"cmd":"fast_forward"}))["fast_forward"],true);
    other.execute(&json!({"cmd":"frames", "count":1000}));
    assert_eq!(other.runtime.snapshot().unwrap(), reference);
}

#[test]
fn held_rewind_steps_pop_each_state_play_on_from_it_and_stop_at_the_oldest() {
    let (mut s,clock) = timed_session();
    s.execute(&json!({"cmd":"rewind_configure","max_bytes":0}));
    assert!(s.rewind_step().unwrap_err().contains("Enable rewind"));
    s.execute(&json!({"cmd":"rewind_configure","max_bytes":xem_core::rewind::MAX_REWIND_BYTES,"interval_seconds":1}));
    s.runtime.services().spu.set_output(true);
    // States at frames 0, 1, 21, 41 and 61; the game at 80.
    for second in 1..=4 {
        clock.set(Duration::from_secs(second));
        for _ in 0..20 { assert!(s.frame().unwrap()); }
    }
    assert_eq!(s.rewind.status()["states"],5);
    // Under fast-forward the music follows the host's clock: the frames
    // played on advance it as they play, more than one advance may (100 ms).
    assert_eq!(s.execute(&json!({"cmd":"fast_forward","enabled":true}))["fast_forward"],true);
    assert!(s.begin_live_music().unwrap());
    // Each step plays on from its state (this stand-in never changes its
    // display's buffers: the most frames), never as far as where it began,
    // and its frames add no state of their own; their sound is queued.
    let most = xem_core::rewind::REWIND_PLAY_BLANKS;
    for (frame, states) in [(61+most,4),(41+most,3),(21+most,2),(1+most,1),(most,0)] {
        assert!(s.rewind_step().unwrap());
        assert_eq!(s.runtime.frames(),frame);
        assert_eq!(s.rewind.status()["states"],states);
    }
    assert!(s.runtime.services().spu.audio_queued() > 0,"the frames played on sound");
    let oldest = s.snapshot_bytes().unwrap();
    assert!(!s.rewind_step().unwrap());
    assert_eq!(s.snapshot_bytes().unwrap(),oldest);
    assert!(s.frame().unwrap()); assert_eq!(s.runtime.frames(),most+1);
}

#[test]
fn music_keeps_normal_time_when_a_host_cannot_run_all_requested_fast_frames() {
    let mut s = session();
    assert!(s.execute(&json!({"cmd":"fast_forward_configure", "rate":{"Fixed":20}})).get("error").is_none());
    assert_eq!(s.execute(&json!({"cmd":"fast_forward"}))["fast_forward"],true);
    s.runtime.services().clock.set_counter(2,0x44e8,xem_core::clock::RCNT_MODE_INTERRUPT);
    s.runtime.services().clock.start_counter(2);
    assert!(s.begin_live_music().unwrap());
    for _ in 0..60 {
        // Three game frames fit this host's budget, although 20 were requested.
        s.execute(&json!({"cmd":"frames","count":3}));
        s.advance_live_music(xem_core::clock::VBLANK_CYCLES).unwrap();
    }
    assert_eq!(s.runtime.frames(),180);
    assert_eq!(s.runtime.memory().read_u32(0x8000_0008).unwrap(),240);
    assert_eq!(s.runtime.services().spu.chip().clock(),44100);
    let saved = s.snapshot_bytes().unwrap();
    let mut other = session();
    other.restore_bytes(&saved).unwrap();
    assert_eq!(other.snapshot_bytes().unwrap(),saved,"host pacing attachments do not enter the state file");
    for _ in 0..30 {
        for s in [&mut s,&mut other] {
            s.runtime.set_external_music(true);
            s.execute(&json!({"cmd":"frames","count":5}));
            s.execute(&json!({"cmd":"music_advance","cycles":xem_core::clock::VBLANK_CYCLES}));
        }
    }
    assert_eq!(s.snapshot_bytes().unwrap(),other.snapshot_bytes().unwrap());
}

#[test]
fn interactive_fixed_fast_forward_records_music_before_the_first_batch_and_keeps_queued_audio() {
    let mut s = session();
    s.runtime.services().clock.set_counter(2,0x44e8,xem_core::clock::RCNT_MODE_INTERRUPT);
    s.runtime.services().clock.start_counter(2);
    s.runtime.services().spu.set_output(true);
    let initial = s.snapshot_bytes().unwrap();
    s.execute(&json!({"cmd":"record"}));
    let revision = s.timeline_revision;
    s.execute(&json!({"cmd":"fast_forward","enabled":true}));
    for (frames, cycles, pace) in [(3,563200,3),(8,800000,4),(2,400000,2)] {
        assert!(s.begin_live_music().unwrap());
        s.execute(&json!({"cmd":"frames","count":frames}));
        s.advance_live_music(cycles).unwrap();
        let queued = s.runtime.services().spu.audio_queued();
        assert!(queued > 0);
        s.pace_fast_forward(Some(pace)).unwrap();
        assert_eq!(s.runtime.services().spu.audio_queued(),queued,"adjusting pace must preserve pending audio");
    }
    let queued = s.runtime.services().spu.audio_queued();
    assert_eq!(queued,2*((563200+800000+400000)/768));
    s.configure_fast_forward(xem_core::time::FastForwardRate::Fixed(20)).unwrap();
    assert!(s.begin_live_music().unwrap());
    s.execute(&json!({"cmd":"frames","count":1}));
    s.advance_live_music(563200).unwrap();
    s.execute(&json!({"cmd":"fast_forward","enabled":false}));
    assert_eq!(s.timeline_revision,revision,"changing speed must not reset a host's playback queue");
    s.execute(&json!({"cmd":"frames","count":2}));
    let mut log = s.execute(&json!({"cmd":"stop_recording"})); log["cmd"] = json!("replay");
    let mut replayed = session();
    replayed.restore_bytes(&initial).unwrap();
    replayed.runtime.services().spu.set_output(true);
    assert!(replayed.execute(&log).get("error").is_none());
    assert!(!replayed.begin_live_music().unwrap(),"replay uses recorded elapsed time");
    assert!(replayed.execute(&json!({"cmd":"frames","count":16})).get("error").is_none());
    assert_eq!(s.runtime.services().spu.audio_queued(),replayed.runtime.services().spu.audio_queued());
    assert_eq!(s.snapshot_bytes().unwrap(),replayed.snapshot_bytes().unwrap());
}

#[test]
fn recording_during_live_fixed_fast_forward_preserves_the_attached_music_clock() {
    let mut s = session();
    s.runtime.services().clock.set_counter(2,0x44e8,xem_core::clock::RCNT_MODE_INTERRUPT);
    s.runtime.services().clock.start_counter(2);
    s.runtime.services().spu.set_output(true);
    s.execute(&json!({"cmd":"fast_forward","enabled":true}));
    assert!(s.begin_live_music().unwrap());
    s.execute(&json!({"cmd":"frames","count":3}));
    s.advance_live_music(xem_core::clock::VBLANK_CYCLES).unwrap();
    let mut previous = vec![0;s.runtime.services().spu.audio_queued()];
    s.runtime.services().spu.take_audio(&mut previous);
    let initial = s.snapshot_bytes().unwrap();
    s.execute(&json!({"cmd":"record"}));
    for frames in [3,5] {
        assert!(s.begin_live_music().unwrap());
        s.execute(&json!({"cmd":"frames","count":frames}));
        s.advance_live_music(xem_core::clock::VBLANK_CYCLES).unwrap();
    }
    let mut log = s.execute(&json!({"cmd":"stop_recording"}));
    log["cmd"] = json!("replay");
    let mut replayed = session();
    replayed.restore_bytes(&initial).unwrap();
    replayed.runtime.services().spu.set_output(true);
    assert!(replayed.execute(&log).get("error").is_none());
    assert!(replayed.execute(&json!({"cmd":"frames","count":8})).get("error").is_none());
    assert_eq!(s.runtime.services().spu.audio_queued(),replayed.runtime.services().spu.audio_queued(),
        "the first recorded batch must not advance music before its elapsed-time input");
    assert_eq!(s.snapshot_bytes().unwrap(),replayed.snapshot_bytes().unwrap());
}

#[test]
fn a_fast_forward_pace_is_recorded_and_replayed_with_the_frames() {
    let mut s = session();
    s.runtime.services().clock.set_counter(2,0x44e8,xem_core::clock::RCNT_MODE_INTERRUPT);
    s.runtime.services().clock.start_counter(2);
    let initial = s.snapshot_bytes().unwrap();
    s.execute(&json!({"cmd":"record"}));
    s.execute(&json!({"cmd":"fast_forward","enabled":true}));
    assert_eq!(s.frame_factor(),Some(10));
    s.execute(&json!({"cmd":"frames","count":20}));
    s.pace_fast_forward(Some(4)).unwrap();
    assert_eq!(s.frame_factor(),Some(4));
    assert_eq!(s.runtime.services().clock.audio_speed,4,"the music is timed by the frames the host reaches");
    s.execute(&json!({"cmd":"frames","count":20}));
    s.execute(&json!({"cmd":"fast_forward","enabled":false}));
    assert_eq!(s.fast_forward_pace,None,"a new fast-forward starts at the configured rate");
    s.execute(&json!({"cmd":"frames","count":5}));
    let mut log = s.execute(&json!({"cmd":"stop_recording"})); log["cmd"] = json!("replay");
    let mut other = session(); other.restore_bytes(&initial).unwrap();
    other.runtime.services().clock.set_counter(2,0x44e8,xem_core::clock::RCNT_MODE_INTERRUPT);
    other.runtime.services().clock.start_counter(2);
    assert!(other.execute(&log).get("error").is_none());
    other.execute(&json!({"cmd":"frames","count":45}));
    assert!(s.snapshot_bytes().unwrap() == other.snapshot_bytes().unwrap());
    assert!(s.pace_fast_forward(Some(21)).is_err());
}

#[test]
fn the_actor_view_is_recorded_and_a_replay_keeps_it() {
    let mut s = session();
    let initial = s.snapshot_bytes().unwrap();
    s.execute(&json!({"cmd":"record"}));
    s.execute(&json!({"cmd":"frames","count":5}));
    let view = 300 | 32 << 16;
    s.set_actor_view(view).unwrap();
    s.execute(&json!({"cmd":"frames","count":5}));
    s.set_actor_view(0).unwrap();
    s.execute(&json!({"cmd":"frames","count":5}));
    let mut log = s.execute(&json!({"cmd":"stop_recording"}));
    let views: Vec<serde_json::Value> = log["controls"].as_array().unwrap().iter().filter(|c| c[1]["cmd"] == "actor_view").map(|c| c[1]["view"].clone()).collect();
    assert_eq!(views, [json!(view), json!(0)]);
    log["cmd"] = json!("replay");
    let mut other = session();
    other.restore_bytes(&initial).unwrap();
    assert!(other.execute(&log).get("error").is_none());
    other.execute(&json!({"cmd":"frames","count":7}));
    other.set_actor_view(500).unwrap();
    assert_eq!(other.execute(&json!({"cmd":"status"}))["actor_view"], json!(view), "a replay keeps the recorded view, not the host's");
    other.execute(&json!({"cmd":"frames","count":8}));
    assert_eq!(other.execute(&json!({"cmd":"status"}))["actor_view"], json!(0));
    assert!(s.set_actor_view(99).is_err(), "a pull-back below 100 percent is no view");
}

fn random_battles_off(s: &mut Session<Counter>) -> serde_json::Value {
    s.execute(&json!({"cmd":"status"}))["random_battles_off"].clone()
}

#[test]
fn the_maps_without_random_battles_are_recorded_and_a_replay_keeps_them() {
    let mut s = session();
    s.runtime.memory().write_u32(DRAW_MAP, 0xC000 | 414).unwrap();
    s.execute(&json!({"cmd":"frames","count":3}));
    assert_eq!(word(&mut s, DRAW_RAN), 3, "the original's draw runs every frame");
    assert_eq!(random_battles_off(&mut s), json!([]));
    let initial = s.snapshot_bytes().unwrap();
    s.execute(&json!({"cmd":"record"}));
    s.execute(&json!({"cmd":"frames","count":2}));
    s.set_random_battles(&[416, 414, 414]).unwrap();
    s.set_random_battles(&[414, 416]).unwrap();
    assert_eq!(random_battles_off(&mut s), json!([414, 416]));
    s.execute(&json!({"cmd":"frames","count":4}));
    assert_eq!(word(&mut s, DRAW_RAN), 5, "no draw runs on a map turned off (its flag bits aside)");
    s.set_random_battles(&[22]).unwrap();
    s.execute(&json!({"cmd":"frames","count":2}));
    s.set_random_battles(&[]).unwrap();
    s.execute(&json!({"cmd":"frames","count":1}));
    assert_eq!(word(&mut s, DRAW_RAN), 8);
    let mut log = s.execute(&json!({"cmd":"stop_recording"}));
    let offs: Vec<serde_json::Value> = log["controls"].as_array().unwrap().iter().filter(|c| c[1]["cmd"] == "random_battles").map(|c| c[1]["off"].clone()).collect();
    assert_eq!(offs, [json!([414, 416]), json!([22]), json!([])], "each change once, its maps ascending");
    // A replay from the same state starts from the original's and keeps
    // the recorded maps, not the host's.
    log["cmd"] = json!("replay");
    let mut other = session();
    other.restore_bytes(&initial).unwrap();
    other.set_random_battles(&[414]).unwrap();
    assert!(other.execute(&log).get("error").is_none());
    assert_eq!(random_battles_off(&mut other), json!([]));
    other.execute(&json!({"cmd":"frames","count":3}));
    other.set_random_battles(&[]).unwrap();
    assert_eq!(random_battles_off(&mut other), json!([414, 416]), "a replay keeps the recorded maps, not the host's");
    other.execute(&json!({"cmd":"frames","count":6}));
    assert_eq!(word(&mut other, DRAW_RAN), 8, "the replay draws as the recording did");
    assert_eq!(random_battles_off(&mut other), json!([]));
    // A recording begun with maps off starts with them.
    s.set_random_battles(&[407]).unwrap();
    s.execute(&json!({"cmd":"record"}));
    let log = s.execute(&json!({"cmd":"stop_recording"}));
    assert!(log["controls"].as_array().unwrap().iter().any(|c| c[1] == json!({"cmd":"random_battles","off":[407]})), "{log}");
    // Session logs and agents set it by command; only field maps are maps.
    let frames = s.runtime.frames();
    let scheduled = s.execute(&json!({"cmd":"schedule","commands":[[frames + 1, {"cmd":"random_battles","off":[23, 22]}]]}));
    assert!(scheduled.get("error").is_none(), "{scheduled}");
    s.execute(&json!({"cmd":"frames","count":2}));
    assert_eq!(random_battles_off(&mut s), json!([22, 23]));
    let valid = s.execute(&json!({"cmd":"readiness"}))["valid_commands"].clone();
    assert!(valid.as_array().unwrap().contains(&json!("random_battles")));
    for bad in [json!({"cmd":"random_battles"}), json!({"cmd":"random_battles","off":[4096]}), json!({"cmd":"random_battles","off":"414"})] {
        assert!(s.execute(&bad)["error"].as_str().is_some_and(|e| e.contains("field maps 0-4095")), "{bad}");
    }
    assert!(s.set_random_battles(&[0x1000]).is_err());
    assert_eq!(random_battles_off(&mut s), json!([22, 23]), "a refused change changes nothing");
    let refused = s.execute(&json!({"cmd":"replay","changes":[],"controls":[[0, {"cmd":"random_battles","off":[-1]}]]}));
    assert!(refused["error"].as_str().is_some_and(|e| e.contains("random-battles")), "{refused}");
}

#[test]
fn the_field_command_reports_the_random_draw_and_whether_it_is_off_here() {
    let mut s = session();
    let memory = s.runtime.memory();
    memory.write_u32(0x8004_F34C, 0xC000 | 414).unwrap();
    for (at, value) in [(0x800B_2294, 120), (0x800B_2298, 300), (0x800B_229C, 1)] {
        memory.write_u32(at, value).unwrap();
    }
    memory.write(0x800B_22A0, &50u16.to_le_bytes()).unwrap();
    let draw = |s: &mut Session<Counter>| s.execute(&json!({"cmd":"field"}))["random_draw"].clone();
    assert_eq!(draw(&mut s), json!({"map": 414, "period": 300, "count": 1, "window": 120, "steps": [50], "off_here": false}));
    s.set_random_battles(&[414]).unwrap();
    assert_eq!(draw(&mut s)["off_here"], json!(true));
}

#[test]
fn inspection_stops_a_route_refuses_new_ones_and_leaves_recorded_ones_to_run() {
    let mut s = session();
    let intent = xem_core::assist::Intent::Walk { to: xem_core::field::V3::new(10.0, 0.0, 20.0), triangle: 0, layer: 0 };
    s.execute(&json!({"cmd":"record"}));
    s.start_assist(intent.clone());
    s.set_inspecting(true);
    assert!(s.assist.is_none(), "turning inspection on stops the route under way");
    assert_eq!(s.assist_outcome, Some(xem_core::assist::Outcome::Stopped));
    assert_eq!(s.execute(&json!({"cmd":"status"}))["inspecting"], json!(true));
    for command in [json!({"cmd":"navigate","to":[1.0,2.0]}), json!({"cmd":"interact","actor":1})] {
        let reply = s.execute(&command);
        assert!(reply["error"].as_str().is_some_and(|e| e.starts_with("first-person inspection keeps the character")), "{command} -> {reply}");
    }
    // A recorded intent (a replay's, a session log's) still runs; staying
    // on stops nothing more, and turning it off restarts nothing.
    assert!(s.execute(&json!({"cmd":"assist","intent":intent})).get("error").is_none());
    s.set_inspecting(true);
    assert!(s.assist.is_some(), "only turning it on stops a route");
    s.execute(&json!({"cmd":"assist_stop"}));
    s.set_inspecting(false);
    assert!(s.assist.is_none(), "turning it off starts no route");
    assert_eq!(s.execute(&json!({"cmd":"status"}))["inspecting"], json!(false));
    let log = s.execute(&json!({"cmd":"stop_recording"}));
    let assists: Vec<&str> = log["controls"].as_array().unwrap().iter().filter_map(|c| c[1]["cmd"].as_str()).filter(|cmd| cmd.starts_with("assist")).collect();
    assert_eq!(assists, ["assist", "assist_stop", "assist", "assist_stop"], "inspection's stop is recorded as the player's own");
    // A replay keeps its own record: the host's inspection stops none of its routes.
    let replay = json!({"cmd":"replay","start_vblank":0,"end_vblank":5,"changes":[],"controls":[[0,{"cmd":"assist","intent":intent}]]});
    assert!(s.execute(&replay).get("error").is_none());
    s.set_inspecting(true);
    assert!(s.assist.is_some(), "a replay's route runs whatever the host shows");
}

#[test]
fn the_sound_mode_is_recorded_and_a_replay_keeps_it() {
    let mut s = session();
    let initial = s.snapshot_bytes().unwrap();
    s.set_sound_mode(0).unwrap();
    s.execute(&json!({"cmd":"record"}));
    s.execute(&json!({"cmd":"frames","count":5}));
    s.set_sound_mode(2).unwrap();
    s.set_sound_mode(2).unwrap();
    s.execute(&json!({"cmd":"frames","count":5}));
    let mut log = s.execute(&json!({"cmd":"stop_recording"}));
    let modes: Vec<serde_json::Value> = log["controls"].as_array().unwrap().iter().filter(|c| c[1]["cmd"] == "sound_mode").map(|c| c[1]["mode"].clone()).collect();
    assert_eq!(modes, [json!(0), json!(2)], "the mode at the start, then each change once");
    log["cmd"] = json!("replay");
    let mut other = session();
    other.restore_bytes(&initial).unwrap();
    assert!(other.execute(&log).get("error").is_none());
    other.execute(&json!({"cmd":"frames","count":7}));
    other.set_sound_mode(1).unwrap();
    assert_eq!(other.execute(&json!({"cmd":"status"}))["sound_mode"], json!(2), "a replay keeps the recorded mode, not the host's");
    assert!(s.set_sound_mode(3).is_err(), "no menu reaches the driver's mode 3");
}
