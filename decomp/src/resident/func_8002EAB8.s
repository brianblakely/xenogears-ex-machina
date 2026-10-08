.include "decomp/src/resident/model_draw.s"
.include "decomp/src/resident/model_depth.s"

# Draw quads ordered by their nearest point (smallest SZ): entries
# func_8002EAB8 (POLY_GT4), func_8002EACC (POLY_G4), func_8002EAE0
# (POLY_F4) and func_8002EAF4 (POLY_FT4), draw mode 3 of D_8004FE50.
model_depth_quads func_8002EAB8, func_8002EACC, func_8002EAE0, func_8002EAF4, beqz
