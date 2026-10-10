# Forward word copy: a0 = destination, a1 = source, a2 = byte count.
# The end pointer is tested after each copy, including the first. This is
# used with positive, word-aligned counts; there is no zero-count guard
# or reverse-copy path for overlapping ranges.
# Handwritten: the word moves through t0 and the end stays in a2; a plain-C
# probe under GCC 2.7.2 and 2.6.3 uses v0 and puts the end in v1.
glabel arena_copy_words
    addu    $a2, $a2, $a1
.Lmenu_word_copy:
    lw      $t0, 0($a1)
    addiu   $a1, $a1, 4
    sw      $t0, 0($a0)
    bne     $a1, $a2, .Lmenu_word_copy
     addiu  $a0, $a0, 4
    jr      $ra
     nop
endlabel arena_copy_words
