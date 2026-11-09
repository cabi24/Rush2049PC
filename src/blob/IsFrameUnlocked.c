#include "game/game.h"

VIEW(s8[][6], gUnlockedFrames);
#define gUnlockedFrames gUnlockedFrames_view
s8 IsFrameUnlocked(s32 row,s32 col) {return gUnlockedFrames[row][col];}
