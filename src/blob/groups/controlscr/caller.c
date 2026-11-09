#include "game/game.h"

/* PC: the pak file record's and the profile's host layouts (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct { void *next, *prev; void (*RemoveFile)(void *); void (*MoveFile)(void *, void *); u8 pak; } PSFile;
typedef struct Profile {
    void *next, *prev;
    PSFile **fileh;
    u8 padC[21];
    s8 controls[10];
    s8 rumble;
    void *data;
} Profile;
typedef struct {
    u8 slot;
    u8 controller;       /* 0x01 */
    u8 pad2[2];
    u32 edges;           /* 0x04 */
    u8 pad8[64];
    Profile **save;      /* 0x48 */
} PadState;              /* tPadState, 0x4C */

s32 *InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);

void *memcpy(void *, const void *, u32);

void *NewMultiBlit(s16, s16, void *, s16);

extern PadState gPlayers[4];           /* gPlayers */

VIEW(s8[4], controlScrActive);
#define controlScrActive controlScrActive_view
VIEW(PlayerRect[4][4], controls_gPlayerRects);
#define controls_gPlayerRects controls_gPlayerRects_view
VIEW(s8[4], prevControlScrActive);
#define prevControlScrActive prevControlScrActive_view
VIEW(s16[4], gControlSelect);
#define gControlSelect gControlSelect_view
VIEW(s16[4], gControlFirst);
#define gControlFirst gControlFirst_view

VIEW(s8[4][10], gConflict);
#define gConflict gConflict_view
VIEW(s8[4], gConflictExists);
#define gConflictExists gConflictExists_view
VIEW(s8[4][10], gNewControls);
#define gNewControls gNewControls_view
VIEW(Profile *[4], gJustPlayers);
#define gJustPlayers gJustPlayers_view
VIEW(s8[4], gControlsSet);
#define gControlsSet gControlsSet_view

VIEW(struct pc_multiblit[81], buttonsFrontEnd);
#define buttonsFrontEnd buttonsFrontEnd_view
VIEW(struct pc_multiblit[21], buttonsPauseSingle);
#define buttonsPauseSingle buttonsPauseSingle_view
VIEW(struct pc_multiblit[41], buttonsPauseMulti);
#define buttonsPauseMulti buttonsPauseMulti_view

