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
    long mode;
    SpuVolume depth;
    long delay;
    long feedback;
} SpuReverbAttr;

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
long SpuReadDecodedData(void *data, long flag);
unsigned long SpuSetTransferStartAddr(unsigned long addr);
long SpuSetTransferMode(long mode);
typedef void (*SpuTransferCallbackProc)(void);
SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc func);
void SpuGetVoiceEnvelopeAttr(int voice, long *status, short *level);
void SpuSetNoiseClock(long clock);
long SpuSetIRQ(long on_off);
long SpuSetReverb(long on_off);
long SpuInitMalloc(long num, char *top);
typedef void (*SpuIRQCallbackProc)(void);
SpuIRQCallbackProc SpuSetIRQCallback(SpuIRQCallbackProc func);
long SpuSetReverbModeType(long mode);
void SpuSetReverbModeDepth(short depth_left, short depth_right);
void SpuSetReverbModeDelayTime(long delay);
void SpuSetReverbModeFeedback(long feedback);
void SpuGetReverbModeType(long *type);

#endif
