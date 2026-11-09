#include "game/game.h"

typedef struct ResSlot {s8 f0,loaded,f2,f3,f4,f5;u8 type,pad07;s32 pad08;u8 **p0C;u8 *w10;} ResSlot;
VIEW(ResSlot[64], gWaveInstance);
#define gWaveInstance gWaveInstance_view

void UnloadWaveAll(s32 type) {
    s32 i;
    for (i = 0; i < 64; i++) {
        if (type == 64) {}
        unload_wave_if_matching(&gWaveInstance[i], i, type, -1);
    }
}
void unload_wave_if_matching(ResSlot *slot, s32 i, s32 type, s8 sub) {
    if ((slot->p0C != 0) && (type == slot->type) && ((sub < 0) || (sub == slot->f5))) UnloadWaveInstance(i);
}
