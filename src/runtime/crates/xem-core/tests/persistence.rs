//! Public tests for restore failure atomicity, queued commands and time controls.
//! This stand-in exercises the real clock/devices; original-game tests live in
//! xem-headless/tests/slice.rs and route.rs.
use serde_json::json;
use std::{cell::Cell,rc::Rc,time::Duration};
use xem_core::control::Session;
use xem_core::memory::OutOfBounds;
use xem_core::module::{AsyncState, ImportHandler};
use xem_core::snapshot::{REGIONS, checked, seal};
use xem_core::{Action, GameMemory, GameModule, Import, Runtime, Trap};

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
}
impl Counter {
    fn new() -> Self {
        Self { memory: Memory(REGIONS.iter().map(|(_, size)| vec![0; *size as usize]).collect()),
            handler: None, state: AsyncState::Normal, sp: 0x1010_0000 }
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
        let action = unsafe { (*self.handler.unwrap()).import(Import { name: "yield", args: &[1] }, &mut self.memory) };
        assert_eq!(action, Action::Unwind);
        self.state = AsyncState::Unwinding;
        Ok(())
    }
    fn run_task(&mut self) -> Result<(), Trap> { unreachable!() }
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
    fn globals(&mut self) -> Vec<u32> { vec![] }
    fn set_globals(&mut self, _: &[u32]) {}
}
fn session() -> Session<Counter> {
    let started = std::time::Instant::now();
    Session::new(Runtime::new(Counter::new()),None,move || started.elapsed())
}
fn timed_session() -> (Session<Counter>,Rc<Cell<Duration>>) {
    let clock = Rc::new(Cell::new(Duration::ZERO));
    let source = clock.clone();
    let mut session = Session::new(Runtime::new(Counter::new()),None,move || source.get());
    session.configure_rewind(xem_core::rewind::DEFAULT_REWIND_BYTES,1,30).unwrap();
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
    s.configure_rewind(xem_core::rewind::DEFAULT_REWIND_BYTES,1,30).unwrap();
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
    use xem_core::devices::card::{CardStorage, CARD_SIZE, formatted};
    use std::cell::RefCell;
    #[derive(Clone)]
    struct Storage(Rc<RefCell<[Vec<u8>; 2]>>);
    impl CardStorage for Storage {
        fn load(&mut self, slot: usize) -> std::io::Result<Option<Vec<u8>>> { Ok(Some(self.0.borrow()[slot].clone())) }
        fn store(&mut self, slot: usize, image: &[u8]) -> std::io::Result<()> { self.0.borrow_mut()[slot] = image.to_vec(); Ok(()) }
    }
    let stored = Rc::new(RefCell::new([formatted(), formatted()]));
    let (mut s, clock) = timed_session();
    s.configure_rewind(0,1,30).unwrap();
    s.runtime.services().card.attach_storage(Box::new(Storage(stored.clone()))).unwrap();
    s.configure_rewind(xem_core::rewind::DEFAULT_REWIND_BYTES, 1, 30).unwrap();
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

#[test]
fn a_corrupt_late_device_or_incompatible_state_changes_nothing() {
    let mut s = session();
    s.execute(&json!({"cmd":"frames", "count":2}));
    let original = s.runtime.snapshot().unwrap();
    s.execute(&json!({"cmd":"frames", "count":3}));
    let current = s.runtime.snapshot().unwrap();
    let mut body = checked(&original).unwrap().to_vec();
    body.pop(); // truncate the last device, keeping a correct envelope checksum
    assert!(s.runtime.restore(&seal(body)).is_err());
    assert_eq!(s.runtime.snapshot().unwrap(), current);
    let mut extra = checked(&original).unwrap().to_vec(); extra.push(7);
    assert!(s.runtime.restore(&seal(extra)).is_err());
    assert_eq!(s.runtime.snapshot().unwrap(), current);
    s.runtime.identity.packages.push("different-package-digest".into());
    let incompatible = s.runtime.snapshot().unwrap();
    assert!(s.runtime.restore(&original).unwrap_err().to_string().contains("incompatible"));
    assert_eq!(s.runtime.snapshot().unwrap(), incompatible);
    let mut broken = original; broken[80] ^= 1;
    assert!(s.runtime.restore(&broken).unwrap_err().to_string().contains("checksum"));
    assert_eq!(s.runtime.snapshot().unwrap(), incompatible);
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
    s.configure_rewind(xem_core::rewind::DEFAULT_REWIND_BYTES,2,30).unwrap();
    assert_eq!(s.rewind.status()["states"],2,"changing the interval keeps existing states");
    clock.set(Duration::from_millis(2999)); s.frame().unwrap();
    assert_eq!(s.rewind.status()["states"],2);
    clock.set(Duration::from_secs(3)); s.frame().unwrap();
    assert_eq!(s.rewind.status()["states"],3);
    s.frame().unwrap();
    assert!(s.rewind_step().unwrap());
    assert!(s.rewind_step().unwrap());
    assert_eq!(s.runtime.frames(),checkpoint_frame);
    assert!(s.snapshot_bytes().unwrap() == saved);
    assert!(s.rewind_step().unwrap());
    assert_eq!(s.runtime.frames(),0);
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
fn held_rewind_steps_stop_at_the_oldest_checkpoint_without_advancing_the_game() {
    let (mut s,clock) = timed_session();
    s.execute(&json!({"cmd":"rewind_configure","max_bytes":0}));
    assert!(s.rewind_step().unwrap_err().contains("Enable rewind"));
    s.execute(&json!({"cmd":"rewind_configure","max_bytes":xem_core::rewind::DEFAULT_REWIND_BYTES,"interval_seconds":2}));
    for at in 1..=8 { frame_at(&mut s,&clock,at); }
    for frame in [6,4,2,0] {
        assert!(s.rewind_step().unwrap()); assert_eq!(s.runtime.frames(),frame);
    }
    let oldest = s.snapshot_bytes().unwrap();
    assert!(!s.rewind_step().unwrap());
    assert_eq!(s.snapshot_bytes().unwrap(),oldest);
    assert!(s.frame().unwrap()); assert_eq!(s.runtime.frames(),1);
}

#[test]
fn music_keeps_normal_time_when_a_host_cannot_run_all_requested_fast_frames() {
    let mut s = session();
    assert!(s.execute(&json!({"cmd":"fast_forward_configure", "rate":{"Fixed":20}})).get("error").is_none());
    assert_eq!(s.execute(&json!({"cmd":"fast_forward"}))["fast_forward"],true);
    s.runtime.services().clock.set_counter(2,0x44e8,xem_core::clock::RCNT_MODE_INTERRUPT);
    s.runtime.services().clock.start_counter(2);
    s.runtime.set_external_music(true);
    for _ in 0..60 {
        // Three game frames fit this host's budget, although 20 were requested.
        s.execute(&json!({"cmd":"frames","count":3}));
        s.execute(&json!({"cmd":"music_advance","cycles":xem_core::clock::VBLANK_CYCLES}));
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
