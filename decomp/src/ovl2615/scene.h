#ifndef OVL2615_SCENE_H
#define OVL2615_SCENE_H

/* The battle scene data (the resident pointer 0x8005949C, a u8 * in
 * resident/mode.h, and the scene data pointer 0x800658C8): the standing
 * positions of each formation group and of the members placed alone, the
 * stage description, its origin and colours, and the offsets of the stage
 * actors, lights and motion. The setup places the formation from it
 * (ovl2615.c) and builds the stage (stage.c). */

#include "common.h"

typedef struct {
    u16 x, z;
} ScenePos;

typedef struct {
    u8 pad0[4];
    ScenePos party[3];  /* 0x04 */
    ScenePos enemy[4];  /* 0x10 */
} SceneGroup;

/* The standing places of the slots that fight in a gear, one per group. */
typedef struct {
    ScenePos party;
    ScenePos enemy;
} SceneGear;

/* A stage object (0x28 bytes): panoramas (types 1 and 2, the second with
 * fills in the fog colour), the backdrop (3), fog (5) and texture scrolls
 * (7). */
typedef struct {
    s32 position[4];      /* 0x00: a VECTOR */
    s16 v10;              /* 0x10 */
    s16 v12;              /* 0x12 */
    s16 v14;              /* 0x14 */
    s16 v16;              /* 0x16 */
    u16 type;             /* 0x18 */
    s16 v1A;              /* 0x1A */
    s16 v1C;              /* 0x1C */
    s16 v1E;              /* 0x1E */
    s16 v20;              /* 0x20 */
    s16 v22;              /* 0x22 */
    s16 v24;              /* 0x24 */
    s16 v26;              /* 0x26 */
} StageObject;

/* The scene's stage description (scene data + 0x340). */
typedef struct {
    u8 flags[4];          /* 0x000: published to 800d2d10 */
    u8 pad4[0x1A];
    u8 fog;               /* 0x01E: set by a fog object */
    u8 pad1F;
    StageObject objects[6]; /* 0x020 */
    s16 backdrop[4];      /* 0x110: backdrop tiling and texture position */
    u8 fogColour[4];      /* 0x118: a CVECTOR */
} StageInfo;

typedef struct {
    SceneGroup group[8];  /* 0x000 */
    SceneGear gear[8];    /* 0x100 */
    u8 pad140[0x340 - 0x140];
    StageInfo info;       /* 0x340 */
    u8 pad45C[0x464 - 0x45C];
    s16 origin[3];        /* 0x464 */
    u8 pad46A[2];
    s16 colours[3];       /* 0x46C: the colour matrix's first column */
    u8 pad472[2];
    u8 back[3];           /* 0x474: the GTE back colour */
    u8 pad477[0x50C - 0x477];
    s32 actors;           /* 0x50C: offsets in the scene data (0: none) */
    s32 lights;           /* 0x510 */
    s32 motion;           /* 0x514 */
} BattleScene;

#endif
