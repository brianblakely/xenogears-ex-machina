#ifndef BATTLE_AREA_H
#define BATTLE_AREA_H

#include "common.h"
#include "psyq/libgpu.h"
#include "resident/sprite.h"
#include "battle/work.h"

/* The battle area D_800C3EB0 (0x8E38 bytes and its work area): the formation,
 * the slots, the presentation events, the walk, the display buffers, the
 * controller state, the slots' sprites and tasks. The battle modules
 * (0x801fc000), the battle setup and script overlays and the debug pages
 * use it with the battle overlay. */

/* Per-slot formation information and placement (0x1C bytes, D_800C3EB4). */
typedef struct {
    u8 group;  /* formation group */
    u8 member;
    u8 field2; /* 0x7F none */
    u8 hidden; /* 0x03 */
    u8 gear;   /* 0x04: fights in a gear */
    u8 pad5;
    u8 targetCode; /* 0x06: from the default target (80085310) */
    u8 pad7[0xA - 0x7];
    s16 x; /* 0x0A */
    s16 z; /* 0x0C */
    s16 y; /* 0x0E */
    u8 pad10[0x1C - 0x10];
} BattleSlot;

/* Presentation event queue slot (0x48 bytes, from D_800C3FE8). */
typedef struct {
    u16 amounts[11];
    u16 targetMask;          /* 0x16 */
    u8 codes[11];            /* 0x18 */
    u8 actor;                /* 0x23 */
    u16 accumulated[11];     /* 0x24 */
    u16 parameter;           /* 0x3A */
    u8 accumulatedCodes[11]; /* 0x3C */
    u8 type;                 /* 0x47: 0xF7 continues, 0xFF ends */
} BattleEvent;

/* One of the two display buffers (0x4070 bytes). */
typedef struct {
    DRAWENV drawEnv;
    DISPENV dispEnv;   /* 0x5C */
    u32 ot[0x1000];    /* 0x70: cleared in reverse */
} FrameBuffer;

/* Controller state history entry (8 bytes). */
typedef struct {
    u16 held;
    u16 pressed;
    u16 released;
    u16 time;
} PadRecord;

/* A point of the battle menu's walk (6 bytes); x and z 0xFFFF end it. */
typedef struct {
    u16 x;
    u16 z;
    u8 run; /* 0x04 */
    u8 pad5;
} PathPoint;

/* A slot's sprite source (0xC bytes). */
typedef struct {
    void *data;
    s16 x;
    s16 y;
    s32 variant;
} SlotSource;

/* Battle state from D_800C3EB0: the frame loop and result screens address
 * the work table through this aggregate. */
typedef struct BattleArea {
    struct Formation *formation; /* 0x0000 */
    BattleSlot slots[11];       /* 0x0004 */
    BattleEvent events[32];     /* 0x0138 */
    u16 knockedOut;            /* 0x0A38: D_800C48E8 */
    u8 outcome;               /* 0x0A3A: D_800C48EA */
    u8 padA3B;
    PathPoint path[51];         /* 0x0A3C */
    u8 padB6E[2];
    FrameBuffer buffers[2];     /* 0x0B70 */
    FrameBuffer *current;       /* 0x8C50 */
    u32 *ot;                    /* 0x8C54 */
    u16 held;                   /* 0x8C58 */
    u16 held2;                  /* 0x8C5A: the second controller */
    u16 pressed;                /* 0x8C5C */
    u16 pressed2;               /* 0x8C5E */
    u16 released;               /* 0x8C60 */
    u16 heldOnly;               /* 0x8C62: held on the first, not the second */
    PadRecord history[4];       /* 0x8C64: the last changes, newest first */
    s32 buffer;                 /* 0x8C84: the buffer being drawn */
    u8 pad8C88[4];
    Sprite *sprites[11];        /* 0x8C8C: the slots' sprites */
    SpriteTask *tasks[11];      /* 0x8CB8: their tasks */
    u8 pad8CE4[0x8D24 - 0x8CE4];
    SlotSource sources[11];     /* 0x8D24 */
    u8 field8DA8;               /* 0x8DA8 */
    u8 pad8DA9[0x8DAC - 0x8DA9];
    s32 frameTicks;             /* 0x8DAC: vertical blanks of the last frame */
    u8 pad8DB0[0x8E38 - 0x8DB0];
    BattleWork work;            /* 0x8E38: D_800CCCE8 */
} BattleArea;

/* The battle area: one global from 800c3eb0 (the other battle overlays
 * declare it with their own view). Member accesses fold into the symbol
 * (D_800C3EB0+4 for the slots, which splat also labels D_800C3EB4); code
 * that addresses the area from its address in a register (8008a684's stores,
 * the late units) uses BATTLE_AREA. */
extern BattleArea D_800C3EB0;
#define BATTLE_AREA (*(BattleArea *)(void *)&D_800C3EB0)

LAYOUT_CHECK(BattleAreaLayout, sizeof(BattleSlot) == 0x1C && sizeof(BattleEvent) == 0x48 &&
                                   sizeof(PathPoint) == 6 && sizeof(FrameBuffer) == 0x4070 &&
                                   OFFSET_OF(BattleArea, knockedOut) == 0xA38 &&
                                   OFFSET_OF(BattleArea, outcome) == 0xA3A &&
                                   OFFSET_OF(BattleArea, buffers) == 0xB70 &&
                                   OFFSET_OF(BattleArea, buffer) == 0x8C84 &&
                                   OFFSET_OF(BattleArea, sources) == 0x8D24 &&
                                   OFFSET_OF(BattleArea, frameTicks) == 0x8DAC &&
                                   OFFSET_OF(BattleArea, work) == 0x8E38);

#endif
