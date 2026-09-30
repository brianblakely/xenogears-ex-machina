#ifndef PSYQ_LIBSPU_H
#define PSYQ_LIBSPU_H

#include "psyq/types.h"

/* PsyQ libspu. */
typedef struct {
    short left;
    short right;
} SpuVolume;

typedef struct {
    SpuVolume volume;
    long reverb;
    long mix;
} SpuExtAttr;

typedef struct {
    u_long mask;
    SpuVolume mvol;
    SpuVolume mvolmode;
    SpuVolume mvolx;
    SpuExtAttr cd;
    SpuExtAttr ext;
} SpuCommonAttr;

void SpuInit(void);
void SpuQuit(void);
void SpuSetCommonAttr(SpuCommonAttr *attr);

#endif
