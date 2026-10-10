/* The host import port/arena.c calls, as a test-owned recorder
 * (tests/test_port_arena.py): the break codes of the trapping divisions. */
unsigned int test_break_codes[8];
int test_break_count;

void xem_host_debug_break(unsigned int code) {
    if (test_break_count < 8) {
        test_break_codes[test_break_count] = code;
    }
    test_break_count++;
}
