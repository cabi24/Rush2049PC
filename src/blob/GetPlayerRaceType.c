/* GetPlayerRaceType: the race type a profile last played (MAT3 save.c). */
#include "game/game.h"

/* The race type (gRace's values) the profile last played, from its file, which must be read. */
u8 GetPlayerRaceType(tProfile **profile) {
    return (*(*profile)->datah)->raceType;
}
