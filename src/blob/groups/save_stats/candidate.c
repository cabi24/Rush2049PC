/* SavePlayerStats: write a profile's stats for a track (MAT3 save.c). */
#include "game/game.h"

extern void PS_WriteFile(void *,void *,u32);

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
    } else if(track>=TRACK_BATTLE_FIRST && track<TRACK_STUNT_FIRST) {
        record=&data->battleStats[track-TRACK_BATTLE_FIRST].checksum;
        *record=CalcChecksumInternal((u8 *)(record+1),8);
        PS_WriteFile((*profile)->fileh,record,12);
    } else if(track>=TRACK_STUNT_FIRST && track<TRACK_OBSTACLE) {
        record=&data->stuntStats[track-TRACK_STUNT_FIRST].checksum;
        *record=CalcChecksumInternal((u8 *)(record+1),60);
        PS_WriteFile((*profile)->fileh,record,64);
    } else {
        record=&data->obstacleStats[track-TRACK_OBSTACLE].checksum;
        *record=CalcChecksumInternal((u8 *)(record+1),20);
        PS_WriteFile((*profile)->fileh,record,24);
    }
}
