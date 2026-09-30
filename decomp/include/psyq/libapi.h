#ifndef PSYQ_LIBAPI_H
#define PSYQ_LIBAPI_H

/* PsyQ libapi (BIOS) kernel event interface. */
long EnableEvent(long event);
long DisableEvent(long event);
void EnterCriticalSection(void);
void ExitCriticalSection(void);

#endif
