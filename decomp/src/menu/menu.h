#ifndef MENU_H
#define MENU_H

#include "common.h"

/* libgte-layout vector: three 32-bit components and padding. */
typedef struct {
    s32 vx;
    s32 vy;
    s32 vz;
    s32 pad;
} Vector;

/* libgpu types (PsyQ). */
typedef struct {
    s16 x, y;
    s16 w, h;
} Rect;

typedef struct {
    u32 mode;
    Rect *crect;
    u32 *caddr;
    Rect *prect;
    u32 *paddr;
} TimImage;

typedef struct {
    u32 tag;
    u32 code[1];
} DrTpage;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
} Sprt16;

/* One of the two display buffers: draw and display environments, then
 * packets linked every frame. */
typedef struct {
    u8 envs[0x74];
    u32 offset_prim[3]; /* 0x74: last word is x | y << 16 */
} MenuFrame;

typedef struct {
    s16 vx, vy, vz, pad;
} SVector;

/* A projectile fired by an actor. */
typedef struct {
    Vector pos;      /* 0x00 */
    Vector prev;     /* 0x10 */
    SVector velocity; /* 0x20 */
    SVector dir;     /* 0x28: unit direction */
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
    Vector a;          /* 0x00 */
    Vector a_prev;     /* 0x10 */
    Vector b;          /* 0x20 */
    Vector b_prev;     /* 0x30 */
    u8 unk40[3];
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
    u16 flags;
} Pose;

/* An actor's current move. */
typedef struct {
    u8 unk0[0x6];
    u16 flags;       /* 0x1000: resets the visible parts */
    u8 anim;
    u8 unk9;
    u8 unkA;
} Move;

/* One object of a model; bit 0 of *flags hides it. */
typedef struct {
    s32 kind;
    u32 *flags;
} ModelObject;

/* A model animation (0x14 bytes). */
typedef struct {
    u8 unk0[0x12];
    s16 unk12;
} ModelAnim;

typedef struct {
    s16 count;
    ModelObject **objects;
    ModelAnim *anims;
} ModelData;

typedef struct {
    u8 unk0[0x4];
    ModelData *data;
} Model;

/* Per-side hit bookkeeping (D_80096FB8, one record per side). */
typedef struct {
    s32 unk0;
    u8 unk4;
    s32 unk8;
    s32 unkC;
    s32 unk10;
} SideHits;

/* A character moved in the menu scene. */
typedef struct Actor {
    Vector pos;          /* 0x00 */
    u8 unk10[0x38];
    s32 state;           /* 0x48 */
    u8 anim;             /* 0x4C */
    u8 unk4D[0x7];
    s32 angle;           /* 0x54: facing, 4096 = full turn */
    s32 target_angle;    /* 0x58 */
    Model *model;        /* 0x5C */
    u8 unk60[0x24];
    u8 *unk84;
    u8 unk88[0x28];
    s32 floor_y;         /* 0xB0 */
    s16 hp;              /* 0xB4 */
    s16 unkB6;           /* 0xB6: charge, 0x1000 = full */
    u8 unkB8[0x2];
    s16 unkBA;
    s16 max_hp;          /* 0xBC */
    s16 unkBE;
    u8 unkC0[0xE];
    s16 unkCE;
    u32 flags;           /* 0xD0: bit 27 = side */
    u8 unkD4[0x4];
    struct Actor *opponent; /* 0xD8 */
    u8 unkDC[0xC];
    s32 unkE8;
    u8 unkEC[0x18];
    Trail trails[16];    /* 0x104 */
    s32 unk644;
    u8 unk648[0x4];
    Shot shots[9];       /* 0x64C */
    u8 unk8B0[0x44];
    s32 nearest_dist;    /* 0x8F4: distance of the closest shot */
    Shot *nearest_shot;  /* 0x8F8 */
    u8 unk8FC[0x8];
    u8 *visible;         /* 0x904: objects shown by the current move */
    u8 visible_count;
    u8 unk909;
    u8 unk90A[0x42];
    Vector core;         /* 0x94C: where shots home in */
    u8 unk95C[0x42];
    s16 unk99E;
    u8 unk9A0[0xC2C];
    Pose *pose;          /* 0x15CC */
    Move *move;          /* 0x15D0 */
    u8 unk15D4[0x14];
    s16 unk15E8;
    s16 unk15EA;
    s16 unk15EC;
    s16 unk15EE;
    u8 unk15F0[0x10];
    u8 *unk1600;
    u8 unk1604[0x50];
    s32 unk1654;
    s32 unk1658;
} Actor;

