/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * animResults (0x80105EA8, 2508 B) = GC hud.c animResults (MAT3 decomp/src/GameSource/src/hud.c animResults,
 * GC locals playerIdx, x, y, str[128], gNumPlayersFinished, gAPlayerFinished; block locals min/sec/thou and
 * str1[32]/str2[32]/place as in GC). End-of-race result panes and texts. The N64 build creates the panes here
 * (GC moved them to CheckResults): header pane resultPane[4] (resultPane[4]) and one pane per finished player
 * (InitPane, ChangePaneColor = tournament_unlock_check with playerPaneColors hud_playerPaneColors),
 * each sized from TB_GetFontLineHeight in a block-scoped `h` (GC CheckResults style).
 * GC-derived names: model D_8014A250 (2056 B), gGameCar gGameCar (952 B), gNumLaps
 * gNumLaps, gDeathMode gDeathMode, gRankedPlayer gRankedPlayer, ties ties, gLap gLap, gStuntPlayer
 * (totalPoints), wrongWayPos wrongWayPos, gPlayers gPlayers, gText gText, TB_Sprintf TB_Sprintf,
 * TB_DrawTextScrollBox TB_DrawTextScrollBox, HAL_SetTextDepth HAL_SetTextDepth, TB_SetTextColor TB_SetTextColor,
 * HAL_SetFont = HAL_SetFontInternal (s2 IPA parameter).
 * History: draft 562/627 -> 150/627 (playerIdx << 4, h, time converted once) -> EQUAL.
 * levers (fresh MAT3 typed port of the tail):
 *   - ar_font_set DEFINED AFTER animResults (prototype first): as1's .loc tie puts the TB_SetJust stores before
 * the osRecvMesg arguments (anim_no_rumble_text lever) -> ops 15 -> 9;
 *   - GC block scoping: the two pane heights are block locals `h` (header block, player block), str1/str2/place
 *     and the else-block min/sec/thou are default-case block locals (retail str at T-140, str1 200, str2 168);
 *   - time blocks: `min = (u32)t; min = min / 60 % 100; sec = (u32)t % 60; thou = (u32)(1000.0f * t);
 *     thou %= 1000;` (the conversion CSE copy is retail's `move v0,t0`; GC's separate `thou %= 1000` statement
 *     gives the round-mode constant and mfhi/ctc1 order);
 *   - GC's two extra varargs on the laps-text TB_Sprintf (`model[..].laps, gNumLaps`, the values the compare just
 *     loaded: retail keeps them in a2/a3) -> 65 rows -> EQUAL.
 * Shaping (disclosed): inlined non-static helpers ar_font_set / ar_SetFont (void wrapper)
 * ar_SetJust, not on keep; `playerIdx << 4` for the pane colour argument.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct OSMesgQueue OSMesgQueue;
typedef struct ArIdx { u16 h[64]; } ArIdx;
typedef struct TextState { void *word0; char **fixed; void *word8; ArIdx *indices; char **strings; } TextState;
typedef struct Model { u8 pad0[1990]; s16 car; u8 pad1992[4]; s8 f1996; u8 pad1997[27]; s8 lap; u8 pad2025[31]; } Model; /* 2056 B */
typedef struct GameCar { u8 pad0[238]; s8 place; s8 f239; f32 time; u8 pad244[0x110 - 0xF4]; struct pc_visual visuals[21]; u8 pad244_308[0x380 - 0x308]; void *input; u8 pad244_384[0x3A3 - 0x384]; s8 f931; u8 pad932[20]; } GameCar; /* 952 B */
typedef struct PlayerRow { u8 vehicle; u8 pak; u8 other[70]; char **name; } PlayerRow;   /* 76 B */
/* *name is the player's profile record (save.c's SaveSlot): 64 bytes on the PC, its name at +0x20 and the
 * rumble byte at +0x37 (PROFILE_OFS, pc_prelude.h; docs/port/POINTERS.md 9.10). */
typedef struct Pos { s32 x, y; } Pos;
/* A player's stunt scores (MAT3 tStuntPlayer, 120 bytes; animStuntPoints.c). */
typedef struct tStuntPlayer {
    s16 show, crash;
    f32 time;
    s32 maxStuntPoints, points, pointsHud, totalPoints, numstunts, lastStuntType;
    s16 stunts[10], stuntsHud[10], totalStunts[10], unused;
    f32 wingTime;
    s8 wingBonus, wingBonusHud, doneBonus, doneBonusHud;
    s32 bonus, bonusHud, showBonus, done;
} tStuntPlayer;

extern OSMesgQueue text_lock_queue;
extern TextState gText;
extern s16 gNumWindows;
extern s16 gNumLaps;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern PlayerRow gPlayers[];                   /* gPlayers */
extern Pos wrongWayPos[][4];                      /* wrongWayPos */
extern s32 gRace;
extern s8 gDeathMode;
extern s8 gRankedPlayer;
extern s32 ties;
extern u8 gLap[];                          /* gLap[p] */
extern tStuntPlayer gStuntPlayer[4];
extern s32 *resultPane[];                        /* result panes [p], [4] = the header pane */
/* PC: playerPaneColors, the two R,G,B,A colours ChangePaneColor reads as bytes. The PC loader fills initialized data
 * by its declarations, and as words their bytes would come out reversed (docs/port/ENDIAN.md). */
typedef struct PaneColors { u8 x[4], y[4]; } PaneColors;
extern PaneColors hud_playerPaneColors[];
extern s32 gJust[2];                        /* TB_SetJust pair */
extern s32 osRecvMesg(OSMesgQueue *, void **, s32), osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32);
extern s16 TB_GetFontLineHeight(void);
extern void TB_SetTextColor(s32);
extern s32 TB_Sprintf(char *, char *, ...);
extern void HAL_SetTextDepth(f32);
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
extern s32 *InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);
extern void TB_DrawTextScrollBox(s16, s16, s16, s16, s16, s16, char *);
extern void ChangePaneColor(void *, void *, void *);

s32 ar_font_set(s32 font);

void ar_SetJust(s32 h, s32 v)
{
    gJust[0] = h;
    gJust[1] = v;
}

void ar_SetFont(s32 font)
{
    ar_font_set(font);
}

s32 animResults(s32 arg0)
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
    ar_SetJust(1, 1);
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
    ar_SetJust(0, 3);
    return 1;
}

s32 ar_font_set(s32 font)
{
    s32 old;
    osRecvMesg(&text_lock_queue, 0, 1);
    old = HAL_SetFontInternal(font);
    osSendMesg(&text_lock_queue, 0, 0);
    return old;
}

