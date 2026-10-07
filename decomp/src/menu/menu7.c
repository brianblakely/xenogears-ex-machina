#include "menu.h"
#include "sparkle.h"
#include "scene.h"
#include "spark.h"
#include "sound.h"
#include "brain.h"
#include "window.h"
#include "gte.h"

/* Load a whole file into a new allocation and return it. */
void *func_800891C0(s32 file) {
    void *data = func_80031BDC(func_80028738(file), 1);

    func_800295D8(file, data, 0, 0);
    return data;
}

/* Set the screen scale matrices for a width x height display (320x240 is
 * unit scale). */
void func_80089210(s32 width, s32 height) {
    s32 sx = ((width << 12) / 320) * height / width;
    s32 sy = ((height << 12) / 240) * height / width;

    D_8009A2D8 = D_80091C0C;
    D_80096FE0 = D_80091C0C;
    D_80096FE0.m[0][0] = sx;
    D_80096FE0.m[1][1] = sy;
}

/* Set both buffers' display environments and background tiles for the
 * resolution; taller than 256 lines is interlaced. */
void func_80089330(s32 width, s32 height) {
    if (height > 256) {
        SetDefDispEnv(&D_8009A0D8[0].disp, 0, 0, width, height);
        SetDefDispEnv(&D_8009A0D8[1].disp, 0, 0, width, height);
        D_8009A0D8[1].disp.isinter = 1;
        D_8009A0D8[0].disp.isinter = 1;
        D_8009A0D8[0].disp.screen.x = 0;
        D_8009A0D8[0].disp.screen.y = 0x10;
        D_8009A0D8[0].disp.screen.w = 0x100;
        D_8009A0D8[0].disp.screen.h = 0xD4;
        D_8009A0D8[1].disp.screen.x = 0;
        D_8009A0D8[1].disp.screen.y = 0x10;
        D_8009A0D8[1].disp.screen.w = 0x100;
        D_8009A0D8[1].disp.screen.h = 0xD4;
    } else {
        SetDefDispEnv(&D_8009A0D8[0].disp, 0, 0x100, width, height);
        SetDefDispEnv(&D_8009A0D8[1].disp, 0, 0, width, height);
        D_8009A0D8[1].disp.isinter = 0;
        D_8009A0D8[0].disp.isinter = 0;
        D_8009A0D8[0].disp.screen.x = 0;
        D_8009A0D8[0].disp.screen.y = 0xA;
        D_8009A0D8[0].disp.screen.w = 0x100;
        D_8009A0D8[0].disp.screen.h = height;
        D_8009A0D8[1].disp.screen.x = 0;
        D_8009A0D8[1].disp.screen.y = 0xA;
        D_8009A0D8[1].disp.screen.w = 0x100;
        D_8009A0D8[1].disp.screen.h = height;
    }
    D_8009285C = width;
    D_8009286C = height;
    func_80089210(D_8009285C, D_8009286C);
    D_8009A0D8[0].background.len = 3;
    D_8009A0D8[0].background.colour = 0x60000000;
    D_8009A0D8[0].background.x0 = 0;
    D_8009A0D8[0].background.y0 = 0;
    D_8009A0D8[0].background.w = width;
    D_8009A0D8[0].background.h = height;
    D_8009A0D8[1].background = D_8009A0D8[0].background;
}

/* Set the geometry and both buffers' drawing environments for the
 * resolution. */
void func_80089534(s32 width, s32 height) {
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(0x180);
    func_8002DFF0(width, height);
    if (height > 256) {
        SetDefDrawEnv(&D_8009A0D8[0].draw, 0, 0, width, height);
        SetDefDrawEnv(&D_8009A0D8[1].draw, 0, 0, width, height);
        D_8009A0D8[1].draw.dfe = 0;
        D_8009A0D8[0].draw.dfe = 0;
    } else {
        SetDefDrawEnv(&D_8009A0D8[0].draw, 0, 0, width, height);
        SetDefDrawEnv(&D_8009A0D8[1].draw, 0, 0x100, width, height);
    }
    D_8009A0D8[0].draw.dtd = D_8009A0D8[1].draw.dtd = 1;
    D_8009A0D8[0].draw.isbg = D_8009A0D8[1].draw.isbg = 0;
    D_8009A0D8[0].draw.tpage = D_8009A0D8[1].draw.tpage = GetTPage(0, 2, 0x280, 0);
    SetDrawEnv(D_8009A0D8[0].draw.dr_env, &D_8009A0D8[0].draw);
    SetDrawEnv(D_8009A0D8[1].draw.dr_env, &D_8009A0D8[1].draw);
    SetDrawArea(D_8009A0D8[0].modeD0, &D_8009A0D8[0].draw.clip);
    SetDrawArea(D_8009A0D8[1].modeD0, &D_8009A0D8[1].draw.clip);
    SetDrawOffset(D_8009A0D8[0].modeDC, D_8009A0D8[0].draw.ofs);
    SetDrawOffset(D_8009A0D8[1].modeDC, D_8009A0D8[1].draw.ofs);
}

/* Set up geometry, screen and scale for a width x height display. */
void func_800896C4(s32 width, s32 height) {
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen((width << 8) / width);
    func_8002DFF0(width, height);
    func_80089210(width, height);
}

/* Re-apply the drawing environments at the current resolution. */
void func_8008973C(void) {
    func_80089534(D_8009285C, D_8009286C);
}

/* Set the display and drawing environments for a resolution. */
void func_8008976C(s32 width, s32 height) {
    func_80089330(width, height);
    func_80089534(width, height);
}

/* Set a layer's drawing areas and offsets for both buffers (the second
 * buffer lies `second` lines lower) and its black background tiles. */
void func_800897AC(OtPair *layer, s32 x, s32 y, s32 w, s32 h, s32 second) {
    Rect area;
    s16 offset[2];

    area.x = x;
    area.y = y;
    area.w = w;
    area.h = h;
    SetDrawArea(layer->area[0], &area);
    area.y = y + second;
    SetDrawArea(layer->area[1], &area);
    offset[0] = x;
    offset[1] = y;
    SetDrawOffset(layer->offset[0], offset);
    offset[1] = y + second;
    SetDrawOffset(layer->offset[1], offset);
    layer->tile[0].len = 3;
    layer->tile[0].colour = 0x60000000;
    layer->tile[0].x0 = 0;
    layer->tile[0].y0 = 0;
    layer->tile[0].w = w;
    layer->tile[0].h = h;
    layer->tile[1] = layer->tile[0];
    layer->flags |= 0xC;
}

/* Build a view matrix looking from eye to at with the given up vector. */
void func_800898BC(Matrix *m, SVector *eye, SVector *at, SVector *up) {
    D_8009A0C8.vx = at->vx - eye->vx;
    D_8009A0C8.vy = at->vy - eye->vy;
    D_8009A0C8.vz = at->vz - eye->vz;
    D_8009A918.vx = up->vx;
    D_8009A918.vy = up->vy;
    D_8009A918.vz = up->vz;
    VectorNormal(&D_8009A0C8, &D_80096F98);
    func_8004A480(&D_8009A918, &D_80096F98, &D_8009A0C8);
    VectorNormal(&D_8009A0C8, &D_80097000);
    func_8004A480(&D_80096F98, &D_80097000, &D_8009A0C8);
    VectorNormal(&D_8009A0C8, &D_8009A918);
    m->m[0][0] = D_80097000.vx;
    m->m[0][1] = D_80097000.vy;
    m->m[0][2] = D_80097000.vz;
    m->m[1][0] = D_8009A918.vx;
    m->m[1][1] = D_8009A918.vy;
    m->m[1][2] = D_8009A918.vz;
    m->m[2][0] = D_80096F98.vx;
    m->m[2][1] = D_80096F98.vy;
    m->m[2][2] = D_80096F98.vz;
    ApplyMatrix(m, eye, &D_8009A0C8);
    MulMatrix2(&D_80096FE0, m);
    m->t[0] = -D_8009A0C8.vx;
    m->t[1] = -D_8009A0C8.vy;
    m->t[2] = -D_8009A0C8.vz;
}

/* Point the owner's view from eye toward target (eye kept as the last eye
 * position). */
void func_80089A98(LightRig *view, Vector *target, Vector *eye) {
    SVector up;
    SVector from;
    SVector origin;

    up.vy = 0x1000;
    up.vz = 0;
    up.vx = 0;
    D_80096FA8 = *eye;
    from.vx = target->vx - eye->vx;
    from.vy = target->vy - eye->vy;
    from.vz = target->vz - eye->vz;
    origin.vz = 0;
    origin.vx = 0;
    origin.vy = 0;
    func_800898BC(&view->camera->view, &from, &origin, &up);
}

/* Reset a node: unlinked, no payload, zero position and angles, identity
 * matrices. */
Node *func_80089B44(Node *node) {
    node->type = 0;
    node->parent = NULL;
    node->child = NULL;
    node->next = NULL;
    node->callback = NULL;
    node->data = NULL;
    node->position.vz = 0;
    node->position.vy = 0;
    node->position.vx = 0;
    node->unk44.vz = 0;
    node->unk44.vy = 0;
    node->unk44.vx = 0;
    node->rotation.vz = 0;
    node->rotation.vy = 0;
    node->rotation.vx = 0;
    node->unk6C = D_80091C0C;
    node->unk4C = node->unk6C;
    node->view = node->unk4C;
    return node;
}

/* Allocate a reset scene node. */
Node *func_80089C54(void) {
    func_800324B8(8);
    return func_80089B44(func_80031BDC(sizeof(Node), 0));
}

/* Append child as the last child of parent. */
void func_80089C88(Node *parent, Node *child) {
    Node *last;

    child->parent = parent;
    if (parent->child == NULL) {
        parent->child = child;
    } else {
        last = parent->child;
        while (last->next != NULL) {
            last = last->next;
        }
        last->next = child;
    }
}

/* Unlink a node from its parent's child list. */
void func_80089CD8(Node *node) {
    Node *parent;
    Node *first;
    Node *prev;

    if (node == NULL) {
        return;
    }
    parent = node->parent;
    if (parent == NULL) {
        return;
    }
    node->parent = NULL;
    first = parent->child;
    if (first == NULL) {
        return;
    }
    if (first == node) {
        parent->child = node->next;
    } else {
        prev = first;
        while (prev->next != node) {
            prev = prev->next;
        }
        prev->next = node->next;
    }
    node->next = NULL;
}

/* Free a node, its children and following siblings, and its payload. */
void func_80089D5C(Node *node) {
    if (node == NULL) {
        return;
    }
    func_80089D5C(node->child);
    func_80089D5C(node->next);
    switch (node->type) {
    case 1:
        func_80089FF8(node->data);
        break;
    case 2:
        func_80089EB4(node->data);
        break;
    case 5:
        func_8008C120(node->data);
        break;
    }
    if (node->data != NULL) {
        func_800320E8(node->data);
    }
    func_800320E8(node);
}

/* Make a node a model node. */
void func_80089E2C(Node *node, Model *model) {
    node->data = model;
    node->type = 1;
}

/* Make a node a type 3 node. */
void func_80089E3C(Node *node) {
    node->type = 3;
}

/* Make a node a type 4 node. */
void func_80089E48(Node *node) {
    node->type = 4;
}

/* Make a node a model set node. */
void func_80089E54(Node *node, ModelSet *set) {
    node->data = set;
    node->type = 2;
}

/* Make a node a type 6 node. */
void func_80089E64(Node *node, void *data) {
    node->data = data;
    node->type = 6;
}

/* Allocate an empty model set payload at unit scale. */
ModelSet *func_80089E74(void) {
    ModelSet *set;

    func_800324B8(4);
    set = func_80031BDC(sizeof(ModelSet), 0);
    set->scale[2] = 0x1000;
    set->scale[1] = 0x1000;
    set->scale[0] = 0x1000;
    set->nodes = NULL;
    set->players = NULL;
    return set;
}

/* Free a model set's node table and players (and, when owned, the
 * players' keys). */
void func_80089EB4(ModelSet *set) {
    s32 i;

    if (set->nodes != NULL) {
        func_800320E8(set->nodes);
    }
    if (set->players != NULL) {
        if (!D_80091C2C) {
            i = set->count;
            while (--i != -1) {
                if (set->players[i].header != NULL) {
                    func_800320E8(set->players[i].keys);
                }
            }
        }
        func_800320E8(set->players);
    }
}

