/* One function per distinct GTE inline-asm statement of the decomp (the
 * macros of decomp/include/psyq/inline_c.h and of the battle, menu, ovl2143
 * and world map gte.h): port/asm_map.json maps each statement's template and
 * constraints to its function here, and the game-module build replaces the
 * statement with a call. Each function makes the statement's transfers in its
 * order, the words and halfwords through the pointer operands as its
 * lwc2/lw/lhu and swc2/sw/sh did, and its command words through the software
 * GTE; inputs come in operand order, a single output is the return value and
 * several outputs go to outputs[] in operand order. */
#include "xem/gte.h"

#define WORD(p, offset) (*(unsigned int *)((char *)(p) + (offset)))
#define HALF(p, offset) (*(unsigned short *)((char *)(p) + (offset)))

/* Five words into control registers first..first+4: a MATRIX's rotation
 * part, as gte_SetRotMatrix, gte_SetLightMatrix and gte_SetColorMatrix. */
static void set_matrix(void *m, int first) {
    xem_gte_write_control(first, WORD(m, 0));
    xem_gte_write_control(first + 1, WORD(m, 4));
    xem_gte_write_control(first + 2, WORD(m, 8));
    xem_gte_write_control(first + 3, WORD(m, 12));
    xem_gte_write_control(first + 4, WORD(m, 16));
}

void xem_gte_SetRotMatrix(void *m) { set_matrix(m, 0); }

void xem_gte_SetLightMatrix(void *m) { set_matrix(m, 8); }

void xem_gte_SetColorMatrix(void *m) { set_matrix(m, 16); }

void xem_gte_SetTransMatrix(void *m) {
    xem_gte_write_control(5, WORD(m, 20));
    xem_gte_write_control(6, WORD(m, 24));
    xem_gte_write_control(7, WORD(m, 28));
}

void xem_gte_SetBackColor(unsigned char r, unsigned char g, unsigned char b) {
    xem_gte_write_control(13, (unsigned int)r << 4);
    xem_gte_write_control(14, (unsigned int)g << 4);
    xem_gte_write_control(15, (unsigned int)b << 4);
}

/* The outer product's first vector into the rotation diagonal. */
void xem_gte_ldopv1(void *v) {
    xem_gte_write_control(0, WORD(v, 0));
    xem_gte_write_control(2, WORD(v, 4));
    xem_gte_write_control(4, WORD(v, 8));
}

/* Three halfwords at the given offsets into IR1-IR3. */
static void load_ir_halves(void *v, int step) {
    xem_gte_write_data(9, HALF(v, 0));
    xem_gte_write_data(10, HALF(v, step));
    xem_gte_write_data(11, HALF(v, step * 2));
}

/* A matrix column (three shorts 6 bytes apart) into IR1-IR3. */
void xem_gte_ldclmv(void *v) { load_ir_halves(v, 6); }

void xem_gte_ldsv(void *v) { load_ir_halves(v, 2); }

static void store_ir_halves(void *v, int step) {
    unsigned int x = xem_gte_read_data(9);
    unsigned int y = xem_gte_read_data(10);
    unsigned int z = xem_gte_read_data(11);

    HALF(v, 0) = (unsigned short)x;
    HALF(v, step) = (unsigned short)y;
    HALF(v, step * 2) = (unsigned short)z;
}

void xem_gte_stclmv(void *v) { store_ir_halves(v, 6); }

void xem_gte_stsv(void *v) { store_ir_halves(v, 2); }

/* A short vector (two words) into V0, V1 or V2. */
static void load_vector(void *v, int first) {
    xem_gte_write_data(first, WORD(v, 0));
    xem_gte_write_data(first + 1, WORD(v, 4));
}

void xem_gte_ldv0(void *v0) { load_vector(v0, 0); }

void xem_gte_ldv1(void *v1) { load_vector(v1, 2); }

void xem_gte_ldv2(void *v2) { load_vector(v2, 4); }

void xem_gte_ldv01(void *v0, void *v1) {
    load_vector(v0, 0);
    load_vector(v1, 2);
}

void xem_gte_ldv3(void *v0, void *v1, void *v2) {
    load_vector(v0, 0);
    load_vector(v1, 2);
    load_vector(v2, 4);
}

/* Three consecutive short vectors. */
void xem_gte_ldv3c(void *v) {
    load_vector(v, 0);
    load_vector((char *)v + 8, 2);
    load_vector((char *)v + 16, 4);
}

/* A long vector's low halves into V0. */
void xem_gte_ldlv0(void *v) {
    unsigned int y = HALF(v, 4);
    unsigned int x = HALF(v, 0);

    xem_gte_write_data(0, x | y << 16);
    xem_gte_write_data(1, WORD(v, 8));
}

void xem_gte_ldrgb(void *c) { xem_gte_write_data(6, WORD(c, 0)); }

/* IR1-IR3 from a long vector. */
void xem_gte_ldlvl(void *v) {
    xem_gte_write_data(9, WORD(v, 0));
    xem_gte_write_data(10, WORD(v, 4));
    xem_gte_write_data(11, WORD(v, 8));
}

/* The outer product's second vector, IR3 first. */
void xem_gte_ldopv2(void *v) {
    xem_gte_write_data(11, WORD(v, 8));
    xem_gte_write_data(9, WORD(v, 0));
    xem_gte_write_data(10, WORD(v, 4));
}

