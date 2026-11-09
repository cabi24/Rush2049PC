/* PS_PakFormatted: whether a pak is formatted (MAT3 paksysdc.c). */
#include "game/game.h"

/* Always 1: a Controller Pak needs no formatting (MAT3's memory cards do). */
s32 PS_PakFormatted(s32 pak) {
    return 1;
}
