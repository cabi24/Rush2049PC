/* IsFrameUnlocked: whether a car body is unlocked (MAT3 save.c). */
#include "game/game.h"

/* Whether frame col is unlocked for row: gUnlockedFrames (CalcUnlockedStuff fills it). */
s8 IsFrameUnlocked(s32 row,s32 col) {return gUnlockedFrames[row][col];}
