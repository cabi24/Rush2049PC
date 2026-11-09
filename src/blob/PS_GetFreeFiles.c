#include "game/game.h"

typedef struct Car { u8 pad0[12]; PcPfs pfs; s32 error, FreeSpace; u8 pad7C[16]; struct { s32 v; u8 pad[36]; } e[16]; } Car;
typedef struct { u8 pad0[12]; PcPfs pfs; u8 pad74[0x304 - 0x74]; } CarSlot;
VIEW(CarSlot[], gPaks);
#define gPaks gPaks_view
s32 PS_GetFreeFiles(s32 i) {
    s32 n = 0;
    s32 k;
    Car *c = (Car *)&gPaks[i];
    for (k = 0; k < 16; k++) {
        if (c->e[k].v == 0) {
            n++;
        }
    }
    return n;
}
