#ifndef RESIDENT_MODE_H
#define RESIDENT_MODE_H

#include "common.h"
#include "gpu.h"

/* Resident startup and the mode dispatcher (0x80019524-0x80019d48). */

/* Mode table 8001808c: the mode's entry, the BSS it clears and whether its
 * overlay file is loaded into the mode block before the call. */
typedef struct {
    void (*entry)(void);
    u8 *bss_start;
    u8 *bss_end;
    s32 loaded;
} ModeEntry;

extern u8 *const D_80018084; /* overlay decode destination */
extern u8 D_8006FAF0[];
extern s32 D_80018088;       /* next mode */
extern ModeEntry D_8001808C[];
extern s32 D_8004EAA0[];     /* each mode's overlay file in directory 1 */
extern u8 D_8004FE44;
extern u8 D_8004FE45;
extern u8 D_8004FE46;
extern u8 D_8004FE47;
extern void *D_800592BC;     /* the loaded mode block */
extern s32 D_800592C0;       /* the mode whose block is loaded, or -1 */
extern s32 D_80059560;
extern s32 D_800595AC;
extern u16 D_80059570;       /* held pad buttons */
extern s32 D_80010000;
extern u8 D_8004EABC[];      /* compressed boot logo image */
extern char *D_8004F0C0[];   /* error messages by number */
extern s32 D_8004F2BC;       /* fatal error count */
extern u8 D_80010004[];
extern u8 D_80018004[];

/* Original hand-written startup code. */
void func_80019524(void);
void func_80019548(void);
void func_80019560(u8 *start, u8 *end);

void func_8001996C(s32 mode);
void *func_800199CC(s32 mode);
void func_80019ACC(s32 error);
void func_80019C7C(void);
void func_80019CD0(void);
void func_80019D48(void);
void func_80019EF8(s32 error, u32 caller);
void func_8001AADC(void);
void func_8001B6BC(void);
void func_8001BB50(void);
void func_80024F20(void);

/* Disc file access. */
void func_80028230(u8 *index, u8 *directories, s32 count);
s32 func_800283D4(void);
void func_80028470(s32 base, s32 index);
void func_800284B4(s32 *base, s32 *index);
s32 func_80028530(void);
s32 func_80028738(s32 file);
void func_80028A60(s32 mode);
s32 func_800295D8(s32 file, void *destination, s32 a2, s32 a3);

/* Resident heap. */
void func_80031A30(void);
void func_80031A68(void *start, void *end);
void func_80031B10(void *start);
s32 func_80031B9C(void);
void func_80031BA8(s32 tag);
s32 func_80031BB4(s32 quiet);
void *func_80031BDC(s32 size, s32 from_top);
void func_800320E8(void *block);
void func_80031BC4(s32 *first, s32 *second);
void func_800322B4(void);
void func_80032E04(char *name);

void *func_8002DFE0(void);
void func_80032498(s32 tag, s32 word);
void func_800324B8(s32 a0);
void *func_80032E88(void *data, s32 a1);
void func_80032EB4(void *source, void *destination);
void func_80033558(void *a0);
void func_800335F4(void *a0);
s32 func_80035734(s32 a0);
void func_80035CDC(void);
void func_80035DB0(void);
void func_80036288(void);
void func_8003634C(void);
void func_800363F0(s32 a0);
void func_800379B4(s32 a0);
void func_80037B88(s32 a0);
void func_8003700C(char *format, ...);
void func_80037324(s32 a0);
void func_800374E8(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7, s32 a8, s32 a9, s32 a10);
void func_80037DC0(void);
s32 func_80037FD8(void *a0, s32 a1);
void func_8003BDFC(s32 a0);

/* PsyQ library. */
void func_80040454(void);
void func_80040464(void);
void func_800404D4(void);
void func_800404E4(void);
void func_800404F4(void);
void func_80040514(void);
void func_800408F4(void);
void func_80040ED4(void);
void func_80044110(s32 mask);
void func_800443A8(s32 mode);
void func_800444D8(s32 a0);
void func_80044534(s32 a0);
void func_800445D0(s32 mode);
void func_80048BC4(void);
void func_8004B54C(s32 a0);
void func_8004B740(void);
void func_8004B7D0(void (*callback)(void));
void func_8004C2F0(s32 a0);
s32 func_8004C338(s32 fd);
s32 func_8004C36C(char *name, s32 mode);
s32 func_8004C38C(void);
s32 func_8004C470(s32 fd, void *buffer, s32 size);
void func_8004C548(void);
void func_8004D294(void);
void func_8004E794(s32 a0);
void func_8004E7E8(void);

#endif
