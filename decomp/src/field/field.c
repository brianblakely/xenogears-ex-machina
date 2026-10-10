/* Field unit 8006FDEC-8007A44C: the field load, the frame and its passes
 * (models, sprites, compass), fades, camera, and the field mode entry.
 * Its rodata runs 0x0-0x9c (the overlay number first); where its text ends
 * is chosen with the next unit (see field_motion.c). */
#include "common.h"
#include "psyq/inline_c.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/libsn.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/formation.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/stream.h"
#include "resident/text.h"
#include "field/monitor.h"
#include "field.h"
#include "field_camera.h"
#include "field_dialogue.h"
#include "field_draw.h"
#include "field_effect.h"
#include "field_event.h"
#include "field_layer.h"
#include "field_load.h"
#include "field_mode.h"
#include "field_motion.h"
#include "field_movie.h"
#include "field_music.h"
#include "field_pad.h"
#include "field_panel.h"
#include "field_party.h"
#include "field_picture.h"
#include "field_resident.h"
#include "field_screen.h"
#include "field_sound.h"

/* The overlay's number, ahead of this unit's other rodata (the field is
 * mode overlay 4). */
const s32 field_overlay_number = 4; /* 8006FAF0 */

/* The field's shared state, defined here in the original order. The words
 * this unit does not read are used by the other units and the debug monitor
 * (debug595); the music table's copy above opens this data. */
s32 field_monitor_page_toggle = 0;                 /* 800ADAFC: debug monitor page toggle (debug595) */
u16 field_pad_port0_allowed_mask = 0xFFFF;         /* 800ADB00 */
s16 field_player_stuck_frames = 0;                 /* 800ADB02: frames stuck against terrain */
u8 field_encounters_enabled = 0;                   /* 800ADB04: random encounters enabled */
u8 field_characters_hidden = 0;                    /* 800ADB05: 1 while character drawing is off */
s32 field_draw_buffer_index = 0;                   /* 800ADB08: current draw buffer */
s32 field_dialogue_portrait_last_slot = 0;         /* 800ADB0C */
void *field_dialogue_portrait_first_image = NULL;  /* 800ADB10: first portrait image */
void *field_dialogue_portrait_second_image = NULL; /* 800ADB14: second portrait image */
s32 field_skip_exit_snapshot = 0;                  /* 800ADB18 */
s32 field_event_runs_per_frame = 0;                /* 800ADB1C: 0 while event scripts run to their end at once */
void *field_layer_module = NULL;                   /* 800ADB20: the 801e module */
s32 field_distortion_buffers_allocated = 0;        /* 800ADB24: screen effect buffers allocated */
s32 field_unread_jump_start_history_index = 0;     /* 800ADB28: latched jump setting */
s32 field_music_stream_running = 0;                /* 800ADB2C */
u32 field_heap_top = 0;                            /* 800ADB30: heap top */
s32 field_vram_column_saved = 0;                   /* 800ADB34 */
s32 field_transition_kind = 0;                     /* 800ADB38: requested transition */
s32 field_transition_frames = 0;                   /* 800ADB3C: transition operand */
s32 field_effect_template_actor = 0xFF;            /* 800ADB40 */
s32 field_unread_effect_last_owner = 0;            /* 800ADB44: last effect owner */
s16 field_compass_needle_heading = 0;              /* 800ADB48: needle heading */
s16 field_compass_needle_goal = 0;                 /* 800ADB4A: needle goal */
s32 field_draw_second_ot_enabled = 0;              /* 800ADB4C */
s32 field_panorama_hidden = 0;                     /* 800ADB50 */
s16 field_wide_overlay_shown = 0;                  /* 800ADB54 */
s32 field_morph_list_descriptor = 0;               /* 800ADB58: descriptor whose list is read */
s32 field_morph_list_index = 0;                    /* 800ADB5C: list position */
s32 field_map_stream_running = 0;                  /* 800ADB60: field stream running */
s32 field_menu_request = 0xFF;                     /* 800ADB64: requested menu (scripts 0-6, menu button 0x80), 0xff none */
s32 field_player_control_polled = 0;               /* 800ADB68: pad input polled this pass */
s32 field_movie_stopped = 0;                       /* 800ADB6C: movie stopped */
s32 field_movie_requested = 0;                     /* 800ADB70: movie requested */
s32 field_movie_mode = 0;                          /* 800ADB74: movie mode */
s32 field_movie_decoded_block = 0;                 /* 800ADB78 */
s32 field_movie_presenting = 0;                    /* 800ADB7C */
s32 field_movie_fade_bits = 0;                     /* 800ADB80 */
s32 field_movie_end_count = 0;                     /* 800ADB84 */
s32 field_exit_request_pending = 0;                /* 800ADB88 */
s32 field_party_rebuilding = 0;                    /* 800ADB8C */
s32 field_actor_block_loading = 0;                 /* 800ADB90 */
s32 field_ground_override_height = 0;              /* 800ADB94: the floor actors stand on while the override is on */
s32 field_ground_override_enabled = 0;             /* 800ADB98 */
s32 field_frame_start_time = 0;                    /* 800ADB9C: frame start time */
s32 field_frame_cpu_time = 0;                      /* 800ADBA0: frame draw (CPU) time */
s32 field_frame_gpu_time = 0;                      /* 800ADBA4: GPU time */
s32 field_camera_target_at_edge = 0;               /* 800ADBA8 */
s32 field_camera_settle_frames = 0;                /* 800ADBAC: camera frames settling */
s32 field_camera_release_frames = 0;               /* 800ADBB0: camera frames releasing */
s32 field_screen_band_upload_pending = 0;          /* 800ADBB4 */
void *field_music_stream_ring = NULL;              /* 800ADBB8: music-wave stream ring */
s32 field_music_stream_arrival_count = 0;          /* 800ADBBC: stream arrivals */
void *field_party_sprite_load_buffer = NULL;       /* 800ADBC0: pending party sprite buffer */
s32 field_party_sprite_load_pending = 0xFF;        /* 800ADBC4 */
s32 field_party_sprite_load_member = 0;            /* 800ADBC8 */
s32 field_party_sprite_load_slot = 0;              /* 800ADBCC: pending party slot */
s32 field_encounter_battle_pending = 0;            /* 800ADBD0 */
s32 field_encounter_music_started = 0;             /* 800ADBD4 */
s32 field_mode_exit_not_requested = 0;             /* 800ADBD8 */
s32 field_battle_not_requested = 0;                /* 800ADBDC */
s32 field_scripted_battle_not_requested = 0;       /* 800ADBE0 */
s32 field_worldmap_exit_not_requested = 0;         /* 800ADBE4 */
s32 field_arena_exit_not_requested = 0;            /* 800ADBE8 */
s32 field_map_change_not_requested = 0;            /* 800ADBEC: publish the field id on the next walk */
void *field_message_table = NULL;                  /* 800ADBF0: field message table */
Zone *field_trigger_zones = NULL;                  /* 800ADBF4: trigger zones */
EventPackage *field_event_package = NULL;          /* 800ADBF8 */
s32 field_event_actor_count = 0;                   /* 800ADBFC: event actor count */
u8 *field_event_bytecode = NULL;                   /* 800ADC00: event bytecode */
s32 field_fade_mode = 2;                           /* 800ADC04: fade mode; fades start only in mode 2 */
s16 field_faded_out = 1;                           /* 800ADC08: fade started */
s32 field_update_ran = 0;                          /* 800ADC0C */
s32 field_scratchpad_used_words = 0;               /* 800ADC10: scratchpad words in use */
void *field_map_stream_ring = NULL;                /* 800ADC14: field stream ring */
s32 field_camera_cut_timer = 0;                    /* 800ADC18 */

/* Octant bits. */
u8 field_camera_octant_bits[8] = {0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08}; /* 800ADC1C */

/* The compass: the heading octant bit of each palette row and the letters'
 * x, z offsets. */
u16 field_compass_row_octant_bits[8] = {0x81, 0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03}; /* 800ADC24 */
DVECTOR field_compass_letter_offsets[4] = {{0, 0x500}, {0x500, 0}, {0, -0x500}, {-0x500, 0}}; /* 800ADC34 */

/* Where the text images go: x, y, palette x, y, w, h per image. Nine rows;
 * 80077620 loads the first eight. */
s16 field_text_image_places[9 * 6] = { /* 800ADC44 */
    0x2A0, 0x1C0, 0,     0xFB, 0,    0,
    0x280, 0x1E0, 0x100, 0xF3, 0x10, 1,
    0x29C, 0x1C0, 0x100, 0xF5, 0,    0,
    0x280, 0x1C0, 0x100, 0xF2, 0,    0,
    0x280, 0x1F0, 0x100, 0xF4, 0x10, 1,
    0x3C0, 0x140, 0x100, 0xF7, 0x10, 1,
    0x298, 0x1C0, 0x100, 0xF6, 0x10, 1,
    0x288, 0x1C0, 0x100, 0xF6, 0x10, 1,
    0x380, 0x100, 0,     0xE8, 0x10, 1,
};

/* The VRAM blocks the menu overwrites (x, y pairs) and where they are saved
 * meanwhile. */
s16 field_menu_vram_blocks[12] = {0, 0xE0, 0x40, 0xE0, 0x80, 0xE0, 0xC0, 0xE0, 0x100, 0xE0, 0x100, 0x1E0}; /* 800ADCB0 */
s16 field_menu_vram_save_places[12] = {0x2C0, 0, 0x2C0, 0x20, 0x2C0, 0x40, 0x2C0, 0x60, 0x2C0, 0x80, 0x2C0, 0xA0}; /* 800ADCC8 */

/* 8006FDEC: Build the camera matrix from the eye, target and up vectors, the world
 * matrix under it, then the three lights and background color from the
 * field's view record, and the light matrix under the world matrix. */
void field_init_view_and_lights(s16 *record) {
    long flag;

    field_camera_build_lookat_matrix(&field_view.previous_view, &field_view.eye, &field_view.target, &field_view.up);
    gpu_build_rotation_matrix(&field_view.world_angles, &field_view.scaled_world);
    MulMatrix2(&field_view.previous_view, &field_view.scaled_world);

    field_view.lights[0].vx = *record++;
    field_view.lights[0].vy = *record++;
    field_view.lights[0].vz = *record;
    record += 2;
    field_view.lights[0].r = *record++ << 3;
    field_view.lights[0].g = *record++ << 3;
    field_view.lights[0].b = *record << 3;
    record += 2;
    model_set_light(0, &field_view.lights[0]);

    field_view.lights[1].vx = *record++;
    field_view.lights[1].vy = *record++;
    field_view.lights[1].vz = *record;
    record += 2;
    field_view.lights[1].r = *record++ << 3;
    field_view.lights[1].g = *record++ << 3;
    field_view.lights[1].b = *record << 3;
    record += 2;
    model_set_light(1, &field_view.lights[1]);

    field_view.lights[2].vx = *record++;
    field_view.lights[2].vy = *record++;
    field_view.lights[2].vz = *record;
    record += 2;
    field_view.lights[2].r = *record++ << 3;
    field_view.lights[2].g = *record++ << 3;
    field_view.lights[2].b = *record << 3;
    field_view.lights[1] = field_view.lights[0];
    field_view.lights[2] = field_view.lights[0];
    record += 2;
    model_set_light(2, &field_view.lights[2]);

    field_view.back_color[0] = record[0] << 4;
    field_view.back_color[1] = record[1] << 4;
    field_view.back_color[2] = record[2] << 4;
    SetRotMatrix(&field_view.previous_view);
    SetTransMatrix(&field_view.previous_view);
    RotTrans(&field_view.anchor, (VECTOR *)field_view.scaled_world.t, &flag);
    model_load_light_matrix(&field_view.scaled_world);
    SetRotMatrix(&field_view.scaled_world);
    SetTransMatrix(&field_view.scaled_world);
}

/* 8007008C: Decode compressed `source` data into `destination`. */
void field_decompress_component(s32 unused, void *source, void *destination) {
    text_unpack_lzss(source, destination);
}

/* 800700B0: Tear the field down: reset the GPU, flush both draw buffers, then release
 * every model instance, the loaded components, the text windows and the
 * 801e module's buffers. */
void field_teardown(void) {
    s32 i;
    s32 next;
    FieldInstance *instance;
    s32 *module_loaded;

    ResetGraph(1);
    task_destroy_all();
    for (i = 0; i < 2;) {
        sprite_queue_run_uploads();
        DrawSync(0);
        next = i + 1;
        sprite_queue_start_fill((field_draw_buffer_index + next) & 1);
        i = next;
        sprite_queue_run_uploads();
        DrawSync(0);
        sprite_free_queues();
    }

    for (i = 0; i < field_view.components.descriptor_count; i++) {
        field_actor_release(i);
        if (!(field_view.components.descriptors[i].flags & 0x40)) {
            instance = field_view.components.descriptors[i].instance;
            if (field_view.components.descriptors[i].flags & 0x2000) {
                model_stop_morph(instance->anims);
            }
            model_free_owned_block((ModelBuffer *)instance->mesh);
            heap_free(instance->packets[0]);
            heap_free(field_view.components.descriptors[i].instance);
        }
    }
    field_distortion_stop();
    heap_free(field_view.components.descriptors);
    heap_free(field_trigger_zones);
    heap_free(field_message_table);
    heap_free(field_event_package);
    heap_free(field_view.components.collision);
    heap_free(field_view.components.geometry);
    heap_free(field_view.components.sprites);
    if (field_panorama_parameters.enabled != 0) {
        gpu_free_panorama(field_panorama);
    }
    for (i = 0; i < field_texture_scrolls.count; i++) {
        gpu_free_texture_scroll(field_texture_scrolls.scrolls[i]);
        heap_free(field_texture_scrolls.buffers[i]);
        heap_free(field_texture_scrolls.scrolls[i]);
    }
    console_close();
    module_loaded = &field_work.unk2264;
    field_texture_scrolls.count = 0;
    if (*module_loaded != 0) {
        gear_model_shut_down();
        heap_free(field_layer_module);
        field_sync_and_flush_cache();
    }
    *module_loaded = 0;
    heap_free_tag(3);
    field_status_panel_release();
}

/* 80070340: Load a TIM's image at (x, y) and its CLUT at (clut_x, clut_y) with the
 * given size; a CLUT y of -1 or a zero size keeps the TIM's own. */
void field_load_tim_at(u32 *tim, s16 x, s16 y, s16 clut_x, s16 clut_y, s16 clut_w, s16 clut_h) {
    TIM_IMAGE image;

    OpenTIM((u_long *)tim);
    if (ReadTIM(&image) != NULL) {
        if (image.caddr != NULL) {
            if (clut_y != -1) {
                image.crect->x = clut_x;
                image.crect->y = clut_y;
            }
            if (clut_w != 0) {
                image.crect->w = clut_w;
            }
            if (clut_h != 0) {
                image.crect->h = clut_h;
            }
            LoadImage(image.crect, image.caddr);
        }
        /* Only the x placement is guarded; the image is always loaded. */
        if (image.paddr != NULL) {
            image.prect->x = x;
        }
        image.prect->y = y;
        LoadImage(image.prect, image.paddr);
    }
}

/* 80070488: Start the map's own stream (file 0xb9 + 2 * map) into a four-sector ring,
 * unless one already runs. */
