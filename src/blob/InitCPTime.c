/* InitCPTime: the checkpoint timing at the start of a race (MAT3 checkpoint.c). */
#include "game/game.h"

/* gNumActiveCars as MAT3 has it, an unsigned byte. */
VIEW(u8, gNumActiveCars);
#define gNumActiveCars gNumActiveCars_view

/* Clears the leader's time and the distance each car has driven, and takes each car's position as where its next
 * stretch of driving starts (CheckCPs adds the distance from there to the car's mileage). */
void InitCPTime(void)
{
    s32 i;

    gFirstPlaceTime = 0;
    gInFirstPlace = 0;
    for (i = 0; i < (s8) gNumActiveCars; i++) {
        PC_CAR_MILEAGE(i) = 0.0f; /* the car's mileage (MAT3 tCarData.mileage) */
        PC_CP_LAST_POS_X(i) = PC_CAR_POS_X(i);
        PC_CP_LAST_POS_Y(i) = PC_CAR_POS_Y(i);
        PC_CP_LAST_POS_Z(i) = PC_CAR_POS_Z(i);
    }
}
