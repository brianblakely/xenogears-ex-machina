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
extern FileLoad D_8009D3F8[4]; /* shared read list */

/* Frame state. */
typedef struct {
    u8 pad0[0x70];
    s32 unk70;
    s32 unk74;
} WorldmapView;

extern s32 D_8009D144;
extern s16 D_8009D558;
extern u16 D_8006EE76;
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
