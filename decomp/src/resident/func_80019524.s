# Program entry point (the executable header's initial pc). Zero the BSS:
# the words after D_800592B8, the final .data word, through D_8006FAEC, the
# final word below the overlay area at 0x8006FAF0. The pointer advances
# before each store, so the end word itself is cleared.
# Then make the boot routine func_80019578 the return address and fall
# through into func_80019548, which installs the initial stack and gp and
# "returns" there. Only t0, t1 and ra are written; nothing is preserved.
# Handwritten: no frame, and it continues into the following routine
# instead of calling it.
glabel func_80019524
    lui     $t0, %hi(D_800592B8)
    addiu   $t0, $t0, %lo(D_800592B8)
    lui     $t1, %hi(D_8006FAEC)
    addiu   $t1, $t1, %lo(D_8006FAEC)
.Lentry_clear_bss:
    addiu   $t0, $t0, 4
    bne     $t0, $t1, .Lentry_clear_bss
     sw     $zero, 0($t0)
    lui     $ra, %hi(func_80019578)
    addiu   $ra, $ra, %lo(func_80019578)
    # No return: execution falls through into func_80019548.
endlabel func_80019524
