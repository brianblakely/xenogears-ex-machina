#ifndef MENU_SHOP_H
#define MENU_SHOP_H

#include "common.h"
#include "psyq/libgpu.h"
#include "resident/menu.h"

/* The shop screens' lists and prices (mode 5; ovl2601's item shop and
 * ovl2602's Gear parts shop; see menu/screen.h). */

/* The shops' packets (MenuState details): sprite groups with their part
 * counts and buffers, the bars and frames of nine rows, the name labels and
 * the resources unpacked from the menu's resource file. */
typedef struct ShopDetails {
    POLY_FT4 members[18];     /* 0x0000 */
    POLY_FT4 group2D0[18];    /* 0x02d0 */
    POLY_FT4 heading[44];     /* 0x05a0 */
    POLY_FT4 digits1[18];     /* 0x0c80 */
    POLY_FT4 digits2[18];     /* 0x0f50 */
    POLY_FT4 group1220[18];   /* 0x1220 */
    POLY_FT4 digits4[18];     /* 0x14f0: ovl2602 */
    POLY_FT4 price[20];       /* 0x17c0 */
    POLY_FT4 digits3[18];     /* 0x1ae0 */
    POLY_FT4 rows[8][8];      /* 0x1db0 */
    POLY_FT4 cells_a[9][6];   /* 0x27b0 */
    POLY_FT4 cells_b[9][6];   /* 0x3020 */
    LINE_F3 bar_upper[18];    /* 0x3890: two per row */
    LINE_F3 bar_lower[18];    /* 0x3a40 */
    LINE_F2 frame[2];         /* 0x3bf0 */
    u8 unk3C10[0x20];
    MenuLabel names_a[8];     /* 0x3c30 */
    MenuLabel names_b[8];     /* 0x4030 */
    MenuLabel label4430;      /* 0x4430 */
    MenuLabel label44B0;      /* 0x44b0 */
    MenuLabel label4530;      /* 0x4530 */
    MenuLabel label45B0;      /* 0x45b0 */
    void *resources[9];       /* 0x4630: unpacked from the resource file */
    u8 amounts[0x30];         /* 0x4654 */
    u8 name_shown[8];         /* 0x4684 */
    u8 row_count[8];          /* 0x468c */
    u8 row_buffer[8];         /* 0x4694 */
    u8 bar_shown[9];          /* 0x469c */
    u8 members_count;         /* 0x46a5 */
    u8 members_buffer;        /* 0x46a6 */
    u8 label4430_shown;       /* 0x46a7 */
    u8 group2D0_buffer;       /* 0x46a8 */
    u8 group2D0_count;        /* 0x46a9 */
    u8 heading_buffer;        /* 0x46aa */
    u8 heading_count;         /* 0x46ab */
    u8 digits1_buffer;        /* 0x46ac */
    u8 digits1_count;         /* 0x46ad */
    u8 digits2_buffer;        /* 0x46ae */
    u8 digits2_count;         /* 0x46af */
    u8 digits3_buffer;        /* 0x46b0 */
    u8 digits3_count;         /* 0x46b1 */
    u8 digits_shown;          /* 0x46b2 */
    u8 group1220_buffer;      /* 0x46b3 */
    u8 group1220_count;       /* 0x46b4 */
    u8 label44B0_shown;       /* 0x46b5 */
    u8 label4530_shown;       /* 0x46b6 */
    u8 digits4_buffer;        /* 0x46b7 */
    u8 digits4_count;         /* 0x46b8 */
    u8 digits4_shown;         /* 0x46b9 */
    u8 price_buffer;          /* 0x46ba */
    u8 price_count;           /* 0x46bb */
    u8 cells_a_count[9];      /* 0x46bc */
    u8 cells_b_count[9];      /* 0x46c5 */
    u8 cells_a_buffer[9];     /* 0x46ce */
    u8 cells_b_buffer[9];     /* 0x46d7 */
    u16 attack[16];           /* 0x46e0: each member's attack before buying (ovl2602: the
                               * gear summary's attack) */
    u16 defense[16];          /* 0x4700: and defence */
    u8 stock[5][20];          /* 0x4720: ovl2602's five gear part lists */
    u8 unk4784;
    u8 label45B0_shown;       /* 0x4785 */
    u8 unk4786[2];
} ShopDetails;

LAYOUT_CHECK(ShopDetailsSize, sizeof(ShopDetails) == 0x4788 && OFFSET_OF(ShopDetails, attack) == 0x46E0);

#endif