void field_map_stream_start(void) {
    void *ring;

    if (field_map_stream_running == 0) {
        field_map_stream_running = 1;
        field_map_stream_ring = ring = stream_create_ring(4, 1);
        stream_start_image_load((mode_field_map_id & 0xFFF) * 2 + 0xB9, ring, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

/* 80070508: Stop the field stream and release its ring, then continue with 80078c5c. */
void field_map_stream_stop(void) {
    if (field_map_stream_running == 1) {
        cd_sync_reads(0);
        DrawSync(0);
        heap_free(field_map_stream_ring);
        field_map_stream_running = 0;
    }
    field_brighten_text_strip();
}

/* 80070560: Widen a short vector to 16.16 fixed point. */
void field_widen_svector_to_fixed(VECTOR *out, SVECTOR *in) {
    out->vx = in->vx << 16;
    out->vy = in->vy << 16;
    out->vz = in->vz << 16;
}

/* 80070594: Identity rotation with a zero translation. */
void field_matrix_set_identity(MATRIX *m) {
    SVECTOR angles;

    angles.vx = 0;
    angles.vy = 0;
    angles.vz = 0;
    gpu_build_rotation_matrix(&angles, m);
    m->t[2] = 0;
    m->t[1] = 0;
    m->t[0] = 0;
}

/* 800705DC: Reset the field state for a new map: flags, counters, the screen and
 * camera defaults, the event variables from the saved game, and the view
 * matrices. */
void field_reset_state(void) {
    VECTOR unused; /* unreferenced, but part of the frame */
    SVECTOR angles;
    s32 i;

    if (field_monitor_absent == 0) {
        field_debug_clear_counters();
    }
    field_draw_set_clip_areas(0, 0, 0x140, 0xE0);
    pad_clear_queue();
    field_pad_port0_allowed_mask = 0xFFFF;
    field_pad_port0_held = 0;
    field_pad_port1_held = 0;
    field_pad_port0_pressed = 0;
    field_pad_unread_port1_pressed = 0;
    field_pad_port0_repeated = 0;
    field_pad_port1_repeated = 0;
    field_work.unk2356 = 5;
    field_work.animation_mode = 3;
    field_work.unk234A = 0x40;
    field_work.gear_riding_lock_override = 0xFF;
    FIELD_MOVIE.sound_bank = 0xFF;
    field_model_cull_margin_y = 0;
    field_model_cull_margin_x = 0;
    field_work.unk21BC[0] = 0;
    field_work.unk21BC[1] = 0;
    field_work.unk21BC[2] = 0;
    field_work.unk2264 = 0;
    field_work.last_sound_effect = 0;
    field_work.script_control[1] = 0;
    field_work.script_control[0] = 0;
    field_work.effect_value[5] = 0;
    field_work.effect_value[4] = 0;
    field_work.effect_value[3] = 0;
    field_work.effect_value[2] = 0;
    field_work.effect_value[1] = 0;
    field_work.effect_value[0] = 0;
    field_work.unk20B0[1] = 0;
    field_work.unk20B0[0] = 0;
    field_work.unk2268 = 0;
    field_work.preserve_nonplayer_motion = 0;
    field_work.piece_drift[1] = 0;
    field_work.piece_drift[2] = 0;
    field_work.piece_drift[0] = 0;
    field_work.unk2078 = 0;
    field_work.camera_floor_fixed = 0;
    field_work.party_bits = 0;
    field_work.clear_color[2] = 0;
    field_work.clear_color[1] = 0;
    field_work.clear_color[0] = 0;
    field_work.party_processing_mode = 0;
    field_work.forced_position = 0;
    field_work.piece_drift_mode = 0;
    field_movie_mode = 0;
    field_draw_second_ot_enabled = 0;
    field_actor_block_loading = 0;
    field_distortion_buffers_allocated = 0;
    field_ground_override_enabled = 0;
    field_ground_override_height = 0;
    field_movie_requested = 0;
    field_music_stream_running = 0;
    field_player_control_polled = 0;
    field_exit_request_pending = 0;
    field_camera_target_at_edge = 0;
    field_exit_game_mode = 0;
    field_skip_exit_snapshot = 0;
    field_panorama_hidden = 0;
    field_no_panorama_after_return = 0;
    field_dialogue_open_blocked = 0;
    field_play_record_stopped = 0;
    field_encounter_music_started = 0;
    field_encounter_battle_pending = 0;
    field_work.unk22E0 = 0;
    field_work.effects_kept = 0;
    field_unread_cleared_word = 0;
    field_menu_parameter = 0;
    field_transition_kind = 0;
    field_transition_frames = 0;
    field_map_change_frames = 0x20;
    field_map_change_kind = 2;
    field_work.emitter_range = 0x3FF;
    field_camera_cut_timer = 4;
    field_unread_effect_last_owner = 0;
    field_player_stuck_frames = 0;
    field_work.unk233E = 0;
    field_work.jump_mode = 0;
    field_work.repeat_remaining = 0;
    field_work.unk2348 = 0;
    field_work.followers_idle = 0;
    field_work.unk2355 = 0;
    field_encounters_enabled = 0;
    field_characters_hidden = 0;
    field_work.unk2357 = 0;
    field_work.unk2354 = 0;
    field_screen_band_upload_pending = 0;
    field_work.unk2350 = 0;
    field_wide_overlay_shown = 0;
    field_work.unk2358 = 0;
    field_movie_end_count = 0;
    field_movie_presenting = 0;
    field_party_rebuilding = 0;
    field_menu_request = 0xFF;
    angles.vx = 0;
    angles.vy = 0;
    angles.vz = 0;
    gpu_build_rotation_matrix(&angles, &field_instance_cull_matrix);
    for (i = 0; i < 3; i++) {
        field_work.emitter_descriptor[i] = -1;
    }
    field_view.scripted_scale = 0x1000;
    field_faded_out = 1;
    field_work.unk21D4 = 0x720;
    field_work.unk21A0[2] = 0x100;
    field_work.unk21A0[1] = 0x100;
    field_work.unk21A0[0] = 0x100;
    field_work.unk21A0[5] = 0x200;
    field_work.unk21A0[4] = 0x200;
    field_work.unk21A0[3] = 0x200;
    field_work.unk21B4 = 0x80;
    field_party_sprite_load_pending = 0xFF;
    field_work.scale = 0x1000;
    field_work.sprite_angles.vx = 0;
    field_work.sprite_angles.vy = 0;
    field_work.sprite_angles.vz = 0;
    field_movie_stopped = -1;
    for (i = 0; i < 16; i++) {
        field_work.encounter_music[i] = 0x1D;
    }
    field_work.battle_music = 0x1D;
    field_map_change_not_requested = -1;
    field_work.camera_counter = 2;
    field_work.input_mask = 0xFFFF;
    field_work.fog_color[2] = 0x80;
    field_work.fog_color[1] = 0x80;
    field_work.fog_color[0] = 0x80;
    field_work.far_color[2] = 0xFF;
    field_work.far_color[1] = 0xFF;
    field_work.far_color[0] = 0xFF;
    field_work.fog_range[0] = 0x15E0;
    field_work.fog_range[1] = 0x300C;
    field_draw_buffer_index = 0;
    field_update_ran = 0;
    field_texture_scrolls.count = 0;
    field_work.controlled = 0;
    field_work.terrain_angle = 0;
    field_work.open_windows = 0;
    field_work.encounter_inhibition = 0;
    field_work.unk2180 = 0;
    field_work.unk217C = 0;
    field_work.sprite_gate = 0;
    field_work.text_speed = 8;
    if (mode_field_return_pending == 0) {
        for (i = 0; i < 3; i++) {
            mode_party_actors[i] = 0xFF;
            mode_party_stand_in_actors[i] = 0xFF;
        }
    }
    for (i = 0; i < 32; i++) {
        field_work.unk22A0[i] = 0xFFFF;
    }
    field_work.unk229C = 0;
    field_work.unk2298 = 0;
    model_ot_depth_shift = 2;
    for (i = 0; i < 0x200; i++) {
        field_event_variables[i] = game_current_data->vars[i];
        field_event_variables[i + 0x200] = 0;
    }
    SetGeomScreen(0x200);
    field_matrix_set_identity(&field_view.previous_view);
    field_matrix_set_identity(&field_camera_next_view);
    field_matrix_set_identity(&field_view.scaled_world);
    field_matrix_set_identity(&field_view.world_matrix);
    field_view.world_angles.vx = 0;
    field_view.world_angles.vy = 0;
    field_view.world_angles.vz = 0;
    field_view.anchor.vx = 0;
    field_view.anchor.vy = 0;
    field_view.anchor.vz = 0;
    field_view.scale = 0x3000;
    gpu_build_rotation_matrix(&field_view.world_angles, &field_view.scaled_world);
    field_current_draw_block = &field_draw_blocks[0];
    field_camera_init();
    field_dialogue_reset_portrait_slots();
    field_sound_clear_emitter_slots();
    field_effect_clear_slots();
    field_wide_overlay_init();
}

/* 80070C84: Reset the three slots at 800b06a4 and clear 800adb0c. */
void field_dialogue_reset_portrait_slots(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        field_dialogue_portrait_slots[i].a = 0xFF;
        field_dialogue_portrait_slots[i].b = 0xFF;
    }
    field_dialogue_portrait_last_slot = 0;
}

/* 80070CC8: Load the field from the map bundle read ahead: reset the field state, take
 * the sprite slot table, build the compass quads, decode each component
 * (palettes, images, models, encounter set, events, zones, collision,
 * sprites) into heap blocks, the encounter set into formation_encounter_set, place the
 * view, create every descriptor's model instance and actor, then initialise
 * the event layer, the camera goals and the actors' facings.
 * One word pointer serves as the palette block and then walks the model
 * and collision offset tables.
 * The chained unk90 = unkA0[0] store keeps the unk90 address pseudo first
 * (it is expanded before the inner store) while storing unkA0[0] first.
 * One offset local holds the descriptor's model index and then the mesh's
 * offset in the model block (it dies twice, so the sum stays out of the
 * mesh argument's register). */
void field_load_from_bundle(void) {
    VECTOR unused = {0, -100, 2000, 0};
    s32 *table;
    s32 *data;
    s32 *images;
    s32 *entry;
    s32 offset;
    s32 size;
    s32 count;
    s32 i;
    s32 j;
    u16 x;
    u16 y;
    u16 *record;
    FieldInstance *instance;

    field_reset_state();
    field_sprite_slots = FIELD_BUNDLE->slots;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            field_compass_build_grid_quad(&field_compass_markers[i * 4 + j], j, i, 0);
        }
    }
    field_compass_build_grid_quad(&field_compass_markers[16], 4, 4, 1);
    field_compass_build_grid_quad(&field_compass_markers[17], 5, 5, 1);
    field_compass_build_grid_quad(&field_compass_markers[18], 6, 6, 1);
    field_compass_build_grid_quad(&field_compass_markers[19], 7, 7, 1);
    field_compass_build_grid_quad(&field_compass_markers[20], 8, 8, 1);
    field_compass_build_quadrant_quads();

    size = BUNDLE_SIZE(BUNDLE_PALETTES) + 0x10;
    data = heap_alloc(size, 1);
    field_decompress_component(size, BUNDLE_COMPONENT(BUNDLE_PALETTES), data);
    entry = data;
    count = *entry;
    for (i = 0; i < count; i++) {
        entry++;
        field_load_tim_list((u32 *)(*entry + (s32)data));
    }

    size = BUNDLE_SIZE(BUNDLE_IMAGES) + 0x10;
    images = heap_alloc(size, 0);
    field_decompress_component(size, BUNDLE_COMPONENT(BUNDLE_IMAGES), images);
    entry = images;
    count = *entry;
    for (i = 0; i < count; i++) {
        x = field_sprite_slots.slot[i].x;
        y = field_sprite_slots.slot[i].y;
        entry++;
        if (field_sprite_slots.slot[i].shared == 0) {
            sprite_upload_images_side_by_side((void *)(*entry + (s32)images), x, y);
        }
    }
    DrawSync(0);
    heap_free(data);
    heap_free(images);

    size = BUNDLE_SIZE(BUNDLE_MODELS) + 0x10;
    field_view.components.geometry = heap_alloc(size, 0);
    field_decompress_component(size, BUNDLE_COMPONENT(BUNDLE_MODELS), field_view.components.geometry);
    data = field_view.components.geometry;
    for (i = 0; i < *field_view.components.geometry; i++) {
        data++;
        model_relocate_group((void *)(*data + (s32)field_view.components.geometry));
    }

    field_decompress_component(BUNDLE_SIZE(BUNDLE_ENCOUNTERS) + 0x10,
                  BUNDLE_COMPONENT(BUNDLE_ENCOUNTERS), &formation_encounter_set);

    size = BUNDLE_SIZE(BUNDLE_EVENTS) + 0x10;
    field_event_package = heap_alloc(size, 0);
    field_decompress_component(size, BUNDLE_COMPONENT(BUNDLE_EVENTS), field_event_package);
    field_event_actor_count = field_event_package->count;
    field_event_bytecode = (u8 *)field_event_package->entries;
    field_event_bytecode += field_event_actor_count * 64;

    size = BUNDLE_SIZE(BUNDLE_ZONES) + 0x10;
    field_trigger_zones = heap_alloc(size, 0);
    field_decompress_component(size, BUNDLE_COMPONENT(BUNDLE_ZONES), field_trigger_zones);

    size = BUNDLE_SIZE(BUNDLE_MESSAGES) + 0x10;
    field_message_table = heap_alloc(size, 0);
    field_decompress_component(size, BUNDLE_COMPONENT(BUNDLE_MESSAGES), field_message_table);

    size = BUNDLE_SIZE(BUNDLE_COLLISION) + 0x10;
    field_view.components.collision = heap_alloc(size, 0);
    field_decompress_component(size, BUNDLE_COMPONENT(BUNDLE_COLLISION), field_view.components.collision);
    field_view.components.layer_count = *field_view.components.collision;
    data = field_view.components.collision;
    data++;
    for (i = 0; i < 4; i++) {
        field_view.components.triangle_counts[i] = (u32)*data++ / sizeof(CollisionTriangle);
    }
    field_view.components.collision_attributes = (Attribute *)(*data++ + (s32)field_view.components.collision);
    for (i = 0; i < field_view.components.layer_count; i++) {
        field_view.components.collision_triangles[i] =
            (CollisionTriangle *)(*data++ + (s32)field_view.components.collision);
        field_view.components.collision_vertices[i] = (void *)(*data++ + (s32)field_view.components.collision);
    }
    field_unread_collision_attribute_count = (Attribute *)field_view.components.collision_triangles[0] - field_view.components.collision_attributes;

    size = BUNDLE_SIZE(BUNDLE_SPRITES) + 0x10;
    field_view.components.sprites = heap_alloc(size, 0);
    field_decompress_component(size, BUNDLE_COMPONENT(BUNDLE_SPRITES), field_view.components.sprites);

    field_view.bounds[0] = 1;
    field_view.bounds[1] = 1;
    field_view.bounds[2] = 1;
    field_view.bounds[3] = 1;
    field_init_view_and_lights(FIELD_BUNDLE->view);

    count = FIELD_BUNDLE->descriptor_count;
    record = FIELD_BUNDLE->descriptors;
    field_view.components.descriptor_count = count;
    table = heap_alloc(count * sizeof(FieldDescriptor), 0);
    field_view.components.descriptors = (FieldDescriptor *)table;
    size = count * (s32)(sizeof(FieldDescriptor) / sizeof(s32));
    for (i = 0; i < size; i++) {
        *table++ = 0;
    }
    for (i = 0; i < field_view.components.descriptor_count; i++) {
        field_view.components.descriptors[i].flags = *record++;
        field_view.components.descriptors[i].rotation.vx = record[0];
        field_view.components.descriptors[i].rotation.vy = record[1];
        field_view.components.descriptors[i].rotation.vz = record[2];
        record += 3;
        field_view.components.descriptors[i].transform.t[0] = field_view.components.descriptors[i].matrix.t[0] = record[0];
        field_view.components.descriptors[i].transform.t[1] = field_view.components.descriptors[i].matrix.t[1] = record[1];
        field_view.components.descriptors[i].transform.t[2] = field_view.components.descriptors[i].matrix.t[2] = record[2];
        record += 3;
        if (!(field_view.components.descriptors[i].flags & 0x40)) {
            instance = heap_alloc(0x24, 0);
            field_view.components.descriptors[i].instance = instance;
            offset = *record;
            entry = (s32 *)(offset * 4 + (s32)field_view.components.geometry);
            offset = entry[1] + (s32)field_view.components.geometry;
            instance->mesh = (SpriteModel *)(offset + 0x10);
            model_alloc_packet_buffers(instance->mesh, &instance->packets[0], &instance->packets[1]);
            model_build_packets(instance->mesh, instance->packets[0], (field_view.components.descriptors[i].flags & 0xC) >> 2);
            memcpy(instance->packets[1], instance->packets[0], instance->mesh->packet_size);
            if (field_view.components.descriptors[i].flags & 0x2000) {
                heap_select_owner_tag(3, 0);
                instance->anims = model_start_morph(instance->mesh, 0);
                heap_select_owner_tag(8, 0);
            }
            model_trim_group((ModelGroup *)instance->mesh);
            field_actor_create(i);
        } else {
            field_view.components.descriptors[i].flags |= 0x20;
            field_view.components.descriptors[i].rotation.vx = 0;
            field_view.components.descriptors[i].rotation.vy = 0;
            field_view.components.descriptors[i].rotation.vz = 0;
            field_actor_create(i);
        }
        record++;
    }
    if (field_monitor_absent == 0) {
        field_debug_reset_lines();
    }
    field_dialogue_reset_windows();
    field_fade_init_channels();
    heap_unprotect_block(mode_read_ahead_block);
    heap_free(mode_read_ahead_block);
    heap_select_owner_tag(5, 0);
    sprite_alloc_queues(0x3C00, 0);
    task_clear_lists();
    heap_select_owner_tag(8, 0);
    field_matrix_set_rotation((MATRIX *)field_work.unk223C, 0x800, 0, 0, 0x800, 0, 0, 0x800, 0, 0);
    field_matrix_set_rotation((MATRIX *)field_work.unk221C, 0x1F8, -0xFC1, -0x1F8, 0, 0, 0, 0, 0, 0);
    field_work.unk225C[2] = 0x1E;
    field_work.unk225C[1] = 0x1E;
    field_work.unk225C[0] = 0x1E;
    field_panorama_parameters.unk80[2] = 0x140;
    field_panorama_parameters.unk80[7] = 0;
    field_panorama_parameters.unkA8[1] = 0;
    field_panorama_parameters.unkA8[0] = 0;
    field_panorama_parameters.unkA4[2] = 0;
    field_panorama_parameters.unkA4[1] = 0;
    field_panorama_parameters.unkA4[0] = 0;
    field_panorama_parameters.unkA0[2] = 0;
    field_panorama_parameters.unkA0[1] = 0;
    field_panorama_parameters.unk90 = field_panorama_parameters.unkA0[0] = 0;
    field_panorama_parameters.unk98 = 0x1000;
    field_panorama_parameters.unkB0 = 0;
    field_panorama_parameters.unkAE = 0;
    field_panorama_parameters.unkAC = 0;
    field_panorama_parameters.unk80[6] = 0;
    field_panorama_parameters.unk80[5] = 0;
    field_panorama_parameters.unk80[4] = 0;
    field_panorama_parameters.unk80[3] = 0;
    field_panorama_parameters.unk80[1] = 0;
    field_panorama_parameters.unk80[0] = 0;
    field_panorama_parameters.enabled = 0;
    field_panorama_parameters.unk94 = 0;
    field_panorama_parameters.unkA8[2] = 0x20;
    field_event_runs_per_frame = 0;
    field_event_init_actors();
    field_event_runs_per_frame = 1;
    gpu_build_rotation_matrix(&field_work.sprite_angles, &field_sprite_view_matrix);
    field_sprite_view_matrix.t[2] = 0;
    field_sprite_view_matrix.t[1] = 0;
    field_sprite_view_matrix.t[0] = 0;
    if (field_panorama_parameters.enabled != 0) {
        field_panorama = gpu_create_panorama(field_panorama_parameters.unk80[0], field_panorama_parameters.unk80[1], field_panorama_parameters.unk80[2], field_panorama_parameters.unk80[3],
                                   field_panorama_parameters.unk80[4], field_panorama_parameters.unk80[5], field_panorama_parameters.unk80[6], field_panorama_parameters.unk80[7],
                                   &field_panorama_parameters.unk90, field_panorama_parameters.unkA0, field_panorama_parameters.unkAC, field_panorama_parameters.unkAE,
                                   field_panorama_parameters.unkB0);
    }
    heap_select_owner_tag(8, 0);
    field_view.target_goal.vx = field_view.components.descriptors[field_work.unk233E].matrix.t[0] << 16;
    field_view.target_goal.vy = field_view.components.descriptors[field_work.unk233E].matrix.t[1] << 16;
    field_view.target_goal.vz = field_view.components.descriptors[field_work.unk233E].matrix.t[2] << 16;
    for (i = 0; i < field_view.components.descriptor_count; i++) {
        gpu_build_rotation_matrix(&field_view.components.descriptors[i].rotation, &field_view.components.descriptors[i].matrix);
        field_view.components.descriptors[i].transform = field_view.components.descriptors[i].matrix;
    }
    field_layer_load_and_start();
    field_reload_actor_blocks();
    mode_read_ahead_slot = -1;
    mode_read_ahead_map = -1;
    field_instance_refresh_bounds_modes();
    field_party_place_at_controlled();
    if (field_panorama_parameters.enabled == 0) {
        field_draw_second_ot_enabled = field_has_second_ot_instances();
    } else {
        field_draw_second_ot_enabled = 1;
    }
    for (i = 0; i < field_event_actor_count; i++) {
        if (field_view.components.descriptors[i].flags & 0x40) {
            if (!(field_view.components.descriptors[i].actor->layer_flags & 0x01000000)) {
                sprite_set_facing(field_view.components.descriptors[i].model,
                              field_view.view_angle + field_view.components.descriptors[i].actor->unk108);
            } else {
                sprite_set_direction(field_view.components.descriptors[i].model,
                              field_view.components.descriptors[i].actor->unk108);
            }
        }
    }
}

/* 80071A64: Initialise both fade channels' primitives. */
void field_fade_init_channels(void) {
    field_fade_init_channel(0);
    field_fade_init_channel(1);
}

/* 80071A8C: Advance one fade channel a step; a finished channel whose levels reached
 * zero turns off unless a fade-out is still in progress. */
void field_fade_step_channel(s32 channel) {
    if (field_work.fades[channel].active != 0) {
        if (field_work.fades[channel].steps <= 0) {
            field_work.fades[channel].steps = 0;
            if (field_faded_out != 1 && field_work.fades[channel].level[0] == 0 &&
                field_work.fades[channel].level[1] == 0 && field_work.fades[channel].level[2] == 0) {
                field_work.fades[channel].active = 0;
            }
        } else {
            field_work.fades[channel].level[0] = field_work.fades[channel].level[0] + field_work.fades[channel].step[0];
            if (field_work.fades[channel].level[0] >> 8 >= 0x100) {
                field_work.fades[channel].level[0] = 0xFF00;
            }
            if (field_work.fades[channel].level[0] < 0) {
                field_work.fades[channel].level[0] = 0;
            }
            field_work.fades[channel].level[1] = field_work.fades[channel].level[1] + field_work.fades[channel].step[1];
            if (field_work.fades[channel].level[1] >> 8 >= 0x100) {
                field_work.fades[channel].level[1] = 0xFF00;
            }
            if (field_work.fades[channel].level[1] < 0) {
                field_work.fades[channel].level[1] = 0;
            }
            field_work.fades[channel].level[2] = field_work.fades[channel].level[2] + field_work.fades[channel].step[2];
            if (field_work.fades[channel].level[2] >> 8 >= 0x100) {
                field_work.fades[channel].level[2] = 0xFF00;
            }
            if (field_work.fades[channel].level[2] < 0) {
                field_work.fades[channel].level[2] = 0;
            }
            field_work.fades[channel].steps = field_work.fades[channel].steps - 1;
        }
    }
}

/* 80071CB4: Step both fade channels while fading, then draw them into `ot`. */
void field_fade_update_channels(void *ot, s32 buffer) {
    if (field_fade_mode == 2) {
        field_fade_step_channel(0);
        field_fade_step_channel(1);
    }
    field_fade_link_channels(ot, field_draw_buffer_index);
}

