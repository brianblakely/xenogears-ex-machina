#ifndef OVL2602_MENU_CARD_H
#define OVL2602_MENU_CARD_H

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "menu/card.h"
#include "menu/panel.h"
#include "menu/screen.h"
#include "menu/tables.h"

extern const CardPrefix D_801C5000; /* "BISLPS-00800" */

/*
 * The shop screen's packets (menu state + 450, 4788h bytes): sprite groups
 * with their part counts and buffers, the bars and frames of nine rows, the
 * name labels and the resources unpacked from file 2.
 */
typedef struct ShopDetails {
    POLY_FT4 members[18];     /* 0000: count 46a5, buffer 46a6 */
    POLY_FT4 group2D0[18];    /* 02d0: count 46a9, buffer 46a8 */
    POLY_FT4 heading[44];     /* 05a0: count 46ab, buffer 46aa */
    POLY_FT4 digits1[18];     /* 0c80: count 46ad, buffer 46ac */
    POLY_FT4 digits2[18];     /* 0f50: count 46af, buffer 46ae */
    POLY_FT4 group1220[18];   /* 1220: count 46b4, buffer 46b3 */
    POLY_FT4 digits4[18];     /* 14f0: count 46b8, buffer 46b7 */
    POLY_FT4 price[20];       /* 17c0: ovl2602, count 46bb, buffer 46ba */
    POLY_FT4 digits3[18];     /* 1ae0: count 46b1, buffer 46b0 */
    POLY_FT4 rows[8][8];      /* 1db0: counts 468c, buffers 4694 */
    POLY_FT4 cells_a[9][6];   /* 27b0: counts 46bc, buffers 46ce */
    POLY_FT4 cells_b[9][6];   /* 3020: counts 46c5, buffers 46d7 */
    LINE_F3 bar_upper[18];    /* 3890: two per row */
    LINE_F3 bar_lower[18];    /* 3a40 */
    LINE_F2 frame[2];         /* 3bf0 */
    u8 unk3C10[0x20];
    MenuLabel names_a[8];         /* 3c30 */
    MenuLabel names_b[8];         /* 4030 */
    MenuLabel label4430;          /* 4430 */
    MenuLabel label44B0;          /* 44b0 */
    MenuLabel label4530;          /* 4530 */
    MenuLabel label45B0;          /* 45b0 */
    void *resources[9];       /* 4630: unpacked from file 2 */
    u8 amounts[0x30];         /* 4654 */
    u8 name_shown[8];         /* 4684 */
    u8 row_count[8];          /* 468c */
    u8 row_buffer[8];         /* 4694 */
    u8 bar_shown[9];          /* 469c */
    u8 members_count;         /* 46a5 */
    u8 members_buffer;        /* 46a6 */
    u8 label4430_shown;       /* 46a7 */
    u8 group2D0_buffer;       /* 46a8 */
    u8 group2D0_count;        /* 46a9 */
    u8 heading_buffer;        /* 46aa */
    u8 heading_count;         /* 46ab */
    u8 digits1_buffer;        /* 46ac */
    u8 digits1_count;         /* 46ad */
    u8 digits2_buffer;        /* 46ae */
    u8 digits2_count;         /* 46af */
    u8 digits3_buffer;        /* 46b0 */
    u8 digits3_count;         /* 46b1 */
    u8 digits_shown;          /* 46b2 */
    u8 group1220_buffer;      /* 46b3 */
    u8 group1220_count;       /* 46b4 */
    u8 label44B0_shown;       /* 46b5 */
    u8 label4530_shown;       /* 46b6 */
    u8 digits4_buffer;        /* 46b7 */
    u8 digits4_count;         /* 46b8 */
    u8 digits4_shown;         /* 46b9 */
    u8 price_buffer;          /* 46ba */
    u8 price_count;           /* 46bb */
    u8 cells_a_count[9];      /* 46bc */
    u8 cells_b_count[9];      /* 46c5 */
    u8 cells_a_buffer[9];     /* 46ce */
    u8 cells_b_buffer[9];     /* 46d7 */
    u16 stat_b0[16];          /* 46e0: per member, the gear summary's b0 value */
    u16 stat_a4[16];          /* 4700: and its a4 value */
    u8 stock[5][20];          /* 4720: the shop's five gear part lists */
    u8 unk4784;
    u8 label45B0_shown;       /* 4785 */
    u8 unk4786[2];
} DetailBlock;

