# Set the environment-map texture coordinate mapping of model_draw_ft3_envmap by
# rewriting its code: a0 = u shift and a1 = v shift (0..31) replace the sa
# fields of its six srl instructions; a2 = u offset and a3 = v offset
# replace the immediates of its six addiu instructions. The image holds
# (6, 6, 0x40, 0x40); callers pass (5, 4, 0x40, 0x40), (2, 2, 0x40, 0x40)
# and (1, 1, 0x40, 0x40). The instructions are addressed by their offsets
# from model_envmap_patch_base, named after the labels in model_draw_ft3_envmap.s. This routine
# does not flush the instruction cache. Clobbers a0, a1, t0, t1.
# Handwritten: self-modifying code.

# The low halfword of a little-endian srl holds funct, sa (bits 6-10) and
# rd; replace sa with the pre-shifted count.
.macro envmap_patch_shift offset, count
    lh      $t1, \offset($t0)
    nop
    andi    $t1, $t1, 0xF83F
    or      $t1, $t1, \count
    sh      $t1, \offset($t0)
.endm

glabel model_set_envmap_mapping
    sll     $a0, $a0, 6         # shift counts in sa position
    sll     $a1, $a1, 6
    lui     $t0, %hi(model_envmap_patch_base)
    addiu   $t0, $t0, %lo(model_envmap_patch_base)
    envmap_patch_shift 0x30, $a0    # .Lenvmap_u0_shift
    envmap_patch_shift 0x5C, $a0    # .Lenvmap_u1_shift
    envmap_patch_shift 0x7C, $a0    # .Lenvmap_u2_shift
    envmap_patch_shift 0x3C, $a1    # .Lenvmap_v0_shift
    envmap_patch_shift 0x68, $a1    # .Lenvmap_v1_shift
    envmap_patch_shift 0x88, $a1    # .Lenvmap_v2_shift
    # An addiu's immediate is its low halfword.
    sh      $a2, 0x34($t0)      # .Lenvmap_u0_offset
    sh      $a2, 0x60($t0)      # .Lenvmap_u1_offset
    sh      $a2, 0x80($t0)      # .Lenvmap_u2_offset
    sh      $a3, 0x40($t0)      # .Lenvmap_v0_offset
    sh      $a3, 0x6C($t0)      # .Lenvmap_v1_offset
    sh      $a3, 0x8C($t0)      # .Lenvmap_v2_offset
    jr      $ra
     nop
endlabel model_set_envmap_mapping
