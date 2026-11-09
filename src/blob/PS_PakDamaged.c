/* PS_PakDamaged: whether a pak can't be read (MAT3 paksysdc.c). */
#include "game/game.h"

/* Whether pak's pak is in but could not be read (or the player gave up repairing it). */
s8 PS_PakDamaged(s32 pak) {
    return gPaks[pak].damaged;
}
