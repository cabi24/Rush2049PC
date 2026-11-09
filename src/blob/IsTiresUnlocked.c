/* IsTiresUnlocked: whether a set of tyres is unlocked (MAT3 save.c). */
#include "game/game.h"

/* Whether tyres column are unlocked for row (CalcUnlockedStuff fills gUnlockedTires). */
s8 IsTiresUnlocked(s32 row,s32 column) {return gUnlockedTires[row][column];}
