#ifndef FIELD_FIELD_PARTY_H
#define FIELD_FIELD_PARTY_H

#include "field.h"

/* Party slot swaps: a party member (8005a444) exchanges its model with the
 * event actor standing in for its slot (8006f990). */

s32 func_8009FEE4(s32 slot);
void func_800A0524(s32 actor, s32 member);

#endif
