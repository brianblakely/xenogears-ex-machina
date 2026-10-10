.include "decomp/src/resident/model_draw.s"
.include "decomp/src/resident/model_depth.s"

# Draw quads ordered by their nearest point (smallest SZ): entries
# model_draw_gt4_near (POLY_GT4), model_draw_g4_near (POLY_G4), model_draw_f4_near
# (POLY_F4) and model_draw_ft4_near (POLY_FT4), draw mode 3 of model_primitive_types.
model_depth_quads model_draw_gt4_near, model_draw_g4_near, model_draw_f4_near, model_draw_ft4_near, beqz  # 8002EACC 8002EAE0 8002EAF4
