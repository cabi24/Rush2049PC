/* AddPlayerStats: add a profile's totals to everyone's (MAT3 gamerecords.c). */
#include "game/game.h"

/* Adds the race, stunt, battle, obstacle and circuit totals of the profile's file to the totals over all profiles
 * (gTotalRaceStats ...): counts summed, bests kept (the highest stunt scores, the lowest obstacle time above 0, the
 * best circuit points), the coins found merged. Nothing for a profile whose file is not read. */
void AddPlayerStats(tProfile **profile) {
    tSaveData *s;
    s32 i, j, k;
    if (!(*profile)->datah) return;
    s = *(*profile)->datah;
    for (i = 0; i < 12; i++) {
        gTotalRaceStats[i].totalTime += s->raceStats[i].totalTime;
        gTotalRaceStats[i].totalLaps += s->raceStats[i].totalLaps;
        gTotalRaceStats[i].totalRaces += s->raceStats[i].totalRaces;
        gTotalRaceStats[i].total1st += s->raceStats[i].total1st;
        gTotalRaceStats[i].total2nd += s->raceStats[i].total2nd;
        gTotalRaceStats[i].total3rd += s->raceStats[i].total3rd;
        gTotalRaceStats[i].totalCrashes += s->raceStats[i].totalCrashes;
        gTotalRaceStats[i].totalDeathAttempted += s->raceStats[i].totalDeathAttempted;
        gTotalRaceStats[i].totalDeathCompleted += s->raceStats[i].totalDeathCompleted;
        gTotalRaceStats[i].totalGhosts += s->raceStats[i].totalGhosts;
        gTotalRaceStats[i].totalMiles += s->raceStats[i].totalMiles;
        gTotalRaceStats[i].coinsFound |= s->raceStats[i].coinsFound;
    }
    for (i = 0; i < 4; i++) {
        if (gTotalStuntStats[i].bestStunt < s->stuntStats[i].bestStunt) gTotalStuntStats[i].bestStunt = s->stuntStats[i].bestStunt;
        if (gTotalStuntStats[i].bestAvg < s->stuntStats[i].bestAvg) gTotalStuntStats[i].bestAvg = s->stuntStats[i].bestAvg;
        gTotalStuntStats[i].totalPoints += s->stuntStats[i].totalPoints;
        gTotalStuntStats[i].totalTime += s->stuntStats[i].totalTime;
        gTotalStuntStats[i].totalCrashes += s->stuntStats[i].totalCrashes;
        for (j = 0; j < 10; j++) gTotalStuntStats[i].totalStunts[j] += s->stuntStats[i].totalStunts[j];
        gTotalStuntStats[i].coinsFound |= s->stuntStats[i].coinsFound;
    }
    for (i = 0; i < 8; i++) {
        gTotalBattleStats[i].totalMatches += s->battleStats[i].totalMatches;
        gTotalBattleStats[i].totalWins += s->battleStats[i].totalWins;
        gTotalBattleStats[i].totalKills += s->battleStats[i].totalKills;
        gTotalBattleStats[i].totalDeaths += s->battleStats[i].totalDeaths;
    }
    for (i = 0; i != 1; i++) {
        if (s->obstacleStats[i].bestTime > 0.0f && (gTotalObstacleStats[i].bestTime == 0.0f || s->obstacleStats[i].bestTime < gTotalObstacleStats[i].bestTime)) gTotalObstacleStats[i].bestTime = s->obstacleStats[i].bestTime;
        gTotalObstacleStats[i].totalTime += s->obstacleStats[i].totalTime;
        gTotalObstacleStats[i].totalRaces += s->obstacleStats[i].totalRaces;
        gTotalObstacleStats[i].totalFinished += s->obstacleStats[i].totalFinished;
        gTotalObstacleStats[i].totalCrashes += s->obstacleStats[i].totalCrashes;
        gTotalObstacleStats[i].numDeathAttempted += s->obstacleStats[i].numDeathAttempted;
        gTotalObstacleStats[i].numDeathCompleted += s->obstacleStats[i].numDeathCompleted;
    }
    for (k = 0; k < 4; k++) {
        gTotalCircuitStats[k].numCircuits += s->circuitStats[k].numCircuits;
        gTotalCircuitStats[k].num1st += s->circuitStats[k].num1st;
        gTotalCircuitStats[k].num2nd += s->circuitStats[k].num2nd;
        gTotalCircuitStats[k].num3rd += s->circuitStats[k].num3rd;
        gTotalCircuitStats[k].numDeathAttempted += s->circuitStats[k].numDeathAttempted;
        gTotalCircuitStats[k].numDeathCompleted += s->circuitStats[k].numDeathCompleted;
        if (gTotalCircuitStats[k].bestPts < s->circuitStats[k].bestPts) gTotalCircuitStats[k].bestPts = s->circuitStats[k].bestPts;
        gTotalCircuitStats[k].totalPts += s->circuitStats[k].totalPts;
    }
}
