# Transform SVector *a0 with the loaded GTE rotation matrix, then scale
# the result through IR0 (a2). Store the three saturated IR components in
# SVector *a1. Translation is disabled by mvmva's cv=3 selector.
glabel func_8008DDFC
    lwc2    $0, 0($a0)           # VXY0
    lwc2    $1, 4($a0)           # VZ0
    mtc2    $a2, $8             # IR0
    nop
    nop
    mvmva   1, 0, 0, 3, 0

    # Route MAC1..3 back through the signed IR1..3 inputs before scaling.
    mfc2    $t0, $25            # MAC1
    mfc2    $t1, $26            # MAC2
    mfc2    $t2, $27            # MAC3
    mtc2    $t0, $9             # IR1
    mtc2    $t1, $10            # IR2
    mtc2    $t2, $11            # IR3
    nop
    nop
    gpf     1
    mfc2    $t0, $9
    mfc2    $t1, $10
    mfc2    $t2, $11
    sh      $t0, 0($a1)
    sh      $t1, 2($a1)
    jr      $ra
     sh     $t2, 4($a1)
endlabel func_8008DDFC
