# In-place RGB555 box filter using adjacent pixels in two rows 0x280 bytes
# apart. a0 starts at the first pixel; a1 is the terminating read cursor.
# IRGB unpacks each colour into IR1..3, and ORGB packs the quarter sum.
# Bit 15 is discarded. There is no width-boundary or empty-range guard.
# The next pair is loaded before the end test, including at the end cursor.
# Handwritten: trapping add/addi throughout, where GCC emits addu/addiu.
glabel func_8007313C
    # Seed the two left-hand colours for the sliding window.
    lhu     $t0, 0($a0)
    lhu     $t1, 0x280($a0)
    mtc2    $t0, $28            # IRGB
    mfc2    $a2, $9
    mfc2    $a3, $10
    mfc2    $t6, $11
    mtc2    $t1, $28
    mfc2    $v0, $9
    mfc2    $v1, $10
    mfc2    $t5, $11
    addi    $a0, $a0, 2
    lhu     $t0, 0($a0)
    lhu     $t1, 0x280($a0)

.Lmenu_colour_window:
    # Carry the upper left colour, then add the new upper right colour.
    mtc2    $t0, $28
    add     $t9, $zero, $a2
    add     $t8, $zero, $a3
    add     $t7, $zero, $t6
    mfc2    $a2, $9
    mfc2    $a3, $10
    mfc2    $t6, $11
    add     $t9, $t9, $a2
    add     $t8, $t8, $a3
    add     $t7, $t7, $t6

    # Add the lower left colour while unpacking the lower right one.
    mtc2    $t1, $28
    add     $t9, $t9, $v0
    add     $t8, $t8, $v1
    add     $t7, $t7, $t5
    mfc2    $v0, $9
    mfc2    $v1, $10
    mfc2    $t5, $11
    add     $t9, $t9, $v0
    add     $t8, $t8, $v1
    add     $t7, $t7, $t5
    sra     $t9, $t9, 2
    sra     $t8, $t8, 2
    sra     $t7, $t7, 2
    mtc2    $t9, $9
    mtc2    $t8, $10
    mtc2    $t7, $11

    # Prefetch both next colours before committing this window's result.
    addi    $a0, $a0, 2
    lhu     $t0, 0($a0)
    lhu     $t1, 0x280($a0)
    mfc2    $t2, $29            # ORGB
    bne     $a0, $a1, .Lmenu_colour_window
     sh     $t2, -4($a0)
    jr      $ra
     nop
endlabel func_8007313C
