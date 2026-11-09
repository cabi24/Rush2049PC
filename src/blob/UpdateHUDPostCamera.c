/* UpdateHUDPostCamera: the HUD's work after the camera has moved (MAT3 hud.c). */
#include "game/game.h"

/* During a race and its end: turns the HUD's 3D objects (hudObjAngle, unless paused), draws the coin counters of the
 * race types with coins (all but obstacle and battle), and the battle and stunt extras' (UpdateHUDExtraPostCamera). */
void UpdateHUDPostCamera(void) {
    if (gGameState != -1 && ((gGameState & GS_COUNTDOWN) || (gGameState & GS_RACING))) {
        if (!gPauseState) hudObjAngle += hud_obj_angle_speed * gFrameSeconds;
        if (gRace == kOneRace || gRace == kPractice || gRace == kGhost || gRace == kCircuit || gRace == kStunt) animCoins(0);
        if (gRace == kBattle || gRace == kStunt) UpdateHUDExtraPostCamera();
    }
}
