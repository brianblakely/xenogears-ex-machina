.include "decomp/src/worldmap/screen_bounds.s"

# Draw a terrain block's billboard sprites as POLY_FT4 quads.
# a0 = eight-byte SVECTOR world positions, a1 = their count (nonzero),
# a2 = ordering table, a3 = the next free quad, whose colour, texture page
# and texture coordinates are preset. The scratchpad holds
# TerrainPassScratch: the sprite's four corners at 0x00, the view matrix at
# 0x28, the roll matrix at 0x48 and 16 depth CLUTs at 0x68.
# Each position is taken relative to the camera target D_8009BE28 (20.12);
# an x or z difference below -0x4000 gains, and one then at or above 0x4000
# loses, one map width or height (D_8009D160 by D_8009D2B4 blocks of
# 0x800). (dx, y, -dz) through the view matrix becomes the translation of
# the roll matrix, and the corners are projected with RTPT and then RTPS.
# A quad is drawn when RTPT reports no FLAG error, the first three corners
# pass screen_bounds_test and SZ3 < 0xE00. It is linked at OT entry
# SZ3 >> 4 with CLUT (IR0 >> 8), IR0 (the fourth corner's depth cue)
# limited to 0xFFF. Nothing more is drawn once the frame's quad count
# D_8009BE04 (a word here) reaches 512; it is stored back on exit.
# Saves s0/s1, s1 in the caller's a0 home slot; clobbers a0, a1, a3, v0,
# v1, t0..t9 and the GTE rotation and translation. Handwritten: saves
# beyond its 8-byte frame and ori for a small positive constant.
glabel func_80099BFC
    addiu   $sp, $sp, -8
    sw      $s0, 4($sp)
    sw      $s1, 8($sp)         # beyond the frame: the caller's a0 slot
    lui     $s1, 0x1F80         # scratchpad
    lui     $s0, %hi(D_8009BE04)
    lw      $s0, %lo(D_8009BE04)($s0)
    lui     $v0, %hi(D_8009BE28)
    lw      $v0, %lo(D_8009BE28)($v0)   # camera target x
    lui     $v1, %hi(D_8009BE28+8)
    lw      $v1, %lo(D_8009BE28+8)($v1) # camera target z
    lui     $t0, %hi(D_8009D160)
    lw      $t0, %lo(D_8009D160)($t0)   # map width in blocks
    lui     $t1, %hi(D_8009D2B4)
    lw      $t1, %lo(D_8009D2B4)($t1)   # map height in blocks
    sra     $v0, $v0, 12
    sra     $v1, $v1, 12
    sll     $t0, $t0, 11
    sll     $t1, $t1, 11
.Lbillboard_next:
    slti    $t9, $s0, 512
    beqz    $t9, .Lbillboard_done
     nop
    lw      $t8, 0($a0)         # x | y << 16
    lh      $t9, 4($a0)         # z
    sra     $t7, $t8, 16
    sll     $t8, $t8, 16
    sra     $t8, $t8, 16
    subu    $t8, $t8, $v0       # dx
    subu    $t9, $t9, $v1       # dz
    slti    $t6, $t8, -0x4000
    beqz    $t6, .Lbillboard_x_upper
     nop
    addu    $t8, $t8, $t0
.Lbillboard_x_upper:
    slti    $t6, $t8, 0x4000
    bnez    $t6, .Lbillboard_z_lower
     nop
    subu    $t8, $t8, $t0
.Lbillboard_z_lower:
    slti    $t6, $t9, -0x4000
    beqz    $t6, .Lbillboard_z_upper
     nop
    addu    $t9, $t9, $t1
.Lbillboard_z_upper:
    slti    $t6, $t9, 0x4000
    bnez    $t6, .Lbillboard_view
     nop
    subu    $t9, $t9, $t1
