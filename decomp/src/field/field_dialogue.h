#ifndef FIELD_FIELD_DIALOGUE_H
#define FIELD_FIELD_DIALOGUE_H

/* Dialogue windows (8007dc..800808xx, field_motion.c): the four windows at
 * 800c2698 with their frames, choices and prompts, their text texture
 * windows and draw modes, the message table and the portraits the events
 * show beside them (8009c154, field_event.c). */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/window.h"
#include "field.h"

/* A dialogue window's frame (window + 0ac): its area and the backing and
 * border packets. The window code addresses these members from the frame,
 * so its address (800c2744 + window) is the base it keeps. */
typedef struct {
    RECT rect;                  /* 00 (0AC): the window's area */
    u8 unk08[0x18 - 0x08];
    DR_MODE back_modes[2];      /* 18 (0C4): per buffer */
    TILE back[2];               /* 30 (0DC): the backing tile per buffer */
    DR_MODE border_modes[2][10]; /* 50 (0FC): per buffer, eight used */
    SPRT border[2][10];         /* 140 (1EC): per buffer, eight used */
} DialogueFrame;

/* A dialogue window's choice (window + 37c): its state, the selectable
 * lines and the cursor packets. The window code addresses the packets from
 * this struct's base. */
typedef struct {
    s16 status;       /* 00 (37C): zero while a choice is shown */
    s16 first;        /* 02 (37E): first selectable line */
    s16 count;        /* 04 (380): selectable line count */
    s16 index;        /* 06 (382): selected line */
    DR_MODE modes[2]; /* 08 (384): per buffer */
    SPRT cursor[2];   /* 20 (39C): the choice cursor per buffer */
} DialogueChoice;

/* A dialogue window's waiting prompt (window + 3c4). */
typedef struct {
    s16 status;       /* 00 (3C4): zero while waiting for the player */
    u8 unk02[2];
    DR_MODE modes[2]; /* 04 (3C8): per buffer */
    SPRT sprite[2];   /* 1C (3E0): the waiting prompt per buffer */
} DialoguePrompt;

/* One of the four 0x498-byte dialogue windows at 800c2698. */
typedef struct DialogueWindow {
    DR_MODE modes[2]; /* 000: per buffer */
    Window text;     /* 018: its text (resident/window.h) */
    s32 message;     /* 0A8: the message 80033728 found, shown by 80034714 */
    DialogueFrame frame; /* 0AC */
    DialogueChoice choice; /* 37C */
    DialoguePrompt prompt; /* 3C4 */
    s16 timer;       /* 408: opening steps left */
    s16 prompt_delay; /* 40A */
    u16 style;       /* 40C: 1 above, 0x81 below the speaker; 0x20 portrait on
                      * the right, 0x40 no frame (kept) */
    s16 busy;        /* 40E */
    u16 age;         /* 410: 0xffff when free */
    s16 unk412;      /* 412 */
    s16 cleared;     /* 414: cleared when its owner hides */
    s16 owner;       /* 416: owning event actor */
    s16 unk418;      /* 418: descriptor index */
    u8 unk41A[2];
    Fixed slide[2];  /* 41C: x, y offset while opening (16.16) */
    s32 slide_step[2]; /* 424 */
    DR_MODE icon_modes[2]; /* 42C: per buffer */
    POLY_FT4 icon[2];  /* 444: per buffer */
    u8 unk494;       /* 494 */
    u8 unk495;       /* 495 */
    u8 unk496[0x498 - 0x496];
} DialogueWindow;

extern DialogueWindow field_dialogue_windows[4];
extern s32 field_dialogue_message_slots[4];      /* message slot per window, -1 free */
extern RECT field_texture_windows[16];    /* text texture windows */
/* Draw modes per buffer, one per text texture window (field_texture_windows): 0 for
 * the text, 1 the compass, 3 the distortion, 4 the grid. */
extern DR_MODE field_texture_window_modes[2][16];
extern void *field_message_table;       /* the field's message table */
extern s32 field_dialogue_cursor_frame;         /* dialogue cursor frame */
extern s32 field_dialogue_tick_count;         /* dialogue ticks */
extern s32 field_dialogue_open_blocked;         /* 1 while the field enters or changes screens: no window opens */
extern s32 field_dialogue_pass_open_count;         /* dialogue windows opened this pass */
extern s16 field_unread_text_pair1[2];      /* only cleared (80077620, the text images) */
extern s16 field_unread_text_pair2[2];      /* only cleared (80077620) */

s32 field_dialogue_open_window(s16 x, s16 y, s32 message, s32 window, s32 columns, s32 rows, s32 owner, s32 speaker,
                  s32 mode, s32 turned, s32 flags); /* open a window */
void field_dialogue_reset_windows(void);                        /* reset the text texture windows and the windows */
void field_dialogue_build_packets(s32 window);                 /* build a window's packets */
void field_load_text_palette(void);                       /* load the text palette */
void field_load_tim_list(u32 *tim);                   /* load the images of a TIM list */
void field_dialogue_draw_frame(u_long *ot, s32 buffer, s32 window); /* draw a window's frame */
void field_dialogue_draw_windows(u_long *ot, s32 buffer);     /* draw the windows */
void field_dialogue_close_expired_windows(void);                       /* close the idle windows whose time ran out */
s32 field_dialogue_close_window(s16 window);                  /* close a window unless busy; -1 when busy */
void field_dialogue_close_all_windows(void);                       /* close every window that is not busy */
s32 field_dialogue_is_full(void);                        /* 0 when a window is free, else -1 */
s32 field_dialogue_find_oldest_window(void);                        /* the oldest window in use, or 0xffff */
s32 field_dialogue_take_free_window(void);                        /* take the first free window, or 0xffff */
void field_descriptor_get_screen_point(s32 index, s32 *x, s32 *y, s32 height); /* screen point above a descriptor */
s32 field_dialogue_is_portrait_shown(s32 id);                      /* -1 when an idle window shows message kind 1 for `id` */
void field_dialogue_mark_window_open(s32 bit);                    /* set talk-inhibit bit `bit` */

/* Dialogue portraits (8009c154): field_dialogue_portrait_slots[slot] holds .a the character,
 * .b the state (1 loaded, 2 shown) and .c whether a second image is used. */
typedef struct FieldSlot6 {
    s16 a;
    s16 b;
    s16 c;
} FieldSlot6;

typedef struct {
    s16 x;
    s16 y;
    s16 clut_x;
    s16 clut_y;
} PortraitPlace;

extern FieldSlot6 field_dialogue_portrait_slots[3];
extern PortraitPlace field_dialogue_portrait_places[4][2]; /* VRAM place per slot and image */
extern u8 field_dialogue_portrait_files[][2];             /* portrait files per character, - 0x46 */
extern void *field_dialogue_portrait_first_image;               /* first portrait image */
extern void *field_dialogue_portrait_second_image;               /* second portrait image */
extern FileRequest field_dialogue_portrait_file_requests[3];      /* file list read by 80029afc */
extern s32 field_dialogue_portrait_last_slot;
void field_dialogue_reset_portrait_slots(void);              /* reset the slots and 800adb0c */

#endif
