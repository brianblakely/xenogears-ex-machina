# The scheduler's two words, which only these routines address: the
# suspended caller's sp while a task runs (D_80096D88) and the running
# TaskContext * (D_80096D8C). Handwritten storage in an explicit .bss, not a
# compiler .lcomm, so the menu assembler's small-data rule (menu.mk)
# left these 8 bytes in .bss, where they open menu7's larger variables
# ahead of its compiled ones.
    .section .bss
dlabel D_80096D88
    .space 4
enddlabel D_80096D88
dlabel D_80096D8C
    .space 4
enddlabel D_80096D8C
    .text

# Save the scheduler's current caller stack and task so a nested scheduler
# can restore them. a0 points to two words: caller sp, then TaskContext *.
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
