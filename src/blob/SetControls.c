/* SetControls (MAT3 controls.c): each player's control layout into their input record, and into their profile when
 * it changed. */
#include "game/game.h"

extern tPadState gPlayers[];

/* For each player: takes the default layout (resetToDefault) or the layout chosen for their controller
 * (gNewControls); if the controls screen changed that controller's layout (gControlsSet), saves it in the player's
 * profile and, without a profile or when resetting, in the controller's own (gJustPlayers); then turns the layout's
 * gControlTable indices into the button bits the car reads (tPadState.controls). MAT3 also keeps the rumble setting
 * in the layout.
 * Original game: a player without a controller (5) takes gNewControls[5], past the four layouts (MAT3 skips it). */
void SetControls(s32 resetToDefault)
{
    s32 p;
    s8 *controls;
    for (p = 0; p < gNumPlayers; p++) {
        if (resetToDefault) {
            controls = gDefaultControls;
        } else {
            controls = gNewControls[gPlayers[p].controller];
        }
        if (gPlayers[p].controller != CONTROLLER_NONE && gControlsSet[gPlayers[p].controller]) {
            gControlsSet[gPlayers[p].controller] = 0;
            if (gPlayers[p].save) SavePlayerControls(gPlayers[p].save, controls);
            if (!gPlayers[p].save || resetToDefault) SavePlayerControls(&gJustPlayers[gPlayers[p].controller], controls);
        }
        {
            s32 i;
            for (i = 0; i < 10; i++) gPlayers[p].controls[i] = gControlTable[controls[i]];
        }
    }
}
