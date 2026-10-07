/* Field unit 8007A44C-800854D0: quad and compass helpers, the marker and
 * pointer, collision (floor, edge and camera walks), fades, dialogue
 * windows, event actors and the actor update and motion.
 *
 * GCC aligns jump tables to 8 within a unit's rodata (docs/matching.md).
 * The tables of 8007bef4, 8007c694, 8007cd80 and 8007d3d4 (rodata 0x9c-0x11c)
 * sit 4 mod 8 between field.c's rodata, which ends with "Clear OTAG" (used by
 * 80077dac) at 0x9c, and 8008e59c's table at 0x198 (0 mod 8). So this unit's
 * rodata is exactly 0x9c-0x198 (it ends with the strings of 8008110c,
 * 80084158 and 80084a40), and its text starts after 80077dac, at or before
 * 8007bef4, and ends after 80084a40, before 8008e59c.
 * Within those ranges the boundaries are chosen, not measured: 8008a2e8
 * passes unnarrowed ints to 80070340 and 8007a44c (s16 parameters), as do
 * 800a8408/800a8ba4/800a8eac, so it saw neither prototype and the unit that
 * defines 8007a44c ends before 8008a2e8; 80077268 calls 80084a40 without
 * its five-argument prototype. The start is placed where the field-mode
 * code (mode entry, encounters, menus; 80077e10-800799d4) gives way to the
 * drawing and collision helpers, the end where the actor motion gives way
 * to the music and sound-effect code (800854d0). */
#include "common.h"
#include "field.h"
#include "field_anim.h"
#include "field_gte.h"
#include "field_motion.h"

/* Set a quad's texture coordinates, each clamped to 0..255. */
void func_8007A44C(POLY_FT4 *poly, s16 u0, s16 v0, s16 u1, s16 v1, s16 u2, s16 v2, s16 u3, s16 v3) {
    if (u0 < 0) {
        u0 = 0;
    }
    if (u1 < 0) {
        u1 = 0;
    }
    if (u2 < 0) {
        u2 = 0;
    }
    if (u3 < 0) {
        u3 = 0;
    }
    if (v0 < 0) {
        v0 = 0;
    }
    if (v1 < 0) {
        v1 = 0;
    }
    if (v2 < 0) {
        v2 = 0;
    }
    if (v3 < 0) {
        v3 = 0;
    }
    if (u0 >= 0x100) {
        u0 = 0xFF;
    }
    if (u1 >= 0x100) {
        u1 = 0xFF;
    }
    if (u2 >= 0x100) {
        u2 = 0xFF;
    }
    if (u3 >= 0x100) {
        u3 = 0xFF;
    }
    if (v0 >= 0x100) {
        v0 = 0xFF;
    }
    if (v1 >= 0x100) {
        v1 = 0xFF;
    }
    if (v2 >= 0x100) {
        v2 = 0xFF;
    }
    if (v3 >= 0x100) {
        v3 = 0xFF;
    }
    poly->u0 = u0;
    poly->v0 = v0;
    poly->u1 = u1;
    poly->v1 = v1;
    poly->u2 = u2;
    poly->v2 = v2;
    poly->u3 = u3;
    poly->v3 = v3;
}

extern FieldMarker D_800B0FEC[4]; /* the four compass letters */
extern s16 D_800ADE30[32];        /* letter corners: x, z per corner */
extern u8 D_800ADE70[32];         /* letter texture coordinates */

/* Build the four compass letters: corners from 800ade30, texture
 * coordinates from 800ade70 (v offset c0), semi-transparent, then copy the
 * quad to the second buffer. */
void func_8007A5C4(void) {
    FieldMarker *record;
    POLY_FT4 *quad;
    POLY_FT4 *copy;
    s32 i;

    for (i = 0; i < 4; i++) {
        record = &D_800B0FEC[i];
        copy = &D_800B0FEC[i].poly[1];
        quad = &D_800B0FEC[i].poly[0];
        SetPolyFT4(quad);
        record->v[0].vx = D_800ADE30[i * 8];
        record->v[0].vy = 0;
        record->v[0].vz = D_800ADE30[i * 8 + 1];
        record->v[1].vx = D_800ADE30[i * 8 + 2];
        record->v[1].vy = 0;
        record->v[1].vz = D_800ADE30[i * 8 + 3];
        record->v[2].vx = D_800ADE30[i * 8 + 4];
        record->v[2].vy = 0;
        record->v[2].vz = D_800ADE30[i * 8 + 5];
        record->v[3].vx = D_800ADE30[i * 8 + 6];
        record->v[3].vy = 0;
        record->v[3].vz = D_800ADE30[i * 8 + 7];
        quad->r0 = 0x80;
        quad->g0 = 0x80;
        quad->b0 = 0x80;
        func_8007A44C(quad, D_800ADE70[i * 8], D_800ADE70[i * 8 + 1] + 0xC0, D_800ADE70[i * 8 + 2],
                      D_800ADE70[i * 8 + 3] + 0xC0, D_800ADE70[i * 8 + 4], D_800ADE70[i * 8 + 5] + 0xC0, D_800ADE70[i * 8 + 6],
                      D_800ADE70[i * 8 + 7] + 0xC0);
        SetSemiTrans(quad, 1);
        quad->tpage = GetTPage(0, 2, 0x280, 0x1C0);
        quad->clut = GetClut(0x100, 0xF2);
        *copy = *quad;
    }
}

extern s16 D_800ADCE0[][4]; /* compass corner x by column */
extern s16 D_800ADD28[][4]; /* compass corner z by row */
extern s16 D_800ADD70[][4]; /* texture u by column */
extern s16 D_800ADDB8[][4]; /* texture v by row */
extern s16 D_800ADE00[][6]; /* texture page (tp, abr, x, y) and palette (x, y) by style */

/* Build a compass quad record for a grid column and row in a style, then
 * copy the quad to the second buffer. */
void func_8007A7F4(FieldMarker *record, s32 column, s32 row, s32 style) {
    u8 unused[0x88]; /* never used; the original frame reserves it */
    POLY_FT4 *quad;
    POLY_FT4 *copy;

    copy = &record->poly[1];
    quad = &record->poly[0];
    SetPolyFT4(quad);
    record->v[0].vx = D_800ADCE0[column][0];
    record->v[0].vy = 0;
    record->v[0].vz = D_800ADD28[row][0];
    record->v[1].vx = D_800ADCE0[column][1];
    record->v[1].vy = 0;
    record->v[1].vz = D_800ADD28[row][1];
    record->v[2].vx = D_800ADCE0[column][2];
    record->v[2].vy = 0;
    record->v[2].vz = D_800ADD28[row][2];
    record->v[3].vx = D_800ADCE0[column][3];
    record->v[3].vy = 0;
    record->v[3].vz = D_800ADD28[row][3];
    quad->r0 = 0x80;
    quad->g0 = 0x80;
    quad->b0 = 0x80;
    quad->tpage = GetTPage(D_800ADE00[style][0], D_800ADE00[style][1], D_800ADE00[style][2], D_800ADE00[style][3]);
    quad->clut = GetClut(D_800ADE00[style][4], D_800ADE00[style][5]);
    func_8007A44C(quad, D_800ADD70[column][0], D_800ADDB8[row][0], D_800ADD70[column][1], D_800ADDB8[row][1],
                  D_800ADD70[column][2], D_800ADDB8[row][2], D_800ADD70[column][3], D_800ADDB8[row][3]);
    *copy = record->poly[0];
}

/* Set up a pointer marker: a 48x48 quad around the origin and its
 * semi-transparent textured primitive, copied for the second buffer. */
void func_8007AA44(FieldMarker *m) {
    POLY_FT4 *copy = &m->poly[1];

    SetPolyFT4(&m->poly[0]);
    m->v[3].vx = -0x18;
    m->v[3].vy = 0;
    m->v[3].vz = -0x18;
    m->v[2].vx = 0x18;
    m->v[2].vy = 0;
    m->v[2].vz = -0x18;
    m->v[1].vx = -0x18;
    m->v[1].vy = 0;
    m->v[1].vz = 0x18;
    m->v[0].vx = 0x18;
    m->v[0].vy = 0;
    m->v[0].vz = 0x18;
    m->poly[0].r0 = 0x80;
    m->poly[0].g0 = 0x80;
    m->poly[0].b0 = 0x80;
    m->poly[0].tpage = GetTPage(0, 2, 0x280, 0x1E0);
    m->poly[0].clut = GetClut(0x100, 0xF3);
    SetSemiTrans(&m->poly[0], 1);
    m->poly[0].u0 = 0;
    m->poly[0].v0 = 0xE0;
    m->poly[0].u1 = 0xF;
    m->poly[0].v1 = 0xE0;
    m->poly[0].u2 = 0;
    m->poly[0].v2 = 0xEF;
    m->poly[0].u3 = 0xF;
    m->poly[0].v3 = 0xEF;
    *copy = m->poly[0];
}

/* Project a marker quad's four corners with the given matrix into its buffer's
 * textured polygon and link that polygon into the ordering table entry. */
void func_8007AB6C(u32 *ot, FieldMarker *marker, MATRIX *m, s32 buffer) {
    POLY_FT4 *poly = &marker->poly[buffer];
    s32 p;
    s32 flag;

    PushMatrix();
    SetRotMatrix(m);
    SetTransMatrix(m);
    RotAverage4(&marker->v[0], &marker->v[1], &marker->v[2], &marker->v[3],
                &poly->x0, &poly->x1, &poly->x2, &poly->x3, &p, &flag);
    addPrim(ot + 1, poly);
    PopMatrix();
}

/* Project a quad, then replace it with a 16x10 screen-aligned sprite standing
 * on the midpoint of its projected bottom edge, and link it into the ordering
 * table entry. */
void func_8007AC58(u32 *ot, FieldMarker *marker, MATRIX *m, s32 buffer) {
    POLY_FT4 *poly = &marker->poly[buffer];
    s32 p;
    s32 flag;
    s32 x;
    s32 y;
    s32 right;

    PushMatrix();
    SetRotMatrix(m);
    SetTransMatrix(m);
    RotAverage4(&marker->v[0], &marker->v[1], &marker->v[2], &marker->v[3],
                &poly->x0, &poly->x1, &poly->x2, &poly->x3, &p, &flag);
    x = (poly->x3 + poly->x2) / 2;
    right = x + 8;
    x -= 8;
    y = poly->y3;
    setXY4(poly, x, y - 10, right, y - 10, x, y, right, y);
    addPrim(ot + 1, poly);
    PopMatrix();
}

/* Set the pointer's two pad buffers. */
void func_8007AD8C(void *pad0, void *pad1) {
    D_800B0054[0] = pad0;
    D_800B0054[1] = pad1;
}

/* Set the pointer bounds, scaled by the divisors. */
void func_8007ADA4(s32 left, s32 right, s32 top, s32 bottom) {
    D_800C3A44 = left * D_800B005C;
    D_800C3A50 = right * D_800B005C;
    D_800C3A4C = top * D_800B0060;
    D_800C3A54 = bottom * D_800B0060;
}

/* Set the pointer's X and Y divisors. */
void func_8007AE14(s32 x_divisor, s32 y_divisor) {
    D_800B005C = x_divisor;
    D_800B0060 = y_divisor;
}

/* Set a port's pointer position, scaled by the divisors. */
void func_8007AE2C(s32 port, s32 x, s32 y) {
    D_800B0068[port] = x * D_800B005C;
    D_800B0070[port] = y * D_800B0060;
}

/* Read a port's pointer into out[0..4]: x and y (unscaled), buttons (mouse
 * pads only, else -0x100), x and y motion. Declared int but returns nothing. */
s32 func_8007AE78(s32 port, s32 *out) {
    func_8007AF74(port);
    out[0] = D_800B0068[port] / D_800B005C;
    out[1] = D_800B0070[port] / D_800B0060;
    out[2] = -0x100;
    out[3] = D_800B0054[port][4];
    out[4] = D_800B0054[port][5];
    if (D_800B0054[port][0] == 0 && D_800B0054[port][1] == 0x12) {
        out[2] = ~D_800B0054[port][3] & 0xC;
    }
}

/* Move a mouse port's pointer by its motion, kept inside the bounds. */
void func_8007AF74(s32 port) {
    if (D_800B0054[port][0] == 0 && D_800B0054[port][1] == 0x12) {
        D_800B0068[port] += D_800B0054[port][4];
        D_800B0070[port] += D_800B0054[port][5];
        if (D_800B0068[port] > D_800C3A50) {
            D_800B0068[port] = D_800C3A50;
        } else if (D_800B0068[port] < D_800C3A44) {
            D_800B0068[port] = D_800C3A44;
        }
        if (D_800B0070[port] > D_800C3A54) {
            D_800B0070[port] = D_800C3A54;
        } else if (D_800B0070[port] < D_800C3A4C) {
            D_800B0070[port] = D_800C3A4C;
        }
    }
}

/* The height of `p` on the plane through triangle a, b, c (0 for a vertical
 * plane); the plane normal is left in `normal`. */
void func_8007B07C(SVECTOR *a, SVECTOR *b, SVECTOR *c, SVECTOR *p, VECTOR *normal) {
    VECTOR edge_b;
    VECTOR edge_c;
    VECTOR d;

    d.vx = b->vx - a->vx;
    d.vy = b->vy - a->vy;
    d.vz = b->vz - a->vz;
    VectorNormal(&d, &edge_b);
    d.vx = c->vx - a->vx;
    d.vy = c->vy - a->vy;
    d.vz = c->vz - a->vz;
    VectorNormal(&d, &edge_c);
    func_8004A480(&edge_b, &edge_c, normal);
    if (normal->vy == 0) {
        p->vy = 0;
        return;
    }
    p->vy = a->vy + (-(normal->vx * (p->vx - a->vx)) - normal->vz * (p->vz - a->vz)) / normal->vy;
}

void func_8007B07C(SVECTOR *a, SVECTOR *b, SVECTOR *c, SVECTOR *p, VECTOR *normal);

/* Find the collision triangle of `layer` under the X/Z point: the index of
 * the first triangle whose three edges wind around it (normal clip on the
 * X/Z plane), with the point's height on it in `point` and the plane normal
 * in `normal`; 0 with both cleared when none does. */
s32 func_8007B1C4(s32 x, s32 z, s32 layer, SVECTOR *point, VECTOR *normal) {
    u8 unused[0x30]; /* never used; the original frame reserves it */
    SVECTOR a;
    SVECTOR b;
    SVECTOR c;
    SVECTOR p;
    s32 winding[3];
    CollisionTriangle *triangles;
    SVECTOR *vertices;
    s32 count;
    s32 sxy0;
    s32 sxy1;
    s32 sxy2;
    s32 sxy;
    s32 i;

    p.vx = x;
    p.vy = 0;
    p.vz = z;
    triangles = D_800AF880.components.collision_triangles[layer];
    count = D_800AF880.components.triangle_counts[layer];
    vertices = D_800AF880.components.collision_vertices[layer];
    sxy = (x << 16) + z;
    for (i = 0; i < count; i++) {
        sxy0 = (vertices[triangles[i].unk00[0]].vx << 16) + vertices[triangles[i].unk00[0]].vz;
        sxy1 = (vertices[triangles[i].unk00[1]].vx << 16) + vertices[triangles[i].unk00[1]].vz;
        sxy2 = (vertices[triangles[i].unk00[2]].vx << 16) + vertices[triangles[i].unk00[2]].vz;
        gte_ldsxy3(sxy0, sxy1, sxy);
        gte_nclip();
        gte_stopz(&winding[0]);
        if (winding[0] < 0) {
            continue;
        }
        gte_ldsxy3(sxy1, sxy2, sxy);
        gte_nclip();
        gte_stopz(&winding[1]);
        if (winding[1] < 0) {
            continue;
        }
        gte_ldsxy3(sxy2, sxy0, sxy);
        gte_nclip();
        gte_stopz(&winding[2]);
        if (winding[2] < 0) {
            continue;
        }
        a.vx = vertices[triangles[i].unk00[0]].vx;
        a.vy = vertices[triangles[i].unk00[0]].vy;
        a.vz = vertices[triangles[i].unk00[0]].vz;
        b.vx = vertices[triangles[i].unk00[1]].vx;
        b.vy = vertices[triangles[i].unk00[1]].vy;
        b.vz = vertices[triangles[i].unk00[1]].vz;
        c.vx = vertices[triangles[i].unk00[2]].vx;
        c.vy = vertices[triangles[i].unk00[2]].vy;
        c.vz = vertices[triangles[i].unk00[2]].vz;
        func_8007B07C(&a, &b, &c, &p, normal);
        point->vx = p.vx;
        point->vy = p.vy;
        point->vz = p.vz;
        return i;
    }
    point->vx = 0;
    point->vy = 0;
    point->vz = 0;
    normal->vx = 0;
    normal->vy = 0;
    normal->vz = 0;
    return 0;
}

/* -1 when `p` lies outside triangle a, b, c on the X/Z plane (to the
 * negative side of an edge), else 0. */
s32 func_8007B478(SVECTOR *a, SVECTOR *b, SVECTOR *c, SVECTOR *p) {
    VECTOR edge;
    VECTOR to_p;
    VECTOR cross;

    edge.vx = b->vx - a->vx;
    edge.vy = 0;
    edge.vz = b->vz - a->vz;
    to_p.vx = p->vx - a->vx;
    to_p.vy = 0;
    to_p.vz = p->vz - a->vz;
    OuterProduct0(&edge, &to_p, &cross);
    if (cross.vy < 0) {
        return -1;
    }
    edge.vx = c->vx - b->vx;
    edge.vy = 0;
    edge.vz = c->vz - b->vz;
    to_p.vx = p->vx - b->vx;
    to_p.vy = 0;
    to_p.vz = p->vz - b->vz;
    OuterProduct0(&edge, &to_p, &cross);
    if (cross.vy < 0) {
        return -1;
    }
    edge.vx = a->vx - c->vx;
    edge.vy = 0;
    edge.vz = a->vz - c->vz;
    to_p.vx = p->vx - c->vx;
    to_p.vy = 0;
    to_p.vz = p->vz - c->vz;
    OuterProduct0(&edge, &to_p, &cross);
    if (cross.vy < 0) {
        return -1;
    }
    return 0;
}

/* The X/Z offset `distance` away at `angle`, scaled by 800b218c. */
void func_8007B614(VECTOR *out, s32 distance, s32 angle) {
    s32 heading;

    distance *= 16;
    distance = (distance * D_800B2078.scale) >> 12;
    heading = angle & 0xFFF;
    out->vx = func_8003F8CC(heading) * distance;
    out->vz = -(func_8003F8B0(heading) * distance);
    out->vy = 0;
}

/* The heading of an X/Z offset. */
s32 func_8007B694(VECTOR *v) {
    return -ratan2(v->vz, v->vx) & 0xFFF;
}

/* Slide along a wall edge: the heading of `edge` (its two X/Z endpoints);
 * when `heading` meets it at an angle (not within 0x80 of parallel), the
 * velocity becomes the X/Z speed (80099a4c) along the edge direction nearer
 * the heading and that direction's heading is returned; otherwise the
 * velocity is cleared. */
