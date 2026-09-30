#ifndef WORLDMAP_H
#define WORLDMAP_H

#include "common.h"

#define ABS(x) ((x) < 0 ? -(x) : (x))

/* Resident services used by the world map. */
s32 func_800288EC(s32 file);                              /* file size, rounded to words */
void *func_80031BDC(s32 size, s32 mode);                  /* allocate a block */
s32 func_800295D8(s32 file, void *dest, s32 offset, s32 mode); /* read one file */

/* One entry of a disc-read list; a zero file ends the list. */
typedef struct {
    s16 file;
    void *dest;
} FileLoad;

s32 func_80029AFC(FileLoad *list, s32 offset, s32 mode);  /* read a file list */

/* Per-area file set: the base disc file number and three area parameters. */
typedef struct {
    s16 file;
    s16 param2;
    s16 param4;
    s16 param6;
} WorldmapArea;

extern u16 D_8009B564[];          /* area thresholds, indexed from 1 */
extern WorldmapArea D_8009B57C[]; /* area file sets */

/* Loaded area files and their buffers. */
extern s32 D_8009D3C4, D_8009C174, D_8009C17C, D_8009D3D0, D_8009CC98;
extern s32 D_8009D800, D_8009D3C8, D_8009BCD8, D_8009BCC8, D_8009BD08;
extern s32 D_8009D2B4, D_8009D160, D_8009D7CC, D_8009C610;
extern void *D_8009C59C, *D_8009BD20, *D_8009C180, *D_8009D528;
extern void *D_8005945C;
typedef struct {
    u8 pad0[0x14];
    u16 id;
} SoundBank;

extern SoundBank *D_8006259C; /* area sound bank */
extern void *D_8009BC38[2], *D_8009BCB0[2]; /* work and packet buffers, per display buffer */
extern FileLoad D_8009D3F8[]; /* shared read list */

/* Party: three character ids (0xFF empty) and per-character records. */
typedef struct {
    u8 gear; /* piloted gear, 0xFF none */
    u8 pad1[0xA3];
} CharacterRecord;

extern u8 D_8006F368[3];
extern CharacterRecord D_8006D940[];
extern void *D_8009CD34[3]; /* character model buffers */
extern void *D_8009BDF8[3]; /* gear model buffers */
extern s32 D_8009C170;      /* loaded party members */
extern s32 D_8004F304;
extern void *D_8009C88C, *D_8009C884, *D_8009C888, *D_8009C614;

/* Gouraud quad packet (PsyQ POLY_G4 layout); colour words carry the code
 * in their top byte. */
typedef struct {
    u32 tag;
    u32 rgb0;
    s32 xy0;
    u32 rgb1;
    s32 xy1;
    u32 rgb2;
    s32 xy2;
    u32 rgb3;
    s32 xy3;
} PolyG4;

#define setPolyG4(p) (((u8 *)(p))[3] = 8, ((u8 *)(p))[7] = 0x38)

extern PolyG4 D_8009D194[4][2]; /* sky gradient bands, per buffer */

/* Resident world-map return state. */
typedef struct {
    u16 x;       /* 8006ee54 */
    u16 z;
    u16 heading;
    u16 unk5A;
    u16 unk5C;
    u16 unk5E;
    s16 unk60;   /* saved vehicle position */
    s16 unk62;
    s16 unk64;
    u16 vehicle_heading; /* 8006ee66 */
    u16 flags;   /* 8006ee68: 0x4000 vehicle, 0x2000 restore, low bits kind */
    s16 unk6A;
    u16 unk6C;
    u16 unk6E;
    u16 unk70;
    u16 unk72;
    u16 unk74;
    u16 unk76;   /* 8006ee76 */
} WorldmapReturn;

extern WorldmapReturn D_8006EE54;
extern u8 D_8006F8E5, D_8006F8E6, D_8006F8E7;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

/* Camera: its target and orientation. */
typedef struct {
    VECTOR target;
} Camera;

extern Camera D_8009BE28;

/* Named arrival point: position in world units and its id; -1 ends a list. */
typedef struct {
    s16 x;
    s16 id;
    s16 z;
    s16 pad;
} WorldmapSpot;

extern s32 D_8009BE10; /* movement mode */
extern VECTOR D_8009C5AC; /* player position (20.12) */
extern s32 D_8009C584;  /* player heading */
extern WorldmapSpot *D_8009D3F4;

void func_8008DFF4(VECTOR *position);
void func_800848B4(s32 parent, s32 child);

/* PsyQ libgpu environments. */
typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd, dfe, isbg, r0, g0, b0;
    u32 dr_env[16];
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter, isrgb24, pad0, pad1;
} DISPENV;

typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u32 unk70;
    u32 unk74;
} DisplayBuffer;

extern DisplayBuffer D_8009BBC8[2];
extern s32 D_8009BCDC;
extern u8 D_8009BB48[3]; /* background colour */

void ResetGraph(s32 mode);
void SetGeomScreen(s32 value);
DRAWENV *SetDefDrawEnv(DRAWENV *env, s32 x, s32 y, s32 w, s32 h);
DISPENV *SetDefDispEnv(DISPENV *env, s32 x, s32 y, s32 w, s32 h);
void func_8002C6E0(s32 r, s32 g, s32 b);
void SetBackColor(s32 r, s32 g, s32 b);
void func_8004A10C(s32 r, s32 g, s32 b);
void SetFogNearFar(s32 a, s32 b, s32 c);

/* Actor slots (0x80 bytes each). */
typedef struct {
    s16 command;     /* 0x00: pending command */
    s16 command_arg;
    s16 unk4;
    s16 unk6;
    s32 unk8;
    s32 unkC;
    s32 unk10;
    s32 unk14;
    s32 kind;        /* 0x18 */
    s32 update;      /* 0x1C: nonzero while the slot is in use */
    s16 state;    /* 0x20 */
    s16 wait;     /* 0x22: script wait counter */
    s16 unk24;
    s16 unk26;
    VECTOR position; /* 0x28 */
    VECTOR motion;   /* 0x38 */
    s16 heading;  /* 0x48 */
    s16 turn;     /* 0x4A: turn step */
    s32 handle;   /* 0x4C */
    union {
        s16 *script; /* script position */
        u16 value;   /* low half of step */
        s32 step;
    } u;          /* 0x50 */
    s32 unk54;
    s32 unk58;
    s32 unk5C;
    s32 unk60;
    s32 unk64;
    s32 unk68;
    s32 unk6C;
    s32 unk70;
    s32 unk74;
    s32 unk78;
    s32 unk7C;
} WorldmapActor;

/* Script opcode handler: returns the halfwords to advance, 0 to yield. */
typedef s32 (*ScriptOp)(WorldmapActor *actor, s32 arg1, s32 arg2, s32 arg3);

extern ScriptOp D_8009A3C0[];

extern WorldmapActor *D_8009BE24;
extern void *D_80062528;
extern u16 D_8006F954[]; /* resident flag words */

