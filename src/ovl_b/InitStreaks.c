/* InitStreaks.c: set the light streaks up (MAT3 streaks.c, InitStreaks()).
 *
 * ShotInit calls it when a battle or stunt race starts: the 50 streaks (gStreakArray) and their 150 ribbon
 * segments (gStreakPolyArray) become two free lists. Runtime image B. */
#include "game/game.h"

/* PC: the element arrays have their elements' host layout, streaks.c's tStreak (64 bytes on the N64, 72 here: types.h's
 * PcTrail) and tStreakPoly (28, 48: PcFade), which are linked through their first pointer, and the lists the host
 * element size (docs/port/POINTERS.md 9). */

/* Make gStreakList a singly linked list of the 50 streaks, gStreakPolyList one of the 150 segments, all free. */
void InitStreaks(void)
{
    InitStatLL(&gStreakList, gStreakArray, sizeof(PcTrail), 50, 0);
    InitStatLL(&gStreakPolyList, gStreakPolyArray, sizeof(PcFade), 150, 0);
}