s32 func_8007B6C4(s16 heading, SVECTOR *edge, VECTOR *velocity, s32 unused) {
    VECTOR d;
    VECTOR n;
    s16 angle;
    s32 relative;
    s16 result;
    s32 speed;

    relative = (0xC00 - heading) & 0xFFF;
    angle = -ratan2(edge[1].vz - edge[0].vz, edge[1].vx - edge[0].vx) & 0xFFF;
    relative = (relative + angle) & 0xFFF;
    result = angle;
    if (relative - 0x80 > 0xF00U) {
        velocity->vx = 0;
        velocity->vy = 0;
        velocity->vz = 0;
        return angle;
    }
    if (relative < 0x800) {
        d.vx = edge[0].vx - edge[1].vx;
        d.vy = 0;
        d.vz = edge[0].vz - edge[1].vz;
        result = (angle + 0x800) & 0xFFF;
    } else {
        d.vx = edge[1].vx - edge[0].vx;
        d.vy = 0;
        d.vz = edge[1].vz - edge[0].vz;
    }
    VectorNormal(&d, &n);
    speed = func_80099A4C(velocity->vx >> 12, velocity->vz >> 12);
    velocity->vx = n.vx * speed;
    velocity->vy = 0;
    velocity->vz = n.vz * speed;
    return result;
}

s32 func_8007B6C4(s16 heading, SVECTOR *edge, VECTOR *velocity, s32 unused);
s32 func_8007C694(VECTOR *probe, s32 *position, FieldActor *actor, SVECTOR *edge, SVECTOR *floor, s32 mode);

/* Move `actor` by `delta` (in/out) heading `heading`: probe 64 units ahead
 * and 0x100 to each side (8007c694 mode -1); when any probe is blocked, slide
 * the delta along the blocking edge (8007b6c4). Then find the floor under the
 * moved point: -1 when there is none or it rises above the actor (unless
 * 800adb98 is set or the actor has flag 40000, which keeps its +ec height),
 * else put the delta's height on the floor, update the actor's +72 and
 * return 0. */
s32 func_8007B814(VECTOR *delta, FieldActor *actor, SVECTOR *edge, s16 heading) {
    VECTOR probe;
    SVECTOR floor;
    u8 unused[0x20]; /* never used; the original frame reserves it */
    s32 angle;

    angle = heading & 0xFFF;
    probe.vx = delta->vx + (func_8003F8CC(angle) << 6);
    probe.vz = delta->vz - (func_8003F8B0(angle) << 6);
    if (func_8007C694(&probe, actor->position, actor, edge, &floor, -1) == -1) {
        probe.vx = delta->vx;
        probe.vy = delta->vy;
        probe.vz = delta->vz;
        func_8007B6C4(heading, edge, &probe, 0);
    } else {
        angle = heading - 0x100;
        angle &= 0xFFF;
        probe.vx = delta->vx + (func_8003F8CC(angle) << 6);
        probe.vz = delta->vz - (func_8003F8B0(angle) << 6);
        if (func_8007C694(&probe, actor->position, actor, edge, &floor, -1) == -1) {
            probe.vx = delta->vx;
            probe.vy = delta->vy;
            probe.vz = delta->vz;
            func_8007B6C4(heading, edge, &probe, 0);
        } else {
            angle = heading + 0x100;
            angle &= 0xFFF;
            probe.vx = delta->vx + (func_8003F8CC(angle) << 6);
            probe.vz = delta->vz - (func_8003F8B0(angle) << 6);
            if (func_8007C694(&probe, actor->position, actor, edge, &floor, -1) == -1) {
                probe.vx = delta->vx;
                probe.vy = delta->vy;
                probe.vz = delta->vz;
                func_8007B6C4(heading, edge, &probe, 0);
            } else {
                probe.vx = delta->vx;
                probe.vy = delta->vy;
                probe.vz = delta->vz;
            }
        }
    }
    if (func_8007C694(&probe, actor->position, actor, edge, &floor, 0) == -1) {
        return -1;
    }
    if (!(actor->flags & 0x40000)) {
        if ((floor.vy << 16) < actor->position[1] && D_800ADB98 == 0) {
            return -1;
        }
    } else {
        floor.vy = actor->unkEC;
    }
    probe.vy = (floor.vy << 16) - actor->position[1];
    delta->vx = probe.vx;
    delta->vy = probe.vy;
    delta->vz = probe.vz;
    actor->unk72 = (actor->position[1] + delta->vy) >> 16;
    return 0;
}


s32 func_8007BEF4(VECTOR *probe, s32 *position, FieldActor *actor, SVECTOR *edge, SVECTOR *floor, s32 mode,
                  s32 *attribute);

/* Move `actor` by `delta` (in/out) heading `heading`, as 8007b814 but with
 * the attribute-aware floor search (8007bef4): probe 0x100 to each side and
 * straight ahead, sliding along a blocking edge. The delta is pushed back
 * away from the floor found (flag 4000000) when its height is above the
 * actor's (smaller y), it has attribute 200000, it has 420000 while the
 * actor's +14 does too, or (without 420000) it is less than 0x40 below;
 * -1 when no floor remains. */
s32 func_8007BAC0(VECTOR *delta, FieldActor *actor, SVECTOR *edge, s16 heading) {
    VECTOR probe;
    VECTOR saved;
    SVECTOR floor;
    SVECTOR saved_floor;
    VECTOR d;
    VECTOR n;
    s32 attribute;
    s32 angle;
    s32 speed;

    angle = heading - 0x100;
    angle &= 0xFFF;
    probe.vx = delta->vx + (func_8003F8CC(angle) << 6);
    probe.vz = delta->vz - (func_8003F8B0(angle) << 6);
    if (func_8007BEF4(&probe, actor->position, actor, edge, &floor, -1, &attribute) == -1) {
        probe.vx = delta->vx;
        probe.vy = delta->vy;
        probe.vz = delta->vz;
        func_8007B6C4(heading, edge, &probe, attribute);
    } else {
        angle = heading + 0x100;
        angle &= 0xFFF;
        probe.vx = delta->vx + (func_8003F8CC(angle) << 6);
        probe.vz = delta->vz - (func_8003F8B0(angle) << 6);
        if (func_8007BEF4(&probe, actor->position, actor, edge, &floor, -1, &attribute) == -1) {
            probe.vx = delta->vx;
            probe.vy = delta->vy;
            probe.vz = delta->vz;
            func_8007B6C4(heading, edge, &probe, attribute);
        } else {
            angle = heading & 0xFFF;
            probe.vx = delta->vx + (func_8003F8CC(angle) << 6);
            probe.vz = delta->vz - (func_8003F8B0(angle) << 6);
            if (func_8007BEF4(&probe, actor->position, actor, edge, &floor, -1, &attribute) == -1) {
                probe.vx = delta->vx;
                probe.vy = delta->vy;
                probe.vz = delta->vz;
                func_8007B6C4(heading, edge, &probe, attribute);
            } else {
                probe.vx = delta->vx;
                probe.vy = delta->vy;
                probe.vz = delta->vz;
            }
        }
    }
    if (func_8007BEF4(&probe, actor->position, actor, edge, &floor, 0, &attribute) == -1) {
        return -1;
    }
    saved.vx = probe.vx;
    saved.vy = probe.vy;
    saved.vz = probe.vz;
    saved_floor.vx = floor.vx;
    saved_floor.vy = floor.vy;
    saved_floor.vz = floor.vz;
    if (WHOLE(actor->position[1]) > floor.vy) {
    push:
        d.vx = -probe.vx >> 8;
        d.vy = ((floor.vy << 16) - actor->position[1]) >> 8;
        d.vz = -probe.vz >> 8;
        VectorNormal(&d, &n);
        speed = func_80099A4C(probe.vx >> 8, probe.vz >> 8);
        probe.vx = -(speed * n.vx) >> 4;
        probe.vy = (speed * n.vy) >> 4;
        probe.vz = -(speed * n.vz) >> 4;
        if (func_8007BEF4(&probe, actor->position, actor, edge, &floor, 0, &attribute) == -1) {
            return -1;
        }
        actor->flags |= 0x4000000;
        goto done;
    }
    if (attribute & 0x200000) {
        goto push;
    }
    if (attribute & 0x420000) {
        if (!(actor->unk014 & 0x420000)) {
            goto restore;
        }
        goto push;
    }
    if (floor.vy < WHOLE(actor->position[1]) + 0x40) {
        goto push;
    }
restore:
    probe.vx = saved.vx;
    probe.vy = saved.vy;
    probe.vz = saved.vz;
    floor.vx = saved_floor.vx;
    floor.vy = saved_floor.vy;
    floor.vz = saved_floor.vz;
done:
    probe.vy = (floor.vy << 16) - actor->position[1];
    delta->vx = probe.vx;
    delta->vy = probe.vy;
    delta->vz = probe.vz;
    actor->unk72 = (actor->position[1] + delta->vy) >> 16;
    return 0;
}

/* The attribute-aware floor search: walk the actor's collision layer from
 * its current triangle toward `position` moved by `probe` (at most 32
 * steps), stopping at triangles whose attribute blocks the actor (its flag
 * bits 8-10 against attribute bits 5-7, or 800000 on the first layer) and
 * at ledges (attribute 400000 above the actor, unless it started on one or
 * `mode` is 0x80). Returns 0 with the floor point (and, unless `mode` is -1,
 * its plane height) in `floor`, else -1 with the crossed edge in `edge`.
 * The masked attribute of the last triangle goes to `attribute`. */
s32 func_8007BEF4(VECTOR *probe, s32 *position, FieldActor *actor, SVECTOR *edge, SVECTOR *floor, s32 mode,
                  s32 *attribute) {
    VECTOR normal;
    CollisionTriangle *triangles;
    SVECTOR *vertices;
    s32 previous;
    s32 mask;
    s32 origin;
    s32 steps;
    s32 on_ledge;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 side;
    u32 bits;
    s32 current;

    current = actor->triangle[actor->layer];
    triangles = D_800AF880.components.collision_triangles[actor->layer];
    vertices = D_800AF880.components.collision_vertices[actor->layer];
    if (current == -1) {
        return -1;
    }
    point = (((position[0] + probe->vx) >> 16) << 16) + ((position[2] + probe->vz) >> 16);
    origin = ((position[0] >> 16) << 16) + (position[2] >> 16);
    floor->vx = (position[0] + probe->vx) >> 16;
    floor->vz = (position[2] + probe->vz) >> 16;
    mask = 0;
    floor->vy = 0;
    if (!((actor->layer_flags >> (actor->layer + 3)) & 1)) {
        mask = -(D_800B2078.party_processing_mode == 0);
    }
    bits = D_800AF880.components.collision_attributes[triangles[current].attribute].word & mask;
    if ((bits & 0x400000) || mode == 0x80) {
        on_ledge = 1;
    } else {
        on_ledge = 0;
    }
    steps = 0;
    do {
        previous = current;
        a = (vertices[triangles[current].unk00[0]].vx << 16) + vertices[triangles[current].unk00[0]].vz;
        b = (vertices[triangles[current].unk00[1]].vx << 16) + vertices[triangles[current].unk00[1]].vz;
        c = (vertices[triangles[current].unk00[2]].vx << 16) + vertices[triangles[current].unk00[2]].vz;
        side = (u32)func_8004A70C(a, b, point) >> 31;
        if (func_8004A70C(b, c, point) < 0) {
            side |= 2;
        }
        if (func_8004A70C(c, a, point) < 0) {
            side |= 4;
        }
        switch (side) {
        case 0:
            steps = 0xFF;
            break;
        case 1:
            current = triangles[current].unk00[3];
            break;
        case 2:
            current = triangles[current].unk00[4];
            break;
        case 3:
            if (func_8004A70C(b, point, origin) < 0) {
                current = triangles[current].unk00[3];
                side = 1;
            } else {
                current = triangles[current].unk00[4];
                side = 2;
            }
            break;
        case 4:
            current = triangles[current].unk00[5];
            break;
        case 5:
            if (func_8004A70C(a, point, origin) >= 0) {
                current = triangles[current].unk00[3];
                side = 1;
            } else {
                current = triangles[current].unk00[5];
                side = 4;
            }
            break;
        case 6:
            if (func_8004A70C(c, point, origin) < 0) {
                current = triangles[current].unk00[4];
                side = 2;
            } else {
                current = triangles[current].unk00[5];
                side = 4;
            }
            break;
        case 7:
            current = -1;
            break;
        }
        bits = D_800AF880.components.collision_attributes[triangles[current].attribute].word & mask;
        *attribute = bits;
        if ((actor->flags >> 8) & ((bits >> 5) & 7)) {
            current = -1;
            break;
        }
        if ((bits & 0x800000) && actor->layer == 0) {
            current = -1;
            break;
        }
        if ((bits & 0x400000) && on_ledge == 0) {
            func_8007B07C(&vertices[triangles[current].unk00[0]], &vertices[triangles[current].unk00[1]],
                          &vertices[triangles[current].unk00[2]], floor, &normal);
            if (floor->vy < WHOLE(position[1])) {
                current = -1;
                break;
            }
        }
        if (current == -1) {
            break;
        }
    } while (++steps < 0x20);
    if (current != -1 && steps != 0x20) {
        if (mode == -1) {
            return 0;
        }
        func_8007B07C(&vertices[triangles[current].unk00[0]], &vertices[triangles[current].unk00[1]],
                      &vertices[triangles[current].unk00[2]], floor, &normal);
        return 0;
    }
    switch (side) {
    case 1:
        edge[0].vx = vertices[triangles[previous].unk00[0]].vx;
        edge[0].vy = vertices[triangles[previous].unk00[0]].vy;
        edge[0].vz = vertices[triangles[previous].unk00[0]].vz;
        edge[1].vx = vertices[triangles[previous].unk00[1]].vx;
        edge[1].vy = vertices[triangles[previous].unk00[1]].vy;
        edge[1].vz = vertices[triangles[previous].unk00[1]].vz;
        break;
    case 2:
        edge[0].vx = vertices[triangles[previous].unk00[1]].vx;
        edge[0].vy = vertices[triangles[previous].unk00[1]].vy;
        edge[0].vz = vertices[triangles[previous].unk00[1]].vz;
        edge[1].vx = vertices[triangles[previous].unk00[2]].vx;
        edge[1].vy = vertices[triangles[previous].unk00[2]].vy;
        edge[1].vz = vertices[triangles[previous].unk00[2]].vz;
        break;
    case 4:
        edge[0].vx = vertices[triangles[previous].unk00[2]].vx;
        edge[0].vy = vertices[triangles[previous].unk00[2]].vy;
        edge[0].vz = vertices[triangles[previous].unk00[2]].vz;
        edge[1].vx = vertices[triangles[previous].unk00[0]].vx;
        edge[1].vy = vertices[triangles[previous].unk00[0]].vy;
        edge[1].vz = vertices[triangles[previous].unk00[0]].vz;
        break;
    }
    return -1;
}

/* The floor range: `high` is `low` raised by a nonnegative extent. */
void func_8007C670(s32 *low, s32 *high, s32 extent) {
    s32 base = *low;

    if (extent >= 0) {
        *low = base;
        base += extent;
    } else {
        *low = base;
    }
    *high = base;
}

/* Walk the actor's collision layer from its current triangle to the one
 * under position + probe (X/Z, in 16.16): across the edge each normal clip
 * rejects, at most 32 steps. On arrival return 0 with the point's height in
 * `floor` (skipped for mode -1). Return -1 when the walk leaves the mesh,
 * runs out of steps or enters a triangle whose attribute the actor may not
 * cross (masked off by the actor's layer bits or 800b21cc; 800000 rejects
 * layer 0), leaving the edge last crossed in `edge`. */
s32 func_8007C694(VECTOR *probe, s32 *position, FieldActor *actor, SVECTOR *edge, SVECTOR *floor, s32 mode) {
    VECTOR normal;
    CollisionTriangle *triangles;
    SVECTOR *vertices;
    s32 triangle;
    s32 current;
    s32 point;
    u32 mask;
    s32 origin;
    s32 a;
    s32 b;
    s32 c;
    s32 side;
    u32 attribute;
    s32 steps;

    triangle = actor->triangle[actor->layer];
    triangles = D_800AF880.components.collision_triangles[actor->layer];
    vertices = D_800AF880.components.collision_vertices[actor->layer];
    if (triangle == -1) {
        return -1;
    }
    point = (((position[0] + probe->vx) >> 16) << 16) + ((position[2] + probe->vz) >> 16);
    origin = ((position[0] >> 16) << 16) + (position[2] >> 16);
    floor->vx = (position[0] + probe->vx) >> 16;
    floor->vz = (position[2] + probe->vz) >> 16;
    mask = 0;
    floor->vy = 0;
    if (!((actor->layer_flags >> (actor->layer + 3)) & 1)) {
        mask = -(D_800B2078.party_processing_mode == 0);
    }
    steps = 0;
    do {
        current = triangle;
        a = (vertices[triangles[triangle].unk00[0]].vx << 16) + vertices[triangles[triangle].unk00[0]].vz;
        b = (vertices[triangles[triangle].unk00[1]].vx << 16) + vertices[triangles[triangle].unk00[1]].vz;
        c = (vertices[triangles[triangle].unk00[2]].vx << 16) + vertices[triangles[triangle].unk00[2]].vz;
        side = (u32)func_8004A70C(a, b, point) >> 31;
        if (func_8004A70C(b, c, point) < 0) {
            side |= 2;
        }
        if (func_8004A70C(c, a, point) < 0) {
            side |= 4;
        }
        switch (side) {
        case 0:
            steps = 0xFF;
            break;
        case 1:
            triangle = triangles[triangle].unk00[3];
            break;
        case 2:
            triangle = triangles[triangle].unk00[4];
            break;
        case 3:
            if (func_8004A70C(b, point, origin) < 0) {
                triangle = triangles[triangle].unk00[3];
                side = 1;
            } else {
                triangle = triangles[triangle].unk00[4];
                side = 2;
            }
            break;
        case 4:
            triangle = triangles[triangle].unk00[5];
            break;
        case 5:
            if (func_8004A70C(a, point, origin) < 0) {
                triangle = triangles[triangle].unk00[5];
                side = 4;
            } else {
                triangle = triangles[triangle].unk00[3];
                side = 1;
            }
            break;
        case 6:
            if (func_8004A70C(c, point, origin) < 0) {
                triangle = triangles[triangle].unk00[4];
                side = 2;
            } else {
                triangle = triangles[triangle].unk00[5];
                side = 4;
            }
            break;
        case 7:
            triangle = -1;
            break;
        }
        attribute = D_800AF880.components.collision_attributes[triangles[triangle].attribute].word & mask;
        if ((actor->flags >> 9) & ((attribute >> 3) & 3)) {
            triangle = -1;
            break;
        }
        if ((actor->flags >> 8) & ((attribute >> 5) & 7)) {
            triangle = -1;
            break;
        }
        if ((attribute & 0x800000) && actor->layer == 0) {
            triangle = -1;
            break;
        }
        if (triangle == -1) {
            break;
        }
        steps++;
    } while (steps < 0x20);
    if (triangle != -1 && steps != 0x20) {
        if (mode == -1) {
            return 0;
        }
        func_8007B07C(&vertices[triangles[triangle].unk00[0]], &vertices[triangles[triangle].unk00[1]],
                      &vertices[triangles[triangle].unk00[2]], floor, &normal);
        return 0;
    }
    switch (side) {
    case 1:
        edge[0].vx = vertices[triangles[current].unk00[0]].vx;
        edge[0].vy = vertices[triangles[current].unk00[0]].vy;
        edge[0].vz = vertices[triangles[current].unk00[0]].vz;
        edge[1].vx = vertices[triangles[current].unk00[1]].vx;
        edge[1].vy = vertices[triangles[current].unk00[1]].vy;
        edge[1].vz = vertices[triangles[current].unk00[1]].vz;
        break;
    case 2:
        edge[0].vx = vertices[triangles[current].unk00[1]].vx;
        edge[0].vy = vertices[triangles[current].unk00[1]].vy;
        edge[0].vz = vertices[triangles[current].unk00[1]].vz;
        edge[1].vx = vertices[triangles[current].unk00[2]].vx;
        edge[1].vy = vertices[triangles[current].unk00[2]].vy;
        edge[1].vz = vertices[triangles[current].unk00[2]].vz;
        break;
    case 4:
        edge[0].vx = vertices[triangles[current].unk00[2]].vx;
        edge[0].vy = vertices[triangles[current].unk00[2]].vy;
        edge[0].vz = vertices[triangles[current].unk00[2]].vz;
        edge[1].vx = vertices[triangles[current].unk00[0]].vx;
        edge[1].vy = vertices[triangles[current].unk00[0]].vy;
        edge[1].vz = vertices[triangles[current].unk00[0]].vz;
        break;
    }
    return -1;
}

