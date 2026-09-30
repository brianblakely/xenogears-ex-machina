#ifndef OVL2598_PARTY_MENU_H
#define OVL2598_PARTY_MENU_H

#include "common.h"

/* The shared menu state (*D_800625A0), as far as this overlay uses it. */
typedef struct {
    u8 pad_0[0x2DC];
    void *sprite_sheet; /* 0x2DC: sprite table for func_8002675C */
    void *label_text;   /* 0x2E0: label text offset table */
    void *effect_bank;  /* 0x2E4 */
    u8 pad_2E8[0x308 - 0x2E8];
    u8 buffer_index;     /* 0x308: draw buffer being built (0/1) */
    u8 pad_309[3];
    u8 available[16];    /* 0x30C: character may join the party */
    u8 pad_31C[0x325 - 0x31C];
    u8 input_code;       /* 0x325: decoded input of this frame */
    u8 card_poll_timer;  /* 0x326 */
    u8 active;           /* 0x327 */
    u8 pad_328[0x32C - 0x328];
    u8 *work;            /* 0x32C: 0x5034-byte work block */
    u8 *block_330;       /* 0x330: 0xCC bytes */
    u8 b_334;            /* 0x334 */
    u8 b_335;            /* 0x335 */
    u8 top_cursor;       /* 0x336 */
    u8 b_337;            /* 0x337 */
    u8 pad_338[0x33C - 0x338];
    u8 *party;           /* 0x33C: party list (0x6C bytes; +0x30 three ids) */
    u8 pad_340[0x348 - 0x340];
    u8 *block_348;       /* 0x348: 0x15C bytes */
    u8 pad_34C[0x350 - 0x34C];
    u8 *block_350;       /* 0x350: 0x1194 bytes */
    u8 *block_354;       /* 0x354: 0x140C bytes */
    u8 pad_358[0x1DF0 - 0x358];
    u8 *member_panels[6]; /* 0x1DF0: 0xBEC bytes each */
    u8 *party_panels[3];  /* 0x1E08: 0xBEC bytes each */
} MenuState;

extern MenuState *D_800625A0;

extern void *func_80031BDC(s32 size, s32 mode); /* allocate */
extern void func_800320E8(void *block);         /* release */
extern void func_8003F8E8(void *dst, s32 size); /* bzero */

#endif
