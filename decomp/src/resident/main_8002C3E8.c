#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/inline_c.h"
#include "psyq/libsn.h"
#include "psyq/libspu.h"
#include "mode.h"
#include "menu.h"
#include "sprite.h"
#include "cd.h"
#include "stream.h"
#include "model.h"
#include "heap.h"
#include "text.h"
#include "pad.h"
#include "console.h"
#include "sound.h"

/* Relocate a model group's offsets to addresses (once). Returns the number
 * of models.
 * Nonmatching: the original walks the models through one pointer at their
 * list fields. */
#ifdef NON_MATCHING
s32 func_8002C3E8(ModelGroup *group) {
    s32 flags = group->flags;
    s32 count = group->count;
    Model *model;
    ModelListEntry *entry;
    s32 i;
    s32 n;

    if (!(flags & 1)) {
        group->flags = flags | 1;
        for (i = 0, model = group->models; i < count; i++, model++) {
            model->table0 += (s32)group;
            model->table8 += (s32)group;
            model->table4 += (s32)group;
            model->primitives += (s32)group;
            if (model->list != NULL) {
                model->list = (ModelList *)((u8 *)model->list + (s32)group);
                n = model->list->last;
                if (n != -1) {
                    entry = &model->list->entries[n];
                    do {
                        n--;
                        entry->first += (s32)group;
                        entry->second += (s32)group;
                        entry--;
                    } while (n != -1);
                }
            }
        }
    }
    return count;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002C3E8);
#endif

/* Undo 8002C3E8: turn a relocated model group's addresses back into
 * offsets. Returns the number of models.
 * Nonmatching: the original walks the models through one pointer at their
 * list fields. */
#ifdef NON_MATCHING
s32 func_8002C4BC(ModelGroup *group) {
    s32 flags = group->flags;
    s32 count = group->count;
    Model *model;
    ModelListEntry *entry;
    s32 i;
    s32 n;

    if (flags & 1) {
        group->flags = flags & ~1;
        for (i = 0, model = group->models; i < count; i++, model++) {
            model->table0 -= (s32)group;
            model->table8 -= (s32)group;
            model->table4 -= (s32)group;
            model->primitives -= (s32)group;
            if (model->list != NULL) {
                n = model->list->last;
                if (n != -1) {
                    entry = &model->list->entries[n];
                    do {
                        n--;
                        entry->first -= (s32)group;
                        entry->second -= (s32)group;
                        entry--;
                    } while (n != -1);
                }
                model->list = (ModelList *)((u8 *)model->list - (s32)group);
            }
        }
    }
    return count;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002C4BC);
#endif

/* Relocate a sprite model's offsets to addresses (once).
 * Nonmatching: the target loop's pointer is biased differently. */
#ifdef NON_MATCHING
void func_8002C59C(SpriteModel *model) {
    MorphTable *table;
    MorphTarget *targets;
    MorphTarget *target;
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
                target = &targets[n];
                do {
                    n--;
                    target->deltas = (u8 *)target->deltas + (s32)model;
                    target->normals = (MorphDelta *)((u8 *)target->normals + (s32)model);
                    target--;
                } while (n != -1);
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002C59C);
#endif

/* Trim a model group's heap block to its data (once). Returns 1 when it
 * was already trimmed. */
s32 func_8002C644(ModelGroup *group) {
    if (group->flags & 2) {
        return 1;
    }
    group->flags |= 2;
    func_80031F70((u8 *)group, group->models[0].primitives - (u8 *)group);
    return 0;
}

/* Trim a model buffer's heap block at its end (once). Returns 1 when it
 * was already trimmed. */
s32 func_8002C68C(ModelBuffer *buffer) {
    if (buffer->flags & 0x40) {
        return 1;
    }
    buffer->flags |= 0x40;
    func_80031F70((u8 *)buffer, buffer->end - (u8 *)buffer);
    buffer->end = NULL;
    return 0;
}

extern u8 D_80059598;
extern u8 D_80059599;
extern u8 D_8005959A;

void func_8002C6E0(u8 r, u8 g, u8 b) {
    D_80059598 = r;
    D_80059599 = g;
    D_8005959A = b;
}

