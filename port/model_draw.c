/*
 * The resident model primitive renderers and the environment-map patcher
 * (decomp/src/resident/model_draw*.s, model_depth.s,
 * model_set_envmap_mapping.s), as portable C on the software GTE.
 */
#include "common.h"
#include "xem/gte_ops.h"

/* model_draw_ft3_envmap's patch base (800308D0): the code of its texture
 * coordinate mapping, which model_set_envmap_mapping rewrites. */
extern u8 model_envmap_patch_base[];

/* Offsets from model_envmap_patch_base of model_draw_ft3_envmap's six srl
 * instructions (u0, u1, u2, then v0, v1, v2) and of the addiu after each. */
static const u8 xem_envmap_u_shifts[3] = { 0x30, 0x5C, 0x7C };
static const u8 xem_envmap_v_shifts[3] = { 0x3C, 0x68, 0x88 };
#define XEM_ENVMAP_OFFSET(shift) ((shift) + 4)

/* Replace the sa field (bits 6-10) of the srl at `address` with `count`, as
 * the original's halfword read, mask, or and store do. */
static void xem_envmap_patch_shift(u32 address, u32 count) {
    XEM_U16(address) = (u16)((XEM_U16(address) & 0xF83F) | count);
}

/* Set the environment-map mapping by rewriting model_draw_ft3_envmap's code
 * in game memory: the shift counts u_shift and v_shift (0..31) into its six
 * srl instructions, the offsets into the immediates of its six addiu. The
 * image holds (6, 6, 0x40, 0x40). */
void model_set_envmap_mapping(s32 u_shift, s32 v_shift, s32 u_offset, s32 v_offset) {
    u32 base = XEM_ADDRESS_OF(model_envmap_patch_base[0]);
    u32 u = (u32)u_shift << 6;
    u32 v = (u32)v_shift << 6;
    s32 i;

    for (i = 0; i < 3; i++) {
        xem_envmap_patch_shift(base + xem_envmap_u_shifts[i], u);
    }
    for (i = 0; i < 3; i++) {
        xem_envmap_patch_shift(base + xem_envmap_v_shifts[i], v);
    }
    for (i = 0; i < 3; i++) {
        XEM_U16(base + XEM_ENVMAP_OFFSET(xem_envmap_u_shifts[i])) = (u16)u_offset;
    }
    for (i = 0; i < 3; i++) {
        XEM_U16(base + XEM_ENVMAP_OFFSET(xem_envmap_v_shifts[i])) = (u16)v_offset;
    }
}
