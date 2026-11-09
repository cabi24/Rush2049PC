/* InitStreaks.c: set the light streaks up (MAT3 streaks.c, InitStreaks()).
 *
 * ShotInit calls it when a battle or stunt race starts: the 50 streaks (gStreakArray) and their 150 ribbon
 * segments (gStreakPolyArray) become two free lists. Runtime image B. */
#include "game/game.h"

/* PC: the list headers have the StatLL host layout (40 bytes, shots.c) and the element arrays their elements' host
 * layout, streaks.c's tStreak (64 bytes on the N64, 72 here: types.h's PcTrail) and tStreakPoly (28, 48: PcFade),
 * which are linked through their first pointer. These declarations give them that storage (gen_data), and the lists
 * the host element size (docs/port/POINTERS.md 9). */
typedef struct { u8 dbl; s32 numElements, elementSize; void *list, *head, *store; } PcStatLL;

VIEW(PcStatLL[1], gStreakList);
#define gStreakList gStreakList_view
VIEW(PcTrail[50], gStreakArray);
#define gStreakArray gStreakArray_view
VIEW(PcStatLL[1], gStreakPolyList);
#define gStreakPolyList gStreakPolyList_view
VIEW(PcFade[150], gStreakPolyArray);
#define gStreakPolyArray gStreakPolyArray_view

/* Make gStreakList a singly linked list of the 50 streaks, gStreakPolyList one of the 150 segments, all free. */
void InitStreaks(void)
{
    InitStatLL(gStreakList, gStreakArray, sizeof(PcTrail), 50, 0);
    InitStatLL(gStreakPolyList, gStreakPolyArray, sizeof(PcFade), 150, 0);
}