/* Draw a sprite model's primitive groups into `ot` with the routines of
 * sort mode `mode`, building packets from `packets`. Returns 0 when its
 * bounding box test finds it off screen, else 1.
 * Nonmatching: the model fields loaded before the loop take other
 * registers (the primitive count is loaded first). */
#ifdef NON_MATCHING
s32 func_8002C700(SpriteModel *model, RenderPacket *packets, u32 *ot, s32 mode) {
    s32 count;
    PrimitiveGroup *group;
    PrimitiveType *type;
    void (*draw)(u8 *records, s32 count);

    if (D_80050104 != 0 && func_8003101C(model, D_80050104)) {
        return 0;
    }
    count = model->group_count;
    D_80059424 = packets;
    D_80059568 = ot;
    D_800595C0 += model->primitive_count;
    D_80059528 = (PrimitiveGroup *)model->unk10;
    D_80059498 = (s32 *)model->unk18;
    D_8005952C = model->normals;
    D_8005953C = model->vertices;
    for (count--; count != -1; count--) {
        group = D_80059528;
        type = &D_8004FE50[group->type];
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
        D_80059528++;
        draw((u8 *)D_80059528, group->count);
        D_80059528 = (PrimitiveGroup *)((u8 *)D_80059528 + group->count * type->stride);
    }
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002C700);
#endif

/* Build a sprite model's packets in `packets`. A mode other than 0 keeps
 * an auxiliary block and selects the variant the preparing routines
 * build: 1 plain, 2 and 3 the lit variants once the auxiliary block
 * exists (3 when the model was not yet built with them, 4 after). */
void func_8002C8CC(SpriteModel *model, RenderPacket *packets, s32 mode) {
    s32 groups;
    s32 count;
    PrimitiveGroup *group;
    PrimitiveType *type;
    s32 (*prepare)(u8 *aux, u8 *record, s16 kind);
    s32 kind;
    u16 flags;

    D_80059424 = packets;
    if (!(model->flags & 1) && model->aux_size != 0 && mode != 0) {
        func_800324B8(0x26);
        model->unk18 = func_80031BDC(model->aux_size, 0);
        model->flags |= 1;
    }
    D_80059528 = (PrimitiveGroup *)model->unk10;
    D_80059538 = model->unk14;
    D_8005952C = model->normals;
    D_8005953C = model->vertices;
    D_80059498 = (s32 *)model->unk18;
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
    D_800595C0 += model->primitive_count;
    for (groups--; groups != -1; groups--) {
        group = D_80059528;
        count = group->count;
        D_80059528 = group + 1;
        type = &D_8004FE50[group->type];
        prepare = type->prepare;
        for (count--; count != -1; count--) {
            if (prepare(D_80059538, (u8 *)D_80059528, kind)) {
                D_80059528 = (PrimitiveGroup *)((u8 *)D_80059528 + type->stride);
                D_80059424 = (RenderPacket *)((u8 *)D_80059424 + type->packet_size);
                D_80059538 += type->aux_stride;
            } else {
                D_80059538 += 4;
                count++;
            }
        }
    }
    func_8002CCAC();
}

/* Allocate a model buffer's two halves of `size` bytes each. */
void func_8002CB54(ModelBuffer *buffer, u8 **first, u8 **second) {
    u8 *block;

    func_800324B8(0x25);
    block = func_80031BDC(buffer->size * 2, 0);
    *first = block;
    *second = block + buffer->size;
}

/* Release a model buffer's owned block. */
void func_8002CBBC(ModelBuffer *buffer) {
    if (buffer->flags & 1) {
        func_800320E8(buffer->buffer);
        buffer->flags &= ~1;
    }
}

extern s32 D_80050108; /* texture page override: 0 none, 1 page, 2 raw */
extern s32 D_8005010C; /* CLUT override: 0 on */
extern s32 D_80059310;
extern s32 D_80059314;

/* Override model texture pages with the page at (x, y). */
void func_8002CC10(u16 x, u16 y) {
    D_80059310 = GetTPage(0, 0, x, y) & 0x1F;
    D_80050108 = 1;
}

