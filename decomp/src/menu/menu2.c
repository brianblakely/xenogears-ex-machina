#include "menu.h"
#include "sparkle.h"
#include "scene.h"
#include "spark.h"
#include "sound.h"
#include "brain.h"
#include "window.h"
#include "gte.h"

#ifdef NON_MATCHING
/* Place the menu camera for one of the view modes. Nearly matches (the jump
 * table lands at 0x8006faf4 since the unit split): GCC keeps the address of
 * D_8009872C.angle in a callee-saved register in cases 3/4, where the
 * original reloads it with lui/lw for each call. */
void func_8007099C(u32 mode) {
    Vector target;
    s32 top;

    switch (mode) {
    case 0:
        func_80083738(&D_8009872C, &D_80097010);
        break;
    case 1:
        target = D_8009872C.pos;
        D_800925F4 = 0;
        target.vy -= 0xA0;
        func_80070808(&target, 4);
        break;
    case 2:
        target = D_80097010.pos;
        D_800925F4 = 0;
        target.vy -= 0xA0;
        func_80070808(&target, 4);
        break;
    case 3:
        D_800925F4 = 0xA0;
        goto lifted;
    case 4:
        D_800925F4 = 0x80;
    lifted:
        target = D_80099078;
        target.vy += D_800925F4;
        func_80070808(&target, 0x10);
        target.vx = D_8009872C.pos.vx + ((func_8003F8B0(D_8009872C.angle + 0xA80) * 0xD0) >> 12);
        target.vy = D_8009872C.pos.vy - (D_800925F4 + 0x20);
        target.vz = D_8009872C.pos.vz + ((func_8003F8CC(D_8009872C.angle + 0xA80) * 0xD0) >> 12);
        func_800708C4(&target, 0x46);
        top = func_80082488(&D_8009871C, 0) - (D_800925F4 + 0x40);
        if (top < D_8009871C.vy) {
            D_8009871C.vy = top;
        }
        break;
    case 5:
        D_8009867C.vx = D_80097010.pos.vx;
        D_8009867C.vy = D_80097010.pos.vy - 0xC0;
        D_8009867C.vz = D_80097010.pos.vz;
        D_8009871C.vx = D_8009867C.vx + ((func_8003F8B0(D_80097010.angle + 0x900) * 0xE0) >> 12);
        D_8009871C.vy = D_80097010.pos.vy - 0xD0;
        D_8009871C.vz = D_8009867C.vz + ((func_8003F8CC(D_80097010.angle + 0x900) * 0xE0) >> 12);
        break;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu2", func_8007099C);
#endif

/* Re-centre the two actors and the look-at point on a fixed scene spot:
 * the midpoint of the actors moves to the layout's anchor, actors on the
 * floor and the look-at point at a fixed height. */
void func_80070C7C(s32 layout) {
    Vector first = D_8009872C.pos;
    Vector second = D_80097010.pos;
    Vector look = D_8009871C;
    Vector centre = first;

    centre.vx += second.vx;
    centre.vy += second.vy;
    centre.vz += second.vz;
    centre.vx /= 2;
    centre.vy /= 2;
    centre.vz /= 2;
    first.vx -= centre.vx;
    first.vy -= centre.vy;
    first.vz -= centre.vz;
    second.vx -= centre.vx;
    second.vy -= centre.vy;
    second.vz -= centre.vz;
    look.vx -= centre.vx;
    look.vy -= centre.vy;
    look.vz -= centre.vz;
    switch (layout) {
    case 0:
        centre.vx = 0x4000;
        centre.vy = 0;
        centre.vz = 0x4000;
        break;
    case 1:
        centre.vx = 0x6000;
        centre.vy = 0;
        centre.vz = 0x6000;
        break;
    case 2:
        centre.vx = 0x2000;
        centre.vy = 0;
        centre.vz = 0x2000;
        break;
    case 3:
        centre.vx = 0x4000;
        centre.vy = 0;
        centre.vz = 0x2400;
        break;
    }
    first.vx += centre.vx;
    first.vy += centre.vy;
    first.vz += centre.vz;
    second.vx += centre.vx;
    second.vy += centre.vy;
    second.vz += centre.vz;
    look.vx += centre.vx;
    look.vy += centre.vy;
    look.vz += centre.vz;
    first.vy = 0;
    second.vy = 0;
    look.vy = -0x300;
    D_8009872C.pos = first;
    D_80097010.pos = second;
    D_8009871C = look;
    func_8007E24C();
}

/* Start a scene script; reset both actors' states and clear their 0x8000
 * flag. */
void func_80070F80(u8 *script) {
    D_800925F8 = script;
    D_8009872C.state = 0;
    D_80097010.state = 0;
    D_800925FC = 0;
    D_8009872C.flags &= ~0x8000;
    D_80097010.flags &= ~0x8000;
}

/* Turn an actor toward one of two headings depending on which side of the
 * scene centre it stands, and reset its state. */
s32 func_80070FD8(Actor *actor) {
    Vector pos = actor->pos;

    pos.vx -= 0x3F80;
    pos.vz -= 0x3F80;
    if ((ratan2(pos.vx, pos.vz) & 0xFFF) > 0x200) {
        actor->target_angle = 0x800 - D_80092934;
    } else {
        actor->target_angle = 0xC00 - D_80092934;
    }
    actor->state = 0xFF;
    actor->unkCE = 0;
    return 0;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu2", func_8007107C);

/* Link the screen offset packet and this frame's texture page packet. */
void func_80071724(u32 *ot) {
    Window *frame = D_80092868;

    /* x0 and y0 of the offset sprite, stored as one word */
    *(u32 *)&frame->sprite.x0 = D_800925E0 | (D_800925E4 << 16);
    AddPrim(ot, &frame->sprite);
    AddPrim(ot, &D_800929E4[D_800928A0]);
}

/* Upload the menu's sprite sheet TIM (its first CLUT colour made
 * transparent), build both texture page packets and the sprite template. */
void func_80071794(u32 **resources) {
    TimImage image;
    Rect unused; /* the original frame reserves 8 more bytes */
    s16 *clut;

    OpenTIM(resources[0x60 / 4]);
    ReadTIM(&image);
    clut = (s16 *)image.caddr;
    clut[2] = -0x8000;
    clut[0] = 0;
    clut[3] = -1;
    LoadImage(image.crect, image.caddr);
    LoadImage(image.prect, image.paddr);
    SetDrawTPage(&D_800929E4[0], 0, 0, GetTPage(0, 1, image.prect->x, image.prect->y));
    D_800929E4[1] = D_800929E4[0];
    D_8009A14C.u0 = (image.prect->x & 0x3F) * 4;
    D_8009A14C.v0 = image.prect->y;
    D_8009A14C.clut = GetClut(image.crect->x, image.crect->y);
    D_8009A244 = D_8009A14C;
}

/* Open the menu message window. */
void func_800718C0(void) {
    D_800925D8 = -1;
    D_800925D4 = 0;
    func_80032F54(&D_8009868C, 0x140, 0x30, 0x1C, 0x9A, 0x40, 4);
}

/* Enter a menu scene: the first scene also starts sound 0x37 and uses a
 * taller window; restarts both actors at full HP and centres the screen
 * offset. */
void func_8007191C(s32 scene) {
    D_80092608 = scene == 0;
    if (scene == 0) {
        func_8008EB4C(0x37);
        D_8009868C.unkC = 2;
        D_8009868C.unk6 = 0xB4;
    } else {
        D_8009868C.unkC = 4;
        D_8009868C.unk6 = 0x9A;
    }
    func_80070F80(D_8009105C[scene]);
    D_800925E8 = 0xA0;
    D_800925E0 = 0xA0;
    D_800925EC = 0x6D;
    D_800925E4 = 0x6D;
    D_80092600 = 0;
    D_8009872C.hp = D_8009872C.max_hp;
    D_80097010.hp = D_80097010.max_hp;
}

/* Start the menu's opening: text window with message 0x42, the intro
 * script, then scene 9. */
void func_800719F0(void) {
    D_80099D9D = 0;
    D_80099D98.driven = 0;
    func_80083C0C(7);
    D_800928C8 = 5;
    D_80092884 = 0;
    func_80032F54(&D_80092954, 0x140, 0x70, 0xA2, 0x2A, 0x1C, 8);
    func_80034714(&D_80092954, func_80033728(D_80092880, 0x42));
    D_800929BC = 0x1E;
    D_800925DC = 0;
    func_80070F80(D_80090F38);
    D_80092604 = 0;
    D_80092904 = 0;
    D_80092900 = 0;
    func_8007191C(9);
}

/* Per-frame menu scene update: scene choice input, the scene script, the
 * screen offset easing, the message window and the camera. */
void func_80071AD0(void) {
    MenuWindow *message;

    if (D_800925F0 != 0 && (D_800928E8 & 4)) {
        func_80071724(D_80092938);
    }
    func_80036420();
    message = &D_8009868C;
    if (D_80092608 != 0) {
        if (D_800594A4 & 0x1000) {
            func_8008EB4C(0x1E);
            D_80092604--;
        }
        if (D_800594A4 & 0x4000) {
            func_8008EB4C(0x1E);
            D_80092604++;
        }
        if (D_80092604 >= 8) {
            D_80092604 = 0;
        }
        if (D_80092604 < 0) {
            D_80092604 = 7;
        }
        func_80034800(&D_80092954, (D_800928E8 * 7) & 0x3F, 0xC0, 0x10);
        func_80034874(&D_80092954, D_80092604);
        if (D_8005948C & 0x20) {
            func_8008EB4C(0x21);
            func_8007191C(D_80092604 + 1);
            func_800346A4(message);
        }
    }
    if (*D_800925F8 == 0) {
        func_8007191C(0);
    }
    func_8007107C();
    D_800925E0 += func_800707D8(D_800925E8, D_800925E0, 4);
    D_800925E4 += func_800707D8(D_800925EC, D_800925E4, 4);
    if (D_80092608 != 0) {
        func_80034888(&D_80092954, D_80092938, D_800928A0);
    }
    if (D_800925D4 != D_800925D8) {
        message->unk68 = 3;
        func_800346A4(message);
        func_80034714(message, func_80033728(D_80092880, D_800925D4));
        D_800925D8 = D_800925D4;
    }
    func_80079DF0(&D_8009872C, &D_80097010);
    func_8007099C(D_80092904);
}

#ifdef NON_MATCHING
/* Settle an actor on the floor: while a probe 0xC0 away in one of eight
 * directions finds the floor more than 0x40 higher, step away from it (at
 * most 20 times); then record the floor height and its attribute bits.
 * Does not match: the original strength-reduces the step table walk into
 * two pointers (x from a register base, z from the symbol + 4). */
void func_80071DA4(Actor *actor) {
    Vector *pos = &actor->pos;
    s32 tries = 0;
    s32 best;
    s32 highest;
    s32 dir;
    s32 floor;
    Vector probe;

    do {
        pos->vy = highest = func_80082488(pos, 1);
        for (dir = 0; dir < 8; dir++) {
            probe = *pos;
            probe.vx += D_80091084[dir].x * 0xC0;
            probe.vz += D_80091084[dir].z * 0xC0;
            floor = func_80082488(&probe, 1);
            if (floor < highest - 0x40) {
                best = dir;
                highest = floor;
            }
        }
        if (highest >= pos->vy - 0x40) {
            pos->vy = func_80082488(pos, 1);
            break;
        }
        pos->vx -= D_80091084[best].x * 0xC0;
        pos->vz -= D_80091084[best].z * 0xC0;
        tries++;
    } while (tries < 20);
    actor->floor_y = func_80082488(&actor->pos, 1);
    actor->flags = (actor->flags & 0x9FFFFFFF) | (((func_800828C4(&actor->pos) >> 24) & 3) << 29);
}

#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu2", func_80071DA4);
#endif

/* Scene script callback: 0 plays the stored sound, 1/2 act on one actor
 * (1 also picks the message for whichever actor has more HP left), 3 sets
 * the look-at height, capped at -0x600. */
s32 func_80071F8C(s32 command) {
    switch (command) {
    case 0:
        func_80039C4C(D_80092948);
        func_80039FF8();
        break;
    case 1:
        func_80071DA4(&D_8009872C);
        if ((D_8009872C.hp << 8) / D_8009872C.max_hp > (D_80097010.hp << 8) / D_80097010.max_hp) {
            D_800925D4 = 0x43;
        } else {
            D_800925D4 = 0x44;
        }
        break;
    case 2:
        func_80071DA4(&D_80097010);
        break;
    case 3:
        D_8009871C.vy = -D_8009284C;
        if (D_8009871C.vy < -0x600) {
            D_8009871C.vy = -0x600;
        }
        break;
    }
}

/* Allow the next menu scene setup. */
void func_800720C4(void) {
    D_8009293C = 0;
}

/* One-time scene setup: start the scene script and clear the actors'
 * counters and the message state. */
void func_800720D4(void) {
    if (D_8009293C == 0) {
        func_80070F80(D_800910C4);
        D_8009293C = 1;
        D_800928D4 = 0;
        D_80099D9D = 0;
        D_80099D98.driven = 0;
        D_80092900 = 0;
        D_800925D4 = 0;
        D_800925D8 = 0;
        D_80092A00 = 1;
        D_80092A10 = 1;
        D_80092A20 = 1;
        D_80097010.unkE8 = 0;
        D_8009872C.unkE8 = 0;
    }
}
/* Per-frame scene effects of the bout-end sequence (step D_80092900):
 * three sparking embers on the first actor's body, or the first actor
 * knocked down while a flash fades out and back in (which ends the bout);
 * then the caption when its text changed and the camera view. */
void func_80072170(void) {
    Actor *actor;
    MenuWindow *window;
    Vector pos;
    Node *part;
    s32 i;
    s32 count;

    if (D_8009293C != 0) {
        actor = &D_8009872C;
        window = &D_8009868C;
        switch (D_80092900) {
        case 11:
            func_8008EB4C(0x2A);
            func_8008EB4C(0x2B);
            func_8008EB4C(0x2C);
            D_80092900 = 1;
        case 1:
            for (i = 0; i < 3; i++) {
                if (--D_800929F4[i].pad == -1) {
                    part = ((ModelSet *)D_8009872C.node->data)->nodes[rand() % ((ModelSet *)D_8009872C.node->data)->nodeCount];
                    D_800929F4[i].vx = part->unk4C.t[0] + D_8009872C.pos.vx;
                    D_800929F4[i].vy = part->unk4C.t[1] + D_8009872C.pos.vy;
                    D_800929F4[i].vz = part->unk4C.t[2] + D_8009872C.pos.vz;
                    D_800929F4[i].pad = rand() % 16 + 8;
                }
                if (D_800929F4[i].pad & 1) {
                    func_8007D190(&D_800929F4[i], 9);
                }
            }
            break;
        case 2:
            actor->hp = 1;
            D_8009260C = 0xFF;
            D_80092900++;
        case 3:
            func_8008E2B8(D_80092938, D_8009260C, 0);
            if (D_8009260C < 0) {
                D_8009260C = 0;
            }
            D_8009260C -= 8;
        knocked:
            actor->unk4F = 0x10;
            actor->anim = 0xA;
            actor->unk52 = 0;
            actor->charge = 0x1000;
            actor->flags |= 0x400;
            if (rand() & 1) {
                count = ((ModelSet *)D_8009872C.node->data)->nodeCount;
                part = ((ModelSet *)D_8009872C.node->data)->nodes[rand() % count];
                pos.vx = part->unk4C.t[0] + D_8009872C.pos.vx;
                pos.vy = part->unk4C.t[1] + D_8009872C.pos.vy;
                pos.vz = part->unk4C.t[2] + D_8009872C.pos.vz;
                func_8007D190(&pos, 0xB);
                func_8007D190(&pos, 8);
            }
            break;
        case 4:
            D_8009260C += 3;
            if (D_8009260C >= 0x100) {
                D_8009260C = 0xFF;
                D_80050622 = 0x7F;
                func_80083BB4(0);
                func_800851D4();
            }
            func_8008E2B8(D_80092938, D_8009260C, 1);
            goto knocked;
        }
        func_8007107C();
        if (D_800925D4 != D_800925D8) {
            window->unk68 = 1;
            func_8003463C(window);
            func_80034714(window, func_80033728(D_80092880, D_800925D4));
            window->unkC = 2;
            window->unk6 = 0xB4;
            D_800925D8 = D_800925D4;
        }
        func_8007099C(D_80092904);
        D_8009872C.state = 0;
    }
}

void func_800725A8(void) {
}

/* Set up the scene around an actor: graphics state, lights, its model
 * copy, the scene origin and the values from its move header. */
void func_800725B0(Actor *scene) {
    SceneHeader *header;

    func_80030988(5, 4, 0x40, 0x40);
    D_800910F0 = func_8008A3E0(func_8008A2B8(0x10));
    D_80092610 = func_8008C2C0(scene->node);
    func_8008976C(0x280, 0xDA);
    SetGeomScreen(0x400);
    D_800928D0 = 0;
    D_80092614 = scene;
    func_80078F00(scene);
    D_80096FA8.vz = 0;
    D_80096FA8.vy = 0;
    D_80096FA8.vx = 0;
    header = scene->header;
    D_80092618 = 1;
    D_8009261C = header->unk14;
    D_80092620 = header->unk16;
    D_80092624 = header->unk18;
    D_80092628 = header->unk1A;
    D_8009262C = header->unk1C;
    D_80092630.vy = header->unk1E;
}

/* Tear down the scene set up by func_800725B0. */
void func_800726B4(void) {
    func_8008BC04();
    func_8007F834();
    func_80030988(1, 1, 0x40, 0x40);
    func_8008A5BC(D_800910F0);
    func_80089D5C(D_80092610);
    func_8008976C(0x140, 0xDA);
    SetGeomScreen(0xC0);
    func_80083C0C(3);
    func_8007E954(0x100);
    func_80080D10();
}

/* Copy a model's matrix to out, rotated by the base matrix, with its
 * translation set to the model position relative to the scene origin. */
void func_8007273C(Node *model, Matrix *matrix, Matrix *out) {
    Matrix local;

    *out = *matrix;
    local = D_80091C0C;
    local.t[0] = model->position.vx - D_80096FA8.vx;
    local.t[1] = -D_80096FA8.vy;
    local.t[2] = model->position.vz - D_80096FA8.vz;
    CompMatrix(matrix, &local, &local);
    out->t[0] = local.t[0];
    out->t[1] = local.t[1];
    out->t[2] = local.t[2];
}

/* One frame of the winner screen: turn the winner's model with the
 * shoulder buttons, toggle its record text with the first button, leave
 * with 0x20; draw the record (name, level, matches, time) and the model
 * turning in front of the scene's lights. */
void func_80072858(LightRig *rig) {
    Matrix unused1; /* the original frame has 32 unused bytes on */
    Matrix m;
    Matrix unused2; /* either side of the matrix */
    char text[64];
    Actor *winner = D_80092614;
    u8 y;
    ModelSet *set;

    func_80036420();
    if (winner->model_id != 7) {
        if (D_80059570 & 0x2000) {
            D_80092620--;
        }
        if (D_80059570 & 0x8000) {
            D_80092620++;
        }
    }
    if (D_80092620 < -0x80) {
        D_80092620 = -0x80;
    }
    if (D_80092620 > 0x80) {
        D_80092620 = 0x80;
    }
    if (D_800928FC == 1) {
        if (D_80059490 & 0x20) {
            func_800726B4();
            return;
        }
        if (D_80059490 & 1) {
            D_80092618++;
        }
    } else {
        if (D_8005948C & 0x20) {
            func_800726B4();
            return;
        }
        if (D_8005948C & 1) {
            D_80092618++;
        }
    }
    if (D_80092618 & 1) {
        y = 0x86;
        if (D_800928C8 == 2 || D_800928C8 == 3) {
            y = 0x9A;
        }
        func_8007E954(0x1C0);
        func_8007E894(0x18, y);
        func_8007EBE0("WINNER");
        if (D_800928C8 != 2 && D_800928C8 != 3) {
            func_8007EBE0("LEVEL");
        }
        func_8007EBE0("MATCHES");
        func_8007EBE0("TIME");
        func_8007E894(0xA8, y);
        func_8007EBE0(D_8009196C[winner->model_id].name);
        if (D_800928C8 != 2 && D_800928C8 != 3) {
            sprintf(text, "%s", func_8007F97C());
            func_8007EBE0(text);
        }
        sprintf(text, "%d/%d VS %s", winner->unkF2, D_80092950, D_8009196C[winner->opponent->model_id].name);
        func_8007EBE0(text);
        func_80083CE8();
    }
    func_80080D20(D_80092938);
    D_80092630.vx = D_8009261C;
    D_80092630.vy += D_80092620;
    D_80092630.vz = 0;
    func_8003F738(&D_80092630, &m);
    func_80049BDC(&D_80096FE0, &m);
    m.t[0] = 0;
    m.t[1] = D_80092624;
    m.t[2] = D_80092628;
    func_8008AC0C(rig->layer);
    func_8007B210(winner, 0);
    winner->node->position.vx = winner->node->position.vy = winner->node->position.vz = 0;
    winner->node->unk44.vy = 0;
    set = winner->node->data;
    set->scale[0] = set->scale[1] = set->scale[2] = D_8009262C;
    ((Node *)winner->object)->view = m;
    func_8008A7E0(winner->node);
    func_8008AE1C(rig->layer);
    func_8008AC0C(D_800910F0->layer);
    func_8007273C(winner->node, &m, &D_80092610->view);
    func_8008A7E0(D_80092610);
    gte_SetRotMatrix(&D_80092610->view);
    gte_SetTransMatrix(&D_80092610->view);
    func_8008C2E8(D_80092610);
    func_8008AE1C(D_800910F0->layer);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu2", func_80072D18);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu2", func_80073064);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu2", func_800730AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu2", func_800730F4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu2", func_8007313C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu2", func_800731F8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu2", func_800732AC);
