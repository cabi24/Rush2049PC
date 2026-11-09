#include "game/game.h"

typedef struct { s32 a; s8 b; s8 c; u8 pad[6]; } SelectCarWave;
VIEW(SelectCarWave[], gSelectCarWavePtrs);
#define gSelectCarWavePtrs gSelectCarWavePtrs_view

void UnloadSelectCar(s16 id)
{
    s32 i;
    SelectCarWave *p;

    for (i = 0; i < 16; i++) {
        p = &gSelectCarWavePtrs[i];
        if (p->c == id) {
            while (WaveInstanceReady(p->a) == 0) {
            }
            UnloadWaveInstance(p->a);
            free_car_palette(id);
            p->a = -1;
            p->c = -1;
            p->b = 0;
            return;
        }
    }
}
