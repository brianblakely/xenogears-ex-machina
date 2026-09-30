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
extern void func_800A1EC8(s32 limit);
extern void func_80077268(void);
extern void func_80080A74(s32 actor);

/* A party member's sprite data (D_8005A414 per slot), copied whole. */
typedef struct {
    s32 data[0x14000 / 4];
} PartySprite;

/* Movie playback parameters (800c3a20 block; see field.h) set by 0xa0. */
extern u16 D_800C3A30;
extern u16 D_800C3A32;
extern u16 D_800C3A34;
extern s32 D_800ADB70; /* movie requested */

extern void func_800379C8(char *format, ...); /* resident debug print */

/* Resident text box calls (main2.c). */
extern s32 func_80033CD0(TextBox *box); /* chosen answer, 0 while open */
extern void func_80034800(TextBox *box, s32 r, s32 g, s32 b);

/* Party gathering (8009aee0). */
extern s16 D_800AEA34[8]; /* heading per direction */
extern void func_8009E574(s32 x, s32 z);

#endif
