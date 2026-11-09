#include "game/game.h"

typedef struct CtIdx { u8 pad0[32]; u16 rumbleOff; u16 rumbleNames; u8 pad36[2]; u16 controlNames; } CtIdx;
typedef struct TextState { void *word0; char **fixed; void *word8; CtIdx *indices; char **strings; } TextState;
typedef struct PlayerRow { u8 vehicle; u8 pak; u8 other[70]; char **name; } PlayerRow;   /* 76 B */
/* *name is the player's profile record (save.c's SaveSlot): 64 bytes on the PC, its name at +0x20 and the
 * rumble byte at +0x37 (PROFILE_OFS, pc_prelude.h; docs/port/POINTERS.md 9.10). */
typedef struct CtRect { s32 f0; s32 y; s32 w; s32 h; s32 x; s32 f20; } CtRect;          /* 24 B */

typedef struct ContData { s8 b0; u8 rest[15]; } ContData;                                   /* 16 B */

VIEW(TextState, gText);
#define gText gText_view

extern PlayerRow gPlayers[];                   /* gPlayers */

VIEW(CtRect[][4], controls_gPlayerRects);
#define controls_gPlayerRects controls_gPlayerRects_view
VIEW(ContData[], gControllerData);
#define gControllerData gControllerData_view

VIEW(s32, gGameState);
#define gGameState gGameState_view

typedef struct CtButton { u8 pad[8]; f32 angle; u8 pad12[36]; u8 pos[8]; f32 depth; u8 alpha; u8 pad61[3]; } CtButton; /* 64 B */
typedef struct CtCamera { u8 data[152]; } CtCamera;
VIEW(CtButton[][17], controls_buttonInfo);
#define controls_buttonInfo controls_buttonInfo_view
VIEW(CtCamera[], gCamera);
#define gCamera gCamera_view

VIEW(TextColor[2], flashColors);
#define flashColors flashColors_view

void TB_FlashTextColors(TextColor first, TextColor second);   /* TB_FlashTextColors (locked, audioscr) */

