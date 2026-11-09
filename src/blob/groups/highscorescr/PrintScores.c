#include "game/game.h"

typedef struct Player {                 /* GC struct __anon_3bdf (0x4c) */
    void *next, *prev; void *fileh; u32 id[2];   /* the profile's host layout (pc_prelude.h) */
    char name[13];                      /* +0x14 */
} Player;
typedef struct RaceStats {              /* GC struct __anon_2dfd (0x60) */
    u32 checksum;
    f32 scores[3][5];                   /* +0x04 */
    u8 rest[0x60 - 0x40];
} RaceStats;
typedef struct TextIndices { u8 other[54]; u16 tableNames; } TextIndices;
typedef struct TextState { void *word0; char **fixed; void *word8; TextIndices *indices; char **strings; } TextState;

VIEW(Player **[][3][5], gTotalNames);
#define gTotalNames gTotalNames_view
VIEW(RaceStats[], gTotalRaceStats);
#define gTotalRaceStats gTotalRaceStats_view

VIEW(char[5][10], gScoreTimes);
#define gScoreTimes gScoreTimes_view
VIEW(char *[5], gScoreNames);
#define gScoreNames gScoreNames_view
VIEW(void *, scorePanes);
#define scorePanes scorePanes_view
VIEW(TextState, gText);
#define gText gText_view

extern void sprintf(char *, char *, ...);

extern void ChangePaneSize(void *, s32, s32, s32, s32);

void PrintScores(void)
{
    s32 i;
    f32 *s;
    Player ***p;
    f32 defTime;
    u32 seed;
    s32 width;
    s32 height;

    gThisTrack = (gBackwards ? 6 : 0) + gTrackNum;
    p = gTotalNames[gThisTrack][gTable];
    s = gTotalRaceStats[gThisTrack].scores[gTable];
    defTime = gDefStartTimes[(gBackwards ? 19 : 0) + gTrackNum];
    seed = defTime * 6969.0f;
    if (gTable == 1) {
        defTime += 1.0f + (f32)(seeded_rand(&seed) % 2000) / 1000.0f;
    }
    for (i = 0; i < 5; i++) {
        if (*p == (Player **)-1) {
            gScoreNames[i] = gDefaultNames[seeded_rand(&seed) % 55];
            defTime += (f32)(seeded_rand(&seed) % 1000) / 1000.0f;
            sprintf(gScoreTimes[i], default_score_time_format, (u32)defTime / 60, (u32)defTime % 60,
                    (u32)(defTime * 1000.0f) % 1000);
        } else {
            if ((uintptr_t)*p <= 20) {
                gScoreNames[i] = gTempNames[(intptr_t)*p];
            } else {
                gScoreNames[i] = (**p)->name;
            }
            defTime = *s;
            seed = defTime;
            sprintf(gScoreTimes[i], score_time_format, (u32)defTime / 60, (u32)defTime % 60, (u32)(defTime * 1000.0f) % 1000);
        }
        s++;
        p++;
    }
    if (scorePanes != 0) {
        HAL_SetFont(13);
        width = HAL_GetTextWidth(gText.strings[gText.indices->tableNames + gTable], -1);
        height = TB_GetFontLineHeight();
        ChangePaneSize(scorePanes, 160 - width / 2 - 8, 6, width / 2 + 168, height + 14);
    }
}
