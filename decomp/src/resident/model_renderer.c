/* Models (8002c3e8-80031894), GCC 2.7.2 at -G0: 8002c700 matches only so
 * (mflo into $t0), as do 8002ccc8, 8002cd24 and 8002cd64, and 8002c3e8,
 * 8002c4bc and 8002c59c add the relocation base second as only 2.7.2 does.
 * Model and sprite model relocation, packet building and drawing through
 * the primitive type table with the handwritten renderers (model_draw_gt3_avg.s
 * to model_set_envmap_mapping.s, sharing model_draw.s and model_depth.s), the texture
 * page and CLUT overrides, image list uploads, morphing, lights, the bounding
 * box test and the handwritten ordering table link helpers (gpu_ot_link_poly_f3.s
 * to gpu_ot_link_tile_1.s, sharing ot_link.s). It starts at the
 * jump table phase change after cd_reads_and_streams's tables (0x80018980, 0 mod 8)
 * and ends at the heap unit, whose allocation-name strings open its rodata
 * (0x80018998) and which addresses its small globals through $gp. */
#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/inline_c.h"
#include "resident/heap.h"
#include "resident/model.h"
#include "own_declarations.h"

/* This unit's own variables: those of up to 8 bytes in its .sbss
 * (80059308), the larger light matrices in its .bss (80059f64), as the
 * original assembler placed them (SBSS_model_renderer in slus_006.64.mk). */
static u16 model_effective_tpage; /* 80059308: the primitive's texture page, overridden */
static u16 model_effective_clut; /* 8005930C: the primitive's CLUT, overridden */
static s32 model_tpage_override; /* 80059310: the texture page override */
static s32 model_clut_override; /* 80059314: the CLUT override */
static MATRIX model_light_directions; /* 80059F64: light directions, one per row */
static MATRIX model_light_colors; /* 80059F84: light colors, one per column */

/* The handwritten renderers with their alternate entries (model_draw.s) and
 * the C routines that prepare one record's packets. */
void model_draw_gt3_avg(), model_draw_g3_avg(), model_draw_f3_avg(), model_draw_ft3_avg(),
    model_draw_gt4_avg(), model_draw_g4_avg(), model_draw_f4_avg(), model_draw_ft4_avg(), model_draw_gt3_far(),
    model_draw_g3_far(), model_draw_f3_far(), model_draw_ft3_far(), model_draw_gt4_far(), model_draw_g4_far(),
    model_draw_f4_far(), model_draw_ft4_far(), model_draw_gt3_near(), model_draw_g3_near(), model_draw_f3_near(),
    model_draw_ft3_near(), model_draw_gt4_near(), model_draw_g4_near(), model_draw_f4_near(), model_draw_ft4_near(),
    model_draw_f3_lit(), model_draw_ft3_cued(), model_draw_ft3_cued_far(), model_draw_ft3_lit(), model_draw_gt3_lit(),
    model_draw_g3_lit(), model_draw_f4_lit(), model_draw_ft4_lit(), model_draw_ft4_cued(), model_draw_ft4_cued_far(),
    model_draw_ft3_envmap();
s32 model_prepare_f3_lit(CVECTOR *color, s16 *vertices, s32 flags);
s32 model_prepare_f3_plain(s32 *value);
s32 model_prepare_f4_lit(CVECTOR *color, s16 *vertices, s32 flags);
s32 model_prepare_f4_plain(s32 *value);
s32 model_prepare_ft4_plain(u16 *command);
s32 model_prepare_g4_lit(CVECTOR *color, s16 *vertices);
s32 model_prepare_gt4_lit(u16 *command, s16 *vertices);
s32 model_prepare_ft4_lit(u16 *command, s16 *vertices, s32 flags);
s32 model_prepare_g3_lit(CVECTOR *color, s16 *vertices, s32 flags);
s32 model_prepare_g3_lit_uncached(CVECTOR *color, s16 *vertices);
s32 model_prepare_ft3_lit(u16 *command, s16 *vertices, s32 flags);
s32 model_prepare_ft3_plain(u16 *command);
s32 model_prepare_gt3_lit(u16 *command, s16 *vertices);
s32 model_prepare_ft3_envmap(void);

/* The renderer of each primitive type, indexed by PrimitiveGroup.type
 * without a range check (model_draw_sprite_model, model_build_packets). Each row names the
 * packet its prepare routine builds; the types with a texture (odd ones but
 * 16) take texture page and CLUT override words first (8002cd64).
 * tools/analysis/dispatch_tables.py counts the types the game's models use. */
