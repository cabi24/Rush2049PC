/* CheckForEgg: the secret pad sequences ("eggs": cheat codes) followed press by press (MAT3 eggs.c CheckForEgg). */
#include "game/game.h"

#define EGG_STEP_TIME 1.0f          /* seconds to make the next step */

/* Follow the code InitEgg chose (gEgg) for this frame's buttons. A pause of over a second or a press of a button
 * the step doesn't use starts it again; a frame whose held and new buttons are exactly the step's moves to the next
 * step. 1 when the last step is made, else 0 (also without a code). */
s32 CheckForEgg(void)
{
    u32 held, pressed, other;
    if (!gEgg) return 0;
    if (gEggIdx && gNextTimeOut <= gSeconds)
        gEggIdx = 0;
    pressed = gEgg->edges[gEggIdx];
    held = gEgg->levels[gEggIdx];
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
