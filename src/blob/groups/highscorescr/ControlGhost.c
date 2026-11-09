#include "game/game.h"

VIEW(s32[4], gGhostStatus);
#define gGhostStatus gGhostStatus_view
VIEW(u32, gActiveLevels);
#define gActiveLevels gActiveLevels_view
VIEW(u32, gActiveEdges);
#define gActiveEdges gActiveEdges_view
VIEW(void *, gPlayers_save);
#define gPlayers_save gPlayers_save_view

void ControlGhost(void)
{
    s32 finished;
    while (gGhostMenuSelect == 1 || (gGhostMenuSelect == 2 && gGhostStatus[gGhostPak] != 0 &&
                             gGhostStatus[gGhostPak] != 1)) {
        gGhostMenuSelect--;
        if (gGhostMenuSelect < 0) gGhostMenuSelect = 3;
    }
    if (gGhostMenuConfirm == 1 && (gActiveLevels & 0x3000)) {
        if (gActiveEdges & 0x3000) highscore_gYes = !highscore_gYes;
    } else if (gGhostMenuSelect == 0 && (gActiveLevels & 0x3000)) {
        if (gActiveEdges & 0x1000) {
            gGhostPak--;
            if (gGhostPak < 0) gGhostPak = 3;
        } else if (gActiveEdges & 0x2000) {
            gGhostPak++;
            if (gGhostPak > 3) gGhostPak = 0;
        }
    } else if (gActiveEdges & 0xC00) {
        if (gActiveEdges & 0x400) {
            do {
                gGhostMenuSelect--;
                if (gGhostMenuSelect < 0) gGhostMenuSelect = 3;
            } while (gGhostMenuSelect == 1 || (gGhostMenuSelect == 2 && gGhostStatus[gGhostPak] != 0 &&
                                       gGhostStatus[gGhostPak] != 1));
        } else if (gActiveEdges & 0x800) {
            do {
                gGhostMenuSelect++;
                if (gGhostMenuSelect > 3) gGhostMenuSelect = 0;
            } while (gGhostMenuSelect == 1 || (gGhostMenuSelect == 2 && gGhostStatus[gGhostPak] != 0 &&
                                       gGhostStatus[gGhostPak] != 1));
        }
    } else if (gActiveEdges & 3) {
        finished = 0;
        if (gGhostMenuConflict == 1) {
            gGhostMenuConflict = 0;
        } else if (gGhostMenuConfirm == 1) {
            if ((gActiveEdges & 2) && highscore_gYes == 1) {
                if (SaveGhost(gGhostPak, gPlayers_save, gSavedGhost, 0)) {
                    gGhostMenuNotify = 1;
                }
            }
            gGhostMenuConfirm = 0;
        } else if (gGhostMenuNotify == 1) {
            gGhostMenuNotify = 0;
            finished = 1;
        } else if ((gActiveEdges & 2) && gGhostMenuSelect == 2) {
            if (gGhostStatus[gGhostPak] == 0) {
                if (SaveGhost(gGhostPak, gPlayers_save, gSavedGhost, 0)) {
                    gGhostMenuNotify = 1;
                }
            } else if (gGhostStatus[gGhostPak] == 1) {
                gGhostMenuConfirm = 1;
                highscore_gYes = 0;
                gGhostMenuNotify = 0;
            } else if (gGhostStatus[gGhostPak] == 2) {
                gGhostMenuConflict = 1;
            }
        }
        if (gGhostMenuSelect == 3) {
            SaveGhost(gGhostPak, gPlayers_save, gSavedGhost, 1);
            gSavedGhost = 0;
            finished = 1;
        }
        if (finished == 1) SetHighScoreState(5);
    }
    FindGhosts(gGhostPak);
}
