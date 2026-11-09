/* InitSelectCarLoad: the car select screens' model loader reset (MAT3 selectcarload.c InitSelectCarLoad). */
#include "game/game.h"

/* Each of the 13 cars' links points at its own car, and every model load slot is free (no wave, no car). */
void InitSelectCarLoad(void)
{
    s32 i;

    for (i = 0; i < 13; i++) {
        PC_LINK_CAR(i) = i;
    }
    for (i = 0; i < SELECT_CAR_LOADS; i++) {
        gSelectCarWavePtrs[i].wave = -1;
        gSelectCarWavePtrs[i].slot = -1;
        gSelectCarWavePtrs[i].loading = 0;
    }
}
