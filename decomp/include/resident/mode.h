#ifndef RESIDENT_MODE_H
#define RESIDENT_MODE_H

#include "common.h"
#include "gpu.h"
#include "cd.h"

/* Resident startup and the mode dispatcher (0x80019524-0x80019d48). */

/* Mode table 8001808c: the mode's entry, the BSS it clears and whether its
 * overlay file is loaded into the mode block before the call. */
typedef struct {
    void (*entry)(void);
    u8 *bss_start;
    u8 *bss_end;
    s32 loaded;
} ModeEntry;

/* Kernel menu (mode 0) double buffer, 8005 95e8. */
typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u_long ot[1];
    POLY_F3 cursor;
} KernelBuffer;

/* An 8x8 tile of the debug Game of Life screen (a TILE_8 primitive). */
typedef struct {
    u32 tag;
    u32 rgbc;
    u32 xy;
} LifeTile;

extern s32 D_8004F2D8;           /* kernel menu cursor */
extern s32 *D_8005917C;
extern u8 D_8006F9DC[0x20]; /* scene state: [2] the scene selector */
extern u8 *D_80059470;  /* the scene music sequence */
extern s32 D_80059520;
extern u8 *D_8005949C;  /* the scene music instrument data */

/* Game state reset by 8001aadc. */
extern s32 D_8004F2F4, D_8004F2F8, D_8004F2FC, D_8004F300, D_8004F304, D_8004F308;
extern s32 D_8004F30C, D_8004F310, D_8004F314, D_8004F318, D_8004F31C, D_8004F320;
extern s32 D_8004F324, D_8004F328, D_8004F32C, D_8004F330, D_8004F334, D_8004F338;
extern s32 D_8004F33C, D_8004F340, D_8004F344, D_8004F348, D_8004F34C, D_8004F350;
extern s32 D_8004F354, D_8004F358, D_8004F35C, D_8004F360, D_8004F364, D_8004F368;
extern s32 D_8004F36C, D_8004F370, D_8004F374, D_8004F378, D_8004F37C, D_8004F380;
extern s16 D_8004F384;
extern u8 D_8005942C;
extern u8 D_800594D0;
extern s32 D_8005A444[3];
extern s32 D_80062518[4]; /* loaded wave bank per slot */
extern s32 D_80062590[3];
extern s32 D_8006F990[3];
extern s32 D_8006FABC[3];
extern FileRequest D_800625A4[4]; /* party file list, zero-terminated */
extern FileRequest D_8006F9BC[4]; /* the battle mode's sound files (8001bbac) */
extern void *D_80065AFC[3];       /* party character file blocks */
extern void *D_8005A4A0;          /* file 0xa7 block */
extern void *D_8005A4BC;          /* file 0xa8 block */
extern void *D_8005A414[3];       /* party field sprite blocks */
extern s32 D_8005A4C0;            /* map read-ahead size */
extern void *D_8005A4E0;          /* map read-ahead block */
extern s32 D_80062528;            /* the active sequence */
extern struct SoundSequence *D_8006258C; /* the transferred wave bank */

extern u8 *const D_80018084; /* overlay decode destination */
extern u8 D_8006FAEC[];      /* the last word below the overlay area */
extern s32 D_80018088;       /* next mode */
extern ModeEntry D_8001808C[];
extern s32 D_8004EAA0[];     /* each mode's overlay file in directory 1 */
extern struct SoundSequence *D_80059560; /* resident wave banks */
extern struct SoundSequence *D_800595AC;
extern s32 D_80010000;
extern u8 D_8004EABC[];      /* compressed boot logo image */
extern char *D_8004F2C0[];   /* messages of the fatal errors 0x80-0x85 */
extern s32 D_8004F2BC;       /* fatal error count */
extern u8 D_80010004[];
extern u16 D_80018004[];

/* Original hand-written startup code. */
void func_80019524(void);               /* entry: clear the BSS, reset the stack, boot */
void func_80019548(void);               /* sp = fp = 0x80200000, gp = _gp */
void func_80019560(u8 *start, u8 *end); /* zero the words after start through end */

void func_8001A4B4(void); /* mode 0, the kernel menu */
void func_8001B6C4(void); /* mode 2 */
void func_8001B844(void); /* the battle's display buffers and projection */
void func_8001C634(void); /* mode 5 */
void func_8001996C(s32 mode);
void *func_800199CC(s32 mode);
void func_80019ACC(s32 error) __attribute__((noreturn));
void func_80019C7C(void);
void func_80019CD0(void);
void func_80019D48(void);
void func_80019EF8(s32 error, u32 caller);
void func_8001A250(void);
void func_8001AADC(void);
void func_8001B6BC(void);
void func_8001B158(s32 extra);
void func_8001B3A8(void);
void func_8001B53C(s32 map);
void func_8001AD4C(void);
void func_8001AEB8(void);
void func_8001AD1C(void);
s32 func_8001ACF0(s32 index);
void func_8001BB50(void);
void func_80024F20(void);

/* More of the mode dispatcher's calls and state. */
/* An empty debug print: the menu passes it a message. */
void func_80019964();
void func_80019CA0(void);
void func_8001AC94(void);
void func_8001ACA4(void);
void func_8001B044(void);
s32 func_8001B484(s32 map, s32 slot);
void func_8001B66C(void);
void func_8001B970(void);
void func_8001BB0C(void);
void func_8001BBAC(void);
extern u8 D_80059430;
extern u8 D_80059434;
extern u8 D_80059438;
extern u8 D_8005943C;
extern s32 *D_800594F0;
extern u8 D_800594F8;
extern u8 D_8005959C;
extern void *D_8005A420[4];
extern void *D_8005A450[4];
extern s32 D_80065B08;
extern u8 D_80062648[0x3200]; /* a work buffer of the field, the world map, battle and its overlays */
extern s32 D_80065848[5];
extern u8 D_8005947C; /* the pending scene + 1 */
extern u8 D_80059508;
extern u8 D_80059179; /* the battle-entry flag (the field and world map set it) */
extern u8 D_80065ADC[16];
extern s16 D_8006BE2C[3]; /* per party slot (the field) */
extern u8 D_80059180; /* battle music playing */

#endif
