//! The arena task's fibers end to end: tests/fiber_module/build.sh builds a
//! stand-in module from the port's fiber.c and arena_task.c with asyncify, as
//! tools/game_module.py builds the game, and runs this test against it through
//! wasm2c and the real runtime. With the game module (or none) it does nothing.

#[cfg(has_game_module)]
#[test]
fn task_and_game_fibers_keep_their_stacks_across_switches_and_waits() {
    use xem_core::{Runtime, Stop, YieldReason};

    if std::env::var_os("XEM_FIBER_MODULE").is_none() {
        return;
    }
    const LOG: u32 = 0x8010_0200;
    let mut runtime = Runtime::new(xem_game::NativeModule::new());
    // The game resumes the new task, which polls.
    assert_eq!(runtime.step(), Ok(Stop::Yield(YieldReason::Poll)));
    // An interrupt while the task is suspended runs below its frames.
    runtime.interrupt(0, 0).unwrap();
    // The task yields; the game waits for a frame.
    assert_eq!(runtime.step(), Ok(Stop::Yield(YieldReason::VSync)));
    // The game resumes the task, which yields again; the game restarts.
    assert_eq!(runtime.step(), Ok(Stop::Restart { kind: 1, arg: 9 }));
    // The restart abandons the task: the game starts afresh with a new one.
    assert_eq!(runtime.step(), Ok(Stop::Yield(YieldReason::Poll)));
    let memory = runtime.memory();
    let count = memory.read_u32(LOG).unwrap();
    let log: Vec<u32> = (0..count).map(|i| memory.read_u32(LOG + 4 + i * 4).unwrap()).collect();
    println!("fiber log: {log:x?}");
    assert_eq!(log, [0x1000, 0x7005, 0x9000, 0x7006, 0x1001, 0x1002, 0x7007, 0x1003, 0x1001, 0x7005]);
}

/// What a runaway game does to its host: nothing. A store beyond game
/// memory and calls without end both stop as traps, the module runs on
/// after each, and neither leaves anything behind for the next.
#[cfg(has_game_module)]
#[test]
fn an_access_beyond_game_memory_and_endless_calls_trap_and_the_module_runs_on() {
    use xem_core::{GameModule, Trap};

    if std::env::var_os("XEM_FIBER_MODULE").is_none() {
        return;
    }
    let mut module = xem_game::NativeModule::new();
    for _ in 0..3 {
        assert_eq!(module.call(0), Ok(()));
        assert_eq!(module.call(1), Err(Trap::Wasm("Out-of-bounds access in linear memory or a table".into())));
        assert_eq!(module.call(0), Ok(()));
        assert_eq!(module.call(2), Err(Trap::Wasm("Call stack exhausted".into())));
    }
    // A second module in the same process, on another thread, traps the
    // same way while the first still stands.
    std::thread::spawn(|| {
        let mut other = xem_game::NativeModule::new();
        assert_eq!(other.call(1), Err(Trap::Wasm("Out-of-bounds access in linear memory or a table".into())));
        assert_eq!(other.call(2), Err(Trap::Wasm("Call stack exhausted".into())));
        assert_eq!(other.call(0), Ok(()));
    })
    .join()
    .expect("the other module's thread");
    assert_eq!(module.call(0), Ok(()));
}

/// A fault that is not the game's stays what it was: the process ends by
/// it, whether none of the game runs then or the fault is the host's own,
/// in an import the game called (the guarded memory's handler passes both
/// on). Each case runs in a process of its own, this test started again.
#[cfg(all(has_game_module, unix))]
#[test]
fn a_fault_that_is_not_the_games_ends_the_process_as_ever() {
    use std::os::unix::process::ExitStatusExt;
    use xem_core::memory::GameMemory;
    use xem_core::module::{Action, Import, ImportHandler};
    use xem_core::GameModule;

    if std::env::var_os("XEM_FIBER_MODULE").is_none() {
        return;
    }
    // A read of an address nothing maps.
    fn fault() -> u8 {
        unsafe { std::ptr::read_volatile(std::ptr::without_provenance::<u8>(16)) }
    }
    struct Faulting;
    impl ImportHandler for Faulting {
        fn import(&mut self, _: Import<'_>, _: &mut dyn GameMemory) -> Action {
            Action::Return(u32::from(fault()))
        }
    }
    match std::env::var("XEM_FIBER_FAULT").as_deref() {
        Ok("idle") => {
            let _module = xem_game::NativeModule::new();
            fault();
        }
        Ok("import") => {
            let mut module = xem_game::NativeModule::new();
            let mut handler = Faulting;
            // SAFETY: the handler outlives the run below, which alone uses it.
            unsafe { module.set_import_handler(&mut handler as *mut dyn ImportHandler) };
            let _ = module.run(0, 0);
        }
        _ => {
            for case in ["idle", "import"] {
                let status = std::process::Command::new(std::env::current_exe().expect("this test's binary"))
                    .args(["--exact", "a_fault_that_is_not_the_games_ends_the_process_as_ever", "--test-threads=1"])
                    .env("XEM_FIBER_FAULT", case)
                    .stdout(std::process::Stdio::null())
                    .stderr(std::process::Stdio::null())
                    .status()
                    .expect("this test again");
                // SIGSEGV.
                assert_eq!(status.signal(), Some(11), "{case}: {status:?}");
            }
        }
    }
}
