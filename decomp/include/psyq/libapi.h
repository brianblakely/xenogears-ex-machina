#ifndef PSYQ_LIBAPI_H
#define PSYQ_LIBAPI_H

/* PsyQ libapi (BIOS kernel services, controller and memory card startup). */
long OpenEvent(unsigned long desc, long spec, long mode, long (*func)());
long CloseEvent(long event);
long EnableEvent(long event);
long DisableEvent(long event);
long EnterCriticalSection(void);
void ExitCriticalSection(void);
void SwEnterCriticalSection(void);
void SwExitCriticalSection(void);
void FlushCache(void);
long open(char *devname, unsigned long flag);
long write(long fd, void *buf, long n);
void InitPAD(char *bufA, long lenA, char *bufB, long lenB);
int StartPAD(void);
void StopPAD(void);
void ChangeClearPAD(long val);
long InitCARD(long val);
long StartCARD(void);
void _bu_init(void);

/* A controller member between _send_pad and _remove_ChgclrPAD that the
 * symbol file does not name yet: registers the two actuator buffers. */
void func_80040C3C(unsigned char *data0, long size0, unsigned char *data1, long size1);

#endif
