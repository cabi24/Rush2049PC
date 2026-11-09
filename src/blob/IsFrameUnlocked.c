/* IsFrameUnlocked: whether a car body is unlocked (MAT3 save.c). */
#include "game/game.h"

VIEW(s8[][6], gUnlockedFrames);
#define gUnlockedFrames gUnlockedFrames_view

/* Whether frame col is unlocked for row: gUnlockedFrames as rows of 6 (CalcUnlockedStuff fills it). */
s8 IsFrameUnlocked(s32 row,s32 col) {return gUnlockedFrames[row][col];}
