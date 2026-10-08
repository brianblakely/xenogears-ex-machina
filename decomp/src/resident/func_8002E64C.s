.include "decomp/src/resident/model_draw.s"
.include "decomp/src/resident/model_depth.s"

# Draw quads ordered by their farthest point (largest SZ): entries
# func_8002E64C (POLY_GT4), func_8002E660 (POLY_G4), func_8002E674
# (POLY_F4) and func_8002E688 (POLY_FT4), draw mode 2 of D_8004FE50.
model_depth_quads func_8002E64C, func_8002E660, func_8002E674, func_8002E688, bnez
