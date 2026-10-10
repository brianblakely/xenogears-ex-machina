/* arena_scene_graph_and_opponent: text 800891C0-80090F38, rodata 800706E8-800707A8, data
 * 80091C0C-800925D4, variables 80092800-8009284C and 80096D88-80096FA8.
 * The display and its layers, the 3D scene graph (nodes, models, model
 * sets, lights, animation players, instances and meshes), the task switch
 * (handwritten, 8008BB00-8008BCC8), the spark emitters, the glow field,
 * positional sound and the computer opponent. Its jump tables lie at 0 mod
 * 8 (800706E8-80070748) after arena_mode_entry's strings. Its variables place its
 * start after arena_mode_entry's last reader of theirs (80088E90) and at or before
 * 8008A040, the first reader of its own; it is kept where the file and
 * display code starts (800891C0). Its .bss opens with the task switch's
 * two words, and its data ends with the embedded sprite model arena_actor_extra_model
 * and the combo inputs arena_actor_combo_inputs. */
#include "common.h"
#include "psyq/inline_c.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "actor.h"
#include "bout.h"
#include "brain.h"
#include "display.h"
#include "glow.h"
#include "helpers.h"
#include "mode.h"
#include "node.h"
#include "resident_views.h"
#include "sound.h"
#include "spark.h"
#include "stage.h"
#include "task.h"

/* The unit's small uninitialized variables, zero in the file after every
 * unit's data, each in a slot of whole words (decomp/Makefile). */
static s16 arena_node_override_tpage_x; /* 80092800: model texture page x (-1: none) */
static s16 arena_node_override_tpage_y; /* 80092804: model texture page y */
static s16 arena_node_override_clut_x; /* 80092808: model CLUT x (-1: none) */
static s16 arena_node_override_clut_y; /* 8009280C: model CLUT y */
static s32 arena_node_color_overrides_on; /* 80092810 */
static s32 arena_node_unused_word; /* 80092814: unreferenced */
static CVECTOR arena_node_buffer_colors[2]; /* 80092818: current colour per display buffer */
static s32 arena_display_frame_start_time; /* 80092820: root counter at the frame start */
static s32 arena_node_unread_instanced_count; /* 80092824: nodes instanced by the last copy */
static Node *arena_node_instanced_root; /* 80092828: root being instanced */
static SVECTOR *arena_spark_gte_scratch; /* 8009282C: scratch vectors for GTE loads */
static SVECTOR *arena_spark_view_origin; /* 80092830: view origin subtracted before projection */
static Emitter *arena_spark_burst_emitter; /* 80092834: the menu's spark emitter */
static s32 arena_spark_burst_strength; /* 80092838: spark burst strength, fading by 4 per frame */
static s16 *arena_glow_old_field; /* 8009283C */
static s16 *arena_glow_new_field; /* 80092840 */
static u8 *arena_glow_image; /* 80092844 */
static u8 arena_brain_last_command; /* 80092848: the command the brain last started */

/* Its larger ones, past the program's end (not in the file), each unit's
 * after every unit's small ones (menu.mk), behind the task scheduler's
 * two words (arena_task_save_scheduler.s). Nothing addresses the words marked
 * unreferenced. */
static POLY_FT4 arena_glow_quads[2];       /* 80096D90: glow field quad per draw buffer */
static TILE arena_glow_shade_tiles[2];     /* 80096DE0: full-screen shade tile per draw buffer */
static DR_MODE arena_glow_blend_modes[2];  /* 80096E00: its blend mode per draw buffer */
static s32 arena_glow_unused_words[34];    /* 80096E18: unreferenced */
static SoundVoice arena_sound_voices[4];   /* 80096EA0 */
static Brain arena_brain_for_first_actor;  /* 80096F30: brain of the side-0 opponent */
static Brain arena_brain_for_second_actor; /* 80096F64: brain of the side-1 opponent */
static VECTOR arena_look_at_axis_z;        /* 80096F98: look-at work: side */

MATRIX arena_identity_matrix = { { { 0x1000, 0, 0 }, { 0, 0x1000, 0 }, { 0, 0, 0x1000 } }, { 0, 0, 0 } }; /* 80091C0C */

s32 arena_node_players_share_keys = 0; /* 80091C2C */

OtPair *arena_display_layer_to_compact = NULL; /* 80091C30 */

/* Unused: the corners of a cube. */
SVECTOR arena_node_unused_cube_corners[8] = { /* 80091C34 */
    { -0x1000, -0x1000, -0x1000, 0 }, { 0x1000, -0x1000, -0x1000, 0 },
    { 0x1000, 0x1000, -0x1000, 0 }, { -0x1000, 0x1000, -0x1000, 0 },
    { -0x1000, -0x1000, 0x1000, 0 }, { 0x1000, -0x1000, 0x1000, 0 },
    { 0x1000, 0x1000, 0x1000, 0 }, { -0x1000, 0x1000, 0x1000, 0 },
};

/* Spark shapes: setup, reset and draw of a spark record, and its size. */
void arena_spark_init_line4();
void arena_spark_reset_line4();
void arena_spark_draw_line4();
void arena_spark_init_line3();
void arena_spark_reset_line3();
void arena_spark_draw_line3();
void arena_spark_init_line2();
void arena_spark_reset_line2();
void arena_spark_draw_line2();
void arena_spark_init_tile();
void arena_spark_reset_tile();
void arena_spark_draw_tile();
void arena_spark_init_dot();
void arena_spark_reset_dot();
void arena_spark_draw_dot();
SparkShape arena_spark_shapes[] = { /* 80091C74 */
    { arena_spark_init_line4, arena_spark_reset_line4, arena_spark_draw_line4, 0x60 },
    { arena_spark_init_line3, arena_spark_reset_line3, arena_spark_draw_line3, 0x50 },
    { arena_spark_init_line2, arena_spark_reset_line2, arena_spark_draw_line2, 0x38 },
    { arena_spark_init_tile, arena_spark_reset_tile, arena_spark_draw_tile, 0x30 },
    { arena_spark_init_dot, arena_spark_reset_dot, arena_spark_draw_dot, 0x28 },
};

/* Spark placement rules. */
void arena_spark_place_at_origin(Emitter *source, SVECTOR *pos);
void arena_spark_place_in_rotated_box(Emitter *source, SVECTOR *pos);
void arena_spark_place_in_box(Emitter *source, SVECTOR *pos);
void arena_spark_place_in_rotated_rect(Emitter *source, SVECTOR *pos);
void arena_spark_place_in_rotated_ellipse(Emitter *source, SVECTOR *pos);
void arena_spark_place_on_rotated_ring(Emitter *source, SVECTOR *pos);
void (*arena_spark_placement_rules[])(Emitter *emitter, SVECTOR *pos) = { /* 80091CC4 */
    arena_spark_place_at_origin, arena_spark_place_in_rotated_rect, arena_spark_place_in_rotated_box, arena_spark_place_in_rotated_ellipse, arena_spark_place_on_rotated_ring, arena_spark_place_in_box,
};

void arena_spark_bounce_on_floor(Spark *spark);
void (*arena_spark_update_callbacks[1])(Spark *spark) = { arena_spark_bounce_on_floor }; /* 80091CDC */

/* Glow palette. */
u16 arena_glow_palette[256] = { /* 80091CE0 */
    0x0000, 0x0000, 0x0C00, 0x0C00, 0x1000, 0x1000, 0x1000, 0x1400,
    0x1401, 0x1002, 0x1003, 0x1004, 0x0C05, 0x0C06, 0x0C07, 0x0808,
    0x0809, 0x080A, 0x080B, 0x040C, 0x040D, 0x040E, 0x000F, 0x0010,
    0x0010, 0x0010, 0x0011, 0x0011, 0x0012, 0x0012, 0x0012, 0x0013,
    0x0013, 0x0014, 0x0014, 0x0014, 0x0015, 0x0015, 0x0016, 0x0016,
    0x0017, 0x0017, 0x0038, 0x0038, 0x0039, 0x0039, 0x005A, 0x005A,
    0x005B, 0x005B, 0x007C, 0x007C, 0x007D, 0x007D, 0x009E, 0x009E,
    0x009F, 0x009F, 0x00BF, 0x00BF, 0x00BF, 0x00BF, 0x00DF, 0x00DF,
    0x00DF, 0x00DF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x011F, 0x011F,
    0x011F, 0x011F, 0x013F, 0x013F, 0x013F, 0x013F, 0x015F, 0x015F,
    0x015F, 0x015F, 0x017F, 0x017F, 0x017F, 0x019F, 0x019F, 0x019F,
    0x019F, 0x01BF, 0x01BF, 0x01BF, 0x01BF, 0x01DF, 0x01DF, 0x01DF,
    0x01DF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x021F, 0x021F, 0x021F,
    0x021F, 0x023F, 0x023F, 0x023F, 0x023F, 0x025F, 0x025F, 0x025F,
    0x027F, 0x027F, 0x027F, 0x027F, 0x029F, 0x029F, 0x029F, 0x029F,
    0x02BF, 0x02BF, 0x02BF, 0x02BF, 0x02DF, 0x02DF, 0x02DF, 0x02DF,
    0x02FF, 0x02FF, 0x02FF, 0x02FF, 0x031F, 0x031F, 0x031F, 0x031F,
    0x033F, 0x033F, 0x033F, 0x035F, 0x035F, 0x035F, 0x035F, 0x035F,
    0x035F, 0x035F, 0x035F, 0x035F, 0x037F, 0x037F, 0x037F, 0x037F,
    0x037F, 0x037F, 0x037F, 0x037F, 0x037F, 0x039F, 0x039F, 0x039F,
    0x039F, 0x039F, 0x039F, 0x039F, 0x039F, 0x039F, 0x03BF, 0x03BF,
    0x03BF, 0x03BF, 0x03BF, 0x03BF, 0x03BF, 0x03BF, 0x03DF, 0x03DF,
    0x03DF, 0x03DF, 0x03DF, 0x03DF, 0x03DF, 0x03DF, 0x03DF, 0x03FF,
    0x03FF, 0x03FF, 0x03FF, 0x03FF, 0x03FF, 0x07FF, 0x07FF, 0x0BFF,
    0x0BFF, 0x0FFF, 0x0FFF, 0x13FF, 0x13FF, 0x17FF, 0x17FF, 0x17FF,
    0x1BFF, 0x1BFF, 0x1FFF, 0x1FFF, 0x23FF, 0x23FF, 0x27FF, 0x27FF,
    0x2BFF, 0x2BFF, 0x2BFF, 0x2FFF, 0x2FFF, 0x33FF, 0x33FF, 0x37FF,
    0x37FF, 0x3BFF, 0x3BFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x43FF, 0x43FF,
    0x47FF, 0x47FF, 0x4BFF, 0x4BFF, 0x4FFF, 0x4FFF, 0x53FF, 0x53FF,
    0x57FF, 0x57FF, 0x57FF, 0x5BFF, 0x5BFF, 0x5FFF, 0x5FFF, 0x63FF,
    0x63FF, 0x67FF, 0x67FF, 0x6BFF, 0x6BFF, 0x6BFF, 0x6FFF, 0x6FFF,
    0x73FF, 0x73FF, 0x77FF, 0x77FF, 0x7BFF, 0x7BFF, 0x7FFF, 0x7FFF,
};

/* Command sounds: two effect ids (0: none) per command. */
u8 arena_sound_command_effect_pairs[128] = { /* 80091EE0 */
    0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00,
    0x08, 0x00, 0x09, 0x00, 0x0A, 0x00, 0x0B, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x0E, 0x00, 0x0F, 0x00,
    0x10, 0x00, 0x11, 0x00, 0x12, 0x00, 0x13, 0x14, 0x15, 0x16, 0x17, 0x00, 0x18, 0x00, 0x19, 0x00,
    0x1A, 0x00, 0x1B, 0x00, 0x1C, 0x1D, 0x1E, 0x00, 0x1F, 0x00, 0x20, 0x00, 0x21, 0x00, 0x22, 0x00,
    0x23, 0x00, 0x24, 0x00, 0x25, 0x00, 0x26, 0x00, 0x27, 0x00, 0x28, 0x29, 0x2D, 0x00, 0x2E, 0x00,
    0x2F, 0x00, 0x30, 0x00, 0x31, 0x00, 0x32, 0x00, 0x33, 0x00, 0x34, 0x00, 0x3A, 0x00, 0x3B, 0x00,
    0x3C, 0x00, 0x3D, 0x00, 0x3E, 0x00, 0x3F, 0x00, 0x40, 0x00, 0x41, 0x00, 0x42, 0x00, 0x43, 0x00,
    0x44, 0x00, 0x45, 0x00, 0x46, 0x00, 0x47, 0x00, 0x48, 0x00, 0x49, 0x00, 0x4A, 0x00, 0x4B, 0x00,
};

/* Command sound tables, chosen per model (Actor sounds). */
u8 arena_sound_model29_command_sounds[16] = { /* 80091F60 */
    0x00, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x2F, 0xFF, 0x30, 0x34, 0x23, 0x36, 0x37, 0x38, 0x33,
};
u8 arena_sound_model36_command_sounds[16] = { /* 80091F70 */
    0x00, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0xFF, 0xFF, 0xFF, 0x02, 0x23, 0x36, 0x37, 0xFF, 0x33,
};
u8 arena_sound_model27_command_sounds[16] = { /* 80091F80 */
    0x00, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x39, 0xFF, 0x3A, 0x34, 0x23, 0x36, 0x37, 0x38, 0x33,
};
u8 arena_sound_default_command_sounds[16] = { /* 80091F90 */
    0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0x11, 0xFF, 0x02, 0x23, 0x17, 0x25, 0x24, 0x3F,
};
u8 arena_sound_model9_command_sounds[16] = { /* 80091FA0 */
    0x00, 0x0C, 0x0C, 0x0D, 0x0D, 0x0E, 0x0E, 0x0A, 0xFF, 0x0B, 0x10, 0x23, 0x17, 0x2C, 0x24, 0x3D,
};
/* An embedded sprite model, user-supplied (menu.classification.txt). */
INCLUDE_ASSET(".data", arena_actor_extra_model, 0x80091FB0, 0x5F4);
/* Each combo's command inputs (1 A, 2 B), by special move, ending the data:
 * stray bytes ("ind") that nothing reads follow it, so it stays original. */
INCLUDE_ORIGINAL(".data", arena_actor_combo_inputs, 0x800925A4, 0x30);

/* 800891C0: Load a whole file into a new allocation and return it. */
void *arena_load_whole_file(s32 file) {
    void *data = heap_alloc(cd_get_file_size(file), 1);

    cd_read_file(file, data, 0, 0);
    return data;
}

/* 80089210: Set the screen scale matrices for a width x height display (320x240 is
 * unit scale). */
void arena_display_set_screen_scale(s32 width, s32 height) {
    s32 sx = ((width << 12) / 320) * height / width;
    s32 sy = ((height << 12) / 240) * height / width;

    arena_display_unread_identity = arena_identity_matrix;
    arena_display_screen_scale = arena_identity_matrix;
    arena_display_screen_scale.m[0][0] = sx;
    arena_display_screen_scale.m[1][1] = sy;
}

/* 80089330: Set both buffers' display environments and background tiles for the
 * resolution; taller than 256 lines is interlaced. */
