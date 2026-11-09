/* SavePlayerStats: write a profile's stats for a track (MAT3 save.c). */
#include "game/game.h"

#define NUM_RACE_TRACKS 6      /* raceStats[6..11] are the race tracks' second records (gTrackNum 19..24) */
#define FIRST_BATTLE_TRACK 6   /* gTrackNum: 6..13 the battle arenas */
#define FIRST_STUNT_TRACK 14   /* 14..17 the stunt arenas, 18 the obstacle course */
#define OBSTACLE_TRACK 18

/* After a race on track (gTrackNum's numbering): the stats the race changed get their checksum again and are marked
 * to write: a race track's (0..5, or 19..24 for the second records), a battle arena's, a stunt arena's or the
 * obstacle course's. The file must be read. */
void SavePlayerStats(tProfile **profile,u8 track)
{
    tSaveData *data=*(*profile)->datah;
    u32 *record;
    if((track>=0 && track<6) || (track>=19 && track<25)) {
        if(track>=19) record=&data->raceStats[track-19+NUM_RACE_TRACKS].checksum;
        else record=&data->raceStats[track].checksum;
        *record=CalcChecksumInternal((u8 *)(record+1),92);
        PS_WriteFile((*profile)->fileh,record,96);
    } else if(track>=FIRST_BATTLE_TRACK && track<FIRST_STUNT_TRACK) {
        record=&data->battleStats[track-FIRST_BATTLE_TRACK].checksum;
        *record=CalcChecksumInternal((u8 *)(record+1),8);
        PS_WriteFile((*profile)->fileh,record,12);
    } else if(track>=FIRST_STUNT_TRACK && track<OBSTACLE_TRACK) {
        record=&data->stuntStats[track-FIRST_STUNT_TRACK].checksum;
        *record=CalcChecksumInternal((u8 *)(record+1),60);
        PS_WriteFile((*profile)->fileh,record,64);
    } else {
        record=&data->obstacleStats[track-OBSTACLE_TRACK].checksum;
        *record=CalcChecksumInternal((u8 *)(record+1),20);
        PS_WriteFile((*profile)->fileh,record,24);
    }
}
