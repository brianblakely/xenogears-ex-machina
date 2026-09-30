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

/* Screen point (libgte DVECTOR). */
typedef struct {
    s16 vx;
    s16 vy;
} DVector;

/* libgte-layout short vector. */
typedef struct {
    s16 vx;
    s16 vy;
    s16 vz;
    s16 pad;
} SVector;

/* Per-model table entry (D_80092874, 32 bytes). */
typedef struct {
    u8 unk0[0xA];
    u8 parts[14];      /* 0x0A: nonzero = part present */
    u8 unk18[8];
} ModelRecord;

/* Header of a loaded model file. */
typedef struct {
    u8 unk0[0xE];
    u8 unkE;
    u8 unkF;
    u8 unk10[3];
    u8 unk13[0x1D];
    s32 unk30;         /* offset of a table from the header */
} ModelHeader;

/* A loaded model file. */
typedef struct {
    u8 unk0[0x10];
    ModelHeader *header; /* 0x10 */
    s32 unk14;
    u8 (*parts)[4];    /* 0x18: byte 3 set = part kind A */
    u8 unk1C[4];
    u8 *image;         /* 0x20: palette and emblem pixels */
} ModelData;

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

/* A character moved in the menu scene. */
typedef struct Actor {
    Vector pos;          /* 0x00 */
    u8 unk10[0x38];
    s32 state;           /* 0x48 */
    u8 unk4C[0x8];
    s32 angle;           /* 0x54: facing, 4096 = full turn */
    s32 target_angle;    /* 0x58 */
    ModelNode *model;    /* 0x5C */
    void *object;        /* 0x60 */
    u8 unk64[0x18];
    s32 unk7C;
    u8 (*parts)[4];      /* 0x80 */
    u8 unk84[0x4A];
    s16 unkCE;
    u32 flags;           /* 0xD0: bit 27 = side */
    u8 unkD4[0x4];
    struct Actor *opponent; /* 0xD8 */
    u8 unkDC[0x16];
    s16 unkF2;
    u8 unkF4[0x808];
    ModelHeader *header; /* 0x8FC */
    u8 *unk900;
    u8 *unk904;
    u8 unk908;
    u8 model_id;         /* 0x909 */
    u8 kind;             /* 0x90A: bits 0-2 */
    u8 unk90B[6];
    u8 parts_a;          /* 0x911 */
    u8 parts_b;          /* 0x912 */
    u8 unk913[0xCC1];
    u8 unk15D4[3];
    u8 unk15D7[0x29];
    ModelRecord *record; /* 0x1600 */
    PolyFT4 backdrop[2]; /* 0x1604: one per buffer */
} Actor;

/* Menu camera: eye position (D_8009867C) and look-at point (D_8009871C). */
extern Vector D_8009867C;
extern Vector D_8009871C;
extern s32 D_800925F4; /* vertical camera lift of the current view */
extern Actor D_80097010;
extern Actor D_8009872C;
extern Vector D_80099078;
extern u8 D_80092954[];

s32 func_8003F8B0(s32 angle); /* sine, 4096 = 1.0 */
s32 func_8003F8CC(s32 angle); /* cosine, 4096 = 1.0 */
void func_800346D4(void *arg);
void func_80083C0C(s32 arg);
void func_80083738(Actor *actor, Actor *other);
void func_800828F8(Vector *position, Vector *step, s32 limit);
s32 func_80082488(Vector *position, s32 arg);
s32 func_8004B32C(s32 x, s32 z); /* angle of a direction, 4096 = full turn */
void func_8007E24C(void);

extern s32 D_800925F8;
extern s32 D_800925FC;
extern s32 D_80092934;

/* Idle scene camera. */
extern s32 D_80092770;
extern s32 D_80092774;
extern u8 D_8009287C;
extern s32 D_8009290C;
extern s32 D_800927AC; /* orbit angle */
extern s32 D_800927B0; /* orbit speed */
extern s32 D_80092794; /* scene mode */
extern s32 D_80092790;
extern s32 D_8009294C;
extern s32 D_80092944; /* elapsed frames */

s32 func_8003FA38(void); /* rand */
s32 func_8003FBF8(char *out, const char *format, ...); /* sprintf */
void func_80083310(s32 arg);
void func_8007A21C(s32 arg);
void func_8007AC3C(void);
void func_800725B0(Actor *actor);

/* Scene actor models. */
typedef struct {
    u16 file;
    void *data;
} Resource;

extern void *D_800927B4[2]; /* loaded model of each actor slot */
extern u8 D_80092920;
extern u8 D_80099D9D;
extern u8 D_80099D9E;
extern s32 D_800928C8; /* menu screen state */
extern s32 D_80059488; /* resident pad buttons */

void func_800320E8(void *block); /* free */
void func_80028470(s32 arg0, s32 arg1);
void func_80028A60(s32 arg);
void *func_800891C0(s32 id);
s32 func_800288EC(s32 file);
void *func_80031BDC(s32 size, s32 arg); /* allocate */
void func_80083BB4(s32 both);

/* Menu mode exit. */
extern s32 D_800927C4;
extern s32 D_800917F0;
extern u8 D_8005061C;
void func_8003852C(s32 arg);
void func_80039C4C(s32 arg);
void func_800399D4(s32 arg);
void func_80088A40(void);
void func_8001996C(s32 arg);
void func_8004B54C(s32 arg);
void func_80019ACC(s32 arg); /* resident mode dispatcher */

/* Resident flags. */
extern u8 D_8006F978[];
extern s32 D_8006F980;
extern u8 D_800927EC;
extern s32 D_80092888;
extern void **D_800928EC;
extern u8 D_80091A6C[];

/* Actor setup. */
extern ModelRecord *D_80092874;
void func_8008A140(s32 x, s32 y, s32 arg2, s32 arg3);
void func_8008AF6C(ModelData *data);
void *func_8008B38C(ModelData *data);
void *func_80089C54(void);
void func_80089C88(void *object, void *model);
void func_8008A168(void);
void func_80084BEC(Actor *actor);
void func_8008E6F8(Actor *actor);
void *func_80089FC4(void);
void func_80089E2C(void *object, void *part);
void func_8008A184(void *part, void *model);
extern u8 D_80091FB0[];
void func_80032C18(void *block, s32 arg);

#endif
