/* UnloadSelectCar: a car select screen's car model unloaded (MAT3 selectcarload.c UnloadSelectCar). */
#include "game/game.h"

/* Unload the model loaded for car slot id: wait for its wave to finish loading, unload it, free the car's palette
 * and free the load slot. Nothing happens if nothing is loaded for it. */
void UnloadSelectCar(s16 id)
{
    s32 i;
    SelectCarWaveSlot *p;

    for (i = 0; i < SELECT_CAR_LOADS; i++) {
        p = &gSelectCarWavePtrs[i];
        if (p->slot == id) {
            while (WaveInstanceReady(p->wave) == 0) {
            }
            UnloadWaveInstance(p->wave);
            free_car_palette(id);
            p->wave = -1;
            p->slot = -1;
            p->loading = 0;
            return;
        }
    }
}
