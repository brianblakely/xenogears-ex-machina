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

#endif
