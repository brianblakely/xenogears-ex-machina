#ifndef PSYQ_LIBPRESS_H
#define PSYQ_LIBPRESS_H

#include "psyq/types.h"

/* PsyQ libpress: the MDEC decoder (run-level decoding of a frame's bitstream,
 * DCT input and output). */
void DecDCTReset(int mode);
int DecDCTvlc(u_long *bs, u_long *buf);
int DecDCTvlcSize(int size);
void DecDCTin(u_long *buf, int mode);
void DecDCTout(u_long *buf, int size);
int DecDCToutCallback(void (*func)());

#endif
