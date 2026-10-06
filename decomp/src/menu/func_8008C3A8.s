# Project transformed mesh vertices along D_8009A2C8 onto the y=0 plane.
# a0: SVector inputs; a1: SVector work records; a2: positive vertex count.
# With L = the mesh light vector and (x,y,z) = the GTE MAC1..3 result:
#   work.vx = (x*L.y - L.x*y) / (L.y-y)
#   work.vz = (z*L.y - L.z*y) / (L.y-y)
# Products and sums wrap to 32 bits before signed division, and stores
# truncate to 16 bits. A zero denominator leaves that work record intact.
# work.vy/pad are always preserved. The pipeline reads one extra input.

# Signed division keeps the original divide-by-zero and overflow traps.
# t0 is the numerator/result, t2 the denominator; at is scratch.
.macro mesh_shadow_divide
    div     $zero, $t0, $t2
    bnez    $t2, .Lshadow_nonzero_\@
     nop
    break   7
.Lshadow_nonzero_\@:
    addiu   $at, $zero, -1
    bne     $t2, $at, .Lshadow_quotient_\@
     lui    $at, 0x8000
    bne     $t0, $at, .Lshadow_quotient_\@
     nop
    break   6
.Lshadow_quotient_\@:
    mflo    $t0
.endm

glabel func_8008C3A8
    lui     $t0, %hi(D_8009A2C8)
    addiu   $t0, $t0, %lo(D_8009A2C8)
    lw      $t6, 0($t0)         # L.x
    lw      $t5, 4($t0)         # L.y
    lw      $t4, 8($t0)         # L.z
    lwc2    $0, 0($a0)
    lwc2    $1, 4($a0)
    addiu   $a0, $a0, 8
.Lshadow_vertex:
    nop
    mvmva   1, 0, 0, 0, 0     # loaded rotation and translation, V0
    nop
    lwc2    $0, 0($a0)         # preload the next input
    lwc2    $1, 4($a0)
    mfc2    $t8, $26            # transformed y
    mfc2    $t9, $25            # transformed x
    subu    $t2, $t5, $t8       # L.y - y
    beqz    $t2, .Lshadow_parallel
     mult   $t9, $t5

    # x coordinate of the intersection.
    mflo    $t0
    mfc2    $t3, $27            # transformed z
    addiu   $a1, $a1, 8
    mult    $t6, $t8
    mflo    $t1
    subu    $t0, $t0, $t1
    negu    $t7, $t3
    mesh_shadow_divide
    mult    $t7, $t5
    sh      $t0, -8($a1)

    # z numerator uses two negations, preserving 32-bit wraparound.
    mflo    $t3
    addiu   $a2, $a2, -1
    addiu   $a0, $a0, 8
    mult    $t4, $t8
    mflo    $t1
    addu    $t0, $t3, $t1
    negu    $t0, $t0
    mesh_shadow_divide
    bnez    $a2, .Lshadow_vertex
     sh     $t0, -4($a1)
    jr      $ra
     nop

.Lshadow_parallel:
    addiu   $a2, $a2, -1
    addiu   $a0, $a0, 8
    bnez    $a2, .Lshadow_vertex
     addiu  $a1, $a1, 8
    jr      $ra
     nop
endlabel func_8008C3A8
