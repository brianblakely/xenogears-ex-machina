/* Field overlay symbols the debug595 source addresses from their base, where
 * the generated undefined-symbol list only names the accessed fields. */
D_800B02CC = 0x800B02CC; /* particle emitters, 8 x 0x78 */
D_800AF880 = 0x800AF880; /* the field view (field.h FieldView): camera eye, 16.16 */
D_800AF890 = D_800AF880 + 0x10; /* its target: camera look-at */
D_800AF8B0 = D_800AF880 + 0x30; /* its eye_goal: second camera eye */
D_800AF8C0 = D_800AF880 + 0x40; /* its target_goal: second camera look-at */
