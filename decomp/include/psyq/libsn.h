#ifndef PSYQ_LIBSN_H
#define PSYQ_LIBSN_H

/* PsyQ libsn host-file (PC) interface, used by development configurations. */
int PCopen(char *name, int flags, int perms);
int PCcreat(char *name, int perms);
int PClseek(int fd, int offset, int mode);
int PCclose(int fd);

/* Trap into the host debugger (the SDK macro assembles `break 1024`). */
#define pollhost() __asm__ volatile("break 1024")

/* Members of the same library that no library signature names (PCinit,
 * PCread and PCwrite by their signatures and callers). */
int PCinit(void);
int PCread(int fd, char *buff, int len);
int PCwrite(int fd, char *buff, int len);

#endif
