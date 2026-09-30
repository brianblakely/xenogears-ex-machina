#ifndef PSYQ_LIBC_H
#define PSYQ_LIBC_H

/* PsyQ libc (stdio.h, string.h, memory.h and stdlib.h declarations). */
int sprintf(char *s, const char *format, ...);
int strlen(const char *s);
void *bzero(unsigned char *p, int n);
int rand(void);
int abs(int i);

#endif
