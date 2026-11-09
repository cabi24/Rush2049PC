/* IsCarUnlocked: whether a car is unlocked (MAT3 save.c). */
#include "game/game.h"

VIEW(s8[][13], gUnlockedCars);
#define gUnlockedCars gUnlockedCars_view

/* Whether car col is unlocked for row: gUnlockedCars as rows of 13 cars (CalcUnlockedStuff fills it). */
s8 IsCarUnlocked(s32 row,s32 col) {return gUnlockedCars[row][col];}
