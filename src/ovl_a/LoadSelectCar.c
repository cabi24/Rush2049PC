/* LoadSelectCar: a car select screen's car model loaded (MAT3 selectcarload.c LoadSelectCar). */
#include "game/game.h"

#define WAVE_CAR_MODELS 88          /* car index's model wave: this + index */

/* Load the model of car `index` for car slot `slot`, putting its wave instance in *waveInst. A load already started
 * for the slot is reused; else the slot's load record (or a free one) starts the car's wave loading in the
 * background, records it as the slot's model (gCarModelNums) and sets up the car's palette. 1 done, 0 when all 16
 * load records are taken. */
s32 LoadSelectCar(s16 slot, s16 index, s32 *waveInst)
{
    s32 i;
    s32 count = 16;

    for (i = 0; i < count; i++) {
        if (gSelectCarWavePtrs[i].slot == slot && gSelectCarWavePtrs[i].loading != 0) {
            *waveInst = gSelectCarWavePtrs[i].wave;
            return 1;
        }
    }
    for (i = 0; i != count; i++) {
        if (gSelectCarWavePtrs[i].slot == slot)
            break;
    }
    if (i == count) {
        for (i = 0; i != count; i++) {
            if (gSelectCarWavePtrs[i].slot < 0)
                break;
        }
        if (i == count)
            return 0;
        gSelectCarWavePtrs[i].slot = slot;
    }
    *waveInst = gSelectCarWavePtrs[i].wave = LoadWave(index + WAVE_CAR_MODELS, 1, 1, 0, 0);
    gSelectCarWavePtrs[i].loading = 1;
    gCarModelNums[slot] = gSelectCarWavePtrs[i].wave;
    car_palette_init(slot, index, 0);
    return 1;
}
