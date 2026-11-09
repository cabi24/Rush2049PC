/* updateControlScreen: the controller-setup screen (MAT3 controlscr.c updateControlScreen), where each player
 * maps the car's ten controls (gas, brake, steering, shifts, camera ...) to buttons and sets the rumble. It is the
 * front end's GS_CONTROLS screen (one window per player) and a page of the pause menu, where each player opens it
 * with A. */
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

void *memcpy(void *, const void *, u32);

extern PadState gPlayers[4];           /* gPlayers */

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

/* The list's rows (gControlSelect): the ten controls, then these two. */
#define CONTROLS_ROW_RUMBLE 10      /* the rumble setting (0-2), offered when the player's pak is a Rumble Pak */
#define CONTROLS_ROW_DEFAULTS 11    /* A sets the default controls */
#define CONTROLS_ROW_LAST 11
#define CONTROLS_COUNT 10           /* the controls a profile keeps */
#define CONTROL_CHOICES 26          /* a control's values: 19 game buttons, then the analog sources (SetControls) */
#define CONTROLLER_NONE 5           /* gPlayers[].controller of a player without one */
#define PANE_STYLE 176              /* InitPane's style for the screen's boxes */
/* The menu sounds (the effects PlayUDSound, PlayLRSound and PlayStartABSound play). */
#define SND_MENU_DOWN 39
#define SND_MENU_UP 40
#define SND_MENU_LEFT 41
#define SND_MENU_ERROR 42
#define SND_MENU_DEFAULTS 43
#define SND_MENU_RIGHT 44

/* The screen's frame, for every player at once.
 * - First frame: in the front end only if a player has the screen open; in the pause menu a box per window. The
 *   list's height, the text blit (front end, or the pause menu's one-window or split layout) and the buttons'
 *   layout by window count.
 * - Each player with a controller: in a race A opens it. When it opens, the player's controls are copied from the
 *   profile (or the guest's, gJustPlayers) into gNewControls, and in the front end the buttons are made. Up and down
 *   move between the offered rows (with a conflict, only the conflicting ones and the defaults row), the list
 *   scrolls to keep the row in view, and left and right change the row's value: the rumble 0-2 or the control,
 *   skipping values controls_validControl refuses. Then every pair of controls on one value (or on the two
 *   halves of one analog source: 8 and 21, 9 and 22; 19 and 25 may share) is marked a conflict.
 * - Start or B leaves, unless there is a conflict: a changed control marks the controller's controls set
 *   (gControlsSet), SetControls applies them. In the front end the screen also closes when the profile's pak is
 *   pulled.
 * - When no player has it open, or on a soft reset, the screen closes; the pause menu goes back to its main page. */