/* Allocate `words` words of the scratchpad. */
u32 *func_8007CD3C(s32 words) {
    u32 *p = (u32 *)0x1F800000 + D_800ADC10;

    D_800ADC10 += words;
    return p;
}

/* Release `words` words of the scratchpad. */
void func_8007CD60(s32 words) {
    D_800ADC10 -= words;
}

/* Walk the top collision layer from the camera point clamped to the view
 * bounds toward the followed point (x, z whole parts of `point`), through
 * triangles with attribute 800000, at most 0xf0 steps. Returns 0 when the
 * point is reached; otherwise -1 with the crossed edge's two vertices in
 * `edge` and the point and clamped point in `segment`. Each winding test
 * reads the previous one's result after the next GTE load and nclip. */
s32 func_8007CD80(VECTOR *point, SVECTOR *edge, DVECTOR *segment) {
    SVECTOR floor;
    VECTOR normal;
    s32 opz;
    CollisionTriangle *triangles;
    SVECTOR *vertices;
    s32 x;
    s32 z;
    s32 cx;
    s32 cz;
    s32 packed;
    s32 clamped;
    s32 a;
    s32 b;
    s32 c;
    s32 side;
    s32 steps;
    s32 current;
    s32 previous;

    triangles = D_800AF880.components.collision_triangles[D_800AF880.components.layer_count - 1];
    vertices = D_800AF880.components.collision_vertices[D_800AF880.components.layer_count - 1];
    packed = (WHOLE(point->vx) << 16) + WHOLE(point->vz);
    x = WHOLE(point->vx);
    z = WHOLE(point->vz);
    if (x < D_800AF880.bounds[0]) {
        cx = D_800AF880.bounds[0];
    } else if (D_800AF880.bounds[0] + D_800AF880.bounds[2] < x) {
        cx = D_800AF880.bounds[0] + D_800AF880.bounds[2];
    } else {
        cx = x;
    }
    if (D_800AF880.bounds[1] < z) {
        cz = D_800AF880.bounds[1];
    } else if (z < D_800AF880.bounds[1] + D_800AF880.bounds[3]) {
        cz = D_800AF880.bounds[1] + D_800AF880.bounds[3];
    } else {
        cz = z;
    }
    clamped = (cx << 16) + cz;
    current = func_8007B1C4(cx, cz, D_800AF880.components.layer_count - 1, &floor, &normal);
    steps = 0;
    for (;;) {
        previous = current;
        a = (vertices[triangles[current].unk00[0]].vx << 16) + vertices[triangles[current].unk00[0]].vz;
        b = (vertices[triangles[current].unk00[1]].vx << 16) + vertices[triangles[current].unk00[1]].vz;
        c = (vertices[triangles[current].unk00[2]].vx << 16) + vertices[triangles[current].unk00[2]].vz;
        gte_ldsxy3(a, b, packed);
        gte_nclip();
        gte_stopz(&opz);
        gte_ldsxy3(b, c, packed);
        gte_nclip();
        side = (u32)opz >> 31;
        gte_stopz(&opz);
        gte_ldsxy3(c, a, packed);
        gte_nclip();
        if (opz < 0) {
            side |= 2;
        }
        gte_stopz(&opz);
        if (opz < 0) {
            side |= 4;
        }
        switch (side) {
        case 0:
            steps = 0xFF;
            break;
        case 1:
            current = triangles[current].unk00[3];
            break;
        case 2:
            current = triangles[current].unk00[4];
            break;
        case 3:
            gte_ldsxy3(b, packed, clamped);
            gte_nclip();
            gte_stopz(&opz);
            if (opz < 0) {
                current = triangles[current].unk00[3];
                side = 1;
            } else {
                current = triangles[current].unk00[4];
                side = 2;
            }
            break;
        case 4:
            current = triangles[current].unk00[5];
            break;
        case 5:
            gte_ldsxy3(a, packed, clamped);
            gte_nclip();
            gte_stopz(&opz);
            if (opz >= 0) {
                current = triangles[current].unk00[3];
                side = 1;
            } else {
                current = triangles[current].unk00[5];
                side = 4;
            }
            break;
        case 6:
            gte_ldsxy3(c, packed, clamped);
            gte_nclip();
            gte_stopz(&opz);
            if (opz < 0) {
                current = triangles[current].unk00[4];
                side = 2;
            } else {
                current = triangles[current].unk00[5];
                side = 4;
            }
            break;
        case 7:
            current = -1;
            break;
        }
        if (!(D_800AF880.components.collision_attributes[triangles[current].attribute].word & 0x800000)) {
            current = -1;
            break;
        }
        steps++;
        if (current == -1 || steps >= 0xF0) {
            break;
        }
    }
    if (current != -1 && steps != 0xF0) {
        return 0;
    }
    switch (side) {
    case 1:
        edge[0].vx = vertices[triangles[previous].unk00[0]].vx;
        edge[0].vy = vertices[triangles[previous].unk00[0]].vy;
        edge[0].vz = vertices[triangles[previous].unk00[0]].vz;
        edge[1].vx = vertices[triangles[previous].unk00[1]].vx;
        edge[1].vy = vertices[triangles[previous].unk00[1]].vy;
        edge[1].vz = vertices[triangles[previous].unk00[1]].vz;
        break;
    case 2:
        edge[0].vx = vertices[triangles[previous].unk00[1]].vx;
        edge[0].vy = vertices[triangles[previous].unk00[1]].vy;
        edge[0].vz = vertices[triangles[previous].unk00[1]].vz;
        edge[1].vx = vertices[triangles[previous].unk00[2]].vx;
        edge[1].vy = vertices[triangles[previous].unk00[2]].vy;
        edge[1].vz = vertices[triangles[previous].unk00[2]].vz;
        break;
    case 4:
        edge[0].vx = vertices[triangles[previous].unk00[2]].vx;
        edge[0].vy = vertices[triangles[previous].unk00[2]].vy;
        edge[0].vz = vertices[triangles[previous].unk00[2]].vz;
        edge[1].vx = vertices[triangles[previous].unk00[0]].vx;
        edge[1].vy = vertices[triangles[previous].unk00[0]].vy;
        edge[1].vz = vertices[triangles[previous].unk00[0]].vz;
        break;
    }
    segment[0].vx = x;
    segment[0].vy = z;
    segment[1].vx = cx;
    segment[1].vy = cz;
    return -1;
}

/* Find the floor of collision layer `layer` under the actor's next
 * position: walk the layer's triangles from the actor's current one toward
 * the point (at most 32 steps), then take the found triangle's plane
 * height and normal, the triangle, and its floor range; attribute bit
 * 800000 blocks the layer unless it is disabled for the actor or party
 * processing runs. Returns -1 when the walk leaves the mesh. */
s32 func_8007D3D4(FieldActor *actor, s32 layer, s32 *floor, VECTOR *normal, s16 *triangle, s32 *upper) {
    SVECTOR query;
    CollisionTriangle *triangles;
    SVECTOR *vertices;
    s32 mask;
    s32 origin;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 side;
    s32 steps;
    s32 bump;
    s32 current;

    triangles = D_800AF880.components.collision_triangles[layer];
    current = actor->triangle[layer];
    vertices = D_800AF880.components.collision_vertices[layer];
    if (current != -1) {
        point = (((actor->position[0] + actor->unk030[0]) >> 16) << 16) +
                ((actor->position[2] + actor->unk030[2]) >> 16);
        origin = ((actor->position[0] >> 16) << 16) + (actor->position[2] >> 16);
        query.vx = (actor->position[0] + actor->unk030[0]) >> 16;
        query.vz = (actor->position[2] + actor->unk030[2]) >> 16;
        mask = 0;
        query.vy = 0;
        if (!((actor->layer_flags >> (layer + 3)) & 1)) {
            mask = -(D_800B2078.party_processing_mode == 0);
        }
        steps = 0;
        do {
            a = (vertices[triangles[current].unk00[0]].vx << 16) + vertices[triangles[current].unk00[0]].vz;
            b = (vertices[triangles[current].unk00[1]].vx << 16) + vertices[triangles[current].unk00[1]].vz;
            c = (vertices[triangles[current].unk00[2]].vx << 16) + vertices[triangles[current].unk00[2]].vz;
            side = (u32)func_8004A70C(a, b, point) >> 31;
            if (func_8004A70C(b, c, point) < 0) {
                side |= 2;
            }
            if (func_8004A70C(c, a, point) < 0) {
                side |= 4;
            }
            switch (side) {
            case 0:
                steps = 0xFF;
                break;
            case 3:
                if (func_8004A70C(b, point, origin) < 0) {
                    current = triangles[current].unk00[3];
                } else {
                    current = triangles[current].unk00[4];
                }
                break;
            case 5:
                if (func_8004A70C(a, point, origin) < 0) {
                    current = triangles[current].unk00[5];
                    break;
                }
                /* fallthrough */
            case 1:
                current = triangles[current].unk00[3];
                break;
            case 6:
                if (func_8004A70C(c, point, origin) >= 0) {
                    current = triangles[current].unk00[5];
                    break;
                }
                /* fallthrough */
            case 2:
                current = triangles[current].unk00[4];
                break;
            case 4:
                current = triangles[current].unk00[5];
                break;
            case 7:
                current = -1;
                break;
            }
            steps++;
        } while (current != -1 && steps < 0x20);
        if (current != -1 && steps != 0x20) {
            func_8007B07C(&vertices[triangles[current].unk00[0]], &vertices[triangles[current].unk00[1]],
                          &vertices[triangles[current].unk00[2]], &query, normal);
            *triangle = current;
            bump = (s8)triangles[current].unk0D * 4;
            if (bump < 0) {
                bump = 0;
            }
            if (actor->layer != layer) {
                if (!((D_800AF880.components.collision_attributes[triangles[current].attribute].word & 0x800000) &
                      mask)) {
                    *floor = query.vy;
                    func_8007C670(floor, upper, bump);
                } else {
                    *floor = 0x7FFFFFFF;
                    *upper = 0x7FFFFFFF;
                }
            } else if ((D_800AF880.components.collision_attributes[triangles[current].attribute].word & 0x800000) &
                       mask) {
                *floor = 0x7FFFFFFF;
                *upper = 0x7FFFFFFF;
            } else if (actor->unk030[0] == 0 && actor->unk030[1] == 0 && actor->unk030[2] == 0) {
                *floor = query.vy;
                func_8007C670(floor, upper, bump);
            } else {
                *floor = actor->unk72;
                func_8007C670(floor, upper, bump);
            }
            return 0;
        }
    }
    return -1;
}

/* Normalise a 20.12 vector, pointing it along its largest component. */
void func_8007D818(VECTOR *v, VECTOR *out) {
    s32 largest = func_8007D8B4(v->vx, v->vy, v->vz);

    v->vx >>= 12;
    v->vy >>= 12;
    v->vz >>= 12;
    if (largest < 0) {
        v->vx = -v->vx;
        v->vy = -v->vy;
        v->vz = -v->vz;
    }
    VectorNormal(v, out);
}

/* The component of largest magnitude (0 when none is strictly ahead). */
s32 func_8007D8B4(s32 x, s32 y, s32 z) {
    s32 ax = x;
    s32 ay = y;
    s32 az = z;

    if (ax < 0) {
        ax = -ax;
    }
    if (ay < 0) {
        ay = -ay;
    }
    if (az < 0) {
        az = -az;
    }
    if (ax >= ay && ax >= az) {
        return x;
    }
    if (ay >= ax && ay >= az) {
        return y;
    }
    if (az >= ax && az >= ay) {
        return z;
    }
    return 0;
}

/* Prepare a fade channel: a full-screen semi-transparent tile in both
 * buffers, inactive, levels zero, additive blend. */
void func_8007D93C(s32 channel) {
    SetTile(&D_800B2078.fades[channel].tiles[0]);
    SetSemiTrans(&D_800B2078.fades[channel].tiles[0], 1);
    D_800B2078.fades[channel].tiles[0].w = 0x140;
    D_800B2078.fades[channel].tiles[0].h = 0xE0;
    D_800B2078.fades[channel].tiles[0].x0 = 0;
    D_800B2078.fades[channel].tiles[0].y0 = 0;
    D_800B2078.fades[channel].tiles[1] = D_800B2078.fades[channel].tiles[0];
    D_800B2078.fades[channel].active = 0;
    D_800B2078.fades[channel].steps = 0;
    D_800B2078.fades[channel].level[0] = D_800B2078.fades[channel].level[1] = D_800B2078.fades[channel].level[2] = 0;
    D_800B2078.fades[channel].abr = 2;
}

#ifdef NON_MATCHING
/* Link each active fade channel's tile and draw mode into `ot` (channel 1
 * one entry further); a channel whose levels reached zero stops.
 * NON_MATCHING (measured 118 instruction edits; ours 604 bytes, original
 * 692): the original strength-reduces more of the loop (the mode and tile
 * offsets buffer*12/16 + i*0x58 and the tile base as separate induction
 * values, spilled with ot, the fade base and the 0xff000000 mask to a
 * 0x78-byte frame), computes ot + (i == 1) with a store-flag instead of a
 * branch, and loads abr with lh. Passing the tile/mode expressions to
 * addPrim directly reproduces the spills but not the tile colour stores. */
