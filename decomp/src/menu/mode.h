#ifndef MENU_MODE_H
#define MENU_MODE_H

#include "common.h"
#include "resident/cd.h"

/* The menu mode (menu5 80084FD0-8008509C, 8008518C-80085E34, 800888B0-
 * 80088BFC; menu6): its entry and frame loop, the task that loads its
 * files and runs the title, options and scene screens, its music, and the
 * option settings and progress flags it keeps in the game data. */

/* Current option settings (0x80099d98). */
typedef struct Settings {
    u8 level;     /* 0x00: saved as option13 */
    u8 unk1;
    u8 rate;      /* 0x02: frame rate choice */
    u8 option4;   /* 0x03: port 1 vibration */
    u8 option5;   /* 0x04: port 2 vibration */
    u8 com1;      /* 0x05: side 1 played by the computer */
    u8 driven;    /* 0x06: side 2 played by the computer */
    u8 option6;   /* 0x07 */
    u8 unk8;
    u8 speed;     /* 0x09 */
    u8 command;   /* 0x0A: the opponent's current command */
    u8 unkB;
    s16 unkC;
} Settings;

extern void (*D_80088BFC[])(s32);  /* mode tasks, by D_80050618 */
extern FileRequest D_800917C0[6];  /* sequence, sound bank, messages, map, scene; zero file */
extern s32 D_800917F0;             /* nonzero: the menu plays its own sequence */
extern char *D_80091BB0[];         /* names of the menu's heap block kinds */
extern s16 D_80092898;             /* vertical blanks per frame */
extern s32 D_800928C8;             /* menu mode */
extern s32 D_800928CC;
extern u16 D_800928D0;             /* debug display switches */
extern u8 D_80092920;              /* bit 0: the menu screen is shown */
extern void (*D_80092930)(void *block); /* debug hook (switch 0x10) */
extern s32 D_80092948;             /* the music sequence */
extern Settings D_80099D98;

void func_800851D4(void);
void func_800852C4(s32 arg);       /* the menu task */
void func_800888B0(s32 flag);
s32 func_800888E4(s32 flag);
void func_8008895C(void);
s32 func_800889C8(void);
void func_80088A40(void);
void func_80088AF8(void);
void func_80088BD4(s32 flag);
void func_80088C00(void);
void func_80088C28(void);
void func_80088CBC(s32 index);

#endif
