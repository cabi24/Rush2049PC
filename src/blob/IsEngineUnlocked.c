/* IsEngineUnlocked: whether an engine is unlocked (MAT3 save.c). */
#include "game/game.h"

/* Whether engine column is unlocked for row (CalcUnlockedStuff fills gUnlockedEngines). */
s8 IsEngineUnlocked(s32 row,s32 column) {return gUnlockedEngines[row][column];}
