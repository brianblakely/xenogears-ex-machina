#ifndef MENU_H
#define MENU_H

#include "common.h"
#include "gpu.h"

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
    s16 *caddr;
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

typedef struct {
    s16 vx, vy, vz, pad;
} SVector;

/* libgpu CVECTOR layout. */
typedef struct {
    u8 r, g, b, cd;
} Color;

/* libgte matrix. */
typedef struct {
    s16 m[3][3];
    s32 t[3];
} Matrix;

#include "system.h"

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
    u16 flags;       /* bit 13, 14: mirrored actor flags 15, 19 */
    s16 unk8;
    u16 unkA;        /* bit 8: mirrored actor flag 2 */
} Pose;

/* An actor's current move. */
typedef struct {
    u8 unk0[0x6];
    u16 flags;       /* 0x1000: resets the visible parts */
    u8 anim;
    u8 unk9;
    u8 unkA;
} Move;

/* Per-side hit bookkeeping (D_80096FB8, one record per side). */
typedef struct {
    s32 unk0;
    u8 unk4;
    s32 unk8;
    s32 unkC;
    s32 unk10;
} SideHits;

/* Header of an actor's loaded model file (Actor 0x8FC). */
typedef struct {
    u8 unk0[0xE];
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
    u8 unk20[0x4];
    s16 unk24;       /* camera values for the victory view */
    s16 unk26;
    s16 unk28;
    s16 unk2A;
    s16 unk2C;       /* nonzero: the winner keeps the stage */
    u8 unk2E[0x2];
    s32 unk30;       /* offset of a table from the header */
} SceneHeader;

typedef SceneHeader ModelHeader;

/* A loaded model file. */
typedef struct {
    u8 unk0[0x10];
    ModelHeader *header; /* 0x10 */
    s32 unk14;
    u8 (*parts)[4];    /* 0x18: byte 3 set = part kind A */
    u8 unk1C[4];
    u8 *image;         /* 0x20: palette and emblem pixels */
} ModelData;


/* Screen point (libgte DVECTOR). */
typedef struct {
    s16 vx;
    s16 vy;
} DVector;




/* A node of a loaded model hierarchy. */
typedef struct ModelNode {
    u8 unk0[4];
    struct ModelNode *next; /* 0x04 */
    u8 unk8[0x28];
    void *unk30;
} ModelNode;

/* A placed scene object. */
typedef struct {
    u8 unk0[0x44];
    SVector rotation;  /* 0x44 */
} SceneObject;

typedef struct MoveList ModelRecord;

/* A character moved in the menu scene. */
typedef struct Actor {
    Vector pos;          /* 0x00 */
    Vector velocity;     /* 0x10: vy is the vertical speed */
    Vector push;         /* 0x20: horizontal push (vx, vz) */
    u8 unk30[0x10];
    s32 unk40;
    u8 unk44[0x4];
    s32 state;           /* 0x48 */
    u8 anim;             /* 0x4C */
    u8 unk4D;
    u8 unk4E;
    u8 unk4F;
    u8 unk50[0x2];
    u8 unk52;
    u8 unk53;
    s32 angle;           /* 0x54: facing, 4096 = full turn */
    s32 target_angle;    /* 0x58 */
    Node *node;          /* 0x5C: model set node */
    void *object;        /* 0x60 */
    u8 unk64[0xC];
    s32 unk70;
    u8 unk74[0x8];
    s32 unk7C;
    struct MoveSlot *move_slots; /* 0x80: one per combo number */
    u8 *unk84;
    u8 unk88[0x10];
    s32 accel;           /* 0x98 */
    s32 brake;           /* 0x9C */
    u8 unkA0[0x10];
    s32 floor_y;         /* 0xB0 */
    s16 hp;              /* 0xB4 */
    s16 charge;          /* 0xB6: 0x1000 = full */
    u8 unkB8[0x2];
    s16 unkBA;
    s16 max_hp;          /* 0xBC */
    s16 unkBE;           /* 0xBE: charge a special move needs */
    u8 unkC0;
    u8 unkC1;
    u8 level;            /* 0xC2 */
    u8 unkC3;
    u8 unkC4;
    u8 unkC5;
    u8 unkC6[0x4];
    s16 unkCA;
    s16 unkCC;
    s16 unkCE;
    u32 flags;           /* 0xD0: bit 27 = side */
    u32 unkD4;
    struct Actor *opponent; /* 0xD8 */
    u8 unkDC[0xC];
    s32 unkE8;
    u8 unkEC[0x6];
    s16 unkF2;
    u8 unkF4[0xC];
    s32 unk100;
    Trail trails[16];    /* 0x104 */
    s32 unk644;
    u8 unk648[0x4];
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
    u8 unk90C[0x5];
    u8 move_count;       /* 0x911: special moves the opponent may pick */
    u8 parts_b;          /* 0x912 */
    u8 unk913[0x3];
    s16 unk916;
    u8 glow;             /* 0x918: light level, fades by 0x18 a frame */
    u8 unk919[0x13];
    Vector unk92C;       /* 0x92C: with home, spans the actor's extent */
    Vector home;         /* 0x93C */
    Vector core;         /* 0x94C: where shots home in */
    u8 unk95C[0x3C];
    s16 unk998;
    s16 unk99A;
    u8 unk99C[0x2];
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
    Color colour;        /* 0x15D4: effect colour */
    u8 unk15D8[0x10];
    s16 unk15E8;
    s16 unk15EA;
    s16 unk15EC;
    s16 unk15EE;
    u8 unk15F0[0x2];
    s16 unk15F2;
    s16 unk15F4;
    u8 unk15F6[0x6];
    struct Brain *brain; /* 0x15FC: the computer opponent's state */
    struct MoveList *moves; /* 0x1600 */
    PolyFT4 backdrop[2]; /* 0x1604: one per buffer */
    s32 unk1654;
    s32 unk1658;
    u8 unk165C[0x8];
    u8 *sounds;          /* 0x1664: command sound table */
    s16 unk1668;
} Actor;


