/* debug595: the field debug monitor (disc 1 file 595, disc 2 file 590), a
 * development tool linked for the dev kit's 8 MB RAM at 0x80280000. The field
 * overlay's setup (80077e88) reads it as directory 4 file 0xad into 0x80280000
 * when field_monitor_absent and mode_field_standalone are clear; on a retail 2 MB unit the image
 * would alias low RAM, so only development hardware runs it. Its screens
 * print player/scene/camera/event/memory/CPU-time state, edit particle
 * emitters, the encounter timer and fog colours, play sound banks, and draw
 * debug lines.
 *
 * The whole image is this one unit (rodata 80280000-802811EC, text
 * 802811EC-8028596C, data 8028596C-802861C8), built by GCC 2.7.2
 * (debug595.mk); its three jump tables share one phase. */
#include "debug595.h"

/* Monitor statics (all zero in the image). */
s32 field_debug_rgb_calc_red = 0; /* 8028596C: RGB calc red */
s32 field_debug_rgb_calc_green = 0; /* 80285970: green */
s32 field_debug_rgb_calc_blue = 0; /* 80285974: blue */
u32 field_debug_rgb_calc_mode = 0; /* 80285978: RGB calc mode (bits 4..5) */
s32 field_debug_screen_cursor = 0; /* 8028597C: screen cursor */
s32 field_debug_unread_cleared_word = 0; /* 80285980: second counter, reset with the cursor */
s32 field_debug_screen_index = 0; /* 80285984: monitor screen */
s32 field_debug_lines_shown = 0; /* 80285988: debug lines shown (set by the field overlay) */
s32 field_debug_encounter_notice_shown = 0; /* 8028598C: encounter notice shown */
s32 field_debug_encounter_notice_timer = 0; /* 80285990: encounter notice frames */
s32 field_debug_encounter_total = 0; /* 80285994: encounters counted */
s32 field_debug_last_formation = 0; /* 80285998: the last encounter's formation */
s32 field_debug_particle_row = 0; /* 8028599C: particle editor row */
s32 field_debug_particle_column = 0; /* 802859A0: particle editor column */
s16 field_debug_cpu_mark_count = 0; /* 802859A4: CPU-time marks this frame */
/* The CPU-time marks: 32 records of 12 bytes exactly fill 802859a8-80285b28;
 * the code addresses their time (+4) and name (+8). */
CpuMark field_debug_cpu_marks[32] = {0}; /* 802859A8 */
s16 field_debug_formation_counts[16] = {0}; /* 80285B28: encounters per formation */
DebugLine field_debug_lines[16] = {0}; /* 80285B48: the debug lines */

/* 802811EC: Reset the two frame counters. */
void field_debug_reset_screen_cursor(void) {
    field_debug_unread_cleared_word = 0;
    field_debug_screen_cursor = 0;
}

/* 80281204: Count an encounter of `formation` (of the map's set, as the field's
 * field_encounter_count_down draws it) and restart the 60-frame notice. */
void field_debug_count_encounter(s32 formation) {
    field_debug_encounter_notice_shown = 1;
    field_debug_encounter_notice_timer = 60;
    field_debug_last_formation = formation;
    field_debug_formation_counts[formation]++;
    field_debug_encounter_total++;
}

/* 8028125C: Clear the event counters. */
void field_debug_clear_counters(void) {
    s32 i;

    field_debug_cpu_mark_count = 0;
    field_debug_encounter_notice_shown = 0;
    field_debug_encounter_notice_timer = 30;
    field_debug_encounter_total = 0;
    for (i = 15; i >= 0; i--) {
        field_debug_formation_counts[i] = 0;
    }
}

/* 802812A4: Reset the debug lines to grey-to-white segments at the origin; line 1 is red. */
void field_debug_reset_lines(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        setVector(&field_debug_lines[i].rot, 0, 0, 0);
        setVector(&field_debug_lines[i].trans, 0, 0, 0);
        SetLineG2(&field_debug_lines[i].line[0]);
        setRGB0(&field_debug_lines[i].line[0], 0x80, 0x80, 0x80);
        setRGB1(&field_debug_lines[i].line[0], 0xFF, 0xFF, 0xFF);
        field_debug_lines[i].line[1] = field_debug_lines[i].line[0];
    }
    setRGB0(&field_debug_lines[1].line[0], 0x80, 0, 0);
    setRGB1(&field_debug_lines[1].line[0], 0xFF, 0, 0);
    setRGB0(&field_debug_lines[1].line[1], 0x80, 0, 0);
    setRGB1(&field_debug_lines[1].line[1], 0xFF, 0, 0);
}

/* 80281400: Hide the debug lines; when the monitor drew anything, draw its ordering table. */
void field_debug_finish_frame(void) {
    field_debug_lines_shown = 0;
    if (field_debug_run_screen(field_current_draw_block + 0x34) != 0) {
        console_flush(field_current_draw_block + 0x34);
    }
    field_debug_cpu_mark_count = 0;
}

/* 80281450: Draw the first twelve debug lines into the current buffer. */
void field_debug_draw_lines(void) {
    s32 i;

    if (field_monitor_absent == 0 && field_debug_lines_shown != 0) {
        for (i = 0; i < 12; i++) {
            field_debug_draw_line(field_current_draw_block + 0x33, &field_debug_lines[i], &field_debug_lines[i].matrix, field_draw_buffer_index);
        }
    }
}

/* 802814D4: Project a debug line's end points with `m` and link its primitive for `buffer`. */
void field_debug_draw_line(u_long *ot, DebugLine *line, MATRIX *m, s32 buffer) {
    LINE_G2 *prim;
    SVECTOR unused;
    long sxy2;
    long flag;
    long p;

    prim = &line->line[buffer];
    PushMatrix();
    SetRotMatrix(m);
    SetTransMatrix(m);
    RotTransPers3(&line->start, &line->end, &unused, (long *)&prim->x0, (long *)&prim->x1, &sxy2, &p, &flag);
    addPrim(ot + 1, prim);
    PopMatrix();
}

/* 802815B0: Rebuild each debug line's matrix from its rotation and translation in
 * the camera frame. */
void field_debug_update_line_matrices(void) {
    s32 i;
    long flag;

    if (field_monitor_absent == 0) {
        for (i = 0; i < 16; i++) {
            gpu_build_rotation_matrix(&field_debug_lines[i].rot, &field_debug_lines[i].matrix);
            PushMatrix();
            MulMatrix2(&field_view_scaled_world, &field_debug_lines[i].matrix);
            PopMatrix();
            RotTrans(&field_debug_lines[i].trans, (VECTOR *)field_debug_lines[i].matrix.t, &flag);
        }
    }
}

/* 80281678: Place the first twelve debug lines on `actor` and shape eight of them into
 * the outline of its collision box (bottom and top rectangles). */
