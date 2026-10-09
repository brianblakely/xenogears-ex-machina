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

extern PadBuffer D_800625FC[];
extern u16 D_80059570;    /* held pad buttons */
extern u16 D_8005948C;    /* pad buttons pressed */
extern u16 D_800594A4;    /* pad buttons repeated */
/* The vertical blank count D_80059488 (an s32 that 8003634c increments; saves
 * keep it as the play time in frames) is declared by its users: the mode 4
 * menu reads it as volatile at each use (decomp/src/menu/resident_views.h),
 * the other targets as a plain s32 (volatile, 8003634c's increment would
 * load it again after the store). */
extern u8 D_80059484;     /* play time hours */
extern u8 D_80059420;     /* play time minutes */
extern u8 D_80059418;     /* play time seconds */
extern u16 D_800501E8[8]; /* button bits */
extern u8 D_80050238[8];  /* button assignment */
extern u8 D_8005020C[16];
extern u8 D_8005021C[16];

s32 func_80035734(s32 port);
u32 func_80035CDC(void); /* next queued pad entry (8005 94a4), 0 when none */
void func_80035DB0(void);
void func_80036288(void);
void func_8003634C(void);
void func_800363F0(void (*hook)(void));
s32 func_80036410(void);

/* More of the controller services and their state. */
s32 func_8003569C(s32 port);
u8 func_80035884(s32 buttons);
u8 func_800358A0(s32 buttons);
void func_80036420(void);
extern u16 D_8005941C;
extern u16 D_80059490;
extern u16 D_800594A8;
extern u16 D_80059574;

/* Stick positions (analog bytes 0-1; mode.h has bytes 2-3, which a digital
 * pad's directional buttons set) and controller states cleared with the
 * queue (80035db0), in the commons. */
extern u8 D_80059444, D_8005944C; /* first port */
extern u8 D_80059448, D_80059450; /* second port */
extern u16 D_800594DC, D_800594E0, D_800594E8, D_800594EC, D_800595C8, D_800595CC;

#endif