/* Reset a model payload: grey, nothing loaded. */
Model *func_80089F8C(Model *model) {
    model->unk4 = 1;
    model->flags = 0;
    model->packets[0] = NULL;
    model->packets[1] = NULL;
    model->unk10 = 0;
    model->file = NULL;
    model->unk1C = 0;
    model->b = 0x40;
    model->g = 0x40;
    model->r = 0x40;
    return model;
}

/* Allocate a reset model payload. */
Model *func_80089FC4(void) {
    func_800324B8(1);
    return func_80089F8C(func_80031BDC(sizeof(Model), 0));
}

/* Release a model payload's resources. */
void func_80089FF8(Model *model) {
    if (model->packets[0] != NULL) {
        func_80032C18(model->packets[0], 2);
    }
    func_8002CBBC(model->file);
}

/* Pass the model texture page and CLUT positions to target, or zeros when
 * no texture page is set. */
void func_8008A040(void *target) {
    if (D_80092800 > 0) {
        func_8002DDE4(target, 1, D_80092800, D_80092804, 1, D_80092808, D_8009280C);
    } else {
        func_8002DDE4(target, 0, 0, 0, 0, 0, 0);
    }
}

/* Set a model node's colour. */
void func_8008A0B4(Node *node, u8 r, u8 g, u8 b) {
    ((Model *)node->data)->r = r;
    ((Model *)node->data)->g = g;
    ((Model *)node->data)->b = b;
    ((Model *)node->data)->flags |= 0x10;
}

/* Clear a model node's colour override. */
void func_8008A0F4(Node *node) {
    ((Model *)node->data)->flags &= ~0x10;
}

/* Set the texture page position used for loaded models (-1 = none). */
void func_8008A110(s16 x, s16 y) {
    D_80092800 = x;
    D_80092804 = y;
}

/* Set the CLUT position used for loaded models (-1 = none). */
void func_8008A128(s16 x, s16 y) {
    D_80092808 = x;
    D_8009280C = y;
}

/* Set the texture page and CLUT positions used for loaded models. */
void func_8008A140(s16 tx, s16 ty, s16 cx, s16 cy) {
    D_80092800 = tx;
    D_80092804 = ty;
    D_80092808 = cx;
    D_8009280C = cy;
}

/* Use no texture page or CLUT override for loaded models. */
void func_8008A168(void) {
    D_80092800 = D_80092808 = -1;
}

/* Load a model file into a model payload, applying the texture page and
 * CLUT overrides. */
void func_8008A184(Model *model, ModelFile *file) {
    model->file = file;
    model->unk10 = func_800303C8(file, 1);
    func_8002CB54(model->file, &model->packets[0], &model->packets[1]);
    if (D_80092800 >= 0) {
        func_8002CC54(GetTPage(0, 1, D_80092800, D_80092804));
    }
    if (D_80092808 >= 0) {
        func_8002CC74(D_80092808, D_8009280C);
    }
    func_8002C8CC(model->file, model->packets[0], 2);
    func_800732AC(model->packets[1], model->packets[0], model->file->unk34);
    model->flags |= 2;
}

/* Allocate a light with a small diagonal direction and no colour. */
Light *func_8008A254(void) {
    Light *light;

    func_800324B8(0xB);
    light = func_80031BDC(sizeof(Light), 0);
    light->direction[0] = light->direction[1] = light->direction[2] = 0x10;
    light->colour[0] = light->colour[1] = light->colour[2] = 0;
    return light;
}

/* Free a payload. */
void func_8008A298(void *p) {
    func_800320E8(p);
}

/* Allocate an ordering table pair of the given length and its depth
 * shift (the length should be a power of two up to 0x4000). */
OtPair *func_8008A2B8(u16 length) {
    OtPair *pair;
    u32 *ot;
    s32 bit;

    func_800324B8(0xC);
    pair = func_80031BDC(sizeof(OtPair), 0);
    func_800324B8(0xC);
    ot = func_80031BDC(length * 8, 0);
    pair->ot[0] = ot;
    pair->ot[1] = ot + length;
    pair->flags = 1;
    pair->shift = 14;
    pair->unk0 = 0;
    pair->length = length;
    pair->last[0] = &pair->ot[0][length - 1];
    pair->last[1] = &pair->ot[1][length - 1];
    for (bit = 1; bit != length;) {
        bit <<= 1;
        if (bit > 0x4000) {
            pair->shift = 14;
            break;
        }
        pair->shift--;
    }
    return pair;
}

void func_8008A3A0(void) {
}

/* Free a holder and its resource. */
void func_8008A3A8(OtPair *layer) {
    func_80032C18(layer->ot[0], 3);
    func_800320E8(layer);
}

/* Allocate a light rig: a root and three light nodes (key light turned
 * round, fill lights level), grey ambient, owning holder. */
LightRig *func_8008A3E0(OtPair *layer) {
    LightRig *rig;

    func_800324B8(9);
    rig = func_80031BDC(sizeof(LightRig), 0);
    rig->unk0 = 0;
    rig->camera = &rig->storage[0];
    rig->lights[0] = &rig->storage[1];
    rig->lights[1] = &rig->storage[2];
    rig->lights[2] = &rig->storage[3];
    func_80089B44(&rig->storage[0]);
    func_80089B44(&rig->storage[1]);
    func_80089B44(&rig->storage[2]);
    func_80089B44(&rig->storage[3]);
    func_80089E64(rig->lights[0], func_8008A254());
    func_80089E64(rig->lights[1], func_8008A254());
    func_80089E64(rig->lights[2], func_8008A254());
    rig->lights[0]->position.vy = rig->lights[1]->position.vy = rig->lights[2]->position.vy = -2;
    rig->lights[0]->position.vx = 0;
    rig->lights[0]->position.vz = 1;
    rig->lights[1]->position.vx = -1;
    rig->lights[1]->position.vz = -1;
    rig->lights[2]->position.vx = 1;
    rig->lights[2]->position.vz = -1;
    NODE_LIGHT(rig->lights[0])->colour[0] = NODE_LIGHT(rig->lights[0])->colour[1] =
        NODE_LIGHT(rig->lights[0])->colour[2] = 0x800;
    NODE_LIGHT(rig->lights[1])->colour[0] = NODE_LIGHT(rig->lights[1])->colour[1] =
        NODE_LIGHT(rig->lights[1])->colour[2] = 0;
    *(Light *)rig->lights[2]->data = *(Light *)rig->lights[1]->data;
    rig->layer = layer;
    rig->r = rig->g = rig->b = 0;
    rig->r = rig->g = rig->b = 0x10;
    func_8008ABAC(rig->lights);
    return rig;
}

/* Free a light rig, its lights and its holder. */
void func_8008A5BC(LightRig *rig) {
    func_800320E8(rig->storage[1].data);
    func_800320E8(rig->storage[2].data);
    func_800320E8(rig->storage[3].data);
    func_8008A3A8(rig->layer);
    func_800320E8(rig);
}

/* Enable model colour overrides. */
void func_8008A618(void) {
    D_80092810 = 1;
}

/* Disable model colour overrides. */
void func_8008A62C(void) {
    D_80092810 = 0;
}

/* Draw a model into the current ordering table, with its colour override
 * (or grey) as the GTE back colour when overrides are enabled. */
void func_8008A63C(Model *model) {
    D_80050104 = 0;
    if (D_80092810) {
        if (model->flags & 0x10) {
            gte_SetBackColor(model->r, model->g, model->b);
        } else {
            gte_SetBackColor(0x40, 0x40, 0x40);
        }
    }
    func_8002C700(model->file, model->packets[D_800928A0], D_800928E4, model->unk4);
}

/* Set the current buffer's colour, noting whether it changed. */
void func_8008A6F8(CVector *colour) {
    CVector *current = &D_80092818[D_800928A0];

    if (colour->r == current->r && colour->g == current->g && colour->b == current->b) {
        D_80092914 = 0;
    } else {
        D_80092914 = 1;
        *current = *colour;
        current->cd = 0x20;
    }
}

/* Write the current buffer's colour into every primitive of an instance. */
void func_8008A78C(Node *node) {
    ModelPrims *prims = ((Instance *)node->data)->prims;
    s32 i = prims->count;
    ModelPrim *prim = prims->prims[D_800928A0];
    u32 colour = *(u32 *)&D_80092818[D_800928A0];

    while (--i != -1) {
        prim->colour = colour;
        prim++;
    }
}

/* Update a node tree's matrices (model sets relative to the eye, other
 * nodes relative to their parent) and draw its shown models and
 * instances. */
void func_8008A7E0(Node *node) {
    if (node->callback != NULL) {
        node->callback(node);
    }
    switch (node->type) {
    case 2:
        func_8003F738(&node->unk44, &node->view);
        if (D_8009289C) {
            MulMatrix0(&node->parent->view, &node->view, &node->unk6C);
        } else {
            node->unk6C = node->view;
        }
        func_800731F8(&node->view, ((ModelSet *)node->data)->scale);
        node->unk4C = node->view;
        node->unk4C.t[0] = node->unk4C.t[1] = node->unk4C.t[2] = 0;
        node->view.t[0] = node->position.vx - D_80096FA8.vx;
        node->view.t[1] = node->position.vy - D_80096FA8.vy;
        node->view.t[2] = node->position.vz - D_80096FA8.vz;
        CompMatrix(&node->parent->view, &node->view, &node->view);
        break;
    case 0:
    case 1:
        if (node->parent != NULL) {
            node->position.vx = node->rotation.vx;
            node->position.vy = node->rotation.vy;
            node->position.vz = node->rotation.vz;
            func_8003F738(&node->unk44, &node->view);
            TransMatrix(&node->view, &node->position);
            MulMatrix0(&node->parent->unk6C, &node->view, &node->unk6C);
            CompMatrix(&node->parent->unk4C, &node->view, &node->unk4C);
            CompMatrix(&node->parent->view, &node->view, &node->view);
        }
        if (node->type == 1 && !(((Model *)node->data)->flags & 1)) {
            func_80030B14(&node->unk6C);
            gte_SetRotMatrix(&node->view);
            gte_SetTransMatrix(&node->view);
            func_8008A63C(node->data);
        }
        break;
    case 5:
        if (((Instance *)node->data)->type == 1) {
            node->view = ((Instance *)node->data)->source->unk4C;
            gte_SetRotMatrix(&node->view);
            gte_SetTransMatrix(&node->view);
            if (D_80092914) {
                func_8008A78C(node);
            }
            func_8008BCC8(((Instance *)node->data)->prims->mesh, ((Instance *)node->data)->prims->work);
        }
        break;
    }
    if (node->child != NULL) {
        func_8008A7E0(node->child);
    }
    if (node->next != NULL) {
        func_8008A7E0(node->next);
    }
}

/* Load the three rig lights into the light slots. */
void func_8008ABAC(Node **lights) {
    func_80030A30(0, lights[0]->data);
    func_80030A30(1, lights[1]->data);
    func_80030A30(2, lights[2]->data);
}

/* Clear the current buffer's ordering table of a pair and make it the one
 * primitives are added to. */
void func_8008AC0C(OtPair *pair) {
    ClearOTagR(pair->ot[D_800928A0], pair->length);
    D_800928E4 = pair->ot[D_800928A0];
    D_80050100 = pair->shift;
}

/* Choose the ordering table pair to compact at the end of the frame. */
void func_8008AC7C(OtPair *pair) {
    D_80091C30 = pair;
}

/* Note the frame's start time. */
void func_8008AC8C(void) {
    D_80092820 = GetRCnt(0xF2000001);
}

/* While time remains in the frame budget (frames x 240 ticks, default
 * 192), link the tags the chosen table's entries point at past runs of
 * empty primitives, from the deepest entry down to entry 4. */
void func_8008ACB8(s32 frames) {
    OtPair *pair = D_80091C30;
    s32 start;
    s32 limit;
    s32 elapsed;
    s32 i;
    u32 *entry;
    u32 *tag;

    if (pair == NULL) {
        return;
    }
    start = D_80092820;
    D_80091C30 = NULL;
    if (frames != 0) {
        limit = frames * 240;
    } else {
        limit = 0xC0;
    }
    for (i = pair->length - 1; i >= 4; i--) {
        elapsed = GetRCnt(0xF2000001) - start;
        if (elapsed < 0) {
            elapsed += 0x10000;
        }
        if (elapsed > limit) {
            return;
        }
        entry = (u32 *)pair->ot[D_800928A0][i];
        tag = (u32 *)((*entry & 0xFFFFFF) - 0x80000000);
        if (TAG_LEN(tag) == 0) {
            while (i >= 5) {
                tag = (u32 *)((*tag & 0xFFFFFF) - 0x80000000);
                i--;
                if (TAG_LEN(tag) != 0) {
                    break;
                }
            }
            *entry = (*entry & 0xFF000000) | ((u32)tag & 0xFFFFFF);
        }
    }
}

