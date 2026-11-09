/* SavePlayerTrack: save the track a profile chose (MAT3 save.c). */
#include "game/game.h"

extern void PS_WriteFile(void *,u32 *,int);

/* Stores track value in the settings of race type raceType (a race, practice or ghost race: the race settings; a
 * circuit isn't stored), with their checksum, marked to write, and makes raceType the profile's last
 * (SavePlayerRaceType). The file must be read. */
void SavePlayerTrack(tProfile **profile,int raceType,u8 value)
{
    tSaveData *data=*(*profile)->datah;
    switch(raceType) {
    case 0:case 1:case 2:
        if(value==data->raceSettings.track)break;
        data->raceSettings.track=value;
        data->raceSettings.checksum=CalcChecksumInternal(&data->raceSettings.track,16);
        PS_WriteFile((*profile)->fileh,&data->raceSettings.checksum,20);
        break;
    case kBattle:
        if(value==data->battleSettings.track)break;
        data->battleSettings.track=value;
        data->battleSettings.checksum=CalcChecksumInternal(&data->battleSettings.track,12);
        PS_WriteFile((*profile)->fileh,&data->battleSettings.checksum,16);
        break;
    case kStunt:
        if(value==data->stuntSettings.track)break;
        data->stuntSettings.track=value;
        data->stuntSettings.checksum=CalcChecksumInternal(&data->stuntSettings.track,16);
        PS_WriteFile((*profile)->fileh,&data->stuntSettings.checksum,20);
        break;
    case kObstacle:
        if(value==data->obstacleSettings.track)break;
        data->obstacleSettings.track=value;
        data->obstacleSettings.checksum=CalcChecksumInternal(&data->obstacleSettings.track,8);
        PS_WriteFile((*profile)->fileh,&data->obstacleSettings.checksum,12);
        break;
    default:return;
    }
    SavePlayerRaceType(profile,raceType);
}