/* libgte matrix. */
typedef struct {
    s16 m[3][3];
    s32 t[3];
} Matrix;

/* Header of the loaded scene data. */
typedef struct {
    u8 unk0[0x14];
    s16 unk14;
    s16 unk16;
    s16 unk18;
    s16 unk1A;
    s16 unk1C;
    u16 unk1E;
} SceneHeader;

/* Loaded scene data block. */
typedef struct {
    u8 unk0[0x5C];
    s32 unk5C;
    u8 unk60[0x89C];
    SceneHeader *header; /* 0x8FC */
} SceneData;

/* A placed scene model: its position is at 0x34. */
typedef struct {
    u8 unk0[0x34];
    s32 x;
    s32 y;
    s32 z;
} SceneModel;

/* Where a hit effect goes: model part and vertex of one or two points. */
typedef struct {
    u8 unk0;
    u8 type;
    u8 part_a;
    u8 part_b;
    s16 vertex_a;
    s16 vertex_b;
} HitSpec;

/* A sprite effect from the overlay's effect pool (func_8008D3F4). */
typedef struct {
    s16 kind;
    u8 unk2[0x42];
    s16 unk44;
    u8 unk46[0x2];
    s16 unk48;
    s16 unk4A;
    u8 unk4C[0x1C];
    s16 unk68;
    s16 unk6A;
    u8 unk6C[0x8];
    u8 r, g, b; /* 0x74 */
} Effect;

#define ACTOR_SIDE(actor) (((actor)->flags >> 27) & 1)

/* Menu window (resident window code at 80032f54). */
typedef struct {
    s16 unk0[3];
    s16 unk6;
    s16 unk8[2];
    s16 unkC;
    u8 unkE[0x5A];
    u8 unk68;
} MenuWindow;

/* A step in one of eight directions on the floor plane. */
typedef struct {
    s32 x;
    s32 z;
} FloorStep;

/* Scene actors and camera: eye position (D_8009867C) and look-at point
 * (D_8009871C). */
extern Actor D_80097010;
extern Actor D_8009872C;
extern Vector D_8009867C;
extern Vector D_8009871C;
extern Vector D_80099078;
extern s32 D_800925F4; /* vertical camera lift of the current view */

extern MenuWindow D_8009868C; /* message window */
extern MenuWindow D_80092954;

extern u8 *D_800925F8;   /* running scene script */
extern u8 *D_8009105C[]; /* scene scripts */
extern u8 D_80090F38[];
extern u8 D_800910C4[];

extern s32 D_800925D4;
extern s32 D_800925D8;
extern s32 D_800925DC;
extern s32 D_800925E0; /* screen offset x, y */
extern s32 D_800925E4;
extern s32 D_800925E8;
extern s32 D_800925EC;
extern s32 D_800925FC;
extern s16 D_80092600;
extern s8 D_80092604; /* scene choice cursor */
extern u8 D_80092608;
extern s32 D_8009284C;
extern MenuFrame *D_80092868; /* frame being built */
extern s32 D_80092880;
extern u8 D_80092884;
extern u8 D_800928A0; /* index of the frame being built */
extern s32 D_800928C8; /* menu mode */
extern u8 D_800928D4;
extern s32 D_80092900;
extern s32 D_80092904;
extern s32 D_80092934;
extern u8 D_8009293C;
extern s32 D_80092948;
extern u8 D_800929BC;
extern u8 D_800925F0;
extern s32 D_800928E8; /* frame counter */
extern u32 *D_80092938; /* ordering table being built */
extern FloorStep D_80091084[8];
extern u16 D_8005948C; /* pad buttons newly pressed */
extern u16 D_800594A4; /* pad buttons repeating */
extern DrTpage D_800929E4[2];
extern s32 D_80092A00;
extern s32 D_80092A10;
extern s32 D_80092A20;
extern u8 D_80099D9D;
extern u8 D_80099D9E;
extern Sprt16 D_8009A14C;
extern Sprt16 D_8009A244;
extern s32 D_800910F0;
extern s32 D_80092610;
extern SceneData *D_80092614;
extern s8 D_80092618;
extern s32 D_8009261C;
extern s32 D_80092620;
extern s32 D_80092624;
extern s32 D_80092628;
extern s32 D_8009262C;
extern u16 D_80092632;
extern s16 D_800928D0;
extern Vector D_80096FA8; /* scene origin */
extern Matrix D_80091C0C;
extern ShotKind D_800910F4[];
extern SideHits D_80096FB8[2];
extern s32 D_8009112C;
extern s32 D_80099D88;
extern s32 D_80099D8C;
extern u8 D_80050622; /* resident: result of the last menu battle */
extern Effect *D_80092644;
extern s32 D_80092650; /* trail segments added */

