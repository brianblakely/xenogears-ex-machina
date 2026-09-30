#ifndef MENU_SCENE_H
#define MENU_SCENE_H

#include "menu.h"

/* Scratchpad work area of the scene drawing. */
typedef struct {
    Vector camera; /* 0x00: camera position of this frame */
} SceneScratch;

#define SCENE_SCRATCH ((SceneScratch *)0x1F800000)

/* libgpu LINE_F2 layout. */
typedef struct {
    u8 addr[3];
    u8 len;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LineF2;

typedef struct {
    LineF2 line;
    u8 unk10[0x10];
} SceneLine;

typedef struct {
    s16 unk0, unk2, unk4, unk6;
    u8 unk8[4];
} SceneCell12;

typedef struct {
    s16 unk0, unk2, unk4;
    u8 unk6;
    u8 unk7[3];
} SceneCell10;

extern Vector D_80096FA8; /* camera position */
extern SceneLine D_80094818[100];
extern SceneCell12 *D_800926C8;
extern SceneCell10 *D_800926BC;
extern u8 D_80092708;
extern s32 D_800926DC;
extern s16 D_800926E8;
extern s16 D_800926EC;
extern s32 D_800912DC;

void func_8007E3CC(void *arg);
void func_8007D334(s32 arg0, s32 arg1, s32 kind);
void func_8008EB4C(s32 sound);

#endif