void func_8002CC54(u16 tpage) {
    D_80059310 = tpage;
    D_80050108 = 2;
}

/* Override model CLUTs with the CLUT at (x, y). */
void func_8002CC74(u16 x, u16 y) {
    D_80059314 = GetClut(x, y) & 0xFFF0;
    D_8005010C = 0;
}

void func_8002CCAC(void) {
    D_80050108 = 0;
    D_8005010C = 1;
}

extern u16 D_80059308;
extern u16 D_8005930C;

/* Apply the texture page override to a primitive's page. */
void func_8002CCC8(u16 *tpage) {
    u16 value = *tpage;

    D_80059308 = value;
    if (D_80050108 == 1) {
        D_80059308 = value & 0xFFE0;
        D_80059308 = (value & 0xFFE0) | D_80059310;
    } else if (D_80050108 == 2) {
        D_80059308 = D_80059310;
    }
}

/* Apply the CLUT override to a primitive's CLUT. */
void func_8002CD24(u16 *clut) {
    u16 value = *clut;

    D_8005930C = value;
    if (D_8005010C == 0) {
        D_8005930C = value & 0xF;
        D_8005930C = (value & 0xF) | D_80059314;
    }
}

/* Handle a texture page (0xC4) or CLUT (0xC8) command. Returns 1 for any
 * other command. */
s32 func_8002CD64(u8 *command) {
    if ((command[3] & 0xF0) != 0xC0) {
        return 1;
    }
    switch (command[3]) {
    case 0xC4:
        func_8002CCC8((u16 *)command);
        return 0;
    case 0xC8:
        func_8002CD24((u16 *)command);
        return 0;
    }
    return 1;
}

/* Build a flat triangle's color: lit by the face normal of `vertices`
 * (flag 1; with flag 2 the color and normal are also recorded in the
 * lit-color cache), lit from the cache (flag 4), or copied. */
s32 func_8002CDCC(CVECTOR *color, s16 *vertices, s32 flags) {
    POLY_F3 *poly = (POLY_F3 *)D_80059424;
    SVECTOR normal;

    setlen(poly, 4);
    if (flags & 1) {
        if (flags & 2) {
            *D_80059498 = *(s32 *)color;
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], (SVECTOR *)++D_80059498);
            NormalColorCol((SVECTOR *)D_80059498, color, (CVECTOR *)&poly->r0);
            D_80059498 += 2;
        } else {
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], &normal);
            NormalColorCol(&normal, color, (CVECTOR *)&poly->r0);
        }
        poly->code = color->cd;
    } else if (flags & 4) {
        D_80059498++;
        NormalColorCol((SVECTOR *)D_80059498, color, (CVECTOR *)&poly->r0);
        D_80059498 += 2;
        poly->code = color->cd;
    } else {
        *(s32 *)&poly->r0 = *(s32 *)color;
    }
    return 1;
}

s32 func_8002CF34(s32 *value) {
    RenderPacket *packet = D_80059424;

    packet->code = 4;
    packet->value = *value;
    return 1;
}

/* The same flat triangle color handler for a second primitive command. */
s32 func_8002CF58(CVECTOR *color, s16 *vertices, s32 flags) {
    POLY_F3 *poly = (POLY_F3 *)D_80059424;
    SVECTOR normal;

    setlen(poly, 4);
    if (flags & 1) {
        if (flags & 2) {
            *D_80059498 = *(s32 *)color;
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], (SVECTOR *)++D_80059498);
            NormalColorCol((SVECTOR *)D_80059498, color, (CVECTOR *)&poly->r0);
            D_80059498 += 2;
        } else {
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], &normal);
            NormalColorCol(&normal, color, (CVECTOR *)&poly->r0);
        }
        poly->code = color->cd;
    } else if (flags & 4) {
        D_80059498++;
        NormalColorCol((SVECTOR *)D_80059498, color, (CVECTOR *)&poly->r0);
        D_80059498 += 2;
        poly->code = color->cd;
    } else {
        *(s32 *)&poly->r0 = *(s32 *)color;
    }
    return 1;
}

