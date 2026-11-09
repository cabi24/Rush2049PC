/* The controller-setup screen's text (MAT3 controlscr.c animText and animButtonText): each player's name and list
 * of controls, drawn over the 3D buttons in the front end, or as a flat list in the pause menu. */
#include "game/game.h"

/* A profile (MAT3 tPlayerProfile; save.c's SaveSlot) in its PC layout, 64 bytes: its links and its pak file
 * handle are pointers, so the rest is later than on the N64 (pc_prelude.h PROFILE_OFS, docs/port/POINTERS.md
 * 9.10). */
typedef struct PlayerProfile {
    void *next, *prev;      /* 0x00 */
    void *fileh;            /* 0x10 its pak file */
    u32 id[2];              /* 0x18 */
    char name[13];          /* 0x20 (N64 0x14) */
    s8 controls[10];        /* 0x2D */
    s8 rumble;              /* 0x37 (N64 0x2B) the rumble setting, 0-2 */
    void *datah;            /* 0x38 */
} PlayerProfile;

/* A player's pad record (MAT3 tPadState, gPlayers; 76 bytes, 80 on the PC). */
typedef struct PadState {
    u8 slot;
    u8 controller;          /* 0x01 its controller, CONTROLLER_NONE without one */
    u8 other[70];
    PlayerProfile **profile; /* 0x48 its profile */
} PadState;

typedef struct ContData { s8 connected; u8 rest[15]; } ContData;   /* gControllerData: 16 bytes a controller */

extern PadState gPlayers[];

VIEW(ContData[], gControllerData);
#define gControllerData gControllerData_view


/* gText's entries */
#define TEXT_CONTROLS_TITLE 37      /* single[]: "%s's controls" over a player's list */
#define TEXT_PRESS_A 38             /* single[]: the prompt to open the screen in the pause menu */
#define TEXT_NO_CONTROLLER 233      /* single[]: after "controller n" when it is pulled out */
#define TEXT_RUMBLE_OFF 16          /* tab[]: the rumble row without a Rumble Pak */
#define TEXT_RUMBLE_NAMES 17        /* tab[]: the rumble settings' names */
#define TEXT_CONTROL_NAMES 19       /* tab[]: the rows' names */

/* Player p's text over the 3D buttons (the front end): the player's name on the title button; if the player hasn't
 * opened the screen (in a race), the prompt to press A; else each row's name beside its button, the button's
 * alpha and depth, skipped while the button faces away, flashing on the selected one, grey for the rumble row
 * without a Rumble Pak, and the rumble setting beside its value button. */
