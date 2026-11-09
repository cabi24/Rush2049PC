#include "game/game.h"

VIEW(int, gGameState);
#define gGameState gGameState_view

void RecordLap(int player, f32 time)
{
    int i;

    if (!(gGameState & 8) && gRace != 1 && player < gNumPlayers) {
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
