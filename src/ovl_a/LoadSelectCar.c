#include "game/game.h"

typedef struct SelectCarWave {   /* anon_wave_loading_c */
    s32 wave;
    s8 loading;
    s8 slot;
    u8 pad[6];
} SelectCarWave;

VIEW(SelectCarWave[], gSelectCarWavePtrs);
#define gSelectCarWavePtrs gSelectCarWavePtrs_view

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
    *waveInst = gSelectCarWavePtrs[i].wave = LoadWave(index + 88, 1, 1, 0, 0);
    gSelectCarWavePtrs[i].loading = 1;
    gCarModelNums[slot] = gSelectCarWavePtrs[i].wave;
    car_palette_init(slot, index, 0);
    return 1;
}
