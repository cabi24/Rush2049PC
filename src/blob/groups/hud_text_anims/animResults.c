/* The HUD's race results (MAT3 hud.c animResults): an animation function, run each frame at the end of a race,
 * that draws each finished player's result in their window and, with two or more windows, the winner. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;
typedef struct ArIdx { u16 h[64]; } ArIdx;
typedef struct TextState { void *word0; char **fixed; void *word8; ArIdx *indices; char **strings; } TextState;

typedef struct PlayerRow { u8 vehicle; u8 pak; u8 other[70]; char **name; } PlayerRow;   /* 76 B */
/* *name is the player's profile record (save.c's SaveSlot): 64 bytes on the PC, its name at +0x20 and the
 * rumble byte at +0x37 (PROFILE_OFS, pc_prelude.h; docs/port/POINTERS.md 9.10). */
typedef struct Pos { s32 x, y; } Pos;

VIEW(OSMesgQueue, text_lock_queue);
#define text_lock_queue text_lock_queue_view
VIEW(TextState, gText);
#define gText gText_view

/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern PlayerRow gPlayers[];                   /* gPlayers */
VIEW(Pos[][4], wrongWayPos);
#define wrongWayPos wrongWayPos_view

VIEW(s8, gRankedPlayer);
#define gRankedPlayer gRankedPlayer_view

VIEW(tStuntPlayer[4], gStuntPlayer);
#define gStuntPlayer gStuntPlayer_view

extern s32 osRecvMesg(OSMesgQueue *, void **, s32), osSendMesg(OSMesgQueue *, void *, s32);

/* HAL_SetFont without its result. */
void ar_SetFont(s32 font)
{
    ar_font_set(font);
}

/* Draws, for each window whose player has finished (in death mode, done every lap), a pane in the player's colour
 * with their name and result: stunt points (gRace 4), obstacle time or "failed" (5), battle points (6), else
 * their place and time or laps. With two or more windows all finished, a middle pane names the winner (or the
 * drone, or a tie of n players): resultPane[4], the fifth of resultPane's panes (MAT3 hud.c resultPane[5]). */
s32 animResults(s32 unused)
{
    s32 playerIdx;
    s32 x;
    s32 y;
    char str[128];
    s32 numFinished;
    s32 aFinished;

    numFinished = 0;
    aFinished = 0;
    for (playerIdx = 0; playerIdx < gNumWindows; playerIdx++) {
        if (gNumLaps == PC_MODEL_LAP(playerIdx)) aFinished = 1;
        if (PC_CAR_FINISHED(PC_MODEL_SLOT(playerIdx)) == 1) numFinished++;
    }
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 1);
    ar_SetFont(10);
    if (gNumWindows >= 2 && numFinished == gNumWindows && (!gDeathMode || (gDeathMode && aFinished))) {
        if (resultPane[4] == 0) {
            s32 h = TB_GetFontLineHeight() / 2;
            resultPane[4] = InitPane(91, 106 - h, 229, h + 114, 192, 0, 0, 1);
        }
        TB_SetTextColor(22);
        if (PC_MODEL_DRONE_TYPE(PC_MODEL_SLOT(gRankedPlayer)) == 1) {
            TB_Sprintf(str, gText.fixed[200]);
        } else if (ties) {
            TB_Sprintf(str, gText.fixed[201], ties + 1);
        } else {
            TB_Sprintf(str, gText.fixed[199], *gPlayers[gRankedPlayer].name + PROFILE_OFS(20));
        }
        TB_DrawTextScrollBox(160, 110, 160, 110, -1, 0, str);
    }
    TB_SetTextColor(1);
    for (playerIdx = 0; playerIdx < gNumWindows; playerIdx++) {
        if (gDeathMode && gNumLaps != PC_MODEL_LAP(playerIdx)) continue;
        if (PC_CAR_FINISHED(PC_MODEL_SLOT(playerIdx)) != 1) continue;
        x = wrongWayPos[gNumWindows - 1][playerIdx].x;
        y = wrongWayPos[gNumWindows - 1][playerIdx].y;
        if (gNumWindows == 4 && playerIdx >= 2) y += 12;
        if (resultPane[playerIdx] == 0) {
            s32 h = TB_GetFontLineHeight() * 3 / 2;
            resultPane[playerIdx] = InitPane(x - 69, y - h - 4, x + 69, h + y + 4, 192, 0, playerIdx << 4, 1);
            ChangePaneColor(resultPane[playerIdx], &hud_playerPaneColors[playerIdx].x, &hud_playerPaneColors[playerIdx].y);
        }
        if (gRace == 4) {
            TB_Sprintf(str, "%s\n%d %s", *gPlayers[playerIdx].name + PROFILE_OFS(20), gStuntPlayer[playerIdx].totalPoints,
                         gText.fixed[130]);
        } else if (gRace == 5) {
            if (gLap[playerIdx] != 1) {
                TB_Sprintf(str, "%s\n%s", *gPlayers[playerIdx].name + PROFILE_OFS(20), gText.fixed[214]);
            } else {
                u32 min;
                u32 sec;
                u32 thou;

                min = (u32)PC_CAR_TIME(gPlayers[playerIdx].vehicle);
                min = min / 60 % 100;
                sec = (u32)PC_CAR_TIME(gPlayers[playerIdx].vehicle) % 60;
                thou = (u32)(1000.0f * PC_CAR_TIME(gPlayers[playerIdx].vehicle));
                thou %= 1000;
                TB_Sprintf(str, "%s\n%s %d:%02d.%03d", *gPlayers[playerIdx].name + PROFILE_OFS(20), gText.fixed[208],
                             min, sec, thou);
            }
        } else if (gRace == 6) {
            TB_Sprintf(str, "%s\n%d %s", *gPlayers[playerIdx].name + PROFILE_OFS(20), PC_CAR_POINTS(playerIdx),
                         gText.fixed[130]);
        } else {
            char str1[32], str2[32];
            s32 place = PC_CAR_PLACE(gPlayers[playerIdx].vehicle);

            TB_Sprintf(str1, gText.fixed[198], place + 1,
                         gText.strings[gText.indices->h[55] + place]);
            if (gNumLaps != PC_MODEL_LAP(gPlayers[playerIdx].vehicle)) {
                TB_Sprintf(str2, gText.fixed[207], PC_MODEL_LAP(gPlayers[playerIdx].vehicle), gNumLaps);
            } else {
                u32 min;
                u32 sec;
                u32 thou;

                min = (u32)PC_CAR_TIME(gPlayers[playerIdx].vehicle);
                min = min / 60 % 100;
                sec = (u32)PC_CAR_TIME(gPlayers[playerIdx].vehicle) % 60;
                thou = (u32)(1000.0f * PC_CAR_TIME(gPlayers[playerIdx].vehicle));
                thou %= 1000;
                TB_Sprintf(str2, "%s %d:%02d.%03d", gText.fixed[208], min, sec, thou);
            }
            TB_Sprintf(str, "%s\n%s\n%s", *gPlayers[playerIdx].name + PROFILE_OFS(20), str1, str2);
        }
        TB_DrawTextScrollBox(x, y, 160, 110, -1, 0, str);
    }
    HAL_SetTextDepth(-1.0f);
    TB_SetJust(0, 3);
    return 1;
}

/* HAL_SetFont: the text font, under the text lock; returns the one before. */
s32 ar_font_set(s32 font)
{
    s32 old;
    osRecvMesg(&text_lock_queue, 0, 1);
    old = HAL_SetFontInternal(font);
    osSendMesg(&text_lock_queue, 0, 0);
    return old;
}