/* PsyQ SDK (resident). */
void func_80043B48(void *ot, void *prim);                   /* AddPrim */
u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y);           /* GetTPage */
u16 func_80043A58(s32 x, s32 y);                            /* GetClut */
void func_80043E20(DrTpage *p, s32 dfe, s32 dtd, s32 tpage); /* SetDrawTPage */
void func_80044894(Rect *rect, u32 *data);                  /* LoadImage */
void func_800471B4(u32 *tim);                               /* OpenTIM */
TimImage *func_800471C4(TimImage *image);                   /* ReadTIM */
s32 func_8004B32C(s32 x, s32 z);                            /* ratan2 */
Matrix *func_8004931C(Matrix *m0, Matrix *m1, Matrix *m2);  /* CompMatrix */
s32 func_8003FA38(void);                                    /* rand */
void func_80048D68(Vector *v, SVector *unit);               /* VectorNormalS */
void func_8004901C(SVector *a, SVector *b, s32 pa, s32 pb, SVector *out); /* LoadAverageShort12 */
void func_8004A14C(s32 h);                                  /* SetGeomScreen */

/* Resident game code. */
void func_80030988(s32 a0, s32 a1, s32 a2, s32 a3);
void func_80032F54(MenuWindow *window, s32 x, s32 y, s32 w, s32 h, s32 a5, s32 a6);
s32 func_80033728(s32 table, s32 index);
void func_800346D4(MenuWindow *window);
void func_80034714(MenuWindow *window, s32 text);
void func_80039C4C(s32 arg);
void func_80039FF8(void);
void func_800346A4(MenuWindow *window);
void func_80034800(MenuWindow *window, s32 colour, s32 a2, s32 a3);
void func_80034874(MenuWindow *window, s32 cursor);
void func_80034888(MenuWindow *window, u32 *ot, s32 frame);
void func_80036420(void);
s32 func_8003F8B0(s32 angle); /* sine, 4096 = 1.0 */
s32 func_8003F8CC(s32 angle); /* cosine, 4096 = 1.0 */

/* This overlay. */
s32 func_800707D8(s32 target, s32 current, s32 steps);
void func_8007099C(u32 mode);
void func_80070F80(u8 *script);
void func_8007107C(void);
void func_80071724(u32 *ot);
void func_80073064(SVector *dir, SVector *out, s32 scale);
void func_8008859C(Vector *v, SVector *unit);
s32 func_800886FC(Vector *v);
void func_8007E31C(Vector *from, Vector *to, u8 *colour);
void func_8008EBD0(Actor *actor, s32 sound, Shot *shot, s32 arg);
void func_80073B7C(Actor *actor, s32 part, s32 vertex, Vector *out);
void func_8007C100(u8 *arg);
void func_80076424(Actor *actor);
void func_80074678(Actor *actor, s32 arg1, s32 arg2);
void func_8008B0D8(ModelAnim *anim);
void func_8008B730(ModelAnim *anim, s32 arg1, s32 arg2);
void func_8007C880(s32 side, Vector *at, s32 style, s32 type);
s32 func_8007CD14(s32 side, s32 part, s32 vertex, s32 arg);
void func_8007CD44(s32 side, Vector *a, Vector *b, s32 style);
void func_8007D190(Vector *at, s32 type);
s32 func_8007D25C(s32 type);
void func_8007D65C(Vector *a, Vector *b, s32 type);
void func_80079DF0(Actor *actor, Actor *other);
u32 func_800828C4(Actor *actor);
void func_8007191C(s32 scene);
void func_80071DA4(Actor *actor);
void func_8007E24C(void);
s32 func_80082488(Vector *position, s32 arg);
void func_800828F8(Vector *position, Vector *step, s32 limit);
void func_80083738(Actor *actor, Actor *other);
void func_80083C0C(s32 arg);
s32 func_80083CD8(void);
s32 func_8008F4F4(Actor *actor, s32 mask);
void func_8008EB4C(s32 id);
void func_8007E954(s32 arg);
void func_8007F834(void);
void func_80078F00(SceneData *scene);
void func_80080D10(void);
void func_8008976C(s32 a0, s32 a1);
void func_80089D5C(s32 arg);
s32 func_8008A2B8(s32 arg);
s32 func_8008A3E0(s32 arg);
void func_8008A5BC(s32 arg);
void func_8008BC04(void);
s32 func_8008C2C0(s32 arg);
Effect *func_8008D3F4(s32 kind, s32 arg);
void func_8008D5C0(Effect *effect, s32 arg);

#endif
