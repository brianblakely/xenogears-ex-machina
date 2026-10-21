/*
 * A battle's script message for presentation (docs/runtime.md, Hiding a
 * dialogue), as the field's dialogue (dialogue.c). The battle's event
 * script shows its message in battle window 0, with the speaker's
 * portrait, the message text window and, while it waits for Confirm, a
 * cursor. The script's frame mode (2) draws them at the ordering table's
 * second entry among the interface's other packets, in three calls, which
 * run unchanged: the portrait and the text (80074F70: all it links), every
 * shown window (80073FB8: window 0's packets, which lie in its block, are
 * linked first of the windows'), and the list page's glyphs with the cursor
 * (80073F08: its parts, linked one after the other). The turns' frame mode
 * (1) keeps window 0 and the cursor for its commands. The host gets each
 * run's newest and oldest packets (presentation record 6), made whether or
 * not a trace records. Nothing reaches the game: its packets, links and
 * RAM are as the originals left them. The port keeps no data of its own.
 */
#include "common.h"
#include "xem/memory.h"
#include "battle/setup.h"
#include "battle/ui.h"
#include "battle/windows.h"

void xem_dialogue_present(u32 newest, u32 oldest);
void xem_dialogue_present_linked(u_long *entry, u32 before);

void xem_original_battle_draw_script_portrait_and_text(void);
void xem_original_battle_draw_gauge_bars_and_windows(void);
void xem_original_battle_draw_list_page_glyphs(void);

/* The 24-bit DMA address of a packet. */
#define XEM_DIALOGUE_NODE(packet) ((u32)(XemUintptr)(packet) & 0xFFFFFF)

/* The most links followed within window 0's block: it links a few dozen. */
#define XEM_WINDOW_LINKS 256

void battle_draw_script_portrait_and_text(void) {
    u_long *entry = (u_long *)(battle_drawing_state.ot + 1);
    u32 before = getaddr(entry);

    xem_original_battle_draw_script_portrait_and_text();
    xem_dialogue_present_linked(entry, before);
}

/* Window 0's packets: from its draw mode, the last it links, through each
 * link within its block. */
void battle_draw_gauge_bars_and_windows(void) {
    WindowBlock *window;
    u32 block;
    u32 newest;
    u32 oldest;
    u32 next;
    s32 i;

    xem_original_battle_draw_gauge_bars_and_windows();
    if (battle_frame_mode != 2 || battle_ui->windows[0] == 0) {
        return;
    }
    window = battle_window_blocks[0];
    block = XEM_DIALOGUE_NODE(window);
    newest = XEM_DIALOGUE_NODE(&window->mode[window->buffer]);
    oldest = newest;
    for (i = 0; i < XEM_WINDOW_LINKS; i++) {
        next = XEM_U32(oldest) & 0xFFFFFF;
        if (next - block >= sizeof(WindowBlock)) {
            xem_dialogue_present(newest, oldest);
            return;
        }
        oldest = next;
    }
}

/* The cursor's parts, one primitive a part in each draw buffer
 * (battle_add_prims_to_ot): the last linked is the newest. */
void battle_draw_list_page_glyphs(void) {
    POLY_FT4 *cursor;
    s32 parts;

    xem_original_battle_draw_list_page_glyphs();
    if (battle_frame_mode != 2 || battle_ui->cursorShown == 0 || battle_ui->cursorParts <= 0) {
        return;
    }
    cursor = &battle_graphics->cursor[battle_ui->cursorBuffer];
    parts = battle_ui->cursorParts;
    xem_dialogue_present(XEM_DIALOGUE_NODE(&cursor[(parts - 1) * 2]), XEM_DIALOGUE_NODE(cursor));
}
