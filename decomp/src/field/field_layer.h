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
extern void *field_layer_module;           /* the module */
extern s32 field_event_runs_per_frame;             /* the module is loaded */
extern FileRequest field_layer_file_requests[10]; /* its file list: two files per layer, the module, the zero end */
void field_layer_load(void);          /* load the module and its layers' files */
void field_layer_start(void);          /* start its layers */
void field_layer_load_and_start(void);          /* load, then start */

/* The module's draw, which each target declares itself (ovl2143/actors.h
 * says why); the field's light matrix is its work block's s16 rows. */
void gear_model_step_and_draw(MATRIX *m, s16 (*light)[3], u_long *ot, s32 buffer, s32 elapsed);

#endif
