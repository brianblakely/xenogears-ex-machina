# Link a packet of eleven words after its tag. This is POLY_GT4's place
# in the table, but a POLY_GT4 has twelve; no supported image calls
# it directly.
.include "decomp/src/resident/ot_link.s"
ot_link gpu_ot_link_11_words, 11
