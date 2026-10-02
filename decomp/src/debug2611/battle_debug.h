#ifndef DEBUG2611_BATTLE_DEBUG_H
#define DEBUG2611_BATTLE_DEBUG_H

#include "common.h"

/* libgte / libgpu types. */
typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
} POLY_F3;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} POLY_FT4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LINE_F2;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
} TILE_1;

void SetPolyF3(POLY_F3 *p);
void SetPolyFT4(POLY_FT4 *p);
void SetLineF2(LINE_F2 *p);
void SetShadeTex(void *p, s32 tge);
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
u16 GetClut(s32 x, s32 y);
void AddPrim(void *ot, void *p);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
s32 RotTransPers(SVECTOR *v0, s32 *sxy, s32 *p, s32 *flag);
s32 RotTransSV(SVECTOR *v0, SVECTOR *v1, s32 *flag);
void SetGeomOffset(s32 ofx, s32 ofy);
MATRIX *func_8003F738(SVECTOR *rot, MATRIX *m); /* RotMatrix */

/* Resident services. */
void func_8003700C(const char *format, ...); /* debug text print */
void func_80037058(s32 x, s32 y);            /* debug text position */
void func_800379C8(const char *format, ...); /* debug console print */
void func_8003748C(void);                    /* debug text: begin frame */
void func_800374E8(s32 x, s32 y, s32 w, s32 h, s32 a, s32 b, s32 c, s32 d, s32 e, s32 f,
                   s32 g);                   /* debug text: window */
void func_80036E4C(s32 a, s32 b);            /* debug text: flush */
void func_8003278C(s32 mode, s32 top, s32 step, s32 flags); /* heap monitor */
void func_80032E04(char *name);               /* dump memory to a host file */
void func_80023FD8(s32 kind, void *effect, SVECTOR *pos, s32 arg);
void *func_80031BDC(s32 size, s32 mode);     /* allocate */
void func_800320E8(void *block);             /* release */

/* Resident task system: a task node (update) followed by its drawing node;
 * both callbacks receive their node, whose +4 names the task's object. */
typedef struct TaskNode {
    u32 unk0;
    void *object;
    void (*update)(struct TaskNode *node);
    void (*destroy)(struct TaskNode *node);
    u32 unk10;
    u32 unk14;
    struct TaskNode *next;
} TaskNode;

void *func_8001D1D8(s32 size, void *owner, void (*update)(TaskNode *),
                    void (*draw)(TaskNode *), void (*destroy)(TaskNode *)); /* create a task */

extern s32 D_80059188; /* tasks running */
extern u8 *D_80059534; /* primitive buffer end */
extern u8 *D_80059580; /* next free primitive */
extern void *D_8005956C; /* current ordering table */
extern u8 D_8005959C;  /* battle debug page */

/* A running sound sequence (only the link the heap list follows). */
typedef struct DebugSequence {
    u8 unk0[0x2C];
    struct DebugSequence *next; /* +2c */
} DebugSequence;
extern DebugSequence *D_80059558;

/* Battle overlay work area (800c3eb0); only the members the tool reads. */
typedef struct {
    u8 unk0[8];
    u8 enemy;       /* +08: nonzero for an enemy placement */
    u8 unk9[0x13];
} Placement;        /* 0x1c */

typedef struct {
    Placement placements[11];
    u8 unk134[0x8C5A - 0x134];
    u16 held;         /* +8c5a: pad buttons held */
    u8 unk8C5C[2];
    u16 pressed;      /* +8c5e: pad buttons pressed */
    u8 unk8C60[0x8DAC - 0x8C60];
    s32 frame_rate;   /* +8dac: frames per update - 1 */
} BattleWork;
extern BattleWork D_800C3EB0;

/* Battle combatant records (0x170 each: party 0..2, enemies 3..10). */
typedef struct {
    u8 unk0[0x4C];
    u16 hp;          /* +4c */
    u8 unk4E[0x42];
    u16 work[7];     /* +90: script work values */
    u8 unk9E[0x66];
    s32 enemy_hp;    /* +104 */
    u8 unk108[0x68];
} Combatant;         /* 0x170 */
extern Combatant D_800CCCE8[11];

/* Game data character records (0xa4 each). */
typedef struct {
    u8 unk0[0x90];
    u16 work[7];     /* +90 */
    u8 unk9E[6];
} Character;         /* 0xa4 */
extern Character D_8006D8A0[11];

/* Battle command records (0x48 each); the page names their fields. */
typedef struct {
    u16 target;      /* +00 Tg */
    u8 unk2[0x22];
    u16 sub;         /* +24 Sb */
    u8 unk26[0xB];
    u8 anim;         /* +31 An */
    u8 unk32[0x16];
} Command;           /* 0x48 */
extern Command D_800C3FFE[32];