void field_debug_outline_collision_box(DebugActor *actor) {
    s32 i;

    if (field_monitor_absent == 0) {
        for (i = 0; i < 12; i++) {
            field_debug_lines[i].trans.vx = actor->pos.vx >> 16;
            field_debug_lines[i].trans.vy = actor->pos.vy >> 16;
            field_debug_lines[i].trans.vz = actor->pos.vz >> 16;
            field_debug_lines[i].start.vy = 0;
            field_debug_lines[i].end.vy = 0;
        }
        field_debug_lines[0].start.vx = -actor->size.vx;
        field_debug_lines[0].start.vz = -actor->size.vz;
        field_debug_lines[0].end.vx = actor->size.vx;
        field_debug_lines[0].end.vz = -actor->size.vz;
        field_debug_lines[1].start.vx = actor->size.vx;
        field_debug_lines[1].start.vz = -actor->size.vz;
        field_debug_lines[1].end.vx = actor->size.vx;
        field_debug_lines[1].end.vz = actor->size.vz;
        field_debug_lines[2].start.vx = actor->size.vx;
        field_debug_lines[2].start.vz = actor->size.vz;
        field_debug_lines[2].end.vx = -actor->size.vx;
        field_debug_lines[2].end.vz = actor->size.vz;
        field_debug_lines[3].start.vx = -actor->size.vx;
        field_debug_lines[3].start.vz = actor->size.vz;
        field_debug_lines[3].end.vx = -actor->size.vx;
        field_debug_lines[3].end.vz = -actor->size.vz;
        field_debug_lines[4].start.vx = -actor->size.vx;
        field_debug_lines[4].start.vz = -actor->size.vz;
        field_debug_lines[4].end.vx = actor->size.vx;
        field_debug_lines[4].end.vz = -actor->size.vz;
        field_debug_lines[4].start.vy = -actor->size.vy;
        field_debug_lines[4].end.vy = -actor->size.vy;
        field_debug_lines[5].start.vx = actor->size.vx;
        field_debug_lines[5].start.vz = -actor->size.vz;
        field_debug_lines[5].end.vx = actor->size.vx;
        field_debug_lines[5].end.vz = actor->size.vz;
        field_debug_lines[5].start.vy = -actor->size.vy;
        field_debug_lines[5].end.vy = -actor->size.vy;
        field_debug_lines[6].start.vx = actor->size.vx;
        field_debug_lines[6].start.vz = actor->size.vz;
        field_debug_lines[6].end.vx = -actor->size.vx;
        field_debug_lines[6].end.vz = actor->size.vz;
        field_debug_lines[6].start.vy = -actor->size.vy;
        field_debug_lines[6].end.vy = -actor->size.vy;
        field_debug_lines[7].start.vx = -actor->size.vx;
        field_debug_lines[7].start.vz = actor->size.vz;
        field_debug_lines[7].end.vx = -actor->size.vx;
        field_debug_lines[7].end.vz = -actor->size.vz;
        field_debug_lines[7].start.vy = -actor->size.vy;
        field_debug_lines[7].end.vy = -actor->size.vy;
    }
}

/* 80281994: Print a four-component record. */
void field_debug_print_record(s16 *v) {
    if (field_monitor_absent != 1) {
        console_report_printf("REC %07d %07d %07d %07d\n", v[0], v[1], v[2], v[3]);
    }
}

/* 802819DC: Print a matrix row by row with its translation. */
void field_debug_print_matrix(MATRIX *m) {
    if (field_monitor_absent != 1) {
        console_report_printf("MTX %06d %06d %06d %06d\n", m->m[0][0], m->m[0][1], m->m[0][2], m->t[0]);
        console_report_printf("    %06d %06d %06d %06d\n", m->m[1][0], m->m[1][1], m->m[1][2], m->t[1]);
        console_report_printf("    %06d %06d %06d %06d\n", m->m[2][0], m->m[2][1], m->m[2][2], m->t[2]);
    }
}

/* 80281A78: Print a long vector. */
void field_debug_print_vector(VECTOR *v) {
    if (field_monitor_absent != 1) {
        console_report_printf("VEC  %d %d %d\n", v->vx, v->vy, v->vz);
    }
}

/* 80281ABC: Print a short vector. */
void field_debug_print_svector(SVECTOR *v) {
    if (field_monitor_absent != 1) {
        console_report_printf("SVEC %d %d %d\n", v->vx, v->vy, v->vz);
    }
}

/* 80281B00: Record the scanlines spent since the last mark under `name` for the CPU-time screen. */
void field_debug_mark_cpu_time(char *name) {
    s32 now;

    if (field_monitor_absent == 0) {
        now = VSync(1);
        field_debug_cpu_marks[field_debug_cpu_mark_count].time = now - field_frame_start_time;
        field_debug_cpu_marks[field_debug_cpu_mark_count].name = name;
        field_debug_cpu_mark_count++;
        field_frame_start_time = VSync(1);
    }
}

/* 80281B90: The monitor's frame: report mime models whose instance never loaded, cycle
 * the screen (Select, or Select with the debug button for the extra screens;
 * screens 11-12 also toggle the fog editor) and print it: 1 player and
 * scene, 2 memory, 3 event and party state, 4 event actors, 5 CPU time,
 * 6 event variables, 7 particle editor, 8 items, 9 accessories, 10
 * encounters, 11 fog colours, 12 CPU/GPU summary, 13 RGB calculation.
 * Returns the screen shown. */
