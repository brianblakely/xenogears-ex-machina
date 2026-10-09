#include "worldmap.h"

/* Declared here only: other units call it without a prototype. */
void func_80093354(VECTOR *position);

/* Scene object draw mode, by the object's flags (func_800848F4). */
s16 D_8009AD2C[10] = {4, 4, 5, 5, 0, 0, 2, 2, 3, 3};

/* Drifting sprites (clouds, func_80086798): texture origins, the far sprite
 * corners and the near sprites' quads (four corners each), in three layers. */
u16 D_8009AD40[8] = {0, 0x40, 0x80, 0, 0xC0, 0x4000, 0x4040, 0};
SVECTOR D_8009AD50[12] = {
    {-192, 0, 192}, {192, 0, 192}, {-192, 0, -192}, {192, 0, -192},
    {-192, -8, 192}, {192, -8, 192}, {-192, -8, -192}, {192, -8, -192},
    {-192, -16, 192}, {192, -16, 192}, {-192, -16, -192}, {192, -16, -192},
};
SVECTOR D_8009ADB0[48] = {
    {-192, 0, 192}, {0, 0, 192}, {-192, 0, 0}, {0, 0, 0},
    {0, 0, 192}, {192, 0, 192}, {0, 0, 0}, {192, 0, 0},
    {-192, 0, 0}, {0, 0, 0}, {-192, 0, -192}, {0, 0, -192},
    {0, 0, 0}, {192, 0, 0}, {0, 0, -192}, {192, 0, -192},
    {-192, -8, 192}, {0, -8, 192}, {-192, -8, 0}, {0, -8, 0},
    {0, -8, 192}, {192, -8, 192}, {0, -8, 0}, {192, -8, 0},
    {-192, -8, 0}, {0, -8, 0}, {-192, -8, -192}, {0, -8, -192},
    {0, -8, 0}, {192, -8, 0}, {0, -8, -192}, {192, -8, -192},
    {-192, -16, 192}, {0, -16, 192}, {-192, -16, 0}, {0, -16, 0},
    {0, -16, 192}, {192, -16, 192}, {0, -16, 0}, {192, -16, 0},
    {-192, -16, 0}, {0, -16, 0}, {-192, -16, -192}, {0, -16, -192},
    {0, -16, 0}, {192, -16, 0}, {0, -16, -192}, {192, -16, -192},
};

/* Drift template points (func_800863E0). */
Drift D_8009AF30[5] = {
    {0xC0, 0, 0xC0}, {0x240, 0, 0x340}, {0x4C0, 0, 0x140}, {0x5C0, 0, 0x440}, {0x2C0, 0, 0x640},
};

/* Ferry waypoints (x, z). */
u16 D_8009AF80[8] = {23296, 18208, 14968, 11491, 3072, 31144, 28148, 25620};
u16 D_8009AF90[8] = {23736, 16576, 16008, 16554, 16200, 14552, 14612, 16928};

/* Scene object links (parent, child pairs; -1 ends). */
s16 D_8009AFA0[30] = {
    7, 0, 7, 1, 7, 2, 7, 3, 5, 7, 5, 4, 12, 10, 12, 5, 12, 11, 12, 13, 12, 6, 12, 9, 12, 8,
    12, 13, -1, 0,
};

/* Scene objects to show (-1 ends). */
s16 D_8009AFDC[10] = {67, 68, 70, 71, 72, 73, 74, 75, 76, -1};

/* Particle kinds (func_80089C78): four packed u,v corners and the quad shape. */
u16 D_8009AFF0[10 * 4] = {
    0, 0, 0, 0,
    0, 0x3F, 0x3F00, 0x3F3F,
    0x4000, 0x403F, 0x7F00, 0x7F3F,
    0x40, 0x5F, 0x1F40, 0x1F5F,
    0x2040, 0x205F, 0x3F40, 0x3F5F,
    0x4040, 0x405F, 0x5F40, 0x5F5F,
    0xF000, 0xF05F, 0xFF00, 0xFF5F,
    0x60, 0x7F, 0xFF60, 0xFF7F,
    0x8000, 0x801F, 0xDF00, 0xDF1F,
    0xB020, 0xB05F, 0xEF20, 0xEF5F,
};
ParticleShape D_8009B040[10] = {
    {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}},
    {{{-32, -32, 0}, {32, -32, 0}, {-32, 32, 0}, {32, 32, 0}}},
    {{{-32, -64, 0}, {32, -64, 0}, {-32, 0, 0}, {32, 0, 0}}},
    {{{-32, -32, 0}, {32, -32, 0}, {-32, 32, 0}, {32, 32, 0}}},
    {{{-32, -32, 0}, {32, -32, 0}, {-32, 32, 0}, {32, 32, 0}}},
    {{{-32, -32, 0}, {32, -32, 0}, {-32, 32, 0}, {32, 32, 0}}},
    {{{0, -8, 0}, {96, -8, 0}, {0, 8, 0}, {96, 8, 0}}},
    {{{-16, -255, 0}, {16, -255, 0}, {-16, 0, 0}, {16, 0, 0}}},
    {{{-16, -96, 0}, {16, -96, 0}, {-16, 0, 0}, {16, 0, 0}}},
    {{{-32, -32, 0}, {32, -32, 0}, {-32, 32, 0}, {32, 32, 0}}},
};

/* Per landing kind: whether the player may stand there. */
s16 D_8009B180[6] = {1, 1, 0, 1, 1, 1};

/* Per party member: the parameters func_8008C28C passes with its gear
 * model. The last table (0x140, 0x140, 0x100) ends the unit's data before a
 * stray halfword (00 3c) that nothing reads, so it stays original data
 * (worldmap.classification.txt). */
s16 D_8009B18C[3] = {0x100, 0x100, 0x100};
s16 D_8009B194[3] = {0x1FD, 0x1FC, 0x1FB};
s16 D_8009B19C[3] = {0x140, 0x160, 0x280};
INCLUDE_ORIGINAL(".data", D_8009B1A4, 0x8009B1A4, 8);

/* Scripted camera stages 1-6 around the player (commands set the angle, distance
 * and position of each stage), with easing and a random vertical shake. */