/* Where a hit effect goes: model part and vertex of one or two points. */
typedef struct {
    u8 unk0;
    u8 type;
    u8 part_a;
    u8 part_b;
    s16 vertex_a;
    s16 vertex_b;
} HitSpec;

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
extern s32 D_80092880;
extern u8 D_80092884;
extern s32 D_800928C8; /* menu mode */
extern u8 D_800928D4;
extern s32 D_80092900;
extern s32 D_80092904;
extern s32 D_80092934;
extern u8 D_8009293C;
extern s32 D_80092948;
extern u8 D_800929BC;
extern u8 D_800925F0;
extern FloorStep D_80091084[8];
extern u16 D_8005948C; /* pad buttons newly pressed */
extern u16 D_800594A4; /* pad buttons repeating */
extern DrTpage D_800929E4[2];
extern s32 D_80092A00;
extern s32 D_80092A10;
extern s32 D_80092A20;
extern u8 D_80099D9A;
extern u8 D_80099D9D;
extern u8 D_80099D9E;
extern u8 D_80099DA1;
extern u8 D_80099DA2;
extern s16 D_80099DA4;
extern u8 D_800928FC;
extern s32 D_80092918;
extern s32 D_80092944;
extern s32 D_80092950;
extern s32 D_80092640;
extern u8 D_80091150;
extern s8 D_80091151;
extern s32 D_80092668;
extern s32 D_8009266C;
extern s32 D_80092670;
extern s32 D_80092674;
extern s32 D_800928AC;
extern s32 D_80092638;
extern s32 D_8009263C;
extern s32 D_80092648;
extern u8 D_80092664;
extern s32 D_80092890;
extern u8 D_800928B4;
extern u8 D_800928F0;
extern u8 D_800928F4;
extern s32 D_8009290C;
extern s32 D_8009294C;
extern u8 D_80091144;
extern u8 D_80091145;
extern Sprt16 D_8009A14C;
extern Sprt16 D_8009A244;
extern Actor *D_80092614;
extern LightRig *D_800910F0; /* the scene's lights */
extern Node *D_80092610;     /* the scene's root node */
extern Vector D_80096FA8;    /* scene origin (last eye position) */
extern s8 D_80092618;       /* odd: show the record text */
extern s32 D_8009261C;
extern s32 D_80092620;
extern s32 D_80092624;
extern s32 D_80092628;
extern s32 D_8009262C;
extern SVector D_80092630;  /* model view angles */
extern ShotKind D_800910F4[];
extern SideHits D_80096FB8[2];
extern s32 D_8009112C;
extern s32 D_80091198[];
extern u8 D_80091178[]; /* pairs: next combo number after each button */
extern u8 D_80099D9B;
extern u8 D_80099D9C;
extern s32 D_8009292C;
extern s32 D_80092654; /* last crossing point x, z */
extern s32 D_80092658;
extern s32 D_80099D88;
extern s32 D_80099D8C;
extern u8 D_80050622; /* resident: result of the last menu battle */
extern s32 D_80092650; /* trail segments added */

/* PsyQ SDK (resident). */
void AddPrim(u32 *ot, void *prim);                    /* AddPrim */
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);           /* GetTPage */
s16 GetClut(s32 x, s32 y);                            /* GetClut */
void SetDrawTPage(DrTpage *p, s32 dfe, s32 dtd, s32 tpage); /* SetDrawTPage */
void LoadImage(Rect *rect, void *data);                 /* LoadImage */
void OpenTIM(u32 *tim);                               /* OpenTIM */
TimImage *ReadTIM(TimImage *image);                   /* ReadTIM */
s32 ratan2(s32 x, s32 z);                            /* ratan2 */
Matrix *CompMatrix(Matrix *m0, Matrix *m1, Matrix *m2);  /* CompMatrix */
s32 rand(void);                                    /* rand */
void func_8004A414(Vector *v, Vector *squares);             /* Square0 */
s32 SquareRoot0(s32 x);                                   /* SquareRoot0 */
void VectorNormalS(Vector *v, SVector *unit);               /* VectorNormalS */
void func_8004901C(SVector *a, SVector *b, s32 pa, s32 pb, SVector *out); /* LoadAverageShort12 */
void SetGeomScreen(s32 h);                                  /* SetGeomScreen */

