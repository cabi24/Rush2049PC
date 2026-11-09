#include "game/game.h"

typedef struct {
    u8 b0;
    u8 b1;
    u8 b2;
    u8 b3;
    s32 w4;
    s8 b8;
    s8 b9;
    s8 b10;
    u8 pad11;
    s32 w12;
    f32 f16;
    f32 f20;
} Rec24;
VIEW(Rec24[], gSelectCar);
#define gSelectCar gSelectCar_view

void InitVisualsCarScr(void)
{
    s32 i;

    for (i = 0; i < 52; i++) {
        gSelectCar[i].b0 = 0;
        gSelectCar[i].b1 = 1;
        gSelectCar[i].b2 = 0;
        gSelectCar[i].w4 = -1;
        gSelectCar[i].b3 = 0;
        gSelectCar[i].b8 = -1;
        gSelectCar[i].b9 = -1;
        gSelectCar[i].b10 = -1;
        gSelectCar[i].w12 = -1;
        gSelectCar[i].f16 = -1.0f;
        gSelectCar[i].f20 = -1.0f;
        gCarPaintJobInited[i] = 0;
    }
}
