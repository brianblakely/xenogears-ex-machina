#ifndef MENU_ACTOR_H
#define MENU_ACTOR_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/model.h"
#include "node.h"

/* The two fighters of the arena (menu3 80073424-8007B270 but the bout and
 * camera functions; menu5 80084BEC-80084FD0, 8008509C-8008518C): their
 * models and moves, shots, trails, hits and input, and the per-frame
 * status, action and motion. */

/* A projectile fired by an actor. */
typedef struct {
    VECTOR pos;      /* 0x00 */
    VECTOR prev;     /* 0x10 */
    SVECTOR velocity; /* 0x20 */
    SVECTOR dir;     /* 0x28: unit direction */
    u8 active;       /* 0x30 */
    u8 speed;
    u8 look;         /* 0x32: trail/impact style */
    u8 steer;        /* 0x33: how speed and homing change */
    s32 dist;        /* 0x34: distance to the opponent's core */
    s32 unk38;
    s16 unk3C;
    s16 homing;      /* 0x3E: 0x1000 = turn fully toward the target */
    s16 life;        /* 0x40: frames left */
    u8 unk42[0x2];
} Shot;

/* Projectile kinds (D_800910F4). */
typedef struct {
    u16 unk0;
    u8 unk2;
    u8 unk3;
    u8 speed;
    u8 unk5;
    u8 sound;
    u8 unk7;
} ShotKind;

/* A trail segment: two end points with their previous positions. */
typedef struct {
    VECTOR a;          /* 0x00 */
    VECTOR a_prev;     /* 0x10 */
    VECTOR b;          /* 0x20 */
    VECTOR b_prev;     /* 0x30 */
    s16 effect;        /* 0x40: hit effect */
    u8 unk42;
    u8 unk43;
    u32 unk44_0 : 1;
    u32 flip : 1;
    u32 unk44_2 : 6;
    u32 state : 8;     /* 0x45: 0 free, 1 fading, 2 new */
    u32 unk46 : 8;
    u32 unk47 : 8;
    u8 frame;          /* 0x48: frame it was last extended */
    u8 unk49[0x3];
    s32 style;         /* 0x4C */
    u8 *unk50;
} Trail;

/* An actor's current pose: position and, in the low 12 bits of flags,
 * its facing; bit 15 marks a charged pose. */
typedef struct {
    s16 x, y, z;
    u16 flags;       /* bit 13, 14: mirrored actor flags 15, 19 */
    s16 unk8;
    u16 unkA;        /* bit 8: mirrored actor flag 2 */
} Pose;

/* Pose flags read as signed bit-fields (80079d6c). */
typedef struct {
    s16 facing : 12;
    s16 unk12 : 2;
    s16 flag19 : 1;  /* mirrored actor flag 19 */
} PoseFlagBits;

/* An actor's current move. */
typedef struct {
    u8 unk0[0x6];
    u16 flags;       /* 0x1000: resets the visible parts */
    u8 anim;
    u8 unk9;
    u8 unkA;
} Move;

/* Per-side hit bookkeeping (D_80096FB8, one record per side). */
typedef struct SideHits {
    s32 unk0;
    u8 unk4;
    s32 unk8;
    s32 unkC;
    s32 unk10;
} SideHits;

/* Header of an actor's loaded model file (Actor 0x8FC). */
typedef struct SceneHeader {
    u8 core_part;    /* model part and vertex of the upper anchor */
    u8 foot_a_part;  /* model parts and vertices of the feet */
    u8 foot_b_part;
    u8 unk3;
    s16 foot_a_vertex;
    s16 foot_b_vertex;
    s16 core_vertex;
    u8 unkA[0x2];
    u16 unkC;
    u8 unkE;
    u8 unkF;
    u8 unk10[3];
    u8 unk13;
    s16 unk14;
    s16 unk16;
    s16 unk18;
    s16 unk1A;
    s16 unk1C;
    u16 unk1E;
    u16 unk20;       /* model scale */
    u8 unk22[0x2];
    s16 unk24;       /* camera values for the victory view */
    s16 unk26;
    s16 unk28;
    s16 unk2A;
    s16 unk2C;       /* nonzero: the winner keeps the stage */
    u8 unk2E[0x2];
    s32 unk30;       /* offset of a table from the header */
} SceneHeader;

/* A loaded model file. */
typedef struct {
    u8 unk0[0x10];
    SceneHeader *header; /* 0x10 */
    s32 unk14;
    u8 (*parts)[4];    /* 0x18: byte 3 set = part kind A */
    u8 unk1C[4];
    u8 *image;         /* 0x20: palette and emblem pixels */
} ModelData;

