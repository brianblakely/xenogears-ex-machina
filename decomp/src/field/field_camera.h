#ifndef FIELD_FIELD_CAMERA_H
#define FIELD_FIELD_CAMERA_H

/* The field camera: its initial state, look-at and follow helpers, the angle
 * steps and octants, and the matrix utilities of field.c. The camera state
 * itself is the view field_view (field.h); its octant field_camera_get_octant is in
 * field/monitor.h. */

#include "common.h"
#include "psyq/libgte.h"
#include "field/monitor.h"
#include "field.h"

extern MATRIX field_camera_next_view;     /* the composed view, kept across frames */
extern MATRIX field_sprite_view_matrix;   /* sprite view rotation matrix */
extern MATRIX field_instance_cull_matrix; /* instance view: the rotation with its translation */
extern s32 field_camera_pitch;            /* camera pitch */
extern s32 field_camera_target_at_edge;   /* 1 while the target goal is held at the walkable edge */
extern s32 field_camera_settle_frames;    /* camera frames settling */
extern s32 field_camera_release_frames;   /* camera frames releasing */
extern s32 field_camera_cut_timer;        /* camera cut: turns jump to their goal, pad input is dropped */
extern u8 field_camera_octant_bits[8];    /* octant bits */

void field_camera_init(void);                                                                 /* the camera's initial state */
void field_camera_compose_and_load_view(void);                                                /* compose the view, reload the scaled world */
void field_camera_build_lookat_matrix(MATRIX *view, VECTOR *eye, VECTOR *target, VECTOR *up); /* look-at matrix */
void field_camera_rotate_point_by_heading(VECTOR *point, VECTOR *center);                     /* rotate about `center` by the heading */
void field_compute_line_intersection(DVECTOR *a, DVECTOR *b, DVECTOR *out);                   /* intersection of two lines */
s32 field_camera_walk_top_layer(VECTOR *point, SVECTOR *edge, DVECTOR *segment);              /* walk the top layer toward `point` */
s32 field_turn_angle_toward(s32 angle, s32 goal, s32 step);                                   /* turn toward `goal` by `step` */
s32 field_turn_angle_or_cut(s32 angle, s32 goal, s32 step);                                   /* the same, or jump there on a cut */
void field_descriptor_rebuild_matrix(s32 index);                                              /* rebuild descriptor `index`'s matrix */

/* Matrix utilities. */
void field_matrix_set_identity(MATRIX *m);                    /* identity rotation, zero translation */
void field_matrix_clear_translation(MATRIX *m);               /* clear the translation */
void field_matrix_copy(MATRIX *to, MATRIX *from);             /* copy rotation and translation */
void field_matrix_copy_translation(MATRIX *to, MATRIX *from); /* copy the translation */
void field_matrix_copy_rotation(MATRIX *to, MATRIX *from);    /* copy the rotation */
void field_matrix_set_rotation(MATRIX *m, s32 m00, s32 m01, s32 m02, s32 m10, s32 m11, s32 m12, s32 m20, s32 m21,
                   s32 m22);                  /* set the nine rotation elements */

#endif
