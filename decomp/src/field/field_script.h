#ifndef FIELD_FIELD_SCRIPT_H
#define FIELD_FIELD_SCRIPT_H

/* Event-script state addressed by the instruction handlers from 80087af0 on. */

#include "field.h"

extern u32 *D_800B1F74; /* TIM image held by instruction 0x77 */

/* Sound-effect bank instruction 0xb0 (resident sound state). */
extern s32 D_80062518[4];  /* loaded wave bank per slot */
extern s32 D_80062524;     /* slot 3 of D_80062518, read on its own */
extern s32 D_800595AC;     /* active slot-3 bank */
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

extern PortraitPlace D_800AEAE4[4][2]; /* VRAM place per slot and image */
extern u8 D_800AE1E0[][2];             /* portrait files per character, - 0x46 */
extern void *D_800ADB10;               /* first portrait image */
extern void *D_800ADB14;               /* second portrait image */
extern FieldFileRequest D_800B00C8[3];  /* file list read by 80029afc */
extern s32 func_8009C538(s32 id);

/* Dialogue window opening (8009c5a8). */
extern s32 D_800AFD04;     /* dialogue gate */
extern s32 D_800C4268;     /* dialogue windows opened this pass */
extern s32 D_800ADB64;     /* requested menu kind (800799d4 runs it), 0xff none */
extern void *D_800ADBF0;   /* field message table */
extern s32 func_8003373C(void *table, s32 message); /* message columns */
extern s32 func_80033760(void *table, s32 message); /* message rows */
extern void func_8007F814(s32 index, s32 *x, s32 *y, s32 height);
extern s32 func_80080720(void);
extern s32 func_80080760(void);
extern s32 func_800807B4(void);
extern s32 func_8009A514(void);
extern void func_8009CCF8(s32 window);

#endif
