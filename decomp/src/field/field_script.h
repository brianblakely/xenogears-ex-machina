#ifndef FIELD_FIELD_SCRIPT_H
#define FIELD_FIELD_SCRIPT_H

/* Event-script state addressed as scalars by the instruction handlers from
 * 80087af0 on (their scheduling needs them apart from the 800b2078 block). */

#include "field.h"

extern u16 D_800B236C; /* script flag set by instruction FE 99 (read by 80094xxx) */
extern u32 *D_800B1F74; /* TIM image held by instruction 0x77 */

/* Sound-effect bank instruction 0xb0 (resident sound state). */
extern s32 D_80062518[4];  /* loaded wave bank per slot */
extern s32 D_80062524;     /* slot 3 of D_80062518, read on its own */
extern s32 D_800595AC;     /* active slot-3 bank */
extern s32 D_8004F370;     /* 1 selects the alternate bank file */
extern void *D_800AFD08;   /* bank file being loaded */
extern s32 D_800AFD0C;     /* bank file number */
extern s32 D_800AFD18;     /* bank slot being loaded */

/* Event context switching (party joins). */
extern FieldDescriptor *D_800B06B8; /* descriptor of the running actor */
extern s32 D_8006F990[3];            /* party member descriptor per slot */
extern s32 D_800AFFEC;
extern s32 func_800A3090(s32 actor, s32 event);
extern s32 func_800A1EC8(s32 limit);
extern void func_80077268(void);
extern void func_80080A74(s32 actor);

/* A party member's sprite data (D_8005A414 per slot), copied whole. */
typedef struct {
    s32 data[0x14000 / 4];
} PartySprite;

/* The movie request block at 800c3a20. The instructions that fill it
 * address it as one object (their stores are not moved past loads through
 * the actor pointer), although field.h declares its halfwords one by one. */
typedef struct {
    s16 file;        /* 20 */
    u16 x;           /* 22: display position */
    u16 y;           /* 24 */
    u16 source_x;    /* 26 */
    u16 source_y;    /* 28 */
    u16 unk2A;       /* 2A */
    u16 sound_start; /* 2C: movie sound time origin */
    u16 unk2E;       /* 2E */
    u16 mode;        /* 30: low nibble layout, 0x40/0xc0 fade */
    u16 width;       /* 32 */
    u16 height;      /* 34 */
    u16 depth24;     /* 36: 1 for a 24-bit display */
    s16 sound_bank;  /* 38: 0xff none */
    u16 unk3A;       /* 3A */
} FieldMovieRequest;

#define FIELD_MOVIE (*(FieldMovieRequest *)&D_800C3A20)

extern s32 D_800ADB70; /* movie requested */

extern void func_800379C8(char *format, ...); /* resident debug print */

/* Resident text box calls (main2.c). */
extern s32 func_80033CD0(TextBox *box); /* chosen answer, 0 while open */
extern void func_80034800(TextBox *box, s32 r, s32 g, s32 b);

/* Party gathering (8009aee0). */
extern s16 D_800AEA34[8]; /* heading per direction */
extern void func_8009E574(s32 x, s32 z);

/* Controlled-actor walk (80092894). */
extern s32 D_800ADBEC; /* publish the field id on the next walk */
extern void func_80092F44(void);

/* Dialogue portraits (8009c154): D_800B06A4[slot] holds .a the character,
 * .b the state (1 loaded, 2 shown) and .c whether a second image is used. */
typedef struct {
    s16 x;
    s16 y;
    s16 clut_x;
    s16 clut_y;
} PortraitPlace;

typedef struct {
    u16 file;
    void *data;
} PortraitRequest;

extern PortraitPlace D_800AEAE4[3][2]; /* VRAM place per slot and image */
extern u8 D_800AE1E0[][2];             /* portrait files per character, - 0x46 */
extern void *D_800ADB10;               /* first portrait image */
extern void *D_800ADB14;               /* second portrait image */
extern PortraitRequest D_800B00C8[3];  /* file list read by 80029afc */
extern s32 func_80029AFC(void *list, s32 mode, s32 a2);
extern s32 func_8009C538(s32 id);

/* Dialogue window opening (8009c5a8). */
extern s32 D_800AFD04;     /* dialogue gate */
extern s32 D_800C4268;     /* dialogue windows opened this pass */
extern s32 D_800ADB64;     /* 0xff when no input jump is pending */
extern void *D_800ADBF0;   /* field message table */
extern s32 func_8003373C(void *table, s32 message); /* message columns */
extern s32 func_80033760(void *table, s32 message); /* message rows */
extern void func_8007F814(s32 index, s32 *x, s32 *y, s32 height);
extern void func_8007F8DC(s32 x, s32 y, s32 message, s32 window, s32 columns, s32 rows, s32 owner, s32 speaker,
                          s32 mode, s32 flags, s32 style);
extern s32 func_80080720(void);
extern s32 func_80080760(void);
extern s32 func_800807B4(void);
extern s32 func_8009A514(void);
extern void func_8009CCF8(s32 window);

#endif
