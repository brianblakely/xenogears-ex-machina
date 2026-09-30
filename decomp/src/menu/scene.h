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

/* Glyph of the menu font. */
typedef struct {
    u8 unk0[2];
    u8 width; /* 0x02 */
} Glyph;

/* Text cursor and colour of the menu's text drawing. */
extern u8 D_800926F0, D_800926F4, D_800926F8; /* text colour r, g, b */
extern s32 D_80059488;

/* Options of the menu's settings screen, one byte each. */
extern u8 D_80099D98[];
extern u8 D_80092884;
extern s32 D_800912F4[];
extern u32 D_8009274C; /* pad buttons repeating this frame */
extern u32 D_80092750; /* pad buttons pressed this frame */

extern s8 D_8009273C;
extern s8 D_80092740;
extern s32 D_80092744;
extern s32 D_800912F0;
extern s32 D_80092734;
extern s32 D_80092950;

void func_8007E3CC(void *arg);
Glyph *func_8007E8AC(s32 ch);
void func_8007E964(s32 ch);
s32 func_8007EB6C(u8 *text);
void func_8007EE08(s32 highlight);
void func_8007F834(void);
s32 func_8007FF70(s32 value, s32 max, s32 flags);
void func_80080C48(s32 arg);
void func_8007D334(s32 arg0, s32 arg1, s32 kind);
void func_8008EB4C(s32 sound);

#endif