/* 80071D08: Start a fade on `channel` towards (red, green, blue) over `steps` frames. */
void field_fade_start(s32 channel, s32 steps, s32 red, s32 green, s32 blue, s32 abr) {
    s32 red_step = ((red << 8) - field_work.fades[channel].level[0]) / steps;
    s32 green_step = ((green << 8) - field_work.fades[channel].level[1]) / steps;
    s32 blue_step = ((blue << 8) - field_work.fades[channel].level[2]) / steps;

    field_work.fades[channel].steps = steps;
    field_work.fades[channel].active = 1;
    field_work.fades[channel].abr = abr;
    field_work.fades[channel].step[0] = red_step;
    field_work.fades[channel].step[1] = green_step;
    field_work.fades[channel].step[2] = blue_step;
}

/* 80071DCC: Fade channel 0 out to white over `steps` frames, once. */
void field_fade_out(s32 steps) {
    s32 rate;

    if (field_faded_out != 1) {
        field_faded_out = 1;
        if (field_fade_mode == 2) {
            rate = 0xFF00 / steps;
            field_work.fades[0].level[0] = field_work.fades[0].level[1] = field_work.fades[0].level[2] = 0;
            field_work.fades[0].steps = steps;
            field_work.fades[0].active = 1;
            field_work.fades[0].abr = 2;
            field_work.fades[0].step[0] = field_work.fades[0].step[1] = field_work.fades[0].step[2] = rate;
        }
    }
}

/* 80071E58: Fade channel 0 back in from full over `steps` frames, once. */
void field_fade_in(s32 steps) {
    s32 rate;

    if (field_faded_out != 0) {
        field_faded_out = 0;
        if (field_fade_mode == 2) {
            rate = -0x10000 / steps;
            field_work.fades[0].level[0] = field_work.fades[0].level[1] = field_work.fades[0].level[2] = 0xFF00;
            field_work.fades[0].active = 1;
            field_work.fades[0].steps = steps;
            field_work.fades[0].abr = 2;
            field_work.fades[0].step[0] = field_work.fades[0].step[1] = field_work.fades[0].step[2] = rate;
        }
    }
}

/* 80071EE8: Pointer setup: pad buffers, divisors 3 and 4, bounds, both ports' starts. */
void field_pointer_init(void) {
    field_pointer_set_pads(&pad_receive_buffers[0], &pad_receive_buffers[1]);
    field_pointer_set_divisors(3, 4);
    field_pointer_set_bounds(0, 0x140, 0, 0xE0);
    field_pointer_set_position(0, 0x50, 0x64);
    field_pointer_set_position(1, 0xFA, 0x64);
    field_pointer_set_bounds(0, 0x12C, 0xA, 0xDC);
}

/* 80071F64: Set both draw buffers' clip areas; the second sits 0x100 lines lower. */
void field_draw_set_clip_areas(s32 x, s32 y, s32 w, s32 h) {
    field_draw_blocks[0].draw.clip.x = x;
    field_draw_blocks[0].draw.clip.y = y;
    field_draw_blocks[0].draw.clip.w = w;
    field_draw_blocks[0].draw.clip.h = h;
    field_draw_blocks[1].draw.clip.x = x;
    field_draw_blocks[1].draw.clip.y = y + 0x100;
    field_draw_blocks[1].draw.clip.w = w;
    field_draw_blocks[1].draw.clip.h = h;
}

/* 80071FB0: Display setup: geometry defaults and offset, both draw blocks'
 * environments, clip areas and screens, black backgrounds, then show the
 * second block and set the renderer's limits. */
void field_draw_init_display(void) {
    sprite_frame_skip = 1;
    DrawSync(0);
    VSync(0);
    InitGeom();
    SetGeomOffset(0xA0, 0x70);
    SetDefDrawEnv(&field_draw_blocks[0].draw, 0, 0, 0x140, 0xE0);
    SetDefDrawEnv(&field_draw_blocks[1].draw, 0, 0x100, 0x140, 0xE0);
    SetDefDrawEnv(&field_draw_blocks[0].draw2, 0, 0, 0x140, 0xE0);
    SetDefDrawEnv(&field_draw_blocks[1].draw2, 0, 0x100, 0x140, 0xE0);
    SetDefDispEnv(&field_draw_blocks[0].disp, 0, 0x100, 0x140, 0xE0);
    SetDefDispEnv(&field_draw_blocks[1].disp, 0, 0, 0x140, 0xE0);
    field_draw_set_clip_areas(0, 0, 0x140, 0xE0);
    field_draw_set_display_areas();
    field_draw_blocks[0].draw.r0 = 0;
    field_draw_blocks[0].draw.g0 = 0;
    field_draw_blocks[0].draw.b0 = 0;
    field_draw_blocks[1].draw.r0 = 0;
    field_draw_blocks[1].draw.g0 = 0;
    field_draw_blocks[1].draw.b0 = 0;
    field_draw_blocks[0].draw.dtd = 1;
    field_draw_blocks[1].draw.dtd = 1;
    VSync(0);
    PutDispEnv(&field_draw_blocks[1].disp);
    PutDrawEnv(&field_draw_blocks[1].draw);
    model_set_screen_bounds(0x140, 0xF0);
}

/* 80072140: Clear a matrix's translation. */
void field_matrix_clear_translation(MATRIX *m) {
    m->t[2] = 0;
    m->t[1] = 0;
    m->t[0] = 0;
}

/* 80072150: Compose the view (orbit under the previous view), the world matrices, and
 * leave the scaled world matrix loaded for drawing. */
void field_camera_compose_view(void) {
    VECTOR scale;
    MATRIX unused;
    MATRIX composed;
    long flag;

    gpu_build_rotation_matrix(&field_view.orbit_angles, &field_view.orbit);
    field_matrix_clear_translation(&field_view.orbit);
    CompMatrix(&field_view.orbit, &field_view.previous_view, &composed);
    field_matrix_copy(&field_view.previous_view, &composed);
    gpu_build_rotation_matrix(&field_view.world_angles, &field_view.world_matrix);
    field_matrix_clear_translation(&field_view.world_matrix);
    gpu_build_rotation_matrix(&field_view.world_angles, &field_view.scaled_world);
    MulMatrix2(&field_view.previous_view, &field_view.scaled_world);
    SetRotMatrix(&field_view.previous_view);
    SetTransMatrix(&field_view.previous_view);
    RotTrans(&field_view.anchor, (VECTOR *)field_view.scaled_world.t, &flag);
    scale.vx = field_view.scale;
    scale.vy = field_view.scale;
    scale.vz = field_view.scale;
    ScaleMatrix(&field_view.scaled_world, &scale);
    SetRotMatrix(&field_view.scaled_world);
    SetTransMatrix(&field_view.scaled_world);
}

/* 80072254: Rebuild a descriptor's matrix from its rotation, scaled by its actor. */
void field_descriptor_rebuild_matrix(s32 index) {
    VECTOR scale;

    scale.vx = field_view.components.descriptors[index].actor->scale[0];
    scale.vy = field_view.components.descriptors[index].actor->scale[1];
    scale.vz = field_view.components.descriptors[index].actor->scale[2];
    gpu_build_rotation_matrix(&field_view.components.descriptors[index].rotation, &field_view.components.descriptors[index].matrix);
    ScaleMatrix(&field_view.components.descriptors[index].matrix, &scale);
}

/* 800722F4: Compose the view and reload the scaled world matrix; 802815b0 runs unless
 * 800c268c is set. */
void field_camera_compose_and_load_view(void) {
    field_camera_compose_view();
    SetRotMatrix(&field_view.scaled_world);
    SetTransMatrix(&field_view.scaled_world);
    if (field_monitor_absent == 0) {
        field_debug_update_line_matrices();
    }
}

/* 8007234C: Count the blocked octants from `start` upward; zero when all are blocked. */
s32 field_camera_count_blocked_octants_up(s32 mask, s32 start) {
    s32 i;
    s32 count;

    for (i = 0, count = 0; i < 8; i++, count++) {
        if (!(mask & field_camera_octant_bits[start++ & 7])) {
            return count;
        }
    }
    return 0;
}

/* 80072398: Count the blocked octants from `start` downward; zero when all are blocked. */
s32 field_camera_count_blocked_octants_down(s32 mask, s32 start) {
    s32 i;
    s32 count;

    for (i = 0, count = 0; i < 8; i++, count++) {
        if (!(mask & field_camera_octant_bits[start-- & 7])) {
            return count;
        }
    }
    return 0;
}

/* 800723E4: The intersection of the lines through segments `a` and `b` (X/Z points);
 * `b`'s start when they are parallel. */
void field_compute_line_intersection(DVECTOR *a, DVECTOR *b, DVECTOR *out) {
    VECTOR ua;
    VECTOR ub;
    VECTOR d;
    s32 cross;
    s32 t;

    d.vx = a[1].vx - a[0].vx;
    d.vy = 0;
    d.vz = a[1].vy - a[0].vy;
    VectorNormal(&d, &ua);
    d.vx = b[1].vx - b[0].vx;
    d.vy = 0;
    d.vz = b[1].vy - b[0].vy;
    VectorNormal(&d, &ub);
    cross = (ub.vx * ua.vz - ub.vz * ua.vx) >> 12;
    if (cross == 0) {
        t = 0;
    } else {
        t = ((b[0].vy - a[0].vy) * ua.vx - (b[0].vx - a[0].vx) * ua.vz) / cross;
    }
    out->vx = b[0].vx + ((t * ub.vx) >> 12);
    out->vy = b[0].vy + ((t * ub.vz) >> 12);
}

/* Reset the orbit camera. Its do-while scope is what keeps cse2 from folding
 * the 800af984-relative addresses after the call back into absolute ones. */
#define CAMERA_RESET_ORBIT() do { field_matrix_set_identity(&field_view.orbit); } while (0)

/* 8007254C: The camera's initial state. The eye pointer gives the original's
 * $s0-relative eye.vx with absolute eye.vy/vz; the up vectors' y is loaded
 * once ahead of the eye stores. */
void field_camera_init(void) {
    VECTOR *eye;
    s32 up_y;

    field_view.target_a = 8;
    field_view.target_b = 8;
    field_view.heading_velocity = 0x400000;
    field_view.heading_high = 0x08000000;
    field_view.heading_angles.vy = 0x800;
    field_view.shake_amplitude[2] = 0;
    field_view.shake_amplitude[1] = 0;
    field_view.shake_amplitude[0] = 0;
    field_view.shake_step[2] = 0;
    field_view.shake_step[1] = 0;
    field_view.shake_step[0] = 0;
    field_view.shake = 0;
    field_view.shake_stop = 0;
    field_view.flags = 0;
    field_view.view_angle = 0;
    field_view.angle = 0;
    field_view.heading_blocks[0] = 0;
    field_view.heading_blocks[1] = 0;
    field_view.heading_steps = 0;
    field_view.heading_angles.vx = 0;
    field_view.heading_angles.vz = 0;
    field_view.heading = 0x800;
    field_view.orbit_angles.vx = 0;
    field_view.orbit_angles.vy = 0;
    field_view.orbit_angles.vz = 0;
    CAMERA_RESET_ORBIT();
    eye = &field_view.eye;
    up_y = 0x10000000;
    eye->vx = 0;
    eye->vy = 0;
    eye->vz = 0;
    field_view.up.vy = up_y;
    field_view.unk050.vy = up_y;
    field_view.elevation = 0x1E;
    field_view.projection = 0x200;
    field_view.target.vx = 0;
    field_view.target.vy = 0;
    field_view.target.vz = 0;
    field_view.up.vx = 0;
    field_view.up.vz = 0;
    field_view.shake_offset.vx = 0;
    field_view.shake_offset.vy = 0;
    field_view.shake_offset.vz = 0;
    field_view.eye_goal.vx = 0;
    field_view.eye_goal.vy = 0;
    field_view.eye_goal.vz = 0;
    field_view.target_goal.vx = 0;
    field_view.target_goal.vy = 0;
    field_view.target_goal.vz = 0;
    field_view.unk050.vx = 0;
    field_view.unk050.vz = 0;
    field_view.elevation_steps = 0;
    field_view.projection_steps = 0;
    field_view.steps = 0;
    field_view.distance = 0x1000;
    field_view.mode = 0;
    field_view.scripted = 0;
    field_view.target_steps = 0;
    field_view.eye_steps = 0;
}

/* 800726E8: Turn the camera heading by an octant when the current one is blocked or
 * a shoulder button asks for it, then step the heading toward its goal. */
void field_camera_update_heading(void) {
    s32 up;

    if (field_view.heading_blocks[0] != 0xFF && field_view.heading_blocks[1] != 0xFF) {
        if (field_view.heading_steps == 0) {
            if (field_view.heading_blocks[0] & field_camera_octant_bits[(field_view.heading_angles.vy & 0xFFF) >> 9]) {
                if (field_view.heading_velocity != -0x400000 && field_view.heading_velocity != 0x400000) {
                    field_view.heading_velocity = 0x400000;
                    field_view.heading += 0x200;
                }
                field_view.heading_steps = 8;
            }
            if (field_view.heading_blocks[1] & field_camera_octant_bits[(field_view.heading_angles.vy & 0xFFF) >> 9]) {
                up = field_camera_count_blocked_octants_up(field_view.heading_blocks[1], (field_view.heading_angles.vy & 0xFFF) >> 9);
                if (field_camera_count_blocked_octants_down(field_view.heading_blocks[1], (field_view.heading_angles.vy & 0xFFF) >> 9) < up) {
                    field_view.heading_velocity = -0x400000;
                    field_view.heading -= 0x200;
                } else {
                    field_view.heading_velocity = 0x400000;
                    field_view.heading += 0x200;
                }
                field_view.heading_steps = 8;
            }
        }
        if ((field_pad_port0_held & 4) && !(field_view.flags & 0x8000) && field_view.heading_steps == 0 &&
            !(field_view.heading_blocks[1] & field_camera_octant_bits[((field_view.heading_angles.vy - 0x200) & 0xFFF) >> 9])) {
            field_view.heading_velocity = -0x400000;
            field_view.heading_steps = 8;
            field_view.heading -= 0x200;
        }
        if ((field_pad_port0_held & 8) && !(field_view.flags & 0x8000) && field_view.heading_steps == 0 &&
            !(field_view.heading_blocks[1] & field_camera_octant_bits[((field_view.heading_angles.vy + 0x200) & 0xFFF) >> 9])) {
            field_view.heading_velocity = 0x400000;
            field_view.heading_steps = 8;
            field_view.heading += 0x200;
        }
    }
    if (field_view.heading_steps != 0) {
        field_view.heading_high += field_view.heading_velocity;
        field_view.heading_angles.vy = field_view.heading_high >> 16;
        if (--field_view.heading_steps == 0) {
            field_view.heading_angles.vy = field_view.heading;
        }
    } else {
        field_view.heading_angles.vy = field_view.heading;
    }
    if (field_monitor_absent == 0) {
        field_debug_move_camera();
    }
}

/* 80072A38: Place the camera's target goal on the followed position (clamped to the
 * walkable edge when the position leaves the floor mesh) and its eye goal
 * at the elevation and distance around it; then step the distance and
 * elevation interpolations. Declared int but returns nothing. */
s32 field_camera_set_follow_goals(VECTOR *position, s32 floor) {
    SVECTOR edge[2];
    DVECTOR line[2];
    DVECTOR segment[2];
    DVECTOR hit;
    VECTOR point;
    MATRIX unused; /* unreferenced, but part of the frame */

    point.vx = position->vx;
    point.vy = 0;
    point.vz = position->vz;
    if (field_camera_walk_top_layer(&point, edge, segment) == -1) {
        line[0].vx = edge[0].vx;
        line[0].vy = edge[0].vz;
        line[1].vx = edge[1].vx;
        line[1].vy = edge[1].vz;
        field_compute_line_intersection(line, segment, &hit);
        field_view.target_goal.vx = hit.vx << 16;
        field_view.target_goal.vz = hit.vy << 16;
        if (field_work.camera_floor_fixed == 0) {
            if (field_camera_target_at_edge == 0) {
                field_view.target_goal.vy = floor << 16;
                field_camera_target_at_edge = 1;
            }
        } else {
            field_view.target_goal.vy = position->vy - 0x200000;
        }
    } else {
        field_view.target_goal.vx = position->vx;
        field_camera_target_at_edge = 0;
        field_view.target_goal.vy = position->vy;
        field_view.target_goal.vz = position->vz;
        field_view.target_goal.vy -= 0x200000;
    }
    field_view.eye_goal.vy =
        ((-((gpu_get_cos((((s16)field_view.elevation * 0x5B) >> 3) + 0xC00) * field_view.projection) << 5)) >> 16) *
            field_view.distance * 16 +
        field_view.target_goal.vy;
    field_view.eye_goal.vz =
        (((gpu_get_sin((((s16)field_view.elevation * 0x5B) >> 3) + 0xC00) * field_view.projection) << 5) >> 16) *
            field_view.distance * 16 +
        field_view.target_goal.vz;
    field_view.eye_goal.vx = field_view.target_goal.vx;
    field_camera_rotate_point_by_heading(&field_view.eye_goal, &field_view.target_goal);
    if (field_view.flags & 1) {
        if (field_view.steps != 0) {
            field_view.start += field_view.step;
            field_view.distance = field_view.start >> 16;
        }
        if (--field_view.steps == 0) {
            field_view.flags &= 0xFFFE;
        }
    }
    if (field_view.flags & 8) {
        field_view.elevation_value += field_view.elevation_step;
        field_view.elevation = field_view.elevation_value >> 16;
        if (--field_view.elevation_steps == 0) {
            field_view.flags &= 0xFFF7;
        }
    }
}

/* 80072D74: Step the projection interpolation, move the eye and target a fraction of
 * the way toward their goals (skipping axes already within the follow
 * divisor), and draw a random camera shake offset. */
void field_camera_move_toward_goals(void) {
    s32 target_threshold;
    s32 eye_threshold;
    s32 difference;
    s32 part;

    if (field_view.flags & 0x10) {
        if (field_view.projection_steps != 0) {
            field_view.projection_value += field_view.projection_step;
            field_view.projection = field_view.projection_value >> 16;
        }
        if (--field_view.projection_steps < 0) {
            field_view.flags &= 0xFFEF;
            field_view.projection_steps = 0;
        }
    }
    if (field_work.camera_counter != 0) {
        field_view.target_a = 1;
        field_view.target_b = 1;
        field_work.camera_counter--;
    }
    target_threshold = field_view.target_a * field_view.target_a;
    eye_threshold = field_view.target_b * field_view.target_b;

    if ((field_view.eye.vx >> 16) != (field_view.eye_goal.vx >> 16)) {
        difference = field_view.eye_goal.vx - field_view.eye.vx;
        part = difference >> 16;
        if (part * part >= eye_threshold) {
            field_view.eye.vx += difference / field_view.target_b;
        }
    }
    if ((field_view.eye.vz >> 16) != (field_view.eye_goal.vz >> 16)) {
        difference = field_view.eye_goal.vz - field_view.eye.vz;
        part = difference >> 16;
        if (part * part >= eye_threshold) {
            field_view.eye.vz += difference / field_view.target_b;
        }
    }
    if ((field_view.eye.vy >> 16) != (field_view.eye_goal.vy >> 16)) {
        difference = field_view.eye_goal.vy - field_view.eye.vy;
        part = difference >> 16;
        if (part * part >= eye_threshold) {
            field_view.eye.vy += difference / field_view.target_b;
        }
    }
    if ((field_view.target.vx >> 16) != (field_view.target_goal.vx >> 16)) {
        difference = field_view.target_goal.vx - field_view.target.vx;
        part = difference >> 16;
        if (part * part >= target_threshold) {
            field_view.target.vx += difference / field_view.target_a;
        }
    }
    if ((field_view.target.vz >> 16) != (field_view.target_goal.vz >> 16)) {
        difference = field_view.target_goal.vz - field_view.target.vz;
        part = difference >> 16;
        if (part * part >= target_threshold) {
            field_view.target.vz += difference / field_view.target_a;
        }
    }
    if ((field_view.target.vy >> 16) != (field_view.target_goal.vy >> 16)) {
        difference = field_view.target_goal.vy - field_view.target.vy;
        part = difference >> 16;
        if (part * part >= target_threshold) {
            field_view.target.vy += difference / field_view.target_a;
        }
    }

    field_view.shake_offset.vx = 0;
    field_view.shake_offset.vy = 0;
    field_view.shake_offset.vz = 0;
    if (field_view.shake != 0) {
        if (field_view.shake_time != 0) {
            field_view.shake_amplitude[0] += field_view.shake_step[0];
            field_view.shake_amplitude[1] += field_view.shake_step[1];
            field_view.shake_amplitude[2] += field_view.shake_step[2];
        } else if (field_view.shake_stop != 0) {
            field_view.shake_amplitude[2] = 0;
            field_view.shake_amplitude[1] = 0;
            field_view.shake_amplitude[0] = 0;
            field_view.shake = 0;
            field_view.shake_stop = 0;
        }
        field_view.shake_offset.vx = rand() * WHOLE(field_view.shake_amplitude[0]);
        field_view.shake_offset.vy = rand() * WHOLE(field_view.shake_amplitude[1]);
        field_view.shake_offset.vz = rand() * WHOLE(field_view.shake_amplitude[2]);
        if (field_view.shake_offset.vx < 0) {
            field_view.shake_offset.vx = 0;
            field_view.shake_amplitude[0] = 0;
        }
        if (field_view.shake_offset.vy < 0) {
            field_view.shake_offset.vy = 0;
            field_view.shake_amplitude[1] = 0;
        }
        if (field_view.shake_offset.vz < 0) {
            field_view.shake_offset.vz = 0;
            field_view.shake_amplitude[2] = 0;
        }
        if (field_view.shake_time > 0) {
            field_view.shake_time--;
        }
    }
}