void func_800320E8(void *block); /* free a block */
void *func_80032E88(void *block, s32 mode);
void func_800230A8(s32 handle);
void func_8003A89C(void *a, s32 b, s32 c);
void func_80039FF8(void);
void func_8003852C(void *data);
void func_80024FB8(void);
void func_8007474C(void);
void func_80074F04(void);
void func_800750DC(void);
void func_80075460(void);
void func_80084818(void);
void func_80086124(void);
void func_80086568(void);
void func_800866C8(void);
void func_80088FF4(void);
void func_80089128(void);
void func_80092DD0(void);
void func_800931B0(void);
void func_800960BC(void);
void func_800976A0(void);
void func_80097D64(void);

/* Area file: section offsets from its start. */
typedef struct {
    s32 unk0;
    s32 spots;   /* spot block */
    s32 off8, offC, off10, off14, off18, off1C, off20, off24;
    s32 unk28;
    s32 models[16];
} AreaHeader;

/* Spot block: arrival points and a table of four sections. */
typedef struct {
    s32 spots;
    s32 table;
} SpotHeader;

extern void *D_8009D308, *D_8009CD48, *D_8009BD30, *D_8009D784;
extern s32 *D_8009D7C8, *D_8009D77C; /* texture animation sections: count, offsets */
extern void *D_8009D73C[16];
extern s32 *D_8009BD00;

/* Texture animations: each slot uploads one image of a frame sequence. */
typedef struct {
    s16 image;    /* image index in the animation's data */
    s16 duration; /* frames; negative ends the sequence */
} TexAnimFrame;

typedef struct {
    RECT rect;
    s32 unk8;
    TexAnimFrame *frames;
} TexAnimSlot;

typedef struct {
    u8 *images;
    TexAnimSlot *slot;
    s16 frame;
    s16 timer;
} TexAnim;

extern TexAnimSlot D_8009A1E8[], D_8009A250[];
extern TexAnim *D_8009D780, *D_8009D7D0;
extern s32 D_8009CC9C, D_8009CD64;

void LoadImage(RECT *rect, void *data); /* upload to VRAM */

extern void *D_8009BE14, *D_8009BE18;
extern WorldmapSpot *D_8009D30C; /* ring of 16 recent positions */
extern s32 D_8009BE38;

/* PsyQ libgte types. */
typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

/* Scratchpad work area of the sky renderer. */
typedef struct {
    SVECTOR angle;
    MATRIX view;
    MATRIX rotation;
    s32 p;
    s32 flag;
} SkyScratch;

#define SKY_SCRATCH ((SkyScratch *)0x1F800000)

#define addPrim(ot, p) \
    (*(u32 *)(p) = (*(u32 *)(p) & 0xFF000000) | (*(u32 *)(ot) & 0xFFFFFF), \
     *(u32 *)(ot) = (*(u32 *)(ot) & 0xFF000000) | ((u32)(p) & 0xFFFFFF))

extern SVECTOR D_8009A280[4][4]; /* sky band corners */
extern MATRIX D_8009C808;       /* camera matrix */
extern s32 D_8009D7F0;          /* current buffer */
extern s32 D_80050100;          /* ordering-table depth shift */

void func_8004A92C(SVECTOR *angle, MATRIX *m); /* RotMatrix */
MATRIX *CompMatrix(MATRIX *a, MATRIX *b, MATRIX *out); /* MulMatrix0 */
void SetRotMatrix(MATRIX *m); /* SetRotMatrix */
void SetTransMatrix(MATRIX *m); /* SetTransMatrix */
s32 RotTransPers4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, s32 *sxy0, s32 *sxy1,
                  s32 *sxy2, s32 *sxy3, s32 *p, s32 *flag); /* RotAverage4 */

/* Textured quad packet (PsyQ POLY_FT4 layout). */
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
} PolyFT4;

#define setlen(p, n) (((u8 *)(p))[3] = (n))
#define setcode(p, c) (((u8 *)(p))[7] = (c))
#define setPolyFT4(p) (setlen(p, 9), setcode(p, 0x2C))
#define setRGB0(p, r, g, b) ((p)->r0 = (r), (p)->g0 = (g), (p)->b0 = (b))
#define setSemiTrans(p, abe) \
    ((abe) ? (((u8 *)(p))[7] |= 2) : (((u8 *)(p))[7] &= ~2))

typedef struct {
    u32 tag;
    u32 code[2];
} DR_TWIN;

extern PolyFT4 D_8009C744[4];
extern DR_TWIN D_8009D3D8[2];

u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y); /* GetTPage */
u16 GetClut(s32 x, s32 y);                  /* GetClut */
void SetSemiTrans(void *p, s32 abe);             /* SetSemiTrans */
void SetTexWindow(DR_TWIN *p, RECT *tw);         /* SetTexWindow */

extern s16 D_8009C854[16];
extern s32 D_8009D64C, D_8009BE40, D_8009BCC4, D_8009D80C;

extern s32 D_8009D554, D_8009CCA4, D_8009D3CC;
extern u8 D_80062648[];
#define SCRIPT_VECTOR ((SVECTOR *)0x1F8000A0) /* scratchpad script vector */

void DrawSync(s32 mode);
void VSync(s32 mode);
void EnterCriticalSection(void);
void FlushCache(void);
void ExitCriticalSection(void);
void func_80039CC4(void);
void func_800399D4(void *seq);
void *func_80039850(void *header);
void func_80039A80(void *seq, s32 volume, s32 c);
s32 func_80097770(s32 index, s32 arg);
void func_80089160(s32 effect, SVECTOR *position, SVECTOR *angle);
void func_800894C8(s32 a);
void func_80089514(s32 a);
void func_80039E60(s32 sound);
void func_8003A3B8(s32 sound, s32 b, s32 c);

extern SVECTOR D_8009BD38; /* camera angle */
extern s32 D_8009D3F0;    /* camera distance */
extern s32 D_8009BE0C;

extern s16 D_8006F94E; /* next scene */
extern u16 D_8006F950; /* heading carried into the next scene */
extern s32 D_8009BBC4;

/* Scene object (0x54 bytes): a transformed sprite set linked to a parent. */
typedef struct {
    u8 pad0[4];
    u16 count;
    u16 pad6;
    u8 pad8[0x2C];
    s32 size; /* 0x34: primitive bytes */
} SpriteDef;

typedef struct SceneObject {
    s16 visible;
    s16 unk2;
    u16 flags;                  /* 0x04: 1 solid */
    s16 unk6;
    VECTOR position;            /* 0x08 */
    SVECTOR angle;              /* 0x18 */
    MATRIX matrix;              /* 0x20 */
    SpriteDef *def;             /* 0x40 */
    s32 unk44;
    void *prims;                /* 0x48 */
    void *prims2;               /* 0x4C: second buffer's copy */
    struct SceneObject *parent; /* 0x50 */
} SceneObject;

extern SceneObject *D_8009C620; /* scene objects */
extern u16 D_8009A450;
extern u16 D_8009A46C[];
extern u16 D_8009A4D8[];
extern u16 D_8009A4E8[];
extern u16 D_8009A698[];
extern u16 D_8009A6AC[];
extern u16 D_8009A6C0[];
extern u16 D_8009A70C[];
extern s16 D_8009A758[], D_8009AC60[]; /* scripts */

/* Flat-textured triangle packet (PsyQ POLY_FT3 layout). */
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
} PolyFT3;

void func_80083108(SceneObject *object, PolyFT3 *prims, s32 count, s32 mode);

void memcpy(void *dest, void *src, s32 size); /* copy memory */

