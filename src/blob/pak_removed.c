/* pak_removed: whether a pak has been taken out (MAT3 paksysdc.c). */
#include "game/game.h"

/* 1 when pak's pak is not in (tPak.inserted clear): the error screens' poll, which closes a pak's dialog when the
 * pak is pulled. */
s32 pak_removed(s32 pak) {
    return gPaks[pak].inserted == 0;
}
