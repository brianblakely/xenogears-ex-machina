#ifndef PSYQ_LIBSPU_H
#define PSYQ_LIBSPU_H

/* PsyQ libspu transfer interface. */
long SpuReadDecodedData(void *data, long flag);
unsigned long SpuSetTransferStartAddr(unsigned long addr);
long SpuSetTransferMode(long mode);
void *SpuSetTransferCallback(void *func);
void SpuGetVoiceEnvelopeAttr(long voice, long *status, short *level);
void SpuSetNoiseClock(long clock);

#endif
