/*
 * Port functions for the decomp's inline assembly that is not GTE work
 * (port/asm_map_core.json maps each statement to one of these).
 */
#include "common.h"

/* Stack switches (resident/sprite.h STACK_ENTER/STACK_LEAVE, battle/stage.h,
 * ovl3387's burst effect): the port's calls run on its own stack. */
void xem_asm_stack_enter(void *top) {
}

void xem_asm_stack_leave(void) {
}

/* resident/heap.h GET_RA: the port has no MIPS return address; it records 0,
 * which only the heap report reads. */
void xem_asm_get_return_address(u32 *address) {
    *address = 0;
}

/* `break 1`: the debugger breakpoint of the development configurations. */
void xem_asm_break_1(void) {
    xem_host_debug_break(1);
}

/* `break 1024` (pollhost): the PC file server poll of the development
 * configurations. */
void xem_asm_break_1024(void) {
    xem_host_debug_break(1024);
}

/* Link a 9-word packet at the head of an ordering-table entry: the packet takes
 * the entry's link with length 9, and the entry points at the packet. */
void xem_asm_link_9_words(u32 *entry, u32 *packet) {
    u32 link = *entry | 0x09000000;

    *entry = (u32)packet & 0x00FFFFFF;
    *packet = link;
}
