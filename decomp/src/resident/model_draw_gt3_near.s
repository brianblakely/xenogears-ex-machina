.include "decomp/src/resident/model_draw.s"
.include "decomp/src/resident/model_depth.s"

# Draw triangles ordered by their nearest vertex (smallest SZ): entries
# model_draw_gt3_near (POLY_GT3), model_draw_g3_near (POLY_G3), model_draw_f3_near
# (POLY_F3) and model_draw_ft3_near (POLY_FT3), draw mode 3 of model_primitive_types.
model_depth_triangles model_draw_gt3_near, model_draw_g3_near, model_draw_f3_near, model_draw_ft3_near, beqz  # 8002E8C8 8002E8DC 8002E8F0
