#ifndef PSYQ_LIBCD_H
#define PSYQ_LIBCD_H

#include "psyq/types.h"

/* PsyQ libcd interface as linked in the resident (SDK library code). */
#define CdlSetloc 0x02
#define CdlPause 0x09
#define CdlSetfilter 0x0D

typedef struct {
    u_char minute;
    u_char second;
    u_char sector;
    u_char track;
} CdlLOC;

typedef struct {
    u_char file;
    u_char chan;
    u_short pad;
} CdlFILTER;

/* CD audio attenuation (CdMix). */
typedef struct {
    u_char val0; /* left to SPU left */
    u_char val1; /* left to SPU right */
    u_char val2; /* right to SPU right */
    u_char val3; /* right to SPU left */
} CdlATV;

typedef void (*CdlCB)(u_char intr, u_char *result);

int CdInit(void);
int CdSetDebug(int level);
void CdFlush(void);
CdlCB CdSyncCallback(CdlCB func);
CdlCB CdReadyCallback(CdlCB func);
void (*CdDataCallback(void (*func)()))();
int CdControl(u_char com, u_char *param, u_char *result);
int CdSync(int mode, u_char *result);
int CdGetSector(void *madr, int size);
int CdControlB(u_char com, u_char *param, u_char *result);
int CdControlF(u_char com, u_char *param);
int CdMix(CdlATV *vol);
int CdDataSync(int mode);
CdlLOC *CdIntToPos(int i, CdlLOC *p);
int CdPosToInt(CdlLOC *p);
int CdRead2(long mode);

#endif
