/*
 * The host's sound mode setting chooses the game's own output mode
 * (docs/runtime.md, Sound modes): the driver's Mono, Stereo or Wide-Stereo,
 * as the configuration menu's Sound screen chooses it. The driver takes the
 * host's mode when it starts, and its tick takes a mode the host changed
 * since. The Sound screen tells the host the mode it leaves the game in, and
 * the host's setting follows it. The host records its mode with the game's
 * inputs; without one the game runs as the original. The port keeps no data
 * of its own here: a new port global would move the port's stack, whose
 * addresses the game keeps in RAM.
 */
#include "common.h"

/* The host's mode (sound_set_output_mode's 0 Mono, 1 Stereo, 2 Wide), or
 * XEM_SOUND_MODE_NONE for none; xem_host_sound_mode_change gives it once
 * after each change and XEM_SOUND_MODE_NONE otherwise. */
u32 xem_host_sound_mode(void);
u32 xem_host_sound_mode_change(void);
/* The mode the game's Sound screen leaves the game in. */
void xem_host_sound_mode_chosen(u32 mode);

#define XEM_SOUND_MODE_NONE 0xFFFFFFFF

void sound_set_output_mode(s32 mode);
s32 sound_get_output_mode(void);
void xem_original_sound_start_driver(s32 flags);
s32 xem_original_sound_run_tick(void);
u8 xem_original_menu_sound_mode_screen_run(void);

/* Take the host's mode: the game's, unless it is in that mode already. */
static void xem_sound_take_mode(u32 mode) {
    if (mode != XEM_SOUND_MODE_NONE && (s32)mode != sound_get_output_mode()) {
        sound_set_output_mode(mode);
    }
}

void sound_start_driver(s32 flags) {
    xem_original_sound_start_driver(flags);
    xem_sound_take_mode(xem_host_sound_mode());
}

s32 sound_run_tick(void) {
    xem_sound_take_mode(xem_host_sound_mode_change());
    return xem_original_sound_run_tick();
}

u8 menu_sound_mode_screen_run(void) {
    u8 stay = xem_original_menu_sound_mode_screen_run();

    xem_host_sound_mode_chosen(sound_get_output_mode());
    return stay;
}
