/* SavePlayerRaceType: remember the race type a profile last played (MAT3 save.c). */
#include "game/game.h"

void PS_WriteFile(tPakHandle *fileh, void *data, s32 size);

/* Stores raceType (gRace's values) in the profile's file, marking that byte to write, when it changed. The file must
 * be read. */
void SavePlayerRaceType(tProfile **profile, s32 raceType) {
    tSaveData *data;

    data = *(*profile)->datah;
    if (raceType != data->raceType) {
        data->raceType = raceType;
        PS_WriteFile((*profile)->fileh, &(*(*profile)->datah)->raceType, 1);
    }
}
