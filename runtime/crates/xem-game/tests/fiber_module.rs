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
