/* RecordGhost: record a ghost race (MAT3 gamerecords.c). */
#include "game/game.h"

#define GHOST_SCORES 2            /* tRaceStats.scores[2]: the best ghost race times */

/* Adds each player's ghost race to its profile's stats for the track (the backward ones when gBackwards) and to the
 * totals: a finisher's time into the five best ghost race times (in the totals with its owner, a profile with a pak
 * file, and this race's entry in gNewTotalScore), a race, a ghost race, the crashes, the laps and their time, the
 * distance; then writes the profile's stats. A player without a profile uses its "just play" one; a profile whose
 * file isn't read stops it all. */
void RecordGhost(void)
{
    s32 track, saveTrack;
    s32 i, j, k, n;
    tRaceStats *r;
    tCarData *car;
    f32 time;

    track = gTrackNum;
    saveTrack = gTrackNum;
    if (gBackwards) {
        track = gTrackNum + NUM_RACE_TRACKS;
        saveTrack += TRACK_BACKWARD_FIRST;
    }
    for (i = 0; i < gNumPlayers; i++) {
        if (gPlayers[i].save == 0)
            gPlayers[i].save = &gJustPlayers[gPlayers[i].controller];
        if ((*gPlayers[i].save)->datah == 0)
            return;
        for (j = 0; j < 2; j++) {
            if (j == 0)
                r = &(*(*gPlayers[i].save)->datah)->raceStats[track];
            else
                r = &gTotalRaceStats[track];
            car = &pc_cars[gPlayers[i].slot];
            if (pc_cars[gPlayers[i].slot].place_locked) {
                time = car->score;
                for (k = 0; k < 5; k++) {
                    if (r->scores[GHOST_SCORES][k] == 0.0f || time < r->scores[GHOST_SCORES][k]) {
                        for (n = 4; n > k; n--) {
                            r->scores[GHOST_SCORES][n] = r->scores[GHOST_SCORES][n - 1];
                            if (j == 1) {
                                gNewTotalScore[GHOST_SCORES][n] = gNewTotalScore[GHOST_SCORES][n - 1];
                                gTotalNames[track][GHOST_SCORES][n] = gTotalNames[track][GHOST_SCORES][n - 1];
                            }
                        }
                        r->scores[GHOST_SCORES][k] = time;
                        if (j == 1) {
                            gNewTotalScore[GHOST_SCORES][n] = i;
                            if ((*gPlayers[i].save)->fileh)
                                gTotalNames[track][GHOST_SCORES][k].profile = gPlayers[i].save;
                        }
                        break;
                    }
                }
            }
            r->totalRaces++;
            r->totalGhosts++;
            r->totalCrashes += gPlayers[i].numCrashes;
            r->totalLaps += gLap[i];
            for (n = 0; n < gLap[i]; n++)
                r->totalTime += gLapTimes[i][n];
            r->totalMiles += car->mileage / FEET_PER_TENTH_MILE;
        }
        SavePlayerStats(gPlayers[i].save, saveTrack);
    }
}
