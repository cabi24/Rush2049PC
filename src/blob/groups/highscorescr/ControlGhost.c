/* ControlGhost: the input of the high-score screen's ghost save menu (MAT3 highscorescr.c ControlGhost), after a
 * ghost race: pick a pak, save the ghost to it, or don't. */
#include "game/game.h"

/* A player's pad record (MAT3 tPadState, gPlayers): only its profile. */
typedef struct PadRecord { u8 pad00[0x48]; void *profile; } PadRecord;
extern PadRecord gPlayers[];

VIEW(s32[4], gGhostStatus);
#define gGhostStatus gGhostStatus_view
VIEW(u32, gActiveLevels);
#define gActiveLevels gActiveLevels_view
VIEW(u32, gActiveEdges);
#define gActiveEdges gActiveEdges_view

/* The menu's input, each frame. Its rows: the pak (left and right), the pak's status (skipped), save (only when
 * the pak has no ghost of this track or the profile's own) and don't save. Up and down move between the offered
 * rows. Start or A: closes the "can't save" notice; answers the overwrite question (left and right flip yes and
 * no; A on yes saves); closes the "saved" notice and ends the menu; on save, saves (or asks first over the
 * profile's own ghost, or says it can't over another's); on don't save, ends the menu without saving (SaveGhost's
 * last argument 1: only free the compressed ghost). The ghosts of the chosen pak are looked up again every frame.
 * The ghost saved is the first player's (gPlayers_save is the label of gPlayers[0].profile). */
void ControlGhost(void)
{
    s32 finished;
    while (gGhostMenuSelect == kGhostMenuStatus || (gGhostMenuSelect == kGhostMenuSave && gGhostStatus[gGhostPak] != kGhostNone &&
                             gGhostStatus[gGhostPak] != kGhostMine)) {
        gGhostMenuSelect--;
        if (gGhostMenuSelect < 0) gGhostMenuSelect = kGhostMenuCount - 1;
    }
    if (gGhostMenuConfirm == 1 && (gActiveLevels & (BUTTON_LEFT | BUTTON_RIGHT))) {
        if (gActiveEdges & (BUTTON_LEFT | BUTTON_RIGHT)) highscore_gYes = !highscore_gYes;
    } else if (gGhostMenuSelect == kGhostMenuPak && (gActiveLevels & (BUTTON_LEFT | BUTTON_RIGHT))) {
        if (gActiveEdges & BUTTON_LEFT) {
            gGhostPak--;
            if (gGhostPak < 0) gGhostPak = 3;
        } else if (gActiveEdges & BUTTON_RIGHT) {
            gGhostPak++;
            if (gGhostPak > 3) gGhostPak = 0;
        }
    } else if (gActiveEdges & (BUTTON_UP | BUTTON_DOWN)) {
        if (gActiveEdges & BUTTON_UP) {
            do {
                gGhostMenuSelect--;
                if (gGhostMenuSelect < 0) gGhostMenuSelect = kGhostMenuCount - 1;
            } while (gGhostMenuSelect == kGhostMenuStatus || (gGhostMenuSelect == kGhostMenuSave && gGhostStatus[gGhostPak] != kGhostNone &&
                                       gGhostStatus[gGhostPak] != kGhostMine));
        } else if (gActiveEdges & BUTTON_DOWN) {
            do {
                gGhostMenuSelect++;
                if (gGhostMenuSelect > kGhostMenuCount - 1) gGhostMenuSelect = 0;
            } while (gGhostMenuSelect == kGhostMenuStatus || (gGhostMenuSelect == kGhostMenuSave && gGhostStatus[gGhostPak] != kGhostNone &&
                                       gGhostStatus[gGhostPak] != kGhostMine));
        }
    } else if (gActiveEdges & BUTTON_ACCEPT) {
        finished = 0;
        if (gGhostMenuConflict == 1) {
            gGhostMenuConflict = 0;
        } else if (gGhostMenuConfirm == 1) {
            if ((gActiveEdges & BUTTON_A) && highscore_gYes == 1) {
                if (SaveGhost(gGhostPak, gPlayers[0].profile, gSavedGhost, 0)) {
                    gGhostMenuNotify = 1;
                }
            }
            gGhostMenuConfirm = 0;
        } else if (gGhostMenuNotify == 1) {
            gGhostMenuNotify = 0;
            finished = 1;
        } else if ((gActiveEdges & BUTTON_A) && gGhostMenuSelect == kGhostMenuSave) {
            if (gGhostStatus[gGhostPak] == kGhostNone) {
                if (SaveGhost(gGhostPak, gPlayers[0].profile, gSavedGhost, 0)) {
                    gGhostMenuNotify = 1;
                }
            } else if (gGhostStatus[gGhostPak] == kGhostMine) {
                gGhostMenuConfirm = 1;
                highscore_gYes = 0;
                gGhostMenuNotify = 0;
            } else if (gGhostStatus[gGhostPak] == kGhostOther) {
                gGhostMenuConflict = 1;
            }
        }
        if (gGhostMenuSelect == kGhostMenuNoSave) {
            SaveGhost(gGhostPak, gPlayers[0].profile, gSavedGhost, 1);
            gSavedGhost = 0;
            finished = 1;
        }
        if (finished == 1) SetHighScoreState(kHighScoreScores);
    }
    FindGhosts(gGhostPak);
}
