/* Stage setup (rodata 801E4000-801E4020, text 801E70E8-801E7F4C): the stage
 * image list bounds, its images and model, its objects (panoramas, the
 * backdrop, fog, texture scrolls) and the scene's points and triangles. A
 * GCC 2.6.3 unit between the CDK units battle_loader.c and load_modes.c
 * (ovl2615.mk); its rodata opens the image. */
#include "stage.h"

/* Relocate the stage image list and take the bounds of its pixel sections
 * (kind 0x1101: position, offset, size); returns the bounds' area. */
s32 func_801E70E8(s32 *images) {
    s32 left;
    s32 top;
    s32 right;
    s32 bottom;
    s32 count;
    s32 i;
    u16 *p;
    s32 x;
    s32 y;
    s16 width;
    s16 height;

    text_relocate_offset_table(images);
    left = 0x800;
    top = 0x800;
    right = -0x800;
    bottom = -0x800;
    count = images[0];
    for (i = 0; i < count; i++) {
        p = (u16 *)images[i + 1];
        if (*p == 0x1101) {
            p += 2;
            x = *p++;
            y = *p++;
            x += *p++;
            y += *p++;
            if (x < left) {
                left = x;
            }
            if (y < top) {
                top = y;
            }
            x += p[0];
            y += p[1];
            if (right < x) {
                right = x;
            }
            if (bottom < y) {
                bottom = y;
            }
        }
    }
    width = right - left;
    height = bottom - top;
    D_800D2D30 = left;
    D_800D2D34 = top;
    D_800D2D2C = width;
    D_800C3EA8 = height;
    return width * height;
}

/* Set up the battle stage: register the stage model and place its parts,
 * move the scene data into its own block, start the stage motion, take the
 * origin and colour matrix, build the stage objects (panoramas, backdrop,
 * fog, texture scrolls) and publish the scene's points and triangles. Returns whether
 * the stage has fog (its colour goes to tint); 0 without a stage or scene. */
/* The relocated section pointers stay NULL for a zero offset (the original
 * converts these tests to masks), and a failed allocation returns without a
 * value: the original leaves the allocator's NULL in v0. */