PrimitiveType model_primitive_types[17] = { /* 8004FE50 */
    /* 0: POLY_F3, coloured by the face normal as the build kind says */
    {{model_draw_f3_avg, model_draw_f3_lit, model_draw_f3_far, model_draw_f3_near, model_draw_f3_avg, model_draw_f3_avg},
     model_prepare_f3_lit, 8, 4, 0x14},
    /* 1: POLY_FT3, shaded by the face normal as the build kind says */
    {{model_draw_ft3_avg, model_draw_ft3_lit, model_draw_ft3_far, model_draw_ft3_near, model_draw_ft3_cued, model_draw_ft3_cued_far},
     model_prepare_ft3_lit, 8, 8, 0x20},
    /* 2: POLY_G3, lit by the vertex normals */
    {{model_draw_g3_avg, model_draw_g3_lit, model_draw_g3_far, model_draw_g3_near, model_draw_g3_avg, model_draw_g3_avg},
     model_prepare_g3_lit, 8, 4, 0x1C},
    /* 3: POLY_GT3, lit by the vertex normals */
    {{model_draw_gt3_avg, model_draw_gt3_lit, model_draw_gt3_far, model_draw_gt3_near, model_draw_gt3_avg, model_draw_gt3_avg},
     model_prepare_gt3_lit, 8, 8, 0x28},
    /* 4: POLY_F3, its colour as given */
    {{model_draw_f3_avg, model_draw_f3_avg, model_draw_f3_far, model_draw_f3_near, model_draw_f3_avg, model_draw_f3_avg},
     model_prepare_f3_plain, 8, 4, 0x14},
    /* 5: POLY_FT3, unlit */
    {{model_draw_ft3_avg, model_draw_ft3_avg, model_draw_ft3_far, model_draw_ft3_near, model_draw_ft3_cued, model_draw_ft3_cued_far},
     model_prepare_ft3_plain, 8, 8, 0x20},
    /* 6: POLY_G3, lit by the vertex normals without the cache */
    {{model_draw_g3_avg, model_draw_g3_avg, model_draw_g3_far, model_draw_g3_near, model_draw_g3_avg, model_draw_g3_avg},
     model_prepare_g3_lit_uncached, 8, 4, 0x1C},
    /* 7: POLY_GT3, lit by the vertex normals */
    {{model_draw_gt3_avg, model_draw_gt3_avg, model_draw_gt3_far, model_draw_gt3_near, model_draw_gt3_avg, model_draw_gt3_avg},
     model_prepare_gt3_lit, 8, 8, 0x28},
    /* 8: flat quads (0x18-byte packets), coloured as type 0 */
    {{model_draw_f4_avg, model_draw_f4_lit, model_draw_f4_far, model_draw_f4_near, model_draw_f4_avg, model_draw_f4_avg},
     model_prepare_f4_lit, 8, 4, 0x18},
    /* 9: POLY_FT4, shaded by the face normal as the build kind says */
    {{model_draw_ft4_avg, model_draw_ft4_lit, model_draw_ft4_far, model_draw_ft4_near, model_draw_ft4_cued, model_draw_ft4_cued_far},
     model_prepare_ft4_lit, 8, 12, 0x28},
    /* 10: POLY_G4, lit by the vertex normals */
    {{model_draw_g4_avg, model_draw_g4_avg, model_draw_g4_far, model_draw_g4_near, model_draw_g4_avg, model_draw_g4_avg},
     model_prepare_g4_lit, 8, 4, 0x24},
    /* 11: POLY_GT4, lit by the vertex normals */
    {{model_draw_gt4_avg, model_draw_gt4_avg, model_draw_gt4_far, model_draw_gt4_near, model_draw_gt4_avg, model_draw_gt4_avg},
     model_prepare_gt4_lit, 8, 12, 0x34},
    /* 12: POLY_F4, its colour as given */
    {{model_draw_f4_avg, model_draw_f4_avg, model_draw_f4_far, model_draw_f4_near, model_draw_f4_avg, model_draw_f4_avg},
     model_prepare_f4_plain, 8, 4, 0x18},
    /* 13: POLY_FT4, its colour as given */
    {{model_draw_ft4_avg, model_draw_ft4_avg, model_draw_ft4_far, model_draw_ft4_near, model_draw_ft4_cued, model_draw_ft4_cued_far},
     model_prepare_ft4_plain, 8, 12, 0x28},
    /* 14: POLY_G4, as type 10 */
    {{model_draw_g4_avg, model_draw_g4_avg, model_draw_g4_far, model_draw_g4_near, model_draw_g4_avg, model_draw_g4_avg},
     model_prepare_g4_lit, 8, 4, 0x24},
    /* 15: POLY_GT4, as type 11 */
    {{model_draw_gt4_avg, model_draw_gt4_avg, model_draw_gt4_far, model_draw_gt4_near, model_draw_gt4_avg, model_draw_gt4_avg},
     model_prepare_gt4_lit, 8, 12, 0x34},
    /* 16: POLY_FT3 on the override texture page and CLUT */
    {{model_draw_ft3_envmap, model_draw_ft3_envmap, model_draw_ft3_envmap, model_draw_ft3_envmap, model_draw_ft3_envmap, model_draw_ft3_envmap},
     model_prepare_ft3_envmap, 8, 4, 0x20},
};
s32 model_screen_x_limit = 0x13F;    /* 800500F8: right screen edge of the renderers (8002dff0) */
s32 model_screen_y_limit = 0xEE0000; /* 800500FC: bottom screen edge - 1, in the high half */
s32 model_ot_depth_shift = 2;        /* 80050100: depth shift into the ordering table */
s32 model_box_test_mode = 1;        /* 80050104: bounding box test mode (8003101c), 0 off */
s32 model_tpage_override_mode = 0;        /* 80050108: texture page override: 0 none, 1 page, 2 raw */
s32 model_clut_override_disabled = 1;        /* 8005010C: CLUT override: 0 on */

/* 8002C3E8: Relocate a model group's offsets to addresses (once). Returns the number
 * of models. */
s32 model_relocate_group(ModelGroup *group) {
    s32 i = group->flags; /* reused as the model index after the flag update */
    s32 count = group->count;
    Model *model;
    ModelList *list;
    ModelListEntry *entries;
    s32 n;

    if (!(i & 1)) {
        group->flags = i | 1;
        for (i = 0, model = group->models; i < count; i++, model++) {
            model->table0 += (s32)group;
            model->table4 += (s32)group;
            model->table8 += (s32)group;
            model->primitives += (s32)group;
            if (model->list != NULL) {
                list = (ModelList *)((u8 *)model->list + (s32)group);
                model->list = list;
                entries = list->entries;
                n = list->last;
                if (n != -1) {
                    for (; n != -1; n--) {
                        entries[n].first += (s32)group;
                        entries[n].second += (s32)group;
                    }
                }
            }
        }
    }
    return count;
}

/* 8002C4BC: Undo 8002C3E8: turn a relocated model group's addresses back into
 * offsets. Returns the number of models. */
s32 model_unrelocate_group(ModelGroup *group) {
    s32 i = group->flags; /* reused as the model index after the flag update */
    s32 count = group->count;
    Model *model;
    ModelList *list;
    ModelListEntry *entries;
    s32 n;

    if (i & 1) {
        group->flags = i & ~1;
        for (i = 0, model = group->models; i < count; i++, model++) {
            model->table0 -= (s32)group;
            model->table4 -= (s32)group;
            model->table8 -= (s32)group;
            model->primitives -= (s32)group;
            if (model->list != NULL) {
                list = model->list;
                entries = list->entries;
                n = list->last;
                if (n != -1) {
                    for (; n != -1; n--) {
                        entries[n].first -= (s32)group;
                        entries[n].second -= (s32)group;
                    }
                }
                model->list = (ModelList *)((u8 *)model->list - (s32)group);
            }
        }
    }
    return count;
}

/* 8002C59C: Relocate a sprite model's offsets to addresses (once). */
void model_relocate_sprite_model(SpriteModel *model) {
    MorphTable *table;
    MorphTarget *targets;
    s32 n;

    if (!(model->flags & 0x20)) {
        model->flags |= 0x20;
        model->vertices = (SVECTOR *)((u8 *)model->vertices + (s32)model);
        model->normals = (SVECTOR *)((u8 *)model->normals + (s32)model);
        model->unk10 += (s32)model;
        model->unk14 += (s32)model;
        if (model->morphs != NULL) {
            table = (MorphTable *)((u8 *)model->morphs + (s32)model);
            targets = table->targets;
            model->morphs = table;
            n = table->count;
            if (n != -1) {
                for (; n != -1; n--) {
                    targets[n].deltas = (u8 *)targets[n].deltas + (s32)model;
                    targets[n].normals = (MorphDelta *)((u8 *)targets[n].normals + (s32)model);
                }
            }
        }
    }
}

/* 8002C644: Trim a model group's heap block to its data (once). Returns 1 when it
 * was already trimmed. */
s32 model_trim_group(ModelGroup *group) {
    if (group->flags & 2) {
        return 1;
    }
    group->flags |= 2;
    heap_shrink_block((u8 *)group, group->models[0].primitives - (u8 *)group);
    return 0;
}

/* 8002C68C: Trim a model buffer's heap block at its end (once). Returns 1 when it
 * was already trimmed. */
s32 model_trim_buffer(ModelBuffer *buffer) {
    if (buffer->flags & 0x40) {
        return 1;
    }
    buffer->flags |= 0x40;
    heap_shrink_block((u8 *)buffer, buffer->end - (u8 *)buffer);
    buffer->end = NULL;
    return 0;
}