extern void *D_8009D7E8[2], *D_8009D7F8[2]; /* quad buffers, per display buffer */
extern u16 D_8009B64C[][2]; /* per area: two scene objects */
extern u16 D_8009B674[];    /* per area: scene object */

void func_80087904(SceneObject *object, PolyFT4 *quads, s32 count, s32 abr);

extern s16 D_8009AFDC[]; /* scene objects to show; -1 ends */
extern u16 D_8006EF64[]; /* scene id (first of the scene words) */
extern void *D_8009BE1C[2]; /* effect quads, per display buffer */

void func_8008BFD4(s32 index, VECTOR *position, s32 x, s32 z);

/* Queued actor placement (0x18 bytes, ring of 32). */
typedef struct {
    s16 actor;
    s16 pad2;
    s32 px, py, pz;
    s32 z;
    s16 x;
    s16 pad16;
} PlaceRequest;

extern PlaceRequest D_8009BE6C[32];
extern s16 D_8009BD04;

s32 func_8008C364(WorldmapActor *actor, s32 kind);

typedef struct {
    u8 data[0xC];
    u16 count;
    u16 pad;
} PathTable;

extern PathTable D_8009B6C4[2];
extern PathTable *D_8009D7D8;
extern s16 D_8009BD24;
extern u8 D_8009D738, D_8009BD60;

extern u16 D_8009CD4C; /* pad buttons held */
extern s32 D_8009CEC0, D_8009C7E8, D_8009BD34;
extern s16 D_8009BAC8[]; /* 8 columns per row */

void func_800346D4(void *object);
s32 func_80093E8C(VECTOR *position); /* terrain attribute at a position */
u8 *func_80093660(s32 x, s32 z);   /* terrain cell at a position */

extern void *D_8009BE08, *D_8009D3C0, *D_8009D7D4; /* effect command buffers */
extern s32 D_8009D808, D_8009BE44, D_8009BCB8;

typedef struct {
    s32 a, b, c;
} EffectCommand3;

typedef struct {
    s32 a, b, c, d;
} EffectCommand4;

s32 SquareRoot0(s32 value);                  /* SquareRoot0 */
s32 ratan2(s32 y, s32 x);               /* ratan2 */
s32 func_8003F8B0(s32 angle);                  /* rsin */
s32 func_8003F8CC(s32 angle);                  /* rcos */
s32 func_8002C3D8(void);
s32 func_800967E4(void); /* stream step: func_800968E0 status */
s32 func_80096668(void);

extern void *D_8009C184[0x100]; /* terrain block buffers */

void func_800976C8(void);

extern u16 D_8009B6B0[], D_8009B69C[]; /* per area: scene object */

/* Timed sequence: state per step and the step durations. */
typedef struct {
    s16 *states;
    u16 *durations;
} Sequence;

extern Sequence D_8009A65C[];
extern s32 D_8009D3D4;
extern Camera D_8009D55C; /* saved camera */

void func_80097BC0(VECTOR *position);

/* Terrain block: 16x16 cell attributes at 0x510. */
typedef struct {
    u8 pad0[0x510];
    s16 attributes[256];
} TerrainBlock;

/* Terrain texture entry (8 bytes): offset of its data within the section. */
typedef struct {
    u8 *data;
    s32 unk4;
} TerrainTexture;

extern u16 D_8009D478[16]; /* terrain CLUTs */
extern TerrainTexture *D_8009C7EC;

/* Resident text window. */
typedef struct {
    u8 pad0[0x10];
    u16 flags;      /* 0x10 */
    u8 pad12[0x56];
    s8 unk68;       /* 0x68 */
} TextWindow;

extern TextWindow D_8009D498;
extern TextWindow D_8009BD64; /* destination name window */

s32 func_80024524(void *model, s32 a, s32 b, s32 c, s32 d, s32 e);
void func_800245D8(s32 handle, s32 mode);
void func_80022000(s32 handle, s32 scale);
void func_80032F54(void *window, s32 x, s32 y, s32 w, s32 h, s32 a, s32 b);
void func_80034614(void *window);

/* Area object (0x54 bytes, 512 of them, eight per group): a particle
 * emitter. */
typedef struct {
    s32 unk0;         /* emit timer reload */
    s32 unk4;         /* packed emit timer: low delay, high repeats */
    s16 unk8;
    s16 unkA;         /* live particles */
    s16 unkC;
    s16 unkE;
    s16 unk10;
    s16 unk12;
    SVECTOR position; /* 0x14 */
    SVECTOR angle;    /* 0x1C */
    SVECTOR unk24;
    SVECTOR direction; /* 0x2C */
    u8 pad34[0x1B];
    u8 flags;         /* 0x4F: 0x80 active */
    u8 pad50[4];
} AreaObject;

/* Short vectors handled as a word (vx, vy) plus vz. */
#define SVECTOR_ZERO(v) (*(s32 *)&(v)->vx = 0, (v)->vz = 0)
#define SVECTOR_COPY(d, s) (*(s32 *)&(d)->vx = *(s32 *)&(s)->vx, (d)->vz = (s)->vz)

typedef struct {
    u8 r, g, b, cd;
} CVECTOR;

/* Particle life word: low half frames left, high half nonzero while live. */
#define EFFECT_COUNT(slot) (((s16 *)&(slot)->timer)[0])
#define EFFECT_ENABLED(slot) (((s16 *)&(slot)->timer)[1])

/* Effect slot (0x4C bytes, 256 of them): one particle. */
typedef struct {
    s16 id;            /* emitting area object */
    s16 unk2;
    s32 timer;         /* 0x04: see EFFECT_COUNT, EFFECT_ENABLED */
    VECTOR position;   /* 0x08 */
    VECTOR velocity;   /* 0x18 */
    VECTOR accel;      /* 0x28 */
    s16 rot[2];        /* 0x38 */
    s16 spin[2];       /* 0x3C */
    s32 colour;        /* 0x40: packed r, g, b and the primitive code */
    s32 fade;          /* 0x44: packed signed r, g, b steps */
    u8 pad48[4];
} EffectSlot;

/* Drifting position (0x10 bytes) and its velocity (8 bytes). */
typedef struct {
    s32 x;
    s32 unk4;
    s32 z;
    s32 unkC;
} Drift;

typedef struct {
    s16 dx;
    s16 unk2;
    s16 dz;
    s16 unk6;
} DriftVelocity;

extern AreaObject *D_8009BCC0;
extern EffectSlot *D_8009BDF4;
extern Drift *D_8009D150;
extern DriftVelocity *D_8009CEB4;
extern VECTOR D_8009BB4C, D_8009BB5C, D_8009BB6C, D_8009BB7C, D_8009BB8C, D_8009BB9C;
extern VECTOR D_8009C7F0, D_8009C828, D_8009C844, D_8009C874;
extern s16 D_8009AFA0[]; /* object link pairs; -1 ends */
extern u16 D_8009B688[];

void OuterProduct0(VECTOR *a, VECTOR *b, VECTOR *out); /* OuterProduct0 */

extern s16 D_8009D7E0; /* scene object count */
extern u16 D_8009D52C;

void func_8007A06C(SceneObject *object, PolyFT4 *quads, s32 count);
void func_8002CBBC(void *def);
s32 func_80095414(VECTOR *position, VECTOR *direction, VECTOR *hit, s32 range, s32 mode);

