# Scale the signed low halfwords of a Vector's 32-bit components.
# The upper halves of the input words are deliberately ignored.
# Handwritten: t0-t2 temporaries and an unfilled jr slot; a plain-C probe
# under GCC 2.7.2 and 2.6.3 uses v0/v1/a0 and fills the slot with a store.
.include "decomp/src/menu/vector_scale.s"
menu_scale_vector func_800730F4, 4, 1