s32 func_80083A00(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;

    actor = &D_8009BE24[index];
    scratch = (ActorScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->state = 1;
        actor->unk4 = 0;
        actor->unk5C = 0x960000;
        D_8009BD38.vx = -0x20;
        D_8009BD38.vy = 0x400;
        D_8009BD38.vz = 0;
        actor->motion.vx = actor->u.step = -0x20 << 12;
        actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
        D_8009D3F0 = 0x960000;
        actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
        break;
    case 2:
        actor->state = 2;
        actor->unk4 = 0;
        actor->unk5C = 0x180000;
        D_8009BD38.vx = 0x40;
        D_8009BD38.vy = 0x1A0;
        D_8009BD38.vz = 0;
        actor->motion.vx = actor->u.step = 0x40 << 12;
        actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
        D_8009D3F0 = 0x180000;
        actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
        actor->position.vy = D_8009D55C.target.vy = D_8009BE28.target.vy = -0xA0000;
        break;
    case 3:
        actor->state = 3;
        actor->unk4 = 0;
        actor->unk5C = 0x120000;
        D_8009BD38.vx = -0x2A0;
        D_8009BD38.vy = 0x6A0;
        D_8009BD38.vz = 0;
        actor->motion.vx = actor->u.step = -0x2A0 << 12;
        D_8009D3F0 = 0x120000;
        actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
        actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
        actor->position.vx = D_8009D55C.target.vx = D_8009BE28.target.vx = 0x1700000;
        GROUND_SCROLL[0] += 0x1700000 - D_8009C5AC.vx;
        break;
    case 4:
        actor->state = 4;
        actor->unk4 = 0;
        actor->unk5C = 0x180000;
        D_8009BD38.vx = -0x10;
        D_8009BD38.vy = 0xA50;
        D_8009BD38.vz = 0;
        actor->motion.vx = actor->u.step = -0x10 << 12;
        actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
        D_8009D3F0 = 0x180000;
        actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
        actor->position.vx = D_8009D55C.target.vx = D_8009BE28.target.vx = 0x1800000;
        actor->position.vz = D_8009D55C.target.vz = D_8009BE28.target.vz = 0x1900000;
        actor->position.vy = D_8009D55C.target.vy = D_8009BE28.target.vy = -0x80000;
        GROUND_SCROLL[0] += 0x100;
        GROUND_SCROLL[2] -= 0x100;
        break;
    case 5:
        actor->state = 5;
        actor->unk4 = 0;
        actor->unk5C = 0x260000;
        D_8009BD38.vx = -0x100;
        D_8009BD38.vy = 0x670;
        D_8009BD38.vz = 0;
        actor->motion.vx = actor->u.step = -0x100 << 12;
        actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
        actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
        actor->position.vy = D_8009D55C.target.vy = D_8009BE28.target.vy = -0x110000;
        actor->position.vz = D_8009D55C.target.vz = D_8009BE28.target.vz = 0x1A80000;
        D_8009D3F0 = 0x260000;
        GROUND_SCROLL[2] += 0x180;
        break;
    case 6:
        actor->state = 6;
        actor->unk4 = 0;
        actor->unk5C = 0x620000;
        D_8009BD38.vx = -0x80;
        D_8009BD38.vy = 0xA70;
        D_8009BD38.vz = 0;
        actor->motion.vx = actor->u.step = -0x80 << 12;
        actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
        actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
        actor->position.vy = D_8009D55C.target.vy = D_8009BE28.target.vy = -0x120000;
        actor->position.vz = D_8009D55C.target.vz = D_8009BE28.target.vz = 0x1980000;
        D_8009D3F0 = 0x620000;
        GROUND_SCROLL[2] -= 0x100;
        break;
    }
    if (D_8009D144 == 0) {
        func_80096F18(D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
    }
    switch (actor->state) {
    case 0:
    case 2:
    case 3:
    case 4:
    case 6:
        break;
    case 1:
        actor->u.step = func_800771D8(actor->u.step, -0x400000, -0x8000);
        actor->unk54 = func_800771D8(actor->unk54, 0, -0x8000);
        actor->unk5C = func_800771D8(actor->unk5C, 0x380000, -0xD000);
        break;
    case 5:
        actor->u.step = func_800771D8(actor->u.step, 0x200000, 0x2000);
        break;
    }
    func_80076DA4(actor, scratch);
    func_80076F54(actor, scratch);
    func_80076FA8(actor, scratch);
    scratch->position.vy = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    ((s16 *)D_8009BD40)[1] += scratch->position.vy; /* VIEW_VECTORS[0].vy */
    VIEW_VECTORS[1].vy += scratch->position.vy;
    return 1;
}

/* Place the actor and show scene objects 7 and 8 at its position. */
s32 func_80083FE4(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->position.vx = 0x1800000;
    actor->position.vy = 0x80000;
    actor->position.vz = 0x1A00000;
    actor->state = 0;
    D_8009C620[8].visible = 1;
    D_8009C620[7].visible = 1;
    D_8009C620[7].position.vx = D_8009C620[8].position.vx = actor->position.vx >> 12;
    D_8009C620[7].position.vy = D_8009C620[8].position.vy = actor->position.vy >> 12;
    D_8009C620[7].position.vz = D_8009C620[8].position.vz = actor->position.vz >> 12;
    return 1;
}

/* Drive the lift of scene objects 7 and 8: a command (1-4) selects the
 * route state and stops the effect groups of the previous one; each state
 * lowers the actor and moves its effect groups with it. */
s32 func_80084068(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;
    SceneObject *objects;
    SceneObject *lift;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    lift = &objects[7];
    scratch = (ActorScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 1;
        objects[8].visible = 0;
        objects[7].visible = 0;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 2;
        func_800894C8(3);
        func_800894C8(5);
        func_800894C8(6);
        func_800894C8(7);
        func_800894C8(10);
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 3;
        func_800894C8(7);
        func_800894C8(8);
        func_800894C8(9);
        break;
    case 4:
        actor->unk4 = 0;
        actor->state = 4;
        func_800894C8(5);
        func_800894C8(6);
        func_800894C8(7);
        func_800894C8(10);
        func_800894C8(11);
        func_800894C8(12);
        break;
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        actor->position.vy = func_800771D8(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        func_80089160(3, &scratch->position, NULL);
        func_80089160(5, &scratch->position, NULL);
        func_80089160(6, &scratch->position, NULL);
        func_80089160(7, &scratch->position, NULL);
        func_80089160(10, &scratch->position, NULL);
        break;
    case 2:
        actor->position.vy = func_800771D8(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        func_80089160(7, &scratch->position, NULL);
        func_80089160(8, &scratch->position, NULL);
        func_80089160(9, &scratch->position, NULL);
        break;
    case 3:
        actor->position.vy = func_800771D8(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        func_80089160(5, &scratch->position, NULL);
        func_80089160(6, &scratch->position, NULL);
        func_80089160(7, &scratch->position, NULL);
        func_80089160(10, &scratch->position, NULL);
        func_80089160(11, &scratch->position, NULL);
        func_80089160(12, &scratch->position, NULL);
        break;
    case 4:
        actor->position.vy = func_800771D8(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        func_80089160(13, &scratch->position, NULL);
        scratch->position.vy = 0;
        func_80089160(14, &scratch->position, NULL);
        break;
    }
    lift[0].position.vx = lift[1].position.vx = actor->position.vx >> 12;
    lift[0].position.vy = lift[1].position.vy = actor->position.vy >> 12;
    lift[0].position.vz = lift[1].position.vz = actor->position.vz >> 12;
    return 1;
}

/* Unpack the area image to VRAM, then build 15 CLUT rows fading the
 * 0,0x1F0 CLUT row towards a pale blue and record the 16 CLUT ids. */
void func_8008440C(void) {
    RECT rect;
    CVECTOR fade = {0xE0, 0xF5, 0xFF, 0x00};
    s32 i;
    u16 *clut;
    void *source;
    void *faded;

    source = func_80032E88(D_8009BD20, 1);
    i = 0;
    func_8002DD20(source);
    DrawSync(0);
    func_800320E8(source);
    clut = D_8009BCE0;
    func_800320E8(D_8009BD20);
    source = func_80031BDC(0x200, 1);
    faded = func_80031BDC(0x2000, 1);
    rect.x = 0;
    rect.y = 0x1F0;
    rect.w = 0x100;
    rect.h = 1;
    StoreImage(&rect, source);
    DrawSync(0);
    func_800931D8(source, faded, 0x10, (u8 *)&fade);
    rect.x = 0;
    rect.y = 0x1F0;
    rect.w = 0x100;
    rect.h = 0xF;
    LoadImage(&rect, faded);
    DrawSync(0);
    do {
        i++;
        *clut = GetClut(rect.x, rect.y);
        rect.y++;
        clut++;
    } while (i < 0x10);
    func_800320E8(faded);
    func_800320E8(source);
}

/* Build the scene objects from the area's placement list: resolve the
 * animation offsets, then place, orient and build each object's
 * primitives (two buffers) from its sprite definition. */
void func_80084580(void) {
    ScenePlacement *placement;
    s32 *base;
    s32 *offsets;
    s32 i;

    i = 0;
    D_8009BD28 = func_8002C3E8(D_8009CD48);
    base = D_8009D308;
    offsets = base + 1;
    D_8009D308 = offsets;
    for (; i < D_8009BD28; i++) {
        offsets[i] = (s32)base + offsets[i];
    }
    D_8009D7E0 = *(s16 *)D_8009BD30;
    D_8009C620 = func_80031BDC(D_8009D7E0 * sizeof(SceneObject), 0);
    placement = (ScenePlacement *)((u8 *)D_8009BD30 + 2);
    SetColorMatrix(&D_8009A140);
    SetLightMatrix(&D_8009A160);
    for (i = 0; i < D_8009D7E0; i++, placement++) {
        D_8009C620[i].visible = 0;
        D_8009C620[i].unk2 = placement->def;
        D_8009C620[i].flags = placement->flags;
        D_8009C620[i].position.vx = placement->x;
        D_8009C620[i].position.vy = placement->y;
        D_8009C620[i].position.vz = -placement->z;
        D_8009C620[i].angle.vx = placement->ax;
        D_8009C620[i].angle.vy = placement->ay;
        D_8009C620[i].angle.vz = placement->az;
        func_8004A92C(&D_8009C620[i].angle, &D_8009C620[i].matrix);
        D_8009C620[i].def = &((SpriteDefTable *)D_8009CD48)->defs[D_8009C620[i].unk2];
        func_8002CB54(D_8009C620[i].def, &D_8009C620[i].prims, &D_8009C620[i].prims2, &D_8009C620[i]);
        func_8002C8CC(D_8009C620[i].def, D_8009C620[i].prims, 1);
        memcpy(D_8009C620[i].prims2, D_8009C620[i].prims, D_8009C620[i].def->size);
        D_8009C620[i].unk44 = ((s32 *)D_8009D308)[D_8009C620[i].unk2];
        ((s32 *)D_8009C620[i].unk44)[1] = D_8009C620[i].unk44 + ((s32 *)D_8009C620[i].unk44)[1];
        D_8009C620[i].parent = NULL;
    }
    D_80050100 = 2;
    D_8009C16C = -1;
    D_8009C840 = -1;
    D_8009C620[4].visible = 1;
}

/* Free the scene objects' primitives and definitions, then the objects. */
void func_80084818(void) {
    s32 i;

    for (i = 0; i < D_8009D7E0; i++) {
        func_800320E8(D_8009C620[i].prims);
        func_8002CBBC(D_8009C620[i].def);
    }
    func_800320E8(D_8009C620);
}

/* Link scene object `child` to `parent`. */
void func_800848B4(s32 parent, s32 child) {
    D_8009C620[child].parent = &D_8009C620[parent];
}

/* Draw the visible scene objects: build each one's matrix through its
 * parent chain, place it relative to the camera target, and add its sprite
 * set to the ordering table when it projects in front and near enough. */
void func_800848F4(void) {
    SceneScratch *scratch;
    SceneObject *parent;
    s32 i;
    s32 x;
    s32 z;

    scratch = (SceneScratch *)0x1F800000;
    scratch->scale.vz = 0x800;
    scratch->scale.vy = 0x800;
    scratch->scale.vx = 0x800;
    D_80050104 = 3;
    x = D_8009BE28.target.vx >> 12;
    z = D_8009BE28.target.vz >> 12;
    D_800595C0 = 0;
    D_80059578 = 0;
    scratch->origin.vz = 0;
    scratch->origin.vy = 0;
    scratch->origin.vx = 0;
    for (i = 0; i < D_8009D7E0; i++) {
        if (D_8009C620[i].visible == 0) {
            scratch->m = D_8009C620[i].matrix;
            scratch->m.t[0] = D_8009C620[i].position.vx;
            scratch->m.t[1] = D_8009C620[i].position.vy;
            scratch->m.t[2] = -D_8009C620[i].position.vz;
            parent = &D_8009C620[i];
            while (parent->parent != NULL) {
                parent = parent->parent;
                parent->matrix.t[0] = parent->position.vx;
                parent->matrix.t[1] = parent->position.vy;
                parent->matrix.t[2] = -parent->position.vz;
                gte_CompMatrix(&parent->matrix, &scratch->m, &scratch->m);
            }
            scratch->offset.vx = scratch->m.t[0] - x;
            scratch->offset.vz = -scratch->m.t[2] - z;
            func_80093534(&scratch->offset);
            scratch->m.t[0] = scratch->offset.vx;
            scratch->m.t[2] = -scratch->offset.vz;
            ScaleMatrix(&scratch->m, &scratch->scale);
            CompMatrix(&D_8009C808, &scratch->m, &scratch->out);
            SetRotMatrix(&scratch->out);
            SetTransMatrix(&scratch->out);
            gte_ldv0(&scratch->origin);
            gte_rtps();
            gte_stflg(&scratch->flag);
            if (scratch->flag >= 0) {
                gte_stsz(&scratch->sz);
                if (scratch->sz < 0xD80) {
                    func_8002C700(D_8009C620[i].def, (&D_8009C620[i].prims)[D_8009D7F0], D_8009BE3C->ot,
                                  D_8009AD2C[(s16)D_8009C620[i].flags]);
                }
            }
        }
    }
}

/* Probe the solid scene objects; the first hit's result, with its index. */
s16 func_80084D00(s32 probe, s16 *hit) {
    SceneObject *object;
    s16 i;
    s16 result;

    object = D_8009C620;
    i = 0;
    while (i < D_8009D7E0) {
        if (object->flags & 1) {
            result = func_80084DB8(probe, i);
            if (result != 0) {
                *hit = i;
                return result;
            }
        }
        i++;
        object++;
    }
    return 0;
}

/* Test the probe position against scene object `index`: transform its
 * collision faces flat (x, z) and record every face whose outline contains
 * the probe (face number and its attribute) in D_8009D718. Returns a word-sized
 * count of table entries; the caller narrows it to s16. */
s32 func_80084DB8(s32 probe, s32 index) {
    s32 flag;
    SceneObject *object;
    FaceTestScratch *scratch;
    Mesh *mesh;
    SVECTOR *vertices;
    MeshFace *face;
    s32 count;
    s32 i;
    s32 dx, dz, far;
    s32 hits;

    object = &D_8009C620[index];
    dx = (((VECTOR *)probe)->vx >> 12) - object->position.vx;
    FACE_TEST_SCRATCH->u.test.delta.vx = dx;
    dx = dx >= 0 ? dx : -dx;
    far = dx >= 0x800;
    dz = object->position.vz - (((VECTOR *)probe)->vz >> 12);
    FACE_TEST_SCRATCH->u.test.delta.vz = dz;
    dz = dz >= 0 ? dz : -dz;
    far |= dz >= 0x800;
    scratch = FACE_TEST_SCRATCH;
    if (far) {
        return 0;
    }
    scratch->m = object->matrix;
    scratch->m.t[2] = 0;
    scratch->m.t[0] = 0;
    scratch->m.t[1] = object->position.vy;
    scratch->p[0].vz = 0x800;
    scratch->p[0].vy = 0x800;
    scratch->p[0].vx = 0x800;
    hits = 0;
    ScaleMatrix(&scratch->m, &scratch->p[0]);
    SetRotMatrix(&scratch->m);
    SetTransMatrix(&scratch->m);
    mesh = (Mesh *)object->unk44;
    count = mesh->unk0;
    vertices = mesh->vertices;
    scratch->u.test.point = (scratch->u.test.delta.vz << 16) | (scratch->u.test.delta.vx & 0xFFFF);
    face = mesh->faces;
    for (i = 0; i < count; i++, face++) {
        gte_RotTrans(&vertices[face->corner[0]], &scratch->p[0], &flag);
        gte_RotTrans(&vertices[face->corner[1]], &scratch->p[1], &flag);
        gte_RotTrans(&vertices[face->corner[2]], &scratch->p[2], &flag);
        scratch->u.test.edge[0] = (scratch->p[0].vz << 16) | (scratch->p[0].vx & 0xFFFF);
        scratch->u.test.edge[1] = (scratch->p[1].vz << 16) | (scratch->p[1].vx & 0xFFFF);
        if (func_8004A70C(scratch->u.test.edge[0], scratch->u.test.edge[1], scratch->u.test.point) > 0) {
            continue;
        }
        scratch->u.test.edge[0] = (scratch->p[1].vz << 16) | (scratch->p[1].vx & 0xFFFF);
        scratch->u.test.edge[1] = (scratch->p[2].vz << 16) | (scratch->p[2].vx & 0xFFFF);
        if (func_8004A70C(scratch->u.test.edge[0], scratch->u.test.edge[1], scratch->u.test.point) > 0) {
            continue;
        }
        scratch->u.test.edge[0] = (scratch->p[2].vz << 16) | (scratch->p[2].vx & 0xFFFF);
        scratch->u.test.edge[1] = (scratch->p[0].vz << 16) | (scratch->p[0].vx & 0xFFFF);
        if (func_8004A70C(scratch->u.test.edge[0], scratch->u.test.edge[1], scratch->u.test.point) > 0) {
            continue;
        }
        D_8009D718[hits] = i;
        D_8009D718[hits + 1] = face->kind;
        hits += 2;
    }
    return hits;
}

/* Project `position` onto face `face` of scene object `index`: `offset` gets
 * the object-relative x/z, `normal` the face normal and offset->vy the
 * height of the face plane there. */
void func_80085158(VECTOR *position, VECTOR *offset, VECTOR *normal, u16 index, u16 face) {
    s32 flag;
    VECTOR *edge1;
    VECTOR *edge2;
    SceneObject *object;
    MeshFace *corners;
    SVECTOR *vertices;
    s32 y;

    object = &D_8009C620[index];
    offset->vx = (position->vx >> 12) - object->position.vx;
    offset->vz = object->position.vz - (position->vz >> 12);
    FACE_SCRATCH->m = object->matrix;
    FACE_SCRATCH->m.t[2] = 0;
    FACE_SCRATCH->m.t[0] = 0;
    y = object->position.vy;
    FACE_SCRATCH->p[1].vz = 0x800;
    FACE_SCRATCH->p[1].vy = 0x800;
    FACE_SCRATCH->p[1].vx = 0x800;
    FACE_SCRATCH->m.t[1] = y;
    edge1 = &FACE_SCRATCH->p[1];
    ScaleMatrix(&FACE_SCRATCH->m, edge1);
    SetRotMatrix(&FACE_SCRATCH->m);
    SetTransMatrix(&FACE_SCRATCH->m);
    edge2 = &FACE_SCRATCH->p[2];
    corners = ((Mesh *)object->unk44)->faces;
    corners += face;
    vertices = ((Mesh *)object->unk44)->vertices;
    func_8004A6DC(&vertices[corners->corner[0]], &FACE_SCRATCH->p[0], &flag);
    func_8004A6DC(&vertices[corners->corner[1]], &FACE_SCRATCH->p[1], &flag);
    func_8004A6DC(&vertices[corners->corner[2]], &FACE_SCRATCH->p[2], &flag);
    edge1->vx -= FACE_SCRATCH->p[0].vx;
    edge1->vy -= FACE_SCRATCH->p[0].vy;
    edge1->vz -= FACE_SCRATCH->p[0].vz;
    edge2->vx -= FACE_SCRATCH->p[0].vx;
    edge2->vy -= FACE_SCRATCH->p[0].vy;
    edge2->vz -= FACE_SCRATCH->p[0].vz;
    OuterProduct0(&FACE_SCRATCH->p[2], &FACE_SCRATCH->p[1], &FACE_SCRATCH->p[2]);
    edge2->vx >>= 2;
    edge2->vy >>= 2;
    edge2->vz >>= 2;
    VectorNormal(&FACE_SCRATCH->p[2], normal);
    func_800935DC(offset, &FACE_SCRATCH->p[0], normal);
}

/* Does the vertical segment from `position` down by `height` cross the plane
 * of face `face` of scene object `index`? Returns -1 if so, else 0. */
s32 func_80085418(VECTOR *position, s32 height, u16 index, u16 face) {
    s32 flag;
    SceneObject *object;
    MeshFace *corners;
    SVECTOR *vertices;
    VECTOR *origin;
    VECTOR *edge1;
    VECTOR *edge2;
    s32 depth;
    s32 y;
    s16 z;

    object = &D_8009C620[index];
    FACE_SCRATCH->m = object->matrix;
    FACE_SCRATCH->m.t[2] = 0;
    FACE_SCRATCH->m.t[0] = 0;
    FACE_SCRATCH->m.t[1] = object->position.vy;
    FACE_SCRATCH->p[1].vz = 0x800;
    FACE_SCRATCH->p[1].vy = 0x800;
    FACE_SCRATCH->p[1].vx = 0x800;
    edge1 = &FACE_SCRATCH->p[1];
    ScaleMatrix(&FACE_SCRATCH->m, edge1);
    SetRotMatrix(&FACE_SCRATCH->m);
    SetTransMatrix(&FACE_SCRATCH->m);
    edge2 = &FACE_SCRATCH->p[2];
    corners = ((Mesh *)object->unk44)->faces;
    corners += face;
    vertices = ((Mesh *)object->unk44)->vertices;
    func_8004A6DC(&vertices[corners->corner[0]], &FACE_SCRATCH->p[0], &flag);
    func_8004A6DC(&vertices[corners->corner[1]], &FACE_SCRATCH->p[1], &flag);
    func_8004A6DC(&vertices[corners->corner[2]], &FACE_SCRATCH->p[2], &flag);
    origin = &FACE_SCRATCH->p[0];
    edge1->vx -= origin->vx;
    edge1->vy -= origin->vy;
    edge1->vz -= origin->vz;
    edge2->vx -= origin->vx;
    edge2->vy -= origin->vy;
    edge2->vz -= origin->vz;
    OuterProduct0(&FACE_SCRATCH->p[2], &FACE_SCRATCH->p[1], &FACE_SCRATCH->p[2]);
    edge2->vx >>= 2;
    edge2->vy >>= 2;
    edge2->vz >>= 2;
    VectorNormal(&FACE_SCRATCH->p[2], &FACE_SCRATCH->normal);
    FACE_SCRATCH->probe.m[1][0] = FACE_SCRATCH->probe.m[0][0] =
        (position->vx >> 12) - object->position.vx - origin->vx;
    y = (position->vy >> 12) - origin->vy;
    FACE_SCRATCH->probe.m[0][1] = y;
    z = object->position.vz;
    depth = position->vz >> 12;
    FACE_SCRATCH->probe.m[1][1] = y - height;
    z -= depth;
    z -= origin->vz;
    FACE_SCRATCH->probe.m[1][2] = FACE_SCRATCH->probe.m[0][2] = z;
    ApplyMatrixLV(&FACE_SCRATCH->probe, &FACE_SCRATCH->normal, &FACE_SCRATCH->side);
    return (FACE_SCRATCH->side.vx ^ FACE_SCRATCH->side.vy) >> 31;
}

/* Classify the move from `from` to `to` against face `face` of scene object
 * `index`: bit n is set when `to` lies outside edge n (flat x, z); when it
 * leaves through a corner, keep only the edge the move crosses. Also leave
 * the normalised edge directions in the scratchpad. */
s32 func_80085760(VECTOR *from, VECTOR *to, s32 index, s32 face) {
    u32 sides;
    s32 y;
    SceneObject *object;
    FaceTestScratch *scratch;
    VECTOR *p1;
    VECTOR *p2;
    Mesh *mesh;
    MeshFace *corners;
    SVECTOR *vertices;

    scratch = FACE_TEST_SCRATCH;
    object = &D_8009C620[index];
    FACE_TEST_SCRATCH->u.test.delta.vx = (to->vx >> 12) - object->position.vx;
    FACE_TEST_SCRATCH->u.test.delta.vz = object->position.vz - (to->vz >> 12);
    FACE_TEST_SCRATCH->m = object->matrix;
    FACE_TEST_SCRATCH->m.t[2] = 0;
    FACE_TEST_SCRATCH->m.t[0] = 0;
    y = object->position.vy;
    FACE_TEST_SCRATCH->p[0].vz = 0x800;
    FACE_TEST_SCRATCH->p[0].vy = 0x800;
    scratch->p[0].vx = 0x800;
    FACE_TEST_SCRATCH->m.t[1] = y;
    ScaleMatrix(&FACE_TEST_SCRATCH->m, &FACE_TEST_SCRATCH->p[0]);
    SetRotMatrix(&FACE_TEST_SCRATCH->m);
    SetTransMatrix(&FACE_TEST_SCRATCH->m);
    mesh = (Mesh *)object->unk44;
    corners = mesh->faces;
    corners += face;
    vertices = mesh->vertices;
    gte_RotTrans(&vertices[corners->corner[0]], &scratch->p[0], &sides);
    gte_ldv0(&vertices[corners->corner[1]]);
    gte_rt();
    p1 = &FACE_TEST_SCRATCH->p[1];
    gte_stlvnl(p1);
    gte_stflg(&sides);
    gte_ldv0(&vertices[corners->corner[2]]);
    gte_rt();
    p2 = &FACE_TEST_SCRATCH->p[2];
    gte_stlvnl(p2);
    gte_stflg(&sides);
    sides = 0;
    FACE_TEST_SCRATCH->u.test.edge[0] = (FACE_TEST_SCRATCH->p[0].vz << 16) | (u16)scratch->p[0].vx;
    FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[1].vz << 16) | (u16)p1->vx;
    FACE_TEST_SCRATCH->u.test.point =
        (FACE_TEST_SCRATCH->u.test.delta.vz << 16) | (u16)FACE_TEST_SCRATCH->u.test.delta.vx;
    if (func_8004A70C(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                      FACE_TEST_SCRATCH->u.test.point) > 0) {
        sides |= 1;
    }
    FACE_TEST_SCRATCH->u.test.edge[0] = (FACE_TEST_SCRATCH->p[1].vz << 16) | (u16)p1->vx;
    FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[2].vz << 16) | (u16)p2->vx;
    if (func_8004A70C(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                      FACE_TEST_SCRATCH->u.test.point) > 0) {
        sides |= 2;
    }
    FACE_TEST_SCRATCH->u.test.edge[0] = (FACE_TEST_SCRATCH->p[2].vz << 16) | (u16)p2->vx;
    FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[0].vz << 16) | (u16)scratch->p[0].vx;
    if (func_8004A70C(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                      FACE_TEST_SCRATCH->u.test.point) > 0) {
        sides |= 4;
    }
    switch (sides) {
    case 3:
        FACE_TEST_SCRATCH->u.test.edge[0] = ((object->position.vz - (from->vz >> 12)) << 16) |
                                            (((from->vx >> 12) - object->position.vx) & 0xFFFF);
        FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[1].vz << 16) | (u16)p1->vx;
        if (func_8004A70C(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                          FACE_TEST_SCRATCH->u.test.point) != 0) {
            sides = 1;
        }
        break;
    case 5:
        FACE_TEST_SCRATCH->u.test.edge[0] = ((object->position.vz - (from->vz >> 12)) << 16) |
                                            (((from->vx >> 12) - object->position.vx) & 0xFFFF);
        FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[0].vz << 16) | (u16)scratch->p[0].vx;
        if (func_8004A70C(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                          FACE_TEST_SCRATCH->u.test.point) != 0) {
            sides = 4;
        }
        break;
    case 6:
        FACE_TEST_SCRATCH->u.test.edge[0] = ((object->position.vz - (from->vz >> 12)) << 16) |
                                            (((from->vx >> 12) - object->position.vx) & 0xFFFF);
        FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[2].vz << 16) | (u16)p2->vx;
        if (func_8004A70C(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                          FACE_TEST_SCRATCH->u.test.point) != 0) {
            sides = 2;
        }
        break;
    }
    scratch->u.side[0].vx = scratch->p[1].vx - scratch->p[0].vx;
    scratch->u.side[0].vy = 0;
    scratch->u.side[0].vz = scratch->p[1].vz - scratch->p[0].vz;
    VectorNormal(&scratch->u.side[0], &scratch->u.side[0]);
    scratch->u.side[1].vx = scratch->p[2].vx - scratch->p[1].vx;
    scratch->u.side[1].vy = 0;
    scratch->u.side[1].vz = scratch->p[2].vz - scratch->p[1].vz;
    VectorNormal(&scratch->u.side[1], &scratch->u.side[1]);
    scratch->u.side[2].vx = scratch->p[0].vx - scratch->p[2].vx;
    scratch->u.side[2].vy = 0;
    scratch->u.side[2].vz = scratch->p[0].vz - scratch->p[2].vz;
    VectorNormal(&scratch->u.side[2], &scratch->u.side[2]);
    return sides;
}

