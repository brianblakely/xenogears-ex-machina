.include "decomp/src/resident/model_draw.s"

# Draw flat textured triangles (POLY_FT3) lit by cached face normals:
# draw mode 1 of primitive type 1. func_8002D814 recorded one face normal
# (SVECTOR, eight bytes) per face at D_80059498; this consumes one record
# per face, culled or not, and stores the advanced pointer at the end.
# Faces are projected and culled as in func_8002E010. The SXY words are
# stored around the NCLIP test, the first two also for a back face (which
# is never linked). A front-facing face is counted and, unless its AVSZ3
# OTZ is 0, lit by NCS of the cached normal and linked. Its colour word
# takes the lit RGB and keeps the packet's code byte unchanged.
# a0 = records, a1 = count; t9 = cache pointer. V0 carries the normal, so
# the next face's V0 is loaded at the loop top from t6. Exits through
# func_8002E010's shared exit with a3 = 32, the packet size. Handwritten:
# no frame, cross-routine exit, pipelined GTE work.
glabel func_8002F2E0
    model_draw_save
    model_draw_state $t7
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF
    lui     $t8, 0x0700         # POLY_FT3 tag length
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
    addiu   $s3, $s3, -32

.Llit_ft3:
    lwc2    $0, 0($t6)
    lwc2    $1, 4($t6)
    beqz    $a1, .Llit_ft3_done
     rtpt
    addiu   $a1, $a1, -1
    addiu   $a0, $a0, 8
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addiu   $s3, $s3, 32
    model_vertex0 $t6
    addiu   $t9, $t9, 8         # this face's normal ends here
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t1
    lwc2    $4, 0($t1)
    lwc2    $5, 4($t1)
    mfc2    $t0, $31            # FLAG
    mfc2    $t1, $12
    bltz    $t0, .Llit_ft3
     sltu   $t0, $t1, $v0
    mfc2    $t2, $13
    mfc2    $t3, $14
    nclip
    model_y_test 3, .Llit_ft3, .Llit_ft3_y
.Llit_ft3_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 3, .Llit_ft3, .Llit_ft3_x
.Llit_ft3_x:
     mfc2   $t0, $24            # NCLIP result
    sw      $t1, 8($s3)
    blez    $t0, .Llit_ft3
     sw     $t2, 16($s3)
    avsz3
    sw      $t3, 24($s3)
    and     $s3, $s3, $s6
    mfc2    $t0, $7             # OTZ
    addiu   $s2, $s2, 1
    beqz    $t0, .Llit_ft3
     lwc2   $0, -8($t9)         # cached normal
    lwc2    $1, -4($t9)
    ncs
    srav    $t0, $t0, $t7
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    lbu     $t1, 7($s3)         # the packet's code
    mfc2    $t2, $22            # lit RGB2
    sll     $t1, $t1, 24
    and     $t2, $t2, $s6
    or      $t1, $t1, $t2
    sw      $t1, 4($s3)
    lw      $t1, 0($t0)
    sw      $s3, 0($t0)
    or      $t1, $t1, $t8
    j       .Llit_ft3
     sw     $t1, 0($s3)

.Llit_ft3_done:
    lui     $at, %hi(D_80059498)
    sw      $t9, %lo(D_80059498)($at)
    ori     $a3, $zero, 32
    j       .Lmodel_draw_exit
     nop
endlabel func_8002F2E0