/* 80073230: Per-frame camera update: in mode 1 follow the scripted target and eye
 * interpolations; in modes 0 and 2 follow the controlled actor (mode 2
 * returns to 0 once both goals are reached or after 64 frames), keeping
 * the eye above the floor; then move the camera toward its goals. */
void field_camera_update(void) {
    VECTOR position;
    VECTOR normal;
    SVECTOR floor;
    s32 target_distance;
    s32 eye_distance;

    switch (field_view.mode) {
    case 2:
        field_camera_settle_frames = 0;
        if (++field_camera_release_frames >= 0x41) {
            field_view.mode = 0;
        }
        goto follow;
    case 0:
        field_camera_release_frames = 0;
        if (!(field_camera_settle_frames & 3)) {
            if (field_view.target_a < 9) {
                field_view.target_a = 8;
            } else {
                field_view.target_a -= 2;
            }
            if (field_view.target_b < 9) {
                field_view.target_b = 8;
            } else {
                field_view.target_b -= 2;
            }
        }
        field_camera_settle_frames++;
    follow:
        field_camera_update_heading();
        position.vx = field_view.components.descriptors[field_work.unk233E].actor->position[0];
        position.vy = field_view.components.descriptors[field_work.unk233E].actor->position[1];
        position.vz = field_view.components.descriptors[field_work.unk233E].actor->position[2];
        field_camera_set_follow_goals(&position, field_view.components.descriptors[field_work.unk233E].actor->unk72);
        if (!(field_view.flags & 0x4000)) {
            field_collision_find_floor_triangle(WHOLE(field_view.eye_goal.vx), WHOLE(field_view.eye_goal.vz),
                          field_view.components.layer_count - 1, &floor, &normal);
            if (WHOLE(field_view.eye_goal.vy) > floor.vy) {
                field_view.eye_goal.vy = floor.vy << 16;
            }
        }
        if (field_view.mode == 2) {
            target_distance = field_compute_planar_length(WHOLE(field_view.target_goal.vx) - WHOLE(field_view.target.vx),
                                            WHOLE(field_view.target_goal.vz) - WHOLE(field_view.target.vz));
            eye_distance = field_compute_planar_length(WHOLE(field_view.eye_goal.vx) - WHOLE(field_view.eye.vx),
                                         WHOLE(field_view.eye_goal.vz) - WHOLE(field_view.eye.vz));
            if (target_distance < 0x80 && eye_distance < 0x80) {
                field_view.mode = 0;
            }
        }
        break;
    case 1:
        field_camera_settle_frames = 0;
        field_camera_release_frames = 0;
        if (field_view.scripted & 1) {
            if (field_view.target_steps != 0) {
                field_view.scripted_target.vx += field_view.target_step.vx;
                field_view.scripted_target.vy += field_view.target_step.vy;
                field_view.scripted_target.vz += field_view.target_step.vz;
            }
            if (--field_view.target_steps == 0) {
                field_view.scripted &= 0xFFFE;
            }
            field_view.target_goal.vx = field_view.scripted_target.vx;
            field_view.target_goal.vy = field_view.scripted_target.vy;
            field_view.target_goal.vz = field_view.scripted_target.vz;
        }
        if (field_view.scripted & 2) {
            if (field_view.eye_steps != 0) {
                field_view.scripted_eye[0] += field_view.eye_step[0];
                field_view.scripted_eye[1] += field_view.eye_step[1];
                field_view.scripted_eye[2] += field_view.eye_step[2];
            }
            if (--field_view.eye_steps == 0) {
                field_view.scripted &= 0xFFFD;
            }
            field_view.eye_goal.vx = field_view.scripted_eye[0];
            field_view.eye_goal.vy = field_view.scripted_eye[1];
            field_view.eye_goal.vz = field_view.scripted_eye[2];
        }
        break;
    }
    field_camera_move_toward_goals();
    field_view.heading_angles.vy &= 0xFFF;
}

/* 80073684: Rotate `point` in X/Z about `center` by the camera heading angles. */
void field_camera_rotate_point_by_heading(VECTOR *point, VECTOR *center) {
    MATRIX m;
    VECTOR offset;
    VECTOR rotated;

    PushMatrix();
    gpu_build_rotation_matrix(&field_view.heading_angles, &m);
    offset.vx = center->vx - point->vx;
    offset.vy = center->vy - point->vy;
    offset.vz = center->vz - point->vz;
    ApplyMatrixLV(&m, &offset, &rotated);
    point->vx = rotated.vx + center->vx;
    point->vz = rotated.vz + center->vz;
    PopMatrix();
}

/* 80073734: Truncate a 16.16 vector to its integer parts. */
void field_truncate_fixed_vector(VECTOR *v) {
    v->vx = ((s16 *)&v->vx)[1];
    v->vy = ((s16 *)&v->vy)[1];
    v->vz = ((s16 *)&v->vz)[1];
}

/* 80073750: Build a look-at view matrix from 16.16 eye, target and up vectors: the
 * rows are the side, up and forward axes, the translation the rotated
 * eye (scaled by 3) negated. */
void field_camera_build_lookat_matrix(MATRIX *view, VECTOR *eye, VECTOR *target, VECTOR *up) {
    VECTOR v;
    VECTOR forward;
    VECTOR side;
    VECTOR y;
    SVECTOR position;

    v.vx = (target->vx - eye->vx) >> 16;
    v.vy = (target->vy - eye->vy) >> 16;
    v.vz = (target->vz - eye->vz) >> 16;
    y.vx = up->vx;
    y.vy = up->vy;
    y.vz = up->vz;
    y.vx >>= 16;
    y.vy >>= 16;
    y.vz >>= 16;
    VectorNormal(&v, &forward);
    OuterProduct12(&y, &forward, &v);
    VectorNormal(&v, &side);
    OuterProduct12(&forward, &side, &v);
    VectorNormal(&v, &y);
    view->m[0][0] = side.vx;
    view->m[0][1] = side.vy;
    view->m[0][2] = side.vz;
    view->m[1][0] = y.vx;
    view->m[1][1] = y.vy;
    view->m[1][2] = y.vz;
    view->m[2][0] = forward.vx;
    view->m[2][1] = forward.vy;
    view->m[2][2] = forward.vz;
    position.vx = WHOLE(eye->vx) * 3;
    position.vy = WHOLE(eye->vy) * 3;
    position.vz = WHOLE(eye->vz) * 3;
    ApplyMatrix(view, &position, &v);
    view->t[0] = -v.vx;
    view->t[1] = -v.vy;
    view->t[2] = -v.vz;
}

/* 80073930: Turn angle `angle` towards `goal` by `step` the short way round, stopping
 * at the goal; 12-bit angles. */
s32 field_turn_angle_toward(s32 angle, s32 goal, s32 step) {
    if (((angle - goal) & 0xFFF) < 0x800) {
        angle -= step;
        if (((angle - goal) & 0xFFF) >= 0x800) {
            angle = goal;
        }
    } else {
        angle += step;
        if (((angle - goal) & 0xFFF) < 0x800) {
            angle = goal;
        }
    }
    return angle & 0xFFF;
}

/* 80073988: Turn towards `goal` by `step`, or jump there when 800adc18 is set. */
s32 field_turn_angle_or_cut(s32 angle, s32 goal, s32 step) {
    s32 result;

    if (field_camera_cut_timer == 0) {
        result = field_turn_angle_toward(angle, goal, step);
    } else {
        result = goal & 0xFFF;
    }
    return result;
}

/* 800739C0: The move phase: update the field, point the model light and view angles
 * at the camera, update the camera (on a scratchpad stack), build the view
 * matrix from the shaken eye and target, then turn every drawn actor
 * toward its heading goal and set its model's facing. */
void field_run_move_phase(void) {
    VECTOR eye;
    VECTOR target;
    FieldActor *actor;
    s32 i;
    s32 step;

    field_update_events_and_actors();
    gpu_build_rotation_matrix(&field_work.sprite_angles, &field_sprite_view_matrix);
    field_sprite_view_matrix.t[2] = 0;
    field_sprite_view_matrix.t[1] = 0;
    field_sprite_view_matrix.t[0] = 0;
    field_view.view_angle = ratan2(field_view.target.vz - field_view.eye.vz, field_view.target.vx - field_view.eye.vx) - 0x400;
    field_view.angle = ratan2(field_view.target_goal.vz - field_view.eye_goal.vz,
                              field_view.target_goal.vx - field_view.eye_goal.vx) - 0x400;
    field_camera_pitch = ratan2(field_compute_planar_length((field_view.target.vx - field_view.eye.vx) >> 16,
                                      (field_view.target.vz - field_view.eye.vz) >> 16),
                        (field_view.target.vy - field_view.eye.vy) >> 16);
    STACK_ENTER(0x1F8003FC);
    field_camera_update();
    STACK_LEAVE();
    eye.vx = field_view.eye.vx;
    eye.vy = field_view.eye.vy;
    eye.vz = field_view.eye.vz;
    eye.vx += field_view.shake_offset.vx;
    eye.vy += field_view.shake_offset.vy;
    eye.vz += field_view.shake_offset.vz;
    target.vx = field_view.target.vx;
    target.vy = field_view.target.vy;
    target.vz = field_view.target.vz;
    target.vx += field_view.shake_offset.vx;
    target.vy += field_view.shake_offset.vy;
    target.vz += field_view.shake_offset.vz;
    if (field_camera_cut_timer != 0) {
        field_camera_build_lookat_matrix(&field_view.previous_view, &eye, &target, &field_view.up);
        field_camera_next_view = field_view.previous_view;
    } else {
        field_view.previous_view = field_camera_next_view;
        field_camera_build_lookat_matrix(&field_camera_next_view, &eye, &target, &field_view.up);
    }
    STACK_ENTER(0x1F8003FC);
    field_camera_compose_and_load_view();
    STACK_LEAVE();
    for (i = 0; i < field_event_actor_count; i++) {
        if ((field_view.components.descriptors[i].flags & 0xF40) && !(field_view.components.descriptors[i].flags & 0x20)) {
            actor = field_view.components.descriptors[i].actor;
            if (!(actor->layer_flags & 0x100000) && (actor->layer_flags & 0x600) != 0x200) {
                if (!(actor->flags & 0x8000)) {
                    if (!(actor->unk014 & 0x200000) || (actor->flags & 0x1800)) {
                        if (!(actor->layer_flags & 0x2000)) {
                            step = actor->unk11E;
                        } else {
                            step = field_work.unk21B4;
                        }
                        actor->unk108 = field_turn_angle_or_cut(actor->unk108, actor->heading_goal, step);
                    } else {
                        actor->unk108 = field_turn_angle_or_cut(actor->unk108, (((actor->unk014 >> 11) - 2) & 7) << 9, 0x200);
                    }
                }
                if (field_characters_hidden == 0) {
                    if (!(actor->layer_flags & 0x01000000)) {
                        sprite_set_facing(field_view.components.descriptors[i].model,
                                      field_view.view_angle + field_view.components.descriptors[i].actor->unk108);
                    } else {
                        sprite_set_direction(field_view.components.descriptors[i].model,
                                      field_view.components.descriptors[i].actor->unk108);
                    }
                }
            }
        }
    }
    if (field_monitor_absent == 0) {
        field_debug_mark_cpu_time("MATRIX    ");
    }
}

/* 80073E38: Refresh each shown model instance's bounds (800aa9dc) and choose its
 * drawing mode from its descriptor flags. */
void field_instance_refresh_bounds_modes(void) {
    s32 i;
    FieldDescriptor *descriptor;
    FieldInstance *instance;
    u16 flags;

    for (i = 0; i < field_view.components.descriptor_count; i++) {
        descriptor = &field_view.components.descriptors[i];
        if (!(descriptor->flags & 0x40)) {
            instance = descriptor->instance;
            field_instance_set_bounds(instance);
            if (field_work.sprite_gate != 0) {
                if (descriptor->flags & 0x10) {
                    instance->mode = 5;
                } else {
                    instance->mode = 4;
                }
            } else {
                flags = descriptor->flags;
                if (flags & 0xC) {
                    instance->mode = 1;
                } else if (flags & 0x4000) {
                    instance->mode = 3;
                } else if (flags & 0x10) {
                    instance->mode = 2;
                } else {
                    instance->mode = 0;
                }
            }
        }
    }
}

/* 80073F50: Switch to the other draw block and clear its overlay ordering table,
 * polling the debugger host (pollhost, `break 1024`) when 800c268c is
 * clear. */
void field_draw_switch_block(void) {
    if (field_monitor_absent == 0) {
        pollhost();
    }
    field_draw_buffer_index = (field_draw_buffer_index + 1) % 2;
    field_current_draw_block = &field_draw_blocks[field_draw_buffer_index];
    ClearOTagR(field_current_draw_block->overlay_ot, 8);
}

/* 80073FE0: Swap the draw buffer and clear its ordering tables. */
void field_draw_swap_and_clear_ots(void) {
    field_draw_switch_block();
    ClearOTagR(field_current_draw_block->ot, 0x1000);
    if (field_draw_second_ot_enabled != 0) {
        ClearOTagR(field_current_draw_block->ot2, 0x1000);
    }
}

/* 80074038: Copy a matrix's rotation and translation. */
void field_matrix_copy(MATRIX *to, MATRIX *from) {
    field_matrix_copy_rotation(to, from);
    field_matrix_copy_translation(to, from);
}

/* 80074078: Copy a matrix's translation. */
void field_matrix_copy_translation(MATRIX *to, MATRIX *from) {
    to->t[0] = from->t[0];
    to->t[1] = from->t[1];
    to->t[2] = from->t[2];
}

/* 8007409C: Copy a matrix's rotation. */
void field_matrix_copy_rotation(MATRIX *to, MATRIX *from) {
    to->m[0][0] = from->m[0][0];
    to->m[0][1] = from->m[0][1];
    to->m[0][2] = from->m[0][2];
    to->m[1][0] = from->m[1][0];
    to->m[1][1] = from->m[1][1];
    to->m[1][2] = from->m[1][2];
    to->m[2][0] = from->m[2][0];
    to->m[2][1] = from->m[2][1];
    to->m[2][2] = from->m[2][2];
}

/* 80074108: Draw the compass: upload its palette (rows of blocked heading octants
 * black), look at it from the camera's height and distance, turn the
 * needle toward the controlled actor's heading, and draw the needle,
 * letters, ring and pointer quads; also leave the upright view in the
 * model pass matrix. */
void field_compass_draw(void) {
    MATRIX base;
    MATRIX look;
    MATRIX turn;
    MATRIX placed;
    MATRIX tilt;
    SVECTOR angles;
    VECTOR eye;
    VECTOR origin;
    s32 i;
    s32 j;
    u8 blocked;
    s16 view;
    s16 *offset_x;
    s16 *offset_z;

    blocked = field_view.heading_blocks[1];
    for (i = 0; i < 8; i++) {
        if (blocked & field_compass_row_octant_bits[i]) {
            for (j = 0; j < 16; j++) {
                field_compass_palette[i * 16 + j] = 0;
            }
        } else {
            for (j = 0; j < 16; j++) {
                field_compass_palette[i * 16 + j] = field_compass_colors[j];
            }
        }
    }
    field_compass_palette_rect.w = 0x80;
    LoadImage(&field_compass_palette_rect, (u_long *)field_compass_palette);
    SetGeomScreen(0x80);
    SetGeomOffset(0x10A, 0xA6);
    origin.vx = 0;
    origin.vy = 0;
    origin.vz = 0;
    eye.vx = 0;
    eye.vy = field_view.eye.vy - field_view.target.vy;
    eye.vz = -field_compute_planar_length((field_view.eye.vx - field_view.target.vx) >> 16,
                            (field_view.eye.vz - field_view.target.vz) >> 16) << 16;
    field_camera_build_lookat_matrix(&look, &eye, &origin, &field_view.up);
    field_matrix_set_identity(&base);
    base.t[2] = 0x80;
    SetRotMatrix(&base);
    SetTransMatrix(&base);
    view = field_view.view_angle + 0x400;
    field_compass_needle_goal = field_view.components.descriptors[field_work.unk233E].actor->heading_goal + view;
    field_compass_needle_heading = field_turn_angle_or_cut(field_compass_needle_heading, field_compass_needle_goal, 0x40);
    angles.vx = 0;
    angles.vy = field_compass_needle_heading;
    angles.vz = 0;
    field_matrix_clear_translation(&turn);
    gpu_build_rotation_matrix(&angles, &turn);
    MulMatrix2(&look, &turn);
    turn.t[2] = 0x1000;
    CompMatrix(&base, &turn, &placed);
    if (field_work.script_control[1] == 0 && field_camera_cut_timer == 0 && mode_debug_hide_compass == 0) {
        for (i = 20; i < 21; i++) {
            field_marker_project_and_link(field_current_draw_block->overlay_ot, &field_compass_markers[i], &placed, field_draw_buffer_index);
        }
    }
    field_matrix_set_identity(&turn);
    MulMatrix2(&look, &turn);
    turn.t[2] = 0x1000;
    CompMatrix(&base, &turn, &placed);
    MulMatrix0(&base, &turn, &field_view.unk204);
    SetRotMatrix(&base);
    SetTransMatrix(&base);
    field_matrix_set_identity(&turn);
    MulMatrix2(&field_view.previous_view, &turn);
    turn.t[2] = 0x1000;
    CompMatrix(&base, &turn, &placed);
    field_matrix_copy(&base, &placed);
    angles.vx = 0x400;
    angles.vy = 0;
    angles.vz = 0;
    gpu_build_rotation_matrix(&angles, &tilt);
    if (field_work.script_control[1] == 0 && field_camera_cut_timer == 0 && mode_debug_hide_compass == 0) {
        for (i = 16; i < 20; i++) {
            field_matrix_set_identity(&turn);
            offset_x = &field_compass_letter_offsets[i - 16].vx;
            offset_z = &field_compass_letter_offsets[i - 16].vy;
            turn.t[0] = *offset_x;
            turn.t[2] = *offset_z;
            CompMatrix(&base, &turn, &placed);
            field_matrix_copy_rotation(&placed, &tilt);
            field_marker_link_standing_sprite(field_current_draw_block->overlay_ot, &field_compass_markers[i], &placed, field_draw_buffer_index);
        }
        for (i = 0; i < 16; i++) {
            field_marker_project_and_link(field_current_draw_block->overlay_ot, &field_compass_markers[i], &base, field_draw_buffer_index);
        }
        for (i = 21; i < 25; i++) {
            field_marker_project_and_link(field_current_draw_block->overlay_ot, &field_compass_markers[i], &base, field_draw_buffer_index);
        }
    }
    addPrim(field_current_draw_block->overlay_ot, &field_texture_window_modes[field_draw_buffer_index][1]);
    SetGeomOffset(0xA0, 0x70);
    SetGeomScreen(field_view.projection);
}

