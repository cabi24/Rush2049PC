/* RecordPractice: record a practice run (MAT3 gamerecords.c). */
#include "game/game.h"

#define NUM_RACE_TRACKS 6     /* raceStats[6..11]: the tracks backwards */
#define FIRST_BACKWARD_TRACK 19   /* gTrackNum of the first track backwards (SavePlayerStats' numbering) */
#define FEET_PER_TENTH_MILE 528.0f

/* A player's input record as this reads it: the car, which "just play" profile, the profile handle in use. */
typedef struct PlayerPad { u8 car; u8 selector; u8 pad2[0x46]; tProfile **profile; } PlayerPad;
/* A car's record as this reads it: the distance driven, in feet. */
typedef struct CarDistance { u8 pad[264]; f32 dist; u8 pad2[0x110 - 0x10C]; struct pc_visual visuals[21]; u8 pad2_308[0x380 - 0x308]; void *input; u8 pad2_384[0x3B8 - 0x384]; } CarDistance;

extern PlayerPad gPlayers[];

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
        track += FIRST_BACKWARD_TRACK;
    }
    for (i = 0; i < gNumPlayers; i++)
    {
        s32 pass;
        CarDistance *car;
        if (gPlayers[i].profile == 0)
        {
            gPlayers[i].profile = &gJustPlayers[gPlayers[i].selector];
        }
        if ((*gPlayers[i].profile)->datah == 0)
        {
            return;
        }
        for (pass = 0; pass < 2; pass++)
        {
            tRaceStats *stats;
            if (pass == 0)
            {
                stats = &(*(*gPlayers[i].profile)->datah)->raceStats[stats_index];
            }
            else
            {
                stats = &gTotalRaceStats[stats_index];
            }
            car = PC_CAR(gPlayers[i].car);
            stats->totalMiles = (u32) ((f32) stats->totalMiles + car->dist / FEET_PER_TENTH_MILE);
        }
        SavePlayerStats(gPlayers[i].profile, (u8) track);
    }
}