s32 func_8002D0C0(s32 *value) {
    RenderPacket *packet = D_80059424;

    packet->code = 5;
    packet->value = *value;
    return 1;
}

/* Build a textured quad (after any texture page/CLUT override command):
 * its color, CLUT and texture page (with the overrides) and coordinates. */
s32 func_8002D0E4(u16 *command) {
    POLY_FT4 *poly;

    if (func_8002CD64((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_FT4 *)D_80059424;
    setlen(poly, 9);
    *(s32 *)&poly->r0 = *(s32 *)command;
    *(s32 *)&poly->u0 = command[2] | (D_8005930C << 16);
    *(s32 *)&poly->u1 = command[3] | (D_80059308 << 16);
    *(u16 *)&poly->u2 = command[4];
    *(u16 *)&poly->u3 = command[5];
    return 1;
}

/* Build a Gouraud quad: each corner's color lit by its vertex normal. */
s32 func_8002D180(CVECTOR *color, s16 *vertices) {
    POLY_G4 *poly = (POLY_G4 *)D_80059424;

    setlen(poly, 8);
    NormalColorCol3(&D_8005952C[vertices[0]], &D_8005952C[vertices[1]], &D_8005952C[vertices[2]],
                    color, (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    NormalColorCol(&D_8005952C[vertices[3]], color, (CVECTOR *)&poly->r3);
    poly->code = color->cd;
    return 1;
}

/* Build a Gouraud-shaded textured quad lit by its vertex normals (after any
 * texture page/CLUT override command). */
s32 func_8002D244(u16 *command, s16 *vertices) {
    POLY_GT4 *poly;

    if (func_8002CD64((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_GT4 *)D_80059424;
    setlen(poly, 12);
    NormalColor3(&D_8005952C[vertices[0]], &D_8005952C[vertices[1]], &D_8005952C[vertices[2]],
                 (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    NormalColor(&D_8005952C[vertices[3]], (CVECTOR *)&poly->r3);
    *(s32 *)&poly->u0 = command[2] | (D_8005930C << 16);
    *(s32 *)&poly->u1 = command[3] | (D_80059308 << 16);
    *(u16 *)&poly->u2 = command[4];
    *(u16 *)&poly->u3 = command[5];
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* Build a Gouraud quad whose corner colors are `color` lit by the vertex
 * normals. */
s32 func_8002D354(CVECTOR *color, s16 *vertices) {
    POLY_G4 *poly = (POLY_G4 *)D_80059424;

    setlen(poly, 8);
    *(s32 *)&poly->r0 = *(s32 *)color;
    NormalColorCol3(&D_8005952C[vertices[0]], &D_8005952C[vertices[1]], &D_8005952C[vertices[2]],
                    color, (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    NormalColorCol(&D_8005952C[vertices[3]], color, (CVECTOR *)&poly->r3);
    poly->code = color->cd;
    return 1;
}

/* The same Gouraud textured quad builder for a second primitive command. */
s32 func_8002D420(u16 *command, s16 *vertices) {
    POLY_GT4 *poly;

    if (func_8002CD64((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_GT4 *)D_80059424;
    setlen(poly, 12);
    NormalColor3(&D_8005952C[vertices[0]], &D_8005952C[vertices[1]], &D_8005952C[vertices[2]],
                 (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    NormalColor(&D_8005952C[vertices[3]], (CVECTOR *)&poly->r3);
    *(s32 *)&poly->u0 = command[2] | (D_8005930C << 16);
    *(s32 *)&poly->u1 = command[3] | (D_80059308 << 16);
    *(u16 *)&poly->u2 = command[4];
    *(u16 *)&poly->u3 = command[5];
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* Build a flat textured quad lit by the face normal of `vertices` (flag 1;
 * with flag 2 the normal goes to the lit-color cache) or by the cached
 * normal (flag 4); the cache advances either way. */
s32 func_8002D530(u16 *command, s16 *vertices, s32 flags) {
    POLY_FT4 *poly;
    SVECTOR normal;

    if (func_8002CD64((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_FT4 *)D_80059424;
    setlen(poly, 9);
    *(s32 *)&poly->u0 = command[2] | (D_8005930C << 16);
    *(s32 *)&poly->u1 = command[3] | (D_80059308 << 16);
    *(u16 *)&poly->u2 = command[4];
    *(u16 *)&poly->u3 = command[5];
    if (flags & 1) {
        if (flags & 2) {
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], (SVECTOR *)D_80059498);
            NormalColor((SVECTOR *)D_80059498, (CVECTOR *)&poly->r0);
        } else {
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], &normal);
            NormalColor(&normal, (CVECTOR *)&poly->r0);
        }
    } else if (flags & 4) {
        NormalColor((SVECTOR *)D_80059498, (CVECTOR *)&poly->r0);
    }
    D_80059498 += 2;
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* Build a Gouraud triangle whose corner colors are `color` lit by the
 * vertex normals; with flag 2 the color is also recorded in the lit-color
 * cache. */
s32 func_8002D6AC(CVECTOR *color, s16 *vertices, s32 flags) {
    POLY_G3 *poly = (POLY_G3 *)D_80059424;

    setlen(poly, 6);
    if (flags & 2) {
        *D_80059498 = *(s32 *)color;
        D_80059498++;
    }
    NormalColorCol3(&D_8005952C[vertices[0]], &D_8005952C[vertices[1]], &D_8005952C[vertices[2]],
                    color, (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    poly->code = color->cd;
    return 1;
}

/* The same Gouraud triangle builder without the cache. */
s32 func_8002D77C(CVECTOR *color, s16 *vertices) {
    POLY_G3 *poly = (POLY_G3 *)D_80059424;

    setlen(poly, 6);
    NormalColorCol3(&D_8005952C[vertices[0]], &D_8005952C[vertices[1]], &D_8005952C[vertices[2]],
                    color, (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    poly->code = color->cd;
    return 1;
}

/* Build a flat textured triangle lit by the face normal of `vertices`
 * (flag 1; with flag 2 the normal goes to the lit-color cache) or by the
 * cached normal (flag 4); the cache advances either way. */
s32 func_8002D814(u16 *command, s16 *vertices, s32 flags) {
    POLY_FT3 *poly;
    SVECTOR normal;

    if (func_8002CD64((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_FT3 *)D_80059424;
    setlen(poly, 7);
    *(s32 *)&poly->u0 = command[2] | (D_8005930C << 16);
    *(s32 *)&poly->u1 = command[3] | (D_80059308 << 16);
    *(u16 *)&poly->u2 = command[0];
    if (flags & 1) {
        if (flags & 2) {
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], (SVECTOR *)D_80059498);
            NormalColor((SVECTOR *)D_80059498, (CVECTOR *)&poly->r0);
        } else {
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], &normal);
            NormalColor(&normal, (CVECTOR *)&poly->r0);
        }
    } else if (flags & 4) {
        NormalColor((SVECTOR *)D_80059498, (CVECTOR *)&poly->r0);
    }
    D_80059498 += 2;
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* Build an unlit flat textured triangle. */
s32 func_8002D984(u16 *command) {
    POLY_FT3 *poly;

    if (func_8002CD64((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_FT3 *)D_80059424;
    setlen(poly, 7);
    *(s32 *)&poly->u0 = command[2] | (D_8005930C << 16);
    *(s32 *)&poly->u1 = command[3] | (D_80059308 << 16);
    *(u16 *)&poly->u2 = command[0];
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* Build a Gouraud-shaded textured triangle lit by its vertex normals. */
s32 func_8002DA14(u16 *command, s16 *vertices) {
    POLY_GT3 *poly;

    if (func_8002CD64((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_GT3 *)D_80059424;
    setlen(poly, 9);
    NormalColor3(&D_8005952C[vertices[0]], &D_8005952C[vertices[1]], &D_8005952C[vertices[2]],
                 (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    *(s32 *)&poly->u0 = command[2] | (D_8005930C << 16);
    *(s32 *)&poly->u1 = command[3] | (D_80059308 << 16);
    *(u16 *)&poly->u2 = command[0];
    poly->code = ((u8 *)command)[3];
    return 1;
}

/* Make the primitive being built a shade-free textured triangle on the
 * override texture page and CLUT. */
s32 func_8002DAFC(void) {
    POLY_FT3 *poly = (POLY_FT3 *)D_80059424;

    SetPolyFT3(poly);
    SetShadeTex(poly, 1);
    poly->tpage = (GetTPage(1, 0, 640, 0) & 0xFFE0) | D_80059310;
    poly->clut = (GetClut(0, 480) & 0xF) | D_80059314;
    return 1;
}

/* The unit normal of the triangle (v0, v1, v2). */
void func_8002DB84(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *normal) {
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
    largest = func_8002DC9C(cross.vx, cross.vy, cross.vz);
    if (largest < 0) {
        largest = -largest;
    }
    length = SquareRoot0(largest);
    cross.vx /= length;
    cross.vy /= length;
    cross.vz /= length;
    VectorNormalS(&cross, normal);
}

/* The component of (x, y, z) with the largest magnitude. */
s32 func_8002DC9C(s32 x, s32 y, s32 z) {
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

/* Load every image (and CLUT) of a TIM list: a count, then the byte
 * offsets of the TIMs from the list, loaded last to first. */
void func_8002DD20(u32 *list) {
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

/* Upload the images of an image list (a count and an offset table, then
 * the images: type 0x1100 or 0x1101, origin, offset, size and pixels).
 * Each type has a placement mode (1: base + offset, 2: base + origin +
 * offset, otherwise origin + offset) and a base position. Returns 1 at an
 * unknown image type, else 0.
 * Nonmatching: register allocation of the base position and the order of
 * the placement sums differ. */
#ifdef NON_MATCHING
s32 func_8002DDE4(s32 *images, s16 mode, s32 x, s32 y, s16 mode2, u16 x2, u16 y2) {
    RECT rect;
    s32 count = images[0];
    u16 *p = (u16 *)(images + count + 1);
    s32 type;
    s32 i;

    for (i = 0; i < count; i++) {
        type = *(s32 *)p;
        p += 2;
        if (type == 0x1100) {
            switch (mode) {
            case 1:
                rect.x = x + p[2];
                rect.y = y + p[3];
                break;
            case 2:
                rect.x = x + p[0] + p[2];
                rect.y = y + p[1] + p[3];
                break;
            default:
                rect.x = p[0] + p[2];
                rect.y = p[1] + p[3];
                break;
            }
        } else if (type == 0x1101) {
            switch (mode2) {
            case 1:
                rect.x = x2 + p[2];
                rect.y = y2 + p[3];
                break;
            case 2:
                rect.x = x2 + p[0] + p[2];
                rect.y = y2 + p[1] + p[3];
                break;
            default:
                rect.x = p[0] + p[2];
                rect.y = p[1] + p[3];
                break;
            }
        } else {
            return 1;
        }
        p += 4;
        rect.w = *p++;
        rect.h = *p++;
        LoadImage(&rect, (u_long *)p);
        p += rect.w * rect.h;
    }
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002DDE4);
#endif

/* The shared unpack buffer. */
u8 *func_8002DFE0(void) {
    return D_8006FAF0;
}

extern s32 D_800500F8;
extern s32 D_800500FC;

void func_8002DFF0(s32 a, s32 b) {
    D_800500FC = (b - 1) << 16;
    D_800500F8 = a;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002E010);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002E448);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002E64C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002E8B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002EAB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002ED20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002EEF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002F0E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002F2E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002F4B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002F6B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002F8D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002FAE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002FCFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8002FF0C);

/* Default morph channel update: step the weight toward the target by the
 * step, without overshooting. Returns the weight. */
s32 func_8003014C(MorphChannel *channel) {
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

/* Copy the vertices listed in `indices` (last first) from `in` to `out`. */
void func_800301C8(SVECTOR *out, SVECTOR *in, s32 count, s16 *indices) {
    s32 i;
    s32 k;

    for (i = count - 1; i != -1; i--) {
        k = indices[i];
        out[k].vx = in[k].vx;
        out[k].vy = in[k].vy;
        out[k].vz = in[k].vz;
    }
}

/* Add `count` morph deltas times `weight` (4.12) to their vertices. */
void func_80030228(SVECTOR *vertices, MorphDelta *deltas, s32 count, s32 weight) {
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

/* Add `count` morph deltas times `weight` (4.12) to their normals and
 * renormalize them. */
void func_800302D4(SVECTOR *normals, MorphDelta *deltas, s32 count, s32 weight) {
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

/* Start morphing a sprite model: keep its vertex (and normal) arrays in a
 * new morph state and give the model copies to morph; every channel starts
 * at weight 0 with the default update. NULL when the model has no morphs. */
MorphState *func_800303C8(SpriteModel *model, s32 mode) {
    MorphTable *table = model->morphs;
    MorphState *state;
    MorphChannel *channel;
    s32 i;

    if (table == NULL) {
        return NULL;
    }
    func_800324B8(0x2B);
    state = func_80031BDC((table->count << 5) | 0x14, mode);
    state->model = model;
    state->vertices = model->vertices;
    state->normals = model->normals;
    state->channels = channel = (MorphChannel *)(state + 1);
    state->count = table->count;
    func_800324B8(0x2C);
    model->vertices = func_80031BDC(model->vertex_count * 8, mode);
    i = model->vertex_count;
    while (--i != -1) {
        model->vertices[i].vx = state->vertices[i].vx;
        model->vertices[i].vy = state->vertices[i].vy;
        model->vertices[i].vz = state->vertices[i].vz;
    }
    if (model->flags & 0x10) {
        func_800324B8(0x2D);
        model->normals = func_80031BDC(model->vertex_count * 8, mode);
        i = model->vertex_count;
        while (--i != -1) {
            model->normals[i].vx = state->normals[i].vx;
            model->normals[i].vy = state->normals[i].vy;
            model->normals[i].vz = state->normals[i].vz;
        }
    }
    for (i = 0; i < state->count; channel++, i++) {
        channel->update = func_8003014C;
        channel->target = 0;
        channel->weight = 0;
        channel->step = 0;
    }
    return state;
}

/* Morph a sprite model: restore the touched vertices, then add each
 * target's deltas at the weight its channel update returns (and to the
 * normals). */
void func_800305D8(MorphState *state) {
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
        func_800301C8(model->vertices, state->vertices, target[count].count, target[count].deltas);
        for (i = 0; i < count; channel++, i++) {
            weight = channel->update(channel);
            func_80030228(model->vertices, target[i].deltas, target[i].count, weight);
            if (model->flags & 0x10) {
                func_800302D4(model->normals, target[i].normals, target[i].count, weight);
            }
        }
    }
}

/* Stop morphing: free the model's morphed copies, give it back its own
 * vertex and normal arrays and free the state. */
void func_800306D0(MorphState *state) {
    SpriteModel *model;

    if (state != NULL) {
        model = state->model;
        func_800320E8(model->vertices);
        if (model->flags & 0x10) {
            func_800320E8(model->normals);
        }
        model->vertices = state->vertices;
        model->normals = state->normals;
        func_800320E8(state);
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80030750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80030988);

/* Set light `index` (0-2): its row of the light direction matrix is the
 * normalized reverse of the light's vector and its color a column of the
 * light color matrix, which is loaded into the GTE. */
void func_80030A30(u16 index, ModelLight *light) {
    VECTOR reverse;

    reverse.vx = -light->vx;
    reverse.vy = -light->vy;
    reverse.vz = -light->vz;
    VectorNormalS(&reverse, (SVECTOR *)D_80059F64.m[index]);
    D_80059F84.m[0][index] = light->r;
    D_80059F84.m[1][index] = light->g;
    D_80059F84.m[2][index] = light->b;
    gte_SetColorMatrix(&D_80059F84);
}

/* Load the GTE light matrix: the light directions turned by `rotation`. */
void func_80030B14(MATRIX *rotation) {
    MATRIX light;

    gte_MulMatrix0(&D_80059F64, rotation, &light);
    gte_SetLightMatrix(&light);
}

/* Set the GTE background color from 16-bit color components. */
void func_80030C40(u16 r, u16 g, u16 b) {
    gte_SetBackColor(r >> 4, g >> 4, b >> 4);
}

/* Set the GTE background color. */
void func_80030C78(s32 r, s32 g, s32 b) {
    gte_SetBackColor(r, g, b);
}

/* Render `count` flat quads lit from the lit-color cache: transform the
 * four vertices, drop quads with a GTE error or facing away, fill a
 * POLY_F4 and link it into the ordering table at its average depth.
 * Nonmatching: the loop counter and the face index pointer (the original
 * addresses the last two indices from +4) swap registers. */
#ifdef NON_MATCHING
void func_80030C98(QuadFace *faces, s32 count) {
    POLY_F4 *poly;
    s32 flag;
    s32 sz0, sz1, sz2, sz3;
    s32 otz;
    s32 i;

    for (i = count - 1; i != -1; i--) {
        poly = (POLY_F4 *)D_80059424;
        gte_ldv3(&D_8005953C[faces->v01 & 0xFFFF], &D_8005953C[faces->v01 >> 16],
                 &D_8005953C[faces->v2]);
        gte_rtpt();
        gte_stflg(&flag);
        if (flag >= 0) {
            gte_nclip();
            gte_stopz(&flag);
            if (flag > 0) {
                gte_stsxy3(&poly->x0, &poly->x1, &poly->x2);
                gte_stsz3(&sz0, &sz1, &sz2);
                gte_ldv0(&D_8005953C[faces->v3]);
                gte_rtps();
                gte_stsxy(&poly->x3);
                gte_stsz(&sz3);
                gte_ldsz4(sz0, sz1, sz2, sz3);
                gte_avsz4();
                gte_stotz(&otz);
                gte_ldrgb(D_80059498);
                gte_ldv0(D_80059498 + 1);
                gte_nccs();
                gte_strgb(&poly->r0);
                setlen(poly, 5);
                setcode(poly, 0x28);
                otz >>= D_80050100;
                addPrim(&D_80059568[otz], poly);
                D_80059578++;
            }
        }
        faces++;
        D_80059424 = (RenderPacket *)((u8 *)D_80059424 + sizeof(POLY_F4));
        D_80059498 += 3;
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80030C98);
#endif

/* Perspective-transform the three loaded vertices. Nonzero when one of
 * them has a usable depth and lands inside the screen (8002DFF0). */
s32 func_80030EE8(void) {
    s32 sz0, sz1, sz2;
    u32 sxy;

    gte_rtpt();
    gte_stsz3(&sz0, &sz1, &sz2);
    gte_stsxy0(&sxy);
    if ((u16)(sz0 + 1) >= 2 && sxy < D_800500FC && (sxy & 0xFFFF) < D_800500F8) {
        return 1;
    }
    gte_stsxy1(&sxy);
    if ((u16)(sz1 + 1) >= 2 && sxy < D_800500FC && (sxy & 0xFFFF) < D_800500F8) {
        return 1;
    }
    gte_stsxy2(&sxy);
    if ((u16)(sz2 + 1) >= 2 && sxy < D_800500FC && (sxy & 0xFFFF) < D_800500F8) {
        return 1;
    }
    return 0;
}

/* Halfway from a to b. */
#define HALFWAY(a, b) ((a) + ((b) - (a)) / 2)

/* Whether a sprite model's bounding box is off screen: none of the
 * triangles tested reaches the screen (80030EE8). Mode bit 0 tests the box
 * diagonal and three faces' diagonals, bit 1 four triangles through the
 * edge midpoints. */
s32 func_8003101C(model, mode)
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
        if (func_80030EE8()) {
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
        if (func_80030EE8()) {
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
        if (func_80030EE8()) {
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
        if (func_80030EE8()) {
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
        if (func_80030EE8()) {
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
        if (func_80030EE8()) {
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
        if (func_80030EE8()) {
            return 0;
        }
    }
    return 1;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_800315A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_800315C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_800315E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8003160C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80031630);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80031654);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80031678);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8003169C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_800316C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_800316E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80031708);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8003172C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80031750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80031774);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80031798);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_800317BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_800317E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80031804);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80031828);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_8003184C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002C3E8", func_80031870);