void arena_display_set_disp_envs(s32 width, s32 height) {
    if (height > 256) {
        SetDefDispEnv(&arena_display_buffers[0].disp, 0, 0, width, height);
        SetDefDispEnv(&arena_display_buffers[1].disp, 0, 0, width, height);
        arena_display_buffers[1].disp.isinter = 1;
        arena_display_buffers[0].disp.isinter = 1;
        arena_display_buffers[0].disp.screen.x = 0;
        arena_display_buffers[0].disp.screen.y = 0x10;
        arena_display_buffers[0].disp.screen.w = 0x100;
        arena_display_buffers[0].disp.screen.h = 0xD4;
        arena_display_buffers[1].disp.screen.x = 0;
        arena_display_buffers[1].disp.screen.y = 0x10;
        arena_display_buffers[1].disp.screen.w = 0x100;
        arena_display_buffers[1].disp.screen.h = 0xD4;
    } else {
        SetDefDispEnv(&arena_display_buffers[0].disp, 0, 0x100, width, height);
        SetDefDispEnv(&arena_display_buffers[1].disp, 0, 0, width, height);
        arena_display_buffers[1].disp.isinter = 0;
        arena_display_buffers[0].disp.isinter = 0;
        arena_display_buffers[0].disp.screen.x = 0;
        arena_display_buffers[0].disp.screen.y = 0xA;
        arena_display_buffers[0].disp.screen.w = 0x100;
        arena_display_buffers[0].disp.screen.h = height;
        arena_display_buffers[1].disp.screen.x = 0;
        arena_display_buffers[1].disp.screen.y = 0xA;
        arena_display_buffers[1].disp.screen.w = 0x100;
        arena_display_buffers[1].disp.screen.h = height;
    }
    arena_display_width = width;
    arena_display_height = height;
    arena_display_set_screen_scale(arena_display_width, arena_display_height);
    setlen(&arena_display_buffers[0].background, 3);
    *(u32 *)&arena_display_buffers[0].background.r0 = 0x60000000; /* black, TILE */
    arena_display_buffers[0].background.x0 = 0;
    arena_display_buffers[0].background.y0 = 0;
    arena_display_buffers[0].background.w = width;
    arena_display_buffers[0].background.h = height;
    arena_display_buffers[1].background = arena_display_buffers[0].background;
}

/* 80089534: Set the geometry and both buffers' drawing environments for the
 * resolution. */
void arena_display_set_draw_envs(s32 width, s32 height) {
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(0x180);
    model_set_screen_bounds(width, height);
    if (height > 256) {
        SetDefDrawEnv(&arena_display_buffers[0].draw, 0, 0, width, height);
        SetDefDrawEnv(&arena_display_buffers[1].draw, 0, 0, width, height);
        arena_display_buffers[1].draw.dfe = 0;
        arena_display_buffers[0].draw.dfe = 0;
    } else {
        SetDefDrawEnv(&arena_display_buffers[0].draw, 0, 0, width, height);
        SetDefDrawEnv(&arena_display_buffers[1].draw, 0, 0x100, width, height);
    }
    arena_display_buffers[0].draw.dtd = arena_display_buffers[1].draw.dtd = 1;
    arena_display_buffers[0].draw.isbg = arena_display_buffers[1].draw.isbg = 0;
    arena_display_buffers[0].draw.tpage = arena_display_buffers[1].draw.tpage = GetTPage(0, 2, 0x280, 0);
    SetDrawEnv(&arena_display_buffers[0].draw.dr_env, &arena_display_buffers[0].draw);
    SetDrawEnv(&arena_display_buffers[1].draw.dr_env, &arena_display_buffers[1].draw);
    SetDrawArea(&arena_display_buffers[0].area, &arena_display_buffers[0].draw.clip);
    SetDrawArea(&arena_display_buffers[1].area, &arena_display_buffers[1].draw.clip);
    SetDrawOffset(&arena_display_buffers[0].offset, arena_display_buffers[0].draw.ofs);
    SetDrawOffset(&arena_display_buffers[1].offset, arena_display_buffers[1].draw.ofs);
}

/* 800896C4: Set up geometry, screen and scale for a width x height display. */
void arena_display_set_geometry(s32 width, s32 height) {
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen((width << 8) / width);
    model_set_screen_bounds(width, height);
    arena_display_set_screen_scale(width, height);
}

/* 8008973C: Re-apply the drawing environments at the current resolution. */
void arena_display_reapply_draw_envs(void) {
    arena_display_set_draw_envs(arena_display_width, arena_display_height);
}

/* 8008976C: Set the display and drawing environments for a resolution. */
void arena_display_set_resolution(s32 width, s32 height) {
    arena_display_set_disp_envs(width, height);
    arena_display_set_draw_envs(width, height);
}

/* 800897AC: Set a layer's drawing areas and offsets for both buffers (the second
 * buffer lies `second` lines lower) and its black background tiles. */
void arena_display_set_layer_area(OtPair *layer, s32 x, s32 y, s32 w, s32 h, s32 second) {
    RECT area;
    s16 offset[2];

    area.x = x;
    area.y = y;
    area.w = w;
    area.h = h;
    SetDrawArea(&layer->area[0], &area);
    area.y = y + second;
    SetDrawArea(&layer->area[1], &area);
    offset[0] = x;
    offset[1] = y;
    SetDrawOffset(&layer->offset[0], offset);
    offset[1] = y + second;
    SetDrawOffset(&layer->offset[1], offset);
    layer->tile[0].len = 3;
    layer->tile[0].rgbc = 0x60000000;
    layer->tile[0].x0 = 0;
    layer->tile[0].y0 = 0;
    layer->tile[0].w = w;
    layer->tile[0].h = h;
    layer->tile[1] = layer->tile[0];
    layer->flags |= 0xC;
}

/* 800898BC: Build a view matrix looking from eye to at with the given up vector. */
void arena_look_at_build_matrix(MATRIX *m, SVECTOR *eye, SVECTOR *at, SVECTOR *up) {
    arena_look_at_forward.vx = at->vx - eye->vx;
    arena_look_at_forward.vy = at->vy - eye->vy;
    arena_look_at_forward.vz = at->vz - eye->vz;
    arena_look_at_axis_y.vx = up->vx;
    arena_look_at_axis_y.vy = up->vy;
    arena_look_at_axis_y.vz = up->vz;
    VectorNormal(&arena_look_at_forward, &arena_look_at_axis_z);
    OuterProduct12(&arena_look_at_axis_y, &arena_look_at_axis_z, &arena_look_at_forward);
    VectorNormal(&arena_look_at_forward, &arena_look_at_axis_x);
    OuterProduct12(&arena_look_at_axis_z, &arena_look_at_axis_x, &arena_look_at_forward);
    VectorNormal(&arena_look_at_forward, &arena_look_at_axis_y);
    m->m[0][0] = arena_look_at_axis_x.vx;
    m->m[0][1] = arena_look_at_axis_x.vy;
    m->m[0][2] = arena_look_at_axis_x.vz;
    m->m[1][0] = arena_look_at_axis_y.vx;
    m->m[1][1] = arena_look_at_axis_y.vy;
    m->m[1][2] = arena_look_at_axis_y.vz;
    m->m[2][0] = arena_look_at_axis_z.vx;
    m->m[2][1] = arena_look_at_axis_z.vy;
    m->m[2][2] = arena_look_at_axis_z.vz;
    ApplyMatrix(m, eye, &arena_look_at_forward);
    MulMatrix2(&arena_display_screen_scale, m);
    m->t[0] = -arena_look_at_forward.vx;
    m->t[1] = -arena_look_at_forward.vy;
    m->t[2] = -arena_look_at_forward.vz;
}

/* 80089A98: Point the owner's view from eye toward target (eye kept as the last eye
 * position). */
void arena_node_aim_rig_camera(LightRig *view, VECTOR *position, VECTOR *focus) {
    SVECTOR up;
    SVECTOR from;
    SVECTOR origin;

    up.vy = 0x1000;
    up.vz = 0;
    up.vx = 0;
    arena_view_origin = *focus;
    from.vx = position->vx - focus->vx;
    from.vy = position->vy - focus->vy;
    from.vz = position->vz - focus->vz;
    origin.vz = 0;
    origin.vx = 0;
    origin.vy = 0;
    arena_look_at_build_matrix(&view->camera->view, &from, &origin, &up);
}

/* 80089B44: Reset a node: unlinked, no payload, zero position and angles, identity
 * matrices. */
Node *arena_node_reset(Node *node) {
    node->type = 0;
    node->parent = NULL;
    node->child = NULL;
    node->next = NULL;
    node->callback = NULL;
    node->data = NULL;
    node->position.vz = 0;
    node->position.vy = 0;
    node->position.vx = 0;
    node->angles.vz = 0;
    node->angles.vy = 0;
    node->angles.vx = 0;
    node->offset.vz = 0;
    node->offset.vy = 0;
    node->offset.vx = 0;
    node->unk6C = arena_identity_matrix;
    node->unk4C = node->unk6C;
    node->view = node->unk4C;
    return node;
}

/* 80089C54: Allocate a reset scene node. */
Node *arena_node_alloc(void) {
    heap_set_next_class(8);
    return arena_node_reset(heap_alloc(sizeof(Node), 0));
}

/* 80089C88: Append child as the last child of parent. */
void arena_node_add_child(Node *parent, Node *child) {
    Node *last;

    child->parent = parent;
    if (parent->child == NULL) {
        parent->child = child;
    } else {
        last = parent->child;
        while (last->next != NULL) {
            last = last->next;
        }
        last->next = child;
    }
}

/* 80089CD8: Unlink a node from its parent's child list. */
void arena_node_unlink(Node *node) {
    Node *parent;
    Node *first;
    Node *prev;

    if (node == NULL) {
        return;
    }
    parent = node->parent;
    if (parent == NULL) {
        return;
    }
    node->parent = NULL;
    first = parent->child;
    if (first == NULL) {
        return;
    }
    if (first == node) {
        parent->child = node->next;
    } else {
        prev = first;
        while (prev->next != node) {
            prev = prev->next;
        }
        prev->next = node->next;
    }
    node->next = NULL;
}

/* 80089D5C: Free a node, its children and following siblings, and its payload. */
void arena_node_free_tree(Node *node) {
    if (node == NULL) {
        return;
    }
    arena_node_free_tree(node->child);
    arena_node_free_tree(node->next);
    switch (node->type) {
    case 1:
        arena_node_free_model(node->data);
        break;
    case 2:
        arena_node_free_model_set(node->data);
        break;
    case 5:
        arena_node_free_instance(node->data);
        break;
    }
    if (node->data != NULL) {
        heap_free(node->data);
    }
    heap_free(node);
}

/* 80089E2C: Make a node a model node. */
void arena_node_set_model(Node *node, NodeModel *model) {
    node->data = model;
    node->type = 1;
}

/* 80089E3C: Make a node a type 3 node. */
void arena_node_set_type3(Node *node) {
    node->type = 3;
}

/* 80089E48: Make a node a type 4 node. */
void arena_node_set_type4(Node *node) {
    node->type = 4;
}

/* 80089E54: Make a node a model set node. */
void arena_node_set_model_set(Node *node, ModelSet *set) {
    node->data = set;
    node->type = 2;
}

/* 80089E64: Make a node a type 6 node. */
void arena_node_set_light(Node *node, void *data) {
    node->data = data;
    node->type = 6;
}

/* 80089E74: Allocate an empty model set payload at unit scale. */
ModelSet *arena_node_alloc_model_set(void) {
    ModelSet *set;

    heap_set_next_class(4);
    set = heap_alloc(sizeof(ModelSet), 0);
    set->scale[2] = 0x1000;
    set->scale[1] = 0x1000;
    set->scale[0] = 0x1000;
    set->nodes = NULL;
    set->players = NULL;
    return set;
}

/* 80089EB4: Free a model set's node table and players (and, when owned, the
 * players' keys). */
void arena_node_free_model_set(ModelSet *set) {
    s32 i;

    if (set->nodes != NULL) {
        heap_free(set->nodes);
    }
    if (set->players != NULL) {
        if (!arena_node_players_share_keys) {
            i = set->count;
            while (--i != -1) {
                if (set->players[i].header != NULL) {
                    heap_free(set->players[i].keys);
                }
            }
        }
        heap_free(set->players);
    }
}

/* 80089F8C: Reset a model payload: grey, nothing loaded. */
NodeModel *arena_node_reset_model(NodeModel *model) {
    model->unk4 = 1;
    model->flags = 0;
    model->packets[0] = NULL;
    model->packets[1] = NULL;
    model->morph = NULL;
    model->file = NULL;
    model->unk1C = 0;
    model->b = 0x40;
    model->g = 0x40;
    model->r = 0x40;
    return model;
}

/* 80089FC4: Allocate a reset model payload. */
NodeModel *arena_node_alloc_model(void) {
    heap_set_next_class(1);
    return arena_node_reset_model(heap_alloc(sizeof(NodeModel), 0));
}

/* 80089FF8: Release a model payload's resources. */
void arena_node_free_model(NodeModel *model) {
    if (model->packets[0] != NULL) {
        heap_delay_free(model->packets[0], 2);
    }
    model_free_owned_block((ModelBuffer *)model->file);
}

/* 8008A040: Pass the model texture page and CLUT positions to target, or zeros when
 * no texture page is set. */
void arena_node_upload_file_images(void *target) {
    if (arena_node_override_tpage_x > 0) {
        model_load_image_list(target, 1, arena_node_override_tpage_x, arena_node_override_tpage_y, 1, arena_node_override_clut_x, arena_node_override_clut_y);
    } else {
        model_load_image_list(target, 0, 0, 0, 0, 0, 0);
    }
}

/* 8008A0B4: Set a model node's colour. */
void arena_node_set_model_color(Node *node, u8 r, u8 g, u8 b) {
    ((NodeModel *)node->data)->r = r;
    ((NodeModel *)node->data)->g = g;
    ((NodeModel *)node->data)->b = b;
    ((NodeModel *)node->data)->flags |= 0x10;
}

/* 8008A0F4: Clear a model node's colour override. */
void arena_node_clear_model_color(Node *node) {
    ((NodeModel *)node->data)->flags &= ~0x10;
}

/* 8008A110: Set the texture page position used for loaded models (-1 = none). */
void arena_node_set_tpage_override(s16 x, s16 y) {
    arena_node_override_tpage_x = x;
    arena_node_override_tpage_y = y;
}

/* 8008A128: Set the CLUT position used for loaded models (-1 = none). */
void arena_node_set_clut_override(s16 x, s16 y) {
    arena_node_override_clut_x = x;
    arena_node_override_clut_y = y;
}

/* 8008A140: Set the texture page and CLUT positions used for loaded models. */
void arena_node_set_texture_overrides(s16 tx, s16 ty, s16 cx, s16 cy) {
    arena_node_override_tpage_x = tx;
    arena_node_override_tpage_y = ty;
    arena_node_override_clut_x = cx;
    arena_node_override_clut_y = cy;
}

/* 8008A168: Use no texture page or CLUT override for loaded models. */
void arena_node_clear_texture_overrides(void) {
    arena_node_override_tpage_x = arena_node_override_clut_x = -1;
}

/* 8008A184: Load a model file into a model payload, applying the texture page and
 * CLUT overrides. */
void arena_node_init_model(NodeModel *model, SpriteModel *file) {
    model->file = file;
    model->morph = model_start_morph(file, 1);
    model_alloc_packet_buffers(model->file, &model->packets[0], &model->packets[1]);
    if (arena_node_override_tpage_x >= 0) {
        model_set_raw_tpage_override(GetTPage(0, 1, arena_node_override_tpage_x, arena_node_override_tpage_y));
    }
    if (arena_node_override_clut_x >= 0) {
        model_set_clut_override(arena_node_override_clut_x, arena_node_override_clut_y);
    }
    model_build_packets(model->file, model->packets[0], 2);
    arena_copy_words(model->packets[1], model->packets[0], model->file->packet_size);
    model->flags |= 2;
}