/* Scratchpad work area of the actor updaters. */
typedef struct {
    VECTOR work;      /* 0x00 */
    u8 pad10[0x90];
    SVECTOR position; /* 0xA0 */
    SVECTOR angle;    /* 0xA8 */
} ActorScratch;

extern MATRIX D_8009A180; /* identity matrix */
extern MATRIX D_8009D534;
extern s32 D_8009C618;

s32 func_80093A5C(s32 x, s32 z);  /* terrain height */
s16 func_80093F18(VECTOR *position);
void func_8003F738(SVECTOR *angle, MATRIX *m);
void func_80097DC0(void);

extern u16 D_8009A68C[];
extern void *D_8009D788[16]; /* submitted frame lists */

s16 func_80084DB8(s32 probe, s16 object);
void func_8007EBBC(SceneObject *object, PolyFT4 *quads, s32 count, s32 abr);
void func_800963E4(s32 *list);

extern s32 D_8009B224[2], D_8009B22C[2], D_8009B234[2], D_8009B23C[2];
extern void *D_8009C624[16]; /* submitted four-word lists */

/* Terrain slope plane per type (16 bytes). */
typedef struct {
    s32 nx;
    s32 unk4;
    s32 nz;
    s32 unkC;
} SlopeNormal;

extern SlopeNormal D_8009B264[16];

void func_800964B0(s32 *list);

/* Path region (16 bytes); an id of -1 ends a list. */
typedef struct {
    s16 x, z, w, h;
    s16 id;
    s16 padA;
    s16 link; /* 0x0C: path or destination id */
    s16 kind; /* 0x0E: 4 destination */
} PathRegion;

extern s16 D_8009B18C[4], D_8009B194[4], D_8009B19C[4], D_8009B1A4[4];
extern s32 D_8009CD44, D_8009BD2C;

/* Frame state. */
typedef struct {
    u8 pad0[0x70];
    u32 *ot; /* ordering table */
    s32 unk74;
} WorldmapView;

extern s32 D_8009D144;
extern s16 D_8009D558;
extern s32 D_8009C5BC;
extern WorldmapView *D_8009BE3C;
extern u8 D_8009BBB4[], D_8009BD40[];


void func_80073B04(void);
void func_800737EC(void);
void func_800740B8(void);
void func_800747DC(void);
void func_800848F4(void);
void func_80085CDC(void);
void func_8008615C(void);
void func_80086798(void);
void func_80089748(void);
void func_80089C78(void);
void func_80096130(void);
void func_80097244(void *);
void func_80097440(void *);
void func_800980D4(void *);
void func_800981C8(Camera *);
void func_800983A0(Camera *);
void func_80098CC0(void);
void func_8009932C(u32 *ot, s32, Camera *);

/* lead: worldmap_8007A9F8, 8007C3B8, 8007DE98, 80080370, 800811C0, 80090A84 */
extern VECTOR D_8009B364[16]; /* terrain split-plane normal per type */
extern VECTOR D_8009B464[16]; /* terrain split-plane point per type */
extern VECTOR D_8009B244[2];  /* cell diagonal normals, per diagonal direction */

/* Scratchpad work area of the terrain normal and height. */
typedef struct {
    VECTOR edge0;
    VECTOR edge1;
    VECTOR normal;
    u8 pad30[0x70];
    SVECTOR corners[4]; /* 0xA0: wave-displaced cell corners */
} TerrainScratch;

#define TERRAIN_SCRATCH ((TerrainScratch *)0x1F800000)

s32 func_80048D7C(VECTOR *v, VECTOR *out); /* VectorNormal */
extern s16 D_8009CE68; /* destination id, -1 none */
extern u16 D_8009A5CC[]; /* resident flag word per exit */

/* Scene set-up. */
void MoveImage(RECT *rect, s32 x, s32 y);
s32 func_800286CC(void);
void func_80028A60(s32 mode);
void func_8001B66C(void);
void func_80028470(s32 a, s32 b);
void func_80038428(void *bank);
void func_80072BB0(void);
void func_80072DB4(s32 a, s32 b, s32 c, s32 d);
void func_80076954(void);
void func_8009766C(void);
void func_80084580(void);
void func_8008440C(void);
void func_800979C8(void);
void func_80072090(void);
void func_800736DC(void);
void func_800863E0(void);
void func_80074E58(void);
void func_80075030(void);
void func_800739B8(void);
void func_80088F64(void);
void func_800978FC(void);
void func_8008901C(void);
void func_800865A0(void);
void func_80075228(void);
void func_80098044(void);
void func_80097718(s32 kind, s32 update); /* start an actor */
void func_80086700(void);
s32 func_800923A8(), func_800925A0(), func_8007DE14(), func_8007DE98();
s32 func_8007E450(), func_8007E4E4(), func_8007ECA4(), func_8007EE34();
s32 func_8007F8AC(), func_8007F968(), func_8007FC8C(), func_8007FD30();
s32 func_80078948(), func_80078950();
s32 func_8007C36C(), func_8007C3B8(), func_8007C724(), func_8007C7D8(), func_8007CC6C();
s32 func_8007CD20(), func_8007CE84(), func_8007CF18(), func_8007D078(), func_8007D110();
s32 func_8007D228(), func_8007D2B8(), func_8007D414(), func_8007D4A4(), func_8007D600();
s32 func_8007D690(), func_8007D774(), func_8007D7FC();
void func_800721E4(void);
s32 func_80081174(), func_800811C0(), func_800813E8(), func_80081470(), func_800817A0(), func_80081868();
s32 func_800819C8(), func_80081B24(), func_80081C3C(), func_80081D80(), func_80081FB4(), func_80081FD8();
s32 func_800838E8(), func_80076B34(), func_8008390C(), func_80083A00(), func_80083FE4(), func_80084068();

extern MATRIX D_8009BE4C;
extern void (*D_8009CD40)(void);
extern u16 D_8005957C; /* debug switches */
extern s32 D_8006258C;
extern s32 D_8009D804;
extern SVECTOR D_8009A5B4[]; /* start position per entry */
extern SVECTOR D_8009A488; /* exhaust effect angle */

/* Scratchpad work area of the scaled scene objects. */
typedef struct {
    VECTOR scale[2];
    u8 pad20[0x80];
    SVECTOR angle;    /* 0xA0 */
    u8 padA8[0x48];
    MATRIX matrix[2]; /* 0xF0 */
} ScaleScratch;

#define SCALE_SCRATCH ((ScaleScratch *)0x1F800000)

MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);
extern SVECTOR D_8009A674[]; /* flight path start per entry */
void func_800809EC(PolyFT4 *quads, s32 count, s32 r, s32 g, s32 b);
/* worldmap_80094A5C, 8008C364, 8008E190 */

/* Stream reader: disc read requests (sector, bytes, destination) and
 * host-file requests (name, offset, bytes, destination), sorted by position. */

extern EffectCommand3 *volatile D_8009D3BC; /* next disc request (shared with the CD callbacks) */
extern s32 D_8009BE48, D_8009CCB0, D_8009CCA8, D_8009CCA0;
extern s32 D_8009D7F4, D_8009D614, D_8009CEB8, D_8009C590;
extern u32 D_8009D56C; /* sectors left */
extern s32 D_8009BCCC[3]; /* sector header */

