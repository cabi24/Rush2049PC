#include "game/game.h"

typedef struct {s16 pad0,first;u8 pad4[4];s16 count;} Path;
VIEW(Path, gCheckpoints);
#define gCheckpoints gCheckpoints_view
s32 GetNextCheckpoint(s32 index) {if(index+1 == gCheckpoints.count)return gCheckpoints.first;return index+1;}