/* Draw the actors' model sprites: place each visible model relative to the
 * camera target, project it for its depth, then add it to the ordering
 * table and turn its facing towards the actor heading, 0x100 per frame. */
void func_80085CDC(void) {
    WorldmapActor *actor;
    ModelObject *model;
    s32 i;
    s32 offset;
    s32 diff;
    s32 facing;
    s32 heading;
    DepthScratch *scratch;

    scratch = DEPTH_SCRATCH;
    actor = D_8009BE24;
    for (i = 0; i < 0x40; i++, actor++) {
        if ((actor->unk24 == 0) & (actor->handle != 0)) {
            scratch->offset.vx = actor->position.vx - D_8009BE28.target.vx;
            scratch->offset.vz = actor->position.vz - D_8009BE28.target.vz;
            func_80093484(&scratch->offset);
            ((ModelObject *)actor->handle)->position.vx = scratch->offset.vx * 16;
            ((ModelObject *)actor->handle)->position.vz = -scratch->offset.vz * 16;
            ((ModelObject *)actor->handle)->position.vy = actor->position.vy * 16;
        }
    }
    SetRotMatrix(&D_8009C808);
    SetTransMatrix(&D_8009C808);
    actor = D_8009BE24;
    for (i = 0; i < 0x40; i++, actor++) {
        model = (ModelObject *)actor->handle;
        if ((actor->unk24 == 0) & (model != NULL)) {
            scratch->vertex.vx = model->position.vx >> 16;
            scratch->vertex.vy = ((ModelObject *)actor->handle)->position.vy >> 16;
            scratch->vertex.vz = ((ModelObject *)actor->handle)->position.vz >> 16;
            gte_ldv0(&scratch->vertex);
            gte_rtps();
            gte_stsz(&scratch->depth[i]);
        }
    }
    func_80024FF4(&D_8009C808);
    actor = D_8009BE24;
    for (i = 0; i < 0x40; i++, actor++) {
        if ((actor->unk24 == 0) & (actor->handle != 0) & (scratch->depth[i] < 0xB00)) {
            func_8001E298(actor->handle, &D_8009BE3C->ot[scratch->depth[i] >> 4]);
            heading = actor->heading;
            facing = actor->unk5C;
            diff = heading - facing;
            if (diff < 0) {
                diff += 0x1000;
            }
            /* diff becomes the new facing, turned at most 0x100 */
            if (diff < 0x801) {
                if (diff < 0x100) {
                    diff = heading;
                } else {
                    diff = facing + 0x100;
                }
            } else {
                diff -= 0x1000;
                if (diff >= -0xFF) {
                    diff = heading;
                } else {
                    diff = facing - 0x100;
                }
            }
            actor->unk5C = diff;
            func_800223B0(actor->handle, (diff - (u16)D_8009BD38.vy - 0x400) & 0xFFF);
            func_80023210(actor->handle);
        }
    }
}

/* Resolve the terrain texture offsets and create the terrain CLUTs. */
void func_80085F58(void) {
    TerrainTexture *texture;
    s32 i;

    texture = D_8009C7EC;
    for (i = 0; i < 0x100; i++, texture++) {
        if (texture->data != NULL) {
            texture->data += (s32)D_8009C7EC;
        }
    }
    for (i = 0; i < 0x10; i++) {
        D_8009D478[i] = GetClut(0xF0, i + 0x1F0);
    }
}

/* Allocate the two buffers of 512 opaque textured 32x48 quads on the
 * 0x380,0x100 page, the second a copy of the first. */
void func_80085FE0(void) {
    PolyFT4 *quad;
    s32 i;

    D_8009D7E8[0] = func_80031BDC(sizeof(QuadBlock512), 1);
    D_8009D7E8[1] = func_80031BDC(sizeof(QuadBlock512), 1);
    quad = D_8009D7E8[0];
    for (i = 0; i < 0x200; i++, quad++) {
        setPolyFT4(quad);
        setRGB0(quad, 0x80, 0x80, 0x80);
        quad->u0 = 0;
        quad->v0 = 0x40;
        quad->u1 = 0x1F;
        quad->v1 = 0x40;
        quad->u2 = 0;
        quad->v2 = 0x6F;
        quad->u3 = 0x1F;
        quad->v3 = 0x6F;
        quad->clut = GetClut(0xF0, 0x1FF);
        quad->tpage = GetTPage(0, 0, 0x380, 0x100);
    }
    *(QuadBlock512 *)D_8009D7E8[1] = *(QuadBlock512 *)D_8009D7E8[0];
}

/* Free two work buffers. */
void func_80086124(void) {
    func_800320E8(D_8009D7E8[1]);
    func_800320E8(D_8009D7E8[0]);
}

/* Draw the 5x5 terrain blocks around the cursor: set up the scratchpad quad,
 * camera and roll matrices and CLUTs, then submit each present block's
 * texture list into the current quad buffer. */
void func_8008615C(void) {
    TerrainTexture *textures;
    s32 index;
    s32 i;
    s32 row;

    TERRAIN_PASS_SCRATCH->corner[0].vx = -0x18;
    TERRAIN_PASS_SCRATCH->corner[0].vy = -0x48;
    TERRAIN_PASS_SCRATCH->corner[0].vz = 0;
    TERRAIN_PASS_SCRATCH->corner[1].vx = 0x18;
    TERRAIN_PASS_SCRATCH->corner[1].vy = -0x48;
    TERRAIN_PASS_SCRATCH->corner[1].vz = 0;
    TERRAIN_PASS_SCRATCH->corner[2].vx = -0x18;
    TERRAIN_PASS_SCRATCH->corner[2].vy = 0;
    TERRAIN_PASS_SCRATCH->corner[2].vz = 0;
    TERRAIN_PASS_SCRATCH->corner[3].vx = 0x18;
    TERRAIN_PASS_SCRATCH->corner[3].vy = 0;
    TERRAIN_PASS_SCRATCH->corner[3].vz = 0;
    TERRAIN_PASS_SCRATCH->view = D_8009C808;
    TERRAIN_PASS_SCRATCH->roll = *(MATRIX *)&D_8009A180;
    RotMatrixZ(-D_8009BD38.vz, &TERRAIN_PASS_SCRATCH->roll);
    for (i = 0; i < 0x10; i++) {
        TERRAIN_PASS_SCRATCH->clut[i] = D_8009D478[i];
    }
    textures = D_8009C7EC;
    D_8009BE04 = 0;
    for (row = 0; row < 5; row++) {
        for (i = 0; i < 5; i++) {
            if (D_8009D618[row * 5 + i] != -1) {
                index = D_8009D570.cells[(row + D_8009C838.vz) * 9 + i + D_8009C838.vx];
                if (textures[index].unk4 != 0) {
                    func_80099BFC(textures[index].data, textures[index].unk4, D_8009BE3C->ot,
                                  (PolyFT4 *)D_8009D7E8[D_8009D7F0] + D_8009BE04);
                }
            }
        }
    }
}

/* Scatter the 80 drifting positions: five template points repeated over a
 * 4x4 grid of 0x800-unit cells at random heights, with random velocities. */
void func_800863E0(void) {
    Drift *drift;
    DriftVelocity *velocity;
    s32 row;
    s32 column;
    s32 i;

    D_8009D150 = func_80031BDC(0x50 * sizeof(Drift), 0);
    D_8009CEB4 = func_80031BDC(0x50 * sizeof(DriftVelocity), 0);
    drift = D_8009D150;
    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            for (i = 0; i < 5; i++, drift++) {
                drift->x = (D_8009AF30[i].x + (column << 11)) << 12;
                drift->z = (D_8009AF30[i].z + (row << 11)) << 12;
                drift->unk4 = (-(rand() >> 10) * 8 - 0x200) << 12;
            }
        }
    }
    velocity = D_8009CEB4;
    for (i = 0; i < 0x50; i++) {
        column = (rand() & 3) + 1; /* reused as the speed */
        velocity->dx = column * 0xDDB;
        velocity->dz = -(column << 11);
        velocity->unk2 = rand() & 1;
        velocity++;
    }
}

/* Free two work buffers. */
void func_80086568(void) {
    func_800320E8(D_8009CEB4);
    func_800320E8(D_8009D150);
}

/* Allocate the two buffers of 0x120 semi-transparent grey textured quads
 * on the 0x3C0,0x100 page, the second a copy of the first. */
void func_800865A0(void) {
    PolyFT4 *quad;
    s32 i;
    s32 code;
    s32 colour;
    s32 length;

    D_8009D7F8[0] = func_80031BDC(sizeof(QuadBlock288), 1);
    D_8009D7F8[1] = func_80031BDC(sizeof(QuadBlock288), 1);
    colour = 0x26;
    i = 0;
    quad = D_8009D7F8[0];
    length = 9;
    code = 0x2C;
    for (; i < 0x120; i++, quad++) {
        setlen(quad, length);
        setcode(quad, code);
        setRGB0(quad, colour, colour, colour);
        quad->tpage = GetTPage(0, 1, 0x3C0, 0x100);
        quad->clut = GetClut(0x130, 0x1FE);
        SetSemiTrans(quad, 1);
    }
    *(QuadBlock288 *)D_8009D7F8[1] = *(QuadBlock288 *)D_8009D7F8[0];
}

/* Free two work buffers. */
void func_800866C8(void) {
    func_800320E8(D_8009D7F8[1]);
    func_800320E8(D_8009D7F8[0]);
}

/* Move the 80 drifting positions, wrapping them on the 0x2000-unit world. */
void func_80086700(void) {
    Drift *drift;
    DriftVelocity *velocity;
    s32 x;
    s32 z;
    s32 i;

    for (i = 0; i < 0x50; i++) {
        drift = &D_8009D150[i];
        velocity = &D_8009CEB4[i];
        x = drift->x + velocity->dx;
        z = drift->z + velocity->dz;
        if (x > 0x1FFFFFF) {
            x -= 0x2000000;
        }
        if (x < 0) {
            x += 0x2000000;
        }
        if (z > 0x1FFFFFF) {
            z -= 0x2000000;
        }
        if (z < 0) {
            z += 0x2000000;
        }
        drift->x = x;
        drift->z = z;
    }
}

/* Scratchpad work area of the drifting sprites (clouds). */
typedef struct {
    SVECTOR far[12];      /* 0x000: far sprite corners (D_8009AD50) */
    SVECTOR near[48];     /* 0x060: near sprite quads, 4 corners each (D_8009ADB0) */
    u16 uv[8];            /* 0x1E0: texture origins (D_8009AD40) */
    SVECTOR corner[4];    /* 0x1F0: built quad corners */
    VECTOR origin;        /* 0x210: camera target on the 0x2000-unit world */
    VECTOR cell;          /* 0x220 */
    s32 edge[2];          /* 0x230: packed view-cone edges */
    s32 pad238[2];
    VECTOR view;          /* 0x240: view point relative to the eye */
    MATRIX local;         /* 0x250 */
    MATRIX screen;        /* 0x270 */
    MATRIX turn;          /* 0x290 */
    MATRIX work;          /* 0x2B0 */
    s32 uv_index;         /* 0x2D0 */
    s32 flag;             /* 0x2D4 */
    s32 z;                /* 0x2D8 */
    s32 sz[4];            /* 0x2DC */
    SVECTOR *vertices;    /* 0x2EC */
    s32 count;            /* 0x2F0: quads added this frame */
    s32 quads;            /* 0x2F4: quads considered */
} DriftScratch;

