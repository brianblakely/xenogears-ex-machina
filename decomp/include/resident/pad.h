#ifndef RESIDENT_PAD_H
#define RESIDENT_PAD_H

#include "common.h"

/* Controller receive buffer of one port (PsyQ libpad layout). */
typedef struct PadBuffer {
    u8 status;     /* 0 on a successful read, 0xFF without a controller */
    u8 type;       /* high nibble: controller kind */
    u8 buttons[2]; /* active low */
    u8 data[30];
} PadBuffer;

/* Vibration of one controller: the libpad actuator bytes and a timer. */
typedef struct {
    u8 act[4];
    s16 timer;    /* frames left at full strength */
    u8 state;     /* 1 running, 2 winding down */
    u8 disabled;
} Actuator;

extern PadBuffer pad_receive_buffers[];
extern u16 pad_port0_held;    /* held pad buttons */
extern u16 pad_port0_pressed;    /* pad buttons pressed */
extern u16 pad_port0_repeated;    /* pad buttons repeated */
/* The vertical blank count pad_vblank_count (an s32 that 8003634c increments; saves
 * keep it as the play time in frames) is declared by its users: the mode 4
 * menu reads it as volatile at each use (decomp/src/menu/resident_views.h),
 * the other targets as a plain s32 (volatile, 8003634c's increment would
 * load it again after the store). */
extern u8 pad_play_time_hours;     /* play time hours */
extern u8 pad_play_time_minutes;     /* play time minutes */
extern u8 pad_play_time_seconds;     /* play time seconds */
extern u16 pad_button_bits[8]; /* button bits */
extern u8 pad_button_assignment[8];  /* button assignment */
extern u8 pad_dpad_stick_x_table[16];
extern u8 pad_dpad_stick_y_table[16];

s32 pad_get_controller_kind(s32 port);
u32 pad_dequeue_state(void); /* next queued pad entry (8005 94a4), 0 when none */
void pad_clear_queue(void);
void pad_start_controllers(void);
void pad_vblank_callback(void);
void pad_set_vblank_hook(void (*hook)(void));
s32 pad_has_queue_overflowed(void);

/* More of the controller services and their state. */
s32 pad_read_buttons(s32 port);
u8 pad_get_dpad_stick_x(s32 buttons);
u8 pad_get_dpad_stick_y(s32 buttons);
void pad_merge_queued_states(void);
extern u16 mode_battle_turn_count;
extern u16 pad_port1_pressed;
extern u16 pad_port1_repeated;
extern u16 pad_port1_held;

/* Stick positions (analog bytes 0-1; mode.h has bytes 2-3, which a digital
 * pad's directional buttons set) and controller states cleared with the
 * queue (80035db0), in the commons. */
extern u8 pad_port0_right_stick_x, pad_port0_right_stick_y; /* first port */
extern u8 pad_port1_right_stick_x, pad_port1_right_stick_y; /* second port */
extern u16 pad_port0_unread_buttons_a, pad_port1_unread_buttons_a, pad_port0_unread_buttons_b, pad_port1_unread_buttons_b, pad_port0_unread_buttons_c, pad_port1_unread_buttons_c;

#endif
