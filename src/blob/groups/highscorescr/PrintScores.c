/* PrintScores: the high-score screen's table, ready to draw (MAT3 highscorescr.c PrintScores). */
#include "game/game.h"

#define NO_HOLDER (-1)              /* gTotalNames[]: nobody yet (a made-up holder and time) */
#define TEMP_NAMES 20               /* a holder up to this is a typed name (gTempNames), else a profile */

extern int sprintf(char *, const char *, ...);

/* Fill gScoreNames and gScoreTimes with the five rows of table gTable (0 and 1 a race's two tables, 2 the ghost
 * race's) of this track and direction (gThisTrack: the track, plus 6 backwards). Rows nobody holds yet get a name from
 * gDefaultNames and a time a little worse than the one before, from the track's default time; the made-up
 * names and times come from a generator seeded by the default time (the last real time after a real row), so they
 * are the same every time. A table's title pane is resized to its title. */
void PrintScores(void)
{
    s32 i;
    f32 *s;
    tScoreOwner *p;
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
        if (p->name == NO_HOLDER) {
            gScoreNames[i] = gDefaultNames[seeded_rand(&seed) % 55];
            defTime += (f32)(seeded_rand(&seed) % 1000) / 1000.0f;
            sprintf(gScoreTimes[i], default_score_time_format, (u32)defTime / 60, (u32)defTime % 60,
                    (u32)(defTime * 1000.0f) % 1000);
        } else {
            if ((uintptr_t)p->name <= TEMP_NAMES) {
                gScoreNames[i] = gTempNames[p->name];
            } else {
                gScoreNames[i] = (*p->profile)->name;
            }
            defTime = *s;
            seed = defTime;
            sprintf(gScoreTimes[i], score_time_format, (u32)defTime / 60, (u32)defTime % 60, (u32)(defTime * 1000.0f) % 1000);
        }
        s++;
        p++;
    }
    if (scorePanes[0] != 0) {
        HAL_SetFont(13);
        width = HAL_GetTextWidth(gText.taba[gText.tab[TEXT_SCORE_TITLES] + gTable], -1);
        height = TB_GetFontLineHeight();
        ChangePaneSize(scorePanes[0], 160 - width / 2 - 8, 6, width / 2 + 168, height + 14);
    }
}
