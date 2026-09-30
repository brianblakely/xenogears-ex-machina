#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80080370);

/* Start a scripted camera at the player position. */
s32 func_80080578(s32 index) {
    WorldmapActor *actor;

    D_8009D144 = 0;
    actor = &D_8009BE24[index];
    actor->unk7C = 0x1000;
    D_8009BE0C = 0x78;
    D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    actor->unk4 = 1;
    actor->state = 0;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80080600);

/* Copy the player position into the actor. */
s32 func_80080900(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->position.vx = D_8009C5AC.vx;
    actor->position.vy = D_8009C5AC.vy;
    actor->position.vz = D_8009C5AC.vz;
    return 1;
}

/* On command, emit effects 0x28-0x2A at the actor. */
s32 func_80080944(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        func_80089160(0x28, SCRIPT_VECTOR, 0);
        func_80089160(0x29, SCRIPT_VECTOR, 0);
        func_80089160(0x2A, SCRIPT_VECTOR, 0);
    }
    return 1;
}

/* Set the colour of `count` textured quads. */
void func_800809EC(PolyFT4 *quads, s32 count, s32 r, s32 g, s32 b) {
    s32 i;

    for (i = 0; i < count; i++, quads++) {
        setRGB0(quads, r, g, b);
    }
}

/* Start a descent at the player and rebuild scene objects 0 and 1. */
s32 func_80080A28(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;

    object = D_8009C620;
    actor = &D_8009BE24[index];
    actor->position.vx = D_8009C5AC.vx;
    actor->position.vy = -0x80000;
    actor->position.vz = D_8009C5AC.vz;
    actor->u.step = 0;
    actor->unk54 = -0x800;
    actor->unk58 = 0x80;
    func_8007A06C(object, object->prims, object->def->count);
    object++;
    func_8007A06C(object, object->prims, object->def->count);
    return 3;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80080AC4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80080D00);

/* Leave the world map for scene 0x1FA (flag word 0). */
void func_8008106C(void) {
    func_80039FF8();
    func_8003852C(D_8006259C);
    func_800320E8(D_8006259C);
    func_80084818();
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
    D_8006F94E = 0x1FA;
    D_8006F954[0] = 0;
    D_8009BBC4 = 1;
    D_8006F950 = D_8009BD38.vy;
}

/* Start an actor's timed sequence: first state and its duration. */
s32 func_80081174(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->state = D_8009A6C0[0];
    actor->wait = D_8009A70C[actor->u.step];
    actor->u.step++;
    return 1;
}
