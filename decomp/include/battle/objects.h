#ifndef BATTLE_OBJECTS_H
#define BATTLE_OBJECTS_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/model.h"
#include "battle/scene.h"

/* The stage objects (8009E53C's unit, 8009F794-800A0838, 800A2BB8-800A2CA4,
 * 800A8A88-800AAD54 and 800AFB4C-800B15D8): their script files and the
 * camera channels, their creation, selection, effects and per-frame update
 * (their model files are in battle/model.h, their animation events in
 * battle/effect.h). */

/* An object's model or extra data (fields as far as recovered). */
struct ObjectData {
    u8 pad0[4];
    void *image;        /* 0x04: image == sounds when there is none */
    SoundBank *sounds;  /* 0x08: its sound bank */
    void *soundsEnd;     /* 0x0C: the same as sounds when there are none */
    u8 pad10[4];
    void *images;       /* 0x14: additional image data */
    void *imagesEnd;    /* 0x18: the same as images when there are none */
};

/* A stage object's scripts: its animations and effect scripts. */
typedef struct {
    u8 pad0[4];
    u8 **animations; /* 0x04 */
    u8 *scripts[1];  /* 0x08 */
} ObjectScripts;

/* A stage object's script file (relocated by 8003342C). */
typedef struct {
    u8 pad0[4];
    ObjectScripts *scripts;  /* 0x04 */
    struct ObjectData *data; /* 0x08 */
} ObjectScriptFile;

/* Per-frame update and drawing of the stage objects. */
extern s16 battle_surface_wind_strength;     /* a slow wave (4..9) */
extern u16 battle_highlight_slot_mask;     /* highlighted slots */
extern u8 battle_camera_channels_active;      /* effects run */
extern MATRIX *battle_stage_color_matrix; /* the stage colour matrix */
extern SoundBank *battle_sound_bank_of_event_script;
extern SVECTOR battle_camera_view_eye; /* camera position */
extern SVECTOR battle_camera_view_target; /* camera look-at point */
extern s16 battle_camera_ground_triangle;     /* last scene triangle under the camera's view point */
extern s16 battle_camera_ground_height;     /* its ground height */
extern s16 battle_camera_ground_key;     /* key of the last update */

/* The battle's block of a sprite following an object part (0x18 bytes),
 * after the sprite in its resident sprite task (the sprite's size bytes
 * from the task, read signed). */
typedef struct {
    u8 pad0[4];
    void (*update)(Task *task);           /* 0x04: the sprite's own update */
    BattleObject *object;                 /* 0x08 */
    s16 part;                             /* 0x0C: 0 the root */
    s16 onGround;                         /* 0x0E: keep the object's ground height */
    SVECTOR offset;                       /* 0x10: from the part */
} SpriteFollow;

/* A camera channel: an effect entry of battle_camera_channels seen as signed values. */
typedef struct {
    u8 used;
    u8 field1;
    u8 mode;         /* 0x02: bit 0 ease, low nibble < 2 follows objects */
    u8 tag;          /* 0x03: matched against battle_camera_wait_kind */
    s16 current[3];  /* 0x04 */
    s16 slot;        /* 0x0A: the followed object (or the target x) */
    s16 height;      /* 0x0C: subtracted from its height (or the target y) */
    s16 slot2;       /* 0x0E: a second object, -1 none (or the target z) */
    u16 progress;    /* 0x10 */
    s16 duration;    /* 0x12 */
} CameraChannel;

void battle_create_object_from_files(s32 index, s16 texture_x, s16 texture_y, s16 clut_x, s16 clut_y); /* create a gear object */
void battle_start_slot_object_own_script(u16 index, u16 mask, s32 script); /* select an object and start its effect */
s32 battle_get_object_height(s32 index);            /* the scaled size of an object */
void battle_set_object_drawing(s32 value);           /* a sprite script command: set the flag battle_object_drawing_on */
void battle_swap_objects(s32 a, s32 b);        /* swap two stage objects */
void battle_start_effect_script(BattleObject *object, BattleObject *target, EffectPool *pool, s32 id); /* start or queue its effect */
void battle_wait_objects_idle(void);                /* wait until no object is busy */
void battle_end_party_objects(s32 keep);            /* end the party's objects other than keep's */

#endif
