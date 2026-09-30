#ifndef FIELD_FIELD_MOVIE_H
#define FIELD_FIELD_MOVIE_H

#include "field.h"

/* The field movie player (800a7c58) and the VRAM it borrows: party sprite
 * blocks 1 and 2 (8005a414) hold the 320x256 area at (200, 0) while a
 * movie plays. */

/* A file-list entry for resident 80029afc: file index and destination. */
typedef struct {
    u16 file;
    void *destination;
} MovieFileRequest;

extern void *D_8005A41C; /* resident: party sprite block 2 (8005a414[2]), addressed alone */
extern void *D_80065AFC[3]; /* resident: party character file blocks */

s32 func_80029AFC(MovieFileRequest *list, s32 mode, s32 a2); /* resident: read a file list */

#endif
