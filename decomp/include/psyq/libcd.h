#ifndef PSYQ_LIBCD_H
#define PSYQ_LIBCD_H

/* PsyQ libcd interface as linked in the resident (SDK library code). */
#define CdlSetloc 0x02
#define CdlPause 0x09

int CdControlB(unsigned char com, unsigned char *param, unsigned char *result);

#endif
