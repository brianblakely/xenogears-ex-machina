#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include "common.h"

/* Saved system options word (resident, 0x8006f980). */
typedef struct {
    u32 version : 4;   /* 1 once written */
    u32 option4 : 1;   /* mirrors D_80099D9B */
    u32 option5 : 1;   /* mirrors D_80099D9C */
    u32 option6 : 7;   /* mirrors D_80099D9F */
    u32 option13 : 3;  /* mirrors D_80099D98 */
    u32 complete : 1;  /* every tracked flag was set */
    u32 unused : 15;
} SystemOptions;

/* Resident save block at 0x8006f978: 64 progress flags, one bit each,
 * then the options word. */
typedef struct {
    u8 flags[8];
    SystemOptions options;
} SystemSave;

extern SystemSave D_8006F978;
extern u8 D_8005061C;    /* nonzero keeps the options in D_8006F980 */
extern u8 D_800927EC;
/* Current option settings (0x80099d98). */
typedef struct {
    u8 option13;  /* 0x00 */
    u8 unk1;
    u8 unk2;
    u8 option4;   /* 0x03 */
    u8 option5;   /* 0x04 */
    u8 unk5;
    u8 unk6;
    u8 option6;   /* 0x07 */
} Settings;

extern Settings D_80099D98;

/* 0xF8-byte record of the table at 0x8009a0d8. */
typedef struct {
    u8 unk0[0x77];
    u8 unk77;
    u8 unk78[3];
    u8 unk7B;
    u8 unk7C[4];
    s16 unk80;
    s16 unk82;
    u8 unk84[0x74];
} Window;

extern Window D_8009A0D8[];
extern u16 D_80059570;   /* pad buttons held this frame */
extern s32 D_800927F4;

extern s32 D_80010000;   /* boot word: -1, 0 or other start state */
extern u8 D_80091BB0[];
extern s32 D_800928CC;
extern Window *D_80092868;
extern Window *D_80092870;
extern s16 D_80092898;
extern s32 D_8009289C;
extern s32 D_80092930;
extern s32 D_800928E8;
extern u8 D_800928A0;
extern u8 D_80092920;
extern s16 D_800928D0;

void func_80088BFC(void);
void func_800444D8(void *callback);
void func_80048BC4(void);
void func_80032498(s32 kind, void *data);
void func_80028470(s32 a, s32 b);
void func_800374E8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g, s32 h, s32 i, s32 j, s32 k);
void func_80088CBC(s32 index);
void func_8008A110(s32 a, s32 b);
void func_8008A128(s32 a, s32 b);
void func_8008E620(void);
s32 func_800888E4(s32 flag);
void func_800888B0(void);
s32 func_800889C8(void);
void func_8003278C(s32 a, s32 value, s32 c, s32 d);
s16 func_80043A58(s32 x, s32 y);
void func_8008895C(void);
void func_80088A40(void);

#endif
