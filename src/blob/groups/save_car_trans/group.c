/* SavePlayerCarTrans: save the transmission a profile chose (MAT3 save.c). */
#include "game/game.h"

extern void PS_WriteFile(void *,u32 *,int);

/* Stores transmission value in the settings of the profile's last race type, as SavePlayerCar stores the car. */
void SavePlayerCarTrans(tProfile **profile,u8 value)
{
    tSaveData *data=*(*profile)->datah;
    switch(data->raceType) {
    case 0:case 1:case 2:case 3:
        if(value==data->raceSettings.trans)return;
        data->raceSettings.trans=value;
        data->raceSettings.checksum=CalcChecksumInternal(&data->raceSettings.track,16);
        PS_WriteFile((*profile)->fileh,&data->raceSettings.checksum,20);
        break;
    case kBattle:
        if(value==data->battleSettings.trans)return;
        data->battleSettings.trans=value;
        data->battleSettings.checksum=CalcChecksumInternal(&data->battleSettings.track,12);
        PS_WriteFile((*profile)->fileh,&data->battleSettings.checksum,16);
        break;
    case kStunt:
        if(value==data->stuntSettings.trans)return;
        data->stuntSettings.trans=value;
        data->stuntSettings.checksum=CalcChecksumInternal(&data->stuntSettings.track,16);
        PS_WriteFile((*profile)->fileh,&data->stuntSettings.checksum,20);
        break;
    case kObstacle:
        if(value==data->obstacleSettings.trans)return;
        data->obstacleSettings.trans=value;
        data->obstacleSettings.checksum=CalcChecksumInternal(&data->obstacleSettings.track,8);
        PS_WriteFile((*profile)->fileh,&data->obstacleSettings.checksum,12);
        break;
    }
}
