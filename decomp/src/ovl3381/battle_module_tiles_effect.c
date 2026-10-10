/* ovl3381: the default battle module at 0x801fc000 (27 identical copies on
 * the disc). The battle overlay's 800beb04 loads the module of the current
 * battle into 0x801fc000..0x80200000 (the slot above the resident heap, which
 * the boot code bounds at 0x801fc000): file (sprite_requested_battle_module + 2) of the battle
 * directory, sprite_requested_battle_module being the platform bits (6..11) of the directory
 * header of battle file 2, whenever they differ from the loaded module's
 * (sprite_loaded_battle_module). Battle script opcodes call fixed entry addresses in the
 * loaded module. This one provides the battle-entry effect: the captured
 * screen broken into triangles (battle_module_tiles_start starts it; no caller is known
 * from the overlays, so it is reached by address).
 *
 * The module is one unit, its whole file (801fc000-801fc728), built by the
 * Cygnus CDK GCC 2.7.2 with a later ASPSX (see ovl3381.mk). */
#include "common.h"
#include "psyq/abs.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "battle/area.h"
#include "tiles.h"

/* The two triangles of a cell (as in ovl3387's burst effect); this module
 * keeps them but never reads them. */
SVECTOR battle_module_tiles_unused_upper_left_triangle[3] = {{-2, -2, 0}, {6, -2, 0}, {-2, 6, 0}}; /* 801FC6F8 */
SVECTOR battle_module_tiles_unused_lower_right_triangle[3] = {{2, -6, 0}, {2, 2, 0}, {-6, 2, 0}}; /* 801FC710 */

/* 801FC000: Count the effect's frames; select (0x100) newly pressed on the second
 * controller ends it. */
void battle_module_tiles_update(Task *node) {
    ((TileTask *)node->data)->frame++;
    if (battle_area.pressed2 & 0x100) {
        node->destroy(node);
    }
}

/* 801FC064: Queue every triangle of the current display buffer at a fixed 64,64
 * offset. */
void battle_module_tiles_draw(Task *node) {
    TileTask *task;
    Tile *tile;
    POLY_FT3 *prim;
    long ofx, ofy;
    s32 h;
    s32 half, row, column;

    task = node->data;
    ReadGeomOffset(&ofx, &ofy);
    h = ReadGeomScreen();
    SetGeomOffset(64, 64);
    SetGeomScreen(0x200);
    {
        SVECTOR offset;
        s32 unused[10]; /* unused in the original; reserves 40 bytes */
        offset.vy = offset.vx = 64;
        for (half = 0; half != 2; half++) {
            for (row = 0; row < 16; row++) {
                for (column = 0; column < 16; column++) {
                    tile = &task->tiles[half][row][column];
                    prim = &tile->prim[battle_area.buffer];
                    prim->x0 = tile->corner[0].vx + offset.vx;
                    prim->y0 = tile->corner[0].vy + offset.vy;
                    prim->x1 = tile->corner[1].vx + offset.vx;
                    prim->y1 = tile->corner[1].vy + offset.vy;
                    prim->x2 = tile->corner[2].vx + offset.vx;
                    prim->y2 = tile->corner[2].vy + offset.vy;
                    prim->x0 = tile->spread[0].vx + offset.vx;
                    prim->y0 = tile->spread[0].vy + offset.vy;
                    prim->x1 = tile->spread[1].vx + offset.vx;
                    prim->y1 = tile->spread[1].vy + offset.vy;
                    prim->x2 = tile->spread[2].vx + offset.vx;
                    prim->y2 = tile->spread[2].vy + offset.vy;
                    AddPrim((u32 *)sprite_ot, prim);
                }
            }
        }
    }
    SetGeomOffset(ofx, ofy);
    SetGeomScreen(h);
}

/* 801FC278: Unlink the effect's nodes, release it and restore the depth shift. */
void battle_module_tiles_destroy(Task *node) {
    task_unlink_draw_node((Task *)((u8 *)node + 0x1C));
    task_unlink_main_node(node);
    sprite_queue_free_later((u32)node);
    model_ot_depth_shift = 4;
}

/* 801FC2C0: Start the effect: build both triangle halves of every 8x8 cell of the
 * 128x128 area around the screen centre, textured from the displayed buffer,
 * with each corner also pushed out onto the circle of its larger coordinate
 * (the square grid mapped onto a disc). */
void battle_module_tiles_start(void) {
    TileTask *task;
    Tile *tile;
    POLY_FT3 *prim;
    s32 i, column, row, half;
    s32 radius, angle;
    u8 u;

    model_ot_depth_shift = 0;
    task = (TileTask *)task_alloc_two_node_task(sizeof(TileTask), NULL, battle_module_tiles_update, battle_module_tiles_draw, battle_module_tiles_destroy);
    task->frame = 0;
    for (half = 0; half != 2; half++) {
        for (row = 0; row < 16; row++) {
            for (column = 0; column < 16; column++) {
                tile = &task->tiles[half][row][column];
                if (half == 0) {
                    tile->corner[0].vx = column * 8 - 64;
                    tile->corner[0].vy = row * 8 - 64;
                    tile->corner[0].vz = 0x200;
                    tile->corner[1].vx = column * 8 - 56;
                    tile->corner[1].vy = row * 8 - 64;
                    tile->corner[1].vz = 0x200;
                    tile->corner[2].vx = column * 8 - 64;
                    tile->corner[2].vy = (row + 1) * 8 - 64;
                    tile->corner[2].vz = 0x200;
                } else {
                    tile->corner[0].vx = column * 8 - 56;
                    tile->corner[0].vy = row * 8 - 64;
                    tile->corner[0].vz = 0x200;
                    tile->corner[2].vx = column * 8 - 64;
                    tile->corner[2].vy = (row + 1) * 8 - 64;
                    tile->corner[2].vz = 0x200;
                    tile->corner[1].vx = column * 8 - 56;
                    tile->corner[1].vy = (row + 1) * 8 - 64;
                    tile->corner[1].vz = 0x200;
                }
                for (i = 0; i != 3; i++) {
                    radius = ABS(abs(tile->corner[i].vx) > abs(tile->corner[i].vy)
                                     ? tile->corner[i].vx
                                     : tile->corner[i].vy);
                    angle = ratan2(tile->corner[i].vy, tile->corner[i].vx);
                    tile->spread[i].vx = gpu_get_cos(angle) * radius / 4096;
                    tile->spread[i].vy = gpu_get_sin(angle) * radius / 4096;
                }
                for (i = 0; i != 2; i++) {
                    prim = &tile->prim[i];
                    SetPolyFT3(prim);
                    prim->tpage = i != 0 ? GetTPage(2, 0, column * 8, 0) : GetTPage(2, 0, column * 8, 0xE0);
                    prim->r0 = 0x80;
                    prim->g0 = 0xA0;
                    prim->b0 = 0x80;
                    prim->tpage = GetTPage(2, 0, column * 8, (1 - battle_area.buffer) * 0xE0);
                    u = (column * 8) & 0x3F;
                    if (half == 0) {
                        prim->u0 = u;
                        prim->v0 = row * 8;
                        prim->u1 = u + 8;
                        prim->v1 = row * 8;
                        prim->u2 = u;
                        prim->v2 = row * 8 + 8;
                    } else {
                        prim->u0 = u + 8;
                        prim->v0 = row * 8;
                        prim->u1 = u + 8;
                        prim->v1 = row * 8 + 8;
                        prim->u2 = u;
                        prim->v2 = row * 8 + 8;
                    }
                }
            }
        }
    }
}
