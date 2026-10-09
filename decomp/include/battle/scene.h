#ifndef BATTLE_SCENE_H
#define BATTLE_SCENE_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/gpu.h"
#include "resident/sound.h"
#include "battle/area.h"
#include "battle/model.h"

/* The battle scene (8009E53C's unit): the battle objects (stage objects and
 * effects), the scene data and its ground triangles, the effect sprite
 * pools, the lights and trackers, and the scene's set-up and release
 * (800A2CA4-800A3490, 800A4820, 800A579C-800A5E9C, 800A8B0C, 800A9F94-
 * 800AA320, 800AA650, 800AAD54-800ADF1C, the object helpers between the
 * effect VM's, 800B10EC). The stage and event script overlays fill it. */

/* A battle object's extra file: more effect scripts and animations. */
typedef struct {
    u8 pad0[4];
    u8 **scripts; /* 0x04: from script 0x4E */
} ExtraFile;

/* A battle object: a stage object or an effect (fields as far as
 * recovered). */
typedef struct BattleObject {
    ModelTable *field0;    /* 0x00: the object's models (D_800C3ACC), NULL unused */
    ModelPart *hierarchy; /* 0x04 */
    u8 **scripts;         /* 0x08: effect scripts 0-0x4F */
    ExtraFile *extra;     /* 0x0C: scripts from 0x50, NULL none */
    u8 *script;           /* 0x10: the running script */
    u8 **animations;      /* 0x14: count, then animations 0-63 */
    u8 **moreAnimations;  /* 0x18: animations from 64 */
    s16 scale1C;          /* 0x1C */
    s16 field1E;          /* 0x1E */
    u8 slot;              /* 0x20 */
    u8 slot2;             /* 0x21 */
    u8 field22; /* 0x22 */
    u8 field23;  /* 0x23 */
    s16 scale24; /* 0x24 */
    s16 scale26; /* 0x26 */
    s16 scale28; /* 0x28 */
    u8 field2A;  /* 0x2A */
    u8 queueCount; /* 0x2B: the running script and the queued ones (5 at most) */
    u8 queueTargets[4]; /* 0x2C: queued script k's target slot at [k - 2] */
    u8 queueScripts[4]; /* 0x30 */
    u8 active;   /* 0x34 */
    u8 field35;  /* 0x35 */
    u8 field36;  /* 0x36 */
    u8 field37;  /* 0x37 */
    u8 field38;  /* 0x38 */
    u8 field39;  /* 0x39 */
    s16 field3A; /* 0x3A */
    u16 field3C; /* 0x3C */
    u8 pad3E[2];
    u16 scriptWait; /* 0x40 */
    u16 field42;    /* 0x42 */
    u8 pad44[0x4A - 0x44];
    u16 flags4A; /* 0x4A */
    s32 field4C; /* 0x4C */
    s32 field50; /* 0x50 */
    s32 field54; /* 0x54 */
    s16 field58;    /* 0x58: target code (0xFA-0xFF special, 1-127 a slot + 1) */
    s16 targetPart; /* 0x5A: part of the target's hierarchy, 0 its root */
    u8 field5C;     /* 0x5C: parent object, 0xFF none */
    u8 field5D;     /* 0x5D: turn with the parent */
    s16 parentPart; /* 0x5E */
    s16 groundY;    /* 0x60 */
    u8 ownSounds;    /* 0x62: the model data's sound bank is loaded */
    u8 extraSounds;  /* 0x63: the extra data's sound bank is loaded */
    s16 offset[3];   /* 0x64: position relative to the target */
    s16 offset2[3];  /* 0x6A */
    s16 motion[12];  /* 0x70 */
    s16 position[3]; /* 0x88 */
    s16 field8E;     /* 0x8E */
    s16 placement[4]; /* 0x90: where its images went (x, y, z, w), x -1 none */
    s16 animation;       /* 0x98: -1 none */
    s16 animationLoop;   /* 0x9A: -1 none */
    s16 animationFrame;  /* 0x9C: the next event */
    s16 animationLength; /* 0x9E: the events */
    u8 *animationStart;  /* 0xA0: the next event (AnimEvent) */
    u8 *animationCursor; /* 0xA4: the first event */
    u8 *modelBlock;  /* 0xA8: its own copy of its models, NULL none */
    void *scriptFile; /* 0xAC: its script file, NULL shared */
    struct ObjectData *model;     /* 0xB0: the model data */
    struct ObjectData *extraData; /* 0xB4: the extra file's data */
    POLY_FT4 shadow[2]; /* 0xB8: one per frame buffer */
    u8 pad108[2];
    u16 slotMask; /* 0x10A */
    u8 channelCount;          /* 0x10C */
    u8 surfaceCount;          /* 0x10D */
    u8 imageCount;            /* 0x10E: image animations at 0x118 */
    u8 pad10F;
    struct ColorFade *channels; /* 0x110: colour fades */
    struct Surface *surfaces; /* 0x114 */
    struct ImageAnim *images; /* 0x118 */
} BattleObject;

/* Layout check (a negative array size fails the build). */
typedef char BattleObjectLayoutCheck[sizeof(BattleObject) == 0x11C ? 1 : -1];

/* An animation header (fields as far as recovered). */
typedef struct {
    u8 pad0[2];
    u16 loop; /* 0x02 */
    u8 pad4[0x12 - 0x4];
    u16 length;     /* 0x12: the event count */
    u32 dataOffset; /* 0x14: offset of the events (AnimEvent) */
} Animation;

/* A rectangle of the scene's ground (8 bytes; the scene data's areas at
 * 0x100, one per formation group). */
