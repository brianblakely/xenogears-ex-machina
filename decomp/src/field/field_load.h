#ifndef FIELD_FIELD_LOAD_H
#define FIELD_FIELD_LOAD_H

/* The field load and teardown (80070cc8, 800700b0, field.c): the map bundle
 * read ahead of the load, the sprite slots, the trigger zones, the texture
 * scrolls the events add and the panorama backdrop. */

#include "common.h"
#include "resident/gpu.h"
#include "resident/mode.h"

/* One sprite slot's VRAM area; slots with `shared` set keep their image. */
typedef struct {
    u16 x;
    u16 y;
    u16 unk4;
    s16 shared;
} SpriteSlot;

typedef struct SpriteSlotTable {
    SpriteSlot slot[32];
} SpriteSlotTable;

/* Components of the map bundle: sizes at +10c and offsets at +130. */
enum {
    BUNDLE_PALETTES,
    BUNDLE_COLLISION,
    BUNDLE_MODELS,
    BUNDLE_SPRITES,
    BUNDLE_IMAGES,
    BUNDLE_EVENTS,
    BUNDLE_ENCOUNTERS, /* the encounter set and its weights (resident/formation.h) */
    BUNDLE_MESSAGES,   /* the field message table (field_message_table) */
    BUNDLE_ZONES
};

/* The map bundle read ahead of the field load (*8005a4e0). */
typedef struct {
    SpriteSlotTable slots; /* 000 */
    u8 unk100[0x10C - 0x100];
    s32 sizes[9];          /* 10C */
    s32 offsets[9];        /* 130 */
    s16 view[0x1C];        /* 154: lights and background (8006fdec) */
    u16 descriptor_count;  /* 18C */
    u16 unk18E;
    u16 descriptors[1];    /* 190: flags, rotation[3], position[3], model */
} FieldBundle;

/* The map bundle as the field load reads it (mode.h's block mode_read_ahead_block). */
#define FIELD_BUNDLE ((FieldBundle *)mode_read_ahead_block)

/* The size and the data of component k, read as words at their byte offsets
 * in the header (the field load reads them so, not as struct members). */
#define BUNDLE_SIZE(k) (*(s32 *)((u8 *)mode_read_ahead_block + 0x10C + (k) * 4))
#define BUNDLE_COMPONENT(k) ((void *)(*(s32 *)((u8 *)mode_read_ahead_block + 0x130 + (k) * 4) + (s32)mode_read_ahead_block))

extern SpriteSlotTable field_sprite_slots; /* the bundle's sprite slots, kept by the load */
extern s32 field_unread_collision_attribute_count;         /* attributes before the first triangle */
extern u32 field_heap_top;         /* heap top */

void field_load_from_bundle(void);      /* load the field from the bundle */
void field_teardown(void);      /* tear the field down */

/* The map's own stream (file 0xb9 + 2 * map) in a four-sector ring. */
extern s32 field_map_stream_running;         /* the stream is running */
extern void *field_map_stream_ring;       /* its ring */
void field_map_stream_start(void);      /* start it unless one runs */
void field_map_stream_stop(void);      /* stop it and release the ring */

/* Trigger zone (field component 8): four x, y, z corners. */
typedef struct {
    s16 x;
    s16 y;
    s16 z;
} ZonePoint;

typedef struct {
    ZonePoint corner[4];
} Zone;

extern Zone *field_trigger_zones;       /* trigger zones */

/* Up to 32 texture scrolls (80027d64) created by field_event_add_texture_scroll. */
typedef struct WindowList {
    s16 count;
    TextureScroll *scrolls[32];
    u8 *buffers[32];
    s16 lengths[32];
} WindowList;

extern WindowList field_texture_scrolls;
void field_run_texture_scrolls(void);      /* run every scroll (80027eac) */

/* The panorama backdrop (resident 8002709c) built from the parameters events
 * set (field_panorama_parameters, field_event.h). */
extern Panorama *field_panorama;

#endif
