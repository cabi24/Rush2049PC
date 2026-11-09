/* PrintScores: the high-score screen's table, ready to draw (MAT3 highscorescr.c PrintScores). */
#include "game/game.h"

/* A profile (MAT3 tPlayerProfile) in its PC layout (pc_prelude.h PROFILE_OFS). */
typedef struct Player {
    void *next, *prev; void *fileh; u32 id[2];
    char name[13];                      /* 0x20 (N64 0x14) */
} Player;
typedef struct RaceStats {              /* GC struct __anon_2dfd (0x60) */
    u32 checksum;
    f32 scores[3][5];                   /* +0x04 */
    u8 rest[0x60 - 0x40];
} RaceStats;

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

#define TEXT_SCORE_TITLES 27        /* gText.tab[]: the tables' titles (gTable 0-2) */
#define NO_HOLDER ((Player **)-1)   /* gTotalNames[]: nobody yet (a made-up holder and time) */
#define TEMP_NAMES 20               /* a holder up to this is a temporary name's index (gTempNames), else a profile */

extern void sprintf(char *, char *, ...);

extern void ChangePaneSize(void *, s32, s32, s32, s32);

/* Fill gScoreNames and gScoreTimes with the five rows of table gTable (0 and 1 a race's two tables, 2 the ghost
 * race's) of this track and direction (gThisTrack: the track, plus 6 backwards). Rows nobody holds yet get a name from
 * gDefaultNames and a time a little worse than the one before, from the track's default time; the made-up
 * names and times come from a generator seeded by the default time (the last real time after a real row), so they
 * are the same every time. A table's title pane is resized to its title. */
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
        if (*p == NO_HOLDER) {
            gScoreNames[i] = gDefaultNames[seeded_rand(&seed) % 55];
            defTime += (f32)(seeded_rand(&seed) % 1000) / 1000.0f;
            sprintf(gScoreTimes[i], default_score_time_format, (u32)defTime / 60, (u32)defTime % 60,
                    (u32)(defTime * 1000.0f) % 1000);
        } else {
            if ((uintptr_t)*p <= TEMP_NAMES) {
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
        width = HAL_GetTextWidth(gText.taba[gText.tab[TEXT_SCORE_TITLES] + gTable], -1);
        height = TB_GetFontLineHeight();
        ChangePaneSize(scorePanes, 160 - width / 2 - 8, 6, width / 2 + 168, height + 14);
    }
}
