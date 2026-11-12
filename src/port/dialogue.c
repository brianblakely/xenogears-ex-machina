/*
 * The field's dialogue windows for presentation (docs/runtime.md, Hiding a
 * dialogue). The original draws the four windows (8008004C: their frames,
 * text, the speakers' portraits, the waiting prompts and the choice
 * cursors) by linking every packet of theirs at the ordering-table entry
 * it is given, the overlay's first, which the field draws last; it is run
 * unchanged. The packets it linked are one run of that entry's list, from
 * the entry's new head to the packet whose link is what the entry held
 * before. The host gets the run's newest and oldest packets (presentation
 * record 6, as for a battle's script message: battle_dialogue.c), which a
 * draw trace keeps to mark the primitives the GPU draws from the run
 * (xem_gpu::Primitive::dialogue); the hosts leave those out of the picture
 * while the player holds Select over a dialogue. The record is made
 * whether or not a trace records, as the sprites' are (present.c), so the
 * port's memory is the same either way. Nothing reaches the game: its
 * packets, links and RAM are as the original left them. The port keeps no
 * data of its own.
 */
#include "common.h"
#include "xem/memory.h"
#include "../decomp/src/field/field_dialogue.h"

/* Hand a record to the host: `kind`, then `length` bytes at `data`. */
void xem_host_present(u32 kind, void *data, u32 length);

void xem_original_field_dialogue_draw_windows(u_long *ot, s32 buffer);

/* Record kind (src/runtime/crates/xem-core/src/presentation.rs). */
#define XEM_PRESENT_DIALOGUE 6

/* The most links followed to a run's oldest packet: the four windows link
 * a few hundred at most. */
#define XEM_DIALOGUE_LINKS 4096

/* Hands the host a dialogue's run of packets from `newest` to `oldest`
 * (24-bit links). */
void xem_dialogue_present(u32 newest, u32 oldest) {
    u32 run[2];

    run[0] = newest;
    run[1] = oldest;
    xem_host_present(XEM_PRESENT_DIALOGUE, run, sizeof(run));
}

/* Hands the host the packets linked at `entry` since it held `before`: its
 * newest (the entry's head), then each link up to the oldest. */
void xem_dialogue_present_linked(u_long *entry, u32 before) {
    u32 newest = getaddr(entry);
    u32 oldest = newest;
    u32 next;
    s32 i;

    for (i = 0; newest != before && i < XEM_DIALOGUE_LINKS; i++) {
        next = XEM_U32(oldest) & 0xFFFFFF;
        if (next == before) {
            xem_dialogue_present(newest, oldest);
            return;
        }
        oldest = next;
    }
}

void field_dialogue_draw_windows(u_long *ot, s32 buffer) {
    u32 before = getaddr(ot);

    xem_original_field_dialogue_draw_windows(ot, buffer);
    xem_dialogue_present_linked(ot, before);
}
