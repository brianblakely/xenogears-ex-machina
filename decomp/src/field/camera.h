#ifndef FIELD_CAMERA_H
#define FIELD_CAMERA_H

#include "geometry.h"

/* Field camera state (meanings from src/reconstruction/original_layout.cpp).
 * The block at 800af930 is addressed as one aggregate: its members keep their
 * order against actor-record accesses. The shake words from 800afa28 are
 * separate scalars; the shake vectors are arrays. */


typedef struct FieldCamera {
    s32 scripted_zoom;              /* 930 */
    s16 mode;                       /* 934 */
    s16 scripted_elevation;         /* 936 */
    s16 scripted_heading;           /* 938 */
    s16 scripted_scale;             /* 93a */
    s16 scripted;                   /* 93c */
    s16 target_steps;               /* 93e */
    VECTOR scripted_target;   /* 940 */
    VECTOR target_step;       /* 950 */
    s16 eye_steps;                  /* 960 */
    s16 unk962;                     /* 962 */
    VECTOR scripted_eye;      /* 964 */
    VECTOR eye_step;          /* 974 */
    s32 target_a;                   /* 984 */
    s32 target_b;                   /* 988 */
    s16 angle;                      /* 98c */
    s16 view_angle;                 /* 98e */
    s16 previous_view[0x10];        /* 990: matrix */
    s16 orbit[0x10];                /* 9b0: matrix */
    s16 orbit_angles[4];            /* 9d0 */
    s32 flags;                      /* 9d8 */
    s16 bounds[4];                  /* 9dc */
    s16 heading_x;                  /* 9e4 */
    u16 heading_half;               /* 9e6 */
    s16 heading_z;                  /* 9e8 */
    s16 unk9EA;                     /* 9ea */
    s32 heading_velocity;           /* 9ec */
    s32 heading_high;               /* 9f0 */
    u8 heading_blocks[2];           /* 9f4 */
    s16 heading_steps;              /* 9f6 */
    s32 projection;                 /* 9f8 */
    u16 elevation;                  /* 9fc */
    s16 distance;                   /* 9fe */
    s16 elevation_steps;            /* a00 */
    s16 unkA02;                     /* a02 */
    s32 elevation_value;            /* a04 */
    s32 elevation_step;             /* a08 */
    s32 heading;                    /* a0c */
    s16 projection_steps;           /* a10 */
    s16 unkA12;                     /* a12 */
    s32 projection_value;           /* a14 */
    s32 projection_step;            /* a18 */
    s16 steps;                      /* a1c */
    s16 unkA1E;                     /* a1e */
    s32 start;                      /* a20 */
    s32 step;                       /* a24 */
} FieldCamera;

extern VECTOR D_800AF880;   /* eye */
extern VECTOR D_800AF890;   /* target */
extern VECTOR D_800AF8B0;   /* eye goal */
extern VECTOR D_800AF8C0;   /* target goal */
extern VECTOR D_800AF8F0;   /* saved target */
extern VECTOR D_800AF900;   /* actor point a */
extern VECTOR D_800AF910;   /* saved eye */
extern VECTOR D_800AF920;   /* actor point b */
extern FieldCamera D_800AF930;

extern s16 D_800AFA28;      /* shake active */
extern s16 D_800AFA2A;      /* shake frames */
extern s16 D_800AFA2C;      /* shake stops at zero */
extern s32 D_800AFA30[3];   /* shake amplitude */
extern s32 D_800AFA3C[3];   /* shake step */

#endif
