/* Focus loss requests the original pause screens through their existing Start
 * checks. All original eligibility checks and resume input remain in the game.
 * No focus or application settings live in the module's saved memory. */
#include "common.h"
#include "../decomp/src/field/field.h"

extern u16 field_pad_port0_allowed_mask;
extern s32 field_camera_cut_timer;
extern u8 battle_turns_active;
extern u8 arena_retreat_rule_enabled;
extern u16 sound_driver_flags;
extern u8 arena_menu_paused;

/* Read original game memory directly, including after a state restore. The
 * browser's optional inspection schema is not needed to recognize a pause.
 * Arena overlay memory is only meaningful while that mode is running. */
u32 xem_game_paused(u32 mode) {
    return (sound_driver_flags & 0x40) != 0 || (mode == 4 && arena_menu_paused != 0);
}

u32 xem_host_pause_requested(void);
void xem_host_pause_reached(void);
void xem_original_sound_silence_voices(void);
void xem_original_arena_menu_open_pause(s32 mode);

u32 xem_pause_start(u32 buttons) {
    return (buttons & 0x800) | (xem_host_pause_requested() ? 0x800 : 0);
}

u32 xem_field_pause_start(u32 buttons) {
    /* The original queue drain masks Start during camera cuts and script
     * input locks before field_main reads it. Respect those filters as well. */
    if (field_camera_cut_timer != 0 || !(field_work.input_mask & field_pad_port0_allowed_mask & 0x800)) {
        return buttons & 0x800;
    }
    return xem_pause_start(buttons);
}

u32 xem_battle_pause_start(u32 buttons) {
    /* Start otherwise sets battle input code 0xE without opening a pause. */
    return battle_turns_active ? xem_pause_start(buttons) : buttons & 0x800;
}

u32 xem_arena_pause_start(u32 buttons) {
    return arena_retreat_rule_enabled ? buttons & 0x800 : xem_pause_start(buttons);
}

void sound_silence_voices(void) {
    xem_host_pause_reached();
    xem_original_sound_silence_voices();
}

void arena_menu_open_pause(s32 mode) {
    if (mode == 1 || mode == 2) {
        xem_host_pause_reached();
    }
    xem_original_arena_menu_open_pause(mode);
}
