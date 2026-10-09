# Restore the two-word caller context saved by func_8008BB00.
# a0 stays intact; t0 and at are scratch registers in this interface.
# Handwritten: t0 carries both words; a plain-C probe under GCC 2.7.2 and
# 2.6.3 loads them into v0 and v1.
glabel func_8008BB1C
    lw      $t0, 0($a0)
    lui     $at, %hi(D_80096D88)
    sw      $t0, %lo(D_80096D88)($at)
    lw      $t0, 4($a0)
    lui     $at, %hi(D_80096D8C)
    sw      $t0, %lo(D_80096D8C)($at)
    jr      $ra
     nop
endlabel func_8008BB1C
