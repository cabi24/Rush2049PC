#include "game/game.h"

typedef struct {
    /* 0x00 */ s16 state;
    /* 0x02 */ s16 id;
    /* 0x04 */ s16 owner;
    /* 0x06 */ s16 pad06;
    /* 0x08 */ s32 pad08[2];
    /* 0x10 */ void (*func)();
    /* 0x14 */ void *pad14;          /* the next visual's next pointer */
} CarSlot;

typedef struct Car {
    char pad0[0x110 - 0x0]; struct pc_visual visuals[1]; void *pad0_128;
    CarSlot slot[5];
    char pad1[0x1B4 - 0x1A4]; void *pad1_1B4; struct pc_visual pad1_1B8[14]; char pad1_308[0x380 - 0x308]; void *input; char pad1_384[0x3B8 - 0x384];
} Car;

typedef struct {
    char pad0[0x2C];
    s32 id[5];
} Rec40;

VIEW(Rec40[], gCarParts);
#define gCarParts gCarParts_view

void StartSparks(s16 idx) {
    Car *car;
    Rec40 *r;

    if (gGameState & 8) {
        return;
    }
    car = PC_CAR(idx);
    r = &gCarParts[idx];
    car->slot[0].func = AnimateLFSpark;
    car->slot[0].state = -1;
    car->slot[0].owner = idx;
    car->slot[0].id = r->id[0];
    car->slot[1].func = AnimateLRSpark;
    car->slot[1].state = -1;
    car->slot[1].owner = idx;
    car->slot[1].id = r->id[1];
    car->slot[2].func = AnimateRFSpark;
    car->slot[2].state = -1;
    car->slot[2].owner = idx;
    car->slot[2].id = r->id[2];
    car->slot[3].func = AnimateRRSpark;
    car->slot[3].state = -1;
    car->slot[3].owner = idx;
    car->slot[3].id = r->id[3];
    car->slot[4].func = AnimateBSpark;
    car->slot[4].state = -1;
    car->slot[4].owner = idx;
    car->slot[4].id = r->id[4];
}
