#ifndef PSYQ_LIBC_H
#define PSYQ_LIBC_H

/* PsyQ libc (stdio.h, string.h, memory.h and stdlib.h declarations). */
int sprintf(char *s, const char *format, ...);
int strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strcat(char *dst, const char *src);
void *bzero(unsigned char *p, int n);
void *memchr(void *s, int c, int n);
/* As in MEMORY.H (PsyQ 4.6), memcpy and memset are unprototyped "to avoid
 * conflicting" with GCC's built-ins, which they keep: field 800AB808 needs the
 * built-in memcpy. slot39_801DBE54 declares its own, which drops it. */
void *memcpy();
void *memset();
void *memmove(void *dest, void *src, int n);
int rand(void);
int abs(int i);

#endif
