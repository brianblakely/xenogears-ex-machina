.include "decomp/src/resident/model_draw.s"
.include "decomp/src/resident/model_depth.s"

# Draw quads ordered by their farthest point (largest SZ): entries
# model_draw_gt4_far (POLY_GT4), model_draw_g4_far (POLY_G4), model_draw_f4_far
# (POLY_F4) and model_draw_ft4_far (POLY_FT4), draw mode 2 of model_primitive_types.
model_depth_quads model_draw_gt4_far, model_draw_g4_far, model_draw_f4_far, model_draw_ft4_far, bnez  # 8002E660 8002E674 8002E688