s32 field_debug_run_screen(u_long *ot) {
    void *seq;
    FieldActor *actor;
    s32 i, n;
    s32 step;
    s32 a;
    u32 attribute;
    u8 *rate;
    s32 *player;
    s32 length;
    s32 party;

    if (field_monitor_absent == 1) {
        return 0;
    }
    for (i = 0; i < field_view_components.descriptor_count; i++) {
        if (field_view_components.descriptors[i].flags & 0x2000) {
            if (field_view_components.descriptors[i].instance->anims == NULL) {
                console_report_printf("MIME ERROR %d\n", i);
            }
        }
    }
    seq = sound_wave_bank_list;
sequences:
    if (seq != NULL) {
        seq = ((SoundSequence *)seq)->next;
        goto sequences;
    }
    seq = sound_effect_bank_list;
channels:
    if (seq != NULL) {
        seq = ((SoundBank *)seq)->next;
        goto channels;
    }
    if (field_debug_screen_index >= 14) {
        field_debug_screen_index = 0;
    }
    if (field_pad_port1_repeated & 0x800) {
        if (field_monitor_page_toggle & 1) {
            field_debug_screen_index = 7;
        } else {
            field_debug_screen_index = 0;
        }
        field_monitor_page_toggle = (field_monitor_page_toggle + 1) | 0x8000;
    }
    if ((field_pad_port0_repeated & 0x800) && (field_pad_port0_held & 0x40)) {
        field_debug_screen_index++;
        if (field_debug_screen_index == 8 || field_debug_screen_index == 9) {
            field_debug_screen_index = 10;
        }
        if (field_debug_screen_index == 11 || field_debug_screen_index == 12) {
            if (field_debug_screen_index == 11) {
                field_work_sprite_gate = 1;
            } else {
                field_work_sprite_gate = 0;
            }
            field_instance_refresh_bounds_modes();
        }
        field_debug_screen_cursor = 0;
        field_debug_unread_cleared_word = 0;
        if (field_debug_screen_index >= 14) {
            field_debug_screen_index = 0;
        }
    }
    switch (field_debug_screen_index) {
    case 12:
        console_report_printf("\nCPU=%04d GPU=%04d\n", field_frame_cpu_time, field_frame_gpu_time);
        console_report_printf("PolyCount %d / %d\n", model_drawn_primitive_count, model_submitted_primitive_count);
        console_report_printf("Pos X%6d Z=%6d Y=%6d\n", WHOLE(field_view_components.descriptors[field_work_controlled].actor->position[0]),
                      WHOLE(field_view_components.descriptors[field_work_controlled].actor->position[2]),
                      WHOLE(field_view_components.descriptors[field_work_controlled].actor->position[1]));
        goto free_size;
    case 13:
        console_report_printf("RGB CALC\n\n");
        if (field_pad_port0_repeated & 2) {
            if (--field_debug_screen_cursor < 0) {
                field_debug_screen_cursor = 3;
            }
        }
        if (field_pad_port0_repeated & 1) {
            if (++field_debug_screen_cursor >= 4) {
                field_debug_screen_cursor = 0;
            }
        }
        if (mode_field_pointer_state[2] != 8) {
            step = 0;
        } else {
            step = mode_field_pointer_state[4];
        }
        if (field_debug_screen_cursor == 0) {
            field_debug_rgb_calc_mode += step;
            console_report_printf(">MODE %d\n", (field_debug_rgb_calc_mode >> 4) & 3);
        } else {
            console_report_printf(" MODE %d\n", (field_debug_rgb_calc_mode >> 4) & 3);
        }
        if (field_debug_screen_cursor == 1) {
            field_debug_rgb_calc_red = (field_debug_rgb_calc_red + step) & 0xFF;
            console_report_printf(">R %d\n", field_debug_rgb_calc_red);
        } else {
            console_report_printf(" R %d\n", field_debug_rgb_calc_red);
        }
        if (field_debug_screen_cursor == 2) {
            field_debug_rgb_calc_green = (field_debug_rgb_calc_green + step) & 0xFF;
            console_report_printf(">G %d\n", field_debug_rgb_calc_green);
        } else {
            console_report_printf(" G %d\n", field_debug_rgb_calc_green);
        }
        if (field_debug_screen_cursor == 3) {
            field_debug_rgb_calc_blue = (field_debug_rgb_calc_blue + step) & 0xFF;
            console_report_printf(">B %d\n", field_debug_rgb_calc_blue);
        } else {
            console_report_printf(" B %d\n", field_debug_rgb_calc_blue);
        }
        field_fade_start(1, 1, field_debug_rgb_calc_red, field_debug_rgb_calc_green, field_debug_rgb_calc_blue, (field_debug_rgb_calc_mode >> 4) & 3);
        break;
    case 11:
        if (field_pad_port0_repeated & 2) {
            if (--field_debug_screen_cursor < 0) {
                field_debug_screen_cursor = 7;
            }
        }
        if (field_pad_port0_repeated & 1) {
            if (++field_debug_screen_cursor >= 8) {
                field_debug_screen_cursor = 0;
            }
        }
        if (mode_field_pointer_state[2] != 8) {
            step = 0;
        } else {
            step = mode_field_pointer_state[4];
        }
        if (field_debug_screen_cursor == 0) {
            console_report_printf(">NearColor R=%d\n", field_work_fog_color[0] += step);
        } else {
            console_report_printf(" NearColor R=%d\n", field_work_fog_color[0]);
        }
        if (field_debug_screen_cursor == 1) {
            console_report_printf(">          G=%d\n", field_work_fog_color[1] += step);
        } else {
            console_report_printf("           G=%d\n", field_work_fog_color[1]);
        }
        if (field_debug_screen_cursor == 2) {
            console_report_printf(">          B=%d\n", field_work_fog_color[2] += step);
        } else {
            console_report_printf("           B=%d\n", field_work_fog_color[2]);
        }
        if (field_debug_screen_cursor == 3) {
            console_report_printf(">FarColor  R=%d\n", field_work_far_color[0] += step);
        } else {
            console_report_printf(" FarColor  R=%d\n", field_work_far_color[0]);
        }
        if (field_debug_screen_cursor == 4) {
            console_report_printf(">          G=%d\n", field_work_far_color[1] += step);
        } else {
            console_report_printf("           G=%d\n", field_work_far_color[1]);
        }
        if (field_debug_screen_cursor == 5) {
            console_report_printf(">          B=%d\n", field_work_far_color[2] += step);
        } else {
            console_report_printf("           B=%d\n", field_work_far_color[2]);
        }
        if (field_debug_screen_cursor == 6) {
            console_report_printf(">Near        %d\n", field_work_fog_range[0] += step * 10);
        } else {
            console_report_printf(" Near        %d\n", field_work_fog_range[0]);
        }
        if (field_debug_screen_cursor == 7) {
            console_report_printf(">Far         %d\n", field_work_fog_range[1] += step * 10);
        } else {
            console_report_printf(" Far         %d\n", field_work_fog_range[1]);
        }
        break;
    case 1:
        console_report_printf("---------- Player Info -----\n");
        console_report_printf("Pos X%6d Z%6d Y%6d\n", WHOLE(field_view_components.descriptors[field_work_controlled].actor->position[0]),
                      WHOLE(field_view_components.descriptors[field_work_controlled].actor->position[2]),
                      WHOLE(field_view_components.descriptors[field_work_controlled].actor->position[1]));
        actor = field_view_components.descriptors[field_work_controlled].actor;
        console_report_printf("Pol=%d Pri=%d ID=%x:%x\n", actor->triangle[actor->layer], actor->layer,
                      field_view_components.collision_triangles[actor->layer][actor->triangle[actor->layer]].attribute, actor->floor_attribute);
        console_report_printf("P0=%d P1=%d P2=%d C=%d\n", field_view_components.descriptors[field_work_controlled].actor->triangle[0],
                      field_view_components.descriptors[field_work_controlled].actor->triangle[1],
                      field_view_components.descriptors[field_work_controlled].actor->triangle[2], field_player_stuck_frames);
        console_report_printf("MFflag=%x MFlag2=%x N=%d\n", field_view_components.descriptors[field_work_controlled].actor->flags,
                      field_view_components.descriptors[field_work_controlled].actor->layer_flags, field_view_components.descriptors[field_work_controlled].actor->ridden_actor);
        console_report_printf("\n---------- Scene Info ------\n");
        console_report_printf("SCRZ=%d DIP=%d Scale=%d\n", field_view_projection, field_view_elevation, (s16)field_view_distance);
        a = field_camera_get_octant() & 0xFFFF;
        console_report_printf("CamDIR=%d ChrDIR=%d MapNum=%d\n\n", a, field_actor_get_controlled_facing_octant() & 0xFFFF,
                      mode_field_map_id & 0x3FFF);
        console_report_printf("Cam AT   X%6d Z%6d Y%6d\n", field_view_target[0].part.whole, field_view_target[2].part.whole,
                      field_view_target[1].part.whole);
        console_report_printf("Cam EYE  X%6d Z%6d Y%6d\n", field_view[0].part.whole, field_view[2].part.whole,
                      field_view[1].part.whole);
        console_report_printf("Cam AT2  X%6d Z%6d Y%6d\n", field_view_target_goal[0].part.whole, field_view_target_goal[2].part.whole,
                      field_view_target_goal[1].part.whole);
        console_report_printf("Cam EYE2 X%6d Z%6d Y%6d\n", field_view_eye_goal[0].part.whole, field_view_eye_goal[2].part.whole,
                      field_view_eye_goal[1].part.whole);
        console_report_printf("DollySet=%02x DollyStop=%02x\n", field_view_heading_blocks0, field_view_heading_blocks1);
        console_report_printf("Angle=%d\n", field_view_heading_angle);
        length = field_view_projection * (s16)field_view_distance;
        console_report_printf("Length=%d (%d)\n", length >> 12, (length * 2) >> 12);
        console_report_printf("Wave=%02x Music=%02x\n", mode_music_loaded_wave, mode_music_loaded_track);
        console_report_printf("Total Aactor =%d\n", field_event_actor_count);
        console_report_printf("Total Object =%d\n", field_view_components.descriptor_count);
        break;
    case 2:
        console_report_printf("---------- Memory Info -----\n");
        heap_print_report(0, field_debug_screen_cursor, 0xF, 0xDC);
        if (field_pad_port0_repeated & 1) {
            field_debug_screen_cursor += 4;
        }
        if (field_pad_port0_repeated & 2) {
            field_debug_screen_cursor -= 4;
        }
free_size:
        console_report_printf("Free Size=%x\n", heap_get_free_total());
        break;
    case 3:
        i = 0;
        console_report_printf("---------- Event Info ------\n");
        console_report_printf("Event  Time=%d:%d\n", field_event_read_variable(10) >> 8, field_event_read_variable(10) & 0xFF);
        console_report_printf("System Time=%d:%d:%d\n", field_event_read_variable(14), field_event_read_variable(12) >> 8,
                      field_event_read_variable(12) & 0xFF);
        for (; i < 11; i++) {
            console_report_printf("Num=%x HP=%3d MP=%2d\n", i, game_current_data->characters[i].hp, game_current_data->characters[i].ep);
        }
        console_report_printf("Gold=%d\n", game_current_data->gold);
        console_report_printf("SinarioFlag=%d\n", (u16)field_event_variables[0]);
        console_report_printf("Party=%d %d %d\n", mode_party_members[0], mode_party_members[1], mode_party_members[2]);
        n = game_current_data->joined;
        console_report_printf("Member ");
        i = 0;
        while (i < 11) {
            if (n & 1) {
                console_report_printf("%d ", i);
            }
            n >>= 1;
            i++;
        }
        console_report_printf("\n");
        n = game_current_data->available;
        console_report_printf("FrMask ");
        i = 0;
        while (i < 11) {
            if (!(n & 1)) {
                console_report_printf("%d ", i);
            }
            n >>= 1;
            i++;
        }
        console_report_printf("\n");
        n = game_current_data->locked;
        console_report_printf("FrLock ");
        i = 0;
        while (i < 11) {
            if (n & 1) {
                console_report_printf("%d ", i);
            }
            n >>= 1;
            i++;
        }
        console_report_printf("\n");
        console_report_printf("GearRide=%d %d %d\n", game_current_data->inGear[0], game_current_data->inGear[1], game_current_data->inGear[2]);
        console_report_printf("GearNum=");
        for (i = 0; i < 3; i++) {
            party = mode_party_members[i];
            if (party == 0xFF) {
                break;
            }
            console_report_printf(" %d", game_current_data->characters[party].gearId);
        }
        console_report_printf("\nTYPE=");
        for (i = 0; i < 3; i++) {
            if (mode_party_members[i] == 0xFF) {
                break;
            }
            if (mode_party_actors[i] == 0xFF) {
                break;
            }
            switch ((field_view_components.descriptors[mode_party_actors[i]].actor->flags >> 8) & 7) {
            case 1:
                console_report_printf("People ");
                break;
            case 2:
                console_report_printf("Robo ");
                break;
            case 4:
                console_report_printf("Play ");
                break;
            default:
                console_report_printf("?%d ", (field_view_components.descriptors[mode_party_actors[i]].actor->flags >> 8) & 7);
                break;
            }
        }
        console_report_printf(" ID=");
        attribute = field_view_components.descriptors[field_work_controlled].actor->floor_attribute;
        if (!(attribute & 0x80)) {
            console_report_printf("C");
        } else {
            console_report_printf("-");
        }
        if (!(attribute & 0x40)) {
            console_report_printf("G");
        } else {
            console_report_printf("-");
        }
        if (!(attribute & 0x20)) {
            console_report_printf("P");
        } else {
            console_report_printf("-");
        }
        break;
    case 4:
        console_report_printf("---------- Event DEBUG -----\n");
        for (i = field_debug_screen_cursor, n = 0; i < field_event_actor_count; i++, n++) {
            console_report_printf("ActNum=%3d RUN=%04x\n", i,
                          field_view_components.descriptors[i].actor->slots[field_view_components.descriptors[i].actor->slot].resume_pc);
            console_report_printf("P0=%d P1=%d P2=%d P=%d I=%x:%x\n", field_view_components.descriptors[i].actor->triangle[0],
                          field_view_components.descriptors[i].actor->triangle[1], field_view_components.descriptors[i].actor->triangle[2],
                          field_view_components.descriptors[i].actor->layer,
                          field_view_components.collision_triangles[field_view_components.descriptors[i].actor->layer]
                                    [field_view_components.descriptors[i].actor->triangle[field_view_components.descriptors[i].actor->layer]]
                                        .attribute,
                          field_view_components.descriptors[i].actor->floor_attribute);
            console_report_printf("Pos X%6d Z%6d Y%6d\n", WHOLE(field_view_components.descriptors[i].actor->position[0]),
                          WHOLE(field_view_components.descriptors[i].actor->position[2]), WHOLE(field_view_components.descriptors[i].actor->position[1]));
            console_report_printf("M1=%x M2=%x", field_view_components.descriptors[i].actor->flags, field_view_components.descriptors[i].actor->layer_flags);
            if (!(field_view_components.descriptors[i].actor->layer_flags & 0x4000000)) {
                console_report_printf("\n\n");
            } else {
                console_report_printf(" TALK OFF\n\n");
            }
            if (n >= 6) {
                break;
            }
        }
        if (field_pad_port0_repeated & 1) {
            field_debug_screen_cursor++;
        }
        if (field_pad_port0_repeated & 2) {
            field_debug_screen_cursor--;
        }
        break;
    case 5:
        console_report_printf("---------- CPU Time --------\n");
        for (i = 0; i < field_debug_cpu_mark_count; i++) {
            console_report_printf("%s = %6d\n", field_debug_cpu_marks[i].name, field_debug_cpu_marks[i].time);
        }
        console_report_printf("\nCPU=%6d GPU=%6d\n", field_frame_cpu_time, field_frame_gpu_time);
        console_report_printf("PolyCount %d / %d\n", model_drawn_primitive_count, model_submitted_primitive_count);
        break;
    case 6:
        console_report_printf("---------- RAM MAP ---------\n");
        for (i = field_debug_screen_cursor, n = 0; i < 0x400; i++, n++) {
            console_report_printf("ADD %04x:%08x %06d\n", i * 2, field_event_read_variable(i * 2), field_event_read_variable(i * 2));
            if (n >= 16) {
                break;
            }
        }
        if (field_pad_port0_repeated & 1) {
            field_debug_screen_cursor += 4;
        }
        if (field_pad_port0_repeated & 2) {
            field_debug_screen_cursor -= 4;
        }
        break;
    case 7:
        console_report_printf("---------- PARTICLE -----------\n");
        if (field_pad_port1_repeated & 0x100) {
            player = &field_work_controlled;
            field_effect_stop_by_owner(*player, 1);
            for (i = 0; i < 8; i++) {
                if (field_effect_template_actor == 0xFF) {
                    field_effect_templates[i].unk50[1] = *player;
                } else {
                    field_effect_templates[i].unk50[1] = field_effect_template_actor;
                }
            }
            field_effect_start(field_work_controlled);
        }
        field_debug_run_particle_editor();
        break;
    case 8:
        console_report_printf("---------- ITEM -------------\n");
        for (i = field_debug_screen_cursor, n = 0; i < 0x96; i += 4, n++) {
            console_report_printf("%03d=%03d %03d=%03d %03d=%03d %03d=%03d\n", game_current_data->itemIds[i],
                          game_current_data->itemCounts[i], game_current_data->itemIds[i + 1],
                          game_current_data->itemCounts[i + 1], game_current_data->itemIds[i + 2],
                          game_current_data->itemCounts[i + 2], game_current_data->itemIds[i + 3],
                          game_current_data->itemCounts[i + 3]);
            if (n >= 16) {
                break;
            }
        }
        if (field_pad_port0_repeated & 1) {
            field_debug_screen_cursor += 4;
        }
        if (field_pad_port0_repeated & 2) {
            field_debug_screen_cursor -= 4;
        }
        break;
    case 9:
        console_report_printf("---------- ACC --------------\n");
        for (i = field_debug_screen_cursor, n = 0; i < 0xC8; i += 4, n++) {
            console_report_printf("%03d=%03d %03d=%03d %03d=%03d %03d=%03d\n", game_current_data->accessoryIds[i],
                          game_current_data->accessoryCounts[i], game_current_data->accessoryIds[i + 1],
                          game_current_data->accessoryCounts[i + 1], game_current_data->accessoryIds[i + 2],
                          game_current_data->accessoryCounts[i + 2], game_current_data->accessoryIds[i + 3],
                          game_current_data->accessoryCounts[i + 3]);
            if (n >= 16) {
                break;
            }
        }
        if (field_pad_port0_repeated & 1) {
            field_debug_screen_cursor += 4;
        }
        if (field_pad_port0_repeated & 2) {
            field_debug_screen_cursor -= 4;
        }
        break;
    case 10:
        console_report_printf("---------- ENCOUNT -------------\n");
        if (field_pad_port0_repeated & 1) {
            field_debug_screen_cursor++;
        }
        if (field_pad_port0_repeated & 2) {
            field_debug_screen_cursor--;
        }
        rate = formation_encounter_weights;
        for (i = 0; i < 16; i++) {
            console_report_printf("%2d %3d %d\n", i, rate[i], field_debug_formation_counts[i]);
        }
        switch (field_debug_screen_cursor & 3) {
        case 0:
            player = &field_work_encounter_period;
            console_report_printf(">TIME   =%d\n", *player);
            console_report_printf(" ENCOUNT=%d\n", field_work_encounter_step_count);
            console_report_printf(" SET");
            if (field_pad_port0_repeated & 4) {
                (*player)++;
            }
            if (field_pad_port0_repeated & 8) {
                (*player)--;
            }
            break;
        case 1:
            console_report_printf(" TIME   =%d\n", field_work_encounter_period);
            console_report_printf(">ENCOUNT=%d\n", field_work_encounter_step_count);
            console_report_printf(" SET");
            if (field_pad_port0_repeated & 4) {
                field_work_encounter_step_count++;
            }
            if (field_pad_port0_repeated & 8) {
                field_work_encounter_step_count--;
            }
            field_work_encounter_step_count &= 0x1F;
            break;
        case 2:
            console_report_printf(" TIME   =%d\n", field_work_encounter_period);
            console_report_printf(" ENCOUNT=%d\n", field_work_encounter_step_count);
            console_report_printf(">SET");
            field_encounter_draw_steps();
            break;
        }
        if (field_debug_encounter_notice_shown != 0) {
            if (--field_debug_encounter_notice_timer == 0) {
                field_debug_encounter_notice_shown = 0;
            }
            console_report_printf("COUNT=%d NUM=%d\n", field_debug_encounter_total, field_debug_last_formation);
        } else {
            field_debug_encounter_notice_timer = 60;
        }
        field_save_snapshot();
        break;
    }
    if (field_debug_screen_cursor < 0) {
        field_debug_screen_cursor = 0;
    }
    return field_debug_screen_index;
}

