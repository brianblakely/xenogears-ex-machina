.include "decomp/src/resident/model_draw.s"
.include "decomp/src/resident/model_depth.s"

# Draw triangles ordered by their farthest vertex (largest SZ): entries
# func_8002E448 (POLY_GT3), func_8002E45C (POLY_G3), func_8002E470
# (POLY_F3) and func_8002E484 (POLY_FT3), draw mode 2 of D_8004FE50.
model_depth_triangles func_8002E448, func_8002E45C, func_8002E470, func_8002E484, bnez
