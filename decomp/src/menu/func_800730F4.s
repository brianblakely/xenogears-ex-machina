# Scale the signed low halfwords of a Vector's 32-bit components.
# The upper halves of the input words are deliberately ignored.
.include "decomp/src/menu/vector_scale.s"
menu_scale_vector func_800730F4, 4, 1
