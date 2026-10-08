.include "decomp/src/resident/model_draw.s"

# Draw depth-cued flat textured quads (POLY_FT4) ordered by their farthest
# point: draw mode 5 of primitive types 9 and 13. As in func_8002FCFC,
# RGBC holds the model colour D_80059598 and the colour word becomes its
# DPCS result (by the IR0 of the fourth point) with the packet's code less
# bit 0; DPCS is issued as soon as the y test passes. The depth is the
# largest of SZ0..SZ3 (signed comparisons, no zero test per point) shifted
# by D_80050100 + 2. A quad passing the bounds test is written and
# counted, and left unlinked at depth 0.
# a0 = records, a1 = count; takes func_8002E010's shared exit directly.
# Handwritten: no frame, cross-routine exit, pipelined GTE work.
glabel func_8002FF0C
    model_packet 8, 9           # POLY_FT4
    model_draw_save
    model_draw_state $t7
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addi    $t7, $t7, 2
    model_vertex0 $t6
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    lui     $t0, %hi(D_80059598)
    lw      $t0, %lo(D_80059598)($t0)
    subu    $s3, $s3, $a3
    mtc2    $t0, $6             # RGBC
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF

.Lfar_ft4:
    lwc2    $0, 0($t6)
    lwc2    $1, 4($t6)
    beqz    $a1, .Lmodel_draw_exit
     rtpt
    addiu   $a1, $a1, -1
    addiu   $a0, $a0, 8
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addu    $s3, $s3, $a3
    model_vertex0 $t6
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    mfc2    $t0, $31            # RTPT flags
    mfc2    $t1, $12
    bltz    $t0, .Lfar_ft4
     mfc2   $t2, $13
    mfc2    $t3, $14
    nclip
    lhu     $t5, -2($a0)        # index 3
    and     $s3, $s3, $s6
    mfc2    $t4, $24            # NCLIP result
    sll     $t0, $t5, 3
    blez    $t4, .Lfar_ft4
     addu   $t0, $t0, $s0
    lwc2    $0, 0($t0)          # the fourth point
    lwc2    $1, 4($t0)
    rtps
    mfc2    $t0, $31
    mfc2    $t4, $14
    bltz    $t0, .Lfar_ft4
     sltu   $t0, $t1, $v0
    model_y_test 4, .Lfar_ft4, .Lfar_ft4_y
.Lfar_ft4_y:
     dpcs
    andi    $t0, $t1, 0xFFFF
    model_x_test 4, .Lfar_ft4, .Lfar_ft4_x
.Lfar_ft4_x:
     sw     $t1, 8($s3)
    addu    $t0, $s3, $t9
    sw      $t2, 8($t0)
    addu    $t0, $t0, $t9
    mfc2    $t1, $16            # SZ0..SZ3: the four points
    sw      $t3, 8($t0)
    addu    $t0, $t0, $t9
    mfc2    $t2, $17
    sw      $t4, 8($t0)
    slt     $t0, $t2, $t1
    bnez    $t0, .Lfar_ft4_depth1
     mfc2   $t3, $18
    addu    $t1, $t2, $zero
.Lfar_ft4_depth1:
    lbu     $t5, 7($s3)         # the packet's code
    mfc2    $t4, $19
    slt     $t0, $t3, $t1
    bnez    $t0, .Lfar_ft4_depth2
     mfc2   $t2, $22            # depth-cued RGB2
    addu    $t1, $t3, $zero
.Lfar_ft4_depth2:
    sll     $t5, $t5, 24
    slt     $t0, $t4, $t1
    bnez    $t0, .Lfar_ft4_depth3
     addiu  $s2, $s2, 1
    addu    $t1, $t4, $zero
.Lfar_ft4_depth3:
    lui     $at, 0xFE00         # code without bit 0 (raw texture)
    and     $t5, $t5, $at
    beqz    $t1, .Lfar_ft4
     srav   $t0, $t1, $t7
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    and     $t2, $t2, $s6
    or      $t5, $t5, $t2
    sw      $t5, 4($s3)
    lw      $t1, 0($t0)
    sw      $s3, 0($t0)
    or      $t1, $t1, $t8
    j       .Lfar_ft4
     sw     $t1, 0($s3)
endlabel func_8002FF0C