u8 func_801E7210(BattleScene **scene, s32 unused, StageFile *stage, s16 *origin, s16 *colours,
                 u8 *tint) {
    BattleScene *data;
    StageInfo *info;
    StageObject *object;
    ModelPart *part;
    PartPosition *position;
    s32 *table;
    u8 *motions[4];
    u8 *first_motion;
    u16 motion_count;
    s32 *triangle_count;
    SVECTOR *points;
    u8 *pointer;
    SceneTriangle *triangles;
    s32 size;
    s32 i;
    s32 j;
    s32 made;
    s32 placed;
    u8 fog;

    data = *scene;
    if (stage == NULL || scene == NULL || data == NULL) {
        return 0;
    }
    heap_select_owner_tag(4, 0);
    D_800C3E38 = NULL;
    D_800C3EA0 = NULL;
    D_800D3344 = NULL;
    D_800D39CC = NULL;
    D_800D3348 = 0;
    for (i = 1; i >= 0; i--) {
        D_800C3D50[i] = NULL;
    }
    for (j = 0; j < 2; j++) {
        D_800C3DA0[j].phases = NULL;
    }
    D_800D361A = 0;
    if (stage != NULL) {
        heap_unprotect_block(stage);
        func_800A8BF0(0x1F, 0xC4, stage, stage, 0, 0, 0, 0, 0);
        func_801E70E8(stage->images);
        position = stage->positions;
        part = D_800D3368[STAGE_MODEL]->hierarchy;
        D_800C3E38 = part;
        D_800C3E48 = D_800D3368[STAGE_MODEL]->field0;
        for (i = 1; i < part->index; i++, position++) {
            part[i].translation[0] = position->x;
            part[i].translation[1] = position->y;
            part[i].translation[2] = position->z;
            part[i].field52 = position->rotation;
        }
    }
    table = (s32 *)data;
    size = table[-1];
    data = heap_alloc(size, 0);
    mode_battle_scene_data = (u8 *)data;
    if (data == NULL) {
        return; /* no value: v0 still holds the NULL block */
    }
    memcpy(data, table, size);
    heap_unprotect_block(table - 1);
    heap_free(table - 1);
    made = 0;
    placed = 0;
    size = data->points;
    pointer = (u8 *)data + size;
    points = NULL;
    if (size != 0) {
        points = (SVECTOR *)pointer;
    }
    size = data->triangles;
    pointer = (u8 *)data + size + 4; /* the triangles follow the count */
    triangle_count = (s32 *)((u8 *)data + size);
    triangles = NULL;
    if (size != 0) {
        triangles = (SceneTriangle *)pointer;
    }
    /* Reset from the list payload, then use its first relocated entry. */
    size = data->motion;
    pointer = (u8 *)data + size;
    text_relocate_offset_table(pointer);
    info = &data->info;
    table = (s32 *)((s32 *)pointer)[1];
    if (stage != NULL) {
        func_800AA898(D_800D3368[STAGE_MODEL], &D_800C3D0C, (s32 *)pointer + 2, 0);
        func_800AA934(D_800D3368[STAGE_MODEL], D_800D3368[STAGE_MODEL], &D_800C3D0C, 0);
        func_8009EF3C(D_800C3E38, D_800D3368[STAGE_MODEL]->scale1C);
    }
    for (i = 0; i < 4; i++) {
        size = *table;
        pointer = (u8 *)table + size;
        motions[i] = pointer;
    }
    /* Read the header before the output matrix stores, as the original does. */
    first_motion = motions[0];
    motion_count = *(u16 *)first_motion;
    origin[0] = data->origin[0];
    origin[1] = data->origin[1];
    origin[2] = data->origin[2];
    origin[3] = 0;
    origin[4] = 0;
    origin[5] = 0;
    origin[6] = 0;
    origin[7] = 0;
    origin[8] = 0;
    colours[0] = data->colours[0];
    colours[1] = 0;
    colours[2] = 0;
    colours[3] = data->colours[1];
    colours[4] = 0;
    colours[5] = 0;
    colours[6] = data->colours[2];
    colours[7] = 0;
    colours[8] = 0;
    D_800D2FD0 = (u16 *)(first_motion + 2);
    D_800D2FC8 = motion_count;
    D_800D2FC0 = (MATRIX *)colours;
    SetColorMatrix((MATRIX *)colours);
    SetBackColor(data->back[0], data->back[1], data->back[2]);
    for (i = 0; i < 6; i++) {
        object = &info->objects[i];
        switch (object->type) {
        case 0:
            break;
        case 1:
            if (D_800C3D50[made] == NULL && made < 2) {
                D_800C3D50[made] = gpu_create_panorama(object->v10, object->v12, object->v14, object->v16,
                    object->v1A, object->v1C, object->v1E, object->v20, object, NULL, 0,
                    object->v24, object->v26);
            }
            made++;
            break;
        case 2:
            if (D_800C3D50[made] == NULL && made < 2) {
                D_800C3D50[made] = gpu_create_panorama(object->v10, object->v12, object->v14, object->v16,
                    object->v1A, object->v1C, object->v1E, object->v20, object, info->fogColour,
                    object->v22, object->v24, object->v26);
            }
            made++;
            break;
        case 3:
            if (D_800C3EA0 == NULL) {
                D_800C3EA0 = func_801E7914(object->v10, object->v12, info->backdrop[2],
                    info->backdrop[3], object->v14, object->v1E, object->v16, object->v1A,
                    object->v1C, info->backdrop[0], info->backdrop[1], (VECTOR *)object,
                    (CVECTOR *)info->fogColour, object->v20, object->v22);
            }
            break;
        case 5:
            info->fog = 1;
            break;
        case 7:
            if (D_800C3DA0[placed].phases == NULL && placed < 2 && i + 1 < 4) {
                gpu_init_texture_scroll(&D_800C3DA0[placed], object->v10, object->v12, object->v14,
                    object->v16, object->v20, object->v1A, object->v1C, motions[i + 1]);
            }
            placed++;
            break;
        }
    }
    fog = info->fog;
    if (fog != 0 && tint != NULL) {
        tint[0] = info->fogColour[0];
        tint[1] = info->fogColour[1];
        tint[2] = info->fogColour[2];
    }
    if (points != NULL && triangles != NULL) {
        func_801E7EC4(points, triangles, *triangle_count);
    }
    for (i = 0; i < 4; i++) {
        D_800D2D10[i] = ((BattleScene *)mode_battle_scene_data)->info.flags[i];
    }
    DrawSync(0);
    heap_free(stage);
    *scene = data;
    return fog;
}

/* Build the stage backdrop: its placement, the 9 x 9 floor grid spaced by
 * step, the tiles' texture page and palette, the fills and fades in the
 * given colours, and the draw modes. Returns NULL without memory. */
