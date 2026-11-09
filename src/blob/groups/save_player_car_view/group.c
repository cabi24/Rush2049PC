/* SavePlayerCarView: save the camera view a profile chose (MAT3 save.c). */
#include "game/game.h"

#define kStunt 4      /* the race types (MAT3 enum kNumRaceTypes) */
#define kObstacle 5
#define kBattle 6

extern void PS_WriteFile(void *,u32 *,int);

/* Stores camera view value in the settings of the profile's last race type, as SavePlayerCar stores the car. */
void SavePlayerCarView(tProfile **profile,u8 value)
{
    tSaveData *data=*(*profile)->datah;
    switch(data->raceType) {
    case 0:case 1:case 2:case 3:
        if(value==data->raceSettings.carView)return;
        data->raceSettings.carView=value;
        data->raceSettings.checksum=CalcChecksumInternal(&data->raceSettings.track,16);
        PS_WriteFile((*profile)->fileh,&data->raceSettings.checksum,20);
        break;
    case kBattle:
        if(value==data->battleSettings.carView)return;
        data->battleSettings.carView=value;
        data->battleSettings.checksum=CalcChecksumInternal(&data->battleSettings.track,12);
        PS_WriteFile((*profile)->fileh,&data->battleSettings.checksum,16);
        break;
    case kStunt:
        if(value==data->stuntSettings.carView)return;
        data->stuntSettings.carView=value;
        data->stuntSettings.checksum=CalcChecksumInternal(&data->stuntSettings.track,16);
        PS_WriteFile((*profile)->fileh,&data->stuntSettings.checksum,20);
        break;
    case kObstacle:
        if(value==data->obstacleSettings.carView)return;
        data->obstacleSettings.carView=value;
        data->obstacleSettings.checksum=CalcChecksumInternal(&data->obstacleSettings.track,8);
        PS_WriteFile((*profile)->fileh,&data->obstacleSettings.checksum,12);
        break;
    }
}
