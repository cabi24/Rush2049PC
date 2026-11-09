/* animCoins: the coin counters, 3D coins and their counts (MAT3 hud.c). */
#include "game/game.h"

typedef struct RecordSlot { s16 id; u8 pad02[2]; f32 pos[9]; u8 obj[12]; } RecordSlot;   /* 52 bytes; +4 transform, +40 */
typedef struct RecordRow { RecordSlot slot[2]; } RecordRow;                                /* 104 bytes */
typedef struct Cam152 { u8 pad[152]; } Cam152;

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9): scale is N64 +0x10, host +0x14. */
typedef struct Model68 { u32 flags, w4; void *obj; f32 w; f32 scale; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Model68;
typedef struct Pos { s16 x, y; } Pos;

VIEW(RecordRow[], gPlayerCoin);
#define gPlayerCoin gPlayerCoin_view
VIEW(Cam152[], gCamera);
#define gCamera gCamera_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view
VIEW(Model68[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

VIEW(s8[], gOptions);
#define gOptions gOptions_view

s32 ZOID_AddObject(s32 model, void *matrix, s16 parent, s32 flags);

/* For each window's two counters (gold coins, silver coins): makes its spinning 3D coin the first time, places it at
 * coinPos (coinStuntPos in stunt races) turned by hudObjAngle, and with the HUD's coins shown and the race on shows it
 * with the count found (NumCoinsFound) beside it, in white with a black shadow; else hides it. Returns 1. */
s32 animCoins(void *blit) {
    Pos pos;
    char buf[8];
    s16 i;
    s16 j;

    HAL_SetTextDepth(0.0f);
    if (gNumWindows == 1) {
        HAL_SetFont(10);
    } else {
        HAL_SetFont(12);
    }
    TB_SetJust(kTB_JustLeft, kTB_JustCenter);
    for (i = 0; i < gNumWindows; i++) {
        for (j = 0; j < 2; j++) {
            if (gPlayerCoin[i].slot[j].id == -1) {
                char *names[2] = { coin_gold_name, coin_silver_name };
                gPlayerCoin[i].slot[j].id = ZOID_AddObject(MBOX_FindObject_WORK(names[j], 0, gNumModels - 1, 1), gPlayerCoin[i].slot[j].pos, -1, 0);
                MBOX_HideObject(gPlayerCoin[i].slot[j].id, 1, 15);
                ObjInstanceList[gPlayerCoin[i].slot[j].id].scale = 10.0f;
            }
            ScaleUV(&gCamera[i], gPlayerCoin[i].slot[j].pos,
                          (gNumPlayers == 1) ? 0.03f : (gNumPlayers == 2) ? 0.03f : 0.04f);
            YawUV(hudObjAngle, gPlayerCoin[i].slot[j].pos);
            if (gRace == kStunt) {
                pos.x = coinStuntPos[gNumWindows - 1][i][0];
                pos.y = coinStuntPos[gNumWindows - 1][i][1] + j * ((gNumWindows == 1) ? 15 : (gNumWindows == 2) ? 12 : 8);
            } else {
                pos.x = coinPos[gNumWindows - 1][i][0];
                pos.y = coinPos[gNumWindows - 1][i][1] + j * ((gNumWindows == 1) ? 15 : (gNumWindows == 2) ? 12 : 8);
            }
            ZOID_WindowTo3D(i, &pos, &gCamera[i], 2.0f, gPlayerCoin[i].slot[j].obj);
            if (gOptions[kOptCoins] && PC_CAR_FINISHED(i) != 1) {
                s16 x;
                s16 y;
                f32 xoffset, yoffset;

                MBOX_ShowObject(gPlayerCoin[i].slot[j].id, 0, hud_whichCam[i]);
                if (gRace == kStunt) {
                    x = coinStuntPos[gNumWindows - 1][i][0] + ((gNumWindows == 1) ? 12 : 8);
                    y = coinStuntPos[gNumWindows - 1][i][1] + j * ((gNumWindows == 1) ? 15 : (gNumWindows == 2) ? 12 : 8);
                } else {
                    x = coinPos[gNumWindows - 1][i][0] + ((gNumWindows == 1) ? 12 : 8);
                    y = coinPos[gNumWindows - 1][i][1] + j * ((gNumWindows == 1) ? 15 : (gNumWindows == 2) ? 12 : 8);
                }
                TB_Sprintf(buf, coin_count_format, NumCoinsFound(i, j + 1));
                TB_SetTextColor(kTB_BlackText);
                TB_DrawText(x + 1, y + 1, buf);
                TB_SetTextColor(kTB_WhiteText);
                TB_DrawText(x, y, buf);
            } else {
                MBOX_HideObject(gPlayerCoin[i].slot[j].id, 1, 15);
            }
        }
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
