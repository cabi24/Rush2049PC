/* PC only (the N64 build does not read nonmatching/): runtime image B's 8-byte `jr ra; nop` stubs, the deleted
 * procedures of inlined helpers (docs/game-asm-ceiling.md). On the N64 each of these addresses holds an empty
 * function, so that is what the PC defines there.
 *
 * Who reaches them: nothing. No code in any of the three images has a `jal` to one or builds its address (checked
 * against the retail words). They are defined so that every function of image B has C, and gen_data's function
 * table (docs/port/DATA.md) resolves a pointer to one to a function instead of NULL. */

/* Callers not attributed. */
void func_8038CB10(void)
{
}
void func_8038CB18(void)
{
}
/* Inlined into func_80391B00. */
void func_80391640(void)
{
}
void func_80391648(void)
{
}
