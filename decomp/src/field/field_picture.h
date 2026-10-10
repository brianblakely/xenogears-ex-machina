#ifndef FIELD_FIELD_PICTURE_H
#define FIELD_FIELD_PICTURE_H

/* The picture viewer (800aaf80-800aba98, field_effect.c): on some maps an
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

extern s32 field_picture_table[];
extern s32 field_picture_marker_scale_x;
extern s32 field_picture_marker_scale_z;
extern s32 field_picture_marker_origin_x;
extern s32 field_picture_marker_origin_y;

/* The picture's marker sprites (800b1df0), each with a draw mode per
 * draw buffer; only the first (the controlled actor's spot) is drawn. */
typedef struct PictureMarks {
    DR_MODE modes[16][2]; /* 000 */
    SPRT sprites[16][2];  /* 180 */
} PictureMarks;

extern ScreenPieces *field_picture_pieces; /* the picture's three pieces */
extern PictureMarks *field_picture_marker_sprites;

void field_picture_init(void);      /* set the picture up */
void field_picture_draw(s32 level);
void field_picture_add_flagged_pieces(void);
void field_picture_show(void);      /* show the map's picture while its item is held */

#endif
