/* DrawNoControllers: the scrolling message that a player's controller is unplugged (MAT3 game.c). */
#include "game/game.h"



s32 sprintf(char *, const char *, ...);

/* game.frame_rate 60: the scroller moves a pixel a 30 Hz frame, as on the N64 (docs/port/FRAMERATE.md 7) */
#define PC_SCROLL_DEC(v) ((v) -= PC_STEPS(1))

/* Finds the first player (of the race's players; a ghost race has one) whose controller isn't attached
 * (first_unplugged_controller, the players' count if none) and scrolls a message asking for it across the screen,
 * at the top in the attract and demo states, at the bottom otherwise (none in states 1 and 2). */
void DrawNoControllers(void)
{
    char str[256];
    char num[8];
    s32 numPlayers;
    s32 i;
    static s32 x = 320;

    numPlayers = gRace == kGhost ? 1 : gNumPlayers;
    for (i = 0; i < numPlayers; i++) {
        if (!gControllerData[i].attached) break;
    }

    if (i >= numPlayers) {
        first_unplugged_controller = i;
        x = 320;
        return;
    }
    first_unplugged_controller = i;
    sprintf(num, "%d", i + 1);
    sprintf(str, "%s%s%s%s%s%d%s", gText.single[124], num, gText.single[125], num,
            gText.single[126], numPlayers, gText.single[127]);
    game_strcat(str, gText.single[128]);
    HAL_SetFont(11);
    TB_SetTextColor(22);
    if (!(gGameState & GS_BOOT) && !(gGameState & GS_LICENSE)) {
        if ((gGameState & GS_WARNING) || (gGameState & GS_TITLE) || (gGameState & GS_DEMO)) {
            TB_DrawText(x, 8, str);
        } else {
            TB_DrawText(x, 205, str);
        }
    }
    if (PC_SCROLL_DEC(x) < -HAL_GetTextWidth(str, -1)) {
        x = 320;
    }
}
