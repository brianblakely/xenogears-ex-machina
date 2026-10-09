#ifndef BATTLE_OBJECTS_H
#define BATTLE_OBJECTS_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/model.h"
#include "battle/scene.h"

/* The stage objects (8009E53C's unit, 8009F794-800A0838, 800A2BB8-800A2CA4,
 * 800A8A88-800AAD54 and 800AFB4C-800B15D8): their files, their animation
 * events and the camera channels, their creation, selection, effects and
 * per-frame update. */

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

/* The start of an animation event: the frame it runs on and its type. */
typedef struct {
    s16 time;
    u8 type;
    u8 index; /* 0x03 */
    u8 arg4;  /* 0x04 */
    u8 arg5;  /* 0x05 */
} EventHeader;

/* Animation event 1: create a sprite (0x14 bytes). */
typedef struct {
    s16 time;
    u8 type;
    u8 kind;       /* 0x03: with flag 0x80 plus the gear's variant less one */
    u8 flags;      /* 0x04: 0x80 at the acting object; event codes that skip it (800B12D0) */
    u8 part;       /* 0x05 */
    s16 offset[3]; /* 0x06 */
    u8 mode;       /* 0x0C: 1 on the ground, 2 at the scene's centre; 0x80 at the acting object */
    u8 absolute;   /* 0x0D: the angle is not relative to the object's */
    s16 angle;     /* 0x0E */
    s16 scale;     /* 0x10 */
    u8 resource;   /* 0x12: 0 D_8006BE10, else D_8005A474 */
    u8 follow;     /* 0x13 */
} SpriteCommand;

/* Animation event 2: a light following a part of the object (0x12 bytes, 6
 * when off). */
typedef struct {
    s16 time;
    u8 type;
    u8 light;       /* 0x03 */
    u8 on;          /* 0x04 */
    u8 free;        /* 0x05: not following the object */
    u8 part;        /* 0x06 */
    u8 r, g, b;     /* 0x07 */
    s16 offset[3];  /* 0x0A */
    s16 active;     /* 0x10 */
} LightEvent;

/* Animation events 3 and 4: an effect channel (0x1C bytes, 6 when off). */
typedef struct {
    s16 time;
    u8 type;
    u8 channel;     /* 0x03 */
    u8 on;          /* 0x04 */
    u8 field5;
    u8 bytes[8];    /* 0x06 */
    s16 values[6];  /* 0x0E */
    u8 last;        /* 0x1A */
} ChannelEvent;

/* Animation event 5: play a sound (8 bytes). */
typedef struct {
    s16 time;
    u8 type;
    u8 sound;   /* 0x03 */
    u8 flags;   /* 0x04: event codes that skip it (800B12D0) */
    u8 source;  /* 0x05: of the bank (800AE220) */
    u8 sound2;  /* 0x06: a second sound, 0 none */
    u8 kind;    /* 0x07 */
} SoundEvent;

/* Animation event 8: start effect scripts on the selected slots (0xA bytes). */
typedef struct {
    s16 time;
    u8 type;
    u8 kinds;      /* 0x03: event codes that skip a slot (800B12D0) */
    u8 onTarget;   /* 0x04: run on this object's target */
    u8 scripts[5]; /* 0x05: by slot code: 0-1 (in a gear the next), 5, 4, 2-3 */
} SlotEvent;

/* Animation event 9: an image animation (0x1C bytes, 6 when off). */
typedef struct {
    s16 time;
    u8 type;
    u8 anim;       /* 0x03 */
    u8 on;         /* 0x04 */
    u8 source;     /* 0x05: another animation to copy, 0xFF none */
    u8 mode;       /* 0x06: low 7 bits the mode, 0x80 at the object's images */
    u8 step;       /* 0x07: the step handler (800AA820) */
    s16 x;         /* 0x08 */
    s16 y;         /* 0x0A */
    s16 x2;        /* 0x0C */
    s16 y2;        /* 0x0E */
    s16 field10;   /* 0x10 */
    u8 field12;    /* 0x12: high nibble 1 moves x2, y2 with the images too */
    u8 field13;    /* 0x13 */
    u8 field14;    /* 0x14 */
    u8 pad15;
    s16 field16;   /* 0x16 */
    s16 field18;   /* 0x18 */
    s16 field1A;   /* 0x1A */
} ImageEvent;

/* An animation event (the object's event list). */
typedef union {
    EventHeader header;
    SpriteCommand sprite;
    LightEvent light;
    ChannelEvent channel;
    SoundEvent sound;
    SlotEvent slots;
    ImageEvent image;
} AnimEvent;

/* A camera channel: an effect entry of D_800C3BAC seen as signed values. */
typedef struct {
    u8 used;
    u8 field1;
    u8 mode;         /* 0x02: bit 0 ease, low nibble < 2 follows objects */
    u8 kind;         /* 0x03 */
    s16 current[3];  /* 0x04 */
    s16 slot;        /* 0x0A: the followed object (or the target x) */
    s16 height;      /* 0x0C: subtracted from its height (or the target y) */
    s16 slot2;       /* 0x0E: a second object, -1 none (or the target z) */
    u16 progress;    /* 0x10 */
    s16 duration;    /* 0x12 */
} CameraChannel;

/* Create a stage object from its script and model files. */
void func_800A8BF0(s32 index, u16 flags, ObjectScriptFile *scriptFile, ObjectModelFile *modelFile, s16 x, s16 y,
                   s16 z, s16 w, SVECTOR *position);
void func_800A979C(s32 index, s16 texture_x, s16 texture_y, s16 clut_x, s16 clut_y); /* create a gear object */
void func_800AA454(u16 index, u16 mask, s32 script); /* select an object and start its effect */
s32 func_800AA600(s32 index);            /* the scaled size of an object */
void func_800AA788(s32 value);           /* a sprite script command: set the flag D_800C3B74 */
void func_800AA79C(s32 a, s32 b);        /* swap two stage objects */
void func_800AA898(BattleObject *object, EffectPool *pool, u8 **scripts, u8 **animations); /* reset an object */
void func_800AA934(BattleObject *object, BattleObject *target, EffectPool *pool, s32 arg3); /* start or queue its effect */
void func_800B136C(void);                /* wait until no object is busy */
void func_800B14CC(s32 keep);            /* end the party's objects other than keep's */

#endif
