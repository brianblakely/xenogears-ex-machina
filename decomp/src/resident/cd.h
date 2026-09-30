#ifndef RESIDENT_CD_H
#define RESIDENT_CD_H

#include "common.h"
#include "psyq/libcd.h"

/* Resident disc access (0x80028230-0x8002a68c): the file index, reads of
 * files and file lists, the CD command state and the PC file server. */

/* A file-list entry for 80029afc: file index and destination. */
typedef struct {
    u16 file;
    void *destination;
} FileRequest;

/* One loaded file: its file number and the heap block holding its data. A
 * zero id terminates a table. */
typedef struct {
    u16 id;
    void *data;
} FileEntry;

extern u8 *D_8004FDF0;      /* file index: 7 bytes per file */
extern u16 *D_8004FDF4;     /* directory table: first file of each directory, 1-based */
extern s32 D_8004FDF8;      /* bytes of the current read */
extern s32 D_8004FDFC;
extern s32 D_8004FE00;      /* files in the current list */
extern s32 D_8004FE04;      /* sector of the current read */
extern void *D_8004FE08;    /* destination of the current read */
extern FileRequest *D_8004FE0C; /* the file list being read */
extern s32 D_8004FE10;
extern s32 D_8004FE14;      /* selected directory (first file - 1) */
extern s32 D_8004FE18;      /* second directory selection */
extern s32 D_8004FE1C;
extern s32 D_8004FE34;
extern s32 D_8004FE38;      /* read mode */
extern s32 D_8004FE3C;
extern u8 D_8004FE44;
extern u8 D_8004FE45;
extern u8 D_8004FE46;
extern u8 D_8004FE47;
extern char *D_8004FE48;    /* PC file server name table (64 bytes per file), or NULL */
extern s32 D_8004FE4C;
extern s32 D_8005A488, D_8005A48C, D_8005A490, D_8005A494, D_8005A498, D_8005A49C;
extern s32 D_8005A4A4, D_8005A4A8, D_8005A4B4;
extern s32 D_8005A4DC;
extern s32 D_80059EF8[3];   /* read status words */
extern s32 D_80059F0C;      /* the file being read */
extern CdlLOC D_80059F10;   /* CD position of the current read */
extern u8 D_80059F18[4];    /* CD mode parameter */
extern u8 D_80059F1C[];     /* CD command result */

void func_80028230(u8 *files, u16 *directories, u32 mode);
void func_800283D4(void);
s32 func_80028470(s32 group, s32 index);
s32 func_800284B4(s32 *group, s32 *index);
s32 func_80028530(void);
s32 func_80028548(s32 group, s32 index);
s32 func_800286CC(void); /* disc busy */
s32 func_80028738(s32 file);
s32 func_800288EC(s32 file); /* file size rounded up to words */
s16 func_80028928(s32 file);
char *func_80028998(s32 file);
s32 func_800289D0(s32 file);
s32 func_80028A60(s32 mode);
void func_80028ECC(s32 index);
s32 func_8002954C(s32 sector, void *destination, s32 size, s32 a3, s32 a4);
s32 func_800295D8(s32 file, void *destination, s32 a2, s32 a3);
s32 func_80029690(s32 file, void *destination, s32 a2, s32 a3);
s32 func_80029AFC(FileRequest *list, s32 mode, s32 a2);
void func_8002A2D0(s32 file);
void func_8002A394(s32 file);
void func_8002A428(u8 mode);
void func_8002A498(s32 reason);
void func_8002A524(FileEntry *table);
FileEntry *func_8002A57C(s32 first, FileEntry *table);

#endif