/* A character moved in the menu scene. */
typedef struct Actor {
    VECTOR pos;          /* 0x00 */
    VECTOR velocity;     /* 0x10: vy is the vertical speed */
    VECTOR push;         /* 0x20: horizontal push (vx, vz) */
    VECTOR unk30;        /* 0x30: bounce step */
    s32 unk40;           /* 0x40: forward speed */
    s32 unk44;           /* 0x44: bounce speed */
    s32 state;           /* 0x48: stick speed, up to 0x100 */
    u8 anim;             /* 0x4C */
    u8 unk4D;
    u8 unk4E;
    u8 unk4F;
    s16 event_frame;     /* 0x50: last frame whose events ran */
    u8 unk52;
    u8 unk53;
    s32 angle;           /* 0x54: facing, 4096 = full turn */
    s32 target_angle;    /* 0x58: stick heading from the facing toward the opponent */
    Node *node;          /* 0x5C: model set node */
    void *object;        /* 0x60 */
    u8 unk64[0x8];
    VECTOR start;        /* 0x6C: position at the round start */
    s32 unk7C;
    struct MoveSlot *move_slots; /* 0x80: one per combo number */
    u8 *unk84;
    s32 unk88;
    s32 unk8C;
    s32 unk90;
    s32 unk94;
    s32 accel;           /* 0x98 */
    s32 brake;           /* 0x9C */
    u8 unkA0[0x8];
    s32 unkA8;
    u8 unkAC[0x4];
    s32 floor_y;         /* 0xB0 */
    s16 hp;              /* 0xB4 */
    s16 charge;          /* 0xB6: 0x1000 = full */
    s16 unkB8;
    s16 unkBA;
    s16 max_hp;          /* 0xBC */
    s16 unkBE;           /* 0xBE: charge a special move needs */
    u8 unkC0;
    u8 unkC1;
    u8 level;            /* 0xC2 */
    u8 unkC3;
    u8 unkC4;
    u8 unkC5;
    u8 unkC6;            /* 0xC6: last unkC4 */
    u8 unkC7;
    s16 unkC8;
    s16 unkCA;
    s16 unkCC;
    s16 unkCE;
    u32 flags;           /* 0xD0: bit 27 = side */
    u32 unkD4;
    struct Actor *opponent; /* 0xD8 */
    u8 unkDC[0xC];
    s32 unkE8;
    u8 unkEC[0x4];
    s16 unkF0;           /* 0xF0: frames since the last hit reaction */
    s16 unkF2;
    s32 unkF4;
    u8 unkF8[0x4];
    s32 unkFC;           /* 0xFC: heading offset */
    s32 unk100;
    Trail trails[16];    /* 0x104 */
    s32 unk644;
    s32 unk648;          /* 0x648: heading */
    Shot shots[9];       /* 0x64C */
    u8 unk8B0[0x44];
    s32 nearest_dist;    /* 0x8F4: distance of the closest shot */
    Shot *nearest_shot;  /* 0x8F8 */
    SceneHeader *header; /* 0x8FC */
    u8 *unk900;
    u8 *visible;         /* 0x904: objects shown by the current move */
    u8 visible_count;
    u8 model_id;         /* 0x909 */
    u8 kind;             /* 0x90A: bits 0-2 */
    u8 unk90B;
    s16 unk90C;          /* 0x90C: heading held during a move */
    u8 hold_anim;        /* 0x90E: move held by a kind-2 animation */
    u8 unk90F;
    u8 unk910;
    u8 move_count;       /* 0x911: special moves the opponent may pick */
    u8 parts_b;          /* 0x912 */
    u8 unk913;
    s16 unk914;
    s16 unk916;
    u8 glow;             /* 0x918: light level, fades by 0x18 a frame */
    u8 unk919[0x3];
    VECTOR hit_point;    /* 0x91C: where the last hit landed */
    VECTOR unk92C;       /* 0x92C: a second anchor point; with home, spans the actor */
    VECTOR home;         /* 0x93C */
    VECTOR core;         /* 0x94C: where shots home in */
    VECTOR start_home;   /* 0x95C: home at the round start */
    s16 foot_b_y;        /* 0x96C: last foot heights */
    s16 foot_a_y;
    s32 unk970;
    VECTOR hit_from;     /* 0x974: where the last hit came from */
    u8 unk984[0x14];
    s16 unk998;
    s16 unk99A;
    s16 anim_speed;      /* 0x99C: frames per animation step */
    s16 unk99E;
    u8 inputs[32];       /* 0x9A0: queued pad inputs (ring) */
    u8 input_head;
    u8 input_tail;
    u8 input_count;
    u8 unk9C3;
    u8 unk9C4[0x8];
    u8 unk9CC[0xC00];    /* 0x9CC: own pose block */
    Pose *pose;          /* 0x15CC */
    Move *move;          /* 0x15D0 */
    CVECTOR colour;        /* 0x15D4: effect colour */
    u8 unk15D8[0x10];
    s16 unk15E8;
    s16 unk15EA;
    s16 unk15EC;
    s16 unk15EE;
    s16 unk15F0;
    s16 unk15F2;
    s16 unk15F4;
    s16 unk15F6;
    s16 unk15F8;
    u8 unk15FA[0x2];
    struct Brain *brain; /* 0x15FC: the computer opponent's state */
    struct MoveList *moves; /* 0x1600 */
    POLY_FT4 backdrop[2]; /* 0x1604: one per buffer */
    s32 unk1654;
    s32 unk1658;
    s32 unk165C;
    s32 unk1660;
    u8 *sounds;          /* 0x1664: command sound table */
    s16 unk1668;
} Actor;

