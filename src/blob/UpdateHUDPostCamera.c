/* UpdateHUDPostCamera: the HUD's work after the camera has moved (MAT3 hud.c). */
#include "game/game.h"

#define GAME_STATE_RACE 0x200000     /* gGameState: racing */
#define GAME_STATE_RESULTS 0x400000  /* gGameState: the race's end */

VIEW(s32, gGameState);
#define gGameState gGameState_view

/* During a race and its end: turns the HUD's 3D objects (hudObjAngle, unless paused), draws the coin counters of the
 * race types with coins (all but obstacle and battle), and the battle and stunt extras' (UpdateHUDExtraPostCamera). */
void UpdateHUDPostCamera(void) {
    if (gGameState != -1 && ((gGameState & GAME_STATE_RACE) || (gGameState & GAME_STATE_RESULTS))) {
        if (!gPauseState) hudObjAngle += hud_obj_angle_speed * gFrameSeconds;
        if (gRace == 0 || gRace == 1 || gRace == 2 || gRace == 3 || gRace == 4) animCoins(0);
        if (gRace == 6 || gRace == 4) UpdateHUDExtraPostCamera();
    }
}
