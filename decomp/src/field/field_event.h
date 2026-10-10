#ifndef FIELD_FIELD_EVENT_H
#define FIELD_FIELD_EVENT_H

/* The event interpreter (field_event.c): the event package and bytecode,
 * the event variable bank, the current actor, the operand readers and the
 * instruction helpers, the state some instructions keep, and the parameters
 * events set for the panorama. */

#include "common.h"
#include "psyq/libgte.h"
#include "field/monitor.h"
#include "field.h"

/* The event package (field component 5, *800adbf8). */
typedef struct {
    u32 unsigned_bits[32]; /* 00: bit per variable read unsigned */
    s32 count;             /* 80: actors */
    u16 entries[32];       /* 84: 32 event entry PCs per actor */
} EventPackage;

extern EventPackage *field_event_package;
extern u8 *field_event_bytecode;                        /* event bytecode */
extern void (*field_event_extended_handlers[])(void);   /* extended event instructions */
extern FieldActor *field_current_event_actor;           /* current event actor */
extern s32 field_current_event_actor_index;             /* current actor index */
extern FieldDescriptor *field_current_event_descriptor; /* descriptor of the running actor */
extern s32 field_event_yield_ends_run;
extern s32 field_event_yield_requested;                 /* yield */
extern s32 field_event_batch_limit;                     /* batch limit */

s32 field_event_run_all_actors(void);                     /* run every active actor's script for this frame */
s32 field_event_run_instructions(s32 limit);              /* run the current actor's instructions */
s32 field_event_get_entry_pc(s32 actor, s32 event);       /* entry PC of an actor's event */
s32 field_event_has_slot_tag(FieldActor *actor, s32 tag); /* -1 when a slot carries `tag` */

/* Variables: a reference is a byte offset into the bank (the actor count,
 * the bank and its read, field_event_read_variable, are in field/monitor.h). */
void field_event_write_variable(s32 reference, s32 value); /* write */
s32 field_event_is_variable_unsigned(s32 reference);       /* -1 when read unsigned */

/* Operand readers; each takes the byte offset from the working PC. */
#define EVENT_OPERAND_BYTE(offset) (field_event_bytecode[field_current_event_actor->pc + (offset)])
s32 field_event_read_s16(s32 offset);                   /* signed halfword */
s32 field_event_read_u16(s32 offset);                   /* raw halfword */
s32 field_event_read_imm_or_var(s32 offset);            /* bit 15 immediate, else a variable */
s32 field_event_read_actor_index_or_leader(s32 offset); /* actor selector */
s32 field_event_read_actor_index(s32 offset);           /* actor selector; 0xff when none */
s32 field_event_resolve_character(s32 id);              /* resolve a character id (fd-ff: party members) */
/* Selected operands: when the given bit of `flags` is set the operand is a
 * signed immediate halfword, otherwise a variable reference. */
s32 field_event_read_selected_operand_80(s32 offset, s32 flags); /* bit 0x80 */
s32 field_event_read_selected_operand_40(s32 offset, s32 flags); /* bit 0x40 */
s32 field_event_read_selected_operand_20(s32 offset, s32 flags); /* bit 0x20 */
s32 field_event_read_selected_operand_10(s32 offset, s32 flags); /* bit 0x10 */
s32 field_event_read_selected_operand_08(s32 offset, s32 flags); /* bit 0x08 */
s32 field_event_read_selected_operand_04(s32 offset, s32 flags); /* bit 0x04 */
s32 field_event_read_selected_operand_02(s32 offset, s32 flags); /* bit 0x02 */
s32 field_event_read_selected_operand_01(s32 offset, s32 flags); /* bit 0x01 */

/* Instruction helpers (the random picks, field_encounter_draw_steps, are in
 * field/monitor.h). */
void field_event_branch_unless_actor_flags(s32 flags); /* continue past 5 bytes when `flags` has a bit of op1, else jump */
void field_event_branch_unless_flags(s32 flags);       /* the same past 4 bytes */
void field_event_store_flags(s32 value);               /* store `value` in variable op1 */
void field_event_set_animation_complement(void);       /* event fe 4d: the current actor's animation override */
void field_event_record_departure(void);               /* publish the field id in variables 4, 6 and 8 */
void field_event_show_first_sprite(void);              /* give the current actor the field's first sprite */
extern u32 *field_event_loaded_tim;                    /* TIM image held by instruction 0x77 */

/* An actor's boundary quadrilateral (+114 while state bit 12 is set). */
typedef struct {
    s16 x;
    s16 z;
} BoundaryCorner;

typedef struct {
    BoundaryCorner corners[4];
} ActorBoundary;

/* An actor's integer position cached at +68 (x, y, z). */
#define ACTOR_CACHED_POSITION(actor) ((s16 *)((u8 *)(actor) + 0x68))

/* The pieces' accumulated drift (x, y, z) at 800b21bc. */
#define PIECE_DRIFT_TOTAL ((s32 *)field_work.unk21BC)

/* The actor list (+118) the morph channels read through 80080a18: whose
 * list and where. */
extern s32 field_morph_list_descriptor;         /* descriptor whose list is read */
extern s32 field_morph_list_index;              /* list position */

/* Parameters events set at 800b0080, one object: the panorama's (resident
 * 8002709c), built by the field load when `enabled`. */
typedef struct FieldEventParams {
    s16 unk80[8]; /* 800b0080 */
    s32 unk90;    /* 800b0090 */
    s32 unk94;    /* 800b0094 */
    s32 unk98;    /* 800b0098 */
    u8 unk9C[4];
    u8 unkA0[3];  /* 800b00a0 */
    u8 unkA3;
    u8 unkA4[3];  /* 800b00a4 */
    u8 unkA7;
    u8 unkA8[3];  /* 800b00a8 */
    u8 unkAB;
    s16 unkAC;    /* 800b00ac */
    s16 unkAE;    /* 800b00ae */
    s16 unkB0;    /* 800b00b0 */
    s16 enabled;  /* 800b00b2 */
} FieldEventParams;

extern FieldEventParams field_panorama_parameters;

#endif
