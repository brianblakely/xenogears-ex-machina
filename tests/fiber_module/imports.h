/* Force-included by tests/fiber_module/build.sh: the host imports as the game
 * module names them (tools/game_module.py gives port units the same). */
__attribute__((import_module("xem"), import_name("yield"))) void xem_host_yield(int reason);
__attribute__((import_module("xem"), import_name("restart"))) void xem_host_restart(int kind, int arg);
__attribute__((import_module("xem"), import_name("task_switch"))) void xem_host_task_switch(unsigned int fiber,
                                                                                          unsigned int stack_top);
