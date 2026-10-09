# Save the scheduler's current caller stack and task so a nested scheduler
# can restore them. a0 points to two words: caller sp, then Task *.
# This is authored assembly for the original handwritten context interface.
# Handwritten: t0 and t1 hold the two words; a plain-C probe under GCC 2.7.2
# and 2.6.3 reuses v0 for both.
glabel func_8008BB00
    lui     $t0, %hi(D_80096D88)
    lw      $t0, %lo(D_80096D88)($t0)
    lui     $t1, %hi(D_80096D8C)
    lw      $t1, %lo(D_80096D8C)($t1)
    sw      $t0, 0($a0)
    jr      $ra
     sw     $t1, 4($a0)
endlabel func_8008BB00
