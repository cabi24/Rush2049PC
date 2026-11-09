#include "game/game.h"

typedef TextColor Color4;   /* the flashing colours (include/game's TextColor) */
typedef struct FileInfo FileInfo;
typedef struct Profile Profile;
struct Profile { Profile **next, **previous; FileInfo **file; u32 id0, id1; u8 name[24]; u8 **stats; };
typedef struct Player { u32 held, pressed, unknown08, repeated; u8 unknown10[50]; u8 paint, unknown43[5]; Profile **owner; } Player;
typedef struct Record Record;
/* PC: as cleanup_colortrace's Record: unknown24 is pointer-sized. */
struct Record { Record **next; FileInfo **file; s8 track; u8 mode, name[14]; u32 id0, id1; f32 time; void *unknown24; void **active; };
typedef struct Language { s32 unknown0; u8 **text; s32 unknown8; u16 *indices; u8 **indexed; } Language;
typedef struct Slot { s32 kind, handle; f32 angle, matrix[3][3], position[3]; u8 alpha, unknown3D[3]; } Slot;
typedef struct Blit Blit;

VIEW(Language, gText);
#define gText gText_view

extern Player gPlayers[];
VIEW(Slot[][44], carscr_buttonInfo);
#define carscr_buttonInfo carscr_buttonInfo_view

VIEW(Record **[4], gGhosts);
#define gGhosts gGhosts_view
VIEW(Color4, flashColors_at_1);
#define flashColors_at_1 flashColors_at_1_view

extern void TB_FlashTextColors(TextColor first, TextColor second);

static void animButtonText(s32 p);
static void drawOptions(s32 p);

s32 carscr_animText(Blit *blt)
{
    s32 i;
    for (i = 0; i < gNumPlayers; i++) {
        if (carscr_buttonsInited[i] == 1) animButtonText(i);
        else drawOptions(i);
    }
    return 1;
}

