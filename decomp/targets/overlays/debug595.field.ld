/* Field overlay symbols the debug595 source addresses from their base, where
 * the generated undefined-symbol list only names the accessed fields. */
field_effect_templates = 0x800B02CC; /* particle emitters, 8 x 0x78 */
field_view = 0x800AF880; /* the field view (field.h FieldView): camera eye, 16.16 */
field_view_target = field_view + 0x10; /* 800AF890: its target: camera look-at */
field_view_eye_goal = field_view + 0x30; /* 800AF8B0: its eye_goal: second camera eye */
field_view_target_goal = field_view + 0x40; /* 800AF8C0: its target_goal: second camera look-at */
field_view_target_follow_divisor = field_view + 0x104; /* 800AF984: its target_a: target follow divisor */
field_view_eye_follow_divisor = field_view + 0x108; /* 800AF988: its target_b: eye follow divisor */
field_view_heading_angle = field_view + 0x166; /* 800AF9E6: its heading_angles.vy */
field_view_heading_high = field_view + 0x170; /* 800AF9F0: its heading_high */
field_view_heading_blocks0 = field_view + 0x174; /* 800AF9F4: its heading_blocks[0]: dolly set */
field_view_heading_blocks1 = field_view + 0x175; /* 800AF9F5: its heading_blocks[1]: dolly stop */
field_view_projection = field_view + 0x178; /* 800AF9F8: its projection: screen distance */
field_view_elevation = field_view + 0x17C; /* 800AF9FC: its elevation */
field_view_distance = field_view + 0x17E; /* 800AF9FE: its distance */
field_view_scaled_world = field_view + 0x1E4; /* 800AFA64: its scaled_world */
field_view_components = field_view + 0x28C; /* 800AFB0C: its components */
field_work = 0x800B2078; /* the field work block (field.h FieldWork) */
field_work_sprite_gate = field_work + 0x116; /* 800B218E: its sprite_gate */
field_work_fog_color = field_work + 0x118; /* 800B2190: its fog_color: fog near colour */
field_work_far_color = field_work + 0x11C; /* 800B2194: its far_color: fog far colour */
field_work_fog_range = field_work + 0x120; /* 800B2198: its fog_range: fog near, far */
field_work_controlled = field_work + 0x1F4; /* 800B226C: its controlled: player actor */
field_work_encounter_period = field_work + 0x220; /* 800B2298: its unk2298: encounter timer */
field_work_encounter_step_count = field_work + 0x224; /* 800B229C: its unk229C: encounter number */