/* Effect records (8 bytes each). */
typedef struct {
    u8 code;         /* Cd */
    u8 cls;          /* Cl */
    u8 anim;         /* An */
    u8 param[3];     /* P1..P3 */
    u16 target;      /* Tg */
} Effect;
extern Effect D_800D2E5C[23];

/* Enemy AI flags (0x40 per enemy slot 3..10). */
typedef struct {
    u8 unk0[0x10];
    s32 lflag[4];    /* +10 */
    u16 hflag[8];    /* +20 */
    u8 bflag[16];    /* +30 */
} EnemyFlags;
extern EnemyFlags D_800D3400[8];

/* Battle turn state; only the acting slot. */
typedef struct {
    u8 unk0[0x2D3];
    u8 actor;        /* +2d3 */
} TurnState;
extern TurnState *D_800C3EAC;

/* A 16.16 fixed-point coordinate. */
typedef union {
    s32 raw;
    struct {
        u16 frac;
        s16 whole;
    } part;
} Fixed;

typedef struct {
    s16 vx, vy, vz;
} Short3;

typedef struct {
    u8 unk0[0x14];
    s32 polys;         /* +14 */
} DebugShape;

/* A battle model; the members the actor tool edits. */
typedef struct {
    Short3 rot;        /* +00 */
    Short3 scale;      /* +06 */
    u8 unkC[0x28];
    DebugShape *shape; /* +34 */
    u8 unk38[0xC];
    Short3 light_angle; /* +44 */
    u8 unk4A[2];
    Short3 light_color; /* +4c */
} DebugModel;

/* A battle actor (the one selected for the actor tool). */
typedef struct {
    Fixed pos[3];      /* +00 */
    s32 vel[3];        /* +0c */
    u8 unk18[4];
    s32 gravity;       /* +1c */
    DebugModel *model; /* +20 */
    u8 unk24[0x10];
    u16 shape;         /* +34 */
    u8 unk36[6];
    s32 flags;         /* +3c */
    u32 state;         /* +40 */
} DebugActor;
extern DebugActor *D_800C3568;

extern u8 D_8006BE10[]; /* marker effect */
void func_800BC2F0(s32 mode); /* battle display mode */

/* Battle camera (800d309c). */
typedef struct {
    u8 unk0[0x14];
    SVECTOR rot;      /* +14 */
    s32 range;        /* +1c */
    MATRIX matrix;    /* +20 */
    s32 cpu;          /* +40: CPU time last frame */
    s32 gpu;          /* +44: GPU time last frame */
} BattleCamera;
extern BattleCamera D_800D309C;
extern SVECTOR D_800D30B0; /* the camera's rot, addressed on its own by 80280960 */
extern SVECTOR D_800D3354; /* camera position */
extern SVECTOR D_800D335C; /* look-at point */

/* The CPU/GPU load meter task: two needles over a dial, eased averages and
 * peaks held for 80 frames. */
typedef struct {
    TaskNode task;  /* +00 */
    TaskNode draw;  /* +1c */
    s32 cpu;        /* +38: needle angle */
    s32 gpu;        /* +3c */
    s32 cpu_avg;    /* +40: eased, x16 */
    s32 gpu_avg;    /* +44 */
    s32 cpu_peak;   /* +48 */
    s32 gpu_peak;   /* +4c */
    s32 cpu_hold;   /* +50: frames the peak is held */
    s32 gpu_hold;   /* +54 */
} LoadMeter;        /* 0x58 */

/* Tool statics (data). */
extern s32 D_80282034; /* heap monitor shown */
extern s32 D_80282038; /* performance counters shown */
extern s32 D_80282040; /* camera tool shown */
extern s32 D_80282044; /* geometry offset toggle */
extern s32 D_8028205C; /* heap monitor flags */
extern s32 D_80282060; /* heap monitor first block */
extern s32 D_80282064; /* heap monitor scroll repeat delay */
extern s32 D_80282068; /* heap monitor step */
extern SVECTOR D_8028206C[2][3]; /* meter needles */
extern u8 D_802820BB;  /* actor tool shift */
extern u8 D_802820BC;  /* actor tool control mode */
extern char *D_802820C0[]; /* control mode names */
extern s32 D_802820D4; /* memory dump count */
extern char D_802820D8[]; /* memory dump file name */
extern s32 D_802820EC; /* frame counter */

void func_80280844(s32 buttons);
void func_80280960(s32 buttons);
void func_8028103C(void);
void func_802810C4(void);
void func_80281330(TaskNode *node);
void func_802813F4(SVECTOR *v, u8 r, u8 g, u8 b);
void func_802814F8(u8 r, u8 g, u8 b);
void func_802815E8(s16 length, u8 r, u8 g, u8 b);
void func_802816AC(TaskNode *node);
void func_8028191C(void);
void func_80281980(void);
void func_80281F98(void);

#endif