static void animButtonText(s32 p)
{
    s32 i;
    s32 x;
    s32 y;
    s32 w;
    u8 *str;
    u8 str1[64];
    s16 screenPos[2];
    f32 gTextPosScale = carscr_gButtonScale;
    if (gNumWindows > 1) gTextPosScale *= 0.5f;
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 1);
    if (gNumWindows == 1) { w = 140; HAL_SetFont(13); }
    else { w = 69; HAL_SetFont(12); }
    ZOID_ProjectWindow(p, carscr_buttonInfo[p][38].position, &gCamera[p], 0, screenPos);
    x = screenPos[0];
    y = screenPos[1];
    str = (*gPlayers[p].owner)->name;
    TB_SetTextColor(1);
    TB_DrawEllipseText(x, y, w, str);
    TB_SetJust(0, 1);
    if (gRace == kGhost) {
        s32 ghostIdx;
        for (ghostIdx = 1; ghostIdx < gNumGhosts + 1; ghostIdx++) {
            f32 pos[3];
            ZOID_ProjectWindow(ghostIdx, carscr_buttonInfo[0][38].position, &gCamera[ghostIdx], 0, screenPos);
            x = screenPos[0];
            y = screenPos[1];
            TB_Sprintf(str1, "GHOST %d", ghostIdx);
            TB_SetTextColor(1);
            TB_DrawText(TB_CenterText(str1, x), y, str1);
            pos[0] = carscr_buttonInfo[0][38].position[0];
            pos[1] = carscr_buttonInfo[0][38].position[1];
            pos[2] = carscr_buttonInfo[0][38].position[2];
            pos[1] = -pos[1] / 2.0f;
            ZOID_ProjectWindow(ghostIdx, pos, &gCamera[ghostIdx], 0, screenPos);
            x = screenPos[0];
            y = screenPos[1];
            if (!gGhosts[ghostIdx]) {
                str = !ghosts_enabled ? gText.text[225] : gText.text[224];
                TB_SetTextColor(1);
                TB_DrawText(TB_CenterText(str, x), y, str);
            } else {
                str = (*gGhosts[ghostIdx])->name;
                TB_SetTextColor(playerTextColor[ghostIdx]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
                TB_Sprintf(str1, "TIME %01d:%02d.%03d", (s32)(*gGhosts[ghostIdx])->time / 60,
                             (s32)(*gGhosts[ghostIdx])->time % 60, (s32)(1000.0f * (*gGhosts[ghostIdx])->time) % 1000);
                TB_SetTextColor(1);
                TB_DrawText(TB_CenterText(str1, x), y, str1);
            }
        }
    }
    if (gCarLocked[p]) {
        s32 notReadyCount;
        s32 notReadyIdx;
        s32 pIdx;
        f32 msgPos[3] = {0.0f, -120.0f, 300.0f};
        ZOID_ProjectWindow(p, msgPos, &gCamera[p], 0, screenPos);
        x = screenPos[0];
        y = screenPos[1] - TB_GetFontLineHeight();
        notReadyCount = 0;
        for (pIdx = 0; pIdx < gNumPlayers; pIdx++) {
            if (p != pIdx && !gCarLocked[pIdx]) { notReadyIdx = pIdx; notReadyCount++; }
        }
        if (!notReadyCount) {
            y += TB_GetFontLineHeight();
            str = (u8 *)"GET READY TO RACE";
            TB_SetTextColor(1);
        } else {
            str = gText.text[151];
            TB_SetTextColor(1);
            TB_DrawText(TB_CenterText(str, x), y, str);
            y += TB_GetFontLineHeight();
            if (notReadyCount == 1) {
                TB_SetTextColor(playerTextColor[notReadyIdx]);
                str = (*gPlayers[notReadyIdx].owner)->name;
            } else str = gText.text[31];
        }
        TB_DrawText(TB_CenterText(str, x), y, str);
        TB_SetJust(0, 3);
        HAL_SetTextTranslucency(-1.0f);
        return;
    }
    if (gNumWindows == 1) HAL_SetFont(11); else HAL_SetFont(12);
    for (i = 0; i < carscr_gNumOptions[p]; i++) {
        s32 item = carscr_gOpts[p][i];
        if ((carscr_buttonInfo[p][item].angle > 0.6981317f && carscr_buttonInfo[p][item].angle < 2.443461f) || !carscr_buttonInfo[p][item].alpha) continue;
        HAL_SetTextTranslucency(carscr_buttonInfo[p][item].alpha);
        if (carscr_buttonInfo[p][item].angle < 1.5707964f) {
            TB_FlashTextColors(flashColors[0], flashColors[1]);
            TB_SetTextColor(1);
        } else {
            TB_FlashTextColors(flashColors[0], flashColors[1]);
            TB_SetTextColor(22);
        }
        HAL_SetMenuTextDepth(carscr_buttonInfo[p][item].position[2] - 10.0f);
        ZOID_ProjectWindow(p, carscr_buttonInfo[p][item].position, &gCamera[p], 0, screenPos);
        x = screenPos[0] - 45.0f * gTextPosScale * 300.0f / carscr_buttonZ;
        y = screenPos[1];
        str = gNumWindows == 1 ? gText.indexed[gText.indices[2] + item]
                              : gText.indexed[gText.indices[3] + item];
        TB_DrawText(x, y, str);
        switch (item) {
        case 0: str = CarTitles[gThisCar[p]]; break;
        case 1: str = gText.indexed[gText.indices[4] + gCarTrans[p+1][gThisCar[p]]]; break;
        case 2: str = gText.indexed[gText.indices[5] + gCarHandling[p+1][gThisCar[p]]]; break;
        case 3: str = gText.indexed[gText.indices[6] + gCarEngines[p+1][gThisCar[p]]]; break;
        case 4: str = gText.indexed[gText.indices[7] + gCarTires[p+1][gThisCar[p]]]; break;
        case 5: str = gText.indexed[gText.indices[8] + gCarFrame[p+1][gThisCar[p]]]; break;
        case 6: str = gText.indexed[gText.indices[9] + gCarWings[p+1][gThisCar[p]]]; break;
        case 10: str = gText.indexed[gText.indices[12] + gCarTeam[p]]; break;
        case 11: str = gText.indexed[gText.indices[10] + TIRE_RIMS(p+1)[gThisCar[p]]]; break;
        case 12: str = gText.indexed[gText.indices[11] + gCarHorn[p+1][gThisCar[p]]]; break;
        case 15: TB_Sprintf(str1, "%d", gCarSheen[p] + 1); str = str1; break;
        case 16:
            if (gGhosts[1]) TB_Strcpy(str1, (*gGhosts[1])->name);
            else TB_Strcpy(str1, gText.indexed[gText.indices[15]]);
            str = str1;
            break;
        case 17:
            if (gGhosts[2]) TB_Strcpy(str1, (*gGhosts[2])->name);
            else TB_Strcpy(str1, gText.indexed[gText.indices[15]]);
            str = str1;
            break;
        case 18:
            if (gGhosts[3]) TB_Strcpy(str1, (*gGhosts[3])->name);
            else TB_Strcpy(str1, gText.indexed[gText.indices[15]]);
            str = str1;
            break;
        default: str = 0; break;
        }
        if (str) {
            ZOID_ProjectWindow(p, carscr_buttonInfo[p][item+19].position, &gCamera[p], 0, screenPos);
            x = screenPos[0] - 50.0f * gTextPosScale * 300.0f / carscr_buttonZ;
            y = screenPos[1];
            TB_DrawText(x, y, str);
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(-1.0f);
}

static void drawOptions(s32 p)
{
}

