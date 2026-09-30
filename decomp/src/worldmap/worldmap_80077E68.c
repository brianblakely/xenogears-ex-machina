#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_80077E68);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_8007828C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_800783E8);

/* Mode step that has nothing to do; always reports done. */
s32 func_80078948(void) {
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_80078950);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_80078A60);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_80078D24);

/* Start a scripted camera looking down from yaw 0x680. */
s32 func_80078E2C(s32 index) {
    WorldmapActor *actor;

    D_8009BE0C = 0x78;
    D_8009D3F0 = 0x200000;
    D_8009D144 = 0;
    actor = &D_8009BE24[index];
    actor->state = 0x10;
    actor->unk58 = 0;
    actor->unk54 = 0;
    actor->u.script = NULL;
    D_8009BD38.vx = -0xC0;
    D_8009BD38.vy = 0x680;
    D_8009BD38.vz = 0;
    actor->wait = 0x40;
    actor->unk74 = 0;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_80078EA4);

/* Place the scene camera target and set the scene yaw. */
s32 func_800794D8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->position.vx = 0x2000000;
    actor->position.vz = 0x1500000;
    actor->state = 0;
    D_8009C620[16].angle.vx = 0;
    D_8009C620[16].angle.vy = 0x780;
    D_8009C620[16].angle.vz = 0;
    func_8004A92C(&D_8009C620[16].angle, &D_8009C620[16].matrix);
    return 1;
}

/* Keep the actor on the ground and scene object 16 at it. */
s32 func_80079538(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    if (actor->unk4 != 0) {
        actor->unk4 = 0;
        D_8009C620[16].visible = 1;
    }
    func_80093354(&actor->position);
    actor->position.vy = func_80093A5C(actor->position.vx, actor->position.vz) + 0x18000;
    D_8009C620[16].position.vx = actor->position.vx >> 12;
    D_8009C620[16].position.vy = actor->position.vy >> 12;
    D_8009C620[16].position.vz = actor->position.vz >> 12;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_800795E4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_80079778);

/* Build `count` semi-transparent textured quads for a scene sprite. */
void func_8007A06C(SceneObject *object, PolyFT4 *quads, s32 count) {
    PolyFT4 *quad;
    s32 i;

    quad = quads;
    for (i = 0; i < count; i++) {
        setPolyFT4(quad);
        quad->tpage = GetTPage(1, 3, 0x340, 0x100);
        quad->clut = GetClut(0x100, 0x1FF);
        setSemiTrans(quad, 1);
        setRGB0(quad, 0x80, 0x80, 0x80);
        quad++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(PolyFT4));
}

/* Rebuild both scene sprites' quads. */
s32 func_8007A144(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->unk70 = 0;
    actor->unk6C = 0;
    func_8007A06C(&D_8009C620[14], D_8009C620[14].prims, D_8009C620[14].def->count);
    func_8007A06C(&D_8009C620[15], D_8009C620[15].prims, D_8009C620[15].def->count);
    return 3;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_8007A1B4);

/* Wait 0x60 frames. */
s32 func_8007A410(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->wait = 0x60;
    return 3;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_8007A430);

/* Scene step with nothing to do. */
s32 func_8007A568(void) {
    return 3;
}

/* Emit effect 0xA at the scene position. */
s32 func_8007A570(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->unk4 = 0;
    SCRIPT_VECTOR->vx = D_8009C620[0].position.vx;
    SCRIPT_VECTOR->vy = D_8009C620[0].position.vy;
    SCRIPT_VECTOR->vz = D_8009C620[0].position.vz;
    func_80089160(0xA, SCRIPT_VECTOR, 0);
    return 3;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_8007A5DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_8007A8AC);

/* Restart an actor's timed sequence at its first step. */
s32 func_8007A9B4(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->state = D_8009A450;
    actor->wait = D_8009A46C[actor->u.step];
    return 1;
}
