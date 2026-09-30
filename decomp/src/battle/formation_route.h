#ifndef BATTLE_FORMATION_ROUTE_H
#define BATTLE_FORMATION_ROUTE_H

#include "battle_core.h"

/* A point of an approach route (6 bytes). */
typedef struct {
    u16 x;
    u16 z;
    u8 flag; /* the formation point's bit 0x80 */
    u8 pad5;
} RoutePoint;

/* The approach route (800c48ec): the actor's position, then up to seven
 * formation points; unused points are 0xFFFF. */
extern RoutePoint D_800C48EC[9];

#endif