#include "psyq/libcd.h"
extern CdlLOC D_8009CEBC; /* request position */

void func_80096A6C(s32 status, u8 *result);
void func_80096C0C(s32 status, u8 *result);
void func_8009699C(EffectCommand3 *request);
void func_800966CC(EffectCommand4 *request);
s32 func_800968E0(void);

void CdSyncCallback(void (*func)(s32 status, u8 *result));
void CdReadyCallback(void (*func)(s32 status, u8 *result));
s32 CdControlF(u8 com, u8 *param);
void CdGetSector(void *dest, s32 words);
s32 PCopen(char *name, s32 flags, s32 perms);
s32 PCclose(s32 fd);
s32 func_8004C398(s32 fd, void *buffer, s32 size); /* PCread */

/* Scratchpad matrices of the angle and camera helpers. */
#define SCRATCH_MATRIX_A ((MATRIX *)0x1F8000F0)
#define SCRATCH_MATRIX_B ((MATRIX *)0x1F800110)
#define SCRATCH_MATRIX_C ((MATRIX *)0x1F800130)
#define SCRATCH_MATRIX_D ((MATRIX *)0x1F800150)
#define SCRATCH_SVECTOR ((SVECTOR *)0x1F8000A0)
#define SCRATCH_VECTOR ((VECTOR *)0x1F800000)

MATRIX *MulMatrix0(MATRIX *a, MATRIX *b, MATRIX *out);
MATRIX *func_8004AFEC(s32 angle, MATRIX *m); /* RotMatrixY */
MATRIX *func_8004AE4C(s32 angle, MATRIX *m); /* RotMatrixX */
MATRIX *RotMatrixZ(s32 angle, MATRIX *m);

/* Camera placement: eye, target and up direction. */
typedef struct {
    SVECTOR eye;
    SVECTOR target;
    VECTOR up;
} LookAt;

/* Scratchpad work area of the look-at camera. */
typedef struct {
    VECTOR work;
    VECTOR right;
    VECTOR up;
    VECTOR forward;
    SVECTOR eye;
    MATRIX view;
} LookAtScratch;

#define LOOKAT_SCRATCH ((LookAtScratch *)0x1F800000)

void func_8004A480(VECTOR *a, VECTOR *b, VECTOR *out); /* OuterProduct12 */
VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v, VECTOR *out);
MATRIX *TransMatrix(MATRIX *m, VECTOR *t);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v, VECTOR *out);

/* Actor slot entry points (kind: start, update: step); they return the
 * next command. */
typedef s32 (*ActorFunc)(s32 index);

/* Terrain streaming origin (world units, wrapped to the map) and the block
 * cell the camera is in. */
#define TERRAIN_ORIGIN (*(VECTOR *)D_8009BBB4)
extern SVECTOR D_8009C838; /* block cell */

/* Terrain palettes: 64 CLUT ids (two 256-colour palettes faded in 32 steps
 * towards the background colour) and seven texture pages. */
extern u16 D_8009CCB4[0x40];
extern u16 D_8009CD54[7];

void func_8002DD20(void *image); /* upload an image file */
void StoreImage(RECT *rect, void *data);
void func_800931D8(u16 *clut, u16 *out, s32 steps, u8 *colour);

/* 9x9 terrain blocks around the camera: block numbers, row-major. */
typedef struct {
    s16 cells[81];
} BlockGrid;

extern BlockGrid D_8009D570; /* current */
extern BlockGrid D_8009D318; /* previous */

extern s8 D_8009C588[8];

s32 func_80094A5C(VECTOR *position, VECTOR *direction, s32 scale, s32 mode);
s32 func_80094088(VECTOR *position, VECTOR *direction, VECTOR *out);

s32 func_800289D0(s32 file); /* first sector of a disc file */
s32 func_80028998(s32 file); /* host path of a file */
s32 func_8009623C(s32 a, s32 b, s32 c);
s32 func_800962B0(s32 a, s32 b, s32 c, s32 d);
s32 func_80096328(void);
s32 func_800965A4(void);

extern s16 D_800523F0[0x1000][2]; /* PsyQ rcossin_tbl: sine, cosine */
void func_8009980C(u32 *heights, u32 *ot, s32 depth); /* terrain block draw (assembly) */

/* Model instance returned by func_80024524 (actor handle). */
typedef struct {
    u8 pad0[0x3C];
    s32 flags; /* 0x3C: 4 hidden */
    u8 pad40[0x6F];
    s8 animation; /* 0xAF */
} ModelInstance;

/* Parked vehicle state (world units), per party slot. */
typedef struct {
    u16 flags; /* 0x3FFF part >= 0x400: parked on the map */
    u16 x;
    u16 z;
} VehicleSpot;

extern VehicleSpot D_8006EF8E[3];
extern VECTOR D_8009C5AC;

void func_8008C28C(WorldmapActor *actor, s32 member);

/* Recent positions of the player's vehicle (ring of 32). */
typedef struct {
    VECTOR position;
    u16 heading;
    u16 pad;
} TrailPoint;

extern TrailPoint D_8009CEC4[32];
extern s16 D_8009D154; /* trail index */

MATRIX *func_8004ABBC(SVECTOR *angle, MATRIX *m); /* rotation matrix from angles */
extern s32 D_8009C5A8; /* arrival kind */
void func_8008E034(VECTOR *position);

/* Movement probe: the scratchpad position a move is tested at. */
#define SCRATCH_PROBE ((VECTOR *)0x1F800060)

extern u16 D_8009D718[]; /* probe hits: object pairs */

s16 func_80084D00(s32 probe, s16 *hit);
s32 func_80085418(VECTOR *probe, s32 radius, u16 object, u16 other);

extern s16 D_8009BBAC[4]; /* grid corner cells */
/* Parked vehicle headings and the flying vehicle's heading: scalars inside
 * D_8006EE54 (unk5A-unk5E, vehicle_heading) that some vehicle starts address
 * as separate variables. */
extern u16 D_8006EE5A, D_8006EE5C, D_8006EE5E, D_8006EE66;

s32 func_80093978(s32 x, s32 z); /* ground height at a position */

#define setShadeTex(p, tge) \
    ((tge) ? (((u8 *)(p))[7] |= 1) : (((u8 *)(p))[7] &= ~1))

/* Shared quad pool (192 quads), one copy per display buffer. */
typedef struct {
    PolyFT4 quads[0xC0];
} QuadBuffer;

extern QuadBuffer *D_8009D158[2]; /* per display buffer */
extern u16 *D_8009D148; /* per-row wobble spread */
void func_80034714(void *window, s32 text);   /* set the window text */
s32 func_80033728(void *table, s32 id);         /* text by id */
void func_80034888(void *window, u32 *ot, s32 buffer); /* draw the window */

extern PolyFT4 D_8009D2B8[2]; /* destination marker, per display buffer */

/* worldmap_80072238, 80077E68, worldmap */

#define VIEW_VECTORS ((SVECTOR *)D_8009BD40) /* two view vectors, swapped per frame */

void func_80096F18(u8 *view, Camera *camera, s32 distance, SVECTOR *angle);

s32 rand(void);
void func_80093484(VECTOR *offset);