.Lbillboard_view:
    andi    $t8, $t8, 0xFFFF
    sll     $t7, $t7, 16
    or      $t8, $t8, $t7       # VXY0 = dx | y << 16
    negu    $t9, $t9
    andi    $t9, $t9, 0xFFFF    # VZ0 = -dz

    # Rotate by the view matrix without translation, then add its T.
    lw      $t4, 0x28($s1)
    lw      $t5, 0x2C($s1)
    ctc2    $t4, $0
    ctc2    $t5, $1
    lw      $t4, 0x30($s1)
    lw      $t5, 0x34($s1)
    lw      $t6, 0x38($s1)
    ctc2    $t4, $2
    ctc2    $t5, $3
    ctc2    $t6, $4
    mtc2    $t8, $0
    mtc2    $t9, $1
    nop
    nop
    mvmva   1, 0, 0, 3, 0
    mfc2    $t7, $25            # MAC1..3
    mfc2    $t8, $26
    mfc2    $t9, $27
    lw      $t4, 0x3C($s1)
    lw      $t5, 0x40($s1)
    lw      $t6, 0x44($s1)
    addu    $t7, $t7, $t4
    addu    $t8, $t8, $t5
    addu    $t9, $t9, $t6

    # Project the corners rotated by the roll matrix about that point.
    lw      $t4, 0x48($s1)
    lw      $t5, 0x4C($s1)
    ctc2    $t4, $0
    ctc2    $t5, $1
    lw      $t4, 0x50($s1)
    lw      $t5, 0x54($s1)
    lw      $t6, 0x58($s1)
    ctc2    $t4, $2
    ctc2    $t5, $3
    ctc2    $t6, $4
    ctc2    $t7, $5             # TRX..TRZ
    ctc2    $t8, $6
    ctc2    $t9, $7
    lwc2    $0, 0($s1)          # corners 0..2
    lwc2    $1, 4($s1)
    lwc2    $2, 8($s1)
    lwc2    $3, 12($s1)
    lwc2    $4, 16($s1)
    lwc2    $5, 20($s1)
    nop
    nop
    rtpt
    cfc2    $t9, $31            # FLAG
    mfc2    $t2, $12            # SXY0
    bltz    $t9, .Lbillboard_skip
     andi   $t8, $t2, 0xFFFF
    mfc2    $t3, $13            # SXY1
    mfc2    $t4, $14            # SXY2
    screen_bounds_test .Lbillboard_skip
    mfc2    $t6, $19            # SZ3
    nop
    sltiu   $t8, $t6, 0xE00     # also rejects a negative depth
    beqz    $t8, .Lbillboard_skip
     sra    $t6, $t6, 4
    lwc2    $0, 24($s1)         # corner 3
    lwc2    $1, 28($s1)
    sll     $t6, $t6, 2
    addu    $t6, $t6, $a2       # OT entry
    rtps
    mfc2    $t5, $14            # SXY2: corner 3
    mfc2    $t7, $8             # IR0: depth cue
    lw      $t9, 0($t6)         # previous OT entry
    sltiu   $t8, $t7, 0x1000
    bnez    $t8, .Lbillboard_fog
     nop
    ori     $t7, $zero, 0xFFF
.Lbillboard_fog:
    srl     $t7, $t7, 8
    sll     $t7, $t7, 1
    addu    $t7, $t7, $s1
    lhu     $t7, 0x68($t7)      # CLUT for this depth
    sw      $t2, 8($a3)         # xy0
    sw      $t3, 16($a3)        # xy1
    sw      $t4, 24($a3)        # xy2
    sw      $t5, 32($a3)        # xy3
    lui     $t8, 0x0900         # nine words after the tag
    or      $t9, $t9, $t8       # previous entry, not masked
    lui     $t8, 0x00FF
    ori     $t8, $t8, 0xFFFF
    and     $t8, $t8, $a3
    sw      $t8, 0($t6)         # the entry now starts with this quad
    sw      $t9, 0($a3)         # tag
    sh      $t7, 14($a3)        # CLUT
    addiu   $a3, $a3, 40
    addiu   $s0, $s0, 1
.Lbillboard_skip:
    addiu   $a1, $a1, -1
    bnez    $a1, .Lbillboard_next
     addiu  $a0, $a0, 8
.Lbillboard_done:
    lui     $at, %hi(D_8009BE04)
    sw      $s0, %lo(D_8009BE04)($at)
    lw      $s1, 8($sp)
    lw      $s0, 4($sp)
    addiu   $sp, $sp, 8
    jr      $ra
     nop
endlabel func_80099BFC