typedef struct {
    u16 x0;
    u16 z0;
    u16 x1;
    u16 z1;
} SceneArea;

/* A camera preset of the scene data (12 bytes). */
typedef struct {
    s16 lookAt[3];
    s16 eye[3];
} SceneCamera;

/* The battle scene data (fields as far as recovered). */
typedef struct {
    u8 pad0[0x100];
    SceneArea areas[1]; /* 0x100: one per formation group (count not recovered) */
    u8 pad108[0x344 - 0x108];
    s16 objectScale; /* 0x344: 4.12 */
    u8 pad346[2];
    s16 effectCount; /* 0x348 */
    s16 spriteCount; /* 0x34A */
    s16 maxX;        /* 0x34C */
    s16 minX;        /* 0x34E */
    s16 minZ;        /* 0x350 */
    s16 maxZ;        /* 0x352 */
    u8 pad354[0x35F - 0x354];
    u8 soundMode; /* 0x35F */
    u8 pad360[0x474 - 0x360];
    u8 ambient[3]; /* 0x474: the objects' back colour */
    u8 pad477;
    u8 shadow[3]; /* 0x478: the shadow sprites' colour */
    u8 pad47B;
    SceneCamera cameras[8]; /* 0x47C: camera 0 is the battle's start view */
    SVECTOR centre; /* 0x4DC */
} BattleSceneData;

/* The battle scene data (ovl2615 loads it), held in the resident pointer
 * D_800658C8 (resident/sound.h: its music's instrument data in other
 * modes). */
#define SCENE_DATA ((BattleSceneData *)D_800658C8)

/* An effect sprite, a record of a sprite pool (0x7C bytes): a
 * quadrilateral of four vertices, a colour fading each tick, and its
 * primitive for both frame buffers. */
typedef struct {
    s16 x0, y0, z0, pad06;
    s16 x1, y1, z1;
    s16 projected; /* 0x0E: the vertices are 3D, projected with the GTE */
    s16 x2, y2, z2;
    s16 age;       /* 0x16: -1 free */
    s16 x3, y3, z3;
    s16 lifetime;  /* 0x1E */
    u16 color[3];  /* 0x20: 10.6 fixed point */
    s16 fade[3];   /* 0x26: per tick */
    POLY_FT4 packets[2]; /* 0x2C: one per frame buffer */
} EffectSprite;

/* A pool of effect sprites; next is the first record that may be free. */
typedef struct SpritePool {
    EffectSprite *records;
    s16 count;
    s16 next;
} SpritePool;

/* A triangle of the scene's light geometry (0xE bytes). */
typedef struct SceneTriangle {
    s16 vertices[3];   /* indices into the scene's points */
    s16 neighbours[3]; /* 0x06: adjacent triangles, -1 none */
    u8 id;             /* 0x0C */
    u8 visited;        /* 0x0D: visit stamp */
} SceneTriangle;

/* A light slot (6 bytes). */
typedef struct {
    u8 active;
    u8 r;
    u8 g;
    u8 b;
    u8 field4;
    u8 field5;
} LightSlot;

/* A position tracker (0x14 bytes, D_800D3304): an offset from a part of a
 * stage object. */
typedef struct Tracker {
    s16 x;
    s16 y;
    s16 z;
    s16 active;     /* 0x06 */
    SVECTOR offset; /* 0x08 */
    s16 object;     /* 0x10: stage object, negative none */
    s16 part;       /* 0x12: its part less one */
} Tracker;

/* Battle scene and effect state. */
extern SVECTOR *D_800D3344;       /* scene points */
extern SceneTriangle *D_800D39CC; /* scene triangles */
extern s32 D_800D3348;            /* scene triangle count */
extern u8 D_800D2F64;             /* triangle visit stamp */
extern u8 D_800C37C8;             /* effects disabled */
extern s16 D_800D2FC8;  /* point count of D_800D2FD0 */
extern u16 *D_800D2FD0; /* (x, z, y) points */
extern u8 D_800D3611;   /* a light slot changed */
extern u8 D_800C3D6C;
extern u8 D_800C3D68;
extern s32 D_800C3E88;
extern s16 D_800C3CF0;
extern Tracker D_800D3304[2];
extern s32 D_800D2D40;
extern s32 D_800D2D48;
extern BattleObject *D_800D3368[]; /* stage objects */
extern BattleEvent D_800C3FE8[];   /* presentation events */
extern u16 D_800C3E30;             /* slot mask */
extern u16 D_800C3D40;
extern EffectPool D_800C3D0C;
extern SpritePool D_800C3D04;
extern s32 D_800C3E38;
extern Panorama *D_800C3D50[2]; /* the stage backdrops (ovl2615 makes them, 8002709C) */
extern void *D_800C3EA0;
extern TextureScroll D_800C3DA0[2]; /* the stage's texture scrolls */
extern s32 D_800C360C;
extern u8 D_800C4000[]; /* per slot */
extern BattleSlot D_800C3EB4[11];

void func_800A4820(void);         /* free the battle scene's resources */
s32 func_800A579C(SVECTOR *point); /* the first scene triangle containing a point */
s32 func_800A5870(SVECTOR *point, s32 index, void *out); /* relate a point to a triangle */
s32 func_800A5914(SVECTOR *point, s32 triangle, s32 depth); /* the triangle containing a point, from a neighbour */
void func_800A8B0C(void);         /* reset the battle scene */
void func_800A9F94(void);         /* free the stage objects and the pools */
void func_800A9FF0(s32 index);    /* free a stage object */

#endif
