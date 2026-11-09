/* GetPlayerCarTrans: the transmission a profile last chose (MAT3 save.c). */
#include "game/game.h"

#define kStunt 4      /* the race types (MAT3 enum kNumRaceTypes) */
#define kObstacle 5
#define kBattle 6

/* The transmission the profile last chose for its last race type (tSaveData.raceType). The file must be read. */
u8 GetPlayerCarTrans(tProfile **profile) {
    tSaveData *data;

    data = *(*profile)->datah;
    switch (data->raceType) {
    case kBattle:
        return data->battleSettings.trans;
    case kStunt:
        return data->stuntSettings.trans;
    case kObstacle:
        return data->obstacleSettings.trans;
    }
    return data->raceSettings.trans;
}
