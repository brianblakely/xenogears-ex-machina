.include "decomp/src/resident/model_draw.s"

# Draw flat quads (POLY_F4) lit from the lit-colour cache: draw mode 1 of
# primitive type 8. func_8002CF58 recorded twelve bytes per face at
# D_80059498 (colour word, then face normal); this consumes one record
# per face, culled or not, and stores the advanced pointer at the end.
# Quads are projected and culled as in func_8002E010 (error test, NCLIP
# of the first three points, error test, bounds test of four points). A
# quad passing them is counted and, unless its AVSZ4 OTZ is 0, lit by
# NCCS (RGBC = the cached colour, V0 = the cached normal), written (four
# SXY words, the lit RGB with the cached code byte) and linked.
# a0 = records, a1 = count; a2 = cache pointer. Inside the loop, index 0
# is masked to thirteen bits like index 1 (the first face's is not). The
# old OT word is held in s1. Exits through func_8002E010's shared exit
# with a3 = 24, the packet size. Handwritten: no frame, cross-routine
# exit, pipelined GTE work.
glabel func_8002F8D0
    lui     $t8, 0x0500         # POLY_F4 tag length
    model_draw_save
    model_draw_state $t7
    lui     $a2, %hi(D_80059498)
    lw      $a2, %lo(D_80059498)($a2)
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    model_vertex0 $t6
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    addiu   $s3, $s3, -24
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF

.Llit_f4:
    lwc2    $0, 0($t6)
    lwc2    $1, 4($t6)
    beqz    $a1, .Llit_f4_done
     rtpt
    addiu   $a1, $a1, -1
    addiu   $a0, $a0, 8
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addiu   $s3, $s3, 24
    sll     $t6, $t4, 3         # index 0, thirteen bits
    andi    $t6, $t6, 0xFFF8
    addu    $t6, $t6, $s0
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    addiu   $a2, $a2, 12        # this face's cache record ends here
    mfc2    $t0, $31            # LZCR (data register 31), not FLAG
    nclip
    bltz    $t0, .Llit_f4
     and    $s3, $s3, $s6
    lhu     $t5, -2($a0)        # index 3
    mfc2    $t1, $12
    mfc2    $t4, $24            # NCLIP result
    sll     $t0, $t5, 3
    blez    $t4, .Llit_f4
     addu   $t0, $t0, $s0
    mfc2    $t2, $13
    lwc2    $0, 0($t0)          # the fourth point
    lwc2    $1, 4($t0)
    mfc2    $t3, $14
    rtps
    mfc2    $t0, $31            # LZCR (data register 31), not FLAG
    mfc2    $t4, $14
    bltz    $t0, .Llit_f4
     avsz4
    sltu    $t0, $t1, $v0
    model_y_test 4, .Llit_f4, .Llit_f4_y
.Llit_f4_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 4, .Llit_f4, .Llit_f4_x
.Llit_f4_x:
     mfc2   $t0, $7             # OTZ
    addiu   $s2, $s2, 1
    beqz    $t0, .Llit_f4
     srav   $t0, $t0, $t7
    lw      $t5, -12($a2)       # cached colour
    lwc2    $0, -8($a2)         # cached normal
    lwc2    $1, -4($a2)
    mtc2    $t5, $6             # RGBC
    nccs
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    lw      $s1, 0($t0)
    sw      $t1, 8($s3)
    sw      $t2, 12($s3)
    sw      $t3, 16($s3)
    sw      $t4, 20($s3)
    mfc2    $t2, $22            # lit RGB2
    lui     $at, 0xFF00         # the cached colour's code byte
    and     $t5, $t5, $at
    and     $t2, $t2, $s6
    or      $t1, $t5, $t2
    sw      $t1, 4($s3)
    sw      $s3, 0($t0)
    or      $t1, $s1, $t8
    j       .Llit_f4
     sw     $t1, 0($s3)

.Llit_f4_done:
    lui     $at, %hi(D_80059498)
    sw      $a2, %lo(D_80059498)($at)
    ori     $a3, $zero, 24
    j       .Lmodel_draw_exit
     nop
endlabel func_8002F8D0
