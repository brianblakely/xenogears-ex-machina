#ifndef RESIDENT_PAD_H
#define RESIDENT_PAD_H

#include "common.h"

/* Controller receive buffer of one port (PsyQ libpad layout). */
typedef struct {
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
extern u8 D_8005938C;
extern u8 D_80059388;     /* kind of the last read controller */
extern u16 D_800501E8[8]; /* button bits */
extern u8 D_80050238[8];  /* button assignment */
extern u8 D_8005020C[16];
extern u8 D_8005021C[16];

#endif