#define DRIFT_SCRATCH ((DriftScratch *)0x1F800000)

#define gte_ldsxy3(r0, r1, r2) \
    __asm__ volatile("mtc2 %0, $12;" \
                     "mtc2 %2, $14;" \
                     "mtc2 %1, $13" \
                     : \
                     : "r"(r0), "r"(r1), "r"(r2))
/* Keep producer stores for the indirect vertex reads. */
#define gte_ldv3c(r0) \
    __asm__ volatile("lwc2 $0, 0(%0);" \
                     "lwc2 $1, 4(%0);" \
                     "lwc2 $2, 8(%0);" \
                     "lwc2 $3, 12(%0);" \
                     "lwc2 $4, 16(%0);" \
                     "lwc2 $5, 20(%0)" \
                     : \
                     : "r"(r0) \
                     : "memory")
#define gte_stsz4c(r0) \
    __asm__ volatile("swc2 $16, 0(%0);" \
                     "swc2 $17, 4(%0);" \
                     "swc2 $18, 8(%0);" \
                     "swc2 $19, 12(%0)" \
                     : \
                     : "r"(r0) \
                     : "memory")
#define gte_getsxy3(r0, r1, r2) \
    __asm__ volatile("mfc2 %0, $12;" \
                     "mfc2 %1, $13;" \
                     "mfc2 %2, $14;" \
                     "nop" \
                     : "=r"(r0), "=r"(r1), "=r"(r2))
#define gte_getsxy2(r0) __asm__ volatile("mfc2 %0, $14; nop" : "=r"(r0))
/* Link a 9-word primitive into an ordering-table entry. */
#define addPrimLen9(ot, p) \
    __asm__ volatile("lw $12, 0(%0);" \
                     "lui $13, 0x0900;" \
                     "or $12, $12, $13;" \
                     "lui $13, 0x00FF;" \
                     "ori $13, $13, 0xFFFF;" \
                     "and $13, %1, $13;" \
                     "sw $13, 0(%0);" \
                     "sw $12, 0(%1)" \
                     : \
                     : "r"(ot), "r"(p) \
                     : "$12", "$13", "memory")

/* The projected quad lies partly on the 320x216 screen. */
#define ON_SCREEN(a, b, c, d)                                                                 \
    (((u16)(a) < 0x140 || (u16)(b) < 0x140 || (u16)(c) < 0x140 || (u16)(d) < 0x140) &&         \
     ((u32)(a) >> 16 < 0xD8 || (u32)(b) >> 16 < 0xD8 || (u32)(c) >> 16 < 0xD8 || (u32)(d) >> 16 < 0xD8))

/* Draw the 80 drifting cloud sprites around the camera. Each is culled against
 * the view cone, then drawn by distance: far as one to three 64-texel quads,
 * middle as three 4-quad layers of 32 texels, near as three layers of 4x4
 * generated 16-texel quads. At most 0xF1 quads are added per frame. x and z
 * hold the sprite's offset from the camera, then a near layer's base corner,
 * and count the middle rows and the near rows and columns; layer first holds
 * the packed view-cone test point. */
/* The four projected corners are register variables: the original keeps them
 * in t6-t9 ($14/$15/$24/$25) in all three branches. In the near loop these are
 * the only temporaries left: gte_stflg and addPrimLen9 use $12/$13 (t4/t5), and
 * t0-t3 hold the quad's store pointer, layer, the column offset and quad. As
 * plain locals the corners (48 loop-weighted refs over 176-199 insns each) rank
 * in global allocation above layer (53 over 380), the column offset (19 over
 * 118) and quad (46 over 493) and take t1/t2/t3/t6. Bound to t6-t9 the
 * function matches exactly. No compiler release or flag places plain locals
 * there (a replay of GCC 2.7.2's global allocation shows they cannot reach
 * t6-t9), and the corners are read with the three-mfc2-plus-nop shapes of
 * LIBGTE.H's register-argument read_sxsy macros, so the original most likely
 * declared them as register variables too. */
void func_80086798(void) {
    DriftScratch *scratch;
    PolyFT4 *quad;
    s32 i;
    s32 layer;
    s32 x;
    s32 z;
    s32 cx;
    s32 cz;
    u16 uv;
    register s32 sxy0 asm("$14");
    register s32 sxy1 asm("$15");
    register s32 sxy2 asm("$24");
    register s32 sxy3 asm("$25");

    (*D_8009CD40)();
    scratch = DRIFT_SCRATCH;
    for (i = 0; i < 0x30; i++) {
        scratch->near[i] = D_8009ADB0[i];
    }
    for (i = 0; i < 0xC; i++) {
        scratch->far[i] = D_8009AD50[i];
    }
    for (i = 0; i < 8; i++) {
        scratch->uv[i] = D_8009AD40[i];
    }
    scratch->origin.vx = D_8009BE28.target.vx & 0x1FFFFFF;
    scratch->origin.vz = D_8009BE28.target.vz & 0x1FFFFFF;
    scratch->cell.vx = (D_8009BE28.target.vx >> 12) & 0x7FF;
    scratch->cell.vz = (D_8009BE28.target.vz >> 12) & 0x7FF;
    scratch->local = D_8009A180;
    scratch->screen = scratch->local;
    scratch->turn = scratch->local;
    func_8004AE4C((D_8009BD38.vx + 0x400) / 8, &scratch->local);
    func_8004AFEC(-D_8009BD38.vy, &scratch->screen);
    func_8004AFEC(D_8009BD38.vy, &scratch->turn);
    MulMatrix0(&scratch->local, &scratch->screen, &scratch->work);
    MulMatrix0(&scratch->turn, &scratch->work, &scratch->local);
    scratch->edge[0] = (func_8003F8CC(D_8009BD38.vy - 0x169) << 16) | (func_8003F8B0(D_8009BD38.vy - 0x169) & 0xFFFF);
    scratch->edge[1] = (func_8003F8CC(D_8009BD38.vy + 0x169) << 16) | (func_8003F8B0(D_8009BD38.vy + 0x169) & 0xFFFF);
    scratch->view.vx = VIEW_VECTORS[0].vx * 2 + (-func_8003F8B0(D_8009BD38.vy) >> 1);
    scratch->view.vz = VIEW_VECTORS[0].vz * 2 + (-func_8003F8CC(D_8009BD38.vy) >> 1);
    quad = D_8009D7F8[D_8009D7F0];
    scratch->quads = 0;
    scratch->count = 0;
    for (i = 0; i < 0x50; i++) {
        if (scratch->count > 0xF0) {
            break;
        }
        x = (D_8009D150[i].x - scratch->origin.vx) >> 12;
        z = (D_8009D150[i].z - scratch->origin.vz) >> 12;
        if (x < -0x1000) {
            x += 0x2000;
        } else if (x >= 0x1000) {
            x -= 0x2000;
        }
        if (z < -0x1000) {
            z += 0x2000;
        } else if (z >= 0x1000) {
            z -= 0x2000;
        }
        z = -z;
        layer = ((z - scratch->view.vz) << 16) | ((x - scratch->view.vx) & 0xFFFF);
        gte_ldsxy3(layer, scratch->edge[1], 0);
        gte_nclip();
        gte_stopz(&scratch->flag);
        if (scratch->flag > 0) {
            continue;
        }
        layer = ((z - scratch->view.vz) << 16) | ((x - scratch->view.vx) & 0xFFFF);
        gte_ldsxy3(0, scratch->edge[0], layer);
        gte_nclip();
        gte_stopz(&scratch->flag);
        if (scratch->flag > 0) {
            continue;
        }
        scratch->local.t[0] = x;
        scratch->local.t[1] = D_8009D150[i].unk4 >> 12;
        scratch->local.t[2] = z;
        gte_CompMatrix(&D_8009C808, &scratch->local, &scratch->screen);
        gte_SetRotMatrix(&scratch->screen);
        gte_SetTransMatrix(&scratch->screen);
        scratch->corner[0].vx = scratch->corner[0].vy = scratch->corner[0].vz = 0;
        gte_ldv0(&scratch->corner[0]);
        gte_rtps();
        scratch->uv_index = D_8009CEB4[i].unk2 * 4;
        gte_stflg(&scratch->flag);
        if (scratch->flag & 0x7F85E000) {
            continue;
        }
        gte_stsz(&scratch->sz[0]);
        if (scratch->sz[0] > 0x580) {
            /* far: up to three 64-texel quads, fewer as the depth grows */
            scratch->vertices = scratch->far;
            for (layer = 0; layer < 3; layer++) {
                gte_ldv3c(scratch->vertices);
                gte_rtpt();
                uv = scratch->uv[scratch->uv_index++];
                gte_stflg(&scratch->flag);
                if (!(scratch->flag & 0x80000000)) {
                    gte_getsxy3(sxy0, sxy1, sxy2);
                    gte_ldv0(&scratch->vertices[3]);
                    gte_rtps();
                    gte_stflg(&scratch->flag);
                    if (!(scratch->flag & 0x80000000)) {
                        gte_getsxy2(sxy3);
                        if (ON_SCREEN(sxy0, sxy1, sxy2, sxy3)) {
                            gte_stsz4c(scratch->sz);
                            if (scratch->sz[0] > scratch->sz[1]) {
                                scratch->z = scratch->sz[0];
                            } else {
                                scratch->z = scratch->sz[1];
                            }
                            if (scratch->sz[2] > scratch->z) {
                                scratch->z = scratch->sz[2];
                            } else if (scratch->sz[3] > scratch->z) {
                                scratch->z = scratch->sz[3];
                            }
                            if (scratch->z > 0xD00) {
                                break;
                            }
                            addPrimLen9(D_8009BE3C->ot + (scratch->z >> 4), quad);
                            *(u16 *)&quad->u1 = uv | 0x3F;
                            *(u16 *)&quad->u2 = uv | 0x3F00;
                            *(s32 *)&quad->x0 = sxy0;
                            *(s32 *)&quad->x1 = sxy1;
                            *(s32 *)&quad->x2 = sxy2;
                            *(s32 *)&quad->x3 = sxy3;
                            *(u16 *)&quad->u0 = uv;
                            *(u16 *)&quad->u3 = uv | 0x3F3F;
                            quad++;
                            scratch->count++;
                            if (scratch->z > 0xB00) {
                                break;
                            }
                        }
                    }
                }
                scratch->vertices += 4;
                scratch->quads++;
            }
        } else if (scratch->sz[0] > 0x400) {
            /* middle: three layers of four 32-texel quads */
            scratch->vertices = scratch->near;
            for (layer = 0; layer < 3; layer++) {
                for (x = 0; x < 4; x++) {
                    gte_ldv3c(scratch->vertices);
                    gte_rtpt();
                    uv = scratch->uv[layer + scratch->uv_index] + ((x & 2) << 12) + ((x & 1) << 5);
                    gte_stflg(&scratch->flag);
                    if (!(scratch->flag & 0x80000000)) {
                        gte_getsxy3(sxy0, sxy1, sxy2);
                        gte_ldv0(&scratch->vertices[3]);
                        gte_rtps();
                        gte_stflg(&scratch->flag);
                        if (!(scratch->flag & 0x80000000)) {
                            gte_getsxy2(sxy3);
                            if (ON_SCREEN(sxy0, sxy1, sxy2, sxy3)) {
                                gte_stsz4c(scratch->sz);
                                if (scratch->sz[0] > scratch->sz[1]) {
                                    scratch->z = scratch->sz[0];
                                } else {
                                    scratch->z = scratch->sz[1];
                                }
                                if (scratch->sz[2] > scratch->z) {
                                    scratch->z = scratch->sz[2];
                                } else if (scratch->sz[3] > scratch->z) {
                                    scratch->z = scratch->sz[3];
                                }
                                addPrimLen9(D_8009BE3C->ot + (scratch->z >> 4), quad);
                                *(u16 *)&quad->u1 = uv | 0x1F;
                                *(u16 *)&quad->u2 = uv | 0x1F00;
                                *(s32 *)&quad->x0 = sxy0;
                                *(s32 *)&quad->x1 = sxy1;
                                *(s32 *)&quad->x2 = sxy2;
                                *(s32 *)&quad->x3 = sxy3;
                                *(u16 *)&quad->u0 = uv;
                                *(u16 *)&quad->u3 = uv | 0x1F1F;
                                quad++;
                                scratch->count++;
                            }
                        }
                    }
                    scratch->vertices += 4;
                    scratch->quads++;
                }
            }
        } else {
            /* near: three layers of 4x4 16-texel quads over a 0x180 square */
            for (layer = 0; layer < 3; layer++) {
                x = scratch->corner[0].vx = scratch->corner[2].vx = scratch->far[0].vx;
                z = scratch->corner[0].vz = scratch->corner[1].vz = scratch->far[0].vz;
                scratch->corner[1].vx = scratch->corner[3].vx = x + 0x180;
                scratch->corner[2].vz = scratch->corner[3].vz = z - 0x180;
                scratch->corner[0].vy = scratch->corner[1].vy = scratch->corner[2].vy = scratch->corner[3].vy =
                    scratch->far[0].vy - layer * 8;
                gte_ldv0(&scratch->corner[0]);
                gte_rtps();
                gte_stflg(&scratch->flag);
                if (scratch->flag & 0x7F85E000) {
                    continue;
                }
                gte_ldv3c(&scratch->corner[1]);
                gte_rtpt();
                gte_stflg(&scratch->flag);
                if (scratch->flag & 0x7F85E000) {
                    continue;
                }
                for (x = 0; x < 4; x++) {
                    for (z = 0; z < 4; z++) {
                        cx = scratch->corner[0].vx = scratch->corner[2].vx = scratch->far[0].vx + z * 0x60;
                        cz = scratch->corner[0].vz = scratch->corner[1].vz = scratch->far[0].vz - x * 0x60;
                        scratch->corner[1].vx = scratch->corner[3].vx = cx + 0x60;
                        scratch->corner[2].vz = scratch->corner[3].vz = cz - 0x60;
                        gte_ldv3c(&scratch->corner[0]);
                        gte_rtpt();
                        uv = scratch->uv[layer + scratch->uv_index] + ((x << 12) + (z << 4));
                        gte_stflg(&scratch->flag);
                        if (!(scratch->flag & 0x7F85E000)) {
                            gte_getsxy3(sxy0, sxy1, sxy2);
                            gte_ldv0(&scratch->corner[3]);
                            gte_rtps();
                            gte_stflg(&scratch->flag);
                            if (!(scratch->flag & 0x7F85E000)) {
                                gte_getsxy2(sxy3);
                                if (ON_SCREEN(sxy0, sxy1, sxy2, sxy3)) {
                                    gte_stsz4c(scratch->sz);
                                    if (scratch->sz[0] > scratch->sz[1]) {
                                        scratch->z = scratch->sz[0];
                                    } else {
                                        scratch->z = scratch->sz[1];
                                    }
                                    if (scratch->sz[2] > scratch->z) {
                                        scratch->z = scratch->sz[2];
                                    } else if (scratch->sz[3] > scratch->z) {
                                        scratch->z = scratch->sz[3];
                                    }
                                    addPrimLen9(D_8009BE3C->ot + (scratch->z >> 4), quad);
                                    *(u16 *)&quad->u1 = uv | 0xF;
                                    *(u16 *)&quad->u2 = uv | 0xF00;
                                    *(s32 *)&quad->x0 = sxy0;
                                    *(s32 *)&quad->x1 = sxy1;
                                    *(s32 *)&quad->x2 = sxy2;
                                    *(s32 *)&quad->x3 = sxy3;
                                    *(u16 *)&quad->u0 = uv;
                                    *(u16 *)&quad->u3 = uv | 0xF0F;
                                    quad++;
                                    scratch->count++;
                                }
                            }
                        }
                        scratch->quads++;
                    }
                }
            }
        }
    }
}

