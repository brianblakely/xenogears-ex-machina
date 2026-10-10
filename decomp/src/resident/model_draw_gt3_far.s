.include "decomp/src/resident/model_draw.s"
.include "decomp/src/resident/model_depth.s"

# Draw triangles ordered by their farthest vertex (largest SZ): entries
# model_draw_gt3_far (POLY_GT3), model_draw_g3_far (POLY_G3), model_draw_f3_far
# (POLY_F3) and model_draw_ft3_far (POLY_FT3), draw mode 2 of model_primitive_types.
model_depth_triangles model_draw_gt3_far, model_draw_g3_far, model_draw_f3_far, model_draw_ft3_far, bnez  # 8002E45C 8002E470 8002E484