/* 8007469C: Whether any shown descriptor (flag 0x40 clear) has flag 0x8000. */
s32 field_has_second_ot_instances(void) {
    s32 i;
    u16 flags;

    for (i = 0; i < field_view.components.descriptor_count; i++) {
        flags = field_view.components.descriptors[i].flags;
        if (!(flags & 0x40)) {
            if (flags & 0x8000) {
                return 1;
            }
        }
    }
    return 0;
}

/* 80074700: Drain the pad queue into this frame's held, pressed and repeated buttons
 * of both ports; port 1's are masked by the event input mask and the
 * position mask, and all are cleared while the camera cuts. */
void field_pad_drain_queue(void) {
    field_pad_port0_held = 0;
    field_pad_port1_held = 0;
    field_pad_port0_pressed = 0;
    field_pad_unread_port1_pressed = 0;
    field_pad_port0_repeated = 0;
    field_pad_port1_repeated = 0;
    while (pad_dequeue_state() != 0) {
        field_pad_port0_held |= pad_port0_held & field_work.input_mask;
        field_pad_port1_held |= pad_port1_held;
        field_pad_port0_pressed |= pad_port0_pressed & field_work.input_mask;
        field_pad_unread_port1_pressed |= pad_port1_pressed;
        field_pad_port0_repeated |= pad_port0_repeated & field_work.input_mask;
        field_pad_port1_repeated |= pad_port1_repeated;
    }
    field_pad_port0_held &= field_pad_port0_allowed_mask;
    field_pad_port0_pressed &= field_pad_port0_allowed_mask;
    field_pad_port0_repeated &= field_pad_port0_allowed_mask;
    pad_clear_queue();
    field_pointer_read(1, mode_field_pointer_state);
    if (field_camera_cut_timer != 0) {
        field_pad_port0_held = 0;
        field_pad_port1_held = 0;
        field_pad_port0_pressed = 0;
        field_pad_unread_port1_pressed = 0;
        field_pad_port0_repeated = 0;
        field_pad_port1_repeated = 0;
    }
    if (field_battle_not_requested == 0) {
        field_pad_port0_pressed &= ~0x80;
    }
}

/* 800748E8: Draw the field models: set the fog, then for every drawn descriptor build
 * its model matrix (turned about one axis, posed by the 801e bone routine,
 * attached to another descriptor, or placed in the view with the piece
 * drift and orientation modes), light it, and draw its instance unless it
 * is culled. */
void field_draw_models(void) {
    SVECTOR angles;
    VECTOR scale;
    VECTOR half;
    MATRIX view;
    MATRIX placed;
    MATRIX work;
    s32 always;
    FieldDescriptor *descriptor;
    FieldInstance *instance;
    FieldActor *actor;
    u16 pose;
    s32 i;
    s32 orient;

    model_drawn_primitive_count = 0;
    model_submitted_primitive_count = 0;
    if (field_work.sprite_gate != 0) {
        model_set_color(field_work.fog_color[0], field_work.fog_color[1], field_work.fog_color[2]);
        SetFarColor(field_work.far_color[0], field_work.far_color[1], field_work.far_color[2]);
        SetFogNearFar(field_work.fog_range[0], field_work.fog_range[1], field_view.projection);
    }
    half.vx = 0x800;
    half.vy = 0x800;
    half.vz = 0x800;
    scale.vx = field_view.scale;
    scale.vy = field_view.scale;
    scale.vz = field_view.scale;
    ScaleMatrix(&field_view.unk204, &scale);
    CompMatrix(&field_view.scaled_world, &field_sprite_view_matrix, &view);
    model_box_test_mode = 0;
    field_work.unk21BC[0] += field_work.piece_drift[0];
    field_work.unk21BC[1] += field_work.piece_drift[2];
    field_work.unk21BC[2] += field_work.piece_drift[1];
    for (i = 0; i < field_view.components.descriptor_count; i++) {
        field_view.components.descriptors[i].transform = field_view.components.descriptors[i].matrix;
        always = 0;
        if (field_view.components.descriptors[i].flags & 0x40) {
            continue;
        }
        descriptor = &field_view.components.descriptors[i];
        if (i < field_event_actor_count) {
            switch (descriptor->actor->state.bits.mode) {
            case 1:
                angles.vx = descriptor->actor->unk70;
                angles.vy = 0;
                angles.vz = 0;
                goto turn;
            case 2:
                angles.vx = 0;
                angles.vy = descriptor->actor->unk70;
                angles.vz = 0;
                goto turn;
            case 3:
                angles.vx = 0;
                angles.vy = 0;
                angles.vz = descriptor->actor->unk70;
            turn:
                gpu_build_rotation_matrix(&angles, &work);
                MulMatrix2(&field_view.components.descriptors[i].matrix, &work);
                work.t[0] = field_view.components.descriptors[i].matrix.t[0];
                work.t[1] = field_view.components.descriptors[i].matrix.t[1];
                work.t[2] = field_view.components.descriptors[i].matrix.t[2];
                CompMatrix(&field_view.scaled_world, &work, &placed);
                break;
            default:
                actor = descriptor->actor;
                pose = actor->unk128;
                if (pose != 0xFFFF) {
                    gear_model_get_node_matrix(&field_view.components.descriptors[i].transform,
                                  &field_view.components.descriptors[i].transform, pose >> 12, pose & 0xFFF);
                    CompMatrix(&field_view.scaled_world, &field_view.components.descriptors[i].transform, &work);
                    CompMatrix(&work, &field_view.components.descriptors[i].matrix, &placed);
                    CompMatrix(&field_view.components.descriptors[i].transform,
                               &field_view.components.descriptors[i].matrix,
                               &field_view.components.descriptors[i].transform);
                } else if (actor->unk075 != 0xFF) {
                    CompMatrix(&field_view.scaled_world,
                               &field_view.components.descriptors[field_view.components.descriptors[i].actor->unk075].transform,
                               &work);
                    CompMatrix(&work, &field_view.components.descriptors[i].matrix, &placed);
                    CompMatrix(&field_view.components.descriptors[field_view.components.descriptors[i].actor->unk075].transform,
                               &field_view.components.descriptors[i].matrix,
                               &field_view.components.descriptors[i].transform);
                } else {
                    goto general;
                }
                break;
            }
        } else {
            if (!(field_work.piece_drift_mode & 0x7F)) {
                descriptor->matrix.t[0] += field_work.piece_drift[0];
                descriptor->matrix.t[1] += field_work.piece_drift[2];
                descriptor->matrix.t[2] += field_work.piece_drift[1];
            }
        general:
            if ((field_work.piece_drift_mode & 0x7F) == 1) {
                descriptor->matrix.t[0] += field_work.piece_drift[0];
                descriptor->matrix.t[1] += field_work.piece_drift[2];
                descriptor->matrix.t[2] += field_work.piece_drift[1];
            }
            if (field_work.piece_drift_mode & 0x80) {
                always = 1;
            }
            gte_CompMatrix(&view, &descriptor->matrix, &placed);
            orient = field_view.components.descriptors[i].flags & 3;
            if (orient != 0) {
                if (orient == 1) {
                    MulMatrix0(&field_view.unk204, &field_view.components.descriptors[i].matrix, &placed);
                } else {
                    field_matrix_copy_rotation(&placed, &field_view.components.descriptors[i].matrix);
                    ScaleMatrix(&placed, &scale);
                }
                MulMatrix2(&field_view.orbit, &placed);
            }
        }
        instance = descriptor->instance;
        if (instance->mode == 1) {
            work = placed;
            ScaleMatrix(&work, &half);
            model_load_light_matrix(&work);
            model_set_back_color_16bit(field_view.back_color[0], field_view.back_color[1], field_view.back_color[2]);
        }
        model_box_test_mode = 0;
        if (!(field_view.components.descriptors[i].flags & 0x20)) {
            if ((descriptor->flags & 0x2000) && instance->anims != NULL) {
                field_morph_list_descriptor = i;
                field_morph_list_index = 0;
                model_update_morph(instance->anims);
            }
            gte_SetRotMatrix(&placed);
            gte_SetTransMatrix(&placed);
            if (field_instance_is_off_screen(instance) == 0 || always == 1) {
                gte_SetRotMatrix(&placed);
                gte_SetTransMatrix(&placed);
                if (!(descriptor->flags & 0x8000)) {
                    model_draw_sprite_model(instance->mesh, instance->packets[field_draw_buffer_index], (u32 *)field_current_draw_block->ot, instance->mode);
                } else {
                    model_draw_sprite_model(instance->mesh, instance->packets[field_draw_buffer_index], (u32 *)field_current_draw_block->ot2, instance->mode);
                }
            }
        }
    }
    if (field_monitor_absent == 0) {
        field_debug_mark_cpu_time("MODEL     ");
    }
}

/* 8007520C: Draw the 801e module's layer (with the emitters updated and its back
 * colour set) when enabled, then the debug "GEAR" timer. */
void field_layer_draw(void) {
    if (mode_debug_hide_layer == 0) {
        if (field_work.unk2264 != 0) {
            field_layer_update_emitter_lights();
            SetBackColor(field_work.unk225C[0], field_work.unk225C[1], field_work.unk225C[2]);
            gear_model_step_and_draw(&field_view.scaled_world, field_work.unk221C, field_current_draw_block->ot, field_draw_buffer_index, 1);
        }
        if (field_monitor_absent == 0) {
            field_debug_mark_cpu_time("GEAR      ");
        }
    }
}

void field_draw_sprite_actors(u_long *ot, s32 buffer);
void field_draw_shadows(u_long *ot, s32 buffer);

/* 800752C8: Draw the field characters: set up the model renderer for this buffer, then
 * draw each actor's model (shown ones unless their layer is hidden or they
 * are flagged off, others only with layer flag 0x1000000), then the debug
 * "CHAR" timer. The descriptor flags are read as a whole word here. */
void field_draw_characters(void) {
    s32 i;

    if (field_characters_hidden == 1) {
        return;
    }
    sprite_queue_start_fill(field_draw_buffer_index);
    sprite_set_ot((s32)field_current_draw_block->ot);
    sprite_set_view_matrix(&field_view.scaled_world);
    sprite_build_pending_frames();
    task_run_draw_list();
    task_run_main_list();
    field_draw_sprite_actors(field_current_draw_block->ot, field_draw_buffer_index);
    for (i = 0; i < field_event_actor_count; i++) {
        if ((*(u32 *)&field_view.components.descriptors[i].flags & 0x60) == 0x40) {
            if ((field_view.components.descriptors[i].actor->layer_flags & 0x600) != 0x200
                && !(field_view.components.descriptors[i].actor->layer_flags & 0x1000)
                && !(field_view.components.descriptors[i].actor->flags & 1)) {
                sprite_vm_tick(field_view.components.descriptors[i].model);
            }
        } else if (field_view.components.descriptors[i].actor->layer_flags & 0x1000000) {
            sprite_vm_tick(field_view.components.descriptors[i].model);
        }
    }
    field_draw_shadows(field_current_draw_block->ot, field_draw_buffer_index);
    if (field_monitor_absent == 0) {
        field_debug_mark_cpu_time("CHAR      ");
    }
}

/* 80075458: Link a table's primitives into `ot` (AddPrims). */
void field_draw_link_ot(void *ot, u_long *table, s32 depth) {
    AddPrims(ot, table + depth, table);
}

/* 80075484: Draw the 801e-layer object from the camera eye and target when event
 * parameters are enabled. */
void field_draw_panorama(void) {
    SVECTOR eye;
    SVECTOR target;

    if (field_panorama_parameters.enabled != 0 && field_panorama_hidden == 0) {
        eye.vx = field_view.eye.vx >> 16;
        eye.vy = field_view.eye.vy >> 16;
        eye.vz = field_view.eye.vz >> 16;
        target.vx = field_view.target.vx >> 16;
        target.vy = field_view.target.vy >> 16;
        target.vz = field_view.target.vz >> 16;
        gpu_draw_panorama(field_panorama, &eye, &target, &field_view.scaled_world,
                      field_current_draw_block->ot + field_work.unk21D4 + 0x1000, field_draw_buffer_index);
    }
}

/* 8007554C: One field frame: the move phase, then drawing effects, dialogue, the
 * compass, characters and models (on a scratchpad stack) and the other
 * layers; flush, clear or copy the next buffer, put its environments,
 * link the depth tables and draw them, then wait out the frame rate. */
void field_run_frame(void) {
    RECT rect;
    s32 start;
    s32 now;
    s32 frames;

    field_frame_start_time = VSync(1);
    start = VSync(-1);
    field_run_move_phase();
    field_sound_update_emitters();
    if (field_monitor_absent == 0) {
        field_debug_mark_cpu_time("SEFFECT   ");
    }
    field_fade_update_channels(field_current_draw_block->overlay_ot, field_draw_buffer_index);
    if (field_monitor_absent == 0) {
        field_debug_mark_cpu_time("MESSAGE   ");
    }
    field_compass_draw();
    STACK_ENTER(0x1F8003FC);
    field_draw_models();
    field_draw_characters();
    field_effect_update_slots();
    if (field_monitor_absent == 0) {
        field_debug_draw_lines();
    }
    field_distortion_draw();
    STACK_LEAVE();
    field_status_panel_draw();
    field_draw_panorama();
    field_layer_draw();
    field_wide_overlay_draw();
    if (field_monitor_absent == 0) {
        field_debug_finish_frame();
        field_debug_mark_cpu_time("FntPrint  ");
    }
    field_frame_cpu_time = VSync(1);
    DrawSync(0);
    field_dialogue_close_expired_windows();
    field_dialogue_draw_windows(field_current_draw_block->overlay_ot, field_draw_buffer_index);
    VSync(0);
    heap_update_delayed_frees();
    if (field_camera_cut_timer == 0) {
        ClearImage(&field_current_draw_block->draw2.clip, field_work.clear_color[0], field_work.clear_color[1],
                   field_work.clear_color[2]);
    } else if (field_map_change_kind == 3) {
        rect.x = 0x2C0;
        rect.y = 0x100;
        rect.w = 0x140;
        rect.h = 0xE0;
        MoveImage(&rect, 0, field_draw_buffer_index << 8);
    } else {
        ClearImage(&field_current_draw_block->draw2.clip, 0, 0, 0);
    }
    PutDispEnv(&field_current_draw_block->disp);
    PutDrawEnv(&field_current_draw_block->draw);
    if (field_monitor_absent == 0) {
        field_frame_start_time = VSync(1);
    }
    sprite_queue_run_uploads();
    if (field_monitor_absent == 0) {
        field_debug_mark_cpu_time("ShapeTrans");
    }
    field_run_texture_scrolls();
    if (field_screen_band_upload_pending != 0) {
        LoadImage(&field_screen_band_rect, (u_long *)field_screen_band_work_pixels);
        field_screen_band_upload_pending = 0;
    }
    if (field_monitor_absent == 0) {
        field_debug_mark_cpu_time("LineScroll");
    }
    if (field_camera_cut_timer == 0) {
        if (field_draw_second_ot_enabled != 0) {
            field_draw_link_ot(&field_current_draw_block->ot[field_work.unk21D4], field_current_draw_block->ot2, field_work.unk21D4);
        }
        field_draw_link_ot(&field_current_draw_block->overlay_ot[7], field_current_draw_block->ot, field_work.unk21D4);
    }
    DrawOTag(&field_current_draw_block->overlay_ot[7]);
    do {
        now = VSync(-1);
        frames = field_work.unk217C + 2;
    } while (now < start + frames);
}

/* 80075910: Finish the frame: draw the overlays, flush, sync, copy the shown half,
 * then put this block's environments and draw its overlay table. */
void field_movie_run_overlay_frame(void) {
    RECT rect;

    field_pad_drain_queue();
    field_event_run_all_actors();
    field_dialogue_close_expired_windows();
    field_dialogue_draw_windows(field_current_draw_block->overlay_ot, field_draw_buffer_index);
    DrawSync(0);
    VSync(0);
    rect.x = 0x140;
    rect.w = 0x140;
    rect.h = 0xE0;
    rect.y = ((field_movie_decoded_block + 1) & 1) << 8;
    MoveImage(&rect, 0, field_draw_buffer_index << 8);
    PutDispEnv(&field_current_draw_block->disp);
    PutDrawEnv(&field_current_draw_block->draw);
    DrawOTag(&field_current_draw_block->overlay_ot[7]);
}

/* 800759E4: The rotation matrix whose second row is `axis`: the first row is the unit
 * vector perpendicular to world up and `axis`, the third completes the basis. */
void field_matrix_build_from_axis(MATRIX *m, VECTOR *axis) {
    VECTOR up = { 0, 0, 0x1000, 0 };
    VECTOR side;
    VECTOR cross;

    OuterProduct12(&up, axis, &cross);
    VectorNormal(&cross, &side);
    OuterProduct12(&side, axis, &cross);
    VectorNormal(&cross, &up);
    m->m[0][0] = side.vx;
    m->m[0][1] = side.vy;
    m->m[0][2] = side.vz;
    m->m[1][0] = axis->vx;
    m->m[1][1] = axis->vy;
    m->m[1][2] = axis->vz;
    m->m[2][0] = up.vx;
    m->m[2][1] = up.vy;
    m->m[2][2] = up.vz;
}

/* 80075B08: Pass a colour on to resident 80021b98 unless 800b218e is set. */
void field_sprite_set_color_unless_fog(void *target, u8 *color) {
    if (field_work.sprite_gate == 0) {
        sprite_set_part_color(target, color[0], color[1], color[2]);
    }
}

/* 80075B44: Draw the sprite actors: keep each one's previous placement, project it to
 * set its off-screen flag, then scale, fog and draw its sprite at its depth
 * in `ot` (layered sprites twice, split sprites in two parts); party actors
 * drawn by the 801e module get their layer object's state instead. */
