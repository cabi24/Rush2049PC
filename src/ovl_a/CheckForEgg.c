/* CheckForEgg: the secret pad sequences ("eggs": cheat codes) followed press by press (MAT3 eggs.c CheckForEgg). */
#include "game/game.h"

#define EGG_STEP_TIME 1.0f          /* seconds to make the next step */

/* Follow the code InitEgg chose (gEgg) for this frame's buttons. A pause of over a second or a press of a button
 * the step doesn't use starts it again; a frame whose held and new buttons are exactly the step's moves to the next
 * step. 1 when the last step is made, else 0 (also without a code). */
s32 CheckForEgg(void)
{
    u32 held, pressed, other;
    u32 *step;
    if (!gEgg) return 0;
    if (gEggIdx && gNextTimeOut <= gSeconds)
        gEggIdx = 0;
    /* The step's buttons, as the egg's words: held is its word 1 + gEggIdx (levels[gEggIdx]), pressed its word
     * 21 + gEggIdx (edges[gEggIdx]). As words because the index can pass the 20 steps an egg has room for, and the
     * game then reads the words after them: a code whose size the index never meets (the synthetic cartridge's, all
     * zeros, which every frame without a button matches) steps on while the pad is idle. */
    step = (u32 *)gEgg + gEggIdx;
    pressed = step[21];
    held = step[1];
    other = ~(pressed | held);
    if (other & gActiveEdges) gEggIdx = 0;
    else if ((held * 1) == (gActiveLevels & held) &&
             (pressed * 1) == (gActiveEdges & pressed) &&
             !(other & gActiveLevels)) {
        gNextTimeOut = gSeconds + EGG_STEP_TIME;
        gEggIdx++;
        if (gEggIdx == gEgg->size) return 1;
    }
    return 0;
}
