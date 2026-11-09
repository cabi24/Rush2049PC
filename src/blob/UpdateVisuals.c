#include "game/game.h"

typedef struct Visual { struct Visual *next; s32 index; s32 objnum; u32 slot; u32 data; void (*func)(struct Visual *v, s16 op); } Visual;
typedef struct Car { u8 pad0[232]; u32 appearance; u8 pad236[36]; Visual visuals[21]; u8 pad776[0x380 - 0x308]; void *input; u8 pad776_384[0x3B8 - 0x384]; } Car;

void UpdateVisuals(s16 slot)
{
    Car *car = PC_CAR(slot);
    Visual *v = &car->visuals[0];
    s16 i;
    f32 usec;

    usec = (u64)frame_cycle_count * 64 / 3000;
    if (gRetraceRate == 50.0f) {
        gGoodFrameRate = usec < 36000.0f;
    } else if (gNumPlayers >= 2) {
        gGoodFrameRate = usec < 45000.0f;
    } else {
        gGoodFrameRate = usec < 30000.0f;
    }

    if (car->appearance & 0x60)
        StartSmoke(slot, 4, 0, 0);

    if (PC_MODEL_VISCODE(slot)[0] == 2)
        StartSmoke(slot, 0, 1, 0);
    else if (car->appearance & 0x40000)
        StartSmoke(slot, 0, 0, 0);
    if (PC_MODEL_VISCODE(slot)[1] == 2)
        StartSmoke(slot, 1, 1, 0);
    else if (car->appearance & 0x80000)
        StartSmoke(slot, 1, 0, 0);
    if (PC_MODEL_VISCODE(slot)[2] == 2)
        StartSmoke(slot, 2, 1, 0);
    else if (car->appearance & 0x10000)
        StartSmoke(slot, 2, 0, 0);
    if (PC_MODEL_VISCODE(slot)[3] == 2)
        StartSmoke(slot, 3, 1, 0);
    else if (car->appearance & 0x20000)
        StartSmoke(slot, 3, 0, 0);

    for (i = 0; i < 21; ++i, ++v)
        if (v->func)
            (*v->func)(v, 1);

    UpdateSkids(slot);
}
