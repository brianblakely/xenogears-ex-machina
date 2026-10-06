# Shared implementation of the menu's handwritten GTE vector scalers.
# a0 = three signed halfwords at the supplied component stride;
# a1 = packed halfword output, a2 = IR0 scale. GPF writes the saturated
# IR1..3 results; the output's fourth halfword is left untouched.
.ifndef MENU_VECTOR_SCALE_MACROS
.set MENU_VECTOR_SCALE_MACROS, 1

.macro menu_scale_vector name, stride, shift
glabel \name
    lh      $t0, 0($a0)
    lh      $t1, \stride($a0)
    lh      $t2, (2*\stride)($a0)
    mtc2    $a2, $8             # IR0
    mtc2    $t0, $9             # IR1
    mtc2    $t1, $10            # IR2
    mtc2    $t2, $11            # IR3
    nop
    nop
    gpf     \shift             # sf=1 divides the products by 4096
    mfc2    $t0, $9
    mfc2    $t1, $10
    mfc2    $t2, $11
    sh      $t0, 0($a1)
    sh      $t1, 2($a1)
    sh      $t2, 4($a1)
    jr      $ra
     nop
endlabel \name
.endm

.endif
