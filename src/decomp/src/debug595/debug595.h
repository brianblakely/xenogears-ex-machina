#ifndef DEBUG595_DEBUG595_H
#define DEBUG595_DEBUG595_H

/* The field debug monitor: its debug lines, CPU-time marks and the field
 * overlay's objects it reads and edits. What it shares with the field is in
 * field/monitor.h and field/actors.h; its views of the field's own objects
 * are below. */

#include "common.h"

#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libetc.h"
#include "resident/console.h"
#include "resident/formation.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "field/actors.h"
#include "field/monitor.h"

/* One debug line: a segment in its own frame, drawn in both buffers. */
typedef struct {
    MATRIX matrix;   /* 0x00 */
    SVECTOR rot;     /* 0x20 */
    SVECTOR trans;   /* 0x28 */
    SVECTOR start;   /* 0x30 */
    SVECTOR end;     /* 0x38 */
    LINE_G2 line[2]; /* 0x40: one per draw buffer */
} DebugLine;         /* 0x68 */

/* The actor's box that 80281678 outlines, as the field's box test passes it
 * (field_motion.c reads the actor record through a box view of its own): the
 * halfwords at +0x18, +0x1a and +0x1c are the box's half width, height and
 * half depth (field_actor_reset stores 0x10, 0x60 and 0x10), where FieldActor
 * has two s16 and the low half of its Fixed gravity, and the position is read
 * as 16.16 words. */
typedef struct {
    u8 unk0[0x18];
    SVECTOR size; /* 0x18: collision half extents */
    VECTOR pos;   /* 0x20: 16.16 fixed point */
} DebugActor;

/* One CPU-time mark: scanlines spent before it. */
typedef struct {
    s32 unk0;
    s32 time;   /* 0x04 */
    char *name; /* 0x08 */
} CpuMark;

/* An emitter's drawing flags (the editor steps them as one halfword). */
typedef union {
    s16 value;
    struct {
        u16 randrot : 1;  /* RANDROT */
        u16 sort : 2;     /* SORT: top, mid, normal, back */
        u16 unk3 : 3;
        u16 rangemod : 2; /* RANGEMOD: random, line, circle */
        u16 colmode : 2;  /* COLMODE: semi-transparency rate */
        u16 unk10 : 6;
    } bits;
} EmitterFlags;

/* A field particle emitter (field overlay table, 8 entries). */
typedef struct {
    s16 unk0;
    u16 start_wait;           /* 0x02 SWAIT */
    u16 end_wait;             /* 0x04 EWAIT */
    s16 max;                  /* 0x06 MAX */
    s32 speed;                /* 0x08 SPEED */
    SVECTOR start_pos;        /* 0x0C SPOS */
    SVECTOR end_pos;          /* 0x14 EPOS */
    SVECTOR gravity;          /* 0x1C GRAVITE */
    s16 speed_scale;          /* 0x24 SPEED multiplier */
    u16 start_range;          /* 0x26 SRANGE */
    u16 end_range;            /* 0x28 ERANGE */
    EmitterFlags flags;       /* 0x2A */
    s16 unk2C[2];
    s16 angle_offsets[8][2];  /* 0x30 ANGOFFS */
    s16 unk50[2];
    s16 shape;                /* 0x54 SHAPE */
    u16 particle_start_wait;  /* 0x56 PSWAIT */
    u16 particle_end_wait;    /* 0x58 PEWAIT */
    SVECTOR scale;            /* 0x5A SCALE */
    SVECTOR scale_offset;     /* 0x62 SCALEOFS */
    u8 color[4];              /* 0x6A COLOR */
    s8 color_offset[4];       /* 0x6E COLOROFS */
    s16 unk72[2];
    s16 rot_angle;            /* 0x76 ROTANGLE */
} ParticleEmitter;            /* 0x78 */

s32 field_debug_run_screen(u_long *ot);
void field_debug_draw_line(u_long *ot, DebugLine *line, MATRIX *m, s32 buffer);

/* Particle emitter editor. */
void field_debug_run_particle_editor(void);
void field_debug_print_column_marker(s32 row, s32 cursor, s32 blink);
s32 field_debug_start_editor_row(s32 row, s32 cursor, s32 *selected);
void field_debug_edit_emitter_field(s32 axis, u32 item);

/* The monitor's views of the field's own objects, whose types stay in the
 * field's headers (src/field). The view at 800af880 (FieldView) and the
 * work block at 800b2078 (FieldWork) it addresses member by member through
 * symbols of its own: as FieldView members, field_debug_move_camera keeps 800af9fc's
 * address in a register, and field_debug_run_screen differs only because it reads
 * 800af9fc as s16 where the field has u16 (lh, not lhu). Its u16 view of
 * 800af9fe builds the same as the field's s16, and the work block's members
 * build the same either way. It reads the current draw block as its
 * ordering table words (FieldDrawBlock +0xcc), names the emitter templates
 * (the field's Record78) from its editor's labels, reading their flags as
 * bit-fields and their colour offsets as one s8 array (the field reads +0x70
 * as u8). */
extern u_long *field_current_draw_block;          /* the current draw block */
extern ParticleEmitter field_effect_templates[8]; /* the eight template emitters */

/* FieldView members (800af880). */
extern Fixed field_view[3];                    /* camera eye */
extern Fixed field_view_target[3];             /* camera look-at */
extern Fixed field_view_eye_goal[3];           /* second camera eye */
extern Fixed field_view_target_goal[3];        /* second camera look-at */
extern s32 field_view_target_follow_divisor;
extern s32 field_view_eye_follow_divisor;
extern s16 field_view_heading_angle;
extern s32 field_view_heading_high;
extern u8 field_view_heading_blocks0;          /* dolly set */
extern u8 field_view_heading_blocks1;          /* dolly stop */
extern s32 field_view_projection;              /* screen distance */
extern s16 field_view_elevation;
extern u16 field_view_distance;
extern MATRIX field_view_scaled_world;
extern FieldComponents field_view_components;
/* FieldWork members (800b2078). */
extern s16 field_work_sprite_gate;
extern u8 field_work_fog_color[3];          /* fog near colour */
extern u8 field_work_far_color[3];          /* fog far colour */
extern s16 field_work_fog_range[2];         /* fog near, far */
extern s32 field_work_controlled;           /* player actor */
extern s32 field_work_encounter_period;     /* encounter timer */
extern s32 field_work_encounter_step_count; /* encounter number */

#endif
