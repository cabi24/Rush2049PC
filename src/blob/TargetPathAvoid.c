#include "game/game.h"

typedef struct Car {
    char pad0[0x794];
    f32 unk794[3];
    f32 unk7A0[3][3];
} Car;

typedef struct Obj {
    char pad0[0x28];
    f32 unk28[3];
    char pad34[0xC];
} Obj;

VIEW(Obj *, gBBoxModelStructs);
#define gBBoxModelStructs gBBoxModelStructs_view

extern f32 fabsf(f32);
#pragma intrinsic(fabsf)

void TargetPathAvoid(Car *car, f32 *out) {
    Obj *p;
    f32 diff[3];
    f32 loc[3];
    f32 val;

    if (gBBoxModelUsed != 0) {
        for (p = gBBoxModelStructs; p < &gBBoxModelStructs[gBBoxModelUsed - 1]; p++) {
            diff[0] = p->unk28[0] - car->unk794[0];
            diff[1] = p->unk28[1] - car->unk794[1];
            diff[2] = p->unk28[2] - car->unk794[2];
            BodyVector(diff, loc, car->unk7A0[0]);
            if (loc[2] < 0.0f) {
                continue;
            }
            if (loc[2] > 225.0f) {
                continue;
            }
            if (fabsf(loc[0]) > 14.0f) {
                continue;
            }
            if (loc[2] < 125.0f) {
                val = 0.75f;
            } else if (loc[2] < 225.0f) {
                val = 1.0f - (225.0f - loc[2]) * 0.0025f;
            } else {
                val = 1;
            }
            if (val < *out) {
                *out = val;
            }
        }
    }
}