void field_draw_sprite_actors(u_long *ot, s32 buffer) {
    SVECTOR v;
    SVECTOR raised;
    VECTOR scale;
    MATRIX placed;
    MATRIX unused; /* unreferenced, but part of the frame */
    MATRIX orient;
    CVECTOR color;
    s32 sxy;
    s32 interpolation;
    s32 flag;
    s32 depth;
    s32 elevation;
    s32 octant;
    s32 upper;
    s32 i;
    s32 party;
    u16 kind;
    s32 x;
    s32 y;
    u32 layer_flags;
    FieldActor *actor;
    Sprite *sprite;
    u32 side;

    elevation = (s16)field_view.elevation;
    octant = field_camera_get_octant() & 0xFFFF;
    field_matrix_copy_rotation(&orient, &field_view.orbit);
    raised.vx = 0;
    raised.vz = 0;
    raised.vy = -(elevation / 3 * 2);
    party = 0;
    for (i = 0; i < field_event_actor_count; i++) {
        kind = field_view.components.descriptors[i].flags;
        if (!(kind & 0x40)) {
            continue;
        }
        actor = field_view.components.descriptors[i].actor;
        sprite = field_view.components.descriptors[i].model;
        layer_flags = actor->layer_flags;
        field_view.components.descriptors[i].transform = field_view.components.descriptors[i].matrix;
        if (!(layer_flags & 0x2000)) {
            gte_CompMatrix(&field_view.scaled_world, &field_view.components.descriptors[i].matrix, &placed);
            gte_SetRotMatrix(&placed);
            gte_SetTransMatrix(&placed);
            gte_RotTransPers(&raised, &sxy, &interpolation, &flag, &depth);
            y = sxy >> 16;
            x = (s16)sxy;
            if ((u32)(y + 9) < 0x143 && (u32)(x + 0x27) < 0x18F) {
                actor->layer_flags &= ~0x200;
            } else {
                actor->layer_flags |= 0x200;
            }
            if (mode_debug_hide_sprites != 0 || (kind & 0x20) || flag < 0) {
                continue;
            }
            scale.vx = actor->scale[0] * 3 >> 2;
            scale.vy = actor->scale[1] * 3 >> 2;
            scale.vz = actor->scale[2] * 3 >> 2;
            if (actor->unkE4 == 7 && field_work.unk2268 != 0) {
                scale.vx = scale.vx * 5 >> 2;
                scale.vy = scale.vy * 5 >> 2;
                scale.vz = scale.vz * 5 >> 2;
            }
            sprite->renderer->matrix = orient;
            ScaleMatrix(&sprite->renderer->matrix, &scale);
            if (field_view.components.descriptors[i].actor->unk014 & 0x200000) {
                side = (octant - (((field_view.components.descriptors[i].actor->unk014 >> 11) - 2) & 7)) & 7;
                if (side != 0) {
                    if (side < 4) {
                        v.vx = 0;
                        v.vy = -0x80;
                        v.vz = 0;
                        depth = RotTransPers(&v, (long *)&sxy, (long *)&interpolation, (long *)&flag);
                    } else if (side < 8) {
                        if (side >= 5) {
                            v.vx = 0;
                            v.vy = 0x80;
                            v.vz = 0;
                            depth = RotTransPers(&v, (long *)&sxy, (long *)&interpolation, (long *)&flag);
                        }
                    }
                }
            }
            if (field_work.unk2357 == 0 && field_work.sprite_gate != 0) {
                gte_ldrgb(&model_color);
                gte_dpcs();
                gte_strgb(&color);
                sprite_set_part_color(field_view.components.descriptors[i].model, color.r, color.g, color.b);
            }
            depth >>= model_ot_depth_shift;
            if (depth >= 2) {
                depth -= 2;
            }
            if ((u16)(actor->unkE8 + 0x22) < 2) {
                if (!(actor->layer_flags & 0x02000000)) {
                    sprite_set_part_color(sprite, actor->color0[0], actor->color0[1], actor->color0[2]);
                    sprite->render.bytes[1] = 0xEF;
                    sprite_draw(sprite, ot + depth - 0x10);
                    v.vx = 0;
                    v.vy = 300;
                    v.vz = 0;
                    upper = RotTransPers(&v, (long *)&sxy, (long *)&interpolation, (long *)&flag) >> model_ot_depth_shift;
                    sprite_set_part_color(sprite, actor->color1[0], actor->color1[1], actor->color1[2]);
                    sprite->render.bytes[1] = 0xF7;
                    sprite_draw(sprite, ot + upper);
                }
            } else {
                sprite->render.bytes[1] = 0;
                if (!(actor->layer_flags & 0x02000000)) {
                    if (!(actor->unk134 & 0x60)) {
                        field_sprite_set_color_unless_fog(sprite, actor->color0);
                        sprite_draw(sprite, ot + depth);
                    } else {
                        if ((actor->unk134 >> 5) & 1) {
                            field_sprite_set_color_unless_fog(sprite, actor->color0);
                            v.vx = 0;
                            v.vy = (actor->unkEE - elevation / 3) * 2;
                            v.vz = 0;
                            upper = RotTransPers(&v, (long *)&sxy, (long *)&interpolation, (long *)&flag) >> model_ot_depth_shift;
                            if (upper >= 2) {
                                upper -= 2;
                            }
                            sprite_draw_cut_below(sprite, ot + upper, actor->unkEE);
                        }
                        if ((actor->unk134 >> 5) & 2) {
                            field_sprite_set_color_unless_fog(sprite, actor->color1);
                            sprite_draw_cut_above(sprite, ot + depth, actor->unkEE);
                        }
                    }
                }
            }
        } else if (mode_debug_hide_layer == 0) {
            if (!(actor->flags & 0x10000) && !(actor->unk014 & 0x200002) && !(actor->layer_flags & 0x800)) {
                gear_model_actors[party]->flags &= 0xFFFE;
            } else {
                gear_model_actors[party]->flags |= 1;
            }
            if (!(kind & 0x20)) {
                gear_model_actors[party]->active = 1;
            } else {
                gear_model_actors[party]->active = 0;
            }
            if (!(actor->layer_flags & 0x20000)) {
                gear_model_actors[party]->parts->rotation.vy = actor->unk108 + 0xC00;
            } else {
                actor->heading_goal = actor->unk108 = gear_model_actors[party]->parts->rotation.vy - 0xC00;
            }
            gear_model_actors[party]->scale = (actor->scale[0] * field_work.layer_scales[party]) >> 12;
            gear_model_actors[party]->groundY = actor->position[1] >> 16;
            gear_model_actors[party]->parts->translation[0] = actor->position[0] >> 16;
            gear_model_actors[party]->parts->translation[2] = actor->position[2] >> 16;
            party++;
            actor->layer_flags &= ~0x200;
        }
    }
}

/* 800764B4: Draw the drop shadows: for every visible sprite actor build a matrix that
 * lays the shadow quad on the floor under it (its axes from the floor
 * normal), scale it by the actor's size, and link the projected quad into
 * `ot`. */
void field_draw_shadows(u_long *ot, s32 buffer) {
    VECTOR up;
    VECTOR side;
    VECTOR cross;
    MATRIX floor;
    MATRIX placed;
    MATRIX world;
    VECTOR scale;
    long interpolation;
    long flag;
    s32 depth;
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 i;
    s32 sx;
    s32 sy;
    s32 sz;

    world = field_view.scaled_world; /* an unused copy */
    if (mode_debug_hide_sprites != 0) {
        return;
    }
    for (i = 0; i < field_event_actor_count; i++) {
        descriptor = &field_view.components.descriptors[i];
        if ((DESCRIPTOR_FLAGS_WORD(descriptor) & 0x60) != 0x40) {
            continue;
        }
        actor = descriptor->actor;
        if (actor->layer_flags & 0x102200) {
            continue;
        }
        if (actor->layer_flags & 0x800) {
            continue;
        }
        if (actor->flags & 0x10000) {
            continue;
        }
        if (actor->unk014 & 0x200002) {
            continue;
        }
        up.vx = 0;
        up.vy = 0;
        up.vz = 0x1000;
        gte_OuterProduct12(&up, descriptor->actor->unk50, &cross);
        VectorNormal(&cross, &side);
        gte_OuterProduct12(&side, descriptor->actor->unk50, &cross);
        VectorNormal(&cross, &up);
        floor.m[0][0] = side.vx;
        floor.m[0][1] = side.vy;
        floor.m[0][2] = side.vz;
        floor.m[1][0] = descriptor->actor->unk50[0];
        floor.m[1][1] = descriptor->actor->unk50[1];
        floor.m[1][2] = descriptor->actor->unk50[2];
        floor.m[2][0] = up.vx;
        floor.m[2][1] = up.vy;
        floor.m[2][2] = up.vz;
        floor.t[0] = descriptor->matrix.t[0];
        floor.t[1] = descriptor->model->ground;
        floor.t[2] = descriptor->matrix.t[2];
        gte_CompMatrix(&field_view.scaled_world, &floor, &placed);
        sx = descriptor->actor->scale[0] * 0xC00;
        scale.vx = sx >> 12;
        sy = descriptor->actor->scale[1] * 0xC00;
        scale.vy = sy >> 12;
        sz = descriptor->actor->scale[2] * 0xC00;
        scale.vz = sz >> 12;
        if (field_work.unk2268 != 0 && (descriptor->actor->flags & 0x400)) {
            scale.vx = sx >> 14;
            scale.vy = sy >> 14;
            scale.vz = sz >> 14;
        }
        ScaleMatrix(&placed, &scale);
        gte_SetRotMatrix(&placed);
        gte_SetTransMatrix(&placed);
        depth = RotAverage4(&descriptor->shadow->v[0], &descriptor->shadow->v[1], &descriptor->shadow->v[2],
                            &descriptor->shadow->v[3],
                        (long *)&descriptor->shadow->poly[buffer].x0, (long *)&descriptor->shadow->poly[buffer].x1,
                        (long *)&descriptor->shadow->poly[buffer].x2, (long *)&descriptor->shadow->poly[buffer].x3,
                        &interpolation, &flag);
        depth >>= model_ot_depth_shift;
        addPrim(ot + depth, &descriptor->shadow->poly[buffer]);
    }
}

/* 80076A74: Sprite completion callback: flag the sprite's actor (layer bit 16). */
void field_actor_on_sprite_done(Sprite *sprite) {
    field_view.components.descriptors[SPRITE_SEQUENCER(sprite)->actor].actor->layer_flags |= 0x10000;
}

/* 80076AC0: Create an event actor's sprite: record its slot and arguments on the
 * actor, release any sprite the descriptor had, build the new one (a
 * character sheet in the slot's VRAM area, a banked sheet, or one of the two
 * small effect kinds), place it at the actor and register the completion
 * callback. */
void field_actor_create_sprite(s32 index, s32 slot, void *data, s32 kind, s32 bank, s32 unk, s32 flag) {
    s32 width;
    s32 height;
    s32 depth;
    Sprite *sprite;
    s32 y;
    s32 x;

    heap_select_owner_tag(8, 0);
    field_view.components.descriptors[index].actor->unk127 = slot;
    field_view.components.descriptors[index].actor->unk126 = unk;
    field_view.components.descriptors[index].actor->unk134 =
        (field_view.components.descriptors[index].actor->unk134 & ~0xF) | (bank & 0xF);
    field_view.components.descriptors[index].actor->sprite_kind = kind;
    field_view.components.descriptors[index].actor->unk134 =
        (field_view.components.descriptors[index].actor->unk134 & ~0x10) | ((flag & 1) << 4);
    if (kind == 0) {
        y = field_sprite_slots.slot[slot].y;
        x = field_sprite_slots.slot[slot].x;
        if (bank == 0) {
            if (field_view.components.descriptors[index].unk5A & 1) {
                sprite_destroy(field_view.components.descriptors[index].model);
            }
            sprite = sprite_create(data, 0x100, slot + 0x1E0, x, y, 0x40);
            field_view.components.descriptors[index].model = sprite;
        } else {
            if (field_view.components.descriptors[index].unk5A & 1) {
                sprite_destroy(field_view.components.descriptors[index].model);
            }
            sprite = sprite_create_with_palette_bank(data, bank * 16 + 0x100, slot + 0x1E0, x, y, 0x40, bank);
            field_view.components.descriptors[index].model = sprite;
        }
    } else {
        if (field_view.components.descriptors[index].unk5A & 1) {
            sprite_destroy(field_view.components.descriptors[index].model);
        }
        if (kind == 1) {
            y = (slot << 6) + 0x100;
            sprite = sprite_create(data, 0x100, slot + 0xE0, 0x280, y, 8);
            field_view.components.descriptors[index].model = sprite;
            sprite_realloc_parts_from_bottom(sprite, 0x20);
        } else {
            y = (slot << 6) + 0x100;
            sprite = sprite_create(data, 0x100, slot + 0xE3, 0x2A0, y, 8);
            field_view.components.descriptors[index].model = sprite;
            sprite_realloc_parts_from_bottom(sprite, 0x20);
        }
    }
    field_view.components.descriptors[index].unk5A |= 1;
    sprite_get_extent(sprite, 0, &width, &height, &depth);
    sprite_set_scale_shift(sprite, 3);
    sprite->scale = 0xC00;
    sprite->word82 = 0x2000;
    if (mode_field_return_pending == 0) {
        sprite->x = field_view.components.descriptors[index].actor->position[0];
        sprite->y = field_view.components.descriptors[index].actor->position[1];
        sprite->z = field_view.components.descriptors[index].actor->position[2];
        sprite->ground = field_view.components.descriptors[index].matrix.t[1];
        sprite->speed_y = 0;
        sprite->speed_x = 0;
        sprite->speed_y = 0;
        sprite->speed_z = 0;
        sprite->gravity = 0x10000;
        if (kind == 0) {
            field_view.components.descriptors[index].actor->height = height * 2;
        } else {
            field_view.components.descriptors[index].actor->height = 0x40;
        }
    }
    if (field_work.sprite_gate != 0) {
        sprite->flags |= 0x40000;
    }
    sprite_start_animation(sprite, 0);
    sprite_set_direction(sprite, 0);
    heap_select_owner_tag(8, 0);
    SPRITE_SEQUENCER(sprite)->actor = index;
    sprite_set_completion_callback(sprite, field_actor_on_sprite_done);
    if (flag == 0) {
        sprite_vm_tick(sprite);
        task_run_main_list();
        if ((u16)SPRITE_SEQUENCER(sprite)->halfc == 0xFF) {
            field_view.components.descriptors[index].actor->unk0EA = 0xFF;
            field_view.components.descriptors[index].actor->layer_flags |= 0x01000000;
            sprite->x = field_view.components.descriptors[index].actor->position[0];
            sprite->y = field_view.components.descriptors[index].actor->position[1];
            sprite->z = field_view.components.descriptors[index].actor->position[2];
        }
    }
    field_view.components.descriptors[index].transform.t[0] = field_view.components.descriptors[index].matrix.t[0] =
        WHOLE(field_view.components.descriptors[index].actor->position[0]);
    field_view.components.descriptors[index].transform.t[1] = field_view.components.descriptors[index].matrix.t[1] =
        WHOLE(field_view.components.descriptors[index].actor->position[1]);
    field_view.components.descriptors[index].transform.t[2] = field_view.components.descriptors[index].matrix.t[2] =
        WHOLE(field_view.components.descriptors[index].actor->position[2]);
    sprite->ground = field_view.components.descriptors[index].matrix.t[1];
    sprite->x = field_view.components.descriptors[index].actor->position[0];
    sprite->y = field_view.components.descriptors[index].actor->position[1];
    sprite->z = field_view.components.descriptors[index].actor->position[2];
    field_sprite_created_count++;
}

/* 800771B0: Set the semi-transparency bit of each of `count` 16-bit pixels. */
void field_set_pixels_stp_bit(u32 *pixels, s32 count) {
    s32 i;

    i = count / 2;
    while (--i != -1) {
        *pixels++ |= 0x80008000;
    }
}

/* 800771F8: Load every image (and CLUT) of a TIM list into VRAM where it lies. */
void field_load_tim_list(u32 *tim) {
    TIM_IMAGE image;

    OpenTIM((u_long *)tim);
    while (ReadTIM(&image) != NULL) {
        if (image.caddr != NULL) {
            LoadImage(image.crect, image.caddr);
        }
        if (image.paddr != NULL) {
            LoadImage(image.prect, image.paddr);
        }
    }
}

/* Declared without a prototype: 80084a40 also reads a fifth, stack
 * argument that this caller never passes. */
s32 field_actor_update_position();

/* 80077268: Place the party at the controlled actor: run its position pass (80084a40),
 * then give each other party member (slots 1 and 2) the leader's model
 * position and descriptor origin after its own pass, and fill the 32
 * history records. */
void field_party_place_at_controlled(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    Sprite *model;
    s32 slot;
    s32 i;

    field_actor_update_position(field_work.controlled,
                  WHOLE(field_view.components.descriptors[field_work.controlled].actor->position[1]),
                  &field_view.components.descriptors[field_work.controlled],
                  field_view.components.descriptors[field_work.controlled].actor);
    for (i = 0; i < field_event_actor_count; i++) {
        descriptor = &field_view.components.descriptors[i];
        actor = descriptor->actor;
        if ((descriptor->flags & 0xF80) == 0x200) {
            slot = field_party_find_character_slot(actor->unkE4);
            if (slot != -1) {
                model = field_view.components.descriptors[i].model;
                if (slot != 0) {
                    field_actor_update_position(i, WHOLE(field_view.components.descriptors[i].actor->position[1]),
                                  descriptor, actor);
                    model->x = field_view.components.descriptors[field_work.controlled].model->x;
                    model->y = field_view.components.descriptors[field_work.controlled].model->y;
                    model->z = field_view.components.descriptors[field_work.controlled].model->z;
                    descriptor->matrix.t[0] = field_view.components.descriptors[field_work.controlled].matrix.t[0];
                    descriptor->matrix.t[1] = field_view.components.descriptors[field_work.controlled].matrix.t[1];
                    descriptor->matrix.t[2] = field_view.components.descriptors[field_work.controlled].matrix.t[2];
                }
            }
        }
    }
    field_movement_history_indices[2] = 0;
    field_movement_history_indices[1] = 0;
    field_movement_history_indices[0] = 0;
    for (i = 0; i < 0x20; i++) {
        field_record_movement_history(field_work.controlled);
    }
}

/* 80077544: Load the text palette, with the debug font first when enabled. */
void field_load_text_palette(void) {
    if (field_monitor_absent == 0) {
        console_set_external_block(0x80270000);
        console_open(0x10, 0x10, 0x130, 0xE0, 0x400, 4, 0x3C0, 0x100, 0x100, 0x1FF, 0);
    }
    text_load_palette(0x100, 0xF0);
}

/* 800775C0: Select the field's heap tag and directory, then set up the pointer. */
void field_select_heap_dir_and_init_pointer(void) {
    heap_select_owner_tag(8, 0);
    cd_select_directory(4, 0);
    field_pointer_init();
}

/* 800775F8: Wait for drawing to finish (DrawSync), then VSync. */
void field_sync_draw_and_vsync(void) {
    DrawSync(0);
    VSync(0);
}

/* 80077620: Load the field's text images (file a7, read once while 8004f344 is clear):
 * relocate its offset table, load its eight TIMs where 800adc44 places them
 * (x, y, palette x, y, w, h), read the compass colours back from VRAM (0, fb)
 * and release the file. */
void field_load_text_images(void) {
    u32 **tim;
    s32 i;

    if (mode_text_images_preloaded == 0) {
        mode_preloaded_text_images = heap_alloc(cd_get_aligned_file_size(0xA7), 1);
        heap_protect_block(mode_preloaded_text_images);
        cd_read_file(0xA7, mode_preloaded_text_images, 0, 0x80);
        cd_sync_reads(0);
    }
    heap_unprotect_block(mode_preloaded_text_images);
    mode_text_images_preloaded = 0;
    field_unread_text_pair1[1] = 0;
    field_unread_text_pair1[0] = 0;
    field_unread_text_pair2[1] = 0;
    field_unread_text_pair2[0] = 0;
    text_relocate_offset_table(mode_preloaded_text_images);
    tim = (u32 **)mode_preloaded_text_images;
    for (i = 0; i < 8; i++) {
        field_load_tim_at(*++tim, field_text_image_places[i * 6], field_text_image_places[i * 6 + 1], field_text_image_places[i * 6 + 2],
                      field_text_image_places[i * 6 + 3], field_text_image_places[i * 6 + 4], field_text_image_places[i * 6 + 5]);
        DrawSync(0);
    }
    field_compass_palette_rect.x = 0;
    field_compass_palette_rect.y = 0xFB;
    field_compass_palette_rect.w = 0x10;
    field_compass_palette_rect.h = 1;
    StoreImage(&field_compass_palette_rect, (u_long *)field_compass_colors);
    DrawSync(0);
    heap_free(mode_preloaded_text_images);
}

/* 800777DC: Stop the stream, then read the map's data ahead until it is in. */
void field_wait_map_read_ahead(void) {
    cd_sync_reads(0);
    while (mode_read_map_ahead((mode_field_map_id & 0xFFF) * 2, 0) != 0) {
    }
}

/* 8007781C: Record the VSync counter. */
void field_record_gpu_time(void) {
    field_frame_gpu_time = VSync(1);
}

/* 80077844: Set a matrix's nine rotation elements. */
void field_matrix_set_rotation(MATRIX *m, s32 m00, s32 m01, s32 m02, s32 m10, s32 m11, s32 m12, s32 m20,
                   s32 m21, s32 m22) {
    m->m[0][0] = m00;
    m->m[0][1] = m01;
    m->m[0][2] = m02;
    m->m[1][0] = m10;
    m->m[1][1] = m11;
    m->m[1][2] = m12;
    m->m[2][0] = m20;
    m->m[2][1] = m21;
    m->m[2][2] = m22;
}

/* 80077884: Load the 801e module and its per-layer resources when the layer is
 * enabled: allocate the module (file 6b9), two blocks per layer (files
 * 6bb and 6ba plus the layer's id), then read them all as one list. */
