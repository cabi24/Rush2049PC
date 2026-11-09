/* SavePlayerCoins: write the coins a profile found (MAT3 save.c). */
#include "game/game.h"


/* After FoundCoin: the stats holding track's coin bits get their checksum again (on a race track the first record
 * only; the second's checksum is left as it was) and the changed bytes, the checksum's first and the coins', are
 * marked to write. Race tracks are 0..5 and 19..24, stunt arenas 14..17. */
void SavePlayerCoins(tProfile **profile,u8 track)
{
    tSaveData *data=*(*profile)->datah;
    if((track>=0 && track<6) || (track>=19 && track<25)) {
        if(track>=19) track-=19;
        data->raceStats[track].checksum=CalcChecksumInternal((u8 *)data->raceStats[track].scores,92);
        PS_WriteFile((*profile)->fileh,&data->raceStats[track].checksum,1);
        PS_WriteFile((*profile)->fileh,&data->raceStats[track].coinsFound,2);
        PS_WriteFile((*profile)->fileh,&data->raceStats[track+NUM_RACE_TRACKS].checksum,1);
        PS_WriteFile((*profile)->fileh,&data->raceStats[track+NUM_RACE_TRACKS].coinsFound,2);
    } else if(track>=TRACK_STUNT_FIRST && track<18) {
        data->stuntStats[track-TRACK_STUNT_FIRST].checksum=CalcChecksumInternal((u8 *)&data->stuntStats[track-TRACK_STUNT_FIRST].bestStunt,60);
        PS_WriteFile((*profile)->fileh,&data->stuntStats[track-TRACK_STUNT_FIRST].checksum,1);
        PS_WriteFile((*profile)->fileh,&data->stuntStats[track-TRACK_STUNT_FIRST].coinsFound,2);
    }
}
