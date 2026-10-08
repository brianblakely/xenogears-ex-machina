# Zero the words after a0 through a1 inclusive, the entry point's BSS
# convention: the pointer advances before each store. a0 == a1 clears
# nothing. Both must be word aligned with a0 <= a1; there is no other
# bound check. The mode dispatcher clears a mode's BSS with it. Clobbers a0.
# Handwritten: the loop head sits in the delay slot of the empty-range test.
glabel func_80019560
    beq     $a0, $a1, .Lbss_clear_done
.Lbss_clear_word:
     addiu  $a0, $a0, 4         # also executed when the range is empty
    bne     $a0, $a1, .Lbss_clear_word
     sw     $zero, 0($a0)
.Lbss_clear_done:
    jr      $ra
     nop
endlabel func_80019560
