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
                gPakError[j][i].mode = 0;
                gPakError[j][i].busy = 1;
            }
        }
        sInited = 1;
    }
    for (j = 0; j < 2; j++) {
        if (gPakError[player][j].mode == 0) {
            break;
        }
    }
    /* Original game: when both of the pak's records are in use the loop ends with j == 2 and the error is written
     * into the record past the row: the next pak's first one, or past gPakError for pak 3. MAT3 pakerr.c has the
     * same loop over three records. Kept as retail. */
    gPakError[player][j].busy = 0;
    gPakError[player][j].deadline = (s32)(3.0f * gRetraceRate) + gScheduler.retraceCount;
    gPakError[player][j].enabled = 1;
    gPakError[player][j].unk2 = 0;
    gPakError[player][j].mode = mode;
    for (i = 0; i < 4; i++) {
        gPakError[player][j].primary[i] = GetControllerActive(i);
        gPakError[player][j].secondary[i] = GetControllerStickEdges(i);
        gPakError[player][j].previous[i] = 0;
    }
    SetControllerActive(-1, 0);
    SetControllerStickEdges(-1, 0);
    if (mode == kPS_ErrNoCont) {
        SetControllerActive(-1, 1);
        SetControllerStickEdges(-1, 1);
    } else {
        gPakError[player][j].player = player;
        if (mode == kPS_ErrNoPak) {
            SetControllerActive(-1, 1);
            SetControllerStickEdges(-1, 1);
        } else {
            SetControllerActive(gPakError[player][j].player, 1);
            SetControllerStickEdges(gPakError[player][j].player, 1);
        }
    }
    present = gPaks[player].rumble;
    gPakError[player][j].arg[0] = x;
    gPakError[player][j].present = present;
    gPakError[player][j].arg[1] = y;
    gPakError[player][j].arg[2] = w;
    gPakError[player][j].arg[3] = h;
}