/* The model colour; the handwritten renderers load it into the GTE. The
 * menu reads it as one word, so the shared headers leave it out. */
extern CVECTOR model_color;

/* 8002C6E0: Set the model colour. */
void model_set_color(u8 r, u8 g, u8 b) {
    model_color.r = r;
    model_color.g = g;
    model_color.b = b;
}

/* 8002C700: Draw a sprite model's primitive groups into `ot` with the routines of
 * sort mode `mode`, building packets from `packets`. Returns 0 when its
 * bounding box test finds it off screen, else 1. */
s32 model_draw_sprite_model(SpriteModel *model, RenderPacket *packets, u32 *ot, s32 mode) {
    s32 count;
    PrimitiveGroup *group;
    PrimitiveType *type;
    void (*draw)(u8 *records, s32 count);

    if (model_box_test_mode != 0 && model_is_box_off_screen(model, model_box_test_mode)) {
        return 0;
    }
    count = model->group_count;
    model_current_packet = packets;
    model_ot = ot;
    model_current_primitive_group = (PrimitiveGroup *)model->unk10;
    model_lit_color_cache = (s32 *)model->unk18;
    model_current_normals = model->normals;
    model_current_vertices = model->vertices;
    model_submitted_primitive_count += model->primitive_count;
    for (count--; count != -1; count--) {
        group = model_current_primitive_group;
        type = &model_primitive_types[group->type];
        switch (mode) {
        case 0:
            draw = type->draw[0];
            break;
        case 1:
            draw = type->draw[1];
            break;
        case 2:
            draw = type->draw[2];
            break;
        case 3:
            draw = type->draw[3];
            break;
        case 4:
            draw = type->draw[4];
            break;
        case 5:
            draw = type->draw[5];
            break;
        }
        model_current_primitive_group++;
        draw((u8 *)model_current_primitive_group, group->count);
        model_current_primitive_group = (PrimitiveGroup *)((u8 *)model_current_primitive_group + group->count * type->stride);
    }
    return 1;
}

/* 8002C8CC: Build a sprite model's packets in `packets`. A mode other than 0 keeps
 * an auxiliary block and selects the variant the preparing routines
 * build: 1 plain, 2 and 3 the lit variants once the auxiliary block
 * exists (3 when the model was not yet built with them, 4 after). */
void model_build_packets(SpriteModel *model, RenderPacket *packets, s32 mode) {
    s32 groups;
    s32 count;
    PrimitiveGroup *group;
    PrimitiveType *type;
    s32 (*prepare)(u8 *aux, u8 *record, s16 kind);
    s32 kind;
    u16 flags;

    model_current_packet = packets;
    if (!(model->flags & 1) && model->aux_size != 0 && mode != 0) {
        heap_set_next_class(0x26);
        model->unk18 = heap_alloc(model->aux_size, 0);
        model->flags |= 1;
    }
    model_current_primitive_group = (PrimitiveGroup *)model->unk10;
    model_current_aux_data = model->unk14;
    model_current_normals = model->normals;
    model_current_vertices = model->vertices;
    model_lit_color_cache = (s32 *)model->unk18;
    flags = model->flags;
    switch (mode) {
    case 0:
        kind = 0;
        break;
    case 1:
        kind = 1;
        break;
    case 2:
        if (flags & 2) {
            if (flags & 1) {
                kind = 4;
            } else {
                kind = 1;
            }
        } else if (flags & 1) {
            kind = 3;
            model->flags = flags | 2;
        } else {
            kind = 1;
        }
        break;
    case 3:
        if (flags & 1) {
            kind = 3;
            model->flags = flags | 2;
        }
        break;
    }
    groups = model->group_count;
    model_submitted_primitive_count += model->primitive_count;
    for (groups--; groups != -1; groups--) {
        group = model_current_primitive_group;
        count = group->count;
        model_current_primitive_group = group + 1;
        type = &model_primitive_types[group->type];
        prepare = type->prepare;
        for (count--; count != -1; count--) {
            if (prepare(model_current_aux_data, (u8 *)model_current_primitive_group, kind)) {
                model_current_primitive_group = (PrimitiveGroup *)((u8 *)model_current_primitive_group + type->stride);
                model_current_packet = (RenderPacket *)((u8 *)model_current_packet + type->packet_size);
                model_current_aux_data += type->aux_stride;
            } else {
                model_current_aux_data += 4;
                count++;
            }
        }
    }
    model_clear_overrides();
}

/* 8002CB54: Allocate a model buffer's two halves of `size` bytes each. */
void model_alloc_packet_buffers(ModelBuffer *buffer, u8 **first, u8 **second) {
    u8 *block;

    heap_set_next_class(0x25);
    block = heap_alloc(buffer->size * 2, 0);
    *first = block;
    *second = block + buffer->size;
}

/* 8002CBBC: Release a model buffer's owned block. */
void model_free_owned_block(ModelBuffer *buffer) {
    if (buffer->flags & 1) {
        heap_free(buffer->buffer);
        buffer->flags &= ~1;
    }
}

/* 8002CC10: Override model texture pages with the page at (x, y). */
void model_set_tpage_override(u16 x, u16 y) {
    model_tpage_override = GetTPage(0, 0, x, y) & 0x1F;
    model_tpage_override_mode = 1;
}

/* 8002CC54: Override model texture pages with the page value `tpage` itself. */
void model_set_raw_tpage_override(u16 tpage) {
    model_tpage_override = tpage;
    model_tpage_override_mode = 2;
}

/* 8002CC74: Override model CLUTs with the CLUT at (x, y). */
void model_set_clut_override(u16 x, u16 y) {
    model_clut_override = GetClut(x, y) & 0xFFF0;
    model_clut_override_disabled = 0;
}

/* 8002CCAC: End both overrides. */
void model_clear_overrides(void) {
    model_tpage_override_mode = 0;
    model_clut_override_disabled = 1;
}

/* 8002CCC8: Apply the texture page override to a primitive's page. */
void model_apply_tpage_override(u16 *tpage) {
    u16 value = *tpage;

    model_effective_tpage = value;
    if (model_tpage_override_mode == 1) {
        model_effective_tpage = value & 0xFFE0;
        model_effective_tpage = (value & 0xFFE0) | model_tpage_override;
    } else if (model_tpage_override_mode == 2) {
        model_effective_tpage = model_tpage_override;
    }
}

/* 8002CD24: Apply the CLUT override to a primitive's CLUT. */
void model_apply_clut_override(u16 *clut) {
    u16 value = *clut;

    model_effective_clut = value;
    if (model_clut_override_disabled == 0) {
        model_effective_clut = value & 0xF;
        model_effective_clut = (value & 0xF) | model_clut_override;
    }
}

/* 8002CD64: Handle a texture page (0xC4) or CLUT (0xC8) command. Returns 1 for any
 * other command. */