/*
 * The gear screen block (ovl2602, menu state + 454, 1f00h bytes): its
 * backdrop and part pictures, and three animated sprite groups: a flicker of
 * three sprites, two lamps that open, idle and close, and a third indicator.
 * States: 0 off, 1 opening, 2 idle, 3 closing (the indicator has 4 steps).
 */
typedef struct GearScreen {
    POLY_FT4 backdrop[2];      /* 0000: drawn with the menu's buffer */
    u8 unk50[0x30];
    POLY_FT4 packets[2];       /* 0080 */
    POLY_FT4 flicker[3][4];    /* 00d0 */
    POLY_FT4 lamps[66];        /* 02b0: 22 per lamp; lamps 0 and 1 */
    POLY_FT4 indicator[36];    /* 0d00 */
    POLY_FT4 frame[28];        /* 12a0 */
    POLY_FT4 parts[5][10];     /* 1700 */
    u8 flicker_count;          /* 1ed0 */
    u8 lamp_count[3];          /* 1ed1: [2] the indicator's */
    u8 flicker_buffer;         /* 1ed4 */
    u8 lamp_buffer[3];         /* 1ed5 */
    u8 flicker_shown;          /* 1ed8 */
    u8 lamp_state[3];          /* 1ed9: [2] the indicator's */
    u8 flicker_timer;          /* 1edc */
    u8 lamp_timer[3];          /* 1edd */
    u8 flicker_frame;          /* 1ee0 */
    u8 buffer;                 /* 1ee1 */
    s16 indicator_frame;       /* 1ee2 */
    s16 lamp_frame[2];         /* 1ee4 */
    u8 part_count[5];          /* 1ee8 */
    u8 parts_buffer;           /* 1eed */
    u8 unk1EEE[2];
    u16 flicker_x, flicker_y;  /* 1ef0 */
    u16 lamp_x[2];             /* 1ef4 */
    u16 lamp_y[2];             /* 1ef8 */
    u16 indicator_x, indicator_y; /* 1efc */
} GearScreen;

/* A model part block (ovl2602, menu state + 458/45c). */
typedef struct ModelParts {
    void *data0; /* 00 */
    void *data1; /* 04 */
    s16 position[3]; /* 08: initial actor position */
    u8 unkE[4];
    u8 unk12;    /* 12 */
} ModelParts;


/* Overlay data. */
extern u16 D_801D6C68[]; /* party bit of each member id */
extern u8 D_801D6A80[];  /* label text ids */
extern s32 D_801D69A0[]; /* list picture pairs (sprite, second layer), eight words per command */
extern s32 D_801D6A60[]; /* marker x */
extern s32 D_801D6A70[]; /* marker y */
extern s32 D_801D6A84[]; /* file slot -> list position */
extern s32 D_801D6AFC[]; /* marker x per list position */
extern s32 D_801D6B7C[]; /* marker y per list position */
extern s32 D_801D6BFC[]; /* cursor x per position */
extern s32 D_801D6C44[]; /* member portrait x */
extern u8 D_801D6D10[];  /* alternative heading sprite ids */
extern s32 D_801D6D14[]; /* heading x */
extern s32 D_801D6D34[]; /* alternative heading x */
extern s32 D_801D6D3C[]; /* heading y */
extern s32 D_801D6D5C[]; /* alternative heading y */
extern s32 D_801D6D64;   /* first number x */
extern s32 D_801D6D68;   /* first number y */
extern s32 D_801D6D6C;   /* second number x */
extern s32 D_801D6D70;   /* second number y */
extern s32 D_801D6D74;   /* third number x */
extern s32 D_801D6D78;   /* third number y */
extern s32 D_801D6C20[]; /* cursor y per position */