void controls_animButtonText(s8 p)
{
    s32 i;
    s32 x;
    s32 y;
    char *str;
    f32 gTextPosScale;
    s16 screenPos[2];
    s32 w;

    gTextPosScale = controls_gButtonScale;
    if (gNumWindows > 1) gTextPosScale *= 0.5f;
    HAL_SetFont(11);
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    if (gPlayers[p].controller != CONTROLLER_NONE && (controlScrActive[p] == 1 || (gGameState & GS_RACE_MASK))) {
        ZOID_ProjectWindow(p, controls_buttonInfo[p][CONTROLS_TITLE].position, &gCamera[p], 0, screenPos);
        x = screenPos[0];
        y = screenPos[1];
        if (gNumWindows > 1) HAL_SetFont(11);
        else HAL_SetFont(13);
        TB_SetTextColor(kTB_WhiteText);
        w = gNumWindows == 1 ? 150 : 69;
        str = (*gPlayers[p].profile)->name;
        ct_DrawEllipseText(x, y, w, str);
        TB_SetJust(kTB_JustLeft, kTB_JustCenter);
        if (!controlScrActive[p]) {
            if (gGameState & GS_RACE_MASK) {
                if (gNumWindows == 1) HAL_SetFont(10);
                else HAL_SetFont(11);
                TB_SetTextColor(kTB_WhiteText);
                TB_SetJust(kTB_JustCenter, kTB_JustCenter);
                TB_DrawTextScrollBox(controls_gPlayerRects[gNumWindows - 1][p].cx, controls_gPlayerRects[gNumWindows - 1][p].cy,
                                   controls_gPlayerRects[gNumWindows - 1][p].w, controls_gPlayerRects[gNumWindows - 1][p].h,
                                   -1, 0, gText.single[TEXT_PRESS_A]);
                TB_SetJust(kTB_JustLeft, kTB_JustTop);
            }
        } else {
            if (gNumWindows == 1) HAL_SetFont(11);
            else HAL_SetFont(12);
            for (i = 0; i < CONTROLS_ROWS; i++) {
                if (controls_buttonInfo[p][i].angle > 0.69813168f && controls_buttonInfo[p][i].angle < 2.443461f) continue;
                if (!controls_buttonInfo[p][i].alpha) continue;
                HAL_SetTextTranslucency(controls_buttonInfo[p][i].alpha);
                HAL_SetMenuTextDepth(controls_buttonInfo[p][i].position[2] - 10.0f);
                ZOID_ProjectWindow(p, controls_buttonInfo[p][i].position, &gCamera[p], 0, screenPos);
                if (i == CONTROLS_ROW_RUMBLE) x = screenPos[0] - 35.0f * gTextPosScale * 300.0f / controls_buttonZ;
                else if (i == CONTROLS_ROW_DEFAULTS) x = screenPos[0] - 55.0f * gTextPosScale * 300.0f / controls_buttonZ;
                else x = screenPos[0] - 45.0f * gTextPosScale * 300.0f / controls_buttonZ;
                y = screenPos[1];
                if (controls_buttonInfo[p][i].angle < 1.5707964f) {
                    TB_FlashTextColors(flashColors[0], flashColors[1]);
                    TB_SetTextColor(kTB_WhiteText);
                    if (!PS_PakRumble(gPlayers[p].controller) && i == CONTROLS_ROW_RUMBLE) TB_SetTextColor(kTB_MedGryText);
                } else {
                    TB_FlashTextColors(flashColors[0], flashColors[1]);
                    TB_SetTextColor(kTB_FlashText);
                }
                str = gText.taba[gText.tab[TEXT_CONTROL_NAMES] + i];
                TB_DrawText(x, y, str);
                if (i == CONTROLS_ROW_RUMBLE) {
                    ZOID_ProjectWindow(p, controls_buttonInfo[p][12].position, &gCamera[p], 0, screenPos);
                    x = screenPos[0] - 25.0f * gTextPosScale * 300.0f / controls_buttonZ;
                    y = screenPos[1];
                    if (!PS_PakRumble(gPlayers[p].controller)) str = gText.taba[gText.tab[TEXT_RUMBLE_OFF]];
                    else str = gText.taba[gText.tab[TEXT_RUMBLE_NAMES] + (*gPlayers[p].profile)->rumble];
                    TB_DrawText(x, y, str);
                }
            }
        }
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(0.0f);
}

/* Draw text at (x, y), cut to w pixels with an ellipsis if it is wider. */
void ct_DrawEllipseText(s16 x, s16 y, s16 w, void *text)
{
    u8 str[128];
    TB_EllipseText((char *)str, text, w);
    TB_DrawText(x, y, (char *)str);
}

/* The screen's text blit callback, each frame: for each window, the 3D buttons' text if its buttons exist,
 * else the flat list (the pause menu): "controller n" and the no-controller text if its controller is out,
 * else the player's name in the title; the prompt if it isn't open; else the offered rows from gControlFirst, as
 * many as fit (gControlMenuSize), the selection flashing, each control's name right-aligned before the centre
 * and the rumble's value after it. Always 1. */
s32 controls_animText(s32 blit)
{
    s32 i;
    s32 p;
    s32 x;
    s32 y;
    char *str;
    char str1[64];
    char pName[14];
    s32 itemsDrawn;

    HAL_SetTextDepth(0.0f);
    for (p = 0; p < gNumWindows; p++) {
        if (controls_buttonsInited[p] == 1) {
            controls_animButtonText(p);
            continue;
        }
        HAL_SetFont(11);
        if (gPlayers[p].controller != CONTROLLER_NONE && (controlScrActive[p] == 1 || (gGameState & GS_RACE_MASK))) {
            y = controls_gPlayerRects[gNumWindows - 1][p].y + 10;
            x = controls_gPlayerRects[gNumWindows - 1][p].cx;
            TB_SetTextColor(playerTextColor[p]);
            if (!gControllerData[p].connected) {
                TB_Sprintf(str1, controller_removed_format, p + 1, gText.single[TEXT_NO_CONTROLLER]);
            } else {
                TB_EllipseText(pName, (*gPlayers[p].profile)->name, 69);
                TB_Sprintf(str1, gText.single[TEXT_CONTROLS_TITLE], pName);
            }
            TB_DrawText(TB_CenterText(str1, x), y, str1);
            TB_GetFontLineHeight();
            if (!controlScrActive[p]) {
                if (gGameState & GS_RACE_MASK) {
                    TB_SetTextColor(kTB_WhiteText);
                    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
                    TB_DrawTextScrollBox(controls_gPlayerRects[gNumWindows - 1][p].cx, controls_gPlayerRects[gNumWindows - 1][p].cy,
                                       controls_gPlayerRects[gNumWindows - 1][p].w / 2, controls_gPlayerRects[gNumWindows - 1][p].h,
                                       -1, 0, gText.single[TEXT_PRESS_A]);
                    TB_SetJust(kTB_JustLeft, kTB_JustTop);
                }
            } else {
                itemsDrawn = 0;
                x = controls_gPlayerRects[gNumWindows - 1][p].cx + 10;
                y = controls_gPlayerRects[gNumWindows - 1][p].y + 24;
                for (i = 0; i < CONTROLS_ROWS; i++) {
                    if (i >= gControlFirst[p] && controls_validOption(p, i)) {
                        if (!PS_PakRumble(gPlayers[p].controller) && i == CONTROLS_ROW_RUMBLE)
                            TB_SetTextColor(kTB_MedGryText);
                        else
                            TB_SetTextColor(i == gControlSelect[p] ? kTB_FlashText : kTB_WhiteText);
                        str = gText.taba[gText.tab[TEXT_CONTROL_NAMES] + i];
                        if (i == CONTROLS_ROW_DEFAULTS)
                            TB_DrawText(TB_CenterText(str, controls_gPlayerRects[gNumWindows - 1][p].cx), y, str);
                        else
                            TB_DrawText(TB_RightText(str, x), y, str);
                        if (i == CONTROLS_ROW_RUMBLE) {
                            if (!PS_PakRumble(gPlayers[p].controller))
                                str = gText.taba[gText.tab[TEXT_RUMBLE_OFF]];
                            else
                                str = gText.taba[gText.tab[TEXT_RUMBLE_NAMES] + (*gPlayers[p].profile)->rumble];
                            TB_DrawText(x + 20, y, str);
                        }
                        y += 16;
                        if (++itemsDrawn >= gControlMenuSize) break;
                    }
                }
            }
        }
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
