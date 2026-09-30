#include "menu.h"
#include "sparkle.h"
#include "scene.h"
#include "spark.h"
#include "sound.h"
#include "brain.h"
#include "window.h"
#include "gte.h"

/* Draw the elapsed time (frames at 30 per second) as minutes, seconds and
 * hundredths. Does not match:
 * the minutes are computed into another register and copied. */
#ifdef NON_MATCHING
void func_80083CE8(void) {
    char text[32];
    s32 minutes;
    s32 seconds;

    minutes = D_80092944 / 1800;
    seconds = D_80092944 - minutes * 1800;
    sprintf(text, "%02d'%02d''%02d", minutes, seconds / 30, D_80092944 % 30 * 99 / 30);
    func_8007EBE0((s32)text);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu5", func_80083CE8);
#endif

/* Update an actor's glow light (fading it) at its position relative to its
 * opponent, and the spot light at its position relative to the camera. */
void func_80083DCC(LightSet *set, Actor *actor, s32 index) {
    Vector unused[2]; /* the original frame has 0x20 unused bytes */
    LightRef *ref = set->lights[index];
    u8 glow = actor->glow;
    s32 level = glow;

    if (level != 0) {
        actor->glow = glow - 0x18;
        if (level < actor->glow) {
            actor->glow = 0;
        }
    }
    if (actor->unkD4 & 0x20) {
        ref->data->r = actor->opponent->colour.r * level / 16;
        ref->data->g = actor->opponent->colour.g * level / 16;
        ref->data->b = actor->opponent->colour.b * level / 16;
    } else {
        ref->data->r = level << 4;
        ref->data->g = level << 3;
        ref->data->b = 0;
    }
    if (D_8009288C->dim) {
        ref->data->r /= 2;
        ref->data->g /= 2;
        ref->data->b /= 2;
    }
    ref->data->x = actor->pos.vx;
    ref->data->y = actor->pos.vy;
    ref->data->z = actor->pos.vz;
    ref->data->x -= actor->opponent->pos.vx;
    ref->data->y -= actor->opponent->pos.vy;
    ref->data->z -= actor->opponent->pos.vz;
    func_80030A30(index, ref->data);
    ref = set->lights[2];
    ref->data->r = ref->data->g = ref->data->b = 0;
    ref->data->x = actor->pos.vx;
    ref->data->y = actor->pos.vy;
    ref->data->z = actor->pos.vz;
    ref->data->x -= D_8009871C.vx;
    ref->data->y -= D_8009871C.vy;
    ref->data->z -= D_8009871C.vz;
    func_80030A30(2, ref->data);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu5", func_800840CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu5", func_800846A0);

/* Draw a 3D panel: update it, link this buffer's packets, finish. */
s32 func_800849E0(PanelOwner *owner) {
    Panel *panel = owner->panel;

    func_8008AC0C(panel);
    func_80080D20(&panel->buffers[D_800928A0]->unk4);
    func_8008AE1C(panel);
    func_80086E24();
    return 0;
}

/* Update a 3D panel without drawing it. */
void func_80084A40(PanelOwner *owner) {
    func_8008AC0C(owner->panel);
}

/* Draw a 3D panel with its shading packet at brightness 0xC0. */
s32 func_80084A64(PanelOwner *owner) {
    Panel *panel = owner->panel;

    func_8008E3CC(&panel->buffers[D_800928A0]->unk8, 0xC0, 0);
    func_80080D20(&panel->buffers[D_800928A0]->unk4);
    func_8008AE1C(panel);
    func_80086E24();
    return 0;
}

/* Draw the fading overlay while a fade is running. */
void func_80084AE0(void) {
    if (D_80092780 != 0) {
        func_8008E120();
        func_8007F258(D_80092938, 0);
        func_80080D20(D_80092938);
        func_8008E3CC(D_80092938, 0xC0, 1);
        func_8008BC04();
    }
}

/* Step the overlay fade down by 4; when it ends, reset it. */
void func_80084B48(void) {
    if (D_80092780 != 0) {
        D_80092780 -= 4;
        if (D_80092780 <= 0) {
            D_80092780 = 0;
            D_80092784 = 0;
            func_8003A838(D_80092948, 0x100, 0);
            D_8009292C = 0x100;
            func_8008E064();
        } else {
            func_8008E3CC(D_80092938, D_80092780, 1);
        }
    } else {
        D_80092784 = 0;
    }
}

/* Attach an extra object (model D_80091FB0) to the actor's model, turned
 * by (0, 0xC00, 0x400). */
void func_80084BEC(Actor *actor) {
    void *parent = ((ModelNode *)actor->node)->next->next->unk30;
    SceneObject *object = func_80089C54();
    void *part = func_80089FC4();

    func_80089E2C(object, part);
    func_8008A184(part, D_80091FB0);
    func_80089C88(parent, object);
    object->rotation.vy = 0xC00;
    object->rotation.vx = 0;
    object->rotation.vz = 0x400;
}

/* Set up an actor from its loaded model file on one side of the scene:
 * opponent link, model object, kind flags from the model id, part counts,
 * and the palette/emblem images in VRAM (mirrored for side 0). */
void func_80084C88(Actor *actor, ModelData *data, s32 side) {
    Rect rect;
    void *block; /* the model object, later the mirrored emblem */
    ModelHeader *header;
    u8 *source;
    s32 i;
    s32 j;
    u8 *out;
    s32 row;

    actor->flags = (actor->flags & ~0x08000000) | ((side & 1) << 27);
    if (side) {
        func_8008A140(0x380, 0, 0, 0x1FE);
        actor->opponent = &D_8009872C;
    } else {
        func_8008A140(0x3C0, 0, 0, 0x1FF);
        actor->opponent = &D_80097010;
    }
    func_8008AF6C(data);
    block = func_8008B38C(data);
    actor->object = func_80089C54();
    func_80089C88(actor->object, block);
    actor->node = (Node *)block;
    actor->moves = &D_80092874[actor->model_id];
    actor->kind = 0;
    switch (actor->model_id) {
    case 36:
    case 37:
        actor->kind |= 1;
    case 38:
        actor->kind |= 2;
        break;
    case 3:
    case 14:
    case 27:
    case 34:
    case 35:
    case 39:
    case 41:
    case 42:
        actor->kind |= 4;
        break;
    case 13:
        func_8008A168();
        func_80084BEC(actor);
        break;
    }
    func_8008E6F8(actor);
    header = data->header;
    actor->header = header;
    actor->unk7C = data->unk14;
    actor->move_slots = (MoveSlot *)data->parts;
    actor->unk900 = (u8 *)header + 0x34;
    actor->visible = (u8 *)(header->unk30 + (s32)header);
    actor->visible_count = header->unkE;
    actor->colour.r = header->unk10[0];
    actor->colour.g = header->unk10[1];
    actor->colour.b = header->unk10[2];
    actor->move_count = 0;
    actor->parts_b = 0;
    for (i = 0; i < 14; i++) {
        if (actor->moves->learned[i]) {
            if (actor->move_slots[i].usable) {
                actor->move_count++;
            } else {
                actor->parts_b++;
            }
        }
    }
    row = 0x100;
    rect.x = 0;
    rect.y = side + 0x1F6;
    rect.w = row;
    rect.h = 1;
    LoadImage(&rect, data->image);
    rect.x = side * 16 + 0x380;
    rect.y = row;
    rect.w = 0xB;
    rect.h = 0x16;
    if (side) {
        LoadImage(&rect, data->image + 0x200);
    } else {
        source = data->image + 0x200;
        block = func_80031BDC(0x1E4, 0);
        out = block;
        for (i = 0; i < 0x16; i++) {
            for (j = 0; j < 0x16; j++) {
                *out++ = source[0x15 - j];
            }
            source += 0x16;
        }
        LoadImage(&rect, block);
        func_80032C18(block, 1);
    }
    rect.x = 0x3A0;
    rect.y = side * 8 + 0x100;
    rect.w = 0x10;
    rect.h = 8;
    LoadImage(&rect, data->image + 0x3E4);
}

/* While the overlay fade runs, redraw it when button bit 0 is down. */
void func_80084FD0(void) {
    if (D_80092784 != 0 && (D_80059488 & 1)) {
        func_8008E120();
    }
}

/* Leave the menu screen for scene mode 5. */
void func_80085014(void) {
    D_80092920 &= ~1;
    func_80083BB4(0);
    func_80083C0C(5);
    D_80099D9E = 1;
    D_80099D9D = 1;
    D_800928C8 = 6;
}

/* Return from scene mode 5 to the menu screen. */
void func_80085070(void) {
    D_80099D9E = 0;
    D_80099D9D = 0;
    D_80092920 |= 1;
}

/* Give actor slot `which` (1 = D_80097010, 0 = D_8009872C) a new model
 * id and load its model, replacing the previous one. */
void func_8008509C(s32 which, s32 id) {
    if (which != 0) {
        D_80097010.model_id = id;
    } else {
        D_8009872C.model_id = id;
    }
    if (D_800927B4[which] != NULL) {
        func_800320E8(D_800927B4[which]);
        D_800927B4[which] = NULL;
    }
    func_80028470(0x30, 1);
    D_800927B4[which] = func_800891C0(id + 2);
    func_80028470(0x30, 0);
}

/* Release actor slot `which`'s model. */
void func_80085134(s32 which) {
    func_80028A60(0);
    if (D_800927B4[which] != NULL) {
        func_800320E8(D_800927B4[which]);
        D_800927B4[which] = NULL;
    }
}

/* Load a resource by its file number. */
void func_8008518C(Resource *resource, s32 arg) {
    resource->data = func_80031BDC(func_800288EC(resource->file), arg);
}

/* Leave the menu mode: stop its sound and streams, wait for drawing and
 * dispatch the next mode. */
void func_800851D4(void) {
    func_8003852C(D_800927C4);
    if (D_800917F0 != 0) {
        func_80039C4C(D_80092948);
        func_800399D4(D_80092948);
    }
    func_80088A40();
    func_8001996C(1);
    DrawSync(0);
    VSync(2);
    D_8005061C = 1;
    func_80019ACC(0);
}

/* Whether the scene is in a state that ends the menu mode. */
s32 func_80085264(void) {
    s32 done = 0;

    if (D_80092794 == 5 || D_80092794 == 7 ||
        (D_80092794 == 1 && D_800928C8 == 4 && D_8005061C == 0)) {
        done = 1;
    }
    return done;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu5", func_800852C4);

/* Screen position of the left-hand gauge for a layout point. */
void func_80085E34(DVector *point, DVector *out) {
    out->vx = point->vx + 0x18;
    out->vy = point->vy + 6;
    out->vx += 0x4F;
}

/* Screen position of the right-hand (mirrored) gauge for a layout point. */
void func_80085E60(DVector *point, DVector *out) {
    out->vx = 0x8B - point->vx;
    out->vy = 0x20 - point->vy;
    out->vx += 0x4F;
}

/* Gauge x for a side (nonzero = mirrored). */
void func_80085E90(s32 mirrored, s16 *out, s32 x) {
    if (mirrored) {
        *out = 0xDA - x;
    } else {
        *out = x + 0x67;
    }
}

/* Gauge y for a side (nonzero = mirrored). */
void func_80085EAC(s32 mirrored, s16 *out, s32 y) {
    if (mirrored) {
        *out = 0x20 - y;
    } else {
        *out = y + 6;
    }
}

/* Build one buffer's overlay packets: texture page modes, the frame
 * outlines and gauge quads of both sides (left from the corner layout,
 * right mirrored), the arrow triangles and the marks. The mirror loop runs
 * over six arrows and so also writes three past the array into the marks,
 * which are set afterwards. Does not match:
 * the arrow mirroring and final mark stores are scheduled differently. */
#ifdef NON_MATCHING
void func_80085EC8(OverlayBuffer *buf) {
    s32 i;

    SetDrawTPage(&buf->tpage[0], 0, 1, GetTPage(0, 2, 0, 0));
    SetDrawTPage(&buf->tpage[1], 0, 0, GetTPage(0, 1, 0, 0));
    ((PacketTag *)&buf->frame[0])->len = 6;
    *(u32 *)&buf->frame[0].r0 = 0x4C000000;
    buf->frame[0].pad = 0x55555555;
    ((PacketTag *)&buf->frame[1])->len = 6;
    *(u32 *)&buf->frame[1].r0 = 0x4C000000;
    buf->frame[1].pad = 0x55555555;
    ((PacketTag *)&buf->frame[2])->len = 6;
    *(u32 *)&buf->frame[2].r0 = 0x4C000000;
    buf->frame[2].pad = 0x55555555;
    ((PacketTag *)&buf->frame[3])->len = 6;
    *(u32 *)&buf->frame[3].r0 = 0x4C000000;
    buf->frame[3].pad = 0x55555555;
    func_80085E34(&D_800917F4[0], (DVector *)&buf->frame[0].x0);
    func_80085E34(&D_800917F4[1], (DVector *)&buf->frame[0].x1);
    func_80085E34(&D_800917F4[2], (DVector *)&buf->frame[0].x2);
    func_80085E34(&D_800917F4[3], (DVector *)&buf->frame[0].x3);
    func_80085E34(&D_800917F4[3], (DVector *)&buf->frame[1].x0);
    func_80085E34(&D_800917F4[4], (DVector *)&buf->frame[1].x1);
    func_80085E34(&D_800917F4[5], (DVector *)&buf->frame[1].x2);
    func_80085E34(&D_800917F4[0], (DVector *)&buf->frame[1].x3);
    func_80085E60(&D_800917F4[0], (DVector *)&buf->frame[2].x0);
    func_80085E60(&D_800917F4[1], (DVector *)&buf->frame[2].x1);
    func_80085E60(&D_800917F4[2], (DVector *)&buf->frame[2].x2);
    func_80085E60(&D_800917F4[3], (DVector *)&buf->frame[2].x3);
    func_80085E60(&D_800917F4[3], (DVector *)&buf->frame[3].x0);
    func_80085E60(&D_800917F4[4], (DVector *)&buf->frame[3].x1);
    func_80085E60(&D_800917F4[5], (DVector *)&buf->frame[3].x2);
    func_80085E60(&D_800917F4[0], (DVector *)&buf->frame[3].x3);
    MargePrim(&buf->frame[0], &buf->frame[1]);
    MargePrim(&buf->frame[2], &buf->frame[3]);
    buf->frame[0].x0 = 0x1D;
    buf->frame[2].x0 = 0x121;
    ((PacketTag *)&buf->bars[0])->len = 5;
    *(u32 *)&buf->bars[0].r0 = 0x280000FF;
    ((PacketTag *)&buf->bars[1])->len = 5;
    *(u32 *)&buf->bars[1].r0 = 0x280000FF;
    ((PacketTag *)&buf->bars[2])->len = 5;
    *(u32 *)&buf->bars[2].r0 = 0x280000FF;
    ((PacketTag *)&buf->bars[3])->len = 5;
    *(u32 *)&buf->bars[3].r0 = 0x280000FF;
    ((PacketTag *)&buf->bars[4])->len = 5;
    *(u32 *)&buf->bars[4].r0 = 0x280000FF;
    ((PacketTag *)&buf->bars[5])->len = 5;
    *(u32 *)&buf->bars[5].r0 = 0x280000FF;
    func_80085E34(&D_800917F4[0], (DVector *)&buf->bars[0].x0);
    func_80085E34(&D_800917F4[7], (DVector *)&buf->bars[0].x1);
    func_80085E34(&D_800917F4[5], (DVector *)&buf->bars[0].x2);
    func_80085E34(&D_800917F4[4], (DVector *)&buf->bars[0].x3);
    func_80085E34(&D_800917F4[7], (DVector *)&buf->bars[1].x0);
    func_80085E34(&D_800917F4[6], (DVector *)&buf->bars[1].x1);
    func_80085E34(&D_800917F4[4], (DVector *)&buf->bars[1].x2);
    func_80085E34(&D_800917F4[3], (DVector *)&buf->bars[1].x3);
    func_80085E34(&D_800917F4[6], (DVector *)&buf->bars[2].x0);
    func_80085E34(&D_800917F4[1], (DVector *)&buf->bars[2].x1);
    func_80085E34(&D_800917F4[3], (DVector *)&buf->bars[2].x2);
    func_80085E34(&D_800917F4[2], (DVector *)&buf->bars[2].x3);
    func_80085E60(&D_800917F4[0], (DVector *)&buf->bars[3].x0);
    func_80085E60(&D_800917F4[7], (DVector *)&buf->bars[3].x1);
    func_80085E60(&D_800917F4[5], (DVector *)&buf->bars[3].x2);
    func_80085E60(&D_800917F4[4], (DVector *)&buf->bars[3].x3);
    func_80085E60(&D_800917F4[7], (DVector *)&buf->bars[4].x0);
    func_80085E60(&D_800917F4[6], (DVector *)&buf->bars[4].x1);
    func_80085E60(&D_800917F4[4], (DVector *)&buf->bars[4].x2);
    func_80085E60(&D_800917F4[3], (DVector *)&buf->bars[4].x3);
    func_80085E60(&D_800917F4[6], (DVector *)&buf->bars[5].x0);
    func_80085E60(&D_800917F4[1], (DVector *)&buf->bars[5].x1);
    func_80085E60(&D_800917F4[3], (DVector *)&buf->bars[5].x2);
    func_80085E60(&D_800917F4[2], (DVector *)&buf->bars[5].x3);
    SetDrawTPage(&buf->bar_tpage, 0, 1, GetTPage(0, 1, 0, 0));
    func_800732AC(buf->bars_dim, buf->bars, sizeof(buf->bars));
    func_800732AC(buf->bars_lit, buf->bars, sizeof(buf->bars));
    for (i = 0; i < 6; i++) {
        ((PacketTag *)&buf->bars_dim[i])->len = 5;
        *(u32 *)&buf->bars_dim[i].r0 = 0x28806060;
    }
    for (i = 0; i < 6; i++) {
        ((PacketTag *)&buf->bars_lit[i])->len = 5;
        *(u32 *)&buf->bars_lit[i].r0 = 0x280000FF;
    }
    *(u32 *)&buf->arrows[0][0].x0 = 0x200014;
    *(u32 *)&buf->arrows[0][0].x1 = 0x20001C;
    *(u32 *)&buf->arrows[0][0].x2 = 0x28001C;
    *(u32 *)&buf->arrows[0][1].x0 = 0x200013;
    *(u32 *)&buf->arrows[0][1].x1 = 0x290013;
    *(u32 *)&buf->arrows[0][1].x2 = 0x29001B;
    *(u32 *)&buf->arrows[0][2].x0 = 0x320013;
    *(u32 *)&buf->arrows[0][2].x1 = 0x2A0013;
    *(u32 *)&buf->arrows[0][2].x2 = 0x2A001B;
    for (i = 0; i < 6; i++) {
        ((PacketTag *)&buf->arrows[0][i])->len = 4;
        *(u32 *)&buf->arrows[0][i].r0 = 0x2000FF00;
        ((PacketTag *)&buf->arrows[1][i])->len = 4;
        *(u32 *)&buf->arrows[1][i].r0 = 0x2000FF00;
        buf->arrows[1][i].y0 = buf->arrows[0][i].y0;
        buf->arrows[1][i].y1 = buf->arrows[0][i].y1;
        buf->arrows[1][i].x0 = 0x140 - buf->arrows[0][i].x0;
        buf->arrows[1][i].y2 = buf->arrows[0][i].y2;
        buf->arrows[1][i].x1 = 0x140 - buf->arrows[0][i].x1;
        buf->arrows[1][i].x2 = 0x140 - buf->arrows[0][i].x2;
    }
    *(u32 *)&buf->marks[1].r0 = *(u32 *)&buf->marks[0].r0 = 0x28000000;
    buf->marks[0].x1 = 0x63;
    buf->marks[0].x3 = 0x5E;
    buf->marks[0].y0 = buf->marks[0].y1 = 9;
    buf->marks[0].y2 = buf->marks[0].y3 = 0x14;
    buf->marks[1].x0 = buf->marks[1].x2 = 0x122;
    buf->marks[1].x1 = 0xE3;
    buf->marks[1].x3 = 0xDE;
    buf->marks[1].y0 = buf->marks[1].y1 = 0x13;
    buf->arrows[1][1].y0--;
    buf->marks[1].y2 = buf->marks[1].y3 = buf->marks[0].x0 = buf->marks[0].x2 = 0x1E;
    buf->arrows[1][1].x2--;
    ((PacketTag *)&buf->marks[2])->len = ((PacketTag *)&buf->marks[1])->len =
        ((PacketTag *)&buf->marks[0])->len = 5;
    *(u32 *)&buf->marks[2].r0 = 0x280000FF;
    *(u32 *)&buf->marks[2].x0 = 0x320006;
    *(u32 *)&buf->marks[2].x1 = 0x36000A;
    *(u32 *)&buf->marks[2].x2 = 0x4C0006;
    *(u32 *)&buf->marks[2].x3 = 0x48000A;
    ((PacketTag *)&buf->marks[3])->len = 5;
    *(u32 *)&buf->marks[3].r0 = 0x280000FF;
    *(u32 *)&buf->marks[3].x0 = 0x32013A;
    *(u32 *)&buf->marks[3].x1 = 0x360136;
    *(u32 *)&buf->marks[3].x2 = 0x4C013A;
    *(u32 *)&buf->marks[3].x3 = 0x480136;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu5", func_80085EC8);
#endif

/* Build a textured quad (and its second-buffer copy) showing a whole TIM
 * image at (x, y); `depth` is the TIM colour mode (0 = 4-bit, 1 = 8-bit,
 * 2 = 16-bit), which sets how many pixels one VRAM word holds. */
void func_800864B4(TimImage *tim, s32 x, s32 y, PolyFT4 *quad, s32 depth) {
    s32 scale;
    s32 right;

    switch (depth) {
    case 0:
        scale = 4;
        break;
    case 1:
        scale = 2;
        break;
    case 2:
        scale = 1;
        break;
    }
    ((PacketTag *)quad)->len = 9;
    quad->code = 0x2D;
    quad->clut = GetClut(tim->crect->x, tim->crect->y);
    quad->tpage = GetTPage(depth, 0, tim->prect->x, tim->prect->y);
    quad->x0 = quad->x2 = x;
    right = x + tim->prect->w * scale;
    if (scale == 2) {
        quad->x3 = right + 1;
    } else {
        quad->x3 = right;
    }
    quad->y0 = quad->y1 = y;
    quad->x1 = quad->x3 = quad->x3; /* the original stores x3 again */
    quad->y2 = quad->y3 = tim->prect->h + y;
    quad->u0 = quad->u2 = tim->prect->x * scale;
    quad->u1 = quad->u3 = (tim->prect->x + tim->prect->w) * scale;
    quad->v0 = quad->v1 = tim->prect->y;
    quad->v2 = quad->v3 = tim->prect->y + tim->prect->h;
    quad[1] = quad[0];
}

/* The same quad mirrored horizontally (texture u runs right to left). */
void func_800866D4(TimImage *tim, s32 x, s32 y, PolyFT4 *quad, s32 depth) {
    s32 scale;
    s32 right;

    switch (depth) {
    case 0:
        scale = 4;
        break;
    case 1:
        scale = 2;
        break;
    case 2:
        scale = 1;
        break;
    }
    ((PacketTag *)quad)->len = 9;
    quad->code = 0x2D;
    quad->clut = GetClut(tim->crect->x, tim->crect->y);
    quad->tpage = GetTPage(depth, 0, tim->prect->x, tim->prect->y);
    right = x + tim->prect->w * scale;
    quad->x1 = quad->x3 = x;
    quad->x0 = quad->x2 = right;
    quad->u0 = quad->u2 = tim->prect->x * scale - 1;
    quad->u1 = quad->u3 = (tim->prect->x + tim->prect->w) * scale - 1;
    quad->y0 = quad->y1 = y;
    quad->y2 = quad->y3 = tim->prect->h + y;
    quad->v0 = quad->v1 = tim->prect->y;
    quad->v2 = quad->v3 = tim->prect->y + tim->prect->h;
    quad[1] = quad[0];
}

/* Build the HUD packets: the two name plates (left and mirrored right)
 * from the name TIM, the icon and gauge sprites, the gauge bar quads from
 * the bar TIM, the HUD texture page modes and the gauge palette. Does not match:
 * the HUD and icon base addresses are held in saved registers from early on. */
#ifdef NON_MATCHING
void func_800868E0(StageFiles *files) {
    TimImage tim;
    Rect rect;
    s16 *clut;
    Hud *hud = &D_80095698;
    SpritePair *icon = hud->icon;

    OpenTIM(files->name_tim);
    ReadTIM(&tim);
    clut = (s16 *)tim.caddr;
    clut[0] = 0;
    clut[1] = 0x8000;
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    func_800864B4(&tim, 6, 7, hud->name_l, 0);
    func_800866D4(&tim, 0x13A - tim.prect->w * 4, 7, hud->name_r, 0);
    SetDrawTPage(&D_80095918[0], 0, 1, GetTPage(1, 0, 0x380, 0x100));
    D_80095918[1] = D_80095918[0];
    SetDrawTPage(&D_80095918[2], 0, 1, GetTPage(0, 0, 0x380, 0x100));
    D_80095918[3] = D_80095918[2];
    ((PacketTag *)&icon[0].s[0])->len = 4;
    icon[0].s[0].code = 0x65;
    *(u32 *)&icon[0].s[0].x0 = 0x90007;
    *(u16 *)&icon[0].s[0].u0 = 0;
    *(u32 *)&icon[0].s[0].w = 0x160016;
    icon[0].s[0].clut = GetClut(0, 0x1F6);
    icon[1] = icon[0];
    icon[2] = icon[1];
    icon[2].s[0].x0 = 0x123;
    icon[2].s[0].u0 = 0x20;
    icon[2].s[0].clut = GetClut(0, 0x1F7);
    icon[3] = icon[2];
    OpenTIM(files->bar_tim);
    ReadTIM(&tim);
    LoadImage(tim.prect, tim.paddr);
    func_800864B4(&tim, 6, 0x20, D_80095698.bar_l, 0);
    func_800866D4(&tim, 0x13A - tim.prect->w * 4, 0x20, D_80095698.bar_r, 0);
    *(u32 *)&D_80095698.gauge[0].s[0].w = 0x80040;
    D_80092860 = D_80095698.bar_l[0].v0;
    D_80092864 = D_80095698.bar_r[0].v0;
    *(u32 *)&D_80095698.bar_l[0].r0 = 0x2C000080;
    *(u32 *)&D_80095698.bar_l[1].r0 = 0x2C000080;
    *(u32 *)&D_80095698.bar_r[0].r0 = 0x2C000080;
    *(u32 *)&D_80095698.bar_r[1].r0 = 0x2C000080;
    ((PacketTag *)&D_80095698.bar_l[0])->len = 9;
    ((PacketTag *)&D_80095698.bar_l[1])->len = 9;
    ((PacketTag *)&D_80095698.bar_r[0])->len = 9;
    ((PacketTag *)&D_80095698.bar_r[1])->len = 9;
    ((PacketTag *)&D_80095698.gauge[0].s[0])->len = 4;
    D_80095698.gauge[0].s[0].code = 0x65;
    *(u16 *)&D_80095698.gauge[0].s[0].u0 = 0x80;
    D_80095698.bar_l[1].clut = hud->name_l[0].clut;
    D_80095698.bar_l[0].clut = hud->name_l[0].clut;
    D_80095698.bar_r[1].clut = hud->name_l[0].clut;
    D_80095698.bar_r[0].clut = hud->name_l[0].clut;
    D_80095698.gauge[0].s[0].clut = GetClut(0x3A0, 0x110);
    *(u32 *)&D_80095698.gauge[0].s[0].x0 = 0xB001E;
    D_80095698.gauge[2] = D_80095698.gauge[0];
    *(u32 *)&D_80095698.gauge[2].s[0].x0 = 0x1500E3;
    *(u16 *)&D_80095698.gauge[2].s[0].u0 = 0x880;
    D_80095698.gauge[1] = D_80095698.gauge[0];
    D_80095698.gauge[3] = D_80095698.gauge[2];
    rect.x = 0x3A0;
    rect.y = 0x110;
    rect.w = 0x10;
    rect.h = 1;
    LoadImage(&rect, D_80091814);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu5", func_800868E0);
#endif

/* Link this buffer's overlay packets into the overlay ordering table. */
void func_80086E24(void) {
    AddPrim(D_80092938, &D_8009A2F8[D_800928A0].tpage[1]);
}

/* Link a gauge bar filled to `value`: the first part up to 0x38, a sloped
 * second part up to 0x48, then the third part. */
void func_80086E70(void *ot, GaugeBar *bar, s32 value, s32 mirrored) {
    s32 over;

    if (value >= 0x38) {
        func_80085E90(mirrored, &bar->parts[0].x1, 0x38);
        func_80085E90(mirrored, &bar->parts[0].x3, 0x34);
        AddPrim(ot, &bar->parts[0]);
    } else {
        func_80085E90(mirrored, &bar->parts[0].x1, value);
        func_80085E90(mirrored, &bar->parts[0].x3, value - 4);
        AddPrim(ot, &bar->parts[0]);
        return;
    }
    if (value >= 0x48) {
        func_80085E90(mirrored, &bar->parts[1].x1, 0x48);
        func_80085E90(mirrored, &bar->parts[1].x3, 0x40);
        func_80085EAC(mirrored, &bar->parts[1].y3, 0x10);
        AddPrim(ot, &bar->parts[1]);
    } else {
        func_80085E90(mirrored, &bar->parts[1].x1, value);
        over = value - 0x38;
        func_80085E90(mirrored, &bar->parts[1].x3, value - (over / 4 + 4));
        func_80085EAC(mirrored, &bar->parts[1].y3, over / 2 + 8);
        AddPrim(ot, &bar->parts[1]);
        return;
    }
    func_80085E90(mirrored, &bar->parts[2].x1, value);
    func_80085E90(mirrored, &bar->parts[2].x3, value - 8);
    AddPrim(ot, &bar->parts[2]);
}

/* Colour a marker packet by an actor's state: none (returns 0),
 * yellow when set, red otherwise. */
s32 func_80086FF8(Actor *actor, PolyF4 *packet) {
    if (func_8008F530(actor, 0)) {
        return 0;
    }
    if (func_8008F530(actor, 1)) {
        ((PacketTag *)packet)->len = 5;
        *(u32 *)&packet->r0 = 0x2800FFFF;
    } else {
        ((PacketTag *)packet)->len = 5;
        *(u32 *)&packet->r0 = 0x280000FF;
    }
    return 1;
}

/* Link the HUD and map overlay for this frame: each side's arrows (one per
 * point), state marks, name plates, icons, gauges, the charge bars (flashing
 * when nearly full) and the level bars (tinted by the level). Does not match:
 * one fewer saved register is used (register allocation of the buffer, HUD and count temporaries). */
#ifdef NON_MATCHING
void func_80087068(Actor *left, Actor *right) {
    OverlayBuffer *buf;
    Hud *hud;
    DrawTPage *tpage;
    PolyF4 *mark;
    PolyFT4 *bar;
    s32 n;
    s32 level;
    s32 green;
    s32 blue;

    buf = &D_8009A2F8[D_800928A0];
    n = left->unkF2;
    if (n > 0) {
        AddPrim(D_80092938, &buf->arrows[0][0]);
    }
    if (n > 1) {
        AddPrim(D_80092938, &buf->arrows[0][1]);
    }
    if (n > 2) {
        AddPrim(D_80092938, &buf->arrows[0][2]);
    }
    n = right->unkF2;
    if (n > 0) {
        AddPrim(D_80092938, &buf->arrows[1][0]);
    }
    if (n > 1) {
        AddPrim(D_80092938, &buf->arrows[1][1]);
    }
    if (n > 2) {
        AddPrim(D_80092938, &buf->arrows[1][2]);
    }
    mark = &buf->marks[2];
    if (func_80086FF8(left, mark)) {
        AddPrim(D_80092938, mark);
    }
    mark = &buf->marks[3];
    if (func_80086FF8(right, mark)) {
        AddPrim(D_80092938, mark);
    }
    hud = &D_80095698;
    AddPrim(D_80092938, &hud->name_l[D_800928A0]);
    AddPrim(D_80092938, &hud->name_r[D_800928A0]);
    AddPrim(D_80092938, &hud->icon[D_800928A0]);
    AddPrim(D_80092938, &hud->icon[2 + D_800928A0]);
    tpage = D_80095918;
    AddPrim(D_80092938, &tpage[D_800928A0]);
    AddPrim(D_80092938, &hud->gauge[D_800928A0]);
    AddPrim(D_80092938, &hud->gauge[2 + D_800928A0]);
    tpage += 2;
    AddPrim(D_80092938, &tpage[D_800928A0]);
    AddPrim(D_80092938, &hud->bar_r[D_800928A0]);
    AddPrim(D_80092938, &hud->bar_l[D_800928A0]);
    AddPrim(D_80092938, &buf->marks[0]);
    AddPrim(D_80092938, &buf->marks[1]);
    AddPrim(D_80092938, &buf->frame[0]);
    AddPrim(D_80092938, &buf->frame[2]);
    func_80086E70(D_80092938, (GaugeBar *)&buf->bars[0], D_8009872C.unkC0, 0);
    func_80086E70(D_80092938, (GaugeBar *)&buf->bars[3], D_80097010.unkC0, 1);
    n = left->charge >> 6;
    bar = &D_80095698.bar_l[D_800928A0];
    if (n > 0x38) {
        bar->r0 = D_80059488 << 3;
    } else {
        bar->r0 = 0xFF;
    }
    bar->y0 = bar->y1 = 0x60 - n;
    bar->v0 = bar->v1 = D_80092860 - (n - 0x40);
    n = right->charge >> 6;
    bar = &D_80095698.bar_r[D_800928A0];
    if (n > 0x38) {
        bar->r0 = D_80059488 << 3;
    } else {
        bar->r0 = 0xFF;
    }
    bar->y0 = bar->y1 = 0x60 - n;
    bar->v0 = bar->v1 = D_80092860 - (n - 0x40);
    if (left->level != 0) {
        level = 0x100 - left->level;
        green = (level * 3) >> 3;
        blue = level >> 1;
        for (n = 0; n < 3; n++) {
            buf->bars_lit[n].g0 = green;
            buf->bars_lit[n].b0 = blue;
            buf->bars_lit[n].r0 = green + ((left->level * 255) >> 8);
        }
        func_80086E70(D_80092938, (GaugeBar *)&buf->bars_lit[0], left->unkC1, 0);
    }
    if (right->level != 0) {
        level = 0x100 - right->level;
        green = (level * 3) >> 3;
        blue = level >> 1;
        for (n = 3; n < 6; n++) {
            buf->bars_lit[n].g0 = green;
            buf->bars_lit[n].b0 = blue;
            buf->bars_lit[n].r0 = green + ((right->level * 255) >> 8);
        }
        func_80086E70(D_80092938, (GaugeBar *)&buf->bars_lit[3], right->unkC1, 1);
    }
    for (n = 0; n < 6; n++) {
        AddPrim(D_80092938, &buf->bars_dim[n]);
    }
    AddPrim(D_80092938, buf);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu5", func_80087068);
#endif

/* Build the overlay packets for buffer 0 and copy them to buffer 1. */
void func_800875EC(void) {
    func_80085EC8(&D_8009A2F8[0]);
    D_8009A2F8[1] = D_8009A2F8[0];
}

/* Empty the map's row spans (left 0xFF, right 0). */
void func_80087650(void) {
    s32 row;

    for (row = 0; row < 0x80; row++) {
        D_800927CC[row] = 0;
        D_800927D0[row] = 0xFF;
    }
}

/* Widen the map's row spans along a line, clamped to each row's limits. */
void func_80087698(s32 x0, s32 y0, s32 x1, s32 y1) {
    s32 x;
    s32 step;
    s32 row;
    s32 column;
    s32 swap;

    if (y0 == y1) {
        return;
    }
    if (y1 < y0) {
        swap = x1;
        x1 = x0;
        x0 = swap;
        swap = y1;
        y1 = y0;
        y0 = swap;
    }
    x = x0 << 8;
    step = ((x1 - x0) << 8) / (y1 - y0);
    for (row = y0; row < y1; row++, x += step) {
        if (row < 0) {
            continue;
        }
        if (row >= 0x80) {
            return;
        }
        column = x >> 8;
        if (column < D_800927D0[row]) {
            if (column < D_80091834[row]) {
                column = D_80091834[row];
            }
            D_800927D0[row] = column;
        }
        if (D_800927CC[row] < column) {
            if (D_800918B4[row] + 1 < column) {
                column = D_800918B4[row] + 1;
            }
            D_800927CC[row] = column;
        }
    }
}

/* Draw the map triangles: load the map colour (as a textured-triangle
 * code) into the GTE, copy the 0x30-byte map table into the scratchpad and
 * run the triangle loop. */
void func_8008779C(s32 arg0, s32 arg1, s32 arg2) {
    D_80059598 = (D_80059598 & 0xFFFFFF) | 0x24000000;
    gte_ldrgb(&D_80059598);
    func_800732AC((void *)0x1F800120, &D_80091934, sizeof(MapTable));
    func_80072D18(arg0, arg1, arg2);
}

/* Set up the map row spans in the scratchpad and the two textured
 * triangle packet pools (0x708 triangles each). */
void func_80087830(void) {
    PolyFT3 *poly;
    s32 i;

    D_800927CC = (u8 *)0x1F800000;
    D_800927D0 = (u8 *)0x1F800080;
    D_80092854[0] = func_80031BDC(0xE100, 0);
    D_80092854[1] = func_80031BDC(0xE100, 0);
    poly = D_80092854[0];
    for (i = 0; i < 0x708; i++) {
        ((PacketTag *)poly)->len = 7;
        poly->code = 0x24;
        poly++;
    }
    func_800732AC(D_80092854[1], D_80092854[0], 0xE100);
}

/* Load the stage's icon, backdrop and extra TIM images into VRAM, noting
 * the icon and backdrop palettes and texture pages; the backdrop palette's
 * first entry is transparent and the rest semi-transparent. Does not match:
 * the icon table is walked with two pointers and the loop counter is kept. */
#ifdef NON_MATCHING
void func_800878DC(StageFiles *files) {
    TimImage tim;
    s16 *clut;
    s32 i;

    for (i = 0; i < 4; i++) {
        OpenTIM(files->icon_tims[i]);
        ReadTIM(&tim);
        D_80091934.icons[i].clut = GetClut(tim.crect->x, tim.crect->y);
        D_80091934.icons[i].tpage = GetTPage(1, 1, tim.prect->x, tim.prect->y);
        LoadImage(tim.crect, tim.caddr);
        LoadImage(tim.prect, tim.paddr);
    }
    OpenTIM(files->backdrop_tim);
    ReadTIM(&tim);
    D_800927D8 = GetClut(tim.crect->x, tim.crect->y);
    D_800927D4 = GetTPage(0, 2, tim.prect->x, tim.prect->y);
    D_800927DC = tim.prect->x << 2;
    D_800927E0 = tim.prect->y;
    clut = (s16 *)tim.caddr;
    clut[0] = 0;
    for (i = 1; i < 16; i++) {
        clut[i] |= 0x8000;
    }
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    for (i = 0x1C; i < 0x25; i++) {
        OpenTIM(files->extra_tims[i - 0x1C]);
        ReadTIM(&tim);
        LoadImage(tim.crect, tim.caddr);
        LoadImage(tim.prect, tim.paddr);
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu5", func_800878DC);
#endif

/* Build an actor's textured backdrop quad (64x64 texels) for both buffers. */
void func_80087AB0(Actor *actor) {
    PolyFT4 *quad = &actor->backdrop[0];

    *(u32 *)&quad->r0 = 0x2C101010;
    ((PacketTag *)quad)->len = 9;
    quad->code |= 2;
    quad->clut = D_800927D8;
    quad->tpage = D_800927D4;
    *(u16 *)&quad->u0 = D_800927DC | (D_800927E0 << 8);
    *(u16 *)&quad->u1 = (D_800927DC + 0x3F) | (D_800927E0 << 8);
    *(u16 *)&quad->u2 = D_800927DC | ((D_800927E0 + 0x3F) << 8);
    *(u16 *)&quad->u3 = (D_800927DC + 0x3F) | ((D_800927E0 + 0x3F) << 8);
    actor->backdrop[1] = actor->backdrop[0];
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu5", func_80087B74);

/* Record a position in the path list (up to 31 entries). Does not match:
 * the entry address is formed base-first and registers differ. */
#ifdef NON_MATCHING
void func_80087E38(Vector *pos) {
    PathPoint *point;

    if (D_800928F8 < 0x1F) {
        point = &D_8009A988[D_800928F8];
        point->x = pos->vx;
        point->y = pos->vy;
        D_800928F8++;
        point->z = pos->vz;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu5", func_80087E38);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu5", func_80087EA0);

/* Start a debug line between two points in one of eight colours (bit 0
 * blue, bit 1 red, bit 2 green). Returns the line, or NULL when all 100
 * are in use. */
Line3D *func_8008820C(Vector *from, Vector *to, s32 colour) {
    Line3D *line;
    s32 i;

    for (i = 0; i < 100; i++) {
        line = &D_80095938[i];
        if (line->timer == 0) {
            line->timer = 1;
            line->from.vx = from->vx;
            line->from.vy = from->vy;
            line->from.vz = from->vz;
            line->to.vx = to->vx;
            line->to.vy = to->vy;
            line->to.vz = to->vz;
            line->packets[0].r0 = (colour & 2) * 0x7F;
            line->packets[0].g0 = (colour & 4) * 0x3F;
            line->packets[0].b0 = (colour & 1) * 0xFF;
            line->packets[1].r0 = (colour & 2) * 0x7F;
            line->packets[1].g0 = (colour & 4) * 0x3F;
            line->packets[1].b0 = (colour & 1) * 0xFF;
            return line;
        }
    }
    return NULL;
}

/* Start a debug line that stays for the given number of frames. */
void func_800882D4(Vector *from, Vector *to, s32 colour, s32 frames) {
    Line3D *line = func_8008820C(from, to, colour);

    if (line != NULL) {
        line->timer = frames;
    }
}

/* Stop every debug line. */
void func_80088308(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        D_80095938[i].timer = 0;
    }
}

/* Project and link every live debug line, counting its frames down. */
void func_8008832C(void *ot) {
    SVector ends[2];
    Line3D *line;
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_80095938[i].timer != 0) {
            line = &D_80095938[i];
            line->timer--;
            ends[0] = line->from;
            ends[1] = line->to;
            ends[0].vx -= D_80096FA8.vx;
            ends[0].vy -= D_80096FA8.vy;
            ends[0].vz -= D_80096FA8.vz;
            ends[1].vx -= D_80096FA8.vx;
            ends[1].vy -= D_80096FA8.vy;
            ends[1].vz -= D_80096FA8.vz;
            gte_ldv01(&ends[0], &ends[1]);
            gte_rtpt();
            gte_stsxy01(&line->packets[D_800928A0].x0, &line->packets[D_800928A0].x1);
            ((PacketTag *)&line->packets[D_800928A0])->len = 3;
            ((PacketTag *)&line->packets[D_800928A0])->code = 0x40;
            func_800316C0(ot, &line->packets[D_800928A0]);
        }
    }
}

/* Scale a vector down by the square root of its (absolute) length measure
 * and pass it on. */
void func_800884E0(Vector *vector, void *out) {
    Vector scaled = *vector;
    s32 square;
    s32 length;

    square = func_8002DC9C(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = SquareRoot0(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    func_80048D7C(&scaled, out);
}

/* Scale a vector down by the square root of its (absolute) length measure
 * and pass it to VectorNormalS. */
void func_8008859C(Vector *vector, void *out) {
    Vector scaled = *vector;
    s32 square;
    s32 length;

    square = func_8002DC9C(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = SquareRoot0(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    VectorNormalS(&scaled, out);
}

/* The same for a short vector. */
void func_80088658(SVector *vector, void *out) {
    Vector scaled;
    s32 square;
    s32 length;

    scaled.vx = vector->vx;
    scaled.vy = vector->vy;
    scaled.vz = vector->vz;
    square = func_8002DC9C(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = SquareRoot0(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    VectorNormalS(&scaled, out);
}

/* Length of a vector. */
s32 func_800886FC(Vector *vector) {
    Vector square;

    gte_ldlvl(vector);
    gte_sqr0();
    gte_stlvnl(&square);
    return SquareRoot0(square.vx + square.vy + square.vz);
}

/* Horizontal (x/z) length of a vector. */
s32 func_80088754(Vector *vector) {
    Vector square;

    gte_ldlvl(vector);
    gte_sqr0();
    gte_stlvnl(&square);
    return SquareRoot0(square.vx + square.vz);
}

/* Distance between two points. */
s32 func_800887A4(Vector *from, Vector *to) {
    Vector delta;

    delta.vx = to->vx - from->vx;
    delta.vy = to->vy - from->vy;
    delta.vz = to->vz - from->vz;
    gte_ldlvl(&delta);
    gte_sqr0();
    gte_stlvnl(&delta);
    return SquareRoot0(delta.vx + delta.vy + delta.vz);
}

/* Horizontal (x/z) distance between two points. */
s32 func_80088838(Vector *from, Vector *to) {
    Vector delta;

    delta.vx = to->vx - from->vx;
    delta.vz = to->vz - from->vz;
    gte_ldlvl(&delta);
    gte_sqr0();
    gte_stlvnl(&delta);
    return SquareRoot0(delta.vx + delta.vz);
}

/* Set a bit of the resident flag array. */
void func_800888B0(s32 flag) {
    s32 bit;

    bit = 1;
    bit <<= flag & 7;
    D_8006F978.flags[flag >> 3] |= bit;
}

/* Test a bit of the resident flag array. */
s32 func_800888E4(s32 flag) {
    s32 bit;

    bit = 1;
    bit <<= flag & 7;
    return D_8006F978.flags[flag >> 3] & bit;
}

/* Clear a bit of the resident flag array. */
void func_80088908(s32 flag) {
    s32 bit;

    bit = 1;
    bit <<= flag & 7;
    D_8006F978.flags[flag >> 3] &= ~bit;
}

/* Set bit 16 of the resident state word. */
void func_80088940(void) {
    s32 *state = &D_8006F980;

    *state |= 0x10000;
}

/* Once bit 16 of the resident state word is set, queue the D_80091A6C
 * entry (only once). */
void func_8008895C(void) {
    if ((D_8006F980 & 0x10000) && D_800927EC == 0) {
        D_800927EC = 1;
        D_800928EC[D_80092888++] = &D_80091A6C;
    }
}

/* Once every progress flag 0..48 except 22 is set, mark the options
 * complete and apply the unlock. */
s32 func_800889C8(void) {
    s32 flag;

    for (flag = 0; flag < 49; flag++) {
        if (flag != 22 && !func_800888E4(flag)) {
            return 0;
        }
    }
    D_8006F978.options.complete = 1;
    func_8008895C();
    return 0;
}

/* Store the current option settings in the saved options word. */
void func_80088A40(void) {
    if (D_8005061C) {
        D_8006F978.options.version = 1;
        D_8006F978.options.option4 = D_80099D98.option4;
        D_8006F978.options.option5 = D_80099D98.option5;
        D_8006F978.options.option6 = D_80099D98.option6;
        D_8006F978.options.option13 = D_80099D98.level;
    }
}

/* Load the option settings from the saved options word, or write the
 * defaults when it was never written; a completed word clears the flags. */
void func_80088AF8(void) {
    s32 i;

    if (D_8005061C) {
        D_800927EC = 0;
        if (D_8006F978.options.version == 1) {
            D_80099D98.option4 = D_8006F978.options.option4;
            D_80099D98.option5 = D_8006F978.options.option5;
            D_80099D98.option6 = D_8006F978.options.option6;
            D_80099D98.level = D_8006F978.options.option13;
            if (D_8006F978.options.complete) {
                for (i = 0; i < 8; i++) {
                    D_8006F978.flags[i] = 0;
                }
            }
        } else {
            D_80099D98.option4 = 0;
            D_80099D98.option5 = 0;
            D_80099D98.option6 = 2;
            D_80099D98.level = 0;
            func_80088A40();
        }
    }
}

/* Set a progress flag, then check for completion. */
void func_80088BD4(s32 flag) {
    func_800888B0(flag);
    func_800889C8();
}
