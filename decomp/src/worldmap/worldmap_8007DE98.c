#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007DE98);

/* Start a scripted camera at the player position (step 0, speed 0x40). */
s32 func_8007E450(s32 index) {
    WorldmapActor *actor;

    D_8009D144 = 0;
    actor = &D_8009BE24[index];
    actor->unk7C = 0x1000;
    D_8009BE0C = 0x78;
    D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    actor->unk4 = 1;
    actor->unk58 = 0x40;
    actor->state = 0;
    actor->u.step = 0;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007E4E4);

/* Set up `count` translucent blue textured quads of a scene object and copy them to its second buffer. */
#ifdef NON_MATCHING /* loop pointer biased to b0 instead of the code byte */
void func_8007EBBC(SceneObject *object, PolyFT4 *quads, s32 count, s32 abr) {
    s32 i;

    for (i = 0; i < count; i++) {
        setPolyFT4(quads);
        quads->tpage = GetTPage(0, abr, 0x300, 0x100);
        quads->clut = GetClut(0, 0x1FF);
        setRGB0(quads, 0x3C, 0x3C, 0xC0);
        setSemiTrans(quads, 1);
        quads++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(PolyFT4));
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007EBBC);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007ECA4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007EE34);

/* Build scene object `index` and start its fall. */
s32 func_8007F8AC(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    s32 slot;
    s32 abr;

    slot = index - 4;
    actor = &D_8009BE24[index];
    object = &D_8009C620[slot + 4];
    abr = 3;
    if (slot == 4) {
        abr = 1;
    }
    func_8007EBBC(object, object->prims, object->def->count, abr);
    actor->motion.vx = -0x85A;
    actor->motion.vz = 0xDA6;
    actor->state = 0;
    actor->motion.vy = 0;
    actor->unk5C = D_8009A68C[slot];
    actor->wait = 0x3C;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007F968);

/* Rebuild scene objects 9 and 10 and start a descent at a fixed point. */
#ifdef NON_MATCHING /* return value loaded before the stores */
s32 func_8007FC8C(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;

    objects = D_8009C620;
    actor = &D_8009BE24[index];
    func_8007A06C(&objects[9], objects[9].prims, objects[9].def->count);
    func_8007A06C(&objects[10], objects[10].prims, objects[10].def->count);
    actor->position.vx = 0x1498000;
    actor->position.vy = -0x80000;
    actor->position.vz = 0x4AF2000;
    actor->state = 0;
    actor->u.step = 0;
    actor->unk54 = -0x800;
    actor->unk58 = 0x80;
    return 3;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007FC8C);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007FD30);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007FF70);

/* Leave the world map for scene 0x84 (flag word 2). */
void func_80080218(void) {
    func_80039FF8();
    func_8003852C(D_8006259C);
    func_800320E8(D_8006259C);
    func_80084818();
    func_80086124();
    func_80086568();
    func_800866C8();
    func_80074F04();
    func_800750DC();
    func_80088FF4();
    func_80089128();
    func_80097D64();
    func_800320E8(D_8009BC38[0]);
    func_800320E8(D_8009BCB0[0]);
    func_800320E8(D_8009BC38[1]);
    func_800320E8(D_8009BCB0[1]);
    func_800320E8(D_8009C180);
    func_800976A0();
    D_8006F94E = 0x84;
    D_8006F954[0] = 2;
    D_8009BBC4 = 1;
    D_8006F950 = D_8009BD38.vy;
}

/* Restart an actor's timed sequence at its first step. */
s32 func_8008032C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->state = D_8009A698[0];
    actor->wait = D_8009A6AC[actor->u.step];
    return 1;
}
