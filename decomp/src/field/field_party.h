#ifndef FIELD_FIELD_PARTY_H
#define FIELD_FIELD_PARTY_H

/* The party on the field: the members' slots and sprites, joins and points
 * (field_800854D0.c), and the slot swaps in which a party member (8005a444)
 * exchanges its model with the event actor standing in for its slot
 * (8006f990; field_800A9274.c). */

#include "common.h"

extern u8 D_800AE294[];        /* sprite of each character */
extern s16 D_800AEA2C[4];      /* party masks */
extern s16 D_800AFD20;         /* the party member an actor took the sprite of */

s32 func_8009FA00(s32 character);         /* party slot of a character, or -1 */
s32 func_8008A790(s32 id, s32 *slot);     /* a free slot of 80062590 for `id` */
s32 func_8008DB68(s32 member, s32 amount); /* add to a member's points */
s32 func_8008DBF0(s32 member, s32 amount); /* take from a member's points */
void func_800A30B4(void);                 /* store the members in variables 3e-42 */
void func_800A0158(s32 slot, s32 *a, s32 *b, s32 *c); /* a slot's variable triple */
s32 func_8009FEE4(s32 slot);              /* record a slot's map and position */
void func_800931F8(void);

/* Gathering the party at the controlled actor. */
extern s16 D_800AEA34[8];      /* heading per direction (8009aee0) */
void func_80077268(void);      /* place the party at the controlled actor */

/* A party member's sprite data (D_8005A414 per slot), copied whole. */
typedef struct {
    s32 data[0x14000 / 4];
} PartySprite;

/* A member's sprite file read for a slot (8008a7dc), then its join event
 * (8008b978). */
extern void *D_800ADBC0;       /* pending party sprite buffer */
extern s32 D_800ADBC4;         /* a party sprite load is pending, 0xff none */
extern s32 D_800ADBC8;         /* its member */
extern s32 D_800ADBCC;         /* its slot */
void func_8008A7DC(s32 member, s32 slot);
void func_8008B978(s32 member);

/* Slot swaps. */
void func_800A0524(s32 actor, s32 member); /* copy an actor's position state to another */
void func_800AD4D4(s32 slot);  /* put the current actor in for a slot */
void func_800ACFD0(s32 slot);  /* return a slot to its member */
void func_800ACE24(void);      /* mark the slots that changed character, refresh */
void func_800ACE90(void);      /* flag the slots whose members are present */
void func_800AD898(void);
void func_800A2488(void);      /* rebuild the party (mode 3) */

#endif