void field_layer_load(void) {
    u32 end;
    s32 i;

    if (field_work.unk2264 != 0) {
        field_music_wait_stream_and_disc_idle();
        cd_select_directory(4, 0);
        field_vram_column_relocate(0);
        end = field_heap_top;
        if (mode_field_standalone == 0) {
            field_layer_module = heap_alloc((end & 0xFFFFFF) - 0x1DC008, 1);
        } else {
            field_layer_module = heap_alloc(cd_get_aligned_file_size(0x6B9), 1);
        }
        field_vram_column_relocate(1);
        for (i = 0; i < field_work.unk2264; i++) {
            field_layer_file_requests[i * 2 + 1].file = field_work.unk21DC[i] + 0x6BB;
            mode_field_layer_model_files[i] = heap_alloc(cd_get_aligned_file_size(field_work.unk21DC[i] + 0x6BB), 1);
            field_layer_file_requests[i * 2 + 1].destination = mode_field_layer_model_files[i];
        }
        for (i = 0; i < field_work.unk2264; i++) {
            field_layer_file_requests[i * 2].file = field_work.unk21DC[i] + 0x6BA;
            mode_field_layer_script_files[i] = heap_alloc(cd_get_aligned_file_size(field_work.unk21DC[i] + 0x6BA), 0);
            field_layer_file_requests[i * 2].destination = mode_field_layer_script_files[i];
        }
        field_layer_file_requests[i * 2].file = 0x6B9;
        field_layer_file_requests[i * 2].destination = field_layer_module;
        field_layer_file_requests[i * 2 + 1].file = 0;
        field_layer_file_requests[i * 2 + 1].destination = 0;
        field_music_wait_stream_and_disc_idle();
        cd_read_file_list(field_layer_file_requests, 0, 0);
    }
}

/* 80077AB4: Start the 801e module's layers when enabled: sync and flush the cache,
 * initialise the module, set the back colour, then create each layer from
 * its two resources (releasing the first) and keep its scale. */
void field_layer_start(void) {
    SVECTOR *position;
    s32 row;
    s32 i;

    if (field_work.unk2264 != 0) {
        field_music_wait_stream_and_disc_idle();
        field_sync_and_flush_cache();
        gear_model_init(field_work.unk234A);
        gear_model_color_matrix = (MATRIX *)field_work.unk223C;
        SetBackColor(field_work.unk225C[0], field_work.unk225C[1], field_work.unk225C[2]);
        for (i = 0; i < field_work.unk2264; i++) {
            position = &field_work.layer_positions[i];
            position->vx = 0;
            position->vy = 0;
            position->vz = 0;
            row = field_work.unk225F[i];
            gear_model_create_actor(i, 0, mode_field_layer_script_files[i], mode_field_layer_model_files[i],
                          (s16)(0x240 - ((i + row) << 6)), 0x100, 0, (s16)(i + 0xFC),
                          &position->vx);
            heap_free(mode_field_layer_model_files[i]);
            field_work.layer_scales[i] = gear_model_actors[i]->scale;
        }
        heap_select_owner_tag(8, 0);
    }
}

/* 80077C60: Run 80077884 then 80077ab4. */
void field_layer_load_and_start(void) {
    field_layer_load();
    field_layer_start();
}

/* 80077C88: Allocate and keep the three party sprite blocks (0x14000 bytes each). */
void field_party_alloc_sprite_blocks(void) {
    heap_select_owner_tag(8, 0);
    mode_party_sprite_blocks[0] = heap_alloc(0x14000, 0);
    mode_party_sprite_blocks[1] = heap_alloc(0x14000, 0);
    mode_party_sprite_blocks[2] = heap_alloc(0x14000, 0);
    heap_protect_block(mode_party_sprite_blocks[0]);
    heap_protect_block(mode_party_sprite_blocks[1]);
    heap_protect_block(mode_party_sprite_blocks[2]);
}

/* 80077D2C: Unlink, then release, the three blocks at 8005a414..8005a41c. */
void field_party_free_sprite_blocks(void) {
    heap_unprotect_block(mode_party_sprite_blocks[0]);
    heap_unprotect_block(mode_party_sprite_blocks[1]);
    heap_unprotect_block(mode_party_sprite_blocks[2]);
    heap_free(mode_party_sprite_blocks[0]);
    heap_free(mode_party_sprite_blocks[1]);
    heap_free(mode_party_sprite_blocks[2]);
}

/* "Clear OTAG". A stray byte (0x6b) follows the string at the end of the
 * unit's rodata, so the literal is linked as original rodata. */
INCLUDE_RODATA(".local/decomp/field/asm/nonmatchings/field", field_clear_otag_label); /* 8006FB80 */
extern char field_clear_otag_label[];

/* 80077DAC: Field pre-frame work: record the VSync counter, clear the order table,
 * run 80074700, start the debug "Clear OTAG" timer and 800a31e8. */
void field_run_pre_frame(void) {
    field_frame_start_time = VSync(1);
    field_draw_swap_and_clear_ots();
    field_pad_drain_queue();
    if (field_monitor_absent == 0) {
        field_debug_mark_cpu_time(field_clear_otag_label);
    }
    field_update_play_record();
}

/* 80077E10: -1 when the field may leave (800adbd0 is 1, 800b2344 clear, and the
 * controlled actor has flag 0x800), else 0. */
s32 field_is_encounter_held_by_jump(void) {
    if (field_encounter_battle_pending == 1 && field_work.jump_mode == 0
        && (field_view.components.descriptors[field_work.controlled].actor->flags & 0x800)) {
        return -1;
    }
    return 0;
}

void field_record_gpu_time(void);

/* 80077E88: The field mode entry: set up the heap and the debug hooks, take the map
 * and music from the game state, run the field entry (80078d44), then the
 * frame loop until an exit is requested: pauses (pad start, or the stream
 * stopping), battle requests (with the battle music), the queued map, world
 * map and movie exits, menus and debug keys. Leaves through 8007954c with
 * the exit kind. The battle music is set ahead of 8004f308 (independent
 * stores): in the other order the address cse shares between it and +2355
 * lives one insn longer (17), and loop.c hoists it out of the frame loop. */
void field_main(void) {
    u8 unused[8]; /* never used; the original frame reserves it */
    s32 exit;
    s32 held;
    s32 saved;
    s32 entered;
    u16 pressed;
    u16 buttons;

    if (mode_disc_mode != -1) {
        field_monitor_absent = 0;
    } else {
        field_monitor_absent = 1;
    }
    field_sync_and_flush_cache();
    if (field_monitor_absent == 0) {
        DrawSyncCallback(field_record_gpu_time);
    }
    mode_wave_bank_slots[1] = (s32)mode_shared_wave_bank;
    mode_wave_bank_slots[3] = (s32)mode_wave_bank_5;
    heap_select_owner_tag(8, 0);
    if (field_monitor_absent == 0 && mode_field_standalone == 0) {
        cd_select_directory(4, 0);
        cd_read_file(0xAD, (void *)0x80280000, 0, 0x80);
        cd_sync_reads(0);
        field_sync_and_flush_cache();
    }
    if (mode_field_return_pending == 0) {
        mode_party_file_ids[2] = 0xFF;
        mode_party_file_ids[1] = 0xFF;
        mode_party_file_ids[0] = 0xFF;
    }
    field_sound_load_effect_bank(0);
    held = 0;
    field_party_alloc_sprite_blocks();
    field_arena_exit_not_requested = -1;
    field_worldmap_exit_not_requested = -1;
    field_scripted_battle_not_requested = -1;
    field_battle_not_requested = -1;
    field_mode_exit_not_requested = -1;
    mode_music_seq_read_pending = 0;
    mode_music_wave_streaming = 0;
    field_scratchpad_used_words = 0;
    field_map_stream_running = 0;
    field_vram_column_saved = 0;
    field_movie_presenting = 0;
    field_fade_mode = 2;
    if (field_monitor_absent == 0) {
        field_debug_reset_screen_cursor();
    }
    field_select_heap_dir_and_init_pointer();
    game_current_data = &game_data;
    mode_field_map_id = game_data.map;
    game_data.vars[1] = game_data.entry[2];
    game_data.vars[4] = game_data.entry[0] >> 9;
    if (mode_field_entered_once == 0) {
        mode_result_code = 0;
        mode_music_selected_track = 0xFF;
    } else {
        mode_music_selected_track = game_data.flagWords[0];
    }
    if (field_monitor_absent == 1) {
        game_current_data->vars[0x28] = 1;
        field_event_write_variable(0x50, 1);
    }
    field_load_text_images();
    mode_party_uses_gear_files = 0;
    mode_sync_party_files();
    mode_unpack_party_files();
    field_heap_top = (u32)heap_alloc(4, 1);
    if (field_monitor_absent == 0) {
        pollhost();
        field_effect_reset_templates(field_work.controlled);
        field_effect_templates[0].unk00 = 1;
        field_effect_templates[0].count = 0x10;
    }
    entered = 0;
    field_enter_map();
    field_encounters_enabled = 1;
    for (;;) {
        if (pad_get_controller_kind(0) == 0) {
            saved = pad_vblank_count;
            sound_silence_voices();
            sprite_upload_pause_image(0x88, (((field_draw_buffer_index + 1) & 1) << 8) | 0x64);
            do {
                DrawSync(0);
                VSync(2);
                field_pad_drain_queue();
                boot_check_soft_reset();
            } while (pad_get_controller_kind(0) == 0);
            sound_restore_voices();
            pad_vblank_count = saved;
        }
        if ((field_pad_port0_repeated & 0x800) && !(field_pad_port0_held & 0x40) && field_work.unk2358 == 0) {
            saved = pad_vblank_count;
            sound_silence_voices();
            sprite_upload_pause_image(0x88, (((field_draw_buffer_index + 1) & 1) << 8) | 0x64);
            do {
                DrawSync(0);
                VSync(2);
                field_pad_drain_queue();
                boot_check_soft_reset();
            } while (!(field_pad_port0_repeated & 0x800));
            sound_restore_voices();
            pad_vblank_count = saved;
        }
        if (field_monitor_absent == 1) {
            field_event_write_variable(0x50, 1);
        }
        boot_check_soft_reset();
        field_run_pre_frame();
        field_run_frame();
        field_transition_run();
        if (field_draw_buffer_index == 1 && field_battle_not_requested == 0 && field_is_exit_blocked() == 0 && field_is_encounter_held_by_jump() == 0) {
            if (mode_read_ahead_slot != -1) {
                heap_unprotect_block(mode_read_ahead_block);
                heap_free(mode_read_ahead_block);
            }
            if (entered == 0) {
                entered = 1;
                field_music_saved_for_battle = mode_music_selected_track;
            }
            field_dialogue_close_all_windows();
            if (field_encounter_battle_pending == 1) {
                mode_battle_kind = field_work.unk2355;
                field_music_saved_for_battle = mode_music_selected_track;
                if (mode_music_loaded_track != field_work.battle_music) {
                    if (mode_music_loaded_track != -1) {
                        mode_music_reuse_seq = 1;
                    }
                    mode_stop_music();
                    mode_music_selected_track = field_work.battle_music;
                    mode_music_load_pending = -1;
                    field_music_change_track(field_work.battle_music, 1);
                }
                field_encounter_battle_pending = 0;
                field_encounter_music_started = 1;
            } else {
                if (field_skip_exit_snapshot == 0) {
                    mode_field_return_pending++;
                    field_save_snapshot();
                }
                exit = 0;
                if (field_encounter_music_started == 1) {
                    sound_set_seq_fade((SoundSeq *)mode_music_seq, 0x7F, 0);
                }
                field_encounter_music_started = 0;
                break;
            }
        }
        if (field_map_change_not_requested == 0 && mode_music_load_pending == 0 && field_party_sprite_load_pending == 0xFF && field_actor_block_loading == 0
            && mode_read_map_ahead((mode_field_map_id & 0xFFF) * 2, 0) == 0 && cd_get_pending_read_count() == 0
            && field_work.fades[0].steps == 0) {
            field_encounters_enabled = 0;
            field_event_save_map_and_variables();
            cd_sync_reads(0);
            field_reload_for_map_change();
            pad_clear_queue();
            field_encounters_enabled = 1;
        }
        if (field_draw_buffer_index == 1 && field_worldmap_exit_not_requested == 0 && field_is_exit_blocked() == 0) {
            cd_sync_reads(0);
            exit = 1;
            if (mode_read_ahead_slot != -1) {
                heap_unprotect_block(mode_read_ahead_block);
                heap_free(mode_read_ahead_block);
            }
            break;
        }
        if (field_draw_buffer_index == 1 && field_arena_exit_not_requested == 0 && field_is_exit_blocked() == 0) {
            cd_sync_reads(0);
            if (mode_read_ahead_slot != -1) {
                heap_unprotect_block(mode_read_ahead_block);
                heap_free(mode_read_ahead_block);
            }
            mode_unread_arena_departure_count++;
            exit = 2;
            field_save_snapshot();
            break;
        }
        if (field_draw_buffer_index == 1 && field_mode_exit_not_requested == 0 && field_is_exit_blocked() == 0) {
            cd_sync_reads(0);
            if (mode_read_ahead_slot != -1) {
                heap_unprotect_block(mode_read_ahead_block);
                heap_free(mode_read_ahead_block);
            }
            exit = 3;
            mode_stop_music();
            break;
        }
        if (field_monitor_absent == 0) {
            pressed = field_pad_port1_repeated;
            if (pressed & 0x40) {
                mode_debug_hide_compass = (mode_debug_hide_compass + 1) & 1;
            }
            if (pressed & 0x10) {
                mode_debug_hide_sprites = (mode_debug_hide_sprites + 1) & 1;
            }
            if (pressed & 0x80) {
                mode_debug_hide_layer = (mode_debug_hide_layer + 1) & 1;
            }
            if ((field_pad_port0_held & 0x40) && (field_pad_port0_repeated & 0x100) && field_map_change_not_requested == -1 && mode_music_load_pending == 0
                && field_vram_column_saved == 0) {
                mode_field_map_id = 0;
                field_map_change_not_requested = 0;
                field_event_write_variable(2, 0);
            }
        }
        if (field_mode_exit_not_requested == -1 && field_battle_not_requested == -1 && field_worldmap_exit_not_requested == -1 && field_is_exit_blocked() == 0
            && field_map_change_not_requested == -1) {
            buttons = field_pad_port0_held;
            if (!(buttons & 3)) {
                held = 0;
            }
            if ((buttons & 1) && field_player_control_polled == 1 && (buttons & 2) && held == 0) {
                held = 1;
                field_update_gear_riding_lock();
                if (field_menu_request == 0xFF
                    && !(field_view.components.descriptors[field_work.controlled].actor->flags & 0x1800)
                    && mode_gear_riding_lock == 0) {
                    field_party_toggle_gear_riding();
                }
            }
            if ((field_pad_port0_repeated & 0x100) && field_map_change_not_requested == -1 && field_player_control_polled == 1) {
                field_picture_show();
            }
            if (field_movie_requested != 0 && field_draw_buffer_index == 1) {
                field_movie_play();
                field_movie_requested = 0;
            }
            if (field_menu_request != 0xFF && field_draw_buffer_index == 0
                && !(field_view.components.descriptors[field_work.controlled].actor->flags & 0x1800)) {
                field_dialogue_close_all_windows();
                field_run_menu();
                field_menu_request = 0xFF;
            }
            if ((field_pad_port0_repeated & 0x10) && field_work.script_control[0] == 0 && field_menu_request == 0xFF
                && field_player_control_polled == 1) {
                field_menu_request = 0x80;
                menu_state_screen_parameter = field_menu_parameter;
            }
        }
        field_run_post_frame();
    }
    field_update_gear_riding_lock();
    field_vram_column_restore();
    field_update_play_record();
    field_effect_release_all_slots();
    field_sound_clear_emitters_stop_voices();
    field_dialogue_close_all_windows();
    DrawSync(0);
    VSync(0);
    field_teardown();
    field_party_free_sprite_blocks();
    field_sound_release_effect_bank();
    mode_party_file_kind = 0;
    heap_free((void *)field_heap_top);
    field_exit_to_mode(exit);
}

/* 80078B5C: Field post-frame work: 8003fa38, resolve a pending sound, count down the
 * instant-turn frames. Declared int but returns nothing (the return register
 * stays live, so the final branch keeps an empty delay slot). */
s32 field_run_post_frame(void) {
    rand();
    if (mode_music_load_pending == -1) {
        mode_music_load_pending = field_music_advance_track_load(mode_music_selected_track);
    }
    if (field_camera_cut_timer != 0) {
        field_camera_cut_timer--;
    }
}

/* 80078BC8: 0 when no battle menu, disc, music, battle request or pending transition
 * is busy and 800adbc4 is 0xff; otherwise -1. */
s32 field_is_exit_blocked(void) {
    if (field_music_stream_running != 0) {
        return -1;
    }
    if (cd_get_pending_read_count() == 0 && mode_music_load_pending == 0 && field_actor_block_loading == 0 && field_vram_column_saved == 0
        && field_party_sprite_load_pending == 0xFF) {
        return 0;
    }
    return -1;
}

/* 80078C5C: With 800b2344 set, brighten the 256x32 text strip at (0, 1e0) in VRAM
 * (every non-transparent pixel gains 0x0c63) and disable both buffers'
 * dithering. */
void field_brighten_text_strip(void) {
    RECT rect;
    u_long *pixels;
    s32 i;

    if (field_work.jump_mode != 0) {
        field_draw_blocks[0].draw.dtd = 0;
        field_draw_blocks[1].draw.dtd = 0;
        pixels = heap_alloc(0x4000, 0);
        rect.y = 0x1E0;
        rect.w = 0x100;
        rect.x = 0;
        rect.h = 0x20;
        StoreImage(&rect, pixels);
        DrawSync(0);
        for (i = 0; i < 0x1000; i++) {
            if (pixels[i] & 0xFFFF) {
                pixels[i] |= 0xC63;
            }
            if (pixels[i] & 0xFFFF0000) {
                pixels[i] |= 0x0C630000;
            }
        }
        LoadImage(&rect, pixels);
        DrawSync(0);
        heap_free(pixels);
    }
}

/* Declared without prototypes: this caller passes arguments they ignore. */

/* 80078D44: The field entry: load the text palette and screen, set up both draw
 * buffers, load the map (80070cc8, 80070488), finish the stream read ahead
 * while fading or growing the screen pieces, release the previous music,
 * start the map's music, then run the first frames (8 after a movie, 32
 * with the fade-in otherwise). */