/* Sixteen footprint quads, copied between display buffers as a whole. */
typedef struct {
    PolyFT4 quad[16];
} QuadSet;

/* Whole-set copies of runtime tables. */
typedef struct {
    WorldmapActor actor[64];
} ActorSet;

typedef struct {
    s16 timer[16];
} TimerSet;

typedef struct {
    TrailPoint points[32];
} WorldmapQueue; /* the vehicle trail, saved as a whole */
extern SVECTOR D_8009C838;

/* Resident save of the world-map state across a scene change (0x8005a4e4). */
typedef struct {
    ActorSet actors;       /* 0x0000 */
    VECTOR position;       /* 0x2000 */
    s32 unk2010;           /* D_8009D52C */
    s32 timer_period;      /* D_8009BE40 */
    s32 timer_count;       /* D_8009BCC4 */
    s32 timer_countdown;   /* D_8009D64C */
    TimerSet timers;       /* 0x2020 */
    WorldmapQueue queue;   /* 0x2040 */
    s32 queue_count;       /* 0x22C0 */
    s32 camera_angle[2];   /* 0x22C4: SVECTOR D_8009BD38 as words */
    s32 camera_distance;   /* 0x22CC */
    s32 unk22D0;           /* D_8009BE0C */
    VECTOR unk22D4;        /* D_8009BBB4 */
    s32 unk22E4[2];        /* SVECTOR D_8009C838 as words */
    VECTOR camera_target;  /* 0x22EC */
} WorldmapSave;

extern WorldmapSave D_8005A4E4;

void StoreImage(RECT *rect, void *pixels);
void MoveImage(RECT *rect, s32 x, s32 y);
void ClearOTagR(u32 *ot, s32 count);

extern void *D_8009C7E4; /* free memory block kept while away */
extern void *D_8009C800, *D_8009C890; /* saved VRAM areas */
extern s32 D_8009D14C, D_8009D804;
extern u8 D_80059179;

void func_80096694(void);
void func_80071FEC(void);
void func_80072BB0(void);
void func_80072DB4(s32 a, s32 b, s32 c, s32 d);
s32 func_800286CC(void);
void func_80032EB4(void *a, void *b);
void func_80028A60(s32 mode);
void func_80028470(s32 a, s32 b);
void func_80032498(s32 a, s32 b);
void func_80033698(s32 a, s32 b);
void func_80035DB0(void);
void func_800978FC(void);
void func_8008440C(void);
void func_80085FE0(void);
void func_800865A0(void);
void func_8008901C(void);
void func_80075D4C(void);

#define GROUND_SCROLL ((s32 *)D_8009BBB4) /* ground scroll offset x, y, z */

/* Encounter tables of a terrain kind: 0x200 bytes of formations, then per level
 * bracket 16 formation weights. */
typedef struct {
    u8 data[0x200];
} EncounterSet;

extern EncounterSet D_800658DC; /* encounter set of the next battle */
extern u8 D_80059508;           /* chosen formation */
extern s16 D_8009A3A0[];        /* terrain kind substitutes */
extern u16 D_8009B578[];        /* level bracket thresholds, from 1 */

s32 func_80094028(VECTOR *position);

/* Resident pad state, gathered per dequeued input event. */
extern s32 D_80059488;
extern u16 D_80059570, D_80059574; /* buttons held */
extern u16 D_8005948C, D_80059490; /* buttons pressed */
extern u16 D_800594A4, D_800594A8;
extern u16 D_8009CD50, D_8009BD10, D_8009BD14, D_8009BD18, D_8009BD1C;

void PutDispEnv(DISPENV *env);
void PutDrawEnv(DRAWENV *env);
s32 func_80035CDC(void); /* dequeue one input event */
void func_80037E8C(void);
void func_80037EE4(void);
void func_8001FAB4(s32 a, s32 b);

void func_8009766C(void);
void func_800721E4(void);
void func_80084580(void);
void func_800979C8(void);
void func_800736DC(void);
void func_800863E0(void);
void func_80088F64(void);
void func_80038428(void *bank);
s32 func_80035734(s32 mode);
void func_80086700(void);
void func_80097718(s32 kind, s32 update);

extern void (*D_8009CD40)(void); /* per-frame hook */

void func_80073530(void);
void func_80085F58(void);

MATRIX *ScaleMatrix(MATRIX *m, VECTOR *scale);


/* Scratchpad work area of the camera steering. */
typedef struct {
    VECTOR delta;     /* 0x00 */
    u8 pad10[0x90];
    SVECTOR view;     /* 0xA0: swap space */
} CameraScratch;

/* Gouraud triangle packet (PsyQ POLY_G3 layout). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
} PolyG3;

/* Flat rectangle packet (PsyQ TILE layout). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 w, h;
} Tile;

typedef struct {
    u32 tag;
    u32 code[1];
} DR_TPAGE;

void SetDrawTPage(DR_TPAGE *p, s32 dfe, s32 dtd, s32 tpage);

extern PolyFT4 D_8009C5C0[2]; /* overlay picture, per buffer */
extern DR_TPAGE D_8009C5A0;
extern PolyG3 D_8009C664[8];
extern Tile D_8009C898[0x40];

/* PsyQ primitive tag view (libgpu P_TAG). */
typedef struct {
    u32 addr : 24;
    u32 len : 8;
    u8 r0, g0, b0, code;
} P_TAG;

#define setPrimLen(p, n) (((P_TAG *)(p))->len = (n))

void func_80076954(void);
void func_80074E58(void);
void func_80075030(void);
void func_800739B8(void);
void func_80075228(void);

/* Actor spawn list entry; a zero kind ends a list. */
typedef struct {
    s32 kind;
    s32 update;
} ActorSpawn;

extern ActorSpawn D_80099E8C[];  /* actors of every area */
extern ActorSpawn *D_8009A034[]; /* per area: its actors */
extern s32 D_8009C894;           /* nonzero when resuming a saved state */
extern s32 D_8009C178, D_80059198;
extern u16 D_8005957C;
extern void *D_8004F2FC;

void func_8001B66C(void);
void func_80024F64(s32 a, s32 b);
s32 func_80037FD8(void *data, s32 mode);
void func_80039B68(void *seq, s32 volume, s32 c);
void func_80071EF0(void);
void func_80072090(void);
void func_80073398(void);
void func_80073448(s32 id);
void func_80073E30(void);
void func_80074594(void);
void func_8007565C(void);
void func_80097CB8(Camera *camera);
void func_800976FC(s32 kind, s32 index);

void DrawOTag(u32 *ot);
#define setShadeTex(p, tge) \
    ((tge) ? (((u8 *)(p))[7] |= 1) : (((u8 *)(p))[7] &= ~1))

/* PsyQ POLY_G4 with per-vertex fields. */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} PolyG4v;

/* Scratchpad work area of the scene rigs. */
typedef struct {
    VECTOR position;   /* 0x00 */
    u8 pad10[0x90];
    SVECTOR angle[4];  /* 0xA0 */
    u8 padC0[0x30];
    MATRIX matrix[4];  /* 0xF0 */
} RigScratch;

#define RIG_SCRATCH ((RigScratch *)0x1F800000)

/* View setup at D_8009BD40: eye and look-at points and the up vector. */
typedef struct {
    SVECTOR eye;
    SVECTOR at;
    VECTOR up;
} ViewSetup;

