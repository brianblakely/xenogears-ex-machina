# Forward word copy: a0 = destination, a1 = source, a2 = byte count.
# The end pointer is tested after each copy, including the first. This is
# used with positive, word-aligned counts; there is no zero-count guard
# or reverse-copy path for overlapping ranges.
glabel func_800732AC
    addu    $a2, $a2, $a1
.Lmenu_word_copy:
    lw      $t0, 0($a1)
    addiu   $a1, $a1, 4
    sw      $t0, 0($a0)
    bne     $a1, $a2, .Lmenu_word_copy
     addiu  $a0, $a0, 4
    jr      $ra
     nop
endlabel func_800732AC
