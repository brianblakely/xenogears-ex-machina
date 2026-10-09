#ifndef BATTLE_OBJECTS_H
#define BATTLE_OBJECTS_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/model.h"
#include "battle/scene.h"

/* The stage objects (8009E53C's unit, 8009F794-800A0838, 800A2BB8-800A2CA4,
 * 800A8A88-800AAD54 and 800AFB4C-800B15D8): their files and the camera
 * channels, their creation, selection, effects and per-frame update (their
 * animation events are in battle/effect.h). */

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

/* A stage object's description: its scales and flags, then its mesh
 * descriptions (MeshDesc and its keys). */
typedef struct {
    u8 pad0[2];
    s16 scale24;       /* 0x02 */
    s16 scale26;       /* 0x04 */
    s16 scale28;       /* 0x06 */
    s16 scale;         /* 0x08 */
    u8 field2A;        /* 0x0A */
    u8 padB;
    u16 flags;         /* 0x0C: the object's flags4A */
    u8 channelCount;   /* 0x0E */
    u8 padF;
    u8 imageAnimCount; /* 0x10 */
    u8 pad11;
    u8 meshCount;      /* 0x12 */
    u8 pad13;
    s16 meshes[1];     /* 0x14 */
} ObjectDesc;

/* The header of a stage object's model file. */
typedef struct {
    u8 pad0[4];
    ObjectDesc *desc;  /* 0x04 */
    void *meshData[1]; /* 0x08: per mesh */
} ObjectHeader;

/* A stage object's model file (relocated by 8003342C). */
typedef struct {
    u8 pad0[4];
    void *images;         /* 0x04 */
    u8 *models;           /* 0x08: the model group */
    u16 *hierarchy;       /* 0x0C: after the models */
    ObjectHeader *header; /* 0x10 */
} ObjectModelFile;

/* Per-frame update and drawing of the stage objects. */
extern s16 D_800D39E8;     /* a slow wave (4..9) */
extern u16 D_800C3D14;     /* highlighted slots */
extern u8 D_800C3DF8;      /* effects run */
extern MATRIX *D_800D2FC0; /* the stage colour matrix */
extern SoundBank *D_800C4924;
extern SVECTOR D_800D3354; /* camera position */
extern SVECTOR D_800D335C; /* camera look-at point */
extern s16 D_800C3542;     /* last scene triangle under the camera's view point */
extern s16 D_800C3544;     /* its ground height */
extern s16 D_800C3546;     /* key of the last update */

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

/* A camera channel: an effect entry of D_800C3BAC seen as signed values. */
typedef struct {
    u8 used;
    u8 field1;
    u8 mode;         /* 0x02: bit 0 ease, low nibble < 2 follows objects */
    u8 tag;          /* 0x03: matched against D_800C3B84 */
    s16 current[3];  /* 0x04 */
    s16 slot;        /* 0x0A: the followed object (or the target x) */
    s16 height;      /* 0x0C: subtracted from its height (or the target y) */
    s16 slot2;       /* 0x0E: a second object, -1 none (or the target z) */
    u16 progress;    /* 0x10 */
    s16 duration;    /* 0x12 */
} CameraChannel;

void func_800A979C(s32 index, s16 texture_x, s16 texture_y, s16 clut_x, s16 clut_y); /* create a gear object */
void func_800AA454(u16 index, u16 mask, s32 script); /* select an object and start its effect */
s32 func_800AA600(s32 index);            /* the scaled size of an object */
void func_800AA788(s32 value);           /* a sprite script command: set the flag D_800C3B74 */
void func_800AA79C(s32 a, s32 b);        /* swap two stage objects */
void func_800AA934(BattleObject *object, BattleObject *target, EffectPool *pool, s32 arg3); /* start or queue its effect */
void func_800B136C(void);                /* wait until no object is busy */
void func_800B14CC(s32 keep);            /* end the party's objects other than keep's */

#endif