#define VIEW (*(ViewSetup *)D_8009BD40)

extern SVECTOR D_8009A3F0[]; /* camera path control points; pad -1 ends */

/* Scratchpad work area of the camera path. */
typedef struct {
    VECTOR at;        /* 0x00: path point, then look-at */
    VECTOR eye;       /* 0x10 */
    s32 distance;     /* 0x20 */
    u8 pad24[0x7C];
    SVECTOR points[3]; /* 0xA0 */
} PathScratch;

#define PATH_SCRATCH ((PathScratch *)0x1F800000)

void func_80076858(s32 t, SVECTOR *p0, SVECTOR *p1, SVECTOR *p2, VECTOR *out);
s32 func_80094154(VECTOR *a, VECTOR *b);
void func_80097070(MATRIX *m, SVECTOR *angle);
void func_8003A2E4(s32 sound, s32 volume);


/* worldmap_80083A00 */
void func_8004A480(VECTOR *a, VECTOR *b, VECTOR *out); /* outer product */
void func_8004A8EC(MATRIX *in, MATRIX *out);
s32 func_80093978(s32 x, s32 z); /* terrain height */

typedef struct {
    PolyFT4 quads[256];
} EffectQuads;

typedef struct {
    PolyFT4 quads[0x200];
} QuadBlock512;

typedef struct {
    PolyFT4 quads[0x120];
} QuadBlock288;

/* Saved flight position: fraction and world-unit halves. */
typedef struct {
    u16 x_frac;
    s16 x;
    u16 z_frac;
    s16 z;
    u16 count; /* flights started */
} FlightSave;

extern FlightSave D_8006EE80;
s32 func_8008868C(void);

extern u16 D_8009BCE0[16]; /* faded CLUT ids */
void func_8002DD20(void *image);                            /* unpack an image to VRAM */
void StoreImage(RECT *rect, void *data);                     /* read back from VRAM */

extern Drift D_8009AF30[5]; /* drift template points */
s32 rand(void);

void func_80093534(VECTOR *delta); /* wrap a world-unit offset */

/* Model sprite object behind an actor's handle. */
typedef struct {
    VECTOR position; /* world units << 4 */
} ModelObject;

/* Scratchpad work area of the actor sprite pass. */
typedef struct {
    SVECTOR vertex;
    VECTOR offset;
    s32 depth[64];
} DepthScratch;

#define DEPTH_SCRATCH ((DepthScratch *)0x1F800000)

void func_80093484(VECTOR *offset);
void func_80024FF4(MATRIX *m);
void func_8001E298(s32 model, u32 *ot);
void func_800223B0(s32 model, s32 angle);
void func_80023210(s32 model);

/* Scratchpad work area of the terrain pass. */
typedef struct {
    SVECTOR corner[4]; /* block quad */
    u8 pad20[8];
    MATRIX view;       /* 0x28 */
    MATRIX roll;       /* 0x48 */
    u16 clut[16];      /* 0x68 */
} TerrainPassScratch;

#define TERRAIN_PASS_SCRATCH ((TerrainPassScratch *)0x1F800000)

extern s16 D_8009D618[25]; /* 5x5 visible blocks; -1 empty */
extern s16 D_8009BE04;     /* quads used this frame */
MATRIX *RotMatrixZ(s32 angle, MATRIX *m);
void func_80099BFC(u8 *data, s32 count, u32 *ot, PolyFT4 *quads);

/* Scene object placement (16 bytes; the list follows a count halfword). */
typedef struct {
    u16 def;
    u16 flags;
    s16 x, y, z;
    s16 ax, ay, az;
} ScenePlacement;

/* Sprite definitions after a 16-byte header. */
typedef struct {
    u8 header[0x10];
    SpriteDef defs[1];
} SpriteDefTable;

extern s16 D_8009BD28; /* animation count */
extern s32 D_8009C16C, D_8009C840;
extern MATRIX D_8009A140, D_8009A160; /* colour and light matrices */
s32 func_8002C3E8(void *defs);
void func_8002CB54(SpriteDef *def, void **prims, void **prims2, SceneObject *object);
void func_8002C8CC(SpriteDef *def, void *prims, s32 mode);
void SetColorMatrix(MATRIX *m);
void SetLightMatrix(MATRIX *m);

/* Scratchpad work area of the face probe. */
typedef struct {
    VECTOR p[3];      /* face corners; p[1] first holds the scale */
    VECTOR normal;    /* 0x30 */
    VECTOR side;      /* 0x40: probe ends against the plane */
    u8 pad50[0xA0];
    MATRIX m;         /* 0xF0 */
    MATRIX probe;     /* 0x110: rows are the probe segment ends */
} FaceScratch;

VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v, VECTOR *out);

#define FACE_SCRATCH ((FaceScratch *)0x1F800000)

/* Collision mesh of a scene object (behind SceneObject.unk44). */
typedef struct {
    s16 corner[3];
    s16 unk6[4];
} MeshFace;

typedef struct {
    s32 unk0;
    SVECTOR *vertices;
    MeshFace faces[1];
} Mesh;

/* Saved ferry route state: x, z (world units), next waypoint; and the
 * number of runs started. */
extern u16 D_8006EE78[3];
extern u16 D_8006EE7E;
extern u16 D_8009AF80[8], D_8009AF90[8]; /* ferry waypoints (x, z) */

/* Ferry heading history (ring of 32). */
typedef struct {
    s16 dx;
    s16 pad2;
    s16 dz;
    s16 pad6;
} FerryHeading;

extern FerryHeading D_8009CD68[32];
s32 func_80094154(VECTOR *a, VECTOR *b); /* distance */
s32 func_80087F60(void);

/* Scratchpad work area of the airship update. */
typedef struct {
    VECTOR work;
    u8 pad10[0x90];
    SVECTOR rotor;       /* 0xA0 */
    SVECTOR tail;        /* 0xA8 */
    u8 padB0[0x40];
    MATRIX rotor_matrix; /* 0xF0 */
    MATRIX tail_matrix;  /* 0x110 */
} FlightScratch;

#define FLIGHT_SCRATCH ((FlightScratch *)0x1F800000)

/* Scratchpad work area of the ferry update. */
typedef struct {
    VECTOR work;
    VECTOR up;           /* 0x10 */
    u8 pad20[0x80];
    SVECTOR wake;        /* 0xA0 */
    SVECTOR wake_angle;  /* 0xA8 */
    u8 padB0[0x40];
    MATRIX m;            /* 0xF0 */
    u8 pad110[0x40];
    MATRIX m2;           /* 0x150 */
} FerryScratch;

#define FERRY_SCRATCH ((FerryScratch *)0x1F800000)

void func_80097070(MATRIX *m, SVECTOR *angle); /* matrix to angles */

MATRIX *ScaleMatrix(MATRIX *m, VECTOR *scale);
void func_8004A6DC(SVECTOR *v, VECTOR *out, s32 *flag); /* RotTrans */
void func_800935DC(VECTOR *point, VECTOR *origin, VECTOR *normal);

#define gte_ldv0(r0) \
    __asm__ volatile("lwc2 $0, 0(%0);" \
                     "lwc2 $1, 4(%0)" \
                     : \
                     : "r"(r0))