/* 8008A254: Allocate a light with a small diagonal direction and no colour. */
Light *arena_node_alloc_light(void) {
    Light *light;

    heap_set_next_class(0xB);
    light = heap_alloc(sizeof(Light), 0);
    light->direction[0] = light->direction[1] = light->direction[2] = 0x10;
    light->colour[0] = light->colour[1] = light->colour[2] = 0;
    return light;
}

/* 8008A298: Free a payload. */
void arena_node_free_payload(void *p) {
    heap_free(p);
}

/* 8008A2B8: Allocate an ordering table pair of the given length and its depth
 * shift (the length should be a power of two up to 0x4000). */
OtPair *arena_display_alloc_layer(u16 length) {
    OtPair *pair;
    u32 *ot;
    s32 bit;

    heap_set_next_class(0xC);
    pair = heap_alloc(sizeof(OtPair), 0);
    heap_set_next_class(0xC);
    ot = heap_alloc(length * 8, 0);
    pair->ot[0] = ot;
    pair->ot[1] = ot + length;
    pair->flags = 1;
    pair->shift = 14;
    pair->unk0 = 0;
    pair->length = length;
    pair->last[0] = &pair->ot[0][length - 1];
    pair->last[1] = &pair->ot[1][length - 1];
    for (bit = 1; bit != length;) {
        bit <<= 1;
        if (bit > 0x4000) {
            pair->shift = 14;
            break;
        }
        pair->shift--;
    }
    return pair;
}

/* 8008A3A0: Unreferenced, and empty. */
void arena_display_empty_unreferenced(void) {
}

/* 8008A3A8: Free a holder and its resource. */
void arena_display_free_layer(OtPair *layer) {
    heap_delay_free(layer->ot[0], 3);
    heap_free(layer);
}

/* 8008A3E0: Allocate a light rig: a root and three light nodes (key light turned
 * round, fill lights level), grey ambient, owning holder. */
LightRig *arena_node_alloc_light_rig(OtPair *layer) {
    LightRig *rig;

    heap_set_next_class(9);
    rig = heap_alloc(sizeof(LightRig), 0);
    rig->unk0 = 0;
    rig->camera = &rig->storage[0];
    rig->lights[0] = &rig->storage[1];
    rig->lights[1] = &rig->storage[2];
    rig->lights[2] = &rig->storage[3];
    arena_node_reset(&rig->storage[0]);
    arena_node_reset(&rig->storage[1]);
    arena_node_reset(&rig->storage[2]);
    arena_node_reset(&rig->storage[3]);
    arena_node_set_light(rig->lights[0], arena_node_alloc_light());
    arena_node_set_light(rig->lights[1], arena_node_alloc_light());
    arena_node_set_light(rig->lights[2], arena_node_alloc_light());
    rig->lights[0]->position.vy = rig->lights[1]->position.vy = rig->lights[2]->position.vy = -2;
    rig->lights[0]->position.vx = 0;
    rig->lights[0]->position.vz = 1;
    rig->lights[1]->position.vx = -1;
    rig->lights[1]->position.vz = -1;
    rig->lights[2]->position.vx = 1;
    rig->lights[2]->position.vz = -1;
    NODE_LIGHT(rig->lights[0])->colour[0] = NODE_LIGHT(rig->lights[0])->colour[1] =
        NODE_LIGHT(rig->lights[0])->colour[2] = 0x800;
    NODE_LIGHT(rig->lights[1])->colour[0] = NODE_LIGHT(rig->lights[1])->colour[1] =
        NODE_LIGHT(rig->lights[1])->colour[2] = 0;
    *(Light *)rig->lights[2]->data = *(Light *)rig->lights[1]->data;
    rig->layer = layer;
    rig->r = rig->g = rig->b = 0;
    rig->r = rig->g = rig->b = 0x10;
    arena_node_load_rig_lights(rig->lights);
    return rig;
}

/* 8008A5BC: Free a light rig, its lights and its holder. */
void arena_node_free_light_rig(LightRig *rig) {
    heap_free(rig->storage[1].data);
    heap_free(rig->storage[2].data);
    heap_free(rig->storage[3].data);
    arena_display_free_layer(rig->layer);
    heap_free(rig);
}

/* 8008A618: Enable model colour overrides. */
void arena_node_enable_color_overrides(void) {
    arena_node_color_overrides_on = 1;
}

/* 8008A62C: Disable model colour overrides. */
void arena_node_disable_color_overrides(void) {
    arena_node_color_overrides_on = 0;
}

/* 8008A63C: Draw a model into the current ordering table, with its colour override
 * (or grey) as the GTE back colour when overrides are enabled. */
void arena_node_draw_model(NodeModel *model) {
    model_box_test_mode = 0;
    if (arena_node_color_overrides_on) {
        if (model->flags & 0x10) {
            gte_SetBackColor(model->r, model->g, model->b);
        } else {
            gte_SetBackColor(0x40, 0x40, 0x40);
        }
    }
    model_draw_sprite_model(model->file, model->packets[arena_draw_buffer_index], arena_current_layer_ot, model->unk4);
}

/* 8008A6F8: Set the current buffer's colour, noting whether it changed. */
void arena_node_set_buffer_color(CVECTOR *colour) {
    CVECTOR *current = &arena_node_buffer_colors[arena_draw_buffer_index];

    if (colour->r == current->r && colour->g == current->g && colour->b == current->b) {
        arena_node_color_changed = 0;
    } else {
        arena_node_color_changed = 1;
        *current = *colour;
        current->cd = 0x20;
    }
}

/* 8008A78C: Write the current buffer's colour into every primitive of an instance. */
void arena_node_color_instance(Node *node) {
    ModelPrims *prims = ((Instance *)node->data)->prims;
    s32 i = prims->count;
    ModelPrim *prim = prims->prims[arena_draw_buffer_index];
    u32 colour = *(u32 *)&arena_node_buffer_colors[arena_draw_buffer_index];

    while (--i != -1) {
        prim->colour = colour;
        prim++;
    }
}

/* 8008A7E0: Update a node tree's matrices (model sets relative to the eye, other
 * nodes relative to their parent) and draw its shown models and
 * instances. */
void arena_node_draw_tree(Node *node) {
    if (node->callback != NULL) {
        node->callback(node);
    }
    switch (node->type) {
    case 2:
        gpu_build_rotation_matrix(&node->angles, &node->view);
        if (arena_node_compose_parent_view) {
            MulMatrix0(&node->parent->view, &node->view, &node->unk6C);
        } else {
            node->unk6C = node->view;
        }
        arena_gte_scale_matrix_columns(&node->view, ((ModelSet *)node->data)->scale);
        node->unk4C = node->view;
        node->unk4C.t[0] = node->unk4C.t[1] = node->unk4C.t[2] = 0;
        node->view.t[0] = node->position.vx - arena_view_origin.vx;
        node->view.t[1] = node->position.vy - arena_view_origin.vy;
        node->view.t[2] = node->position.vz - arena_view_origin.vz;
        CompMatrix(&node->parent->view, &node->view, &node->view);
        break;
    case 0:
    case 1:
        if (node->parent != NULL) {
            node->position.vx = node->offset.vx;
            node->position.vy = node->offset.vy;
            node->position.vz = node->offset.vz;
            gpu_build_rotation_matrix(&node->angles, &node->view);
            TransMatrix(&node->view, &node->position);
            MulMatrix0(&node->parent->unk6C, &node->view, &node->unk6C);
            CompMatrix(&node->parent->unk4C, &node->view, &node->unk4C);
            CompMatrix(&node->parent->view, &node->view, &node->view);
        }
        if (node->type == 1 && !(((NodeModel *)node->data)->flags & 1)) {
            model_load_light_matrix(&node->unk6C);
            gte_SetRotMatrix(&node->view);
            gte_SetTransMatrix(&node->view);
            arena_node_draw_model(node->data);
        }
        break;
    case 5:
        if (((Instance *)node->data)->type == 1) {
            node->view = ((Instance *)node->data)->source->unk4C;
            gte_SetRotMatrix(&node->view);
            gte_SetTransMatrix(&node->view);
            if (arena_node_color_changed) {
                arena_node_color_instance(node);
            }
            arena_mesh_set_light_and_project_shadow(((Instance *)node->data)->prims->mesh, ((Instance *)node->data)->prims->work);
        }
        break;
    }
    if (node->child != NULL) {
        arena_node_draw_tree(node->child);
    }
    if (node->next != NULL) {
        arena_node_draw_tree(node->next);
    }
}

/* 8008ABAC: Load the three rig lights into the light slots. */
void arena_node_load_rig_lights(Node **lights) {
    model_set_light(0, lights[0]->data);
    model_set_light(1, lights[1]->data);
    model_set_light(2, lights[2]->data);
}

/* 8008AC0C: Clear the current buffer's ordering table of a pair and make it the one
 * primitives are added to. */
void arena_display_start_layer(OtPair *pair) {
    ClearOTagR((u_long *)pair->ot[arena_draw_buffer_index], pair->length);
    arena_current_layer_ot = pair->ot[arena_draw_buffer_index];
    model_ot_depth_shift = pair->shift;
}

/* 8008AC7C: Choose the ordering table pair to compact at the end of the frame. */
void arena_display_choose_layer_to_compact(OtPair *pair) {
    arena_display_layer_to_compact = pair;
}

/* 8008AC8C: Note the frame's start time. */
void arena_display_note_frame_start(void) {
    arena_display_frame_start_time = GetRCnt(0xF2000001);
}

/* 8008ACB8: While time remains in the frame budget (frames x 240 ticks, default
 * 192), link the tags the chosen table's entries point at past runs of
 * empty primitives, from the deepest entry down to entry 4. */
void arena_display_compact_layer(s32 frames) {
    OtPair *pair = arena_display_layer_to_compact;
    s32 start;
    s32 limit;
    s32 elapsed;
    s32 i;
    u32 *entry;
    u32 *tag;

    if (pair == NULL) {
        return;
    }
    start = arena_display_frame_start_time;
    arena_display_layer_to_compact = NULL;
    if (frames != 0) {
        limit = frames * 240;
    } else {
        limit = 0xC0;
    }
    for (i = pair->length - 1; i >= 4; i--) {
        elapsed = GetRCnt(0xF2000001) - start;
        if (elapsed < 0) {
            elapsed += 0x10000;
        }
        if (elapsed > limit) {
            return;
        }
        entry = (u32 *)pair->ot[arena_draw_buffer_index][i];
        tag = (u32 *)((*entry & 0xFFFFFF) - 0x80000000);
        if (TAG_LEN(tag) == 0) {
            while (i >= 5) {
                tag = (u32 *)((*tag & 0xFFFFFF) - 0x80000000);
                i--;
                if (TAG_LEN(tag) != 0) {
                    break;
                }
            }
            *entry = (*entry & 0xFF000000) | ((u32)tag & 0xFFFFFF);
        }
    }
}

/* 8008AE1C: Link a layer's table into the frame's ordering table with its area,
 * offset and background packets for the current buffer. */
