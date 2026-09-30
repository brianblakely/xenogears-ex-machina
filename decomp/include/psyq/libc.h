#ifndef PSYQ_LIBC_H
#define PSYQ_LIBC_H

/* PsyQ libc (stdio.h, string.h, memory.h and stdlib.h declarations). */
int sprintf(char *s, const char *format, ...);
int strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strcat(char *dst, const char *src);
void *bzero(unsigned char *p, int n);
void *memchr(void *s, int c, int n);
void *memcpy(void *dest, void *src, int n);
void *memmove(void *dest, void *src, int n);
int rand(void);
int abs(int i);

#endif
