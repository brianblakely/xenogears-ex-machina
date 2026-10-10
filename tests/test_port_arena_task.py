"""The arena task switch (port/arena_task.c over port/fiber.c) against the
contracts of decomp/src/menu/arena_task_*.s, natively (tests/port_native.py),
with the host's task_switch import recorded (tests/port_arena_task/support.c).
The fiber switch itself is the runtime's (runtime/crates/xem-core/tests/fibers.rs);
these tests check the game-visible words and what the port asks the host."""

import ctypes
import tempfile
import unittest

from tests import port_native as mem

SYMBOLS = {
    "arena_task_caller_stack": 0x80096D88,
    "arena_current_task": 0x80096D8C,
}
CALLER_STACK = SYMBOLS["arena_task_caller_stack"]
CURRENT_TASK = SYMBOLS["arena_current_task"]
TASK_STACK = 0x801FE000  # arena_mode_main's 0x400-word task stack
TASK_A = 0x80120000
TASK_B = 0x80120100
CONTEXT = 0x80130000

LIB = None
BUILD = None


def setUpModule():
    global LIB, BUILD
    if not mem.CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = mem.build(
        BUILD.name, ["port/arena_task.c", "port/fiber.c", "tests/port_arena_task/support.c"], SYMBOLS
    )
    LIB.arena_task_resume.argtypes = [ctypes.c_void_p]
    LIB.arena_task_save_scheduler.argtypes = [ctypes.c_void_p]
    LIB.arena_task_restore_scheduler.argtypes = [ctypes.c_void_p]
    LIB.xem_fiber_resume_task.argtypes = [ctypes.c_uint]
    LIB.xem_unwind_area.restype = ctypes.c_void_p


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def switches():
    calls = ctypes.c_uint.in_dll(LIB, "test_switch_calls").value
    fibers = (ctypes.c_uint * 16).in_dll(LIB, "test_switch_fiber")
    tops = (ctypes.c_uint * 16).in_dll(LIB, "test_switch_stack_top")
    return [(fibers[i], tops[i]) for i in range(calls)]


def make_task(address, entry, arg):
    """arena_task_create's context: regs zeroed, gp, ra = entry, a0 = arg, sp = fp = stack top."""
    regs = [0] * 32
    regs[28] = 0x80060000
    regs[31] = entry
    regs[4] = arg
    regs[29] = regs[30] = TASK_STACK + 0x1000
    mem.put32(address, *regs, TASK_STACK)


class ArenaTask(unittest.TestCase):
    def setUp(self):
        mem.clear()
        LIB.xem_fiber_reset()
        ctypes.c_uint.in_dll(LIB, "test_switch_calls").value = 0

    def test_save_and_restore_copy_the_two_scheduler_words(self):
        mem.put32(CALLER_STACK, 0x801FFE40, TASK_A)
        LIB.arena_task_save_scheduler(CONTEXT)
        self.assertEqual((mem.u32(CONTEXT), mem.u32(CONTEXT + 4)), (0x801FFE40, TASK_A))
        mem.put32(CALLER_STACK, 0x11111111, 0x22222222)
        LIB.arena_task_restore_scheduler(CONTEXT)
        self.assertEqual((mem.u32(CALLER_STACK), mem.u32(CURRENT_TASK)), (0x801FFE40, TASK_A))
        self.assertEqual((mem.u32(CONTEXT), mem.u32(CONTEXT + 4)), (0x801FFE40, TASK_A))
        self.assertEqual(switches(), [])

    def test_resume_sets_the_scheduler_words_and_starts_a_new_context_once(self):
        make_task(TASK_A, 0x80088000, 0)
        before = mem.read(TASK_A, 0x84)
        LIB.arena_task_resume(TASK_A)
        self.assertEqual(mem.u32(CURRENT_TASK), TASK_A)
        self.assertNotEqual(mem.u32(CALLER_STACK), 0)
        LIB.arena_task_resume(TASK_A)
        (fiber, top), (fiber2, top2) = switches()
        # The first resume starts the task fiber on its shadow stack, the next rewinds it.
        self.assertEqual((fiber, fiber2), (1, 1))
        self.assertNotEqual(top, 0)
        self.assertEqual(top % 16, 0)
        self.assertEqual(top2, 0)
        # Another context starts the fiber afresh.
        LIB.arena_task_resume(TASK_B)
        self.assertEqual(switches()[2], (1, top))
        self.assertEqual(mem.u32(CURRENT_TASK), TASK_B)
        # Nothing writes the context or the task's MIPS stack.
        self.assertEqual(mem.read(TASK_A, 0x84), before)
        self.assertEqual(mem.read(TASK_STACK, 0x1000), bytes(0x1000))

    def test_yield_switches_to_the_game_and_keeps_the_current_task(self):
        make_task(TASK_A, 0x80088000, 0)
        LIB.arena_task_resume(TASK_A)
        words = mem.read(CALLER_STACK, 8)
        context = mem.read(TASK_A, 0x84)
        LIB.arena_task_yield()
        self.assertEqual(switches()[-1], (0, 0))
        self.assertEqual(mem.read(CALLER_STACK, 8), words)
        self.assertEqual(mem.read(TASK_A, 0x84), context)

    def test_each_fiber_unwinds_into_its_own_area(self):
        game = LIB.xem_unwind_area()
        LIB.arena_task_resume(TASK_A)  # returns as the game fiber, rewound
        self.assertEqual(LIB.xem_unwind_area(), game)
        LIB.arena_task_yield()  # returns as the task fiber, rewound
        task = LIB.xem_unwind_area()
        self.assertNotEqual(task, game)
        words = (ctypes.c_uint * 2).from_address(task)
        self.assertEqual(words[1] - words[0], 0x10000 * 4)
        LIB.xem_fiber_reset()
        self.assertEqual(LIB.xem_unwind_area(), game)


if __name__ == "__main__":
    unittest.main()
