#ifndef BATTLE_OBJECTS_H
#define BATTLE_OBJECTS_H

#include "common.h"
#include "psyq.h"
#include "scene.h"
#include "battle_core.h"
#include "effect.h"

/* An object's model or extra data (fields as far as recovered). */
struct ObjectData {
    u8 pad0[4];
    void *image;        /* 0x04: image == sounds when there is none */
    SoundSystem *sounds; /* 0x08: its sound bank */
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

extern s32 D_800C3B6C; /* the model list slot being filled */
extern u8 *D_800C3B70; /* the model group being loaded */

/* Resident services. */
void func_80030988(s32 a, s32 b, s32 c, s32 d);
s32 func_8003864C(SoundSystem *bank, s32 mode); /* whether a sound bank is loaded */
void func_80038428(SoundSystem *bank);          /* load a sound bank */
void func_8002C644(u8 *group);
void func_8002C4BC(u8 *group);
s32 func_80031894(void *block); /* a block's size */

void func_800AA6E0(BattleObject *object);
void func_800A8A88(Surface *surface);
void func_800A8BF0(s32 index, u16 flags, ObjectScriptFile *scriptFile, ObjectModelFile *modelFile, s16 x, s16 y,
                   s16 z, s16 w, SVECTOR *position);
void func_800AA898(BattleObject *object, EffectPool *pool, u8 **scripts, u8 **animations);

/* Per-frame update and drawing of the stage objects. */
extern s16 D_800C3B80;     /* pulse level of the highlight colour */
extern s16 D_800D39E8;     /* a slow wave (4..9) */
extern u16 D_800C3D14;     /* highlighted slots */
extern u8 D_800C3DF8;      /* effects run */
extern MATRIX *D_800D2FC0; /* the stage colour matrix */
extern s32 D_80050104;     /* resident: drawing with lighting */
extern SoundSystem *D_800C4924;
extern SVECTOR D_800D3354; /* camera position */
extern SVECTOR D_800D335C; /* camera look-at point */
extern s16 D_800C3542;     /* last scene triangle under the camera's view point */
extern s16 D_800C3544;     /* its ground height */
extern s16 D_800C3546;     /* key of the last update */

/* Resident services. */
s32 func_8003F8B0(s32 angle); /* rcos (4096 = 1.0) */

void func_8009F844(BattleObject *object, MATRIX *m, s32 arg2, s32 arg3, s32 skipped, u32 *ot, s32 buffer);
s32 func_800AAA20(BattleObject *object, EffectPool *pool, s32 steps, s32 arg3, s32 arg4);
void func_800AAB34(BattleObject *object);
u8 func_800AA514(s16 a, s16 b, s32 c);
s32 func_800AA600(s32 index);
void func_8009F794(ModelList *list, s32 release);
void func_800A2BB8(EffectPool *pool, ModelPart *part, u8 kind);
void func_800B026C(EffectPool *pool, s32 steps, s32 arg2, s32 arg3);

/* A resident sprite task (fields as far as the battle uses them): its
 * sprite's position from +0x38, and at +link its caller block. */
typedef struct EffectSprite {
    u8 pad0[0x38];
    s32 x, y, z; /* 0x38: 16.16 */
    u8 pad44[0xBE - 0x44];
    s16 link; /* 0xBE */
} EffectSprite;

/* The battle's block of a sprite following an object part (0x18 bytes). */
typedef struct {
    u8 pad0[4];
    void (*update)(EffectSprite *sprite); /* 0x04: the sprite's own update */
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
    u8 flags;      /* 0x04: 0x80 at the acting object; the slot kinds (800B12D0) */
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
    u8 flags;   /* 0x04: the slot kinds (800B12D0) */
    u8 source;  /* 0x05: of the bank (800AE220) */
    u8 sound2;  /* 0x06: a second sound, 0 none */
    u8 kind;    /* 0x07 */
} SoundEvent;

/* Animation event 8: start effect scripts on the selected slots (0xA bytes). */
typedef struct {
    s16 time;
    u8 type;
    u8 kinds;      /* 0x03: the slot kinds (800B12D0) */
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

extern u8 D_8006BE10[]; /* resident sprite resources */
extern u8 D_8005A474[];

/* Resident sound. */
void func_80039E60(s32 sound);             /* play */
void func_8003A2E4(s32 sound, s32 volume); /* set its volume */

void *func_800AA820(s32 mode);
void func_800AA454(u16 index, u16 mask, s32 script);
void func_800AA564(BattleObject *target, u16 index, u16 mask, s32 script);
void func_800BF6CC(void);
s32 func_800B12D0(s32 slot, u8 mask);
void func_800AFB4C(void *resource, s32 kind, SVECTOR *position, s16 direction, s16 scale, SpriteCommand *command,
                   BattleObject *object);

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

/* The camera. */
extern u8 D_800C3B84;  /* the channel kind reported in D_800C3B88 */
extern u8 D_800C3B88;  /* bit 0: that channel runs, bit 1: it finished */
extern u8 D_800C3B8C;  /* snap: channels 7 and 8 start at their targets */
extern s16 D_800C3B90; /* orbit yaw */
extern s16 D_800C3B94; /* orbit pitch */
extern s16 D_800C3B98; /* orbit distance */
extern s16 D_800C3B9C; /* orbit height */
extern s16 D_800C3BA0; /* look-at yaw */
extern s16 D_800C3BA4; /* look-at distance */
extern s16 D_800C3BA8; /* look-at height */

s32 func_800B0FF4(SVECTOR *from, SVECTOR *point);
s16 func_800B0B14(s32 key);

/* An entry of an effect script file (0x1C bytes); the offsets are from the
 * entry until relocated. */
typedef struct {
    u8 *data0;    /* 0x00 */
    s32 field4;   /* 0x04 */
    u8 *data8;    /* 0x08 */
    s32 fieldC;   /* 0x0C */
    u8 *commands; /* 0x10: 4-byte aligned commands; byte 1 is the length in words less one */
    s32 count;    /* 0x14: commands */
    s32 field18;  /* 0x18 */
} ScriptEntry;

/* An effect script file. */
typedef struct {
    u8 pad0[4];
    u32 flags; /* 0x04: bit 0 relocated */
    u8 pad8[4];
    ScriptEntry entries[1]; /* 0x0C */
} ScriptFile;

extern ScriptEntry D_800C3BD0; /* the selected script */

/* Resident sprites. */
EffectSprite *func_80023FD8(s32 kind, void *resource, SVECTOR *position, s32 size);
void func_80021FE0(void *body, s32 direction);
void func_800223B0(void *body, s32 direction);
void func_80022000(s32 *body, s32 scale);
void *func_8001CD7C(void *task); /* a task's update */
void func_8001CD6C(void *task, void (*update)()); /* set it */

void func_800AFC68(EffectSprite *sprite);

#endif
