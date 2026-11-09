#include "game/game.h"

typedef union InputSequence {
    struct {
    u8 count;
    u8 unknown1[3];
    u32 held[20];
    u32 pressed[20];
    } fields;
    u32 words[41];
} InputSequence;

VIEW(volatile float, gSeconds);
#define gSeconds gSeconds_view
VIEW(u32, gActiveEdges);
#define gActiveEdges gActiveEdges_view
VIEW(u32, gActiveLevels);
#define gActiveLevels gActiveLevels_view
s32 CheckForEgg(void)
{
    u32 held, pressed, other;
    u32 *step;
    if (!gEgg) return 0;
    if (gEggIdx && gNextTimeOut <= gSeconds)
        gEggIdx = 0;
    step = &gEgg->words[gEggIdx];
    pressed = step[21];
    held = step[1];
    other = ~(pressed | held);
    if (other & gActiveEdges) gEggIdx = 0;
    else if ((held * 1) == (gActiveLevels & held) &&
             (pressed * 1) == (gActiveEdges & pressed) &&
             !(other & gActiveLevels)) {
        gNextTimeOut = gSeconds + 1.0f;
        gEggIdx++;
        if (gEggIdx == gEgg->fields.count) return 1;
    }
    return 0;
}
