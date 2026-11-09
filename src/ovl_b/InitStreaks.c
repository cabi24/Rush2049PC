#include "game/game.h"

/* PC: the list headers have the StatLL host layout (40 bytes, shots.c) and the element arrays their elements' host
 * layout, runtime_b_trail's Trail (64 bytes on the N64, 72 here) and Fade (28, 48), which are linked through their
 * first pointer. These declarations give them that storage (gen_data), and the lists the host element size
 * (docs/port/POINTERS.md 9). */
typedef struct { u8 dbl; s32 numElements, elementSize; void *list, *head, *store; } PcStatLL;

VIEW(PcStatLL[1], gStreakList);
#define gStreakList gStreakList_view
VIEW(PcTrail[50], gStreakArray);
#define gStreakArray gStreakArray_view
VIEW(PcStatLL[1], gStreakPolyList);
#define gStreakPolyList gStreakPolyList_view
VIEW(PcFade[150], gStreakPolyArray);
#define gStreakPolyArray gStreakPolyArray_view
#define ELEMENT_SIZE(n64, type) sizeof(type)

void InitStreaks(void)
{
    InitStatLL(gStreakList, gStreakArray, ELEMENT_SIZE(64, PcTrail), 50, 0);
    InitStatLL(gStreakPolyList, gStreakPolyArray, ELEMENT_SIZE(28, PcFade), 150, 0);
}
