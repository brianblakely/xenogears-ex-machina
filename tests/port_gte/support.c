/* The data the port's libgte reads from the game at its original addresses,
 * as test-owned host objects that tests/test_port_gte.py fills (invented
 * tables, or the user's own image when present), and the game's empty debug
 * print, counted. */
short rcossin_tbl[4096][2];
short libgte_square_root_table[192];
short libgte_inverse_square_root_table[198];
short libgte_arctangent_table[1026];
int libgte_matrix_stack_depth;
unsigned int libgte_matrix_stack[0x280 / 4];
int test_debug_print_calls;

void mode_empty_debug_print(void) { test_debug_print_calls++; }
