#ifndef FIELD_FIELD_ANIM_H
#define FIELD_FIELD_ANIM_H

#include "common.h"

/* One 32-byte animation channel of a model instance; +00 fetches its next
 * command word. */
typedef struct {
    s32 (*fetch)(void);
    u8 unk04[0x20 - 0x04];
} FieldAnimChannel;

/* A model instance's animation channels (instance +14). */
typedef struct FieldAnimTable {
    u8 unk00[0xC];
    s32 count;                  /* 0C */
    FieldAnimChannel *channels; /* 10 */
} FieldAnimTable;

#endif
