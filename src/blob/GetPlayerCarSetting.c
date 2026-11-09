/* GetPlayerCarSetting: one of a profile's car settings (MAT3 save.c). */
#include "game/game.h"

/* Setting number setting (a slider of the car setup screen) of car car in the profile's file, which must be read. */
s8 GetPlayerCarSetting(tProfile **profile, u8 car, u8 setting) {
    return (*(*profile)->datah)->carSettings[car].settings[setting];
}