void func_8007DA44(u32 *ot, s32 buffer) {
    DR_MODE *mode;
    TILE *tile;
    u32 *entry;
    s32 i;

    for (i = 0; i < 2; i++) {
        if (D_800B2078.fades[i].active != 0) {
            D_800AFE3C[i].w = 0x140;
            D_800AFE3C[i].x = 0;
            D_800AFE3C[i].y = 0;
            D_800AFE3C[i].h = 0xE0;
            mode = &D_800B2078.fades[i].modes[buffer];
            SetDrawMode(mode, 0, 0, GetTPage(0, D_800B2078.fades[i].abr, 0, 0), &D_800AFE3C[i]);
            tile = &D_800B2078.fades[i].tiles[buffer];
            tile->r0 = D_800B2078.fades[i].level[0] >> 8;
            tile->g0 = D_800B2078.fades[i].level[1] >> 8;
            tile->b0 = D_800B2078.fades[i].level[2] >> 8;
            entry = &ot[i == 1];
            addPrim(entry, tile);
            addPrim(entry, mode);
            if (D_800B2078.fades[i].level[0] >> 8 == 0 && D_800B2078.fades[i].level[1] >> 8 == 0
                && D_800B2078.fades[i].level[2] >> 8 == 0) {
                D_800B2078.fades[i].active = 0;
                D_800B2078.fades[i].steps = 0;
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_8007A44C", func_8007DA44);
#endif


/* Move a displayed window's choice (+382 over +380 lines) with the pad and
 * light its line; a window with +410 set lights none. */
void func_8007DCF8(s32 window, u32 *ot, s32 buffer) {
    if (D_800C2698[window].status == 0 && D_800C2698[window].timer == 0) {
        if (D_800C2698[window].age == 0) {
            if (D_800C3900 & 0x4000) {
                D_800C2698[window].unk382++;
                if (D_800C2698[window].unk380 - 1 < D_800C2698[window].unk382) {
                    D_800C2698[window].unk382 = 0;
                }
            }
            if (D_800C3900 & 0x1000) {
                D_800C2698[window].unk382--;
                if (D_800C2698[window].unk382 < 0) {
                    D_800C2698[window].unk382 = D_800C2698[window].unk380 - 1;
                }
            }
            func_80034874(&D_800C2698[window].text, D_800C2698[window].unk382 + D_800C2698[window].unk37E);
        } else {
            func_8003487C(&D_800C2698[window].text);
        }
    }
}

/* Reset the sixteen text texture windows and draw modes, and the four
 * dialogue windows to free. */
void func_8007DECC(void) {
    RECT area;
    RECT *window;
    s32 i;

    area.y = 0;
    area.x = 0;
    area.h = 0xFF;
    area.w = 0xFF;
    for (i = 0; i < 16; i++) {
        window = &D_800AFC80[i];
        window->y = 0;
        D_800AFC80[i].x = 0;
        window->h = 0xFF;
        D_800AFC80[i].w = 0xFF;
        window->y = 0;
        D_800AFC80[i].x = 0;
        window->h = 0xFF;
        D_800AFC80[i].w = 0xFF;
        SetDrawMode(&D_800B1DF4[0][i], 0, 0, GetTPage(0, 0, 0x380, 0x100), window);
        SetDrawMode(&D_800B1DF4[1][i], 0, 0, GetTPage(0, 0, 0x380, 0x100), window);
    }
    for (i = 0; i < 4; i++) {
        D_800C2698[i].owner = 0xFF;
        D_800C2698[i].unk418 = 0xFF;
        D_800C2698[i].status = -1;
        D_800C2698[i].unk3C4 = -1;
        D_800C2698[i].busy = -1;
        D_800C2698[i].cleared = -1;
        D_800C2698[i].age = 0xFFFF;
        D_800C2698[i].owner = 0xFF;
        D_800C2698[i].unk412 = 0;
        func_8007EE0C(i);
        D_800B068C[i] = -1;
        SetDrawMode(&D_800C2698[i].modes[0], 0, 0, GetTPage(0, 0, 0x300, 0x100), &area);
        SetDrawMode(&D_800C2698[i].modes[1], 0, 0, GetTPage(0, 0, 0x300, 0x100), &area);
    }
}

/* Set a dialogue window's rectangle. */
void func_8007E114(s32 window, s32 x, s32 y, s32 w, s32 h) {
    D_800C2698[window].text.rect.x = x;
    D_800C2698[window].text.rect.y = y;
    D_800C2698[window].text.rect.w = w;
    D_800C2698[window].text.rect.h = h;
}

/* Place a quad's corners at (x, y) with size (w, h), optionally mirrored. */
void func_8007E16C(POLY_FT4 *poly, s32 x, s32 y, s32 w, s32 h, s32 mirror) {
    if (mirror == 0) {
        x--;
        poly->x0 = x;
        poly->x1 = x + w;
        poly->x2 = x;
        poly->x3 = x + w;
    } else {
        poly->x1 = x;
        poly->x0 = x + w;
        poly->x3 = x;
        poly->x2 = x + w;
    }
    poly->y0 = y;
    poly->y1 = y;
    poly->y2 = y + h;
    poly->y3 = y + h;
}

extern RECT D_800ADEDC[];   /* choice cursor frames */
extern RECT D_800ADF04[];   /* prompt frames */
s32 func_800347AC(TextBox *text);
s32 func_800347C0(TextBox *text);

#ifdef NON_MATCHING
/* Draw dialogue window `w`'s frame into `ot` for `buffer`: while opening
 * the window grows from its centre (at least 16 pixels each way) and
 * slides; then the waiting prompt, the eight border pieces, the portrait,
 * the choice cursor and the backing tile.
 * Does not match yet: the original spills `buffer` and the area to the
 * stack (0xa8-byte frame) and keeps `ot` in fp; the layout of the
 * statements is otherwise as here. */
void func_8007E1C0(u32 *ot, s32 buffer, s32 w) {
    RECT area;
    s32 x;
    s32 y;
    s32 width;
    s32 height;
    s32 grown_w;
    s32 grown_h;
    s32 steps;
    s32 left;
    s32 top;
    s32 right;
    s32 bottom;
    s32 side;
    s32 icon_x;
    s32 icon_w;
    s32 icon_h;
    s32 i;

    if (D_800C2698[w].busy != 0) {
        return;
    }
    x = D_800C2698[w].text.rect.x;
    y = D_800C2698[w].text.rect.y;
    width = D_800C2698[w].text.rect.w;
    height = D_800C2698[w].text.rect.h;
    if (D_800C2698[w].timer != 0) {
        steps = D_800B2078.text_speed * 2;
        grown_w = ((width << 16) / steps) * (D_800B2078.text_speed - D_800C2698[w].timer);
        grown_h = ((height << 16) / steps) * (D_800B2078.text_speed - D_800C2698[w].timer);
        x = x + width / 2 - (grown_w >> 16);
        width = (grown_w * 2) >> 16;
        y = y + height / 2 - (grown_h >> 16);
        height = (grown_h * 2) >> 16;
        if (width < 0x10) {
            x -= (0x10 - width) / 2;
            width = 0x10;
        }
        if (height < 0x10) {
            y -= (0x10 - height) / 2;
            height = 0x10;
        }
        D_800C2698[w].slide[0].value += D_800C2698[w].slide_step[0];
        D_800C2698[w].slide[1].value += D_800C2698[w].slide_step[1];
        y += D_800C2698[w].slide[1].value >> 16;
        x += D_800C2698[w].slide[0].s.whole;
    }
    if (D_800C2698[w].unk3C4 == 0 && D_800C2698[w].age == 0 && D_800C2698[w].timer == 0
        && !(D_800C2698[w].style & 0x40) && D_800C2698[w].status != 0) {
        if (D_800C2698[w].prompt_delay == 0) {
            left = func_800347AC(&D_800C2698[w].text);
            top = func_800347C0(&D_800C2698[w].text);
            area = D_800ADF04[D_800ADE94];
            SetDrawMode(&D_800C2698[w].prompt_modes[buffer], 0, 0, GetTPage(0, 0, 0x298, 0x1C0), &area);
            D_800C2698[w].prompt[buffer].x0 = left;
            D_800C2698[w].prompt[buffer].y0 = top + 4;
            addPrim(ot, &D_800C2698[w].prompt[buffer]);
            addPrim(ot, &D_800C2698[w].prompt_modes[buffer]);
        } else {
            D_800C2698[w].prompt_delay--;
        }
    } else {
        D_800C2698[w].prompt_delay = 2;
    }
    left = x - 8;
    top = y - 7;
    right = x + width - 8;
    bottom = y + height - 9;
    side = height - 0x12;
    D_800C2698[w].border[buffer][0].x0 = left;
    D_800C2698[w].border[buffer][0].y0 = top;
    D_800C2698[w].border[buffer][2].y0 = top;
    D_800C2698[w].border[buffer][4].x0 = left;
    D_800C2698[w].border[buffer][2].x0 = right;
    D_800C2698[w].border[buffer][6].x0 = right;
    D_800C2698[w].border[buffer][3].x0 = left;
    D_800C2698[w].border[buffer][1].x0 = right;
    D_800C2698[w].border[buffer][4].y0 = bottom;
    D_800C2698[w].border[buffer][6].y0 = bottom;
    D_800C2698[w].border[buffer][3].y0 = y + 9;
    D_800C2698[w].border[buffer][1].y0 = y + 9;
    if (side < 0) {
        side = 0;
    }
    D_800C2698[w].border[buffer][3].h = side;
    D_800C2698[w].border[buffer][1].h = side;
    D_800C2698[w].border[buffer][5].y0 = top;
    D_800C2698[w].border[buffer][5].x0 = x + 8;
    D_800C2698[w].border[buffer][7].x0 = x + 8;
    D_800C2698[w].border[buffer][7].y0 = bottom;
    D_800C2698[w].border[buffer][5].w = width - 0x10;
    D_800C2698[w].border[buffer][7].w = width - 0x10;
    if (!(D_800C2698[w].style & 0x40)) {
        for (i = 0; i < 8; i++) {
            addPrim(ot, &D_800C2698[w].border[buffer][i]);
            addPrim(ot, &D_800C2698[w].border_modes[buffer][i]);
        }
    }
    icon_w = 0x40;
    if (width - 4 < 0x40) {
        icon_w = width - 8;
    }
    icon_h = 0x40;
    if (height - 4 < 0x40) {
        icon_h = height - 8;
    }
    icon_x = x + 4;
    if (D_800C2698[w].style & 0x20) {
        icon_x = x + width - icon_w - 4;
    }
    func_8007E16C(&D_800C2698[w].icon[buffer], icon_x, y + 4, icon_w, icon_h, D_800C2698[w].style & 0x20);
    if (D_800C2698[w].unk494 == 1) {
        addPrim(ot, &D_800C2698[w].icon[buffer]);
        addPrim(ot, &D_800C2698[w].icon_modes[buffer]);
    }
    if (D_800C2698[w].status == 0 && D_800C2698[w].age == 0 && D_800C2698[w].timer == 0) {
        if (D_800C2698[w].unk494 == 1 && !(D_800C2698[w].style & 0x20)) {
            D_800C2698[w].choice[buffer].x0 = x + 0x5A;
        } else {
            D_800C2698[w].choice[buffer].x0 = x + 0x16;
        }
        D_800C2698[w].choice[buffer].y0 = (D_800C2698[w].unk382 + D_800C2698[w].unk37E) * 0xE + y + 8;
        area = D_800ADEDC[D_800ADE94];
        SetDrawMode(&D_800C2698[w].choice_modes[buffer], 0, 0, GetTPage(0, 0, 0x288, 0x1C0), &area);
        addPrim(ot, &D_800C2698[w].choice[buffer]);
        addPrim(ot, &D_800C2698[w].choice_modes[buffer]);
    }
    D_800C2698[w].back[buffer].x0 = x;
    D_800C2698[w].back[buffer].y0 = y + 1;
    D_800C2698[w].back[buffer].w = width;
    D_800C2698[w].back[buffer].h = height - 2;
    if (!(D_800C2698[w].style & 0x40)) {
        addPrim(ot, &D_800C2698[w].back[buffer]);
        addPrim(ot, &D_800C2698[w].back_modes[buffer]);
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_8007A44C", func_8007E1C0);
#endif

extern u8 D_800594D4[3];    /* window colour */
extern RECT D_800ADE9C[8];  /* border piece texture windows */
extern RECT D_800ADEDC[];   /* choice cursor frames */
extern RECT D_800ADF04[];   /* prompt frames */

#ifdef NON_MATCHING
/* Build dialogue window `w`'s packets for both buffers: the backing draw
 * mode and semi-transparent tile in the window colour, the prompt and
 * choice cursor sprites, the eight border sprites (texture windows from
 * 800ade9c) and the portrait quad.
 * Does not match: the original addresses these members from a base at
 * window + 0ac (800c2744) and then from the border sprites (800c2884), where
 * this code shares window + 0c4; the source form giving that base is not
 * known yet. */
void func_8007EE0C(s32 w) {
    RECT area;
    s32 i;

    SetDrawMode(&D_800C2698[w].back_modes[0], 0, 0, GetTPage(0, 2, 0x280, 0x1F0), NULL);
    SetDrawMode(&D_800C2698[w].back_modes[1], 0, 0, GetTPage(0, 2, 0x280, 0x1F0), NULL);
    SetTile(&D_800C2698[w].back[0]);
    D_800C2698[w].back[0].r0 = D_800594D4[0];
    D_800C2698[w].back[0].g0 = D_800594D4[1];
    D_800C2698[w].back[0].b0 = D_800594D4[2];
    SetSemiTrans(&D_800C2698[w].back[0], 1);
    D_800C2698[w].back[1] = D_800C2698[w].back[0];
    area = D_800ADF04[0];
    SetDrawMode(&D_800C2698[w].prompt_modes[0], 0, 0, GetTPage(0, 0, 0x298, 0x1C0), &area);
    SetDrawMode(&D_800C2698[w].prompt_modes[1], 0, 0, GetTPage(0, 0, 0x298, 0x1C0), &area);
    SetSprt(&D_800C2698[w].prompt[0]);
    D_800C2698[w].prompt[0].r0 = 0x80;
    D_800C2698[w].prompt[0].g0 = 0x80;
    D_800C2698[w].prompt[0].b0 = 0x80;
    D_800C2698[w].prompt[0].clut = GetClut(0x100, 0xF6);
    D_800C2698[w].prompt[0].w = 0xC;
    D_800C2698[w].prompt[0].u0 = 0x80;
    D_800C2698[w].prompt[0].v0 = 0xC0;
    D_800C2698[w].prompt[0].h = 8;
    D_800C2698[w].prompt[0].x0 = 0;
    D_800C2698[w].prompt[0].y0 = 0;
    D_800C2698[w].prompt[1] = D_800C2698[w].prompt[0];
    area = D_800ADEDC[0];
    SetDrawMode(&D_800C2698[w].choice_modes[0], 0, 0, GetTPage(0, 0, 0x288, 0x1C0), &area);
    SetDrawMode(&D_800C2698[w].choice_modes[1], 0, 0, GetTPage(0, 0, 0x288, 0x1C0), &area);
    SetSprt(&D_800C2698[w].choice[0]);
    D_800C2698[w].choice[0].r0 = 0x80;
    D_800C2698[w].choice[0].g0 = 0x80;
    D_800C2698[w].choice[0].b0 = 0x80;
    D_800C2698[w].choice[0].clut = GetClut(0x100, 0xF6);
    D_800C2698[w].choice[0].w = 0xC;
    D_800C2698[w].choice[0].u0 = 0x80;
    D_800C2698[w].choice[0].v0 = 0xC0;
    D_800C2698[w].choice[0].h = 8;
    D_800C2698[w].choice[0].x0 = 0;
    D_800C2698[w].choice[0].y0 = 0;
    D_800C2698[w].choice[1] = D_800C2698[w].choice[0];
    D_800C2698[w].prompt_delay = 2;
    for (i = 0; i < 8; i++) {
        area = D_800ADE9C[i];
        SetDrawMode(&D_800C2698[w].border_modes[0][i], 0, 0, GetTPage(0, 2, 0x280, 0x1F0), &area);
        SetDrawMode(&D_800C2698[w].border_modes[1][i], 0, 0, GetTPage(0, 2, 0x280, 0x1F0), &area);
        SetSprt(&D_800C2698[w].border[0][i]);
        D_800C2698[w].border[0][i].r0 = 0x80;
        D_800C2698[w].border[0][i].g0 = 0x80;
        D_800C2698[w].border[0][i].b0 = 0x80;
        D_800C2698[w].border[0][i].clut = GetClut(0x100, 0xF4);
        SetSemiTrans(&D_800C2698[w].border[0][i], 1);
        D_800C2698[w].border[0][i].u0 = 0x80;
        D_800C2698[w].border[0][i].v0 = 0xC0;
        D_800C2698[w].border[0][i].w = D_800ADE9C[i].w;
        D_800C2698[w].border[0][i].x0 = 0;
        D_800C2698[w].border[0][i].y0 = 0;
        D_800C2698[w].border[0][i].h = D_800ADE9C[i].h;
        D_800C2698[w].border[1][i] = D_800C2698[w].border[0][i];
    }
    area.x = 0;
    area.y = 0;
    area.w = 0xFF;
    area.h = 0xFF;
    SetDrawMode(&D_800C2698[w].icon_modes[0], 0, 0, GetTPage(1, 0, 0x2C0, 0x100), &area);
    SetDrawMode(&D_800C2698[w].icon_modes[1], 0, 0, GetTPage(1, 0, 0x2C0, 0x100), &area);
    SetPolyFT4(&D_800C2698[w].icon[0]);
    D_800C2698[w].icon[0].r0 = 0x80;
    D_800C2698[w].icon[0].g0 = 0x80;
    D_800C2698[w].icon[0].b0 = 0x80;
    D_800C2698[w].icon[0].clut = GetClut(0, 0xE0);
    D_800C2698[w].icon[0].tpage = GetTPage(1, 0, 0x2C0, 0x100);
    D_800C2698[w].icon[1] = D_800C2698[w].icon[0];
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_8007A44C", func_8007EE0C);
#endif

extern DVECTOR D_800ADF34[]; /* icon texture origin per frame */

/* Point both buffers' icon of `window` at frame `frame`: a 64x64 texture
 * square and the frame's CLUT row. */
void func_8007F5AC(s32 window, s32 frame) {
    DialogueWindow *w;
    DVECTOR *tex;
    s16 *u;
    s16 *v;

    w = &D_800C2698[window];
    tex = D_800ADF34;
    u = &tex[frame].vx;
    v = &D_800ADF34[frame].vy;
    D_800C2698[window].icon[1].u0 = w->icon[0].u0 = *u;
    D_800C2698[window].icon[1].v0 = w->icon[0].v0 = *v;
    D_800C2698[window].icon[1].u1 = w->icon[0].u1 = *u + 0x40;
    D_800C2698[window].icon[1].v1 = w->icon[0].v1 = *v;
    D_800C2698[window].icon[1].u2 = w->icon[0].u2 = *u;
    D_800C2698[window].icon[1].v2 = w->icon[0].v2 = *v + 0x40;
    D_800C2698[window].icon[1].u3 = w->icon[0].u3 = *u + 0x40;
    D_800C2698[window].icon[1].v3 = w->icon[0].v3 = *v + 0x40;
    D_800C2698[window].icon[1].clut = w->icon[0].clut = GetClut(0, frame + 0xE0);
}

/* Close dialogue window `window` unless it is busy; -1 when busy. */
s32 func_8007F6F8(s16 window) {
    if (D_800C2698[window].busy == 0) {
        func_80034614(&D_800C2698[window].text);
        func_800345E0(&D_800C2698[window].text);
        func_800346D4(&D_800C2698[window].text);
        D_800C2698[window].status = -1;
        D_800C2698[window].busy = -1;
        D_800C2698[window].cleared = -1;
        D_800C2698[window].age = 0xFFFF;
        D_800B068C[window] = -1;
        D_800B2078.open_windows &= (1 << window) ^ 0xFF;
        D_800C2698[window].owner = 0xFF;
        D_800C2698[window].unk412 = 0;
        return 0;
    }
    return -1;
}

/* The screen position of a point `height` above descriptor `index`. */
void func_8007F814(s32 index, s32 *x, s32 *y, s32 height) {
    SVECTOR point;
    MATRIX m;
    s32 screen;
    s32 depth;
    s32 flag;

    CompMatrix(&D_800AF880.scaled_world, &D_800AF880.components.descriptors[index].matrix, &m);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    point.vx = 0;
    point.vy = height;
    point.vz = 0;
    RotTransPers(&point, &screen, &depth, &flag);
    *y = screen >> 16;
    *x = (s16)screen;
}

extern s32 D_800ADE90;         /* next message slot to try */
extern u16 D_800ADF54[4][2];   /* text VRAM position per window */
void func_80032F54(TextBox *text, s32 vram_x, s32 vram_y, s32 x, s32 y, s32 columns, s32 rows);
s32 func_80033728(void *messages, void *message);

#ifdef NON_MATCHING
/* Open dialogue window `w` for `message` at (x, y) with `columns` x `rows`
 * characters for actor `owner`, spoken by `speaker`: take a message slot,
 * keep event variables 16-1c, find where the window flies in from (mode 2:
 * the screen top, 3: its own centre, else above the speaker), show the
 * owner's portrait unless disabled, set up the text and the opening slide.
 * Returns -1 (clearing the window's +414) when the speaker has layer flag
 * 0x200 and the style lacks bit 1, else 0.
 * NON_MATCHING: every instruction matches except the frame size: the
 * original's frame is 0x70 (ours 0x68). Its extra 8 bytes sit above the
 * x/y/message spill slots (0x28/0x30/0x38) and are never accessed, so they
 * look like a fourth spill slot that reload allocated and then left unused;
 * a local declared in C would land below the spill slots instead. */
s32 func_8007F8DC(s16 x, s16 y, void *message, s32 w, s32 columns, s32 rows, s32 owner, s32 speaker,
                  s32 mode, s32 turned, s32 flags) {
    s32 target_x;
    s32 target_y;
    u32 progress;
    s32 style;
    s32 slot;
    s32 extra;
    s32 i;

    y -= 8;
    progress = D_800AF880.components.descriptors[owner].actor->unk84;
    if ((progress >> 16) == 0) {
        style = (u16)progress;
    } else {
        style = (u16)(progress >> 16);
    }
    style |= turned;
    for (i = 0; i < 4; i++) {
        slot = D_800ADE90 & 3;
        D_800ADE90++;
        if (D_800B068C[slot] == -1) {
            D_800B068C[slot] = 0;
            break;
        }
    }
    slot = w;
    D_800C2698[w].text.vars[0] = func_800A3018(0x16);
    D_800C2698[w].text.vars[1] = func_800A3018(0x18);
    D_800C2698[w].text.vars[2] = func_800A3018(0x1A);
    D_800C2698[w].text.vars[3] = func_800A3018(0x1C);
    D_800C2698[w].text.unk80 = D_800C2698[w].text.vars[3];
    switch (mode) {
    case 2:
        target_x = 0xA0;
        target_y = y + 0x20;
        break;
    case 3:
        target_x = x + (columns * 2 + 8);
        target_y = y + (rows * 7 + 8);
        break;
    default:
        func_8007F814(speaker, &target_x, &target_y, -0x40);
        break;
    }
    if (D_800AF880.components.descriptors[owner].actor->character != 0xFF && !(style & 2)) {
        if (!(style & 0x402)) {
            func_8007F5AC(w, ((D_800AF880.components.descriptors[owner].actor->state.word >> 1) & 0xE) | 1);
        } else {
            func_8007F5AC(w, (D_800AF880.components.descriptors[owner].actor->state.word >> 1) & 0xE);
        }
        D_800C2698[w].unk495 = D_800AF880.components.descriptors[owner].actor->character;
        D_800C2698[w].unk494 = 1;
    } else {
        D_800C2698[w].unk495 = 0x80;
        D_800C2698[w].unk494 = 0;
    }
    D_800C2698[w].status = -1;
    func_8007E114(w, x, y, columns * 4 + 0x10, rows * 14 + 0x10);
    extra = 0;
    if (D_800AF880.components.descriptors[owner].actor->character != 0xFF) {
        extra = (style & 0x402) == 0 ? 0x44 : 0;
    }
    func_80032F54(&D_800C2698[w].text, D_800ADF54[slot][0], D_800ADF54[slot][1], x + 8 + extra, y + 8, columns,
                  rows);
    if (style & 0x400) {
        D_800C2698[w].style |= 0x20;
    }
    if (D_800B2078.text_speed == 8) {
        D_800C2698[w].text.speed = 1;
    } else {
        D_800C2698[w].text.speed = 2;
    }
    D_800C2698[w].text.unk90 = func_80033728(D_800ADBF0, message);
    D_800C2698[w].busy = 0;
    D_800C2698[w].text.flags |= 2;
    D_800C2698[w].timer = D_800B2078.text_speed;
    D_800C2698[w].owner = owner;
    D_800C2698[w].unk418 = speaker;
    if (!(flags & 0x800)) {
        D_800C2698[w].unk412 = 0;
    } else {
        D_800C2698[w].unk412 = 1;
    }
    columns = columns * 2 + 8;
    rows = rows * 7 + 8;
    D_800C2698[w].slide[0].value = (target_x - columns - x) << 16;
    D_800C2698[w].slide[1].value = (target_y - rows - y) << 16;
    if (!(style & 0x100)) {
        D_800C2698[w].slide_step[0] = -(D_800C2698[w].slide[0].value / D_800B2078.text_speed);
        D_800C2698[w].slide_step[1] = -(D_800C2698[w].slide[1].value / D_800B2078.text_speed);
    } else {
        D_800C2698[w].timer = 1;
        D_800C2698[w].slide_step[0] = -D_800C2698[w].slide[0].value;
        D_800C2698[w].slide_step[1] = -D_800C2698[w].slide[1].value;
    }
    if ((D_800AF880.components.descriptors[speaker].actor->layer_flags & 0x200) && !(style & 1)) {
        D_800C2698[w].cleared = 0;
        return -1;
    }
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_8007A44C", func_8007F8DC);
#endif

/* Close every dialogue window that is not busy. */
void func_8007FFE8(void) {
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].busy == 0) {
            func_8007F6F8(window);
        }
    }
}

/* Draw the dialogue windows: advance the cursor animation, draw the
 * selected window (+412) first and the others by rank (+410), renumber the
 * ranks, then link the frame's text draw mode into `ot`. */
void func_8008004C(u32 *ot, s32 buffer) {
    s32 order[4];
    s32 next;
    s32 selected;
    s32 rank;
    s32 i;
    TextBox *text;

    if (!(++D_800ADE98 & 3)) {
        D_800ADE94++;
    }
    if (D_800ADE94 >= 5) {
        D_800ADE94 = 0;
    }
    selected = 0xFF;
    for (i = 0; i < 4; i++) {
        if (D_800C2698[i].unk412 != 0) {
            selected = i;
        }
    }
    next = 0;
    for (i = 0; i < 4; i++) {
        order[i] = 0xFFFF;
    }
    for (i = 0; i < 4; i++) {
        if (D_800C2698[i].busy == 0 && D_800C2698[i].unk412 != 0) {
            text = &D_800C2698[i].text;
            D_800C2698[i].unk3C4 = -1;
            if (D_800C2698[i].timer == 0) {
                if (func_80033CD0(text) != 0 && D_800C2698[i].status != 0) {
                    D_800C2698[i].unk3C4 = 0;
                }
                if (D_800C2694 & 0x20) {
                    D_800C2698[i].status = -1;
                    D_800AF880.components.descriptors[D_800C2698[i].owner].actor->unk081 =
                        D_800C2698[i].unk382 + D_800C2698[i].unk37E;
                    func_800345E0(text);
                }
                if (text->unk82 == 0) {
                    func_80034714(text, D_800C2698[i].text.unk90);
                }
                func_80034888(text, ot, buffer);
            }
            addPrim(ot, &D_800C2698[i].modes[buffer]);
            func_8007E1C0(ot, buffer, i);
            func_8007DCF8(i, ot, buffer);
        }
    }
    for (rank = 0; rank < 4; rank++) {
        for (i = 0; i < 4; i++) {
            if (D_800C2698[i].age == rank) {
                order[i] = next++;
                if (D_800C2698[i].busy == 0 && i != selected) {
                    D_800C2698[i].unk3C4 = -1;
                    text = &D_800C2698[i].text;
                    if (D_800C2698[i].timer == 0) {
                        if ((D_800C2694 & 0x20) && D_800C2698[i].age == 0) {
                            D_800C2698[i].status = -1;
                            D_800AF880.components.descriptors[D_800C2698[i].owner].actor->unk081 =
                                D_800C2698[i].unk382 + D_800C2698[i].unk37E;
                            func_800345E0(text);
                        }
                        if (text->unk82 == 0) {
                            func_80034714(text, D_800C2698[i].text.unk90);
                        }
                        func_80034888(text, ot, buffer);
                        if (func_80033CD0(text) != 0 && D_800C2698[i].status != 0) {
                            D_800C2698[i].unk3C4 = 0;
                        }
                    }
                    addPrim(ot, &D_800C2698[i].modes[buffer]);
                    func_8007E1C0(ot, buffer, i);
                    func_8007DCF8(i, ot, buffer);
                }
                if (D_800C2698[i].age == 0xFFFF) {
                    order[i] = 0xFFFF;
                }
            }
        }
    }
    for (rank = 0; rank < 4; rank++) {
        D_800C2698[rank].age = order[rank];
    }
    addPrim(ot, &D_800B1DF4[D_800ADB08][0]);
}

/* Close each idle dialogue window whose timer ran out (unless flag 4 keeps
 * it) or that was cleared, and count the timers down. */
void func_800805F4(void) {
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].busy == 0) {
            if (D_800C2698[window].timer == 0 && !(D_800C2698[window].text.flags & 4)) {
                func_8007F6F8(window);
            }
            if (D_800C2698[window].cleared == 0) {
                func_8007F6F8(window);
            }
            if (D_800C2698[window].timer != 0) {
                D_800C2698[window].timer--;
            }
        }
    }
}

/* The first dialogue window whose age is zero, or 0xffff. */
s32 func_800806E4(void) {
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].age == 0) {
            return window;
        }
    }
    return 0xFFFF;
}

