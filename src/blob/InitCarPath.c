#include "game/game.h"

typedef struct CarA {
    char pad0[0x7CC];
    s8 unk7CC;
    char pad7CD[0xF];
    s8 unk7DC;
} CarA;

typedef struct CarB {
    char pad0[0xFA];
    s16 unkFA;
    s16 unkFC;
    s16 unkFE;
    char pad100[0x110 - 0x100]; struct pc_visual visuals[21]; char pad100_308[0x314 - 0x308];
    f32 unk314;
    f32 unk318;
    char pad31C[0x18];
    f32 unk334;
    s16 unk338;
    s16 unk33A;
    s16 unk33C;
    s16 unk33E;
} CarB;

void InitCarPath(CarA *a, CarB *b) {
    static s32 next_path = 0;

    b->unk338 = 0;
    b->unk314 = 0.0f;
    b->unk318 = 0.0f;
    b->unk334 = gModelTime;
    b->unk33A = -1;
    if (a->unk7CC == 1) {
        b->unk33C = next_path;
        b->unk33E = next_path;
        next_path++;
        if (next_path >= 4) {
            next_path = 0;
        }
    } else {
        b->unk33C = 0;
    }
    if (gCheckpoints.startPt == 0) {
        a->unk7DC = 1;
    }
    b->unkFA = -1;
    b->unkFC = 0;
    b->unkFE = 0;
}
