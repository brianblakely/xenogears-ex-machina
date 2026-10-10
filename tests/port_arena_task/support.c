/* The host import the arena task switch calls, recorded: natively it returns
 * at once, as the import does when the host rewinds the calling fiber. */
unsigned int test_switch_calls;
unsigned int test_switch_fiber[16];
unsigned int test_switch_stack_top[16];

void xem_host_task_switch(unsigned int fiber, unsigned int stack_top) {
    if (test_switch_calls < 16) {
        test_switch_fiber[test_switch_calls] = fiber;
        test_switch_stack_top[test_switch_calls] = stack_top;
    }
    test_switch_calls++;
}
