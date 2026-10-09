.include "decomp/src/resident/model_draw.s"

# Draw flat textured quads (POLY_FT4) lit by cached face normals: draw
# mode 1 of primitive type 9. func_8002D530 recorded one face normal
# (eight bytes) per face at D_80059498; this consumes one per face, culled
# or not, and stores the advanced pointer at the end.
# Quads are projected and culled as in func_8002E010. A quad passing the
# tests is counted and, unless its AVSZ4 OTZ is 0, lit by NCS of the
# cached normal, written (four SXY words, the lit RGB with the packet's
# own code byte) and linked.
# a0 = records, a1 = count; t9 = cache pointer. Inside the loop, index 0
# is masked to thirteen bits like index 1 (the first face's is not). SXY2
# is read twice and the first y test is computed into t5 before the second
# error test. The old OT word is held in s1. Exits through func_8002E010's
# shared exit with a3 = 40, the packet size. Handwritten: no frame,
# cross-routine exit, pipelined GTE work.
glabel func_8002FAE8
    lui     $t8, 0x0900         # POLY_FT4 tag length
    model_draw_save
    model_draw_state $t7
    lui     $t9, %hi(D_80059498)
    lw      $t9, %lo(D_80059498)($t9)
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    model_vertex0 $t6
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    addiu   $s3, $s3, -40
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF

.Llit_ft4:
    lwc2    $0, 0($t6)
    lwc2    $1, 4($t6)
    beqz    $a1, .Llit_ft4_done
     rtpt
    addiu   $a1, $a1, -1
    addiu   $a0, $a0, 8
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addiu   $s3, $s3, 40
    sll     $t6, $t4, 3         # index 0, thirteen bits
    andi    $t6, $t6, 0xFFF8
    addu    $t6, $t6, $s0
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    addiu   $t9, $t9, 8         # this face's normal ends here
    mfc2    $t0, $31            # LZCR (data register 31), not FLAG
    nclip
    bltz    $t0, .Llit_ft4
     and    $s3, $s3, $s6
    lhu     $t0, -2($a0)        # index 3
    mfc2    $t1, $12
    mfc2    $t4, $24            # NCLIP result
    sll     $t0, $t0, 3
    blez    $t4, .Llit_ft4
     addu   $t0, $t0, $s0
    mfc2    $t3, $14
    mfc2    $t2, $13
    lwc2    $0, 0($t0)          # the fourth point
    lwc2    $1, 4($t0)
    mfc2    $t3, $14
    rtps
    sltu    $t5, $t1, $v0
    mfc2    $t0, $31            # LZCR (data register 31), not FLAG
    mfc2    $t4, $14
    bltz    $t0, .Llit_ft4
     avsz4
    model_y_test 4, .Llit_ft4, .Llit_ft4_y, $t5
.Llit_ft4_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 4, .Llit_ft4, .Llit_ft4_x
.Llit_ft4_x:
     mfc2   $t0, $7             # OTZ
    addiu   $s2, $s2, 1
    beqz    $t0, .Llit_ft4
     srav   $t0, $t0, $t7
    lwc2    $0, -8($t9)         # cached normal
    lwc2    $1, -4($t9)
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    ncs
    lw      $s1, 0($t0)
    lbu     $t5, 7($s3)         # the packet's code
    sw      $t1, 8($s3)
    sw      $t2, 16($s3)
    sw      $t3, 24($s3)
    sw      $t4, 32($s3)
    mfc2    $t2, $22            # lit RGB2
    sll     $t1, $t5, 24
    and     $t2, $t2, $s6
    or      $t1, $t1, $t2
    or      $t2, $s1, $t8
    sw      $t1, 4($s3)
    sw      $s3, 0($t0)
    j       .Llit_ft4
     sw     $t2, 0($s3)

.Llit_ft4_done:
    lui     $at, %hi(D_80059498)
    sw      $t9, %lo(D_80059498)($at)
    ori     $a3, $zero, 40
    j       .Lmodel_draw_exit
     nop
endlabel func_8002FAE8