/* Resident game code. */
void func_80030988(s32 a0, s32 a1, s32 a2, s32 a3);
void func_80032F54(MenuWindow *window, s32 x, s32 y, s32 w, s32 h, s32 a5, s32 a6);
s32 func_80033728(s32 table, s32 index);
void func_800346D4(MenuWindow *window);
void func_80034714(MenuWindow *window, s32 text);
void func_80039C4C(s32 arg);
void func_80039FF8(void);
void func_80036258(s32 port, s32 arg);
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
void func_800732AC(void *dst, void *src, s32 size);
void func_80073064(SVector *dir, SVector *out, s32 scale);
void func_8008859C(Vector *vector, void *out);
s32 func_800886FC(Vector *v);
void func_8007E31C(Vector *from, Vector *to, Color *color);
void func_8008EBD0(Actor *owner, s32 index, Vector *pos, s32 mode);
void func_80073B7C(Actor *actor, s32 part, s32 vertex, Vector *out);
void func_8007C100(Color *color);
void func_80076424(Actor *actor);
s32 func_80077584(Actor *actor, s32 angle, s32 shift, s32 lift);
void func_8007E894(s32 x, s32 y);
void func_80074678(Actor *actor, s32 arg1, s32 arg2);
void func_8007C880(s32 column, Vector *pos, s32 key, s32 size);
u32 func_8007CD14(s32 flag, s32 top, s32 middle, s32 low);
void func_8007CD44(s32 column, Vector *from, Vector *to, s32 key);
s32 func_8007D190(Vector *pos, u32 kind);
s32 func_8007D25C(s32 type);
void func_8007D65C(Vector *from, Vector *to, s32 code);
void func_80079DF0(Actor *actor, Actor *other);
void func_8007191C(s32 scene);
void func_80071DA4(Actor *actor);
void func_8007E24C(void);
s32 func_80082488(Vector *position, s32 arg);
void func_80082458(SVector *out);
void func_800828F8(Vector *position, Vector *step, s32 limit);
void func_80083738(Actor *actor, Actor *other);
void func_80083C0C(s32 arg);
s32 func_80083CD8(void);
s32 func_8008F4F4(Actor *actor, s32 mask);
void func_8008EB4C(s32 id);
void func_8007BB7C(void);
void func_800831C8(void);
void func_8008DCA8(s32 arg);
void func_800720C4(void);
void func_800732CC(void);
void func_8008DC28(void);
void func_80088AF8(void);
void func_8007E954(s32 arg);
void func_8007F834(void);
void func_80078F00(Actor *actor);
OtPair *func_8008A2B8(u16 length);
LightRig *func_8008A3E0(OtPair *layer);
void func_8008A5BC(LightRig *rig);
Node *func_8008C2C0(Node *source);
void func_80080D10(void);
void func_8008976C(s32 a0, s32 a1);
void func_8008BC04(void);

/* Idle scene camera. */
extern s32 D_80092770;
extern s32 D_80092774;
extern u8 D_8009287C;
extern s32 D_800927AC; /* orbit angle */
extern s32 D_8009277C; /* framing heading */
extern s32 D_800927B0; /* orbit speed */
extern s32 D_80092794; /* scene mode */
extern s32 D_80092790;

void func_80083310(s32 arg);
void func_8007A21C(s32 arg);
void func_8007AC3C(void);

/* Scene actor models. */
typedef struct {
    u16 file;
    void *data;
} Resource;

extern void *D_800927B4[2]; /* loaded model of each actor slot */
extern u8 D_80099D9E;

void *func_800891C0(s32 id);
s32 func_800288EC(s32 file);
void func_80083BB4(s32 both);

/* Menu mode exit. */
extern s32 D_800927C4;
extern s32 D_800917F0;
void func_8003852C(s32 arg);
void func_800399D4(s32 arg);
void func_8001996C(s32 arg);
void func_80019ACC(s32 arg); /* resident mode dispatcher */

/* Resident flags. */
extern s32 D_8006F980;
extern u8 D_80091A6C[];

/* Actor setup. */
Node *func_8008B38C(ModelSetFile *file);
void func_8008A168(void);
void func_80084BEC(Actor *actor);
void func_8008E6F8(Actor *actor);
extern u8 D_80091FB0[];

#endif