s32 model_apply_override_command(u8 *command) {
    if ((command[3] & 0xF0) != 0xC0) {
        return 1;
    }
    switch (command[3]) {
    case 0xC4:
        model_apply_tpage_override((u16 *)command);
        return 0;
    case 0xC8:
        model_apply_clut_override((u16 *)command);
        return 0;
    }
    return 1;
}

/* 8002CDCC: Build a flat triangle's color: lit by the face normal of `vertices`
 * (flag 1; with flag 2 the color and normal are also recorded in the
 * lit-color cache), lit from the cache (flag 4), or copied. */
s32 model_prepare_f3_lit(CVECTOR *color, s16 *vertices, s32 flags) {
    POLY_F3 *poly = (POLY_F3 *)model_current_packet;
    SVECTOR normal;

    setlen(poly, 4);
    if (flags & 1) {
        if (flags & 2) {
            *model_lit_color_cache = *(s32 *)color;
            model_compute_face_normal(&model_current_vertices[vertices[0]], &model_current_vertices[vertices[1]],
                          &model_current_vertices[vertices[2]], (SVECTOR *)++model_lit_color_cache);
            NormalColorCol((SVECTOR *)model_lit_color_cache, color, (CVECTOR *)&poly->r0);
            model_lit_color_cache += 2;
        } else {
            model_compute_face_normal(&model_current_vertices[vertices[0]], &model_current_vertices[vertices[1]],
                          &model_current_vertices[vertices[2]], &normal);
            NormalColorCol(&normal, color, (CVECTOR *)&poly->r0);
        }
        poly->code = color->cd;
    } else if (flags & 4) {
        model_lit_color_cache++;
        NormalColorCol((SVECTOR *)model_lit_color_cache, color, (CVECTOR *)&poly->r0);
        model_lit_color_cache += 2;
        poly->code = color->cd;
    } else {
        *(s32 *)&poly->r0 = *(s32 *)color;
    }
    return 1;
}

/* 8002CF34: Type 4's record: packet length 4 and the auxiliary colour word (its command
 * byte included) as given. */
s32 model_prepare_f3_plain(s32 *value) {
    RenderPacket *packet = model_current_packet;

    packet->code = 4;
    packet->value = *value;
    return 1;
}

/* 8002CF58: The same flat triangle color handler for a second primitive command. */
s32 model_prepare_f4_lit(CVECTOR *color, s16 *vertices, s32 flags) {
    POLY_F3 *poly = (POLY_F3 *)model_current_packet;
    SVECTOR normal;

    setlen(poly, 4);
    if (flags & 1) {
        if (flags & 2) {
            *model_lit_color_cache = *(s32 *)color;
            model_compute_face_normal(&model_current_vertices[vertices[0]], &model_current_vertices[vertices[1]],
                          &model_current_vertices[vertices[2]], (SVECTOR *)++model_lit_color_cache);
            NormalColorCol((SVECTOR *)model_lit_color_cache, color, (CVECTOR *)&poly->r0);
            model_lit_color_cache += 2;
        } else {
            model_compute_face_normal(&model_current_vertices[vertices[0]], &model_current_vertices[vertices[1]],
                          &model_current_vertices[vertices[2]], &normal);
            NormalColorCol(&normal, color, (CVECTOR *)&poly->r0);
        }
        poly->code = color->cd;
    } else if (flags & 4) {
        model_lit_color_cache++;
        NormalColorCol((SVECTOR *)model_lit_color_cache, color, (CVECTOR *)&poly->r0);
        model_lit_color_cache += 2;
        poly->code = color->cd;
    } else {
        *(s32 *)&poly->r0 = *(s32 *)color;
    }
    return 1;
}

/* 8002D0C0: Type 12's record: packet length 5 and the auxiliary colour word as given. */
s32 model_prepare_f4_plain(s32 *value) {
    RenderPacket *packet = model_current_packet;

    packet->code = 5;
    packet->value = *value;
    return 1;
}

/* 8002D0E4: Build a textured quad (after any texture page/CLUT override command):
 * its color, CLUT and texture page (with the overrides) and coordinates. */
