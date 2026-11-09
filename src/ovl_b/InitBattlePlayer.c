#include "game/game.h"

typedef struct CarBattleView {
    unsigned char unknown000[0x110 - 0x0]; struct pc_visual visuals[21]; unsigned char unknown000_308[0x380 - 0x308]; void *input;
    s8 mode;
    s8 selection;
    unsigned char unknown386[6];
    s32 counter;
    unsigned char unknown390[0x10];
    s8 state;
    unsigned char unknown3a1;
    s8 flags;
    unsigned char unknown3a3;
    s32 timer;
    unsigned char unknown3a8[0x10];
} CarBattleView;

typedef struct tMuzzleObj {
    s32 handle[5];
    unsigned char unknown014[0xF4];
    f32 timer;
} tMuzzleObj;

VIEW(tMuzzleObj[], gGattMuzzleObjs);
#define gGattMuzzleObjs gGattMuzzleObjs_view

void InitBattlePlayer(s16 player)
{
    CarBattleView *state;
    s32 i;

    state = PC_CAR(player);
    state->counter = 0;
    state->flags = 0;
    state->state = 0;
    state->timer = 0;
    state->mode = 8;
    state->selection = -1;
    gCurWeaponType[player] = 9;
    gCannMuzzleObjs[player].handle = -1;
    gCannMuzzleObjs[player].selection = -1;
    gCannMuzzleObjs[player].timer = 0.0f;
    gGattMuzzleObjs[player].timer = 0.0f;
    for (i = 0; i < 5; i++) {
        gGattMuzzleObjs[player].handle[i] = -1;
    }
}
