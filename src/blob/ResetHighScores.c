#include "game/game.h"

VIEW(signed char[3][5], gNewTotalScore);
#define gNewTotalScore gNewTotalScore_view

void ResetHighScores(void)
{
    int i, j;

    for (i = 0; i < gNumPlayers; i++) {
        gLap[i] = 0;
        gBestLapTime[i] = 5999.999f;
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 5; j++) {
            gNewTotalScore[i][j] = -1;
        }
    }
}
