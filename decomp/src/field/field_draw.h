#ifndef FIELD_FIELD_DRAW_H
#define FIELD_FIELD_DRAW_H

/* The field frame's drawing (field.c): the draw blocks' switch, the model
 * and sprite passes, the compass and pointer markers, the screen fades, the
 * overlay sprites and the sprite block of field_effect.c. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "field/monitor.h"
#include "field.h"

void field_draw_switch_block(void);      /* switch draw blocks, clear the overlay table */
void field_draw_swap_and_clear_ots(void);      /* swap the draw buffer, clear its tables */
void field_movie_run_overlay_frame(void);      /* finish the frame */

/* The model pass (800748e8): instances are culled by their bounding square
 * against the screen widened by the margins (the instances' refresh,
 * field_instance_refresh_bounds_modes, is in field/monitor.h). */
extern s32 field_model_cull_margin_x;         /* screen margin x */
extern s32 field_model_cull_margin_y;         /* screen margin y */
void field_draw_models(void);      /* draw the models */
s32 field_has_second_ot_instances(void);       /* whether a shown descriptor has flag 0x8000 */
void field_instance_set_bounds(FieldInstance *instance); /* bounds from the mesh */
s32 field_instance_is_off_screen(FieldInstance *instance);  /* 0 when on screen, else -1 */

/* The sprite pass and the sprite factory (80076ac0). */
extern s32 field_sprite_created_count;         /* sprites created */
void field_actor_create_sprite(s32 index, s32 slot, void *data, s32 kind, s32 bank, s32 unk, s32 flag);

/* A pointer marker: its quad's corners and primitive per buffer. */
typedef struct FieldMarker {
    SVECTOR v[4];
    POLY_FT4 poly[2];
} FieldMarker;

void field_shadow_init_quad(FieldMarker *marker); /* set a marker up */
void field_marker_project_and_link(u_long *ot, FieldMarker *marker, MATRIX *m, s32 buffer); /* project and link */
void field_marker_link_standing_sprite(u_long *ot, FieldMarker *marker, MATRIX *m, s32 buffer); /* as a standing sprite */

/* The compass (80074108). */
extern FieldMarker field_compass_markers[25]; /* ring, letters, needle and pointer quads */
extern u16 field_compass_row_octant_bits[8];      /* heading octant bit per palette row */
extern DVECTOR field_compass_letter_offsets[4];  /* letter x, z offsets */
extern s16 field_compass_needle_heading;         /* needle heading */
extern s16 field_compass_needle_goal;         /* needle goal */
extern u16 field_compass_colors[16];     /* compass colours read back from VRAM */
extern u16 field_compass_palette[128];    /* compass palette */
extern RECT field_compass_palette_rect;        /* compass colour strip */
void field_compass_build_quadrant_quads(void);      /* build the four letters */
void field_compass_build_grid_quad(FieldMarker *record, s32 column, s32 row, s32 style); /* build a grid quad */

/* Screen fades: two channels in the work block (FadeChannel); a fade starts
 * with field_fade_start (field/monitor.h). */
extern s32 field_fade_mode;         /* fade mode; fades start only in mode 2 */
extern s16 field_faded_out;         /* fade started */
extern DR_MODE field_menu_fade_draw_modes[2];  /* fade draw mode per buffer */
extern RECT field_fade_texture_windows[2];     /* fade texture windows */
extern RECT field_menu_fade_source_rect;        /* fade copy source */
extern TILE field_menu_fade_tiles[2];     /* fade tile per buffer */
void field_fade_init_channels(void);          /* set both channels' primitives up */
void field_fade_init_channel(s32 channel);   /* prepare a channel */
void field_fade_out(s32 steps);     /* fade channel 0 out to white, once */
void field_fade_in(s32 steps);     /* fade it back in, once */
void field_fade_update_channels(void *ot, s32 buffer); /* step and draw both channels */
void field_fade_link_channels(u_long *ot, s32 buffer); /* link the active channels */

/* The five overlay sprites (800abd18), per sprite and draw buffer. */
typedef struct OverlaySprites {
    DR_MODE modes[5][2];
    SPRT sprites[5][2];
} OverlaySprites;

extern OverlaySprites field_wide_overlay_sprites;
void field_wide_overlay_init(void);      /* set them up */
void field_wide_overlay_draw(void);      /* link the current buffer's */

/* The sprite block (800aac08): 33 sprites, each with a draw mode, per draw
 * buffer. */
typedef struct FieldSprites {
    DR_MODE modes[33][2];
    SPRT sprites[33][2];
} FieldSprites;

extern FieldSprites *field_overlay_sprites;
void field_overlay_sprite_alloc_all(void);      /* allocate it */
void field_overlay_sprite_release_all(void);      /* release it */
void field_overlay_sprite_set_color(s32, s32, s32, s32); /* set a sprite's colour */
void field_overlay_sprite_place(s32 index, s32 x, s32 y, s32 anchor); /* place and link a sprite */

#endif
