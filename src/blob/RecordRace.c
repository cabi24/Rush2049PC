/* RecordRace: record a race (MAT3 gamerecords.c). */
#include "game/game.h"

#define LAP_SCORES 0              /* tRaceStats.scores[0]: the best laps */
#define AVERAGE_SCORES 1          /* scores[1]: the best average laps */

/* A player's input record as this reads it: the car, which "just play" profile, the crashes, the profile. */
typedef struct PlayerPad {
    u8 car_index;
    u8 resource_index;
    u8 unknown02[62];
    u16 numCrashes;
    u8 unknown66[6];
    tProfile **profile;
} PlayerPad;
/* A car's record as this reads it: its place, whether it finished, the distance driven (feet). */
typedef struct CarRecord {
    u8 unknown00[238];
    s8 place;
    s8 finished;
    u8 unknown240[24];
    float distance;
    u8 unknown268[0x110 - 0x10C]; struct pc_visual visuals[21]; u8 unknown268_308[0x380 - 0x308]; void *input; u8 unknown268_384[0x3B8 - 0x384];
} CarRecord;

extern PlayerPad gPlayers[];

/* The high score tables' owners for a track (gTotalNames): 15 slots, the laps', the averages' and the ghost times'. */
VIEW(tProfile **[][15], gTotalNames);
#define gTotalNames gTotalNames_view

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
    CarRecord *car;
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
        if (gPlayers[player].profile == 0) {
            gPlayers[player].profile = &gJustPlayers[gPlayers[player].resource_index];
        }
        if ((*gPlayers[player].profile)->datah == 0) {
            return;
        }
        for (phase = 0; phase < 2; phase++) {
            if (phase == 0) {
                stats = &(*(*gPlayers[player].profile)->datah)->raceStats[course];
            } else {
                stats = &gTotalRaceStats[course];
            }
            car = PC_CAR(gPlayers[player].car_index);
            for (lap = 0; lap < gLap[player]; lap++) {
                for (rank = 0; rank < 5; rank++) {
                    if (stats->scores[LAP_SCORES][rank] == 0.0f ||
                        gLapTimes[player][lap] < stats->scores[LAP_SCORES][rank]) {
                        for (shift = 4; shift > rank; shift--) {
                            stats->scores[LAP_SCORES][shift] = stats->scores[LAP_SCORES][shift - 1];
                            if (phase == 1) {
                                gNewTotalScore[shift] = gNewTotalScore[shift - 1];
                                gTotalNames[course][shift] = gTotalNames[course][shift - 1];
                            }
                        }
                        stats->scores[LAP_SCORES][rank] = gLapTimes[player][lap];
                        if (phase == 1) {
                            gNewTotalScore[rank] = player;
                            if ((*gPlayers[player].profile)->fileh) {
                                gTotalNames[course][rank] = gPlayers[player].profile;
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
                                gNewTotalScore[shift + 5] = gNewTotalScore[shift + 4];
                                gTotalNames[course][shift + 5] = gTotalNames[course][shift + 4];
                            }
                        }
                        stats->scores[AVERAGE_SCORES][rank] = mean;
                        if (phase == 1) {
                            gNewTotalScore[shift + 5] = player;
                            if ((*gPlayers[player].profile)->fileh) {
                                gTotalNames[course][rank + 5] = gPlayers[player].profile;
                            }
                        }
                        break;
                    }
                }
            }
            if (car->finished) {
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
            stats->totalMiles += car->distance / FEET_PER_TENTH_MILE;
        }
        SavePlayerStats(gPlayers[player].profile, saveTrack);
    }
}
