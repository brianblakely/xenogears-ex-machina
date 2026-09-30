#ifndef PSYQ_LIBSN_H
#define PSYQ_LIBSN_H

/* PsyQ libsn host-file (PC) interface, used by development configurations. */
int PCopen(char *name, int flags, int perms);
int PCcreat(char *name, int perms);
int PClseek(int fd, int offset, int mode);
int PCclose(int fd);

/* Members of the same library that the symbol file does not name yet
 * (PCinit, PCread and PCwrite by their signatures and callers). */
int func_8004C38C(void);
int func_8004C398(int fd, char *buff, int len);
int func_8004C470(int fd, char *buff, int len);

#endif
