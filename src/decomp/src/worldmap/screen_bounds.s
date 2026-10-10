# Screen test shared by the world map's handwritten terrain renderers.
# After RTPT, with SXY0..SXY2 in t2..t4 and the unsigned x of SXY0 already
# in t8: fall through when some vertex has 0 <= x < 320 and some vertex
# (not necessarily the same one) has 0 <= y < 216; otherwise branch to
# \reject. Clobbers t7 and t8. This file emits no code until it is used.
.ifndef WORLDMAP_SCREEN_BOUNDS_MACROS
.set WORLDMAP_SCREEN_BOUNDS_MACROS, 1

.macro screen_bounds_test reject
    sltiu   $t7, $t8, 320
    bnez    $t7, .Lscreen_x_\@
     andi   $t8, $t3, 0xFFFF
    sltiu   $t7, $t8, 320
    bnez    $t7, .Lscreen_x_\@
     andi   $t8, $t4, 0xFFFF
    sltiu   $t7, $t8, 320
    beqz    $t7, \reject
.Lscreen_x_\@:
     sra    $t8, $t2, 16        # also the last x test's delay slot
    sltiu   $t7, $t8, 216
    bnez    $t7, .Lscreen_y_\@
     sra    $t8, $t3, 16
    sltiu   $t7, $t8, 216
    bnez    $t7, .Lscreen_y_\@
     sra    $t8, $t4, 16
    sltiu   $t7, $t8, 216
    beqz    $t7, \reject
     nop
.Lscreen_y_\@:
.endm

.endif