void field_enter_map(void) {
    RECT rect;
    s32 grow;
    s32 shade;
    s32 i;

    field_load_text_palette();
    field_vram_column_save();
    cd_select_directory(4, 0);
    field_wait_map_read_ahead();
    field_sync_draw_and_vsync();
    if (mode_field_entered_once == 0) {
        field_screen_convert_24bit_to_15bit(0);
    }
    field_draw_init_display();
    if (mode_field_entered_once == 0) {
        rect.y = 0x100;
        rect.w = 0x140;
        rect.x = 0;
        rect.h = 0xE0;
        MoveImage(&rect, 0, 0);
    }
    field_draw_buffer_index = 1;
    field_screen_save_copy();
    field_screen_copy_to(0, 0x100);
    DrawSync(0);
    field_draw_swap_and_clear_ots();
    field_sync_draw_and_vsync();
    if (mode_result_code == 1 || mode_battle_return_fade == 1) {
        field_screen_pieces_show(0, 0);
    } else {
        field_screen_pieces_show(1, 1);
    }
    mode_field_entered_once = 1;
    cd_sync_reads(0);
    cd_select_directory(4, 0);
    field_load_from_bundle();
    field_map_stream_start();
    field_dialogue_open_blocked = 1;
    if (field_work.unk2264 != 0) {
        gear_model_set_instant_keyframes(1);
    }
    if (mode_result_code == 1 || mode_battle_return_fade == 1) {
        grow = 0;
    } else {
        grow = 0x20;
    }
    shade = 0x800000;
    if (field_map_stream_running == 1) {
    stream:
            field_draw_swap_and_clear_ots();
            field_screen_pieces_draw();
            field_draw_present_overlay();
            if (mode_battle_return_fade != 1) {
                if (field_screen_pieces_scale < 0x22C0) {
                    field_screen_pieces_scale += grow;
                }
            } else {
                field_screen_pieces_set_shade(shade >> 16);
                shade -= 0x40000;
                if (shade < 0) {
                    shade = 0;
                }
            }
        if (cd_get_pending_read_count() != 0) {
            goto stream;
        }
        DrawSync(0);
        heap_free(field_map_stream_ring);
        field_map_stream_running = 0;
        field_brighten_text_strip();
    }
    if (mode_battle_return_fade == 1) {
        do {
            field_draw_swap_and_clear_ots();
            field_screen_pieces_draw();
            field_draw_present_overlay();
            field_screen_pieces_set_shade(shade >> 16);
            shade -= 0x40000;
        } while (shade >= 0);
    }
    if (mode_result_code == 1) {
        rect.w = 0x140;
        rect.y = 0;
        rect.x = 0;
        rect.h = 0xE0;
        MoveImage(&rect, 0x200, 0);
    }
    if (mode_worldmap_area_load_count != 0) {
        sound_stop_seq((SoundSeq *)mode_music_seq);
        sound_release_seq((SoundSeq *)mode_music_seq);
        sound_release_wave_bank(mode_music_wave_bank);
        mode_worldmap_area_load_count = 0;
    }
    field_finish_return_to_field();
    mode_unread_arena_departure_count = 0;
    mode_field_return_pending = 0;
    field_sync_draw_and_vsync();
    pad_clear_queue();
    mode_music_load_pending = 0;
    if (mode_result_code == 1) {
        mode_music_selected_track = 0xE;
        field_music_release_cached_seq();
    }
    if (mode_music_loaded_track != mode_music_selected_track) {
        mode_stop_music();
        mode_music_load_pending = -1;
        if (mode_music_cached_seq != 0) {
            mode_music_reuse_seq = 1;
        }
        field_music_change_track(mode_music_selected_track, 1);
    } else {
        field_music_release_cached_seq();
    }
    field_update_play_record();
    if (mode_result_code != 1) {
        if (mode_battle_return_fade != 1) {
            field_fade_in(0x20);
            shade = 0x800000;
            for (i = 0; i < 0x20; i++) {
                field_run_pre_frame();
                field_screen_pieces_draw();
                field_run_frame();
                field_run_post_frame();
                if (mode_result_code != 1) {
                    field_screen_pieces_set_shade(shade >> 16);
                    shade -= 0x40000;
                    if (shade < 0) {
                        shade = 0;
                    }
                    if (field_screen_pieces_scale < 0x22C0) {
                        field_screen_pieces_scale += grow;
                    }
                }
            }
        } else {
            field_fade_in(0x20);
        }
    } else {
        for (i = 0; i < 8; i++) {
            field_run_pre_frame();
            field_screen_pieces_draw();
            field_run_frame();
            field_run_post_frame();
        }
    }
    if (field_work.unk2264 != 0) {
        gear_model_set_instant_keyframes(0);
    }
    field_vram_column_restore();
    heap_coalesce();
    console_close();
    field_load_text_palette();
    field_dialogue_open_blocked = 0;
}

/* 80079288: Count down the random-encounter steps while encounters are possible; on a
 * step whose drawn number (800b22a0) reaches zero, pick a formation of the
 * map's set by the weights formation_encounter_weights (resident/formation.h) and request
 * battle with its music. The original is an int function (implicit int)
 * that returns no value: its epilogue keeps $v0 live, so no delay slot is
 * filled with a $v0 write. */
s32 field_encounter_count_down(void) {
    s32 start[16];
    u8 *weights;
    s32 total;
    s32 sum;
    s32 found;
    s32 i;

    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_map_change_not_requested == 0 || mode_music_load_pending == -1 || field_work.unk2298 == 0
        || field_work.encounter_inhibition == -1 || field_music_stream_running == 1 || field_encounters_enabled == 0) {
        return;
    }
    if (--field_work.unk2294 == 0) {
        field_encounter_draw_steps();
    }
    for (i = 0; i < field_work.unk229C; i++) {
        if (field_work.unk22A0[i] != 0xFFFF) {
            field_work.unk22A0[i]--;
        }
    }
    for (i = 0; i < field_work.unk229C; i++) {
        if (field_work.unk22A0[i] == 0) {
            field_work.unk22A0[i] = 0xFFFF;
            goto draw;
        }
    }
    return;
draw:
    weights = formation_encounter_weights;
    total = 0;
    for (i = 0; i < 16; i++) {
        total += weights[i];
    }
    sum = 0;
    for (i = 0; i < 16; i++) {
        start[i] = sum;
        sum += weights[i];
    }
    total = (rand() * (total + 1)) >> 15;
    found = 0;
    for (i = 15; i >= 0; i--) {
        if (weights[i] != 0 && start[i] < total) {
            found++;
            break;
        }
    }
    if (found != 0) {
        formation_selected_index = i;
        mode_battle_standalone = 0;
        field_work.battle_music = field_work.encounter_music[i];
        if (mode_field_standalone == 0) {
            mode_load_overlay_block(2);
        }
        field_battle_not_requested = 0;
        field_encounter_battle_pending = 1;
        if (field_monitor_absent == 0) {
            field_debug_count_encounter(i);
        }
    }
}

/* 8007954C: Leave the field for another game mode, then run the mode dispatcher:
 * kind 0 selects battle (2) after saving the map and event variable 1 in
 * the game state, kind 1 mode 3 (first stopping the field sound and
 * stream work while 8004f384 is 1), kind 2 mode 4, kind 3 the mode in 800b0064's low bits (bit 7 runs
 * 8001bb50 first). Nothing is selected while 8004f370 is set. */
void field_exit_to_mode(s32 kind) {
    mode_battle_return_fade = 0;
    switch (kind) {
    case 0:
        field_event_save_map_and_variables();
        mode_music_selected_track = field_music_saved_for_battle;
        game_current_data->flagWords[0] = field_music_saved_for_battle;
        game_current_data->entry[2] = game_current_data->vars[1];
        if (mode_field_standalone != 0) {
            return;
        }
        mode_select_next_mode(2);
        break;
    case 1:
        if (mode_shared_wave_bank_needs_reload == kind) {
            mode_stop_music();
            field_music_read_shared_wave_bank();
            cd_sync_reads(0);
            field_music_open_shared_wave_bank();
            mode_stop_music();
        }
        if (mode_field_standalone != 0) {
            return;
        }
        mode_select_next_mode(3);
        break;
    case 2:
        game_current_data->flagWords[0] = mode_music_selected_track;
        game_current_data->entry[2] = game_current_data->vars[1];
        if (mode_field_standalone != 0) {
            return;
        }
        mode_select_next_mode(4);
        mode_field_return_pending++;
        break;
    case 3:
        mode_unread_arena_departure_count = 0;
        mode_field_return_pending = 0;
        if (mode_field_standalone != 0) {
            return;
        }
        if (field_exit_game_mode & 0x80) {
            mode_init_game_data();
        }
        mode_select_next_mode(field_exit_game_mode & 0x7F);
        break;
    }
    mode_dispatch(0);
}

/* 800796F4: Empty; nothing calls it. */
void field_layer_redraw_hook(void) {
}

/* 800796FC: Switch to the other draw block and put its display and draw environments. */
void field_draw_flip_and_put_envs(void) {
    field_draw_buffer_index = (field_draw_buffer_index + 1) % 2;
    field_current_draw_block = &field_draw_blocks[field_draw_buffer_index];
    PutDispEnv(&field_current_draw_block->disp);
    PutDrawEnv(&field_current_draw_block->draw);
}

/* 80079784: Present the current buffer under a full-screen tile of brightness
 * `level * 4`: link the tile and its draw mode, copy the display area, then
 * put the environments and draw. */
void field_menu_present_dimmed(s32 level) {
    field_draw_swap_and_clear_ots();
    field_menu_fade_tiles[field_draw_buffer_index].r0 = field_menu_fade_tiles[field_draw_buffer_index].g0 = field_menu_fade_tiles[field_draw_buffer_index].b0 = level * 4;
    addPrim(field_current_draw_block->ot, &field_menu_fade_tiles[field_draw_buffer_index]);
    addPrim(field_current_draw_block->ot, &field_menu_fade_draw_modes[field_draw_buffer_index]);
    field_sync_draw_and_vsync();
    MoveImage(&field_menu_fade_source_rect, 0, field_draw_buffer_index << 8);
    PutDispEnv(&field_current_draw_block->disp);
    PutDrawEnv(&field_current_draw_block->draw);
    DrawOTag(&field_current_draw_block->ot[1]);
}

/* 800798BC: Set mode_gear_riding_lock (80059179), which keeps the party from boarding
 * or leaving its gears: clear only while the controlled actor has neither
 * bit 0x40 nor 0x80 of +14; the event override (800b234c) wins unless 0xff. */
void field_update_gear_riding_lock(void) {
    if (field_work.unk2268 != 0 && !(field_view.components.descriptors[field_work.controlled].actor->unk014 & 0xC0)) {
        mode_gear_riding_lock = 0;
    } else {
        mode_gear_riding_lock = 1;
    }
    if (field_work.gear_riding_lock_override != 0xFF) {
        mode_gear_riding_lock = field_work.gear_riding_lock_override;
    }
}

/* 8007995C: Move a VRAM rectangle (MoveImage) and wait for it. */
void field_vram_move_rect_and_sync(s32 w, s32 h, s32 x, s32 y, s32 to_x, s32 to_y) {
    RECT rect;

    rect.w = w;
    rect.h = h;
    rect.x = x;
    rect.y = y;
    MoveImage(&rect, to_x, to_y);
    DrawSync(0);
}

/* 8007999C: Sync, then flush the instruction cache inside a critical section. */
void field_sync_and_flush_cache(void) {
    field_sync_draw_and_vsync();
    EnterCriticalSection();
    FlushCache();
    ExitCriticalSection();
}

/* 800799D4: Run a menu (kind in 800adb64, 0x80 marks a pending event-only one) over
 * the field: fade out, save the 801e module and the VRAM the menu uses,
 * load the menu (file kind + 5, and the shared file 1), run it (8001c634),
 * apply its results (entering a map from a save), then restore VRAM, fade
 * back in and reload the module and the party sprites. */
void field_run_menu(void) {
    RECT rect;
    FileRequest files[4];
    RECT unused; /* unused in the original; reserves 8 bytes */
    u32 end;
    void *module;
    void *source;
    void *menu;
    void *sprites;
    u_long *saved_a;
    u_long *saved_b;
    s32 i;

    module = NULL;
    if (field_menu_request == 0x80 && field_work.script_control[0] != 0) {
        return;
    }
    setlen(&field_menu_fade_tiles[0], 3);
    setcode(&field_menu_fade_tiles[0], 0x60);
    SetSemiTrans(&field_menu_fade_tiles[0], 1);
    field_menu_fade_tiles[0].w = 0x140;
    field_menu_fade_tiles[0].b0 = 0;
    field_menu_fade_tiles[0].g0 = 0;
    field_menu_fade_tiles[0].r0 = 0;
    field_menu_fade_tiles[0].y0 = 0;
    field_menu_fade_tiles[0].x0 = 0;
    field_menu_fade_tiles[0].h = 0xE0;
    field_menu_fade_tiles[1] = field_menu_fade_tiles[0];
    SetDrawMode(&field_menu_fade_draw_modes[0], 0, 0, GetTPage(0, 2, 0, 0), NULL);
    SetDrawMode(&field_menu_fade_draw_modes[1], 0, 0, GetTPage(0, 2, 0, 0), NULL);
    field_party_free_sprite_blocks();
    if (field_work.unk2264 != 0) {
        cd_select_directory(4, 0);
        module = heap_alloc(cd_get_aligned_file_size(0x6B9), 0);
        source = field_layer_module;
        memcpy(module, source, cd_get_aligned_file_size(0x6B9));
        heap_free(field_layer_module);
    }
    end = field_heap_top;
    cd_select_directory(0x10, 0);
    if (mode_field_standalone == 1) {
        menu = heap_alloc(cd_get_aligned_file_size((field_menu_request + 5) & 0x7F), 1);
    } else {
        menu = heap_alloc((end & 0xFFFFFF) - 0x1C5008, 1);
    }
    files[2].file = 0;
    files[2].destination = NULL;
    files[3].file = 0;
    files[3].destination = NULL;
    files[0].file = 1;
    files[0].destination = menu_state_resource_file = heap_alloc(cd_get_aligned_file_size(1), 1);
    files[1].destination = menu;
    files[1].file = (field_menu_request & 0x7F) + 5;
    if ((field_menu_request & 0x7F) == 5 && mode_field_standalone == 0) {
        files[2].file = 0xC;
        files[2].destination = (void *)0x1DC000;
    }
    cd_sync_reads(0);
    cd_read_file_list(files, 0, 0);
    cd_select_directory(4, 0);
    rect.w = 0x40;
    rect.h = 0x20;
    for (i = 0; i < 6; i++) {
        rect.x = field_menu_vram_blocks[i * 2];
        rect.y = field_menu_vram_blocks[i * 2 + 1];
        MoveImage(&rect, field_menu_vram_save_places[i * 2], field_menu_vram_save_places[i * 2 + 1]);
        DrawSync(0);
    }
    field_vram_move_rect_and_sync(0x40, 0x100, 0x3C0, 0x100, 0x300, 0);
    field_vram_move_rect_and_sync(0x40, 0x100, 0x2C0, 0x100, 0x280, 0);
    field_menu_fade_source_rect.x = 0x2C0;
    field_menu_fade_source_rect.y = 0x100;
    field_menu_fade_source_rect.w = 0x140;
    field_menu_fade_source_rect.h = 0xE0;
    field_screen_save_copy();
    MoveImage(&field_menu_fade_source_rect, 0, 0x100);
    DrawSync(0);
    for (i = 0; i < 0x20; i++) {
        field_menu_present_dimmed(i);
    }
    field_sync_draw_and_vsync();
    field_menu_fade_source_rect.x = 0;
    field_menu_fade_source_rect.y = 0;
    field_draw_flip_and_put_envs();
    console_close();
    MoveImage(&field_menu_fade_source_rect, 0, 0xE0);
    field_sync_draw_and_vsync();
    cd_sync_reads(0);
    mode_result_code = 0;
    menu_state_debug_start = 0;
    menu_state_screen = field_menu_request & 0x7F;
    for (i = 0; i < 3; i++) {
        mode_party_gear_refresh_flags[i] = game_current_data->inGear[i];
    }
    field_update_gear_riding_lock();
    menu_state_big_ots[0] = (u32 *)field_draw_blocks[0].ot;
    menu_state_big_ots[1] = (u32 *)field_draw_blocks[1].ot;
    field_sync_and_flush_cache();
    mode_run_menu();
    field_sync_and_flush_cache();
    model_ot_depth_shift = 2;
    if (mode_result_code == 0 && (field_menu_request & 0x7F) == 2) {
        field_play_record_stopped = 1;
        field_event_write_variable(0x46, 0);
        field_event_write_variable(4, 4);
        mode_field_map_id = 4;
        game_current_data->entry[2] = 0;
        game_current_data->vars[1] = 0;
        game_current_data->map = 4;
    }
    if (mode_result_code == 2) {
        field_play_record_stopped = 1;
        field_event_write_variable(0x46, 2);
        field_event_write_variable(4, game_current_data->map & 0x3FFF);
        if ((game_current_data->map & 0x3FFF) < 0x400) {
            game_current_data->entry[2] = game_current_data->vars[0x2A];
        }
    }
    field_sync_draw_and_vsync();
    field_menu_fade_source_rect.x = 0;
    field_menu_fade_source_rect.y = 0xE0;
    field_menu_fade_source_rect.w = 0x140;
    field_menu_fade_source_rect.h = 0xE0;
    MoveImage(&field_menu_fade_source_rect, 0x140, 0);
    field_sync_draw_and_vsync();
    field_menu_fade_source_rect.x = 0x140;
    field_menu_fade_source_rect.y = 0;
    MoveImage(&field_menu_fade_source_rect, 0, 0);
    MoveImage(&field_menu_fade_source_rect, 0, 0x100);
    field_sync_draw_and_vsync();
    PutDispEnv(&field_current_draw_block->disp);
    PutDrawEnv(&field_current_draw_block->draw);
    field_menu_fade_source_rect.x = 0x2C0;
    field_menu_fade_source_rect.y = 0x100;
    field_menu_present_dimmed(0x1F);
    field_menu_present_dimmed(0x1F);
    rect.w = 0x40;
    rect.h = 0x100;
    rect.x = 0x300;
    rect.y = 0;
    saved_a = heap_alloc(0x8000, 1);
    StoreImage(&rect, saved_a);
    DrawSync(0);
    rect.w = 0x40;
    rect.h = 0x100;
    rect.x = 0x280;
    rect.y = 0;
    saved_b = heap_alloc(0x8000, 1);
    StoreImage(&rect, saved_b);
    DrawSync(0);
    for (i = 0; i < 6; i++) {
        rect.w = 0x40;
        rect.h = 0x20;
        rect.x = field_menu_vram_save_places[i * 2];
        rect.y = field_menu_vram_save_places[i * 2 + 1];
        MoveImage(&rect, field_menu_vram_blocks[i * 2], field_menu_vram_blocks[i * 2 + 1]);
        DrawSync(0);
    }
    cd_select_directory(4, 0);
    heap_select_owner_tag(8, 0);
    field_map_stream_running = 0;
    field_map_stream_start();
    if (field_no_panorama_after_return != 0) {
        for (i = 0x20; i < 0x3F; i++) {
            field_menu_present_dimmed(i);
        }
        field_panorama_hidden = 1;
    } else {
        for (i = 0x1F; i >= 0; i--) {
            field_menu_present_dimmed(i);
        }
        field_menu_present_dimmed(0);
        field_panorama_hidden = 0;
    }
    field_map_stream_stop();
    field_sync_draw_and_vsync();
    rect.w = 0x40;
    rect.h = 0x100;
    rect.x = 0x2C0;
    rect.y = 0x100;
    LoadImage(&rect, saved_b);
    DrawSync(0);
    rect.x = 0x3C0;
    LoadImage(&rect, saved_a);
    DrawSync(0);
    heap_free(saved_b);
    heap_free(saved_a);
    heap_free(menu);
    cd_select_directory(4, 0);
    if (field_work.unk2264 != 0) {
        if (mode_field_standalone == 0) {
            field_layer_module = heap_alloc((end & 0xFFFFFF) - 0x1DC008, 1);
        } else {
            field_layer_module = heap_alloc(cd_get_aligned_file_size(0x6B9), 1);
        }
        source = field_layer_module;
        memcpy(source, module, cd_get_aligned_file_size(0x6B9));
        heap_free(module);
    }
    SetGeomOffset(0xA0, 0x70);
    SetGeomScreen(field_view.projection);
    field_party_alloc_sprite_blocks();
    if (field_menu_request == 1) {
        mode_party_file_ids[2] = 0xFF;
        mode_party_file_ids[1] = 0xFF;
        mode_party_file_ids[0] = 0xFF;
        mode_party_uses_gear_files = 0;
        mode_party_file_kind = 0;
        mode_sync_party_files();
        mode_unpack_party_files();
        field_sync_draw_and_vsync();
        field_map_change_not_requested = 0;
        field_characters_hidden = 1;
    } else {
        for (i = 0; i < 3; i++) {
            if (mode_party_file_ids[i] != 0xFF) {
                sprites = heap_alloc(cd_get_aligned_file_size(mode_party_file_ids[i] + 5), 1);
                cd_read_file(mode_party_file_ids[i] + 5, sprites, 0, 0x80);
                cd_sync_reads(0);
                text_unpack_lzss(sprites, mode_party_sprite_blocks[i]);
                heap_free(sprites);
            }
        }
        field_event_rebuild_party();
        field_sync_draw_and_vsync();
    }
    field_menu_request = 0xFF;
    field_load_text_palette();
    mode_menu_request_count = 0;
}