#define gte_rtps() __asm__ volatile("nop;nop;.word 0x4A180001")
#define gte_stsz(r0) __asm__ volatile("swc2 $19, 0(%0)" : : "r"(r0) : "memory")
extern u16 D_8009B624[][2]; /* per area: two spinning scene objects */

/* lead: screen fade (80090A84) */
extern DR_TPAGE D_8009D310;       /* fade blend mode */
extern PolyG4v D_8009CE6C[2];    /* full-screen fade, per display buffer */

/* worldmap_8007DE98 (round 2) */

/* Scratchpad work area of the exhaust-flame actors. */
typedef struct {
    VECTOR scale;      /* 0x00 */
    u8 pad10[0x90];
    SVECTOR position;  /* 0xA0 */
    SVECTOR angle;     /* 0xA8 */
    u8 padB0[0x40];
    MATRIX base;       /* 0xF0 */
    MATRIX rotation;   /* 0x110 */
} FlameScratch;

extern u16 D_8009A684[]; /* flame sizes per actor */

extern u16 D_8009A5A0[][3]; /* per area: three ambient sounds */

/* Scratchpad work area of the flight-track actor. */
typedef struct {
    VECTOR axis[3];    /* 0x00: forward (or scale), up, side */
    u8 pad30[0x70];
    SVECTOR position;  /* 0xA0 */
    SVECTOR angle;     /* 0xA8 */
    u8 padB0[0x40];
    MATRIX base;       /* 0xF0 */
    MATRIX rotation;   /* 0x110 */
    u8 pad130[0x20];
    MATRIX frame;      /* 0x150 */
} TrackScratch;

/* POLY_FT4 with its texture coordinates as (v << 8 | u) words. */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u16 uv0;
    u16 clut;
    s16 x1, y1;
    u16 uv1;
    u16 tpage;
    s16 x2, y2;
    u16 uv2;
    u16 pad1;
    s16 x3, y3;
    u16 uv3;
    u16 pad2;
} PolyFT4uv;

#define HORIZON_QUADS ((PolyFT4uv *)D_8009C744)

extern SVECTOR D_8009A300[2][4]; /* horizon quad corners */

/* Scratchpad work area of the horizon renderer. */
typedef struct {
    SVECTOR angle;    /* 0x00 */
    u8 pad8[0x10];
    MATRIX view;      /* 0x18 */
    MATRIX rotation;  /* 0x38 */
    s32 p;            /* 0x58 */
    s32 flag;         /* 0x5C */
} HorizonScratch;

#define HORIZON_SCRATCH ((HorizonScratch *)0x1F800000)

/* Scratchpad work area of the cell-crossing probe: step[0] result,
 * step[1] target, step[2..4] corner test; cells crossed from and to. */
typedef struct {
    VECTOR step[5];
    u8 pad50[0x50];
    SVECTOR cell[2]; /* 0xA0 */
} CellProbe;

#define CELL_PROBE ((CellProbe *)0x1F800000)

s32 func_8004A70C(s32 sxy0, s32 sxy1, s32 sxy2); /* NormalClip */
s32 func_8009443C(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);
s32 func_800945C8(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);
s32 func_80094750(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);
s32 func_800948D8(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);
s16 func_80094060(s16 row, s16 column);

/* Party vehicle updaters (worldmap_8008C364). */
extern u8 D_8006F364[]; /* per actor slot: party member state (slots 4-6) */
extern u8 D_8006F8E1[]; /* per actor slot: riding flag (slots 4-6) */

void func_800941C4(VECTOR *from, VECTOR *to, VECTOR *direction, s16 *heading);
s32 func_8008BEC8(WorldmapActor *actor);
void func_8008C1DC(s32 effect, WorldmapActor *actor, ActorScratch *scratch);
void func_80074794(s16 id, VECTOR *position);

/* Player vehicle updater (worldmap_8008C364). */
#define SCRATCH_HIT ((VECTOR *)0x1F800090) /* move probe result */

extern s16 D_8009B180[]; /* per landing kind: may stand there */

s32 func_80090C68(WorldmapActor *actor);
void func_8008C040(VECTOR *position, s32 radius, s32 height, u8 *hit, u8 *actor);
s32 func_80094238(VECTOR *position, s32 table);
void func_8007528C(void);

/* lead: heat-haze rows (800811C0) */
typedef struct {
    u32 tag;
    u32 code[5];
} DR_MOVE;

void SetDrawMove(DR_MOVE *p, RECT *rect, s32 x, s32 y);
extern DR_MOVE D_8009D164[2]; /* haze copy-back, per display buffer */

/* libgpu addPrim through the P_TAG view (struct stores). */
#define addPrimTag(ot, p) \
    (((P_TAG *)(p))->addr = ((P_TAG *)(ot))->addr, ((P_TAG *)(ot))->addr = (u32)(p))

/* Pulsing effect settings per slot: position x, y, z, then the actor's
 * step..unk74 words (see func_80082F64). */
extern s16 D_8009AABC[], D_8009AB48[], D_8009ABD4[]; /* 14 per slot */

/* Party slot spots (x, z world units), 6 bytes apart. */
typedef struct {
    u16 x;
    u16 z;
    u16 flags;
} PartySpot;

extern PartySpot D_8006EF8A[];
extern u8 D_8006F8E4[];   /* per party slot: riding */
extern u16 D_8006EE58[];  /* per party slot: saved heading */
void func_800941C4(VECTOR *from, VECTOR *to, VECTOR *direction, s16 *heading);
void func_80074794(s16 id, VECTOR *position);
void func_8008C1DC(s32 effect, WorldmapActor *actor, ActorScratch *scratch);

/* worldmap.c main loop (round 3) */
void CdSync(s32 mode, u8 *result);
void SetGeomOffset(s32 x, s32 y);
void func_800250E0(s32 buffer);
void func_8001D468(void);
void func_80097800(void);
void func_80019CA0(void);
void func_8001C634(void);
void func_80025044(void);
void func_80074F2C(void);
void func_80075104(void);
void func_800762FC(void);
void func_8007634C(void);
void func_80076594(void);
void func_800758C0(void);
void func_80075B58(void);
s32 func_80075E7C(VECTOR *position, s32 level);
extern u8 D_80059460, D_80059178, D_80059171, D_8005954C;
/* Resident return-state words read-modify-written as their own variables
 * (fields flags and unk76 of D_8006EE54). */
extern u16 D_8006EE68, D_8006EE76;

void func_80039E18(s32 sound);

extern SVECTOR D_8009A490[]; /* rig flight path; pad -1 ends */
/* Scratchpad work area of the rig path follower. */
typedef struct {
    VECTOR axis[4];   /* 0x00 */
    u8 pad40[0x60];
    SVECTOR angle;    /* 0xA0 */
    SVECTOR heading;  /* 0xA8 */
    u8 padB0[0x40];
    MATRIX frame;     /* 0xF0 */
} FollowScratch;

extern SVECTOR D_8009A4F8[], D_8009A568[]; /* camera shot paths; pad -1 ends */

/* Scratchpad work area of the camera shot director. */
typedef struct {
    VECTOR point;     /* 0x00 */
    u8 pad10[0x90];
    SVECTOR spot;     /* 0xA0 */
} ShotScratch;

#endif
