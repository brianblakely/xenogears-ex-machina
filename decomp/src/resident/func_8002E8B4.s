.include "decomp/src/resident/model_draw.s"
.include "decomp/src/resident/model_depth.s"

# Draw triangles ordered by their nearest vertex (smallest SZ): entries
# func_8002E8B4 (POLY_GT3), func_8002E8C8 (POLY_G3), func_8002E8DC
# (POLY_F3) and func_8002E8F0 (POLY_FT3), draw mode 3 of D_8004FE50.
model_depth_triangles func_8002E8B4, func_8002E8C8, func_8002E8DC, func_8002E8F0, beqz
