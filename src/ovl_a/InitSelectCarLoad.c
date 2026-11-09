#include "game/game.h"

typedef struct { s32 a; s8 b; s8 c; u8 pad[6]; } SelectCarWave;
VIEW(SelectCarWave[], gSelectCarWavePtrs);
#define gSelectCarWavePtrs gSelectCarWavePtrs_view

void InitSelectCarLoad(void)
{
    s32 i;

    for (i = 0; i < 13; i++) {
        PC_LINK_CAR(i) = i;
    }
    for (i = 0; i < 16; i++) {
        gSelectCarWavePtrs[i].a = -1;
        gSelectCarWavePtrs[i].c = -1;
        gSelectCarWavePtrs[i].b = 0;
    }
}