s32 model_prepare_ft4_plain(u16 *command) {
    POLY_FT4 *poly;

    if (model_apply_override_command((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_FT4 *)model_current_packet;
    setlen(poly, 9);
    *(s32 *)&poly->r0 = *(s32 *)command;
    *(s32 *)&poly->u0 = command[2] | (model_effective_clut << 16);
    *(s32 *)&poly->u1 = command[3] | (model_effective_tpage << 16);
    *(u16 *)&poly->u2 = command[4];
    *(u16 *)&poly->u3 = command[5];
    return 1;
}

/* 8002D180: Build a Gouraud quad: each corner's color lit by its vertex normal. */
s32 model_prepare_g4_lit(CVECTOR *color, s16 *vertices) {
    POLY_G4 *poly = (POLY_G4 *)model_current_packet;

    setlen(poly, 8);
    NormalColorCol3(&model_current_normals[vertices[0]], &model_current_normals[vertices[1]], &model_current_normals[vertices[2]],
                    color, (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    NormalColorCol(&model_current_normals[vertices[3]], color, (CVECTOR *)&poly->r3);
    poly->code = color->cd;
    return 1;
}

/* 8002D244: Build a Gouraud-shaded textured quad lit by its vertex normals (after any
 * texture page/CLUT override command). */
s32 model_prepare_gt4_lit(u16 *command, s16 *vertices) {
    POLY_GT4 *poly;

    if (model_apply_override_command((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_GT4 *)model_current_packet;
    setlen(poly, 12);
    NormalColor3(&model_current_normals[vertices[0]], &model_current_normals[vertices[1]], &model_current_normals[vertices[2]],
                 (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    NormalColor(&model_current_normals[vertices[3]], (CVECTOR *)&poly->r3);
    *(s32 *)&poly->u0 = command[2] | (model_effective_clut << 16);
    *(s32 *)&poly->u1 = command[3] | (model_effective_tpage << 16);
    *(u16 *)&poly->u2 = command[4];
    *(u16 *)&poly->u3 = command[5];
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* 8002D354: Build a Gouraud quad whose corner colors are `color` lit by the vertex
 * normals. */
s32 model_prepare_g4_lit_unreferenced(CVECTOR *color, s16 *vertices) {
    POLY_G4 *poly = (POLY_G4 *)model_current_packet;

    setlen(poly, 8);
    *(s32 *)&poly->r0 = *(s32 *)color;
    NormalColorCol3(&model_current_normals[vertices[0]], &model_current_normals[vertices[1]], &model_current_normals[vertices[2]],
                    color, (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    NormalColorCol(&model_current_normals[vertices[3]], color, (CVECTOR *)&poly->r3);
    poly->code = color->cd;
    return 1;
}

/* 8002D420: The same Gouraud textured quad builder for a second primitive command. */
s32 model_prepare_gt4_lit_unreferenced(u16 *command, s16 *vertices) {
    POLY_GT4 *poly;

    if (model_apply_override_command((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_GT4 *)model_current_packet;
    setlen(poly, 12);
    NormalColor3(&model_current_normals[vertices[0]], &model_current_normals[vertices[1]], &model_current_normals[vertices[2]],
                 (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    NormalColor(&model_current_normals[vertices[3]], (CVECTOR *)&poly->r3);
    *(s32 *)&poly->u0 = command[2] | (model_effective_clut << 16);
    *(s32 *)&poly->u1 = command[3] | (model_effective_tpage << 16);
    *(u16 *)&poly->u2 = command[4];
    *(u16 *)&poly->u3 = command[5];
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* 8002D530: Build a flat textured quad lit by the face normal of `vertices` (flag 1;
 * with flag 2 the normal goes to the lit-color cache) or by the cached
 * normal (flag 4); the cache advances either way. */
s32 model_prepare_ft4_lit(u16 *command, s16 *vertices, s32 flags) {
    POLY_FT4 *poly;
    SVECTOR normal;

    if (model_apply_override_command((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_FT4 *)model_current_packet;
    setlen(poly, 9);
    *(s32 *)&poly->u0 = command[2] | (model_effective_clut << 16);
    *(s32 *)&poly->u1 = command[3] | (model_effective_tpage << 16);
    *(u16 *)&poly->u2 = command[4];
    *(u16 *)&poly->u3 = command[5];
    if (flags & 1) {
        if (flags & 2) {
            model_compute_face_normal(&model_current_vertices[vertices[0]], &model_current_vertices[vertices[1]],
                          &model_current_vertices[vertices[2]], (SVECTOR *)model_lit_color_cache);
            NormalColor((SVECTOR *)model_lit_color_cache, (CVECTOR *)&poly->r0);
        } else {
            model_compute_face_normal(&model_current_vertices[vertices[0]], &model_current_vertices[vertices[1]],
                          &model_current_vertices[vertices[2]], &normal);
            NormalColor(&normal, (CVECTOR *)&poly->r0);
        }
    } else if (flags & 4) {
        NormalColor((SVECTOR *)model_lit_color_cache, (CVECTOR *)&poly->r0);
    }
    model_lit_color_cache += 2;
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* 8002D6AC: Build a Gouraud triangle whose corner colors are `color` lit by the
 * vertex normals; with flag 2 the color is also recorded in the lit-color
 * cache. */
s32 model_prepare_g3_lit(CVECTOR *color, s16 *vertices, s32 flags) {
    POLY_G3 *poly = (POLY_G3 *)model_current_packet;

    setlen(poly, 6);
    if (flags & 2) {
        *model_lit_color_cache = *(s32 *)color;
        model_lit_color_cache++;
    }
    NormalColorCol3(&model_current_normals[vertices[0]], &model_current_normals[vertices[1]], &model_current_normals[vertices[2]],
                    color, (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    poly->code = color->cd;
    return 1;
}

/* 8002D77C: The same Gouraud triangle builder without the cache. */
s32 model_prepare_g3_lit_uncached(CVECTOR *color, s16 *vertices) {
    POLY_G3 *poly = (POLY_G3 *)model_current_packet;

    setlen(poly, 6);
    NormalColorCol3(&model_current_normals[vertices[0]], &model_current_normals[vertices[1]], &model_current_normals[vertices[2]],
                    color, (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    poly->code = color->cd;
    return 1;
}

/* 8002D814: Build a flat textured triangle lit by the face normal of `vertices`
 * (flag 1; with flag 2 the normal goes to the lit-color cache) or by the
 * cached normal (flag 4); the cache advances either way. */
s32 model_prepare_ft3_lit(u16 *command, s16 *vertices, s32 flags) {
    POLY_FT3 *poly;
    SVECTOR normal;

    if (model_apply_override_command((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_FT3 *)model_current_packet;
    setlen(poly, 7);
    *(s32 *)&poly->u0 = command[2] | (model_effective_clut << 16);
    *(s32 *)&poly->u1 = command[3] | (model_effective_tpage << 16);
    *(u16 *)&poly->u2 = command[0];
    if (flags & 1) {
        if (flags & 2) {
            model_compute_face_normal(&model_current_vertices[vertices[0]], &model_current_vertices[vertices[1]],
                          &model_current_vertices[vertices[2]], (SVECTOR *)model_lit_color_cache);
            NormalColor((SVECTOR *)model_lit_color_cache, (CVECTOR *)&poly->r0);
        } else {
            model_compute_face_normal(&model_current_vertices[vertices[0]], &model_current_vertices[vertices[1]],
                          &model_current_vertices[vertices[2]], &normal);
            NormalColor(&normal, (CVECTOR *)&poly->r0);
        }
    } else if (flags & 4) {
        NormalColor((SVECTOR *)model_lit_color_cache, (CVECTOR *)&poly->r0);
    }
    model_lit_color_cache += 2;
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* 8002D984: Build an unlit flat textured triangle. */
s32 model_prepare_ft3_plain(u16 *command) {
    POLY_FT3 *poly;

    if (model_apply_override_command((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_FT3 *)model_current_packet;
    setlen(poly, 7);
    *(s32 *)&poly->u0 = command[2] | (model_effective_clut << 16);
    *(s32 *)&poly->u1 = command[3] | (model_effective_tpage << 16);
    *(u16 *)&poly->u2 = command[0];
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* 8002DA14: Build a Gouraud-shaded textured triangle lit by its vertex normals. */
s32 model_prepare_gt3_lit(u16 *command, s16 *vertices) {
    POLY_GT3 *poly;

    if (model_apply_override_command((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_GT3 *)model_current_packet;
    setlen(poly, 9);
    NormalColor3(&model_current_normals[vertices[0]], &model_current_normals[vertices[1]], &model_current_normals[vertices[2]],
                 (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    *(s32 *)&poly->u0 = command[2] | (model_effective_clut << 16);
    *(s32 *)&poly->u1 = command[3] | (model_effective_tpage << 16);
    *(u16 *)&poly->u2 = command[0];
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* 8002DAFC: Make the primitive being built a shade-free textured triangle on the
 * override texture page and CLUT. */
s32 model_prepare_ft3_envmap(void) {
    POLY_FT3 *poly = (POLY_FT3 *)model_current_packet;

    SetPolyFT3(poly);
    SetShadeTex(poly, 1);
    poly->tpage = (GetTPage(1, 0, 640, 0) & 0xFFE0) | model_tpage_override;
    poly->clut = (GetClut(0, 480) & 0xF) | model_clut_override;
    return 1;
}

/* 8002DB84: The unit normal of the triangle (v0, v1, v2). */
void model_compute_face_normal(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *normal) {
    VECTOR a;
    VECTOR b;
    VECTOR cross;
    s32 largest;
    s32 length;

    a.vx = v1->vx - v0->vx;
    a.vy = v1->vy - v0->vy;
    a.vz = v1->vz - v0->vz;
    b.vx = v2->vx - v0->vx;
    b.vy = v2->vy - v0->vy;
    b.vz = v2->vz - v0->vz;
    OuterProduct0(&b, &a, &cross);
    largest = model_get_largest_component(cross.vx, cross.vy, cross.vz);
    if (largest < 0) {
        largest = -largest;
    }
    length = SquareRoot0(largest);
    cross.vx /= length;
    cross.vy /= length;
    cross.vz /= length;
    VectorNormalS(&cross, normal);
}

/* 8002DC9C: The component of (x, y, z) with the largest magnitude. */
s32 model_get_largest_component(s32 x, s32 y, s32 z) {
    s32 ax = x;
    s32 ay = y;
    s32 az = z;

    if (ax < 0) {
        ax = -ax;
    }
    if (ay < 0) {
        ay = -ay;
    }
    if (az < 0) {
        az = -z;
    }
    if (ax >= ay && ax >= az) {
        return x;
    }
    if (ay >= ax && ay >= az) {
        return y;
    }
    if (az >= ax && az >= ay) {
        return z;
    }
}

/* 8002DD20: Load every image (and CLUT) of a TIM list: a count, then the byte
 * offsets of the TIMs from the list, loaded last to first. */
void model_load_tim_list(u32 *list) {
    TIM_IMAGE image;
    s32 i = list[0];

    while (--i != -1) {
        OpenTIM((u_long *)(list + (list[i + 1] >> 2)));
        ReadTIM(&image);
        if (image.caddr != NULL) {
            DrawSync(0);
            LoadImage(image.crect, image.caddr);
        }
        DrawSync(0);
        LoadImage(image.prect, image.paddr);
    }
}

/* Skip a listed image's origin and offset, read its size, upload its
 * pixels to `rect` and step `p` past them. */
#define LOAD_LISTED_IMAGE(rect, p)                                                                 \
    do {                                                                                           \
        (p) += 4;                                                                                  \
        (rect).w = *(p)++;                                                                         \
        (rect).h = *(p)++;                                                                         \
        LoadImage(&(rect), (u_long *)(p));                                                         \
        (p) += (rect).w * (rect).h;                                                                \
    } while (0)

/* 8002DDE4: Upload the images of an image list (a count and an offset table, then
 * the images: type 0x1100 or 0x1101, origin, offset, size and pixels).
 * Each type has a placement mode (1: base + offset, 2: base + origin +
 * offset, otherwise origin + offset) and a base position. Returns 1 at an
 * unknown image type, else 0. */
s32 model_load_image_list(s32 *images, s16 mode, s32 x, s32 y, s16 mode2, u16 x2, u16 y2) {
    RECT rect;
    u16 base_x = x;
    u16 base_y = y;
    s32 count = images[0];
    u16 *p = (u16 *)(images + (count + 1));
    s32 type;
    s32 i;

    for (i = 0; i < count; i++) {
        type = *(s32 *)p;
        p += 2;
        if (type == 0x1100) {
            switch (mode) {
            case 1:
                rect.x = base_x + p[2];
                rect.y = base_y + p[3];
                break;
            case 2:
                rect.x = p[2] + (base_x + p[0]);
                rect.y = p[3] + (base_y + p[1]);
                break;
            default:
                rect.x = p[0] + p[2];
                rect.y = p[1] + p[3];
                break;
            }
        } else if (type != 0x1101) {
            return 1;
        } else {
            switch (mode2) {
            case 1:
                rect.x = x2 + p[2];
                rect.y = y2 + p[3];
                break;
            case 2:
                rect.x = p[2] + (x2 + p[0]);
                rect.y = p[3] + (y2 + p[1]);
                break;
            default:
                rect.x = p[0] + p[2];
                rect.y = p[1] + p[3];
                break;
            }
        }
        LOAD_LISTED_IMAGE(rect, p);
    }
    return 0;
}

/* 8002DFE0: The shared unpack buffer. */
u8 *model_get_overlay_area(void) {
    return mode_overlay_area;
}

/* 8002DFF0: Set the screen bounds the renderers test projected vertices against: x
 * below `a`, y below `b` - 1. */
void model_set_screen_bounds(s32 a, s32 b) {
    model_screen_y_limit = (b - 1) << 16;
    model_screen_x_limit = a;
}

/* 8002E010: The handwritten draw routines of model_primitive_types (see model_draw.s). They
 * share the exit in model_draw_gt3_avg and several have alternate entries. */
INCLUDE_ASM("decomp/src/resident", model_draw_gt3_avg);

/* 8002E448 */
INCLUDE_ASM("decomp/src/resident", model_draw_gt3_far);

/* 8002E64C */
INCLUDE_ASM("decomp/src/resident", model_draw_gt4_far);

/* 8002E8B4 */
INCLUDE_ASM("decomp/src/resident", model_draw_gt3_near);

/* 8002EAB8 */
INCLUDE_ASM("decomp/src/resident", model_draw_gt4_near);

/* 8002ED20 */
INCLUDE_ASM("decomp/src/resident", model_draw_f3_lit);

/* 8002EEF8 */
INCLUDE_ASM("decomp/src/resident", model_draw_f3_cued);

/* 8002F0E4 */
INCLUDE_ASM("decomp/src/resident", model_draw_ft3_cued_far);

/* 8002F2E0 */
INCLUDE_ASM("decomp/src/resident", model_draw_ft3_lit);

/* 8002F4B4 */
INCLUDE_ASM("decomp/src/resident", model_draw_gt3_lit);

/* 8002F6B4 */
INCLUDE_ASM("decomp/src/resident", model_draw_g3_lit);

/* 8002F8D0 */
INCLUDE_ASM("decomp/src/resident", model_draw_f4_lit);

/* 8002FAE8 */
INCLUDE_ASM("decomp/src/resident", model_draw_ft4_lit);

/* 8002FCFC */
INCLUDE_ASM("decomp/src/resident", model_draw_ft4_cued);

/* 8002FF0C */
INCLUDE_ASM("decomp/src/resident", model_draw_ft4_cued_far);

/* 8003014C: Default morph channel update: step the weight toward the target by the
 * step, without overshooting. Returns the weight. */
s32 model_step_morph_weight(MorphChannel *channel) {
    if (channel->weight > channel->target) {
        channel->weight -= channel->step;
        if (channel->weight < channel->target) {
            channel->weight = channel->target;
        }
    }
    if (channel->weight < channel->target) {
        channel->weight += channel->step;
        if (channel->weight > channel->target) {
            channel->weight = channel->target;
        }
    }
    return channel->weight;
}

/* 800301C8: Copy the vertices listed in `indices` (last first) from `in` to `out`. */
void model_copy_indexed_vertices(SVECTOR *out, SVECTOR *in, s32 count, s16 *indices) {
    s32 i;
    s32 k;

    for (i = count - 1; i != -1; i--) {
        k = indices[i];
        out[k].vx = in[k].vx;
        out[k].vy = in[k].vy;
        out[k].vz = in[k].vz;
    }
}

/* 80030228: Add `count` morph deltas times `weight` (4.12) to their vertices. */
void model_add_morph_deltas(SVECTOR *vertices, MorphDelta *deltas, s32 count, s32 weight) {
    s32 k;

    if (weight != 0) {
        while (--count != -1) {
            k = deltas->index;
            vertices[k].vx += (deltas->dx * weight) >> 12;
            vertices[k].vy += (deltas->dy * weight) >> 12;
            vertices[k].vz += (deltas->dz * weight) >> 12;
            deltas++;
        }
    }
}

/* 800302D4: Add `count` morph deltas times `weight` (4.12) to their normals and
 * renormalize them. */
void model_add_morph_normal_deltas(SVECTOR *normals, MorphDelta *deltas, s32 count, s32 weight) {
    s32 k;

    if (weight != 0) {
        while (--count != -1) {
            k = deltas->index;
            normals[k].vx += (deltas->dx * weight) >> 12;
            normals[k].vy += (deltas->dy * weight) >> 12;
            normals[k].vz += (deltas->dz * weight) >> 12;
            VectorNormalSS(&normals[k], &normals[k]);
            deltas++;
        }
    }
}

/* 800303C8: Start morphing a sprite model: keep its vertex (and normal) arrays in a
 * new morph state and give the model copies to morph; every channel starts
 * at weight 0 with the default update. NULL when the model has no morphs. */
MorphState *model_start_morph(SpriteModel *model, s32 mode) {
    MorphTable *table = model->morphs;
    MorphState *state;
    MorphChannel *channel;
    s32 i;

    if (table == NULL) {
        return NULL;
    }
    heap_set_next_class(0x2B);
    state = heap_alloc((table->count << 5) | 0x14, mode);
    state->model = model;
    state->vertices = model->vertices;
    state->normals = model->normals;
    state->channels = channel = (MorphChannel *)(state + 1);
    state->count = table->count;
    heap_set_next_class(0x2C);
    model->vertices = heap_alloc(model->vertex_count * 8, mode);
    i = model->vertex_count;
    while (--i != -1) {
        model->vertices[i].vx = state->vertices[i].vx;
        model->vertices[i].vy = state->vertices[i].vy;
        model->vertices[i].vz = state->vertices[i].vz;
    }
    if (model->flags & 0x10) {
        heap_set_next_class(0x2D);
        model->normals = heap_alloc(model->vertex_count * 8, mode);
        i = model->vertex_count;
        while (--i != -1) {
            model->normals[i].vx = state->normals[i].vx;
            model->normals[i].vy = state->normals[i].vy;
            model->normals[i].vz = state->normals[i].vz;
        }
    }
    for (i = 0; i < state->count; channel++, i++) {
        channel->update = model_step_morph_weight;
        channel->target = 0;
        channel->weight = 0;
        channel->step = 0;
    }
    return state;
}

/* 800305D8: Morph a sprite model: restore the touched vertices, then add each
 * target's deltas at the weight its channel update returns (and to the
 * normals). */
void model_update_morph(MorphState *state) {
    SpriteModel *model;
    MorphTarget *target;
    MorphChannel *channel;
    s32 count;
    s32 weight;
    s32 i;

    if (state != NULL) {
        model = state->model;
        count = state->count;
        target = model->morphs->targets;
        channel = state->channels;
        model_copy_indexed_vertices(model->vertices, state->vertices, target[count].count, target[count].deltas);
        for (i = 0; i < count; channel++, i++) {
            weight = channel->update(channel);
            model_add_morph_deltas(model->vertices, target[i].deltas, target[i].count, weight);
            if (model->flags & 0x10) {
                model_add_morph_normal_deltas(model->normals, target[i].normals, target[i].count, weight);
            }
        }
    }
}

/* 800306D0: Stop morphing: free the model's morphed copies, give it back its own
 * vertex and normal arrays and free the state. */
void model_stop_morph(MorphState *state) {
    SpriteModel *model;

    if (state != NULL) {
        model = state->model;
        heap_free(model->vertices);
        if (model->flags & 0x10) {
            heap_free(model->normals);
        }
        model->vertices = state->vertices;
        model->normals = state->normals;
        heap_free(state);
    }
}

/* 80030750: The environment-mapped triangle renderer of primitive type 16 and the
 * routine that rewrites its texture coordinate shifts and offsets. */
INCLUDE_ASM("decomp/src/resident", model_draw_ft3_envmap);

/* 80030988 */
INCLUDE_ASM("decomp/src/resident", model_set_envmap_mapping);

/* 80030A30: Set light `index` (0-2): its row of the light direction matrix is the
 * normalized reverse of the light's vector and its color a column of the
 * light color matrix, which is loaded into the GTE. */
void model_set_light(u16 index, ModelLight *light) {
    VECTOR reverse;

    reverse.vx = -light->vx;
    reverse.vy = -light->vy;
    reverse.vz = -light->vz;
    VectorNormalS(&reverse, (SVECTOR *)model_light_directions.m[index]);
    model_light_colors.m[0][index] = light->r;
    model_light_colors.m[1][index] = light->g;
    model_light_colors.m[2][index] = light->b;
    gte_SetColorMatrix(&model_light_colors);
}

/* 80030B14: Load the GTE light matrix: the light directions turned by `rotation`. */
void model_load_light_matrix(MATRIX *rotation) {
    MATRIX light;

    gte_MulMatrix0(&model_light_directions, rotation, &light);
    gte_SetLightMatrix(&light);
}

/* 80030C40: Set the GTE background color from 16-bit color components. */
void model_set_back_color_16bit(u16 r, u16 g, u16 b) {
    gte_SetBackColor(r >> 4, g >> 4, b >> 4);
}

/* 80030C78: Set the GTE background color. */
void model_set_back_color(s32 r, s32 g, s32 b) {
    gte_SetBackColor(r, g, b);
}

/* 80030C98: Render `count` flat quads lit from the lit-color cache: transform the
 * four vertices, drop quads with a GTE error or facing away, fill a
 * POLY_F4 and link it into the ordering table at its average depth.
 * The last two indices share a packed word, like the first pair. */
void model_draw_f4_lit_unreferenced(QuadFace *faces, s32 count) {
    POLY_F4 *poly;
    s32 flag;
    s32 sz0, sz1, sz2, sz3;
    s32 otz;

    for (count--; count != -1; count--) {
        poly = (POLY_F4 *)model_current_packet;
        gte_ldv3(&model_current_vertices[faces->v01 & 0xFFFF], &model_current_vertices[faces->v01 >> 16],
                 &model_current_vertices[(*(u32 *)&faces->v2 & 0xFFFF)]);
        gte_rtpt();
        gte_stflg(&flag);
        if (flag >= 0) {
            gte_nclip();
            gte_stopz(&flag);
            if (flag > 0) {
                gte_stsxy3(&poly->x0, &poly->x1, &poly->x2);
                gte_stsz3(&sz0, &sz1, &sz2);
                gte_ldv0(&model_current_vertices[(*(u32 *)&faces->v2 >> 16)]);
                gte_rtps();
                gte_stsxy(&poly->x3);
                gte_stsz(&sz3);
                gte_ldsz4(sz0, sz1, sz2, sz3);
                gte_avsz4();
                gte_stotz(&otz);
                gte_ldrgb(model_lit_color_cache);
                gte_ldv0(model_lit_color_cache + 1);
                gte_nccs();
                gte_strgb(&poly->r0);
                setlen(poly, 5);
                setcode(poly, 0x28);
                otz >>= model_ot_depth_shift;
                addPrim(&model_ot[otz], poly);
                model_drawn_primitive_count++;
            }
        }
        faces++;
        model_current_packet = (RenderPacket *)((u8 *)model_current_packet + sizeof(POLY_F4));
        model_lit_color_cache += 3;
    }
}

/* 80030EE8: Perspective-transform the three loaded vertices. Nonzero when one of
 * them has a usable depth and lands inside the screen (8002DFF0). */
s32 model_is_any_vertex_on_screen(void) {
    s32 sz0, sz1, sz2;
    u32 sxy;

    gte_rtpt();
    gte_stsz3(&sz0, &sz1, &sz2);
    gte_stsxy0(&sxy);
    if ((u16)(sz0 + 1) >= 2 && sxy < model_screen_y_limit && (sxy & 0xFFFF) < model_screen_x_limit) {
        return 1;
    }
    gte_stsxy1(&sxy);
    if ((u16)(sz1 + 1) >= 2 && sxy < model_screen_y_limit && (sxy & 0xFFFF) < model_screen_x_limit) {
        return 1;
    }
    gte_stsxy2(&sxy);
    if ((u16)(sz2 + 1) >= 2 && sxy < model_screen_y_limit && (sxy & 0xFFFF) < model_screen_x_limit) {
        return 1;
    }
    return 0;
}

/* Halfway from a to b. */
#define HALFWAY(a, b) ((a) + ((b) - (a)) / 2)

/* 8003101C: Whether a sprite model's bounding box is off screen: none of the
 * triangles tested reaches the screen (80030EE8). Mode bit 0 tests the box
 * diagonal and three faces' diagonals, bit 1 four triangles through the
 * edge midpoints. */
s32 model_is_box_off_screen(model, mode)
SpriteModel *model;
u16 mode;
{
    SVECTOR v;

    if (mode & 1) {
        gte_ldv0(&model->box_min);
        gte_ldv1(&model->box_max);
        v.vx = HALFWAY(model->box_min.vx, model->box_max.vx);
        v.vy = HALFWAY(model->box_min.vy, model->box_max.vy);
        v.vz = HALFWAY(model->box_min.vz, model->box_max.vz);
        gte_ldv2(&v);
        if (model_is_any_vertex_on_screen()) {
            return 0;
        }
        v.vx = model->box_max.vx;
        v.vy = model->box_min.vy;
        v.vz = model->box_min.vz;
        gte_ldv0(&v);
        v.vx = model->box_min.vx;
        v.vy = model->box_max.vy;
        v.vz = model->box_min.vz;
        gte_ldv1(&v);
        v.vx = model->box_max.vx;
        v.vy = model->box_max.vy;
        v.vz = model->box_min.vz;
        gte_ldv2(&v);
        if (model_is_any_vertex_on_screen()) {
            return 0;
        }
        v.vx = model->box_min.vx;
        v.vy = model->box_min.vy;
        v.vz = model->box_max.vz;
        gte_ldv0(&v);
        v.vx = model->box_max.vx;
        v.vy = model->box_min.vy;
        v.vz = model->box_max.vz;
        gte_ldv1(&v);
        v.vx = model->box_min.vx;
        v.vy = model->box_max.vy;
        v.vz = model->box_max.vz;
        gte_ldv2(&v);
        if (model_is_any_vertex_on_screen()) {
            return 0;
        }
    }
    if (mode & 2) {
        v.vx = HALFWAY(model->box_min.vx, model->box_max.vx);
        v.vy = model->box_min.vy;
        v.vz = model->box_min.vz;
        gte_ldv0(&v);
        v.vx = model->box_min.vx;
        v.vy = model->box_min.vy;
        v.vz = HALFWAY(model->box_min.vz, model->box_max.vz);
        gte_ldv1(&v);
        v.vx = model->box_min.vx;
        v.vy = HALFWAY(model->box_min.vy, model->box_max.vy);
        v.vz = model->box_min.vz;
        gte_ldv2(&v);
        if (model_is_any_vertex_on_screen()) {
            return 0;
        }
        v.vx = model->box_max.vx;
        v.vy = HALFWAY(model->box_max.vy, model->box_min.vy);
        v.vz = model->box_min.vz;
        gte_ldv0(&v);
        v.vx = HALFWAY(model->box_max.vx, model->box_min.vx);
        v.vy = model->box_max.vy;
        v.vz = model->box_min.vz;
        gte_ldv1(&v);
        v.vx = model->box_max.vx;
        v.vy = model->box_max.vy;
        v.vz = HALFWAY(model->box_min.vz, model->box_max.vz);
        gte_ldv2(&v);
        if (model_is_any_vertex_on_screen()) {
            return 0;
        }
        v.vx = model->box_min.vx;
        v.vy = model->box_max.vy;
        v.vz = HALFWAY(model->box_max.vz, model->box_min.vz);
        gte_ldv0(&v);
        v.vx = model->box_min.vx;
        v.vy = HALFWAY(model->box_max.vy, model->box_min.vy);
        v.vz = model->box_max.vz;
        gte_ldv1(&v);
        v.vx = HALFWAY(model->box_min.vx, model->box_max.vx);
        v.vy = model->box_max.vy;
        v.vz = model->box_max.vz;
        gte_ldv2(&v);
        if (model_is_any_vertex_on_screen()) {
            return 0;
        }
        v.vx = model->box_max.vx;
        v.vy = model->box_min.vy;
        v.vz = HALFWAY(model->box_max.vz, model->box_min.vz);
        gte_ldv0(&v);
        v.vx = HALFWAY(model->box_max.vx, model->box_min.vx);
        v.vy = model->box_min.vy;
        v.vz = model->box_max.vz;
        gte_ldv1(&v);
        v.vx = model->box_max.vx;
        v.vy = HALFWAY(model->box_min.vy, model->box_max.vy);
        v.vz = model->box_max.vz;
        gte_ldv2(&v);
        if (model_is_any_vertex_on_screen()) {
            return 0;
        }
    }
    return 1;
}

/* 800315A0 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_poly_f3);

/* 800315C4 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_poly_ft3);

/* 800315E8 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_poly_g3);

/* 8003160C */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_poly_gt3);

/* 80031630 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_poly_f4);

/* 80031654 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_poly_ft4);

/* 80031678 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_poly_g4);

/* 8003169C */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_11_words);

/* 800316C0 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_line_f2);

/* 800316E4 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_line_g2);

/* 80031708 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_line_f3);

/* 8003172C */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_line_g3);

/* 80031750 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_line_f4);

/* 80031774 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_line_g4);

/* 80031798 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_sprt);

/* 800317BC */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_sprt_16);

/* 800317E0 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_sprt_8);

/* 80031804 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_tile);

/* 80031828 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_tile_16);

/* 8003184C */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_tile_8);

/* 80031870 */
INCLUDE_ASM("decomp/src/resident", gpu_ot_link_tile_1);