/* An actor's flag word as bit-fields (8007920c): bit 16 keeps last
 * frame's bit 15 (the 0x8000 dash flag). */
typedef struct {
    u32 unk0 : 11;
    u32 flag11 : 1;    /* the animation reached its end */
    u32 unk12 : 3;
    u32 flag15 : 1;
    u32 flag16 : 1;
    u32 flag17 : 1;    /* the actor faces the other way (D_800928F4) */
    u32 unk18 : 14;
} ActorFlagBits;

/* The low bits of an actor's unkD4 word as bit-fields (8007920c): the
 * stance effect shown (0 none, 1 stance 1, 2 stance 1 dashing, 3 move)
 * and last frame's. */
typedef struct {
    u32 stance : 2;
    u32 prev_stance : 2;
    u32 unk4 : 28;
} ActorStanceBits;

#define ACTOR_FLAG_BITS(actor) ((ActorFlagBits *)&(actor)->flags)
#define ACTOR_STANCE_BITS(actor) ((ActorStanceBits *)&(actor)->unkD4)

/* A move's frame event: runs its spec (header offset) on frames first..last. */
typedef struct {
    u8 first;
    u8 last;
    s16 spec;
} FrameEvent;

/* A frame event's operands: its kind (func_80074678's case), the type its
 * kind dispatches on, and the model part and vertex of one or two points.
 * Kinds 1, 3, 4 and 5 read only the first 4, 1, 2 and 2 bytes. */
typedef struct {
    u8 unk0; /* kind */
    u8 type;
    u8 part_a;
    u8 part_b;
    s16 vertex_a;
    s16 vertex_b;
} HitSpec;

#define ACTOR_SIDE(actor) (((actor)->flags >> 27) & 1)

/* Per animation: how it ends (0 stop, 1 chain, 2 hold, 3 loop) and the next one. */
typedef struct {
    u8 kind;
    s8 next;
} AnimRule;

/* Which special moves an actor has learned and may use. */
/* Per model id (D_80092874, 0x20 bytes each). */
typedef struct MoveList {
    s16 base;       /* 0x00: scales the combo damage (percent per level) */
    u8 unk2[0x2];
    s16 level;      /* 0x04: level required to pick the model */
    u8 tendency[4]; /* 0x06: eagerness values for the brain */
    u8 learned[14]; /* 0x0A: per combo number from 1 (parts present) */
    u8 unk18;       /* 0x18: power of the charged shot */
    u8 unk19[0x7];
} MoveList;

typedef struct MoveSlot {
    u8 unk0[0x3];
    u8 usable;
} MoveSlot;

extern ShotKind D_800910F4[];
extern s32 D_8009112C;           /* "ETHER": the name of an ether attack */
extern AnimRule D_80091130[];
extern u8 D_80091178[];          /* pairs: next combo number after each button */
extern s32 D_80091198[];         /* the name of each combo number */
extern SpriteModel D_80091FB0;   /* the extra object attached to the actors */
extern u8 D_800925A4[15][3];     /* each combo's command inputs (1 A, 2 B), by special move */
extern s32 D_8009284C;           /* horizontal distance between the actors */
extern s32 D_80092850;           /* distance between the actors */
extern MoveList *D_80092874;     /* per model id */
extern s32 D_80092934;           /* heading from the second actor to the first */
extern SideHits D_80096FB8[2];
extern Actor D_80097010;
extern Actor D_8009872C;

void func_80073B7C(Actor *actor, s32 part, s32 vertex, VECTOR *out);
s32 func_80073DE4(Actor *actor, s32 amount);
void func_80074678(Actor *actor, s16 frame, s16 count);
void func_80074BA4(Actor *actor);
void func_8007639C(Actor *actor, u8 input); /* queue a command input */
void func_80076424(Actor *actor);
void func_8007661C(Actor *actor);
s32 func_800767C8(Actor *actor);
s32 func_80077584(Actor *actor, s32 angle, s32 shift, s32 lift);
void func_80078ED4(s16 *params);
void func_80078F00(Actor *actor);
void func_8007B210(Actor *model, s32 mode);
void func_80084BEC(Actor *actor);
void func_8008509C(s32 which, s32 id);
void func_80085134(s32 which);

#endif
