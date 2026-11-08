/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* updateControlScreen = updateControlScreen (controlscr.c, the CONTROLS screen; map: proven).
 * Provenance: direct typed port of MAT3 controlscr.c updateControlScreen (GC 0x8006AFCC; locals and their
 * order from rush_d_dwarf.c: forcedExit, p, i, j, menuItem, maxFirstItem, controls + block-scope skipInit,
 * rumble, rumble, conflict, playerCont), built on
 * (itself on), which supplied every N64
 * offset and the private-ABI group context. N64 differences from the GC body, all read from the retail words:
 * ten controls (GC eleven), option 10 = rumble and 11 = defaults (GC 11/12); the conflict reset runs before the
 * profile copy; the defaults option does not reset rumble and the rumble options do not set gControlsSet; the
 * 21->8 / 22->9 fold loop is absent and SetControls(0) follows the memcmp.
 * PS_PakRumble / PS_PakInserted = the locked kept getters PS_PakRumble / PS_PakInserted, inlined by umerge
 * (their per-site areas give retail's frame 240). validOption = controls_validOption, called by jal here (umerge's
 * budget) and inlined in updateButtons / calcControlMenuMaxSize.
 * Shaping (disclosed): `j`, dead after the duplicate scan, carries gPlayers[p].edges through the 0x1000/0x2000
 * else-if chain and the &5 test (retail keeps both loads in a0; found the &5 half). The two clear loops
 * put their two stores on separate lines (as1 store order). memcmp's count is size_t (u32): the locked
 * declaration says s32; with s32 the 10 shares the t1 constant web. No unused locals, no compiled-out reads.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

/* PC: the pak file record's and the profile's host layouts (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct { void *next, *prev; void (*RemoveFile)(void *); void (*MoveFile)(void *, void *); u8 pak; } PSFile;
typedef struct {
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
typedef struct { s32 x, y, w, h, cx, cy; } PlayerRect;        /* 0x18 */

s32 *InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);
void SetControls(s32);
s32 AudPlayEffect(s32, s32, s32, u8);
s32 game_memcmp(u8 *, u8 *, u32);                              /* memcmp (size_t count) */
void setPauseState(s32);
void SetDefaultControls(void *);
s32 controls_validControl(s8);                                           /* validControl */
s32 controls_validOption(s8, s32);                                      /* validOption */
void calcControlMenuMaxSize(void);
void *memcpy(void *, const void *, u32);
void cleanupControlScreen(void);
void PlayStartABSound(s32);
void *NewMultiBlit(s16, s16, void *, s16);
void RemovePanes(s32);
void controls_initButtons(s8);                                    /* initButtons */
void controls_updateButtons(s8);                                          /* updateButtons */
void controls_uninitButtons(s8);                                       /* uninitButtons */

extern u32 gGameState;
extern PadState gPlayers[4];           /* gPlayers */
extern s16 gNumWindows;
extern s8 controlScrActive[4];                 /* controlScrActive */
extern PlayerRect controls_gPlayerRects[4][4];      /* gPlayerRects */
extern s8 prevControlScrActive[4];                 /* prevControlScrActive */
extern s16 gControlSelect[4];                /* gControlSelect */
extern s16 gControlFirst[4];                /* gControlFirst */
extern s16 gControlMenuSize;
extern s8 gConflict[4][10];             /* gConflict */
extern s8 gConflictExists[4];                 /* gConflictExists */
extern s8 gNewControls[4][10];             /* gNewControls */
extern Profile *gJustPlayers[4];           /* gJustPlayers */
extern s8 gControlsSet[4];                 /* gControlsSet */
extern s8 gSoftReset;
extern s8 controls_inited;                    /* inited */
extern void *controls_blit;                 /* blit: pointer-sized storage */
extern s32 exitControlScreen;
extern f32 controls_gButtonZoom, controls_gButtonScale, controls_gButtonSpace, controls_buttonX, controls_maxY, controls_minY, controls_buttonZ, controls_cursorZ;
extern struct pc_multiblit buttonsFrontEnd[81], buttonsPauseSingle[21], buttonsPauseMulti[41];   /* NewMultiBlit's (pc_prelude.h) */

s8 PS_PakRumble(s32 pak);                                       /* PS_PakRumble (locked, SaveInit) */
s8 PS_PakInserted(s32 pak);                                       /* PS_PakInserted (locked, SaveInit) */

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
