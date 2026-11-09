/* ResetHighScores: forget the race's laps and new high scores (MAT3 gamerecords.c). */
#include "game/game.h"

VIEW(signed char[3][5], gNewTotalScore);
#define gNewTotalScore gNewTotalScore_view

/* At a race's start: no laps done and no best lap (5999.999 s) for each player, and no new entry in the high score
 * lists (gNewTotalScore). */
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