/* 0 when a dialogue window is free, else -1. */
s32 func_80080720(void) {
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].age == 0xFFFF) {
            return 0;
        }
    }
    return -1;
}

/* The oldest dialogue window in use, or 0xffff. */
s32 func_80080760(void) {
    s32 oldest_age = 0;
    s32 oldest = 0xFFFF;
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].age != 0xFFFF && D_800C2698[window].age >= oldest_age) {
            oldest_age = D_800C2698[window].age;
            oldest = window;
        }
    }
    return oldest;
}

/* Age the windows in use and take the first free one (age 0); 0xffff when
 * all are in use. */
s32 func_800807B4(void) {
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].age != 0xFFFF) {
            D_800C2698[window].age++;
        }
    }
    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].age == 0xFFFF) {
            D_800C2698[window].age = 0;
            return window;
        }
    }
    return 0xFFFF;
}

/* Release an event actor's blocks, its record, its descriptor block and
 * its model. */
void func_8008083C(s32 index) {
    FieldActor *actor;

    if (index < D_800ADBFC) {
        actor = D_800AF880.components.descriptors[index].actor;
        if (actor->unk134 & 0x80) {
            func_800320E8(actor->link);
        }
        if (actor->state.word & 0x1000) {
            func_800320E8(actor->unk114);
        }
        if (D_800AF880.components.descriptors[index].flags & 0x2000) {
            func_800320E8(actor->list);
        }
        if (actor->unk124 != -1) {
            func_800320E8(actor->unk120);
        }
        func_800320E8(actor);
        func_800320E8(D_800AF880.components.descriptors[index].shadow);
        func_800230A8(D_800AF880.components.descriptors[index].model);
    }
}

/* The collision attribute under an actor on its layer, or 0 when the layer
 * is switched off for it. */
u32 func_80080968(FieldActor *actor) {
    s16 layer = actor->layer;

    if ((actor->layer_flags >> (layer + 3)) & 1) {
        return 0;
    }
    return D_800AF880.components.collision_attributes[D_800AF880.components.collision_triangles[layer][actor->triangle[layer]].attribute].word;
}

/* Frames (in 800b14ac, two per step) and height of a jump under the actor's
 * gravity from the fixed launch speed. */
s32 func_800809D0(FieldActor *actor) {
    s32 speed = -0x14D000;
    s32 height = 0;

    D_800B14AC = 0;
    do {
        height += speed;
        speed += actor->gravity.value;
        D_800B14AC += 2;
    } while (speed <= 0);
    return height >> 16;
}

/* The next word of the current descriptor's actor list. */
s32 func_80080A18(void) {
    return D_800AF880.components.descriptors[D_800ADB58].actor->list[D_800ADB5C++];
}

#ifdef NON_MATCHING
/* Reset event actor `index` to its defaults and settle it on the floor of
 * each collision layer under its descriptor's position.
 * NON_MATCHING (10 edits): only the first constant stores differ: the
 * original hoists the 0x10 (+18/+1c) constant into $v1 above the +0 store
 * and loads 0xff for +74/+75 later; ours hoists the 0xff constant instead
 * (the store order and statement permutations tried do not change it).
 * The (point + i)-> and (normal + layer)-> forms keep the point clears off
 * the call argument as the original does. */