void xem_gte_lddp(unsigned int p) { xem_gte_write_data(8, p); }

void xem_gte_ldsz4(unsigned int z0, unsigned int z1, unsigned int z2, unsigned int z3) {
    xem_gte_write_data(16, z0);
    xem_gte_write_data(17, z1);
    xem_gte_write_data(18, z2);
    xem_gte_write_data(19, z3);
}

/* SXY0, SXY2, SXY1, in the SDK macro's order. */
void xem_gte_ldsxy3(unsigned int sxy0, unsigned int sxy1, unsigned int sxy2) {
    xem_gte_write_data(12, sxy0);
    xem_gte_write_data(14, sxy2);
    xem_gte_write_data(13, sxy1);
}

/* Commands, by the instruction word the statement holds. */
void xem_gte_rtps(void) { xem_gte_execute(0x4A180001); }

void xem_gte_rtpt(void) { xem_gte_execute(0x4A280030); }

/* gte_rt, and gte_rtv0tr in both its spellings (ovl2143's word, the
 * battle's cop2 mnemonic): MAC = RT * V0 + TR. */
void xem_gte_rt(void) { xem_gte_execute(0x4A480012); }

void xem_gte_rtv0(void) { xem_gte_execute(0x4A486012); }

void xem_gte_rtir(void) { xem_gte_execute(0x4A49E012); }

void xem_gte_dpcs(void) { xem_gte_execute(0x4A780010); }

void xem_gte_sqr0(void) { xem_gte_execute(0x4AA00428); }

void xem_gte_nccs(void) { xem_gte_execute(0x4B08041B); }

void xem_gte_nclip(void) { xem_gte_execute(0x4B400006); }

void xem_gte_avsz3(void) { xem_gte_execute(0x4B58002D); }

void xem_gte_avsz4(void) { xem_gte_execute(0x4B68002E); }

void xem_gte_op0(void) { xem_gte_execute(0x4B70000C); }

void xem_gte_op12(void) { xem_gte_execute(0x4B78000C); }

void xem_gte_gpf0(void) { xem_gte_execute(0x4B90003D); }

void xem_gte_gpf12(void) { xem_gte_execute(0x4B98003D); }

/* Results. */
void xem_gte_stflg(void *flag) { WORD(flag, 0) = xem_gte_read_control(31); }

/* SZ3 / 4 as an ordering-table depth. */
void xem_gte_stszotz(void *otz) { WORD(otz, 0) = (unsigned int)((int)xem_gte_read_data(19) >> 2); }

static void store(void *p, int reg) { WORD(p, 0) = xem_gte_read_data(reg); }

void xem_gte_stsxy0(void *sxy) { store(sxy, 12); }

void xem_gte_stsxy1(void *sxy) { store(sxy, 13); }

/* gte_stsxy and gte_stsxy2. */
void xem_gte_stsxy2(void *sxy) { store(sxy, 14); }

void xem_gte_stsxy01(void *sxy0, void *sxy1) {
    store(sxy0, 12);
    store(sxy1, 13);
}

void xem_gte_stsxy3(void *sxy0, void *sxy1, void *sxy2) {
    store(sxy0, 12);
    store(sxy1, 13);
    store(sxy2, 14);
}

/* The three screen points into a POLY_FT4's x0, x1 and x2. */
void xem_gte_stsxy3_ft4(void *poly) {
    store((char *)poly + 8, 12);
    store((char *)poly + 16, 13);
    store((char *)poly + 24, 14);
}

void xem_gte_stotz(void *otz) { store(otz, 7); }

void xem_gte_stdp(void *p) { store(p, 8); }

/* The menu's gte_stsz1 and gte_stsz2, and gte_stsz (SZ3). */
void xem_gte_stsz1(void *sz) { store(sz, 17); }

void xem_gte_stsz2(void *sz) { store(sz, 18); }

void xem_gte_stsz(void *sz) { store(sz, 19); }

void xem_gte_stsz3(void *sz1, void *sz2, void *sz3) {
    store(sz1, 17);
    store(sz2, 18);
    store(sz3, 19);
}

void xem_gte_stsz4c(void *sz) {
    store(sz, 16);
    store((char *)sz + 4, 17);
    store((char *)sz + 8, 18);
    store((char *)sz + 12, 19);
}

void xem_gte_strgb(void *c) { store(c, 22); }

void xem_gte_stopz(void *opz) { store(opz, 24); }

void xem_gte_stlvl(void *v) {
    store(v, 9);
    store((char *)v + 4, 10);
    store((char *)v + 8, 11);
}

void xem_gte_stlvnl(void *v) {
    store(v, 25);
    store((char *)v + 4, 26);
    store((char *)v + 8, 27);
}

/* The world map's register reads (worldmap/gte.h): SXY0-SXY2, or SXY2. */
void xem_gte_getsxy3(unsigned int *outputs) {
    outputs[0] = xem_gte_read_data(12);
    outputs[1] = xem_gte_read_data(13);
    outputs[2] = xem_gte_read_data(14);
}

unsigned int xem_gte_getsxy2(void) { return xem_gte_read_data(14); }