/* Reset an actor to step 0 with parameter 8. */
s32 func_80087710(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->unk54 = 8;
    return 1;
}

/* Spin scene objects 12 and 14 about their own axis. */
s32 func_80087734(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;
    SVECTOR *first;
    SVECTOR *second;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    first = SCRIPT_VECTOR;
    second = SCRIPT_VECTOR + 1;
    second->vx = 0;
    first->vx = 0;
    first->vy = objects[12].angle.vy;
    second->vy = objects[14].angle.vy;
    first->vz = second->vz = actor->u.step;
    func_8003F738(first, &objects[12].matrix);
    func_8003F738(second, &objects[14].matrix);
    return 1;
}

/* Reset an actor to step 0 with parameter 8. */
s32 func_800877E0(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->unk54 = 8;
    return 1;
}

/* Spin the current area's two scene objects about their own axis. */
s32 func_80087804(s32 index) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */
    WorldmapActor *actor;
    SceneObject *objects;
    SceneObject *first;
    SceneObject *second;
    s32 first_id;
    s32 second_id;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    first_id = D_8009B624[D_8009C610][0];
    second_id = D_8009B624[D_8009C610][1];
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    SCRIPT_VECTOR[1].vx = 0;
    SCRIPT_VECTOR[0].vx = 0;
    first = &objects[first_id];
    SCRIPT_VECTOR[0].vy = first->angle.vy;
    second = &objects[second_id];
    SCRIPT_VECTOR[1].vy = second->angle.vy;
    SCRIPT_VECTOR[0].vz = SCRIPT_VECTOR[1].vz = actor->u.step;
    func_8003F738(&SCRIPT_VECTOR[0], &first->matrix);
    func_8003F738(&SCRIPT_VECTOR[1], &second->matrix);
    return 1;
}

/* Give `count` quads the semi-transparent 0x1A0,0xA0 texture page. */
void func_80087904(SceneObject *object, PolyFT4 *quads, s32 count, s32 abr) {
    s32 i;

    for (i = 0; i < count; i++) {
        quads->tpage = GetTPage(0, abr, 0x1A0, 0xA0);
        setSemiTrans(quads, 1);
        quads++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(PolyFT4));
}

s32 func_800879E0(s32 index);

/* Reset an actor to step 0 with parameter 0x10 and rebuild the area's two
 * scene objects. */
s32 func_800879A8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->unk54 = 0x10;
    func_800879E0(index);
    return 1;
}

/* Rebuild the primitives of the current area's two scene objects. */
s32 func_800879E0(s32 index) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */
    SceneObject *first;
    SceneObject *second;

    first = &D_8009C620[D_8009B64C[D_8009C610][0]];
    second = &D_8009C620[D_8009B64C[D_8009C610][1]];
    func_80087904(first, first->prims, first->def->count, 3);
    func_80087904(second, second->prims, second->def->count, 3);
    return 1;
}

/* Roll the current area's two scene objects together. */
s32 func_80087A8C(s32 index) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */
    WorldmapActor *actor;
    SceneObject *objects;
    SceneObject *first;
    SceneObject *second;
    s32 first_id;
    s32 second_id;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    first_id = D_8009B64C[D_8009C610][0];
    second_id = D_8009B64C[D_8009C610][1];
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    first = &objects[first_id];
    SCRIPT_VECTOR[1].vz = 0;
    SCRIPT_VECTOR[0].vz = 0;
    SCRIPT_VECTOR[1].vx = 0;
    SCRIPT_VECTOR[0].vx = 0;
    SCRIPT_VECTOR[0].vy = SCRIPT_VECTOR[1].vy = actor->u.step;
    second = &objects[second_id];
    func_8003F738(&SCRIPT_VECTOR[0], &first->matrix);
    func_8003F738(&SCRIPT_VECTOR[1], &second->matrix);
    return 1;
}

/* Build a rotation matrix whose third row faces `direction`, using `up`
 * as scratch for the first two rows. */
void func_80087B84(VECTOR *direction, VECTOR *up, MATRIX *m) {
    up->vz = 0;
    up->vx = 0;
    up->vy = 0x1000;
    func_8004A480(up, direction, up);
    VectorNormal(up, up);
    m->m[0][0] = up->vx;
    m->m[0][1] = up->vy;
    m->m[0][2] = up->vz;
    func_8004A480(direction, up, up);
    VectorNormal(up, up);
    m->m[1][0] = up->vx;
    m->m[1][1] = up->vy;
    m->m[1][2] = up->vz;
    m->m[2][0] = direction->vx;
    m->m[2][1] = direction->vy;
    m->m[2][2] = direction->vz;
    func_8004A8EC(m, m);
}

/* Compiled-out debug trace of the ferry's resumed position. */
#define FERRY_TRACE_POSITION(actor) do { } while (0)

/* Start the area's ferry: before scene 0xCD it rests at a fixed dock;
 * otherwise it resumes its route (first time: at waypoint 0), advancing
 * when within 8 units of the waypoint, and heads for the next one. */
s32 func_80087C6C(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    FerryScratch *scratch;
    s32 id;
    s32 distance;
    s32 i;
    u16 z;

    id = D_8009B674[D_8009C610];
    func_80087F60(index);
    scratch = FERRY_SCRATCH;
    actor = &D_8009BE24[index];
    actor->turn = 0x4000;
    actor->state = 0;
    object = &D_8009C620[id];
    if (D_8006EF64[0] < 0xCD) {
        actor->position.vx = 0xD80000;
        actor->position.vz = 0x7280000;
        object->matrix = *(MATRIX *)&D_8009A180;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    } else {
        if (D_8006EE7E == 0) {
            D_8006EE7E++;
            actor->u.step = 0;
            actor->position.vx = D_8009AF80[actor->u.step] << 12;
            z = D_8009AF90[actor->u.step];
        } else {
            actor->u.step = D_8006EE78[2];
            actor->position.vx = D_8006EE78[0] << 12;
            z = D_8006EE78[1];
        }
        actor->position.vz = z << 12;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        FERRY_TRACE_POSITION(actor);
        scratch->work.vx = D_8009AF80[actor->u.step] << 12;
        scratch->work.vz = D_8009AF90[actor->u.step] << 12;
        distance = func_80094154(&actor->position, &scratch->work);
        if (distance < 0) {
            distance = -distance;
        }
        if (distance < 8) {
            actor->u.step = (actor->u.step + 1) & 7;
        }
        scratch->work.vx = D_8009AF80[actor->u.step] - (actor->position.vx >> 12);
        scratch->work.vz = D_8009AF90[actor->u.step] - (actor->position.vz >> 12);
        scratch->work.vy = 0;
        func_80093534(&scratch->work);
        VectorNormal(&scratch->work, &scratch->work);
        for (i = 0; i < 0x20; i++) {
            D_8009CD68[i].dx = scratch->work.vx;
            D_8009CD68[i].dz = scratch->work.vz;
        }
        actor->unk58 = 1;
        actor->unk54 = 0;
        actor->motion.vx = D_8009CD68[1].dx;
        actor->motion.vz = D_8009CD68[actor->unk58].dz;
        actor->motion.vy = 0;
    }
    return 1;
}
/* Link the four objects before the area's scene object to it. The actor
 * dispatcher supplies an index, which this area-wide handler does not use. */
s32 func_80087F60(s32 index) {
    u16 object;

    object = D_8009B674[D_8009C610];
    func_800848B4(object, object - 4);
    func_800848B4(object, object - 3);
    func_800848B4(object, object - 2);
    func_800848B4(object, object - 1);
    return 1;
}

/* Run the ferry: before scene 0xCD it only follows the ground; otherwise
 * it steers along its waypoints, turns its model, moves by its delayed
 * heading history at a speed that eases in and out near the dock, sprays
 * its wake above speed 0x1000 and saves its route state. */
s32 func_80087FD0(s32 index) {
    FerryScratch *scratch;
    WorldmapActor *actor;
    SceneObject *object;
    s32 distance;
    s32 dock;

    scratch = FERRY_SCRATCH;
    actor = &D_8009BE24[index];
    object = &D_8009C620[D_8009B674[D_8009C610]];
    if (D_8006EF64[0] < 0xCD) {
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        object->position.vx = actor->position.vx >> 12;
        object->position.vy = actor->position.vy >> 12;
        object->position.vz = actor->position.vz >> 12;
    } else {
        scratch->work.vx = D_8009AF80[actor->u.step] << 12;
        scratch->work.vz = D_8009AF90[actor->u.step] << 12;
        distance = func_80094154(&actor->position, &scratch->work);
        if (distance < 0) {
            distance = -distance;
        }
        if (distance < 8) {
            actor->u.step = (actor->u.step + 1) & 7;
        }
        scratch->work.vx = D_8009AF80[actor->u.step] - (actor->position.vx >> 12);
        scratch->work.vz = D_8009AF90[actor->u.step] - (actor->position.vz >> 12);
        scratch->work.vy = 0;
        func_80093534(&scratch->work);
        VectorNormal(&scratch->work, &scratch->work);
        actor->motion.vx = ((scratch->work.vx + actor->motion.vx * 63) << 6) >> 12;
        actor->motion.vz = ((scratch->work.vz + actor->motion.vz * 63) << 6) >> 12;
        VectorNormal(&actor->motion, &actor->motion);
        D_8009CD68[actor->unk54].dx = actor->motion.vx;
        D_8009CD68[actor->unk54].dz = actor->motion.vz;
        actor->unk54 = (actor->unk54 + 1) & 0x1F;
        scratch->work.vx = actor->motion.vx;
        scratch->work.vy = actor->motion.vy;
        scratch->work.vz = -actor->motion.vz;
        func_80087B84(&scratch->work, &scratch->up, &scratch->m);
        object->matrix = scratch->m;
        actor->position.vx += D_8009CD68[actor->unk58].dx * (actor->turn >> 12);
        actor->position.vz += D_8009CD68[actor->unk58].dz * (actor->turn >> 12);
        actor->unk58 = (actor->unk58 + 1) & 0x1F;
        func_80093354(&actor->position);
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        object->position.vx = actor->position.vx >> 12;
        object->position.vy = actor->position.vy >> 12;
        object->position.vz = actor->position.vz >> 12;
        scratch->work.vx = D_8006EE54.unk60 - (actor->position.vx >> 12);
        scratch->work.vz = D_8006EE54.unk64 - (actor->position.vz >> 12);
        scratch->work.vy = (s16)D_8006EE54.unk62;
        func_80093534(&scratch->work);
        dock = SquareRoot0(scratch->work.vx * scratch->work.vx + scratch->work.vz * scratch->work.vz);
        if (dock < 0x100 && scratch->work.vy >= -0xBF) {
            actor->turn = 0;
        } else if (dock < 0x300 && scratch->work.vy >= -0xBF) {
            actor->turn -= 0x100;
            if (actor->turn < 0) {
                actor->turn = 0;
            }
        } else {
            actor->turn += 0x200;
            if (actor->turn > 0x4000) {
                actor->turn = 0x4000;
            }
        }
        if (actor->turn > 0x1000) {
            scratch->wake.vx = object->position.vx;
            scratch->wake.vy = object->position.vy;
            scratch->wake.vz = object->position.vz;
            func_8004A8EC(&object->matrix, &scratch->m2);
            func_80097070(&scratch->m2, &scratch->wake_angle);
            func_80089160(0x13, &scratch->wake, &scratch->wake_angle);
        } else {
            func_800894C8(0x13);
        }
    }
    D_8006EE78[0] = actor->position.vx >> 12;
    D_8006EE78[1] = actor->position.vz >> 12;
    D_8006EE78[2] = actor->u.step;
    scratch->work.vx = actor->position.vx;
    scratch->work.vy = actor->position.vy + 0x30000;
    scratch->work.vz = actor->position.vz;
    func_8008BFD4(index, &scratch->work, 0x80, 0xB0);
    return 1;
}

/* Start the actor circling above the map from the saved position (first
 * time: from 0, 0x480), and save it back. */
s32 func_80088570(s32 index) {
    WorldmapActor *actor;

    func_8008868C();
    actor = &D_8009BE24[index];
    actor->unk54 = 4;
    actor->state = 0;
    actor->u.step = 0;
    actor->unk58 = 0;
    actor->unk5C = 0xC;
    if (D_8006EE80.count == 0) {
        D_8006EE80.count++;
        actor->position.vy = -0x280000;
        actor->position.vx = 0;
        actor->position.vz = 0x4800000;
    } else {
        actor->position.vx = (D_8006EE80.x << 12) + D_8006EE80.x_frac;
        actor->position.vy = -0x280000;
        actor->position.vz = (D_8006EE80.z << 12) + D_8006EE80.z_frac;
    }
    actor->motion.vz = 0xB50;
    actor->motion.vx = 0xB50;
    actor->motion.vy = 0;
    actor->turn = 1;
    D_8006EE80.x_frac = actor->position.vx;
    D_8006EE80.x = actor->position.vx >> 12;
    D_8006EE80.z_frac = actor->position.vz;
    D_8006EE80.z = actor->position.vz >> 12;
    return 1;
}

/* Link the area's scene objects in the listed (parent, child) pairs. */
s32 func_8008868C(void) {
    s32 i;
    s32 base;

    base = D_8009B688[D_8009C610];
    for (i = 0; D_8009AFA0[i] != -1; i += 2) {
        func_800848B4(base + D_8009AFA0[i], base + D_8009AFA0[i + 1]);
    }
    return 1;
}

/* Fly the airship: spin its rotors, stop over the saved landing point when
 * low enough, move, place its shadow object and save the position. */
s32 func_80088720(s32 index) {
    WorldmapActor *actor;
    FlightScratch *scratch;
    s32 base;

    actor = &D_8009BE24[index];
    base = D_8009B688[D_8009C610];
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    actor->unk58 = (actor->unk58 + actor->unk5C) & 0xFFF;
    FLIGHT_SCRATCH->rotor.vx = FLIGHT_SCRATCH->rotor.vz = 0;
    FLIGHT_SCRATCH->rotor.vy = actor->u.step;
    FLIGHT_SCRATCH->tail.vx = FLIGHT_SCRATCH->tail.vz = 0;
    FLIGHT_SCRATCH->tail.vy = actor->unk58;
    func_8004A92C(&FLIGHT_SCRATCH->rotor, &FLIGHT_SCRATCH->rotor_matrix);
    func_8004A92C(&FLIGHT_SCRATCH->tail, &FLIGHT_SCRATCH->tail_matrix);
    D_8009C620[base + 5].matrix = FLIGHT_SCRATCH->rotor_matrix;
    scratch = FLIGHT_SCRATCH;
    D_8009C620[base].matrix = D_8009C620[base + 1].matrix = D_8009C620[base + 2].matrix =
        D_8009C620[base + 3].matrix = FLIGHT_SCRATCH->tail_matrix;
    scratch->work.vx = D_8006EE54.unk60 - (actor->position.vx >> 12);
    scratch->work.vz = D_8006EE54.unk64 - (actor->position.vz >> 12);
    scratch->work.vy = (s16)D_8006EE54.unk62;
    func_80093534(&scratch->work);
    if (SquareRoot0(scratch->work.vx * scratch->work.vx + scratch->work.vz * scratch->work.vz) < 0x300 && scratch->work.vy < -0x240) {
        actor->turn = 0;
    } else {
        actor->turn = 1;
    }
    actor->position.vx += actor->motion.vx * actor->turn;
    actor->position.vz += actor->motion.vz * actor->turn;
    func_80093354(&actor->position);
    scratch->work.vx = actor->position.vx >> 12;
    scratch->work.vy = actor->position.vy >> 12;
    scratch->work.vz = actor->position.vz >> 12;
    D_8009C620[base + 12].position = scratch->work;
    func_8008BFD4(index, &actor->position, 0x180, 0xC0);
    D_8006EE80.x_frac = actor->position.vx;
    D_8006EE80.x = actor->position.vx >> 12;
    D_8006EE80.z_frac = actor->position.vz;
    D_8006EE80.z = actor->position.vz >> 12;
    return 1;
}