void func_80080A74(s32 index) {
    VECTOR normal[4];
    SVECTOR point[4];
    FieldActor *actor;
    s32 i;

    actor = D_800AF880.components.descriptors[index].actor;
    actor->flags = 0xB0;
    actor->layer_flags = 0x800;
    actor->unk074 = 0xFF;
    actor->unk18 = 0x10;
    actor->gravity.s.fraction = 0x10;
    actor->height = 0x60;
    actor->unk075 = 0xFF;
    actor->unk40[0] = 0;
    actor->unk40[1] = 0;
    actor->unk40[2] = 0;
    actor->unk030[0] = 0;
    actor->unk030[1] = 0;
    actor->unk030[2] = 0;
    actor->unk64 = 0;
    actor->unk60 = 0;
    actor->unk62 = 0;
    actor->target[0] = 0;
    actor->target[1] = 0;
    actor->target[2] = 0;
    actor->unkE6 = 0;
    actor->unk0EA = 0xFF;
    actor->unkE2 = 0;
    actor->pc = 0;
    actor->stuck = 0;
    actor->state.bits.unk5 = 0;
    actor->unk11E = 0x200;
    actor->gravity.s.whole = actor->unk18;
    actor->state.bits.mode = 0;
    actor->color1[2] = 0x80;
    actor->color1[1] = 0x80;
    actor->color1[0] = 0x80;
    actor->color0[2] = 0x80;
    actor->color0[1] = 0x80;
    actor->color0[0] = 0x80;
    actor->unk128 = 0xFFFF;
    actor->state.bits.unk16 = 0;
    actor->unk130_19 = 0;
    actor->unk130 = 0;
    actor->unk130_9 = 0;
    actor->state.bits.unk18 = 0;
    for (i = 0; i < 8; i++) {
        actor->slots[i].countdown = 0;
        actor->slots[i].unk16 = 0;
        actor->slots[i].unk22 = 0;
        actor->slots[i].move_mode = 0;
        actor->slots[i].priority = 15;
        actor->slots[i].value = 0xFFFF;
        actor->slots[i].resume_pc = 0xFFFF;
        actor->slots[i].tag = 0xFF;
    }
    actor->unk120 = NULL;
    actor->unkE4 = 0xFF;
    actor->unk76 = 0x100;
    actor->unk83 = 0;
    actor->unk82 = 0;
    actor->unk8A = 0;
    actor->unk88 = 0;
    actor->unk84 = 0;
    actor->unk0CF = 0;
    actor->slot = 0;
    actor->unkE8 = 0;
    actor->layer = 0;
    actor->unkEC = 0;
    actor->unk134 &= ~0x80;
    actor->state.bits.depth = 0;
    actor->state.bits.octant = 0;
    actor->state.bits.unk12 = 0;
    ACTOR_BITS134(actor).unk5 = 0;
    actor->unk102 = rand();
    actor->scale[0] = 0x1000;
    actor->scale[1] = 0x1000;
    actor->scale[2] = 0x1000;
    actor->sound_mode = 0xFF;
    actor->character = 0xFF;
    actor->heading_goal = 0x8000;
    actor->heading = 0x8000;
    actor->unk108 = 0x8000;
    actor->unk124 = -1;
    actor->unk0E3 = 0;
    actor->triangle[3] = 0;
    actor->triangle[2] = 0;
    actor->triangle[1] = 0;
    actor->triangle[0] = 0;
    actor->state.bits.unk2 = 0;
    for (i = 0; i < D_800AF880.components.layer_count - 1; i++) {
        actor->triangle[i] = func_8007B1C4((s16)D_800AF880.components.descriptors[index].matrix.t[0],
                                           (s16)D_800AF880.components.descriptors[index].matrix.t[2], i,
                                           point + i, &normal[i]);
        if (actor->triangle[i] != -1 && (u32)actor->triangle[i] >= D_800AF880.components.triangle_counts[i]) {
            D_800AF880.components.triangle_counts[i] = 0;
            normal[i].vx = 0;
            normal[i].vy = 0;
            normal[i].vz = 0;
            (point + i)->vx = 0;
            (point + i)->vy = 0;
            (point + i)->vz = 0;
        }
    }
    actor->unk014 = func_80080968(actor);
    actor->unk50[0] = (normal + actor->layer)->vx;
    actor->unk50[1] = (normal + actor->layer)->vy;
    actor->unk50[2] = (normal + actor->layer)->vz;
    if (!(D_800AF880.components.descriptors[index].flags & 0x80)) {
        D_800AF880.components.descriptors[index].matrix.t[1] = point[actor->layer].vy;
    }
    actor->position[0] = D_800AF880.components.descriptors[index].matrix.t[0] << 16;
    actor->position[1] = D_800AF880.components.descriptors[index].matrix.t[1] << 16;
    actor->position[2] = D_800AF880.components.descriptors[index].matrix.t[2] << 16;
    actor->unk72 = D_800AF880.components.descriptors[index].matrix.t[1];
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_8007A44C", func_80080A74);
#endif

/* Create event actor `index`: a cleared 0x138-byte record, the fetch hooks of
 * an animated model's channels, its defaults (80080a74) and its ground
 * marker (8007aa44). */
void func_80080F44(s32 index) {
    u8 unused[0x60];
    FieldActor *actor;
    FieldInstance *instance;
    s32 *word;
    s32 words = sizeof(FieldActor) / 4;
    s32 i;

    if (index < D_800ADBFC) {
        D_800B2180[0]++;
        D_800AF880.components.descriptors[index].actor = func_80031BDC(0x138, 0);
        word = (s32 *)D_800AF880.components.descriptors[index].actor;
        for (i = 0; i < words; i++) {
            *word++ = 0;
        }
        actor = D_800AF880.components.descriptors[index].actor;
        D_800AF880.components.descriptors[index].unk5A = 0;
        if (D_800AF880.components.descriptors[index].flags & 0x2000) {
            instance = D_800AF880.components.descriptors[index].instance;
            actor->list = func_80031BDC(0x80, 0);
            if (instance->anims != NULL) {
                for (i = 0; i < instance->anims->count; i++) {
                    instance->anims->channels[i].fetch = func_80080A18;
                    actor->list[i] = 0;
                }
            }
        }
        func_80080A74(index);
        D_800AF880.components.descriptors[index].shadow = func_80031BDC(0x70, 0);
        func_8007AA44(D_800AF880.components.descriptors[index].shadow);
    }
}

/* The field update: run the events, then move every actor (motion stages,
 * the controlled actor's contact and position, the others' positions,
 * encounters) and the followers. */
void func_8008110C(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 i;

    D_800C3910 = -1;
    func_800A2030();
    for (i = 0; i < D_800ADBFC; i++) {
        D_800AF880.components.descriptors[i].actor->last_position[0] = D_800AF880.components.descriptors[i].actor->position[0] >> 16;
        D_800AF880.components.descriptors[i].actor->last_position[1] = D_800AF880.components.descriptors[i].actor->position[1] >> 16;
        D_800AF880.components.descriptors[i].actor->last_position[2] = D_800AF880.components.descriptors[i].actor->position[2] >> 16;
    }
    if (D_800C268C == 0) {
        func_80281B00("EVENT CODE");
    }
    D_800AF858 = 0;
    for (i = 0; i < D_800ADBFC; i++) {
        descriptor = &D_800AF880.components.descriptors[i];
        actor = descriptor->actor;
        if ((descriptor->flags & 0xF80) == 0x200) {
            if (!(actor->flags & 0x10001)) {
                if ((actor->layer_flags & 0x600) != 0x200) {
                    actor->unk014 = func_80080968(actor);
                    func_80082620(i, descriptor, actor);
                    func_80082BB8(i, descriptor, actor);
                }
            } else if ((actor->layer_flags & 0x1000000) && !(actor->flags & 0x10000)) {
                if (actor->unkE8 != actor->unk0EA) {
                    actor->unk0EA = 2;
                    D_800AF880.components.descriptors[i].actor->unkE8 = D_800AF880.components.descriptors[i].actor->unk0EA;
                    func_800245D8(D_800AF880.components.descriptors[i].model, actor->unk0EA);
                }
            } else if (actor->layer_flags & 0x200000) {
                if (D_800AF880.components.descriptors[i].actor->unkE8 != D_800AF880.components.descriptors[i].actor->unk0EA) {
                    D_800AF880.components.descriptors[i].actor->unkE8 = D_800AF880.components.descriptors[i].actor->unk0EA;
                    func_800821F4(D_800AF880.components.descriptors[i].model, D_800AF880.components.descriptors[i].actor->unk0EA,
                                  &D_800AF880.components.descriptors[i]);
                }
            }
        }
    }
    if (D_800C268C == 0) {
        func_80281B00("MOV CHECK0");
    }
    descriptor = &D_800AF880.components.descriptors[D_800B2078.controlled];
    func_80084158(D_800B2078.controlled, descriptor, descriptor->actor);
    if (D_800C268C == 0) {
        func_80281B00("MOV CHECK1");
    }
    for (i = 0; i < D_800ADBFC; i++) {
        if (D_800AF880.components.descriptors[i].flags & 0xF00) {
            descriptor = &D_800AF880.components.descriptors[i];
            actor = descriptor->actor;
            if ((actor->layer_flags & 0x600) != 0x200 && (descriptor->flags & 0xF80) == 0x200
                && !(actor->flags & 0x10001) && i != D_800B2078.controlled) {
                func_80084A40(i, 0x7FFFFFFF, descriptor, actor, 0);
                if (D_800AF880.components.descriptors[i].model->animation->unk0C == 1) {
                    actor->flags &= ~0x800;
                }
            }
        }
    }
    if (D_800C268C == 0) {
        func_80281B00("MOV CHECK2");
    }
    if (D_800B2078.encounter_inhibition == 0 && D_800B2078.party_processing_mode == 0) {
        func_8008399C(D_800B2078.controlled, &D_800AF880.components.descriptors[D_800B2078.controlled],
                      D_800AF880.components.descriptors[D_800B2078.controlled].actor);
    }
    D_800ADC0C = 1;
    func_800815F0();
    if (D_800C268C == 0) {
        func_80281B00("MOV CHECK3");
    }
}

/* Move the party followers: while they idle, return each to its idle
 * animation; otherwise replay the leader's movement history, each follower
 * lagging its own number of records behind, settling when it catches up. */
void func_800815F0(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    FieldModel *sprite;
    s32 i;
    s32 k;
    s32 slot;
    s32 lag;
    u32 recorded;
    s32 *index;
    s16 animation;
    s16 idle;
    u32 motion;

    if (D_800B2078.followers_idle != 0) {
        for (i = 0; i < D_800ADBFC; i++) {
            if ((D_800AF880.components.descriptors[i].actor->flags & 0x01000000) && i != D_800B2078.controlled &&
                !(D_800AF880.components.descriptors[i].flags & 0x20)) {
                actor = D_800AF880.components.descriptors[i].actor;
                descriptor = &D_800AF880.components.descriptors[i];
                sprite = descriptor->model;
                idle = actor->unkE6;
                if (actor->unkE8 != idle) {
                    actor->unkE8 = idle;
                    if (idle < 0) {
                        actor->unkE8 = 0;
                    }
                    func_800821F4(sprite, actor->unkE8, descriptor);
                }
            }
        }
        return;
    }
    for (i = 0; i < D_800ADBFC; i++) {
        if (!(D_800AF880.components.descriptors[i].actor->flags & 0x01000000) || i == D_800B2078.controlled ||
            (D_800AF880.components.descriptors[i].flags & 0x20)) {
            continue;
        }
        descriptor = &D_800AF880.components.descriptors[i];
        actor = D_800AF880.components.descriptors[i].actor;
        sprite = descriptor->model;
        slot = func_8009FA00(actor->unkE4);
        if (slot == -1) {
            continue;
        }
        index = &D_800B2360[slot];
        func_80081F80(sprite, D_800B14F0[*index].heading, descriptor);
        recorded = D_800B14F0[*index].flags;
        motion = actor->unk014;
        if (D_800B2078.forced_position == 1) {
            *index = (D_800B2360[0] + 1) & 0x1F;
        } else {
            if (!(recorded & 0x800)) {
                actor->layer_flags &= ~0x1000;
                if (!(motion & 0x420000)) {
                    if (D_800C3910 == -1) {
                        if ((s16)sprite->unk84 == actor->position[1] >> 16) {
                            if (actor->unkE8 != 6) {
                                s16 rest = actor->unkE6;

                                if (actor->unkE8 == rest) {
                                    continue;
                                }
                                actor->unkE8 = rest;
                                if (rest < 0) {
                                    actor->unkE8 = 0;
                                }
                                func_800821F4(sprite, actor->unkE8, descriptor);
                            } else {
                                actor->layer_flags |= 0x1000;
                            }
                            continue;
                        }
                    } else {
                        lag = 20;
                        if (slot == 1) {
                            lag = 10;
                        }
                        if (((D_800B2360[0] + lag) & 0x1F) != *index) {
                            continue;
                        }
                    }
                }
            }
            if (D_800B2360[slot] == D_800B2360[0]) {
                actor->flags &= ~0x800;
                actor->unkE8 = actor->unkE6;
                if (actor->unkE8 < 0) {
                    actor->unkE8 = 0;
                }
                func_800821F4(sprite, actor->unkE8, descriptor);
                continue;
            }
        }
        if (recorded & 0x800) {
            actor->flags |= 0x800;
        } else {
            actor->flags &= ~0x800;
        }
        animation = D_800B14F0[D_800B2360[slot]].unk12;
        if (actor->unkE8 != animation) {
            actor->unkE8 = animation;
            if (animation < 0) {
                actor->unkE8 = 0;
            }
            func_800821F4(sprite, actor->unkE8, descriptor);
        }
        for (k = 0; k < 4; k++) {
            actor->triangle[k] = D_800B14F0[D_800B2360[slot]].triangle[k];
        }
        actor->layer = D_800B14F0[D_800B2360[slot]].layer;
        copyVector((VECTOR *)actor->unk50, (VECTOR *)D_800B14F0[D_800B2360[slot]].unk30);
        copyVector((VECTOR *)sprite->velocity, (VECTOR *)D_800B14F0[D_800B2360[slot]].model_velocity);
        descriptor->matrix.t[0] = D_800B14F0[D_800B2360[slot]].position[0];
        descriptor->matrix.t[1] = D_800B14F0[D_800B2360[slot]].position[1];
        descriptor->matrix.t[2] = D_800B14F0[D_800B2360[slot]].position[2];
        actor->position[0] = sprite->position[0] = descriptor->matrix.t[0] << 16;
        actor->position[1] = sprite->position[1] = descriptor->matrix.t[1] << 16;
        actor->position[2] = sprite->position[2] = descriptor->matrix.t[2] << 16;
        sprite->unk84 = D_800B14F0[D_800B2360[slot]].model84;
        actor->heading = actor->heading_goal = D_800B14F0[D_800B2360[slot]].heading;
        D_800B2360[slot] = (D_800B2360[slot] - 1) & 0x1F;
    }
}

/* Record the controlled actor `index`'s state in the next movement-history
 * slot, unless party processing is suspended. */
void func_80081C54(s32 index) {
    FieldModel *model;
    FieldActor *actor;
    s32 i;
    FieldDescriptor *descriptor;

    descriptor = &D_800AF880.components.descriptors[index];
    actor = descriptor->actor;
    model = descriptor->model;
    if (index == D_800B2078.controlled && D_800B2078.party_processing_mode == 0) {
        copyVector((VECTOR *)D_800B14F0[D_800B2360[0]].model_velocity, (VECTOR *)model->velocity);
        copyVector((VECTOR *)D_800B14F0[D_800B2360[0]].unk30, (VECTOR *)actor->unk50);
        D_800B14F0[D_800B2360[0]].heading = actor->heading_goal & 0xFFF;
        D_800B14F0[D_800B2360[0]].model84 = model->unk84;
        D_800B14F0[D_800B2360[0]].position[0] = actor->position[0] >> 16;
        D_800B14F0[D_800B2360[0]].position[1] = actor->position[1] >> 16;
        D_800B14F0[D_800B2360[0]].position[2] = actor->position[2] >> 16;
        D_800B14F0[D_800B2360[0]].unk12 = actor->unkE8;
        D_800B14F0[D_800B2360[0]].unk40 = actor->unk014;
        D_800B14F0[D_800B2360[0]].flags = actor->flags;
        D_800B14F0[D_800B2360[0]].layer_flags = actor->layer_flags;
        for (i = 0; i < 4; i++) {
            D_800B14F0[D_800B2360[0]].triangle[i] = actor->triangle[i];
        }
        D_800B14F0[D_800B2360[0]].layer = actor->layer;
        D_800C3910 = 0;
        D_800B2360[0] = (D_800B2360[0] - 1) & 0x1F;
    }
}

/* -1 when the actor's bits 9-10 meet bits 3-4 of +14, else 0. */
s32 func_80081F5C(FieldActor *actor) {
    u32 bits = (actor->flags >> 9) & 3;

    return -((bits & (actor->unk014 >> 3)) != 0);
}

/* Set a sprite's planar velocity from `heading` (0-fff; bit 15 stops it):
 * scaled by the actor's speed ratio (+76) and axis scales (+f4/+f8), taken
 * from its layer's gear object, or through the sprite's own heading for an
 * ordinary party actor; both components keep 1/16 unit precision. */
void func_80081F80(FieldModel *sprite, s16 heading, FieldDescriptor *descriptor) {
    FieldActor *actor;
    s32 layer;
    s32 speed;
    s32 angle;

    if (!(descriptor->flags & 0x40)) {
        speed = ((0x40000 / (u16)descriptor->actor->unk76) >> 8) << 5;
        angle = heading & 0xFFF;
        if (!(heading & 0x8000)) {
            sprite->velocity[0] = ((func_8003F8CC(angle) * speed) >> 12) * descriptor->actor->scale[0];
            sprite->velocity[2] = (-(func_8003F8B0(angle) * speed) >> 12) * descriptor->actor->scale[2];
        } else {
            sprite->velocity[0] = 0;
            sprite->velocity[2] = 0;
        }
    } else if (!(heading & 0x8000)) {
        actor = descriptor->actor;
        if (!(actor->layer_flags & 0x2000)) {
            if (!(actor->layer_flags & 0x80000)) {
                func_80021FE0(sprite, heading);
            } else {
                speed = ((0x40000 / (u16)actor->unk76) >> 8) << 5;
                angle = heading & 0xFFF;
                sprite->velocity[0] = ((func_8003F8CC(angle) * speed) >> 12) * descriptor->actor->scale[0];
                sprite->velocity[2] = (-(func_8003F8B0(angle) * speed) >> 12) * descriptor->actor->scale[2];
                sprite->unk18 = 0x4000000 / (u16)descriptor->actor->unk76;
            }
        } else if (!(actor->layer_flags & 0x20000)) {
            speed = ((0x80000 / (u16)actor->unk76) >> 8) << 5;
            angle = heading & 0xFFF;
            sprite->velocity[0] = ((func_8003F8CC(angle) * speed) >> 12) * descriptor->actor->scale[0];
            sprite->velocity[2] = (-(func_8003F8B0(angle) * speed) >> 12) * descriptor->actor->scale[2];
        } else {
            layer = actor->state.bits.layer;
            sprite->velocity[0] = -D_801E8670[layer]->speed_x << 16;
            sprite->velocity[2] = -D_801E8670[layer]->speed_z << 16;
        }
    } else {
        sprite->velocity[0] = 0;
        sprite->velocity[2] = 0;
    }
    sprite->velocity[0] &= ~0xFFF;
    sprite->velocity[2] &= ~0xFFF;
}






extern void func_800245D8(void *model, s32 animation);
extern u8 D_800ADFB8[];
/* Start animation `animation` on a descriptor's model (flag 0x40 set):
 * clears the actor's flag 0x800 outside jumps or on a change; 801e layer
 * actors (layer bit 13) set their layer frame instead (below 0x10 through
 * the 800adfb8 table). */
void func_800821F4(void *model, s32 animation, FieldDescriptor *descriptor) {
    if (!(descriptor->flags & 0x40)) {
        return;
    }
    if (animation != 3 && D_800B2078.jump_mode == 0) {
        descriptor->actor->flags &= ~0x800;
    }
    if (animation == 0xFF) {
        animation = 0;
    }
    if (animation != D_800B2078.animation_mode) {
        descriptor->actor->flags &= ~0x800;
    }
    if (!(descriptor->actor->layer_flags & 0x2000)) {
        if (!(descriptor->actor->layer_flags & 0x1000000)) {
            func_800245D8(model, animation);
        }
    } else if (animation < 0x10) {
        func_801E8330(descriptor->actor->state.bits.layer, 0, D_800ADFB8[animation]);
        D_800B2078.unk21E4[descriptor->actor->state.bits.layer] = D_800ADFB8[animation];
    } else {
        animation -= 0x10;
        func_801E8330(descriptor->actor->state.bits.layer, 0, animation);
        D_800B2078.unk21E4[descriptor->actor->state.bits.layer] = animation;
    }
}

typedef struct {
    u8 unk00[0x18];
    u16 half_x;  /* 18 */
    u8 unk1A[2];
    u16 half_z;  /* 1C */
    u8 unk1E[4];
    s16 x;       /* 22 */
    u8 unk24[6];
    s16 z;       /* 2A */
} FieldBox;
extern void func_80281678(FieldBox *box);
/* -1 unless point (x, z) lies inside `box` grown by `margin`; inside, run
 * 80281678 on it (unless 800c268c is set) and return 0. */
s32 func_8008237C(s32 x, s32 z, FieldBox *box, s32 margin) {
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    point = (x << 16) + z;
    a = ((box->x - box->half_x - margin) << 16) + (box->z + box->half_z + margin);
    b = ((box->x + box->half_x + margin) << 16) + (box->z + box->half_z + margin);
    c = ((box->x + box->half_x + margin) << 16) + (box->z - box->half_z - margin);
    d = ((box->x - box->half_x - margin) << 16) + (box->z - box->half_z - margin);
    if (func_8004A70C(a, b, point) < 0 || func_8004A70C(b, c, point) < 0 ||
        func_8004A70C(c, d, point) < 0 || func_8004A70C(d, a, point) < 0) {
        return -1;
    }
    if (D_800C268C == 0) {
        func_80281678(box);
    }
    return 0;
}

/* 0 when the actor has no quad (state bit 12); else -1 unless its position
 * plus `offset` lies inside the quad at +114. */
s32 func_80082494(s32 *offset, FieldActor *actor) {
    s16 *quad;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    if (!(actor->state.word & 0x1000)) {
        return 0;
    }
    point = (((actor->position[0] + offset[0]) >> 16) << 16) + ((actor->position[2] + offset[2]) >> 16);
    quad = actor->unk114;
    a = (quad[0] << 16) + quad[1];
    b = (quad[2] << 16) + quad[3];
    c = (quad[4] << 16) + quad[5];
    d = (quad[6] << 16) + quad[7];
    if (func_8004A70C(a, b, point) < 0 || func_8004A70C(b, c, point) < 0 ||
        func_8004A70C(c, d, point) < 0) {
        return -1;
    }
    return func_8004A70C(d, a, point) >> 31;
}

/* Planar distance between two descriptors' actors (integer positions). */
s32 func_800825AC(s32 from, s32 to) {
    s32 to_x = D_800AF880.components.descriptors[to].actor->position[0] >> 16;
    s32 to_z = D_800AF880.components.descriptors[to].actor->position[2] >> 16;
    s32 from_x = D_800AF880.components.descriptors[from].actor->position[0] >> 16;
    s32 from_z = D_800AF880.components.descriptors[from].actor->position[2] >> 16;

    return func_80099A4C(to_x - from_x, to_z - from_z);
}

#ifdef NON_MATCHING
/* An actor's additive motion before it moves: the terrain push and conveyor
 * of the floor it stands on, the platform it rides and its gear layer's
 * drift.
 * NON_MATCHING (14 edits): allocation now matches (a block-local `dir`
 * takes $s0 before radius, so actor/terrain/radius land as in the
 * original). Left: the original computes ratan2 - angle in a temporary
 * ($v0) and only the final - 0x800 into $s0, where ours reuses angle for
 * the difference; and it schedules the turn.vy sign extension (sra) into
 * the heading test's delay slot, so the heading load uses $v1, not $a0.
 * One variable for angle and the result gives the temporary but puts
 * radius in $s0 (local-alloc) and swaps actor/terrain. */
void func_80082620(s32 index, FieldDescriptor *descriptor, FieldActor *actor) {
    u32 terrain;
    VECTOR conveyor;
    VECTOR slope;
    VECTOR normal;
    VECTOR unused;
    SVECTOR turn;
    FieldModel *model;
    FieldActor *platform;
    LayerObject **entry;
    s32 push_x;
    s32 push_z;
    s32 speed;
    s32 limit;
    s32 self_x;
    s32 self_z;
    s32 other_x;
    s32 other_z;
    s32 radius;
    s32 angle;
    s32 dir;

    push_x = 0;
    push_z = 0;
    terrain = 0;
    if (!((actor->layer_flags >> (actor->layer + 3)) & 1) && D_800B2078.party_processing_mode == 0) {
        terrain = actor->unk014;
    }
    model = descriptor->model;
    func_8007B614(&conveyor, D_800ADFC4[(terrain >> 9) & 3],
                  (D_800ADFA8[(terrain >> 11) & 7] + (u16)D_800B2078.terrain_angle) & 0xFFF);
    if (!(actor->flags & 0x41800)) {
        speed = actor->unkF0;
        if (terrain & 0x420000) {
            slope.vx = -(actor->unk50[0] * actor->unk50[1]) >> 15;
            slope.vy = 0;
            slope.vz = -(actor->unk50[2] * actor->unk50[1]) >> 15;
            if (slope.vx == 0) {
                slope.vx = 1;
            }
            slope.vy = 1;
            if (slope.vz == 0) {
                slope.vz = 1;
            }
            VectorNormal(&slope, &normal);
            if (normal.vx == 0) {
                normal.vx = 1;
            }
            if (normal.vy == 0) {
                normal.vy = 1;
            }
            if (normal.vz == 0) {
                normal.vz = 1;
            }
            push_x = normal.vx * (speed >> 17) << 4;
            push_z = normal.vz * (speed >> 17) << 4;
            limit = 0xC;
            if (terrain & 0x400000) {
                limit = 0x18;
            }
            if ((actor->unkF0 >> 16) >= limit) {
                actor->unkF0 = limit << 16;
            } else {
                actor->unkF0 += model->gravity.value;
            }
            model->velocity[1] = actor->unkF0 >> 1;
        }
        if (terrain & 0x400000) {
            actor->unk40[0] += push_x;
            actor->unk40[2] += push_z;
            actor->heading |= 0x8000;
        }
        if (actor->unk074 != 0xFF) {
            goto linked;
        }
        if (terrain & 0x20000) {
            actor->unk40[0] += push_x;
            actor->unk40[2] += push_z;
        }
        if (!(terrain & 0x8000)) {
            goto conveyed;
        }
    } else if (!(terrain & 0x4000)) {
        goto conveyed;
    }
    actor->unk40[0] += conveyor.vx;
    actor->unk40[1] += conveyor.vy;
    actor->unk40[2] += conveyor.vz;
conveyed:
    if (actor->unk074 != 0xFF) {
    linked:
        if ((D_800AF880.components.descriptors[actor->unk074].actor->layer_flags & 0xC0) == 0xC0) {
            if (!(actor->unk134 & 0x80)) {
                actor->link = func_80031BDC(sizeof(PlatformLink), 0);
                actor->unk134 |= 0x80;
            }
            turn.vx = D_800AF880.components.descriptors[actor->unk074].rotation.vx - actor->link->rotation.vx;
            angle = turn.vy = D_800AF880.components.descriptors[actor->unk074].rotation.vy - actor->link->rotation.vy;
            turn.vz = D_800AF880.components.descriptors[actor->unk074].rotation.vz - actor->link->rotation.vz;
            actor->link->rotation.vy = D_800AF880.components.descriptors[actor->unk074].rotation.vy;
            self_x = actor->position[0];
            self_z = actor->position[2];
            platform = D_800AF880.components.descriptors[actor->unk074].actor;
            other_x = platform->position[0];
            other_z = platform->position[2];
            if (!(actor->heading & 0x8000)) {
                actor->link->radius = func_800825AC(index, actor->unk074);
            }
            radius = actor->link->radius;
            angle = (s16)ratan2(other_z - self_z, other_x - self_x) - angle;
            dir = angle - 0x800;
            actor->unk40[0] += other_x + func_8003F8CC(dir) * radius * 16 - self_x;
            actor->unk40[2] += other_z + func_8003F8B0(dir) * radius * 16 - self_z;
        }
    }
    if ((actor->layer_flags & 0x22000) == 0x22000) {
        entry = &D_801E8670[D_800AF858];
        actor->unk40[0] -= (((*entry)->speed_x << 16) / (u16)actor->unk76) << 8;
        actor->unk40[2] -= (((*entry)->speed_z << 16) / (u16)actor->unk76) << 8;
        D_800AF858++;
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_8007A44C", func_80082620);
#endif

#ifdef NON_MATCHING
/* Move actor `index` for this frame: choose its walk/run mode, turn its
 * requested heading into a velocity (80081f80) plus its additive motion,
 * sweep that against the collision layers, then pick its animation and
 * store the result as its velocity (+30).
 * NON_MATCHING (16 edits): only index and descriptor swap $s5/$s6. Global
 * allocation takes descriptor (4 refs over 261 insns) before index (3 refs
 * over 111); the original allocates index first. The (u16) view of the
 * first heading test keeps jump threading from merging it with the second,
 * as in the original. */
void func_80082BB8(s32 index, FieldDescriptor *descriptor, FieldActor *actor) {
    VECTOR move;
    SVECTOR edge[2];
    FieldModel *model;
    s32 moving;
    s32 result;
    u32 saved;
    s32 heading;
    s16 mode;

    heading = actor->heading;
    model = descriptor->model;
    D_80065B08 = index;
    if (actor->flags & 0x1000000) {
        return;
    }
    mode = 1;
    if ((actor->flags & 0x4000) && (D_800AFE9C & 0x40) && D_800ADB68 == 1) {
        mode = 2;
    }
    if ((actor->flags & 0x1800) && actor->unkE8 != mode) {
        switch (actor->unkE8) {
        case 1:
            mode = 1;
            break;
        case 2:
            mode = 2;
            break;
        }
    }
    if (actor->unk0E3 >= 9) {
        actor->unk0E3--;
    }
    moving = actor->unk40[0] | actor->unk40[1] | actor->unk40[2];
    if (func_8008492C(actor) == -1) {
        moving = 1;
    }
    if (((u16)heading & 0x8000) && moving == 0 && !(actor->flags & 0x40800)) {
        goto idle;
    }
    if (!(heading & 0x8000)) {
        func_80081F80(model, heading, descriptor);
        move.vx = model->velocity[0];
        move.vy = model->velocity[1];
        move.vz = model->velocity[2];
        move.vx += actor->unk40[0];
        move.vy += actor->unk40[1];
        move.vz += actor->unk40[2];
        actor->heading_goal = heading;
    } else {
        heading = actor->heading_goal & 0xFFF;
        move.vx = actor->unk40[0];
        move.vy = actor->unk40[1];
        move.vz = actor->unk40[2];
    }
    if (func_80082494(&move.vx, actor) != 0) {
        goto stop;
    }
    if (move.vx != 0 || move.vz != 0) {
        heading = -(s16)ratan2(move.vz, move.vx) & 0xFFF;
    }
    result = -1;
    if (actor->triangle[actor->layer] != -1) {
        saved = actor->flags;
        if (index == D_800B2078.controlled) {
            if (D_8005A444[1] != 0xFF) {
                actor->flags = saved | (D_800AF880.components.descriptors[D_8005A444[1]].actor->flags & 0x600);
            }
            if (D_8005A444[2] != 0xFF) {
                actor->flags |= D_800AF880.components.descriptors[D_8005A444[2]].actor->flags & 0x600;
            }
        }
        if (!(actor->flags & 0x41800) && actor->unk074 == 0xFF && D_800ADB98 == 0) {
            result = func_8007BAC0(&move, actor, edge, heading);
        } else {
            result = func_8007B814(&move, actor, edge, heading);
        }
        actor->flags = (actor->flags & ~0x600) | (saved & 0x600);
    }
    if (result == -1) {
        goto stop;
    }
    goto moved;
idle:
    mode = actor->unkE6;
    actor->heading |= 0x8000;
stop:
    actor->unkF0 = 0x10000;
    actor->unk40[0] = 0;
    actor->unk40[1] = 0;
    actor->unk40[2] = 0;
    move.vx = 0;
    move.vy = 0;
    move.vz = 0;
    model->velocity[0] = 0;
    model->velocity[2] = 0;
    actor->heading_goal |= 0x8000;
moved:
    actor->layer_flags &= ~0x1000;
    if (actor->flags & 0x800) {
        if (D_800B2078.jump_mode == 0) {
            if (WHOLE(model->position[1]) != (s16)model->unk84) {
                if (mode == 2) {
                    model->unk18 = model->unk82 * 0x60;
                } else {
                    model->unk18 = model->unk82 * 0x30;
                }
            } else {
                model->unk18 = 0;
            }
        }
        mode = D_800B2078.animation_mode;
    } else {
        if (actor->heading & 0x8000) {
            mode = actor->unkE6;
        }
        if (func_80080968(actor) & 0x200000) {
            if ((actor->heading & 0x8000) && actor->unkE8 == 6) {
                actor->layer_flags |= 0x1000;
            }
            mode = 6;
        }
    }
    if (actor->unk0EA != 0xFF) {
        mode = actor->unk0EA;
    }
    if (actor->unkE8 != mode && !(actor->flags & 0x2000000)) {
        actor->unkE8 = mode;
        func_800821F4(model, mode, descriptor);
    }
    if (actor->unk014 & 0x100) {
        move.vx >>= 1;
        move.vz >>= 1;
    }
    actor->unk030[0] = move.vx;
    actor->unk030[1] = move.vy;
    actor->unk030[2] = move.vz;
    actor->unk40[0] = 0;
    actor->unk40[1] = 0;
    actor->unk40[2] = 0;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_8007A44C", func_80082BB8);
#endif

/* Ease `value` 0x4000 towards zero, bounded by +-`limit`. */
s32 func_80083178(s32 value, s32 limit) {
    if (value < 0) {
        value += 0x4000;
        if (value < -limit) {
            value = -limit;
        }
        if (value > 0) {
            value = 0;
        }
    } else {
        value -= 0x4000;
        if (value > limit) {
            value = limit;
        }
        if (value < 0) {
            value = 0;
        }
    }
    return value;
}

/* The integer parts of a 16.16 vector. */
void func_800831D0(SVECTOR *out, VECTOR *in) {
    out->vx = in->vx >> 16;
    out->vy = in->vy >> 16;
    out->vz = in->vz >> 16;
}

/* While the actor moves, turn its heading a quarter (left with flag bit 0,
 * else right) once, apply it, and mark the heading as turned. */
void func_800831F4(void *owner, FieldActor *actor, FieldDescriptor *descriptor, s32 flags) {
    s16 heading;
    s16 turned;

    if (actor->unk030[0] != 0 || actor->unk030[2] != 0) {
        heading = actor->heading_goal;
        if (!(heading & 0x8000)) {
            if (flags & 1) {
                turned = heading - 0x400;
                actor->heading = turned & 0xFFF;
                actor->heading_goal = turned & 0xFFF;
            } else {
                turned = heading + 0x400;
                actor->heading = turned & 0xFFF;
                actor->heading_goal = turned & 0xFFF;
            }
            func_80081F80(owner, actor->heading, descriptor);
            actor->heading_goal = actor->heading = actor->heading_goal | 0x8000;
        }
    }
}

/* POLYCHECK: the lowest floor height of descriptor `index`'s collision model
 * under x/z (0, with the height and the last hit's normal), or -1. */
s32 func_80083288(s32 index, PolyModel *model, s32 x, s32 z, s32 *height, VECTOR *normal) {
    PolyCheck *work;
    FieldActor *actor;
    u32 *prim;
    u32 header;
    s32 count;
    s32 groups;
    s32 i;

    work = (PolyCheck *)func_8007CD3C(sizeof(PolyCheck));
    prim = model->prims;
    work->vertices = model->vertices;
    work->point = (x << 16) + z;
    work->lowest = 0x7FFFFFFF;
    actor = D_800AF880.components.descriptors[index].actor;
    switch (actor->state.bits.mode) {
    case 1:
        work->angles.vx = actor->unk70;
        work->angles.vy = 0;
        work->angles.vz = 0;
        goto local;
    case 2:
        work->angles.vx = 0;
        work->angles.vy = D_800AF880.components.descriptors[index].actor->unk70;
        work->angles.vz = 0;
        goto local;
    case 3:
        work->angles.vx = 0;
        work->angles.vy = 0;
        work->angles.vz = D_800AF880.components.descriptors[index].actor->unk70;
    local:
        func_8003F738(&work->angles, &work->local);
        MulMatrix2(&D_800AF880.components.descriptors[index].matrix, &work->local);
        work->local.t[0] = D_800AF880.components.descriptors[index].matrix.t[0];
        work->local.t[1] = D_800AF880.components.descriptors[index].matrix.t[1];
        work->local.t[2] = D_800AF880.components.descriptors[index].matrix.t[2];
        CompMatrix(&D_800AF880.world_matrix, &work->local, &work->transform);
        break;
    default:
        work->transform.t[2] = 0;
        work->transform.t[1] = 0;
        work->transform.t[0] = 0;
        work->view.t[2] = 0;
        work->view.t[1] = 0;
        work->view.t[0] = 0;
        if (D_800AF880.components.descriptors[index].actor->unk075 != 0xFF) {
            CompMatrix(&D_800AF880.world_matrix, &D_800AFC30, &work->view);
            CompMatrix(&work->view, &D_800AF880.components.descriptors[D_800AF880.components.descriptors[index].actor->unk075].transform,
                       &work->local);
            CompMatrix(&work->local, &D_800AF880.components.descriptors[index].matrix, &work->transform);
        } else {
            CompMatrix(&D_800AF880.world_matrix, &D_800AFC30, &work->view);
            CompMatrix(&work->view, &D_800AF880.components.descriptors[index].matrix, &work->transform);
        }
        break;
    }
    SetRotMatrix(&work->transform);
    SetTransMatrix(&work->transform);
    for (groups = model->groups; groups > 0; groups--) {
        header = *prim;
        count = header >> 16;
        work->type = header & 0xFF;
        if (work->type == 0xC4 || work->type == 0xC8) {
            prim++;
            continue;
        }
        prim++;
        if (!(header & 8)) {
            for (i = 0; i < count; i++) {
                RotTransSV(&work->vertices[((u16 *)prim)[0]], &work->v[0], &work->flag);
                RotTransSV(&work->vertices[((u16 *)prim)[1]], &work->v[1], &work->flag);
                prim++;
                RotTransSV(&work->vertices[((u16 *)prim)[0]], &work->v[2], &work->flag);
                prim++;
                work->packed[0] = (work->v[0].vx << 16) + work->v[0].vz;
                work->packed[1] = (work->v[1].vx << 16) + work->v[1].vz;
                work->packed[2] = (work->v[2].vx << 16) + work->v[2].vz;
                if (func_8004A70C(work->packed[0], work->packed[1], work->point) >= 0
                    && func_8004A70C(work->packed[1], work->packed[2], work->point) >= 0
                    && func_8004A70C(work->packed[2], work->packed[0], work->point) >= 0
                    && func_8004A70C(work->packed[0], work->packed[1], work->packed[2]) >= 0) {
                    work->p.vx = x;
                    work->p.vz = z;
                    func_8007B07C(&work->v[0], &work->v[1], &work->v[2], &work->p, normal);
                    if (work->p.vy < work->lowest) {
                        work->lowest = work->p.vy;
                    }
                }
            }
        } else {
            for (i = 0; i < count; i++) {
                RotTransSV(&work->vertices[((u16 *)prim)[0]], &work->v[0], &work->flag);
                RotTransSV(&work->vertices[((u16 *)prim)[1]], &work->v[1], &work->flag);
                prim++;
                RotTransSV(&work->vertices[((u16 *)prim)[0]], &work->v[2], &work->flag);
                RotTransSV(&work->vertices[((u16 *)prim)[1]], &work->v[3], &work->flag);
                prim++;
                work->packed[0] = (work->v[0].vx << 16) + work->v[0].vz;
                work->packed[1] = (work->v[1].vx << 16) + work->v[1].vz;
                work->packed[2] = (work->v[2].vx << 16) + work->v[2].vz;
                work->packed[3] = (work->v[3].vx << 16) + work->v[3].vz;
                if (func_8004A70C(work->packed[0], work->packed[1], work->point) >= 0
                    && func_8004A70C(work->packed[1], work->packed[3], work->point) >= 0
                    && func_8004A70C(work->packed[3], work->packed[2], work->point) >= 0
                    && func_8004A70C(work->packed[2], work->packed[0], work->point) >= 0
                    && func_8004A70C(work->packed[0], work->packed[1], work->packed[2]) >= 0) {
                    work->p.vx = x;
                    work->p.vz = z;
                    if (func_8004A70C(work->packed[1], work->packed[2], work->point) >= 0) {
                        func_8007B07C(&work->v[0], &work->v[1], &work->v[2], &work->p, normal);
                    } else {
                        func_8007B07C(&work->v[1], &work->v[3], &work->v[2], &work->p, normal);
                    }
                    if (work->p.vy < work->lowest) {
                        work->lowest = work->p.vy;
                    }
                }
            }
        }
    }
    if (work->lowest == 0x7FFFFFFF) {
        func_8007CD60(sizeof(PolyCheck));
        return -1;
    }
    *height = work->lowest;
    func_8007CD60(sizeof(PolyCheck));
    return 0;
}

void func_80083994(void) {
}

/* The controlled actor's talk (event 2) and touch (event 3) triggers: each
 * other actor within reach (rectangle 0x2000 or circle), facing and not
 * inhibited, turns toward it and gets the event in a free script slot. */
void func_8008399C(s32 index, FieldDescriptor *descriptor, FieldActor *player) {
    VECTOR offset;
    VECTOR square;
    VECTOR reach;
    VECTOR reach_square;
    FieldActor *other;
    s32 priority;
    s32 touch;
    s32 talk;
    s32 py;
    s32 head;
    s16 facing;
    s32 px;
    s32 pz;
    s32 talked;
    s32 event;
    s32 top;
    s32 distance;
    s32 angle;
    s32 octant;
    s32 relative;
    s32 i;
    s32 j;

    talked = 0;
    priority = 7;
    py = WHOLE(player->position[1]);
    head = py - (u16)player->height;
    touch = (u16)player->gravity.s.whole + 8;
    talk = (u16)player->gravity.s.whole + 0x20;
    facing = player->heading_goal & 0xFFF;
    px = WHOLE(player->position[0]);
    pz = WHOLE(player->position[2]);
    for (i = 0; i < D_800ADBFC; i++) {
        other = D_800AF880.components.descriptors[i].actor;
        event = 0xFF;
        if ((other->flags & 1) || player->unk074 == i) {
            goto insert;
        }
        top = WHOLE(other->position[1]) + other->unk62;
        if (other->layer_flags & 0x180) {
            if (other->layer_flags & 0x100) {
                if ((D_800C2694 & 0x20) && talked == 0 && !(other->layer_flags & 0x4000000)) {
                    if (!(other->flags & 0x220000) && (s16)D_800B2078.open_windows == 0) {
                        talked = 1;
                        event = 2;
                        priority = 3;
                        offset.vx = WHOLE(other->position[0]) - px + other->unk60;
                        offset.vz = WHOLE(other->position[2]) - pz + other->unk64;
                        angle = -ratan2(offset.vz, offset.vx);
                        other->state.word = (other->state.word & ~0xE00) | (angle & 0xE00);
                    }
                } else if (!(other->flags & 0xA20000)) {
                    event = 3;
                    priority = 4;
                    offset.vx = WHOLE(other->position[0]) - px + other->unk60;
                    offset.vz = WHOLE(other->position[2]) - pz + other->unk64;
                    octant = -(ratan2(offset.vz, offset.vx) >> 9) & 7;
                    other->state.bits.octant = octant;
                    if (D_800ADF64 == 0 && (other->flags & 0x8000000)) {
                        D_800ADF64 = 1;
                        descriptor->model->velocity[1] = 0;
                    }
                }
            } else {
                D_800ADF64 = 0;
            }
        }
        if (other->flags & 0x2000) {
            if (top < head || py < top - (u16)other->height || i == index
                || func_8008237C(px, pz, (FieldBox *)other, 0x10) != 0) {
                goto insert;
            }
            if ((D_800C2694 & 0x20) && talked == 0 && !(other->layer_flags & 0x4000000)) {
                if (other->flags & 0x220000) {
                    goto insert;
                }
                if ((s16)D_800B2078.open_windows != 0) {
                    goto insert;
                }
                offset.vx = WHOLE(other->position[0]) - px + other->unk60;
                offset.vz = WHOLE(other->position[2]) - pz + other->unk64;
                angle = -ratan2(offset.vz, offset.vx);
                octant = (angle >> 9) & 7;
                relative = (facing - (angle & 0xFFF)) & 0xFFF;
                if ((other->layer_flags & 0x40000) && (u32)(relative - 0x2BC) < 0xA89) {
                    goto insert;
                }
                talked = 1;
                event = 2;
                priority = 3;
                other->state.bits.octant = octant;
                if (D_800C268C == 0) {
                    D_80285988 = 1;
                }
            } else if (!(other->flags & 0xA20000)) {
                event = 3;
                priority = 4;
                offset.vx = WHOLE(other->position[0]) - px + other->unk60;
                offset.vz = WHOLE(other->position[2]) - pz + other->unk64;
                octant = -(ratan2(offset.vz, offset.vx) >> 9) & 7;
                other->state.bits.octant = octant;
                if (D_800C268C == 0) {
                    D_80285988 = 1;
                }
            }
        } else {
            offset.vx = WHOLE(other->position[0]) - px + other->unk60;
            offset.vz = WHOLE(other->position[2]) - pz + other->unk64;
            offset.vy = talk + (u16)other->gravity.s.whole;
            gte_ldlvl(&offset);
            gte_sqr0();
            gte_stlvnl(&square);
            if (square.vx + square.vz >= square.vy || top < head || py < top - (u16)other->height || i == index) {
                goto insert;
            }
            offset.vx = WHOLE(other->position[0]) - px + other->unk60;
            offset.vz = WHOLE(other->position[2]) - pz + other->unk64;
            func_8004A414(&offset, &square);
            reach.vx = touch + (u16)other->gravity.s.whole;
            distance = square.vx + square.vz;
            reach.vz = talk + (u16)other->gravity.s.whole;
            func_8004A414(&reach, &reach_square);
            if (distance < reach_square.vz && (D_800C2694 & 0x20) && talked == 0
                && !(other->layer_flags & 0x4000000)) {
                if (other->flags & 0x220000) {
                    goto insert;
                }
                angle = -ratan2(offset.vz, offset.vx);
                octant = (angle >> 9) & 7;
                relative = (facing - (angle & 0xFFF)) & 0xFFF;
                if ((u32)(relative - 0x2BC) < 0xA89 || (s16)D_800B2078.open_windows != 0) {
                    goto insert;
                }
                talked = 1;
                event = 2;
                priority = 3;
                other->state.bits.octant = octant;
            } else if (!(other->flags & 0xA20000) && distance < reach_square.vx) {
                event = 3;
                priority = 4;
                octant = -(ratan2(offset.vz, offset.vx) >> 9) & 7;
                other->state.bits.octant = octant;
            }
        }
    insert:
        if (event != 0xFF) {
            for (j = 0; j < 8; j++) {
                if (other->slots[j].tag == (u8)event) {
                    break;
                }
            }
            if (j == 8) {
                for (j = 0; j < 8; j++) {
                    if (other->slots[j].priority == 15 && !other->slots[j].unk22) {
                        other->slots[j].resume_pc = func_800A3090(i, event);
                        other->slots[j].tag = event;
                        other->slots[j].priority = priority;
                        other->heading = other->heading_goal = other->heading_goal | 0x8000;
                        break;
                    }
                }
            }
        }
    }
}

/* The controlled actor's contacts: against every other actor (its floor
 * polygon, its box, or its radius) either ride it, stand below it or push
 * it, recording the lowest ceiling; then remember the ridden actor, and
 * integrate the actor's own position against the floor. */
void func_80084158(s32 index, FieldDescriptor *descriptor, FieldActor *actor) {
    VECTOR next;
    SVECTOR cell;
    VECTOR normal;
    s32 top;
    s32 x;
    s32 z;
    s32 head;
    u32 entry_flags;
    s32 linked;
    s32 status;
    s32 link;
    s32 *scratch;
    FieldActor *other;
    u32 flags;
    u32 layer_flags;
    u32 cleared;
    s32 bottom;
    s32 y;
    s32 lowest;
    s32 u;

    scratch = (s32 *)func_8007CD3C(0x20);
    next.vx = actor->position[0];
    next.vy = actor->position[1];
    next.vz = actor->position[2];
    next.vx += actor->unk030[0];
    next.vy += actor->unk030[1];
    next.vz += actor->unk030[2];
    func_800831D0(&cell, &next);
    x = cell.vx;
    y = actor->position[1] >> 16;
    head = y - (u16)actor->height;
    status = 0;
    z = cell.vz;
    lowest = 0x7FFFFFFF;
    linked = 0;
    entry_flags = actor->flags;
    link = actor->unk074;
    for (u = 0; u < D_800ADBFC; u++) {
        if (u == index) {
            continue;
        }
        if (D_800AF880.components.descriptors[u].actor->flags & 1) {
            continue;
        }
        other = D_800AF880.components.descriptors[u].actor;
        flags = other->flags;
        layer_flags = other->layer_flags;
        other->layer_flags = layer_flags & 0xFFFF3EFF;
        if (layer_flags & 0x80) {
            if (func_80083288(u, (PolyModel *)D_800AF880.components.descriptors[u].instance->mesh, x, z, &top, &normal) != 0) {
                other->layer_flags &= 0xFF3FFFFF;
                continue;
            }
            if (D_800C268C == 0) {
                func_800379C8("POLYCHECK %d\n", u);
            }
            other->layer_flags |= 0x100;
            bottom = top + (u16)other->height;
            if (actor->unk074 == u) {
                actor->unk50[0] = normal.vx;
                actor->unk50[1] = normal.vy;
                actor->unk50[2] = normal.vz;
                other->layer_flags |= 0x4000;
                goto owned;
            }
        } else {
            if (flags & 0x2000) {
                if (func_8008237C(x, z, (FieldBox *)other, 0) != 0) {
                    other->layer_flags &= 0xFF3FFFFF;
                    continue;
                }
            } else {
                scratch[0] = ((other->position[0] + other->unk030[0]) >> 16) - x;
                scratch[2] = ((other->position[2] + other->unk030[2]) >> 16) - z;
                scratch[1] = (u16)actor->gravity.s.whole + (u16)other->gravity.s.whole;
                func_8004A414((VECTOR *)scratch, (VECTOR *)(scratch + 4));
                if (scratch[4] + scratch[6] >= scratch[5]) {
                    other->layer_flags &= 0xFF3FFFFF;
                    continue;
                }
            }
            if (actor->unk014 & 0x400000) {
                if (D_800C268C == 0) {
                    func_800379C8("HITOFF\n");
                }
                continue;
            }
            if ((flags | entry_flags) & 0x80) {
                continue;
            }
            if (D_800B2078.party_processing_mode != 0) {
                continue;
            }
            bottom = other->position[1] >> 16;
            top = bottom - (u16)other->height;
        owned:
            if (actor->unk074 == u) {
                if ((entry_flags & 0x40800) == 0) {
                    goto ride;
                }
            }
        }
        if (bottom >= head && y >= top) {
            if (y < top + 0x10 || (other->layer_flags & 0x800000)) {
            ride:
                lowest = top;
                other->layer_flags |= 0x800000;
                actor->unk40[0] = other->unk030[0];
                actor->unk40[1] = other->unk030[1];
                status = 2;
                actor->unk40[2] = other->unk030[2];
                if ((entry_flags & 0x40800) == 0) {
                    actor->unk074 = u;
                    linked = 1;
                }
            } else {
                if (!(flags & 0x10)) {
                    if (other->unk0E3 < 0x30) {
                        other->unk0E3 += 2;
                    }
                    if (other->unk0E3 > 0x20) {
                        other->unk40[0] += actor->unk030[0] / 4;
                        other->unk40[2] += actor->unk030[2] / 4;
                        actor->unk030[0] = 0;
                        actor->unk030[1] = 0;
                        actor->unk030[2] = 0;
                        actor->unk40[0] = actor->unk030[0] / 4;
                        actor->unk40[1] = 0;
                        actor->unk40[2] = actor->unk030[2] / 4;
                        goto mark;
                    }
                }
                other->unk40[0] = 0;
                other->unk40[1] = 0;
                other->unk40[2] = 0;
                other->unk030[0] = 0;
                other->unk030[1] = 0;
                other->unk030[2] = 0;
                actor->unk030[0] = 0;
                actor->unk030[1] = 0;
                actor->unk030[2] = 0;
                actor->unk40[0] = 0;
                actor->unk40[1] = 0;
                actor->unk40[2] = 0;
            }
        } else {
            cleared = other->layer_flags & ~0x100;
            other->layer_flags = cleared;
            if (y < top) {
                other->layer_flags = cleared | 0x800000;
                if (top < lowest) {
                    lowest = top;
                }
            } else {
                other->layer_flags = cleared & 0xFF7FFFFF;
            }
        }
    mark:
        other->layer_flags |= 0x400000;
    }
    if (D_800ADB98 != 0) {
        lowest = D_800ADB94;
        linked = 0;
        status++;
    }
    if (!linked) {
        actor->unk074 = 0xFF;
    } else {
        D_800AF880.components.descriptors[actor->unk074].actor->layer_flags |= 0x8000;
        if (link == 0xFF) {
            if (!(actor->unk134 & 0x80)) {
                actor->link = func_80031BDC(sizeof(PlatformLink), 0);
                actor->unk134 |= 0x80;
            }
            actor->link->rotation.vx = D_800AF880.components.descriptors[actor->unk074].rotation.vx;
            actor->link->rotation.vy = D_800AF880.components.descriptors[actor->unk074].rotation.vy;
            actor->link->rotation.vz = D_800AF880.components.descriptors[actor->unk074].rotation.vz;
            actor->link->radius = func_800825AC(index, actor->unk074);
        }
    }
    if (!(actor->flags & 0x10000) && !(actor->layer_flags & 0x200000)) {
        func_80084A40(index, lowest, descriptor, actor, status);
    }
    if (D_800AF880.components.descriptors[index].model->animation->unk0C == 1) {
        func_80035DB0();
        actor->flags &= ~0x800;
    }
    func_8007CD60(0x20);
}

/* 0 when the actor may idle: no motion, collision or script state is
 * pending and none of its disabled layers (layer flag bits 0-2) is the
 * layer it stands on; -1 otherwise. */
s32 func_8008492C(FieldActor *actor) {
    if (!(actor->unk014 & 0x420000) && D_800ADB98 == 0 && actor->unk030[0] == 0 && actor->unk030[1] == 0 &&
        actor->unk030[2] == 0 && D_800ADC0C == 1 && actor->unk074 == 0xFF && !(actor->flags & 0x401800)) {
        if ((actor->layer_flags & 1) && actor->layer == 0) {
            return -1;
        }
        if ((actor->layer_flags & 2) && actor->layer == 1) {
            return -1;
        }
        if ((actor->layer_flags & 4) && actor->layer == 2) {
            return -1;
        }
        return 0;
    }
    return -1;
}

/* Move an actor to its next position: query every collision layer's floor
 * and ceiling there, sort the layers by floor, choose the actor's layer,
 * commit the planar move (or roll it back on a blocking attribute or a
 * ceiling), then fall, land or rise onto the floor, and record the
 * controlled actor's history. Returns -1 when the actor did not move.
 * NON_MATCHING: only register choice differs: the original keeps the
 * position height (y) in $a0 and the floor-scan pointer in $a1; ours swaps them. */
#ifdef NON_MATCHING
s32 func_80084A40(s32 index, s32 lowest, FieldDescriptor *descriptor, FieldActor *actor, s32 status) {
    s32 floors[4];
    s32 uppers[4];
    s32 ids[4];
    s16 triangles[4];
    VECTOR normals[4];
    VECTOR old;
    s16 old_triangles[4];
    FieldModel *sprite;
    s16 old_layer;
    s32 old_floor;
    s32 layer;
    s32 i;
    s32 k;
    s32 swap;
    s32 y;
    s32 bump;
    u32 attributes;
    u32 layers;

    sprite = D_800AF880.components.descriptors[index].model;
    if (index == D_800B2078.controlled) {
        D_800ADB00 = 0xFFFF;
    }
    if (actor->flags & 0x01000000) {
        return -1;
    }
    if (actor->layer_flags & 0x200000) {
        return -1;
    }
    if (actor->flags & 0x10000) {
        return -1;
    }
    if (!(index == D_800B2078.controlled && D_800B2078.forced_position == 1) && sprite->velocity[1] == 0 &&
        func_8008492C(actor) == 0 && (s16)sprite->unk84 == actor->position[1] >> 16) {
        return -1;
    }
    old.vx = actor->position[0];
    old.vy = actor->position[1];
    old.vz = actor->position[2];
    old_layer = actor->layer;
    for (i = 0; i < 4; i++) {
        old_triangles[i] = actor->triangle[i];
    }
    for (i = 0; i < 4; i++) {
        ids[i] = i;
        floors[i] = 0x7FFFFFFF;
        uppers[i] = 0x7FFFFFFF;
    }
    for (layer = 0; layer < D_800AF880.components.layer_count - 1; layer++) {
        if (func_8007D3D4(actor, layer, &floors[layer], &normals[layer], &triangles[layer], &uppers[layer]) != 0) {
            break;
        }
    }
    if (actor->layer_flags & 1) {
        floors[0] = 0x7FFFFFFF;
        uppers[0] = 0x7FFFFFFF;
    }
    if (actor->layer_flags & 2) {
        floors[1] = 0x7FFFFFFF;
        uppers[1] = 0x7FFFFFFF;
    }
    if (actor->layer_flags & 4) {
        floors[2] = 0x7FFFFFFF;
        uppers[2] = 0x7FFFFFFF;
    }
    old_floor = floors[actor->layer];
    for (i = 0; i < 2; i++) {
        for (k = 0; k < 2; k++) {
            if (floors[k] > floors[k + 1]) {
                swap = floors[k + 1];
                floors[k + 1] = floors[k];
                floors[k] = swap;
                swap = uppers[k + 1];
                uppers[k + 1] = uppers[k];
                uppers[k] = swap;
                swap = ids[k + 1];
                ids[k + 1] = ids[k];
                ids[k] = swap;
            }
        }
    }
    if (layer == D_800AF880.components.layer_count - 1) {
        for (i = 0; i < D_800AF880.components.layer_count - 1; i++) {
            actor->triangle[i] = triangles[i];
        }
        y = actor->position[1] >> 16;
        if (y < old_floor || (actor->flags & 0x1800)) {
            for (i = 0; i < D_800AF880.components.layer_count - 1; i++) {
                if (!(floors[i] < y)) {
                    actor->layer = ids[i];
                    break;
                }
            }
        } else {
            for (i = 0; i < D_800AF880.components.layer_count - 1; i++) {
                if (actor->layer == ids[i]) {
                    break;
                }
            }
        }
        if ((func_80080968(actor) & 4) && i != 0 && actor->layer <= D_800AF880.components.layer_count - 1) {
            i--;
            actor->layer = ids[i];
        }
        attributes = func_80080968(actor);
        layers = (actor->flags >> 8) & 7;
        if (layers & (attributes >> 5)) {
            if (D_800C268C == 0) {
                func_800379C8("ERROR ID1 ACT=%d\n", index);
            }
            goto blocked;
        } else if (attributes & 0x800000) {
            if (D_800C268C == 0) {
                func_800379C8("ERROR ID0 ACT=%d\n", index);
            }
        blocked:
            if (index == D_800B2078.controlled) {
                D_800ADB00 = 0xFFF;
            }
            actor->position[1] += sprite->velocity[1];
            goto rollback;
        }
        actor->position[0] += actor->unk030[0];
        actor->position[2] += actor->unk030[2];
        for (i = 0; i < D_800AF880.components.layer_count - 1; i++) {
            if (actor->layer == ids[i]) {
                sprite->unk84 = floors[i];
                break;
            }
        }
        VectorNormal(&normals[actor->layer], (VECTOR *)actor->unk50);
    } else {
        actor->unkF0 = 0;
    }
    if (D_800ADB98 != 0) {
        if ((u32)status < 2) {
            sprite->unk84 = lowest;
        }
    } else if (status != 0) {
        if ((s16)sprite->unk84 < lowest + 10) {
            actor->unk074 = 0xFF;
        }
        sprite->unk84 = lowest;
        actor->position[1] = lowest << 16;
    }
    if (actor->flags & 0x40000) {
        actor->position[1] = actor->unkEC << 16;
        sprite->velocity[1] = 0;
    }
    actor->position[1] += sprite->velocity[1];
    attributes = func_80080968(actor);
    if (actor->layer != old_layer) {
        actor->flags &= 0xFBFFFFFF;
    }
    if (!(actor->flags & 0x04000000) && (s16)sprite->unk84 > actor->position[1] >> 16) {
        if ((s16)sprite->unk84 != actor->position[1] >> 16) {
            sprite->velocity[1] += sprite->gravity.value;
        }
        actor->flags |= 0x1000;
        actor->unkF0 = sprite->velocity[1];
    } else {
        if (!(attributes & 0x420000)) {
            actor->unkF0 = 0;
        }
        if (sprite->velocity[1] > 0) {
            sprite->velocity[1] = 0;
        }
        actor->flags &= 0xFFBFEFFF;
        actor->position[1] = (s16)sprite->unk84 << 16;
    }
    actor->flags &= 0xFBFFFFFF;
    for (i = 0; i < D_800AF880.components.layer_count - 1; i++) {
        if (floors[i] < actor->position[1] >> 16 && (actor->position[1] >> 16) - (u16)actor->height < uppers[i] &&
            floors[i] != uppers[i]) {
            break;
        }
    }
    if (i == D_800AF880.components.layer_count - 1) {
        bump = (s8)D_800AF880.components.collision_triangles[actor->layer][actor->triangle[actor->layer]].unk0D * 4;
        if (bump >= 0 ||
            !((actor->position[1] >> 16) - (u16)actor->height < bump + (s16)sprite->unk84)) {
            sprite->position[0] = actor->position[0];
            sprite->position[1] = actor->position[1];
            sprite->position[2] = actor->position[2];
            D_800AF880.components.descriptors[index].matrix.t[0] = actor->position[0] >> 16;
            D_800AF880.components.descriptors[index].matrix.t[1] = actor->position[1] >> 16;
            D_800AF880.components.descriptors[index].matrix.t[2] = actor->position[2] >> 16;
            actor->unk014 = func_80080968(actor);
            goto done;
        }
    }
    actor->position[0] = old.vx;
    actor->position[2] = old.vz;
    actor->layer = old_layer;
    actor->unkF0 = 0;
    for (i = 0; i < 4; i++) {
        actor->triangle[i] = old_triangles[i];
    }
    if ((s16)sprite->unk84 != actor->position[1] >> 16) {
        sprite->velocity[1] += sprite->gravity.value;
    }
    if (sprite->velocity[1] < 0) {
        sprite->velocity[1] = 0;
        actor->position[1] = old.vy;
    }
    sprite->position[0] = actor->position[0];
    sprite->position[1] = actor->position[1];
    sprite->position[2] = actor->position[2];
    D_800AF880.components.descriptors[index].matrix.t[1] = actor->position[1] >> 16;
done:
    func_80081C54(index);
    return 0;

rollback:
    actor->position[0] = old.vx;
    actor->position[2] = old.vz;
    actor->layer = old_layer;
    actor->unkF0 = 0;
    for (i = 0; i < 4; i++) {
        actor->triangle[i] = old_triangles[i];
    }
    if ((s16)sprite->unk84 > actor->position[1] >> 16) {
        if ((s16)sprite->unk84 != actor->position[1] >> 16) {
            sprite->velocity[1] += sprite->gravity.value;
        }
    } else {
        if (sprite->velocity[1] > 0) {
            sprite->velocity[1] = 0;
        }
        actor->flags &= 0xFFBFEFFF;
        actor->position[1] = (s16)sprite->unk84 << 16;
    }
    sprite->position[0] = actor->position[0];
    sprite->position[1] = actor->position[1];
    sprite->position[2] = actor->position[2];
    D_800AF880.components.descriptors[index].matrix.t[1] = actor->position[1] >> 16;
    func_80081C54(index);
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_8007A44C", func_80084A40);
#endif
