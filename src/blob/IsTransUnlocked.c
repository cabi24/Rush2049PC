/* IsTransUnlocked: whether a transmission is unlocked (MAT3 save.c). */
#include "game/game.h"

/* Whether transmission column is unlocked for row (CalcUnlockedStuff fills gUnlockedTrans). */
s8 IsTransUnlocked(s32 row,s32 column) {return gUnlockedTrans[row][column];}
