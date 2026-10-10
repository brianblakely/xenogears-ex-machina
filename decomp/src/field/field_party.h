#ifndef FIELD_FIELD_PARTY_H
#define FIELD_FIELD_PARTY_H

/* The party on the field: the members' slots and sprites, joins and points
 * (field_event.c), and the slot swaps in which a party member (8005a444)
 * exchanges its model with the event actor standing in for its slot
 * (8006f990; field_effect.c). */

#include "common.h"

extern u8 field_character_sprite_ids[];         /* sprite of each character */
extern s16 field_event_party_slot_masks[4];     /* party masks */
extern s16 field_unread_party_sprite_take_mark; /* the party member an actor took the sprite of */

s32 field_party_find_character_slot(s32 character);                    /* party slot of a character, or -1 */
s32 field_party_find_free_slot(s32 id, s32 *slot);                     /* a free slot of 80062590 for `id` */
s32 field_party_add_gear_hp(s32 member, s32 amount);                   /* add to a member's points */
s32 field_party_take_gear_hp(s32 member, s32 amount);                  /* take from a member's points */
void field_event_store_party_members(void);                            /* store the members in variables 3e-42 */
void field_party_read_slot_position(s32 slot, s32 *a, s32 *b, s32 *c); /* a slot's variable triple */
s32 field_party_record_slot_position(s32 slot);                        /* record a slot's map and position */

/* Gathering the party at the controlled actor. */
extern s16 field_event_direction_table[8];      /* heading per direction (8009aee0) */
void field_party_place_at_controlled(void);     /* place the party at the controlled actor */

/* A party member's sprite data (mode_party_sprite_blocks per slot), copied whole. */
typedef struct {
    s32 data[0x14000 / 4];
} PartySprite;

/* A member's sprite file read for a slot (8008a7dc), then its join event
 * (8008b978). */
extern void *field_party_sprite_load_buffer;       /* pending party sprite buffer */
extern s32 field_party_sprite_load_pending;        /* a party sprite load is pending, 0xff none */
extern s32 field_party_sprite_load_member;         /* its member */
extern s32 field_party_sprite_load_slot;           /* its slot */
void field_party_read_member_sprite(s32 member, s32 slot);
void field_party_run_join_event(s32 member);

/* Slot swaps. */
void field_actor_copy_position_state(s32 actor, s32 member); /* copy an actor's position state to another */
void field_party_board_gear(s32 slot);                       /* put the current actor in for a slot */
void field_party_leave_gear(s32 slot);                       /* return a slot to its member */
void field_party_apply_gear_changes(void);                   /* mark the slots that changed character, refresh */
void field_party_toggle_gear_riding(void);                   /* flag the slots whose members are present */
void field_party_set_gear_rider_flags(void);
void field_event_rebuild_party(void);                        /* rebuild the party (mode 3) */

#endif
