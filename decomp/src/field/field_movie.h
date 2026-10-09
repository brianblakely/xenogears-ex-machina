#ifndef FIELD_FIELD_MOVIE_H
#define FIELD_FIELD_MOVIE_H

#include "field.h"

/* The field movie player (800a7c58) and the VRAM it borrows: party sprite
 * blocks 1 and 2 (8005a414) hold the 320x256 area at (200, 0) while a
 * movie plays. */

extern void *D_8005A41C; /* resident: party sprite block 2 (8005a414[2]), addressed alone */

extern s32 D_801E89E0; /* movie library: 1 lets it present frames itself */

extern u16 D_800C3900;   /* buttons pressed */

void func_800775F8(void);
void func_8007999C(void);
void func_80085738(void);
void func_80085788(void);
void func_801D43B0(void);                      /* movie library: close */
void func_801E7FD4(void);                      /* 801e module: stop */
void func_800A77C4(s32 unused);
void func_800AC99C(void);
void func_800ACB90(void);
void func_800ACCB0(void);
void func_800ACCF4(void);

#endif