StageBackdrop *func_801E7914(s16 texX, s16 texY, s16 width, s16 height, s16 size, s16 step,
                             s16 v0A, s16 clutX, s16 clutY, s16 v10, s16 v12, VECTOR *position,
                             CVECTOR *colour, s16 v0C, s16 v0E) {
    DRAWENV env;
    RECT window;
    StageBackdrop *backdrop;
    s32 i;
    s32 j;
    s32 k;

    heap_select_owner_tag(4, 0);
    backdrop = heap_alloc(sizeof(StageBackdrop), 0);
    if (backdrop != NULL) {
        GetDrawEnv(&env);
        backdrop->x = 0;
        backdrop->y = 0;
        backdrop->width = width;
        backdrop->height = height;
        backdrop->v08 = size;
        backdrop->v0A = v0A;
        backdrop->v0C = v0C;
        backdrop->v0E = v0E;
        backdrop->v10 = v10;
        backdrop->v12 = v12;
        backdrop->position[0] = position->vx;
        backdrop->position[1] = position->vy;
        backdrop->position[2] = position->vz;
        for (i = 0; i < 2; i++) {
            backdrop->colours[i].r = colour[i].r;
            backdrop->colours[i].g = colour[i].g;
            backdrop->colours[i].b = colour[i].b;
        }
        k = 0;
        for (i = 0; i < 9; i++) {
            for (j = 0; j < 9; j++) {
                backdrop->grid[k].vx = j * step - step * 4;
                backdrop->grid[k].vy = 0;
                backdrop->grid[k].vz = i * step - step * 4;
                k++;
            }
        }
        for (i = 0; i < 2; i++) {
            SetPolyF4(&backdrop->fills[i]);
            backdrop->fills[i].r0 = colour->r;
            backdrop->fills[i].g0 = colour->g;
            backdrop->fills[i].b0 = colour->b;
            backdrop->fills[i].x0 = 0;
            backdrop->fills[i].y0 = 0;
            backdrop->fills[i].x1 = 320;
            backdrop->fills[i].y1 = 0;
            backdrop->fills[i].x2 = 0;
            backdrop->fills[i].x3 = 320;
        }
        for (i = 0; i < 2; i++) {
            SetPolyG4(&backdrop->fades[i]);
            SetSemiTrans(&backdrop->fades[i], 1);
            backdrop->fades[i].r2 = 0;
            backdrop->fades[i].g2 = 0;
            backdrop->fades[i].b2 = 0;
            backdrop->fades[i].r3 = 0;
            backdrop->fades[i].g3 = 0;
            backdrop->fades[i].b3 = 0;
            backdrop->fades[i].r0 = 255 - colour->r;
            backdrop->fades[i].g0 = 255 - colour->g;
            backdrop->fades[i].b0 = 255 - colour->b;
            backdrop->fades[i].r1 = 255 - colour->r;
            backdrop->fades[i].g1 = 255 - colour->g;
            backdrop->fades[i].b1 = 255 - colour->b;
            backdrop->fades[i].x0 = 0;
            backdrop->fades[i].x1 = 320;
            backdrop->fades[i].x2 = 0;
            backdrop->fades[i].x3 = 320;
        }
        colour++;
        for (i = 2; i < 4; i++) {
            SetPolyG4(&backdrop->fades[i]);
            backdrop->fades[i].r0 = colour->r;
            backdrop->fades[i].g0 = colour->g;
            backdrop->fades[i].b0 = colour->b;
            backdrop->fades[i].r1 = colour->r;
            backdrop->fades[i].g1 = colour->g;
            backdrop->fades[i].b1 = colour->b;
            colour++;
            backdrop->fades[i].r2 = colour->r;
            backdrop->fades[i].g2 = colour->g;
            backdrop->fades[i].b2 = colour->b;
            backdrop->fades[i].r3 = colour->r;
            backdrop->fades[i].g3 = colour->g;
            backdrop->fades[i].b3 = colour->b;
            colour--;
            backdrop->fades[i].x0 = 0;
            backdrop->fades[i].x1 = 320;
            backdrop->fades[i].x2 = 0;
            backdrop->fades[i].x3 = 320;
        }
        colour++;
        for (i = 2; i < 4; i++) {
            SetPolyF4(&backdrop->fills[i]);
            backdrop->fills[i].r0 = colour->r;
            backdrop->fills[i].g0 = colour->g;
            backdrop->fills[i].b0 = colour->b;
            backdrop->fills[i].x0 = 0;
            backdrop->fills[i].x1 = 320;
            backdrop->fills[i].x2 = 0;
            backdrop->fills[i].y2 = 240;
            backdrop->fills[i].x3 = 320;
            backdrop->fills[i].y3 = 240;
        }
        for (i = 0; i < 128; i++) {
            SetPolyFT4(&backdrop->tiles[i]);
            SetShadeTex(&backdrop->tiles[i], 1);
            backdrop->tiles[i].clut = GetClut(clutX, clutY);
            backdrop->tiles[i].tpage = GetTPage(0, 1, texX / 64 * 64, texY / 256 * 256);
        }
        for (i = 0; i < 2; i++) {
            window.x = texX % 64;
            window.y = texY % 256;
            window.w = size;
            window.h = size;
            SetDrawMode(&backdrop->modes[i], env.dfe, env.dtd, GetTPage(0, 1, 0, 0), &window);
        }
        for (i = 2; i < 4; i++) {
            SetDrawMode(&backdrop->modes[i], env.dfe, env.dtd, GetTPage(0, 1, 0, 0), &env.tw);
        }
    }
    return backdrop;
}

/* Register the scene's points and triangles (the battle's ground geometry,
 * battle/scene.h); clear each triangle's visit stamp. Without triangles both
 * pointers are cleared. */
void func_801E7EC4(SVECTOR *points, SceneTriangle *triangles, s32 count) {
    s32 i;

    D_800D3344 = points;
    D_800D39CC = triangles;
    D_800D3348 = count;
    D_800D2F64 = 1;
    if (triangles != NULL) {
        for (i = 0; i < D_800D3348; i++) {
            D_800D39CC[i].visited = 0;
        }
    }
    if (count == 0) {
        D_800D3344 = NULL;
        D_800D39CC = NULL;
    }
}
