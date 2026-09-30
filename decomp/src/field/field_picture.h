#ifndef FIELD_FIELD_PICTURE_H
#define FIELD_FIELD_PICTURE_H

#include "field.h"

/* The picture viewer: on some maps an item shows a full-screen picture
 * (file 0x7fb + picture) faded in over the field until dismissed. */

/* One picture of the table at 800af47c, ended by map 0xffff. */
typedef struct {
    s32 map;      /* 00 */
    s32 unk04;    /* 04: to 800c3914 */
    s32 unk08;    /* 08: to 800c3a18 */
    s32 picture;  /* 0C: file 0x7fb + picture */
    s32 item;     /* 10: shown while held */
    s32 x;        /* 14: to 800afe78 */
    s32 y;        /* 18: to 800afe7c */
    s32 pieces;   /* 1C: 1 adds the flagged pieces (800ab808) */
} Picture;

extern Picture D_800AF47C[];
extern s32 D_800C3914;
extern s32 D_800C3A18;
extern s32 D_800AFE78;
extern s32 D_800AFE7C;
/* The picture's marker sprites (800b1df0), each with a draw mode per
 * draw buffer; only the first (the controlled actor's spot) is drawn. */
typedef struct {
    DR_MODE modes[16][2]; /* 000 */
    SPRT sprites[16][2];  /* 180 */
} PictureMarks;

extern ScreenPieces *D_800C3A3C; /* the picture's three pieces */
extern PictureMarks *D_800B1DF0;

void func_800AAF80(void);
void func_800AB378(s32 level);
void func_800AB808(void);

#endif
