/* AddPlayerScores: merge a profile's best times into the high score tables (MAT3 gamerecords.c). */
#include "game/game.h"

VIEW(tProfile **[12][3][5], gTotalNames);
#define gTotalNames gTotalNames_view

/* Merges the best times of the profile's file (each track's three lists of five) into the tables over all profiles
 * (gTotalRaceStats' scores, the owners in gTotalNames): each time above 0 that beats an entry is put in its place,
 * pushing the rest down and the last off. Nothing for a profile whose file is not read. */
void AddPlayerScores(tProfile **profile)
{
    tSaveData *data;
    s32 i;
    s32 j;
    s32 k;
    s32 m;
    s32 l;
    f32 *src;
    f32 *times;
    tProfile ***owners;

    if ((*profile)->datah == 0) {
        return;
    }
    data = *(*profile)->datah;
    for (i = 0; i < 12; i++) {
        for (j = 0; j < 3; j++) {
            src = data->raceStats[i].scores[j];
            l = 0;
            times = gTotalRaceStats[i].scores[j];
            owners = gTotalNames[i][j];
            for (k = 0; k < 5; k++) {
                if (0.0f < src[l]) {
                    if (0.0f == times[k] || src[l] < times[k]) {
                        for (m = 4; m > k; m--) {
                            times[m] = times[m - 1];
                            owners[m] = owners[m - 1];
                        }
                        times[k] = src[l];
                        owners[m] = profile;
                        l++;
                    }
                }
            }
        }
    }
}