/* Link the listed scene objects to object 69 and place it at the actor;
 * outside scene 0x99 also show them. */
s32 func_80088B40(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    s32 i;
    s32 result;

    for (i = 0; D_8009AFDC[i] != -1; i++) {
        func_800848B4(69, D_8009AFDC[i]);
    }
    actor = &D_8009BE24[index];
    object = D_8009C620;
    actor->state = 0;
    if (D_8006EF64[0] == 0x99) {
        result = 1;
        actor->position.vx = 0x2000000;
        actor->position.vz = 0x4120000;
        actor->position.vy = 0;
    } else {
        object[69].visible = 1;
        for (i = 0; D_8009AFDC[i] != -1; i++) {
            object[D_8009AFDC[i]].visible = 1;
        }
        result = 3;
    }
    object += 69;
    object->position.vx = actor->position.vx >> 12;
    object->position.vy = actor->position.vy >> 12;
    object->position.vz = actor->position.vz >> 12;
    return result;
}

/* Show scene object 69 and the listed objects. */
s32 func_80088C90(void) {
    SceneObject *objects;
    s32 i;

    objects = D_8009C620;
    objects[69].visible = 1;
    for (i = 0; D_8009AFDC[i] != -1; i++) {
        objects[D_8009AFDC[i]].visible = 1;
    }
    return 3;
}

/* In scene 0x99, move scene object 69 to the actor (world units). */
s32 func_80088D00(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;

    object = D_8009C620;
    actor = &D_8009BE24[index];
    object += 69;
    if (D_8006EF64[0] == 0x99) {
        object->position.vx = actor->position.vx >> 12;
        object->position.vy = actor->position.vy >> 12;
        object->position.vz = actor->position.vz >> 12;
    }
    return 1;
}

/* Place the actor just above the current area's scene object. */
s32 func_80088D64(s32 index) {
    SceneObject *object;
    WorldmapActor *actor;

    object = &D_8009C620[D_8009B69C[D_8009C610]];
    actor = &D_8009BE24[index];
    actor->position.vx = object->position.vx << 12;
    actor->position.vy = (object->position.vy << 12) + 0x40000;
    actor->position.vz = object->position.vz << 12;
    return 1;
}

/* Place the actor at (0x68, 0x60). */
s32 func_80088DE4(s32 index) {
    func_8008BFD4(index, &D_8009BE24[index].position, 0x68, 0x60);
    return 1;
}

/* Place the actor at scene object 75. */
s32 func_80088E1C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->position.vx = D_8009C620[75].position.vx << 12;
    actor->position.vy = D_8009C620[75].position.vy << 12;
    actor->position.vz = D_8009C620[75].position.vz << 12;
    return 1;
}

/* Place the actor at (0x10C, 0x1A6). */
s32 func_80088E68(s32 index) {
    func_8008BFD4(index, &D_8009BE24[index].position, 0x10C, 0x1A6);
    return 1;
}

/* Place the actor at the current area's scene object. */
s32 func_80088EA0(s32 index) {
    SceneObject *object;
    WorldmapActor *actor;

    object = &D_8009C620[D_8009B6B0[D_8009C610]];
    actor = &D_8009BE24[index];
    actor->position.vx = object->position.vx << 12;
    actor->position.vy = object->position.vy << 12;
    actor->position.vz = object->position.vz << 12;
    return 1;
}

/* Place the actor at (0xC9, 0x392). */
s32 func_80088F1C(s32 index) {
    func_8008BFD4(index, &D_8009BE24[index].position, 0xC9, 0x392);
    return 1;
}

/* Scene step with nothing to do. */
s32 func_80088F54(void) {
    return 3;
}

/* Scene step with nothing to do. */
s32 func_80088F5C(void) {
    return 3;
}

/* Clear the area objects and allocate the effect slots. */
void func_80088F64(void) {
    AreaObject *object;
    EffectSlot *slot;
    s32 i;

    object = D_8009BCC0;
    for (i = 0x1FF; i != -1; i--) {
        object->unk4 = 0;
        object->unkA = 0;
        object->unk12 = 0;
        object->position.vz = 0;
        object->position.vy = 0;
        object->position.vx = 0;
        object->angle.vz = 0;
        object->angle.vy = 0;
        object->angle.vx = 0;
        object++;
    }
    D_8009BDF4 = slot = func_80031BDC(0x4C00, 0);
    for (i = 0xFF; i != -1; i--) {
        EFFECT_ENABLED(slot) = 0;
        EFFECT_COUNT(slot) = 0;
        slot++;
    }
}

/* Free the effect table. */
void func_80088FF4(void) {
    func_800320E8(D_8009BDF4);
}

/* Allocate the two effect quad buffers: semi-transparent textured quads
 * on the 0x340,0x100 page, the second a copy of the first. */
void func_8008901C(void) {
    PolyFT4 *quad;
    s32 i;

    D_8009BE1C[0] = func_80031BDC(sizeof(EffectQuads), 1);
    D_8009BE1C[1] = func_80031BDC(sizeof(EffectQuads), 1);
    quad = D_8009BE1C[0];
    for (i = 0xFF; i != -1; i--) {
        setPolyFT4(quad);
        quad->tpage = GetTPage(1, 1, 0x340, 0x100);
        quad->clut = GetClut(0x100, 0x1FF);
        setSemiTrans(quad, 1);
        quad++;
    }
    *(EffectQuads *)D_8009BE1C[1] = *(EffectQuads *)D_8009BE1C[0];
}

/* Free two effect buffers. */
void func_80089128(void) {
    func_800320E8(D_8009BE1C[0]);
    func_800320E8(D_8009BE1C[1]);
}

/* Place the eight emitters of group `group` at `position` facing `angle`
 * (either may be NULL for zero); start them unless one is already live. */
void func_80089160(s32 group, SVECTOR *position, SVECTOR *angle) {
    AreaObject *object;
    s32 live;
    s32 i;

    live = 0;
    object = &D_8009BCC0[group * 8];
    for (i = 7; i != -1; i--) {
        if (object->flags & 0x80) {
            live++;
            break;
        }
    }
    object = &D_8009BCC0[group * 8];
    if ((position == NULL) & (angle == NULL)) {
        for (i = 7; i != -1; object++, i--) {
            if (live == 0) {
                object->flags |= 0x80;
                object->unk4 = object->unk0;
                object->unk12 = object->unk10;
                object->unkA = 0;
            }
            *(s32 *)&object->position.vx = *(s32 *)&object->angle.vx = 0;
            object->position.vz = object->angle.vz = 0;
        }
    } else if ((position != NULL) & (angle == NULL)) {
        for (i = 7; i != -1; object++, i--) {
            if (live == 0) {
                object->flags |= 0x80;
                object->unk4 = object->unk0;
                object->unk12 = object->unk10;
                object->unkA = 0;
            }
            object->position = *position;
            SVECTOR_ZERO(&object->angle);
        }
    } else if ((position == NULL) & (angle != NULL)) {
        for (i = 7; i != -1; object++, i--) {
            if (live == 0) {
                object->flags |= 0x80;
                object->unk4 = object->unk0;
                object->unk12 = object->unk10;
                object->unkA = 0;
            }
            SVECTOR_ZERO(&object->position);
            object->angle.vx = -angle->vx;
            object->angle.vy = -angle->vy;
            object->angle.vz = -angle->vz;
        }
    } else {
        for (i = 7; i != -1; object++, i--) {
            if (live == 0) {
                object->flags |= 0x80;
                object->unk4 = object->unk0;
                object->unk12 = object->unk10;
                object->unkA = 0;
            }
            object->position = *position;
            object->angle.vx = -angle->vx;
            object->angle.vy = -angle->vy;
            object->angle.vz = -angle->vz;
        }
    }
}

/* Place the eight emitters of group `group` at the origin, aimed along
 * `direction` and turned by `angle`; start them unless one is already live. */
void func_800893E0(s32 group, SVECTOR *direction, SVECTOR *angle) {
    AreaObject *object;
    s32 live;
    s32 i;

    live = 0;
    object = &D_8009BCC0[group * 8];
    for (i = 7; i != -1; i--) {
        if (object->flags & 0x80) {
            live++;
            break;
        }
    }
    object = &D_8009BCC0[group * 8];
    for (i = 7; i != -1; object++, i--) {
        if (live == 0) {
            object->flags |= 0x80;
            object->unk4 = object->unk0;
            object->unk12 = object->unk10;
            object->unkA = 0;
        }
        *(s32 *)&object->direction.vx = *(s32 *)&direction->vx;
        *(s32 *)&object->position.vx = 0;
        *(s32 *)&object->angle.vx = *(s32 *)&angle->vx;
        object->direction.vz = direction->vz;
        object->position.vz = 0;
        object->angle.vz = angle->vz;
    }
}

/* Deactivate the eight area objects of group `group`. */
void func_800894C8(s32 group) {
    AreaObject *object;
    s32 i;

    object = &D_8009BCC0[group * 8];
    for (i = 7; i != -1; i--) {
        object->flags &= 0x7F;
        object++;
    }
}

/* Stop the effects that belong to group `group`. */
void func_80089514(s32 group) {
    EffectSlot *slot;
    s32 i;
    s32 j;

    slot = D_8009BDF4;
    for (i = 0xFF; i != -1; i--) {
        for (j = 0; j < 8; j++) {
            if (slot->id == group * 8 + j && EFFECT_ENABLED(slot) != 0) {
                EFFECT_COUNT(slot) = 0;
                break;
            }
        }
        slot++;
    }
}

/* Age the particles: move, accelerate, spin and fade the live ones; when a
 * particle's life runs out, release it from its emitter. */
void func_80089580(void) {
    EffectSlot *slot;
    s32 step;   /* life word, then the colour step */
    s32 colour; /* live flag, then the colour */
    s32 i;
    s32 r;
    s32 g;
    s32 b;
    s32 px, py, pz;
    s32 vx, vy, vz;

    slot = D_8009BDF4;
    for (i = 0xFF; i != -1; slot++, i--) {
        step = slot->timer;
        colour = step >> 16;
        step = (s16)step;
        if (colour != 0) {
            if (step > 0) {
                px = slot->position.vx;
                py = slot->position.vy;
                pz = slot->position.vz;
                vx = slot->velocity.vx;
                vy = slot->velocity.vy;
                vz = slot->velocity.vz;
                colour = slot->colour;
                EFFECT_COUNT(slot)--;
                step = slot->fade;
                px += vx;
                py += vy;
                pz += vz;
                vx += slot->accel.vx;
                vy += slot->accel.vy;
                vz += slot->accel.vz;
                r = (colour & 0xFF) + (s8)step;
                g = ((colour >> 8) & 0xFF) + (s8)(step >> 8);
                b = ((colour >> 16) & 0xFF) + (s8)(step >> 16);
                slot->rot[0] += slot->spin[0];
                slot->rot[1] += slot->spin[1];
                if (r < 0) {
                    r = 0;
                }
                if (r > 0xFF) {
                    r = 0xFF;
                }
                if (g < 0) {
                    g = 0;
                }
                if (g > 0xFF) {
                    g = 0xFF;
                }
                if (b < 0) {
                    b = 0;
                }
                if (b > 0xFF) {
                    b = 0xFF;
                }
                slot->colour = (colour & 0xFF000000) | (b << 16) | (g << 8) | r;
                slot->position.vx = px;
                slot->position.vy = py;
                slot->position.vz = pz;
                slot->velocity.vx = vx;
                slot->velocity.vy = vy;
                slot->velocity.vz = vz;
            } else {
                D_8009BCC0[slot->id].unkA--;
                slot->id = 0;
                slot->timer = 0;
            }
        }
    }
}

/* Run the emitters: count down their timers and every interval spawn one
 * particle into a free effect slot, starting at a random point around the
 * emitter and flying towards a random point around its target. */
void func_80089748(void) {
    AreaObject *object;
    EffectSlot *slot;
    EmitScratch *scratch;
    s32 i;
    s32 flags;
    /* The original initializes these timer locals only for flag 0x10. */
    s32 delay;
    s32 repeats;
    s32 value;

    object = D_8009BCC0;
    scratch = EMIT_SCRATCH;
    for (i = 0; i < 0x200; i++, object++) {
        flags = object->flags;
        if (!(flags & 0x80)) {
            continue;
        }
        if (flags & 0x10) {
            delay = object->unk4;
            repeats = delay >> 16;
            delay = (s16)delay;
            if (delay != 0) {
                delay--;
                goto store;
            }
            if (repeats == 0) {
                goto expire;
            }
            repeats--;
        }
        if (object->unk12 != 0) {
            object->unk12--;
            goto store;
        }
        object->unk12 = object->unk10;
        if ((((s16 *)&object->life)[1] != 0) & (object->unk8 > 0) & (object->unkA < object->unk8)) {
            value = 0xFF;
            slot = D_8009BDF4;
            for (; value != -1; value--, slot++) {
                if (EFFECT_ENABLED(slot) == 0) {
                    slot->id = i;
                    slot->timer = object->life;
                    func_8004A92C(&object->angle, &scratch->m);
                    ApplyMatrix(&scratch->m, &object->unk24, &scratch->offset);
                    if (!(flags & 0x20)) {
                        scratch->random.vy = (rand() & 0xFFF) - 0x800;
                    } else {
                        scratch->random.vy = 0;
                    }
                    scratch->random.vx = (rand() & 0xFFF) - 0x800;
                    scratch->random.vz = (rand() & 0xFFF) - 0x800;
                    VectorNormal(&scratch->random, &scratch->normal);
                    if (flags & 4) {
                        value = object->spread[0];
                    } else {
                        value = rand() % object->spread[0];
                    }
                    slot->position.vx = (scratch->offset.vx << 12) + scratch->normal.vx * value +
                                        (object->position.vx << 12);
                    slot->position.vy = (scratch->offset.vy << 12) + scratch->normal.vy * value +
                                        (object->position.vy << 12);
                    slot->position.vz = (scratch->offset.vz << 12) + scratch->normal.vz * value +
                                        (object->position.vz << 12);
                    ApplyMatrix(&scratch->m, &object->direction, &scratch->offset);
                    if (!(flags & 0x40)) {
                        scratch->random.vy = (rand() & 0xFFF) - 0x800;
                    } else {
                        scratch->random.vy = 0;
                    }
                    scratch->random.vx = (rand() & 0xFFF) - 0x800;
                    scratch->random.vz = (rand() & 0xFFF) - 0x800;
                    VectorNormal(&scratch->random, &scratch->normal);
                    if (flags & 8) {
                        value = object->spread[1];
                    } else {
                        value = rand() % object->spread[1];
                    }
                    scratch->normal.vx = (scratch->offset.vx << 12) + scratch->normal.vx * value +
                                         (object->position.vx << 12);
                    scratch->normal.vy = (scratch->offset.vy << 12) + scratch->normal.vy * value +
                                         (object->position.vy << 12);
                    scratch->normal.vz = (scratch->offset.vz << 12) + scratch->normal.vz * value +
                                         (object->position.vz << 12);
                    scratch->normal.vx = (scratch->normal.vx - slot->position.vx) >> 12;
                    scratch->normal.vy = (scratch->normal.vy - slot->position.vy) >> 12;
                    scratch->normal.vz = (scratch->normal.vz - slot->position.vz) >> 12;
                    VectorNormal(&scratch->normal, &scratch->random);
                    slot->velocity.vx = (scratch->random.vx * object->speed) >> 12;
                    slot->velocity.vy = (scratch->random.vy * object->speed) >> 12;
                    slot->velocity.vz = (scratch->random.vz * object->speed) >> 12;
                    flags &= 3; /* only the blend mode is used from here */
                    slot->unk2 = ratan2(scratch->random.vy, scratch->random.vx);
                    slot->accel.vx = object->accel[0];
                    slot->accel.vy = object->accel[1];
                    slot->accel.vz = object->accel[2];
                    slot->colour = *(s32 *)object->rgb;
                    slot->fade = object->fade;
                    *(s32 *)slot->rot = object->rot;
                    *(s32 *)slot->spin = object->spin;
                    slot->code = (flags << 5) | 0x9D;
                    object->unkA++;
                    goto store;
                }
            }
        }
        goto store;
    expire:
        object->flags ^= 0x80;
    store:
        object->unk4 = (repeats << 16) | delay;
    }
    func_80089580();
}

