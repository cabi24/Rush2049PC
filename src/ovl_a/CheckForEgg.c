/* CheckForEgg: the secret pad sequences ("eggs": cheat codes) followed press by press (MAT3 eggs.c CheckForEgg). */
#include "game/game.h"

/* A code (MAT3's egg record, gEggs; 0xA4 bytes): its length, then per step the buttons held and newly pressed. */
typedef union InputSequence {
    struct {
    u8 count;
    u8 unknown1[3];
    u32 held[20];
    u32 pressed[20];
    } fields;
    u32 words[41];
} InputSequence;

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
    step = &gEgg->words[gEggIdx];
    pressed = step[21];             /* fields.pressed[gEggIdx] */
    held = step[1];                 /* fields.held[gEggIdx] */
    other = ~(pressed | held);
    if (other & gActiveEdges) gEggIdx = 0;
    else if ((held * 1) == (gActiveLevels & held) &&
             (pressed * 1) == (gActiveEdges & pressed) &&
             !(other & gActiveLevels)) {
        gNextTimeOut = gSeconds + EGG_STEP_TIME;
        gEggIdx++;
        if (gEggIdx == gEgg->fields.count) return 1;
    }
    return 0;
}
