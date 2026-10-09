#ifndef OVL2143_ACTORS_H
#define OVL2143_ACTORS_H

/* ovl2143, the actor module linked at 0x801DC000: up to ten actors
 * (D_801E8670), each a model hierarchy (battle/model.h) posed by tweens and
 * run by effect scripts and animation events with the battle's records
 * (battle/effect.h; decomp/src/ovl2143/ovl2143.c pairs its code with the
 * battle's). Each disc holds three byte-identical copies: directory (4, 0)
 * file 0x6B9 (Disc 1 slot 2143, Disc 2 slot 2138), directory (0x10, 0) file
 * 0xC (2604/2599) and directory (0x24, 0) file 0x28 (3960/3955). Loaders:
 * - the field (its layers are the actors): 80077884 reads file 0x6B9 into
 *   the heap's top block of (end & 0xFFFFFF) - 0x1DC008 bytes, whose data
 *   starts at 0x801DC000, with each layer's files 0x6BA/0x6BB + id; 80077AB4
 *   resets the module and creates the layers; the menu runner 800799D4 keeps
 *   a copy of it while a menu runs.
 * - menu screen 5, the Gear parts shop (ovl2602), which shows the chosen
 *   gear as actor 1: the field's menu runner 800799D4 reads directory
 *   (0x10, 0) file 0xC to 0x1DC000, the resident's debug start 8001C1A8
 *   directory (4, 0) file 0x6B9 to 0x801DC000.
 * The world map's 80076098 would draw actor 0 as a distant landmark, but no
 * image calls or points to it, no world map code creates actors and none of
 * its file reads names its directory's copy.
 *
 * The field calls 801E72CC, 801E7378, 801E738C, 801E742C, 801E7D14,
 * 801E7FD4, 801E8030 and 801E8330; ovl2602 801E738C, 801E742C, 801E7D14,
 * 801E7FD4, 801E8030 and 801E8330; both set D_801E8644 and read actors of
 * D_801E8670, as does the world map's 80076098 (801E7D14). Declared here are
 * the entries every caller converts as the definition does (the field's
 * calls of 801E8330 pass values that already fit its halfwords); each target
 * declares 801E7D14 itself, since ovl2602 calls it with four arguments. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/sound.h"
#include "battle/model.h"

/* An actor's sound block (the script file's second part, an offset table
 * relocated in place): its sound effect bank, then the end of the bank's
 * data (the same address when there is none). */
typedef struct SoundOwner {
    u8 pad0[8];
    SoundBank *bank;        /* +8 */
    void *end;              /* +c */
} SoundOwner;

/* A table of script entry points. */
typedef struct EntryTable {
    s32 count;
    s32 *entries;
} EntryTable;

/* An actor (0x134 bytes): a model hierarchy with its script state. Up to
 * +0x11C it has the layout of the battle's BattleObject (battle/scene.h);
 * the module adds the root's movement of each step. */
typedef struct Actor {
    ModelTable *models;     /* +0 */
    ModelPart *parts;       /* +4: the root, then the other parts */
    s32 *entries;           /* +8: script entry points */
    EntryTable *shared;     /* +c: entries 0x50- */
    s32 pc;                 /* +10: script position */
    s32 *locals;            /* +14: [0] count, then entries 0-0x3f */
    s32 *globals;           /* +18: [0] count, then entries 0x40- */
    s16 scale;              /* +1c */
    s16 h1E;                /* +1e */
    u8 index;               /* +20: slot in D_801E8670 */
    u8 b21;                 /* +21 */
    u8 b22;                 /* +22 */
    u8 b23;                 /* +23 */
    s16 size[3];            /* +24: height, x and z extents */
    u8 reference;           /* +2a: default entry reference (bit 7 flag) */
    u8 depth;               /* +2b: queued calls + 1 */
    u8 queue_source[4];     /* +2c */
    u8 queue_entry[4];      /* +30 */
    u8 active;              /* +34: drawn and run */
    u8 b35;                 /* +35 */
    u8 b36;                 /* +36: held: 801E7298 leaves the root's height */
    u8 scaled;              /* +37: build with the scaled hierarchy update */
    u8 b38;                 /* +38 */
    u8 b39;                 /* +39 */
    s16 h3A;                /* +3a */
    u16 h3C;                /* +3c */
    s16 h3E;                /* +3e */
    u16 h40;                /* +40 */
    u16 h42;                /* +42 */
    u16 h44;                /* +44 */
    u16 h46;                /* +46 */
    s16 h48;                /* +48 */
    u16 flags;              /* +4a: bit 0 no shadow (801DCEC8); 4 the images
                             * uploaded off and the parts without the texture
                             * override (801E742C); 8 the width from the x
                             * size (801E8480) */
    s32 w4C;                /* +4c */
    s32 w50;                /* +50 */
    s32 w54;                /* +54 */
    s16 aim_actor;          /* +58: reference of the actor aimed at */
    s16 aim_node;           /* +5a */
    u8 parent;              /* +5c: actor carrying this one, 0xff none */
    u8 inherit;             /* +5d: take the carrier node's rotation */
    s16 parent_node;        /* +5e */
    s16 groundY;            /* +60: the shadow lies here and shrinks with the
                             * root's height above it (801DCEC8, as the
                             * battle's 8009F844); 801E7298 puts the root here */
    u8 b62;                 /* +62 */
    u8 b63;                 /* +63 */
    s16 aim_offset[3];      /* +64: point aimed at in the node's space */
    s16 offset[3];          /* +6a: position in the carrier node's space */
    s16 spin[3];            /* +70: angular step, applied in eighths */
    s16 spin_accel[3];      /* +76: added to spin each tick */
    s16 drift[3];           /* +7c: local movement before model/actor scaling */
    s16 drift_accel[3];     /* +82: added to drift each tick */
    s16 target[3];          /* +88 */
    s16 h8E;                /* +8e */
    s16 h90;                /* +90: -1 none */
    s16 h92;                /* +92 */
    u16 shift_x;            /* +94: added to image animation positions */
    u16 shift_y;            /* +96 */
    s16 anim_state;         /* +98: the frame, -1 none */
    s16 anim_loop;          /* +9a: -1 no loop */
    s16 anim_frame;         /* +9c: the events run */
    s16 anim_frames;        /* +9e: the event count */
    u8 *anim_pos;           /* +a0: next event */
    u8 *anim_start;         /* +a4 */
    void *group;            /* +a8: the model group block */
    void *blockAC;          /* +ac */
    SoundOwner *ownerB0;    /* +b0: the actor's sound bank */
    SoundOwner *ownerB4;    /* +b4 */
    POLY_FT4 prims[2];      /* +b8: a shadow quad per buffer */
    u8 pad108[2];
    u16 mask;               /* +10a */
    u8 channel_count;       /* +10c */
    u8 surfaceCount;        /* +10d */
    u8 imageCount;          /* +10e */
    u8 pad10F;
    struct ColorFade *channels; /* +110: battle/effect.h */
    struct Surface *surfaces;   /* +114 */
    struct ImageAnim *images;   /* +118 */
    s32 previous[3];        /* +11c: root position before the step */
    s32 moved[3];           /* +128: root movement of the step (previous less
                             * the new position, 801E7D14) */
} Actor;