/* Draw the live particles: build each one's billboard quad (kind shape,
 * scaled and optionally rolled), place it relative to the camera target,
 * project it and add the visible ones to the ordering table. */
void func_80089C78(void) {
    ParticleScratch *scratch;
    EffectSlot *slot;
    PolyFT4 *quad;
    s32 camera_x;
    s32 camera_z;
    s32 i;

    scratch = (ParticleScratch *)0x1F800000;
    PARTICLE_SCRATCH->view = D_8009C808;
    PARTICLE_SCRATCH->identity = *(MATRIX *)&D_8009A180;
    i = 0;
    camera_x = D_8009BE28.target.vx >> 12;
    camera_z = D_8009BE28.target.vz >> 12;
    quad = D_8009BE1C[D_8009D7F0];
    slot = D_8009BDF4;
    for (; i < 0x100; i++, slot++) {
        if (EFFECT_ENABLED(slot) == 0) {
            continue;
        }
        scratch->scale.vx = (u16)slot->rot[0];
        scratch->scale.vy = (u16)slot->rot[1];
        scratch->scale.vz = 0x1000;
        scratch->m = scratch->identity;
        if (((u8 *)&slot->fade)[3] & 1) {
            RotMatrixZ(slot->unk2, &scratch->m);
        }
        ScaleMatrix(&scratch->m, &scratch->scale);
        scratch->v[0] = D_8009B040[EFFECT_ENABLED(slot)].v[0];
        scratch->v[1] = D_8009B040[EFFECT_ENABLED(slot)].v[1];
        scratch->v[2] = D_8009B040[EFFECT_ENABLED(slot)].v[2];
        scratch->v[3] = D_8009B040[EFFECT_ENABLED(slot)].v[3];
        scratch->offset.vx = (slot->position.vx >> 12) - camera_x;
        scratch->offset.vz = (slot->position.vz >> 12) - camera_z;
        func_80093534(&scratch->offset);
        scratch->centre.vx = scratch->offset.vx;
        scratch->centre.vy = slot->position.vy >> 12;
        scratch->centre.vz = -scratch->offset.vz;
        gte_SetRotMatrix(&scratch->view);
        gte_ldv0(&scratch->centre);
        gte_rtv0();
        gte_stlvnl(&scratch->offset);
        scratch->m.t[0] = scratch->offset.vx + scratch->view.t[0];
        scratch->m.t[1] = scratch->offset.vy + scratch->view.t[1];
        scratch->m.t[2] = scratch->offset.vz + scratch->view.t[2];
        gte_SetRotMatrix(&scratch->m);
        gte_SetTransMatrix(&scratch->m);
        gte_ldv3(&scratch->v[0], &scratch->v[1], &scratch->v[2]);
        gte_rtpt();
        gte_stflg(&scratch->flag);
        if (scratch->flag & 0x80000000) {
            continue;
        }
        gte_stsxy3(&quad->x0, &quad->x1, &quad->x2);
        gte_ldv0(&scratch->v[3]);
        gte_rtps();
        gte_stflg(&scratch->flag);
        if (scratch->flag & 0x80000000) {
            continue;
        }
        gte_stsxy(&quad->x3);
        if (!(quad->x0 < 0x140 || quad->x1 < 0x140 || quad->x2 < 0x140 || quad->x3 < 0x140)) {
            continue;
        }
        if (!(quad->y0 < 0xD8 || quad->y1 < 0xD8 || quad->y2 < 0xD8 || quad->y3 < 0xD8)) {
            continue;
        }
        gte_stsz(&scratch->sz);
        if (scratch->sz < 0xC00) {
            quad->r0 = ((u8 *)&slot->colour)[0];
            quad->g0 = ((u8 *)&slot->colour)[1];
            quad->b0 = ((u8 *)&slot->colour)[2];
            quad->tpage = slot->code;
            *(u16 *)&quad->u0 = D_8009AFF0[EFFECT_ENABLED(slot) * 4];
            *(u16 *)&quad->u1 = D_8009AFF0[EFFECT_ENABLED(slot) * 4 + 1];
            *(u16 *)&quad->u2 = D_8009AFF0[EFFECT_ENABLED(slot) * 4 + 2];
            *(u16 *)&quad->u3 = D_8009AFF0[EFFECT_ENABLED(slot) * 4 + 3];
            addPrim(&D_8009BE3C->ot[scratch->sz >> 4], quad);
            quad++;
        }
    }
}

/* Create the party leader's model sprite at the scene's entry position; in
 * movement modes 1-7 follow the player or start hidden. Fill the position
 * trail and save the position as the world-map return point. */
s32 func_8008A2C8(s32 index) {
    WorldmapActor *actor;
    TrailPoint *point;
    s32 i;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009CD34[0], 0x100, 0x1E0, 0x140, 0x100, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x1800);
    ((s32 *)actor->handle)[15] &= ~4;
    actor->unk24 = 0;
    actor->position.vx = D_8006EF64[0] << 12;
    actor->position.vz = D_8006EF64[1] << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->heading = D_8009C584;
    actor->turn = 8;
    actor->unk5C = actor->heading;
    switch (D_8009BE10) {
    case 1:
    case 2:
    case 3:
        if (D_8006F8E5 == 0) {
            actor->position.vx = D_8009C5AC.vx;
            actor->position.vz = D_8009C5AC.vz;
            actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
            actor->heading = D_8009C584;
            D_8009D55C.target = actor->position;
            D_8009D52C = actor->heading;
        } else {
            actor->state = 1;
            actor->unk24 = 1;
        }
        break;
    case 4:
    case 5:
    case 6:
        actor->state = 3;
        actor->unk24 = 1;
        if (D_8006F368[0] != 0xFF) {
            D_8006F8E5 = 1;
        }
        break;
    case 7:
        actor->state = 2;
        actor->unk24 = 1;
        if (D_8006F368[0] != 0xFF) {
            D_8006F8E5 = 1;
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        break;
    }
    D_8009D154 = 0;
    i = 0;
    point = D_8009CEC4;
    do {
        point->position = actor->position;
        i++;
        point->heading = actor->heading;
        point++;
    } while (i < 0x20);
    D_8006EE54.x = actor->position.vx >> 12;
    D_8006EE54.z = actor->position.vz >> 12;
    D_8006EE54.heading = actor->heading;
    return 1;
}

/* Create the lead party member's model sprite for the actor. */
s32 func_8008A52C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009CD34[0], 0x100, 0x1E0, 0x140, 0x100, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x1800);
    ((s32 *)actor->handle)[15] &= ~4;
    return 1;
}

/* Create the party leader's model sprite at the saved world-map position
 * and fill the position trail and saved camera target with it. */
s32 func_8008A5B8(s32 index) {
    WorldmapActor *actor;
    TrailPoint *point;
    s32 i;
    s16 heading;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009CD34[0], 0x100, 0x1E0, 0x140, 0x100, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x1800);
    ((s32 *)actor->handle)[15] &= ~4;
    actor->position.vx = D_8006EE54.x << 12;
    actor->position.vz = D_8006EE54.z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    i = 0;
    point = D_8009CEC4;
    heading = D_8006EE54.heading;
    actor->unk24 = 1;
    actor->turn = 8;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->state = 2;
    D_8009D154 = 0;
    actor->heading = heading;
    do {
        point->position = actor->position;
        i++;
        point->heading = actor->heading;
        point++;
    } while (i < 0x20);
    D_8009D55C.target = actor->position;
    D_8009D52C = actor->heading;
    return 1;
}

/* Update the party leader on foot: walk by the pad and record the trail,
 * gather the others into a vehicle or let them out on command, walk out of
 * the parked vehicle, and save the return spot and heading. */
s32 func_8008A72C(s32 index) {
    WorldmapActor *actor;
    WorldmapActor *target;
    LeaderScratch *scratch;
    TrailPoint *point;
    s32 result;
    s16 value;
    u16 heading;

    result = 1;
    scratch = (LeaderScratch *)0x1F800000;
    actor = &D_8009BE24[index];
    switch (actor->unk4) {
    case 2:
        actor->unk4 = 0;
        actor->state = 0x28;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 6:
        actor->unk4 = 0;
        if (D_8009C170 == ++actor->unk58) {
            actor->state = 0;
            D_8009BE10 = 1;
            D_8009BD04 = 0;
        }
        break;
    }
    switch (actor->state) {
    case 0:
    case 1:
        if (D_8006F8E5 == 0) {
            switch (func_80090A84(actor)) {
            case 2:
            case 4:
            case 5:
                break;
            case 1:
                actor->state = 0x40;
                D_8009D554 = 0;
                D_8009D7CC = 0;
                break;
            case 3:
                if (D_8009BD60 == 7) {
                    actor->state = 8;
                } else {
                    D_8009BD60 = 4;
                    actor->state = 0x10;
                }
                break;
            default:
                if ((actor->motion.vx == 0) & (actor->motion.vy == 0) & (actor->motion.vz == 0)) {
                    if (((ModelInstance *)actor->handle)->animation != 0) {
                        func_800245D8(actor->handle, 0);
                        func_800894C8(0x2F);
                    }
                } else {
                    if (((ModelInstance *)actor->handle)->animation != 1) {
                        func_800245D8(actor->handle, 1);
                    }
                    func_8008C1DC(0x2F, actor, (ActorScratch *)scratch);
                }
                value = func_80095414(&actor->position, &actor->motion, &scratch->probe, actor->turn << 12,
                                    D_8009BE10);
                if (value == 0) {
                    actor->motion = scratch->probe;
                    value = func_80095414(&actor->position, &actor->motion, &scratch->probe, actor->turn << 12,
                                        D_8009BE10);
                    if (value == 0) {
                        actor->motion.vz = 0;
                        actor->motion.vx = 0;
                    }
                }
                if (value == 1) {
                    func_8008C040(&scratch->probe, 0x10, 0x20, &D_8009D738, &D_8009BD60);
                    if (D_8009BD60 == 7) {
                        value = D_8009D738 + 3;
                    } else {
                        value = D_8009D738;
                    }
                    if ((u16)D_8009B180[value] != 0) {
                        actor->position = scratch->probe;
                        if (actor->motion.vx | actor->motion.vz) {
                            value = (D_8009D154 + 1) & 0x1F;
                            point = &D_8009CEC4[value];
                            D_8009D154 = value;
                            point->position = actor->position;
                            point->heading = actor->heading;
                            func_8007528C();
                        }
                    }
                } else {
                    func_8008C040(&actor->position, 0x10, 0x20, &D_8009D738, &D_8009BD60);
                }
                func_80094238(&actor->position, 0);
                actor->motion.vz = 0;
                actor->motion.vy = 0;
                actor->motion.vx = 0;
                D_8009D55C.target = actor->position;
                D_8009D52C = actor->heading;
                break;
            }
            D_8009BD04 = 0;
            actor->unk24 = 0;
        } else {
            actor->unk24 = 1;
            actor->position.vx = D_8009BE24[4].position.vx;
            actor->position.vy = D_8009BE24[4].position.vy;
            actor->position.vz = D_8009BE24[4].position.vz;
            actor->heading = D_8009BE24[4].heading;
            func_800894C8(0x2F);
        }
        break;
    case 2:
    case 3:
        actor->position.vx = D_8009BE24[7].position.vx;
        actor->position.vy = D_8009BE24[7].position.vy;
        actor->position.vz = D_8009BE24[7].position.vz;
        actor->heading = D_8009BE24[7].heading;
        break;
    case 8:
        if (D_8006F368[1] != 0xFF) {
            if (D_8006F8E6 == 0) {
                func_80097770(2, 1);
                actor[1].unk6 = D_8009BD60;
                func_80097770(5, 8);
            } else {
                func_80097770(5, 1);
                D_8009BE24[5].unk6 = D_8009BD60;
            }
        }
        actor->state++;
        break;
    case 9:
        if (D_8006F368[2] != 0xFF) {
            if (D_8006F8E7 == 0) {
                func_80097770(3, 1);
                actor[2].unk6 = D_8009BD60;
                func_80097770(6, 8);
            } else {
                func_80097770(6, 1);
                D_8009BE24[6].unk6 = D_8009BD60;
            }
        }
        actor->state++;
        break;
    case 10:
        if (D_8006F368[0] != 0xFF) {
            if (func_80097770(4, 8) != 0) {
                actor->state = 0xD;
            }
        } else {
            actor->state = 0xD;
        }
        break;
    case 0xD:
        func_800941C4(&actor->position, &D_8009BE24[D_8009BD60].position, &actor->motion, &actor->heading);
        target = (WorldmapActor *)((D_8009BD60 * sizeof(*target)) + (u32)D_8009BE24);
        actor->u.step = target->position.vx >> 12;
        actor->unk54 = target->position.vz >> 12;
        func_800245D8(actor->handle, 1);
        actor->state++;
        /* fallthrough */
    case 0xE:
        if (func_8008BEC8(actor) == 3) {
            actor->state++;
        }
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        func_8008C1DC(index + 0x2E, actor, (ActorScratch *)scratch);
        break;
    case 0xF:
        if (func_80097770(D_8009BD60, 4) != 0) {
            actor->unk24 = 1;
            actor->state = 2;
            func_800894C8(0x2F);
        }
        break;
    case 0x10:
        if (D_8006F368[1] != 0xFF) {
            if (func_80097770(2, 1) != 0) {
                actor[1].unk6 = 5;
                actor->state++;
            }
        } else {
            actor->state++;
        }
        break;
    case 0x11:
        if (D_8006F368[2] != 0xFF) {
            if (func_80097770(3, 1) != 0) {
                actor[2].unk6 = 6;
                actor->state++;
            }
        } else {
            actor->state++;
        }
        break;
    case 0x12:
        point = D_8009CEC4;
        D_8009D154 = 0;
        scratch->start.vx = D_8006EF8E[0].x << 12;
        scratch->start.vz = D_8006EF8E[0].z << 12;
        scratch->start.vy = func_80093978(scratch->start.vx, scratch->start.vz);
        scratch->heading = D_8006EE54.unk5A;
        value = 0x1F;
        do {
            point->position = scratch->start;
            point->heading = scratch->heading;
            point++;
        } while (--value != -1);
        actor->state = 0xD;
        break;
    case 0x28:
        actor->position.vx = D_8006EF8E[0].x << 12;
        actor->position.vz = D_8006EF8E[0].z << 12;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        heading = D_8006EE54.unk5A;
        actor->unk5C = heading;
        actor->heading = heading;
        scratch->target.vx = actor->position.vx + func_8003F8B0(actor->heading) * 0x30;
        scratch->target.vz = actor->position.vz + -func_8003F8CC(actor->heading) * 0x30;
        func_800941C4(&actor->position, &scratch->target, &actor->motion, &actor->heading);
        actor->u.step = scratch->target.vx >> 12;
        actor->unk54 = scratch->target.vz >> 12;
        actor->unk24 = 0;
        func_800245D8(actor->handle, 1);
        actor->state++;
        break;
    case 0x29:
        if (func_8008BEC8(actor) == 3) {
            func_800245D8(actor->handle, 0);
            actor->motion.vz = 0;
            actor->motion.vy = 0;
            actor->motion.vx = 0;
            actor->state++;
        }
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0x2A:
        point = D_8009CEC4;
        D_8006F8E5 = 0;
        actor->unk58 = 1;
        D_8009D154 = 0;
        scratch->target = actor->position;
        scratch->heading = actor->heading;
        value = 0x1F;
        do {
            point->position = scratch->target;
            point->heading = scratch->heading;
            point++;
        } while (--value != -1);
        actor->state++;
        /* fallthrough */
    case 0x2B:
        switch (D_8009C170) {
        case 1:
            actor->state = 1;
            break;
        case 2:
            if (D_8006F8E6 == 0) {
                actor->state++;
            }
            break;
        case 3:
            if ((D_8006F8E6 | D_8006F8E7) == 0) {
                actor->state++;
            }
            break;
        }
        break;
    case 0x2C:
        if (D_8006F368[1] != 0xFF) {
            if (func_80097770(2, 5) != 0) {
                actor->state++;
            }
        } else {
            actor->state++;
        }
        break;
    case 0x2D:
        if (D_8006F368[2] == 0xFF || func_80097770(3, 5) != 0) {
            actor->state = 0x40;
        }
        break;
    case 0x40:
        break;
    }
    D_8006EE54.x = actor->position.vx >> 12;
    D_8006EE54.z = actor->position.vz >> 12;
    D_8006EE54.heading = actor->heading;
    if (actor->unk24 == 0) {
        func_80074794(0, &actor->position);
    }
    return result;
}

