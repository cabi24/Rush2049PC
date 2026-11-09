/* validControl (MAT3 controlscr.c): whether the controls screen may give the chosen control the chosen button. */
#include "game/game.h"

extern tPadState gPlayers[];

/* Whether player's choice on the controls screen is allowed: the control's assignment isn't "none" (23) and is a
 * valid one (gControlValid), the steering (2) takes only the stick (19, or 25 with a dead zone), and the controls
 * after the steering take anything but the stick. */
s32 controls_validControl(s8 player)
{
    s8 *row = gNewControls[gPlayers[player].controller];
    s16 idx = gControlSelect[player];

    return row[idx] != 23 && gControlValid[row[idx]] == 1 && (idx != 2 || row[2] == 25 || row[2] == 19) &&
           (idx < 3 || (row[idx] != 19 && row[idx] != 25));
}