void updateControlScreen(void) {
    s32 forcedExit;
    s32 p;
    s32 i;
    s32 j;
    s32 menuItem;
    s32 maxFirstItem;
    s8 *controls;

    if (!controls_inited) {
        if (gGameState & GS_FRONTEND_MASK) {
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
                    PANE_STYLE, 0, 0, 0);
            }
            for (p = gNumWindows; p < 4; p++) prevControlScrActive[p] = 0;
        }
        calcControlMenuMaxSize();
        if (gGameState & GS_FRONTEND_MASK)
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
        if (gPlayers[p].controller == CONTROLLER_NONE) continue;
        if (!controlScrActive[p]) {
            if ((gGameState & GS_RACE_MASK) && (gPlayers[p].edges & BUTTON_A)) controlScrActive[p] = 1;
            continue;
        }
        if (!prevControlScrActive[p]) {
            gControlSelect[p] = 0;
            gControlFirst[p] = 0;
            for (i = 0; i < CONTROLS_COUNT; i++) {
                gConflict[p][i] = 0;
                gConflictExists[p] = 0;
            }
            if (gPlayers[p].save)
                memcpy(gNewControls[gPlayers[p].controller], (*gPlayers[p].save)->controls, CONTROLS_COUNT);
            else
                memcpy(gNewControls[gPlayers[p].controller], gJustPlayers[gPlayers[p].controller]->controls, CONTROLS_COUNT);
            if (gGameState & GS_FRONTEND_MASK) controls_initButtons(p);
        }
        controls_updateButtons(p);
        controls = gNewControls[gPlayers[p].controller];
        while ((gConflictExists[p] == 1 && !gConflict[p][gControlSelect[p]] && gControlSelect[p] != CONTROLS_ROW_DEFAULTS) ||
            !controls_validOption(p, gControlSelect[p]) || (!PS_PakRumble(gPlayers[p].controller) && gControlSelect[p] == CONTROLS_ROW_RUMBLE)) {
            gControlSelect[p]++;
            if (gControlSelect[p] > CONTROLS_ROW_LAST) gControlSelect[p] = 0;
        }
        if (gPlayers[p].edges & BUTTON_UP) {
            AudPlayEffect(SND_MENU_UP, 0, 1, 0);
            do {
                gControlSelect[p]--;
                if (gControlSelect[p] < 0) gControlSelect[p] = CONTROLS_ROW_LAST;
            } while ((gConflictExists[p] == 1 && !gConflict[p][gControlSelect[p]] && gControlSelect[p] != CONTROLS_ROW_DEFAULTS) ||
                !controls_validOption(p, gControlSelect[p]) || (!PS_PakRumble(gPlayers[p].controller) && gControlSelect[p] == CONTROLS_ROW_RUMBLE));
        }
        if (gPlayers[p].edges & BUTTON_DOWN) {
            AudPlayEffect(SND_MENU_DOWN, 0, 1, 0);
            do {
                gControlSelect[p]++;
                if (gControlSelect[p] > CONTROLS_ROW_LAST) gControlSelect[p] = 0;
            } while ((gConflictExists[p] == 1 && !gConflict[p][gControlSelect[p]] && gControlSelect[p] != CONTROLS_ROW_DEFAULTS) ||
                !controls_validOption(p, gControlSelect[p]) || (!PS_PakRumble(gPlayers[p].controller) && gControlSelect[p] == CONTROLS_ROW_RUMBLE));
        }
        /* Scroll: the first row shown stays between the row two above the selection's place and the selection, and
         * no further down than leaves the list's last screenful in view. */
        maxFirstItem = 0;
        menuItem = 0;
        for (i = CONTROLS_ROW_LAST; i >= 0; i--) {
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
        if (gControlSelect[p] == CONTROLS_ROW_DEFAULTS) {
            if (gPlayers[p].edges & BUTTON_A) {
                AudPlayEffect(SND_MENU_DEFAULTS, 0, 1, 0);
                SetDefaultControls(controls);
            }
        } else if ((j = gPlayers[p].edges) & BUTTON_LEFT) {
            AudPlayEffect(SND_MENU_LEFT, 0, 1, 0);
            if (gControlSelect[p] == CONTROLS_ROW_RUMBLE) {
                s8 rumble;
                rumble = (*gPlayers[p].save)->rumble;
                rumble--;
                if (rumble < 0) rumble = 2;
                (*gPlayers[p].save)->rumble = rumble;
            } else {
                do {
                    controls[gControlSelect[p]]--;
                    if (controls[gControlSelect[p]] < 0) controls[gControlSelect[p]] = CONTROL_CHOICES - 1;
                } while (!controls_validControl(p));
            }
        } else if (j & BUTTON_RIGHT) {
            AudPlayEffect(SND_MENU_RIGHT, 0, 1, 0);
            if (gControlSelect[p] == CONTROLS_ROW_RUMBLE) {
                s8 rumble;
                rumble = (*gPlayers[p].save)->rumble;
                rumble++;
                if (rumble > 2) rumble = 0;
                (*gPlayers[p].save)->rumble = rumble;
            } else {
                do {
                    controls[gControlSelect[p]]++;
                    if (controls[gControlSelect[p]] >= CONTROL_CHOICES) controls[gControlSelect[p]] = 0;
                } while (!controls_validControl(p));
            }
        }
        for (i = 0; i < CONTROLS_COUNT; i++) {
            gConflict[p][i] = 0;
            gConflictExists[p] = 0;
        }
        for (i = 0; i < CONTROLS_COUNT - 1; i++) {
            for (j = i + 1; j < CONTROLS_COUNT; j++) {
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
        if ((gGameState & GS_FRONTEND_MASK) && gPlayers[p].save && (*gPlayers[p].save)->fileh &&
            !PS_PakInserted((*(*gPlayers[p].save)->fileh)->pak)) forcedExit = 1;
        if (forcedExit) {
            controlScrActive[p] = 0;
            controls_uninitButtons(p);
            exitControlScreen = 2;
        } else if ((j = gPlayers[p].edges) & (BUTTON_START | BUTTON_B)) {
            s8 conflict = 0;
            for (i = 0; i < CONTROLS_COUNT; i++) {
                if (gConflict[p][i] == 1) conflict = 1;
            }
            if (conflict == 1) AudPlayEffect(SND_MENU_ERROR, 0, 1, 0);
            else {
                s32 playerCont = gPlayers[p].controller;
                PlayStartABSound(j);
                if (playerCont != CONTROLLER_NONE) {
                    if (gPlayers[p].save) {
                        if (game_memcmp((u8 *)gNewControls[playerCont], (u8 *)(*gPlayers[p].save)->controls, CONTROLS_COUNT)) gControlsSet[playerCont] = 1;
                    } else {
                        if (game_memcmp((u8 *)gNewControls[playerCont], (u8 *)gJustPlayers[playerCont]->controls, CONTROLS_COUNT)) gControlsSet[playerCont] = 1;
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
        if (!(gGameState & GS_FRONTEND_MASK)) {
            RemovePanes(0);
            setPauseState(kPauseMainScreen);
        }
    }
}
