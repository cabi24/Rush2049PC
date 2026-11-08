/* PC only (the N64 build does not read nonmatching/): runtime image A's 8-byte `jr ra; nop` stubs, the deleted
 * procedures of helpers IDO's whole-program inliner put into their callers (docs/game-asm-ceiling.md). On the N64
 * each of these addresses holds an empty function, so that is what the PC defines there.
 *
 * Who reaches them: nothing. No code in any of the three images has a `jal` to one or builds its address (checked
 * against the retail words), and a body is deleted only when every use of the helper was a call that got inlined.
 * They are defined so that every function of image A has C, and a pointer to one in the image's data would resolve
 * through gen_data's function table (docs/port/DATA.md) to a function instead of NULL. */

/* Callers not attributed. */
void circuitscr_first_deleted_stub(void)
{
}
void carscr_slider_inline_stub(void)
{
}
/* animButtonText@carscr.c and drawOptions@carscr.c, inlined into carscr_animText (animText@carscr.c). */
void carscr_drawOptions_stub(void)
{
}
void carscr_animButtonText_stub(void)
{
}
/* drawControls@playerscr.c and two more helpers, inlined into playerscr_animText (animText@playerscr.c). */
void drawControls(void)
{
}
void drawPakFull(void)
{
}
void drawPakDamaged(void)
{
}
/* Inlined into recordscr_animText. */
void recordscr_drawView_stub(void)
{
}
/* Callers not attributed. */
void recordscr_drawStuntTotals_stub(void)
{
}
void trackscr_slider_stub_first(void)
{
}
void trackscr_slider_stub_second(void)
{
}
void trackscr_button_text_stub_first(void)
{
}
void trackscr_button_text_stub_second(void)
{
}
void trackscr_button_text_stub_third(void)
{
}
