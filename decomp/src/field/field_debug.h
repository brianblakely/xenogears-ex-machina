#ifndef FIELD_FIELD_DEBUG_H
#define FIELD_FIELD_DEBUG_H

/* The debug monitor (debug595, at 80280000) the field calls while D_800C268C
 * is clear. */

#include "common.h"

extern s32 D_800C268C;         /* set when the debug monitor is absent */
extern s32 D_80285988;         /* the monitor's interaction debug flag */

void func_80281B00(char *name); /* start a named timer ("EVENT CODE", "PARTICLE  ") */
void func_802811EC(void);
void func_80281204(s32 kind);
void func_8028125C(void);
void func_802812A4(void);
void func_80281400(void);
void func_80281450(void);
void func_802815B0(void);
void func_80284EA4(void);

#endif
