#ifndef BATTLE_LISTS_H
#define BATTLE_LISTS_H

#include "common.h"
#include "psyq/libgpu.h"

/* The battle HUD's primitive lists (the 800d2db4 block): the glyph lists of
 * the command pages and the gear HUD's values, and the stepped line effect
 * (battle.c 8008860C-80089B50; the gear HUD, 8008CCCC's unit 8009A2D4). */

/* The HUD primitive lists (*800d2db4, a 0x5da4-byte heap block) with their
 * counts and draw buffers. */
typedef struct ListPrims {
    POLY_FT4 extra0[24];      /* +0x0000 count extraCounts[0] */
    POLY_FT4 extra1[60];      /* +0x03C0 count extraCounts[1] */
    POLY_FT4 extra2[16];      /* +0x0D20 count extraCounts[2] */
    POLY_FT4 extra3[48];      /* +0x0FA0 count extraCounts[3] */
    POLY_FT4 extra4[90];      /* +0x1720 count extraCounts[4] */
    POLY_FT4 list9[86];       /* +0x2530 */
    POLY_FT4 unk32A0[52];     /* +0x32A0 count +0x5D96, buffer +0x5D97 */
    POLY_FT4 list0[22];       /* +0x3AC0 */
    POLY_FT4 list1[2];        /* +0x3E30 */
    POLY_FT4 list2[34];       /* +0x3E80 */
    POLY_FT4 list10[18];      /* +0x43D0 */
    POLY_FT4 unk46A0[40];     /* +0x46A0 twenty drawn, buffer +0x5D98 */
    POLY_FT4 unk4CE0[10];     /* +0x4CE0 count +0x5DA1, buffer +0x5DA0 */
    POLY_FT4 list3[8];        /* +0x4E70 */
    POLY_FT4 list4[6];        /* +0x4FB0 */
    POLY_FT4 list5[8];        /* +0x50A0 */
    POLY_FT4 list6[4];        /* +0x51E0 */
    POLY_FT4 list7[8];        /* +0x5280 */
    POLY_FT4 list8[10];       /* +0x53C0 */
    POLY_FT4 list11[6];       /* +0x5550 */
    POLY_FT4 list12[40];      /* +0x5640 */
    POLY_FT4 list13[6];       /* +0x5C80 */
    u8 extraCounts[5];        /* +0x5D70 */
    u8 counts[14];            /* +0x5D75 per list */
    u8 extraBuffer4;          /* +0x5D83 */
    u8 buffers[14];           /* +0x5D84 per list */
    u8 extraBuffers[4];       /* +0x5D92 extra0..extra3 */
    u8 count32A0;             /* +0x5D96 */
    u8 buffer32A0;            /* +0x5D97 */
    u8 buffer46A0;            /* +0x5D98 */
    u8 unk5D99[3];
    u16 lineX;                /* +0x5D9C the stepped line's current point */
    u16 lineY;
    u8 buffer4CE0;            /* +0x5DA0 */
    u8 count4CE0;             /* +0x5DA1 */
    s16 blink;                /* +0x5DA2 frame counter of the blinking list */
} ListPrims;

extern ListPrims *battle_hud_primitive_lists;

/* The glyph lists' values: names of parts of the battle work area's gear HUD
 * (battle.data.ld) and the glyphs of lists extra4 and list9. */
extern u8 battle_gear_hud_fixed_glyph_ids[4];    /* glyph ids: two for list extra4, two for list9 */
extern u8 battle_gear_hud[3][2]; /* values shown in number strings 2-4 */
extern u16 battle_gear_hud_attack;
extern u16 battle_gear_hud_warning_flags;
extern u8 battle_gear_hud_speed;
extern u8 battle_gear_hud_defense;
extern u8 battle_gear_hud_overheat;
extern u16 battle_gear_hud_boost_chance;
extern u8 battle_uses_fixed_party;

/* The stepped line (80088b80). */
extern u8 battle_stepped_line_ended; /* the stepped line reached its end */
extern s32 battle_stepped_line_progress_x;
extern s32 battle_stepped_line_progress_y;
extern s32 battle_stepped_line_end_points[2][5]; /* end points (x, then y) of the stepped line */

void battle_stepped_line_draw(void);      /* draw the stepped line effect */
void battle_gear_hud_build_fuel_glyphs(u8 member); /* build the member's gear fuel glyphs */
void battle_gear_hud_build_lists(u8 member); /* run the eight list building steps */
void battle_gear_hud_fill(u8 member); /* fill the gear HUD (8008CCCC's unit) */

#endif
