/* RecordLap: note a lap's time (MAT3 gamerecords.c). */
#include "game/game.h"

#define kPractice 1   /* gRace (MAT3 enum kNumRaceTypes) */

VIEW(int, gGameState);
#define gGameState gGameState_view

/* A human player finished a lap at race time time: the lap's own time (time less the laps before) goes into
 * gLapTimes, and gBestLapTime if better. Not in attract mode or practice. */
void RecordLap(int player, f32 time)
{
    int i;

    if (!(gGameState & 8) && gRace != kPractice && player < gNumPlayers) {
        gLapTimes[player][gLap[player]] = time;
        for (i = 0; i < gLap[player]; i++) {
            gLapTimes[player][gLap[player]] -= gLapTimes[player][i];
        }
        if (gLapTimes[player][gLap[player]] < gBestLapTime[player]) {
            gBestLapTime[player] = gLapTimes[player][gLap[player]];
        }
        gLap[player]++;
    }
}