/* Link a layer's table into the frame's ordering table with its area,
 * offset and background packets for the current buffer. */
void func_8008AE1C(OtPair *layer) {
    func_8008AC7C(layer);
    AddPrims(D_80092938, layer->last[D_800928A0], layer->ot[D_800928A0]);
    if (!(layer->flags & 4)) {
        SetDrawArea(layer->area[D_800928A0], &D_80092868->draw.clip);
    }
    if (!(layer->flags & 8)) {
        SetDrawOffset(layer->offset[D_800928A0], D_80092868->draw.ofs);
    }
    if (layer->flags & 0x10) {
        AddPrim(D_80092938, &layer->tile[D_800928A0]);
    }
    AddPrim(D_80092938, layer->offset[D_800928A0]);
    AddPrim(D_80092938, layer->area[D_800928A0]);
}

/* Relocate a scene file's pointers to where it was loaded. */
SceneFile *func_8008AF6C(SceneFile *scene) {
    s32 delta = (u8 *)scene - scene->base;
    u32 i;

    scene->base = (u8 *)scene;
    scene->unk0 += delta;
    scene->unk4 += delta;
    scene->unk10 += delta;
    scene->unk14 += delta;
    scene->unk18 += delta;
    scene->unk20 += delta;
    scene->unk24 += delta;
    if (scene->target != NULL) {
        scene->target += delta;
        func_8008A040(scene->target);
    }
    if (scene->table != NULL) {
        scene->table = (u32 *)((u8 *)scene->table + delta);
        for (i = 1; i < scene->table[0] + 1; i++) {
            if (scene->table[i] != 0) {
                scene->table[i] += delta;
            }
        }
    }
    return scene;
}

/* Load and relocate a scene file. */
SceneFile *func_8008B070(s32 file) {
    SceneFile *scene;

    func_800324B8(0xA);
    scene = func_80031BDC(func_80028738(file), 0);
    func_800295D8(file, scene, 0, 0);
    func_80028A60(0);
    return func_8008AF6C(scene);
}

/* Rewind every channel of a player. */
void func_8008B0D8(Player *player) {
    Channel *channel;
    s32 i;

    player->unk10 = 0;
    player->frame = 0;
    channel = player->channels;
    for (i = 0; i < player->header->channels; i++) {
        channel->hold = 0;
        channel->value = 0;
        channel->current = channel->start;
        channel->delta = 0;
        channel++;
    }
}

#ifdef NON_MATCHING
/* Bind an animation to a model set node: its constant keys and streamed
 * channels drive node angle (short way round) or 0x2C components.
 * Does not match: the data pointer's register copies (t1/s0/s1) differ. */
