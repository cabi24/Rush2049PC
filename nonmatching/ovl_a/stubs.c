/* PC only (the N64 build does not read nonmatching/): runtime image A's 8-byte `jr ra; nop` stubs, the deleted
 * procedures of helpers IDO's whole-program inliner put into their callers (docs/game-asm-ceiling.md). On the N64
 * each of these addresses holds an empty function, so that is what the PC defines there.
 *
 * Who reaches them: nothing. No code in any of the three images has a `jal` to one or builds its address (checked
 * against the retail words), and a body is deleted only when every use of the helper was a call that got inlined.
 * They are defined so that every function of image A has C, and a pointer to one in the image's data would resolve
 * through gen_data's function table (docs/port/DATA.md) to a function instead of NULL. */

/* Callers not attributed. */
void func_8039A440(void)
{
}
void func_803A3D9C(void)
{
}
/* animButtonText@carscr.c and drawOptions@carscr.c, inlined into func_803A44FC (animText@carscr.c). */
void func_803A44EC(void)
{
}
void func_803A44F4(void)
{
}
/* drawControls@playerscr.c and two more helpers, inlined into func_803A9EEC (animText@playerscr.c). */
void func_803A8B6C(void)
{
}
void func_803A9EDC(void)
{
}
void func_803A9EE4(void)
{
}
/* Inlined into func_803AD57C. */
void func_803AD574(void)
{
}
/* Callers not attributed. */
void func_803ACA8C(void)
{
}
void func_803AE62C(void)
{
}
void func_803AE634(void)
{
}
void func_803AECC4(void)
{
}
void func_803AECCC(void)
{
}
void func_803AECD4(void)
{
}
