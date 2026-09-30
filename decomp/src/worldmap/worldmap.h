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
    s32 unk48;
    s32 handle;   /* 0x4C */
    union {
        s16 *script; /* script position */
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

extern void *D_8009D7E8, *D_8009D7EC, *D_8009D7F8, *D_8009D7FC;
extern u16 D_8009B64C[][2]; /* per area: two scene objects */
extern u16 D_8009B674[];    /* per area: scene object */

void func_80087904(SceneObject *object, PolyFT4 *quads, s32 count, s32 abr);

extern s16 D_8009AFDC[]; /* scene objects to show; -1 ends */
extern u16 D_8006EF64;
extern void *D_8009BE1C, *D_8009BE20;

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
extern u8 D_8009BD64[];
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
void func_800967E4(void);
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

s32 func_80024524(void *model, s32 a, s32 b, s32 c, s32 d, s32 e);
void func_800245D8(s32 handle, s32 mode);
void func_80022000(s32 handle, s32 scale);
void func_80032F54(void *window, s32 x, s32 y, s32 w, s32 h, s32 a, s32 b);
void func_80034614(void *window);

/* Area object (0x54 bytes, 512 of them, eight per group). */
typedef struct {
    s32 unk0;
    s32 unk4;
    s16 unk8;
    s16 unkA;
    s32 unkC;
    s16 unk10;
    s16 unk12;
    s16 unk14;
    s16 unk16;
    s16 unk18;
    s16 unk1A;
    s16 unk1C;
    s16 unk1E;
    s16 unk20;
    u8 pad22[0x2D];
    u8 flags;     /* 0x4F: 0x80 active */
    u8 pad50[4];
} AreaObject;

/* Effect slot (0x4C bytes, 256 of them). */
typedef struct {
    s16 id;
    s16 unk2;
    s16 active;
    s16 unk6;
    u8 pad8[0x44];
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
    u8 pad0[0xA0];
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
void *func_80037FD8(void *data, s32 mode);
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

extern MATRIX D_8009BE4C;
extern void (*D_8009CD40)(void);
extern u16 D_8005957C; /* debug switches */
extern void *D_8006258C;
extern s32 D_8009D804;
extern SVECTOR D_8009A5B4[]; /* start position per entry */
extern SVECTOR D_8009A488; /* exhaust effect angle */

/* Scratchpad work area of the scaled scene objects. */
typedef struct {
    VECTOR scale[2];
    u8 pad20[0xD0];
    MATRIX matrix[2]; /* 0xF0 */
} ScaleScratch;

#define SCALE_SCRATCH ((ScaleScratch *)0x1F800000)

MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);
extern SVECTOR D_8009A674[]; /* flight path start per entry */
void func_800809EC(PolyFT4 *quads, s32 count, s32 r, s32 g, s32 b);

#endif