void func_8008B13C(u8 *data, Player *player, Node *root) {
    Node **nodes = ((ModelSet *)root->data)->nodes;
    AnimHeader *anim = (AnimHeader *)data;
    AnimRecord *record;
    Key *key;
    Channel *channel;
    Node *node;
    u32 i;

    player->header = anim;
    func_800324B8(0x10);
    key = func_80031BDC(anim->keys * sizeof(Key) + anim->channels * sizeof(Channel), 0);
    player->keys = key;
    record = anim->records;
    channel = player->channels = (Channel *)(key + anim->keys);
    for (i = 0; i < anim->keys; i++) {
        node = nodes[record->node];
        key->value = record->value;
        switch (record->kind & 0x7F) {
        case 3:
            key->target = &node->unk44.vx;
            key->angular = 1;
            break;
        case 4:
            key->target = &node->unk44.vy;
            key->angular = 1;
            break;
        case 5:
            key->target = &node->unk44.vz;
            key->angular = 1;
            break;
        case 6:
            key->target = &node->rotation.vx;
            key->angular = 0;
            break;
        case 7:
            key->target = &node->rotation.vy;
            key->angular = 0;
            break;
        case 8:
            key->target = &node->rotation.vz;
            key->angular = 0;
            break;
        }
        key++;
        record++;
    }
    for (i = 0; i < anim->channels; i++) {
        node = nodes[record->node];
        channel->current = (u8 *)(record->value + (u32)data);
        channel->start = (u8 *)(record->value + (u32)data);
        switch (record->kind & 0x7F) {
        case 3:
            channel->target = &node->unk44.vx;
            channel->angular = 1;
            break;
        case 4:
            channel->target = &node->unk44.vy;
            channel->angular = 1;
            break;
        case 5:
            channel->target = &node->unk44.vz;
            channel->angular = 1;
            break;
        case 6:
            channel->target = &node->rotation.vx;
            channel->angular = 0;
            break;
        case 7:
            channel->target = &node->rotation.vy;
            channel->angular = 0;
            break;
        case 8:
            channel->target = &node->rotation.vz;
            channel->angular = 0;
            break;
        }
        channel++;
        record++;
    }
    func_8008B0D8(player);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu7", func_8008B13C);
#endif

#ifdef NON_MATCHING
/* Build a model set node tree from a model set file: a node per hierarchy
 * record (with its model, parent, angle and offset) and a player per
 * animation. Returns the root node. Does not match: register allocation
 * spills the models pointer where the original spills the animations. */
Node *func_8008B38C(ModelSetFile *file) {
    u32 i;
    u8 *models = file->models;
    u32 *hierarchy = file->hierarchy;
    u32 *animations = file->animations;
    u32 count = hierarchy[0];
    HierarchyRecord *records = (HierarchyRecord *)(hierarchy + 1);
    Node **nodes;
    ModelSet *set;
    Node *root;
    Node *node;
    Model *model;
    Player *player;
    Node *parent;

    func_8002C3E8(models);
    func_800324B8(0x12);
    nodes = func_80031BDC(count * 4, 0);
    set = func_80089E74();
    root = func_80089C54();
    func_80089E54(root, set);
    set->nodes = nodes;
    set->nodeCount = count;
    set->records = records;
    for (i = 0; i < count; i++) {
        node = func_80089C54();
        nodes[i] = node;
        if (records[i].model != -1) {
            model = func_80089FC4();
            func_80089E2C(node, model);
            func_8008A184(model, (ModelFile *)(models + (records[i].model * 0x38 + 0x10)));
        }
        if (records[i].parent == -1) {
            parent = root;
        } else {
            parent = nodes[records[i].parent];
        }
        func_80089C88(parent, node);
        node->unk44.vx = records[i].angle.vx;
        node->unk44.vy = records[i].angle.vy;
        node->unk44.vz = records[i].angle.vz;
        node->rotation.vx = records[i].offset[0];
        node->rotation.vy = records[i].offset[1];
        node->rotation.vz = records[i].offset[2];
    }
    if (animations != NULL) {
        func_800324B8(0x11);
        player = set->players = func_80031BDC(animations[0] * sizeof(Player), 0);
        set->count = animations[0];
        for (i = 0; i < animations[0]; i++) {
            if (animations[i + 1] != 0) {
                func_8008B13C((u8 *)animations[i + 1], player, root);
            } else {
                player->header = NULL;
            }
            player++;
        }
    }
    return root;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu7", func_8008B38C);
#endif

/* Advance a player by some frames, snapping to the keys. */
s32 func_8008B5DC(Player *player, s32 frames) {
    return func_8008B730(player, frames, 1);
}

/* Ease an angle toward target by a fraction (1/steps) of the shorter way
 * round (angles are 12-bit). */
s16 func_8008B5FC(s32 angle, s32 target, s32 steps) {
    s32 diff;
    s16 result;

    angle &= 0xFFF;
    diff = (angle - target) & 0xFFF;
    result = angle;
    if (diff != 0) {
        if (diff < 0x800) {
            result = angle - diff / steps;
        } else {
            result = angle + (0x1000 - diff) / steps;
        }
    }
    return result;
}

/* Turn an angle toward target by a fixed step the shorter way round
 * (a random way when opposite), stopping on the target. */
s32 func_8008B650(s32 from, s32 to, s32 step) {
    s16 angle = from;
    s16 target = to;
    s32 diff = (from - to) & 0xFFF;

    if (diff != 0) {
        if (diff == 0x800 ? (rand() & 1) : diff < 0x800) {
            angle -= step;
            if (((angle - target) & 0xFFF) > 0x800) {
                angle = target;
            }
        } else {
            angle += step;
            if (((angle - target) & 0xFFF) < 0x800) {
                angle = target;
            }
        }
    }
    return angle;
}

/* Advance a player by some frames (clamped to the animation's end) and
 * move every target 1/steps of the way to its key or channel value.
 * Channel streams hold a byte per frame: 0xxxxxxx a 7-bit delta, 10xxxxxx
 * hold the previous delta for x following frames, 11xxxxxx plus a byte
 * a signed 14-bit delta.
 * Returns whether the end was reached in a final step (1 without an
 * animation). */
s32 func_8008B730(Player *player, s32 frames, s32 steps) {
    Key *key;
    Channel *channel;
    u8 *code;
    s8 command;
    s16 value; /* decoded payload/delta or the current target value */
    s32 i;
    s32 j;

    if (player->header == NULL) {
        return 1;
    }
    if (frames == 0) {
        return;
    }
    if (player->frame + frames > player->header->frames) {
        frames = player->header->frames - player->frame;
    }
    player->frame += frames;
    steps -= frames;
    if (steps <= 0) {
        steps = 1;
    }
    key = player->keys;
    if (steps == 1) {
        for (i = 0; i < player->header->keys; i++, key++) {
            *key->target = key->value;
        }
    } else {
        for (i = 0; i < player->header->keys; i++, key++) {
            value = *key->target;
            if (key->angular) {
                *key->target = func_8008B5FC(value, key->value, steps);
            } else {
                *key->target = value + (key->value - value) / steps;
            }
        }
    }
    channel = player->channels;
    for (i = 0; i < player->header->channels; i++, channel++) {
        for (j = 0; j < frames; j++) {
            if (channel->hold) {
                channel->hold--;
            } else {
                code = channel->current++;
                command = *(s8 *)code;
                if (command & 0x80) {
                    value = command & 0x3F;
                    if (command & 0x40) {
                        channel->current = code + 2;
                        value |= (s8)code[1] << 6;
                        channel->delta = value;
                    } else {
                        channel->hold = value;
                    }
                } else {
                    channel->delta = ((u8)command << 25) >> 25; /* 7-bit signed delta */
                }
            }
            channel->value += channel->delta;
        }
        if (steps == 1) {
            *channel->target = channel->value;
        } else {
            value = *channel->target;
            if (channel->angular) {
                *channel->target = func_8008B5FC(value, channel->value, steps);
            } else {
                *channel->target = value + (channel->value - value) / steps;
            }
        }
    }
    if (player->frame == player->header->frames) {
        if (steps == 1) {
            return 1;
        }
    }
    return 0;
}

/* Create a task running entry(arg) on its own stack of `words` words and
 * run it until it first yields. */
Task *func_8008BA2C(void (*entry)(s32), s32 arg, u32 *stack, s32 words) {
    Task *task;
    s32 i;

    func_800324B8(3);
    task = func_80031BDC(sizeof(Task), 2);
    for (i = 0; i < 32; i++) {
        task->regs[i] = 0;
    }
    task->stack = stack;
    task->regs[28] = func_800405E4();
    task->regs[31] = (u32)entry;
    task->regs[4] = arg;
    task->regs[30] = task->regs[29] = (u32)(task->stack + words);
    func_8008BB3C(task);
    return task;
}

/* Free a task. */
void func_8008BAE0(Task *task) {
    func_800320E8(task);
}

INCLUDE_ASM("decomp/src/menu", func_8008BB00);

INCLUDE_ASM("decomp/src/menu", func_8008BB1C);

INCLUDE_ASM("decomp/src/menu", func_8008BB3C);

INCLUDE_ASM("decomp/src/menu", func_8008BC04);

/* Set the mesh light direction (a fixed down-left vector) and project
 * its vertices onto the ground plane for the shadow packets. */
void func_8008BCC8(Mesh *mesh, u8 *work) {
    Vector direction;
    Vector unused; /* unused in the original; reserves 16 bytes */

    direction.vx = -8;
    direction.vy = -8;
    direction.vz = -0x10;
    VectorNormal(&direction, &D_8009A2C8);
    D_8009A2C8.vx <<= 4;
    D_8009A2C8.vy <<= 4;
    D_8009A2C8.vz <<= 4;
    func_8008C3A8(mesh->data, work, mesh->count);
}

/* Draw a mesh's primitive groups (flag 8: quads, else triangles) into the
 * given packets and ordering table using the vertex work area. */
void func_8008BD70(Mesh *mesh, ModelPrim *prims, u32 *ot, u8 *work) {
    u8 *group;
    s32 groups = mesh->groups;

    D_80059528 = mesh->groupData;
    D_80059424 = (s32)prims;
    D_80059568 = (s32)ot;
    D_8005953C = (s32)work;
    D_800595C0 += mesh->prims;
    while (--groups != -1) {
        group = D_80059528;
        D_80059528 = group + 4;
        if (group[0] & 8) {
            func_8008C620(D_80059528, ((s16 *)group)[1]);
        } else {
            func_8008C4B0(D_80059528, ((s16 *)group)[1]);
        }
        D_80059528 += ((s16 *)group)[1] * 8;
    }
}

/* Build a mesh's packet buffers: a vertex work area and, per display
 * buffer, a flat grey quad (0x18 bytes) or triangle (0x14 bytes) packet
 * for every primitive. */
void func_8008BE4C(ModelPrims *mp, Mesh *mesh) {
    s32 n; /* vertex, then group counter, then packet bytes per buffer */
    s32 i;
    s32 j;
    s32 triangles;
    s32 quads;
    u8 *group;
    u8 *vertex;
    u8 *packet;

    mp->vertices = mesh->count;
    mp->count = mesh->prims;
    mp->vertexData = mesh->data;
    mp->mesh = mesh;
    D_80059528 = mesh->groupData;
    func_800324B8(0x13);
    vertex = mp->work = func_80031BDC(mp->vertices * 8, 2);
    n = mp->vertices;
    while (--n != -1) {
        ((s16 *)vertex)[1] = 0;
        vertex += 8;
    }
    func_800324B8(5);
    triangles = 0;
    quads = 0;
    n = mesh->groups;
    while (--n != -1) {
        group = D_80059528;
        D_80059528 = group + 4;
        if (group[0] & 8) {
            quads += ((s16 *)group)[1];
        } else {
            triangles += ((s16 *)group)[1];
        }
        D_80059528 += ((s16 *)group)[1] * 8;
    }
    n = triangles * 0x14 + quads * 0x18;
    packet = func_80031BDC(n * 2, 2);
    mp->prims[0] = (ModelPrim *)packet;
    mp->prims[1] = (ModelPrim *)(packet + n);
    i = mesh->groups;
    D_80059528 = mesh->groupData;
    while (--i != -1) {
        group = D_80059528;
        D_80059528 = group + 4;
        if (group[0] & 8) {
            j = ((s16 *)group)[1];
            while (--j != -1) {
                TAG_LEN(packet) = 5;
                ((u32 *)packet)[1] = 0x28403030;
                packet += 0x18;
            }
        } else {
            j = ((s16 *)group)[1];
            while (--j != -1) {
                TAG_LEN(packet) = 4;
                ((u32 *)packet)[1] = 0x20403030;
                packet += 0x14;
            }
        }
        D_80059528 += ((s16 *)group)[1] * 8;
    }
    func_800732AC(mp->prims[1], mp->prims[0], n);
}

/* Make a node an instance node. */
void func_8008C0BC(Node *node, Instance *instance) {
    node->type = 5;
    node->data = instance;
}

/* Allocate an instance payload drawing source. */
Instance *func_8008C0CC(Node *source) {
    Instance *instance;

    func_800324B8(7);
    instance = func_80031BDC(sizeof(Instance), 2);
    instance->type = source->type;
    instance->unk8 = (s32)D_80092828;
    instance->source = source;
    instance->prims = NULL;
    return instance;
}

/* Free an instance payload's packet buffers. */
void func_8008C120(Instance *instance) {
    ModelPrims *prims = instance->prims;

    if (prims != NULL) {
        if (prims->work != NULL) {
            func_800320E8(prims->work);
        }
        if (prims->prims[0] != NULL) {
            func_80032C18(prims->prims[0], 2);
        }
        func_800320E8(prims);
    }
}

/* Copy a node tree as instance nodes (model sources get their own packet
 * buffers); following siblings are appended to parent. */
Node *func_8008C188(Node *source, Node *parent) {
    Node *node;
    Instance *instance;
    Mesh *mesh;

    func_800324B8(6);
    node = func_80089C54();
    instance = func_8008C0CC(source);
    func_8008C0BC(node, instance);
    if (instance->type == 1) {
        mesh = (Mesh *)((Model *)source->data)->file;
        func_800324B8(5);
        instance->prims = func_80031BDC(sizeof(ModelPrims), 2);
        func_8008BE4C(instance->prims, mesh);
    }
    D_80092824++;
    if (source->child != NULL) {
        func_80089C88(node, func_8008C188(source->child, node));
    }
    if (source->next != NULL) {
        func_80089C88(parent, func_8008C188(source->next, parent));
    }
    return node;
}

/* Copy a node tree as instances. */
Node *func_8008C298(Node *source) {
    D_80092824 = 0;
    return func_8008C188(source, NULL);
}

/* Copy a node tree as instances of itself. */
Node *func_8008C2C0(Node *source) {
    D_80092828 = source;
    return func_8008C298(source);
}

/* Draw a tree of instance nodes whose model sources are shown. */
void func_8008C2E8(Node *node) {
    Instance *instance = node->data;
    ModelPrims *prims;

    if (instance->type == 1 && !(((Model *)instance->source->data)->flags & 1)) {
        prims = instance->prims;
        func_8008BD70(prims->mesh, prims->prims[D_800928A0], D_800928E4 + 1, prims->work);
    }
    if (node->child != NULL) {
        func_8008C2E8(node->child);
    }
    if (node->next != NULL) {
        func_8008C2E8(node->next);
    }
}

INCLUDE_ASM("decomp/src/menu", func_8008C3A8);

INCLUDE_ASM("decomp/src/menu", func_8008C4B0);

INCLUDE_ASM("decomp/src/menu", func_8008C620);

/* Shift a vector history: entries 4, 3 and 2 all take entry 0. */
void func_8008C7C0(SVector *history) {
    history[4] = history[0];
    history[3] = history[4];
    history[2] = history[3];
}

/* Set up a four-point spark line: semi-transparent, in the source colour,
 * the same in both draw buffers. */
void func_8008C828(SparkLine4 *spark, Emitter *source) {
    LineF4 *line = &spark->line[0];

    setlen(line, 6), setcode(line, 0x4C), line->pad = 0x55555555;
    setSemiTrans(line, 1);
    setRGB0(line, source->r, source->g, source->b);
    spark->line[1] = spark->line[0];
}

/* Project a four-point spark line through its trail, age the trail and add
 * the line to the ordering table. */
void func_8008C8B4(SparkLine4 *spark, u32 *ot) {
    LineF4 *line = &spark->line[D_800928A0];
    s32 depth;
    s32 otz;

    otz = RotTransPers4(&spark->pos, &spark->trail[0], &spark->trail[1], &spark->trail[2],
                        (s32 *)&line->x0, (s32 *)&line->x1, (s32 *)&line->x2, (s32 *)&line->x3,
                        &depth, &depth);
    spark->trail[2] = spark->trail[1];
    spark->trail[1] = spark->trail[0];
    spark->trail[0] = spark->pos;
    func_80031750(ot + (otz >> 2), line);
}

/* Collapse a three-point spark line's trail onto its position. */
void func_8008C9B8(SparkLine3 *spark) {
    spark->trail[1] = spark->pos;
    spark->trail[0] = spark->trail[1];
}

/* Set up a three-point spark line. */
void func_8008CA00(SparkLine3 *spark, Emitter *source) {
    LineF3 *line = &spark->line[0];

    setlen(line, 5), setcode(line, 0x48), line->pad = 0x55555555;
    setSemiTrans(line, 1);
    setRGB0(line, source->r, source->g, source->b);
    spark->line[1] = spark->line[0];
}

/* Project a three-point spark line relative to the view origin with the
 * GTE, age its trail and add it. */
void func_8008CA84(SparkLine3 *spark, u32 *ot) {
    SVector *origin = D_80092830;
    SVector *work = D_8009282C;
    LineF3 *line;
    s32 otz;

    work[0].vx = spark->pos.vx - origin->vx;
    work[0].vy = spark->pos.vy - origin->vy;
    work[0].vz = spark->pos.vz - origin->vz;
    work[1].vx = spark->trail[0].vx - origin->vx;
    work[1].vy = spark->trail[0].vy - origin->vy;
    work[1].vz = spark->trail[0].vz - origin->vz;
    work[2].vx = spark->trail[1].vx - origin->vx;
    work[2].vy = spark->trail[1].vy - origin->vy;
    work[2].vz = spark->trail[1].vz - origin->vz;
    gte_ldv3c(D_8009282C);
    gte_rtpt();
    line = &spark->line[D_800928A0];
    spark->trail[1] = spark->trail[0];
    spark->trail[0] = spark->pos;
    gte_stsxy3(&line->x0, &line->x1, &line->x2);
    gte_stszotz(&otz);
    func_80031708(ot + (otz >> 2), line);
}

/* Collapse a two-point spark line's trail onto its position. */
void func_8008CC2C(SparkLine2 *spark) {
    spark->trail[0] = spark->pos;
}

/* Set up a two-point spark line. */
void func_8008CC54(SparkLine2 *spark, Emitter *source) {
    LineF2Tag *line = &spark->line[0];

    setlen(line, 3), setcode(line, 0x40);
    setSemiTrans(line, 1);
    setRGB0(line, source->r, source->g, source->b);
    spark->line[1] = spark->line[0];
}

/* Project a two-point spark line, age its trail and add it. */
void func_8008CCB0(SparkLine2 *spark, u32 *ot) {
    LineF2Tag *line = &spark->line[D_800928A0];
    s32 depth;
    s32 otz;

    otz = RotTransPers3(&spark->pos, &spark->trail[0], &depth, (s32 *)&line->x0,
                        (s32 *)&line->x1, &depth, &depth, &depth);
    spark->trail[0] = spark->pos;
    func_800316C0(ot + (otz >> 2), line);
}

void func_8008CD54(void) {
}

/* Set up a spark drawn as a small semi-transparent tile of random size. */
void func_8008CD5C(SparkTile *spark, Emitter *source) {
    TileRgb *tile = &spark->tile[0];

    setlen(tile, 3), setcode(tile, 0x62);
    tile->h = rand() % 2 + 2;
    tile->w = tile->h * 2;
    setRGB0(tile, source->r, source->g, source->b);
    spark->tile[1] = spark->tile[0];
}

/* Project a tile spark relative to the view origin and add it. */
void func_8008CE0C(SparkTile *spark, u32 *ot) {
    SVector *origin = D_80092830;
    SVector v;
    TileRgb *tile;
    s32 otz;

    v.vx = spark->pos.vx - origin->vx;
    v.vy = spark->pos.vy - origin->vy;
    v.vz = spark->pos.vz - origin->vz;
    gte_ldv0(&v);
    gte_rtps();
    tile = &spark->tile[D_800928A0];
    gte_stsxy(&tile->x0);
    gte_stszotz(&otz);
    func_80031804(ot + (otz >> 2), tile);
}

void func_8008CED4(void) {
}

/* Set up a spark drawn as a single semi-transparent dot. */
void func_8008CEDC(SparkDot *spark, Emitter *source) {
    Tile1Tag *dot = &spark->dot[0];

    setlen(dot, 2), setcode(dot, 0x6A);
    setRGB0(dot, source->r, source->g, source->b);
    spark->dot[1] = spark->dot[0];
}

/* Project a dot spark and add it. */
void func_8008CF30(SparkDot *spark, u32 *ot) {
    Tile1Tag *dot = &spark->dot[D_800928A0];
    s32 depth;

    func_80031870(ot + (RotTransPers(&spark->pos, (s32 *)&dot->x0, &depth, &depth) >> 2), dot);
}

/* Place a spark at its source's origin. */
void func_8008CF9C(Emitter *source, SVector *pos) {
    *pos = source->origin;
}

/* Place a spark at a random point of its source's box, rotated with the
 * source. */
void func_8008CFC4(Emitter *source, SVector *pos) {
    SVector v;
    Vector r;

    v.vx = rand() % source->range.vx - source->offset.vx;
    v.vy = rand() % source->range.vy - source->offset.vy;
    v.vz = rand() % source->range.vz - source->offset.vz;
    func_800495DC(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* Place a spark at a random point of its source's box. */
void func_8008D0A4(Emitter *source, SVector *pos) {
    pos->vx = source->origin.vx + rand() % source->range.vx - source->offset.vx;
    pos->vy = source->origin.vy + rand() % source->range.vy - source->offset.vy;
    pos->vz = source->origin.vz + rand() % source->range.vz - source->offset.vz;
}

/* Place a spark at a random point of its source's horizontal rectangle,
 * rotated with the source. */
void func_8008D14C(Emitter *source, SVector *pos) {
    SVector v;
    Vector r;

    v.vx = rand() % source->range.vx - source->offset.vx;
    v.vy = 0;
    v.vz = rand() % source->range.vz - source->offset.vz;
    func_800495DC(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* Place a spark at a random point of its source's horizontal ellipse,
 * rotated with the source. */
void func_8008D208(Emitter *source, SVector *pos) {
    SVector v;
    Vector r;
    s32 angle = rand();
    s32 radius = rand();

    v.vx = (func_8003F8B0(angle) * (radius % source->range.vx)) >> 13;
    v.vy = 0;
    v.vz = (func_8003F8CC(angle) * (radius % source->range.vz)) >> 13;
    func_800495DC(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* Place a spark on its source's ring at a random height, rotated with the
 * source. The ring angle is never initialised in the original. */
void func_8008D304(Emitter *source, SVector *pos) {
    SVector v;
    Vector r;
    s32 angle;

    v.vx = (func_8003F8B0(angle) * source->range.vx) >> 12;
    v.vy = rand() % source->range.vy - source->range.vy / 2;
    v.vz = (func_8003F8CC(angle) * source->range.vz) >> 12;
    func_800495DC(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* Create an emitter of the given spark shape and placement rule: unit
 * spread centred on the origin, white, no sparks yet. */
Emitter *func_8008D3F4(s32 shape, s32 placement) {
    Emitter *emitter;
    SparkShape *kind;

    func_800324B8(0x15);
    emitter = func_80031BDC(0x7C, 0);
    emitter->range.vx = 0x1000;
    emitter->range.vy = 0x1000;
    emitter->range.vz = 0x1000;
    emitter->spread = 1;
    emitter->placement = placement;
    emitter->unk2 = 0;
    emitter->unk4 = 0;
    emitter->unk6 = 0;
    emitter->base.vx = 0;
    emitter->base.vy = 0;
    emitter->base.vz = 0;
    emitter->angles.vx = 0;
    emitter->angles.vy = 0;
    emitter->angles.vz = 0;
    emitter->turn.vx = 0;
    emitter->turn.vy = 0;
    emitter->turn.vz = 0;
    emitter->gravity = 0;
    emitter->unk46 = 0;
    emitter->speed = 0x100;
    emitter->speed_range = 0x100;
    emitter->offset.vx = emitter->range.vx / 2;
    emitter->offset.vy = emitter->range.vy / 2;
    emitter->offset.vz = emitter->range.vz / 2;
    emitter->place = D_80091CC4[(s16)placement];
    emitter->update = D_80091CDC[0];
    emitter->sparks = NULL;
    emitter->shape = shape;
    emitter->unk64 = 0;
    emitter->life = 100;
    emitter->r = 0xFF;
    emitter->g = 0xFF;
    emitter->b = 0xFF;
    emitter->unk68 = 0;
    kind = &D_80091C74[emitter->shape];
    emitter->size = kind->size;
    emitter->reset = kind->reset;
    emitter->draw = kind->draw;
    emitter->setup = kind->setup;
    return emitter;
}

/* Mark every spark of an emitter for restart. */
void func_8008D580(Emitter *emitter) {
    u8 *spark = emitter->sparks;
    s32 i;

    for (i = 0; i < emitter->count; i++) {
        ((SVector *)spark)->pad = 0;
        spark += emitter->size;
    }
}

/* (Re)allocate an emitter's pool for count sparks and set each one up. */
void func_8008D5C0(Emitter *emitter, s32 count) {
    u8 *spark;
    void (*setup)(void *, Emitter *);
    s32 i;

    if (emitter->sparks != NULL) {
        func_80032C18(emitter->sparks, 3);
    }
    emitter->count = count;
    func_800324B8(0x14);
    emitter->sparks = func_80031BDC(emitter->size * emitter->count, 0);
    spark = emitter->sparks;
    setup = emitter->setup;
    for (i = 0; i < emitter->count; i++) {
        setup(spark, emitter);
        ((SVector *)spark)->pad = 0;
        spark += emitter->size;
    }
}

/* Launch up to count idle sparks: each gets a random direction inside the
 * emitter's spread cone and a random speed, both rotated into place, then a
 * position from the placement rule, the emitter's life and a fresh shape. */
void func_8008D680(Emitter *emitter, Matrix *rotation, s32 count) {
    SVector dir;
    SVector unit;
    Matrix local;
    Matrix world;
    Matrix turned;
    u8 *spark;
    void (*place)(Emitter *, SVector *);
    void (*reset)(void *);
    s32 left;
    s32 i;
    s32 angle;
    s32 heading;

    reset = emitter->reset;
    place = emitter->place;
    world = *rotation;
    emitter->origin.vx = emitter->base.vx + rotation->t[0];
    emitter->origin.vy = emitter->base.vy + rotation->t[1];
    emitter->origin.vz = emitter->base.vz + rotation->t[2];
    func_8003F738(&emitter->angles, &local);
    func_80049ACC(&world, &local);
    func_8003F738(&emitter->turn, &turned);
    func_80049ACC(&turned, &world);
    SetRotMatrix(&turned);
    left = count;
    spark = emitter->sparks;
    for (i = 0; i < emitter->count; i++) {
        if (((Spark *)spark)->pos.pad == 0) {
            if (--left == -1) {
                break;
            }
            angle = rand() % emitter->spread;
            heading = rand();
            dir.vy = -func_8003F8CC(angle);
            angle = func_8003F8B0(angle);
            dir.vx = (func_8003F8B0(heading) * angle) >> 12;
            dir.vz = (func_8003F8CC(heading) * angle) >> 12;
            heading = emitter->speed + rand() % emitter->speed_range; /* now the speed */
            gte_ldv0(&dir);
            gte_rtv0();
            gte_stsv(&unit);
            gte_lddp(heading);
            gte_ldsv(&unit);
            gte_gpf12();
            gte_stsv(&((Spark *)spark)->vel);
        }
        spark += emitter->size;
    }
    SetRotMatrix(&world);
    left = count;
    spark = emitter->sparks;
    for (i = 0; i < emitter->count; i++) {
        if (((Spark *)spark)->pos.pad == 0) {
            if (--left == -1) {
                break;
            }
            place(emitter, (SVector *)spark);
            ((Spark *)spark)->pos.pad = emitter->life;
            reset(spark);
        }
        spark += emitter->size;
    }
}

/* Bounce a falling spark off the floor under it, losing half its speed.
 * The floor query reads the spark position as a 32-bit vector. */
void func_8008D980(Spark *spark) {
    if (spark->vel.vy > 0 && spark->pos.vy > func_80082488((Vector *)spark, 0)) {
        spark->vel.vy = -spark->vel.vy / 2;
    }
}

/* Bounce a spark off the ground plane (y = 0), losing half its speed;
 * a spark that has come to rest dies. */
void func_8008D9F0(Spark *spark) {
    if (spark->pos.vy > 0) {
        spark->vel.vy = -spark->vel.vy / 2;
        if (abs(spark->vel.vy) < 8) {
            spark->pos.pad = 0;
        }
    }
}

/* Move and draw every live spark of an emitter: gravity, a bounce on the
 * ground plane, projection relative to the camera through the scratchpad. */
void func_8008DA48(Emitter *emitter, u32 *ot, Matrix *view) {
    SVector unused[5]; /* unused in the original; reserves 40 bytes */
    Spark *spark;
    void (*draw)(void *, u32 *);
    s32 i;

    D_8009282C = (SVector *)0x1F800000;
    D_80092830 = (SVector *)0x1F800030;
    ((SVector *)0x1F800030)->vx = D_80096FA8.vx;
    ((SVector *)0x1F800030)->vy = D_80096FA8.vy;
    ((SVector *)0x1F800030)->vz = D_80096FA8.vz;
    spark = (Spark *)emitter->sparks;
    draw = emitter->draw;
    for (i = 0; i < emitter->count; i++) {
        if (spark->pos.pad != 0) {
            spark->pos.pad--;
            spark->vel.vy += emitter->gravity;
            spark->pos.vx += spark->vel.vx;
            spark->pos.vy += spark->vel.vy;
            spark->pos.vz += spark->vel.vz;
            if (spark->pos.vy > 0) {
                spark->vel.vy = -spark->vel.vy * 2 / 3;
                if (abs(spark->vel.vy) < 4) {
                    spark->pos.pad = 0;
                }
            }
            draw(spark, ot);
        }
        spark = (Spark *)((u8 *)spark + emitter->size);
    }
}

/* Copy one model part's local transform. */
void func_8008DBC0(SparkModel *model, s16 part, Matrix *out) {
    Matrix unused; /* unused in the original; reserves 32 bytes */

    *out = model->list->parts[part]->matrix;
}

/* Create the menu's spark emitter: 256 orange three-point sparks. */
void func_8008DC28(void) {
    Emitter *emitter = func_8008D3F4(1, 0);

    emitter->r = 0xFF;
    emitter->g = 0xA0;
    emitter->b = 0x70;
    func_8008D5C0(emitter, 0x100);
    emitter->gravity = 4;
    emitter->spread = 0x60;
    emitter->speed_range = 0x60;
    emitter->speed = 4;
    emitter->unk68 = 0;
    emitter->life = 0x20;
    D_80092834 = emitter;
}

/* Start a spark burst of the given strength. */
void func_8008DCA8(s32 strength) {
    D_80092838 = strength;
}

/* Emit a burst from a model part while the burst lasts, then move and draw
 * the menu's sparks under the given view. */
void func_8008DCB8(u32 *ot, SparkModel *model, Matrix *view, Vector *pos) {
    Emitter *emitter = D_80092834;
    Matrix rotation;
    Matrix part;

    if (D_80092838 >= 0x10) {
        func_8008DBC0(model, 0x27, &part);
        func_80048E94(&part, &rotation);
        rotation.t[0] = rotation.t[1] = rotation.t[2] = 0;
        emitter->base.vx = pos->vx;
        emitter->base.vy = pos->vy;
        emitter->base.vz = pos->vz;
        emitter->turn.vx = 0;
        emitter->turn.vy = 0;
        emitter->turn.vz = 0;
        emitter->angles.vx = 0;
        emitter->angles.vy = 0;
        emitter->angles.vz = 0x800;
        func_8008D680(emitter, &rotation, D_80092838 >> 4);
        D_80092838 -= 4;
    }
    gte_SetTransMatrix(view);
    gte_SetRotMatrix(view);
    func_8008DA48(emitter, ot, view);
}

INCLUDE_ASM("decomp/src/menu", func_8008DDFC);

INCLUDE_ASM("decomp/src/menu", func_8008DE54);

/* Forget the three glow buffers. */
void func_8008DF30(void) {
    D_80092844 = NULL;
    D_8009283C = NULL;
    D_80092840 = NULL;
}

/* Allocate the glow buffers once, clear them, and upload the glow palette
 * with every entry marked semi-transparent. */
void func_8008DF50(void) {
    Rect rect;
    s32 i;

    if (D_80092844 == NULL) {
        D_80092844 = func_80031BDC(0x1500, 1);
        D_8009283C = func_80031BDC(0x2BC0, 1);
        D_80092840 = func_80031BDC(0x2BC0, 1);
    }
    for (i = 0x1570; i != -1; i--) {
        D_80092840[i] = 0;
        D_8009283C[i] = 0;
    }
    for (i = 0; i < 0x1500; i++) {
        D_80092844[i] = 0;
    }
    for (i = 0; i < 0x100; i++) {
        D_80091CE0[i] |= 0x8000;
    }
    rect.y = 0x1FD;
    rect.w = 0xFF;
    rect.x = 0;
    rect.h = 1;
    LoadImage(&rect, (u32 *)D_80091CE0);
}

/* Release the glow buffers. */
void func_8008E064(void) {
    if (D_80092844 != NULL) {
        func_80032C18(D_80092844, 2);
        func_80032C18(D_8009283C, 2);
        func_80032C18(D_80092840, 2);
        D_80092844 = NULL;
        D_8009283C = NULL;
        D_80092840 = NULL;
    }
}

/* Copy the new glow field over the old one and pack every word's low bytes
 * of both halves into the byte field. */
void func_8008E0C8(void) {
    u8 *bytes = D_80092844;
    u32 *old = (u32 *)D_8009283C;
    u32 *new = (u32 *)D_80092840;
    u32 value;
    s32 i;

    for (i = 0xA7F; i != -1; i--) {
        value = *new++;
        *old++ = value;
        *bytes++ = value;
        *bytes++ = value >> 16;
    }
}

#ifdef NON_MATCHING
/* Advance the glow field one step: seed the two bottom rows with random
 * heat, let every cell take the cooled average of its neighbours below,
 * then keep the result for the next step.
 * Does not match (408 vs 400 bytes): the original also keeps the source
 * index i in $s1 (so the quotient lands in $s5, frame 0x30 vs 0x28), the
 * first loop swaps $s0/$s1, and the loop setup is ordered differently. */
void func_8008E120(void) {
    s16 *new;
    s16 *old;
    s16 *seed;
    s32 heat;
    s32 value;
    s32 i;
    s32 x;
    s32 y;
    s16 *up;
    s16 *right;
    s16 *left;
    s16 *down_right;
    s16 *down_left;
    s16 *dst;

    if (D_80092844 != NULL) {
        heat = 0;
        new = D_80092840;
        seed = &new[47 * 0x70];
        for (x = 0; x < 0x70; x++) {
            switch (rand() & 3) {
            case 0:
                heat = 0x180;
                break;
            case 1:
                heat = 0;
                break;
            }
            seed[x] = seed[x + 0x70] = heat;
        }
        old = D_8009283C;
        up = old - 0x70;
        right = old + 1;
        left = old - 1;
        down_right = old + 0x71;
        down_left = old + 0x6F;
        for (y = 0x2F; y > 1; y--) {
            i = y * 0x70 + 1;
            dst = &new[(y - 1) * 0x70];
            for (x = 1; x < 0x70; x++) {
                value = (up[i] + right[i] + left[i] + down_right[i] + down_left[i]) / 5;
                i++;
                if (value > 3) {
                    value -= 3;
                }
                dst[x] = value;
            }
        }
        func_8008E0C8();
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu7", func_8008E120);
#endif

/* Draw a full-screen grey tile of the given level, additive or subtractive,
 * with the draw mode that selects the blend. */
void func_8008E2B8(u32 *ot, s32 level, s32 subtract) {
    TileRgb *tile = &D_80096DE0[D_800928A0];

    *(u32 *)&tile->r0 = level | (level << 8) | (level << 16) | 0x60000000;
    setlen(tile, 3);
    *(u32 *)&tile->x0 = 0;
    *(u32 *)&tile->w = 0xDA0140;
    setSemiTrans(tile, 1);
    AddPrim(ot, tile);
    if (subtract) {
        SetDrawMode(&D_80096E00[D_800928A0], 0, 1, GetTPage(0, 2, 0, 0), NULL);
    } else {
        SetDrawMode(&D_80096E00[D_800928A0], 0, 1, GetTPage(0, 1, 0, 0), NULL);
    }
    AddPrim(ot, &D_80096E00[D_800928A0]);
}

/* Draw the glow field: upload its byte image and stretch it over the
 * screen as a semi-transparent textured quad at two thirds of the level
 * (plain texture at full level); optionally add a brightening tile. */
void func_8008E3CC(u32 *ot, s32 level, s32 brighten) {
    PolyFT4 *quad = &D_80096D90[D_800928A0];
    TileRgb *tile;
    Rect rect;
    s32 shade;

    setlen(quad, 9);
    shade = level * 2 / 3;
    *(u32 *)&quad->r0 = shade | (shade << 8) | (shade << 16) | 0x2C000000;
    setShadeTex(quad, shade == 0x80);
    *(u32 *)&quad->x0 = 0;
    *(u32 *)&quad->x1 = 0x140;
    *(u32 *)&quad->x2 = 0xDA0000;
    *(u32 *)&quad->x3 = 0xDA0140;
    *(u16 *)&quad->u0 = 0;
    *(u16 *)&quad->u1 = 0x6F;
    *(u16 *)&quad->u2 = 0x2A00;
    *(u16 *)&quad->u3 = 0x2A6F;
    setSemiTrans(quad, 1);
    quad->tpage = GetTPage(1, 1, 0x140, 0x100);
    quad->clut = GetClut(0, 0x1FD);
    AddPrim(ot, quad);
    rect.x = 0x140;
    rect.y = 0x100;
    rect.w = 0x38;
    rect.h = 0x2B;
    LoadImage(&rect, (u32 *)D_80092844);
    if (brighten) {
        tile = &D_80096DE0[D_800928A0];
        if (level > 0x80) {
            shade = level * 2;
            *(u32 *)&tile->r0 = shade | (shade << 8) | (shade << 16) | 0x60000000;
            setlen(tile, 3);
            *(u32 *)&tile->x0 = 0;
            *(u32 *)&tile->w = 0xDA0140;
            setSemiTrans(tile, 1);
            AddPrim(ot, tile);
        }
    }
    SetDrawMode(&D_80096E00[D_800928A0], 0, 1, GetTPage(0, 2, 0, 0), NULL);
    AddPrim(ot, &D_80096E00[D_800928A0]);
}

/* Reset the sound driver and the four positional voices. */
void func_8008E620(void) {
    s32 mask = 0x300;
    SoundVoice *voice;
    u32 i;

    func_80039FF8();
    for (i = 0; i < 4; i++) {
        voice = &D_80096EA0[i];
        voice->mask = mask;
        mask <<= 2;
        voice->active = 0;
        voice->age = 0;
        voice->voice = i * 2;
    }
}

/* Age every positional voice (saturating). */
void func_8008E67C(void) {
    if (D_80096EA0[0].age != 0xFFFF) {
        D_80096EA0[0].age++;
    }
    if (D_80096EA0[1].age != 0xFFFF) {
        D_80096EA0[1].age++;
    }
    if (D_80096EA0[2].age != 0xFFFF) {
        D_80096EA0[2].age++;
    }
    if (D_80096EA0[3].age != 0xFFFF) {
        D_80096EA0[3].age++;
    }
}

/* Choose a character's command sound table by its model kind. */
void func_8008E6F8(Actor *owner) {
    switch (owner->model_id) {
    case 9:
        owner->sounds = D_80091FA0;
        break;
    case 0x1D:
        owner->sounds = D_80091F60;
        break;
    case 0x1B:
        owner->sounds = D_80091F80;
        break;
    case 0x24:
        owner->sounds = D_80091F70;
        break;
    default:
        owner->sounds = D_80091F90;
        break;
    }
}

/* Start a sound on a free positional voice (or a matching unpositioned
 * one, else the oldest); positioned sounds follow pos or its snapshot. */
void func_8008E78C(s32 sound, s32 mode, Vector *pos, s32 arg3) {
    s32 oldest = 0;
    SoundVoice *chosen = &D_80096EA0[3];
    SoundVoice *voice;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &D_80096EA0[i];
        if (voice->active == 0) {
            chosen = voice;
            break;
        }
        if (mode == 0 && voice->mode == 0) {
            chosen = voice;
            break;
        }
        if (oldest < voice->age) {
            oldest = voice->age;
            chosen = voice;
        }
    }
    func_8008E67C();
    voice = chosen;
    voice->mode = mode;
    voice->sound = sound;
    voice->active = 1;
    voice->unk3 = arg3;
    voice->follow = pos;
    if (pos != NULL) {
        voice->pos = *pos;
    }
    voice->age = 0;
    if (mode == 0) {
        func_80039F9C(voice->sound, voice->voice, 0x7F, 0x40);
    }
}

/* Pan and attenuate every positioned voice from its screen position and
 * depth; a voice just started is keyed on with those values. */
void func_8008E8B0(void) {
    SVector v;
    SVector screen;
    s32 sz;
    SoundVoice *voice;
    s32 volume;
    s32 x;
    s32 pan;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &D_80096EA0[i];
        if (voice->active && voice->mode != 0) {
            if (voice->mode == 1) {
                v.vx = voice->pos.vx;
                v.vy = voice->pos.vy;
                v.vz = voice->pos.vz;
            } else {
                v.vx = voice->follow->vx;
                v.vy = voice->follow->vy;
                v.vz = voice->follow->vz;
            }
            v.vx -= D_80096FA8.vx;
            v.vy -= D_80096FA8.vy;
            v.vz -= D_80096FA8.vz;
            gte_ldv0(&v);
            gte_rtps();
            gte_stsxy(&screen);
            gte_stsz(&sz);
            x = screen.vx;
            if (x < 0) {
                x = 0;
            }
            if (x > 0x140) {
                x = 0x140;
            }
            volume = (0x3000 - sz) * 0x7F / 0x3000;
            if (volume < 0x28) {
                volume = 0x28;
            }
            if (volume > 0x7F) {
                volume = 0x7F;
            }
            pan = x * 0x7F / 0x140;
            if (voice->age != 0) {
                func_8003A55C(voice->voice, pan);
                func_8003A344(voice->voice, volume);
            } else {
                func_80039F9C(voice->sound, voice->voice, volume, pan);
            }
        }
    }
    func_8008E67C();
}

/* Free the voices whose sound has stopped, then age them all. */
void func_8008EADC(void) {
    SoundVoice *voice;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &D_80096EA0[i];
        if (!(func_8003A5D0(voice->sound) & voice->mask)) {
            voice->active = 0;
        }
    }
    func_8008E67C();
}

/* Play a menu sound effect (unpositioned). */
void func_8008EB4C(s32 id) {
    if (id != 0) {
        func_8008E78C(0x60000 + id, 0, NULL, D_80059488);
    }
}

/* Play a character's sound effect, tagged with its id and side. */
void func_8008EB88(Actor *owner, s32 id, Vector *pos, s32 mode) {
    if (id != 0) {
        func_8008E78C(id + 0x60000, mode, pos, (id & 0x7F) | ((owner->flags >> 20) & 0x80));
    }
}

#ifdef NON_MATCHING
/* Play one of a character's command sounds (random 1-6 when index is 0):
 * up to two effects from the shared pair table.
 * Nonmatching: saved-register allocation differs (original s0 table,
 * s1 entry offset, s2 owner, s3 mode, s4 pos; compiled s0 owner, s2 mode,
 * s3 pos, s4 table) and the prologue saves are ordered differently. */
void func_8008EBD0(Actor *owner, s32 index, Vector *pos, s32 mode) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    s32 entry;
    SoundPair *table;

    if (index == 0) {
        index = rand() % 6 + 1;
    }
    entry = owner->sounds[index];
    if (entry != 0xFF) {
        table = D_80091EE0;
        index = D_80091EE0[entry].first;
        if (index != 0) {
            func_8008E78C(index | 0x60000, mode, pos, (index & 0x7F) | ((owner->flags >> 20) & 0x80));
        }
        index = table[entry].second;
        if (index != 0) {
            func_8008E78C(index | 0x60000, mode, pos, (index & 0x7F) | ((owner->flags >> 20) & 0x80));
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu7", func_8008EBD0);
#endif

/* Stop every voice started with the given tag. */
void func_8008ECEC(u8 tag) {
    SoundVoice *voice;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &D_80096EA0[i];
        if (voice->active && voice->unk3 == tag) {
            func_8003A20C(voice->voice);
            voice->active = 0;
        }
    }
}

/* Stop the sounds a character's command sound entry started. Declared int
 * without a return value, as the original's unfilled last delay slot shows.
 * Does not match: the table base and the entry offset swap $s1/$s2. */
#ifdef NON_MATCHING
s32 func_8008ED6C(Actor *owner, s32 index) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    s32 entry;
    SoundPair *table;

    entry = owner->sounds[index];
    if (entry != 0xFF) {
        table = D_80091EE0;
        index = D_80091EE0[entry].first;
        if (index != 0) {
            func_8008ECEC((index & 0x7F) | ((owner->flags >> 20) & 0x80));
        }
        index = table[entry].second;
        if (index != 0) {
            func_8008ECEC((index & 0x7F) | ((owner->flags >> 20) & 0x80));
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu7", func_8008ED6C);
#endif

/* Accelerate an actor toward the speed limit (or brake to a stop, harder
 * when not guarding) for two ticks, and turn it toward a heading. */
void func_8008EE1C(Actor *actor, s16 heading, s16 limit) {
    s32 unused[4]; /* unused in the original; reserves 16 bytes */
    s32 brake = actor->brake;
    s32 accel = actor->accel;
    s32 moving;
    s32 i;

    if (actor->flags & 0x100) {
        brake = brake * 2 / 3;
        moving = 0;
    } else {
        moving = 1;
    }
    for (i = 0; i < 2; i++) {
        if (limit != 0 && moving) {
            actor->state += accel;
            if (limit < actor->state) {
                actor->state = limit;
            }
        } else {
            actor->state -= brake;
            if (actor->state < 0) {
                actor->state = 0;
            }
        }
    }
    actor->target_angle = func_8008B650(actor->target_angle, heading, 0x40);
    actor->unkCE = 0;
}

/* Opponent command: act, then wait a second. */
void func_8008EF00(Actor *actor, Brain *brain) {
    func_800767C8(actor);
    brain->timer = 0x3C;
}

/* Opponent command: act, then wait longer when told to. */
void func_8008EF30(Actor *actor, Brain *brain, s32 long_wait) {
    func_8008FE80(actor);
    brain->timer = long_wait ? 0x1E : 0xA;
}

/* Opponent command: input 3, then wait a second. */
void func_8008EF74(Actor *actor, Brain *brain) {
    func_8007639C(actor, 3);
    brain->timer = 0x3C;
}

/* Opponent command: toggle guarding. */
void func_8008EFA8(Actor *actor, Brain *brain) {
    brain->defending ^= 1;
    if (brain->defending) {
        actor->flags |= 2;
        brain->timer = 0x3C;
    } else {
        actor->flags &= ~2;
        actor->flags &= ~0x38;
        brain->timer = 0x1E;
    }
}

/* Opponent command: inputs 4 and 3, then wait a second. */
void func_8008F014(Actor *actor, Brain *brain) {
    func_8007639C(actor, 4);
    func_8007639C(actor, 3);
    brain->timer = 0x3C;
}

/* Opponent command: input 4, then wait a second. */
void func_8008F060(Actor *actor, Brain *brain) {
    func_8007639C(actor, 4);
    brain->timer = 0x3C;
}

/* Opponent roaming: while far away keep deciding every frame; otherwise
 * pick a new random heading and duration when the timer runs out. */
void func_8008F094(Actor *actor, Brain *brain) {
    if (D_8009284C > 0x800) {
        brain->timer = 1;
        brain->unkC = 0;
    } else if (--brain->timer == -1) {
        brain->unkA = rand() % 0x600 + 0x500;
        brain->timer = rand() % 50 + 10;
        brain->unkC = 0xFF;
    }
}

/* Opponent circling: while very close keep deciding every frame; otherwise
 * pick a new random turn and duration when the timer runs out. */
void func_8008F17C(Actor *actor, Brain *brain) {
    if (D_8009284C < 0x100) {
        brain->timer = 1;
        brain->unkC = 0;
    } else if (--brain->timer == -1) {
        brain->unkA = rand() % 0x600 - 0x300;
        brain->timer = rand() % 120 + 10;
        brain->unkC = 0xFF;
    }
}

/* Opponent command: store its argument, then run the mode's step. */
void func_8008F260(Actor *actor, Brain *brain, u8 arg) {
    brain->unkF = arg;
    func_80090E10(actor);
}

/* Drive the computer opponent one frame: reset its state when the command
 * changes, count down to the next decision (some commands decide every
 * frame), run the command and then steer and accelerate. */
void func_8008F280(Actor *actor) {
    extern u8 D_80099DA2; /* D_80099D98.command, the byte at +0x0A. */
    Brain *brain = actor->brain;
    s32 command = D_80099DA2;

    D_80099D98.driven = 1;
    if (D_80092848 != command) {
        brain->timer = 0;
        brain->unkC = 0;
        actor->flags &= ~2;
        actor->state = 0;
        actor->flags &= ~0x38;
        brain->defending = 0;
        D_80092848 = command;
        actor->unkCE = actor->unkCC + 0x800;
    }
    if (brain->defending) {
        actor->flags |= 2;
    }
    switch (D_80099DA2) {
    case 2:
    case 8:
    case 9:
    case 11:
    case 12:
    case 13:
        break;
    default:
        if (--brain->timer != -1) {
            return;
        }
        break;
    }
    switch (D_80099DA2) {
    case 3:
        func_8008EF30(actor, brain, 0);
        break;
    case 4:
        func_8008EF30(actor, brain, 1);
        break;
    case 5:
        func_8008EF74(actor, brain);
        break;
    case 6:
        func_8008F014(actor, brain);
        break;
    case 7:
        func_8008F060(actor, brain);
        break;
    case 9:
        func_8008F094(actor, brain);
        break;
    case 8:
        func_8008F17C(actor, brain);
        break;
    case 10:
        func_8008EF00(actor, brain);
        break;
    case 11:
        func_8008F260(actor, brain, 0);
        return;
    case 12:
        func_8008F260(actor, brain, 1);
        return;
    case 13:
        func_8008F260(actor, brain, 2);
        return;
    case 2:
        D_80099D98.driven = 0;
        return;
    case 1:
        func_8008EFA8(actor, brain);
        break;
    case 0:
    default:
        brain->timer = 1;
        break;
    }
    func_8008EE1C(actor, brain->unkA, brain->unkC);
}

/* Whether an actor's hp is still above the given fraction (of 255) of
 * its maximum. */
s32 func_8008F4F4(Actor *actor, s32 fraction) {
    return actor->max_hp * fraction / 255 < actor->hp;
}

/* Whether an actor lacks the charge for its special move (or, with a
 * flag, whether spending it is allowed). */
s32 func_8008F530(Actor *actor, s32 check) {
    if (check) {
        return func_80073DE4(actor, actor->unkBE);
    }
    return actor->charge < 0x1000 - actor->unkBE;
}

/* The charge left over after a special move. */
s32 func_8008F570(Actor *actor, Brain *brain) {
    return 0x1000 - actor->unkBE;
}

/* Compare an actor's charge with the level its brain waits for: 2 while
 * well below, else 1 up to the level and 0 above it. */
s32 func_8008F580(Actor *actor) {
    Brain *brain = actor->brain;

    if (brain->unk24 - 0x200 >= actor->charge) {
        return 2;
    }
    return !(brain->unk24 < actor->charge);
}

/* Decide whether the opponent attacks now, weighing its eagerness, its
 * charge and hp and the other actor's hp. */
s32 func_8008F5B4(Actor *actor, s32 unused) {
    Brain *brain = actor->brain;

    if ((rand() & 0xFF) < (brain->unk10 * 320) >> 4) {
        if (func_8008F530(actor, 0)) {
            goto press;
        }
        if (func_8008F4F4(actor, 0xC0)) {
            goto press;
        }
        if ((rand() & 0xFF) < (brain->unk1C * 192) >> 4) {
            goto press;
        }
        if (!func_8008F4F4(actor, 0x80) || actor->opponent->hp >= actor->hp) {
            return 0;
        }
    } else if ((rand() & 0xFF) >= (brain->unk1C * 320) >> 4) {
        return 0;
    }
press:
    if ((rand() & 0xFF) < brain->unk18) {
        if (actor->opponent->unkC4 == 4) {
            return 0;
        }
        if (func_8008F4F4(actor->opponent, 0x20)) {
            return 1;
        }
        if (actor->opponent->hp < actor->hp) {
            return 0;
        }
    }
    return 1;
}

/* Decide whether the opponent closes in: an eager opponent that is already
 * near holds back; otherwise it follows its charge or its eagerness. */
s32 func_8008F720(Actor *actor, s32 eager) {
    Brain *brain = actor->brain;

    if (eager && (rand() & 0xFF) < brain->unk10 && D_8009284C < 0x600) {
        return 0;
    }
    if (func_8008F580(actor)) {
        return 1;
    }
    return (rand() & 0xFF) < brain->unk1C;
}

/* Roll the opponent's choices for the next round from its tendencies. */
void func_8008F7B8(Brain *brain) {
    brain->unk2C_9 = (rand() & 0xFF) < brain->unk10;
    brain->unk2C_10 = (rand() & 0xFF) < brain->unk14;
    brain->unk2C_12 = rand() & 1;
    brain->unk2C_11 = (rand() & 0xFF) < brain->unk18;
    brain->unk2C_8 = (rand() & 0xFF) < brain->unk10 && rand() % 10 < 3;
    brain->roll = rand();
    brain->unk30 = brain->owner->unk1668;
}

/* Opponent jump attack: unless the other actor is airborne (then only one
 * time in four), act or jump and attack. */
void func_8008F900(Actor *actor) {
    if ((actor->opponent->flags & 0x60000000) != 0x20000000 || (rand() & 3) == 0) {
        if (D_80092884) {
            func_800767C8(actor);
            func_8007639C(actor, 4);
        } else if ((actor->flags & 0x60000000) == 0x20000000) {
            func_8007639C(actor, 4);
        }
        func_8007639C(actor, 3);
    }
}

/* Whether an actor stands in the far quadrant of the scene or on a floor
 * of kind 1. */
s32 func_8008F9B0(Actor *actor) {
    Vector pos = actor->pos;

    pos.vx -= 0x3F80;
    pos.vz -= 0x3F80;
    if (pos.vx > 0 && pos.vz > 0) {
        return 1;
    }
    return (func_800828C4(actor) & 0x3000000) == 0x1000000;
}

/* Steer the opponent toward one of two headings depending on which side
 * of the scene centre it stands, at full speed. */
s32 func_8008FA2C(Actor *actor, Brain *brain) {
    Vector pos = actor->pos;

    pos.vx -= 0x3F80;
    pos.vz -= 0x3F80;
    if ((ratan2(pos.vx, pos.vz) & 0xFFF) > 0x200) {
        brain->unkA = 0x800 - D_80092934;
    } else {
        brain->unkA = 0xC00 - D_80092934;
    }
    brain->unkC = 0xFF;
    return 0;
}

/* Opponent retreat rule (when enabled and on side 1): leave the far
 * quadrant toward the centre; when the other actor is there, dodge its
 * shots by turning to face away while they are close and stop once they
 * are far. Returns whether the rule took over; between the two distances
 * the original falls off the end with the last comparison (1) in $v0. */
s32 func_8008FACC(Actor *actor, Brain *brain) {
    s32 dist;

    if (!D_800928C4 || !(actor->flags & 0x08000000)) {
        return 0;
    }
    if (func_8008F9B0(actor)) {
        func_8008FA2C(actor, brain);
        return 1;
    }
    if (func_8008F9B0(actor->opponent)) {
        dist = actor->opponent->nearest_dist;
        if (dist < 0x800) {
            actor->flags |= 0x8000;
            brain->unkE = 1;
            brain->unkC = 0xFF;
            brain->unkA = 0x800;
            actor->target_angle = 0x800;
            return 1;
        }
        if (dist > 0x1000) {
            actor->flags &= ~0x8000;
            brain->unkE = 0;
            brain->unkC = 0;
            actor->target_angle = 0x800;
            return 1;
        }
    }
    /* falls off the end: the original returns the failed check (0) or,
     * between the distances, the comparison result (1) */
}

/* Opponent guard reaction: always at level 2, else on a random roll
 * (every other round at level 1, one in six at level 0). */
void func_8008FBD8(Actor *actor, Brain *brain) {
    if (brain->unkF >= 2) {
        actor->flags |= 2;
        func_80076424(actor);
    } else if (brain->unkF != 0) {
        if (brain->roll & 1) {
            actor->flags |= 2;
            func_80076424(actor);
        }
    } else if (brain->roll % 6 == 0) {
        actor->flags |= 2;
        func_80076424(actor);
    }
}

/* Start a new opponent round: idle mode, a pause that is shorter at
 * higher levels, and fresh rolls. */
void func_8008FC7C(Actor *actor) {
    Brain *brain = actor->brain;

    brain->unk20 = 0;
    brain->mode = 0;
    brain->timer = (2 - brain->unkF) * 30 + 90;
    func_8008F7B8(brain);
}

/* The opponent's idle mode: after the retreat rule, react to closeness,
 * guard against a charging opponent, pick a fight when the round's clock
 * runs out, dodge close shots, and wait or attack. */
void func_8008FCC8(Actor *actor, Brain *brain) {
    brain->unkC = 0;
    brain->unkE = 0;
    brain->unkA = 0;
    if (func_8008FACC(actor, brain)) {
        return;
    }
    if (D_8009284C < 0x200) {
        if (brain->unk2C_12) {
            func_80090174(actor);
        } else if (brain->unkF) {
            func_80090894(actor, 1);
        }
        if (actor->opponent->unkC5 == 2) {
            func_8008FBD8(actor, brain);
        }
        if (actor->unk1668 + 2 < brain->unk30) {
            func_80090174(actor);
        }
    }
    if (actor->opponent->nearest_dist < 0x400) {
        func_80090894(actor, 3);
        brain->unkE = 1;
    }
    switch (brain->unk20) {
    case 0:
        if (--brain->timer < 0) {
            brain->unk20++;
        }
        if (actor->opponent->unkC4 != 0) {
            break;
        }
        if (!brain->unk2C_12) {
            break;
        }
        if (func_8008F5B4(actor, 0)) {
            func_8008F900(actor);
        }
        func_80090504(actor, 0);
        break;
    case 1:
        if (func_8008F530(actor, 0)) {
            func_80090504(actor, 0);
        }
        break;
    }
}

/* Opponent command: one to three random inputs (1 or 2). */
void func_8008FE80(Actor *actor) {
    s32 roll = rand() % 10;
    s32 count = roll >= 2 ? 2 : 1;

    if (roll >= 5) {
        count++;
    }
    while (count != 0) {
        count--;
        func_8007639C(actor, (rand() & 1) + 1);
    }
}

/* Opponent attack mode step: against a downed opponent maybe jump in;
 * otherwise press random inputs and wait a level-dependent time. */
void func_8008FF24(Actor *actor, Brain *brain) {
    if (actor->opponent->unkC4 == 4) {
        if (brain->unk2C_9 && !brain->unk2C_11) {
            func_8008F900(actor);
            brain->timer = 3;
        }
    } else {
        func_8008FE80(actor);
        brain->timer = (2 - brain->unkF) * 20 + 1 + rand() % 20;
    }
    func_8008F7B8(brain);
}

/* Opponent special move: enter the inputs of a random usable learned
 * move, then wait a level-dependent time. Returns 1 when it knows none. */
s32 func_8008FFEC(Actor *actor, Brain *brain) {
    s32 pick;
    s32 i;

    /* pick first counts the moves, then selects one of them */
    pick = actor->move_count;
    if (pick == 0) {
        return 1;
    }
    pick = rand() % pick;
    for (i = 0; i < 14; i++) {
        if (actor->moves->learned[i] && actor->move_slots[i].usable) {
            if (pick == 0) {
                if (D_800925A4[i][0]) {
                    func_8007639C(actor, D_800925A4[i][0]);
                }
                if (D_800925A4[i][1]) {
                    func_8007639C(actor, D_800925A4[i][1]);
                }
                if (D_800925A4[i][2]) {
                    func_8007639C(actor, D_800925A4[i][2]);
                }
                break;
            }
            pick--;
        }
    }
    brain->timer = (2 - brain->unkF) * 20 + 1 + rand() % 20;
    func_8008F7B8(brain);
    return 0;
}

/* Enter the opponent's attack mode: one attack step now and a number of
 * further steps that grows with its level. */
void func_80090174(Actor *actor) {
    Brain *brain = actor->brain;

    brain->mode = 1;
    func_8008FF24(actor, brain);
    if (brain->unkF >= 2) {
        brain->unk9 = rand() % 8 + 1;
    } else if (brain->unkF != 0) {
        brain->unk9 = rand() % 6 + 1;
    } else {
        brain->unk9 = rand() % 4 + 1;
    }
    brain->unkC = 0;
    brain->unkE = 0;
    func_8008F7B8(brain);
}

/* Opponent attack choice: a jump attack or a special move (when it knows
 * any and is close enough). Returns 1 when it did nothing. */
s32 func_80090258(Actor *actor, Brain *brain) {
    if (actor->move_count != 0) {
        if (!(rand() & 1)) {
            return 1;
        }
        if (!(rand() & 1) || !func_8008F5B4(actor, 0)) {
            if (D_8009284C > 0x1000) {
                return 1;
            }
            func_8008FFEC(actor, brain);
            return 0;
        }
    } else if (!func_8008F5B4(actor, 0)) {
        return 1;
    }
    func_8008F900(actor);
    return 0;
}

/* The opponent's attack mode: after the retreat rule and guard reactions,
 * when the step timer runs out pick the next action at random, then keep
 * attacking while steps remain or fall back to the approach mode. */
void func_8009031C(Actor *actor, Brain *brain) {
    s32 dist;

    if (func_8008FACC(actor, brain)) {
        return;
    }
    if (actor->unkC5 != 2 && actor->opponent->unkC5 == 2) {
        func_8008FBD8(actor, brain);
    }
    dist = actor->opponent->nearest_dist;
    if (dist > 0x200 && dist < 0x600 && brain->unkF) {
        func_8008FBD8(actor, brain);
    }
    if (--brain->timer > 0) {
        return;
    }
    if (D_8009284C > 0x300) {
        switch (rand() % 10) {
        case 0:
            if ((rand() & 0xFF) >= brain->unk14) {
                func_80090894(actor, 0);
            }
            break;
        case 2:
            if (actor->move_count != 0) {
                func_8008FFEC(actor, brain);
                break;
            }
            func_8008FC7C(actor);
            break;
        case 3:
        case 4:
        case 5:
            if (!func_80090258(actor, brain)) {
                break;
            }
            /* fallthrough */
        case 1:
            func_8008FC7C(actor);
            break;
        case 6:
        case 7:
        case 8:
        case 9:
            func_80090504(actor, 0);
            break;
        }
    }
    if (brain->unk9--) {
        func_8008FF24(actor, brain);
    } else {
        func_80090894(actor, rand() & 1);
    }
}

/* Enter the opponent's approach mode (3): a few steps, fresh rolls, and
 * whether it closes in. */
void func_80090504(Actor *actor, s32 kind) {
    Brain *brain = actor->brain;

    brain->mode = 3;
    brain->unk9 = rand() % 4 + 1;
    brain->timer = 0;
    func_8008F7B8(brain);
    brain->unkE = func_8008F720(actor, 1);
    brain->unk2E = 0;
}

/* The opponent's approach mode (3) step: give up when the other actor retreated,
 * sidestep homing shots (and maybe counter-attack), attack when close,
 * and pick a new heading and duration whenever the timer runs out. */
void func_80090580(Actor *actor, Brain *brain) {
    s32 roll;

    if (func_8008F9B0(actor->opponent) && D_800928C4 && (actor->flags & 0x08000000)) {
        func_8008FC7C(actor);
        return;
    }
    if (actor->opponent->nearest_shot->steer == 1 && actor->opponent->nearest_dist < 0x500 &&
        brain->unkF) {
        brain->unkE = 1;
        brain->unkA = brain->unk2C_12 ? 0x400 : -0x400;
        if (brain->unkF >= 2 && (rand() & 0xFF) < brain->unk14 &&
            func_8008F5B4(actor, 0) && brain->unk2C_12) {
            if ((rand() & 3) == 0) {
                func_8007639C(actor, 4);
            }
            func_8008F900(actor);
            brain->unk2E = 0;
        }
        brain->unk2E++;
    }
    if (D_8009284C < 0x180) {
        if ((rand() & 3) == 0) {
            func_8007639C(actor, 4);
        }
        func_80090174(actor);
    }
    if (!func_8008F580(actor)) {
        brain->unkE = 0;
    }
    if (brain->timer < 0) {
        brain->unkA = rand() % 0x600 - 0x300;
        roll = rand();
        brain->timer = (brain->unk2C_10 ? roll % 120 : roll % 100) + 10;
        brain->unkC = 0xFF;
        if (brain->unk9 != 0) {
            brain->unk9--;
        } else {
            if ((rand() & 3) == 0) {
                func_8007639C(actor, 4);
            }
            if (func_80090258(actor, brain)) {
                func_8008FC7C(actor);
            }
            brain->unk9 = rand() % 4 + 1;
        }
    }
    brain->timer--;
}

/* Enter the opponent's distance mode (2): maybe act first, then a random
 * distance to keep and a few decisions. */
void func_80090894(Actor *actor, s32 kind) {
    Brain *brain = actor->brain;

    brain->mode = 2;
    if (rand() % 3 == 0) {
        func_800767C8(actor);
    }
    brain->unk28 = rand() % 0x600 + 0x100;
    brain->timer = 0;
    brain->unk9 = rand() % 5 + 3;
    func_8008F7B8(brain);
    brain->unkE = func_8008F720(actor, 0);
    brain->unk2E = 0;
}

/* The opponent's distance mode (2) step: sidestep homing shots (maybe
 * countering), use a special move once far enough (or when forced), and
 * pick a new wide heading and duration whenever the timer runs out. */
void func_80090990(Actor *actor, Brain *brain) {
    s32 roll;

    if (func_8008FACC(actor, brain)) {
        return;
    }
    if (actor->opponent->nearest_shot->steer == 1 && actor->opponent->nearest_dist < 0x500 &&
        brain->unkF) {
        brain->unkE = 1;
        brain->unkA = brain->unk2C_12 ? 0x400 : -0x400;
        if (brain->unkF >= 2 && func_8008F5B4(actor, 0) && brain->unk2C_12) {
            if ((rand() & 3) == 0) {
                func_8007639C(actor, 4);
            }
            func_8008F900(actor);
            brain->unk2E = 0;
        }
        brain->unk2E++;
    }
    if (!func_8008F580(actor)) {
        brain->unkE = 0;
    }
    if (D_8009284C > brain->unk28 || (D_80092884 && D_80092850 > 0x4B0)) {
        if (D_80092884) {
            func_8007639C(actor, 4);
        }
        func_8008FFEC(actor, brain);
        func_8008FC7C(actor);
    }
    if (brain->timer < 0 || brain->unkC == 0) {
        brain->unkA = rand() % 0x600 + 0x500;
        roll = rand();
        brain->timer = (brain->unk2C_10 ? roll % 40 : roll % 60) + 10;
        brain->unkC = 0xFF;
        if (brain->unk9 != 0) {
            brain->unk9--;
        } else {
            if (func_8008F5B4(actor, 0)) {
                func_8007639C(actor, 4);
                func_8008F900(actor);
            }
            brain->unk9 = rand() % 5 + 3;
        }
    }
    brain->timer--;
}

/* Load the opponent's four tendencies from its move list. */
void func_80090C88(Actor *actor) {
    MoveList *moves = actor->moves;
    Brain *brain = actor->brain;

    brain->unk10 = moves->tendency[0];
    brain->unk14 = moves->tendency[1];
    brain->unk18 = moves->tendency[2];
    brain->unk1C = moves->tendency[3];
}

/* Attach and reset the opponent brain of the actor's side and start it in
 * a random mode. */
void func_80090CC0(Actor *actor) {
    Brain *brain = &D_80096F30;

    if (actor->flags & 0x08000000) {
        brain = &D_80096F64;
    }
    actor->brain = brain;
    brain->unk6 = 0x10;
    brain->owner = actor;
    brain->timer = 0;
    brain->unk7 = 0xA;
    brain->unkA = 0;
    brain->unkC = 0;
    brain->mode = 0;
    brain->unk9 = 0;
    actor->state = 0;
    brain->unk24 = func_8008F570(actor, brain);
    brain->unkF = D_80099D98.level;
    func_80090C88(actor);
    switch (rand() % 3) {
    case 0:
        func_8008FC7C(actor);
        break;
    case 1:
        func_80090174(actor);
        break;
    case 2:
        func_80090894(actor, 0);
        break;
    case 3:
        func_80090504(actor, 0);
        break;
    }
    func_80076424(actor);
}

/* Run the computer opponent for one frame when enabled: its current mode's
 * step, then dodge and guard flags and steering. */
void func_80090E10(Actor *actor) {
    Brain *brain;

    if (actor->flags & 0x40) {
        brain = actor->brain;
        brain->unkF = D_80099D98.level;
        switch (brain->mode) {
        case 0:
            func_8008FCC8(actor, brain);
            break;
        case 1:
            func_8009031C(actor, brain);
            break;
        case 2:
            func_80090990(actor, brain);
            break;
        case 3:
            func_80090580(actor, brain);
            break;
        }
        if (brain->unkE) {
            actor->flags |= 0x8000;
        }
        if (brain->defending) {
            actor->flags |= 2;
        }
        func_8008EE1C(actor, brain->unkA, brain->unkC);
    }
}