/* 802835E0: Particle emitter editor screen: list the edited emitter's parameters
 * (rows 0-21) or its eight angle offsets (rows 22+), with the cursor row and
 * column marked; Up/Down move the row, Left/Right the column, and the
 * editor steps the selected value. */
void field_debug_run_particle_editor(void) {
    s32 selected;
    s32 row;
    s32 cursor;
    s32 column;
    s32 i;

    cursor = field_debug_particle_row;
    column = field_debug_particle_column;
    if (cursor < 22) {
        row = field_debug_start_editor_row(0, cursor, &selected);
        console_report_printf("BANK    = %d\n", field_effect_edited_template);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("MAX     = %d\n", field_effect_templates[field_effect_edited_template].max);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("SWAIT   = %d\n", field_effect_templates[field_effect_edited_template].start_wait);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("EWAIT   = %d\n", field_effect_templates[field_effect_edited_template].end_wait);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("SPOS    =");
        field_debug_print_column_marker(0, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].start_pos.vx);
        field_debug_print_column_marker(1, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].start_pos.vy);
        field_debug_print_column_marker(2, column, selected);
        console_report_printf("%d\n", field_effect_templates[field_effect_edited_template].start_pos.vz);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("EPOS    =");
        field_debug_print_column_marker(0, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].end_pos.vx);
        field_debug_print_column_marker(1, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].end_pos.vy);
        field_debug_print_column_marker(2, column, selected);
        console_report_printf("%d\n", field_effect_templates[field_effect_edited_template].end_pos.vz);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("SPEED   = ");
        field_debug_print_column_marker(0, column, selected);
        console_report_printf("%d * ", field_effect_templates[field_effect_edited_template].speed);
        field_debug_print_column_marker(1, column, selected);
        console_report_printf("%d\n", field_effect_templates[field_effect_edited_template].speed_scale);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("GRAVITE =");
        field_debug_print_column_marker(0, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].gravity.vx);
        field_debug_print_column_marker(1, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].gravity.vy);
        field_debug_print_column_marker(2, column, selected);
        console_report_printf("%d\n", field_effect_templates[field_effect_edited_template].gravity.vz);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("SRANGE  = %d\n", field_effect_templates[field_effect_edited_template].start_range);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("ERANGE  = %d\n", field_effect_templates[field_effect_edited_template].end_range);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("PSWAIT  = %d\n", field_effect_templates[field_effect_edited_template].particle_start_wait);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("PEWAIT  = %d\n", field_effect_templates[field_effect_edited_template].particle_end_wait);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("SHAPE   = %d\n", field_effect_templates[field_effect_edited_template].shape);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("SCALE   =");
        field_debug_print_column_marker(0, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].scale.vx);
        field_debug_print_column_marker(1, column, selected);
        console_report_printf("%d\n", field_effect_templates[field_effect_edited_template].scale.vy);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("SCALEOFS=");
        field_debug_print_column_marker(0, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].scale_offset.vx);
        field_debug_print_column_marker(1, column, selected);
        console_report_printf("%d\n", field_effect_templates[field_effect_edited_template].scale_offset.vy);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("COLOR   =");
        field_debug_print_column_marker(0, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].color[0]);
        field_debug_print_column_marker(1, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].color[1]);
        field_debug_print_column_marker(2, column, selected);
        console_report_printf("%d\n", field_effect_templates[field_effect_edited_template].color[2]);
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("COLOROFS=");
        field_debug_print_column_marker(0, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].color_offset[0]);
        field_debug_print_column_marker(1, column, selected);
        console_report_printf("%d", field_effect_templates[field_effect_edited_template].color_offset[1]);
        field_debug_print_column_marker(2, column, selected);
        console_report_printf("%d\n", field_effect_templates[field_effect_edited_template].color_offset[2]);
        row = field_debug_start_editor_row(row, cursor, &selected);
        if (!field_effect_templates[field_effect_edited_template].flags.bits.randrot) {
            console_report_printf("RANDROT = OFF\n");
        } else {
            console_report_printf("RANDROT = ON\n");
        }
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("SORT    = ");
        switch (field_effect_templates[field_effect_edited_template].flags.bits.sort) {
        case 0:
            console_report_printf("TOP\n");
            break;
        case 1:
            console_report_printf("MID\n");
            break;
        case 2:
            console_report_printf("NORMAL\n");
            break;
        case 3:
            console_report_printf("BACK\n");
            break;
        }
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("COLMODE = ");
        switch (field_effect_templates[field_effect_edited_template].flags.bits.colmode) {
        case 0:
            console_report_printf("1.0*Bk + 1.0*Fw\n");
            break;
        case 1:
            console_report_printf("1.0*Bk - 1.0*Fw\n");
            break;
        case 2:
            console_report_printf("1.0*Bk + 0.25*Fw\n");
            break;
        case 3:
            console_report_printf("0.5*Bk + 0.5*Fw\n");
            break;
        }
        row = field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("ROTANGLE= %d\n", field_effect_templates[field_effect_edited_template].rot_angle);
        field_debug_start_editor_row(row, cursor, &selected);
        console_report_printf("RANGEMOD= ");
        switch (field_effect_templates[field_effect_edited_template].flags.bits.rangemod) {
        case 0:
            console_report_printf("RANDUM (0)");
            break;
        case 2:
            console_report_printf("CIRCLE (1)");
            break;
        case 1:
            console_report_printf("LINE (2)");
            break;
        }
        console_report_printf("ROTANGLE= %d\n", field_effect_templates[field_effect_edited_template].rot_angle);
    } else {
        row = 22;
        for (i = 0; i < 8; i++) {
            row = field_debug_start_editor_row(row, cursor, &selected);
            console_report_printf("ANGOFFS%d=", i);
            field_debug_print_column_marker(0, column, selected);
            console_report_printf("%d", field_effect_templates[field_effect_edited_template].angle_offsets[i][0]);
            field_debug_print_column_marker(1, column, selected);
            console_report_printf("%d\n", field_effect_templates[field_effect_edited_template].angle_offsets[i][1]);
        }
    }
    console_set_color(0xFF, 0xFF, 0xFF);
    if (field_pad_port1_repeated & 0x4000) {
        column = 0;
        if (cursor < 29) {
            cursor++;
        }
    }
    if (field_pad_port1_repeated & 0x1000) {
        column = 0;
        if (cursor > 0) {
            cursor--;
        }
    }
    if (field_pad_port1_repeated & 0x2000) {
        if (cursor == 13 || cursor == 14) {
            if (column < 1) {
                column++;
            }
        } else if (column < 2) {
            column++;
        }
    }
    if ((field_pad_port1_repeated & 0x8000) && column > 0) {
        column--;
    }
    field_debug_edit_emitter_field(column, cursor);
    field_debug_particle_row = cursor;
    field_debug_particle_column = column;
}