LAYOUT_CHECK(ActorLayoutCheck, sizeof(Actor) == 0x134);

/* An actor's description (the model file's +10 record's +4): its extents,
 * scale, reference, flags and record counts, then each surface's
 * parameters. */
typedef struct ActorDesc {
    u8 pad0[2];
    s16 size[3];            /* +2 */
    s16 scale;              /* +8 */
    u8 reference;           /* +a */
    u8 padB;
    u16 flags;              /* +c */
    u8 channel_count;       /* +e */
    u8 padF;
    u8 imageCount;          /* +10 */
    u8 pad11;
    u8 surfaceCount;        /* +12 */
    u8 pad13;
    u16 records[1];         /* +14: the surfaces' parameters */
} ActorDesc;

typedef struct ActorInfo {
    u8 pad0[4];
    ActorDesc *desc;        /* +4 */
    s32 *tables[1];         /* +8: per surface */
} ActorInfo;

/* A model file's hierarchy entry: a model index (ffff: none) and its parent
 * entry. */
typedef struct HierarchyLink {
    u16 model;
    u16 parent;
} HierarchyLink;

/* An actor's model file (relocated by 8003342C): its images, the model group
 * (up to the hierarchy links) and its description. */
typedef struct ActorFile {
    u8 pad0[4];
    void *images;           /* +4 */
    u8 *group;              /* +8 */
    HierarchyLink *links;   /* +c: follows the model group */
    ActorInfo *info;        /* +10 */
} ActorFile;

/* An actor's script file (relocated by 8003342C): its script block and its
 * sound bank. */
typedef struct ScriptBlock {
    u8 pad0[4];
    s32 *locals;            /* +4 */
    s32 entries[1];         /* +8 */
} ScriptBlock;

typedef struct ActorScript {
    u8 pad0[4];
    ScriptBlock *script;    /* +4 */
    SoundOwner *owner;      /* +8 */
} ActorScript;

/* The actors' colour matrix: the callers set it, 801E7D14 loads it and the
 * light events write its columns (801E5D44). */
extern MATRIX *D_801E8644;
extern Actor *D_801E8670[10]; /* the actors */

/* The world matrix of node `node` of actor `index` (its root's transform for
 * node 0). */
void func_801E72CC(MATRIX *out, MATRIX *unused, s32 index, s32 node);
void func_801E7378(s32 on); /* while on, tweens to keyframes apply at once (801E39F0 op 0x13) */
void func_801E738C(s32 slot_count); /* reset the module and its pools */
void func_801E742C(s32 index, u16 flags, ActorScript *script, ActorFile *file, s16 x, s16 y, s16 z,
                   s16 w, s16 *pos); /* create actor `index` from its files */
void func_801E7FD4(void);   /* release every actor and both pools */
void func_801E8030(s32 index); /* release actor `index` */
/* Select actor `index` and mask `mask` and run its script entry `entry`. */
void func_801E8330(u16 index, u16 mask, s32 entry);

#endif
