#ifndef PSYQ_LIBCD_H
#define PSYQ_LIBCD_H

/* PsyQ libcd interface as linked in the resident (SDK library code). */
#define CdlSetloc 0x02
#define CdlPause 0x09

typedef struct {
    unsigned char minute;
    unsigned char second;
    unsigned char sector;
    unsigned char track;
} CdlLOC;

int CdControlB(unsigned char com, unsigned char *param, unsigned char *result);
CdlLOC *CdIntToPos(int i, CdlLOC *p);
int CdRead2(long mode);

#endif