/* Create party member 2's model sprite (if present) at the saved world-map
 * position; in movement modes 1-7 follow the player or start hidden. */
s32 func_8008B2BC(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    if (D_8006F368[1] != 0xFF) {
        actor->handle = func_80024524(D_8009CD34[1], 0x110, 0x1E0, 0x150, 0x100, 0x40);
        func_800245D8(actor->handle, 0);
        func_80022000(actor->handle, 0x1800);
        ((s32 *)actor->handle)[15] &= ~4;
        actor->unk24 = 0;
    } else {
        actor->unk24 = 1;
        result = 3;
    }
    actor->position.vx = D_8006EE54.x << 12;
    actor->position.vz = D_8006EE54.z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->heading = D_8006EE54.heading;
    actor->turn = 8;
    actor->unk58 = 0xF;
    actor->unk5C = actor->heading;
    switch (D_8009BE10) {
    case 1:
    case 2:
    case 3:
        if (D_8006F8E6 == 0) {
            actor->position.vx = D_8009C5AC.vx;
            actor->position.vz = D_8009C5AC.vz;
            actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
            actor->heading = D_8009C584;
        } else {
            actor->state = 1;
            actor->unk24 = 1;
        }
        break;
    case 4:
    case 5:
    case 6:
        actor->state = 3;
        actor->unk24 = 1;
        if (D_8006F368[1] != 0xFF) {
            D_8006F8E6 = 1;
        }
        break;
    case 7:
        actor->state = 2;
        actor->unk24 = 1;
        if (D_8006F368[1] != 0xFF) {
            D_8006F8E6 = 1;
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        break;
    }
    return result;
}

/* Create the second party member's model sprite, if present. */
s32 func_8008B498(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    if (D_8006F368[1] != 0xFF) {
        actor->handle = func_80024524(D_8009CD34[1], 0x110, 0x1E0, 0x150, 0x100, 0x40);
        func_800245D8(actor->handle, 0);
        func_80022000(actor->handle, 0x1800);
        ((s32 *)actor->handle)[15] &= ~4;
    } else {
        result = 3;
    }
    return result;
}

/* Create party member 2's model sprite at the saved world-map position. */
s32 func_8008B54C(s32 index) {
    WorldmapActor *actor;
    s16 heading;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009CD34[1], 0x110, 0x1E0, 0x150, 0x100, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x1800);
    ((s32 *)actor->handle)[15] &= ~4;
    actor->position.vx = D_8006EE54.x << 12;
    actor->position.vz = D_8006EE54.z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    heading = D_8006EE54.heading;
    actor->unk24 = 1;
    actor->turn = 8;
    actor->state = 2;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk58 = 0xF;
    actor->heading = heading;
    return 1;
}

/* Party follower: a command (1-3, 5) starts a scripted walk; otherwise
 * follow the vehicle trail a fixed delay behind (or stand at the leader
 * while riding), walk to a target, walk out of or back to the vehicle,
 * and keep the model on the ground. */
s32 func_8008B644(s32 index) {
    WorldmapActor *actor;
    WorldmapActor *target;
    TrailPoint *point;
    VECTOR *work;
    s32 result;
    u16 heading;
    VECTOR unused; /* unreferenced; the original frame reserves it */

    result = 1;
    work = (VECTOR *)0x1F800000;
    actor = &D_8009BE24[index];
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 8;
        target = &D_8009BE24[actor->unk6];
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 0x28;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 5:
        actor->unk4 = 0;
        actor->state = 0x30;
        break;
    }
    switch (actor->state) {
    case 0:
    case 1:
        if (D_8006F8E4[index] == 0) {
            point = &D_8009CEC4[(D_8009D154 - actor->unk58) & 0x1F];
            if ((actor->position.vx == point->position.vx) & (actor->position.vy == point->position.vy) &
                (actor->position.vz == point->position.vz)) {
                if (((s8 *)actor->handle)[0xAF] != 0) {
                    func_800245D8(actor->handle, 0);
                    func_800894C8(index + 0x2E);
                }
            } else {
                if (((s8 *)actor->handle)[0xAF] != 1) {
                    func_800245D8(actor->handle, 1);
                }
                func_8008C1DC(index + 0x2E, actor, (ActorScratch *)work);
            }
            actor->position.vx = point->position.vx;
            actor->position.vy = point->position.vy;
            actor->position.vz = point->position.vz;
            actor->heading = point->heading;
            actor->unk24 = 0;
        } else {
            target = &D_8009BE24[index + 3];
            actor->position.vx = target->position.vx;
            actor->position.vy = target->position.vy;
            actor->position.vz = target->position.vz;
            actor->heading = target->heading;
            actor->unk24 = 1;
            func_800894C8(index + 0x2E);
        }
        break;
    case 2:
        actor->position.vx = D_8009BE24[7].position.vx;
        actor->position.vy = D_8009BE24[7].position.vy;
        actor->position.vz = D_8009BE24[7].position.vz;
        actor->heading = D_8009BE24[7].heading;
        break;
    case 8:
        func_800941C4(&actor->position, &target->position, &actor->motion, &actor->heading);
        actor->u.step = target->position.vx >> 12;
        actor->unk54 = target->position.vz >> 12;
        func_800245D8(actor->handle, 1);
        actor->state++;
        /* fallthrough */
    case 9:
        if (func_8008BEC8(actor) == 3) {
            actor->state++;
        }
        func_8008C1DC(index + 0x2E, actor, (ActorScratch *)work);
        break;
    case 10:
        if (func_80097770(actor->unk6, 4) != 0) {
            actor->unk24 = 1;
            actor->state = 2;
            func_800894C8(index + 0x2E);
        }
        break;
    case 0x28:
        actor->position.vx = D_8006EF8A[index].x << 12;
        actor->position.vz = D_8006EF8A[index].z << 12;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        heading = D_8006EE58[index];
        actor->unk5C = heading;
        actor->heading = heading;
        work->vx = actor->position.vx + func_8003F8B0(actor->heading) * 0x30;
        work->vz = actor->position.vz + -func_8003F8CC(actor->heading) * 0x30;
        func_800941C4(&actor->position, work, &actor->motion, &actor->heading);
        actor->u.step = work->vx >> 12;
        actor->unk54 = work->vz >> 12;
        actor->unk24 = 0;
        func_800245D8(actor->handle, 1);
        actor->state++;
        goto walk;
    case 0x2A:
        D_8006F8E4[index] = 0;
        actor->state = 0x40;
        break;
    case 0x30:
        func_800941C4(&actor->position, &D_8009BE24[1].position, &actor->motion, &actor->heading);
        actor->u.step = D_8009BE24[1].position.vx >> 12;
        actor->unk54 = D_8009BE24[1].position.vz >> 12;
        func_800245D8(actor->handle, 1);
        actor->state++;
        /* fallthrough */
    case 0x29:
    case 0x31:
    walk:
        if (func_8008BEC8(actor) == 3) {
            actor->state++;
        }
        break;
    case 0x32:
        if (func_80097770(1, 6) != 0) {
            actor->state = 0;
        }
        break;
    case 0x40: /* parked */
        break;
    }
    if (actor->unk24 == 0) {
        func_80074794(0, &actor->position);
    }
    return result;
}

/* Create party member 3's model sprite (if present) at the saved world-map
 * position; in movement modes 1-7 follow the player or start hidden. */
s32 func_8008BB40(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    if (D_8006F368[2] != 0xFF) {
        actor->handle = func_80024524(D_8009CD34[2], 0x120, 0x1E0, 0x160, 0x100, 0x40);
        func_800245D8(actor->handle, 0);
        func_80022000(actor->handle, 0x1800);
        ((s32 *)actor->handle)[15] &= ~4;
        actor->unk24 = 0;
    } else {
        actor->unk24 = 1;
        result = 3;
    }
    actor->position.vx = D_8006EE54.x << 12;
    actor->position.vz = D_8006EE54.z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->heading = D_8006EE54.heading;
    actor->turn = 8;
    actor->unk58 = 0x1E;
    actor->unk5C = actor->heading;
    switch (D_8009BE10) {
    case 1:
    case 2:
    case 3:
        if (D_8006F8E7 == 0) {
            actor->position.vx = D_8009C5AC.vx;
            actor->position.vz = D_8009C5AC.vz;
            actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
            actor->heading = D_8009C584;
        } else {
            actor->state = 1;
            actor->unk24 = 1;
        }
        break;
    case 4:
    case 5:
    case 6:
        actor->state = 3;
        actor->unk24 = 1;
        if (D_8006F368[2] != 0xFF) {
            D_8006F8E7 = 1;
        }
        break;
    case 7:
        actor->state = 2;
        actor->unk24 = 1;
        if (D_8006F368[2] != 0xFF) {
            D_8006F8E7 = 1;
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        break;
    }
    return result;
}

/* Create the third party member's model sprite, if present. */
s32 func_8008BD1C(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    if (D_8006F368[2] != 0xFF) {
        actor->handle = func_80024524(D_8009CD34[2], 0x120, 0x1E0, 0x160, 0x100, 0x40);
        func_800245D8(actor->handle, 0);
        func_80022000(actor->handle, 0x1800);
        ((s32 *)actor->handle)[15] &= ~4;
    } else {
        result = 3;
    }
    return result;
}

/* Create party member 3's model sprite at the saved world-map position. */
s32 func_8008BDD0(s32 index) {
    WorldmapActor *actor;
    s16 heading;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009CD34[2], 0x120, 0x1E0, 0x160, 0x100, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x1800);
    ((s32 *)actor->handle)[15] &= ~4;
    actor->position.vx = D_8006EE54.x << 12;
    actor->position.vz = D_8006EE54.z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    heading = D_8006EE54.heading;
    actor->unk24 = 1;
    actor->turn = 8;
    actor->state = 2;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk58 = 0x1E;
    actor->heading = heading;
    return 1;
}

/* Step the actor towards its target (x, z in world units) at half its
 * speed; bit 0/1 of the result report x/z arrival. */
s32 func_8008BEC8(WorldmapActor *actor) {
    s32 arrived;
    s32 distance;
    s32 position;
    s32 cell;

    position = actor->position.vx;
    cell = position >> 12;
    distance = actor->u.step - cell;
    arrived = 0;
    if (ABS(distance) >= 5) {
        actor->position.vx = position + actor->motion.vx * (actor->turn / 2);
    } else {
        arrived = 1;
    }
    position = actor->position.vz;
    cell = position >> 12;
    distance = actor->unk54 - cell;
    if (ABS(distance) >= 5) {
        actor->position.vz = position + actor->motion.vz * (actor->turn / 2);
    } else {
        arrived |= 2;
    }
    func_80093354(&actor->position);
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    return arrived;
}

/* Queue a placement of actor `index` at `position`, facing (x, z). */
void func_8008BFD4(s32 index, VECTOR *position, s32 x, s32 z) {
    PlaceRequest *request;

    request = &D_8009BE6C[D_8009BD04];
    request->actor = index;
    request->px = position->vx;
    request->py = position->vy;
    request->pz = position->vz;
    request->z = (s16)z;
    request->x = x;
    D_8009BD04 = (D_8009BD04 + 1) & 0x1F;
}

/* Find the first queued placement that overlaps a cylinder at `position`
 * (radius, height): *hit is 2 inside its radius, 1 within 16 more units. */
void func_8008C040(VECTOR *position, s32 radius, s32 height, u8 *hit, u8 *actor) {
    VECTOR delta;
    VECTOR unused; /* unreferenced; the original frame reserves it */
    PlaceRequest *request;
    s32 i;
    s32 top;
    s32 bottom;
    s32 distance;
    s32 reach;

    *hit = 0;
    *actor = 0;
    request = D_8009BE6C;
    if (D_8009BD04 <= 0) {
        return;
    }
    for (i = 0; i < D_8009BD04; i++, request++) {
        top = position->vy;
        if (top < request->py) {
            bottom = top - (height << 12);
            top = request->py;
        } else {
            bottom = request->py - (request->z << 12);
        }
        if ((top - bottom) >> 12 < height + request->z) {
            delta.vx = (request->px - position->vx) >> 12;
            delta.vz = (request->pz - position->vz) >> 12;
            func_80093534(&delta);
            distance = SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz);
            reach = request->x + radius;
            if (distance < reach) {
                *hit = 2;
                *actor = request->actor;
                return;
            }
            if (distance < reach + 0x10) {
                *hit = 1;
                *actor = request->actor;
                return;
            }
        }
    }
}

/* Emit effect `effect` at the actor while it stands on terrain type 3,
 * otherwise stop the effect group. */
void func_8008C1DC(s32 effect, WorldmapActor *actor, ActorScratch *scratch) {
    if (func_80093F18(&actor->position) == 3) {
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->angle.vz = 0;
        scratch->angle.vx = 0;
        scratch->angle.vy = actor->unk5C;
        func_80089160(effect, &scratch->position, &scratch->angle);
        return;
    }
    func_800894C8(effect);
}

/* Create a party member's gear sprite for the actor. */
void func_8008C28C(WorldmapActor *actor, s32 member) {
    actor->handle = func_80024524(D_8009BDF8[member], D_8009B18C[member], D_8009B194[member],
                                  D_8009B19C[member], D_8009B1A4[member], 0x40);
    if ((&D_8006F8E5)[member] == 1) {
        func_800245D8(actor->handle, 0);
    } else {
        func_800245D8(actor->handle, 3);
    }
    func_80022000(actor->handle, 0x2000);
    ((s32 *)actor->handle)[15] &= ~4;
}
