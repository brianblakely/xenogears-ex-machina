#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_80077E68);

/* Link scene objects 1-13 to object 0 and put the scene camera on the player. */
s32 func_8007828C(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0, 1);
    func_800848B4(0, 2);
    func_800848B4(0, 3);
    func_800848B4(0, 4);
    func_800848B4(0, 5);
    func_800848B4(0, 6);
    func_800848B4(0, 7);
    func_800848B4(0, 8);
    func_800848B4(0, 9);
    func_800848B4(0, 0xA);
    func_800848B4(0, 0xB);
    func_800848B4(0, 0xC);
    func_800848B4(0, 0xD);
    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->position = D_8009C5AC;
    actor->unk54 = 0x40;
    actor->unk58 = 0x200;
    actor->unk5C = 0x60;
    actor->unk60 = 0x300;
    actor->u.script = NULL;
    actor->unk64 = 0x50;
    D_8009BE28.target = actor->position;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_800783E8);

/* Mode step that has nothing to do; always reports done. */
s32 func_80078948(void) {
    return 1;
}

/* Per-frame update and draw of the scene mode. */
s32 func_80078950(void) {
    if (D_8009D144 == 0) {
        func_80097440(D_8009BD40);
    } else {
        func_80097244(D_8009BD40);
    }
    func_80089748();
    func_80089C78();
    func_800848F4();
    func_800980D4(D_8009BBB4);
    if (D_8009D558 != 0) {
        func_800981C8(&D_8009BE28);
        func_80096130();
        func_80098CC0();
    }
    func_800983A0(&D_8009BE28);
    func_8009932C(D_8009BE3C->ot, D_8009BE3C->unk74, &D_8009BE28);
    D_8009C5BC += 0x40;
    func_80073B04();
    func_800737EC();
    func_80086798();
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80077E68", func_80078A60);

/* Leave the scene: release its resources and continue in scene 0x110. */
void func_80078D24(void) {
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
    D_8006F94E = 0x110;
    D_8006F954[0] = 0;
    D_8009BBC4 = 1;
    D_8006F950 = D_8009BD38.vy;
}

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

/* Carry effect 9 forward from scene object 0 for `wait` frames, then stop it. */
s32 func_8007A430(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    if (actor->unk4 == result) {
        actor->unk4 = 0;
        actor->position.vx = D_8009C620[0].position.vx << 12;
        actor->position.vy = D_8009C620[0].position.vy << 12;
        actor->position.vz = D_8009C620[0].position.vz << 12;
    }
    if (--actor->wait > 0) {
        actor->position.vz += 0x20000;
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        func_80089160(9, SCRIPT_VECTOR, NULL);
    } else {
        actor->wait = 0x60;
        actor->position = D_8009C5AC;
        func_800894C8(9);
        result = 3;
    }
    return result;
}

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

/* Leave the scene: release its resources and continue in scene 0x11A. */
void func_8007A8AC(void) {
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
    D_8006F94E = 0x11A;
    D_8006F954[0] = 0;
    D_8009BBC4 = 1;
    D_8006F950 = D_8009BD38.vy;
}

/* Restart an actor's timed sequence at its first step. */
s32 func_8007A9B4(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->state = D_8009A450;
    actor->wait = D_8009A46C[actor->u.step];
    return 1;
}