/* 80284354: Print the cursor mark for `row` when it is the selected row and `blink` is 1. */
void field_debug_print_column_marker(s32 row, s32 cursor, s32 blink) {
    if (cursor == row && blink == 1) {
        console_report_printf(">");
    } else {
        console_report_printf(" ");
    }
}

/* 8028439C: Start menu row `row`: highlight it and mark it with the cursor when selected;
 * returns the next row. */
s32 field_debug_start_editor_row(s32 row, s32 cursor, s32 *selected) {
    if (cursor == row) {
        console_set_color(0, 0xFF, 0xFF);
        console_report_printf(">");
        *selected = 1;
    } else {
        console_set_color(0x40, 0x40, 0x40);
        console_report_printf(" ");
        *selected = 0;
    }
    return ++row;
}

/* 80284424: Step `value` down (Left) or up (Right) within [min, max]; the shoulder
 * buttons pick steps of 10, 100 or 1000. */
s32 field_debug_step_value(s32 value, s32 min, s32 max) {
    s32 step;

    step = 1;
    if (field_pad_port1_held & 4) {
        step = 10;
    }
    if (field_pad_port1_held & 1) {
        step = 100;
    }
    if (field_pad_port1_held & 2) {
        step = 1000;
    }
    if (field_pad_port1_repeated & 0x80) {
        value -= step;
        if (value < min) {
            value = min;
        }
    }
    if (field_pad_port1_repeated & 0x20) {
        value += step;
        if (max < value) {
            value = max;
        }
    }
    return value;
}

