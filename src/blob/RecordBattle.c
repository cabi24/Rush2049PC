/* RecordBattle: record a battle (MAT3 gamerecords.c). */
#include "game/game.h"

VIEW(s8[], gBattlePoints);
#define gBattlePoints gBattlePoints_view

/* Adds each player's battle to its profile's stats for the arena and to the totals: the kills (its points against
 * each other player), the deaths (the size of every player's points against it, its own included), a match, and a win
 * for the winner (gRankedPlayer), its team, or a tie; then writes the profile's stats. A player without a profile
 * uses its "just play" one; a profile whose file isn't read stops it all. */
void RecordBattle(void)
{
    s32 arena;
    s32 i;

    arena = gTrackNum - TRACK_BATTLE_FIRST;
    for (i = 0; i < gNumPlayers; i++)
    {
        if (gPlayers[i].save == 0)
        {
            gPlayers[i].save = &gJustPlayers[gPlayers[i].controller];
        }
        if ((*gPlayers[i].save)->datah == 0)
        {
            return;
        }
        {
          s32 pass;
          tBattleStats *stats;
          for (pass = 0; pass < 2; pass++)
          {
            s32 k;
            if (pass == 0)
            {
                stats = &(*(*gPlayers[i].save)->datah)->battleStats[arena];
            }
            else
            {
                stats = &gTotalBattleStats[arena];
            }
            for (k = 0; k < gNumPlayers; k++)
            {
                if (k != i)
                {
                    stats->totalKills += gBattlePoints[i * 4 + k];
                }
                {
                    s32 deaths;
                    s8 points;
                    points = gBattlePoints[k * 4 + i];
                    if (points >= 0)
                    {
                        deaths = points;
                    }
                    else
                    {
                        deaths = -points;
                    }
                    stats->totalDeaths = (u16) (stats->totalDeaths + deaths);
                }
            }
            stats->totalMatches = (u16) (stats->totalMatches + 1);
            if (i == gRankedPlayer[0] || gCarTeam[i] == gCarTeam[gRankedPlayer[0]] || tied[i] == 1)
            {
                stats->totalWins = (u16) (stats->totalWins + 1);
            }
          }
        }
        SavePlayerStats(gPlayers[i].save, gTrackNum);
    }
}
