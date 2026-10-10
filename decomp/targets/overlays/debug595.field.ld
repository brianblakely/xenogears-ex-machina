/* Field overlay symbols the debug595 source addresses from their base, where
 * the generated undefined-symbol list only names the accessed fields. */
field_effect_templates = 0x800B02CC; /* particle emitters, 8 x 0x78 */
field_view = 0x800AF880; /* the field view (field.h FieldView): camera eye, 16.16 */
field_view_target = field_view + 0x10; /* 800AF890: its target: camera look-at */
field_view_eye_goal = field_view + 0x30; /* 800AF8B0: its eye_goal: second camera eye */
field_view_target_goal = field_view + 0x40; /* 800AF8C0: its target_goal: second camera look-at */
