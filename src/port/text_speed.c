/*
 * A host option that changes the game (docs/runtime.md, Text speed): a
 * message window reveals its text a few glyphs per step
 * (window_draw_frame, 80034888, sets the window's speed, one glyph per
 * field frame, two where the field's text speed is slower, before
 * window_reveal_text, 80033DF0, takes a step) up to a newline, a page's
 * end, a pause or a timed wait. With the host's text speed
 * (xem_host_text_speed: the glyphs a step reveals at least, nonzero) a
 * step reveals that many: the window still stops at every newline, page,
 * pause and timed wait its text holds, so the scripts that wait on the
 * window wait as before, after a shorter reveal. Authored slow text
 * (0F 0E: a glyph every few frames, unk88) keeps its pace, and the
 * one-line layout window (window_render_text_line) keeps its own budget.
 * The host records its speed with the game's inputs; 0 is the original's.
 * The port keeps no data of its own.
 */
#include "common.h"
#include "resident/window.h"

/* The glyphs a window's step reveals at least (1-255), or 0 for the
 * window's own speed. */
u32 xem_host_text_speed(void);

/* window_render_text_line's one-line layout window (80059FD8). */
extern Window window_single_line_window;

void xem_original_window_reveal_text(Window *window);

void window_reveal_text(Window *window) {
    u32 glyphs = xem_host_text_speed();

    if (glyphs != 0 && window != &window_single_line_window && window->unk88 == 0 && window->unk69 < glyphs) {
        window->unk69 = (u8)glyphs;
    }
    xem_original_window_reveal_text(window);
}