/* 802844BC: Set component `axis` of a short vector. */
void field_debug_set_svector_axis(SVECTOR *v, s32 axis, s32 value) {
    switch (axis) {
    case 0:
        v->vx = value;
        break;
    case 1:
        v->vy = value;
        break;
    case 2:
        v->vz = value;
        break;
    }
}

/* 80284510: Component `axis` of a short vector, 0 for another axis. */
s32 field_debug_get_svector_axis(SVECTOR *v, s32 axis) {
    switch (axis) {
    case 0:
        return v->vx;
    case 1:
        return v->vy;
    case 2:
        return v->vz;
    }
    return 0;
}

/* 8028456C: Set channel `channel` of an unsigned colour triple. */
void field_debug_set_color_channel(u8 *color, s32 channel, s32 value) {
    switch (channel) {
    case 0:
        color[0] = value;
        break;
    case 1:
        color[1] = value;
        break;
    case 2:
        color[2] = value;
        break;
    }
}

/* 802845C0: Channel `channel` of an unsigned colour triple, 0 for another channel. */
s32 field_debug_get_color_channel(u8 *color, s32 channel) {
    switch (channel) {
    case 0:
        return color[0];
    case 1:
        return color[1];
    case 2:
        return color[2];
    }
    return 0;
}

/* 8028461C: Set component `axis` of a signed byte triple. */
void field_debug_set_color_offset_channel(s8 *v, s32 axis, s32 value) {
    switch (axis) {
    case 0:
        v[0] = value;
        break;
    case 1:
        v[1] = value;
        break;
    case 2:
        v[2] = value;
        break;
    }
}

/* 80284670: Component `axis` of a signed byte triple, 0 for another axis. */
s32 field_debug_get_color_offset_channel(s8 *v, s32 axis) {
    switch (axis) {
    case 0:
        return v[0];
    case 1:
        return v[1];
    case 2:
        return v[2];
    }
    return 0;
}



/* 802846CC: Edit field `item` (component `axis`) of the selected particle emitter.
 * Preserve the old flag word across the edit call; the destination emitter
 * is selected again after the call. */
