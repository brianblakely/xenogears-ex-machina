#ifndef FIELD_FIELD_PICTURE_H
#define FIELD_FIELD_PICTURE_H

/* The picture viewer (800aaf80-800aba98, field_800A9274.c): on some maps an
 * item shows a full-screen picture (file 0x7fb + picture) faded in over the
 * field until dismissed. */

#include "common.h"
#include "psyq/libgpu.h"
#include "field_screen.h"

/* The picture table at 800af47c: eight words per picture, ended by map
 * 0xffff. The viewer indexes it as one word array (picture * 8 + word);
 * with a struct array GCC folds the base into each address instead of
 * keeping it in a register. */
#define PICTURE_WORDS 8
#define PICTURE_MAP 0     /* map id */
#define PICTURE_UNK04 1   /* to 800c3914 */
#define PICTURE_UNK08 2   /* to 800c3a18 */
#define PICTURE_FILE 3    /* file 0x7fb + this */
#define PICTURE_ITEM 4    /* shown while held */
#define PICTURE_X 5       /* to 800afe78 */
#define PICTURE_Y 6       /* to 800afe7c */
#define PICTURE_PIECES 7  /* 1 adds the flagged pieces (800ab808) */

extern s32 D_800AF47C[];
extern s32 D_800C3914;
extern s32 D_800C3A18;
extern s32 D_800AFE78;
extern s32 D_800AFE7C;

/* The picture's marker sprites (800b1df0), each with a draw mode per
 * draw buffer; only the first (the controlled actor's spot) is drawn. */
typedef struct PictureMarks {
    DR_MODE modes[16][2]; /* 000 */
    SPRT sprites[16][2];  /* 180 */
} PictureMarks;

extern ScreenPieces *D_800C3A3C; /* the picture's three pieces */
extern PictureMarks *D_800B1DF0;

void func_800AAF80(void);      /* set the picture up */
void func_800AB378(s32 level);
void func_800AB808(void);
void func_800ABA98(void);      /* show the map's picture while its item is held */

#endif
