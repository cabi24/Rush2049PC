/* SetPakError: open a pak's error dialog (MAT3 pakerr.c). */
#include "game/game.h"

/* Opens an error dialog of mode (kPS_Err*) for player's pak in its first closed slot, over the rectangle x, y, w, h,
 * with a 3-second deadline: the controllers' active flags and stick edges are saved and only the pak's player (or
 * every controller, for kPS_ErrNoCont and kPS_ErrNoPak) may answer. kPS_ErrRecognize is noted
 * (pak_recognize_error_seen). The first call clears every slot. */
void SetPakError(s32 player, s32 mode, s32 x, s32 y, s32 w, s32 h) {
    s32 i;
    s32 j;
    s8 present;
    static s8 sInited = 0;

    if (mode == kPS_ErrRecognize) {
        pak_recognize_error_seen = 1;
    }
    if (!sInited) {
        for (j = 0; j < 4; j++) {
            for (i = 0; i < 2; i++) {
                gPakError[j][i].err = 0;
                gPakError[j][i].done = 1;
            }
        }
        sInited = 1;
    }
    for (j = 0; j < 2; j++) {
        if (gPakError[player][j].err == 0) {
            break;
        }
    }
    /* Original game: when both of the pak's records are in use the loop ends with j == 2 and the error is written
     * into the record past the row: the next pak's first one, or past gPakError for pak 3. MAT3 pakerr.c has the
     * same loop over three records. Kept as retail. */
    gPakError[player][j].done = 0;
    gPakError[player][j].timeout = (s32)(3.0f * gRetraceRate) + gScheduler.retraceCount;
    gPakError[player][j].choice1 = 1;
    gPakError[player][j].choice2 = 0;
    gPakError[player][j].err = mode;
    for (i = 0; i < 4; i++) {
        gPakError[player][j].contActive[i] = GetControllerActive(i);
        gPakError[player][j].contStick[i] = GetControllerStickEdges(i);
        gPakError[player][j].contDebug[i] = 0;
    }
    SetControllerActive(-1, 0);
    SetControllerStickEdges(-1, 0);
    if (mode == kPS_ErrNoCont) {
        SetControllerActive(-1, 1);
        SetControllerStickEdges(-1, 1);
    } else {
        gPakError[player][j].cont = player;
        if (mode == kPS_ErrNoPak) {
            SetControllerActive(-1, 1);
            SetControllerStickEdges(-1, 1);
        } else {
            SetControllerActive(gPakError[player][j].cont, 1);
            SetControllerStickEdges(gPakError[player][j].cont, 1);
        }
    }
    present = gPaks[player].rumble;
    gPakError[player][j].left = x;
    gPakError[player][j].rumble = present;
    gPakError[player][j].top = y;
    gPakError[player][j].width = w;
    gPakError[player][j].height = h;
}
