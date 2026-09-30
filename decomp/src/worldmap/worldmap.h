#ifndef WORLDMAP_H
#define WORLDMAP_H

#include "common.h"

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
extern void *D_8006259C;
extern void *D_8009BC38, *D_8009BCB0;
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
    u16 unk60;
    u16 unk62;
    u16 unk64;
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
    s32 vx, vy, vz;
} Vec3;

/* Named arrival point: position in world units and its id; -1 ends a list. */
typedef struct {
    s16 x;
    s16 id;
    s16 z;
    s16 pad;
} WorldmapSpot;

extern s32 D_8009BE10; /* movement mode */
extern Vec3 D_8009C5AC; /* player position (20.12) */
extern s32 D_8009C584;  /* player heading */
extern WorldmapSpot *D_8009D3F4;

void func_8008DFF4(Vec3 *position);

/* Frame state. */
typedef struct {
    u8 pad0[0x70];
    s32 unk70;
    s32 unk74;
} WorldmapView;

extern s32 D_8009D144;
extern s16 D_8009D558;
extern s32 D_8009C5BC;
extern WorldmapView *D_8009BE3C;
extern u8 D_8009BBB4[], D_8009BD40[], D_8009BE28[];

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
void func_800981C8(void *);
void func_800983A0(void *);
void func_80098CC0(void);
void func_8009932C(s32, s32, void *);

#endif
