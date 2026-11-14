"""Loop polls: every back-edge suspends in time, and a wait for the disc's reads without spinning."""
import ctypes
import tempfile
import unittest

from tests import port_native
from tools.game_module import poll_back_edges

POLL = '  call void @"xem_loop_poll"()'
SPIN = '  call void @"xem_loop_spin"()'


class BackEdges(unittest.TestCase):
    def test_every_back_edge_polls_and_a_wait_for_the_discs_reads_spins(self):
        lines = [
            "define dso_local i32 @cd_sync_reads(i32 noundef signext %0) #0 {",
            "  %2 = icmp eq i32 %0, 0",
            "  br i1 %2, label %3, label %8",
            "",
            "3:                                                ; preds = %1",
            "  br label %4",
            "",
            "4:                                                ; preds = %7, %3",
            "  %5 = call i32 @cd_get_pending_read_count() #3",
            "  %6 = icmp sgt i32 %5, 0",
            "  br i1 %6, label %7, label %8",
            "",
            "7:                                                ; preds = %4",
            "  br label %4",
            "",
            "8:                                                ; preds = %4, %1",
            "  ret i32 0",
            "}",
        ]
        rewritten = poll_back_edges(lines)
        self.assertEqual(rewritten[:13] + rewritten[14:], lines)
        self.assertEqual(rewritten[13:15], [SPIN, "  br label %4"])

    def loop(self, header, body=()):
        lines = ["define void @wait() {", "  br label %1", "", "1:", *header, "", "2:", *body, "  br label %1", "", "3:", "  ret void", "}"]
        rewritten = poll_back_edges(lines)
        inserted = [line for line in rewritten if line in (POLL, SPIN)]
        self.assertEqual(len(inserted), 1)
        self.assertEqual([line for line in rewritten if line not in (POLL, SPIN)], lines)
        return inserted[0]

    def test_only_a_loop_that_does_nothing_but_read_the_count_spins(self):
        read = "  %5 = call i32 @cd_get_pending_read_count() #4"
        test = "  %6 = icmp sge i32 %5, 3"
        branch = "  br i1 %6, label %2, label %3"
        self.assertEqual(self.loop([read, test, branch]), SPIN)
        self.assertEqual(self.loop([read, "  %6 = icmp ne i32 %5, 0", branch]), SPIN)
        # A body that does anything, a header that does more, another count,
        # another test or the loop left while the test holds: the plain poll.
        self.assertEqual(self.loop([read, test, branch], ["  call void @battle_wait_frame()"]), POLL)
        self.assertEqual(self.loop(["  store i32 1, ptr @seen", read, test, branch]), POLL)
        self.assertEqual(self.loop(["  %5 = call i32 @cd_get_file_size(i32 1)", test, branch]), POLL)
        self.assertEqual(self.loop([read, "  %6 = icmp sge i32 %4, 3", branch]), POLL)
        self.assertEqual(self.loop([read, test, "  br i1 %5, label %2, label %3"]), POLL)
        self.assertEqual(self.loop([read, test, "  br i1 %6, label %3, label %2"]), POLL)

    def test_a_loop_back_to_a_block_that_tests_nothing_polls(self):
        lines = ["define void @forever() {", "  br label %1", "", "1:", "  call void @step()", "  br label %1", "}"]
        self.assertEqual(poll_back_edges(lines), lines[:5] + [POLL] + lines[5:])
        # Labels are each function's own.
        twice = lines + lines
        self.assertEqual(poll_back_edges(twice), (lines[:5] + [POLL] + lines[5:]) * 2)


