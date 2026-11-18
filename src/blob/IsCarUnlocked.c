/* IsCarUnlocked: whether a car is unlocked (MAT3 save.c). */
#include "game/game.h"

/* Whether car col is unlocked for row: gUnlockedCars (CalcUnlockedStuff fills it). */
s8 IsCarUnlocked(s32 row,s32 col) {return gUnlockedCars[row][col];}
