# Scale the three columns of Matrix *a0 by the signed halfword scales at
# a1, with a 12-bit fractional shift and GTE IR saturation. Load all of
# the original rotation and scales before the first in-place matrix store.
# Matrix padding and translation are untouched. The GTE rotation retains
# the original unscaled matrix; its IR values finish on the last column.
glabel func_800731F8
    lw      $t1, 0($a0)
    lw      $t2, 4($a0)
    ctc2    $t1, $0             # packed R11/R12
    ctc2    $t2, $1             # packed R13/R21
    lw      $t3, 8($a0)
    lw      $t4, 12($a0)
    lw      $t5, 16($a0)
    ctc2    $t3, $2             # packed R22/R23
    ctc2    $t4, $3             # packed R31/R32
    lhu     $t4, 0($a1)
    ctc2    $t5, $4             # R33

    # R * (scale.x, 0, 0), with translation disabled.
    mtc2    $t4, $9
    mtc2    $zero, $10
    mtc2    $zero, $11
    lhu     $t5, 2($a1)
    lhu     $t6, 4($a1)
    mvmva   1, 0, 3, 3, 0
    mfc2    $t1, $9
    mfc2    $t2, $10
    mfc2    $t3, $11

    # R * (0, scale.y, 0). Store the first column during this command.
    mtc2    $zero, $9
    mtc2    $t5, $10
    mtc2    $zero, $11
    sh      $t1, 0($a0)
    nop
    mvmva   1, 0, 3, 3, 0
    sh      $t2, 6($a0)
    sh      $t3, 12($a0)
    mfc2    $t1, $9
    mfc2    $t2, $10
    mfc2    $t3, $11

    # R * (0, 0, scale.z). Store the second column during this command.
    mtc2    $zero, $9
    mtc2    $zero, $10
    mtc2    $t6, $11
    sh      $t1, 2($a0)
    sh      $t2, 8($a0)
    mvmva   1, 0, 3, 3, 0
    sh      $t3, 14($a0)
    mfc2    $t1, $9
    mfc2    $t2, $10
    mfc2    $t3, $11
    sh      $t1, 4($a0)
    sh      $t2, 10($a0)
    jr      $ra
     sh     $t3, 16($a0)
endlabel func_800731F8