void field_debug_edit_emitter_field(s32 axis, u32 item) {
    s32 flags;
    s32 value;

    switch (item) {
    case 0:
        field_effect_edited_template = field_debug_step_value(field_effect_edited_template, 0, 7);
        break;
    case 1:
        field_effect_templates[field_effect_edited_template].max = field_debug_step_value(field_effect_templates[field_effect_edited_template].max, 0, 0xFF);
        break;
    case 2:
        field_effect_templates[field_effect_edited_template].start_wait =
            field_debug_step_value(field_effect_templates[field_effect_edited_template].start_wait, 0, 0x7FFF);
        break;
    case 3:
        field_effect_templates[field_effect_edited_template].end_wait =
            field_debug_step_value(field_effect_templates[field_effect_edited_template].end_wait, 1, 0x7FFF);
        break;
    case 4:
        field_debug_set_svector_axis(&field_effect_templates[field_effect_edited_template].start_pos, axis,
                      field_debug_step_value(field_debug_get_svector_axis(&field_effect_templates[field_effect_edited_template].start_pos, axis), -0x8000,
                                    0x7FFF));
        break;
    case 5:
        field_debug_set_svector_axis(&field_effect_templates[field_effect_edited_template].end_pos, axis,
                      field_debug_step_value(field_debug_get_svector_axis(&field_effect_templates[field_effect_edited_template].end_pos, axis), -0x8000,
                                    0x7FFF));
        break;
    case 6:
        if (axis == 0) {
            field_effect_templates[field_effect_edited_template].speed =
                field_debug_step_value(field_effect_templates[field_effect_edited_template].speed, -0x8000, 0x7FFF);
        } else {
            field_effect_templates[field_effect_edited_template].speed_scale =
                field_debug_step_value(field_effect_templates[field_effect_edited_template].speed_scale, 1, 0x7FFF);
        }
        break;
    case 7:
        field_debug_set_svector_axis(&field_effect_templates[field_effect_edited_template].gravity, axis,
                      field_debug_step_value(field_debug_get_svector_axis(&field_effect_templates[field_effect_edited_template].gravity, axis), -0x8000,
                                    0x7FFF));
        break;
    case 8:
        field_effect_templates[field_effect_edited_template].start_range =
            field_debug_step_value(field_effect_templates[field_effect_edited_template].start_range, 0, 0xFFFF);
        break;
    case 9:
        field_effect_templates[field_effect_edited_template].end_range =
            field_debug_step_value(field_effect_templates[field_effect_edited_template].end_range, 0, 0xFFFF);
        break;
    case 10:
        field_effect_templates[field_effect_edited_template].particle_start_wait =
            field_debug_step_value(field_effect_templates[field_effect_edited_template].particle_start_wait, 1, 0x7FFF);
        break;
    case 11:
        field_effect_templates[field_effect_edited_template].particle_end_wait =
            field_debug_step_value(field_effect_templates[field_effect_edited_template].particle_end_wait, 1, 0x7FFF);
        break;
    case 12:
        field_effect_templates[field_effect_edited_template].shape = field_debug_step_value(field_effect_templates[field_effect_edited_template].shape, 0, 0x7FFF);
        break;
    case 13:
        field_debug_set_svector_axis(&field_effect_templates[field_effect_edited_template].scale, axis,
                      field_debug_step_value(field_debug_get_svector_axis(&field_effect_templates[field_effect_edited_template].scale, axis), -0x8000,
                                    0x7FFF));
        break;
    case 14:
        field_debug_set_svector_axis(&field_effect_templates[field_effect_edited_template].scale_offset, axis,
                      field_debug_step_value(field_debug_get_svector_axis(&field_effect_templates[field_effect_edited_template].scale_offset, axis),
                                    -0x8000, 0x7FFF));
        break;
    case 15:
        field_debug_set_color_channel(field_effect_templates[field_effect_edited_template].color, axis,
                      field_debug_step_value(field_debug_get_color_channel(field_effect_templates[field_effect_edited_template].color, axis), 0, 0xFF));
        break;
    case 16:
        field_debug_set_color_offset_channel(field_effect_templates[field_effect_edited_template].color_offset, axis,
                      field_debug_step_value(field_debug_get_color_offset_channel(field_effect_templates[field_effect_edited_template].color_offset, axis), -0x80,
                                    0x7F));
        break;
    case 17:
        flags = field_effect_templates[field_effect_edited_template].flags.value;
        value = flags & 1;
        flags &= 0xFFFE;
        flags |= field_debug_step_value(value, 0, 1);
        field_effect_templates[field_effect_edited_template].flags.value = flags;
        break;
    case 18:
        flags = field_effect_templates[field_effect_edited_template].flags.value;
        value = (flags >> 1) & 3;
        flags &= 0xFFF9;
        flags |= field_debug_step_value(value, 0, 3) << 1;
        field_effect_templates[field_effect_edited_template].flags.value = flags;
        break;
    case 19:
        flags = field_effect_templates[field_effect_edited_template].flags.value;
        value = (flags >> 8) & 3;
        flags &= 0xFCFF;
        flags |= field_debug_step_value(value, 0, 3) << 8;
        field_effect_templates[field_effect_edited_template].flags.value = flags;
        break;
    case 20:
        field_effect_templates[field_effect_edited_template].rot_angle =
            field_debug_step_value(field_effect_templates[field_effect_edited_template].rot_angle, 0, 0xFFF);
        break;
    case 21:
        flags = field_effect_templates[field_effect_edited_template].flags.value;
        value = (flags >> 6) & 3;
        flags &= 0xFF3F;
        flags |= field_debug_step_value(value, 0, 2) << 6;
        field_effect_templates[field_effect_edited_template].flags.value = flags;
        break;
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
        if (axis == 0) {
            field_effect_templates[field_effect_edited_template].angle_offsets[item - 22][0] =
                field_debug_step_value(field_effect_templates[field_effect_edited_template].angle_offsets[item - 22][0], -0x8000, 0x7FFF);
        } else {
            field_effect_templates[field_effect_edited_template].angle_offsets[item - 22][1] =
                field_debug_step_value(field_effect_templates[field_effect_edited_template].angle_offsets[item - 22][1], -0x8000, 0x7FFF);
        }
        break;
    }
}

/* 80284EA4: With L2 and the debug button held, move the camera by the pad's analog
 * steps: dolly (mode 4), zoom (mode 8) or rotate and raise (other modes).
 * Accumulate zoom as a signed word and narrow only at its final store. */
void field_debug_move_camera(void) {
    s32 zoom;

    if ((field_pad_port0_held & 1) && (field_pad_port0_held & 0x40)) {
        if (mode_field_pointer_state[2] == 4) {
            field_ground_override_enabled = 1;
            field_ground_override_height += mode_field_pointer_state[4];
        } else if (mode_field_pointer_state[2] == 8) {
            zoom = field_view_elevation;
            zoom += ((u32)mode_field_pointer_state[4] << 4) >> 5;
            field_view_elevation = zoom;
            field_view_target_follow_divisor = 1;
            field_view_eye_follow_divisor = 1;
        } else {
            field_view_target_follow_divisor = 1;
            field_view_eye_follow_divisor = 1;
            field_view_distance += mode_field_pointer_state[4] << 4;
            field_view_heading_high += mode_field_pointer_state[3] << 18;
            field_view_heading_angle = field_view_heading_high >> 16;
        }
    }
}

/* Skip a section's position and offset, read its size and upload its
 * pixels at `rect`, leaving `p` past the pixels. */
#define LOAD_SECTION_IMAGE(rect, p) do {     \
        (p) += 4;                            \
        (rect).w = *(p)++;                   \
        (rect).h = *(p)++;                   \
        LoadImage(&(rect), (u_long *)(p));   \
        (p) += (rect).w * (rect).h;          \
    } while (0)

/* 80284FB4: Load an image archive's sections into VRAM; sections of kind 0x1100 and
 * 0x1101 are placed by `mode0`/`mode1`: 1 at the given origin plus the
 * section offset, 2 also plus the section position, else at the position.
 * Returns 1 for an unknown section kind, 0 after all sections are loaded. */
s32 field_debug_load_image_list(u32 *archive, s16 mode0, s16 x0, s16 y0, s16 mode1, u16 x1, u16 y1) {
    s32 count;
    s32 i;
    u16 *p;
    u32 kind;
    RECT rect;

    count = archive[0];
    p = (u16 *)(archive + (count + 1));
    for (i = 0; i < count; i++) {
        kind = *(u32 *)p;
        p += 2;
        if (kind == 0x1100) {
            switch (mode0) {
            case 1:
                rect.x = x0 + p[2];
                rect.y = y0 + p[3];
                break;
            case 2:
                rect.x = p[2] + (x0 + p[0]);
                rect.y = p[3] + (y0 + p[1]);
                break;
            default:
                rect.x = p[0] + p[2];
                rect.y = p[1] + p[3];
                break;
            }
        } else {
            if (kind != 0x1101) {
                return 1;
            }
            switch (mode1) {
            case 1:
                rect.x = x1 + p[2];
                rect.y = y1 + p[3];
                break;
            case 2:
                rect.x = p[2] + (x1 + p[0]);
                rect.y = p[3] + (y1 + p[1]);
                break;
            default:
                rect.x = p[0] + p[2];
                rect.y = p[1] + p[3];
                break;
            }
        }
        LOAD_SECTION_IMAGE(rect, p);
    }
    return 0;
}

