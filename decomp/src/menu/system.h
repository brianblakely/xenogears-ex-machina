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

s32 func_800888E4(s32 flag);
void func_8008895C(void);
void func_80088A40(void);

#endif
