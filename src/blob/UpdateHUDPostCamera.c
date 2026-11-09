#include "game/game.h"

VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

void UpdateHUDPostCamera(void) {
    if (gGameState != -1 && ((gGameState & 0x200000) || (gGameState & 0x400000))) {
        if (!gPauseState) hudObjAngle += hud_obj_angle_speed * gFrameSeconds;
        if (gRace == 0 || gRace == 1 || gRace == 2 || gRace == 3 || gRace == 4) animCoins(0);
        if (gRace == 6 || gRace == 4) UpdateHUDExtraPostCamera();
    }
}