/* 802851B0: Print the name of sound bank `bank` (effects, music and voice banks). */
void field_debug_print_sound_bank_name(s32 bank) {
    switch (bank) {
    case 0:
        console_report_printf("main_se");
        break;
    case 1:
        console_report_printf("bat_se");
        break;
    case 2:
        console_report_printf("gear_se");
        break;
    case 3:
        console_report_printf("ambi");
        break;
    case 4:
        console_report_printf("ambi2");
        break;
    case 5:
        console_report_printf("ambi3");
        break;
    case 6:
        console_report_printf("ambi4");
        break;
    case 32:
        console_report_printf("minato");
        break;
    case 33:
        console_report_printf("lahan");
        break;
    case 34:
        console_report_printf("jyukai");
        break;
    case 35:
        console_report_printf("shitan");
        break;
    case 36:
        console_report_printf("musi");
        break;
    case 37:
        console_report_printf("church");
        break;
    case 38:
        console_report_printf("battle2");
        break;
    case 39:
        console_report_printf("chuchu");
        break;
    case 40:
        console_report_printf("over");
        break;
    case 41:
        console_report_printf("orgel");
        break;
    case 42:
        console_report_printf("battle3");
        break;
    case 43:
        console_report_printf("ajito");
        break;
    case 44:
        console_report_printf("emerada");
        break;
    case 45:
        console_report_printf("ellie");
        break;
    case 46:
        console_report_printf("world");
        break;
    case 47:
        console_report_printf("sad");
        break;
    case 48:
        console_report_printf("ave");
        break;
    case 49:
        console_report_printf("ellie2");
        break;
    case 50:
        console_report_printf("balto");
        break;
    case 51:
        console_report_printf("dajil");
        break;
    case 52:
        console_report_printf("maria1");
        break;
    case 53:
        console_report_printf("maria2");
        break;
    case 54:
        console_report_printf("heshu");
        break;
    case 55:
        console_report_printf("kaisou");
        break;
    case 56:
        console_report_printf("pinch");
        break;
    case 57:
        console_report_printf("porgan");
        break;
    case 58:
        console_report_printf("babel");
        break;
    case 59:
        console_report_printf("solachu");
        break;
    case 60:
        console_report_printf("shinnyu");
        break;
    case 61:
        console_report_printf("inbou");
        break;
    case 62:
        console_report_printf("ido");
        break;
    case 63:
        console_report_printf("takeoff");
        break;
    case 64:
        console_report_printf("glaerf");
        break;
    case 65:
        console_report_printf("last");
        break;
    case 66:
        console_report_printf("shebat");
        break;
    case 67:
        console_report_printf("dungeon");
        break;
    case 68:
        console_report_printf("lastbat");
        break;
    case 69:
        console_report_printf("solaris");
        break;
    case 181:
        console_report_printf("vomaria");
        break;
    case 182:
        console_report_printf("melmv");
        break;
    case 183:
        console_report_printf("yugumv");
        break;
    case 184:
        console_report_printf("zoharumv");
        break;
    case 185:
        console_report_printf("vomagic5");
        break;
    case 186:
        console_report_printf("vomagic4");
        break;
    case 187:
        console_report_printf("vomagic3");
        break;
    case 188:
        console_report_printf("voivent3");
        break;
    case 189:
        console_report_printf("voivent2");
        break;
    case 190:
        console_report_printf("vobossm");
        break;
    case 191:
        console_report_printf("vobossl");
        break;
    case 192:
        console_report_printf("vochu6");
        break;
    case 193:
        console_report_printf("vomagic2");
        break;
    case 194:
        console_report_printf("vomagic1");
        break;
    case 7:
        console_report_printf("movie14");
        break;
    case 195:
        console_report_printf("movie15");
        break;
    case 196:
        console_report_printf("movie16");
        break;
    case 197:
        console_report_printf("movie18");
        break;
    case 198:
        console_report_printf("voivent");
        break;
    case 199:
        console_report_printf("damage");
        break;
    case 200:
        console_report_printf("vofei");
        break;
    case 201:
        console_report_printf("vofei1");
        break;
    case 202:
        console_report_printf("vofei2");
        break;
    case 203:
        console_report_printf("vofei3");
        break;
    case 204:
        console_report_printf("vofei4");
        break;
    case 205:
        console_report_printf("vofei5");
        break;
    case 206:
        console_report_printf("vofei6");
        break;
    case 207:
        console_report_printf("voellie");
        break;
    case 208:
        console_report_printf("voellie1");
        break;
    case 209:
        console_report_printf("voellie2");
        break;
    case 210:
        console_report_printf("voellie3");
        break;
    case 211:
        console_report_printf("voellie4");
        break;
    case 212:
        console_report_printf("voellie5");
        break;
    case 213:
        console_report_printf("voellie6");
        break;
    case 214:
        console_report_printf("voellie7");
        break;
    case 215:
        console_report_printf("voellie8");
        break;
    case 216:
        console_report_printf("voshita");
        break;
    case 217:
        console_report_printf("voshita1");
        break;
    case 218:
        console_report_printf("voshita2");
        break;
    case 219:
        console_report_printf("voshita3");
        break;
    case 220:
        console_report_printf("voshita4");
        break;
    case 221:
        console_report_printf("voshita5");
        break;
    case 222:
        console_report_printf("voshita6");
        break;
    case 223:
        console_report_printf("vobaluto");
        break;
    case 224:
        console_report_printf("vobalu1");
        break;
    case 225:
        console_report_printf("vobalu2");
        break;
    case 226:
        console_report_printf("vobalu3");
        break;
    case 227:
        console_report_printf("vobalu4");
        break;
    case 228:
        console_report_printf("vobalu5");
        break;
    case 229:
        console_report_printf("vobalu6");
        break;
    case 230:
        console_report_printf("vobalu7");
        break;
    case 231:
        console_report_printf("vorico");
        break;
    case 232:
        console_report_printf("vorico1");
        break;
    case 233:
        console_report_printf("vorico2");
        break;
    case 234:
        console_report_printf("vorico3");
        break;
    case 235:
        console_report_printf("vorico4");
        break;
    case 236:
        console_report_printf("vorico5");
        break;
    case 237:
        console_report_printf("vobilly");
        break;
    case 238:
        console_report_printf("vobilly1");
        break;
    case 239:
        console_report_printf("vobilly2");
        break;
    case 240:
        console_report_printf("vobilly3");
        break;
    case 241:
        console_report_printf("vobilly4");
        break;
    case 242:
        console_report_printf("vobilly5");
        break;
    case 243:
        console_report_printf("voeme");
        break;
    case 244:
        console_report_printf("voeme1");
        break;
    case 245:
        console_report_printf("voeme2");
        break;
    case 246:
        console_report_printf("voeme3");
        break;
    case 247:
        console_report_printf("voeme4");
        break;
    case 248:
        console_report_printf("voeme5");
        break;
    case 249:
        console_report_printf("vochu");
        break;
    case 250:
        console_report_printf("vochu1");
        break;
    case 251:
        console_report_printf("vochu2");
        break;
    case 252:
        console_report_printf("vochu3");
        break;
    case 253:
        console_report_printf("vochu4");
        break;
    case 254:
        console_report_printf("vochu5");
        break;
    }
}
