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

/* Kernel menu (mode 0) double buffer, 8005 95e8. */
typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u32 ot[1];
    POLY_F3 cursor;
} KernelBuffer;

extern KernelBuffer D_800595E8[2];
extern s32 D_800592C4;           /* kernel menu frame count */
extern s32 D_800592C8;           /* kernel menu buffer index */
extern KernelBuffer *D_800592CC; /* kernel menu current buffer */
extern s32 D_800592D0;           /* kernel menu running */
extern u8 *D_800592D4;
extern u8 *D_800592D8;
/* An 8x8 tile of the debug Game of Life screen (a TILE_8 primitive). */
typedef struct {
    u32 tag;
    u32 rgbc;
    u32 xy;
} LifeTile;

extern LifeTile *D_800592DC[2]; /* tile buffers per display buffer */
extern s32 D_8004F2D8;           /* kernel menu cursor */
extern u16 D_800594A4;           /* pad buttons repeated */
extern u16 D_8005948C;           /* pad buttons pressed */
extern s32 *D_8005917C;
extern u8 D_8006F9DE;
extern u8 D_80059470[];
extern u8 D_80059520[];
extern u8 D_8005949C[];
extern u8 D_80059484;            /* play time hours */
extern u8 D_80059420;            /* play time minutes */
extern u8 D_80059418;            /* play time seconds */

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
extern s32 D_80062524;
extern s32 D_80062590[3];
extern s32 D_8006F990[3];
extern s32 D_8006FABC[3];
/* A 0xa4-byte character record of the game data. */
typedef struct {
    u8 first;
    u8 rest[0xA3];
} CharacterRecord;

/* Game data 8006d634 (saved with the game). The record count is not
 * established; the party list follows at +0x1d34. */
typedef struct {
    u8 unknown0[0x30C];
    CharacterRecord characters[11];
    u8 unknown1[0x1D34 - 0x30C - 11 * 0xA4];
    u8 party[3]; /* character per slot, 0xff empty */
} GameData;

/* A file-list entry for 80029afc: file index and destination. */
typedef struct {
    s16 file;
    void *destination;
} FileRequest;

extern GameData D_8006D634;
extern GameData *D_8005A39C;
extern FileRequest D_800625A4[4]; /* party file list, zero-terminated */
extern void *D_80065AFC[3];       /* party character file blocks */
extern void *D_8005A4A0;          /* file 0xa7 block */
extern void *D_8005A4BC;          /* file 0xa8 block */
extern void *D_8005A414[3];       /* party field sprite blocks */
extern s32 D_8005A4C0;            /* map read-ahead size */
extern void *D_8005A4E0;          /* map read-ahead block */
extern s32 D_80062528;            /* the active sequence */
extern s32 D_8006258C;            /* the transferred wave bank */

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
extern u16 D_80018004[];

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
void func_8001B158(s32 extra);
void func_8001B3A8(void);
void func_8001B53C(s32 map);
void func_8001AD4C(void);
void func_8001AEB8(void);
void func_8001AD1C(void);
s32 func_8001ACF0(s32 index);
void func_8001BB50(void);
void func_80024F20(void);

/* Disc file access. */
extern u8 *D_8004FDF0;      /* file index: 7 bytes per file */
extern u16 *D_8004FDF4;     /* directory table: first file of each directory, 1-based */
extern s32 D_8004FDF8;
extern s32 D_8004FDFC;
extern s32 D_8004FE14;      /* selected directory (first file - 1) */
extern s32 D_8004FE1C;
extern char *D_8004FE48;    /* PC file server name table (64 bytes per file), or NULL */
extern s32 D_8004FE18;      /* second directory selection */
extern s32 D_8004FE4C;
extern s32 D_8005A488, D_8005A48C, D_8005A490, D_8005A494, D_8005A498, D_8005A49C;
extern s32 D_8005A4A4, D_8005A4A8, D_8005A4B4;
extern u8 D_80059F1C[];     /* CD command result */

void func_80028230(u8 *files, u16 *directories, u32 mode);
void func_800283D4(void);
s32 func_80028470(s32 group, s32 index);
s32 func_800284B4(s32 *group, s32 *index);
s32 func_80028548(s32 group, s32 index);
char *func_80028998(s32 file);
void func_8002954C(s32 sector, void *destination, s32 size, s32 a3, s32 a4);
void func_8002A428(s32 mode);
void func_8002A498(s32 offset);
s32 func_80028530(void);
s32 func_800286CC(void); /* disc busy */
s32 func_80028738(s32 file);
void func_80028A60(s32 mode);
s32 func_800295D8(s32 file, void *destination, s32 a2, s32 a3);
s32 func_800288EC(s32 file); /* file size rounded up to words */
s32 func_80029AFC(FileRequest *list, s32 a1, s32 a2);

/* Resident heap. */
void func_80031A30(void);
void func_80031A68(void *start, void *end);
void func_80031B10(void *start);
s32 func_80031B9C(void);
void func_80031BA8(s32 tag);
s32 func_80031BB4(s32 quiet);
void *func_80031BDC(s32 size, s32 from_top);
void func_800320E8(void *block);
void func_800320A4(void *block); /* keep the block across heap restarts */
void func_800320B8(void *block); /* stop keeping the block */
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
s32 func_80035CDC(void); /* next queued pad entry (8005 94a4), 0 when none */
s32 func_80036410(void);
void func_80035DB0(void);
void func_80036288(void);
void func_8003634C(void);
void func_800363F0(s32 a0);
void func_800379B4(s32 a0);
void func_80037B88(s32 a0);
void func_8003700C(char *format, ...);
s32 func_8003FBF8(char *buffer, char *format, ...);
void func_80037324(u32 *ot); /* flush the debug text into ot */
void func_800374E8(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7, s32 a8, s32 a9, s32 a10);
void func_80037DC0(void);
s32 func_80037FD8(void *a0, s32 a1);
void func_8003BDFC(s32 a0);
s32 func_8003FA38(void); /* rand */
void func_800379D8(s32 scene, s32 a1, void *a2, void *a3, void *a4);
void func_8003278C(s32 a0, s32 a1, s32 a2, s32 a3);
void func_80038310(s32 bank);   /* release a wave bank */
void func_800399D4(s32 sequence); /* release a sequence */
void func_80039C4C(s32 sequence); /* stop a sequence */

/* PsyQ library. */
s32 func_80040D08(void);                           /* CdInit */
void func_80040EF4(s32 a0);
void func_800413EC(s32 a0);
void func_80040FB4(void *callback);                /* CdReadyCallback */
void func_80040FCC(void *callback);                /* CdSyncCallback */
s32 func_80040FE4(s32 command, u8 *parameter, u8 *result); /* CdControlF */
s32 func_80041248(s32 command, u8 *parameter, u8 *result); /* CdControlB */
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
s32 func_8004C338(s32 fd);                       /* PCclose */
s32 func_8004C318(char *name, s32 flags, s32 mode); /* PCopen */
s32 func_8004C348(s32 fd, s32 offset, s32 whence); /* PClseek */
s32 func_8004C398(s32 fd, void *buffer, s32 size); /* PCread */
s32 func_80041410(s32 mode);                     /* CdSync */
s32 func_8004C36C(char *name, s32 mode);
s32 func_8004C38C(void);
s32 func_8004C470(s32 fd, void *buffer, s32 size);
void func_8004C548(void);
void func_8004D294(void);
void func_8004E794(s32 a0);
void func_8004E7E8(void);

#endif