void updateControlScreen(void) {
    s32 forcedExit;
    s32 p;
    s32 i;
    s32 j;
    s32 menuItem;
    s32 maxFirstItem;
    s8 *controls;

    if (!controls_inited) {
        if (gGameState & 0x7C03FFFE) {
            s8 skipInit = 1;
            for (p = 0; p < gNumWindows; p++) {
                if (controlScrActive[p] == 1) skipInit = 0;
            }
            if (skipInit == 1) return;
        } else {
            for (p = 0; p < gNumWindows; p++) {
                InitPane(controls_gPlayerRects[gNumWindows - 1][p].x,
                    controls_gPlayerRects[gNumWindows - 1][p].y,
                    controls_gPlayerRects[gNumWindows - 1][p].x + controls_gPlayerRects[gNumWindows - 1][p].w,
                    controls_gPlayerRects[gNumWindows - 1][p].y + controls_gPlayerRects[gNumWindows - 1][p].h,
                    176, 0, 0, 0);
            }
            for (p = gNumWindows; p < 4; p++) prevControlScrActive[p] = 0;
        }
        calcControlMenuMaxSize();
        if (gGameState & 0x7C03FFFE)
            controls_blit = (intptr_t)NewMultiBlit(0, 0, buttonsFrontEnd, 81);
        else if (gNumWindows == 1)
            controls_blit = (intptr_t)NewMultiBlit(0, 0, buttonsPauseSingle, 21);
        else
            controls_blit = (intptr_t)NewMultiBlit(0, 0, buttonsPauseMulti, 41);
        if (gNumWindows == 1) {
            controls_gButtonZoom = 1.0f;
            controls_gButtonScale = 0.9f;
            controls_gButtonSpace = 40.0f;
            controls_buttonX = 0.0f;
            controls_maxY = 120.0f;
            controls_minY = -120.0f;
            controls_buttonZ = 300.0f;
            controls_cursorZ = 225.0f;
        } else if (gNumWindows == 2) {
            controls_gButtonZoom = 1.0f;
            controls_gButtonScale = 1.3f;
            controls_gButtonSpace = 40.0f;
            controls_buttonX = 0.0f;
            controls_maxY = 95.0f;
            controls_minY = -120.0f;
            controls_buttonZ = 300.0f;
            controls_cursorZ = 225.0f;
        } else {
            controls_gButtonZoom = 1.0f;
            controls_gButtonScale = 1.3f;
            controls_gButtonSpace = 40.0f;
            controls_buttonX = -30.0f;
            controls_maxY = 95.0f;
            controls_minY = -120.0f;
            controls_buttonZ = 300.0f;
            controls_cursorZ = 225.0f;
        }
        exitControlScreen = 0;
        controls_inited = 1;
    }
    for (p = 0; p < 4; p++) {
        if (gPlayers[p].controller == 5) continue;
        if (!controlScrActive[p]) {
            if ((gGameState & 0x7C0000) && (gPlayers[p].edges & 2)) controlScrActive[p] = 1;
            continue;
        }
        if (!prevControlScrActive[p]) {
            gControlSelect[p] = 0;
            gControlFirst[p] = 0;
            for (i = 0; i < 10; i++) {
                gConflict[p][i] = 0;
                gConflictExists[p] = 0;
            }
            if (gPlayers[p].save)
                memcpy(gNewControls[gPlayers[p].controller], (*gPlayers[p].save)->controls, 10);
            else
                memcpy(gNewControls[gPlayers[p].controller], gJustPlayers[gPlayers[p].controller]->controls, 10);
            if (gGameState & 0x7C03FFFE) controls_initButtons(p);
        }
        controls_updateButtons(p);
        controls = gNewControls[gPlayers[p].controller];
        while ((gConflictExists[p] == 1 && !gConflict[p][gControlSelect[p]] && gControlSelect[p] != 11) ||
            !controls_validOption(p, gControlSelect[p]) || (!PS_PakRumble(gPlayers[p].controller) && gControlSelect[p] == 10)) {
            gControlSelect[p]++;
            if (gControlSelect[p] > 11) gControlSelect[p] = 0;
        }
        if (gPlayers[p].edges & 0x400) {
            AudPlayEffect(40, 0, 1, 0);
            do {
                gControlSelect[p]--;
                if (gControlSelect[p] < 0) gControlSelect[p] = 11;
            } while ((gConflictExists[p] == 1 && !gConflict[p][gControlSelect[p]] && gControlSelect[p] != 11) ||
                !controls_validOption(p, gControlSelect[p]) || (!PS_PakRumble(gPlayers[p].controller) && gControlSelect[p] == 10));
        }
        if (gPlayers[p].edges & 0x800) {
            AudPlayEffect(39, 0, 1, 0);
            do {
                gControlSelect[p]++;
                if (gControlSelect[p] > 11) gControlSelect[p] = 0;
            } while ((gConflictExists[p] == 1 && !gConflict[p][gControlSelect[p]] && gControlSelect[p] != 11) ||
                !controls_validOption(p, gControlSelect[p]) || (!PS_PakRumble(gPlayers[p].controller) && gControlSelect[p] == 10));
        }
        maxFirstItem = 0;
        menuItem = 0;
        for (i = 11; i >= 0; i--) {
            if (controls_validOption(p, i)) {
                menuItem++;
                if (menuItem == gControlMenuSize) {
                    maxFirstItem = i;
                    break;
                }
            }
        }
        menuItem = 0;
        for (i = 0; i < gControlSelect[p]; i++) {
            if (controls_validOption(p, i)) menuItem++;
        }
        if (gControlFirst[p] < menuItem - (gControlMenuSize - 2)) gControlFirst[p] = menuItem - (gControlMenuSize - 2);
        if (gControlFirst[p] > menuItem - 1) gControlFirst[p] = menuItem - 1;
        if (gControlFirst[p] > maxFirstItem) gControlFirst[p] = maxFirstItem;
        if (gControlFirst[p] < 0) gControlFirst[p] = 0;
        if (gControlSelect[p] == 11) {
            if (gPlayers[p].edges & 2) {
                AudPlayEffect(43, 0, 1, 0);
                SetDefaultControls(controls);
            }
        } else if ((j = gPlayers[p].edges) & 0x1000) {
            AudPlayEffect(41, 0, 1, 0);
            if (gControlSelect[p] == 10) {
                s8 rumble;
                rumble = (*gPlayers[p].save)->rumble;
                rumble--;
                if (rumble < 0) rumble = 2;
                (*gPlayers[p].save)->rumble = rumble;
            } else {
                do {
                    controls[gControlSelect[p]]--;
                    if (controls[gControlSelect[p]] < 0) controls[gControlSelect[p]] = 25;
                } while (!controls_validControl(p));
            }
        } else if (j & 0x2000) {
            AudPlayEffect(44, 0, 1, 0);
            if (gControlSelect[p] == 10) {
                s8 rumble;
                rumble = (*gPlayers[p].save)->rumble;
                rumble++;
                if (rumble > 2) rumble = 0;
                (*gPlayers[p].save)->rumble = rumble;
            } else {
                do {
                    controls[gControlSelect[p]]++;
                    if (controls[gControlSelect[p]] >= 26) controls[gControlSelect[p]] = 0;
                } while (!controls_validControl(p));
            }
        }
        for (i = 0; i < 10; i++) {
            gConflict[p][i] = 0;
            gConflictExists[p] = 0;
        }
        for (i = 0; i < 9; i++) {
            for (j = i + 1; j < 10; j++) {
                if ((controls[i] == controls[j] ||
                    (controls[i] == 21 && controls[j] == 8) ||
                    (controls[i] == 8 && controls[j] == 21) ||
                    (controls[i] == 22 && controls[j] == 9) ||
                    (controls[i] == 9 && controls[j] == 22)) && controls[i] != 19 && controls[i] != 25) {
                    gConflict[p][i] = 1;
                    gConflict[p][j] = 1;
                    gConflictExists[p] = 1;
                }
            }
        }
        forcedExit = 0;
        if ((gGameState & 0x7C03FFFE) && gPlayers[p].save && (*gPlayers[p].save)->fileh &&
            !PS_PakInserted((*(*gPlayers[p].save)->fileh)->pak)) forcedExit = 1;
        if (forcedExit) {
            controlScrActive[p] = 0;
            controls_uninitButtons(p);
            exitControlScreen = 2;
        } else if ((j = gPlayers[p].edges) & 5) {
            s8 conflict = 0;
            for (i = 0; i < 10; i++) {
                if (gConflict[p][i] == 1) conflict = 1;
            }
            if (conflict == 1) AudPlayEffect(42, 0, 1, 0);
            else {
                s32 playerCont = gPlayers[p].controller;
                PlayStartABSound(j);
                if (playerCont != 5) {
                    if (gPlayers[p].save) {
                        if (game_memcmp((u8 *)gNewControls[playerCont], (u8 *)(*gPlayers[p].save)->controls, 10)) gControlsSet[playerCont] = 1;
                    } else {
                        if (game_memcmp((u8 *)gNewControls[playerCont], (u8 *)gJustPlayers[playerCont]->controls, 10)) gControlsSet[playerCont] = 1;
                    }
                }
                SetControls(0);
                controlScrActive[p] = 0;
                controls_uninitButtons(p);
                exitControlScreen = 2;
            }
        }
        prevControlScrActive[p] = controlScrActive[p];
    }
    for (p = 0; p < gNumWindows; p++) {
        if (controlScrActive[p] == 1) exitControlScreen = 0;
    }
    if (gSoftReset || exitControlScreen) {
        cleanupControlScreen();
        if (!(gGameState & 0x7C03FFFE)) {
            RemovePanes(0);
            setPauseState(1);
        }
    }
}
