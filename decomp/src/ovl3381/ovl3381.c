/* ovl3381: the default battle module at 0x801fc000 (27 identical copies on
 * the disc). The battle overlay's 800beb04 loads the module of the current
 * battle into 0x801fc000..0x80200000 (the slot above the resident heap, which
 * the boot code bounds at 0x801fc000): file (D_800591B3 + 2) of the battle
 * directory, D_800591B3 being the platform bits (6..11) of the directory
 * header of battle file 2, whenever they differ from the loaded module's
 * (D_800591B2). Battle script opcodes call fixed entry addresses in the
 * loaded module. This one provides the battle-entry effect: the captured
 * screen broken into triangles (func_801FC2C0 starts it; no caller is known
 * from the overlays, so it is reached by address).
 *
 * The module was built by the Cygnus CDK GCC 2.7.2 with a later ASPSX
 * (see ovl3381.mk). */
#include "psyq/libc.h"
#include "tiles.h"

#define ABS(x) ((x) >= 0 ? (x) : -(x))

/* Count the effect's frames; the battle's flag 0x100 ends it. */
void func_801FC000(TaskNode *node) {
    ((TileTask *)node->object)->frame++;
    if (D_800C3EB0.flags & 0x100) {
        node->destroy(node);
    }
}

/* Queue every triangle of the current display buffer at a fixed 64,64
 * offset. */
/* NON_MATCHING: the compiled frame is 0x50; the original reserves 0x78. */
#ifdef NON_MATCHING
void func_801FC064(TaskNode *node) {
    TileTask *task;
    Tile *tile;
    POLY_FT3 *prim;
    s32 ofx, ofy;
    s32 h;
    s32 half, row, column;

    task = node->object;
    ReadGeomOffset(&ofx, &ofy);
    h = ReadGeomScreen();
    SetGeomOffset(64, 64);
    SetGeomScreen(0x200);
    {
        SVECTOR offset;
        offset.vy = offset.vx = 64;
        for (half = 0; half != 2; half++) {
            for (row = 0; row < 16; row++) {
                for (column = 0; column < 16; column++) {
                    tile = &task->tiles[half][row][column];
                    prim = &tile->prim[D_800C3EB0.buffer];
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
                    AddPrim(D_8005956C, prim);
                }
            }
        }
    }
    SetGeomOffset(ofx, ofy);
    SetGeomScreen(h);
}

#else
INCLUDE_ASM(".local/decomp/ovl3381/asm/nonmatchings/ovl3381", func_801FC064);
#endif

/* Unlink the effect's nodes, release it and restore the depth shift. */
void func_801FC278(TaskNode *node) {
    func_8001CB48((TaskNode *)((u8 *)node + 0x1C));
    func_8001CD94(node);
    func_80025180(node);
    D_80050100 = 4;
}

/* Start the effect: build both triangle halves of every 8x8 cell of the
 * 128x128 area around the screen centre, textured from the displayed buffer,
 * with each corner also pushed out onto the circle of its larger coordinate
 * (the square grid mapped onto a disc).
 * NON_MATCHING: the row loop strength-reduces row * 8 (for v) into its own
 * induction slot where the original recomputes it from the row counter and
 * keeps y0/y1 as copies of theirs; the rest follows from that allocation. */
#ifdef NON_MATCHING
void func_801FC2C0(void) {
    TileTask *task;
    Tile *tile;
    POLY_FT3 *prim;
    s32 i, column, row, half;
    s32 y1, x1, y0, x0;
    s32 radius, angle;
    u8 u;

    D_80050100 = 0;
    task = func_8001D1D8(sizeof(TileTask), NULL, func_801FC000, func_801FC064, func_801FC278);
    task->frame = 0;
    for (half = 0; half != 2; half++) {
        for (row = 0; row < 16; row++) {
            y0 = row * 8 - 64;
            y1 = y0 + 8;
            for (column = 0; column < 16; column++) {
                x0 = column * 8 - 64;
                x1 = x0 + 8;
                tile = &task->tiles[half][row][column];
                if (half == 0) {
                    tile->corner[0].vx = x0;
                    tile->corner[0].vy = y0;
                    tile->corner[0].vz = 0x200;
                    tile->corner[1].vx = x1;
                    tile->corner[1].vy = y0;
                    tile->corner[1].vz = 0x200;
                    tile->corner[2].vx = x0;
                    tile->corner[2].vy = y1;
                    tile->corner[2].vz = 0x200;
                } else {
                    tile->corner[0].vx = x1;
                    tile->corner[0].vy = y0;
                    tile->corner[0].vz = 0x200;
                    tile->corner[1].vx = x1;
                    tile->corner[1].vy = y1;
                    tile->corner[1].vz = 0x200;
                    tile->corner[2].vx = x0;
                    tile->corner[2].vy = y1;
                    tile->corner[2].vz = 0x200;
                }
                for (i = 0; i != 3; i++) {
                    radius = ABS(abs(tile->corner[i].vx) > abs(tile->corner[i].vy)
                                     ? tile->corner[i].vx
                                     : tile->corner[i].vy);
                    angle = ratan2(tile->corner[i].vy, tile->corner[i].vx);
                    tile->spread[i].vx = func_8003F8CC(angle) * radius / 4096;
                    tile->spread[i].vy = func_8003F8B0(angle) * radius / 4096;
                }
                for (i = 0; i != 2; i++) {
                    prim = &tile->prim[i];
                    SetPolyFT3(prim);
                    prim->tpage = i != 0 ? GetTPage(2, 0, column * 8, 0) : GetTPage(2, 0, column * 8, 0xE0);
                    prim->r0 = 0x80;
                    prim->g0 = 0xA0;
                    prim->b0 = 0x80;
                    prim->tpage = GetTPage(2, 0, column * 8, (1 - D_800C3EB0.buffer) * 0xE0);
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
#else
INCLUDE_ASM(".local/decomp/ovl3381/asm/nonmatchings/ovl3381", func_801FC2C0);
#endif
