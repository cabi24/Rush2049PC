/* SavePlayerCar: save the car a profile chose (MAT3 save.c). */
#include "game/game.h"

#define kStunt 4      /* the race types (MAT3 enum kNumRaceTypes) */
#define kObstacle 5
#define kBattle 6

/* Stores car value in the settings of the profile's last race type (tSaveData.raceType: the race types of a race
 * share the race settings), with their checksum, marked to write; nothing when it is the same. The file must be
 * read. */
void SavePlayerCar(tProfile **profile,u8 value)
{
    tSaveData *data=*(*profile)->datah;
    switch(data->raceType) {
    case 0:case 1:case 2:case 3:
        if(value==data->raceSettings.car)return;
        data->raceSettings.car=value;
        data->raceSettings.checksum=CalcChecksumInternal(&data->raceSettings.track,16);
        PS_WriteFile((*profile)->fileh,&data->raceSettings.checksum,20);
        break;
    case kBattle:
        if(value==data->battleSettings.car)return;
        data->battleSettings.car=value;
        data->battleSettings.checksum=CalcChecksumInternal(&data->battleSettings.track,12);
        PS_WriteFile((*profile)->fileh,&data->battleSettings.checksum,16);
        break;
    case kStunt:
        if(value==data->stuntSettings.car)return;
        data->stuntSettings.car=value;
        data->stuntSettings.checksum=CalcChecksumInternal(&data->stuntSettings.track,16);
        PS_WriteFile((*profile)->fileh,&data->stuntSettings.checksum,20);
        break;
    case kObstacle:
        if(value==data->obstacleSettings.car)return;
        data->obstacleSettings.car=value;
        data->obstacleSettings.checksum=CalcChecksumInternal(&data->obstacleSettings.track,8);
        PS_WriteFile((*profile)->fileh,&data->obstacleSettings.checksum,12);
        break;
    }
}
