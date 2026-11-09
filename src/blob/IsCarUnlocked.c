#include "game/game.h"

VIEW(s8[][13], gUnlockedCars);
#define gUnlockedCars gUnlockedCars_view
s8 IsCarUnlocked(s32 row,s32 col) {return gUnlockedCars[row][col];}