void controls_animButtonText(s8 p)
{
    s32 i;
    s32 x;
    s32 y;
    char *str;
    u8 str1[64];
    f32 gTextPosScale;
    s16 screenPos[2];
    s32 w;

    gTextPosScale = controls_gButtonScale;
    if (gNumWindows > 1) gTextPosScale *= 0.5f;
    HAL_SetFont(11);
    TB_SetJust(1, 1);
    if (gPlayers[p].pak != 5 && (controlScrActive[p] == 1 || (gGameState & 0x7C0000))) {
        ZOID_ProjectWindow(p, controls_buttonInfo[p][13].pos, &gCamera[p], 0, screenPos);
        x = screenPos[0];
        y = screenPos[1];
        if (gNumWindows > 1) HAL_SetFont(11);
        else HAL_SetFont(13);
        TB_SetTextColor(1);
        w = gNumWindows == 1 ? 150 : 69;
        str = *gPlayers[p].name + PROFILE_OFS(20);
        ct_DrawEllipseText(x, y, w, str);
        TB_SetJust(0, 1);
        if (!controlScrActive[p]) {
            if (gGameState & 0x7C0000) {
                if (gNumWindows == 1) HAL_SetFont(10);
                else HAL_SetFont(11);
                TB_SetTextColor(1);
                TB_SetJust(1, 1);
                TB_DrawTextScrollBox(controls_gPlayerRects[gNumWindows - 1][p].x, controls_gPlayerRects[gNumWindows - 1][p].f20,
                                   controls_gPlayerRects[gNumWindows - 1][p].w, controls_gPlayerRects[gNumWindows - 1][p].h,
                                   -1, 0, gText.fixed[38]);
                TB_SetJust(0, 3);
            }
        } else {
            if (gNumWindows == 1) HAL_SetFont(11);
            else HAL_SetFont(12);
            for (i = 0; i < 12; i++) {
                if (controls_buttonInfo[p][i].angle > 0.69813168f && controls_buttonInfo[p][i].angle < 2.443461f) continue;
                if (!controls_buttonInfo[p][i].alpha) continue;
                HAL_SetTextTranslucency(controls_buttonInfo[p][i].alpha);
                HAL_SetMenuTextDepth(controls_buttonInfo[p][i].depth - 10.0f);
                ZOID_ProjectWindow(p, controls_buttonInfo[p][i].pos, &gCamera[p], 0, screenPos);
                if (i == 10) x = screenPos[0] - 35.0f * gTextPosScale * 300.0f / controls_buttonZ;
                else if (i == 11) x = screenPos[0] - 55.0f * gTextPosScale * 300.0f / controls_buttonZ;
                else x = screenPos[0] - 45.0f * gTextPosScale * 300.0f / controls_buttonZ;
                y = screenPos[1];
                if (controls_buttonInfo[p][i].angle < 1.5707964f) {
                    TB_FlashTextColors(flashColors[0], flashColors[1]);
                    TB_SetTextColor(1);
                    if (!PS_PakRumble(gPlayers[p].pak) && i == 10) TB_SetTextColor(3);
                } else {
                    TB_FlashTextColors(flashColors[0], flashColors[1]);
                    TB_SetTextColor(22);
                }
                str = gText.strings[gText.indices->controlNames + i];
                TB_DrawText(x, y, str);
                if (i == 10) {
                    ZOID_ProjectWindow(p, controls_buttonInfo[p][12].pos, &gCamera[p], 0, screenPos);
                    x = screenPos[0] - 25.0f * gTextPosScale * 300.0f / controls_buttonZ;
                    y = screenPos[1];
                    if (!PS_PakRumble(gPlayers[p].pak)) str = gText.strings[gText.indices->rumbleOff];
                    else str = gText.strings[gText.indices->rumbleNames + ((s8 *)*gPlayers[p].name)[PROFILE_OFS(43)]];
                    TB_DrawText(x, y, str);
                }
            }
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(0.0f);
}

void ct_DrawEllipseText(s16 x, s16 y, s16 w, void *text)
{
    u8 str[128];
    TB_EllipseText((char *)str, text, w);
    TB_DrawText(x, y, (char *)str);
}

s32 controls_animText(s32 arg0)
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
        if (gPlayers[p].pak != 5 && (controlScrActive[p] == 1 || (gGameState & 0x7C0000))) {
            y = controls_gPlayerRects[gNumWindows - 1][p].y + 10;
            x = controls_gPlayerRects[gNumWindows - 1][p].x;
            TB_SetTextColor(playerTextColor[p]);
            if (!gControllerData[p].b0) {
                TB_Sprintf(str1, controller_removed_format, p + 1, gText.fixed[233]);
            } else {
                TB_EllipseText(pName, *gPlayers[p].name + PROFILE_OFS(20), 69);
                TB_Sprintf(str1, gText.fixed[37], pName);
            }
            TB_DrawText(TB_CenterText(str1, x), y, str1);
            TB_GetFontLineHeight();
            if (!controlScrActive[p]) {
                if (gGameState & 0x7C0000) {
                    TB_SetTextColor(1);
                    TB_SetJust(1, 1);
                    TB_DrawTextScrollBox(controls_gPlayerRects[gNumWindows - 1][p].x, controls_gPlayerRects[gNumWindows - 1][p].f20,
                                       controls_gPlayerRects[gNumWindows - 1][p].w / 2, controls_gPlayerRects[gNumWindows - 1][p].h,
                                       -1, 0, gText.fixed[38]);
                    TB_SetJust(0, 3);
                }
            } else {
                itemsDrawn = 0;
                x = controls_gPlayerRects[gNumWindows - 1][p].x + 10;
                y = controls_gPlayerRects[gNumWindows - 1][p].y + 24;
                for (i = 0; i < 12; i++) {
                    if (i >= gControlFirst[p] && controls_validOption(p, i)) {
                        if (i == 10) {}
                        if (!PS_PakRumble(gPlayers[p].pak) && i == 10)
                            TB_SetTextColor(3);
                        else
                            TB_SetTextColor(i == gControlSelect[p] ? 22 : 1);
                        str = gText.strings[gText.indices->controlNames + i];
                        if (i == 11)
                            TB_DrawText(TB_CenterText(str, controls_gPlayerRects[gNumWindows - 1][p].x), y, str);
                        else
                            TB_DrawText(TB_RightText(str, x), y, str);
                        if (i == 10) {
                            if (!PS_PakRumble(gPlayers[p].pak))
                                str = gText.strings[gText.indices->rumbleOff];
                            else
                                str = gText.strings[gText.indices->rumbleNames + ((s8 *)*gPlayers[p].name)[PROFILE_OFS(43)]];
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