/* Resident services. */
void func_80039DB8(s32 effect);          /* play a sound effect */
extern s32 D_80059488;                   /* sound state saved while paused */
void func_80033698(s32 x, s32 y);        /* text palettes */
u8 *func_80033728(void *table, s32 index); /* entry of a text table */
u8 *func_80033A2C(s32 id);               /* kind 3 part name */
u8 *func_80033A5C(s32 id);               /* kind 4 part name */
s32 func_80034EAC(u8 *text, void *pixels, s32 width, s32 line); /* render a text line */
s32 func_8002675C(void *sheet, s32 id, void *packets, s32 buffer, s32 x, s32 y, s32 scale); /* sprite */
s32 func_800263E4(void *sheet, s32 id, void *packets, s32 buffer, s32 x, s32 y, s32 scale, s32 flip_x,
                  s32 flip_y); /* mirrored sprite */


/* This overlay. */
u16 func_801C5228(u16 mask, u8 id);
void func_801C56C8(void);
void func_801C5CA8(MenuLabel *label, s32 index, s32 row, s32 mode);
void func_801C6098(void);
void func_801C665C(void);
void func_801C668C(POLY_G4 *poly, u8 r, u8 g, u8 b);
void func_801C765C(POLY_FT4 *poly);
void func_801C7AE4(u8 index);
void func_801C7E00(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C7F64(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C81AC(u8 index, u16 x, u16 y, u16 w);
void func_801C84F0(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C883C(u8 index, u16 x, u16 y, u16 h);
void func_801C8B84(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C8ED0(u8 index, u16 x, u16 y, u16 w, u16 h, u8 flat, s32 ot_entry, u8 has_bar);
void func_801C9690(s32 index);
void func_801C9864(s32 index);
void func_801C9A38(s32 index);
void func_801C9C0C(s32 index);
void func_801C9DE0(s32 index);
void func_801C9F1C(s32 index);
void func_801CA068(s32 index);
void func_801CA754(void);
void func_801CA7E4(void);
void func_801CA874(void);
void func_801CA9EC(void);
void func_801CAA7C(void);
void func_801C9264(void);
void func_801C5344(u8 allocate);
void func_801C53A8(u8 allocate);
void func_801C540C(u8 allocate);
void func_801C5470(u8 allocate);
void func_801C54D4(u8 allocate);
void func_801C5538(u8 allocate);
void func_801C559C(u8 allocate);
void func_801C5600(u8 allocate);
void func_801C6A54(u8 mode);
void func_801C9054(u8 index);
void func_801C5EE8(MenuLabel *labels, u8 *text_ids, s32 row, s32 count);
void func_801CC9A0(void);
void func_801CC1C4(void);
void func_801CD564(u8 menu);
void func_801C782C(void);
void func_801C7A88(u8 index);
void func_80033B34(u8 *codes, u8 *text, s32 count); /* codes to text */
void func_801D2054(s32 n, u8 *ids, u8 *counts, u8 kind, u8 unk4, u8 *counts2, u8 unk6);
void func_801C5298(u32 value);
u16 func_801C5244(u8 id);
void func_801C5B08(void);
void func_801C5C98(void);
void func_801C6114(void);
void func_801C6708(void);
void func_801C6170(void);
void func_801CCD20(void);
void func_801CD310(s32 count, s32 *ids);
void func_801CD838(u8 count, u8 selected, s32 *ids);
void func_801CCEE8(u8 count, MenuLabel *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row, u8 mode);
void func_801C6278(s32 position, u8 frame);
void func_801C90E0(u8 index, s16 x, s16 y, s16 w, u16 h, u8 grow, u8 flat, s32 ot_entry, u8 has_bar);
void func_801CA28C(void);
void func_801CA404(void);
void func_801CABE0(void);
void func_801C959C(void);
void func_801C9550(void);
void func_801CAC20(void);
void func_801CB2E8(void);
void func_801CB35C(void);
void func_801CB498(u8 sound);
void func_801C94CC(s32 count, POLY_FT4 *packets, s32 first);
void func_801C93B0(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first);

/* ovl2602 only. */
void func_801C5600(u8 allocate);
void func_801C5664(u8 allocate);
u8 func_801CCA40(u8 wait, u8 movable);


extern u32 D_801D6C88[];  /* party bit of each member id */
extern u8 D_801D70F4[];   /* pilot of each gear */
extern s32 D_801D6FD8;    /* available members 1-10 */
void func_801E7D14(void *a, void *b, u32 *ot, s32 buffer);
u32 func_801C527C(u32 mask, u8 id);
u32 func_801C5260(u8 id);
u32 func_801D1078(u8 id, u8 kind);
void func_801D3558(s32 *change, u8 *decrease, u8 part, u8 kind, u8 member);
void func_801D06D8(u32 first, u32 second, u32 third, u32 fourth, u8 lower);
void func_801D18F8(s32 top, u8 *ids, u8 *kinds, u8 *chosen, u8 *held);
void func_801CCEBC(u8 count, u8 *shown);
void func_801D0EC8(u8 close);
void func_801D61B8(MenuTables *table, u8 id);
void func_801D62A4(MenuTables *table, u8 id);
void func_801D6250(MenuTables *table, u8 id);
void func_801D6334(MenuTables *table, u8 id);
void func_801D6738(MenuTables *table, u8 id);
extern u8 D_801D697C;
u8 func_801D498C(u8 page, u8 fit);
u32 func_801D2B74(s32 top, s32 gold, u8 *dims);
u32 func_801D3C78(s32 row, s32 top, u8 *dims);
void func_801D5398(void);
void func_801D5F94(MenuTables *table, u8 id);
void func_801D6150(MenuTables *table, u8 id);
u8 func_801D690C(u8 id);
void func_801CBA2C(void);
void func_801CEA68(void);
void func_801CEEA8(void);
void func_801CF184(void);
void func_801E7FD4(void);
extern u16 D_801D6D7C[];   /* model file ids */
extern u8 D_801D6A20[];    /* command label text ids */
extern s32 D_801D6FD0[];   /* second marker x */
extern u8 D_801D6D08[];    /* heading sprite ids, four per set */
extern s32 D_801D6D14[];   /* heading x */
extern s32 D_801D6D3C[];   /* heading y */
void func_801CC530(u8 first);
void func_801CC9A0(void);
void func_801CC528(void);
u8 func_801D5828(void);
void func_801CCE90(u8 count, MenuLabel *labels, u8 *text_ids, u8 *shown);
extern u8 D_801D6A24[];    /* sell list label text ids */
extern u8 D_801D6A2C[];    /* buy list label text ids */
extern s32 D_801D6A40[];   /* gear list label x offsets */
extern s32 D_801D6FDC;     /* index of the gear screen's member among the available ones */
/* The gear record's bytes 0x55-0x57 (pad55, equipAttackScale, chargeRate)
 * as the stat rebuild clears them: one array indexed from the record, which
 * the separate members of GearRecord do not compile alike. */
typedef struct {
    u8 pad[0x55];
    u8 bytes55[3];
} GearRecordBytes55;

/* The camera's move between two points (801d9050). */
typedef struct {
    s32 from[3];     /* 00: previous target */
    s32 to[3];       /* 0c: target */
    s32 step[3];     /* 18: 16.16 step per frame */
    s32 offset[3];   /* 24: 16.16 distance travelled */
    u8 negative[3];  /* 30: moving towards smaller coordinates */
    u8 frames;       /* 33: steps per update */
} CameraMove;
/* The overlay's commons (ovl2602_common.c), in address order: GCC
 * allocates tentative definitions in the order of their first declaration. */
extern CameraMove D_801D9050;
extern u8 D_801D9084;      /* gear being edited */
extern u8 *D_801D9088;     /* name pixel buffer */
extern s32 D_801D908C[5];  /* entries in each of the five gear part lists */

/* The Gear model code's state (801e8674, outside this overlay). */
typedef struct {
    u8 unk0[0x54];
    s16 unk54;    /* 54 */
    s16 distance; /* 56: camera distance */
} ModelView;
typedef struct {
    u8 unk0[4];
    ModelView *view; /* 04 */
    u8 unk8[0x1C - 8];
    s16 unk1C;       /* 1c */
    u8 unk1E[0x60 - 0x1E];
    s16 unk60;       /* 60 */
} ModelState;
extern ModelState *D_801E8674;
extern MATRIX *D_801E8644;        /* the model code's light colour matrix */
void func_801E738C(s32 unk0);     /* model code setup */
extern ModelState *D_801E8670[2]; /* per model slot */
extern u8 D_801D6DA0[];           /* model variant per gear */
extern u16 D_801D6DB4[];          /* model value 60h per gear */
extern u16 D_801D6DD8[];          /* model value 1ch per gear */
void func_801E742C(s32 index, u16 flags, void *script, void *file, s16 x, s16 y, s16 z,
                   s16 w, s16 *pos);
void func_801E8330(u16 index, u16 mask, s32 variant);
u8 func_8001BD40(u8 low, u8 high); /* random number in [low, high] */
extern u16 D_801D6FE0[];        /* lamp sprite ids, four per frame, five frames per lamp (ffff none) */
extern u8 D_801D7030[];         /* lamp and indicator position per command and list cursor */
extern u16 D_801D7040[2];       /* lamp x */
extern u16 D_801D7044[];        /* lamp y choices, six per lamp */
extern u16 D_801D705C[6];       /* indicator x choices */
extern u16 D_801D7068[6];       /* indicator y choices */
extern u16 D_801D7074[6];       /* flicker x choices */
extern u16 D_801D7080[6];       /* flicker y choices */
extern s16 D_801D6DFC[];        /* camera x per gear */
extern s16 D_801D6E20[];        /* camera y per gear, command (1-3) and list cursor */
extern s16 D_801D6FB8[];        /* camera distance per command (1-3) and list cursor */
extern u16 D_801D708C[14];      /* gear parts frame sprite ids */
extern u16 D_801D70A8[14];      /* their x */
extern u16 D_801D70C4[14];      /* their y */
/* Gear value positions (x, y). */
extern u16 D_801D70E0, D_801D70E2, D_801D70E4, D_801D70E6, D_801D70E8;
extern u16 D_801D70EA, D_801D70EC, D_801D70EE, D_801D70F0, D_801D70F2;
void func_801CB4E4(void);
void func_801CBE60(void);
void func_801CABD8(void);
void func_801E8030(s32 unk0);
void func_801CFAB8(u8 unk0, u8 id);
void func_801CB690(void);
void func_801D2784(void);
void func_801D27C4(void);
void func_801CE82C(void);
void func_801CF33C(void);
void func_801CE7E0(void);
void func_801C962C(void);
void func_801C5C98(void);
void func_801C6114(void);
void func_801C6708(void);
void func_801C6170(void);
void func_801C6E74(void);
void func_801D5D38(void);
void func_801CE1D0(void);
void func_801CDD74(void);
void func_801CCD20(void);
void func_801C5B08(void);
extern s32 D_801D6980[];   /* command picture pairs */
extern s32 D_801D6A30[];   /* command label x offsets */
void func_801CCEE8(u8 count, MenuLabel *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row, u8 mode);
void func_801D0398(u8 back);
u8 func_801CDC68(void);
void func_801CE2E8(void);

#endif
