/* RecordGhost: record a ghost race (MAT3 gamerecords.c). */
#include "game/game.h"

#define GHOST_SCORES 2            /* tRaceStats.scores[2]: the best ghost race times */


/* A player's input record as this reads it: the car, which "just play" profile, the crashes, the profile. */
typedef struct PlayerPad {     /* 0x4C */
    u8 car;                    /* 0x00 */
    u8 slot;                   /* 0x01 */
    u8 pad02[0x40 - 0x02];
    u16 numCrashes;            /* 0x40 */
    u8 pad42[0x48 - 0x42];
    tProfile **profile;        /* 0x48 */
} PlayerPad;

/* A car's record as this reads it: the race time and the distance driven (feet). */
typedef struct CarData {       /* 0x3B8 */
    u8 pad00[0xEF];
    s8 finished;               /* 0xEF */
    f32 time;                  /* 0xF0 */
    u8 padF4[0x108 - 0xF4];
    f32 dist;                  /* 0x108 */
    u8 pad10C[0x110 - 0x10C]; struct pc_visual visuals[21]; u8 pad10C_308[0x380 - 0x308]; void *input; u8 pad10C_384[0x3B8 - 0x384];
} CarData;

extern PlayerPad gPlayers[];

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
    CarData *car;
    f32 time;

    track = gTrackNum;
    saveTrack = gTrackNum;
    if (gBackwards) {
        track = gTrackNum + NUM_RACE_TRACKS;
        saveTrack += TRACK_BACKWARD_FIRST;
    }
    for (i = 0; i < gNumPlayers; i++) {
        if (gPlayers[i].profile == 0)
            gPlayers[i].profile = &gJustPlayers[gPlayers[i].slot];
        if ((*gPlayers[i].profile)->datah == 0)
            return;
        for (j = 0; j < 2; j++) {
            if (j == 0)
                r = &(*(*gPlayers[i].profile)->datah)->raceStats[track];
            else
                r = &gTotalRaceStats[track];
            car = PC_CAR(gPlayers[i].car);
            if (PC_CAR_FINISHED(gPlayers[i].car)) {
                time = car->time;
                for (k = 0; k < 5; k++) {
                    if (r->scores[GHOST_SCORES][k] == 0.0f || time < r->scores[GHOST_SCORES][k]) {
                        for (n = 4; n > k; n--) {
                            r->scores[GHOST_SCORES][n] = r->scores[GHOST_SCORES][n - 1];
                            if (j == 1) {
                                gNewTotalScore[n + 10] = gNewTotalScore[n + 9];
                                gTotalNames[track].who[n] = gTotalNames[track].who[n - 1];
                            }
                        }
                        r->scores[GHOST_SCORES][k] = time;
                        if (j == 1) {
                            gNewTotalScore[n + 10] = i;
                            if ((*gPlayers[i].profile)->fileh)
                                gTotalNames[track].who[k] = gPlayers[i].profile;
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
            r->totalMiles += car->dist / FEET_PER_TENTH_MILE;
        }
        SavePlayerStats(gPlayers[i].profile, saveTrack);
    }
}
