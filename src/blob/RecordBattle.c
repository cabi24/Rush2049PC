/* RecordBattle: record a battle (MAT3 gamerecords.c). */
#include "game/game.h"

/* A player's input record as this reads it: which "just play" profile, the profile handle in use. */
typedef struct PlayerPad { u8 pad0; u8 selector; u8 pad2[0x46]; tProfile **profile; } PlayerPad;

extern PlayerPad gPlayers[];
VIEW(tProfile *[], gJustPlayers);
#define gJustPlayers gJustPlayers_view
VIEW(s8, gRankedPlayer);
#define gRankedPlayer gRankedPlayer_view

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
        if (gPlayers[i].profile == 0)
        {
            gPlayers[i].profile = &gJustPlayers[gPlayers[i].selector];
        }
        if ((*gPlayers[i].profile)->datah == 0)
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
                stats = &(*(*gPlayers[i].profile)->datah)->battleStats[arena];
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
            if (i == gRankedPlayer || gCarTeam[i] == gCarTeam[gRankedPlayer] || tied[i] == 1)
            {
                stats->totalWins = (u16) (stats->totalWins + 1);
            }
          }
        }
        SavePlayerStats(gPlayers[i].profile, gTrackNum);
    }
}
