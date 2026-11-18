/* RecordPractice: record a practice run (MAT3 gamerecords.c). */
#include "game/game.h"


/* Adds the distance each player drove (in tenths of a mile, kept whole) to its profile's stats for the track (the
 * backward ones when gBackwards) and to the totals, then writes the profile's stats. A player without a profile uses
 * its "just play" one; a profile whose file isn't read stops it all. */
void RecordPractice(void)
{
    s32 i;
    s32 stats_index;
    s32 track;

    stats_index = gTrackNum;
    track = gTrackNum;
    if (gBackwards)
    {
        stats_index += NUM_RACE_TRACKS;
        track += TRACK_BACKWARD_FIRST;
    }
    for (i = 0; i < gNumPlayers; i++)
    {
        s32 pass;
        tCarData *car;
        if (gPlayers[i].save == 0)
        {
            gPlayers[i].save = &gJustPlayers[gPlayers[i].controller];
        }
        if ((*gPlayers[i].save)->datah == 0)
        {
            return;
        }
        for (pass = 0; pass < 2; pass++)
        {
            tRaceStats *stats;
            if (pass == 0)
            {
                stats = &(*(*gPlayers[i].save)->datah)->raceStats[stats_index];
            }
            else
            {
                stats = &gTotalRaceStats[stats_index];
            }
            car = &pc_cars[gPlayers[i].slot];
            stats->totalMiles = (u32) ((f32) stats->totalMiles + car->mileage / FEET_PER_TENTH_MILE);
        }
        SavePlayerStats(gPlayers[i].save, (u8) track);
    }
}
