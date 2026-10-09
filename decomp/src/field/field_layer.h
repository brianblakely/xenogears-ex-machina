#ifndef FIELD_FIELD_LAYER_H
#define FIELD_FIELD_LAYER_H

/* The 801e module (ovl2143, file 6b9; its actors, entries and variables are
 * in ovl2143/actors.h) and its layers: the files the field loads for it (two
 * per layer) and the module entries the field declares itself. The work
 * block keeps the field's view of each layer. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "ovl2143/actors.h"

/* The field's side (field.c). */
extern void *D_800ADB20;           /* the module */
extern s32 D_800ADB1C;             /* the module is loaded */
extern FileRequest D_800B2394[10]; /* its file list: two files per layer, the module, the zero end */
void func_80077884(void);          /* load the module and its layers' files */
void func_80077AB4(void);          /* start its layers */
void func_80077C60(void);          /* load, then start */

/* The module's draw, which each target declares itself (ovl2143/actors.h
 * says why); the field's light matrix is its work block's s16 rows. */
void func_801E7D14(MATRIX *m, s16 (*light)[3], u_long *ot, s32 buffer, s32 elapsed);

#endif
