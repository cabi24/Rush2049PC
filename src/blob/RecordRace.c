/* RecordRace: record a race (MAT3 gamerecords.c). */
#include "game/game.h"

#define LAP_SCORES 0              /* tRaceStats.scores[0]: the best laps */
#define AVERAGE_SCORES 1          /* scores[1]: the best average laps */


/* Adds each player's race to its profile's stats for the track (the backward ones when gBackwards) and to the
 * totals: every lap into the five best laps and the race's average lap into the five best averages (in the totals
 * with the owner, a profile with a pak file, and this race's entries in gNewTotalScore); for a finisher a finished
 * race, a first, second or third place (when enough cars ran), the death mode attempt and completion; the crashes,
 * the laps and their time, the distance; then writes the profile's stats. A player without a profile uses its "just
 * play" one; a profile whose file isn't read stops it all. */
void RecordRace(void)
{
    s32 course;
    s32 saveTrack;
    s32 player;
    s32 phase;
    tCarData *car;
    s32 lap;
    s32 rank;
    s32 shift;
    float mean;
    tRaceStats *stats;

    course = gTrackNum;
    saveTrack = gTrackNum;
    if (gBackwards) {
        course += NUM_RACE_TRACKS;
        saveTrack += TRACK_BACKWARD_FIRST;
    }
    for (player = 0; player < gNumPlayers; player++) {
        if (gPlayers[player].save == 0) {
            gPlayers[player].save = &gJustPlayers[gPlayers[player].controller];
        }
        if ((*gPlayers[player].save)->datah == 0) {
            return;
        }
        for (phase = 0; phase < 2; phase++) {
            if (phase == 0) {
                stats = &(*(*gPlayers[player].save)->datah)->raceStats[course];
            } else {
                stats = &gTotalRaceStats[course];
            }
            car = &pc_cars[gPlayers[player].slot];
            for (lap = 0; lap < gLap[player]; lap++) {
                for (rank = 0; rank < 5; rank++) {
                    if (stats->scores[LAP_SCORES][rank] == 0.0f ||
                        gLapTimes[player][lap] < stats->scores[LAP_SCORES][rank]) {
                        for (shift = 4; shift > rank; shift--) {
                            stats->scores[LAP_SCORES][shift] = stats->scores[LAP_SCORES][shift - 1];
                            if (phase == 1) {
                                gNewTotalScore[LAP_SCORES][shift] = gNewTotalScore[LAP_SCORES][shift - 1];
                                gTotalNames[course][LAP_SCORES][shift] = gTotalNames[course][LAP_SCORES][shift - 1];
                            }
                        }
                        stats->scores[LAP_SCORES][rank] = gLapTimes[player][lap];
                        if (phase == 1) {
                            gNewTotalScore[LAP_SCORES][rank] = player;
                            if ((*gPlayers[player].save)->fileh) {
                                gTotalNames[course][LAP_SCORES][rank].profile = gPlayers[player].save;
                            }
                        }
                        break;
                    }
                }
            }
            if (gLap[player] > 0) {
                mean = 0;
                for (shift = 0; shift < gLap[player]; shift++) {
                    mean += gLapTimes[player][shift];
                }
                mean /= (u32)gLap[player];
                for (rank = 0; rank < 5; rank++) {
                    if (stats->scores[AVERAGE_SCORES][rank] == 0.0f || mean < stats->scores[AVERAGE_SCORES][rank]) {
                        for (shift = 4; shift > rank; shift--) {
                            stats->scores[AVERAGE_SCORES][shift] = stats->scores[AVERAGE_SCORES][shift - 1];
                            if (phase == 1) {
                                gNewTotalScore[AVERAGE_SCORES][shift] = gNewTotalScore[AVERAGE_SCORES][shift - 1];
                                gTotalNames[course][AVERAGE_SCORES][shift] = gTotalNames[course][AVERAGE_SCORES][shift - 1];
                            }
                        }
                        stats->scores[AVERAGE_SCORES][rank] = mean;
                        if (phase == 1) {
                            gNewTotalScore[AVERAGE_SCORES][shift] = player;
                            if ((*gPlayers[player].save)->fileh) {
                                gTotalNames[course][AVERAGE_SCORES][rank].profile = gPlayers[player].save;
                            }
                        }
                        break;
                    }
                }
            }
            if (car->place_locked) {
                stats->totalRaces++;
                if (car->place == 0 && gNumActiveCars >= 2) {
                    stats->total1st++;
                } else if (car->place == 1 && gNumActiveCars >= 3) {
                    stats->total2nd++;
                } else if (car->place == 2 && gNumActiveCars >= 4) {
                    stats->total3rd++;
                }
                if (gDeathMode) {
                    stats->totalDeathAttempted++;
                    if (gNumLaps == gLap[player]) {
                        stats->totalDeathCompleted++;
                    }
                }
            }
            stats->totalCrashes += gPlayers[player].numCrashes;
            stats->totalLaps += gLap[player];
            for (shift = 0; shift < gLap[player]; shift++) {
                stats->totalTime += gLapTimes[player][shift];
            }
            stats->totalMiles += car->mileage / FEET_PER_TENTH_MILE;
        }
        SavePlayerStats(gPlayers[player].save, saveTrack);
    }
}
