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

/* BIOS file calls (the memory card's file system), GetGp and the start of a
 * card check: stubs the resident folded into neighbouring objects, each its
 * own object by the pinned signatures and named in the symbol file. */
struct DIRENTRY { /* a directory entry (LIBAPI.H) */
    char name[20];
    long attr;
    long size;
    struct DIRENTRY *next;
    long head;
    char system[4];
};
long format(char *device);                                    /* B(41h) */
struct DIRENTRY *firstfile(char *name, struct DIRENTRY *dir); /* B(42h) */
struct DIRENTRY *nextfile(struct DIRENTRY *dir);              /* B(43h) */
long rename(char *from, char *to);                            /* B(44h) */
long delete(char *name);                                      /* B(45h): erase */
unsigned long GetGp(void);
long _card_info(long channel);                                /* A(ABh): start a card check */

#endif
