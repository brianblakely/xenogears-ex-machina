#ifndef PSYQ_LIBETC_H
#define PSYQ_LIBETC_H

/* PsyQ libetc (callbacks, vertical sync, video mode). */
int ResetCallback(void);
int VSync(int mode);
int VSyncCallback(void (*func)());
long SetVideoMode(long mode);

#endif
