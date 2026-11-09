/* GetPlayerCarView: the camera view a profile last chose (MAT3 save.c). */
#include "game/game.h"

/* The camera view the profile last chose for its last race type (tSaveData.raceType). The file must be read. */
u8 GetPlayerCarView(tProfile **profile) {
    tSaveData *data;

    data = *(*profile)->datah;
    switch (data->raceType) {
    case kBattle:
        return data->battleSettings.carView;
    case kStunt:
        return data->stuntSettings.carView;
    case kObstacle:
        return data->obstacleSettings.carView;
    }
    return data->raceSettings.carView;
}