class Spin(unittest.TestCase):
    """xem_loop_spin beside the loop it stands for (src/port/boot.c), natively."""

    @classmethod
    def setUpClass(cls):
        if not port_native.CLANG:
            raise unittest.SkipTest("needs the pinned runtime clang")
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        support = f"{cls.build.name}/spin_support.c"
        with open(support, "w") as file:
            file.write('''#include "common.h"
#include "resident/cd.h"
#include "resident/mode.h"
#include "xem/port.h"
s32 cd_pending_read_count;
s32 mode_next_mode;
s32 xem_in_interrupt;
u32 test_yields, test_critical, test_facing, test_interrupt_after;
void (*heap_report_output)(char *line);
u8 boot_bss_last_word[4];
s32 xem_in_critical_section(void) { return test_critical; }
s32 xem_present_sprite_facing_active(void) { return test_facing; }
/* A wait: the drive's interrupt, once enough of them came, ends the reads. */
void xem_host_yield(int reason) {
    test_yields++;
    if (test_interrupt_after != 0 && --test_interrupt_after == 0) { cd_pending_read_count = 0; }
}
unsigned int xem_host_poll_now(void) { return 0; }
void xem_host_bad_call(unsigned int address, int signature) {}
void xem_host_missing(int id) {}
void xem_host_mode(int mode) {}
void xem_host_input_read(unsigned int state) {}
__attribute__((noreturn)) void xem_host_restart(int kind, int arg) { for (;;) {} }
u32 xem_original_pad_dequeue_state(void) { return 0; }
void xem_original_mode_dispatch(s32 error) {}
void boot_main(void) {}
void xem_libapi_power_on(void) {}
void xem_libetc_power_on(void) {}
void xem_libcd_power_on(void) {}
void xem_fiber_reset(void) {}
void xem_loop_poll(void);
void xem_loop_spin(void);
/* As after a wait: no iteration counted since the game last suspended. */
void test_suspend(void) {
    u32 yields = test_yields;
    test_critical = test_facing = test_interrupt_after = 0;
    while (test_yields == yields) { xem_loop_poll(); }
}
/* Iterations other loops ran since then. */
void test_iterate(u32 count) {
    while (count--) { xem_loop_poll(); }
}
/* The game's loop as it is built, its poll the plain one or the spin's:
 * the iterations it ran, or -1 once `limit` passed without its end. */
s32 test_wait(s32 spin, s32 limit) {
    s32 iterations = 0;
    while (cd_pending_read_count > 0) {
        if (++iterations > limit) { return -1; }
        if (spin) { xem_loop_spin(); } else { xem_loop_poll(); }
    }
    return iterations;
}
''')
        # boot.c takes the host's declarations from the module build's
        # prelude, and calls game code by its 32-bit address.
        cls.lib = port_native.build(cls.build.name, ["src/port/boot.c", support],
                                    flags=["-include", "src/port/include/xem/port.h", "-Wno-int-to-pointer-cast", "-Wno-comment"])

    def wait(self, spin, pending=2, interrupts=3, before=0, critical=0, facing=0, limit=1 << 20):
        lib = self.lib
        value = lambda name: ctypes.c_uint32.in_dll(lib, name)  # noqa: E731
        lib.test_suspend()
        lib.test_iterate(before)
        for name, to in (("cd_pending_read_count", pending), ("test_yields", 0), ("test_critical", critical), ("test_facing", facing),
                         ("test_interrupt_after", interrupts)):
            value(name).value = to
        return lib.test_wait(spin, limit), value("test_yields").value

    def test_the_spin_suspends_as_often_as_the_loop_without_its_iterations(self):
        for before in (0, 1, 0x7FFF, 0xFFFE):
            for interrupts in (1, 2, 5):
                with self.subTest(before=before, interrupts=interrupts):
                    plain, plain_yields = self.wait(0, interrupts=interrupts, before=before)
                    spun, spun_yields = self.wait(1, interrupts=interrupts, before=before)
                    self.assertEqual((plain_yields, spun_yields), (interrupts, interrupts))
                    self.assertEqual(plain, interrupts * 0x10000 - before)
                    self.assertEqual(spun, interrupts)

    def test_no_reads_pending_ends_at_once_and_a_loop_that_cannot_suspend_spins_on(self):
        self.assertEqual(self.wait(1, pending=0), (0, 0))
        # In a critical section the poll never suspends: the loop goes on as
        # the plain one does (the test gives up on both).
        self.assertEqual(self.wait(0, critical=1, limit=0x30000), (-1, 0))
        self.assertEqual(self.wait(1, critical=1, limit=0x30000), (-1, 0))
        # While sprite facings replay, polls count nothing: neither does the spin.
        self.assertEqual(self.wait(0, facing=1, limit=0x30000), (-1, 0))
        self.assertEqual(self.wait(1, facing=1, limit=0x30000), (-1, 0))


if __name__ == "__main__":
    unittest.main()
