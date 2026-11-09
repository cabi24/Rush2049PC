/* GetPlayerCar: the car a profile last chose (MAT3 save.c). */
#include "game/game.h"

/* The car the profile last chose for its last race type (tSaveData.raceType): from the battle, stunt or obstacle
 * settings, else the race settings. The file must be read. */
u8 GetPlayerCar(tProfile **profile) {
    tSaveData *data = *(*profile)->datah;
    switch (data->raceType) {
    case kBattle: return data->battleSettings.car;
    case kStunt: return data->stuntSettings.car;
    case kObstacle: return data->obstacleSettings.car;
    default: return data->raceSettings.car;
    }
}