void arena_display_link_layer(OtPair *layer) {
    arena_display_choose_layer_to_compact(layer);
    AddPrims(arena_current_ot, layer->last[arena_draw_buffer_index], layer->ot[arena_draw_buffer_index]);
    if (!(layer->flags & 4)) {
        SetDrawArea(&layer->area[arena_draw_buffer_index], &arena_current_draw_buffer->draw.clip);
    }
    if (!(layer->flags & 8)) {
        SetDrawOffset(&layer->offset[arena_draw_buffer_index], arena_current_draw_buffer->draw.ofs);
    }
    if (layer->flags & 0x10) {
        AddPrim(arena_current_ot, &layer->tile[arena_draw_buffer_index]);
    }
    AddPrim(arena_current_ot, &layer->offset[arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &layer->area[arena_draw_buffer_index]);
}

/* 8008AF6C: Relocate a model file's pointers to where it was loaded. */
ModelFile *arena_node_relocate_model_file(ModelFile *file) {
    s32 delta = (u8 *)file - file->base;
    u32 i;

    file->base = (u8 *)file;
    file->hierarchy = (u32 *)((u8 *)file->hierarchy + delta);
    file->models += delta;
    file->header = (SceneHeader *)((u8 *)file->header + delta);
    file->unk14 += delta;
    file->slots = (MoveSlot *)((u8 *)file->slots + delta);
    file->image += delta;
    file->unk24 += delta;
    if (file->target != NULL) {
        file->target += delta;
        arena_node_upload_file_images(file->target);
    }
    if (file->animations != NULL) {
        file->animations = (u32 *)((u8 *)file->animations + delta);
        for (i = 1; i < file->animations[0] + 1; i++) {
            if (file->animations[i] != 0) {
                file->animations[i] += delta;
            }
        }
    }
    return file;
}

/* 8008B070: Load and relocate a model file. */
ModelFile *arena_node_read_model_file(s32 file) {
    ModelFile *scene;

    heap_set_next_class(0xA);
    scene = heap_alloc(cd_get_file_size(file), 0);
    cd_read_file(file, scene, 0, 0);
    cd_sync_reads(0);
    return arena_node_relocate_model_file(scene);
}

/* 8008B0D8: Rewind every channel of a player. */
void arena_node_rewind_anim_player(Player *player) {
    Channel *channel;
    s32 i;

    player->unk10 = 0;
    player->frame = 0;
    channel = player->channels;
    for (i = 0; i < player->header->channels; i++) {
        channel->hold = 0;
        channel->value = 0;
        channel->current = channel->start;
        channel->delta = 0;
        channel++;
    }
}

/* 8008B13C: Bind an animation to a model set node: its constant keys and streamed
 * channels drive node angle (short way round) or 0x2C components. Keep the
 * animation's base for stream offsets, then walk its records. The record
 * cursor starts before the channel allocation; the header remains available
 * for both counts. */
void arena_node_bind_animation(AnimRecord *record, Player *player, Node *root) {
    Node **nodes = ((ModelSet *)root->data)->nodes;
    AnimHeader *anim;
    Key *key;
    Channel *channel;
    Node *node;
    u32 i;
    u32 base;

    base = (u32)record;
    anim = (AnimHeader *)record;
    player->header = anim;
    record = anim->records;
    heap_set_next_class(0x10);
    key = heap_alloc(anim->keys * sizeof(Key) + anim->channels * sizeof(Channel), 0);
    player->keys = key;
    channel = (Channel *)(key + anim->keys);
    player->channels = channel;
    for (i = 0; i < anim->keys; i++) {
        node = nodes[record->node];
        key->value = record->value;
        switch (record->kind & 0x7F) {
        case 3:
            key->target = &node->angles.vx;
            key->angular = 1;
            break;
        case 4:
            key->target = &node->angles.vy;
            key->angular = 1;
            break;
        case 5:
            key->target = &node->angles.vz;
            key->angular = 1;
            break;
        case 6:
            key->target = &node->offset.vx;
            key->angular = 0;
            break;
        case 7:
            key->target = &node->offset.vy;
            key->angular = 0;
            break;
        case 8:
            key->target = &node->offset.vz;
            key->angular = 0;
            break;
        }
        key++;
        record++;
    }
    for (i = 0; i < anim->channels; i++) {
        node = nodes[record->node];
        channel->current = (u8 *)(record->value + base);
        channel->start = (u8 *)(record->value + base);
        switch (record->kind & 0x7F) {
        case 3:
            channel->target = &node->angles.vx;
            channel->angular = 1;
            break;
        case 4:
            channel->target = &node->angles.vy;
            channel->angular = 1;
            break;
        case 5:
            channel->target = &node->angles.vz;
            channel->angular = 1;
            break;
        case 6:
            channel->target = &node->offset.vx;
            channel->angular = 0;
            break;
        case 7:
            channel->target = &node->offset.vy;
            channel->angular = 0;
            break;
        case 8:
            channel->target = &node->offset.vz;
            channel->angular = 0;
            break;
        }
        channel++;
        record++;
    }
    arena_node_rewind_anim_player(player);
}

/* 8008B38C: Build a model set node tree from a model set file: a node per hierarchy
 * record (with its model, parent, angle and offset) and a player per
 * animation. Returns the root node. One pointer serves first as the model
 * file and then as the animation table, as the original's register use
 * shows. */
Node *arena_node_build_model_set(ModelFile *file) {
    Node *root;
    u32 i;
    u32 *data = (u32 *)file->models;
    u32 *hierarchy = file->hierarchy;
    u32 *animations = file->animations;
    u32 count = hierarchy[0];
    HierarchyRecord *records = (HierarchyRecord *)(hierarchy + 1);
    Node **nodes;
    ModelSet *set;
    Node *node;
    NodeModel *model;
    Player *player;

    model_relocate_group((ModelGroup *)data);
    heap_set_next_class(0x12);
    nodes = heap_alloc(count * 4, 0);
    set = arena_node_alloc_model_set();
    root = arena_node_alloc();
    arena_node_set_model_set(root, set);
    set->nodes = nodes;
    set->nodeCount = count;
    set->records = records;
    for (i = 0; i < count; i++) {
        node = arena_node_alloc();
        nodes[i] = node;
        if (records[i].model != -1) {
            model = arena_node_alloc_model();
            arena_node_set_model(node, model);
            arena_node_init_model(model, (SpriteModel *)((u8 *)data + (records[i].model * 0x38 + 0x10)));
        }
        if (records[i].parent == -1) {
            arena_node_add_child(root, node);
        } else {
            arena_node_add_child(nodes[records[i].parent], node);
        }
        node->angles.vx = records[i].angle.vx;
        node->angles.vy = records[i].angle.vy;
        node->angles.vz = records[i].angle.vz;
        node->offset.vx = records[i].offset[0];
        node->offset.vy = records[i].offset[1];
        node->offset.vz = records[i].offset[2];
    }
    if (animations != NULL) {
        data = animations;
        heap_set_next_class(0x11);
        player = set->players = heap_alloc(data[0] * sizeof(Player), 0);
        set->count = data[0];
        for (i = 0; i < data[0]; i++) {
            if (data[i + 1] != 0) {
                arena_node_bind_animation((AnimRecord *)data[i + 1], &player[i], root);
            } else {
                player[i].header = NULL;
            }
        }
    }
    return root;
}

/* 8008B5DC: Advance a player by some frames, snapping to the keys. */
s32 arena_node_advance_anim_player(Player *player, s32 frames) {
    return arena_node_step_anim_player(player, frames, 1);
}

/* 8008B5FC: Ease an angle toward target by a fraction (1/steps) of the shorter way
 * round (angles are 12-bit). */
s16 arena_angle_ease_toward(s32 angle, s32 target, s32 steps) {
    s32 diff;
    s16 result;

    angle &= 0xFFF;
    diff = (angle - target) & 0xFFF;
    result = angle;
    if (diff != 0) {
        if (diff < 0x800) {
            result = angle - diff / steps;
        } else {
            result = angle + (0x1000 - diff) / steps;
        }
    }
    return result;
}

/* 8008B650: Turn an angle toward target by a fixed step the shorter way round
 * (a random way when opposite), stopping on the target. */
s32 arena_angle_turn_toward(s32 from, s32 to, s32 step) {
    s16 angle = from;
    s16 target = to;
    s32 diff = (from - to) & 0xFFF;

    if (diff != 0) {
        if (diff == 0x800 ? (rand() & 1) : diff < 0x800) {
            angle -= step;
            if (((angle - target) & 0xFFF) > 0x800) {
                angle = target;
            }
        } else {
            angle += step;
            if (((angle - target) & 0xFFF) < 0x800) {
                angle = target;
            }
        }
    }
    return angle;
}

/* 8008B730: Advance a player by some frames (clamped to the animation's end) and
 * move every target 1/steps of the way to its key or channel value.
 * Channel streams hold a byte per frame: 0xxxxxxx a 7-bit delta, 10xxxxxx
 * hold the previous delta for x following frames, 11xxxxxx plus a byte
 * a signed 14-bit delta.
 * Returns whether the end was reached in a final step (1 without an
 * animation). */
s32 arena_node_step_anim_player(Player *player, s32 frames, s32 steps) {
    Key *key;
    Channel *channel;
    u8 *code;
    s8 command;
    s16 value; /* decoded payload/delta or the current target value */
    s32 i;
    s32 j;

    if (player->header == NULL) {
        return 1;
    }
    if (frames == 0) {
        return;
    }
    if (player->frame + frames > player->header->frames) {
        frames = player->header->frames - player->frame;
    }
    player->frame += frames;
    steps -= frames;
    if (steps <= 0) {
        steps = 1;
    }
    key = player->keys;
    if (steps == 1) {
        for (i = 0; i < player->header->keys; i++, key++) {
            *key->target = key->value;
        }
    } else {
        for (i = 0; i < player->header->keys; i++, key++) {
            value = *key->target;
            if (key->angular) {
                *key->target = arena_angle_ease_toward(value, key->value, steps);
            } else {
                *key->target = value + (key->value - value) / steps;
            }
        }
    }
    channel = player->channels;
    for (i = 0; i < player->header->channels; i++, channel++) {
        for (j = 0; j < frames; j++) {
            if (channel->hold) {
                channel->hold--;
            } else {
                code = channel->current++;
                command = *(s8 *)code;
                if (command & 0x80) {
                    value = command & 0x3F;
                    if (command & 0x40) {
                        channel->current = code + 2;
                        value |= (s8)code[1] << 6;
                        channel->delta = value;
                    } else {
                        channel->hold = value;
                    }
                } else {
                    channel->delta = ((u8)command << 25) >> 25; /* 7-bit signed delta */
                }
            }
            channel->value += channel->delta;
        }
        if (steps == 1) {
            *channel->target = channel->value;
        } else {
            value = *channel->target;
            if (channel->angular) {
                *channel->target = arena_angle_ease_toward(value, channel->value, steps);
            } else {
                *channel->target = value + (channel->value - value) / steps;
            }
        }
    }
    if (player->frame == player->header->frames) {
        if (steps == 1) {
            return 1;
        }
    }
    return 0;
}

/* 8008BA2C: Create a task running entry(arg) on its own stack of `words` words and
 * run it until it first yields. */
TaskContext *arena_task_create(void (*entry)(s32), s32 arg, u32 *stack, s32 words) {
    TaskContext *task;
    s32 i;

    heap_set_next_class(3);
    task = heap_alloc(sizeof(TaskContext), 2);
    for (i = 0; i < 32; i++) {
        task->regs[i] = 0;
    }
    task->stack = stack;
    task->regs[28] = GetGp();
    task->regs[31] = (u32)entry;
    task->regs[4] = arg;
    task->regs[30] = task->regs[29] = (u32)(task->stack + words);
    arena_task_resume(task);
    return task;
}

/* 8008BAE0: Free a task. */
void arena_task_free(TaskContext *task) {
    heap_free(task);
}

/* 8008BB00 */
INCLUDE_ASM("decomp/src/menu", arena_task_save_scheduler);

/* 8008BB1C */
INCLUDE_ASM("decomp/src/menu", arena_task_restore_scheduler);

/* 8008BB3C */
INCLUDE_ASM("decomp/src/menu", arena_task_resume);

/* 8008BC04 */
INCLUDE_ASM("decomp/src/menu", arena_task_yield);

/* 8008BCC8: Set the mesh light direction (a fixed down-left vector) and project
 * its vertices onto the ground plane for the shadow packets. */
void arena_mesh_set_light_and_project_shadow(SpriteModel *mesh, u8 *work) {
    VECTOR direction;
    VECTOR unused; /* unused in the original; reserves 16 bytes */

    direction.vx = -8;
    direction.vy = -8;
    direction.vz = -0x10;
    VectorNormal(&direction, &arena_mesh_light_direction);
    arena_mesh_light_direction.vx <<= 4;
    arena_mesh_light_direction.vy <<= 4;
    arena_mesh_light_direction.vz <<= 4;
    arena_mesh_project_shadow(mesh->vertices, work, mesh->vertex_count);
}

/* 8008BD70: Draw a mesh's primitive groups (flag 8: quads, else triangles) into the
 * given packets and ordering table using the vertex work area. */
void arena_mesh_draw_groups(SpriteModel *mesh, ModelPrim *prims, u32 *ot, u8 *work) {
    PrimitiveGroup *group;
    s32 groups = mesh->group_count;

    model_current_primitive_group = (PrimitiveGroup *)mesh->unk10;
    model_current_packet = (RenderPacket *)prims;
    model_ot = ot;
    model_current_vertices = (SVECTOR *)work;
    model_submitted_primitive_count += mesh->primitive_count;
    while (--groups != -1) {
        group = model_current_primitive_group;
        model_current_primitive_group = group + 1;
        if (group->type & 8) {
            arena_mesh_draw_flat_quads((u8 *)model_current_primitive_group, group->count);
        } else {
            arena_mesh_draw_flat_triangles((u8 *)model_current_primitive_group, group->count);
        }
        model_current_primitive_group = (PrimitiveGroup *)((u8 *)model_current_primitive_group + group->count * 8);
    }
}

/* 8008BE4C: Build a mesh's packet buffers: a vertex work area and, per display
 * buffer, a flat grey quad (0x18 bytes) or triangle (0x14 bytes) packet
 * for every primitive. */
void arena_mesh_build_packets(ModelPrims *mp, SpriteModel *mesh) {
    s32 n; /* vertex, then group counter, then packet bytes per buffer */
    s32 i;
    s32 j;
    s32 triangles;
    s32 quads;
    PrimitiveGroup *group;
    u8 *vertex;
    u8 *packet;

    mp->vertices = mesh->vertex_count;
    mp->count = mesh->primitive_count;
    mp->vertexData = mesh->vertices;
    mp->mesh = mesh;
    model_current_primitive_group = (PrimitiveGroup *)mesh->unk10;
    heap_set_next_class(0x13);
    vertex = mp->work = heap_alloc(mp->vertices * 8, 2);
    n = mp->vertices;
    while (--n != -1) {
        ((s16 *)vertex)[1] = 0;
        vertex += 8;
    }
    heap_set_next_class(5);
    triangles = 0;
    quads = 0;
    n = mesh->group_count;
    while (--n != -1) {
        group = model_current_primitive_group;
        model_current_primitive_group = group + 1;
        if (group->type & 8) {
            quads += group->count;
        } else {
            triangles += group->count;
        }
        model_current_primitive_group = (PrimitiveGroup *)((u8 *)model_current_primitive_group + group->count * 8);
    }
    n = triangles * 0x14 + quads * 0x18;
    packet = heap_alloc(n * 2, 2);
    mp->prims[0] = (ModelPrim *)packet;
    mp->prims[1] = (ModelPrim *)(packet + n);
    i = mesh->group_count;
    model_current_primitive_group = (PrimitiveGroup *)mesh->unk10;
    while (--i != -1) {
        group = model_current_primitive_group;
        model_current_primitive_group = group + 1;
        if (group->type & 8) {
            j = group->count;
            while (--j != -1) {
                TAG_LEN(packet) = 5;
                ((u32 *)packet)[1] = 0x28403030;
                packet += 0x18;
            }
        } else {
            j = group->count;
            while (--j != -1) {
                TAG_LEN(packet) = 4;
                ((u32 *)packet)[1] = 0x20403030;
                packet += 0x14;
            }
        }
        model_current_primitive_group = (PrimitiveGroup *)((u8 *)model_current_primitive_group + group->count * 8);
    }
    arena_copy_words(mp->prims[1], mp->prims[0], n);
}

/* 8008C0BC: Make a node an instance node. */
void arena_node_set_instance(Node *node, Instance *instance) {
    node->type = 5;
    node->data = instance;
}

/* 8008C0CC: Allocate an instance payload drawing source. */
Instance *arena_node_alloc_instance(Node *source) {
    Instance *instance;

    heap_set_next_class(7);
    instance = heap_alloc(sizeof(Instance), 2);
    instance->type = source->type;
    instance->unk8 = (s32)arena_node_instanced_root;
    instance->source = source;
    instance->prims = NULL;
    return instance;
}

/* 8008C120: Free an instance payload's packet buffers. */
void arena_node_free_instance(Instance *instance) {
    ModelPrims *prims = instance->prims;

    if (prims != NULL) {
        if (prims->work != NULL) {
            heap_free(prims->work);
        }
        if (prims->prims[0] != NULL) {
            heap_delay_free(prims->prims[0], 2);
        }
        heap_free(prims);
    }
}

/* 8008C188: Copy a node tree as instance nodes (model sources get their own packet
 * buffers); following siblings are appended to parent. */
Node *arena_node_copy_subtree_as_instances(Node *source, Node *parent) {
    Node *node;
    Instance *instance;
    SpriteModel *mesh;

    heap_set_next_class(6);
    node = arena_node_alloc();
    instance = arena_node_alloc_instance(source);
    arena_node_set_instance(node, instance);
    if (instance->type == 1) {
        mesh = ((NodeModel *)source->data)->file;
        heap_set_next_class(5);
        instance->prims = heap_alloc(sizeof(ModelPrims), 2);
        arena_mesh_build_packets(instance->prims, mesh);
    }
    arena_node_unread_instanced_count++;
    if (source->child != NULL) {
        arena_node_add_child(node, arena_node_copy_subtree_as_instances(source->child, node));
    }
    if (source->next != NULL) {
        arena_node_add_child(parent, arena_node_copy_subtree_as_instances(source->next, parent));
    }
    return node;
}

/* 8008C298: Copy a node tree as instances. */
Node *arena_node_copy_tree_as_instances(Node *source) {
    arena_node_unread_instanced_count = 0;
    return arena_node_copy_subtree_as_instances(source, NULL);
}

/* 8008C2C0: Copy a node tree as instances of itself. */
Node *arena_node_copy_root_as_instances(Node *source) {
    arena_node_instanced_root = source;
    return arena_node_copy_tree_as_instances(source);
}

/* 8008C2E8: Draw a tree of instance nodes whose model sources are shown. */
void arena_node_draw_instances(Node *node) {
    Instance *instance = node->data;
    ModelPrims *prims;

    if (instance->type == 1 && !(((NodeModel *)instance->source->data)->flags & 1)) {
        prims = instance->prims;
        arena_mesh_draw_groups(prims->mesh, prims->prims[arena_draw_buffer_index], arena_current_layer_ot + 1, prims->work);
    }
    if (node->child != NULL) {
        arena_node_draw_instances(node->child);
    }
    if (node->next != NULL) {
        arena_node_draw_instances(node->next);
    }
}

/* 8008C3A8 */
INCLUDE_ASM("decomp/src/menu", arena_mesh_project_shadow);

/* 8008C4B0 */
INCLUDE_ASM("decomp/src/menu", arena_mesh_draw_flat_triangles);

/* 8008C620 */
INCLUDE_ASM("decomp/src/menu", arena_mesh_draw_flat_quads);

/* 8008C7C0: Shift a vector history: entries 4, 3 and 2 all take entry 0. */
void arena_spark_reset_line4(SVECTOR *history) {
    history[4] = history[0];
    history[3] = history[4];
    history[2] = history[3];
}

/* 8008C828: Set up a four-point spark line: semi-transparent, in the source colour,
 * the same in both draw buffers. */
void arena_spark_init_line4(SparkLine4 *spark, Emitter *source) {
    LINE_F4 *line = &spark->line[0];

    setlen(line, 6), setcode(line, 0x4C), line->pad = 0x55555555;
    setSemiTrans(line, 1);
    setRGB0(line, source->r, source->g, source->b);
    spark->line[1] = spark->line[0];
}

/* 8008C8B4: Project a four-point spark line through its trail, age the trail and add
 * the line to the ordering table. */
void arena_spark_draw_line4(SparkLine4 *spark, u32 *ot) {
    LINE_F4 *line = &spark->line[arena_draw_buffer_index];
    long depth;
    s32 otz;

    otz = RotTransPers4(&spark->pos, &spark->trail[0], &spark->trail[1], &spark->trail[2],
                        (long *)&line->x0, (long *)&line->x1, (long *)&line->x2, (long *)&line->x3,
                        &depth, &depth);
    spark->trail[2] = spark->trail[1];
    spark->trail[1] = spark->trail[0];
    spark->trail[0] = spark->pos;
    gpu_ot_link_line_f4((u_long *)(ot + (otz >> 2)), line);
}

/* 8008C9B8: Collapse a three-point spark line's trail onto its position. */
void arena_spark_reset_line3(SparkLine3 *spark) {
    spark->trail[1] = spark->pos;
    spark->trail[0] = spark->trail[1];
}

/* 8008CA00: Set up a three-point spark line. */
void arena_spark_init_line3(SparkLine3 *spark, Emitter *source) {
    LINE_F3 *line = &spark->line[0];

    setlen(line, 5), setcode(line, 0x48), line->pad = 0x55555555;
    setSemiTrans(line, 1);
    setRGB0(line, source->r, source->g, source->b);
    spark->line[1] = spark->line[0];
}

/* 8008CA84: Project a three-point spark line relative to the view origin with the
 * GTE, age its trail and add it. */
void arena_spark_draw_line3(SparkLine3 *spark, u32 *ot) {
    SVECTOR *origin = arena_spark_view_origin;
    SVECTOR *work = arena_spark_gte_scratch;
    LINE_F3 *line;
    s32 otz;

    work[0].vx = spark->pos.vx - origin->vx;
    work[0].vy = spark->pos.vy - origin->vy;
    work[0].vz = spark->pos.vz - origin->vz;
    work[1].vx = spark->trail[0].vx - origin->vx;
    work[1].vy = spark->trail[0].vy - origin->vy;
    work[1].vz = spark->trail[0].vz - origin->vz;
    work[2].vx = spark->trail[1].vx - origin->vx;
    work[2].vy = spark->trail[1].vy - origin->vy;
    work[2].vz = spark->trail[1].vz - origin->vz;
    gte_ldv3c(arena_spark_gte_scratch);
    gte_rtpt();
    line = &spark->line[arena_draw_buffer_index];
    spark->trail[1] = spark->trail[0];
    spark->trail[0] = spark->pos;
    gte_stsxy3(&line->x0, &line->x1, &line->x2);
    gte_stszotz(&otz);
    gpu_ot_link_line_f3((u_long *)(ot + (otz >> 2)), line);
}

/* 8008CC2C: Collapse a two-point spark line's trail onto its position. */
void arena_spark_reset_line2(SparkLine2 *spark) {
    spark->trail[0] = spark->pos;
}

/* 8008CC54: Set up a two-point spark line. */
void arena_spark_init_line2(SparkLine2 *spark, Emitter *source) {
    LINE_F2 *line = &spark->line[0];

    setlen(line, 3), setcode(line, 0x40);
    setSemiTrans(line, 1);
    setRGB0(line, source->r, source->g, source->b);
    spark->line[1] = spark->line[0];
}

/* 8008CCB0: Project a two-point spark line, age its trail and add it. */
void arena_spark_draw_line2(SparkLine2 *spark, u32 *ot) {
    LINE_F2 *line = &spark->line[arena_draw_buffer_index];
    long depth;
    s32 otz;

    /* A LINE_F2 keeps two points: the third vertex and its results are the
     * depth word. */
    otz = RotTransPers3(&spark->pos, &spark->trail[0], (SVECTOR *)&depth, (long *)&line->x0,
                        (long *)&line->x1, &depth, &depth, &depth);
    spark->trail[0] = spark->pos;
    gpu_ot_link_line_f2((u_long *)(ot + (otz >> 2)), line);
}

/* 8008CD54: Reset a tile spark: it keeps no trail. */
void arena_spark_reset_tile(void) {
}

/* 8008CD5C: Set up a spark drawn as a small semi-transparent tile of random size. */
void arena_spark_init_tile(SparkTile *spark, Emitter *source) {
    TILE *tile = &spark->tile[0];

    setlen(tile, 3), setcode(tile, 0x62);
    tile->h = rand() % 2 + 2;
    tile->w = tile->h * 2;
    setRGB0(tile, source->r, source->g, source->b);
    spark->tile[1] = spark->tile[0];
}

/* 8008CE0C: Project a tile spark relative to the view origin and add it. */
void arena_spark_draw_tile(SparkTile *spark, u32 *ot) {
    SVECTOR *origin = arena_spark_view_origin;
    SVECTOR v;
    TILE *tile;
    s32 otz;

    v.vx = spark->pos.vx - origin->vx;
    v.vy = spark->pos.vy - origin->vy;
    v.vz = spark->pos.vz - origin->vz;
    gte_ldv0(&v);
    gte_rtps();
    tile = &spark->tile[arena_draw_buffer_index];
    gte_stsxy(&tile->x0);
    gte_stszotz(&otz);
    gpu_ot_link_tile((u_long *)(ot + (otz >> 2)), tile);
}

/* 8008CED4: Reset a dot spark: it keeps no trail. */
void arena_spark_reset_dot(void) {
}

/* 8008CEDC: Set up a spark drawn as a single semi-transparent dot. */
void arena_spark_init_dot(SparkDot *spark, Emitter *source) {
    TILE_1 *dot = &spark->dot[0];

    setlen(dot, 2), setcode(dot, 0x6A);
    setRGB0(dot, source->r, source->g, source->b);
    spark->dot[1] = spark->dot[0];
}

/* 8008CF30: Project a dot spark and add it. */
void arena_spark_draw_dot(SparkDot *spark, u32 *ot) {
    TILE_1 *dot = &spark->dot[arena_draw_buffer_index];
    long depth;

    gpu_ot_link_tile_1(
        (u_long *)(ot + (RotTransPers(&spark->pos, (long *)&dot->x0, &depth, &depth) >> 2)), dot);
}

/* 8008CF9C: Place a spark at its source's origin. */
void arena_spark_place_at_origin(Emitter *source, SVECTOR *pos) {
    *pos = source->origin;
}

/* 8008CFC4: Place a spark at a random point of its source's box, rotated with the
 * source. */
void arena_spark_place_in_rotated_box(Emitter *source, SVECTOR *pos) {
    SVECTOR v;
    VECTOR r;

    v.vx = rand() % source->range.vx - source->offset.vx;
    v.vy = rand() % source->range.vy - source->offset.vy;
    v.vz = rand() % source->range.vz - source->offset.vz;
    libgte_rotate_svector(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* 8008D0A4: Place a spark at a random point of its source's box. */
void arena_spark_place_in_box(Emitter *source, SVECTOR *pos) {
    pos->vx = source->origin.vx + rand() % source->range.vx - source->offset.vx;
    pos->vy = source->origin.vy + rand() % source->range.vy - source->offset.vy;
    pos->vz = source->origin.vz + rand() % source->range.vz - source->offset.vz;
}

/* 8008D14C: Place a spark at a random point of its source's horizontal rectangle,
 * rotated with the source. */
void arena_spark_place_in_rotated_rect(Emitter *source, SVECTOR *pos) {
    SVECTOR v;
    VECTOR r;

    v.vx = rand() % source->range.vx - source->offset.vx;
    v.vy = 0;
    v.vz = rand() % source->range.vz - source->offset.vz;
    libgte_rotate_svector(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* 8008D208: Place a spark at a random point of its source's horizontal ellipse,
 * rotated with the source. */
void arena_spark_place_in_rotated_ellipse(Emitter *source, SVECTOR *pos) {
    SVECTOR v;
    VECTOR r;
    s32 angle = rand();
    s32 radius = rand();

    v.vx = (gpu_get_sin(angle) * (radius % source->range.vx)) >> 13;
    v.vy = 0;
    v.vz = (gpu_get_cos(angle) * (radius % source->range.vz)) >> 13;
    libgte_rotate_svector(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* 8008D304: Place a spark on its source's ring at a random height, rotated with the
 * source. The ring angle is never initialised in the original. */
void arena_spark_place_on_rotated_ring(Emitter *source, SVECTOR *pos) {
    SVECTOR v;
    VECTOR r;
    s32 angle;

    v.vx = (gpu_get_sin(angle) * source->range.vx) >> 12;
    v.vy = rand() % source->range.vy - source->range.vy / 2;
    v.vz = (gpu_get_cos(angle) * source->range.vz) >> 12;
    libgte_rotate_svector(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* 8008D3F4: Create an emitter of the given spark shape and placement rule: unit
 * spread centred on the origin, white, no sparks yet. */
Emitter *arena_spark_create_emitter(s32 shape, s32 placement) {
    Emitter *emitter;
    SparkShape *kind;

    heap_set_next_class(0x15);
    emitter = heap_alloc(0x7C, 0);
    emitter->range.vx = 0x1000;
    emitter->range.vy = 0x1000;
    emitter->range.vz = 0x1000;
    emitter->spread = 1;
    emitter->placement = placement;
    emitter->unk2 = 0;
    emitter->unk4 = 0;
    emitter->unk6 = 0;
    emitter->base.vx = 0;
    emitter->base.vy = 0;
    emitter->base.vz = 0;
    emitter->angles.vx = 0;
    emitter->angles.vy = 0;
    emitter->angles.vz = 0;
    emitter->turn.vx = 0;
    emitter->turn.vy = 0;
    emitter->turn.vz = 0;
    emitter->gravity = 0;
    emitter->unk46 = 0;
    emitter->speed = 0x100;
    emitter->speed_range = 0x100;
    emitter->offset.vx = emitter->range.vx / 2;
    emitter->offset.vy = emitter->range.vy / 2;
    emitter->offset.vz = emitter->range.vz / 2;
    emitter->place = arena_spark_placement_rules[(s16)placement];
    emitter->update = arena_spark_update_callbacks[0];
    emitter->sparks = NULL;
    emitter->shape = shape;
    emitter->unk64 = 0;
    emitter->life = 100;
    emitter->r = 0xFF;
    emitter->g = 0xFF;
    emitter->b = 0xFF;
    emitter->unk68 = 0;
    kind = &arena_spark_shapes[emitter->shape];
    emitter->size = kind->size;
    emitter->reset = kind->reset;
    emitter->draw = kind->draw;
    emitter->setup = kind->setup;
    return emitter;
}

/* 8008D580: Mark every spark of an emitter for restart. */
void arena_spark_stop_all(Emitter *emitter) {
    u8 *spark = emitter->sparks;
    s32 i;

    for (i = 0; i < emitter->count; i++) {
        ((SVECTOR *)spark)->pad = 0;
        spark += emitter->size;
    }
}

/* 8008D5C0: (Re)allocate an emitter's pool for count sparks and set each one up. */
void arena_spark_alloc_pool(Emitter *emitter, s32 count) {
    u8 *spark;
    void (*setup)(void *, Emitter *);
    s32 i;

    if (emitter->sparks != NULL) {
        heap_delay_free(emitter->sparks, 3);
    }
    emitter->count = count;
    heap_set_next_class(0x14);
    emitter->sparks = heap_alloc(emitter->size * emitter->count, 0);
    spark = emitter->sparks;
    setup = emitter->setup;
    for (i = 0; i < emitter->count; i++) {
        setup(spark, emitter);
        ((SVECTOR *)spark)->pad = 0;
        spark += emitter->size;
    }
}

/* 8008D680: Launch up to count idle sparks: each gets a random direction inside the
 * emitter's spread cone and a random speed, both rotated into place, then a
 * position from the placement rule, the emitter's life and a fresh shape. */
void arena_spark_launch(Emitter *emitter, MATRIX *rotation, s32 count) {
    SVECTOR dir;
    SVECTOR unit;
    MATRIX local;
    MATRIX world;
    MATRIX turned;
    u8 *spark;
    void (*place)(Emitter *, SVECTOR *);
    void (*reset)(void *);
    s32 left;
    s32 i;
    s32 angle;
    s32 heading;

    reset = emitter->reset;
    place = emitter->place;
    world = *rotation;
    emitter->origin.vx = emitter->base.vx + rotation->t[0];
    emitter->origin.vy = emitter->base.vy + rotation->t[1];
    emitter->origin.vz = emitter->base.vz + rotation->t[2];
    gpu_build_rotation_matrix(&emitter->angles, &local);
    libgte_multiply_matrix_in_place(&world, &local);
    gpu_build_rotation_matrix(&emitter->turn, &turned);
    libgte_multiply_matrix_in_place(&turned, &world);
    SetRotMatrix(&turned);
    left = count;
    spark = emitter->sparks;
    for (i = 0; i < emitter->count; i++) {
        if (((Spark *)spark)->pos.pad == 0) {
            if (--left == -1) {
                break;
            }
            angle = rand() % emitter->spread;
            heading = rand();
            dir.vy = -gpu_get_cos(angle);
            angle = gpu_get_sin(angle);
            dir.vx = (gpu_get_sin(heading) * angle) >> 12;
            dir.vz = (gpu_get_cos(heading) * angle) >> 12;
            heading = emitter->speed + rand() % emitter->speed_range; /* now the speed */
            gte_ldv0(&dir);
            gte_rtv0();
            gte_stsv(&unit);
            gte_lddp(heading);
            gte_ldsv(&unit);
            gte_gpf12();
            gte_stsv(&((Spark *)spark)->vel);
        }
        spark += emitter->size;
    }
    SetRotMatrix(&world);
    left = count;
    spark = emitter->sparks;
    for (i = 0; i < emitter->count; i++) {
        if (((Spark *)spark)->pos.pad == 0) {
            if (--left == -1) {
                break;
            }
            place(emitter, (SVECTOR *)spark);
            ((Spark *)spark)->pos.pad = emitter->life;
            reset(spark);
        }
        spark += emitter->size;
    }
}

/* 8008D980: Bounce a falling spark off the floor under it, losing half its speed.
 * The floor query reads the spark position as a 32-bit vector. */
void arena_spark_bounce_on_floor(Spark *spark) {
    if (spark->vel.vy > 0 && spark->pos.vy > arena_stage_get_ground_height((VECTOR *)spark, 0)) {
        spark->vel.vy = -spark->vel.vy / 2;
    }
}

/* 8008D9F0: Bounce a spark off the ground plane (y = 0), losing half its speed;
 * a spark that has come to rest dies. */
void arena_spark_bounce_on_plane(Spark *spark) {
    if (spark->pos.vy > 0) {
        spark->vel.vy = -spark->vel.vy / 2;
        if (abs(spark->vel.vy) < 8) {
            spark->pos.pad = 0;
        }
    }
}

/* 8008DA48: Move and draw every live spark of an emitter: gravity, a bounce on the
 * ground plane, projection relative to the camera through the scratchpad. */
void arena_spark_update_and_draw(Emitter *emitter, u32 *ot, MATRIX *view) {
    SVECTOR unused[5]; /* unused in the original; reserves 40 bytes */
    Spark *spark;
    void (*draw)(void *, u32 *);
    s32 i;

    arena_spark_gte_scratch = (SVECTOR *)0x1F800000;
    arena_spark_view_origin = (SVECTOR *)0x1F800030;
    ((SVECTOR *)0x1F800030)->vx = arena_view_origin.vx;
    ((SVECTOR *)0x1F800030)->vy = arena_view_origin.vy;
    ((SVECTOR *)0x1F800030)->vz = arena_view_origin.vz;
    spark = (Spark *)emitter->sparks;
    draw = emitter->draw;
    for (i = 0; i < emitter->count; i++) {
        if (spark->pos.pad != 0) {
            spark->pos.pad--;
            spark->vel.vy += emitter->gravity;
            spark->pos.vx += spark->vel.vx;
            spark->pos.vy += spark->vel.vy;
            spark->pos.vz += spark->vel.vz;
            if (spark->pos.vy > 0) {
                spark->vel.vy = -spark->vel.vy * 2 / 3;
                if (abs(spark->vel.vy) < 4) {
                    spark->pos.pad = 0;
                }
            }
            draw(spark, ot);
        }
        spark = (Spark *)((u8 *)spark + emitter->size);
    }
}

/* 8008DBC0: Copy one model part's local transform. */
void arena_spark_copy_part_matrix(Node *model, s16 part, MATRIX *out) {
    MATRIX unused; /* unused in the original; reserves 32 bytes */

    *out = ((ModelSet *)model->data)->nodes[part]->unk4C;
}

/* 8008DC28: Create the menu's spark emitter: 256 orange three-point sparks. */
void arena_spark_create_burst_emitter(void) {
    Emitter *emitter = arena_spark_create_emitter(1, 0);

    emitter->r = 0xFF;
    emitter->g = 0xA0;
    emitter->b = 0x70;
    arena_spark_alloc_pool(emitter, 0x100);
    emitter->gravity = 4;
    emitter->spread = 0x60;
    emitter->speed_range = 0x60;
    emitter->speed = 4;
    emitter->unk68 = 0;
    emitter->life = 0x20;
    arena_spark_burst_emitter = emitter;
}

/* 8008DCA8: Start a spark burst of the given strength. */
void arena_spark_start_burst(s32 strength) {
    arena_spark_burst_strength = strength;
}

/* 8008DCB8: Emit a burst from a model part while the burst lasts, then move and draw
 * the menu's sparks under the given view. */
void arena_spark_emit_burst_and_draw(u32 *ot, Node *model, MATRIX *view, VECTOR *pos) {
    Emitter *emitter = arena_spark_burst_emitter;
    MATRIX rotation;
    MATRIX part;

    if (arena_spark_burst_strength >= 0x10) {
        arena_spark_copy_part_matrix(model, 0x27, &part);
        libgte_orthonormalize_matrix(&part, &rotation);
        rotation.t[0] = rotation.t[1] = rotation.t[2] = 0;
        emitter->base.vx = pos->vx;
        emitter->base.vy = pos->vy;
        emitter->base.vz = pos->vz;
        emitter->turn.vx = 0;
        emitter->turn.vy = 0;
        emitter->turn.vz = 0;
        emitter->angles.vx = 0;
        emitter->angles.vy = 0;
        emitter->angles.vz = 0x800;
        arena_spark_launch(emitter, &rotation, arena_spark_burst_strength >> 4);
        arena_spark_burst_strength -= 4;
    }
    gte_SetTransMatrix(view);
    gte_SetRotMatrix(view);
    arena_spark_update_and_draw(emitter, ot, view);
}

/* 8008DDFC */
INCLUDE_ASM("decomp/src/menu", arena_gte_rotate_scale_svector);

/* 8008DE54 */
INCLUDE_ASM("decomp/src/menu", arena_spark_link_tile_packet_unreferenced);

/* 8008DF30: Forget the three glow buffers. */
void arena_glow_forget_buffers(void) {
    arena_glow_image = NULL;
    arena_glow_old_field = NULL;
    arena_glow_new_field = NULL;
}

/* 8008DF50: Allocate the glow buffers once, clear them, and upload the glow palette
 * with every entry marked semi-transparent. */
void arena_glow_init(void) {
    RECT rect;
    s32 i;

    if (arena_glow_image == NULL) {
        arena_glow_image = heap_alloc(0x1500, 1);
        arena_glow_old_field = heap_alloc(0x2BC0, 1);
        arena_glow_new_field = heap_alloc(0x2BC0, 1);
    }
    for (i = 0x1570; i != -1; i--) {
        arena_glow_new_field[i] = 0;
        arena_glow_old_field[i] = 0;
    }
    for (i = 0; i < 0x1500; i++) {
        arena_glow_image[i] = 0;
    }
    for (i = 0; i < 0x100; i++) {
        arena_glow_palette[i] |= 0x8000;
    }
    rect.y = 0x1FD;
    rect.w = 0xFF;
    rect.x = 0;
    rect.h = 1;
    LoadImage(&rect, (u_long *)arena_glow_palette);
}

/* 8008E064: Release the glow buffers. */
void arena_glow_free_buffers(void) {
    if (arena_glow_image != NULL) {
        heap_delay_free(arena_glow_image, 2);
        heap_delay_free(arena_glow_old_field, 2);
        heap_delay_free(arena_glow_new_field, 2);
        arena_glow_image = NULL;
        arena_glow_old_field = NULL;
        arena_glow_new_field = NULL;
    }
}

/* 8008E0C8: Copy the new glow field over the old one and pack every word's low bytes
 * of both halves into the byte field. */
void arena_glow_commit_field(void) {
    u8 *bytes = arena_glow_image;
    u32 *old = (u32 *)arena_glow_old_field;
    u32 *new = (u32 *)arena_glow_new_field;
    u32 value;
    s32 i;

    for (i = 0xA7F; i != -1; i--) {
        value = *new++;
        *old++ = value;
        *bytes++ = value;
        *bytes++ = value >> 16;
    }
}

/* 8008E120: Advance the glow field one step: seed the two bottom rows with random
 * heat, let every cell take the cooled average of its neighbours below,
 * then keep the result for the next step. The seeding loop and the
 * source index share i. */
void arena_glow_step(void) {
    s16 *new;
    s16 *old;
    s16 *seed;
    s32 heat;
    s32 value;
    s32 i;
    s32 x;
    s32 y;
    s16 *up;
    s16 *right;
    s16 *left;
    s16 *down_right;
    s16 *down_left;
    s16 *dst;

    if (arena_glow_image != NULL) {
        heat = 0;
        new = arena_glow_new_field;
        seed = &new[47 * 0x70];
        for (i = 0; i < 0x70; i++) {
            switch (rand() & 3) {
            case 0:
                heat = 0x180;
                break;
            case 1:
                heat = 0;
                break;
            }
            seed[i] = seed[i + 0x70] = heat;
        }
        old = arena_glow_old_field;
        up = old - 0x70;
        right = old + 1;
        left = old - 1;
        down_right = old + 0x71;
        down_left = old + 0x6F;
        for (y = 0x2F; y > 1; y--) {
            i = y * 0x70 + 1;
            dst = &new[(y - 1) * 0x70];
            for (x = 1; x < 0x70; x++) {
                value = (up[i] + right[i] + left[i] + down_right[i] + down_left[i]) / 5;
                i++;
                if (value > 3) {
                    value -= 3;
                }
                dst[x] = value;
            }
        }
        arena_glow_commit_field();
    }
}

/* 8008E2B8: Draw a full-screen grey tile of the given level, additive or subtractive,
 * with the draw mode that selects the blend. */
void arena_glow_draw_shade_tile(u32 *ot, s32 level, s32 subtract) {
    TILE *tile = &arena_glow_shade_tiles[arena_draw_buffer_index];

    *(u32 *)&tile->r0 = level | (level << 8) | (level << 16) | 0x60000000;
    setlen(tile, 3);
    *(u32 *)&tile->x0 = 0;
    *(u32 *)&tile->w = 0xDA0140;
    setSemiTrans(tile, 1);
    AddPrim(ot, tile);
    if (subtract) {
        SetDrawMode(&arena_glow_blend_modes[arena_draw_buffer_index], 0, 1, GetTPage(0, 2, 0, 0), NULL);
    } else {
        SetDrawMode(&arena_glow_blend_modes[arena_draw_buffer_index], 0, 1, GetTPage(0, 1, 0, 0), NULL);
    }
    AddPrim(ot, &arena_glow_blend_modes[arena_draw_buffer_index]);
}

/* 8008E3CC: Draw the glow field: upload its byte image and stretch it over the
 * screen as a semi-transparent textured quad at two thirds of the level
 * (plain texture at full level); optionally add a brightening tile. */
void arena_glow_draw(u32 *ot, s32 level, s32 brighten) {
    POLY_FT4 *quad = &arena_glow_quads[arena_draw_buffer_index];
    TILE *tile;
    RECT rect;
    s32 shade;

    setlen(quad, 9);
    shade = level * 2 / 3;
    *(u32 *)&quad->r0 = shade | (shade << 8) | (shade << 16) | 0x2C000000;
    setShadeTex(quad, shade == 0x80);
    *(u32 *)&quad->x0 = 0;
    *(u32 *)&quad->x1 = 0x140;
    *(u32 *)&quad->x2 = 0xDA0000;
    *(u32 *)&quad->x3 = 0xDA0140;
    *(u16 *)&quad->u0 = 0;
    *(u16 *)&quad->u1 = 0x6F;
    *(u16 *)&quad->u2 = 0x2A00;
    *(u16 *)&quad->u3 = 0x2A6F;
    setSemiTrans(quad, 1);
    quad->tpage = GetTPage(1, 1, 0x140, 0x100);
    quad->clut = GetClut(0, 0x1FD);
    AddPrim(ot, quad);
    rect.x = 0x140;
    rect.y = 0x100;
    rect.w = 0x38;
    rect.h = 0x2B;
    LoadImage(&rect, (u_long *)arena_glow_image);
    if (brighten) {
        tile = &arena_glow_shade_tiles[arena_draw_buffer_index];
        if (level > 0x80) {
            shade = level * 2;
            *(u32 *)&tile->r0 = shade | (shade << 8) | (shade << 16) | 0x60000000;
            setlen(tile, 3);
            *(u32 *)&tile->x0 = 0;
            *(u32 *)&tile->w = 0xDA0140;
            setSemiTrans(tile, 1);
            AddPrim(ot, tile);
        }
    }
    SetDrawMode(&arena_glow_blend_modes[arena_draw_buffer_index], 0, 1, GetTPage(0, 2, 0, 0), NULL);
    AddPrim(ot, &arena_glow_blend_modes[arena_draw_buffer_index]);
}

/* 8008E620: Reset the sound driver and the four positional voices. */
void arena_sound_reset(void) {
    s32 mask = 0x300;
    SoundVoice *voice;
    u32 i;

    sound_stop_all_effects();
    for (i = 0; i < 4; i++) {
        voice = &arena_sound_voices[i];
        voice->mask = mask;
        mask <<= 2;
        voice->active = 0;
        voice->age = 0;
        voice->voice = i * 2;
    }
}

/* 8008E67C: Age every positional voice (saturating). */
void arena_sound_age_voices(void) {
    if (arena_sound_voices[0].age != 0xFFFF) {
        arena_sound_voices[0].age++;
    }
    if (arena_sound_voices[1].age != 0xFFFF) {
        arena_sound_voices[1].age++;
    }
    if (arena_sound_voices[2].age != 0xFFFF) {
        arena_sound_voices[2].age++;
    }
    if (arena_sound_voices[3].age != 0xFFFF) {
        arena_sound_voices[3].age++;
    }
}

/* 8008E6F8: Choose a character's command sound table by its model kind. */
void arena_sound_choose_command_table(Actor *owner) {
    switch (owner->model_id) {
    case 9:
        owner->sounds = arena_sound_model9_command_sounds;
        break;
    case 0x1D:
        owner->sounds = arena_sound_model29_command_sounds;
        break;
    case 0x1B:
        owner->sounds = arena_sound_model27_command_sounds;
        break;
    case 0x24:
        owner->sounds = arena_sound_model36_command_sounds;
        break;
    default:
        owner->sounds = arena_sound_default_command_sounds;
        break;
    }
}

/* 8008E78C: Start a sound on a free positional voice (or a matching unpositioned
 * one, else the oldest); positioned sounds follow pos or its snapshot. */
void arena_sound_start_voice(s32 sound, s32 mode, VECTOR *pos, s32 tag) {
    s32 oldest = 0;
    SoundVoice *chosen = &arena_sound_voices[3];
    SoundVoice *voice;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &arena_sound_voices[i];
        if (voice->active == 0) {
            chosen = voice;
            break;
        }
        if (mode == 0 && voice->mode == 0) {
            chosen = voice;
            break;
        }
        if (oldest < voice->age) {
            oldest = voice->age;
            chosen = voice;
        }
    }
    arena_sound_age_voices();
    voice = chosen;
    voice->mode = mode;
    voice->sound = sound;
    voice->active = 1;
    voice->unk3 = tag;
    voice->follow = pos;
    if (pos != NULL) {
        voice->pos = *pos;
    }
    voice->age = 0;
    if (mode == 0) {
        sound_play_effect_on_channel_volume_pan(voice->sound, voice->voice, 0x7F, 0x40);
    }
}

/* 8008E8B0: Pan and attenuate every positioned voice from its screen position and
 * depth; a voice just started is keyed on with those values. */
void arena_sound_update_voices(void) {
    SVECTOR v;
    SVECTOR screen;
    s32 sz;
    SoundVoice *voice;
    s32 volume;
    s32 x;
    s32 pan;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &arena_sound_voices[i];
        if (voice->active && voice->mode != 0) {
            if (voice->mode == 1) {
                v.vx = voice->pos.vx;
                v.vy = voice->pos.vy;
                v.vz = voice->pos.vz;
            } else {
                v.vx = voice->follow->vx;
                v.vy = voice->follow->vy;
                v.vz = voice->follow->vz;
            }
            v.vx -= arena_view_origin.vx;
            v.vy -= arena_view_origin.vy;
            v.vz -= arena_view_origin.vz;
            gte_ldv0(&v);
            gte_rtps();
            gte_stsxy(&screen);
            gte_stsz(&sz);
            x = screen.vx;
            if (x < 0) {
                x = 0;
            }
            if (x > 0x140) {
                x = 0x140;
            }
            volume = (0x3000 - sz) * 0x7F / 0x3000;
            if (volume < 0x28) {
                volume = 0x28;
            }
            if (volume > 0x7F) {
                volume = 0x7F;
            }
            pan = x * 0x7F / 0x140;
            if (voice->age != 0) {
                sound_set_effect_pan_on_channel(voice->voice, pan);
                sound_set_effect_volume_on_channel(voice->voice, volume);
            } else {
                sound_play_effect_on_channel_volume_pan(voice->sound, voice->voice, volume, pan);
            }
        }
    }
    arena_sound_age_voices();
}

/* 8008EADC: Free the voices whose sound has stopped, then age them all. */
void arena_sound_free_stopped_voices(void) {
    SoundVoice *voice;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &arena_sound_voices[i];
        if (!(sound_get_active_effect_mask(voice->sound) & voice->mask)) {
            voice->active = 0;
        }
    }
    arena_sound_age_voices();
}

/* 8008EB4C: Play a menu sound effect (unpositioned). */
void arena_sound_play_effect(s32 id) {
    if (id != 0) {
        arena_sound_start_voice(0x60000 + id, 0, NULL, pad_vblank_count);
    }
}

/* 8008EB88: Play a character's sound effect, tagged with its id and side. */
void arena_sound_play_actor_effect(Actor *owner, s32 id, VECTOR *pos, s32 mode) {
    if (id != 0) {
        arena_sound_start_voice(id + 0x60000, mode, pos, (id & 0x7F) | ((owner->flags >> 20) & 0x80));
    }
}

/* 8008EBD0: Play one of a character's command sounds (random 1-6 when index is 0):
 * up to two effects from the shared pair table. */
void arena_sound_play_command_sound(Actor *owner, s32 index, VECTOR *pos, s32 mode) {
    s32 entry;

    if (index == 0) {
        index = rand() % 6 + 1;
    }
    entry = owner->sounds[index];
    if (entry != 0xFF) {
        index = arena_sound_command_effect_pairs[entry * 2];
        if (index != 0) {
            arena_sound_start_voice(index | 0x60000, mode, pos, (index & 0x7F) | ((owner->flags >> 20) & 0x80));
        }
        index = arena_sound_command_effect_pairs[entry * 2 + 1];
        if (index != 0) {
            arena_sound_start_voice(index | 0x60000, mode, pos, (index & 0x7F) | ((owner->flags >> 20) & 0x80));
        }
    }
}

/* 8008ECEC: Stop every voice started with the given tag. */
void arena_sound_stop_tag(u8 tag) {
    SoundVoice *voice;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &arena_sound_voices[i];
        if (voice->active && voice->unk3 == tag) {
            sound_stop_effect_on_channel(voice->voice);
            voice->active = 0;
        }
    }
}

/* 8008ED6C: Stop the sounds a character's command sound entry started. Declared int
 * without a return value, as the original's unfilled last delay slot shows. */
s32 arena_sound_stop_command_sound(Actor *owner, s32 index) {
    s32 entry;

    entry = owner->sounds[index];
    if (entry != 0xFF) {
        index = arena_sound_command_effect_pairs[entry * 2];
        if (index != 0) {
            arena_sound_stop_tag((index & 0x7F) | ((owner->flags >> 20) & 0x80));
        }
        index = arena_sound_command_effect_pairs[entry * 2 + 1];
        if (index != 0) {
            arena_sound_stop_tag((index & 0x7F) | ((owner->flags >> 20) & 0x80));
        }
    }
}

/* 8008EE1C: Accelerate an actor toward the speed limit (or brake to a stop, harder
 * when not guarding) for two ticks, and turn it toward a heading. */
void arena_brain_steer(Actor *actor, s16 heading, s16 limit) {
    s32 unused[4]; /* unused in the original; reserves 16 bytes */
    s32 brake = actor->brake;
    s32 accel = actor->accel;
    s32 moving;
    s32 i;

    if (actor->flags & 0x100) {
        brake = brake * 2 / 3;
        moving = 0;
    } else {
        moving = 1;
    }
    for (i = 0; i < 2; i++) {
        if (limit != 0 && moving) {
            actor->state += accel;
            if (limit < actor->state) {
                actor->state = limit;
            }
        } else {
            actor->state -= brake;
            if (actor->state < 0) {
                actor->state = 0;
            }
        }
    }
    actor->target_angle = arena_angle_turn_toward(actor->target_angle, heading, 0x40);
    actor->unkCE = 0;
}

/* 8008EF00: Opponent command: act, then wait a second. */
void arena_brain_command_back_dash(Actor *actor, Brain *brain) {
    arena_actor_queue_back_dash(actor);
    brain->timer = 0x3C;
}

/* 8008EF30: Opponent command: act, then wait longer when told to. */
void arena_brain_command_random_attacks(Actor *actor, Brain *brain, s32 long_wait) {
    arena_brain_queue_random_attacks(actor);
    brain->timer = long_wait ? 0x1E : 0xA;
}

/* 8008EF74: Opponent command: input 3, then wait a second. */
void arena_brain_command_magic_firer(Actor *actor, Brain *brain) {
    arena_actor_queue_input(actor, 3);
    brain->timer = 0x3C;
}

/* 8008EFA8: Opponent command: toggle guarding. */
void arena_brain_command_on_guard(Actor *actor, Brain *brain) {
    brain->defending ^= 1;
    if (brain->defending) {
        actor->flags |= 2;
        brain->timer = 0x3C;
    } else {
        actor->flags &= ~2;
        actor->flags &= ~0x38;
        brain->timer = 0x1E;
    }
}

/* 8008F014: Opponent command: inputs 4 and 3, then wait a second. */
void arena_brain_command_magic_jumper(Actor *actor, Brain *brain) {
    arena_actor_queue_input(actor, 4);
    arena_actor_queue_input(actor, 3);
    brain->timer = 0x3C;
}

/* 8008F060: Opponent command: input 4, then wait a second. */
void arena_brain_command_kangaroo(Actor *actor, Brain *brain) {
    arena_actor_queue_input(actor, 4);
    brain->timer = 0x3C;
}

/* 8008F094: Opponent roaming: while far away keep deciding every frame; otherwise
 * pick a new random heading and duration when the timer runs out. */
void arena_brain_command_run_away(Actor *actor, Brain *brain) {
    if (arena_actors_flat_distance > 0x800) {
        brain->timer = 1;
        brain->unkC = 0;
    } else if (--brain->timer == -1) {
        brain->unkA = rand() % 0x600 + 0x500;
        brain->timer = rand() % 50 + 10;
        brain->unkC = 0xFF;
    }
}

/* 8008F17C: Opponent circling: while very close keep deciding every frame; otherwise
 * pick a new random turn and duration when the timer runs out. */
void arena_brain_command_give_chase(Actor *actor, Brain *brain) {
    if (arena_actors_flat_distance < 0x100) {
        brain->timer = 1;
        brain->unkC = 0;
    } else if (--brain->timer == -1) {
        brain->unkA = rand() % 0x600 - 0x300;
        brain->timer = rand() % 120 + 10;
        brain->unkC = 0xFF;
    }
}

/* 8008F260: Opponent command: store its argument, then run the mode's step. */
void arena_brain_command_battle(Actor *actor, Brain *brain, u8 arg) {
    brain->unkF = arg;
    arena_brain_update(actor);
}

/* 8008F280: Drive the computer opponent one frame: reset its state when the command
 * changes, count down to the next decision (some commands decide every
 * frame), run the command and then steer and accelerate. */
void arena_brain_run_practice_command(Actor *actor) {
    extern u8 arena_settings_command; /* arena_settings.command, the byte at +0x0A (menu.bss.ld). */
    Brain *brain = actor->brain;
    s32 command = arena_settings_command;

    arena_settings.driven = 1;
    if (arena_brain_last_command != command) {
        brain->timer = 0;
        brain->unkC = 0;
        actor->flags &= ~2;
        actor->state = 0;
        actor->flags &= ~0x38;
        brain->defending = 0;
        arena_brain_last_command = command;
        actor->unkCE = actor->unkCC + 0x800;
    }
    if (brain->defending) {
        actor->flags |= 2;
    }
    switch (arena_settings_command) {
    case 2:
    case 8:
    case 9:
    case 11:
    case 12:
    case 13:
        break;
    default:
        if (--brain->timer != -1) {
            return;
        }
        break;
    }
    switch (arena_settings_command) {
    case 3:
        arena_brain_command_random_attacks(actor, brain, 0);
        break;
    case 4:
        arena_brain_command_random_attacks(actor, brain, 1);
        break;
    case 5:
        arena_brain_command_magic_firer(actor, brain);
        break;
    case 6:
        arena_brain_command_magic_jumper(actor, brain);
        break;
    case 7:
        arena_brain_command_kangaroo(actor, brain);
        break;
    case 9:
        arena_brain_command_run_away(actor, brain);
        break;
    case 8:
        arena_brain_command_give_chase(actor, brain);
        break;
    case 10:
        arena_brain_command_back_dash(actor, brain);
        break;
    case 11:
        arena_brain_command_battle(actor, brain, 0);
        return;
    case 12:
        arena_brain_command_battle(actor, brain, 1);
        return;
    case 13:
        arena_brain_command_battle(actor, brain, 2);
        return;
    case 2:
        arena_settings.driven = 0;
        return;
    case 1:
        arena_brain_command_on_guard(actor, brain);
        break;
    case 0:
    default:
        brain->timer = 1;
        break;
    }
    arena_brain_steer(actor, brain->unkA, brain->unkC);
}

/* 8008F4F4: Whether an actor's hp is still above the given fraction (of 255) of
 * its maximum. */
s32 arena_brain_is_hp_above_fraction(Actor *actor, s32 fraction) {
    return actor->max_hp * fraction / 255 < actor->hp;
}

/* 8008F530: Whether an actor lacks the charge for its special move (or, with a
 * flag, whether spending it is allowed). */
s32 arena_brain_check_special_charge(Actor *actor, s32 check) {
    if (check) {
        return arena_actor_can_take_charge(actor, actor->unkBE);
    }
    return actor->charge < 0x1000 - actor->unkBE;
}

/* 8008F570: The charge left over after a special move. */
s32 arena_brain_get_charge_after_special(Actor *actor, Brain *brain) {
    return 0x1000 - actor->unkBE;
}

/* 8008F580: Compare an actor's charge with the level its brain waits for: 2 while
 * well below, else 1 up to the level and 0 above it. */
s32 arena_brain_compare_charge(Actor *actor) {
    Brain *brain = actor->brain;

    if (brain->unk24 - 0x200 >= actor->charge) {
        return 2;
    }
    return !(brain->unk24 < actor->charge);
}

/* 8008F5B4: Decide whether the opponent attacks now, weighing its eagerness, its
 * charge and hp and the other actor's hp. */
s32 arena_brain_decide_attack(Actor *actor, s32 unused) {
    Brain *brain = actor->brain;

    if ((rand() & 0xFF) < (brain->unk10 * 320) >> 4) {
        if (arena_brain_check_special_charge(actor, 0)) {
            goto press;
        }
        if (arena_brain_is_hp_above_fraction(actor, 0xC0)) {
            goto press;
        }
        if ((rand() & 0xFF) < (brain->unk1C * 192) >> 4) {
            goto press;
        }
        if (!arena_brain_is_hp_above_fraction(actor, 0x80) || actor->opponent->hp >= actor->hp) {
            return 0;
        }
    } else if ((rand() & 0xFF) >= (brain->unk1C * 320) >> 4) {
        return 0;
    }
press:
    if ((rand() & 0xFF) < brain->unk18) {
        if (actor->opponent->unkC4 == 4) {
            return 0;
        }
        if (arena_brain_is_hp_above_fraction(actor->opponent, 0x20)) {
            return 1;
        }
        if (actor->opponent->hp < actor->hp) {
            return 0;
        }
    }
    return 1;
}

/* 8008F720: Decide whether the opponent closes in: an eager opponent that is already
 * near holds back; otherwise it follows its charge or its eagerness. */
s32 arena_brain_decide_close_in(Actor *actor, s32 eager) {
    Brain *brain = actor->brain;

    if (eager && (rand() & 0xFF) < brain->unk10 && arena_actors_flat_distance < 0x600) {
        return 0;
    }
    if (arena_brain_compare_charge(actor)) {
        return 1;
    }
    return (rand() & 0xFF) < brain->unk1C;
}

/* 8008F7B8: Roll the opponent's choices for the next round from its tendencies. */
void arena_brain_roll_choices(Brain *brain) {
    brain->unk2C_9 = (rand() & 0xFF) < brain->unk10;
    brain->unk2C_10 = (rand() & 0xFF) < brain->unk14;
    brain->unk2C_12 = rand() & 1;
    brain->unk2C_11 = (rand() & 0xFF) < brain->unk18;
    brain->unk2C_8 = (rand() & 0xFF) < brain->unk10 && rand() % 10 < 3;
    brain->roll = rand();
    brain->unk30 = brain->owner->unk1668;
}

/* 8008F900: Opponent jump attack: unless the other actor is airborne (then only one
 * time in four), act or jump and attack. */
void arena_brain_jump_attack(Actor *actor) {
    if ((actor->opponent->flags & 0x60000000) != 0x20000000 || (rand() & 3) == 0) {
        if (arena_rubber_band_enabled) {
            arena_actor_queue_back_dash(actor);
            arena_actor_queue_input(actor, 4);
        } else if ((actor->flags & 0x60000000) == 0x20000000) {
            arena_actor_queue_input(actor, 4);
        }
        arena_actor_queue_input(actor, 3);
    }
}

/* 8008F9B0: Whether an actor stands in the far quadrant of the scene or on a floor
 * of kind 1. */
s32 arena_brain_is_in_far_quadrant(Actor *actor) {
    VECTOR pos = actor->pos;

    pos.vx -= 0x3F80;
    pos.vz -= 0x3F80;
    if (pos.vx > 0 && pos.vz > 0) {
        return 1;
    }
    return (arena_stage_get_ground_square(&actor->pos) & 0x3000000) == 0x1000000;
}

/* 8008FA2C: Steer the opponent toward one of two headings depending on which side
 * of the scene centre it stands, at full speed. */
s32 arena_brain_steer_out_of_quadrant(Actor *actor, Brain *brain) {
    VECTOR pos = actor->pos;

    pos.vx -= 0x3F80;
    pos.vz -= 0x3F80;
    if ((ratan2(pos.vx, pos.vz) & 0xFFF) > 0x200) {
        brain->unkA = 0x800 - arena_actors_heading;
    } else {
        brain->unkA = 0xC00 - arena_actors_heading;
    }
    brain->unkC = 0xFF;
    return 0;
}

/* 8008FACC: Opponent retreat rule (when enabled and on side 1): leave the far
 * quadrant toward the centre; when the other actor is there, dodge its
 * shots by turning to face away while they are close and stop once they
 * are far. Returns whether the rule took over; between the two distances
 * the original falls off the end with the last comparison (1) in $v0. */
s32 arena_brain_apply_retreat_rule(Actor *actor, Brain *brain) {
    s32 dist;

    if (!arena_retreat_rule_enabled || !(actor->flags & 0x08000000)) {
        return 0;
    }
    if (arena_brain_is_in_far_quadrant(actor)) {
        arena_brain_steer_out_of_quadrant(actor, brain);
        return 1;
    }
    if (arena_brain_is_in_far_quadrant(actor->opponent)) {
        dist = actor->opponent->nearest_dist;
        if (dist < 0x800) {
            actor->flags |= 0x8000;
            brain->unkE = 1;
            brain->unkC = 0xFF;
            brain->unkA = 0x800;
            actor->target_angle = 0x800;
            return 1;
        }
        if (dist > 0x1000) {
            actor->flags &= ~0x8000;
            brain->unkE = 0;
            brain->unkC = 0;
            actor->target_angle = 0x800;
            return 1;
        }
    }
    /* falls off the end: the original returns the failed check (0) or,
     * between the distances, the comparison result (1) */
}

/* 8008FBD8: Opponent guard reaction: always at level 2, else on a random roll
 * (every other round at level 1, one in six at level 0). */
void arena_brain_react_with_guard(Actor *actor, Brain *brain) {
    if (brain->unkF >= 2) {
        actor->flags |= 2;
        arena_actor_clear_inputs(actor);
    } else if (brain->unkF != 0) {
        if (brain->roll & 1) {
            actor->flags |= 2;
            arena_actor_clear_inputs(actor);
        }
    } else if (brain->roll % 6 == 0) {
        actor->flags |= 2;
        arena_actor_clear_inputs(actor);
    }
}

/* 8008FC7C: Start a new opponent round: idle mode, a pause that is shorter at
 * higher levels, and fresh rolls. */
void arena_brain_enter_idle_mode(Actor *actor) {
    Brain *brain = actor->brain;

    brain->unk20 = 0;
    brain->mode = 0;
    brain->timer = (2 - brain->unkF) * 30 + 90;
    arena_brain_roll_choices(brain);
}

/* 8008FCC8: The opponent's idle mode: after the retreat rule, react to closeness,
 * guard against a charging opponent, pick a fight when the round's clock
 * runs out, dodge close shots, and wait or attack. */
void arena_brain_step_idle_mode(Actor *actor, Brain *brain) {
    brain->unkC = 0;
    brain->unkE = 0;
    brain->unkA = 0;
    if (arena_brain_apply_retreat_rule(actor, brain)) {
        return;
    }
    if (arena_actors_flat_distance < 0x200) {
        if (brain->unk2C_12) {
            arena_brain_enter_attack_mode(actor);
        } else if (brain->unkF) {
            arena_brain_enter_distance_mode(actor, 1);
        }
        if (actor->opponent->unkC5 == 2) {
            arena_brain_react_with_guard(actor, brain);
        }
        if (actor->unk1668 + 2 < brain->unk30) {
            arena_brain_enter_attack_mode(actor);
        }
    }
    if (actor->opponent->nearest_dist < 0x400) {
        arena_brain_enter_distance_mode(actor, 3);
        brain->unkE = 1;
    }
    switch (brain->unk20) {
    case 0:
        if (--brain->timer < 0) {
            brain->unk20++;
        }
        if (actor->opponent->unkC4 != 0) {
            break;
        }
        if (!brain->unk2C_12) {
            break;
        }
        if (arena_brain_decide_attack(actor, 0)) {
            arena_brain_jump_attack(actor);
        }
        arena_brain_enter_approach_mode(actor, 0);
        break;
    case 1:
        if (arena_brain_check_special_charge(actor, 0)) {
            arena_brain_enter_approach_mode(actor, 0);
        }
        break;
    }
}

/* 8008FE80: Opponent command: one to three random inputs (1 or 2). */
void arena_brain_queue_random_attacks(Actor *actor) {
    s32 roll = rand() % 10;
    s32 count = roll >= 2 ? 2 : 1;

    if (roll >= 5) {
        count++;
    }
    while (count != 0) {
        count--;
        arena_actor_queue_input(actor, (rand() & 1) + 1);
    }
}

/* 8008FF24: Opponent attack mode step: against a downed opponent maybe jump in;
 * otherwise press random inputs and wait a level-dependent time. */
void arena_brain_take_attack_step(Actor *actor, Brain *brain) {
    if (actor->opponent->unkC4 == 4) {
        if (brain->unk2C_9 && !brain->unk2C_11) {
            arena_brain_jump_attack(actor);
            brain->timer = 3;
        }
    } else {
        arena_brain_queue_random_attacks(actor);
        brain->timer = (2 - brain->unkF) * 20 + 1 + rand() % 20;
    }
    arena_brain_roll_choices(brain);
}

/* 8008FFEC: Opponent special move: enter the inputs of a random usable learned
 * move, then wait a level-dependent time. Returns 1 when it knows none. */
s32 arena_brain_use_special_move(Actor *actor, Brain *brain) {
    s32 pick;
    s32 i;

    /* pick first counts the moves, then selects one of them */
    pick = actor->move_count;
    if (pick == 0) {
        return 1;
    }
    pick = rand() % pick;
    for (i = 0; i < 14; i++) {
        if (actor->moves->learned[i] && actor->move_slots[i].usable) {
            if (pick == 0) {
                if (arena_actor_combo_inputs[i][0]) {
                    arena_actor_queue_input(actor, arena_actor_combo_inputs[i][0]);
                }
                if (arena_actor_combo_inputs[i][1]) {
                    arena_actor_queue_input(actor, arena_actor_combo_inputs[i][1]);
                }
                if (arena_actor_combo_inputs[i][2]) {
                    arena_actor_queue_input(actor, arena_actor_combo_inputs[i][2]);
                }
                break;
            }
            pick--;
        }
    }
    brain->timer = (2 - brain->unkF) * 20 + 1 + rand() % 20;
    arena_brain_roll_choices(brain);
    return 0;
}

/* 80090174: Enter the opponent's attack mode: one attack step now and a number of
 * further steps that grows with its level. */
void arena_brain_enter_attack_mode(Actor *actor) {
    Brain *brain = actor->brain;

    brain->mode = 1;
    arena_brain_take_attack_step(actor, brain);
    if (brain->unkF >= 2) {
        brain->unk9 = rand() % 8 + 1;
    } else if (brain->unkF != 0) {
        brain->unk9 = rand() % 6 + 1;
    } else {
        brain->unk9 = rand() % 4 + 1;
    }
    brain->unkC = 0;
    brain->unkE = 0;
    arena_brain_roll_choices(brain);
}

/* 80090258: Opponent attack choice: a jump attack or a special move (when it knows
 * any and is close enough). Returns 1 when it did nothing. */
s32 arena_brain_choose_attack(Actor *actor, Brain *brain) {
    if (actor->move_count != 0) {
        if (!(rand() & 1)) {
            return 1;
        }
        if (!(rand() & 1) || !arena_brain_decide_attack(actor, 0)) {
            if (arena_actors_flat_distance > 0x1000) {
                return 1;
            }
            arena_brain_use_special_move(actor, brain);
            return 0;
        }
    } else if (!arena_brain_decide_attack(actor, 0)) {
        return 1;
    }
    arena_brain_jump_attack(actor);
    return 0;
}

/* 8009031C: The opponent's attack mode: after the retreat rule and guard reactions,
 * when the step timer runs out pick the next action at random, then keep
 * attacking while steps remain or fall back to the approach mode. */
void arena_brain_step_attack_mode(Actor *actor, Brain *brain) {
    s32 dist;

    if (arena_brain_apply_retreat_rule(actor, brain)) {
        return;
    }
    if (actor->unkC5 != 2 && actor->opponent->unkC5 == 2) {
        arena_brain_react_with_guard(actor, brain);
    }
    dist = actor->opponent->nearest_dist;
    if (dist > 0x200 && dist < 0x600 && brain->unkF) {
        arena_brain_react_with_guard(actor, brain);
    }
    if (--brain->timer > 0) {
        return;
    }
    if (arena_actors_flat_distance > 0x300) {
        switch (rand() % 10) {
        case 0:
            if ((rand() & 0xFF) >= brain->unk14) {
                arena_brain_enter_distance_mode(actor, 0);
            }
            break;
        case 2:
            if (actor->move_count != 0) {
                arena_brain_use_special_move(actor, brain);
                break;
            }
            arena_brain_enter_idle_mode(actor);
            break;
        case 3:
        case 4:
        case 5:
            if (!arena_brain_choose_attack(actor, brain)) {
                break;
            }
            /* fallthrough */
        case 1:
            arena_brain_enter_idle_mode(actor);
            break;
        case 6:
        case 7:
        case 8:
        case 9:
            arena_brain_enter_approach_mode(actor, 0);
            break;
        }
    }
    if (brain->unk9--) {
        arena_brain_take_attack_step(actor, brain);
    } else {
        arena_brain_enter_distance_mode(actor, rand() & 1);
    }
}

/* 80090504: Enter the opponent's approach mode (3): a few steps, fresh rolls, and
 * whether it closes in. */
void arena_brain_enter_approach_mode(Actor *actor, s32 kind) {
    Brain *brain = actor->brain;

    brain->mode = 3;
    brain->unk9 = rand() % 4 + 1;
    brain->timer = 0;
    arena_brain_roll_choices(brain);
    brain->unkE = arena_brain_decide_close_in(actor, 1);
    brain->unk2E = 0;
}

/* 80090580: The opponent's approach mode (3) step: give up when the other actor retreated,
 * sidestep homing shots (and maybe counter-attack), attack when close,
 * and pick a new heading and duration whenever the timer runs out. */
void arena_brain_step_approach_mode(Actor *actor, Brain *brain) {
    s32 roll;

    if (arena_brain_is_in_far_quadrant(actor->opponent) && arena_retreat_rule_enabled && (actor->flags & 0x08000000)) {
        arena_brain_enter_idle_mode(actor);
        return;
    }
    if (actor->opponent->nearest_shot->steer == 1 && actor->opponent->nearest_dist < 0x500 &&
        brain->unkF) {
        brain->unkE = 1;
        brain->unkA = brain->unk2C_12 ? 0x400 : -0x400;
        if (brain->unkF >= 2 && (rand() & 0xFF) < brain->unk14 &&
            arena_brain_decide_attack(actor, 0) && brain->unk2C_12) {
            if ((rand() & 3) == 0) {
                arena_actor_queue_input(actor, 4);
            }
            arena_brain_jump_attack(actor);
            brain->unk2E = 0;
        }
        brain->unk2E++;
    }
    if (arena_actors_flat_distance < 0x180) {
        if ((rand() & 3) == 0) {
            arena_actor_queue_input(actor, 4);
        }
        arena_brain_enter_attack_mode(actor);
    }
    if (!arena_brain_compare_charge(actor)) {
        brain->unkE = 0;
    }
    if (brain->timer < 0) {
        brain->unkA = rand() % 0x600 - 0x300;
        roll = rand();
        brain->timer = (brain->unk2C_10 ? roll % 120 : roll % 100) + 10;
        brain->unkC = 0xFF;
        if (brain->unk9 != 0) {
            brain->unk9--;
        } else {
            if ((rand() & 3) == 0) {
                arena_actor_queue_input(actor, 4);
            }
            if (arena_brain_choose_attack(actor, brain)) {
                arena_brain_enter_idle_mode(actor);
            }
            brain->unk9 = rand() % 4 + 1;
        }
    }
    brain->timer--;
}

/* 80090894: Enter the opponent's distance mode (2): maybe act first, then a random
 * distance to keep and a few decisions. */
void arena_brain_enter_distance_mode(Actor *actor, s32 kind) {
    Brain *brain = actor->brain;

    brain->mode = 2;
    if (rand() % 3 == 0) {
        arena_actor_queue_back_dash(actor);
    }
    brain->unk28 = rand() % 0x600 + 0x100;
    brain->timer = 0;
    brain->unk9 = rand() % 5 + 3;
    arena_brain_roll_choices(brain);
    brain->unkE = arena_brain_decide_close_in(actor, 0);
    brain->unk2E = 0;
}

/* 80090990: The opponent's distance mode (2) step: sidestep homing shots (maybe
 * countering), use a special move once far enough (or when forced), and
 * pick a new wide heading and duration whenever the timer runs out. */
void arena_brain_step_distance_mode(Actor *actor, Brain *brain) {
    s32 roll;

    if (arena_brain_apply_retreat_rule(actor, brain)) {
        return;
    }
    if (actor->opponent->nearest_shot->steer == 1 && actor->opponent->nearest_dist < 0x500 &&
        brain->unkF) {
        brain->unkE = 1;
        brain->unkA = brain->unk2C_12 ? 0x400 : -0x400;
        if (brain->unkF >= 2 && arena_brain_decide_attack(actor, 0) && brain->unk2C_12) {
            if ((rand() & 3) == 0) {
                arena_actor_queue_input(actor, 4);
            }
            arena_brain_jump_attack(actor);
            brain->unk2E = 0;
        }
        brain->unk2E++;
    }
    if (!arena_brain_compare_charge(actor)) {
        brain->unkE = 0;
    }
    if (arena_actors_flat_distance > brain->unk28 || (arena_rubber_band_enabled && arena_actors_distance > 0x4B0)) {
        if (arena_rubber_band_enabled) {
            arena_actor_queue_input(actor, 4);
        }
        arena_brain_use_special_move(actor, brain);
        arena_brain_enter_idle_mode(actor);
    }
    if (brain->timer < 0 || brain->unkC == 0) {
        brain->unkA = rand() % 0x600 + 0x500;
        roll = rand();
        brain->timer = (brain->unk2C_10 ? roll % 40 : roll % 60) + 10;
        brain->unkC = 0xFF;
        if (brain->unk9 != 0) {
            brain->unk9--;
        } else {
            if (arena_brain_decide_attack(actor, 0)) {
                arena_actor_queue_input(actor, 4);
                arena_brain_jump_attack(actor);
            }
            brain->unk9 = rand() % 5 + 3;
        }
    }
    brain->timer--;
}

/* 80090C88: Load the opponent's four tendencies from its move list. */
void arena_brain_load_tendencies(Actor *actor) {
    MoveList *moves = actor->moves;
    Brain *brain = actor->brain;

    brain->unk10 = moves->tendency[0];
    brain->unk14 = moves->tendency[1];
    brain->unk18 = moves->tendency[2];
    brain->unk1C = moves->tendency[3];
}

/* 80090CC0: Attach and reset the opponent brain of the actor's side and start it in
 * a random mode. */
void arena_brain_attach(Actor *actor) {
    Brain *brain = &arena_brain_for_first_actor;

    if (actor->flags & 0x08000000) {
        brain = &arena_brain_for_second_actor;
    }
    actor->brain = brain;
    brain->unk6 = 0x10;
    brain->owner = actor;
    brain->timer = 0;
    brain->unk7 = 0xA;
    brain->unkA = 0;
    brain->unkC = 0;
    brain->mode = 0;
    brain->unk9 = 0;
    actor->state = 0;
    brain->unk24 = arena_brain_get_charge_after_special(actor, brain);
    brain->unkF = arena_settings.level;
    arena_brain_load_tendencies(actor);
    switch (rand() % 3) {
    case 0:
        arena_brain_enter_idle_mode(actor);
        break;
    case 1:
        arena_brain_enter_attack_mode(actor);
        break;
    case 2:
        arena_brain_enter_distance_mode(actor, 0);
        break;
    case 3:
        arena_brain_enter_approach_mode(actor, 0);
        break;
    }
    arena_actor_clear_inputs(actor);
}

/* 80090E10: Run the computer opponent for one frame when enabled: its current mode's
 * step, then dodge and guard flags and steering. */
void arena_brain_update(Actor *actor) {
    Brain *brain;

    if (actor->flags & 0x40) {
        brain = actor->brain;
        brain->unkF = arena_settings.level;
        switch (brain->mode) {
        case 0:
            arena_brain_step_idle_mode(actor, brain);
            break;
        case 1:
            arena_brain_step_attack_mode(actor, brain);
            break;
        case 2:
            arena_brain_step_distance_mode(actor, brain);
            break;
        case 3:
            arena_brain_step_approach_mode(actor, brain);
            break;
        }
        if (brain->unkE) {
            actor->flags |= 0x8000;
        }
        if (brain->defending) {
            actor->flags |= 2;
        }
        arena_brain_steer(actor, brain->unkA, brain->unkC);
    }
}
