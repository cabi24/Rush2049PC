/* InitVisualsCarScr: the car select screens' car records reset (MAT3 selectcarload.c InitVisualsCarScr). */
#include "game/game.h"

#define SELECT_CARS 52              /* gSelectCar: 13 cars for each of 4 players */

/* Every car select record empty: no objects, hidden, not ready, no model, no colours, rims or tyre scales, and no
 * paint job set up. */
void InitVisualsCarScr(void)
{
    s32 i;

    for (i = 0; i < SELECT_CARS; i++) {
        gSelectCar[i].created = 0;
        gSelectCar[i].hidden = 1;
        gSelectCar[i].ready = 0;
        gSelectCar[i].waveInst = -1;
        gSelectCar[i].loading = 0;
        gSelectCar[i].color[0] = -1;
        gSelectCar[i].color[1] = -1;
        gSelectCar[i].color[2] = -1;
        gSelectCar[i].rims = -1;
        gSelectCar[i].frontTireScale = -1.0f;
        gSelectCar[i].rearTireScale = -1.0f;
        gCarPaintJobInited[i] = 0;
    }
}
