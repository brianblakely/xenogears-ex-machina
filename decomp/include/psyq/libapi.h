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
long SetRCnt(unsigned long spec, unsigned short target, long mode);
long GetRCnt(unsigned long spec);
long StartRCnt(unsigned long spec);
long StopRCnt(unsigned long spec);
long TestEvent(long event);
void UnDeliverEvent(unsigned long event, unsigned long spec);
long open(char *devname, unsigned long flag);
long write(long fd, void *buf, long n);
long read(long fd, void *buf, long n);
long close(long fd);
long Krom2RawAdd(unsigned long sjiscode);
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

/* BIOS file calls the symbol file does not name yet (the memory card's
 * file system), and the start of a card check. */
struct DIRENTRY { /* a directory entry (LIBAPI.H) */
    char name[20];
    long attr;
    long size;
    struct DIRENTRY *next;
    long head;
    char system[4];
};
long func_80040574(char *device);                                 /* format */
struct DIRENTRY *func_80040584(char *name, struct DIRENTRY *dir); /* firstfile */
struct DIRENTRY *func_80040594(struct DIRENTRY *dir);             /* nextfile */
long func_800405A4(char *from, char *to);                         /* rename */
long func_800405B4(char *name);                                   /* erase */
unsigned long func_800405E4(void);
long func_8004E784(long channel);                                 /* start a card check */

#endif
